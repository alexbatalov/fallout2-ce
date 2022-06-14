#include "actions.h"

#include "animation.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "config.h"
#include "critter.h"
#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "game_config.h"
#include "game_sound.h"
#include "geometry.h"
#include "interface.h"
#include "item.h"
#include "map.h"
#include "memory.h"
#include "object.h"
#include "perk.h"
#include "proto.h"
#include "proto_instance.h"
#include "random.h"
#include "scripts.h"
#include "skill.h"
#include "stat.h"
#include "text_object.h"
#include "tile.h"
#include "trait.h"

#include <limits.h>
#include <string.h>

#define MAX_KNOCKDOWN_DISTANCE 20

// 0x5106D0
int _action_in_explode = 0;

// 0x5106E0
const int gNormalDeathAnimations[DAMAGE_TYPE_COUNT] = {
    ANIM_DANCING_AUTOFIRE,
    ANIM_SLICED_IN_HALF,
    ANIM_CHARRED_BODY,
    ANIM_CHARRED_BODY,
    ANIM_ELECTRIFY,
    ANIM_FALL_BACK,
    ANIM_BIG_HOLE,
};

// 0x5106FC
const int gMaximumBloodDeathAnimations[DAMAGE_TYPE_COUNT] = {
    ANIM_CHUNKS_OF_FLESH,
    ANIM_SLICED_IN_HALF,
    ANIM_FIRE_DANCE,
    ANIM_MELTED_TO_NOTHING,
    ANIM_ELECTRIFIED_TO_NOTHING,
    ANIM_FALL_BACK,
    ANIM_EXPLODED_TO_NOTHING,
};

