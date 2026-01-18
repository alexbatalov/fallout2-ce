#ifndef ACTIONS_H
#define ACTIONS_H

#include "combat_defs.h"
#include "obj_types.h"

namespace fallout {

extern int rotation;

int _action_attack(Attack* attack);
int _action_use_an_item_on_object(Object* user, Object* targetObj, Object* item);
int _action_use_an_object(Object* user, Object* targetObj);
int actionPickUp(Object* critter, Object* item);
int _action_loot_container(Object* critter, Object* container);
int _action_skill_use(int skill);
int actionUseSkill(Object* user, Object* target, int skill);
bool _is_hit_from_front(Object* attacker, Object* defender);
bool _can_see(Object* source, Object* target);
bool _action_explode_running();
int actionExplode(int tile, int elevation, int minDamage, int maxDamage, Object* sourceObj, bool animate);
int actionTalk(Object* obj, Object* critter);
void actionDamage(int tile, int elevation, int minDamage, int maxDamage, int damageType, bool animated, bool bypassArmor);
bool actionCheckPush(Object* obj, Object* target);
int actionPush(Object* obj, Object* target);
int _action_can_talk_to(Object* obj, Object* critter);

} // namespace fallout

#endif /* ACTIONS_H */
