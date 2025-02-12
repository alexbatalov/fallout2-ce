#include "proto_instance.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "critter.h"
#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "game_dialog.h"
#include "game_sound.h"
#include "geometry.h"
#include "interface.h"
#include "item.h"
#include "map.h"
#include "message.h"
#include "object.h"
#include "palette.h"
#include "perk.h"
#include "proto.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "skill.h"
#include "stat.h"
#include "tile.h"
#include "worldmap.h"

namespace fallout {

static int _obj_remove_from_inven(Object* critter, Object* item);
static int _obj_use_book(Object* item);
static int _obj_use_flare(Object* critter, Object* item);
static int _obj_use_radio(Object* item);
static int _obj_use_explosive(Object* explosive);
static int _obj_use_power_on_car(Object* ammo);
static int _obj_use_misc_item(Object* item);
static int _protinstTestDroppedExplosive(Object* explosiveItem);
static int _protinst_default_use_item(Object* user, Object* targetObj, Object* item);
static int useLadderDown(Object* user, Object* ladder);
static int useLadderUp(Object* user, Object* ladder);
static int useStairs(Object* user, Object* stairs);
static int _set_door_state_open(Object* door, Object* obj2);
static int _set_door_state_closed(Object* door, Object* obj2);
static int _check_door_state(Object* door, Object* obj2);
static bool _obj_is_portal(Object* obj);
static bool _obj_is_lockable(Object* obj);
static bool _obj_is_openable(Object* obj);
static int objectOpenClose(Object* obj);
static bool objectIsJammed(Object* obj);

// Accessed but not really used
//
//  0x49A990
static MessageListItem stru_49A990;

// 0x49A9A0
int _obj_sid(Object* object, int* sidPtr)
{
    *sidPtr = object->sid;
    if (*sidPtr == -1) {
        return -1;
    }

    return 0;
}

// 0x49A9B4
int _obj_new_sid(Object* object, int* sidPtr)
{
    *sidPtr = -1;

    Proto* proto;
    if (protoGetProto(object->pid, &proto) == -1) {
        return -1;
    }

    int sid;
    int objectType = PID_TYPE(object->pid);
    if (objectType < OBJ_TYPE_TILE) {
        sid = proto->sid;
    } else if (objectType == OBJ_TYPE_TILE) {
        sid = proto->tile.sid;
    } else if (objectType == OBJ_TYPE_MISC) {
        sid = -1;
    } else {
        assert(false && "Should be unreachable");
    }

    if (sid == -1) {
        return -1;
    }

    int scriptType = SID_TYPE(sid);
    if (scriptAdd(sidPtr, scriptType) == -1) {
        return -1;
    }

    Script* script;
    if (scriptGetScript(*sidPtr, &script) == -1) {
        return -1;
    }

    script->index = sid & 0xFFFFFF;

    if (objectType == OBJ_TYPE_CRITTER) {
        object->scriptIndex = script->index;
    }

    if (scriptType == SCRIPT_TYPE_SPATIAL) {
        script->sp.built_tile = builtTileCreate(object->tile, object->elevation);
        script->sp.radius = 3;
    }

    if (object->id == -1) {
        object->id = scriptsNewObjectId();
    }

    script->ownerId = object->id;
    script->owner = object;

    _scr_find_str_run_info(sid & 0xFFFFFF, &(script->field_50), *sidPtr);

    return 0;
}

// 0x49AAC0
int _obj_new_sid_inst(Object* obj, int scriptType, int scriptIndex)
{
    if (scriptIndex == -1) {
        return -1;
    }

    int sid;
    if (scriptAdd(&sid, scriptType) == -1) {
        return -1;
    }

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        return -1;
    }

    script->index = scriptIndex;
    if (scriptType == SCRIPT_TYPE_SPATIAL) {
        script->sp.built_tile = builtTileCreate(obj->tile, obj->elevation);
        script->sp.radius = 3;
    }

    obj->sid = sid;

    obj->id = scriptsNewObjectId();
    script->ownerId = obj->id;

    script->owner = obj;

    _scr_find_str_run_info(scriptIndex & 0xFFFFFF, &(script->field_50), sid);

    if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
        obj->scriptIndex = script->index;
    }

    return 0;
}

// 0x49AC3C
int _obj_look_at(Object* a1, Object* a2)
{
    return _obj_look_at_func(a1, a2, displayMonitorAddMessage);
}

// 0x49AC4C
int _obj_look_at_func(Object* a1, Object* a2, void (*a3)(char* string))
{
    if (critterIsDead(a1)) {
        return -1;
    }

    if (FID_TYPE(a2->fid) == OBJ_TYPE_TILE) {
        return -1;
    }

    Proto* proto;
    if (protoGetProto(a2->pid, &proto) == -1) {
        return -1;
    }

    bool scriptOverrides = false;

    if (a2->sid != -1) {
        scriptSetObjects(a2->sid, a1, a2);
        scriptExecProc(a2->sid, SCRIPT_PROC_LOOK_AT);

        Script* script;
        if (scriptGetScript(a2->sid, &script) == -1) {
            return -1;
        }

        scriptOverrides = script->scriptOverrides;
    }

    if (!scriptOverrides) {
        MessageListItem messageListItem;

        if (PID_TYPE(a2->pid) == OBJ_TYPE_CRITTER && critterIsDead(a2)) {
            messageListItem.num = 491 + randomBetween(0, 1);
        } else {
            messageListItem.num = 490;
        }

        if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
            const char* objectName = objectGetName(a2);

            char formattedText[260];
            snprintf(formattedText, sizeof(formattedText), messageListItem.text, objectName);

            a3(formattedText);
        }
    }

    return -1;
}

// 0x49AD78
int _obj_examine(Object* a1, Object* a2)
{
    return _obj_examine_func(a1, a2, displayMonitorAddMessage);
}

