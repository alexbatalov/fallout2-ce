#ifndef COMBAT_H
#define COMBAT_H

#include "combat_defs.h"
#include "db.h"
#include "obj_types.h"
#include "proto_types.h"

namespace fallout {

extern int _combatNumTurns;
extern unsigned int gCombatState;

extern int _combat_free_move;

int combatInit();
void combatReset();
void combatExit();
int _find_cid(int a1, int a2, Object** a3, int a4);
int combatLoad(File* stream);
int combatSave(File* stream);
bool _combat_safety_invalidate_weapon(Object* attacker, Object* weapon, int hitMode, Object* defender, int* safeDistancePtr);
bool _combatTestIncidentalHit(Object* attacker, Object* defender, Object* attackerFriend, Object* weapon);
Object* _combat_whose_turn();
void _combat_data_init(Object* obj);
Object* aiInfoGetFriendlyDead(Object* obj);
int aiInfoSetFriendlyDead(Object* a1, Object* a2);
Object* aiInfoGetLastTarget(Object* obj);
int aiInfoSetLastTarget(Object* a1, Object* a2);
Object* aiInfoGetLastItem(Object* obj);
int aiInfoSetLastItem(Object* obj, Object* a2);
void _combat_update_critter_outline_for_los(Object* critter, bool a2);
void _combat_over_from_load();
void _combat_give_exps(int exp_points);
void _combat_turn_run();
void _combat(CombatStartData* csd);
void attackInit(Attack* attack, Object* attacker, Object* defender, int hitMode, int hitLocation);
int _combat_attack(Object* attacker, Object* defender, int hitMode, int hitLocation);
int _combat_bullet_start(const Object* attacker, const Object* target);
void _compute_explosion_on_extras(Attack* attack, bool isFromAttacker, bool isGrenade, bool noDamage);
int _determine_to_hit(Object* a1, Object* a2, int hitLocation, int hitMode);
int _determine_to_hit_no_range(Object* attacker, Object* defender, int hitLocation, int hitMode, unsigned char* a5);
int _determine_to_hit_from_tile(Object* attacker, int tile, Object* defender, int hitLocation, int hitMode);
void attackComputeDeathFlags(Attack* attack);
void _apply_damage(Attack* attack, bool animated);
void _combat_display(Attack* attack);
void _combat_anim_begin();
void _combat_anim_finished();
int _combat_check_bad_shot(Object* attacker, Object* defender, int hitMode, bool aiming);
bool _combat_to_hit(Object* target, int* accuracy);
void _combat_attack_this(Object* target);
void _combat_outline_on();
void _combat_outline_off();
void _combat_highlight_change();
bool _combat_is_shot_blocked(Object* sourceObj, int from, int to, Object* targetObj, int* numCrittersOnLof);
int _combat_player_knocked_out_by();
int _combat_explode_scenery(Object* a1, Object* a2);
void _combat_delete_critter(Object* obj);
void _combatKillCritterOutsideCombat(Object* critter_obj, char* msg);

int combatGetTargetHighlight();
int criticalsGetValue(int killType, int hitLocation, int effect, int dataMember);
void criticalsSetValue(int killType, int hitLocation, int effect, int dataMember, int value);
void criticalsResetValue(int killType, int hitLocation, int effect, int dataMember);
int unarmedGetDamage(int hitMode, int* minDamagePtr, int* maxDamagePtr);
int unarmedGetBonusCriticalChance(int hitMode);
int unarmedGetActionPointCost(int hitMode);
bool unarmedIsPenetrating(int hitMode);
int unarmedGetPunchHitMode(bool isSecondary);
int unarmedGetKickHitMode(bool isSecondary);
bool unarmedIsPenetrating(int hitMode);
bool damageModGetBonusHthDamageFix();
bool damageModGetDisplayBonusDamage();
int combat_get_hit_location_penalty(int hit_location);
void combat_set_hit_location_penalty(int hit_location, int penalty);
void combat_reset_hit_location_penalty();
Attack* combat_get_data();

static inline bool isInCombat()
{
    return (gCombatState & COMBAT_STATE_0x01) != 0;
}

static inline bool isUnarmedHitMode(int hitMode)
{
    return hitMode == HIT_MODE_PUNCH
        || hitMode == HIT_MODE_KICK
        || (hitMode >= FIRST_ADVANCED_UNARMED_HIT_MODE && hitMode <= LAST_ADVANCED_UNARMED_HIT_MODE);
}

} // namespace fallout

#endif /* COMBAT_H */
