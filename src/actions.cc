#include "actions.h"

#include <limits.h>
#include <string.h>

#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "config.h"
#include "critter.h"
#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "game_sound.h"
#include "geometry.h"
#include "interface.h"
#include "item.h"
#include "map.h"
#include "memory.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "proto.h"
#include "proto_instance.h"
#include "proto_types.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "sfall_config.h"
#include "skill.h"
#include "stat.h"
#include "text_object.h"
#include "tile.h"
#include "trait.h"

namespace fallout {

#define MAX_KNOCKDOWN_DISTANCE 20

typedef enum ScienceRepairTargetType {
    SCIENCE_REPAIR_TARGET_TYPE_DEFAULT,
    SCIENCE_REPAIR_TARGET_TYPE_DUDE,
    SCIENCE_REPAIR_TARGET_TYPE_ANYONE,
} ScienceRepairTargetType;

// 0x5106D0
static int _action_in_explode = 0;

// 0x5106E0
static const int gNormalDeathAnimations[DAMAGE_TYPE_COUNT] = {
    ANIM_DANCING_AUTOFIRE,
    ANIM_SLICED_IN_HALF,
    ANIM_CHARRED_BODY,
    ANIM_CHARRED_BODY,
    ANIM_ELECTRIFY,
    ANIM_FALL_BACK,
    ANIM_BIG_HOLE,
};

// 0x5106FC
static const int gMaximumBloodDeathAnimations[DAMAGE_TYPE_COUNT] = {
    ANIM_CHUNKS_OF_FLESH,
    ANIM_SLICED_IN_HALF,
    ANIM_FIRE_DANCE,
    ANIM_MELTED_TO_NOTHING,
    ANIM_ELECTRIFIED_TO_NOTHING,
    ANIM_FALL_BACK,
    ANIM_EXPLODED_TO_NOTHING,
};

static int actionKnockdown(Object* obj, int* anim, int maxDistance, int rotation, int delay);
static int _action_blood(Object* obj, int anim, int delay);
static int _pick_death(Object* attacker, Object* defender, Object* weapon, int damage, int anim, bool isFallingBack);
static int _check_death(Object* obj, int anim, int minViolenceLevel, bool isFallingBack);
static int _internal_destroy(Object* a1, Object* a2);
static void _show_damage_to_object(Object* a1, int damage, int flags, Object* weapon, bool isFallingBack, int knockbackDistance, int knockbackRotation, int a8, Object* a9, int a10);
static int _show_death(Object* obj, int anim);
static int _show_damage_extras(Attack* attack);
static void _show_damage(Attack* attack, int a2, int a3);
static int _action_melee(Attack* attack, int a2);
static int _action_ranged(Attack* attack, int a2);
static int _is_next_to(Object* a1, Object* a2);
static int _action_climb_ladder(Object* a1, Object* a2);
static int _action_use_skill_in_combat_error(Object* critter);
static int _pick_fall(Object* obj, int anim);
static int _report_explosion(Attack* attack, Object* a2);
static int _finished_explosion(Object* a1, Object* a2);
static int _compute_explosion_damage(int min, int max, Object* a3, int* a4);
static int _can_talk_to(Object* a1, Object* a2);
static int _talk_to(Object* a1, Object* a2);
static int _report_dmg(Attack* attack, Object* a2);
static int _compute_dmg_damage(int min, int max, Object* obj, int* a4, int damage_type);

// 0x410468
int actionKnockdown(Object* obj, int* anim, int maxDistance, int rotation, int delay)
{
    if (_critter_flag_check(obj->pid, CRITTER_NO_KNOCKBACK)) {
        return -1;
    }

    if (*anim == ANIM_FALL_FRONT) {
        int fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, *anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (!artExists(fid)) {
            *anim = ANIM_FALL_BACK;
        }
    }

    // SFALL: Fix to limit the maximum distance for the knockback animation.
    if (maxDistance > MAX_KNOCKDOWN_DISTANCE) {
        maxDistance = MAX_KNOCKDOWN_DISTANCE;
    }

    int distance;
    int tile;
    for (distance = 1; distance <= maxDistance; distance++) {
        tile = tileGetTileInDirection(obj->tile, rotation, distance);
        if (_obj_blocking_at(obj, tile, obj->elevation) != NULL) {
            distance--;
            break;
        }
    }

    const char* soundEffectName = sfxBuildCharName(obj, *anim, CHARACTER_SOUND_EFFECT_KNOCKDOWN);
    animationRegisterPlaySoundEffect(obj, soundEffectName, delay);

    // TODO: Check, probably step back because we've started with 1?
    distance--;

    if (distance <= 0) {
        tile = obj->tile;
        animationRegisterAnimate(obj, *anim, 0);
    } else {
        tile = tileGetTileInDirection(obj->tile, rotation, distance);
        animationRegisterMoveToTileStraightAndWaitForComplete(obj, tile, obj->elevation, *anim, 0);
    }

    return tile;
}

// 0x410568
int _action_blood(Object* obj, int anim, int delay)
{
    if (settings.preferences.violence_level == VIOLENCE_LEVEL_NONE) {
        return anim;
    }

    int bloodyAnim;
    if (anim == ANIM_FALL_BACK) {
        bloodyAnim = ANIM_FALL_BACK_BLOOD;
    } else if (anim == ANIM_FALL_FRONT) {
        bloodyAnim = ANIM_FALL_FRONT_BLOOD;
    } else {
        return anim;
    }

    int fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, bloodyAnim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    if (artExists(fid)) {
        animationRegisterAnimate(obj, bloodyAnim, delay);
    } else {
        bloodyAnim = anim;
    }

    return bloodyAnim;
}

// 0x41060C
int _pick_death(Object* attacker, Object* defender, Object* weapon, int damage, int anim, bool isFallingBack)
{
    int normalViolenceLevelDamageThreshold = 15;
    int maximumBloodViolenceLevelDamageThreshold = 45;

    int damageType = weaponGetDamageType(attacker, weapon);

    if (weapon != NULL && weapon->pid == PROTO_ID_MOLOTOV_COCKTAIL) {
        normalViolenceLevelDamageThreshold = 5;
        maximumBloodViolenceLevelDamageThreshold = 15;
        damageType = DAMAGE_TYPE_FIRE;
        anim = ANIM_FIRE_SINGLE;
    }

    if (attacker == gDude && perkHasRank(attacker, PERK_PYROMANIAC) && damageType == DAMAGE_TYPE_FIRE) {
        normalViolenceLevelDamageThreshold = 1;
        maximumBloodViolenceLevelDamageThreshold = 1;
    }

    if (weapon != NULL && weaponGetPerk(weapon) == PERK_WEAPON_FLAMEBOY) {
        normalViolenceLevelDamageThreshold /= 3;
        maximumBloodViolenceLevelDamageThreshold /= 3;
    }

    int violenceLevel = settings.preferences.violence_level;

    if (_critter_flag_check(defender->pid, CRITTER_SPECIAL_DEATH)) {
        return _check_death(defender, ANIM_EXPLODED_TO_NOTHING, VIOLENCE_LEVEL_NORMAL, isFallingBack);
    }

    bool hasBloodyMess = false;
    if (attacker == gDude && traitIsSelected(TRAIT_BLOODY_MESS)) {
        hasBloodyMess = true;
    }

    // NOTE: Original code is slightly different. There are lots of jumps and
    // conditions. It's easier to set the default in advance, rather than catch
    // it with bunch of "else" statements.
    int deathAnim = ANIM_FALL_BACK;

    if ((anim == ANIM_THROW_PUNCH && damageType == DAMAGE_TYPE_NORMAL)
        || anim == ANIM_KICK_LEG
        || anim == ANIM_THRUST_ANIM
        || anim == ANIM_SWING_ANIM
        || (anim == ANIM_THROW_ANIM && damageType != DAMAGE_TYPE_EXPLOSION)) {
        if (violenceLevel == VIOLENCE_LEVEL_MAXIMUM_BLOOD && hasBloodyMess) {
            deathAnim = ANIM_BIG_HOLE;
        }
    } else {
        if (anim == ANIM_FIRE_SINGLE && damageType == DAMAGE_TYPE_NORMAL) {
            if (violenceLevel == VIOLENCE_LEVEL_MAXIMUM_BLOOD) {
                if (hasBloodyMess || maximumBloodViolenceLevelDamageThreshold <= damage) {
                    deathAnim = ANIM_BIG_HOLE;
                }
            }
        } else {
            if (violenceLevel > VIOLENCE_LEVEL_MINIMAL && (hasBloodyMess || normalViolenceLevelDamageThreshold <= damage)) {
                if (violenceLevel > VIOLENCE_LEVEL_NORMAL && (hasBloodyMess || maximumBloodViolenceLevelDamageThreshold <= damage)) {
                    deathAnim = gMaximumBloodDeathAnimations[damageType];
                    if (_check_death(defender, deathAnim, VIOLENCE_LEVEL_MAXIMUM_BLOOD, isFallingBack) != deathAnim) {
                        deathAnim = gNormalDeathAnimations[damageType];
                    }
                } else {
                    deathAnim = gNormalDeathAnimations[damageType];
                }
            }
        }
    }

    if (!isFallingBack && deathAnim == ANIM_FALL_BACK) {
        deathAnim = ANIM_FALL_FRONT;
    }

    return _check_death(defender, deathAnim, VIOLENCE_LEVEL_NONE, isFallingBack);
}

// 0x410814
int _check_death(Object* obj, int anim, int minViolenceLevel, bool isFallingBack)
{
    int fid;

    if (settings.preferences.violence_level >= minViolenceLevel) {
        fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (artExists(fid)) {
            return anim;
        }
    }

    if (isFallingBack) {
        return ANIM_FALL_BACK;
    }

    fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, ANIM_FALL_FRONT, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    if (artExists(fid)) {
        return ANIM_FALL_BACK;
    }

    return ANIM_FALL_FRONT;
}

// 0x4108C8
int _internal_destroy(Object* a1, Object* a2)
{
    return _obj_destroy(a2);
}

// TODO: Check very carefully, lots of conditions and jumps.
//
// 0x4108D0
void _show_damage_to_object(Object* a1, int damage, int flags, Object* weapon, bool isFallingBack, int knockbackDistance, int knockbackRotation, int a8, Object* a9, int a10)
{
    int anim;
    int fid;
    const char* sfx_name;

    if (_critter_flag_check(a1->pid, CRITTER_NO_KNOCKBACK)) {
        knockbackDistance = 0;
    }

    anim = FID_ANIM_TYPE(a1->fid);
    if (!_critter_is_prone(a1)) {
        if ((flags & DAM_DEAD) != 0) {
            fid = buildFid(OBJ_TYPE_MISC, 10, 0, 0, 0);
            if (fid == a9->fid) {
                anim = _check_death(a1, ANIM_EXPLODED_TO_NOTHING, VIOLENCE_LEVEL_MAXIMUM_BLOOD, isFallingBack);
            } else if (a9->pid == PROTO_ID_0x20001EB) {
                anim = _check_death(a1, ANIM_ELECTRIFIED_TO_NOTHING, VIOLENCE_LEVEL_MAXIMUM_BLOOD, isFallingBack);
            } else if (a9->fid == FID_0x20001F5) {
                anim = _check_death(a1, a8, VIOLENCE_LEVEL_MAXIMUM_BLOOD, isFallingBack);
            } else {
                anim = _pick_death(a9, a1, weapon, damage, a8, isFallingBack);
            }

            if (anim != ANIM_FIRE_DANCE) {
                if (knockbackDistance != 0 && (anim == ANIM_FALL_FRONT || anim == ANIM_FALL_BACK)) {
                    actionKnockdown(a1, &anim, knockbackDistance, knockbackRotation, a10);
                    anim = _action_blood(a1, anim, -1);
                } else {
                    sfx_name = sfxBuildCharName(a1, anim, CHARACTER_SOUND_EFFECT_DIE);
                    animationRegisterPlaySoundEffect(a1, sfx_name, a10);

                    anim = _pick_fall(a1, anim);
                    animationRegisterAnimate(a1, anim, 0);

                    if (anim == ANIM_FALL_FRONT || anim == ANIM_FALL_BACK) {
                        anim = _action_blood(a1, anim, -1);
                    }
                }
            } else {
                fid = buildFid(OBJ_TYPE_CRITTER, a1->fid & 0xFFF, ANIM_FIRE_DANCE, (a1->fid & 0xF000) >> 12, a1->rotation + 1);
                if (artExists(fid)) {
                    sfx_name = sfxBuildCharName(a1, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                    animationRegisterPlaySoundEffect(a1, sfx_name, a10);

                    // SFALL
                    if (explosionEmitsLight()) {
                        // 0xFFFF0002:
                        // - distance: 2
                        // - intensity: 65535
                        //
                        // NOTE: Change intensity to 65536 (which is on par with
                        // `anim_set_check_light_fix` Sfall's hack).
                        animationRegisterSetLightIntensity(a1, 2, 65536, 0);
                    }

                    animationRegisterAnimate(a1, anim, 0);

                    // SFALL
                    if (explosionEmitsLight()) {
                        // 0x00010000:
                        // - distance: 0
                        // - intensity: 1
                        //
                        // NOTE: Change intensity to 0. I guess using 1 was a
                        // workaround for `anim_set_check_light_fix` hack which
                        // requires two upper bytes to be non-zero to override
                        // default behaviour.
                        animationRegisterSetLightIntensity(a1, 0, 0, -1);
                    }

                    int randomDistance = randomBetween(2, 5);
                    int randomRotation = randomBetween(0, 5);

                    while (randomDistance > 0) {
                        int tile = tileGetTileInDirection(a1->tile, randomRotation, randomDistance);
                        Object* v35 = NULL;
                        _make_straight_path(a1, a1->tile, tile, NULL, &v35, 4);
                        if (v35 == NULL) {
                            animationRegisterRotateToTile(a1, tile);
                            animationRegisterMoveToTileStraight(a1, tile, a1->elevation, anim, 0);
                            break;
                        }
                        randomDistance--;
                    }
                }

                anim = ANIM_BURNED_TO_NOTHING;
                sfx_name = sfxBuildCharName(a1, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                animationRegisterPlaySoundEffect(a1, sfx_name, -1);
                animationRegisterAnimate(a1, anim, 0);
            }
        } else {
            if ((flags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0) {
                anim = isFallingBack ? ANIM_FALL_BACK : ANIM_FALL_FRONT;
                sfx_name = sfxBuildCharName(a1, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                animationRegisterPlaySoundEffect(a1, sfx_name, a10);
                if (knockbackDistance != 0) {
                    actionKnockdown(a1, &anim, knockbackDistance, knockbackRotation, 0);
                } else {
                    anim = _pick_fall(a1, anim);
                    animationRegisterAnimate(a1, anim, 0);
                }
            } else if ((flags & DAM_ON_FIRE) != 0 && artExists(buildFid(OBJ_TYPE_CRITTER, a1->fid & 0xFFF, ANIM_FIRE_DANCE, (a1->fid & 0xF000) >> 12, a1->rotation + 1))) {
                animationRegisterAnimate(a1, ANIM_FIRE_DANCE, a10);

                fid = buildFid(OBJ_TYPE_CRITTER, a1->fid & 0xFFF, ANIM_STAND, (a1->fid & 0xF000) >> 12, a1->rotation + 1);
                animationRegisterSetFid(a1, fid, -1);
            } else {
                if (knockbackDistance != 0) {
                    anim = isFallingBack ? ANIM_FALL_BACK : ANIM_FALL_FRONT;
                    actionKnockdown(a1, &anim, knockbackDistance, knockbackRotation, a10);
                    if (anim == ANIM_FALL_BACK) {
                        animationRegisterAnimate(a1, ANIM_BACK_TO_STANDING, -1);
                    } else {
                        animationRegisterAnimate(a1, ANIM_PRONE_TO_STANDING, -1);
                    }
                } else {
                    if (isFallingBack || !artExists(buildFid(OBJ_TYPE_CRITTER, a1->fid & 0xFFF, ANIM_HIT_FROM_BACK, (a1->fid & 0xF000) >> 12, a1->rotation + 1))) {
                        anim = ANIM_HIT_FROM_FRONT;
                    } else {
                        anim = ANIM_HIT_FROM_BACK;
                    }

                    sfx_name = sfxBuildCharName(a1, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                    animationRegisterPlaySoundEffect(a1, sfx_name, a10);

                    animationRegisterAnimate(a1, anim, 0);
                }
            }
        }
    } else {
        if ((flags & DAM_DEAD) != 0 && (a1->data.critter.combat.results & DAM_DEAD) == 0) {
            anim = _action_blood(a1, anim, a10);
        } else {
            return;
        }
    }

    if (weapon != NULL) {
        if ((flags & DAM_EXPLODE) != 0) {
            animationRegisterCallbackForced(a1, weapon, (AnimationCallback*)_obj_drop, -1);
            fid = buildFid(OBJ_TYPE_MISC, 10, 0, 0, 0);
            animationRegisterSetFid(weapon, fid, 0);
            animationRegisterAnimateAndHide(weapon, ANIM_STAND, 0);

            sfx_name = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_HIT, weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY, a1);
            animationRegisterPlaySoundEffect(weapon, sfx_name, 0);

            animationRegisterHideObjectForced(weapon);
        } else if ((flags & DAM_DESTROY) != 0) {
            animationRegisterCallbackForced(a1, weapon, (AnimationCallback*)_internal_destroy, -1);
        } else if ((flags & DAM_DROP) != 0) {
            animationRegisterCallbackForced(a1, weapon, (AnimationCallback*)_obj_drop, -1);
        }
    }

    if ((flags & DAM_DEAD) != 0) {
        // TODO: Get rid of casts.
        animationRegisterCallbackForced(a1, (void*)anim, (AnimationCallback*)_show_death, -1);
    }
}

// 0x410E24
int _show_death(Object* obj, int anim)
{
    Rect v7;
    Rect v8;
    int fid;

    objectGetRect(obj, &v8);
    if (anim < 48 && anim > 63) {
        fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, anim + 28, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (objectSetFid(obj, fid, &v7) == 0) {
            rectUnion(&v8, &v7, &v8);
        }

        if (objectSetFrame(obj, 0, &v7) == 0) {
            rectUnion(&v8, &v7, &v8);
        }
    }

    if (!_critter_flag_check(obj->pid, CRITTER_FLAT)) {
        obj->flags |= OBJECT_NO_BLOCK;
        if (_obj_toggle_flat(obj, &v7) == 0) {
            rectUnion(&v8, &v7, &v8);
        }
    }

    if (objectDisableOutline(obj, &v7) == 0) {
        rectUnion(&v8, &v7, &v8);
    }

    if (anim >= 30 && anim <= 31 && !_critter_flag_check(obj->pid, CRITTER_SPECIAL_DEATH) && !_critter_flag_check(obj->pid, CRITTER_NO_DROP)) {
        itemDropAll(obj, obj->tile);
    }

    tileWindowRefreshRect(&v8, obj->elevation);

    return 0;
}

// 0x410FEC
int _show_damage_extras(Attack* attack)
{
    int v6;
    int v8;
    int v9;

    for (int index = 0; index < attack->extrasLength; index++) {
        Object* obj = attack->extras[index];
        if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER) {
            int delta = attack->attacker->rotation - obj->rotation;
            if (delta < 0) {
                delta = -delta;
            }

            v6 = delta != 0 && delta != 1 && delta != 5;
            reg_anim_begin(ANIMATION_REQUEST_RESERVED);
            _register_priority(1);
            v8 = critterGetAnimationForHitMode(attack->attacker, attack->hitMode);
            v9 = tileGetRotationTo(attack->attacker->tile, obj->tile);
            _show_damage_to_object(obj, attack->extrasDamage[index], attack->extrasFlags[index], attack->weapon, v6, attack->extrasKnockback[index], v9, v8, attack->attacker, 0);
            reg_anim_end();
        }
    }

    return 0;
}

// 0x4110AC
void _show_damage(Attack* attack, int a2, int a3)
{
    int v5;
    int v14;
    int v17;
    int v15;

    v5 = a3;
    for (int index = 0; index < attack->extrasLength; index++) {
        Object* object = attack->extras[index];
        if (FID_TYPE(object->fid) == OBJ_TYPE_CRITTER) {
            reg_anim_26(2, v5);
            v5 = 0;
        }
    }

    if ((attack->attackerFlags & DAM_HIT) == 0) {
        if ((attack->attackerFlags & DAM_CRITICAL) != 0) {
            _show_damage_to_object(attack->attacker, attack->attackerDamage, attack->attackerFlags, attack->weapon, 1, 0, 0, a2, attack->attacker, -1);
        } else if ((attack->attackerFlags & DAM_BACKWASH) != 0) {
            _show_damage_to_object(attack->attacker, attack->attackerDamage, attack->attackerFlags, attack->weapon, 1, 0, 0, a2, attack->attacker, -1);
        }
    } else {
        if (attack->defender != NULL) {
            // TODO: Looks very similar to _show_damage_extras.
            int delta = attack->defender->rotation - attack->attacker->rotation;
            if (delta < 0) {
                delta = -delta;
            }

            v15 = delta != 0 && delta != 1 && delta != 5;

            if (FID_TYPE(attack->defender->fid) == OBJ_TYPE_CRITTER) {
                if (attack->attacker->fid == 33554933) {
                    v14 = tileGetRotationTo(attack->attacker->tile, attack->defender->tile);
                    _show_damage_to_object(attack->defender, attack->defenderDamage, attack->defenderFlags, attack->weapon, v15, attack->defenderKnockback, v14, a2, attack->attacker, a3);
                } else {
                    v17 = critterGetAnimationForHitMode(attack->attacker, attack->hitMode);
                    v14 = tileGetRotationTo(attack->attacker->tile, attack->defender->tile);
                    _show_damage_to_object(attack->defender, attack->defenderDamage, attack->defenderFlags, attack->weapon, v15, attack->defenderKnockback, v14, v17, attack->attacker, a3);
                }
            } else {
                tileGetRotationTo(attack->attacker->tile, attack->defender->tile);
                critterGetAnimationForHitMode(attack->attacker, attack->hitMode);
            }
        }

        if ((attack->attackerFlags & DAM_DUD) != 0) {
            _show_damage_to_object(attack->attacker, attack->attackerDamage, attack->attackerFlags, attack->weapon, 1, 0, 0, a2, attack->attacker, -1);
        }
    }
}

// 0x411224
int _action_attack(Attack* attack)
{
    if (reg_anim_clear(attack->attacker) == -2) {
        return -1;
    }

    if (reg_anim_clear(attack->defender) == -2) {
        return -1;
    }

    for (int index = 0; index < attack->extrasLength; index++) {
        if (reg_anim_clear(attack->extras[index]) == -2) {
            return -1;
        }
    }

    int anim = critterGetAnimationForHitMode(attack->attacker, attack->hitMode);
    if (anim < ANIM_FIRE_SINGLE && anim != ANIM_THROW_ANIM) {
        return _action_melee(attack, anim);
    } else {
        return _action_ranged(attack, anim);
    }
}

// 0x4112B4
int _action_melee(Attack* attack, int anim)
{
    int fid;
    Art* art;
    CacheEntry* cache_entry;
    int v17;
    int v18;
    int delta;
    int flag;
    const char* sfx_name;
    char sfx_name_temp[16];

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);
    _register_priority(1);

    fid = buildFid(OBJ_TYPE_CRITTER, attack->attacker->fid & 0xFFF, anim, (attack->attacker->fid & 0xF000) >> 12, attack->attacker->rotation + 1);
    art = artLock(fid, &cache_entry);
    if (art != NULL) {
        v17 = artGetActionFrame(art);
    } else {
        v17 = 0;
    }
    artUnlock(cache_entry);

    tileGetTileInDirection(attack->attacker->tile, attack->attacker->rotation, 1);
    animationRegisterRotateToTile(attack->attacker, attack->defender->tile);

    delta = attack->attacker->rotation - attack->defender->rotation;
    if (delta < 0) {
        delta = -delta;
    }
    flag = delta != 0 && delta != 1 && delta != 5;

    if (anim != ANIM_THROW_PUNCH && anim != ANIM_KICK_LEG) {
        sfx_name = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_ATTACK, attack->weapon, attack->hitMode, attack->defender);
    } else {
        sfx_name = sfxBuildCharName(attack->attacker, anim, CHARACTER_SOUND_EFFECT_UNUSED);
    }

    strcpy(sfx_name_temp, sfx_name);

    _combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_ATTACK, 0);

    if (attack->attackerFlags & 0x0300) {
        animationRegisterPlaySoundEffect(attack->attacker, sfx_name_temp, 0);
        if (anim != ANIM_THROW_PUNCH && anim != ANIM_KICK_LEG) {
            sfx_name = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_HIT, attack->weapon, attack->hitMode, attack->defender);
        } else {
            sfx_name = sfxBuildCharName(attack->attacker, anim, CHARACTER_SOUND_EFFECT_CONTACT);
        }

        strcpy(sfx_name_temp, sfx_name);

        animationRegisterAnimate(attack->attacker, anim, 0);
        animationRegisterPlaySoundEffect(attack->attacker, sfx_name_temp, v17);
        _show_damage(attack, anim, 0);
    } else {
        if (attack->defender->data.critter.combat.results & 0x03) {
            animationRegisterPlaySoundEffect(attack->attacker, sfx_name_temp, -1);
            animationRegisterAnimate(attack->attacker, anim, 0);
        } else {
            fid = buildFid(OBJ_TYPE_CRITTER, attack->defender->fid & 0xFFF, ANIM_DODGE_ANIM, (attack->defender->fid & 0xF000) >> 12, attack->defender->rotation + 1);
            art = artLock(fid, &cache_entry);
            if (art != NULL) {
                v18 = artGetActionFrame(art);
                artUnlock(cache_entry);

                if (v18 <= v17) {
                    animationRegisterPlaySoundEffect(attack->attacker, sfx_name_temp, -1);
                    animationRegisterAnimate(attack->attacker, anim, 0);

                    sfx_name = sfxBuildCharName(attack->defender, ANIM_DODGE_ANIM, CHARACTER_SOUND_EFFECT_UNUSED);
                    animationRegisterPlaySoundEffect(attack->defender, sfx_name, v17 - v18);
                    animationRegisterAnimate(attack->defender, ANIM_DODGE_ANIM, 0);
                } else {
                    sfx_name = sfxBuildCharName(attack->defender, ANIM_DODGE_ANIM, CHARACTER_SOUND_EFFECT_UNUSED);
                    animationRegisterPlaySoundEffect(attack->defender, sfx_name, -1);
                    animationRegisterAnimate(attack->defender, ANIM_DODGE_ANIM, 0);
                    animationRegisterPlaySoundEffect(attack->attacker, sfx_name_temp, v18 - v17);
                    animationRegisterAnimate(attack->attacker, anim, 0);
                }
            }
        }
    }

