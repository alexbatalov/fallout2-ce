#ifndef PROTOTYPE_INSTANCES_H
#define PROTOTYPE_INSTANCES_H

#include "message.h"
#include "obj_types.h"

extern MessageListItem stru_49A990;

int _obj_sid(Object* object, int* sidPtr);
int _obj_new_sid(Object* object, int* sidPtr);
int _obj_new_sid_inst(Object* obj, int a2, int a3);
int _obj_look_at(Object* a1, Object* a2);
int _obj_look_at_func(Object* a1, Object* a2, void (*a3)(char* string));
int _obj_examine(Object* a1, Object* a2);
int _obj_examine_func(Object* critter, Object* target, void (*fn)(char* string));
int _obj_pickup(Object* critter, Object* item);
int _obj_remove_from_inven(Object* critter, Object* item);
int _obj_drop(Object* a1, Object* a2);
int _obj_destroy(Object* obj);
int _obj_use_book(Object* item_obj);
int _obj_use_flare(Object* critter_obj, Object* item_obj);
int _obj_use_radio(Object* item_obj);
int _obj_use_explosive(Object* explosive);
int _obj_use_power_on_car(Object* ammo);
int _obj_use_misc_item(Object* item_obj);
int _protinst_use_item(Object* a1, Object* a2);
int _protinstTestDroppedExplosive(Object* a1);
int _obj_use_item(Object* a1, Object* a2);
int _protinst_default_use_item(Object* a1, Object* a2, Object* item);
int _protinst_use_item_on(Object* a1, Object* a2, Object* item);
int _obj_use_item_on(Object* a1, Object* a2, Object* a3);
int _check_scenery_ap_cost(Object* obj, Object* a2);
int _obj_use(Object* a1, Object* a2);
int useLadderDown(Object* a1, Object* ladder, int a3);
int useLadderUp(Object* a1, Object* ladder, int a3);
int useStairs(Object* a1, Object* stairs, int a3);
int _set_door_state_open(Object* a1, Object* a2);
int _set_door_state_closed(Object* a1, Object* a2);
int _check_door_state(Object* a1, Object* a2);
int _obj_use_door(Object* a1, Object* a2, int a3);
int _obj_use_container(Object* critter, Object* item);
int _obj_use_skill_on(Object* a1, Object* a2, int skill);
bool _obj_is_lockable(Object* obj);
bool objectIsLocked(Object* obj);
int objectLock(Object* obj);
int objectUnlock(Object* obj);
bool _obj_is_openable(Object* obj);
int objectIsOpen(Object* obj);
int objectOpenClose(Object* obj);
int objectOpen(Object* obj);
int objectClose(Object* obj);
bool objectIsJammed(Object* obj);
int objectJamLock(Object* obj);
int objectUnjamLock(Object* obj);
int objectUnjamAll();
int _obj_attempt_placement(Object* obj, int tile, int elevation, int a4);
int _objPMAttemptPlacement(Object* obj, int tile, int elevation);

#endif /* PROTOTYPE_INSTANCES_H */
