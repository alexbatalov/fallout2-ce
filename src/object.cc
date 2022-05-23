#include "object.h"

#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "core.h"
#include "critter.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_config.h"
#include "game_mouse.h"
#include "item.h"
#include "light.h"
#include "map.h"
#include "memory.h"
#include "party_member.h"
#include "proto.h"
#include "proto_instance.h"
#include "scripts.h"
#include "text_object.h"
#include "tile.h"
#include "world_map.h"

#include <assert.h>
#include <string.h>

// 0x5195F8
bool gObjectsInitialized = false;

// 0x5195FC
int _updateHexWidth = 0;

// 0x519600
int _updateHexHeight = 0;

// 0x519604
int _updateHexArea = 0;

// 0x519608
int* _orderTable[2] = {
    NULL,
    NULL,
};

// 0x519610
int* _offsetTable[2] = {
    NULL,
    NULL,
};

// 0x519618
int* _offsetDivTable = NULL;

// 0x51961C
int* _offsetModTable = NULL;

// 0x519620
ObjectListNode** _renderTable = NULL;

// Number of objects in _outlinedObjects.
//
// 0x519624
int _outlineCount = 0;

// Contains objects that are not bounded to tiles.
//
// 0x519628
ObjectListNode* gObjectListHead = NULL;

// 0x51962C
int _centerToUpperLeft = 0;

// 0x519630
int gObjectFindElevation = 0;

// 0x519634
int gObjectFindTile = 0;

// 0x519638
ObjectListNode* gObjectFindLastObjectListNode = NULL;

// 0x51963C
int* gObjectFids = NULL;

// 0x519640
int gObjectFidsLength = 0;

// 0x51964C
Rect _light_rect[9] = {
    { 0, 0, 96, 42 },
    { 0, 0, 160, 74 },
    { 0, 0, 224, 106 },
    { 0, 0, 288, 138 },
    { 0, 0, 352, 170 },
    { 0, 0, 416, 202 },
    { 0, 0, 480, 234 },
    { 0, 0, 544, 266 },
    { 0, 0, 608, 298 },
};

// 0x5196DC
int _light_distance[36] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    3,
    4,
    5,
    6,
    7,
    8,
    4,
    5,
    6,
    7,
    8,
    5,
    6,
    7,
    8,
    6,
    7,
    8,
    7,
    8,
    8,
};

// 0x51976C
int gViolenceLevel = -1;

// 0x519770
int _obj_last_roof_x = -1;

// 0x519774
int _obj_last_roof_y = -1;

// 0x519778
int _obj_last_elev = -1;

// 0x51977C
int _obj_last_is_empty = 1;

// 0x519780
unsigned char* _wallBlendTable = NULL;

// 0x519784
unsigned char* _glassBlendTable = NULL;

// 0x519788
unsigned char* _steamBlendTable = NULL;

// 0x51978C
unsigned char* _energyBlendTable = NULL;

// 0x519790
unsigned char* _redBlendTable = NULL;

// 0x519794
Object* _moveBlockObj = NULL;

// 0x519798
int _objItemOutlineState = 0;

// 0x51979C
int _cd_order[9] = {
    1,
    0,
    3,
    5,
    4,
    2,
    0,
    0,
    0,
};

// 0x6391D0
int _light_blocked[6][36];

// 0x639530
int _light_offsets[2][6][36];

// 0x639BF0
Rect gObjectsWindowRect;

// Likely outlined objects on the screen.
//
// 0x639C00
Object* _outlinedObjects[100];

// 0x639D90
int _updateAreaPixelBounds;

// 0x639D94
int dword_639D94;

// 0x639D98
int dword_639D98;

// 0x639D9C
int dword_639D9C;

// Contains objects that are bounded to tiles.
//
// 0x639DA0
ObjectListNode* gObjectListHeadByTile[HEX_GRID_SIZE];

// 0x660EA0
unsigned char _glassGrayTable[256];

// 0x660FA0
unsigned char _commonGrayTable[256];

// 0x6610A0
int gObjectsWindowBufferSize;

// 0x6610A4
unsigned char* gObjectsWindowBuffer;

// 0x6610A8
int gObjectsWindowHeight;

// Translucent "egg" effect around player.
//
// 0x6610AC
Object* gEgg;

// 0x6610B0
int gObjectsWindowPitch;

// 0x6610B4
int gObjectsWindowWidth;

// obj_dude
// 0x6610B8
Object* gDude;

// 0x6610BC
char _obj_seen_check[5001];

// 0x662445
char _obj_seen[5001];

// obj_init
// 0x488780
int objectsInit(unsigned char* buf, int width, int height, int pitch)
{
    memset(_obj_seen, 0, 5001);
    dword_639D98 = width + 320;
    _updateAreaPixelBounds = -320;
    dword_639D9C = height + 240;
    dword_639D94 = -240;

    _updateHexWidth = (dword_639D98 + 320 + 1) / 32 + 1;
    _updateHexHeight = (dword_639D9C + 240 + 1) / 12 + 1;
    _updateHexArea = _updateHexWidth * _updateHexHeight;

    memset(gObjectListHeadByTile, 0, sizeof(gObjectListHeadByTile));

    if (_obj_offset_table_init() == -1) {
        return -1;
    }

    if (_obj_order_table_init() == -1) {
        goto err;
    }

    if (_obj_render_table_init() == -1) {
        goto err_2;
    }

    if (lightInit() == -1) {
        goto err_2;
    }

    if (textObjectsInit(buf, width, height) == -1) {
        goto err_2;
    }

    _obj_light_table_init();
    _obj_blend_table_init();

    _centerToUpperLeft = tileFromScreenXY(_updateAreaPixelBounds, dword_639D94, 0) - gCenterTile;
    gObjectsWindowWidth = width;
    gObjectsWindowHeight = height;
    gObjectsWindowBuffer = buf;

    gObjectsWindowRect.left = 0;
    gObjectsWindowRect.top = 0;
    gObjectsWindowRect.right = width - 1;
    gObjectsWindowRect.bottom = height - 1;

    gObjectsWindowBufferSize = height * width;
    gObjectsWindowPitch = pitch;

    int dudeFid = buildFid(1, _art_vault_guy_num, 0, 0, 0);
    objectCreateWithFidPid(&gDude, dudeFid, 0x1000000);

    gDude->flags |= OBJECT_FLAG_0x400;
    gDude->flags |= OBJECT_TEMPORARY;
    gDude->flags |= OBJECT_HIDDEN;
    gDude->flags |= OBJECT_FLAG_0x20000000;
    objectSetLight(gDude, 4, 0x10000, NULL);

    if (partyMemberAdd(gDude) == -1) {
        debugPrint("\n  Error: Can't add Player into party!");
        exit(1);
    }

    int eggFid = buildFid(6, 2, 0, 0, 0);
    objectCreateWithFidPid(&gEgg, eggFid, -1);
    gEgg->flags |= OBJECT_FLAG_0x400;
    gEgg->flags |= OBJECT_TEMPORARY;
    gEgg->flags |= OBJECT_HIDDEN;
    gEgg->flags |= OBJECT_FLAG_0x20000000;

    gObjectsInitialized = true;

    return 0;

err_2:

    // NOTE: Uninline.
    _obj_order_table_exit();

err:

    _obj_offset_table_exit();

    return -1;
}

// 0x488A00
void objectsReset()
{
    if (gObjectsInitialized) {
        textObjectsReset();
        _obj_remove_all();
        memset(_obj_seen, 0, 5001);
        lightResetIntensity();
    }
}

// 0x488A30
void objectsExit()
{
    if (gObjectsInitialized) {
        gDude->flags &= ~OBJECT_FLAG_0x400;
        gEgg->flags &= ~OBJECT_FLAG_0x400;

        _obj_remove_all();
        textObjectsFree();

        // NOTE: Uninline.
        _obj_blend_table_exit();

        lightResetIntensity();

        // NOTE: Uninline.
        _obj_render_table_exit();

        // NOTE: Uninline.
        _obj_order_table_exit();

        _obj_offset_table_exit();
    }
}

// 0x488AF4
int objectRead(Object* obj, File* stream)
{
    int field_74;

    if (fileReadInt32(stream, &(obj->id)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->tile)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->x)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->y)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->sx)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->sy)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->frame)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->rotation)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->fid)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->flags)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->elevation)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->pid)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->cid)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->lightDistance)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->lightIntensity)) == -1) return -1;
    if (fileReadInt32(stream, &field_74) == -1) return -1;
    if (fileReadInt32(stream, &(obj->sid)) == -1) return -1;
    if (fileReadInt32(stream, &(obj->field_80)) == -1) return -1;

    obj->outline = 0;
    obj->owner = NULL;

    if (objectDataRead(obj, stream) != 0) {
        return -1;
    }

    if (obj->pid < 0x5000010 || obj->pid > 0x5000017) {
        if ((obj->pid >> 24) == 0 && !(gMapHeader.flags & 0x01)) {
            _object_fix_weapon_ammo(obj);
        }
    } else {
        if (obj->data.misc.map <= 0) {
            if ((obj->fid & 0xFFF) < 33) {
                obj->fid = buildFid(5, (obj->fid & 0xFFF) + 16, (obj->fid & 0xFF0000) >> 16, 0, 0);
            }
        }
    }

    return 0;
}

// 0x488CE4
int objectLoadAll(File* stream)
{
    int rc = objectLoadAllInternal(stream);

    gViolenceLevel = -1;

    return rc;
}

// 0x488CF8
int objectLoadAllInternal(File* stream)
{
    if (stream == NULL) {
        return -1;
    }

    bool fixMapInventory;
    if (!configGetBool(&gGameConfig, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_FIX_MAP_INVENTORY_KEY, &fixMapInventory)) {
        fixMapInventory = false;
    }

    if (!configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &gViolenceLevel)) {
        gViolenceLevel = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    }

    int objectCount;
    if (fileReadInt32(stream, &objectCount) == -1) {
        return -1;
    }

    if (gObjectFids != NULL) {
        internal_free(gObjectFids);
    }

    if (objectCount != 0) {
        gObjectFids = (int*)internal_malloc(sizeof(*gObjectFids) * objectCount);
        memset(gObjectFids, 0, sizeof(*gObjectFids) * objectCount);
        if (gObjectFids == NULL) {
            return -1;
        }
        gObjectFidsLength = 0;
    }

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int objectCountAtElevation;
        if (fileReadInt32(stream, &objectCountAtElevation) == -1) {
            return -1;
        }

        for (int objectIndex = 0; objectIndex < objectCountAtElevation; objectIndex++) {
            ObjectListNode* objectListNode;

            // NOTE: Uninline.
            if (objectListNodeCreate(&objectListNode) == -1) {
                return -1;
            }

            if (objectAllocate(&(objectListNode->obj)) == -1) {
                // NOTE: Uninline.
                objectListNodeDestroy(&objectListNode);
                return -1;
            }

            if (objectRead(objectListNode->obj, stream) != 0) {
                // NOTE: Uninline.
                objectDeallocate(&(objectListNode->obj));

                // NOTE: Uninline.
                objectListNodeDestroy(&objectListNode);

                return -1;
            }

            objectListNode->obj->outline = 0;
            gObjectFids[gObjectFidsLength++] = objectListNode->obj->fid;

            if (objectListNode->obj->sid != -1) {
                Script* script;
                if (scriptGetScript(objectListNode->obj->sid, &script) == -1) {
                    objectListNode->obj->sid = -1;
                    debugPrint("\nError connecting object to script!");
                } else {
                    script->owner = objectListNode->obj;
                    objectListNode->obj->field_80 = script->field_14;
                }
            }

            _obj_fix_violence_settings(&(objectListNode->obj->fid));
            objectListNode->obj->elevation = elevation;

            _obj_insert(objectListNode);

            if ((objectListNode->obj->flags & OBJECT_FLAG_0x400) && (objectListNode->obj->flags >> 24) == OBJ_TYPE_CRITTER && objectListNode->obj->pid != 18000) {
                objectListNode->obj->flags &= ~OBJECT_FLAG_0x400;
            }

            Inventory* inventory = &(objectListNode->obj->data.inventory);
            if (inventory->length != 0) {
                inventory->items = (InventoryItem*)internal_malloc(sizeof(InventoryItem) * inventory->capacity);
                if (inventory->items == NULL) {
                    return -1;
                }

                for (int inventoryItemIndex = 0; inventoryItemIndex < inventory->length; inventoryItemIndex++) {
                    InventoryItem* inventoryItem = &(inventory->items[inventoryItemIndex]);
                    if (fileReadInt32(stream, &(inventoryItem->quantity)) != 0) {
                        debugPrint("Error loading inventory\n");
                        return -1;
                    }

                    if (fixMapInventory) {
                        inventoryItem->item = (Object*)internal_malloc(sizeof(Object));
                        if (inventoryItem->item == NULL) {
                            debugPrint("Error loading inventory\n");
                            return -1;
                        }

                        if (objectRead(inventoryItem->item, stream) != 0) {
                            debugPrint("Error loading inventory\n");
                            return -1;
                        }
                    } else {
                        if (_obj_load_obj(stream, &(inventoryItem->item), elevation, objectListNode->obj) == -1) {
                            return -1;
                        }
                    }
                }
            } else {
                inventory->capacity = 0;
                inventory->items = NULL;
            }
        }
    }

    _obj_rebuild_all_light();

    return 0;
}

// 0x48909C
void _obj_fix_combat_cid_for_dude()
{
    Object** critterList;
    int critterListLength = objectListCreate(-1, gElevation, OBJ_TYPE_CRITTER, &critterList);

    if (gDude->data.critter.combat.whoHitMeCid == -1) {
        gDude->data.critter.combat.whoHitMe = NULL;
    } else {
        int index = _find_cid(0, gDude->data.critter.combat.whoHitMeCid, critterList, critterListLength);
        if (index != critterListLength) {
            gDude->data.critter.combat.whoHitMe = critterList[index];
        } else {
            gDude->data.critter.combat.whoHitMe = NULL;
        }
    }

    if (critterListLength != 0) {
        // NOTE: Uninline.
        objectListFree(critterList);
    }
}

// Fixes ammo pid and number of charges.
//
// 0x48911C
void _object_fix_weapon_ammo(Object* obj)
{
    if ((obj->pid >> 24) != OBJ_TYPE_ITEM) {
        return;
    }

    Proto* proto;
    if (protoGetProto(obj->pid, &proto) == -1) {
        debugPrint("\nError: obj_load: proto_ptr failed on pid");
        exit(1);
    }

    int charges;
    if (itemGetType(obj) == ITEM_TYPE_WEAPON) {
        int ammoTypePid = obj->data.item.weapon.ammoTypePid;
        if (ammoTypePid == 0xCCCCCCCC || ammoTypePid == -1) {
            obj->data.item.weapon.ammoTypePid = proto->item.data.weapon.ammoTypePid;
        }

        charges = obj->data.item.weapon.ammoQuantity;
        if (charges == 0xCCCCCCCC || charges == -1 || charges != proto->item.data.weapon.ammoCapacity) {
            obj->data.item.weapon.ammoQuantity = proto->item.data.weapon.ammoCapacity;
        }
    } else {
        if ((obj->pid >> 24) == OBJ_TYPE_MISC) {
            // FIXME: looks like this code in unreachable
            charges = obj->data.item.misc.charges;
            if (charges == 0xCCCCCCCC) {
                charges = proto->item.data.misc.charges;
                obj->data.item.misc.charges = charges;
                if (charges == 0xCCCCCCCC) {
                    debugPrint("\nError: Misc Item Prototype %s: charges incorrect!", protoGetName(obj->pid));
                    obj->data.item.misc.charges = 0;
                }
            } else {
                if (charges != proto->item.data.misc.charges) {
                    obj->data.item.misc.charges = proto->item.data.misc.charges;
                }
            }
        }
    }
}

// 0x489200
int objectWrite(Object* obj, File* stream)
{
    if (fileWriteInt32(stream, obj->id) == -1) return -1;
    if (fileWriteInt32(stream, obj->tile) == -1) return -1;
    if (fileWriteInt32(stream, obj->x) == -1) return -1;
    if (fileWriteInt32(stream, obj->y) == -1) return -1;
    if (fileWriteInt32(stream, obj->sx) == -1) return -1;
    if (fileWriteInt32(stream, obj->sy) == -1) return -1;
    if (fileWriteInt32(stream, obj->frame) == -1) return -1;
    if (fileWriteInt32(stream, obj->rotation) == -1) return -1;
    if (fileWriteInt32(stream, obj->fid) == -1) return -1;
    if (fileWriteInt32(stream, obj->flags) == -1) return -1;
    if (fileWriteInt32(stream, obj->elevation) == -1) return -1;
    if (fileWriteInt32(stream, obj->pid) == -1) return -1;
    if (fileWriteInt32(stream, obj->cid) == -1) return -1;
    if (fileWriteInt32(stream, obj->lightDistance) == -1) return -1;
    if (fileWriteInt32(stream, obj->lightIntensity) == -1) return -1;
    if (fileWriteInt32(stream, obj->outline) == -1) return -1;
    if (fileWriteInt32(stream, obj->sid) == -1) return -1;
    if (fileWriteInt32(stream, obj->field_80) == -1) return -1;
    if (objectDataWrite(obj, stream) == -1) return -1;

    return 0;
}