    if ((attack->attackerFlags & DAM_HIT) != 0) {
        if ((attack->defenderFlags & DAM_DEAD) == 0) {
            _combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_HIT, -1);
        }
    } else {
        _combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_MISS, -1);
    }

    if (reg_anim_end() == -1) {
        return -1;
    }

    _show_damage_extras(attack);

    return 0;
}

// 0x411600
int _action_ranged(Attack* attack, int anim)
{
    Object* neighboors[6];
    memset(neighboors, 0, sizeof(neighboors));

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);
    _register_priority(1);

    Object* projectile = NULL;
    Object* v50 = NULL;
    int weaponFid = -1;

    Proto* weaponProto;
    Object* weapon = attack->weapon;
    protoGetProto(weapon->pid, &weaponProto);

    int fid = buildFid(OBJ_TYPE_CRITTER, attack->attacker->fid & 0xFFF, anim, (attack->attacker->fid & 0xF000) >> 12, attack->attacker->rotation + 1);
    CacheEntry* artHandle;
    Art* art = artLock(fid, &artHandle);
    int actionFrame = (art != NULL) ? artGetActionFrame(art) : 0;
    artUnlock(artHandle);

    weaponGetRange(attack->attacker, attack->hitMode);

    int damageType = weaponGetDamageType(attack->attacker, attack->weapon);

    tileGetTileInDirection(attack->attacker->tile, attack->attacker->rotation, 1);

    animationRegisterRotateToTile(attack->attacker, attack->defender->tile);

    bool isGrenade = false;
    if (anim == ANIM_THROW_ANIM) {
        // SFALL
        if (damageType == explosionGetDamageType() || damageType == DAMAGE_TYPE_PLASMA || damageType == DAMAGE_TYPE_EMP) {
            isGrenade = true;
        }
    } else {
        animationRegisterAnimate(attack->attacker, ANIM_POINT, -1);
    }

    _combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_ATTACK, 0);

    const char* sfx;
    if (((attack->attacker->fid & 0xF000) >> 12) != 0) {
        sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_ATTACK, weapon, attack->hitMode, attack->defender);
    } else {
        sfx = sfxBuildCharName(attack->attacker, anim, CHARACTER_SOUND_EFFECT_UNUSED);
    }
    animationRegisterPlaySoundEffect(attack->attacker, sfx, -1);

    animationRegisterAnimate(attack->attacker, anim, 0);

    if (anim != ANIM_FIRE_CONTINUOUS) {
        if ((attack->attackerFlags & DAM_HIT) != 0 || (attack->attackerFlags & DAM_CRITICAL) == 0) {
            bool l56 = false;

            int projectilePid = weaponGetProjectilePid(weapon);
            Proto* projectileProto;
            if (protoGetProto(projectilePid, &projectileProto) != -1 && projectileProto->fid != -1) {
                if (anim == ANIM_THROW_ANIM) {
                    projectile = weapon;
                    weaponFid = weapon->fid;
                    int weaponFlags = weapon->flags;

                    int leftItemAction;
                    int rightItemAction;
                    interfaceGetItemActions(&leftItemAction, &rightItemAction);

                    itemRemove(attack->attacker, weapon, 1);
                    v50 = itemReplace(attack->attacker, weapon, weaponFlags & OBJECT_IN_ANY_HAND);
                    objectSetFid(projectile, projectileProto->fid, NULL);
                    _cAIPrepWeaponItem(attack->attacker, weapon);

                    if (attack->attacker == gDude) {
                        if (v50 == NULL) {
                            if ((weaponFlags & OBJECT_IN_LEFT_HAND) != 0) {
                                leftItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                            } else if ((weaponFlags & OBJECT_IN_RIGHT_HAND) != 0) {
                                rightItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                            }
                        }
                        interfaceUpdateItems(false, leftItemAction, rightItemAction);
                    }

                    _obj_connect(weapon, attack->attacker->tile, attack->attacker->elevation, NULL);
                } else {
                    objectCreateWithFidPid(&projectile, projectileProto->fid, -1);
                }

                objectHide(projectile, NULL);

                // SFALL
                if (explosionEmitsLight() && projectile->lightIntensity == 0) {
                    objectSetLight(projectile, projectileProto->item.lightDistance, projectileProto->item.lightIntensity, NULL);
                } else {
                    objectSetLight(projectile, 9, projectile->lightIntensity, NULL);
                }

                int projectileOrigin = _combat_bullet_start(attack->attacker, attack->defender);
                objectSetLocation(projectile, projectileOrigin, attack->attacker->elevation, NULL);

                int projectileRotation = tileGetRotationTo(attack->attacker->tile, attack->defender->tile);
                objectSetRotation(projectile, projectileRotation, NULL);

                animationRegisterUnsetFlag(projectile, OBJECT_HIDDEN, actionFrame);

                const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_AMMO_FLYING, weapon, attack->hitMode, attack->defender);
                animationRegisterPlaySoundEffect(projectile, sfx, 0);

                int v24;
                if ((attack->attackerFlags & DAM_HIT) != 0) {
                    animationRegisterMoveToTileStraight(projectile, attack->defender->tile, attack->defender->elevation, ANIM_WALK, 0);
                    actionFrame = _make_straight_path(projectile, projectileOrigin, attack->defender->tile, NULL, NULL, 32) - 1;
                    v24 = attack->defender->tile;
                } else {
                    animationRegisterMoveToTileStraight(projectile, attack->tile, attack->defender->elevation, ANIM_WALK, 0);
                    actionFrame = 0;
                    v24 = attack->tile;
                }

                // SFALL
                if (isGrenade || damageType == explosionGetDamageType()) {
                    if ((attack->attackerFlags & DAM_DROP) == 0) {
                        int explosionFrmId;
                        if (isGrenade) {
                            switch (damageType) {
                            case DAMAGE_TYPE_EMP:
                                explosionFrmId = 2;
                                break;
                            case DAMAGE_TYPE_PLASMA:
                                explosionFrmId = 31;
                                break;
                            default:
                                explosionFrmId = 29;
                                break;
                            }
                        } else {
                            explosionFrmId = 10;
                        }

                        // SFALL
                        int explosionFrmIdOverride = explosionGetFrm();
                        if (explosionFrmIdOverride != -1) {
                            explosionFrmId = explosionFrmIdOverride;
                        }

                        if (isGrenade) {
                            animationRegisterSetFid(projectile, weaponFid, -1);
                        }

                        int explosionFid = buildFid(OBJ_TYPE_MISC, explosionFrmId, 0, 0, 0);
                        animationRegisterSetFid(projectile, explosionFid, -1);

                        const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_HIT, weapon, attack->hitMode, attack->defender);
                        animationRegisterPlaySoundEffect(projectile, sfx, 0);

                        // SFALL
                        if (explosionEmitsLight()) {
                            animationRegisterAnimate(projectile, ANIM_STAND, 0);
                            // 0xFFFF0008
                            // - distance: 8
                            // - intensity: 65535
                            animationRegisterSetLightIntensity(projectile, 8, 65536, 0);
                        } else {
                            animationRegisterAnimateAndHide(projectile, ANIM_STAND, 0);
                        }

                        // SFALL
                        int startRotation;
                        int endRotation;
                        explosionGetPattern(&startRotation, &endRotation);

                        for (int rotation = startRotation; rotation < endRotation; rotation++) {
                            if (objectCreateWithFidPid(&(neighboors[rotation]), explosionFid, -1) != -1) {
                                objectHide(neighboors[rotation], NULL);

                                int v31 = tileGetTileInDirection(v24, rotation, 1);
                                objectSetLocation(neighboors[rotation], v31, projectile->elevation, NULL);

                                int delay;
                                if (rotation != ROTATION_NE) {
                                    delay = 0;
                                } else {
                                    if (damageType == DAMAGE_TYPE_PLASMA) {
                                        delay = 4;
                                    } else {
                                        delay = 2;
                                    }
                                }

                                animationRegisterUnsetFlag(neighboors[rotation], OBJECT_HIDDEN, delay);
                                animationRegisterAnimateAndHide(neighboors[rotation], ANIM_STAND, 0);
                            }
                        }

                        l56 = true;
                    }
                } else {
                    if (anim != ANIM_THROW_ANIM) {
                        animationRegisterHideObjectForced(projectile);
                    }
                }

                if (!l56) {
                    const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_HIT, weapon, attack->hitMode, attack->defender);
                    animationRegisterPlaySoundEffect(weapon, sfx, actionFrame);
                }

                actionFrame = 0;
            } else {
                if ((attack->attackerFlags & DAM_HIT) == 0) {
                    Object* defender = attack->defender;
                    if ((defender->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) == 0) {
                        animationRegisterAnimate(defender, ANIM_DODGE_ANIM, actionFrame);
                        l56 = true;
                    }
                }
            }
        }
    }

    _show_damage(attack, anim, actionFrame);

    if ((attack->attackerFlags & DAM_HIT) == 0) {
        _combatai_msg(attack->defender, attack, AI_MESSAGE_TYPE_MISS, -1);
    } else {
        if ((attack->defenderFlags & DAM_DEAD) == 0) {
            _combatai_msg(attack->defender, attack, AI_MESSAGE_TYPE_HIT, -1);
        }
    }

    // SFALL
    if (projectile != NULL && (isGrenade || damageType == explosionGetDamageType())) {
        animationRegisterHideObjectForced(projectile);
    } else if (anim == ANIM_THROW_ANIM && projectile != NULL) {
        animationRegisterSetFid(projectile, weaponFid, -1);
    }

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        if (neighboors[rotation] != NULL) {
            animationRegisterHideObjectForced(neighboors[rotation]);
        }
    }

    if ((attack->attackerFlags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_DEAD)) == 0) {
        if (anim == ANIM_THROW_ANIM) {
            bool l9 = false;
            if (v50 != NULL) {
                int v38 = weaponGetAnimationCode(v50);
                if (v38 != 0) {
                    animationRegisterTakeOutWeapon(attack->attacker, v38, -1);
                    l9 = true;
                }
            }

            if (!l9) {
                int fid = buildFid(OBJ_TYPE_CRITTER, attack->attacker->fid & 0xFFF, ANIM_STAND, 0, attack->attacker->rotation + 1);
                animationRegisterSetFid(attack->attacker, fid, -1);
            }
        } else {
            animationRegisterAnimate(attack->attacker, ANIM_UNPOINT, -1);
        }
    }

    if (reg_anim_end() == -1) {
        debugPrint("Something went wrong with a ranged attack sequence!\n");
        if (projectile != NULL && (isGrenade || damageType == DAMAGE_TYPE_EXPLOSION || anim != ANIM_THROW_ANIM)) {
            objectDestroy(projectile, NULL);
        }

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            if (neighboors[rotation] != NULL) {
                objectDestroy(neighboors[rotation], NULL);
            }
        }

        return -1;
    }

    _show_damage_extras(attack);

    return 0;
}

