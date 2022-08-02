#ifndef COMBAT_H
#define COMBAT_H

#include "combat_defs.h"
#include "db.h"
#include "obj_types.h"
#include "proto_types.h"

extern int _combatNumTurns;
extern unsigned int gCombatState;

extern int _combat_free_move;

int combatInit();
void combatReset();
void combatExit();
int _find_cid(int a1, int a2, Object** a3, int a4);
int combatLoad(File* stream);
int combatSave(File* stream);
bool _combat_safety_invalidate_weapon(Object* a1, Object* a2, int hitMode, Object* a4, int* a5);
bool _combatTestIncidentalHit(Object* a1, Object* a2, Object* a3, Object* a4);
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
void _combat(STRUCT_664980* attack);
void attackInit(Attack* attack, Object* a2, Object* a3, int a4, int a5);
int _combat_attack(Object* a1, Object* a2, int a3, int a4);
int _combat_bullet_start(const Object* a1, const Object* a2);
void _compute_explosion_on_extras(Attack* attack, int a2, bool isGrenade, int a4);
int _determine_to_hit(Object* a1, Object* a2, int hitLocation, int hitMode);
int _determine_to_hit_no_range(Object* a1, Object* a2, int a3, int a4, unsigned char* a5);
int _determine_to_hit_from_tile(Object* a1, int a2, Object* a3, int a4, int a5);
void attackComputeDeathFlags(Attack* attack);
void _apply_damage(Attack* attack, bool animated);
void _combat_display(Attack* attack);
void _combat_anim_begin();
void _combat_anim_finished();
int _combat_check_bad_shot(Object* attacker, Object* defender, int hitMode, bool aiming);
bool _combat_to_hit(Object* target, int* accuracy);
void _combat_attack_this(Object* a1);
void _combat_outline_on();
void _combat_outline_off();
void _combat_highlight_change();
bool _combat_is_shot_blocked(Object* a1, int from, int to, Object* a4, int* a5);
int _combat_player_knocked_out_by();
int _combat_explode_scenery(Object* a1, Object* a2);
void _combat_delete_critter(Object* obj);
void _combatKillCritterOutsideCombat(Object* critter_obj, char* msg);

int criticalsGetValue(int killType, int hitLocation, int effect, int dataMember);
void criticalsSetValue(int killType, int hitLocation, int effect, int dataMember, int value);
void criticalsResetValue(int killType, int hitLocation, int effect, int dataMember);

static inline bool isInCombat()
{
    return (gCombatState & COMBAT_STATE_0x01) != 0;
}

#endif /* COMBAT_H */