// 0x48935C
int objectSaveAll(File* stream)
{
    if (stream == NULL) {
        return -1;
    }

    _obj_process_seen();

    int objectCount = 0;

    long objectCountPos = fileTell(stream);
    if (fileWriteInt32(stream, objectCount) == -1) {
        return -1;
    }

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int objectCountAtElevation = 0;

        long objectCountAtElevationPos = fileTell(stream);
        if (fileWriteInt32(stream, objectCountAtElevation) == -1) {
            return -1;
        }

        for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
            for (ObjectListNode* objectListNode = gObjectListHeadByTile[tile]; objectListNode != NULL; objectListNode = objectListNode->next) {
                Object* object = objectListNode->obj;
                if (object->elevation != elevation) {
                    continue;
                }

                if ((object->flags & OBJECT_TEMPORARY) != 0) {
                    continue;
                }

                CritterCombatData* combatData = NULL;
                Object* whoHitMe = NULL;
                if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
                    combatData = &(object->data.critter.combat);
                    whoHitMe = combatData->whoHitMe;
                    if (whoHitMe != 0) {
                        if (combatData->whoHitMeCid != -1) {
                            combatData->whoHitMeCid = whoHitMe->cid;
                        }
                    } else {
                        combatData->whoHitMeCid = -1;
                    }
                }

                if (objectWrite(object, stream) == -1) {
                    return -1;
                }

                if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
                    combatData->whoHitMe = whoHitMe;
                }

                Inventory* inventory = &(object->data.inventory);
                for (int index = 0; index < inventory->length; index++) {
                    InventoryItem* inventoryItem = &(inventory->items[index]);

                    if (fileWriteInt32(stream, inventoryItem->quantity) == -1) {
                        return -1;
                    }

                    if (_obj_save_obj(stream, inventoryItem->item) == -1) {
                        return -1;
                    }
                }

                objectCountAtElevation++;
            }
        }

        long pos = fileTell(stream);
        fileSeek(stream, objectCountAtElevationPos, SEEK_SET);
        fileWriteInt32(stream, objectCountAtElevation);
        fileSeek(stream, pos, SEEK_SET);

        objectCount += objectCountAtElevation;
    }

    long pos = fileTell(stream);
    fileSeek(stream, objectCountPos, SEEK_SET);
    fileWriteInt32(stream, objectCount);
    fileSeek(stream, pos, SEEK_SET);

    return 0;
}

// 0x489550
void _obj_render_pre_roof(Rect* rect, int elevation)
{
    if (!gObjectsInitialized) {
        return;
    }

    Rect updatedRect;
    if (rectIntersection(rect, &gObjectsWindowRect, &updatedRect) != 0) {
        return;
    }

    int lightLevel = lightGetLightLevel();
    int v5 = updatedRect.left - 320;
    int v4 = updatedRect.top - 240;
    int v17 = updatedRect.right + 320;
    int v18 = updatedRect.bottom + 240;
    int v19 = tileFromScreenXY(v5, v4, elevation);
    int v20 = (v17 - v5 + 1) / 32;
    int v23 = (v18 - v4 + 1) / 12;

    int odd = gCenterTile & 1;
    int* v7 = _orderTable[odd];
    int* v8 = _offsetTable[odd];

    _outlineCount = 0;

    int v34 = 0;
    for (int i = 0; i < _updateHexArea; i++) {
        int v9 = *v7++;
        if (v23 > _offsetDivTable[v9] && v20 > _offsetModTable[v9]) {
            int v2;

            ObjectListNode* objectListNode = gObjectListHeadByTile[v19 + v8[v9]];
            if (objectListNode != NULL) {
                // NOTE: calls _light_get_tile two times, probably result of min/max macro
                int q = _light_get_tile(elevation, objectListNode->obj->tile);
                if (q >= lightLevel) {
                    v2 = q;
                } else {
                    v2 = lightLevel;
                }
            }

            while (objectListNode != NULL) {
                if (elevation < objectListNode->obj->elevation) {
                    break;
                }

                if (elevation == objectListNode->obj->elevation) {
                    if ((objectListNode->obj->flags & OBJECT_FLAG_0x08) == 0) {
                        break;
                    }

                    if ((objectListNode->obj->flags & OBJECT_HIDDEN) == 0) {
                        _obj_render_object(objectListNode->obj, &updatedRect, v2);

                        if ((objectListNode->obj->outline & OUTLINE_TYPE_MASK) != 0) {
                            if ((objectListNode->obj->outline & OUTLINE_DISABLED) == 0 && _outlineCount < 100) {
                                _outlinedObjects[_outlineCount++] = objectListNode->obj;
                            }
                        }
                    }
                }

                objectListNode = objectListNode->next;
            }

            if (objectListNode != NULL) {
                _renderTable[v34++] = objectListNode;
            }
        }
    }

    for (int i = 0; i < v34; i++) {
        int v2;

        ObjectListNode* objectListNode = _renderTable[i];
        if (objectListNode != NULL) {
            v2 = lightLevel;
            int w = _light_get_tile(elevation, objectListNode->obj->tile);
            if (w > v2) {
                v2 = w;
            }
        }

        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (elevation < object->elevation) {
                break;
            }

            if (elevation == objectListNode->obj->elevation) {
                if ((objectListNode->obj->flags & OBJECT_HIDDEN) == 0) {
                    _obj_render_object(object, &updatedRect, v2);

                    if ((objectListNode->obj->outline & OUTLINE_TYPE_MASK) != 0) {
                        if ((objectListNode->obj->outline & OUTLINE_DISABLED) == 0 && _outlineCount < 100) {
                            _outlinedObjects[_outlineCount++] = objectListNode->obj;
                        }
                    }
                }
            }

            objectListNode = objectListNode->next;
        }
    }
}

// 0x4897EC
void _obj_render_post_roof(Rect* rect, int elevation)
{
    if (!gObjectsInitialized) {
        return;
    }

    Rect updatedRect;
    if (rectIntersection(rect, &gObjectsWindowRect, &updatedRect) != 0) {
        return;
    }

    for (int index = 0; index < _outlineCount; index++) {
        objectDrawOutline(_outlinedObjects[index], &updatedRect);
    }

    textObjectsRenderInRect(&updatedRect);

    ObjectListNode* objectListNode = gObjectListHead;
    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if ((object->flags & OBJECT_HIDDEN) == 0) {
            _obj_render_object(object, &updatedRect, 0x10000);
        }
        objectListNode = objectListNode->next;
    }
}

// 0x489A84
int objectCreateWithFidPid(Object** objectPtr, int fid, int pid)
{
    ObjectListNode* objectListNode;

    // NOTE: Uninline;
    if (objectListNodeCreate(&objectListNode) == -1) {
        return -1;
    }

    if (objectAllocate(&(objectListNode->obj)) == -1) {
        // Uninline.
        objectListNodeDestroy(&objectListNode);
        return -1;
    }

    objectListNode->obj->fid = fid;
    _obj_insert(objectListNode);

    if (objectPtr) {
        *objectPtr = objectListNode->obj;
    }

    objectListNode->obj->pid = pid;
    objectListNode->obj->id = scriptsNewObjectId();

    if (pid == -1 || (pid >> 24) == OBJ_TYPE_TILE) {
        Inventory* inventory = &(objectListNode->obj->data.inventory);
        inventory->length = 0;
        inventory->items = NULL;
        return 0;
    }

    _proto_update_init(objectListNode->obj);

    Proto* proto = NULL;
    if (protoGetProto(pid, &proto) == -1) {
        return 0;
    }

    objectSetLight(objectListNode->obj, proto->lightDistance, proto->lightIntensity, NULL);

    if ((proto->flags & 0x08) != 0) {
        _obj_toggle_flat(objectListNode->obj, NULL);
    }

    if ((proto->flags & 0x10) != 0) {
        objectListNode->obj->flags |= OBJECT_NO_BLOCK;
    }

    if ((proto->flags & 0x800) != 0) {
        objectListNode->obj->flags |= OBJECT_FLAG_0x800;
    }

    if ((proto->flags & 0x8000) != 0) {
        objectListNode->obj->flags |= OBJECT_FLAG_0x8000;
    } else {
        if ((proto->flags & 0x10000) != 0) {
            objectListNode->obj->flags |= OBJECT_FLAG_0x10000;
        } else if ((proto->flags & 0x20000) != 0) {
            objectListNode->obj->flags |= OBJECT_FLAG_0x20000;
        } else if ((proto->flags & 0x40000) != 0) {
            objectListNode->obj->flags |= OBJECT_FLAG_0x40000;
        } else if ((proto->flags & 0x80000) != 0) {
            objectListNode->obj->flags |= OBJECT_FLAG_0x80000;
        } else if ((proto->flags & 0x4000) != 0) {
            objectListNode->obj->flags |= OBJECT_FLAG_0x4000;
        }
    }

    if ((proto->flags & 0x20000000) != 0) {
        objectListNode->obj->flags |= OBJECT_FLAG_0x20000000;
    }

    if ((proto->flags & 0x80000000) != 0) {
        objectListNode->obj->flags |= OBJECT_FLAG_0x80000000;
    }

    if ((proto->flags & 0x10000000) != 0) {
        objectListNode->obj->flags |= OBJECT_FLAG_0x10000000;
    }

    if ((proto->flags & 0x1000) != 0) {
        objectListNode->obj->flags |= OBJECT_FLAG_0x1000;
    }

    _obj_new_sid(objectListNode->obj, &(objectListNode->obj->sid));

    return 0;
}

// 0x489C9C
int objectCreateWithPid(Object** objectPtr, int pid)
{
    Proto* proto;

    *objectPtr = NULL;

    if (protoGetProto(pid, &proto) == -1) {
        return -1;
    }

    return objectCreateWithFidPid(objectPtr, proto->fid, pid);
}

// 0x489CCC
int _obj_copy(Object** a1, Object* a2)
{
    if (a2 == NULL) {
        return -1;
    }

    ObjectListNode* objectListNode;

    // NOTE: Uninline.
    if (objectListNodeCreate(&objectListNode) == -1) {
        return -1;
    }

    if (objectAllocate(&(objectListNode->obj)) == -1) {
        // NOTE: Uninline.
        objectListNodeDestroy(&objectListNode);
        return -1;
    }

    objectDataReset(objectListNode->obj);

    memcpy(objectListNode->obj, a2, sizeof(Object));

    if (a1 != NULL) {
        *a1 = objectListNode->obj;
    }

    _obj_insert(objectListNode);

    objectListNode->obj->id = scriptsNewObjectId();

    if (objectListNode->obj->sid != -1) {
        objectListNode->obj->sid = -1;
        _obj_new_sid(objectListNode->obj, &(objectListNode->obj->sid));
    }

    if (objectSetRotation(objectListNode->obj, a2->rotation, NULL) == -1) {
        // TODO: Probably leaking object allocated with objectAllocate.
        // NOTE: Uninline.
        objectListNodeDestroy(&objectListNode);
        return -1;
    }

    objectListNode->obj->flags &= ~OBJECT_FLAG_0x2000;

    Inventory* newInventory = &(objectListNode->obj->data.inventory);
    newInventory->length = 0;
    newInventory->capacity = 0;

    Inventory* oldInventory = &(a2->data.inventory);
    for (int index = 0; index < oldInventory->length; index++) {
        InventoryItem* oldInventoryItem = &(oldInventory->items[index]);

        Object* newItem;
        if (_obj_copy(&newItem, oldInventoryItem->item) == -1) {
            // TODO: Probably leaking object allocated with objectAllocate.
            // NOTE: Uninline.
            objectListNodeDestroy(&objectListNode);
            return -1;
        }

        if (itemAdd(objectListNode->obj, newItem, oldInventoryItem->quantity) == 1) {
            // TODO: Probably leaking object allocated with objectAllocate.
            // NOTE: Uninline.
            objectListNodeDestroy(&objectListNode);
            return -1;
        }
    }

    return 0;
}

// 0x489EC4
int _obj_connect(Object* object, int tile, int elevation, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if (!hexGridTileIsValid(tile)) {
        return -1;
    }

    if (!elevationIsValid(elevation)) {
        return -1;
    }

    ObjectListNode* objectListNode;

    // NOTE: Uninline.
    if (objectListNodeCreate(&objectListNode) == -1) {
        return -1;
    }

    objectListNode->obj = object;

    return _obj_connect_to_tile(objectListNode, tile, elevation, rect);
}

// 0x489F34
int _obj_disconnect(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    ObjectListNode* node;
    ObjectListNode* prev_node;
    if (objectGetListNode(obj, &node, &prev_node) != 0) {
        return -1;
    }

    if (_obj_adjust_light(obj, 1, rect) == -1) {
        if (rect != NULL) {
            objectGetRect(obj, rect);
        }
    }

    if (prev_node != NULL) {
        prev_node->next = node->next;
    } else {
        int tile = node->obj->tile;
        if (tile == -1) {
            gObjectListHead = gObjectListHead->next;
        } else {
            gObjectListHeadByTile[tile] = gObjectListHeadByTile[tile]->next;
        }
    }

    if (node != NULL) {
        internal_free(node);
    }

    obj->tile = -1;

    return 0;
}