// 0x411D68
int _is_next_to(Object* a1, Object* a2)
{
    if (objectGetDistanceBetween(a1, a2) > 1) {
        if (a2 == gDude) {
            MessageListItem messageListItem;
            // You cannot get there.
            messageListItem.num = 2000;
            if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }
        return -1;
    }

    return 0;
}

// 0x411DB4
int _action_climb_ladder(Object* a1, Object* a2)
{
    if (a1 == gDude) {
        int anim = FID_ANIM_TYPE(gDude->fid);
        if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
            reg_anim_clear(gDude);
        }
    }

    int animationRequestOptions;
    int actionPoints;
    if (isInCombat()) {
        animationRequestOptions = ANIMATION_REQUEST_RESERVED;
        actionPoints = a1->data.critter.combat.ap;
    } else {
        animationRequestOptions = ANIMATION_REQUEST_UNRESERVED;
        actionPoints = -1;
    }

    if (a1 == gDude) {
        animationRequestOptions = ANIMATION_REQUEST_RESERVED;
    }

    animationRequestOptions |= ANIMATION_REQUEST_NO_STAND;
    reg_anim_begin(animationRequestOptions);

    int tile = tileGetTileInDirection(a2->tile, ROTATION_SE, 1);
    if (actionPoints != -1 || objectGetDistanceBetween(a1, a2) < 5) {
        animationRegisterMoveToTile(a1, tile, a2->elevation, actionPoints, 0);
    } else {
        animationRegisterRunToTile(a1, tile, a2->elevation, actionPoints, 0);
    }

    animationRegisterCallbackForced(a1, a2, (AnimationCallback*)_is_next_to, -1);
    animationRegisterRotateToTile(a1, a2->tile);
    animationRegisterCallbackForced(a1, a2, (AnimationCallback*)_check_scenery_ap_cost, -1);

    int weaponAnimationCode = (a1->fid & 0xF000) >> 12;
    if (weaponAnimationCode != 0) {
        const char* puttingAwaySfx = sfxBuildCharName(a1, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
        animationRegisterPlaySoundEffect(a1, puttingAwaySfx, -1);
        animationRegisterAnimate(a1, ANIM_PUT_AWAY, 0);
    }

    const char* climbingSfx = sfxBuildCharName(a1, ANIM_CLIMB_LADDER, CHARACTER_SOUND_EFFECT_UNUSED);
    animationRegisterPlaySoundEffect(a1, climbingSfx, -1);
    animationRegisterAnimate(a1, ANIM_CLIMB_LADDER, 0);
    animationRegisterCallback(a1, a2, (AnimationCallback*)_obj_use, -1);

    if (weaponAnimationCode != 0) {
        animationRegisterTakeOutWeapon(a1, weaponAnimationCode, -1);
    }

    return reg_anim_end();
}