// 0x410468
int actionKnockdown(Object* obj, int* anim, int maxDistance, int rotation, int delay)
{
    if (_critter_flag_check(obj->pid, 0x4000)) {
        return -1;
    }

    if (*anim == ANIM_FALL_FRONT) {
        int fid = buildFid(1, obj->fid & 0xFFF, *anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
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
    reg_anim_play_sfx(obj, soundEffectName, delay);

    // TODO: Check, probably step back because we've started with 1?
    distance--;

    if (distance <= 0) {
        tile = obj->tile;
        reg_anim_animate(obj, *anim, 0);
    } else {
        tile = tileGetTileInDirection(obj->tile, rotation, distance);
        reg_anim_knockdown(obj, tile, obj->elevation, *anim, 0);
    }

    return tile;
}

// 0x410568
int _action_blood(Object* obj, int anim, int delay)
{

    int violence_level = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violence_level);
    if (violence_level == VIOLENCE_LEVEL_NONE) {
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

    int fid = buildFid(1, obj->fid & 0xFFF, bloodyAnim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    if (artExists(fid)) {
        reg_anim_animate(obj, bloodyAnim, delay);
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

    int violenceLevel = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violenceLevel);

    if (_critter_flag_check(defender->pid, 0x1000)) {
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

    int violenceLevel = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violenceLevel);
    if (violenceLevel >= minViolenceLevel) {
        fid = buildFid(1, obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (artExists(fid)) {
            return anim;
        }
    }

    if (isFallingBack) {
        return ANIM_FALL_BACK;
    }

    fid = buildFid(1, obj->fid & 0xFFF, ANIM_FALL_FRONT, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
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

    if (_critter_flag_check(a1->pid, 0x4000)) {
        knockbackDistance = 0;
    }

    anim = (a1->fid & 0xFF0000) >> 16;
    if (!_critter_is_prone(a1)) {
        if ((flags & DAM_DEAD) != 0) {
            fid = buildFid(5, 10, 0, 0, 0);
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
                    reg_anim_play_sfx(a1, sfx_name, a10);

                    anim = _pick_fall(a1, anim);
                    reg_anim_animate(a1, anim, 0);

                    if (anim == ANIM_FALL_FRONT || anim == ANIM_FALL_BACK) {
                        anim = _action_blood(a1, anim, -1);
                    }
                }
            } else {
                fid = buildFid(1, a1->fid & 0xFFF, ANIM_FIRE_DANCE, (a1->fid & 0xF000) >> 12, a1->rotation + 1);
                if (artExists(fid)) {
                    sfx_name = sfxBuildCharName(a1, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                    reg_anim_play_sfx(a1, sfx_name, a10);

                    reg_anim_animate(a1, anim, 0);

                    int randomDistance = randomBetween(2, 5);
                    int randomRotation = randomBetween(0, 5);

                    while (randomDistance > 0) {
                        int tile = tileGetTileInDirection(a1->tile, randomRotation, randomDistance);
                        Object* v35 = NULL;
                        _make_straight_path(a1, a1->tile, tile, NULL, &v35, 4);
                        if (v35 == NULL) {
                            reg_anim_set_rotation_to_tile(a1, tile);
                            reg_anim_2(a1, tile, a1->elevation, anim, 0);
                            break;
                        }
                        randomDistance--;
                    }
                }

                anim = ANIM_BURNED_TO_NOTHING;
                sfx_name = sfxBuildCharName(a1, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                reg_anim_play_sfx(a1, sfx_name, -1);
                reg_anim_animate(a1, anim, 0);
            }
        } else {
            if ((flags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0) {
                anim = isFallingBack ? ANIM_FALL_BACK : ANIM_FALL_FRONT;
                sfx_name = sfxBuildCharName(a1, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                reg_anim_play_sfx(a1, sfx_name, a10);
                if (knockbackDistance != 0) {
                    actionKnockdown(a1, &anim, knockbackDistance, knockbackRotation, 0);
                } else {
                    anim = _pick_fall(a1, anim);
                    reg_anim_animate(a1, anim, 0);
                }
            } else if ((flags & DAM_ON_FIRE) != 0 && artExists(buildFid(1, a1->fid & 0xFFF, ANIM_FIRE_DANCE, (a1->fid & 0xF000) >> 12, a1->rotation + 1))) {
                reg_anim_animate(a1, ANIM_FIRE_DANCE, a10);

                fid = buildFid(1, a1->fid & 0xFFF, ANIM_STAND, (a1->fid & 0xF000) >> 12, a1->rotation + 1);
                reg_anim_17(a1, fid, -1);
            } else {
                if (knockbackDistance != 0) {
                    anim = isFallingBack ? ANIM_FALL_BACK : ANIM_FALL_FRONT;
                    actionKnockdown(a1, &anim, knockbackDistance, knockbackRotation, a10);
                    if (anim == ANIM_FALL_BACK) {
                        reg_anim_animate(a1, ANIM_BACK_TO_STANDING, -1);
                    } else {
                        reg_anim_animate(a1, ANIM_PRONE_TO_STANDING, -1);
                    }
                } else {
                    if (isFallingBack || !artExists(buildFid(1, a1->fid & 0xFFF, ANIM_HIT_FROM_BACK, (a1->fid & 0xF000) >> 12, a1->rotation + 1))) {
                        anim = ANIM_HIT_FROM_FRONT;
                    } else {
                        anim = ANIM_HIT_FROM_BACK;
                    }

                    sfx_name = sfxBuildCharName(a1, anim, CHARACTER_SOUND_EFFECT_UNUSED);
                    reg_anim_play_sfx(a1, sfx_name, a10);

                    reg_anim_animate(a1, anim, 0);
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
            reg_anim_11_1(a1, weapon, _obj_drop, -1);
            fid = buildFid(5, 10, 0, 0, 0);
            reg_anim_17(weapon, fid, 0);
            reg_anim_6(weapon, ANIM_STAND, 0);

            sfx_name = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_HIT, weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY, a1);
            reg_anim_play_sfx(weapon, sfx_name, 0);

            reg_anim_hide(weapon);
        } else if ((flags & DAM_DESTROY) != 0) {
            reg_anim_11_1(a1, weapon, _internal_destroy, -1);
        } else if ((flags & DAM_DROP) != 0) {
            reg_anim_11_1(a1, weapon, _obj_drop, -1);
        }
    }

    if ((flags & DAM_DEAD) != 0) {
        // TODO: Get rid of casts.
        reg_anim_11_1(a1, (Object*)anim, (AnimationProc*)_show_death, -1);
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
        fid = buildFid(1, obj->fid & 0xFFF, anim + 28, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
        if (objectSetFid(obj, fid, &v7) == 0) {
            rectUnion(&v8, &v7, &v8);
        }

        if (objectSetFrame(obj, 0, &v7) == 0) {
            rectUnion(&v8, &v7, &v8);
        }
    }

    if (_critter_flag_check(obj->pid, 2048) == 0) {
        obj->flags |= OBJECT_NO_BLOCK;
        if (_obj_toggle_flat(obj, &v7) == 0) {
            rectUnion(&v8, &v7, &v8);
        }
    }

    if (objectDisableOutline(obj, &v7) == 0) {
        rectUnion(&v8, &v7, &v8);
    }

    if (anim >= 30 && anim <= 31 && _critter_flag_check(obj->pid, 4096) == 0 && _critter_flag_check(obj->pid, 64) == 0) {
        _item_drop_all(obj, obj->tile);
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
        if ((obj->fid & 0xF000000) >> 24 == 1) {
            int delta = attack->attacker->rotation - obj->rotation;
            if (delta < 0) {
                delta = -delta;
            }

            v6 = delta != 0 && delta != 1 && delta != 5;
            reg_anim_begin(2);
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
        if ((object->fid & 0xF000000) >> 24 == OBJ_TYPE_CRITTER) {
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

            if ((attack->defender->fid & 0xF000000) >> 24 == OBJ_TYPE_CRITTER) {
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

    reg_anim_begin(2);
    _register_priority(1);

    fid = buildFid(1, attack->attacker->fid & 0xFFF, anim, (attack->attacker->fid & 0xF000) >> 12, attack->attacker->rotation + 1);
    art = artLock(fid, &cache_entry);
    if (art != NULL) {
        v17 = artGetActionFrame(art);
    } else {
        v17 = 0;
    }
    artUnlock(cache_entry);

    tileGetTileInDirection(attack->attacker->tile, attack->attacker->rotation, 1);
    reg_anim_set_rotation_to_tile(attack->attacker, attack->defender->tile);

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
        reg_anim_play_sfx(attack->attacker, sfx_name_temp, 0);
        if (anim != ANIM_THROW_PUNCH && anim != ANIM_KICK_LEG) {
            sfx_name = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_HIT, attack->weapon, attack->hitMode, attack->defender);
        } else {
            sfx_name = sfxBuildCharName(attack->attacker, anim, CHARACTER_SOUND_EFFECT_CONTACT);
        }

        strcpy(sfx_name_temp, sfx_name);

        reg_anim_animate(attack->attacker, anim, 0);
        reg_anim_play_sfx(attack->attacker, sfx_name_temp, v17);
        _show_damage(attack, anim, 0);
    } else {
        if (attack->defender->data.critter.combat.results & 0x03) {
            reg_anim_play_sfx(attack->attacker, sfx_name_temp, -1);
            reg_anim_animate(attack->attacker, anim, 0);
        } else {
            fid = buildFid(1, attack->defender->fid & 0xFFF, ANIM_DODGE_ANIM, (attack->defender->fid & 0xF000) >> 12, attack->defender->rotation + 1);
            art = artLock(fid, &cache_entry);
            if (art != NULL) {
                v18 = artGetActionFrame(art);
                artUnlock(cache_entry);

                if (v18 <= v17) {
                    reg_anim_play_sfx(attack->attacker, sfx_name_temp, -1);
                    reg_anim_animate(attack->attacker, anim, 0);

                    sfx_name = sfxBuildCharName(attack->defender, ANIM_DODGE_ANIM, CHARACTER_SOUND_EFFECT_UNUSED);
                    reg_anim_play_sfx(attack->defender, sfx_name, v17 - v18);
                    reg_anim_animate(attack->defender, 13, 0);
                } else {
                    sfx_name = sfxBuildCharName(attack->defender, ANIM_DODGE_ANIM, CHARACTER_SOUND_EFFECT_UNUSED);
                    reg_anim_play_sfx(attack->defender, sfx_name, -1);
                    reg_anim_animate(attack->defender, 13, 0);
                    reg_anim_play_sfx(attack->attacker, sfx_name_temp, v18 - v17);
                    reg_anim_animate(attack->attacker, anim, 0);
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

    reg_anim_begin(2);
    _register_priority(1);

    Object* projectile = NULL;
    Object* v50 = NULL;
    int weaponFid = -1;

    Proto* weaponProto;
    Object* weapon = attack->weapon;
    protoGetProto(weapon->pid, &weaponProto);

    int fid = buildFid(1, attack->attacker->fid & 0xFFF, anim, (attack->attacker->fid & 0xF000) >> 12, attack->attacker->rotation + 1);
    CacheEntry* artHandle;
    Art* art = artLock(fid, &artHandle);
    int actionFrame = (art != NULL) ? artGetActionFrame(art) : 0;
    artUnlock(artHandle);

    _item_w_range(attack->attacker, attack->hitMode);

    int damageType = weaponGetDamageType(attack->attacker, attack->weapon);

    tileGetTileInDirection(attack->attacker->tile, attack->attacker->rotation, 1);

    reg_anim_set_rotation_to_tile(attack->attacker, attack->defender->tile);

    bool isGrenade = false;
    if (anim == ANIM_THROW_ANIM) {
        if (damageType == DAMAGE_TYPE_EXPLOSION || damageType == DAMAGE_TYPE_PLASMA || damageType == DAMAGE_TYPE_EMP) {
            isGrenade = true;
        }
    } else {
        reg_anim_animate(attack->attacker, ANIM_POINT, -1);
    }

    _combatai_msg(attack->attacker, attack, AI_MESSAGE_TYPE_ATTACK, 0);

    const char* sfx;
    if (((attack->attacker->fid & 0xF000) >> 12) != 0) {
        sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_ATTACK, weapon, attack->hitMode, attack->defender);
    } else {
        sfx = sfxBuildCharName(attack->attacker, anim, CHARACTER_SOUND_EFFECT_UNUSED);
    }
    reg_anim_play_sfx(attack->attacker, sfx, -1);

    reg_anim_animate(attack->attacker, anim, 0);

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
                    v50 = _item_replace(attack->attacker, weapon, weaponFlags & OBJECT_IN_ANY_HAND);
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

                objectSetLight(projectile, 9, projectile->lightIntensity, NULL);

                int projectileOrigin = _combat_bullet_start(attack->attacker, attack->defender);
                objectSetLocation(projectile, projectileOrigin, attack->attacker->elevation, NULL);

                int projectileRotation = tileGetRotationTo(attack->attacker->tile, attack->defender->tile);
                objectSetRotation(projectile, projectileRotation, NULL);

                reg_anim_15(projectile, 1, actionFrame);

                const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_AMMO_FLYING, weapon, attack->hitMode, attack->defender);
                reg_anim_play_sfx(projectile, sfx, 0);

                int v24;
                if ((attack->attackerFlags & DAM_HIT) != 0) {
                    reg_anim_2(projectile, attack->defender->tile, attack->defender->elevation, ANIM_WALK, 0);
                    actionFrame = _make_straight_path(projectile, projectileOrigin, attack->defender->tile, NULL, NULL, 32) - 1;
                    v24 = attack->defender->tile;
                } else {
                    reg_anim_2(projectile, attack->tile, attack->defender->elevation, ANIM_WALK, 0);
                    actionFrame = 0;
                    v24 = attack->tile;
                }

                if (isGrenade || damageType == DAMAGE_TYPE_EXPLOSION) {
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

                        if (isGrenade) {
                            reg_anim_17(projectile, weaponFid, -1);
                        }

                        int explosionFid = buildFid(5, explosionFrmId, 0, 0, 0);
                        reg_anim_17(projectile, explosionFid, -1);

                        const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_HIT, weapon, attack->hitMode, attack->defender);
                        reg_anim_play_sfx(projectile, sfx, 0);

                        reg_anim_6(projectile, ANIM_STAND, 0);

                        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
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

                                reg_anim_15(neighboors[rotation], 1, delay);
                                reg_anim_6(neighboors[rotation], ANIM_STAND, 0);
                            }
                        }

                        l56 = true;
                    }
                } else {
                    if (anim != ANIM_THROW_ANIM) {
                        reg_anim_hide(projectile);
                    }
                }

                if (!l56) {
                    const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_HIT, weapon, attack->hitMode, attack->defender);
                    reg_anim_play_sfx(weapon, sfx, actionFrame);
                }

                actionFrame = 0;
            } else {
                if ((attack->attackerFlags & DAM_HIT) == 0) {
                    Object* defender = attack->defender;
                    if ((defender->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) == 0) {
                        reg_anim_animate(defender, ANIM_DODGE_ANIM, actionFrame);
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

    if (projectile != NULL && (isGrenade || damageType == DAMAGE_TYPE_EXPLOSION)) {
        reg_anim_hide(projectile);
    } else if (anim == ANIM_THROW_ANIM && projectile != NULL) {
        reg_anim_17(projectile, weaponFid, -1);
    }

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        if (neighboors[rotation] != NULL) {
            reg_anim_hide(neighboors[rotation]);
        }
    }

    if ((attack->attackerFlags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_DEAD)) == 0) {
        if (anim == ANIM_THROW_ANIM) {
            bool l9 = false;
            if (v50 != NULL) {
                int v38 = weaponGetAnimationCode(v50);
                if (v38 != 0) {
                    reg_anim_18(attack->attacker, v38, -1);
                    l9 = true;
                }
            }

            if (!l9) {
                int fid = buildFid(1, attack->attacker->fid & 0xFFF, ANIM_STAND, 0, attack->attacker->rotation + 1);
                reg_anim_17(attack->attacker, fid, -1);
            }
        } else {
            reg_anim_animate(attack->attacker, ANIM_UNPOINT, -1);
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
    int anim;
    int v5;
    int v6;
    int tile_num;
    int v11;
    const char* sfx_name;

    if (a1 == gDude) {
        anim = (gDude->fid & 0xFF0000) >> 16;
        if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
            reg_anim_clear(gDude);
        }
    }

    if (isInCombat()) {
        v5 = 2;
        v6 = a1->data.critter.combat.ap;
    } else {
        v5 = 1;
        v6 = -1;
    }

    if (a1 == gDude) {
        v5 = 2;
    }

    v5 |= 4;
    reg_anim_begin(v5);

    tile_num = tileGetTileInDirection(a2->tile, 2, 1);
    if (v6 != -1 || objectGetDistanceBetween(a1, a2) < 5) {
        reg_anim_obj_move_to_tile(a1, tile_num, a2->elevation, v6, 0);
    } else {
        reg_anim_obj_run_to_tile(a1, tile_num, a2->elevation, v6, 0);
    }

    reg_anim_11_1(a1, a2, _is_next_to, -1);
    reg_anim_set_rotation_to_tile(a1, a2->tile);
    reg_anim_11_1(a1, a2, _check_scenery_ap_cost, -1);

    v11 = (a1->fid & 0xF000) >> 12;
    if (v11 != 0) {
        sfx_name = sfxBuildCharName(a1, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
        reg_anim_play_sfx(a1, sfx_name, -1);
        reg_anim_animate(a1, 39, 0);
    }

    sfx_name = sfxBuildCharName(a1, ANIM_CLIMB_LADDER, CHARACTER_SOUND_EFFECT_UNUSED);
    reg_anim_play_sfx(a1, sfx_name, -1);
    reg_anim_animate(a1, 4, 0);
    reg_anim_11_0(a1, a2, _obj_use, -1);

    if (v11 != 0) {
        reg_anim_18(a1, v11, -1);
    }

    return reg_anim_end();
}

// 0x411F2C
int _action_use_an_item_on_object(Object* a1, Object* a2, Object* a3)
{
    Proto* proto = NULL;
    int type = (a2->fid & 0xF000000) >> 24;
    int sceneryType = -1;
    if (type == OBJ_TYPE_SCENERY) {
        if (protoGetProto(a2->pid, &proto) == -1) {
            return -1;
        }

        sceneryType = proto->scenery.type;
    }

    if (sceneryType != SCENERY_TYPE_LADDER_UP || a3 != NULL) {
        if (a1 == gDude) {
            int anim = (gDude->fid & 0xFF0000) >> 16;
            if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
                reg_anim_clear(gDude);
            }
        }

        int v9;
        int actionPoints;
        if (isInCombat()) {
            v9 = 2;
            actionPoints = a1->data.critter.combat.ap;
        } else {
            v9 = 1;
            actionPoints = -1;
        }

        if (a1 == gDude) {
            v9 = 2;
        }

        reg_anim_begin(v9);

        if (actionPoints != -1 || objectGetDistanceBetween(a1, a2) < 5) {
            reg_anim_obj_move_to_obj(a1, a2, actionPoints, 0);
        } else {
            reg_anim_obj_run_to_obj(a1, a2, -1, 0);
        }

        reg_anim_11_1(a1, a2, _is_next_to, -1);

        if (a3 == NULL) {
            reg_anim_11_0(a1, a2, _check_scenery_ap_cost, -1);
        }

        int a2a = (a1->fid & 0xF000) >> 12;
        if (a2a != 0) {
            const char* sfx = sfxBuildCharName(a1, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
            reg_anim_play_sfx(a1, sfx, -1);
            reg_anim_animate(a1, ANIM_PUT_AWAY, 0);
        }

        int v13;
        int v12 = (a2->fid & 0xF000000) >> 24;
        if (v12 == OBJ_TYPE_CRITTER && _critter_is_prone(a2)) {
            v13 = ANIM_MAGIC_HANDS_GROUND;
        } else if (v12 == OBJ_TYPE_SCENERY && (proto->scenery.extendedFlags & 0x01) != 0) {
            v13 = ANIM_MAGIC_HANDS_GROUND;
        } else {
            v13 = ANIM_MAGIC_HANDS_MIDDLE;
        }

        if (sceneryType != SCENERY_TYPE_STAIRS && a3 == NULL) {
            reg_anim_animate(a1, v13, -1);
        }

        if (a3 != NULL) {
            // TODO: Get rid of cast.
            reg_anim_12(a1, a2, a3, (AnimationProc2*)_obj_use_item_on, -1);
        } else {
            reg_anim_11_0(a1, a2, _obj_use, -1);
        }

        if (a2a != 0) {
            reg_anim_18(a1, a2a, -1);
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
    if (((item->fid & 0xF000000) >> 24) != OBJ_TYPE_ITEM) {
        return -1;
    }

    if (critter == gDude) {
        int animationCode = (gDude->fid & 0xFF0000) >> 16;
        if (animationCode == ANIM_WALK || animationCode == ANIM_RUNNING) {
            reg_anim_clear(gDude);
        }
    }

    if (isInCombat()) {
        reg_anim_begin(2);
        reg_anim_obj_move_to_obj(critter, item, critter->data.critter.combat.ap, 0);
    } else {
        int flags = (critter == gDude) ? 2 : 1;
        reg_anim_begin(flags);
        if (objectGetDistanceBetween(critter, item) >= 5) {
            reg_anim_obj_run_to_obj(critter, item, -1, 0);
        } else {
            reg_anim_obj_move_to_obj(critter, item, -1, 0);
        }
    }

    reg_anim_11_1(critter, item, _is_next_to, -1);
    reg_anim_11_0(critter, item, _check_scenery_ap_cost, -1);

    Proto* itemProto;
    protoGetProto(item->pid, &itemProto);

    if (itemProto->item.type != ITEM_TYPE_CONTAINER || _proto_action_can_pickup(item->pid)) {
        reg_anim_animate(critter, ANIM_MAGIC_HANDS_GROUND, 0);

        int fid = buildFid(1, critter->fid & 0xFFF, ANIM_MAGIC_HANDS_GROUND, (critter->fid & 0xF000) >> 12, critter->rotation + 1);

        int actionFrame;
        CacheEntry* cacheEntry;
        Art* art = artLock(fid, &cacheEntry);
        if (art != NULL) {
            actionFrame = artGetActionFrame(art);
        } else {
            actionFrame = -1;
        }

        char sfx[16];
        if (artCopyFileName((item->fid & 0xF000000) >> 24, item->fid & 0xFFF, sfx) == 0) {
            // NOTE: looks like they copy sfx one more time, what for?
            reg_anim_play_sfx(item, sfx, actionFrame);
        }

        reg_anim_11_0(critter, item, _obj_pickup, actionFrame);
    } else {
        int v27 = (critter->fid & 0xF000) >> 12;
        if (v27 != 0) {
            const char* sfx = sfxBuildCharName(critter, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
            reg_anim_play_sfx(critter, sfx, -1);
            reg_anim_animate(critter, ANIM_PUT_AWAY, -1);
        }

        // ground vs middle animation
        int animationCode = 10 + ((itemProto->item.data.container.openFlags & 0x01) == 0);
        reg_anim_animate(critter, animationCode, 0);

        int fid = buildFid(1, critter->fid & 0xFFF, animationCode, 0, critter->rotation + 1);

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
            reg_anim_11_0(critter, item, _obj_use_container, actionFrame);
        }

        if (v27 != 0) {
            reg_anim_18(critter, v27, -1);
        }

        if (item->frame == 0 || item->frame == 1) {
            reg_anim_11_0(critter, item, scriptsRequestLooting, -1);
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
    if ((container->fid & 0xF000000) >> 24 != OBJ_TYPE_CRITTER) {
        return -1;
    }

    if (critter == gDude) {
        int anim = (gDude->fid & 0xFF0000) >> 16;
        if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
            reg_anim_clear(gDude);
        }
    }

    if (isInCombat()) {
        reg_anim_begin(2);
        reg_anim_obj_move_to_obj(critter, container, critter->data.critter.combat.ap, 0);
    } else {
        reg_anim_begin(critter == gDude ? 2 : 1);

        if (objectGetDistanceBetween(critter, container) < 5) {
            reg_anim_obj_move_to_obj(critter, container, -1, 0);
        } else {
            reg_anim_obj_run_to_obj(critter, container, -1, 0);
        }
    }

    reg_anim_11_1(critter, container, _is_next_to, -1);
    reg_anim_11_0(critter, container, _check_scenery_ap_cost, -1);
    reg_anim_11_0(critter, container, scriptsRequestLooting, -1);
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

// skill_use
// 0x41255C
int actionUseSkill(Object* a1, Object* a2, int skill)
{
    MessageListItem messageListItem;

    switch (skill) {
    case SKILL_FIRST_AID:
    case SKILL_DOCTOR:
        if (isInCombat()) {
            if (a1 == gDude) {
                messageListItem.num = 902;
                if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                    displayMonitorAddMessage(messageListItem.text);
                }
            }
            return -1;
        }

        if ((a2->pid >> 24) != OBJ_TYPE_CRITTER) {
            return -1;
        }
        break;
    case SKILL_LOCKPICK:
        if (isInCombat()) {
            if (a1 == gDude) {
                messageListItem.num = 902;
                if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                    displayMonitorAddMessage(messageListItem.text);
                }
            }
            return -1;
        }

        if ((a2->pid >> 24) != OBJ_TYPE_ITEM && (a2->pid >> 24) != OBJ_TYPE_SCENERY) {
            return -1;
        }

        break;
    case SKILL_STEAL:
        if (isInCombat()) {
            if (a1 == gDude) {
                messageListItem.num = 902;
                if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                    displayMonitorAddMessage(messageListItem.text);
                }
            }
            return -1;
        }

        if ((a2->pid >> 24) != OBJ_TYPE_ITEM && (a2->pid >> 24) != OBJ_TYPE_CRITTER) {
            return -1;
        }

        if (a2 == a1) {
            return -1;
        }

        break;
    case SKILL_TRAPS:
        if (isInCombat()) {
            if (a1 == gDude) {
                messageListItem.num = 902;
                if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                    displayMonitorAddMessage(messageListItem.text);
                }
            }
            return -1;
        }

        if ((a2->pid >> 24) == OBJ_TYPE_CRITTER) {
            return -1;
        }

        break;
    case SKILL_SCIENCE:
    case SKILL_REPAIR:
        if (isInCombat()) {
            if (a1 == gDude) {
                messageListItem.num = 902;
                if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                    displayMonitorAddMessage(messageListItem.text);
                }
            }
            return -1;
        }

        if ((a2->pid >> 24) != OBJ_TYPE_CRITTER) {
            break;
        }

        if (critterGetKillType(a2) == KILL_TYPE_ROBOT) {
            break;
        }

        if (critterGetKillType(a2) == KILL_TYPE_BRAHMIN
            && skill == SKILL_SCIENCE) {
            break;
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
            int anim = (partyMember->fid & 0xFF0000) >> 16;
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
            int anim = (performer->fid & 0xFF0000) >> 16;
            if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
                reg_anim_clear(performer);
            }
        }
    }

    if (isInCombat()) {
        reg_anim_begin(2);
        reg_anim_obj_move_to_obj(performer, a2, performer->data.critter.combat.ap, 0);
    } else {
        reg_anim_begin(a1 == gDude ? 2 : 1);
        if (a2 != gDude) {
            if (objectGetDistanceBetween(performer, a2) >= 5) {
                reg_anim_obj_run_to_obj(performer, a2, -1, 0);
            } else {
                reg_anim_obj_move_to_obj(performer, a2, -1, 0);
            }
        }
    }

    reg_anim_11_1(performer, a2, _is_next_to, -1);

    int anim = (((a2->fid & 0xF000000) >> 24) == OBJ_TYPE_CRITTER && _critter_is_prone(a2)) ? ANIM_MAGIC_HANDS_GROUND : ANIM_MAGIC_HANDS_MIDDLE;
    int fid = buildFid(1, performer->fid & 0xFFF, anim, 0, performer->rotation + 1);

    CacheEntry* artHandle;
    Art* art = artLock(fid, &artHandle);
    if (art != NULL) {
        artGetActionFrame(art);
        artUnlock(artHandle);
    }

    reg_anim_animate(performer, anim, -1);
    // TODO: Get rid of casts.
    reg_anim_12(performer, a2, (void*)skill, (AnimationProc2*)_obj_use_skill_on, -1);
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
        fid = buildFid(1, obj->fid & 0xFFF, ANIM_FALL_FRONT, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
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
    int fid = buildFid(5, 10, 0, 0, 0);
    if (objectCreateWithFidPid(&explosion, fid, -1) == -1) {
        internal_free(attack);
        return -1;
    }

    objectHide(explosion, NULL);
    explosion->flags |= OBJECT_TEMPORARY;

    objectSetLocation(explosion, tile, elevation, NULL);

    Object* adjacentExplosions[ROTATION_COUNT];
    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int fid = buildFid(5, 10, 0, 0, 0);
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
        if ((critter->fid & 0xF000000) >> 24 != OBJ_TYPE_CRITTER || (critter->data.critter.combat.results & DAM_DEAD) != 0) {
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

        reg_anim_begin(2);
        _register_priority(1);
        reg_anim_play_sfx(explosion, "whn1xxx1", 0);
        reg_anim_15(explosion, 1, 0);
        reg_anim_6(explosion, ANIM_STAND, 0);
        _show_damage(attack, 0, 1);

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            reg_anim_15(adjacentExplosions[rotation], 1, 0);
            reg_anim_6(adjacentExplosions[rotation], ANIM_STAND, 0);
        }

        reg_anim_11_1(explosion, 0, _combat_explode_scenery, -1);
        reg_anim_hide(explosion);

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            reg_anim_hide(adjacentExplosions[rotation]);
        }

        // TODO: Get rid of casts.
        reg_anim_11_1((Object*)attack, a5, (AnimationProc*)_report_explosion, -1);
        reg_anim_11_1(NULL, NULL, _finished_explosion, -1);
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

    if ((a2->fid & 0xF000000) >> 24 != OBJ_TYPE_CRITTER) {
        return -1;
    }

    int anim = (gDude->fid & 0xFF0000) >> 16;
    if (anim == ANIM_WALK || anim == ANIM_RUNNING) {
        reg_anim_clear(gDude);
    }

    if (isInCombat()) {
        reg_anim_begin(2);
        reg_anim_obj_move_to_obj(a1, a2, a1->data.critter.combat.ap, 0);
    } else {
        reg_anim_begin((a1 == gDude) ? 2 : 1);

        if (objectGetDistanceBetween(a1, a2) >= 9 || _combat_is_shot_blocked(a1, a1->tile, a2->tile, a2, NULL)) {
            reg_anim_obj_run_to_obj(a1, a2, -1, 0);
        }
    }

    reg_anim_11_1(a1, a2, _can_talk_to, -1);
    reg_anim_11_0(a1, a2, _talk_to, -1);
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
void _action_dmg(int tile, int elevation, int minDamage, int maxDamage, int damageType, bool animated, bool bypassArmor)
{
    Attack* attack = (Attack*)internal_malloc(sizeof(*attack));
    if (attack == NULL) {
        return;
    }

    Object* obj;
    if (objectCreateWithFidPid(&obj, FID_0x20001F5, -1) == -1) {
        internal_free(attack);
        return;
    }

    objectHide(obj, NULL);

    obj->flags |= OBJECT_TEMPORARY;

    objectSetLocation(obj, tile, elevation, NULL);

    Object* v9 = _obj_blocking_at(NULL, tile, elevation);
    attackInit(attack, obj, v9, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);
    attack->tile = tile;
    attack->attackerFlags = DAM_HIT;
    gameUiDisable(1);

    if (v9 != NULL) {
        reg_anim_clear(v9);

        int damage;
        if (bypassArmor) {
            damage = maxDamage;
        } else {
            damage = _compute_dmg_damage(minDamage, maxDamage, v9, &(attack->defenderKnockback), damageType);
        }

        attack->defenderDamage = damage;
    }

    attackComputeDeathFlags(attack);

    if (animated) {
        reg_anim_begin(2);
        reg_anim_play_sfx(obj, "whc1xxx1", 0);
        _show_damage(attack, gMaximumBloodDeathAnimations[damageType], 0);
        // TODO: Get rid of casts.
        reg_anim_11_1((Object*)attack, NULL, (AnimationProc*)_report_dmg, 0);
        reg_anim_hide(obj);

        if (reg_anim_end() == -1) {
            objectDestroy(obj, NULL);
            internal_free(attack);
            return;
        }
    } else {
        if (v9 != NULL) {
            if ((attack->defenderFlags & DAM_DEAD) != 0) {
                critterKill(v9, -1, 1);
            }
        }

        _combat_display(attack);
        _apply_damage(attack, false);
        internal_free(attack);
        gameUiEnable();
        objectDestroy(obj, NULL);
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
    if (!_critter_flag_check(obj->pid, 0x4000)) {
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
    if ((a2->fid & 0xF000000) >> 24 != OBJ_TYPE_CRITTER) {
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

    reg_anim_begin(2);
    reg_anim_set_rotation_to_tile(a2, tile);
    reg_anim_obj_move_to_tile(a2, tile, a2->elevation, actionPoints, 0);
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
