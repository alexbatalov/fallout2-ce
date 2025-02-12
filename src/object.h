#ifndef OBJECT_H
#define OBJECT_H

#include "db.h"
#include "geometry.h"
#include "inventory.h"
#include "map_defs.h"
#include "obj_types.h"

namespace fallout {

typedef struct ObjectWithFlags {
    int flags;
    Object* object;
} ObjectWithFlags;

extern unsigned char* _wallBlendTable;
extern Object* _moveBlockObj;

extern unsigned char _commonGrayTable[256];
extern Object* gEgg;
extern Object* gDude;

int objectsInit(unsigned char* buf, int width, int height, int pitch);
void objectsReset();
void objectsExit();
int objectRead(Object* obj, File* stream);
int objectLoadAll(File* stream);
int objectSaveAll(File* stream);
void _obj_render_pre_roof(Rect* rect, int elevation);
void _obj_render_post_roof(Rect* rect, int elevation);
int objectCreateWithFidPid(Object** objectPtr, int fid, int pid);
int objectCreateWithPid(Object** objectPtr, int pid);
int _obj_copy(Object** a1, Object* a2);
int _obj_connect(Object* obj, int tile_index, int elev, Rect* rect);
int _obj_disconnect(Object* obj, Rect* rect);
int _obj_offset(Object* obj, int x, int y, Rect* rect);
int _obj_move(Object* a1, int a2, int a3, int elevation, Rect* a5);
int objectSetLocation(Object* obj, int tile, int elevation, Rect* rect);
int _obj_reset_roof();
int objectSetFid(Object* obj, int fid, Rect* rect);
int objectSetFrame(Object* obj, int frame, Rect* rect);
int objectSetNextFrame(Object* obj, Rect* rect);
int objectSetPrevFrame(Object* obj, Rect* rect);
int objectSetRotation(Object* obj, int direction, Rect* rect);
int objectRotateClockwise(Object* obj, Rect* rect);
int objectRotateCounterClockwise(Object* obj, Rect* rect);
void _obj_rebuild_all_light();
int objectSetLight(Object* obj, int lightDistance, int lightIntensity, Rect* rect);
int objectGetLightIntensity(Object* obj);
int _obj_turn_on_light(Object* obj, Rect* rect);
int _obj_turn_off_light(Object* obj, Rect* rect);
int objectShow(Object* obj, Rect* rect);
int objectHide(Object* obj, Rect* rect);
int objectEnableOutline(Object* obj, Rect* rect);
int objectDisableOutline(Object* obj, Rect* rect);
int _obj_toggle_flat(Object* obj, Rect* rect);
int objectDestroy(Object* object, Rect* rect);
int _obj_inven_free(Inventory* inventory);
bool _obj_action_can_use(Object* obj);
bool _obj_action_can_talk_to(Object* obj);
bool _obj_portal_is_walk_thru(Object* obj);
Object* objectFindById(int a1);
Object* objectGetOwner(Object* obj);
void _obj_remove_all();
Object* objectFindFirst();
Object* objectFindNext();
Object* objectFindFirstAtElevation(int elevation);
Object* objectFindNextAtElevation();
Object* objectFindFirstAtLocation(int elevation, int tile);
Object* objectFindNextAtLocation();
void objectGetRect(Object* obj, Rect* rect);
bool _obj_occupied(int tile_num, int elev);
Object* _obj_blocking_at(Object* excludeObj, int tile_num, int elev);
Object* _obj_shoot_blocking_at(Object* excludeObj, int tile, int elev);
Object* _obj_ai_blocking_at(Object* excludeObj, int tile, int elevation);
int _obj_scroll_blocking_at(int tile_num, int elev);
Object* _obj_sight_blocking_at(Object* excludeObj, int tile_num, int elev);
int objectGetDistanceBetween(Object* object1, Object* object2);
int objectGetDistanceBetweenTiles(Object* object1, int tile1, Object* object2, int tile2);
int objectListCreate(int tile, int elevation, int objectType, Object*** objectsPtr);
void objectListFree(Object** objects);
void _translucent_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, unsigned char* a9, unsigned char* a10);
void _dark_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, int light);
void _dark_translucent_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, int light, unsigned char* a10, unsigned char* a11);
void _intensity_mask_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destPitch, unsigned char* mask, int maskPitch, int light);
int objectSetOutline(Object* obj, int a2, Rect* rect);
int objectClearOutline(Object* obj, Rect* rect);
int _obj_intersects_with(Object* object, int x, int y);
int _obj_create_intersect_list(int x, int y, int elevation, int objectType, ObjectWithFlags** entriesPtr);
void _obj_delete_intersect_list(ObjectWithFlags** a1);
void obj_set_seen(int tile);
void _obj_clear_seen();
void _obj_process_seen();
char* objectGetName(Object* obj);
char* objectGetDescription(Object* obj);
void _obj_preload_art_cache(int flags);
int _obj_save_dude(File* stream);
int _obj_load_dude(File* stream);
void _obj_fix_violence_settings(int* fid);

Object* objectTypedFindById(int id, int type);
bool isExitGridAt(int tile, int elevation);

} // namespace fallout

#endif /* OBJECT_H */