// 0x411F2C
int _action_use_an_item_on_object(Object* a1, Object* a2, Object* a3)
{
    Proto* proto = NULL;
    int type = FID_TYPE(a2->fid);
    int sceneryType = -1;
    if (type == OBJ_TYPE_SCENERY) {
        if (protoGetProto(a2->pid, &proto) == -1) {
            return -1;
        }

        sceneryType = proto->scenery.type;
    }

    if (sceneryType != SCENERY_TYPE_LADDER_UP || a3 != NULL) {
        if (a1 == gDude) {
            int anim = FID_ANIM_TYPE(gDude->fid);
            if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
                reg_anim_clear(gDude);
            }
        }

        int animationRequestOptions;
        int actionPoints;
        if (isInCombat()) {
            animationRequestOptions = ANIMATION_REQUEST_RESERVED;
            actionPoints = a1->data.critter.combat.ap;
        } else {
            animationRequestOptions = ANIMATION_REQUEST_UNRESERVED;
            actionPoints = -1;
        }

        if (a1 == gDude) {
            animationRequestOptions = ANIMATION_REQUEST_RESERVED;
        }

        reg_anim_begin(animationRequestOptions);

        if (actionPoints != -1 || objectGetDistanceBetween(a1, a2) < 5) {
            animationRegisterMoveToObject(a1, a2, actionPoints, 0);
        } else {
            animationRegisterRunToObject(a1, a2, -1, 0);
        }

        animationRegisterCallbackForced(a1, a2, (AnimationCallback*)_is_next_to, -1);

        if (a3 == NULL) {
            animationRegisterCallback(a1, a2, (AnimationCallback*)_check_scenery_ap_cost, -1);
        }

        int a2a = (a1->fid & 0xF000) >> 12;
        if (a2a != 0) {
            const char* sfx = sfxBuildCharName(a1, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
            animationRegisterPlaySoundEffect(a1, sfx, -1);
            animationRegisterAnimate(a1, ANIM_PUT_AWAY, 0);
        }

        int anim;
        int objectType = FID_TYPE(a2->fid);
        if (objectType == OBJ_TYPE_CRITTER && _critter_is_prone(a2)) {
            anim = ANIM_MAGIC_HANDS_GROUND;
        } else if (objectType == OBJ_TYPE_SCENERY && (proto->scenery.extendedFlags & 0x01) != 0) {
            anim = ANIM_MAGIC_HANDS_GROUND;
        } else {
            anim = ANIM_MAGIC_HANDS_MIDDLE;
        }

        if (sceneryType != SCENERY_TYPE_STAIRS && a3 == NULL) {
            animationRegisterAnimate(a1, anim, -1);
        }

        if (a3 != NULL) {
            // TODO: Get rid of cast.
            animationRegisterCallback3(a1, a2, a3, (AnimationCallback3*)_obj_use_item_on, -1);
        } else {
            animationRegisterCallback(a1, a2, (AnimationCallback*)_obj_use, -1);
        }

        if (a2a != 0) {
            animationRegisterTakeOutWeapon(a1, a2a, -1);
        }

        return reg_anim_end();
    }

    return _action_climb_ladder(a1, a2);
}