// 0x489FF8
int _obj_offset(Object* obj, int x, int y, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    ObjectListNode* node = NULL;
    ObjectListNode* previousNode = NULL;
    if (objectGetListNode(obj, &node, &previousNode) == -1) {
        return -1;
    }

    if (obj == gDude) {
        if (rect != NULL) {
            Rect eggRect;
            objectGetRect(gEgg, &eggRect);
            rectCopy(rect, &eggRect);

            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    gObjectListHead = gObjectListHead->next;
                } else {
                    gObjectListHeadByTile[tile] = gObjectListHeadByTile[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            _obj_insert(node);

            rectOffset(&eggRect, x, y);

            _obj_offset(gEgg, x, y, NULL);
            rectUnion(rect, &eggRect, rect);
        } else {
            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    gObjectListHead = gObjectListHead->next;
                } else {
                    gObjectListHeadByTile[tile] = gObjectListHeadByTile[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            _obj_insert(node);

            _obj_offset(gEgg, x, y, NULL);
        }
    } else {
        if (rect != NULL) {
            objectGetRect(obj, rect);

            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    gObjectListHead = gObjectListHead->next;
                } else {
                    gObjectListHeadByTile[tile] = gObjectListHeadByTile[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            _obj_insert(node);

            Rect objectRect;
            rectCopy(&objectRect, rect);

            rectOffset(&objectRect, x, y);

            rectUnion(rect, &objectRect, rect);
        } else {
            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    gObjectListHead = gObjectListHead->next;
                } else {
                    gObjectListHeadByTile[tile] = gObjectListHeadByTile[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            _obj_insert(node);
        }
    }

    return 0;
}

// 0x48A324
int _obj_move(Object* a1, int a2, int a3, int elevation, Rect* a5)
{
    if (a1 == NULL) {
        return -1;
    }

    // TODO: Get rid of initialization.
    ObjectListNode* node = NULL;
    ObjectListNode* previousNode;
    int v22 = 0;

    int tile = a1->tile;
    if (hexGridTileIsValid(tile)) {
        if (objectGetListNode(a1, &node, &previousNode) == -1) {
            return -1;
        }

        if (_obj_adjust_light(a1, 1, a5) == -1) {
            if (a5 != NULL) {
                objectGetRect(a1, a5);
            }
        }

        if (previousNode != NULL) {
            previousNode->next = node->next;
        } else {
            int tile = node->obj->tile;
            if (tile == -1) {
                gObjectListHead = gObjectListHead->next;
            } else {
                gObjectListHeadByTile[tile] = gObjectListHeadByTile[tile]->next;
            }
        }

        a1->tile = -1;
        a1->elevation = elevation;
        v22 = 1;
    } else {
        if (elevation == a1->elevation) {
            if (a5 != NULL) {
                objectGetRect(a1, a5);
            }
        } else {
            if (objectGetListNode(a1, &node, &previousNode) == -1) {
                return -1;
            }

            if (a5 != NULL) {
                objectGetRect(a1, a5);
            }

            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile != -1) {
                    gObjectListHeadByTile[tile] = gObjectListHeadByTile[tile]->next;
                } else {
                    gObjectListHead = gObjectListHead->next;
                }
            }

            a1->elevation = elevation;
            v22 = 1;
        }
    }

    CacheEntry* cacheHandle;
    int width;
    int height;
    Art* art = artLock(a1->fid, &cacheHandle);
    if (art != NULL) {
        artGetSize(art, a1->frame, a1->rotation, &width, &height);
        a1->sx = a2 - width / 2;
        a1->sy = a3 - (height - 1);
        artUnlock(cacheHandle);
    }

    if (v22) {
        _obj_insert(node);
    }

    if (a5 != NULL) {
        Rect rect;
        objectGetRect(a1, &rect);
        rectUnion(a5, &rect, a5);
    }

    if (a1 == gDude) {
        if (a1 != NULL) {
            Rect rect;
            _obj_move(gEgg, a2, a3, elevation, &rect);
            rectUnion(a5, &rect, a5);
        } else {
            _obj_move(gEgg, a2, a3, elevation, NULL);
        }
    }

    return 0;
}

// 0x48A568
int objectSetLocation(Object* obj, int tile, int elevation, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (!hexGridTileIsValid(tile)) {
        return -1;
    }

    if (!elevationIsValid(elevation)) {
        return -1;
    }

    ObjectListNode* node;
    ObjectListNode* prevNode;
    if (objectGetListNode(obj, &node, &prevNode) == -1) {
        return -1;
    }

    Rect v23;
    int v5 = _obj_adjust_light(obj, 1, rect);
    if (rect != NULL) {
        if (v5 == -1) {
            objectGetRect(obj, rect);
        }

        rectCopy(&v23, rect);
    }

    int oldElevation = obj->elevation;
    if (prevNode != NULL) {
        prevNode->next = node->next;
    } else {
        int tileIndex = node->obj->tile;
        if (tileIndex == -1) {
            gObjectListHead = gObjectListHead->next;
        } else {
            gObjectListHeadByTile[tileIndex] = gObjectListHeadByTile[tileIndex]->next;
        }
    }

    if (_obj_connect_to_tile(node, tile, elevation, rect) == -1) {
        return -1;
    }

    if (isInCombat()) {
        if ((obj->fid & 0xF000000) >> 24 == OBJ_TYPE_CRITTER) {
            bool v8 = obj->outline != 0 && (obj->outline & OUTLINE_DISABLED) == 0;
            _combat_update_critter_outline_for_los(obj, v8);
        }
    }

    if (rect != NULL) {
        rectUnion(rect, &v23, rect);
    }

    if (obj == gDude) {
        ObjectListNode* objectListNode = gObjectListHeadByTile[tile];
        while (objectListNode != NULL) {
            Object* obj = objectListNode->obj;
            int elev = obj->elevation;
            if (elevation < elev) {
                break;
            }

            if (elevation == elev) {
                if ((obj->fid & 0xF000000) >> 24 == OBJ_TYPE_MISC) {
                    if (obj->pid >= 0x5000010 && obj->pid <= 0x5000017) {
                        ObjectData* data = &(obj->data);

                        MapTransition transition;
                        memset(&transition, 0, sizeof(transition));

                        transition.map = data->misc.map;
                        transition.tile = data->misc.tile;
                        transition.elevation = data->misc.elevation;
                        transition.rotation = data->misc.rotation;
                        mapSetTransition(&transition);

                        _wmMapMarkMapEntranceState(transition.map, transition.elevation, 1);
                    }
                }
            }

            objectListNode = objectListNode->next;
        }

        _obj_seen[tile >> 3] |= 1 << (tile & 7);

        int v14 = tile % 200 / 2;
        int v15 = tile / 200 / 2;
        if (v14 != _obj_last_roof_x || v15 != _obj_last_roof_y || elevation != _obj_last_elev) {
            int v16 = _square[elevation]->field_0[v14 + 100 * v15];
            int v31 = buildFid(4, (v16 >> 16) & 0xFFF, 0, 0, 0);
            int v32 = _square[elevation]->field_0[_obj_last_roof_x + 100 * _obj_last_roof_y];
            int v34 = buildFid(4, 1, 0, 0, 0) == v31;

            if (v34 != _obj_last_is_empty || (((v16 >> 16) & 0xF000) >> 12) != (((v32 >> 16) & 0xF000) >> 12)) {
                if (_obj_last_is_empty == 0) {
                    _tile_fill_roof(_obj_last_roof_x, _obj_last_roof_y, elevation, 1);
                }

                if (v34 == 0) {
                    _tile_fill_roof(v14, v15, elevation, 0);
                }

                if (rect != NULL) {
                    rectUnion(rect, &_scr_size, rect);
                }
            }

            _obj_last_roof_x = v14;
            _obj_last_roof_y = v15;
            _obj_last_elev = elevation;
            _obj_last_is_empty = v34;
        }

        if (rect != NULL) {
            Rect r;
            objectSetLocation(gEgg, tile, elevation, &r);
            rectUnion(rect, &r, rect);
        } else {
            objectSetLocation(gEgg, tile, elevation, 0);
        }

        if (elevation != oldElevation) {
            mapSetElevation(elevation);
            tileSetCenter(tile, TILE_SET_CENTER_FLAG_0x01 | TILE_SET_CENTER_FLAG_0x02);
            if (isInCombat()) {
                _game_user_wants_to_quit = 1;
            }
        }
    } else {
        if (elevation != _obj_last_elev && (obj->pid >> 24) == OBJ_TYPE_CRITTER) {
            _combat_delete_critter(obj);
        }
    }

    return 0;
}

// 0x48A9A0
int _obj_reset_roof()
{
    int fid = buildFid(4, (_square[gDude->elevation]->field_0[_obj_last_roof_x + 100 * _obj_last_roof_y] >> 16) & 0xFFF, 0, 0, 0);
    if (fid != buildFid(4, 1, 0, 0, 0)) {
        _tile_fill_roof(_obj_last_roof_x, _obj_last_roof_y, gDude->elevation, 1);
    }
    return 0;
}

// Sets object fid.
//
// 0x48AA3C
int objectSetFid(Object* obj, int fid, Rect* dirtyRect)
{
    Rect new_rect;

    if (obj == NULL) {
        return -1;
    }

    if (dirtyRect != NULL) {
        objectGetRect(obj, dirtyRect);

        obj->fid = fid;

        objectGetRect(obj, &new_rect);
        rectUnion(dirtyRect, &new_rect, dirtyRect);
    } else {
        obj->fid = fid;
    }

    return 0;
}

// Sets object frame.
//
// 0x48AA84
int objectSetFrame(Object* obj, int frame, Rect* rect)
{
    Rect new_rect;
    Art* art;
    CacheEntry* cache_entry;
    int framesPerDirection;

    if (obj == NULL) {
        return -1;
    }

    art = artLock(obj->fid, &cache_entry);
    if (art == NULL) {
        return -1;
    }

    framesPerDirection = art->frameCount;

    artUnlock(cache_entry);

    if (frame >= framesPerDirection) {
        return -1;
    }

    if (rect != NULL) {
        objectGetRect(obj, rect);
        obj->frame = frame;
        objectGetRect(obj, &new_rect);
        rectUnion(rect, &new_rect, rect);
    } else {
        obj->frame = frame;
    }

    return 0;
}

// 0x48AAF0
int objectSetNextFrame(Object* obj, Rect* dirtyRect)
{
    Art* art;
    CacheEntry* cache_entry;
    int framesPerDirection;
    int nextFrame;

    if (obj == NULL) {
        return -1;
    }

    art = artLock(obj->fid, &cache_entry);
    if (art == NULL) {
        return -1;
    }

    framesPerDirection = art->frameCount;

    artUnlock(cache_entry);

    nextFrame = obj->frame + 1;
    if (nextFrame >= framesPerDirection) {
        nextFrame = 0;
    }

    if (dirtyRect != NULL) {

        objectGetRect(obj, dirtyRect);

        obj->frame = nextFrame;

        Rect updatedRect;
        objectGetRect(obj, &updatedRect);
        rectUnion(dirtyRect, &updatedRect, dirtyRect);
    } else {
        obj->frame = nextFrame;
    }

    return 0;
}

// 0x48AB60
//
int objectSetPrevFrame(Object* obj, Rect* dirtyRect)
{
    Art* art;
    CacheEntry* cache_entry;
    int framesPerDirection;
    int prevFrame;
    Rect newRect;

    if (obj == NULL) {
        return -1;
    }

    art = artLock(obj->fid, &cache_entry);
    if (art == NULL) {
        return -1;
    }

    framesPerDirection = art->frameCount;

    artUnlock(cache_entry);

    prevFrame = obj->frame - 1;
    if (prevFrame < 0) {
        prevFrame = framesPerDirection - 1;
    }

    if (dirtyRect != NULL) {
        objectGetRect(obj, dirtyRect);
        obj->frame = prevFrame;
        objectGetRect(obj, &newRect);
        rectUnion(dirtyRect, &newRect, dirtyRect);
    } else {
        obj->frame = prevFrame;
    }

    return 0;
}

// 0x48ABD4
int objectSetRotation(Object* obj, int direction, Rect* dirtyRect)
{
    if (obj == NULL) {
        return -1;
    }

    if (direction >= ROTATION_COUNT) {
        return -1;
    }

    if (dirtyRect != NULL) {
        objectGetRect(obj, dirtyRect);
        obj->rotation = direction;

        Rect newRect;
        objectGetRect(obj, &newRect);
        rectUnion(dirtyRect, &newRect, dirtyRect);
    } else {
        obj->rotation = direction;
    }

    return 0;
}

// 0x48AC20
int objectRotateClockwise(Object* obj, Rect* dirtyRect)
{
    int rotation = obj->rotation + 1;
    if (rotation >= ROTATION_COUNT) {
        rotation = ROTATION_NE;
    }

    return objectSetRotation(obj, rotation, dirtyRect);
}

// 0x48AC38
int objectRotateCounterClockwise(Object* obj, Rect* dirtyRect)
{
    int rotation = obj->rotation - 1;
    if (rotation < 0) {
        rotation = ROTATION_NW;
    }

    return objectSetRotation(obj, rotation, dirtyRect);
}

// 0x48AC54
void _obj_rebuild_all_light()
{
    lightResetIntensity();

    for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
        ObjectListNode* objectListNode = gObjectListHeadByTile[tile];
        while (objectListNode != NULL) {
            _obj_adjust_light(objectListNode->obj, 0, NULL);
            objectListNode = objectListNode->next;
        }
    }
}

// 0x48AC90
int objectSetLight(Object* obj, int lightDistance, int lightIntensity, Rect* rect)
{
    int v7;
    Rect new_rect;

    if (obj == NULL) {
        return -1;
    }

    v7 = _obj_turn_off_light(obj, rect);
    if (lightIntensity > 0) {
        if (lightDistance >= 8) {
            lightDistance = 8;
        }

        obj->lightIntensity = lightIntensity;
        obj->lightDistance = lightDistance;

        if (rect != NULL) {
            v7 = _obj_turn_on_light(obj, &new_rect);
            rectUnion(rect, &new_rect, rect);
        } else {
            v7 = _obj_turn_on_light(obj, NULL);
        }
    } else {
        obj->lightIntensity = 0;
        obj->lightDistance = 0;
    }

    return v7;
}

// 0x48AD04
int objectGetLightIntensity(Object* obj)
{
    int lightLevel = lightGetLightLevel();
    int lightIntensity = lightGetIntensity(obj->elevation, obj->tile);

    if (obj == gDude) {
        lightIntensity -= gDude->lightIntensity;
    }

    if (lightIntensity >= lightLevel) {
        if (lightIntensity > 0x10000) {
            lightIntensity = 0x10000;
        }
    } else {
        lightIntensity = lightLevel;
    }

    return lightIntensity;
}

// 0x48AD48
int _obj_turn_on_light(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (obj->lightIntensity <= 0) {
        obj->flags &= ~OBJECT_LIGHTING;
        return -1;
    }

    if ((obj->flags & OBJECT_LIGHTING) == 0) {
        obj->flags |= OBJECT_LIGHTING;

        if (_obj_adjust_light(obj, 0, rect) == -1) {
            if (rect != NULL) {
                objectGetRect(obj, rect);
            }
        }
    }

    return 0;
}

// 0x48AD9C
int _obj_turn_off_light(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (obj->lightIntensity <= 0) {
        obj->flags &= ~OBJECT_LIGHTING;
        return -1;
    }

    if ((obj->flags & OBJECT_LIGHTING) != 0) {
        if (_obj_adjust_light(obj, 1, rect) == -1) {
            if (rect != NULL) {
                objectGetRect(obj, rect);
            }
        }

        obj->flags &= ~OBJECT_LIGHTING;
    }

    return 0;
}

// 0x48ADF0
int objectShow(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if ((obj->flags & OBJECT_HIDDEN) == 0) {
        return -1;
    }

    obj->flags &= ~OBJECT_HIDDEN;
    obj->outline &= ~OUTLINE_DISABLED;

    if (_obj_adjust_light(obj, 0, rect) == -1) {
        if (rect != NULL) {
            objectGetRect(obj, rect);
        }
    }

    if (obj == gDude) {
        if (rect != NULL) {
            Rect eggRect;
            objectGetRect(gEgg, &eggRect);
            rectUnion(rect, &eggRect, rect);
        }
    }

    return 0;
}

// 0x48AE68
int objectHide(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if ((object->flags & OBJECT_HIDDEN) != 0) {
        return -1;
    }

    if (_obj_adjust_light(object, 1, rect) == -1) {
        if (rect != NULL) {
            objectGetRect(object, rect);
        }
    }

    object->flags |= OBJECT_HIDDEN;

    if ((object->outline & OUTLINE_TYPE_MASK) != 0) {
        object->outline |= OUTLINE_DISABLED;
    }

    if (object == gDude) {
        if (rect != NULL) {
            Rect eggRect;
            objectGetRect(gEgg, &eggRect);
            rectUnion(rect, &eggRect, rect);
        }
    }

    return 0;
}

// 0x48AEE4
int objectEnableOutline(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    object->outline &= ~OUTLINE_DISABLED;

    if (rect != NULL) {
        objectGetRect(object, rect);
    }

    return 0;
}

// 0x48AF00
int objectDisableOutline(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if ((object->outline & OUTLINE_TYPE_MASK) != 0) {
        object->outline |= OUTLINE_DISABLED;
    }

    if (rect != NULL) {
        objectGetRect(object, rect);
    }

    return 0;
}

// 0x48AF2C
int _obj_toggle_flat(Object* object, Rect* rect)
{
    Rect v1;

    if (object == NULL) {
        return -1;
    }

    ObjectListNode* node;
    ObjectListNode* previousNode;
    if (objectGetListNode(object, &node, &previousNode) == -1) {
        return -1;
    }

    if (rect != NULL) {
        objectGetRect(object, rect);

        if (previousNode != NULL) {
            previousNode->next = node->next;
        } else {
            int tile_index = node->obj->tile;
            if (tile_index == -1) {
                gObjectListHead = gObjectListHead->next;
            } else {
                gObjectListHeadByTile[tile_index] = gObjectListHeadByTile[tile_index]->next;
            }
        }

        object->flags ^= OBJECT_FLAG_0x08;

        _obj_insert(node);
        objectGetRect(object, &v1);
        rectUnion(rect, &v1, rect);
    } else {
        if (previousNode != NULL) {
            previousNode->next = node->next;
        } else {
            int tile = node->obj->tile;
            if (tile == -1) {
                gObjectListHead = gObjectListHead->next;
            } else {
                gObjectListHeadByTile[tile] = gObjectListHeadByTile[tile]->next;
            }
        }

        object->flags ^= OBJECT_FLAG_0x08;

        _obj_insert(node);
    }

    return 0;
}

// 0x48B0FC
int objectDestroy(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    _gmouse_remove_item_outline(object);

    ObjectListNode* node;
    ObjectListNode* previousNode;
    if (objectGetListNode(object, &node, &previousNode) == 0) {
        if (_obj_adjust_light(object, 1, rect) == -1) {
            if (rect != NULL) {
                objectGetRect(object, rect);
            }
        }

        if (_obj_remove(node, previousNode) != 0) {
            return -1;
        }

        return 0;
    }

    // NOTE: Uninline.
    if (objectListNodeCreate(&node) == -1) {
        return -1;
    }

    node->obj = object;

    if (_obj_remove(node, node) == -1) {
        return -1;
    }

    return 0;
}

// 0x48B1B0
int _obj_inven_free(Inventory* inventory)
{
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);

        ObjectListNode* node;
        // NOTE: Uninline.
        objectListNodeCreate(&node);

        node->obj = inventoryItem->item;
        node->obj->flags &= ~OBJECT_FLAG_0x400;
        _obj_remove(node, node);

        inventoryItem->item = NULL;
    }

    if (inventory->items != NULL) {
        internal_free(inventory->items);
        inventory->items = NULL;
        inventory->capacity = 0;
        inventory->length = 0;
    }

    return 0;
}

// 0x48B24C
bool _obj_action_can_use(Object* obj)
{
    int pid = obj->pid;
    if (pid != PROTO_ID_LIT_FLARE && pid != PROTO_ID_DYNAMITE_II && pid != PROTO_ID_PLASTIC_EXPLOSIVES_II) {
        return _proto_action_can_use(pid);
    } else {
        return false;
    }
}

// 0x48B278
bool _obj_action_can_talk_to(Object* obj)
{
    return _proto_action_can_talk_to(obj->pid) && ((obj->pid >> 24) == OBJ_TYPE_CRITTER) && critterIsActive(obj);
}

// 0x48B2A8
bool _obj_portal_is_walk_thru(Object* obj)
{
    if ((obj->pid >> 24) != OBJ_TYPE_SCENERY) {
        return false;
    }

    Proto* proto;
    if (protoGetProto(obj->pid, &proto) == -1) {
        return false;
    }

    return (proto->scenery.data.generic.field_0 & 0x04) != 0;
}

// 0x48B2E8
Object* objectFindById(int a1)
{
    Object* obj = objectFindFirst();
    while (obj != NULL) {
        if (obj->id == a1) {
            return obj;
        }
        obj = objectFindNext();
    }

    return NULL;
}

// Returns root owner of given object.
//
// 0x48B304
Object* objectGetOwner(Object* object)
{
    Object* owner = object->owner;
    if (owner == NULL) {
        return NULL;
    }

    while (owner->owner != NULL) {
        owner = owner->owner;
    }

    return owner;
}

// 0x48B318
void _obj_remove_all()
{
    ObjectListNode* node;
    ObjectListNode* prev;
    ObjectListNode* next;

    _scr_remove_all();

    for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
        node = gObjectListHeadByTile[tile];
        prev = NULL;

        while (node != NULL) {
            next = node->next;
            if (_obj_remove(node, prev) == -1) {
                prev = node;
            }
            node = next;
        }
    }

    node = gObjectListHead;
    prev = NULL;

    while (node != NULL) {
        next = node->next;
        if (_obj_remove(node, prev) == -1) {
            prev = node;
        }
        node = next;
    }

    _obj_last_roof_y = -1;
    _obj_last_elev = -1;
    _obj_last_is_empty = 1;
    _obj_last_roof_x = -1;
}

