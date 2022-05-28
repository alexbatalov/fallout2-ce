#include "proto_instance.h"

#include "animation.h"
#include "color.h"
#include "combat.h"
#include "critter.h"
#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "game_sound.h"
#include "geometry.h"
#include "interface.h"
#include "item.h"
#include "map.h"
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
#include "world_map.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// 0x49A990
MessageListItem stru_49A990;

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
    int objectType = object->pid >> 24;
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

    int scriptType = sid >> 24;
    if (scriptAdd(sidPtr, scriptType) == -1) {
        return -1;
    }

    Script* script;
    if (scriptGetScript(*sidPtr, &script) == -1) {
        return -1;
    }

    script->field_14 = sid & 0xFFFFFF;

    if (objectType == OBJ_TYPE_CRITTER) {
        object->field_80 = script->field_14;
    }

    if (scriptType == SCRIPT_TYPE_SPATIAL) {
        script->sp.built_tile = object->tile | ((object->elevation << 29) & 0xE0000000);
        script->sp.radius = 3;
    }

    if (object->id == -1) {
        object->id = scriptsNewObjectId();
    }

    script->field_1C = object->id;
    script->owner = object;

    _scr_find_str_run_info(sid & 0xFFFFFF, &(script->field_50), *sidPtr);

    return 0;
}

