#ifndef ACTIONS_H
#define ACTIONS_H

#include "combat_defs.h"
#include "obj_types.h"
#include "proto_types.h"

extern int _action_in_explode;
extern const int gNormalDeathAnimations[DAMAGE_TYPE_COUNT];
extern const int gMaximumBloodDeathAnimations[DAMAGE_TYPE_COUNT];

int actionKnockdown(Object* obj, int* anim, int maxDistance, int rotation, int delay);
int _action_blood(Object* obj, int anim, int delay);
int _pick_death(Object* attacker, Object* defender, Object* weapon, int damage, int anim, bool isFallingBack);
int _check_death(Object* obj, int anim, int minViolenceLevel, bool isFallingBack);
int _internal_destroy(Object* a1, Object* a2);
void _show_damage_to_object(Object* a1, int damage, int flags, Object* weapon, bool isFallingBack, int knockbackDistance, int knockbackRotation, int a8, Object* a9, int a10);
int _show_death(Object* obj, int anim);
int _show_damage_extras(Attack* attack);
void _show_damage(Attack* attack, int a2, int a3);
int _action_attack(Attack* attack);
int _action_melee(Attack* attack, int a2);
int _action_ranged(Attack* attack, int a2);
int _is_next_to(Object* a1, Object* a2);
int _action_climb_ladder(Object* a1, Object* a2);
int _action_use_an_item_on_object(Object* a1, Object* a2, Object* a3);
int _action_use_an_object(Object* a1, Object* a2);
int actionPickUp(Object* critter, Object* item);
int _action_loot_container(Object* critter, Object* container);
int _action_skill_use(int a1);
int actionUseSkill(Object* a1, Object* a2, int skill);
bool _is_hit_from_front(Object* a1, Object* a2);
bool _can_see(Object* a1, Object* a2);
int _pick_fall(Object* obj, int anim);
bool _action_explode_running();
int actionExplode(int tile, int elevation, int minDamage, int maxDamage, Object* a5, bool a6);
int _report_explosion(Attack* attack, Object* a2);
int _finished_explosion(Object* a1, Object* a2);
int _compute_explosion_damage(int min, int max, Object* a3, int* a4);
int actionTalk(Object* a1, Object* a2);
int _can_talk_to(Object* a1, Object* a2);
int _talk_to(Object* a1, Object* a2);
void _action_dmg(int tile, int elevation, int minDamage, int maxDamage, int damageType, bool animated, bool bypassArmor);
int _report_dmg(Attack* attack, Object* a2);
int _compute_dmg_damage(int min, int max, Object* obj, int* a4, int damage_type);
bool actionCheckPush(Object* a1, Object* a2);
int actionPush(Object* a1, Object* a2);
int _action_can_talk_to(Object* a1, Object* a2);

#endif /* ACTIONS_H */