// 0x48B3A8
Object* objectFindFirst()
{
    gObjectFindElevation = 0;

    ObjectListNode* objectListNode;
    for (gObjectFindTile = 0; gObjectFindTile < HEX_GRID_SIZE; gObjectFindTile++) {
        objectListNode = gObjectListHeadByTile[gObjectFindTile];
        if (objectListNode) {
            break;
        }
    }

    if (gObjectFindTile == HEX_GRID_SIZE) {
        gObjectFindLastObjectListNode = NULL;
        return NULL;
    }

    while (objectListNode != NULL) {
        if (artIsObjectTypeHidden((objectListNode->obj->fid & 0xF000000) >> 24) == 0) {
            gObjectFindLastObjectListNode = objectListNode;
            return objectListNode->obj;
        }
        objectListNode = objectListNode->next;
    }

    gObjectFindLastObjectListNode = NULL;
    return NULL;
}

// 0x48B41C
Object* objectFindNext()
{
    if (gObjectFindLastObjectListNode == NULL) {
        return NULL;
    }

    ObjectListNode* objectListNode = gObjectFindLastObjectListNode->next;

    while (gObjectFindTile < HEX_GRID_SIZE) {
        if (objectListNode == NULL) {
            objectListNode = gObjectListHeadByTile[gObjectFindTile++];
        }

        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (!artIsObjectTypeHidden((object->fid & 0xF000000) >> 24)) {
                gObjectFindLastObjectListNode = objectListNode;
                return object;
            }
            objectListNode = objectListNode->next;
        }
    }

    gObjectFindLastObjectListNode = NULL;
    return NULL;
}

// 0x48B48C
Object* objectFindFirstAtElevation(int elevation)
{
    gObjectFindElevation = elevation;
    gObjectFindTile = 0;

    for (gObjectFindTile = 0; gObjectFindTile < HEX_GRID_SIZE; gObjectFindTile++) {
        ObjectListNode* objectListNode = gObjectListHeadByTile[gObjectFindTile];
        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (object->elevation == elevation) {
                if (!artIsObjectTypeHidden((object->fid & 0xF000000) >> 24)) {
                    gObjectFindLastObjectListNode = objectListNode;
                    return object;
                }
            }
            objectListNode = objectListNode->next;
        }
    }

    gObjectFindLastObjectListNode = NULL;
    return NULL;
}

// 0x48B510
Object* objectFindNextAtElevation()
{
    if (gObjectFindLastObjectListNode == NULL) {
        return NULL;
    }

    ObjectListNode* objectListNode = gObjectFindLastObjectListNode->next;

    while (gObjectFindTile < HEX_GRID_SIZE) {
        if (objectListNode == NULL) {
            objectListNode = gObjectListHeadByTile[gObjectFindTile++];
        }

        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (object->elevation == gObjectFindElevation) {
                if (!artIsObjectTypeHidden((object->fid & 0xF000000) >> 24)) {
                    gObjectFindLastObjectListNode = objectListNode;
                    return object;
                }
            }
            objectListNode = objectListNode->next;
        }
    }

    gObjectFindLastObjectListNode = NULL;
    return NULL;
}

// 0x48B5A8
Object* objectFindFirstAtLocation(int elevation, int tile)
{
    gObjectFindElevation = elevation;
    gObjectFindTile = tile;

    ObjectListNode* objectListNode = gObjectListHeadByTile[tile];
    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if (object->elevation == elevation) {
            if (!artIsObjectTypeHidden((object->fid & 0xF000000) >> 24)) {
                gObjectFindLastObjectListNode = objectListNode;
                return object;
            }
        }
        objectListNode = objectListNode->next;
    }

    gObjectFindLastObjectListNode = NULL;
    return NULL;
}

// 0x48B608
Object* objectFindNextAtLocation()
{
    if (gObjectFindLastObjectListNode == NULL) {
        return NULL;
    }

    ObjectListNode* objectListNode = gObjectFindLastObjectListNode->next;

    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if (object->elevation == gObjectFindElevation) {
            if (!artIsObjectTypeHidden((object->fid & 0xF000000) >> 24)) {
                gObjectFindLastObjectListNode = objectListNode;
                return object;
            }
        }
        objectListNode = objectListNode->next;
    }

    gObjectFindLastObjectListNode = NULL;
    return NULL;
}

// 0x0x48B66C
void objectGetRect(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return;
    }

    if (rect == NULL) {
        return;
    }

    bool isOutlined = false;
    if ((obj->outline & OUTLINE_TYPE_MASK) != 0) {
        isOutlined = true;
    }

    CacheEntry* artHandle;
    Art* art = artLock(obj->fid, &artHandle);
    if (art == NULL) {
        rect->left = 0;
        rect->top = 0;
        rect->right = 0;
        rect->bottom = 0;
        return;
    }

    int width;
    int height;
    artGetSize(art, obj->frame, obj->rotation, &width, &height);

    if (obj->tile == -1) {
        rect->left = obj->sx;
        rect->top = obj->sy;
        rect->right = obj->sx + width - 1;
        rect->bottom = obj->sy + height - 1;
    } else {
        int tileScreenY;
        int tileScreenX;
        if (tileToScreenXY(obj->tile, &tileScreenX, &tileScreenY, obj->elevation) == 0) {
            tileScreenX += 16;
            tileScreenY += 8;

            tileScreenX += art->xOffsets[obj->rotation];
            tileScreenY += art->yOffsets[obj->rotation];

            tileScreenX += obj->x;
            tileScreenY += obj->y;

            rect->left = tileScreenX - width / 2;
            rect->top = tileScreenY - height + 1;
            rect->right = width + rect->left - 1;
            rect->bottom = tileScreenY;
        } else {
            rect->left = 0;
            rect->top = 0;
            rect->right = 0;
            rect->bottom = 0;
            isOutlined = false;
        }
    }

    artUnlock(artHandle);

    if (isOutlined) {
        rect->left--;
        rect->top--;
        rect->right++;
        rect->bottom++;
    }
}

// 0x48B7F8
bool _obj_occupied(int tile, int elevation)
{
    ObjectListNode* objectListNode = gObjectListHeadByTile[tile];
    while (objectListNode != NULL) {
        if (objectListNode->obj->elevation == elevation
            && objectListNode->obj != gGameMouseBouncingCursor
            && objectListNode->obj != gGameMouseHexCursor) {
            return true;
        }
        objectListNode = objectListNode->next;
    }

    return false;
}

// 0x48B848
Object* _obj_blocking_at(Object* a1, int tile, int elev)
{
    ObjectListNode* objectListNode;
    Object* v7;
    int type;

    if (!hexGridTileIsValid(tile)) {
        return NULL;
    }

    objectListNode = gObjectListHeadByTile[tile];
    while (objectListNode != NULL) {
        v7 = objectListNode->obj;
        if (v7->elevation == elev) {
            if ((v7->flags & OBJECT_HIDDEN) == 0 && (v7->flags & OBJECT_NO_BLOCK) == 0 && v7 != a1) {
                type = (v7->fid & 0xF000000) >> 24;
                if (type == OBJ_TYPE_CRITTER
                    || type == OBJ_TYPE_SCENERY
                    || type == OBJ_TYPE_WALL) {
                    return v7;
                }
            }
        }
        objectListNode = objectListNode->next;
    }

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int neighboor = tileGetTileInDirection(tile, rotation, 1);
        if (hexGridTileIsValid(neighboor)) {
            objectListNode = gObjectListHeadByTile[neighboor];
            while (objectListNode != NULL) {
                v7 = objectListNode->obj;
                if ((v7->flags & OBJECT_FLAG_0x800) != 0) {
                    if (v7->elevation == elev) {
                        if ((v7->flags & OBJECT_HIDDEN) == 0 && (v7->flags & OBJECT_NO_BLOCK) == 0 && v7 != a1) {
                            type = (v7->fid & 0xF000000) >> 24;
                            if (type == OBJ_TYPE_CRITTER
                                || type == OBJ_TYPE_SCENERY
                                || type == OBJ_TYPE_WALL) {
                                return v7;
                            }
                        }
                    }
                }
                objectListNode = objectListNode->next;
            }
        }
    }

    return NULL;
}

// 0x48B930
Object* _obj_shoot_blocking_at(Object* obj, int tile, int elev)
{
    if (!hexGridTileIsValid(tile)) {
        return NULL;
    }

    ObjectListNode* objectListItem = gObjectListHeadByTile[tile];
    while (objectListItem != NULL) {
        Object* candidate = objectListItem->obj;
        if (candidate->elevation == elev) {
            unsigned int flags = candidate->flags;
            if ((flags & OBJECT_HIDDEN) == 0 && ((flags & OBJECT_NO_BLOCK) == 0 || (flags & OBJECT_FLAG_0x80000000) == 0) && candidate != obj) {
                int type = (candidate->fid & 0xF000000) >> 24;
                // SFALL: Fix to prevent corpses from blocking line of fire.
                if ((type == OBJ_TYPE_CRITTER && !critterIsDead(candidate))
                    || type == OBJ_TYPE_SCENERY
                    || type == OBJ_TYPE_WALL) {
                    return candidate;
                }
            }
        }
        objectListItem = objectListItem->next;
    }

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int adjacentTile = tileGetTileInDirection(tile, rotation, 1);
        if (!hexGridTileIsValid(adjacentTile)) {
            continue;
        }

        ObjectListNode* objectListItem = gObjectListHeadByTile[adjacentTile];
        while (objectListItem != NULL) {
            Object* candidate = objectListItem->obj;
            unsigned int flags = candidate->flags;
            if ((flags & OBJECT_FLAG_0x800) != 0) {
                if (candidate->elevation == elev) {
                    if ((flags & OBJECT_HIDDEN) == 0 && (flags & OBJECT_NO_BLOCK) == 0 && candidate != obj) {
                        int type = (candidate->fid & 0xF000000) >> 24;
                        // SFALL: Fix to prevent corpses from blocking line of
                        // fire.
                        if ((type == OBJ_TYPE_CRITTER && !critterIsDead(candidate))
                            || type == OBJ_TYPE_SCENERY
                            || type == OBJ_TYPE_WALL) {
                            return candidate;
                        }
                    }
                }
            }
            objectListItem = objectListItem->next;
        }
    }

    return NULL;
}

// 0x48BA20
Object* _obj_ai_blocking_at(Object* a1, int tile, int elevation)
{
    if (!hexGridTileIsValid(tile)) {
        return NULL;
    }

    ObjectListNode* objectListNode = gObjectListHeadByTile[tile];
    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if (object->elevation == elevation) {
            if ((object->flags & OBJECT_HIDDEN) == 0
                && (object->flags & OBJECT_NO_BLOCK) == 0
                && object != a1) {
                int objectType = (object->fid & 0xF000000) >> 24;
                if (objectType == OBJ_TYPE_CRITTER
                    || objectType == OBJ_TYPE_SCENERY
                    || objectType == OBJ_TYPE_WALL) {
                    if (_moveBlockObj != NULL || objectType != OBJ_TYPE_CRITTER) {
                        return object;
                    }

                    _moveBlockObj = object;
                }
            }
        }
        objectListNode = objectListNode->next;
    }

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int candidate = tileGetTileInDirection(tile, rotation, 1);
        if (!hexGridTileIsValid(candidate)) {
            continue;
        }

        objectListNode = gObjectListHeadByTile[candidate];
        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if ((object->flags & OBJECT_FLAG_0x800) != 0) {
                if (object->elevation == elevation) {
                    if ((object->flags & OBJECT_HIDDEN) == 0
                        && (object->flags & OBJECT_NO_BLOCK) == 0
                        && object != a1) {
                        int objectType = (object->fid & 0xF000000) >> 24;
                        if (objectType == OBJ_TYPE_CRITTER
                            || objectType == OBJ_TYPE_SCENERY
                            || objectType == OBJ_TYPE_WALL) {
                            if (_moveBlockObj != NULL || objectType != OBJ_TYPE_CRITTER) {
                                return object;
                            }

                            _moveBlockObj = object;
                        }
                    }
                }
            }
            objectListNode = objectListNode->next;
        }
    }

    return NULL;
}

// 0x48BB44
int _obj_scroll_blocking_at(int tile, int elev)
{
    // TODO: Might be an error - why tile 0 is excluded?
    if (tile <= 0 || tile >= 40000) {
        return -1;
    }

    ObjectListNode* objectListNode = gObjectListHeadByTile[tile];
    while (objectListNode != NULL) {
        if (elev < objectListNode->obj->elevation) {
            break;
        }

        if (objectListNode->obj->elevation == elev && objectListNode->obj->pid == 0x500000C) {
            return 0;
        }

        objectListNode = objectListNode->next;
    }

    return -1;
}

// 0x48BB88
Object* _obj_sight_blocking_at(Object* a1, int tile, int elevation)
{
    ObjectListNode* objectListNode = gObjectListHeadByTile[tile];
    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if (object->elevation == elevation
            && (object->flags & OBJECT_HIDDEN) == 0
            && (object->flags & OBJECT_FLAG_0x20000000) == 0
            && object != a1) {
            int objectType = (object->fid & 0xF000000) >> 24;
            if (objectType == OBJ_TYPE_SCENERY || objectType == OBJ_TYPE_WALL) {
                return object;
            }
        }
        objectListNode = objectListNode->next;
    }

    return NULL;
}

// 0x48BBD4
int objectGetDistanceBetween(Object* object1, Object* object2)
{
    if (object1 == NULL || object2 == NULL) {
        return 0;
    }

    int distance = tileDistanceBetween(object1->tile, object2->tile);

    if ((object1->flags & OBJECT_FLAG_0x800) != 0) {
        distance -= 1;
    }

    if ((object2->flags & OBJECT_FLAG_0x800) != 0) {
        distance -= 1;
    }

    if (distance < 0) {
        distance = 0;
    }

    return distance;
}

// 0x48BC08
int objectGetDistanceBetweenTiles(Object* object1, int tile1, Object* object2, int tile2)
{
    if (object1 == NULL || object2 == NULL) {
        return 0;
    }

    int distance = tileDistanceBetween(tile1, tile2);

    if ((object1->flags & OBJECT_FLAG_0x800) != 0) {
        distance -= 1;
    }

    if ((object2->flags & OBJECT_FLAG_0x800) != 0) {
        distance -= 1;
    }

    if (distance < 0) {
        distance = 0;
    }

    return distance;
}

// 0x48BC38
int objectListCreate(int tile, int elevation, int objectType, Object*** objectListPtr)
{
    if (objectListPtr == NULL) {
        return -1;
    }

    int count = 0;
    if (tile == -1) {
        for (int index = 0; index < HEX_GRID_SIZE; index++) {
            ObjectListNode* objectListNode = gObjectListHeadByTile[index];
            while (objectListNode != NULL) {
                Object* obj = objectListNode->obj;
                if ((obj->flags & OBJECT_HIDDEN) == 0
                    && obj->elevation == elevation
                    && ((obj->fid & 0xF000000) >> 24) == objectType) {
                    count++;
                }
                objectListNode = objectListNode->next;
            }
        }
    } else {
        ObjectListNode* objectListNode = gObjectListHeadByTile[tile];
        while (objectListNode != NULL) {
            Object* obj = objectListNode->obj;
            if ((obj->flags & OBJECT_HIDDEN) == 0
                && obj->elevation == elevation
                && ((objectListNode->obj->fid & 0xF000000) >> 24) == objectType) {
                count++;
            }
            objectListNode = objectListNode->next;
        }
    }

    if (count == 0) {
        return 0;
    }

    Object** objects = *objectListPtr = (Object**)internal_malloc(sizeof(*objects) * count);
    if (objects == NULL) {
        return -1;
    }

    if (tile == -1) {
        for (int index = 0; index < HEX_GRID_SIZE; index++) {
            ObjectListNode* objectListNode = gObjectListHeadByTile[index];
            while (objectListNode) {
                Object* obj = objectListNode->obj;
                if ((obj->flags & OBJECT_HIDDEN) == 0
                    && obj->elevation == elevation
                    && ((obj->fid & 0xF000000) >> 24) == objectType) {
                    *objects++ = obj;
                }
                objectListNode = objectListNode->next;
            }
        }
    } else {
        ObjectListNode* objectListNode = gObjectListHeadByTile[tile];
        while (objectListNode != NULL) {
            Object* obj = objectListNode->obj;
            if ((obj->flags & OBJECT_HIDDEN) == 0
                && obj->elevation == elevation
                && ((obj->fid & 0xF000000) >> 24) == objectType) {
                *objects++ = obj;
            }
            objectListNode = objectListNode->next;
        }
    }

    return count;
}

// 0x48BDCC
void objectListFree(Object** objectList)
{
    if (objectList != NULL) {
        internal_free(objectList);
    }
}

// 0x48BDD8
void _translucent_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, unsigned char* a9, unsigned char* a10)
{
    dest += destPitch * destY + destX;
    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            // TODO: Probably wrong.
            unsigned char v1 = a10[*src];
            unsigned char* v2 = a9 + (v1 << 8);
            unsigned char v3 = *dest;

            *dest = v2[v3];

            src++;
            dest++;
        }

        src += srcStep;
        dest += destStep;
    }
}

// 0x48BEFC
void _dark_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, int light)
{
    unsigned char* sp = src;
    unsigned char* dp = dest + destPitch * destY + destX;

    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;
    // TODO: Name might be confusing.
    int lightModifier = light >> 9;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            unsigned char b = *sp;
            if (b != 0) {
                if (b < 0xE5) {
                    int t = (b << 8) + lightModifier;
                    b = _intensityColorTable[t];
                }

                *dp = b;
            }

            sp++;
            dp++;
        }

        sp += srcStep;
        dp += destStep;
    }
}