// Performs examine (reading description) action and passes resulting text
// to given callback.
//
// [critter] is a critter who's performing an action. Can be NULL.
// [fn] can be called up to three times when [a2] is an ammo.
//
// 0x49AD88
int _obj_examine_func(Object* critter, Object* target, void (*fn)(char* string))
{
    if (critterIsDead(critter)) {
        return -1;
    }

    if (FID_TYPE(target->fid) == OBJ_TYPE_TILE) {
        return -1;
    }

    bool scriptOverrides = false;
    if (target->sid != -1) {
        scriptSetObjects(target->sid, critter, target);
        scriptExecProc(target->sid, SCRIPT_PROC_DESCRIPTION);

        Script* script;
        if (scriptGetScript(target->sid, &script) == -1) {
            return -1;
        }

        scriptOverrides = script->scriptOverrides;
    }

    if (!scriptOverrides) {
        char* description = objectGetDescription(target);
        if (description != nullptr && strcmp(description, _proto_none_str) == 0) {
            description = nullptr;
        }

        if (description == nullptr || *description == '\0') {
            MessageListItem messageListItem;
            messageListItem.num = 493;
            if (!messageListGetItem(&gProtoMessageList, &messageListItem)) {
                debugPrint("\nError: Can't find msg num!");
            }
            fn(messageListItem.text);
        } else {
            if (PID_TYPE(target->pid) != OBJ_TYPE_CRITTER || !critterIsDead(target)) {
                fn(description);
            }
        }
    }

    if (critter == nullptr || critter != gDude) {
        return 0;
    }

    char formattedText[260];

    int type = PID_TYPE(target->pid);
    if (type == OBJ_TYPE_CRITTER) {
        if (target != gDude && perkGetRank(gDude, PERK_AWARENESS) && !critterIsDead(target)) {
            MessageListItem hpMessageListItem;

            if (critterGetBodyType(target) != BODY_TYPE_BIPED) {
                // It has %d/%d hps
                hpMessageListItem.num = 537;
            } else {
                // 535: He has %d/%d hps
                // 536: She has %d/%d hps
                hpMessageListItem.num = 535 + critterGetStat(target, STAT_GENDER);
            }

            Object* item2 = critterGetItem2(target);
            if (item2 != nullptr && itemGetType(item2) != ITEM_TYPE_WEAPON) {
                item2 = nullptr;
            }

            if (!messageListGetItem(&gProtoMessageList, &hpMessageListItem)) {
                debugPrint("\nError: Can't find msg num!");
                exit(1);
            }

            if (item2 != nullptr) {
                MessageListItem weaponMessageListItem;

                if (ammoGetCaliber(item2) != 0) {
                    weaponMessageListItem.num = 547; // and is wielding a %s with %d/%d shots of %s.
                } else {
                    weaponMessageListItem.num = 546; // and is wielding a %s.
                }

                if (!messageListGetItem(&gProtoMessageList, &weaponMessageListItem)) {
                    debugPrint("\nError: Can't find msg num!");
                    exit(1);
                }

                char format[80];
                snprintf(format, sizeof(format), "%s%s", hpMessageListItem.text, weaponMessageListItem.text);

                if (ammoGetCaliber(item2) != 0) {
                    const int ammoTypePid = weaponGetAmmoTypePid(item2);
                    const char* ammoName = protoGetName(ammoTypePid);
                    const int ammoCapacity = ammoGetCapacity(item2);
                    const int ammoQuantity = ammoGetQuantity(item2);
                    const char* weaponName = objectGetName(item2);
                    const int maxiumHitPoints = critterGetStat(target, STAT_MAXIMUM_HIT_POINTS);
                    const int currentHitPoints = critterGetStat(target, STAT_CURRENT_HIT_POINTS);
                    snprintf(formattedText, sizeof(formattedText),
                        format,
                        currentHitPoints,
                        maxiumHitPoints,
                        weaponName,
                        ammoQuantity,
                        ammoCapacity,
                        ammoName);
                } else {
                    const char* weaponName = objectGetName(item2);
                    const int maxiumHitPoints = critterGetStat(target, STAT_MAXIMUM_HIT_POINTS);
                    const int currentHitPoints = critterGetStat(target, STAT_CURRENT_HIT_POINTS);
                    snprintf(formattedText, sizeof(formattedText),
                        format,
                        currentHitPoints,
                        maxiumHitPoints,
                        weaponName);
                }
            } else {
                MessageListItem endingMessageListItem;

                if (critterIsCrippled(target)) {
                    endingMessageListItem.num = 544; // ,
                } else {
                    endingMessageListItem.num = 545; // .
                }

                if (!messageListGetItem(&gProtoMessageList, &endingMessageListItem)) {
                    debugPrint("\nError: Can't find msg num!");
                    exit(1);
                }

                const int maxiumHitPoints = critterGetStat(target, STAT_MAXIMUM_HIT_POINTS);
                const int currentHitPoints = critterGetStat(target, STAT_CURRENT_HIT_POINTS);
                snprintf(formattedText, sizeof(formattedText), hpMessageListItem.text, currentHitPoints, maxiumHitPoints);
                strcat(formattedText, endingMessageListItem.text);
            }
        } else {
            int v12 = 0;
            if (critterIsCrippled(target)) {
                v12 -= 2;
            }

            int v16;

            const int maxiumHitPoints = critterGetStat(target, STAT_MAXIMUM_HIT_POINTS);
            const int currentHitPoints = critterGetStat(target, STAT_CURRENT_HIT_POINTS);
            if (currentHitPoints <= 0 || critterIsDead(target)) {
                v16 = 0;
            } else if (currentHitPoints == maxiumHitPoints) {
                v16 = 4;
            } else {
                v16 = (currentHitPoints * 3) / maxiumHitPoints + 1;
            }

            MessageListItem hpMessageListItem;
            hpMessageListItem.num = 500 + v16;
            if (!messageListGetItem(&gProtoMessageList, &hpMessageListItem)) {
                debugPrint("\nError: Can't find msg num!");
                exit(1);
            }

            if (v16 > 4) {
                // Error: lookup_val out of range
                hpMessageListItem.num = 550;
                if (!messageListGetItem(&gProtoMessageList, &hpMessageListItem)) {
                    debugPrint("\nError: Can't find msg num!");
                    exit(1);
                }

                debugPrint(hpMessageListItem.text);
                return 0;
            }

            MessageListItem v66;
            if (target == gDude) {
                // You look %s
                v66.num = 520 + v12;
                if (!messageListGetItem(&gProtoMessageList, &v66)) {
                    debugPrint("\nError: Can't find msg num!");
                    exit(1);
                }

                snprintf(formattedText, sizeof(formattedText), v66.text, hpMessageListItem.text);
            } else {
                // %s %s
                v66.num = 521 + v12;
                if (!messageListGetItem(&gProtoMessageList, &v66)) {
                    debugPrint("\nError: Can't find msg num!");
                    exit(1);
                }

                MessageListItem v63;
                v63.num = 522 + critterGetStat(target, STAT_GENDER);
                if (!messageListGetItem(&gProtoMessageList, &v63)) {
                    debugPrint("\nError: Can't find msg num!");
                    exit(1);
                }

                snprintf(formattedText, sizeof(formattedText), v63.text, hpMessageListItem.text);
            }
        }

        if (critterIsCrippled(target)) {
            const int maxiumHitPoints = critterGetStat(target, STAT_MAXIMUM_HIT_POINTS);
            const int currentHitPoints = critterGetStat(target, STAT_CURRENT_HIT_POINTS);

            MessageListItem v63;
            v63.num = maxiumHitPoints >= currentHitPoints ? 531 : 530;

            if (target == gDude) {
                v63.num += 2;
            }

            if (!messageListGetItem(&gProtoMessageList, &v63)) {
                debugPrint("\nError: Can't find msg num!");
                exit(1);
            }

            strcat(formattedText, v63.text);
        }

        fn(formattedText);
    } else if (type == OBJ_TYPE_SCENERY) {
        if (target->pid == PROTO_ID_CAR) {
            MessageListItem carMessageListItem;
            carMessageListItem.num = 549; // The car is running at %d%% power.

            int car = gameGetGlobalVar(GVAR_PLAYER_GOT_CAR);
            if (car == 0) {
                carMessageListItem.num = 548; // The car doesn't look like it's working right now.
            }

            if (!messageListGetItem(&gProtoMessageList, &carMessageListItem)) {
                debugPrint("\nError: Can't find msg num!");
                exit(1);
            }

            if (car != 0) {
                snprintf(formattedText, sizeof(formattedText), carMessageListItem.text, 100 * wmCarGasAmount() / 80000);
            } else {
                strcpy(formattedText, carMessageListItem.text);
            }

            fn(formattedText);
        }
    } else if (type == OBJ_TYPE_ITEM) {
        int itemType = itemGetType(target);
        if (itemType == ITEM_TYPE_WEAPON) {
            if (ammoGetCaliber(target) != 0) {
                MessageListItem weaponMessageListItem;
                weaponMessageListItem.num = 526;

                if (!messageListGetItem(&gProtoMessageList, &weaponMessageListItem)) {
                    debugPrint("\nError: Can't find msg num!");
                    exit(1);
                }

                int ammoTypePid = weaponGetAmmoTypePid(target);
                const char* ammoName = protoGetName(ammoTypePid);
                int ammoCapacity = ammoGetCapacity(target);
                int ammoQuantity = ammoGetQuantity(target);
                snprintf(formattedText, sizeof(formattedText), weaponMessageListItem.text, ammoQuantity, ammoCapacity, ammoName);
                fn(formattedText);
            }
        } else if (itemType == ITEM_TYPE_AMMO) {
            // SFALL: Fix ammo details when examining in barter screen.
            // CE: Underlying `gameDialogRenderSupplementaryMessage` cannot
            // accumulate strings like `inventoryRenderItemDescription` does.
            char ammoFormattedText[260 * 3];
            ammoFormattedText[0] = '\0';

            MessageListItem ammoMessageListItem;
            ammoMessageListItem.num = 510;

            if (!messageListGetItem(&gProtoMessageList, &ammoMessageListItem)) {
                debugPrint("\nError: Can't find msg num!");
                exit(1);
            }

            snprintf(formattedText, sizeof(formattedText),
                ammoMessageListItem.text,
                ammoGetArmorClassModifier(target));
            if (fn == gameDialogRenderSupplementaryMessage) {
                strcat(ammoFormattedText, formattedText);
            } else {
                fn(formattedText);
            }

            ammoMessageListItem.num++;
            if (!messageListGetItem(&gProtoMessageList, &ammoMessageListItem)) {
                debugPrint("\nError: Can't find msg num!");
                exit(1);
            }

            snprintf(formattedText, sizeof(formattedText),
                ammoMessageListItem.text,
                ammoGetDamageResistanceModifier(target));
            if (fn == gameDialogRenderSupplementaryMessage) {
                strcat(ammoFormattedText, ", ");
                strcat(ammoFormattedText, formattedText);
            } else {
                fn(formattedText);
            }

            ammoMessageListItem.num++;
            if (!messageListGetItem(&gProtoMessageList, &ammoMessageListItem)) {
                debugPrint("\nError: Can't find msg num!");
                exit(1);
            }

            snprintf(formattedText, sizeof(formattedText),
                ammoMessageListItem.text,
                ammoGetDamageMultiplier(target),
                ammoGetDamageDivisor(target));
            if (fn == gameDialogRenderSupplementaryMessage) {
                strcat(ammoFormattedText, ", ");
                strcat(ammoFormattedText, formattedText);
                strcat(ammoFormattedText, ".");
                fn(ammoFormattedText);
            } else {
                fn(formattedText);
            }
        }
    }

    return 0;
}