// 0x412114
int _action_use_an_object(Object* a1, Object* a2)
{
    return _action_use_an_item_on_object(a1, a2, NULL);
}

// 0x412134
int actionPickUp(Object* critter, Object* item)
{
    if (FID_TYPE(item->fid) != OBJ_TYPE_ITEM) {
        return -1;
    }

    if (critter == gDude) {
        int animationCode = FID_ANIM_TYPE(gDude->fid);
        if (animationCode == ANIM_WALK || animationCode == ANIM_RUNNING) {
            reg_anim_clear(gDude);
        }
    }

    if (isInCombat()) {
        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        animationRegisterMoveToObject(critter, item, critter->data.critter.combat.ap, 0);
    } else {
        reg_anim_begin(critter == gDude ? ANIMATION_REQUEST_RESERVED : ANIMATION_REQUEST_UNRESERVED);
        if (objectGetDistanceBetween(critter, item) >= 5) {
            animationRegisterRunToObject(critter, item, -1, 0);
        } else {
            animationRegisterMoveToObject(critter, item, -1, 0);
        }
    }

    animationRegisterCallbackForced(critter, item, (AnimationCallback*)_is_next_to, -1);
    animationRegisterCallback(critter, item, (AnimationCallback*)_check_scenery_ap_cost, -1);

    Proto* itemProto;
    protoGetProto(item->pid, &itemProto);

    if (itemProto->item.type != ITEM_TYPE_CONTAINER || _proto_action_can_pickup(item->pid)) {
        animationRegisterAnimate(critter, ANIM_MAGIC_HANDS_GROUND, 0);

        int fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, ANIM_MAGIC_HANDS_GROUND, (critter->fid & 0xF000) >> 12, critter->rotation + 1);

        int actionFrame;
        CacheEntry* cacheEntry;
        Art* art = artLock(fid, &cacheEntry);
        if (art != NULL) {
            actionFrame = artGetActionFrame(art);
        } else {
            actionFrame = -1;
        }

        char sfx[16];
        if (artCopyFileName(FID_TYPE(item->fid), item->fid & 0xFFF, sfx) == 0) {
            // NOTE: looks like they copy sfx one more time, what for?
            animationRegisterPlaySoundEffect(item, sfx, actionFrame);
        }

        animationRegisterCallback(critter, item, (AnimationCallback*)_obj_pickup, actionFrame);
    } else {
        int weaponAnimationCode = (critter->fid & 0xF000) >> 12;
        if (weaponAnimationCode != 0) {
            const char* sfx = sfxBuildCharName(critter, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
            animationRegisterPlaySoundEffect(critter, sfx, -1);
            animationRegisterAnimate(critter, ANIM_PUT_AWAY, -1);
        }

        // ground vs middle animation
        int anim = (itemProto->item.data.container.openFlags & 0x01) == 0
            ? ANIM_MAGIC_HANDS_MIDDLE
            : ANIM_MAGIC_HANDS_GROUND;
        animationRegisterAnimate(critter, anim, 0);

        int fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, anim, 0, critter->rotation + 1);

        int actionFrame;
        CacheEntry* cacheEntry;
        Art* art = artLock(fid, &cacheEntry);
        if (art == NULL) {
            actionFrame = artGetActionFrame(art);
            artUnlock(cacheEntry);
        } else {
            actionFrame = -1;
        }

        if (item->frame != 1) {
            animationRegisterCallback(critter, item, (AnimationCallback*)_obj_use_container, actionFrame);
        }

        if (weaponAnimationCode != 0) {
            animationRegisterTakeOutWeapon(critter, weaponAnimationCode, -1);
        }

        if (item->frame == 0 || item->frame == 1) {
            animationRegisterCallback(critter, item, (AnimationCallback*)scriptsRequestLooting, -1);
        }
    }

    return reg_anim_end();
}