// 0x48BF88
void _dark_translucent_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, int light, unsigned char* a10, unsigned char* a11)
{
    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;
    int lightModifier = light >> 9;

    dest += destPitch * destY + destX;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            unsigned char srcByte = *src;
            if (srcByte != 0) {
                unsigned char destByte = *dest;
                unsigned int index = a11[srcByte] << 8;
                index = a10[index + destByte];
                index <<= 8;
                index += lightModifier;
                *dest = _intensityColorTable[index];
            }

            src++;
            dest++;
        }

        src += srcStep;
        dest += destStep;
    }
}

// 0x48C03C
void _intensity_mask_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destPitch, unsigned char* mask, int maskPitch, int light)
{
    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;
    int maskStep = maskPitch - srcWidth;
    light >>= 9;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            unsigned char b = *src;
            if (b != 0) {
                int off = (b << 8) + light;
                b = _intensityColorTable[off];
                unsigned char m = *mask;
                if (m != 0) {
                    unsigned char d = *dest;
                    int off = (d << 8) + 128 - m;
                    int q = _intensityColorTable[off];

                    off = (b << 8) + m;
                    m = _intensityColorTable[off];

                    off = (m << 8) + q;
                    b = _colorMixAddTable[off];
                }
                *dest = b;
            }

            src++;
            dest++;
            mask++;
        }

        src += srcStep;
        dest += destStep;
        mask += maskStep;
    }
}

// 0x48C2B4
int objectSetOutline(Object* obj, int outlineType, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if ((obj->outline & OUTLINE_TYPE_MASK) != 0) {
        return -1;
    }

    if ((obj->flags & OBJECT_FLAG_0x1000) != 0) {
        return -1;
    }

    obj->outline = outlineType;

    if ((obj->flags & OBJECT_HIDDEN) != 0) {
        obj->outline |= OUTLINE_DISABLED;
    }

    if (rect != NULL) {
        objectGetRect(obj, rect);
    }

    return 0;
}

// 0x48C2F0
int objectClearOutline(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if (rect != NULL) {
        objectGetRect(object, rect);
    }

    object->outline = 0;

    return 0;
}