// 0x49B650
int _obj_pickup(Object* critter, Object* item)
{
    bool overriden = false;

    if (item->sid != -1) {
        scriptSetObjects(item->sid, critter, item);
        scriptExecProc(item->sid, SCRIPT_PROC_PICKUP);

        Script* script;
        if (scriptGetScript(item->sid, &script) == -1) {
            return -1;
        }

        overriden = script->scriptOverrides;
    }

    if (!overriden) {
        int rc;
        if (item->pid == PROTO_ID_MONEY) {
            int amount = itemGetMoney(item);
            if (amount <= 0) {
                amount = 1;
            }

            rc = itemAttemptAdd(critter, item, amount);
            if (rc == 0) {
                itemSetMoney(item, 0);
            }
        } else {
            rc = itemAttemptAdd(critter, item, 1);
        }

        if (rc == 0) {
            Rect rect;
            _obj_disconnect(item, &rect);
            tileWindowRefreshRect(&rect, item->elevation);
        } else {
            MessageListItem messageListItem;
            // You cannot pick up that item. You are at your maximum weight capacity.
            messageListItem.num = 905;
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }
    }

    return 0;
}

// 0x49B73C
static int _obj_remove_from_inven(Object* critter, Object* item)
{
    Rect updatedRect;
    int fid;
    int v11 = 0;
    if (critterGetItem2(critter) == item) {
        if (critter != gDude || interfaceGetCurrentHand()) {
            fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, FID_ANIM_TYPE(critter->fid), 0, critter->rotation);
            objectSetFid(critter, fid, &updatedRect);
            v11 = 2;
        } else {
            v11 = 1;
        }
    } else if (critterGetItem1(critter) == item) {
        if (critter == gDude && !interfaceGetCurrentHand()) {
            fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, FID_ANIM_TYPE(critter->fid), 0, critter->rotation);
            objectSetFid(critter, fid, &updatedRect);
            v11 = 2;
        } else {
            v11 = 1;
        }
    } else if (critterGetArmor(critter) == item) {
        if (critter == gDude) {
            int v5 = 1;

            Proto* proto;
            if (protoGetProto(0x1000000, &proto) != -1) {
                v5 = proto->fid;
            }

            fid = buildFid(OBJ_TYPE_CRITTER, v5, FID_ANIM_TYPE(critter->fid), (critter->fid & 0xF000) >> 12, critter->rotation);
            objectSetFid(critter, fid, &updatedRect);
            v11 = 3;
        }
    }

    int rc = itemRemove(critter, item, 1);

    if (v11 >= 2) {
        tileWindowRefreshRect(&updatedRect, critter->elevation);
    }

    if (v11 <= 2 && critter == gDude) {
        interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }

    return rc;
}

// 0x49B8B0
int _obj_drop(Object* a1, Object* a2)
{
    if (a2 == nullptr) {
        return -1;
    }

    bool scriptOverrides = false;
    if (a1->sid != -1) {
        scriptSetObjects(a1->sid, a2, nullptr);
        scriptExecProc(a1->sid, SCRIPT_PROC_IS_DROPPING);

        Script* scr;
        if (scriptGetScript(a1->sid, &scr) == -1) {
            return -1;
        }

        scriptOverrides = scr->scriptOverrides;
    }

    if (scriptOverrides) {
        return 0;
    }

    if (a2->sid != -1) {
        scriptSetObjects(a2->sid, a1, a2);
        scriptExecProc(a2->sid, SCRIPT_PROC_DROP);

        Script* scr;
        if (scriptGetScript(a2->sid, &scr) == -1) {
            return -1;
        }

        scriptOverrides = scr->scriptOverrides;
    }

    if (scriptOverrides) {
        return 0;
    }

    if (_obj_remove_from_inven(a1, a2) == 0) {
        Object* owner = objectGetOwner(a1);
        if (owner == nullptr) {
            owner = a1;
        }

        Rect updatedRect;
        _obj_connect(a2, owner->tile, owner->elevation, &updatedRect);
        tileWindowRefreshRect(&updatedRect, owner->elevation);
    }

    return 0;
}

// 0x49B9A0
int _obj_destroy(Object* obj)
{
    if (obj == nullptr) {
        return -1;
    }

    int elev;
    Object* owner = obj->owner;
    if (owner != nullptr) {
        _obj_remove_from_inven(owner, obj);
    } else {
        elev = obj->elevation;
    }

    queueRemoveEvents(obj);

    Rect rect;
    objectDestroy(obj, &rect);

    if (owner == nullptr) {
        tileWindowRefreshRect(&rect, elev);
    }

    return 0;
}

// Read a book.
//
// 0x49B9F0
static int _obj_use_book(Object* book)
{
    MessageListItem messageListItem;

    int messageId = -1;
    int skill;

    // SFALL
    if (!booksGetInfo(book->pid, &messageId, &skill)) {
        return -1;
    }

    if (isInCombat()) {
        // You cannot do that in combat.
        messageListItem.num = 902;
        if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }

        return 0;
    }

    int increase = (100 - skillGetValue(gDude, skill)) / 10;
    if (increase <= 0) {
        messageId = 801;
    } else {
        if (perkGetRank(gDude, PERK_COMPREHENSION)) {
            increase = 150 * increase / 100;
        }

        for (int i = 0; i < increase; i++) {
            skillAddForce(gDude, skill);
        }
    }

    paletteFadeTo(gPaletteBlack);

    int intelligence = critterGetStat(gDude, STAT_INTELLIGENCE);
    gameTimeAddSeconds(3600 * (11 - intelligence));

    scriptsExecMapUpdateProc();

    paletteFadeTo(_cmap);

    // You read the book.
    messageListItem.num = 800;
    if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
        displayMonitorAddMessage(messageListItem.text);
    }

    messageListItem.num = messageId;
    if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
        displayMonitorAddMessage(messageListItem.text);
    }

    return 1;
}

