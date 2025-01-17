#ifndef PROTOTYPE_INSTANCES_H
#define PROTOTYPE_INSTANCES_H

#include "obj_types.h"

namespace fallout {

int _obj_sid(Object* object, int* sidPtr);
int _obj_new_sid(Object* object, int* sidPtr);
int _obj_new_sid_inst(Object* obj, int a2, int a3);
int _obj_look_at(Object* a1, Object* a2);
int _obj_look_at_func(Object* a1, Object* a2, void (*a3)(char* string));
int _obj_examine(Object* a1, Object* a2);
int _obj_examine_func(Object* critter, Object* target, void (*fn)(char* string));
int _obj_pickup(Object* critter, Object* item);
int _obj_drop(Object* a1, Object* a2);
int _obj_destroy(Object* obj);
int _protinst_use_item(Object* a1, Object* a2);
int _obj_use_item(Object* a1, Object* a2);
int _protinst_use_item_on(Object* critter, Object* targetObj, Object* item);
int _obj_use_item_on(Object* user, Object* targetObj, Object* item);
int _check_scenery_ap_cost(Object* obj, Object* a2);
int _obj_use(Object* user, Object* targetObj);
int _obj_use_door(Object* user, Object* doorObj, bool animateOnly = false);
int _obj_use_container(Object* critter, Object* item);
int _obj_use_skill_on(Object* a1, Object* a2, int skill);
bool objectIsLocked(Object* obj);
int objectLock(Object* obj);
int objectUnlock(Object* obj);
int objectIsOpen(Object* obj);
int objectOpen(Object* obj);
int objectClose(Object* obj);
int objectJamLock(Object* obj);
int objectUnjamLock(Object* obj);
int objectUnjamAll();
int _obj_attempt_placement(Object* obj, int tile, int elevation, int radius);
int _objPMAttemptPlacement(Object* obj, int tile, int elevation);

} // namespace fallout

#endif /* PROTOTYPE_INSTANCES_H */