// TODO: Looks like the name is a little misleading, container can only be a
// critter, which is enforced by this function as well as at the call sites.
// Used to loot corpses, so probably should be something like actionLootCorpse.
// Check if it can be called with a living critter.
//
// 0x4123E8
int _action_loot_container(Object* critter, Object* container)
{
    if (FID_TYPE(container->fid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    // SFALL: Fix for trying to loot corpses with the "NoSteal" flag.
    if (_critter_flag_check(container->pid, CRITTER_NO_STEAL)) {
        return -1;
    }

    if (critter == gDude) {
        int anim = FID_ANIM_TYPE(gDude->fid);
        if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
            reg_anim_clear(gDude);
        }
    }

    if (isInCombat()) {
        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        animationRegisterMoveToObject(critter, container, critter->data.critter.combat.ap, 0);
    } else {
        reg_anim_begin(critter == gDude ? ANIMATION_REQUEST_RESERVED : ANIMATION_REQUEST_UNRESERVED);

        if (objectGetDistanceBetween(critter, container) < 5) {
            animationRegisterMoveToObject(critter, container, -1, 0);
        } else {
            animationRegisterRunToObject(critter, container, -1, 0);
        }
    }

    animationRegisterCallbackForced(critter, container, (AnimationCallback*)_is_next_to, -1);
    animationRegisterCallback(critter, container, (AnimationCallback*)_check_scenery_ap_cost, -1);
    animationRegisterCallback(critter, container, (AnimationCallback*)scriptsRequestLooting, -1);
    return reg_anim_end();
}

// 0x4124E0
int _action_skill_use(int skill)
{
    if (skill == SKILL_SNEAK) {
        reg_anim_clear(gDude);
        dudeToggleState(DUDE_STATE_SNEAKING);
        return 0;
    }

    return -1;
}

// NOTE: Inlined.
//
// 0x412500
static int _action_use_skill_in_combat_error(Object* critter)
{
    MessageListItem messageListItem;

    if (critter == gDude) {
        messageListItem.num = 902;
        if (messageListGetItem(&gProtoMessageList, &messageListItem) == 1) {
            displayMonitorAddMessage(messageListItem.text);
        }
    }

    return -1;
}

// skill_use
// 0x41255C
int actionUseSkill(Object* a1, Object* a2, int skill)
{
    switch (skill) {
    case SKILL_FIRST_AID:
    case SKILL_DOCTOR:
        if (isInCombat()) {
            // NOTE: Uninline.
            return _action_use_skill_in_combat_error(a1);
        }

        if (PID_TYPE(a2->pid) != OBJ_TYPE_CRITTER) {
            return -1;
        }
        break;
    case SKILL_LOCKPICK:
        if (isInCombat()) {
            // NOTE: Uninline.
            return _action_use_skill_in_combat_error(a1);
        }

        if (PID_TYPE(a2->pid) != OBJ_TYPE_ITEM && PID_TYPE(a2->pid) != OBJ_TYPE_SCENERY) {
            return -1;
        }

        break;
    case SKILL_STEAL:
        if (isInCombat()) {
            // NOTE: Uninline.
            return _action_use_skill_in_combat_error(a1);
        }

        if (PID_TYPE(a2->pid) != OBJ_TYPE_ITEM && PID_TYPE(a2->pid) != OBJ_TYPE_CRITTER) {
            return -1;
        }

        if (a2 == a1) {
            return -1;
        }

        break;
    case SKILL_TRAPS:
        if (isInCombat()) {
            // NOTE: Uninline.
            return _action_use_skill_in_combat_error(a1);
        }

        if (PID_TYPE(a2->pid) == OBJ_TYPE_CRITTER) {
            return -1;
        }

        break;
    case SKILL_SCIENCE:
    case SKILL_REPAIR:
        if (isInCombat()) {
            // NOTE: Uninline.
            return _action_use_skill_in_combat_error(a1);
        }

        if (PID_TYPE(a2->pid) != OBJ_TYPE_CRITTER) {
            break;
        }

        if (critterGetKillType(a2) == KILL_TYPE_ROBOT) {
            break;
        }

        if (critterGetKillType(a2) == KILL_TYPE_BRAHMIN
            && skill == SKILL_SCIENCE) {
            break;
        }

        // SFALL: Science on critters patch.
        if (1) {
            int targetType = SCIENCE_REPAIR_TARGET_TYPE_DEFAULT;
            configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_SCIENCE_REPAIR_TARGET_TYPE_KEY, &targetType);
            if (targetType == SCIENCE_REPAIR_TARGET_TYPE_DUDE) {
                if (a2 == gDude) {
                    break;
                }
            } else if (targetType == SCIENCE_REPAIR_TARGET_TYPE_ANYONE) {
                break;
            }
        }

        return -1;
    case SKILL_SNEAK:
        dudeToggleState(0);
        return 0;
    default:
        debugPrint("\nskill_use: invalid skill used.");
    }

    // Performer is either dude, or party member who's best at the specified
    // skill in entire party, and this skill is his/her own best.
    Object* performer = gDude;

    if (a1 == gDude) {
        Object* partyMember = partyMemberGetBestInSkill(skill);

        if (partyMember == gDude) {
            partyMember = NULL;
        }

        // Only dude can perform stealing.
        if (skill == SKILL_STEAL) {
            partyMember = NULL;
        }

        if (partyMember != NULL) {
            if (partyMemberGetBestSkill(partyMember) != skill) {
                partyMember = NULL;
            }
        }

        if (partyMember != NULL) {
            performer = partyMember;
            int anim = FID_ANIM_TYPE(partyMember->fid);
            if (anim != ANIM_WALK && anim != ANIM_RUNNING) {
                if (anim != ANIM_STAND) {
                    performer = gDude;
                    partyMember = NULL;
                }
            } else {
                reg_anim_clear(partyMember);
            }
        }

        if (partyMember != NULL) {
            bool v32 = false;
            if (objectGetDistanceBetween(gDude, a2) <= 1) {
                v32 = true;
            }

            char* msg = skillsGetGenericResponse(partyMember, v32);

            Rect rect;
            if (textObjectAdd(partyMember, msg, 101, _colorTable[32747], _colorTable[0], &rect) == 0) {
                tileWindowRefreshRect(&rect, gElevation);
            }

            if (v32) {
                performer = gDude;
                partyMember = NULL;
            }
        }

        if (partyMember == NULL) {
            int anim = FID_ANIM_TYPE(performer->fid);
            if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
                reg_anim_clear(performer);
            }
        }
    }

    if (isInCombat()) {
        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        animationRegisterMoveToObject(performer, a2, performer->data.critter.combat.ap, 0);
    } else {
        reg_anim_begin(a1 == gDude ? ANIMATION_REQUEST_RESERVED : ANIMATION_REQUEST_UNRESERVED);
        if (a2 != gDude) {
            if (objectGetDistanceBetween(performer, a2) >= 5) {
                animationRegisterRunToObject(performer, a2, -1, 0);
            } else {
                animationRegisterMoveToObject(performer, a2, -1, 0);
            }
        }
    }

    animationRegisterCallbackForced(performer, a2, (AnimationCallback*)_is_next_to, -1);

    int anim = (FID_TYPE(a2->fid) == OBJ_TYPE_CRITTER && _critter_is_prone(a2)) ? ANIM_MAGIC_HANDS_GROUND : ANIM_MAGIC_HANDS_MIDDLE;
    int fid = buildFid(OBJ_TYPE_CRITTER, performer->fid & 0xFFF, anim, 0, performer->rotation + 1);

    CacheEntry* artHandle;
    Art* art = artLock(fid, &artHandle);
    if (art != NULL) {
        artGetActionFrame(art);
        artUnlock(artHandle);
    }

    animationRegisterAnimate(performer, anim, -1);
    // TODO: Get rid of casts.
    animationRegisterCallback3(performer, a2, (void*)skill, (AnimationCallback3*)_obj_use_skill_on, -1);
    return reg_anim_end();
}