// 0x49AAC0
int _obj_new_sid_inst(Object* obj, int scriptType, int a3)
{
    if (a3 == -1) {
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

    script->field_14 = a3;
    if (scriptType == SCRIPT_TYPE_SPATIAL) {
        script->sp.built_tile = ((obj->elevation << 29) & 0xE0000000) | obj->tile;
        script->sp.radius = 3;
    }

    obj->sid = sid;

    obj->id = scriptsNewObjectId();
    script->field_1C = obj->id;

    script->owner = obj;

    _scr_find_str_run_info(a3 & 0xFFFFFF, &(script->field_50), sid);

    if ((obj->pid >> 24) == OBJ_TYPE_CRITTER) {
        obj->field_80 = script->field_14;
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

    if (((a2->fid & 0xF000000) >> 24) == OBJ_TYPE_TILE) {
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

        if ((a2->pid >> 24) == OBJ_TYPE_CRITTER && critterIsDead(a2)) {
            messageListItem.num = 491 + randomBetween(0, 1);
        } else {
            messageListItem.num = 490;
        }

        if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
            const char* objectName = objectGetName(a2);

            char formattedText[260];
            sprintf(formattedText, messageListItem.text, objectName);

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

    if ((target->fid & 0xF000000) >> 24 == OBJ_TYPE_TILE) {
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
        if (description != NULL && strcmp(description, _proto_none_str) == 0) {
            description = NULL;
        }

        if (description == NULL || *description == '\0') {
            MessageListItem messageListItem;
            messageListItem.num = 493;
            if (!messageListGetItem(&gProtoMessageList, &messageListItem)) {
                debugPrint("\nError: Can't find msg num!");
            }
            fn(messageListItem.text);
        } else {
            if ((target->pid >> 24) != OBJ_TYPE_CRITTER || !critterIsDead(target)) {
                fn(description);
            }
        }
    }

    if (critter == NULL || critter != gDude) {
        return 0;
    }

    char formattedText[260];

    int type = target->pid >> 24;
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
            if (item2 != NULL && itemGetType(item2) != ITEM_TYPE_WEAPON) {
                item2 = NULL;
            }

            if (!messageListGetItem(&gProtoMessageList, &hpMessageListItem)) {
                debugPrint("\nError: Can't find msg num!");
                exit(1);
            }

            if (item2 != NULL) {
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
                sprintf(format, "%s%s", hpMessageListItem.text, weaponMessageListItem.text);

                if (ammoGetCaliber(item2) != 0) {
                    const int ammoTypePid = weaponGetAmmoTypePid(item2);
                    const char* ammoName = protoGetName(ammoTypePid);
                    const int ammoCapacity = ammoGetCapacity(item2);
                    const int ammoQuantity = ammoGetQuantity(item2);
                    const char* weaponName = objectGetName(item2);
                    const int maxiumHitPoints = critterGetStat(target, STAT_MAXIMUM_HIT_POINTS);
                    const int currentHitPoints = critterGetStat(target, STAT_CURRENT_HIT_POINTS);
                    sprintf(formattedText,
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
                    sprintf(formattedText,
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
                sprintf(formattedText, hpMessageListItem.text, currentHitPoints, maxiumHitPoints);
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

                sprintf(formattedText, v66.text, hpMessageListItem.text);
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

                sprintf(formattedText, v63.text, hpMessageListItem.text);
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
                sprintf(formattedText, carMessageListItem.text, 100 * carGetFuel() / 80000);
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
                sprintf(formattedText, weaponMessageListItem.text, ammoQuantity, ammoCapacity, ammoName);
                fn(formattedText);
            }
        } else if (itemType == ITEM_TYPE_AMMO) {
            MessageListItem ammoMessageListItem;
            ammoMessageListItem.num = 510;

            if (!messageListGetItem(&gProtoMessageList, &ammoMessageListItem)) {
                debugPrint("\nError: Can't find msg num!");
                exit(1);
            }

            sprintf(formattedText,
                ammoMessageListItem.text,
                ammoGetArmorClassModifier(target));
            fn(formattedText);

            ammoMessageListItem.num++;
            if (!messageListGetItem(&gProtoMessageList, &ammoMessageListItem)) {
                debugPrint("\nError: Can't find msg num!");
                exit(1);
            }

            sprintf(formattedText,
                ammoMessageListItem.text,
                ammoGetDamageResistanceModifier(target));
            fn(formattedText);

            ammoMessageListItem.num++;
            if (!messageListGetItem(&gProtoMessageList, &ammoMessageListItem)) {
                debugPrint("\nError: Can't find msg num!");
                exit(1);
            }

            sprintf(formattedText,
                ammoMessageListItem.text,
                ammoGetDamageMultiplier(target),
                ammoGetDamageDivisor(target));
            fn(formattedText);
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
int _obj_remove_from_inven(Object* critter, Object* item)
{
    Rect updatedRect;
    int fid;
    int v11 = 0;
    if (critterGetItem2(critter) == item) {
        if (critter != gDude || interfaceGetCurrentHand()) {
            fid = buildFid(1, critter->fid & 0xFFF, (critter->fid & 0xFF0000) >> 16, 0, critter->rotation);
            objectSetFid(critter, fid, &updatedRect);
            v11 = 2;
        } else {
            v11 = 1;
        }
    } else if (critterGetItem1(critter) == item) {
        if (critter == gDude && !interfaceGetCurrentHand()) {
            fid = buildFid(1, critter->fid & 0xFFF, (critter->fid & 0xFF0000) >> 16, 0, critter->rotation);
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

            fid = buildFid(1, v5, (critter->fid & 0xFF0000) >> 16, (critter->fid & 0xF000) >> 12, critter->rotation);
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
    if (a2 == NULL) {
        return -1;
    }

    bool scriptOverrides = false;
    if (a1->sid != -1) {
        scriptSetObjects(a1->sid, a2, NULL);
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
        if (owner == NULL) {
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
    if (obj == NULL) {
        return -1;
    }

    int elev;
    Object* owner = obj->owner;
    if (owner != NULL) {
        _obj_remove_from_inven(owner, obj);
    } else {
        elev = obj->elevation;
    }

    queueRemoveEvents(obj);

    Rect rect;
    objectDestroy(obj, &rect);

    if (owner == NULL) {
        tileWindowRefreshRect(&rect, elev);
    }

    return 0;
}

// Read a book.
//
// 0x49B9F0
int _obj_use_book(Object* book)
{
    MessageListItem messageListItem;

    int messageId = -1;
    int skill;

    switch (book->pid) {
    case PROTO_ID_BIG_BOOK_OF_SCIENCE:
        // You learn new science information.
        messageId = 802;
        skill = SKILL_SCIENCE;
        break;
    case PROTO_ID_DEANS_ELECTRONICS:
        // You learn a lot about repairing broken electronics.
        messageId = 803;
        skill = SKILL_REPAIR;
        break;
    case PROTO_ID_FIRST_AID_BOOK:
        // You learn new ways to heal injury.
        messageId = 804;
        skill = SKILL_FIRST_AID;
        break;
    case PROTO_ID_SCOUT_HANDBOOK:
        // You learn a lot about wilderness survival.
        messageId = 806;
        skill = SKILL_OUTDOORSMAN;
        break;
    case PROTO_ID_GUNS_AND_BULLETS:
        // You learn how to handle your guns better.
        messageId = 805;
        skill = SKILL_SMALL_GUNS;
        break;
    }

    if (messageId == -1) {
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
int _obj_use_flare(Object* critter_obj, Object* flare)
{
    MessageListItem messageListItem;

    if (flare->pid != PROTO_ID_FLARE) {
        return -1;
    }

    if ((flare->flags & OBJECT_FLAG_0x2000) != 0) {
        if (critter_obj == gDude) {
            // The flare is already lit.
            messageListItem.num = 588;
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }
    } else {
        if (critter_obj == gDude) {
            // You light the flare.
            messageListItem.num = 588;
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }

        flare->pid = PROTO_ID_LIT_FLARE;

        objectSetLight(flare, 8, 0x10000, NULL);
        queueAddEvent(72000, flare, NULL, EVENT_TYPE_FLARE);
    }

    return 0;
}

// 0x49BC60
int _obj_use_radio(Object* item)
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
int _obj_use_explosive(Object* explosive)
{
    MessageListItem messageListItem;

    int pid = explosive->pid;
    if (pid != PROTO_ID_DYNAMITE_I
        && pid != PROTO_ID_PLASTIC_EXPLOSIVES_I
        && pid != PROTO_ID_DYNAMITE_II
        && pid != PROTO_ID_PLASTIC_EXPLOSIVES_II) {
        return -1;
    }

    if ((explosive->flags & OBJECT_FLAG_0x2000) != 0) {
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

            if (pid == PROTO_ID_DYNAMITE_I) {
                explosive->pid = PROTO_ID_DYNAMITE_II;
            } else if (pid == PROTO_ID_PLASTIC_EXPLOSIVES_I) {
                explosive->pid = PROTO_ID_PLASTIC_EXPLOSIVES_II;
            }

            int delay = 10 * seconds;

            int roll;
            if (perkHasRank(gDude, PERK_DEMOLITION_EXPERT)) {
                roll = ROLL_SUCCESS;
            } else {
                roll = skillRoll(gDude, SKILL_TRAPS, 0, NULL);
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

            queueAddEvent(delay, explosive, NULL, eventType);
        }
    }

    return 2;
}

// Recharge car with given item
// Returns -1 when car cannot be recharged with given item.
// Returns 1 when car is recharged.
//
// 0x49BDE8
int _obj_use_power_on_car(Object* item)
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

    if (carGetFuel() < CAR_FUEL_MAX) {
        int energy = ammoGetQuantity(item) * energyDensity;
        int capacity = ammoGetCapacity(item);

        // NOTE: that function will never return -1
        if (carAddFuel(energy / capacity) == -1) {
            return -1;
        }

        // You charge the car with more power.
        messageNum = 595;
    } else {
        // The car is already full of power.
        messageNum = 596;
    }

    char* text = getmsg(&gProtoMessageList, &messageListItem, messageNum);
    displayMonitorAddMessage(text);

    return 1;
}

// 0x49BE88
int _obj_use_misc_item(Object* item)
{

    if (item == NULL) {
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
int _protinstTestDroppedExplosive(Object* a1)
{
    if (a1->pid == PROTO_ID_DYNAMITE_II || a1->pid == PROTO_ID_PLASTIC_EXPLOSIVES_II) {
        Attack attack;
        attackInit(&attack, gDude, 0, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);
        attack.attackerFlags = DAM_HIT;
        attack.tile = gDude->tile;
        _compute_explosion_on_extras(&attack, 0, 0, 1);

        int team = gDude->data.critter.combat.team;
        Object* v2 = NULL;
        for (int index = 0; index < attack.extrasLength; index++) {
            Object* v5 = attack.extras[index];
            if (v5 != gDude
                && v5->data.critter.combat.team != team
                && statRoll(v5, STAT_PERCEPTION, 0, NULL) >= 2) {
                _critter_set_who_hit_me(v5, gDude);
                if (v2 == NULL) {
                    v2 = v5;
                }
            }
        }

        if (v2 != NULL && !isInCombat()) {
            STRUCT_664980 attack;
            attack.attacker = v2;
            attack.defender = gDude;
            attack.actionPointsBonus = 0;
            attack.accuracyBonus = 0;
            attack.minDamage = 0;
            attack.maxDamage = 99999;
            attack.field_1C = 0;
            scriptsRequestCombat(&attack);
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
        if (root != NULL) {
            int flags = a2->flags & OBJECT_IN_ANY_HAND;
            itemRemove(root, a2, 1);
            Object* v8 = _item_replace(root, a2, flags);
            if (root == gDude) {
                int leftItemAction;
                int rightItemAction;
                interfaceGetItemActions(&leftItemAction, &rightItemAction);
                if (v8 == NULL) {
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
        } else if (rc == 2 && root != NULL) {
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
int _protinst_default_use_item(Object* a1, Object* a2, Object* item)
{
    char formattedText[90];
    MessageListItem messageListItem;

    int rc;
    switch (itemGetType(item)) {
    case ITEM_TYPE_DRUG:
        if ((a2->pid >> 24) != OBJ_TYPE_CRITTER) {
            if (a1 == gDude) {
                // That does nothing
                messageListItem.num = 582;
                if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                    displayMonitorAddMessage(messageListItem.text);
                }
            }
            return -1;
        }

        if (critterIsDead(a2)) {
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

        rc = _item_d_take_drug(a2, item);

        if (a1 == gDude && a2 != gDude) {
            // TODO: Looks like there is bug in this branch, message 580 will never be shown,
            // as we can only be here when target is not dude.

            // 580: You use the %s.
            // 581: You use the %s on %s.
            messageListItem.num = 580 + (a2 != gDude);
            if (!messageListGetItem(&gProtoMessageList, &messageListItem)) {
                return -1;
            }

            sprintf(formattedText, messageListItem.text, objectGetName(item), objectGetName(a2));
            displayMonitorAddMessage(formattedText);
        }

        if (a2 == gDude) {
            interfaceRenderHitPoints(true);
        }

        return rc;
    case ITEM_TYPE_AMMO:
        rc = _obj_use_power_on_car(item);
        if (rc == 1) {
            return 1;
        }
        break;
    case ITEM_TYPE_WEAPON:
    case ITEM_TYPE_MISC:
        rc = _obj_use_flare(a1, item);
        if (rc == 0) {
            return 0;
        }
        break;
    }

    messageListItem.num = 582;
    if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
        sprintf(formattedText, messageListItem.text);
        displayMonitorAddMessage(formattedText);
    }
    return -1;
}

// 0x49C3CC
int _protinst_use_item_on(Object* a1, Object* a2, Object* item)
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
            if (a2->sid == -1) {
                return _protinst_default_use_item(a1, a2, item);
            }

            scriptSetObjects(a2->sid, a1, item);
            scriptExecProc(a2->sid, SCRIPT_PROC_USE_OBJ_ON);

            if (scriptGetScript(a2->sid, &script) == -1) {
                return -1;
            }

            if (!script->scriptOverrides) {
                return _protinst_default_use_item(a1, a2, item);
            }
        } else {
            scriptSetObjects(item->sid, a1, a2);
            scriptExecProc(item->sid, SCRIPT_PROC_USE_OBJ_ON);

            if (scriptGetScript(item->sid, &script) == -1) {
                return -1;
            }

            if (script->field_28 == 0) {
                if (a2->sid == -1) {
                    return _protinst_default_use_item(a1, a2, item);
                }

                scriptSetObjects(a2->sid, a1, item);
                scriptExecProc(a2->sid, SCRIPT_PROC_USE_OBJ_ON);

                Script* script;
                if (scriptGetScript(a2->sid, &script) == -1) {
                    return -1;
                }

                if (!script->scriptOverrides) {
                    return _protinst_default_use_item(a1, a2, item);
                }
            }
        }

        return script->field_28;
    }

    if (isInCombat()) {
        MessageListItem messageListItem;
        // You cannot do that in combat.
        messageListItem.num = 902;
        if (a1 == gDude) {
            if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }
        return -1;
    }

    if (skillUse(a1, a2, skill, criticalChanceModifier) != 0) {
        return 0;
    }

    if (randomBetween(1, 10) != 1) {
        return 0;
    }

    MessageListItem messageListItem;
    messageListItem.num = messageId;
    if (a1 == gDude) {
        if (messageListGetItem(&gProtoMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
    }

    return 1;
}

// 0x49C5FC
int _obj_use_item_on(Object* a1, Object* a2, Object* a3)
{
    int rc = _protinst_use_item_on(a1, a2, a3);

    if (rc == 1) {
        if (a1 != NULL) {
            int flags = a3->flags & OBJECT_IN_ANY_HAND;
            itemRemove(a1, a3, 1);

            Object* v7 = _item_replace(a1, a3, flags);

            int leftItemAction;
            int rightItemAction;
            if (a1 == gDude) {
                interfaceGetItemActions(&leftItemAction, &rightItemAction);
            }

            if (v7 == NULL) {
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

        _obj_destroy(a3);

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
int _obj_use(Object* a1, Object* a2)
{
    int type = (a2->fid & 0xF000000) >> 24;
    if (a1 == gDude) {
        if (type != OBJ_TYPE_SCENERY) {
            return -1;
        }
    } else {
        if (type != OBJ_TYPE_SCENERY) {
            return 0;
        }
    }

    Proto* sceneryProto;
    if (protoGetProto(a2->pid, &sceneryProto) == -1) {
        return -1;
    }

    if ((a2->pid >> 24) == OBJ_TYPE_SCENERY && sceneryProto->scenery.type == SCENERY_TYPE_DOOR) {
        return _obj_use_door(a1, a2, 0);
    }

    bool scriptOverrides = false;

    if (a2->sid != -1) {
        scriptSetObjects(a2->sid, a1, a2);
        scriptExecProc(a2->sid, SCRIPT_PROC_USE);

        Script* script;
        if (scriptGetScript(a2->sid, &script) == -1) {
            return -1;
        }

        scriptOverrides = script->scriptOverrides;
    }

    if (!scriptOverrides) {
        if ((a2->pid >> 24) == OBJ_TYPE_SCENERY) {
            if (sceneryProto->scenery.type == SCENERY_TYPE_LADDER_DOWN) {
                if (useLadderDown(a1, a2, 0) == 0) {
                    scriptOverrides = true;
                }
            } else if (sceneryProto->scenery.type == SCENERY_TYPE_LADDER_UP) {
                if (useLadderUp(a1, a2, 0) == 0) {
                    scriptOverrides = true;
                }
            } else if (sceneryProto->scenery.type == SCENERY_TYPE_STAIRS) {
                if (useStairs(a1, a2, 0) == 0) {
                    scriptOverrides = true;
                }
            }
        }
    }

    if (!scriptOverrides) {
        if (a1 == gDude) {
            // You see: %s
            MessageListItem messageListItem;
            messageListItem.num = 480;
            if (!messageListGetItem(&gProtoMessageList, &messageListItem)) {
                return -1;
            }

            char formattedText[260];
            const char* name = objectGetName(a2);
            sprintf(formattedText, messageListItem.text, name);
            displayMonitorAddMessage(formattedText);
        }
    }

    scriptsExecMapUpdateProc();

    return 0;
}

// 0x49C900
int useLadderDown(Object* a1, Object* ladder, int a3)
{
    int builtTile = ladder->data.scenery.ladder.field_4;
    if (builtTile == -1) {
        return -1;
    }

    int tile = builtTile & 0x3FFFFFF;
    int elevation = (builtTile & 0xE0000000) >> 29;
    if (ladder->data.scenery.ladder.field_0 != 0) {
        MapTransition transition;
        memset(&transition, 0, sizeof(transition));

        transition.map = ladder->data.scenery.ladder.field_0;
        transition.elevation = elevation;
        transition.tile = tile;
        transition.rotation = (builtTile & 0x1C000000) >> 26;

        mapSetTransition(&transition);

        _wmMapMarkMapEntranceState(transition.map, elevation, 1);
    } else {
        Rect updatedRect;
        if (objectSetLocation(a1, tile, elevation, &updatedRect) == -1) {
            return -1;
        }

        tileWindowRefreshRect(&updatedRect, gElevation);
    }

    return 0;
}

// 0x49C9A4
int useLadderUp(Object* a1, Object* ladder, int a3)
{
    int builtTile = ladder->data.scenery.ladder.field_4;
    if (builtTile == -1) {
        return -1;
    }

    int tile = builtTile & 0x3FFFFFF;
    int elevation = (builtTile & 0xE0000000) >> 29;
    if (ladder->data.scenery.ladder.field_0 != 0) {
        MapTransition transition;
        memset(&transition, 0, sizeof(transition));

        transition.map = ladder->data.scenery.ladder.field_0;
        transition.elevation = elevation;
        transition.tile = tile;
        transition.rotation = (builtTile & 0x1C000000) >> 26;

        mapSetTransition(&transition);

        _wmMapMarkMapEntranceState(transition.map, elevation, 1);
    } else {
        Rect updatedRect;
        if (objectSetLocation(a1, tile, elevation, &updatedRect) == -1) {
            return -1;
        }

        tileWindowRefreshRect(&updatedRect, gElevation);
    }

    return 0;
}

// 0x49CA48
int useStairs(Object* a1, Object* stairs, int a3)
{
    int builtTile = stairs->data.scenery.stairs.field_4;
    if (builtTile == -1) {
        return -1;
    }

    int tile = builtTile & 0x3FFFFFF;
    int elevation = (builtTile & 0xE0000000) >> 29;
    if (stairs->data.scenery.stairs.field_0 > 0) {
        MapTransition transition;
        memset(&transition, 0, sizeof(transition));

        transition.map = stairs->data.scenery.stairs.field_0;
        transition.elevation = elevation;
        transition.tile = tile;
        transition.rotation = (builtTile & 0x1C000000) >> 26;

        mapSetTransition(&transition);

        _wmMapMarkMapEntranceState(transition.map, elevation, 1);
    } else {
        Rect updatedRect;
        if (objectSetLocation(a1, tile, elevation, &updatedRect) == -1) {
            return -1;
        }

        tileWindowRefreshRect(&updatedRect, gElevation);
    }

    return 0;
}

// 0x49CAF4
int _set_door_state_open(Object* a1, Object* a2)
{
    a1->data.scenery.door.openFlags |= 0x01;
    return 0;
}

// 0x49CB04
int _set_door_state_closed(Object* a1, Object* a2)
{
    a1->data.scenery.door.openFlags &= ~0x01;
    return 0;
}

// 0x49CB14
int _check_door_state(Object* a1, Object* a2)
{
    if ((a1->data.scenery.door.openFlags & 0x01) == 0) {
        a1->flags &= ~OBJECT_OPEN_DOOR;

        _obj_rebuild_all_light();
        tileWindowRefresh();

        if (a1->frame == 0) {
            return 0;
        }

        CacheEntry* artHandle;
        Art* art = artLock(a1->fid, &artHandle);
        if (art == NULL) {
            return -1;
        }

        Rect dirty;
        Rect temp;

        objectGetRect(a1, &dirty);

        for (int frame = a1->frame - 1; frame >= 0; frame--) {
            int x;
            int y;
            artGetFrameOffsets(art, frame, a1->rotation, &x, &y);
            _obj_offset(a1, -x, -y, &temp);
        }

        objectSetFrame(a1, 0, &temp);
        rectUnion(&dirty, &temp, &dirty);

        tileWindowRefreshRect(&dirty, gElevation);

        artUnlock(artHandle);
        return 0;
    } else {
        a1->flags |= OBJECT_OPEN_DOOR;

        _obj_rebuild_all_light();
        tileWindowRefresh();

        CacheEntry* artHandle;
        Art* art = artLock(a1->fid, &artHandle);
        if (art == NULL) {
            return -1;
        }

        int frameCount = artGetFrameCount(art);
        if (a1->frame == frameCount - 1) {
            artUnlock(artHandle);
            return 0;
        }

        Rect dirty;
        Rect temp;

        objectGetRect(a1, &dirty);

        for (int frame = a1->frame + 1; frame < frameCount; frame++) {
            int x;
            int y;
            artGetFrameOffsets(art, frame, a1->rotation, &x, &y);
            _obj_offset(a1, x, y, &temp);
        }

        objectSetFrame(a1, frameCount - 1, &temp);
        rectUnion(&dirty, &temp, &dirty);

        tileWindowRefreshRect(&dirty, gElevation);

        artUnlock(artHandle);
        return 0;
    }
}

// 0x49CCB8
int _obj_use_door(Object* a1, Object* a2, int a3)
{
    if (objectIsLocked(a2)) {
        const char* sfx = sfxBuildOpenName(a2, SCENERY_SOUND_EFFECT_LOCKED);
        soundPlayFile(sfx);
    }

    bool scriptOverrides = false;
    if (a2->sid != -1) {
        scriptSetObjects(a2->sid, a1, a2);
        scriptExecProc(a2->sid, SCRIPT_PROC_USE);

        Script* script;
        if (scriptGetScript(a2->sid, &script) == -1) {
            return -1;
        }

        scriptOverrides = script->scriptOverrides;
    }

    if (!scriptOverrides) {
        int start;
        int end;
        int step;
        if (a2->frame != 0) {
            if (_obj_blocking_at(NULL, a2->tile, a2->elevation) != 0) {
                MessageListItem messageListItem;
                char* text = getmsg(&gProtoMessageList, &messageListItem, 597);
                displayMonitorAddMessage(text);
                return -1;
            }
            start = 1;
            end = (a3 == 0) - 1;
            step = -1;
        } else {
            if (a2->data.scenery.door.openFlags & 0x01) {
                return -1;
            }

            start = 0;
            end = (a3 != 0) + 1;
            step = 1;
        }

        reg_anim_begin(2);

        for (int i = start; i != end; i += step) {
            if (i != 0) {
                if (a3 == 0) {
                    reg_anim_11_0(a2, a2, _set_door_state_closed, -1);
                }

                const char* sfx = sfxBuildOpenName(a2, SCENERY_SOUND_EFFECT_CLOSED);
                reg_anim_play_sfx(a2, sfx, -1);

                reg_anim_animate_reverse(a2, 0, 0);
            } else {
                if (a3 == 0) {
                    reg_anim_11_0(a2, a2, _set_door_state_open, -1);
                }

                const char* sfx = sfxBuildOpenName(a2, SCENERY_SOUND_EFFECT_CLOSED);
                reg_anim_play_sfx(a2, sfx, -1);

                reg_anim_animate(a2, 0, 0);
            }
        }

        reg_anim_11_1(a2, a2, _check_door_state, -1);

        reg_anim_end();
    }

    return 0;
}

// 0x49CE7C
int _obj_use_container(Object* critter, Object* item)
{
    if (((item->fid & 0xF000000) >> 24) != OBJ_TYPE_ITEM) {
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

    reg_anim_begin(2);

    if (item->frame == 0) {
        const char* sfx = sfxBuildOpenName(item, SCENERY_SOUND_EFFECT_OPEN);
        reg_anim_play_sfx(item, sfx, 0);
        reg_anim_animate(item, 0, 0);
    } else {
        const char* sfx = sfxBuildOpenName(item, SCENERY_SOUND_EFFECT_CLOSED);
        reg_anim_play_sfx(item, sfx, 0);
        reg_anim_animate_reverse(item, 0, 0);
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
        sprintf(formattedText, messageListItem.text, objectName);
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

// 0x49D178
bool _obj_is_lockable(Object* obj)
{
    Proto* proto;

    if (obj == NULL) {
        return false;
    }

    if (protoGetProto(obj->pid, &proto) == -1) {
        return false;
    }

    switch (obj->pid >> 24) {
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
    if (obj == NULL) {
        return false;
    }

    ObjectData* data = &(obj->data);
    switch (obj->pid >> 24) {
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
    if (object == NULL) {
        return -1;
    }

    switch (object->pid >> 24) {
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
    if (object == NULL) {
        return -1;
    }

    switch (object->pid >> 24) {
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
bool _obj_is_openable(Object* obj)
{
    Proto* proto;

    if (obj == NULL) {
        return false;
    }

    if (protoGetProto(obj->pid, &proto) == -1) {
        return false;
    }

    switch (obj->pid >> 24) {
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
int objectOpenClose(Object* obj)
{
    if (obj == NULL) {
        return -1;
    }

    if (!_obj_is_openable(obj)) {
        return -1;
    }

    if (objectIsLocked(obj)) {
        return -1;
    }

    objectUnjamLock(obj);

    reg_anim_begin(2);

    if (obj->frame != 0) {
        reg_anim_11_1(obj, obj, _set_door_state_closed, -1);

        const char* sfx = sfxBuildOpenName(obj, SCENERY_SOUND_EFFECT_CLOSED);
        reg_anim_play_sfx(obj, sfx, -1);

        reg_anim_animate_reverse(obj, 0, 0);
    } else {
        reg_anim_11_1(obj, obj, _set_door_state_open, -1);

        const char* sfx = sfxBuildOpenName(obj, SCENERY_SOUND_EFFECT_OPEN);
        reg_anim_play_sfx(obj, sfx, -1);
        reg_anim_animate(obj, 0, 0);
    }

    reg_anim_11_1(obj, obj, _check_door_state, -1);

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
bool objectIsJammed(Object* obj)
{
    if (!_obj_is_lockable(obj)) {
        return false;
    }

    if ((obj->pid >> 24) == OBJ_TYPE_SCENERY) {
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
    switch (obj->pid >> 24) {
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
    switch (obj->pid >> 24) {
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
    while (obj != NULL) {
        objectUnjamLock(obj);
        obj = objectFindNext();
    }

    return 0;
}

// critter_attempt_placement
// 0x49D4D4
int _obj_attempt_placement(Object* obj, int tile, int elevation, int a4)
{
    if (tile == -1) {
        return -1;
    }

    int newTile = tile;
    if (_obj_blocking_at(NULL, tile, elevation) != NULL) {
        int v6 = a4;
        if (a4 < 1) {
            v6 = 1;
        }

        int attempts = 0;
        while (v6 < 7) {
            attempts++;
            if (attempts >= 100) {
                break;
            }

            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                newTile = tileGetTileInDirection(tile, rotation, v6);
                if (_obj_blocking_at(NULL, newTile, elevation) == NULL && v6 > 1 && _make_path(gDude, gDude->tile, newTile, NULL, 0) != 0) {
                    break;
                }
            }

            v6++;
        }

        if (a4 != 1 && v6 > a4 + 2) {
            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                int candidate = tileGetTileInDirection(tile, rotation, 1);
                if (_obj_blocking_at(NULL, candidate, elevation) == NULL) {
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
    if (obj == NULL) {
        return -1;
    }

    if (tile == -1) {
        return -1;
    }

    int v9 = tile;
    int v7 = 0;
    if (!_wmEvalTileNumForPlacement(tile)) {
        v9 = gDude->tile;
        for (int v4 = 1; v4 <= 100; v4++) {
            // TODO: Check.
            v7++;
            v9 = tileGetTileInDirection(v9, v7 % 6, 1);
            if (_wmEvalTileNumForPlacement(v9) != 0) {
                break;
            }

            if (tileDistanceBetween(gDude->tile, v9) > 8) {
                v9 = tile;
                break;
            }
        }
    }

    objectShow(obj, NULL);
    objectSetLocation(obj, v9, elevation, NULL);

    return 0;
}
