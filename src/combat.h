#ifndef COMBAT_H
#define COMBAT_H

#include "animation.h"
#include "db.h"
#include "combat_defs.h"
#include "message.h"
#include "obj_types.h"
#include "party_member.h"
#include "proto_types.h"

#define CALLED_SHOW_WINDOW_X (68)
#define CALLED_SHOW_WINDOW_Y (20)
#define CALLED_SHOW_WINDOW_WIDTH (504)
#define CALLED_SHOW_WINDOW_HEIGHT (309)

extern char _a_1[];

extern int _combat_turn_running;
extern int _combatNumTurns;
extern unsigned int gCombatState;
extern STRUCT_510948* _aiInfoList;
extern STRUCT_664980* _gcsd;
extern bool _combat_call_display;
extern const int _hit_location_penalty[HIT_LOCATION_COUNT];
extern CriticalHitDescription gCriticalHitTables[KILL_TYPE_COUNT][HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];
extern CriticalHitDescription gPlayerCriticalHitTable[HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];
extern int _combat_end_due_to_load;
extern bool _combat_cleanup_enabled;
extern const int _cf_table[WEAPON_CRITICAL_FAILURE_TYPE_COUNT][WEAPON_CRITICAL_FAILURE_EFFECT_COUNT];
extern const int _call_ty[4];
extern const int _hit_loc_left[4];
extern const int _hit_loc_right[4];

extern Attack _main_ctd;
extern MessageList gCombatMessageList;
extern Object* gCalledShotCritter;
extern int gCalledShotWindow;
extern int _combat_elev;
extern int _list_total;
extern Object* _combat_ending_guy;
extern int _list_noncom;
extern Object* _combat_turn_obj;
extern int _combat_highlight;
extern Object** _combat_list;
extern int _list_com;
extern int _combat_exps;
extern int _combat_free_move;
extern Attack _shoot_ctd;
extern Attack _explosion_ctd;

int combatInit();
void combatReset();
void combatExit();
int _find_cid(int a1, int a2, Object** a3, int a4);
int combatLoad(File* stream);
int combatSave(File* stream);
bool _combat_safety_invalidate_weapon(Object* a1, Object* a2, int hitMode, Object* a4, int* a5);
bool _combat_safety_invalidate_weapon_func(Object* critter, Object* weapon, int hitMode, Object* a4, int* a5, Object* a6);
bool _combatTestIncidentalHit(Object* a1, Object* a2, Object* a3, Object* a4);
Object* _combat_whose_turn();
void _combat_data_init(Object* obj);
int _combatCopyAIInfo(int a1, int a2);
Object* _combatAIInfoGetFriendlyDead(Object* obj);
int _combatAIInfoSetFriendlyDead(Object* a1, Object* a2);
Object* _combatAIInfoGetLastTarget(Object* obj);
int _combatAIInfoSetLastTarget(Object* a1, Object* a2);
Object* _combatAIInfoGetLastItem(Object* obj);
int _combatAIInfoSetLastItem(Object* obj, Object* a2);
void _combat_begin(Object* a1);
void _combat_begin_extra(Object* a1);
void _combat_update_critter_outline_for_los(Object* critter, bool a2);
void _combat_over();
void _combat_over_from_load();
void _combat_give_exps(int exp_points);
void _combat_add_noncoms();
int _compare_faster(const void* a1, const void* a2);
void _combat_sequence_init(Object* a1, Object* a2);
void _combat_sequence();
void combatAttemptEnd();
void _combat_turn_run();
int _combat_input();
void _combat_set_move_all();
int _combat_turn(Object* a1, bool a2);
bool _combat_should_end();
void _combat(STRUCT_664980* attack);
void attackInit(Attack* attack, Object* a2, Object* a3, int a4, int a5);
int _combat_attack(Object* a1, Object* a2, int a3, int a4);
int _combat_bullet_start(const Object* a1, const Object* a2);
bool _check_ranged_miss(Attack* attack);
int _shoot_along_path(Attack* attack, int a2, int a3, int anim);
int _compute_spray(Attack* attack, int accuracy, int* a3, int* a4, int anim);
int attackComputeEnhancedKnockout(Attack* attack);
int attackCompute(Attack* attack);
void _compute_explosion_on_extras(Attack* attack, int a2, int a3, int a4);
int attackComputeCriticalHit(Attack* a1);
int _attackFindInvalidFlags(Object* a1, Object* a2);
int attackComputeCriticalFailure(Attack* attack);
int _determine_to_hit(Object* a1, Object* a2, int hitLocation, int hitMode);
int _determine_to_hit_no_range(Object* a1, Object* a2, int a3, int a4, unsigned char* a5);
int _determine_to_hit_from_tile(Object* a1, int a2, Object* a3, int a4, int a5);
int attackDetermineToHit(Object* attacker, int tile, Object* defender, int hitLocation, int hitMode, int a6);
void attackComputeDamage(Attack* attack, int ammoQuantity, int a3);
void attackComputeDeathFlags(Attack* attack);
void _apply_damage(Attack* attack, bool animated);
void _check_for_death(Object* a1, int a2, int* a3);
void _set_new_results(Object* a1, int a2);
void _damage_object(Object* a1, int damage, bool animated, int a4, Object* a5);
void _combat_display(Attack* attack);
void combatCopyDamageAmountDescription(char* dest, Object* critter_obj, int damage);
void combatAddDamageFlagsDescription(char* a1, int flags, Object* a3);
void _combat_anim_begin();
void _combat_anim_finished();
void _combat_standup(Object* a1);
void _print_tohit(unsigned char* dest, int dest_pitch, int a3);
char* hitLocationGetName(Object* critter, int hitLocation);
void _draw_loc_off(int a1, int a2);
void _draw_loc_on_(int a1, int a2);
void _draw_loc_(int eventCode, int color);
int calledShotSelectHitLocation(Object* critter, int* hitLocation, int hitMode);
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

static inline bool isInCombat()
{
    return (gCombatState & COMBAT_STATE_0x01) != 0;
}

#endif /* COMBAT_H */