// 0x412BC4
bool _is_hit_from_front(Object* a1, Object* a2)
{
    int diff = a1->rotation - a2->rotation;
    if (diff < 0) {
        diff = -diff;
    }

    return diff != 0 && diff != 1 && diff != 5;
}

// 0x412BEC
bool _can_see(Object* a1, Object* a2)
{
    int diff;

    diff = a1->rotation - tileGetRotationTo(a1->tile, a2->tile);
    if (diff < 0) {
        diff = -diff;
    }

    return diff == 0 || diff == 1 || diff == 5;
}

// looks like it tries to change fall animation depending on object's current rotation
// 0x412C1C
int _pick_fall(Object* obj, int anim)
{
    int i;
    int rotation;
    int tile_num;
    int fid;

    if (anim == ANIM_FALL_FRONT) {
        rotation = obj->rotation;
        for (i = 1; i < 3; i++) {
            tile_num = tileGetTileInDirection(obj->tile, rotation, i);
            if (_obj_blocking_at(obj, tile_num, obj->elevation) != NULL) {
                anim = ANIM_FALL_BACK;
                break;
            }
        }
    } else if (anim == ANIM_FALL_BACK) {
        rotation = (obj->rotation + 3) % ROTATION_COUNT;
        for (i = 1; i < 3; i++) {
            tile_num = tileGetTileInDirection(obj->tile, rotation, i);
            if (_obj_blocking_at(obj, tile_num, obj->elevation) != NULL) {
                anim = ANIM_FALL_FRONT;
                break;
            }
        }
    }

    if (anim == ANIM_FALL_FRONT) {
        fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, ANIM_FALL_FRONT, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (!artExists(fid)) {
            anim = ANIM_FALL_BACK;
        }
    }

    return anim;
}

// 0x412CE4
bool _action_explode_running()
{
    return _action_in_explode != 0;
}

// action_explode
// 0x412CF4
int actionExplode(int tile, int elevation, int minDamage, int maxDamage, Object* a5, bool a6)
{
    if (a6 && _action_in_explode) {
        return -2;
    }

    Attack* attack = (Attack*)internal_malloc(sizeof(*attack));
    if (attack == NULL) {
        return -1;
    }

    Object* explosion;
    int fid = buildFid(OBJ_TYPE_MISC, 10, 0, 0, 0);
    if (objectCreateWithFidPid(&explosion, fid, -1) == -1) {
        internal_free(attack);
        return -1;
    }

    objectHide(explosion, NULL);
    explosion->flags |= OBJECT_TEMPORARY;

    objectSetLocation(explosion, tile, elevation, NULL);

    Object* adjacentExplosions[ROTATION_COUNT];
    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int fid = buildFid(OBJ_TYPE_MISC, 10, 0, 0, 0);
        if (objectCreateWithFidPid(&(adjacentExplosions[rotation]), fid, -1) == -1) {
            while (--rotation >= 0) {
                objectDestroy(adjacentExplosions[rotation], NULL);
            }

            objectDestroy(explosion, NULL);
            internal_free(attack);
            return -1;
        }

        objectHide(adjacentExplosions[rotation], NULL);
        adjacentExplosions[rotation]->flags |= OBJECT_TEMPORARY;

        int adjacentTile = tileGetTileInDirection(tile, rotation, 1);
        objectSetLocation(adjacentExplosions[rotation], adjacentTile, elevation, NULL);
    }

    Object* critter = _obj_blocking_at(NULL, tile, elevation);
    if (critter != NULL) {
        if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER || (critter->data.critter.combat.results & DAM_DEAD) != 0) {
            critter = NULL;
        }
    }

    attackInit(attack, explosion, critter, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);

    attack->tile = tile;
    attack->attackerFlags = DAM_HIT;

    gameUiDisable(1);

    if (critter != NULL) {
        if (reg_anim_clear(critter) == -2) {
            debugPrint("Cannot clear target's animation for action_explode!\n");
        }
        attack->defenderDamage = _compute_explosion_damage(minDamage, maxDamage, critter, &(attack->defenderKnockback));
    }

    _compute_explosion_on_extras(attack, 0, 0, 1);

    for (int index = 0; index < attack->extrasLength; index++) {
        Object* critter = attack->extras[index];
        if (reg_anim_clear(critter) == -2) {
            debugPrint("Cannot clear extra's animation for action_explode!\n");
        }

        attack->extrasDamage[index] = _compute_explosion_damage(minDamage, maxDamage, critter, &(attack->extrasKnockback[index]));
    }

    attackComputeDeathFlags(attack);

    if (a6) {
        _action_in_explode = 1;

        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        _register_priority(1);
        animationRegisterPlaySoundEffect(explosion, "whn1xxx1", 0);
        animationRegisterUnsetFlag(explosion, OBJECT_HIDDEN, 0);
        animationRegisterAnimateAndHide(explosion, ANIM_STAND, 0);
        _show_damage(attack, 0, 1);

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            animationRegisterUnsetFlag(adjacentExplosions[rotation], OBJECT_HIDDEN, 0);
            animationRegisterAnimateAndHide(adjacentExplosions[rotation], ANIM_STAND, 0);
        }

        animationRegisterCallbackForced(explosion, 0, (AnimationCallback*)_combat_explode_scenery, -1);
        animationRegisterHideObjectForced(explosion);

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            animationRegisterHideObjectForced(adjacentExplosions[rotation]);
        }

        animationRegisterCallbackForced(attack, a5, (AnimationCallback*)_report_explosion, -1);
        animationRegisterCallbackForced(NULL, NULL, (AnimationCallback*)_finished_explosion, -1);
        if (reg_anim_end() == -1) {
            _action_in_explode = 0;

            objectDestroy(explosion, NULL);

            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                objectDestroy(adjacentExplosions[rotation], NULL);
            }

            internal_free(attack);

            gameUiEnable();
            return -1;
        }

        _show_damage_extras(attack);
    } else {
        if (critter != NULL) {
            if ((attack->defenderFlags & DAM_DEAD) != 0) {
                critterKill(critter, -1, false);
            }
        }

        for (int index = 0; index < attack->extrasLength; index++) {
            if ((attack->extrasFlags[index] & DAM_DEAD) != 0) {
                critterKill(attack->extras[index], -1, false);
            }
        }

        _report_explosion(attack, a5);

        _combat_explode_scenery(explosion, NULL);

        objectDestroy(explosion, NULL);

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            objectDestroy(adjacentExplosions[rotation], NULL);
        }
    }

    return 0;
}

// 0x413144
int _report_explosion(Attack* attack, Object* a2)
{
    bool mainTargetWasDead;
    if (attack->defender != NULL) {
        mainTargetWasDead = (attack->defender->data.critter.combat.results & DAM_DEAD) != 0;
    } else {
        mainTargetWasDead = false;
    }

    bool extrasWasDead[6];
    for (int index = 0; index < attack->extrasLength; index++) {
        extrasWasDead[index] = (attack->extras[index]->data.critter.combat.results & DAM_DEAD) != 0;
    }

    attackComputeDeathFlags(attack);
    _combat_display(attack);
    _apply_damage(attack, false);

    Object* anyDefender = NULL;
    int xp = 0;
    if (a2 != NULL) {
        if (attack->defender != NULL && attack->defender != a2) {
            if ((attack->defender->data.critter.combat.results & DAM_DEAD) != 0) {
                if (a2 == gDude && !mainTargetWasDead) {
                    xp += critterGetExp(attack->defender);
                }
            } else {
                _critter_set_who_hit_me(attack->defender, a2);
                anyDefender = attack->defender;
            }
        }

        for (int index = 0; index < attack->extrasLength; index++) {
            Object* critter = attack->extras[index];
            if (critter != a2) {
                if ((critter->data.critter.combat.results & DAM_DEAD) != 0) {
                    if (a2 == gDude && !extrasWasDead[index]) {
                        xp += critterGetExp(critter);
                    }
                } else {
                    _critter_set_who_hit_me(critter, a2);

                    if (anyDefender == NULL) {
                        anyDefender = critter;
                    }
                }
            }
        }

        if (anyDefender != NULL) {
            if (!isInCombat()) {
                STRUCT_664980 combat;
                combat.attacker = anyDefender;
                combat.defender = a2;
                combat.actionPointsBonus = 0;
                combat.accuracyBonus = 0;
                combat.damageBonus = 0;
                combat.minDamage = 0;
                combat.maxDamage = INT_MAX;
                combat.field_1C = 0;
                scriptsRequestCombat(&combat);
            }
        }
    }

    internal_free(attack);
    gameUiEnable();

    if (a2 == gDude) {
        _combat_give_exps(xp);
    }

    return 0;
}