// 0x48C340
int _obj_intersects_with(Object* object, int x, int y)
{
    int flags = 0;

    if (object == gEgg || (object->flags & OBJECT_HIDDEN) == 0) {
        CacheEntry* handle;
        Art* art = artLock(object->fid, &handle);
        if (art != NULL) {
            int width;
            int height;
            artGetSize(art, object->frame, object->rotation, &width, &height);

            int minX;
            int minY;
            int maxX;
            int maxY;
            if (object->tile == -1) {
                minX = object->sx;
                minY = object->sy;
                maxX = minX + width - 1;
                maxY = minY + height - 1;
            } else {
                int tileScreenX;
                int tileScreenY;
                tileToScreenXY(object->tile, &tileScreenX, &tileScreenY, object->elevation);
                tileScreenX += 16;
                tileScreenY += 8;

                tileScreenX += art->xOffsets[object->rotation];
                tileScreenY += art->yOffsets[object->rotation];

                tileScreenX += object->x;
                tileScreenY += object->y;

                minX = tileScreenX - width / 2;
                maxX = minX + width - 1;

                minY = tileScreenY - height + 1;
                maxY = tileScreenY;
            }

            if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                unsigned char* data = artGetFrameData(art, object->frame, object->rotation);
                if (data != NULL) {
                    if (data[width * (y - minY) + x - minX] != 0) {
                        flags |= 0x01;

                        if ((object->flags & OBJECT_FLAG_0xFC000) != 0) {
                            if ((object->flags & OBJECT_FLAG_0x8000) == 0) {
                                flags &= ~0x03;
                                flags |= 0x02;
                            }
                        } else {
                            int type = (object->fid & 0xF000000) >> 24;
                            if (type == OBJ_TYPE_SCENERY || type == OBJ_TYPE_WALL) {
                                Proto* proto;
                                protoGetProto(object->pid, &proto);

                                bool v20;
                                int extendedFlags = proto->scenery.extendedFlags;
                                if ((extendedFlags & 0x8000000) != 0 || (extendedFlags & 0x80000000) != 0) {
                                    v20 = tileIsInFrontOf(object->tile, gDude->tile);
                                } else if ((extendedFlags & 0x10000000) != 0) {
                                    // NOTE: Original code uses bitwise or, but given the fact that these functions return
                                    // bools, logical or is more suitable.
                                    v20 = tileIsInFrontOf(object->tile, gDude->tile) || tileIsToRightOf(gDude->tile, object->tile);
                                } else if ((extendedFlags & 0x20000000) != 0) {
                                    v20 = tileIsInFrontOf(object->tile, gDude->tile) && tileIsToRightOf(gDude->tile, object->tile);
                                } else {
                                    v20 = tileIsToRightOf(gDude->tile, object->tile);
                                }

                                if (v20) {
                                    if (_obj_intersects_with(gEgg, x, y) != 0) {
                                        flags |= 0x04;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            artUnlock(handle);
        }
    }

    return flags;
}

// 0x48C5C4
int _obj_create_intersect_list(int x, int y, int elevation, int objectType, ObjectWithFlags** entriesPtr)
{
    int v5 = tileFromScreenXY(x - 320, y - 240, elevation);
    *entriesPtr = NULL;

    if (_updateHexArea <= 0) {
        return 0;
    }

    int count = 0;

    int parity = gCenterTile & 1;
    for (int index = 0; index < _updateHexArea; index++) {
        int v7 = _orderTable[parity][index];
        if (_offsetDivTable[v7] < 30 && _offsetModTable[v7] < 20) {
            ObjectListNode* objectListNode = gObjectListHeadByTile[_offsetTable[parity][v7] + v5];
            while (objectListNode != NULL) {
                Object* object = objectListNode->obj;
                if (object->elevation > elevation) {
                    break;
                }

                if (object->elevation == elevation
                    && (objectType == -1 || (object->fid & 0xF000000) >> 24 == objectType)
                    && object != gEgg) {
                    int flags = _obj_intersects_with(object, x, y);
                    if (flags != 0) {
                        ObjectWithFlags* entries = (ObjectWithFlags*)internal_realloc(*entriesPtr, sizeof(*entries) * (count + 1));
                        if (entries != NULL) {
                            *entriesPtr = entries;
                            entries[count].object = object;
                            entries[count].flags = flags;
                            count++;
                        }
                    }
                }

                objectListNode = objectListNode->next;
            }
        }
    }

    return count;
}

// 0x48C74C
void _obj_delete_intersect_list(ObjectWithFlags** entriesPtr)
{
    if (entriesPtr != NULL && *entriesPtr != NULL) {
        internal_free(*entriesPtr);
        *entriesPtr = NULL;
    }
}

// 0x48C788
void _obj_clear_seen()
{
    memset(_obj_seen, 0, sizeof(_obj_seen));
}

// 0x48C7A0
void _obj_process_seen()
{
    int i;
    int v7;
    int v8;
    int v5;
    int v0;
    int v3;
    ObjectListNode* obj_entry;

    memset(_obj_seen_check, 0, 5001);

    v0 = 400;
    for (i = 0; i < 5001; i++) {
        if (_obj_seen[i] != 0) {
            for (v3 = i - 400; v3 != v0; v3 += 25) {
                if (v3 >= 0 && v3 < 5001) {
                    _obj_seen_check[v3] = -1;
                    if (v3 > 0) {
                        _obj_seen_check[v3 - 1] = -1;
                    }
                    if (v3 < 5000) {
                        _obj_seen_check[v3 + 1] = -1;
                    }
                    if (v3 > 1) {
                        _obj_seen_check[v3 - 2] = -1;
                    }
                    if (v3 < 4999) {
                        _obj_seen_check[v3 + 2] = -1;
                    }
                }
            }
        }
        v0++;
    }

    v7 = 0;
    for (i = 0; i < 5001; i++) {
        if (_obj_seen_check[i] != 0) {
            v8 = 1;
            for (v5 = v7; v5 < v7 + 8; v5++) {
                if (v8 & _obj_seen_check[i]) {
                    if (v5 < 40000) {
                        for (obj_entry = gObjectListHeadByTile[v5]; obj_entry != NULL; obj_entry = obj_entry->next) {
                            if (obj_entry->obj->elevation == gDude->elevation) {
                                obj_entry->obj->flags |= OBJECT_FLAG_0x40000000;
                            }
                        }
                    }
                }
                v8 *= 2;
            }
        }
        v7 += 8;
    }

    memset(_obj_seen, 0, 5001);
}

// 0x48C8E4
char* objectGetName(Object* obj)
{
    int objectType = (obj->fid & 0xF000000) >> 24;
    switch (objectType) {
    case OBJ_TYPE_ITEM:
        return itemGetName(obj);
    case OBJ_TYPE_CRITTER:
        return critterGetName(obj);
    default:
        return protoGetName(obj->pid);
    }
}

// 0x48C914
char* objectGetDescription(Object* obj)
{
    if (((obj->fid & 0xF000000) >> 24) == OBJ_TYPE_ITEM) {
        return itemGetDescription(obj);
    }

    return protoGetDescription(obj->pid);
}

// Warm objects cache?
//
// 0x48C938
void _obj_preload_art_cache(int flags)
{
    if (gObjectFids == NULL) {
        return;
    }

    unsigned char arr[4096];
    memset(arr, 0, sizeof(arr));

    if ((flags & 0x02) == 0) {
        for (int i = 0; i < SQUARE_GRID_SIZE; i++) {
            int v3 = _square[0]->field_0[i];
            arr[v3 & 0xFFF] = 1;
            arr[(v3 >> 16) & 0xFFF] = 1;
        }
    }

    if ((flags & 0x04) == 0) {
        for (int i = 0; i < SQUARE_GRID_SIZE; i++) {
            int v3 = _square[1]->field_0[i];
            arr[v3 & 0xFFF] = 1;
            arr[(v3 >> 16) & 0xFFF] = 1;
        }
    }

    if ((flags & 0x08) == 0) {
        for (int i = 0; i < SQUARE_GRID_SIZE; i++) {
            int v3 = _square[2]->field_0[i];
            arr[v3 & 0xFFF] = 1;
            arr[(v3 >> 16) & 0xFFF] = 1;
        }
    }

    qsort(gObjectFids, gObjectFidsLength, sizeof(*gObjectFids), _obj_preload_sort);

    int v11 = gObjectFidsLength;
    int v12 = gObjectFidsLength;

    if ((gObjectFids[v12 - 1] & 0xF000000) >> 24 == 3) {
        int v13 = 0;
        do {
            v11--;
            v13 = (gObjectFids[v12 - 1] & 0xF000000) >> 24;
            v12--;
        } while (v13 == 3);
        v11++;
    }

    CacheEntry* cache_handle;
    if (artLock(*gObjectFids, &cache_handle) != NULL) {
        artUnlock(cache_handle);
    }

    for (int i = 1; i < v11; i++) {
        if (gObjectFids[i - 1] != gObjectFids[i]) {
            if (artLock(gObjectFids[i], &cache_handle) != NULL) {
                artUnlock(cache_handle);
            }
        }
    }

    for (int i = 0; i < 4096; i++) {
        if (arr[i] != 0) {
            int fid = buildFid(4, i, 0, 0, 0);
            if (artLock(fid, &cache_handle) != NULL) {
                artUnlock(cache_handle);
            }
        }
    }

    for (int i = v11; i < gObjectFidsLength; i++) {
        if (gObjectFids[i - 1] != gObjectFids[i]) {
            if (artLock(gObjectFids[i], &cache_handle) != NULL) {
                artUnlock(cache_handle);
            }
        }
    }

    internal_free(gObjectFids);
    gObjectFids = NULL;

    gObjectFidsLength = 0;
}

// 0x48CB88
int _obj_offset_table_init()
{
    int i;

    if (_offsetTable[0] != NULL) {
        return -1;
    }

    if (_offsetTable[1] != NULL) {
        return -1;
    }

    _offsetTable[0] = (int*)internal_malloc(sizeof(int) * _updateHexArea);
    if (_offsetTable[0] == NULL) {
        goto err;
    }

    _offsetTable[1] = (int*)internal_malloc(sizeof(int) * _updateHexArea);
    if (_offsetTable[1] == NULL) {
        goto err;
    }

    for (int i = 0; i < 2; i++) {
        int tile = tileFromScreenXY(_updateAreaPixelBounds, dword_639D94, 0);
        if (tile != -1) {
            int* v5 = _offsetTable[gCenterTile & 1];
            int v20;
            int v19;
            tileToScreenXY(tile, &v20, &v19, 0);

            int v23 = 16;
            v20 += 16;
            v19 += 8;
            if (v20 > _updateAreaPixelBounds) {
                v23 = -v23;
            }

            int v6 = v20;
            for (int j = 0; j < _updateHexHeight; j++) {
                for (int m = 0; m < _updateHexWidth; m++) {
                    int t = tileFromScreenXY(v6, v19, 0);
                    if (t == -1) {
                        goto err;
                    }

                    v6 += 32;
                    *v5++ = t - tile;
                }

                v6 = v23 + v20;
                v19 += 12;
                v23 = -v23;
            }
        }

        if (tileSetCenter(gCenterTile + 1, 2) == -1) {
            goto err;
        }
    }

    _offsetDivTable = (int*)internal_malloc(sizeof(int) * _updateHexArea);
    if (_offsetDivTable == NULL) {
        goto err;
    }

    for (i = 0; i < _updateHexArea; i++) {
        _offsetDivTable[i] = i / _updateHexWidth;
    }

    _offsetModTable = (int*)internal_malloc(sizeof(int) * _updateHexArea);
    if (_offsetModTable == NULL) {
        goto err;
    }

    for (i = 0; i < _updateHexArea; i++) {
        _offsetModTable[i] = i % _updateHexWidth;
    }

    return 0;

err:
    _obj_offset_table_exit();

    return -1;
}

// 0x48CDA0
void _obj_offset_table_exit()
{
    if (_offsetModTable != NULL) {
        internal_free(_offsetModTable);
        _offsetModTable = NULL;
    }

    if (_offsetDivTable != NULL) {
        internal_free(_offsetDivTable);
        _offsetDivTable = NULL;
    }

    if (_offsetTable[1] != NULL) {
        internal_free(_offsetTable[1]);
        _offsetTable[1] = NULL;
    }

    if (_offsetTable[0] != NULL) {
        internal_free(_offsetTable[0]);
        _offsetTable[0] = NULL;
    }
}

// 0x48CE10
int _obj_order_table_init()
{
    if (_orderTable[0] != NULL || _orderTable[1] != NULL) {
        return -1;
    }

    _orderTable[0] = (int*)internal_malloc(sizeof(int) * _updateHexArea);
    if (_orderTable[0] == NULL) {
        goto err;
    }

    _orderTable[1] = (int*)internal_malloc(sizeof(int) * _updateHexArea);
    if (_orderTable[1] == NULL) {
        goto err;
    }

    for (int index = 0; index < _updateHexArea; index++) {
        _orderTable[0][index] = index;
        _orderTable[1][index] = index;
    }

    qsort(_orderTable[0], _updateHexArea, sizeof(int), _obj_order_comp_func_even);
    qsort(_orderTable[1], _updateHexArea, sizeof(int), _obj_order_comp_func_odd);

    return 0;

err:

    // NOTE: Uninline.
    _obj_order_table_exit();

    return -1;
}

// 0x48CF20
int _obj_order_comp_func_even(const void* a1, const void* a2)
{
    int v1 = *(int*)a1;
    int v2 = *(int*)a2;
    return _offsetTable[0][v1] - _offsetTable[0][v2];
}

// 0x48CF38
int _obj_order_comp_func_odd(const void* a1, const void* a2)
{
    int v1 = *(int*)a1;
    int v2 = *(int*)a2;
    return _offsetTable[1][v1] - _offsetTable[1][v2];
}

// NOTE: Inlined.
//
// 0x48CF50
void _obj_order_table_exit()
{
    if (_orderTable[1] != NULL) {
        internal_free(_orderTable[1]);
        _orderTable[1] = NULL;
    }

    if (_orderTable[0] != NULL) {
        internal_free(_orderTable[0]);
        _orderTable[0] = NULL;
    }
}

// 0x48CF8C
int _obj_render_table_init()
{
    if (_renderTable != NULL) {
        return -1;
    }

    _renderTable = (ObjectListNode**)internal_malloc(sizeof(*_renderTable) * _updateHexArea);
    if (_renderTable == NULL) {
        return -1;
    }

    for (int index = 0; index < _updateHexArea; index++) {
        _renderTable[index] = NULL;
    }

    return 0;
}

// NOTE: Inlined.
//
// 0x48D000
void _obj_render_table_exit()
{
    if (_renderTable != NULL) {
        internal_free(_renderTable);
        _renderTable = NULL;
    }
}

// 0x48D020
void _obj_light_table_init()
{
    for (int s = 0; s < 2; s++) {
        int v4 = gCenterTile + s;
        for (int i = 0; i < 6; i++) {
            int v15 = 8;
            int* p = _light_offsets[v4 & 1][i];
            for (int j = 0; j < 8; j++) {
                int tile = tileGetTileInDirection(v4, (i + 1) % 6, j);

                for (int m = 0; m < v15; m++) {
                    *p++ = tileGetTileInDirection(tile, i, m + 1) - v4;
                }

                v15--;
            }
        }
    }
}

// 0x48D1E4
void _obj_blend_table_init()
{
    for (int index = 0; index < 256; index++) {
        int r = (_Color2RGB_(index) & 0x7C00) >> 10;
        int g = (_Color2RGB_(index) & 0x3E0) >> 5;
        int b = _Color2RGB_(index) & 0x1F;
        _glassGrayTable[index] = ((r + 5 * g + 4 * b) / 10) >> 2;
        _commonGrayTable[index] = ((b + 3 * r + 6 * g) / 10) >> 2;
    }

    _glassGrayTable[0] = 0;
    _commonGrayTable[0] = 0;

    _wallBlendTable = _getColorBlendTable(_colorTable[25439]);
    _glassBlendTable = _getColorBlendTable(_colorTable[10239]);
    _steamBlendTable = _getColorBlendTable(_colorTable[32767]);
    _energyBlendTable = _getColorBlendTable(_colorTable[30689]);
    _redBlendTable = _getColorBlendTable(_colorTable[31744]);
}

// NOTE: Inlined.
//
// 0x48D2E8
void _obj_blend_table_exit()
{
    _freeColorBlendTable(_colorTable[25439]);
    _freeColorBlendTable(_colorTable[10239]);
    _freeColorBlendTable(_colorTable[32767]);
    _freeColorBlendTable(_colorTable[30689]);
    _freeColorBlendTable(_colorTable[31744]);
}

// 0x48D348
int _obj_save_obj(File* stream, Object* object)
{
    if ((object->flags & OBJECT_TEMPORARY) != 0) {
        return 0;
    }

    CritterCombatData* combatData = NULL;
    Object* whoHitMe = NULL;
    if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
        combatData = &(object->data.critter.combat);
        whoHitMe = combatData->whoHitMe;
        if (whoHitMe != 0) {
            if (combatData->whoHitMeCid != -1) {
                combatData->whoHitMeCid = whoHitMe->cid;
            }
        } else {
            combatData->whoHitMeCid = -1;
        }
    }

    if (objectWrite(object, stream) == -1) {
        return -1;
    }

    if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
        combatData->whoHitMe = whoHitMe;
    }

    Inventory* inventory = &(object->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);

        if (fileWriteInt32(stream, inventoryItem->quantity) == -1) {
            return -1;
        }

        if (_obj_save_obj(stream, inventoryItem->item) == -1) {
            return -1;
        }

        if ((inventoryItem->item->flags & OBJECT_TEMPORARY) != 0) {
            return -1;
        }
    }

    return 0;
}

// 0x48D414
int _obj_load_obj(File* stream, Object** objectPtr, int elevation, Object* owner)
{
    Object* obj;

    if (objectAllocate(&obj) == -1) {
        *objectPtr = NULL;
        return -1;
    }

    if (objectRead(obj, stream) != 0) {
        *objectPtr = NULL;
        return -1;
    }

    if (obj->sid != -1) {
        Script* script;
        if (scriptGetScript(obj->sid, &script) == -1) {
            obj->sid = -1;
        } else {
            script->owner = obj;
        }
    }

    _obj_fix_violence_settings(&(obj->fid));

    if (!_art_fid_valid(obj->fid)) {
        debugPrint("\nError: invalid object art fid: %u\n", obj->fid);
        // NOTE: Uninline.
        objectDeallocate(&obj);
        return -2;
    }

    if (elevation == -1) {
        elevation = obj->elevation;
    } else {
        obj->elevation = elevation;
    }

    obj->owner = owner;

    Inventory* inventory = &(obj->data.inventory);
    if (inventory->length <= 0) {
        inventory->capacity = 0;
        inventory->items = NULL;
        *objectPtr = obj;
        return 0;
    }

    InventoryItem* inventoryItems = inventory->items = (InventoryItem*)internal_malloc(sizeof(*inventoryItems) * inventory->capacity);
    if (inventoryItems == NULL) {
        return -1;
    }

    for (int inventoryItemIndex = 0; inventoryItemIndex < inventory->length; inventoryItemIndex++) {
        InventoryItem* inventoryItem = &(inventoryItems[inventoryItemIndex]);
        if (fileReadInt32(stream, &(inventoryItem->quantity)) != 0) {
            return -1;
        }

        if (_obj_load_obj(stream, &(inventoryItem->item), elevation, obj) != 0) {
            return -1;
        }
    }

    *objectPtr = obj;

    return 0;
}

// obj_save_dude
// 0x48D59C
int _obj_save_dude(File* stream)
{
    int field_78 = gDude->sid;

    gDude->flags &= ~OBJECT_TEMPORARY;
    gDude->sid = -1;

    int rc = _obj_save_obj(stream, gDude);

    gDude->sid = field_78;
    gDude->flags |= OBJECT_TEMPORARY;

    if (fileWriteInt32(stream, gCenterTile) == -1) {
        fileClose(stream);
        return -1;
    }

    return rc;
}

// obj_load_dude
// 0x48D600
int _obj_load_dude(File* stream)
{
    int savedTile = gDude->tile;
    int savedElevation = gDude->elevation;
    int savedRotation = gDude->rotation;
    int savedOid = gDude->id;

    scriptsClearDudeScript();

    Object* temp;
    int rc = _obj_load_obj(stream, &temp, -1, NULL);

    memcpy(gDude, temp, sizeof(*gDude));

    gDude->flags |= OBJECT_TEMPORARY;

    scriptsClearDudeScript();

    gDude->id = savedOid;

    scriptsSetDudeScript();

    int newTile = gDude->tile;
    gDude->tile = savedTile;

    int newElevation = gDude->elevation;
    gDude->elevation = savedElevation;

    int newRotation = gDude->rotation;
    gDude->rotation = newRotation;

    scriptsSetDudeScript();

    if (rc != -1) {
        objectSetLocation(gDude, newTile, newElevation, NULL);
        objectSetRotation(gDude, newRotation, NULL);
    }

    // Set ownership of inventory items from temporary instance to dude.
    Inventory* inventory = &(gDude->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        inventoryItem->item->owner = gDude;
    }

    _obj_fix_combat_cid_for_dude();

    // Dude has claimed ownership of items in temporary instance's inventory.
    // We don't need object's dealloc routine to remove these items from the
    // game, so simply nullify temporary inventory as if nothing was there.
    Inventory* tempInventory = &(temp->data.inventory);
    tempInventory->length = 0;
    tempInventory->capacity = 0;
    tempInventory->items = NULL;

    temp->flags &= ~OBJECT_FLAG_0x400;

    if (objectDestroy(temp, NULL) == -1) {
        debugPrint("\nError: obj_load_dude: Can't destroy temp object!\n");
    }

    _inven_reset_dude();

    int tile;
    if (fileReadInt32(stream, &tile) == -1) {
        fileClose(stream);
        return -1;
    }

    tileSetCenter(tile, TILE_SET_CENTER_FLAG_0x01 | TILE_SET_CENTER_FLAG_0x02);

    return rc;
}

// 0x48D778
int objectAllocate(Object** objectPtr)
{
    if (objectPtr == NULL) {
        return -1;
    }

    Object* object = *objectPtr = (Object*)internal_malloc(sizeof(Object));
    if (object == NULL) {
        return -1;
    }

    memset(object, 0, sizeof(Object));

    object->id = -1;
    object->tile = -1;
    object->cid = -1;
    object->outline = 0;
    object->pid = -1;
    object->sid = -1;
    object->owner = NULL;
    object->field_80 = -1;

    return 0;
}

// NOTE: Inlined.
//
// 0x48D7F8
void objectDeallocate(Object** objectPtr)
{
    if (objectPtr == NULL) {
        return;
    }

    if (*objectPtr == NULL) {
        return;
    }

    internal_free(*objectPtr);

    *objectPtr = NULL;
}

// NOTE: Inlined.
//
// 0x48D818
int objectListNodeCreate(ObjectListNode** nodePtr)
{
    if (nodePtr == NULL) {
        return -1;
    }

    ObjectListNode* node = *nodePtr = (ObjectListNode*)internal_malloc(sizeof(*node));
    if (node == NULL) {
        return -1;
    }

    node->obj = NULL;
    node->next = NULL;

    return 0;
}

// NOTE: Inlined.
//
// 0x48D84C
void objectListNodeDestroy(ObjectListNode** nodePtr)
{
    if (nodePtr == NULL) {
        return;
    }

    if (*nodePtr == NULL) {
        return;
    }

    internal_free(*nodePtr);

    *nodePtr = NULL;
}

// 0x48D86C
int objectGetListNode(Object* object, ObjectListNode** nodePtr, ObjectListNode** previousNodePtr)
{
    if (object == NULL) {
        return -1;
    }

    if (nodePtr == NULL) {
        return -1;
    }

    int tile = object->tile;
    if (tile != -1) {
        *nodePtr = gObjectListHeadByTile[tile];
    } else {
        *nodePtr = gObjectListHead;
    }

    if (previousNodePtr != NULL) {
        *previousNodePtr = NULL;
        while (*nodePtr != NULL) {
            if (object == (*nodePtr)->obj) {
                break;
            }

            *previousNodePtr = *nodePtr;

            *nodePtr = (*nodePtr)->next;
        }
    } else {
        while (*nodePtr != NULL) {
            if (object == (*nodePtr)->obj) {
                break;
            }

            *nodePtr = (*nodePtr)->next;
        }
    }

    if (*nodePtr != NULL) {
        return 0;
    }

    return -1;
}

// 0x48D8E8
void _obj_insert(ObjectListNode* objectListNode)
{
    ObjectListNode** objectListNodePtr;

    if (objectListNode == NULL) {
        return;
    }

    if (objectListNode->obj->tile == -1) {
        objectListNodePtr = &gObjectListHead;
    } else {
        Art* art = NULL;
        CacheEntry* cacheHandle = NULL;

        objectListNodePtr = &(gObjectListHeadByTile[objectListNode->obj->tile]);

        while (*objectListNodePtr != NULL) {
            Object* obj = (*objectListNodePtr)->obj;
            if (obj->elevation > objectListNode->obj->elevation) {
                break;
            }

            if (obj->elevation == objectListNode->obj->elevation) {
                if ((obj->flags & OBJECT_FLAG_0x08) == 0 && (objectListNode->obj->flags & OBJECT_FLAG_0x08) != 0) {
                    break;
                }

                if ((obj->flags & OBJECT_FLAG_0x08) == (objectListNode->obj->flags & OBJECT_FLAG_0x08)) {
                    bool v11 = false;
                    CacheEntry* a2;
                    Art* v12 = artLock(obj->fid, &a2);
                    if (v12 != NULL) {

                        if (art == NULL) {
                            art = artLock(objectListNode->obj->fid, &cacheHandle);
                        }

                        // TODO: Incomplete.

                        artUnlock(a2);

                        if (v11) {
                            break;
                        }
                    }
                }
            }

            objectListNodePtr = &((*objectListNodePtr)->next);
        }

        if (art != NULL) {
            artUnlock(cacheHandle);
        }
    }

    objectListNode->next = *objectListNodePtr;
    *objectListNodePtr = objectListNode;
}

// 0x48DA58
int _obj_remove(ObjectListNode* a1, ObjectListNode* a2)
{
    if (a1->obj == NULL) {
        return -1;
    }

    if ((a1->obj->flags & OBJECT_FLAG_0x400) != 0) {
        return -1;
    }

    _obj_inven_free(&(a1->obj->data.inventory));

    if (a1->obj->sid != -1) {
        scriptExecProc(a1->obj->sid, SCRIPT_PROC_DESTROY);
        scriptRemove(a1->obj->sid);
    }

    if (a1 != a2) {
        if (a2 != NULL) {
            a2->next = a1->next;
        } else {
            int tile = a1->obj->tile;
            if (tile == -1) {
                gObjectListHead = gObjectListHead->next;
            } else {
                gObjectListHeadByTile[tile] = gObjectListHeadByTile[tile]->next;
            }
        }
    }

    // NOTE: Uninline.
    objectDeallocate(&(a1->obj));

    // NOTE: Uninline.
    objectListNodeDestroy(&a1);

    return 0;
}

// 0x48DB28
int _obj_connect_to_tile(ObjectListNode* node, int tile, int elevation, Rect* rect)
{
    if (node == NULL) {
        return -1;
    }

    if (!hexGridTileIsValid(tile)) {
        return -1;
    }

    if (!elevationIsValid(elevation)) {
        return -1;
    }

    node->obj->tile = tile;
    node->obj->elevation = elevation;
    node->obj->x = 0;
    node->obj->y = 0;
    node->obj->owner = 0;

    _obj_insert(node);

    if (_obj_adjust_light(node->obj, 0, rect) == -1) {
        if (rect != NULL) {
            objectGetRect(node->obj, rect);
        }
    }

    return 0;
}

// 0x48DC28
int _obj_adjust_light(Object* obj, int a2, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (obj->lightIntensity <= 0) {
        return -1;
    }

    if ((obj->flags & OBJECT_HIDDEN) != 0) {
        return -1;
    }

    if ((obj->flags & OBJECT_LIGHTING) == 0) {
        return -1;
    }

    if (!hexGridTileIsValid(obj->tile)) {
        return -1;
    }

    AdjustLightIntensityProc* adjustLightIntensity = a2 ? lightDecreaseIntensity : lightIncreaseIntensity;
    adjustLightIntensity(obj->elevation, obj->tile, obj->lightIntensity);

    Rect objectRect;
    objectGetRect(obj, &objectRect);

    if (obj->lightDistance > 8) {
        obj->lightDistance = 8;
    }

    if (obj->lightIntensity > 65536) {
        obj->lightIntensity = 65536;
    }

    int(*v70)[36] = _light_offsets[obj->tile & 1];
    int v7 = (obj->lightIntensity - 655) / (obj->lightDistance + 1);
    int v28[36];
    v28[0] = obj->lightIntensity - v7;
    v28[1] = v28[0] - v7;
    v28[8] = v28[0] - v7;
    v28[2] = v28[0] - v7 - v7;
    v28[9] = v28[2];
    v28[15] = v28[0] - v7 - v7;
    v28[3] = v28[2] - v7;
    v28[10] = v28[2] - v7;
    v28[16] = v28[2] - v7;
    v28[21] = v28[2] - v7;
    v28[4] = v28[2] - v7 - v7;
    v28[11] = v28[4];
    v28[17] = v28[2] - v7 - v7;
    v28[22] = v28[2] - v7 - v7;
    v28[26] = v28[2] - v7 - v7;
    v28[5] = v28[4] - v7;
    v28[12] = v28[4] - v7;
    v28[18] = v28[4] - v7;
    v28[23] = v28[4] - v7;
    v28[27] = v28[4] - v7;
    v28[30] = v28[4] - v7;
    v28[6] = v28[4] - v7 - v7;
    v28[13] = v28[6];
    v28[19] = v28[4] - v7 - v7;
    v28[24] = v28[4] - v7 - v7;
    v28[28] = v28[4] - v7 - v7;
    v28[31] = v28[4] - v7 - v7;
    v28[33] = v28[4] - v7 - v7;
    v28[7] = v28[6] - v7;
    v28[14] = v28[6] - v7;
    v28[20] = v28[6] - v7;
    v28[25] = v28[6] - v7;
    v28[29] = v28[6] - v7;
    v28[32] = v28[6] - v7;
    v28[34] = v28[6] - v7;
    v28[35] = v28[6] - v7;

    for (int index = 0; index < 36; index++) {
        if (obj->lightDistance >= _light_distance[index]) {
            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                int v14;
                int nextRotation = (rotation + 1) % ROTATION_COUNT;
                int eax;
                int edx;
                int ebx;
                int esi;
                int edi;
                switch (index) {
                case 0:
                    v14 = 0;
                    break;
                case 1:
                    v14 = _light_blocked[rotation][0];
                    break;
                case 2:
                    v14 = _light_blocked[rotation][1];
                    break;
                case 3:
                    v14 = _light_blocked[rotation][2];
                    break;
                case 4:
                    v14 = _light_blocked[rotation][3];
                    break;
                case 5:
                    v14 = _light_blocked[rotation][4];
                    break;
                case 6:
                    v14 = _light_blocked[rotation][5];
                    break;
                case 7:
                    v14 = _light_blocked[rotation][6];
                    break;
                case 8:
                    v14 = _light_blocked[rotation][0] & _light_blocked[nextRotation][0];
                    break;
                case 9:
                    v14 = _light_blocked[rotation][1] & _light_blocked[rotation][8];
                    break;
                case 10:
                    v14 = _light_blocked[rotation][2] & _light_blocked[rotation][9];
                    break;
                case 11:
                    v14 = _light_blocked[rotation][3] & _light_blocked[rotation][10];
                    break;
                case 12:
                    v14 = _light_blocked[rotation][4] & _light_blocked[rotation][11];
                    break;
                case 13:
                    v14 = _light_blocked[rotation][5] & _light_blocked[rotation][12];
                    break;
                case 14:
                    v14 = _light_blocked[rotation][6] & _light_blocked[rotation][13];
                    break;
                case 15:
                    v14 = _light_blocked[rotation][8] & _light_blocked[nextRotation][1];
                    break;
                case 16:
                    v14 = _light_blocked[rotation][8] | (_light_blocked[rotation][9] & _light_blocked[rotation][15]);
                    break;
                case 17:
                    edx = _light_blocked[rotation][9];
                    edx |= _light_blocked[rotation][10];
                    ebx = _light_blocked[rotation][8];
                    esi = _light_blocked[rotation][16];
                    ebx &= edx;
                    edx &= esi;
                    edi = _light_blocked[rotation][15];
                    ebx |= edx;
                    edx = _light_blocked[rotation][10];
                    eax = _light_blocked[rotation][9];
                    edx |= edi;
                    eax &= edx;
                    v14 = ebx | eax;
                    break;
                case 18:
                    edx = _light_blocked[rotation][0];
                    ebx = _light_blocked[rotation][9];
                    esi = _light_blocked[rotation][10];
                    edx |= ebx;
                    edi = _light_blocked[rotation][11];
                    edx |= esi;
                    ebx = _light_blocked[rotation][17];
                    edx |= edi;
                    ebx &= edx;
                    edx = esi;
                    esi = _light_blocked[rotation][16];
                    edi = _light_blocked[rotation][9];
                    edx &= esi;
                    edx |= edi;
                    edx |= ebx;
                    v14 = edx;
                    break;
                case 19:
                    edx = _light_blocked[rotation][17];
                    edi = _light_blocked[rotation][18];
                    ebx = _light_blocked[rotation][11];
                    edx |= edi;
                    esi = _light_blocked[rotation][10];
                    ebx &= edx;
                    edx = _light_blocked[rotation][9];
                    edx |= esi;
                    ebx |= edx;
                    edx = _light_blocked[rotation][12];
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 20:
                    edx = _light_blocked[rotation][2];
                    esi = _light_blocked[rotation][11];
                    edi = _light_blocked[rotation][12];
                    ebx = _light_blocked[rotation][8];
                    edx |= esi;
                    esi = _light_blocked[rotation][9];
                    edx |= edi;
                    edi = _light_blocked[rotation][10];
                    ebx &= edx;
                    edx &= esi;
                    esi = _light_blocked[rotation][17];
                    ebx |= edx;
                    edx = _light_blocked[rotation][16];
                    ebx |= edi;
                    edi = _light_blocked[rotation][18];
                    edx |= esi;
                    esi = _light_blocked[rotation][19];
                    edx |= edi;
                    eax = _light_blocked[rotation][11];
                    edx |= esi;
                    eax &= edx;
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 21:
                    v14 = (_light_blocked[rotation][8] & _light_blocked[nextRotation][1])
                        | (_light_blocked[rotation][15] & _light_blocked[nextRotation][2]);
                    break;
                case 22:
                    edx = _light_blocked[nextRotation][1];
                    ebx = _light_blocked[rotation][15];
                    esi = _light_blocked[rotation][21];
                    edx |= ebx;
                    ebx = _light_blocked[rotation][8];
                    edx |= esi;
                    ebx &= edx;
                    edx = _light_blocked[rotation][9];
                    edi = esi;
                    edx |= esi;
                    esi = _light_blocked[rotation][15];
                    edx &= esi;
                    ebx |= edx;
                    edx = esi;
                    esi = _light_blocked[rotation][16];
                    edx |= edi;
                    edx &= esi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 23:
                    edx = _light_blocked[rotation][3];
                    ebx = _light_blocked[rotation][16];
                    esi = _light_blocked[rotation][15];
                    ebx |= edx;
                    edx = _light_blocked[rotation][9];
                    edx &= esi;
                    edi = _light_blocked[rotation][22];
                    ebx |= edx;
                    edx = _light_blocked[rotation][17];
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 24:
                    edx = _light_blocked[rotation][0];
                    edi = _light_blocked[rotation][9];
                    ebx = _light_blocked[rotation][10];
                    edx |= edi;
                    esi = _light_blocked[rotation][17];
                    edx |= ebx;
                    edi = _light_blocked[rotation][18];
                    edx |= esi;
                    ebx = _light_blocked[rotation][16];
                    edx |= edi;
                    esi = _light_blocked[rotation][16];
                    ebx &= edx;
                    edx = _light_blocked[rotation][15];
                    edi = _light_blocked[rotation][23];
                    edx |= esi;
                    esi = _light_blocked[rotation][9];
                    edx |= edi;
                    edi = _light_blocked[rotation][8];
                    edx &= esi;
                    edx |= edi;
                    esi = _light_blocked[rotation][22];
                    ebx |= edx;
                    edx = _light_blocked[rotation][15];
                    edi = _light_blocked[rotation][23];
                    edx |= esi;
                    esi = _light_blocked[rotation][17];
                    edx |= edi;
                    edx &= esi;
                    ebx |= edx;
                    edx = _light_blocked[rotation][18];
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 25:
                    edx = _light_blocked[rotation][8];
                    edi = _light_blocked[rotation][15];
                    ebx = _light_blocked[rotation][16];
                    edx |= edi;
                    esi = _light_blocked[rotation][23];
                    edx |= ebx;
                    edi = _light_blocked[rotation][24];
                    edx |= esi;
                    ebx = _light_blocked[rotation][9];
                    edx |= edi;
                    esi = _light_blocked[rotation][1];
                    ebx &= edx;
                    edx = _light_blocked[rotation][8];
                    edx &= esi;
                    edi = _light_blocked[rotation][16];
                    ebx |= edx;
                    edx = _light_blocked[rotation][8];
                    esi = _light_blocked[rotation][17];
                    edx |= edi;
                    edi = _light_blocked[rotation][24];
                    esi |= edx;
                    esi |= edi;
                    esi &= _light_blocked[rotation][10];
                    edi = _light_blocked[rotation][23];
                    ebx |= esi;
                    esi = _light_blocked[rotation][17];
                    edx |= edi;
                    ebx |= esi;
                    esi = _light_blocked[rotation][24];
                    edi = _light_blocked[rotation][18];
                    edx |= esi;
                    edx &= edi;
                    esi = _light_blocked[rotation][19];
                    ebx |= edx;
                    edx = _light_blocked[rotation][0];
                    eax = _light_blocked[rotation][24];
                    edx |= esi;
                    eax &= edx;
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 26:
                    ebx = _light_blocked[rotation][8];
                    esi = _light_blocked[nextRotation][1];
                    edi = _light_blocked[nextRotation][2];
                    esi &= ebx;
                    ebx = _light_blocked[rotation][15];
                    ebx &= edi;
                    eax = _light_blocked[rotation][21];
                    ebx |= esi;
                    eax &= _light_blocked[nextRotation][3];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 27:
                    edx = _light_blocked[nextRotation][0];
                    edi = _light_blocked[rotation][15];
                    esi = _light_blocked[rotation][21];
                    edx |= edi;
                    edi = _light_blocked[rotation][26];
                    edx |= esi;
                    esi = _light_blocked[rotation][22];
                    edx |= edi;
                    edi = _light_blocked[nextRotation][1];
                    esi &= edx;
                    edx = _light_blocked[rotation][8];
                    ebx = _light_blocked[rotation][15];
                    edx &= edi;
                    edx |= ebx;
                    edi = _light_blocked[rotation][16];
                    esi |= edx;
                    edx = _light_blocked[rotation][8];
                    eax = _light_blocked[rotation][21];
                    edx |= edi;
                    eax &= edx;
                    esi |= eax;
                    v14 = esi;
                    break;
                case 28:
                    ebx = _light_blocked[rotation][9];
                    edi = _light_blocked[rotation][16];
                    esi = _light_blocked[rotation][23];
                    edx = _light_blocked[nextRotation][0];
                    ebx |= edi;
                    edi = _light_blocked[rotation][15];
                    ebx |= esi;
                    esi = _light_blocked[rotation][8];
                    ebx &= edi;
                    edi = _light_blocked[rotation][21];
                    ebx |= esi;
                    esi = _light_blocked[rotation][22];
                    edx |= edi;
                    edi = _light_blocked[rotation][27];
                    edx |= esi;
                    esi = _light_blocked[rotation][16];
                    edx |= edi;
                    edx &= esi;
                    edi = _light_blocked[rotation][17];
                    ebx |= edx;
                    edx = _light_blocked[rotation][9];
                    esi = _light_blocked[rotation][23];
                    edx |= edi;
                    edi = _light_blocked[rotation][22];
                    edx |= esi;
                    edx &= edi;
                    ebx |= edx;
                    edx = esi;
                    edx &= _light_blocked[rotation][27];
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 29:
                    edx = _light_blocked[rotation][8];
                    edi = _light_blocked[rotation][16];
                    ebx = _light_blocked[rotation][23];
                    edx |= edi;
                    esi = _light_blocked[rotation][15];
                    ebx |= edx;
                    edx = _light_blocked[rotation][9];
                    edx &= esi;
                    edi = _light_blocked[rotation][22];
                    ebx |= edx;
                    edx = _light_blocked[rotation][17];
                    edx &= edi;
                    esi = _light_blocked[rotation][28];
                    ebx |= edx;
                    edx = _light_blocked[rotation][24];
                    edx &= esi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 30:
                    ebx = _light_blocked[rotation][8];
                    esi = _light_blocked[nextRotation][1];
                    edi = _light_blocked[nextRotation][2];
                    esi &= ebx;
                    ebx = _light_blocked[rotation][15];
                    ebx &= edi;
                    edi = _light_blocked[nextRotation][3];
                    esi |= ebx;
                    ebx = _light_blocked[rotation][21];
                    ebx &= edi;
                    eax = _light_blocked[rotation][26];
                    ebx |= esi;
                    eax &= _light_blocked[nextRotation][4];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 31:
                    edx = _light_blocked[rotation][8];
                    esi = _light_blocked[nextRotation][1];
                    edi = _light_blocked[rotation][15];
                    edx &= esi;
                    ebx = _light_blocked[rotation][21];
                    edx |= edi;
                    esi = _light_blocked[rotation][22];
                    ebx |= edx;
                    edx = _light_blocked[rotation][8];
                    edi = _light_blocked[rotation][27];
                    edx |= esi;
                    esi = _light_blocked[rotation][26];
                    edx |= edi;
                    edx &= esi;
                    ebx |= edx;
                    edx = edi;
                    edx &= _light_blocked[rotation][30];
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 32:
                    ebx = _light_blocked[rotation][8];
                    edi = _light_blocked[rotation][9];
                    esi = _light_blocked[rotation][16];
                    ebx |= edi;
                    edi = _light_blocked[rotation][23];
                    ebx |= esi;
                    esi = _light_blocked[rotation][28];
                    ebx |= edi;
                    ebx |= esi;
                    esi = _light_blocked[rotation][15];
                    esi &= ebx;
                    edx = _light_blocked[rotation][8];
                    edx &= _light_blocked[nextRotation][1];
                    ebx = _light_blocked[rotation][16];
                    esi |= edx;
                    edx = _light_blocked[rotation][8];
                    edx |= ebx;
                    ebx = _light_blocked[rotation][28];
                    edi = _light_blocked[rotation][21];
                    ebx |= edx;
                    ebx &= edi;
                    edi = _light_blocked[rotation][23];
                    ebx |= esi;
                    esi = _light_blocked[rotation][22];
                    edx |= edi;
                    ebx |= esi;
                    esi = _light_blocked[rotation][28];
                    edi = _light_blocked[rotation][27];
                    edx |= esi;
                    edx &= edi;
                    esi = _light_blocked[rotation][31];
                    ebx |= edx;
                    edx = _light_blocked[rotation][0];
                    edi = _light_blocked[rotation][28];
                    edx |= esi;
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 33:
                    esi = _light_blocked[rotation][8];
                    edi = _light_blocked[nextRotation][1];
                    ebx = _light_blocked[rotation][15];
                    esi &= edi;
                    ebx &= _light_blocked[nextRotation][2];
                    edi = _light_blocked[nextRotation][3];
                    esi |= ebx;
                    ebx = _light_blocked[rotation][21];
                    ebx &= edi;
                    edi = _light_blocked[nextRotation][4];
                    esi |= ebx;
                    ebx = _light_blocked[rotation][26];
                    ebx &= edi;
                    eax = _light_blocked[rotation][30];
                    ebx |= esi;
                    eax &= _light_blocked[nextRotation][5];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 34:
                    edx = _light_blocked[nextRotation][2];
                    edi = _light_blocked[rotation][26];
                    ebx = _light_blocked[rotation][30];
                    edx |= edi;
                    esi = _light_blocked[rotation][15];
                    edx |= ebx;
                    ebx = _light_blocked[rotation][8];
                    edi = _light_blocked[rotation][21];
                    ebx &= edx;
                    edx &= esi;
                    esi = _light_blocked[rotation][22];
                    ebx |= edx;
                    edx = _light_blocked[rotation][16];
                    ebx |= edi;
                    edi = _light_blocked[rotation][27];
                    edx |= esi;
                    esi = _light_blocked[rotation][31];
                    edx |= edi;
                    eax = _light_blocked[rotation][26];
                    edx |= esi;
                    eax &= edx;
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 35:
                    ebx = _light_blocked[rotation][8];
                    esi = _light_blocked[nextRotation][1];
                    edi = _light_blocked[nextRotation][2];
                    esi &= ebx;
                    ebx = _light_blocked[rotation][15];
                    ebx &= edi;
                    edi = _light_blocked[nextRotation][3];
                    esi |= ebx;
                    ebx = _light_blocked[rotation][21];
                    ebx &= edi;
                    edi = _light_blocked[nextRotation][4];
                    esi |= ebx;
                    ebx = _light_blocked[rotation][26];
                    ebx &= edi;
                    edi = _light_blocked[nextRotation][5];
                    esi |= ebx;
                    ebx = _light_blocked[rotation][30];
                    ebx &= edi;
                    eax = _light_blocked[rotation][33];
                    ebx |= esi;
                    eax &= _light_blocked[nextRotation][6];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                default:
                    assert(false && "Should be unreachable");
                }

                if (v14 == 0) {
                    // TODO: Check.
                    int tile = obj->tile + v70[rotation][index];
                    if (hexGridTileIsValid(tile)) {
                        bool v12 = true;

                        ObjectListNode* objectListNode = gObjectListHeadByTile[tile];
                        while (objectListNode != NULL) {
                            if ((objectListNode->obj->flags & OBJECT_HIDDEN) == 0) {
                                if (objectListNode->obj->elevation > obj->elevation) {
                                    break;
                                }

                                if (objectListNode->obj->elevation == obj->elevation) {
                                    Rect v29;
                                    objectGetRect(objectListNode->obj, &v29);
                                    rectUnion(&objectRect, &v29, &objectRect);

                                    v14 = (objectListNode->obj->flags & OBJECT_FLAG_0x20000000) == 0;

                                    if ((objectListNode->obj->fid & 0xF000000) >> 24 == OBJ_TYPE_WALL) {
                                        if ((objectListNode->obj->flags & OBJECT_FLAG_0x08) == 0) {
                                            Proto* proto;
                                            protoGetProto(objectListNode->obj->pid, &proto);
                                            if ((proto->wall.extendedFlags & 0x8000000) != 0 || (proto->wall.extendedFlags & 0x40000000) != 0) {
                                                if (rotation != ROTATION_W
                                                    && rotation != ROTATION_NW
                                                    && (rotation != ROTATION_NE || index >= 8)
                                                    && (rotation != ROTATION_SW || index <= 15)) {
                                                    v12 = false;
                                                }
                                            } else if ((proto->wall.extendedFlags & 0x10000000) != 0) {
                                                if (rotation != ROTATION_NE && rotation != ROTATION_NW) {
                                                    v12 = false;
                                                }
                                            } else if ((proto->wall.extendedFlags & 0x20000000) != 0) {
                                                if (rotation != ROTATION_NE
                                                    && rotation != ROTATION_E
                                                    && rotation != ROTATION_W
                                                    && rotation != ROTATION_NW
                                                    && (rotation != ROTATION_SW || index <= 15)) {
                                                    v12 = false;
                                                }
                                            } else {
                                                if (rotation != ROTATION_NE
                                                    && rotation != ROTATION_E
                                                    && (rotation != ROTATION_NW || index <= 7)) {
                                                    v12 = false;
                                                }
                                            }
                                        }
                                    } else {
                                        if (v14 && rotation >= ROTATION_E && rotation <= ROTATION_SW) {
                                            v12 = false;
                                        }
                                    }

                                    if (v14) {
                                        break;
                                    }
                                }
                            }
                            objectListNode = objectListNode->next;
                        }

                        if (v12) {
                            adjustLightIntensity(obj->elevation, tile, v28[index]);
                        }
                    }
                }

                _light_blocked[rotation][index] = v14;
            }
        }
    }

    if (rect != NULL) {
        Rect* lightDistanceRect = &(_light_rect[obj->lightDistance]);
        memcpy(rect, lightDistanceRect, sizeof(*lightDistanceRect));

        int x;
        int y;
        tileToScreenXY(obj->tile, &x, &y, obj->elevation);
        x += 16;
        y += 8;

        x -= rect->right / 2;
        y -= rect->bottom / 2;

        rectOffset(rect, x, y);
        rectUnion(rect, &objectRect, rect);
    }

    return 0;
}

// 0x48EABC
void objectDrawOutline(Object* object, Rect* rect)
{
    CacheEntry* cacheEntry;
    Art* art = artLock(object->fid, &cacheEntry);
    if (art == NULL) {
        return;
    }

    int frameWidth = 0;
    int frameHeight = 0;
    artGetSize(art, object->frame, object->rotation, &frameWidth, &frameHeight);

    Rect v49;
    v49.left = 0;
    v49.top = 0;
    v49.right = frameWidth - 1;

    // FIXME: I'm not sure why it ignores frameHeight and makes separate call
    // to obtain height.
    int v8 = artGetHeight(art, object->frame, object->rotation);
    v49.bottom = v8 - 1;

    Rect objectRect;
    if (object->tile == -1) {
        objectRect.left = object->sx;
        objectRect.top = object->sy;
        objectRect.right = object->sx + frameWidth - 1;
        objectRect.bottom = object->sy + frameHeight - 1;
    } else {
        int x;
        int y;
        tileToScreenXY(object->tile, &x, &y, object->elevation);
        x += 16;
        y += 8;

        x += art->xOffsets[object->rotation];
        y += art->yOffsets[object->rotation];

        x += object->x;
        y += object->y;

        objectRect.left = x - frameWidth / 2;
        objectRect.top = y - (frameHeight - 1);
        objectRect.right = objectRect.left + frameWidth - 1;
        objectRect.bottom = y;

        object->sx = objectRect.left;
        object->sy = objectRect.top;
    }

    Rect v32;
    rectCopy(&v32, rect);

    v32.left--;
    v32.top--;
    v32.right++;
    v32.bottom++;

    rectIntersection(&v32, &gObjectsWindowRect, &v32);

    if (rectIntersection(&objectRect, &v32, &objectRect) == 0) {
        v49.left += objectRect.left - object->sx;
        v49.top += objectRect.top - object->sy;
        v49.right = v49.left + (objectRect.right - objectRect.left);
        v49.bottom = v49.top + (objectRect.bottom - objectRect.top);

        unsigned char* src = artGetFrameData(art, object->frame, object->rotation);

        unsigned char* dest = gObjectsWindowBuffer + gObjectsWindowPitch * object->sy + object->sx;
        int destStep = gObjectsWindowPitch - frameWidth;

        unsigned char color;
        unsigned char* v47 = NULL;
        unsigned char* v48 = NULL;
        int v53 = object->outline & OUTLINE_PALETTED;
        int outlineType = object->outline & OUTLINE_TYPE_MASK;
        int v43;
        int v44;

        switch (outlineType) {
        case OUTLINE_TYPE_HOSTILE:
            color = 243;
            v53 = 0;
            v43 = 5;
            v44 = frameHeight / 5;
            break;
        case OUTLINE_TYPE_2:
            color = _colorTable[31744];
            v44 = 0;
            if (v53 != 0) {
                v47 = _commonGrayTable;
                v48 = _redBlendTable;
            }
            break;
        case OUTLINE_TYPE_4:
            color = _colorTable[15855];
            v44 = 0;
            if (v53 != 0) {
                v47 = _commonGrayTable;
                v48 = _wallBlendTable;
            }
            break;
        case OUTLINE_TYPE_FRIENDLY:
            v43 = 4;
            v44 = frameHeight / 4;
            color = 229;
            v53 = 0;
            break;
        case OUTLINE_TYPE_ITEM:
            v44 = 0;
            color = _colorTable[30632];
            if (v53 != 0) {
                v47 = _commonGrayTable;
                v48 = _redBlendTable;
            }
            break;
        case OUTLINE_TYPE_32:
            color = 61;
            v53 = 0;
            v43 = 1;
            v44 = frameHeight;
            break;
        default:
            color = _colorTable[31775];
            v53 = 0;
            v44 = 0;
            break;
        }

        unsigned char v54 = color;
        unsigned char* dest14 = dest;
        unsigned char* src15 = src;
        for (int y = 0; y < frameHeight; y++) {
            bool cycle = true;
            if (v44 != 0) {
                if (y % v44 == 0) {
                    v54++;
                }

                if (v54 > v43 + color - 1) {
                    v54 = color;
                }
            }

            int v22 = dest14 - gObjectsWindowBuffer;
            for (int x = 0; x < frameWidth; x++) {
                v22 = dest14 - gObjectsWindowBuffer;
                if (*src15 != 0 && cycle) {
                    if (x >= v49.left && x <= v49.right && y >= v49.top && y <= v49.bottom && v22 > 0 && v22 % gObjectsWindowPitch != 0) {
                        unsigned char v20;
                        if (v53 != 0) {
                            v20 = v48[(v47[v54] << 8) + *(dest14 - 1)];
                        } else {
                            v20 = v54;
                        }
                        *(dest14 - 1) = v20;
                    }
                    cycle = false;
                } else if (*src15 == 0 && !cycle) {
                    if (x >= v49.left && x <= v49.right && y >= v49.top && y <= v49.bottom) {
                        int v21;
                        if (v53 != 0) {
                            v21 = v48[(v47[v54] << 8) + *dest14];
                        } else {
                            v21 = v54;
                        }
                        *dest14 = v21 & 0xFF;
                    }
                    cycle = true;
                }
                dest14++;
                src15++;
            }

            if (*(src15 - 1) != 0) {
                if (v22 < gObjectsWindowBufferSize) {
                    int v23 = frameWidth - 1;
                    if (v23 >= v49.left && v23 <= v49.right && y >= v49.top && y <= v49.bottom) {
                        if (v53 != 0) {
                            *dest14 = v48[(v47[v54] << 8) + *dest14];
                        } else {
                            *dest14 = v54;
                        }
                    }
                }
            }

            dest14 += destStep;
        }

        for (int x = 0; x < frameWidth; x++) {
            bool cycle = true;
            unsigned char v28 = color;
            unsigned char* dest27 = dest + x;
            unsigned char* src27 = src + x;
            for (int y = 0; y < frameHeight; y++) {
                if (v44 != 0) {
                    if (y % v44 == 0) {
                        v28++;
                    }

                    if (v28 > color + v43 - 1) {
                        v28 = color;
                    }
                }

                if (*src27 != 0 && cycle) {
                    if (x >= v49.left && x <= v49.right && y >= v49.top && y <= v49.bottom) {
                        unsigned char* v29 = dest27 - gObjectsWindowPitch;
                        if (v29 >= gObjectsWindowBuffer) {
                            if (v53) {
                                *v29 = v48[(v47[v28] << 8) + *v29];
                            } else {
                                *v29 = v28;
                            }
                        }
                    }
                    cycle = false;
                } else if (*src27 == 0 && !cycle) {
                    if (x >= v49.left && x <= v49.right && y >= v49.top && y <= v49.bottom) {
                        if (v53) {
                            *dest27 = v48[(v47[v28] << 8) + *dest27];
                        } else {
                            *dest27 = v28;
                        }
                    }
                    cycle = true;
                }

                dest27 += gObjectsWindowPitch;
                src27 += frameWidth;
            }

            if (src27[-frameWidth] != 0) {
                if (dest27 - gObjectsWindowBuffer < gObjectsWindowBufferSize) {
                    int y = frameHeight - 1;
                    if (x >= v49.left && x <= v49.right && y >= v49.top && y <= v49.bottom) {
                        if (v53) {
                            *dest27 = v48[(v47[v28] << 8) + *dest27];
                        } else {
                            *dest27 = v28;
                        }
                    }
                }
            }
        }
    }

    artUnlock(cacheEntry);
}

// 0x48F1B0
void _obj_render_object(Object* object, Rect* rect, int light)
{
    int type = (object->fid & 0xF000000) >> 24;
    if (artIsObjectTypeHidden(type)) {
        return;
    }

    CacheEntry* cacheEntry;
    Art* art = artLock(object->fid, &cacheEntry);
    if (art == NULL) {
        return;
    }

    int frameWidth = artGetWidth(art, object->frame, object->rotation);
    int frameHeight = artGetHeight(art, object->frame, object->rotation);

    Rect objectRect;
    if (object->tile == -1) {
        objectRect.left = object->sx;
        objectRect.top = object->sy;
        objectRect.right = object->sx + frameWidth - 1;
        objectRect.bottom = object->sy + frameHeight - 1;
    } else {
        int objectScreenX;
        int objectScreenY;
        tileToScreenXY(object->tile, &objectScreenX, &objectScreenY, object->elevation);
        objectScreenX += 16;
        objectScreenY += 8;

        objectScreenX += art->xOffsets[object->rotation];
        objectScreenY += art->yOffsets[object->rotation];

        objectScreenX += object->x;
        objectScreenY += object->y;

        objectRect.left = objectScreenX - frameWidth / 2;
        objectRect.top = objectScreenY - (frameHeight - 1);
        objectRect.right = objectRect.left + frameWidth - 1;
        objectRect.bottom = objectScreenY;

        object->sx = objectRect.left;
        object->sy = objectRect.top;
    }

    if (rectIntersection(&objectRect, rect, &objectRect) != 0) {
        artUnlock(cacheEntry);
        return;
    }

    unsigned char* src = artGetFrameData(art, object->frame, object->rotation);
    unsigned char* src2 = src;
    int v50 = objectRect.left - object->sx;
    int v49 = objectRect.top - object->sy;
    src += frameWidth * v49 + v50;
    int objectWidth = objectRect.right - objectRect.left + 1;
    int objectHeight = objectRect.bottom - objectRect.top + 1;

    if (type == 6) {
        blitBufferToBufferTrans(src,
            objectWidth,
            objectHeight,
            frameWidth,
            gObjectsWindowBuffer + gObjectsWindowPitch * objectRect.top + objectRect.left,
            gObjectsWindowPitch);
        artUnlock(cacheEntry);
        return;
    }

    if (type == 2 || type == 3) {
        if ((gDude->flags & OBJECT_HIDDEN) == 0 && (object->flags & OBJECT_FLAG_0xFC000) == 0) {
            Proto* proto;
            protoGetProto(object->pid, &proto);

            bool v17;
            int extendedFlags = proto->critter.extendedFlags;
            if ((extendedFlags & 0x8000000) != 0 || (extendedFlags & 0x80000000) != 0) {
                // TODO: Probably wrong.
                v17 = tileIsInFrontOf(object->tile, gDude->tile);
                if (!v17
                    || !tileIsToRightOf(object->tile, gDude->tile)
                    || (object->flags & OBJECT_FLAG_0x10000000) == 0) {
                    // nothing
                } else {
                    v17 = false;
                }
            } else if ((extendedFlags & 0x10000000) != 0) {
                // NOTE: Uses bitwise OR, so both functions are evaluated.
                v17 = tileIsInFrontOf(object->tile, gDude->tile)
                    || tileIsToRightOf(gDude->tile, object->tile);
            } else if ((extendedFlags & 0x20000000) != 0) {
                v17 = tileIsInFrontOf(object->tile, gDude->tile)
                    && tileIsToRightOf(gDude->tile, object->tile);
            } else {
                v17 = tileIsToRightOf(gDude->tile, object->tile);
                if (v17
                    && tileIsInFrontOf(gDude->tile, object->tile)
                    && (object->flags & OBJECT_FLAG_0x10000000) != 0) {
                    v17 = 0;
                }
            }

            if (v17) {
                CacheEntry* eggHandle;
                Art* egg = artLock(gEgg->fid, &eggHandle);
                if (egg == NULL) {
                    return;
                }

                int eggWidth;
                int eggHeight;
                artGetSize(egg, 0, 0, &eggWidth, &eggHeight);

                int eggScreenX;
                int eggScreenY;
                tileToScreenXY(gEgg->tile, &eggScreenX, &eggScreenY, gEgg->elevation);
                eggScreenX += 16;
                eggScreenY += 8;

                eggScreenX += egg->xOffsets[0];
                eggScreenY += egg->yOffsets[0];

                eggScreenX += gEgg->x;
                eggScreenY += gEgg->y;

                Rect eggRect;
                eggRect.left = eggScreenX - eggWidth / 2;
                eggRect.top = eggScreenY - (eggHeight - 1);
                eggRect.right = eggRect.left + eggWidth - 1;
                eggRect.bottom = eggScreenY;

                gEgg->sx = eggRect.left;
                gEgg->sy = eggRect.top;

                Rect updatedEggRect;
                if (rectIntersection(&eggRect, &objectRect, &updatedEggRect) == 0) {
                    Rect rects[4];

                    rects[0].left = objectRect.left;
                    rects[0].top = objectRect.top;
                    rects[0].right = objectRect.right;
                    rects[0].bottom = updatedEggRect.top - 1;

                    rects[1].left = objectRect.left;
                    rects[1].top = updatedEggRect.top;
                    rects[1].right = updatedEggRect.left - 1;
                    rects[1].bottom = updatedEggRect.bottom;

                    rects[2].left = updatedEggRect.right + 1;
                    rects[2].top = updatedEggRect.top;
                    rects[2].right = objectRect.right;
                    rects[2].bottom = updatedEggRect.bottom;

                    rects[3].left = objectRect.left;
                    rects[3].top = updatedEggRect.bottom + 1;
                    rects[3].right = objectRect.right;
                    rects[3].bottom = objectRect.bottom;

                    for (int i = 0; i < 4; i++) {
                        Rect* v21 = &(rects[i]);
                        if (v21->left <= v21->right && v21->top <= v21->bottom) {
                            unsigned char* sp = src + frameWidth * (v21->top - objectRect.top) + (v21->left - objectRect.left);
                            _dark_trans_buf_to_buf(sp, v21->right - v21->left + 1, v21->bottom - v21->top + 1, frameWidth, gObjectsWindowBuffer, v21->left, v21->top, gObjectsWindowPitch, light);
                        }
                    }

                    unsigned char* mask = artGetFrameData(egg, 0, 0);
                    _intensity_mask_buf_to_buf(
                        src + frameWidth * (updatedEggRect.top - objectRect.top) + (updatedEggRect.left - objectRect.left),
                        updatedEggRect.right - updatedEggRect.left + 1,
                        updatedEggRect.bottom - updatedEggRect.top + 1,
                        frameWidth,
                        gObjectsWindowBuffer + gObjectsWindowPitch * updatedEggRect.top + updatedEggRect.left,
                        gObjectsWindowPitch,
                        mask + eggWidth * (updatedEggRect.top - eggRect.top) + (updatedEggRect.left - eggRect.left),
                        eggWidth,
                        light);
                    artUnlock(eggHandle);
                    artUnlock(cacheEntry);
                    return;
                }

                artUnlock(eggHandle);
            }
        }
    }

    switch (object->flags & OBJECT_FLAG_0xFC000) {
    case OBJECT_FLAG_0x4000:
        _dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, gObjectsWindowBuffer, objectRect.left, objectRect.top, gObjectsWindowPitch, light, _redBlendTable, _commonGrayTable);
        break;
    case OBJECT_FLAG_0x10000:
        _dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, gObjectsWindowBuffer, objectRect.left, objectRect.top, gObjectsWindowPitch, 0x10000, _wallBlendTable, _commonGrayTable);
        break;
    case OBJECT_FLAG_0x20000:
        _dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, gObjectsWindowBuffer, objectRect.left, objectRect.top, gObjectsWindowPitch, light, _glassBlendTable, _glassGrayTable);
        break;
    case OBJECT_FLAG_0x40000:
        _dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, gObjectsWindowBuffer, objectRect.left, objectRect.top, gObjectsWindowPitch, light, _steamBlendTable, _commonGrayTable);
        break;
    case OBJECT_FLAG_0x80000:
        _dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, gObjectsWindowBuffer, objectRect.left, objectRect.top, gObjectsWindowPitch, light, _energyBlendTable, _commonGrayTable);
        break;
    default:
        _dark_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, gObjectsWindowBuffer, objectRect.left, objectRect.top, gObjectsWindowPitch, light);
        break;
    }

    artUnlock(cacheEntry);
}