// Light a flare.
//
// 0x49BBA8
static int _obj_use_flare(Object* critter, Object* flare)
{
    MessageListItem messageListItem;

    if (flare->pid != PROTO_ID_FLARE) {
        return -1;
    }

    if ((flare->flags & OBJECT_QUEUED) != 0) {
        if (critter == gDude) {
            // The flare is already lit.
            messageListItem.num = 588;
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }
    } else {
        if (critter == gDude) {
            // You light the flare.
            messageListItem.num = 588;
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }

        flare->pid = PROTO_ID_LIT_FLARE;

        objectSetLight(flare, 8, 0x10000, nullptr);
        queueAddEvent(72000, flare, nullptr, EVENT_TYPE_FLARE);
    }

    return 0;
}

// 0x49BC60
static int _obj_use_radio(Object* item)
{
    Script* scr;

    if (item->sid == -1) {
        return -1;
    }

    scriptSetObjects(item->sid, gDude, item);
    scriptExecProc(item->sid, SCRIPT_PROC_USE);

    if (scriptGetScript(item->sid, &scr) == -1) {
        return -1;
    }

    return 0;
}

// 0x49BCB4
static int _obj_use_explosive(Object* explosive)
{
    MessageListItem messageListItem;

    int pid = explosive->pid;
    // SFALL
    if (!explosiveIsExplosive(pid)) {
        return -1;
    }

    if ((explosive->flags & OBJECT_QUEUED) != 0) {
        // The timer is already ticking.
        messageListItem.num = 590;
        if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
    } else {
        int seconds = _inven_set_timer(explosive);
        if (seconds != -1) {
            // You set the timer.
            messageListItem.num = 589;
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }

            // SFALL
            explosiveActivate(&(explosive->pid));

            int delay = 10 * seconds;

            int roll;
            if (perkHasRank(gDude, PERK_DEMOLITION_EXPERT)) {
                roll = ROLL_SUCCESS;
            } else {
                roll = skillRoll(gDude, SKILL_TRAPS, 0, nullptr);
            }

            int eventType;
            switch (roll) {
            case ROLL_CRITICAL_FAILURE:
                delay = 0;
                eventType = EVENT_TYPE_EXPLOSION_FAILURE;
                break;
            case ROLL_FAILURE:
                eventType = EVENT_TYPE_EXPLOSION_FAILURE;
                delay /= 2;
                break;
            default:
                eventType = EVENT_TYPE_EXPLOSION;
                break;
            }

            queueAddEvent(delay, explosive, nullptr, eventType);
        }
    }

    return 2;
}

// Recharge car with given item
// Returns -1 when car cannot be recharged with given item.
// Returns 1 when car is recharged.
//
// 0x49BDE8
static int _obj_use_power_on_car(Object* item)
{
    MessageListItem messageListItem;
    int messageNum;

    memcpy(&messageListItem, &stru_49A990, sizeof(messageListItem));

    bool isEnergy = false;
    int energyDensity;

    switch (item->pid) {
    case PROTO_ID_SMALL_ENERGY_CELL:
        energyDensity = 16000;
        isEnergy = true;
        break;
    case PROTO_ID_MICRO_FUSION_CELL:
        energyDensity = 40000;
        isEnergy = true;
        break;
    }

    if (!isEnergy) {
        return -1;
    }

    // SFALL: Fix for cells getting consumed even when the car is already fully
    // charged.
    int rc;
    if (wmCarGasAmount() < CAR_FUEL_MAX) {
        int energy = ammoGetQuantity(item) * energyDensity;
        int capacity = ammoGetCapacity(item);

        // NOTE: that function will never return -1
        if (wmCarFillGas(energy / capacity) == -1) {
            return -1;
        }

        // You charge the car with more power.
        messageNum = 595;
        rc = 1;
    } else {
        // The car is already full of power.
        messageNum = 596;
        rc = 0;
    }

    char* text = getmsg(&gProtoMessageList, &messageListItem, messageNum);
    displayMonitorAddMessage(text);

    return rc;
}

// 0x49BE88
static int _obj_use_misc_item(Object* item)
{

    if (item == nullptr) {
        return -1;
    }

    switch (item->pid) {
    case PROTO_ID_RAMIREZ_BOX_CLOSED:
    case PROTO_ID_RAIDERS_MAP:
    case PROTO_ID_CATS_PAW_ISSUE_5:
    case PROTO_ID_PIP_BOY_LINGUAL_ENHANCER:
    case PROTO_ID_SURVEY_MAP:
    case PROTO_ID_PIP_BOY_MEDICAL_ENHANCER:
        if (item->sid == -1) {
            return 1;
        }

        scriptSetObjects(item->sid, gDude, item);
        scriptExecProc(item->sid, SCRIPT_PROC_USE);

        Script* scr;
        if (scriptGetScript(item->sid, &scr) == -1) {
            return -1;
        }

        return 1;
    }

    return -1;
}

// 0x49BF38
int _protinst_use_item(Object* critter, Object* item)
{
    int rc;
    MessageListItem messageListItem;

    switch (itemGetType(item)) {
    case ITEM_TYPE_DRUG:
        rc = -1;
        break;
    case ITEM_TYPE_WEAPON:
    case ITEM_TYPE_MISC:
        rc = _obj_use_book(item);
        if (rc != -1) {
            break;
        }

        rc = _obj_use_flare(critter, item);
        if (rc == 0) {
            break;
        }

        rc = _obj_use_misc_item(item);
        if (rc != -1) {
            break;
        }

        rc = _obj_use_radio(item);
        if (rc == 0) {
            break;
        }

        rc = _obj_use_explosive(item);
        if (rc == 0 || rc == 2) {
            break;
        }

        // TODO: Not sure about these two conditions.
        if (miscItemIsConsumable(item)) {
            rc = _item_m_use_charged_item(critter, item);
            if (rc == 0) {
                break;
            }
        }
        // FALLTHROUGH
    default:
        // That does nothing
        messageListItem.num = 582;
        if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }

        rc = -1;
    }

    return rc;
}

// 0x49BFE8
static int _protinstTestDroppedExplosive(Object* explosiveItem)
{
    // SFALL
    if (explosiveIsActiveExplosive(explosiveItem->pid)) {
        Attack attack;
        attackInit(&attack, gDude, nullptr, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);
        attack.attackerFlags = DAM_HIT;
        attack.tile = gDude->tile;
        _compute_explosion_on_extras(&attack, 0, 0, 1);

        int team = gDude->data.critter.combat.team;
        Object* watcher = nullptr;
        for (int index = 0; index < attack.extrasLength; index++) {
            Object* v5 = attack.extras[index];
            if (v5 != gDude
                && v5->data.critter.combat.team != team
                && statRoll(v5, STAT_PERCEPTION, 0, nullptr) >= 2) {
                _critter_set_who_hit_me(v5, gDude);
                if (watcher == nullptr) {
                    watcher = v5;
                }
            }
        }

        if (watcher != nullptr && !isInCombat()) {
            CombatStartData combat;
            combat.attacker = watcher;
            combat.defender = gDude;
            combat.actionPointsBonus = 0;
            combat.accuracyBonus = 0;
            combat.minDamage = 0;
            combat.maxDamage = 99999;
            combat.overrideAttackResults = 0;
            scriptsRequestCombat(&combat);
        }
    }
    return 0;
}