// 0x4132C0
int _finished_explosion(Object* a1, Object* a2)
{
    _action_in_explode = 0;
    return 0;
}

// calculate explosion damage applying threshold and resistances
// 0x4132CC
int _compute_explosion_damage(int min, int max, Object* a3, int* a4)
{
    int v5 = randomBetween(min, max);
    int v7 = v5 - critterGetStat(a3, STAT_DAMAGE_THRESHOLD_EXPLOSION);
    if (v7 > 0) {
        v7 -= critterGetStat(a3, STAT_DAMAGE_RESISTANCE_EXPLOSION) * v7 / 100;
    }

    if (v7 < 0) {
        v7 = 0;
    }

    if (a4 != NULL) {
        if ((a3->flags & OBJECT_MULTIHEX) == 0) {
            *a4 = v7 / 10;
        }
    }

    return v7;
}

// 0x413330
int actionTalk(Object* a1, Object* a2)
{
    if (a1 != gDude) {
        return -1;
    }

    if (FID_TYPE(a2->fid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    int anim = FID_ANIM_TYPE(gDude->fid);
    if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
        reg_anim_clear(gDude);
    }

    if (isInCombat()) {
        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        animationRegisterMoveToObject(a1, a2, a1->data.critter.combat.ap, 0);
    } else {
        reg_anim_begin(a1 == gDude ? ANIMATION_REQUEST_RESERVED : ANIMATION_REQUEST_UNRESERVED);

        if (objectGetDistanceBetween(a1, a2) >= 9 || _combat_is_shot_blocked(a1, a1->tile, a2->tile, a2, NULL)) {
            animationRegisterRunToObject(a1, a2, -1, 0);
        }
    }

    animationRegisterCallbackForced(a1, a2, (AnimationCallback*)_can_talk_to, -1);
    animationRegisterCallback(a1, a2, (AnimationCallback*)_talk_to, -1);
    return reg_anim_end();
}

// 0x413420
int _can_talk_to(Object* a1, Object* a2)
{
    if (_combat_is_shot_blocked(a1, a1->tile, a2->tile, a2, NULL) || objectGetDistanceBetween(a1, a2) >= 9) {
        if (a1 == gDude) {
            // You cannot get there. (used in actions.c)
            MessageListItem messageListItem;
            messageListItem.num = 2000;
            if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }

        return -1;
    }

    return 0;
}

// 0x413488
int _talk_to(Object* a1, Object* a2)
{
    scriptsRequestDialog(a2);
    return 0;
}

// 0x413494
void actionDamage(int tile, int elevation, int minDamage, int maxDamage, int damageType, bool animated, bool bypassArmor)
{
    Attack* attack = (Attack*)internal_malloc(sizeof(*attack));
    if (attack == NULL) {
        return;
    }

    Object* attacker;
    if (objectCreateWithFidPid(&attacker, FID_0x20001F5, -1) == -1) {
        internal_free(attack);
        return;
    }

    objectHide(attacker, NULL);

    attacker->flags |= OBJECT_TEMPORARY;

    objectSetLocation(attacker, tile, elevation, NULL);

    Object* defender = _obj_blocking_at(NULL, tile, elevation);
    attackInit(attack, attacker, defender, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);
    attack->tile = tile;
    attack->attackerFlags = DAM_HIT;
    gameUiDisable(1);

    if (defender != NULL) {
        reg_anim_clear(defender);

        int damage;
        if (bypassArmor) {
            damage = maxDamage;
        } else {
            damage = _compute_dmg_damage(minDamage, maxDamage, defender, &(attack->defenderKnockback), damageType);
        }

        attack->defenderDamage = damage;
    }

    attackComputeDeathFlags(attack);

    if (animated) {
        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        animationRegisterPlaySoundEffect(attacker, "whc1xxx1", 0);
        _show_damage(attack, gMaximumBloodDeathAnimations[damageType], 0);
        animationRegisterCallbackForced(attack, NULL, (AnimationCallback*)_report_dmg, 0);
        animationRegisterHideObjectForced(attacker);

        if (reg_anim_end() == -1) {
            objectDestroy(attacker, NULL);
            internal_free(attack);
            return;
        }
    } else {
        if (defender != NULL) {
            if ((attack->defenderFlags & DAM_DEAD) != 0) {
                critterKill(defender, -1, 1);
            }
        }

        // NOTE: Uninline.
        _report_dmg(attack, NULL);

        objectDestroy(attacker, NULL);
    }

    gameUiEnable();
}

// 0x41363C
int _report_dmg(Attack* attack, Object* a2)
{
    _combat_display(attack);
    _apply_damage(attack, false);
    internal_free(attack);
    gameUiEnable();
    return 0;
}

// Calculate damage by applying threshold and resistances.
//
// 0x413660
int _compute_dmg_damage(int min, int max, Object* obj, int* a4, int damageType)
{
    if (!_critter_flag_check(obj->pid, CRITTER_NO_KNOCKBACK)) {
        a4 = NULL;
    }

    int v8 = randomBetween(min, max);
    int v10 = v8 - critterGetStat(obj, STAT_DAMAGE_THRESHOLD + damageType);
    if (v10 > 0) {
        v10 -= critterGetStat(obj, STAT_DAMAGE_RESISTANCE + damageType) * v10 / 100;
    }

    if (v10 < 0) {
        v10 = 0;
    }

    if (a4 != NULL) {
        if ((obj->flags & OBJECT_MULTIHEX) == 0 && damageType != DAMAGE_TYPE_ELECTRICAL) {
            *a4 = v10 / 10;
        }
    }

    return v10;
}

// 0x4136EC
bool actionCheckPush(Object* a1, Object* a2)
{
    // Cannot push anything but critters.
    if (FID_TYPE(a2->fid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    // Cannot push itself.
    if (a1 == a2) {
        return false;
    }

    // Cannot push inactive critters.
    if (!critterIsActive(a2)) {
        return false;
    }

    if (_action_can_talk_to(a1, a2) != 0) {
        return false;
    }

    // Can only push critters that have push handler.
    if (!scriptHasProc(a2->sid, SCRIPT_PROC_PUSH)) {
        return false;
    }

    if (isInCombat()) {
        if (a2->data.critter.combat.team == a1->data.critter.combat.team
            && a2 == a1->data.critter.combat.whoHitMe) {
            return false;
        }

        // TODO: Check.
        Object* whoHitMe = a2->data.critter.combat.whoHitMe;
        if (whoHitMe != NULL
            && whoHitMe->data.critter.combat.team == a1->data.critter.combat.team) {
            return false;
        }
    }

    return true;
}

// 0x413790
int actionPush(Object* a1, Object* a2)
{
    if (!actionCheckPush(a1, a2)) {
        return -1;
    }

    int sid;
    if (_obj_sid(a2, &sid) == 0) {
        scriptSetObjects(sid, a1, a2);
        scriptExecProc(sid, SCRIPT_PROC_PUSH);

        bool scriptOverrides = false;

        Script* scr;
        if (scriptGetScript(sid, &scr) != -1) {
            scriptOverrides = scr->scriptOverrides;
        }

        if (scriptOverrides) {
            return -1;
        }
    }

    int rotation = tileGetRotationTo(a1->tile, a2->tile);
    int tile;
    do {
        tile = tileGetTileInDirection(a2->tile, rotation, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == NULL) {
            break;
        }

        tile = tileGetTileInDirection(a2->tile, (rotation + 1) % ROTATION_COUNT, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == NULL) {
            break;
        }

        tile = tileGetTileInDirection(a2->tile, (rotation + 5) % ROTATION_COUNT, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == NULL) {
            break;
        }

        tile = tileGetTileInDirection(a2->tile, (rotation + 2) % ROTATION_COUNT, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == NULL) {
            break;
        }

        tile = tileGetTileInDirection(a2->tile, (rotation + 4) % ROTATION_COUNT, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == NULL) {
            break;
        }

        tile = tileGetTileInDirection(a2->tile, (rotation + 3) % ROTATION_COUNT, 1);
        if (_obj_blocking_at(a2, tile, a2->elevation) == NULL) {
            break;
        }

        return -1;
    } while (0);

    int actionPoints;
    if (isInCombat()) {
        actionPoints = a2->data.critter.combat.ap;
    } else {
        actionPoints = -1;
    }

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);
    animationRegisterRotateToTile(a2, tile);
    animationRegisterMoveToTile(a2, tile, a2->elevation, actionPoints, 0);
    return reg_anim_end();
}

// Returns -1 if can't see there (can't find a path there)
// Returns -2 if it's too far (> 12 tiles).
//
// 0x413970
int _action_can_talk_to(Object* a1, Object* a2)
{
    if (pathfinderFindPath(a1, a1->tile, a2->tile, NULL, 0, _obj_sight_blocking_at) == 0) {
        return -1;
    }

    if (tileDistanceBetween(a1->tile, a2->tile) > 12) {
        return -2;
    }

    return 0;
}

} // namespace fallout