// Updates fid according to current violence level.
//
// 0x48FA14
void _obj_fix_violence_settings(int* fid)
{
    if ((*fid >> 24) != OBJ_TYPE_CRITTER) {
        return;
    }

    bool shouldResetViolenceLevel = false;
    if (gViolenceLevel == -1) {
        if (!configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &gViolenceLevel)) {
            gViolenceLevel = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
        }
        shouldResetViolenceLevel = true;
    }

    int start;
    int end;

    switch (gViolenceLevel) {
    case VIOLENCE_LEVEL_NONE:
        start = ANIM_BIG_HOLE_SF;
        end = ANIM_FALL_FRONT_BLOOD_SF;
        break;
    case VIOLENCE_LEVEL_MINIMAL:
        start = ANIM_BIG_HOLE_SF;
        end = ANIM_FIRE_DANCE_SF;
        break;
    case VIOLENCE_LEVEL_NORMAL:
        start = ANIM_BIG_HOLE_SF;
        end = ANIM_SLICED_IN_HALF_SF;
        break;
    default:
        // Do not replace anything.
        start = ANIM_COUNT + 1;
        end = ANIM_COUNT + 1;
        break;
    }

    int anim = (*fid & 0xFF0000) >> 16;
    if (anim >= start && anim <= end) {
        anim = (anim == ANIM_FALL_BACK_BLOOD_SF)
            ? ANIM_FALL_BACK_SF
            : ANIM_FALL_FRONT_SF;
        *fid = buildFid(1, *fid & 0xFFF, anim, (*fid & 0xF000) >> 12, (*fid & 0x70000000) >> 28);
    }

    if (shouldResetViolenceLevel) {
        gViolenceLevel = -1;
    }
}

// 0x48FB08
int _obj_preload_sort(const void* a1, const void* a2)
{
    int v1 = *(int*)a1;
    int v2 = *(int*)a2;

    int v3 = _cd_order[(v1 & 0xF000000) >> 24];
    int v4 = _cd_order[(v2 & 0xF000000) >> 24];

    int cmp = v3 - v4;
    if (cmp != 0) {
        return cmp;
    }

    cmp = (v1 & 0xFFF) - (v2 & 0xFFF);
    if (cmp != 0) {
        return cmp;
    }

    cmp = ((v1 & 0xF000) >> 12) - (((v2 & 0xF000) >> 12));
    if (cmp != 0) {
        return cmp;
    }

    cmp = ((v1 & 0xFF0000) >> 16) - (((v2 & 0xFF0000) >> 16));
    return cmp;
}