// 0x49C124
int _obj_use_item(Object* a1, Object* a2)
{
    int rc = _protinst_use_item(a1, a2);
    if (rc == 1 || rc == 2) {
        Object* root = objectGetOwner(a2);
        if (root != nullptr) {
            int flags = a2->flags & OBJECT_IN_ANY_HAND;
            itemRemove(root, a2, 1);
            Object* v8 = itemReplace(root, a2, flags);
            if (root == gDude) {
                int leftItemAction;
                int rightItemAction;
                interfaceGetItemActions(&leftItemAction, &rightItemAction);
                if (v8 == nullptr) {
                    if ((flags & OBJECT_IN_LEFT_HAND) != 0) {
                        leftItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                    } else if ((flags & OBJECT_IN_RIGHT_HAND) != 0) {
                        rightItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                    } else {
                        leftItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                        rightItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                    }
                }
                interfaceUpdateItems(false, leftItemAction, rightItemAction);
            }
        }

        if (rc == 1) {
            _obj_destroy(a2);
        } else if (rc == 2 && root != nullptr) {
            Rect updatedRect;
            _obj_connect(a2, root->tile, root->elevation, &updatedRect);
            tileWindowRefreshRect(&updatedRect, root->elevation);
            _protinstTestDroppedExplosive(a2);
        }

        rc = 0;
    }

    scriptsExecMapUpdateProc();

    return rc;
}

// 0x49C240
static int _protinst_default_use_item(Object* user, Object* targetObj, Object* item)
{
    char formattedText[90];
    MessageListItem messageListItem;

    int rc;
    switch (itemGetType(item)) {
    case ITEM_TYPE_DRUG:
        if (PID_TYPE(targetObj->pid) != OBJ_TYPE_CRITTER) {
            if (user == gDude) {
                // That does nothing
                messageListItem.num = 582;
                if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                    displayMonitorAddMessage(messageListItem.text);
                }
            }
            return -1;
        }

        if (critterIsDead(targetObj)) {
            // 583: To your dismay, you realize that it is already dead.
            // 584: As you reach down, you realize that it is already dead.
            // 585: Alas, you are too late.
            // 586: That won't work on the dead.
            messageListItem.num = 583 + randomBetween(0, 3);
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
            return -1;
        }

        rc = _item_d_take_drug(targetObj, item);

        if (user == gDude && targetObj != gDude) {
            // TODO: Looks like there is bug in this branch, message 580 will never be shown,
            // as we can only be here when target is not dude.

            // 580: You use the %s.
            // 581: You use the %s on %s.
            messageListItem.num = 580 + (targetObj != gDude);
            if (!messageListGetItem(&gProtoMessageList, &messageListItem)) {
                return -1;
            }

            snprintf(formattedText, sizeof(formattedText), messageListItem.text, objectGetName(item), objectGetName(targetObj));
            displayMonitorAddMessage(formattedText);
        }

        if (targetObj == gDude) {
            interfaceRenderHitPoints(true);
        }

        return rc;
    case ITEM_TYPE_AMMO:
        // SFALL: Fix for being able to charge the car by using cells on other
        // scenery/critters.
        if (targetObj->pid == PROTO_ID_CAR || targetObj->pid == PROTO_ID_CAR_TRUNK) {
            rc = _obj_use_power_on_car(item);
            if (rc == 1) {
                return 1;
            } else if (rc == 0) {
                return -1;
            }
        }
        break;
    case ITEM_TYPE_WEAPON:
    case ITEM_TYPE_MISC:
        rc = _obj_use_flare(user, item);
        if (rc == 0) {
            return 0;
        }
        break;
    }

    messageListItem.num = 582;
    if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
        snprintf(formattedText, sizeof(formattedText), "%s", messageListItem.text);
        displayMonitorAddMessage(formattedText);
    }
    return -1;
}

// 0x49C3CC
int _protinst_use_item_on(Object* critter, Object* targetObj, Object* item)
{
    int messageId = -1;
    int criticalChanceModifier = 0;
    int skill = -1;

    switch (item->pid) {
    case PROTO_ID_DOCTORS_BAG:
        // The supplies in the Doctor's Bag run out.
        messageId = 900;
        criticalChanceModifier = 20;
        skill = SKILL_DOCTOR;
        break;
    case PROTO_ID_FIRST_AID_KIT:
        // The supplies in the First Aid Kit run out.
        messageId = 901;
        criticalChanceModifier = 20;
        skill = SKILL_FIRST_AID;
        break;
    case PROTO_ID_PARAMEDICS_BAG:
        // The supplies in the Paramedic's Bag run out.
        messageId = 910;
        criticalChanceModifier = 40;
        skill = SKILL_DOCTOR;
        break;
    case PROTO_ID_FIELD_MEDIC_FIRST_AID_KIT:
        // The supplies in the Field Medic First Aid Kit run out.
        messageId = 911;
        criticalChanceModifier = 40;
        skill = SKILL_FIRST_AID;
        break;
    }

    if (skill == -1) {
        Script* script;

        if (item->sid == -1) {
            if (targetObj->sid == -1) {
                return _protinst_default_use_item(critter, targetObj, item);
            }

            scriptSetObjects(targetObj->sid, critter, item);
            scriptExecProc(targetObj->sid, SCRIPT_PROC_USE_OBJ_ON);

            if (scriptGetScript(targetObj->sid, &script) == -1) {
                return -1;
            }

            if (!script->scriptOverrides) {
                return _protinst_default_use_item(critter, targetObj, item);
            }
        } else {
            scriptSetObjects(item->sid, critter, targetObj);
            scriptExecProc(item->sid, SCRIPT_PROC_USE_OBJ_ON);

            if (scriptGetScript(item->sid, &script) == -1) {
                return -1;
            }

            if (script->returnValue == 0) {
                if (targetObj->sid == -1) {
                    return _protinst_default_use_item(critter, targetObj, item);
                }

                scriptSetObjects(targetObj->sid, critter, item);
                scriptExecProc(targetObj->sid, SCRIPT_PROC_USE_OBJ_ON);

                Script* script;
                if (scriptGetScript(targetObj->sid, &script) == -1) {
                    return -1;
                }

                if (!script->scriptOverrides) {
                    return _protinst_default_use_item(critter, targetObj, item);
                }
            }
        }

        return script->returnValue;
    }

    if (isInCombat()) {
        MessageListItem messageListItem;
        // You cannot do that in combat.
        messageListItem.num = 902;
        if (critter == gDude) {
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }
        return -1;
    }

    if (skillUse(critter, targetObj, skill, criticalChanceModifier) != 0) {
        return 0;
    }

    if (randomBetween(1, 10) != 1) {
        return 0;
    }

    MessageListItem messageListItem;
    messageListItem.num = messageId;
    if (critter == gDude) {
        if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
    }

    return 1;
}

// 0x49C5FC
int _obj_use_item_on(Object* user, Object* targetObj, Object* item)
{
    int rc = _protinst_use_item_on(user, targetObj, item);

    if (rc == 1) {
        if (user != nullptr) {
            int flags = item->flags & OBJECT_IN_ANY_HAND;
            itemRemove(user, item, 1);

            Object* replacedItem = itemReplace(user, item, flags);

            // CE: Fix rare crash when using uninitialized action variables. The
            // following code is on par with |_obj_use_item| which does not
            // crash.
            if (user == gDude) {
                int leftItemAction;
                int rightItemAction;
                interfaceGetItemActions(&leftItemAction, &rightItemAction);

                if (replacedItem == nullptr) {
                    if ((flags & OBJECT_IN_LEFT_HAND) != 0) {
                        leftItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                    } else if ((flags & OBJECT_IN_RIGHT_HAND) != 0) {
                        rightItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                    } else {
                        leftItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                        rightItemAction = INTERFACE_ITEM_ACTION_DEFAULT;
                    }
                }

                interfaceUpdateItems(false, leftItemAction, rightItemAction);
            }
        }

        _obj_destroy(item);

        rc = 0;
    }

    scriptsExecMapUpdateProc();

    return rc;
}

// 0x49C6BC
int _check_scenery_ap_cost(Object* obj, Object* a2)
{
    if (!isInCombat()) {
        return 0;
    }

    int actionPoints = obj->data.critter.combat.ap;
    if (actionPoints >= 3) {
        obj->data.critter.combat.ap = actionPoints - 3;

        if (obj == gDude) {
            interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
        }

        return 0;
    }

    MessageListItem messageListItem;
    // You don't have enough action points.
    messageListItem.num = 700;

    if (obj == gDude) {
        if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
    }

    return -1;
}

// 0x49C740
int _obj_use(Object* user, Object* targetObj)
{
    int type = FID_TYPE(targetObj->fid);
    if (user == gDude) {
        if (type != OBJ_TYPE_SCENERY) {
            return -1;
        }
    } else {
        if (type != OBJ_TYPE_SCENERY) {
            return 0;
        }
    }

    Proto* sceneryProto;
    if (protoGetProto(targetObj->pid, &sceneryProto) == -1) {
        return -1;
    }

    if (PID_TYPE(targetObj->pid) == OBJ_TYPE_SCENERY && sceneryProto->scenery.type == SCENERY_TYPE_DOOR) {
        return _obj_use_door(user, targetObj);
    }

    bool scriptOverrides = false;

    if (targetObj->sid != -1) {
        scriptSetObjects(targetObj->sid, user, targetObj);
        scriptExecProc(targetObj->sid, SCRIPT_PROC_USE);

        Script* script;
        if (scriptGetScript(targetObj->sid, &script) == -1) {
            return -1;
        }

        scriptOverrides = script->scriptOverrides;
    }

    if (!scriptOverrides) {
        if (PID_TYPE(targetObj->pid) == OBJ_TYPE_SCENERY) {
            if (sceneryProto->scenery.type == SCENERY_TYPE_LADDER_DOWN) {
                if (useLadderDown(user, targetObj) == 0) {
                    scriptOverrides = true;
                }
            } else if (sceneryProto->scenery.type == SCENERY_TYPE_LADDER_UP) {
                if (useLadderUp(user, targetObj) == 0) {
                    scriptOverrides = true;
                }
            } else if (sceneryProto->scenery.type == SCENERY_TYPE_STAIRS) {
                if (useStairs(user, targetObj) == 0) {
                    scriptOverrides = true;
                }
            }
        }
    }

    if (!scriptOverrides) {
        if (user == gDude) {
            // You see: %s
            MessageListItem messageListItem;
            messageListItem.num = 480;
            if (!messageListGetItem(&gProtoMessageList, &messageListItem)) {
                return -1;
            }

            char formattedText[260];
            const char* name = objectGetName(targetObj);
            snprintf(formattedText, sizeof(formattedText), messageListItem.text, name);
            displayMonitorAddMessage(formattedText);
        }
    }

    scriptsExecMapUpdateProc();

    return 0;
}

// 0x49C900
static int useLadderDown(Object* user, Object* ladder)
{
    int builtTile = ladder->data.scenery.ladder.destinationBuiltTile;
    if (builtTile == -1) {
        return -1;
    }

    int tile = builtTileGetTile(builtTile);
    int elevation = builtTileGetElevation(builtTile);
    if (ladder->data.scenery.ladder.destinationMap != 0) {
        MapTransition transition;
        memset(&transition, 0, sizeof(transition));

        transition.map = ladder->data.scenery.ladder.destinationMap;
        transition.elevation = elevation;
        transition.tile = tile;
        transition.rotation = builtTileGetRotation(builtTile);

        mapSetTransition(&transition);

        wmMapMarkMapEntranceState(transition.map, elevation, 1);
    } else {
        Rect updatedRect;
        if (objectSetLocation(user, tile, elevation, &updatedRect) == -1) {
            return -1;
        }

        tileWindowRefreshRect(&updatedRect, gElevation);
    }

    return 0;
}

// 0x49C9A4
static int useLadderUp(Object* user, Object* ladder)
{
    int builtTile = ladder->data.scenery.ladder.destinationBuiltTile;
    if (builtTile == -1) {
        return -1;
    }

    int tile = builtTileGetTile(builtTile);
    int elevation = builtTileGetElevation(builtTile);
    if (ladder->data.scenery.ladder.destinationMap != 0) {
        MapTransition transition;
        memset(&transition, 0, sizeof(transition));

        transition.map = ladder->data.scenery.ladder.destinationMap;
        transition.elevation = elevation;
        transition.tile = tile;
        transition.rotation = builtTileGetRotation(builtTile);

        mapSetTransition(&transition);

        wmMapMarkMapEntranceState(transition.map, elevation, 1);
    } else {
        Rect updatedRect;
        if (objectSetLocation(user, tile, elevation, &updatedRect) == -1) {
            return -1;
        }

        tileWindowRefreshRect(&updatedRect, gElevation);
    }

    return 0;
}

// 0x49CA48
static int useStairs(Object* user, Object* stairs)
{
    int builtTile = stairs->data.scenery.stairs.destinationBuiltTile;
    if (builtTile == -1) {
        return -1;
    }

    int tile = builtTileGetTile(builtTile);
    int elevation = builtTileGetElevation(builtTile);
    if (stairs->data.scenery.stairs.destinationMap > 0) {
        MapTransition transition;
        memset(&transition, 0, sizeof(transition));

        transition.map = stairs->data.scenery.stairs.destinationMap;
        transition.elevation = elevation;
        transition.tile = tile;
        transition.rotation = builtTileGetRotation(builtTile);

        mapSetTransition(&transition);

        wmMapMarkMapEntranceState(transition.map, elevation, 1);
    } else {
        Rect updatedRect;
        if (objectSetLocation(user, tile, elevation, &updatedRect) == -1) {
            return -1;
        }

        tileWindowRefreshRect(&updatedRect, gElevation);
    }

    return 0;
}

// 0x49CAF4
static int _set_door_state_open(Object* door, Object* obj2)
{
    door->data.scenery.door.openFlags |= 0x01;
    return 0;
}

// 0x49CB04
static int _set_door_state_closed(Object* door, Object* obj2)
{
    door->data.scenery.door.openFlags &= ~0x01;
    return 0;
}

// 0x49CB14
static int _check_door_state(Object* door, Object* obj2)
{
    if ((door->data.scenery.door.openFlags & 0x01) == 0) {
        // SFALL: Fix flags on non-door objects.
        if (_obj_is_portal(door)) {
            door->flags &= ~OBJECT_OPEN_DOOR;
        }

        _obj_rebuild_all_light();
        tileWindowRefresh();

        if (door->frame == 0) {
            return 0;
        }

        CacheEntry* artHandle;
        Art* art = artLock(door->fid, &artHandle);
        if (art == nullptr) {
            return -1;
        }

        Rect dirty;
        Rect temp;

        objectGetRect(door, &dirty);

        for (int frame = door->frame - 1; frame >= 0; frame--) {
            int x;
            int y;
            artGetFrameOffsets(art, frame, door->rotation, &x, &y);
            _obj_offset(door, -x, -y, &temp);
        }

        objectSetFrame(door, 0, &temp);
        rectUnion(&dirty, &temp, &dirty);

        tileWindowRefreshRect(&dirty, gElevation);

        artUnlock(artHandle);
        return 0;
    } else {
        // SFALL: Fix flags on non-door objects.
        if (_obj_is_portal(door)) {
            door->flags |= OBJECT_OPEN_DOOR;
        }

        _obj_rebuild_all_light();
        tileWindowRefresh();

        CacheEntry* artHandle;
        Art* art = artLock(door->fid, &artHandle);
        if (art == nullptr) {
            return -1;
        }

        int frameCount = artGetFrameCount(art);
        if (door->frame == frameCount - 1) {
            artUnlock(artHandle);
            return 0;
        }

        Rect dirty;
        Rect temp;

        objectGetRect(door, &dirty);

        for (int frame = door->frame + 1; frame < frameCount; frame++) {
            int x;
            int y;
            artGetFrameOffsets(art, frame, door->rotation, &x, &y);
            _obj_offset(door, x, y, &temp);
        }

        objectSetFrame(door, frameCount - 1, &temp);
        rectUnion(&dirty, &temp, &dirty);

        tileWindowRefreshRect(&dirty, gElevation);

        artUnlock(artHandle);
        return 0;
    }
}

// 0x49CCB8
int _obj_use_door(Object* user, Object* door, bool animateOnly)
{
    if (objectIsLocked(door)) {
        const char* sfx = sfxBuildOpenName(door, SCENERY_SOUND_EFFECT_LOCKED);
        soundPlayFile(sfx);
    }

    bool scriptOverrides = false;
    if (door->sid != -1) {
        scriptSetObjects(door->sid, user, door);
        scriptExecProc(door->sid, SCRIPT_PROC_USE);

        Script* script;
        if (scriptGetScript(door->sid, &script) == -1) {
            return -1;
        }

        scriptOverrides = script->scriptOverrides;
    }

    if (!scriptOverrides) {
        int start;
        int end;
        int step;
        if (door->frame != 0) {
            if (_obj_blocking_at(nullptr, door->tile, door->elevation) != nullptr) {
                MessageListItem messageListItem;
                char* text = getmsg(&gProtoMessageList, &messageListItem, 597);
                displayMonitorAddMessage(text);
                return -1;
            }
            start = 1;
            // TODO: strange logic, check if correct
            end = animateOnly ? -1 : 0;
            step = -1;
        } else {
            if (door->data.scenery.door.openFlags & 0x01) {
                return -1;
            }

            start = 0;
            // TODO: strange logic, check if correct
            end = animateOnly ? 2 : 1;
            step = 1;
        }

        reg_anim_begin(ANIMATION_REQUEST_RESERVED);

        for (int i = start; i != end; i += step) {
            if (i != 0) {
                if (!animateOnly) {
                    animationRegisterCallback(door, door, (AnimationCallback*)_set_door_state_closed, -1);
                }

                const char* sfx = sfxBuildOpenName(door, SCENERY_SOUND_EFFECT_CLOSED);
                animationRegisterPlaySoundEffect(door, sfx, -1);

                animationRegisterAnimateReversed(door, ANIM_STAND, 0);
            } else {
                if (!animateOnly) {
                    animationRegisterCallback(door, door, (AnimationCallback*)_set_door_state_open, -1);
                }

                const char* sfx = sfxBuildOpenName(door, SCENERY_SOUND_EFFECT_OPEN);
                animationRegisterPlaySoundEffect(door, sfx, -1);

                animationRegisterAnimate(door, ANIM_STAND, 0);
            }
        }

        animationRegisterCallbackForced(door, door, (AnimationCallback*)_check_door_state, -1);

        reg_anim_end();
    }

    return 0;
}

// 0x49CE7C
int _obj_use_container(Object* critter, Object* item)
{
    if (FID_TYPE(item->fid) != OBJ_TYPE_ITEM) {
        return -1;
    }

    Proto* itemProto;
    if (protoGetProto(item->pid, &itemProto) == -1) {
        return -1;
    }

    if (itemProto->item.type != ITEM_TYPE_CONTAINER) {
        return -1;
    }

    if (objectIsLocked(item)) {
        const char* sfx = sfxBuildOpenName(item, SCENERY_SOUND_EFFECT_LOCKED);
        soundPlayFile(sfx);

        if (critter == gDude) {
            MessageListItem messageListItem;
            // It is locked.
            messageListItem.num = 487;
            if (!messageListGetItem(&gProtoMessageList, &messageListItem)) {
                return -1;
            }

            displayMonitorAddMessage(messageListItem.text);
        }

        return -1;
    }

    bool overriden = false;
    if (item->sid != -1) {
        scriptSetObjects(item->sid, critter, item);
        scriptExecProc(item->sid, SCRIPT_PROC_USE);

        Script* script;
        if (scriptGetScript(item->sid, &script) == -1) {
            return -1;
        }

        overriden = script->scriptOverrides;
    }

    if (overriden) {
        return -1;
    }

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);

    if (item->frame == 0) {
        const char* sfx = sfxBuildOpenName(item, SCENERY_SOUND_EFFECT_OPEN);
        animationRegisterPlaySoundEffect(item, sfx, 0);
        animationRegisterAnimate(item, ANIM_STAND, 0);
    } else {
        const char* sfx = sfxBuildOpenName(item, SCENERY_SOUND_EFFECT_CLOSED);
        animationRegisterPlaySoundEffect(item, sfx, 0);
        animationRegisterAnimateReversed(item, ANIM_STAND, 0);
    }

    reg_anim_end();

    if (critter == gDude) {
        MessageListItem messageListItem;
        messageListItem.num = item->frame != 0
            ? 486 // You search the %s.
            : 485; // You close the %s.
        if (!messageListGetItem(&gProtoMessageList, &messageListItem)) {
            return -1;
        }

        char formattedText[260];
        const char* objectName = objectGetName(item);
        snprintf(formattedText, sizeof(formattedText), messageListItem.text, objectName);
        displayMonitorAddMessage(formattedText);
    }

    return 0;
}

// 0x49D078
int _obj_use_skill_on(Object* source, Object* target, int skill)
{
    if (objectIsJammed(target)) {
        if (source == gDude) {
            MessageListItem messageListItem;
            messageListItem.num = 2001;
            if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }
        return -1;
    }

    Proto* proto;
    if (protoGetProto(target->pid, &proto) == -1) {
        return -1;
    }

    bool scriptOverrides = false;
    if (target->sid != -1) {
        scriptSetObjects(target->sid, source, target);
        scriptSetActionBeingUsed(target->sid, skill);
        scriptExecProc(target->sid, SCRIPT_PROC_USE_SKILL_ON);

        Script* script;
        if (scriptGetScript(target->sid, &script) == -1) {
            return -1;
        }

        scriptOverrides = script->scriptOverrides;
    }

    if (!scriptOverrides) {
        skillUse(source, target, skill, 0);
    }

    return 0;
}

// 0x49D140
static bool _obj_is_portal(Object* obj)
{
    if (obj == nullptr) {
        return false;
    }

    Proto* proto;
    if (protoGetProto(obj->pid, &proto) == -1) {
        return false;
    }

    return proto->scenery.type == SCENERY_TYPE_DOOR;
}

// 0x49D178
static bool _obj_is_lockable(Object* obj)
{
    Proto* proto;

    if (obj == nullptr) {
        return false;
    }

    if (protoGetProto(obj->pid, &proto) == -1) {
        return false;
    }

    switch (PID_TYPE(obj->pid)) {
    case OBJ_TYPE_ITEM:
        if (proto->item.type == ITEM_TYPE_CONTAINER) {
            return true;
        }
        break;
    case OBJ_TYPE_SCENERY:
        if (proto->scenery.type == SCENERY_TYPE_DOOR) {
            return true;
        }
        break;
    }

    return false;
}

// 0x49D1C8
bool objectIsLocked(Object* obj)
{
    if (obj == nullptr) {
        return false;
    }

    ObjectData* data = &(obj->data);
    switch (PID_TYPE(obj->pid)) {
    case OBJ_TYPE_ITEM:
        return data->flags & CONTAINER_FLAG_LOCKED;
    case OBJ_TYPE_SCENERY:
        return data->scenery.door.openFlags & DOOR_FLAG_LOCKED;
    }

    return false;
}

// 0x49D20C
int objectLock(Object* object)
{
    if (object == nullptr) {
        return -1;
    }

    switch (PID_TYPE(object->pid)) {
    case OBJ_TYPE_ITEM:
        object->data.flags |= OBJ_LOCKED;
        break;
    case OBJ_TYPE_SCENERY:
        object->data.scenery.door.openFlags |= OBJ_LOCKED;
        break;
    default:
        return -1;
    }

    return 0;
}

// 0x49D250
int objectUnlock(Object* object)
{
    if (object == nullptr) {
        return -1;
    }

    switch (PID_TYPE(object->pid)) {
    case OBJ_TYPE_ITEM:
        object->data.flags &= ~OBJ_LOCKED;
        return 0;
    case OBJ_TYPE_SCENERY:
        object->data.scenery.door.openFlags &= ~OBJ_LOCKED;
        return 0;
    }

    return -1;
}

// 0x49D294
static bool _obj_is_openable(Object* obj)
{
    Proto* proto;

    if (obj == nullptr) {
        return false;
    }

    if (protoGetProto(obj->pid, &proto) == -1) {
        return false;
    }

    switch (PID_TYPE(obj->pid)) {
    case OBJ_TYPE_ITEM:
        if (proto->item.type == ITEM_TYPE_CONTAINER) {
            return true;
        }
        break;
    case OBJ_TYPE_SCENERY:
        if (proto->scenery.type == SCENERY_TYPE_DOOR) {
            return true;
        }
        break;
    }

    return false;
}

// 0x49D2E4
int objectIsOpen(Object* object)
{
    return object->frame != 0;
}

// 0x49D2F4
static int objectOpenClose(Object* obj)
{
    if (obj == nullptr) {
        return -1;
    }

    if (!_obj_is_openable(obj)) {
        return -1;
    }

    if (objectIsLocked(obj)) {
        return -1;
    }

    objectUnjamLock(obj);

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);

    if (obj->frame != 0) {
        animationRegisterCallbackForced(obj, obj, (AnimationCallback*)_set_door_state_closed, -1);

        const char* sfx = sfxBuildOpenName(obj, SCENERY_SOUND_EFFECT_CLOSED);
        animationRegisterPlaySoundEffect(obj, sfx, -1);

        animationRegisterAnimateReversed(obj, ANIM_STAND, 0);
    } else {
        animationRegisterCallbackForced(obj, obj, (AnimationCallback*)_set_door_state_open, -1);

        const char* sfx = sfxBuildOpenName(obj, SCENERY_SOUND_EFFECT_OPEN);
        animationRegisterPlaySoundEffect(obj, sfx, -1);
        animationRegisterAnimate(obj, ANIM_STAND, 0);
    }

    animationRegisterCallbackForced(obj, obj, (AnimationCallback*)_check_door_state, -1);

    reg_anim_end();

    return 0;
}

// 0x49D3D8
int objectOpen(Object* obj)
{
    if (obj->frame == 0) {
        objectOpenClose(obj);
    }

    return 0;
}

// 0x49D3F4
int objectClose(Object* obj)
{
    if (obj->frame != 0) {
        objectOpenClose(obj);
    }

    return 0;
}

// 0x49D410
static bool objectIsJammed(Object* obj)
{
    if (!_obj_is_lockable(obj)) {
        return false;
    }

    if (PID_TYPE(obj->pid) == OBJ_TYPE_SCENERY) {
        if ((obj->data.scenery.door.openFlags & OBJ_JAMMED) != 0) {
            return true;
        }
    } else {
        if ((obj->data.flags & OBJ_JAMMED) != 0) {
            return true;
        }
    }

    return false;
}

// jam_lock
// 0x49D448
int objectJamLock(Object* obj)
{
    if (!_obj_is_lockable(obj)) {
        return -1;
    }

    ObjectData* data = &(obj->data);
    switch (PID_TYPE(obj->pid)) {
    case OBJ_TYPE_ITEM:
        data->flags |= CONTAINER_FLAG_JAMMED;
        break;
    case OBJ_TYPE_SCENERY:
        data->scenery.door.openFlags |= DOOR_FLAG_JAMMGED;
        break;
    }

    return 0;
}

// 0x49D480
int objectUnjamLock(Object* obj)
{
    if (!_obj_is_lockable(obj)) {
        return -1;
    }

    ObjectData* data = &(obj->data);
    switch (PID_TYPE(obj->pid)) {
    case OBJ_TYPE_ITEM:
        data->flags &= ~CONTAINER_FLAG_JAMMED;
        break;
    case OBJ_TYPE_SCENERY:
        data->scenery.door.openFlags &= ~DOOR_FLAG_JAMMGED;
        break;
    }

    return 0;
}

// 0x49D4B8
int objectUnjamAll()
{
    Object* obj = objectFindFirst();
    while (obj != nullptr) {
        objectUnjamLock(obj);
        obj = objectFindNext();
    }

    return 0;
}

// critter_attempt_placement
// 0x49D4D4
int _obj_attempt_placement(Object* obj, int tile, int elevation, int radius)
{
    constexpr int maxDist = 7;
    constexpr int maxAttempts = 100;

    if (tile == -1) {
        return -1;
    }

    int newTile = tile;
    if (_obj_blocking_at(nullptr, tile, elevation) != nullptr) {
        // Find a suitable alternative tile where the dude can get to.
        int dist = radius >= 1 ? radius : 1;
        int attempts = 0;
        while (dist < maxDist) {
            attempts++;
            if (attempts >= maxAttempts) {
                break;
            }

            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                newTile = tileGetTileInDirection(tile, rotation, dist);
                if (_obj_blocking_at(nullptr, newTile, elevation) == nullptr
                    && dist > 1
                    && _make_path(gDude, gDude->tile, newTile, nullptr, 0) != 0) {
                    break;
                }
            }

            dist++;
        }

        // If location is too far (or not found at all), find any free adjacent tile, regardless if it's reachable or not.
        if (radius != 1 && dist > radius + 2) {
            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                int candidate = tileGetTileInDirection(tile, rotation, 1);
                if (_obj_blocking_at(nullptr, candidate, elevation) == nullptr) {
                    newTile = candidate;
                    break;
                }
            }
        }
    }

    Rect updatedRect;
    objectShow(obj, &updatedRect);

    Rect temp;
    if (objectSetLocation(obj, newTile, elevation, &temp) != -1) {
        rectUnion(&updatedRect, &temp, &updatedRect);

        if (elevation == gElevation) {
            tileWindowRefreshRect(&updatedRect, elevation);
        }
    }

    return 0;
}

// 0x49D628
int _objPMAttemptPlacement(Object* obj, int tile, int elevation)
{
    if (obj == nullptr) {
        return -1;
    }

    if (tile == -1) {
        return -1;
    }

    int v9 = tile;
    int v7 = 0;
    if (!wmEvalTileNumForPlacement(tile)) {
        v9 = gDude->tile;
        for (int v4 = 1; v4 <= 100; v4++) {
            // TODO: Check.
            v7++;
            v9 = tileGetTileInDirection(v9, v7 % ROTATION_COUNT, 1);
            if (wmEvalTileNumForPlacement(v9) != 0) {
                break;
            }

            if (tileDistanceBetween(gDude->tile, v9) > 8) {
                v9 = tile;
                break;
            }
        }
    }

    objectShow(obj, nullptr);
    objectSetLocation(obj, v9, elevation, nullptr);

    return 0;
}

} // namespace fallout
