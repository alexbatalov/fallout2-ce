#include "party_member.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "animation.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "combat_ai_defs.h"
#include "config.h"
#include "critter.h"
#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "game_dialog.h"
#include "item.h"
#include "loadsave.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "proto.h"
#include "proto_instance.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "skill.h"
#include "stat.h"
#include "string_parsers.h"
#include "text_object.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

// SFALL: Enable party members with level 6 protos to reach level 6.
// CE: There are several party members who have 6 pids, but for unknown reason
// the original code cap was 5. This fix affects:
// - Dogmeat
// - Goris
// - Sulik
// - Vik
#define PARTY_MEMBER_MAX_LEVEL 6

typedef struct PartyMemberDescription {
    bool areaAttackMode[AREA_ATTACK_MODE_COUNT];
    bool runAwayMode[RUN_AWAY_MODE_COUNT];
    bool bestWeapon[BEST_WEAPON_COUNT];
    bool distanceMode[DISTANCE_COUNT];
    bool attackWho[ATTACK_WHO_COUNT];
    bool chemUse[CHEM_USE_COUNT];
    bool disposition[DISPOSITION_COUNT];
    int level_minimum;
    int level_up_every;
    int level_pids_num;
    int level_pids[PARTY_MEMBER_MAX_LEVEL];
} PartyMemberDescription;

typedef struct STRU_519DBC {
    int field_0;
    int field_4; // party member level
    int field_8; // early what?
} STRU_519DBC;

typedef struct PartyMemberListItem {
    Object* object;
    Script* script;
    int* vars;
    struct PartyMemberListItem* next;
} PartyMemberListItem;

static int partyMemberGetDescription(Object* object, PartyMemberDescription** partyMemberDescriptionPtr);
static void partyMemberDescriptionInit(PartyMemberDescription* partyMemberDescription);
static int _partyMemberPrepLoadInstance(PartyMemberListItem* a1);
static int _partyMemberRecoverLoadInstance(PartyMemberListItem* a1);
static int _partyMemberNewObjID();
static int _partyMemberNewObjIDRecurseFind(Object* object, int objectId);
static int _partyMemberPrepItemSave(Object* object);
static int _partyMemberItemSave(Object* object);
static int _partyMemberItemRecover(PartyMemberListItem* a1);
static int _partyMemberClearItemList();
static int partyFixMultipleMembers();
static int _partyMemberCopyLevelInfo(Object* object, int a2);

// 0x519D9C
int gPartyMemberDescriptionsLength = 0;

// 0x519DA0
int* gPartyMemberPids = nullptr;

//
static PartyMemberListItem* _itemSaveListHead = nullptr;

// List of party members, it's length is [gPartyMemberDescriptionsLength] + 20.
//
// 0x519DA8
PartyMemberListItem* gPartyMembers = nullptr;

// Number of critters added to party.
//
// 0x519DAC
static int gPartyMembersLength = 0;

// 0x519DB0
static int _partyMemberItemCount = 20000;

// 0x519DB4
static int _partyStatePrepped = 0;

// 0x519DB8
static PartyMemberDescription* gPartyMemberDescriptions = nullptr;

// 0x519DBC
static STRU_519DBC* _partyMemberLevelUpInfoList = nullptr;

// 0x519DC0
static int _curID = 20000;

// partyMember_init
// 0x493BC0
int partyMembersInit()
{
    Config config;

    gPartyMemberDescriptionsLength = 0;

    if (!configInit(&config)) {
        return -1;
    }

    if (!configRead(&config, "data\\party.txt", true)) {
        goto err;
    }

    char section[50];
    snprintf(section, sizeof(section), "Party Member %d", gPartyMemberDescriptionsLength);

    int partyMemberPid;
    while (configGetInt(&config, section, "party_member_pid", &partyMemberPid)) {
        gPartyMemberDescriptionsLength++;
        snprintf(section, sizeof(section), "Party Member %d", gPartyMemberDescriptionsLength);
    }

    gPartyMemberPids = (int*)internal_malloc(sizeof(*gPartyMemberPids) * gPartyMemberDescriptionsLength);
    if (gPartyMemberPids == nullptr) {
        goto err;
    }

    memset(gPartyMemberPids, 0, sizeof(*gPartyMemberPids) * gPartyMemberDescriptionsLength);

    gPartyMembers = (PartyMemberListItem*)internal_malloc(sizeof(*gPartyMembers) * (gPartyMemberDescriptionsLength + 20));
    if (gPartyMembers == nullptr) {
        goto err;
    }

    memset(gPartyMembers, 0, sizeof(*gPartyMembers) * (gPartyMemberDescriptionsLength + 20));

    gPartyMemberDescriptions = (PartyMemberDescription*)internal_malloc(sizeof(*gPartyMemberDescriptions) * gPartyMemberDescriptionsLength);
    if (gPartyMemberDescriptions == nullptr) {
        goto err;
    }

    memset(gPartyMemberDescriptions, 0, sizeof(*gPartyMemberDescriptions) * gPartyMemberDescriptionsLength);

    _partyMemberLevelUpInfoList = (STRU_519DBC*)internal_malloc(sizeof(*_partyMemberLevelUpInfoList) * gPartyMemberDescriptionsLength);
    if (_partyMemberLevelUpInfoList == nullptr) goto err;

    memset(_partyMemberLevelUpInfoList, 0, sizeof(*_partyMemberLevelUpInfoList) * gPartyMemberDescriptionsLength);

    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        snprintf(section, sizeof(section), "Party Member %d", index);

        if (!configGetInt(&config, section, "party_member_pid", &partyMemberPid)) {
            break;
        }

        PartyMemberDescription* partyMemberDescription = &(gPartyMemberDescriptions[index]);

        gPartyMemberPids[index] = partyMemberPid;

        partyMemberDescriptionInit(partyMemberDescription);

        char* string;

        if (configGetString(&config, section, "area_attack_mode", &string)) {
            while (*string != '\0') {
                int areaAttackMode;
                strParseStrFromList(&string, &areaAttackMode, gAreaAttackModeKeys, AREA_ATTACK_MODE_COUNT);
                partyMemberDescription->areaAttackMode[areaAttackMode] = true;
            }
        }

        if (configGetString(&config, section, "attack_who", &string)) {
            while (*string != '\0') {
                int attachWho;
                strParseStrFromList(&string, &attachWho, gAttackWhoKeys, ATTACK_WHO_COUNT);
                partyMemberDescription->attackWho[attachWho] = true;
            }
        }

        if (configGetString(&config, section, "best_weapon", &string)) {
            while (*string != '\0') {
                int bestWeapon;
                strParseStrFromList(&string, &bestWeapon, gBestWeaponKeys, BEST_WEAPON_COUNT);
                partyMemberDescription->bestWeapon[bestWeapon] = true;
            }
        }

        if (configGetString(&config, section, "chem_use", &string)) {
            while (*string != '\0') {
                int chemUse;
                strParseStrFromList(&string, &chemUse, gChemUseKeys, CHEM_USE_COUNT);
                partyMemberDescription->chemUse[chemUse] = true;
            }
        }

        if (configGetString(&config, section, "distance", &string)) {
            while (*string != '\0') {
                int distanceMode;
                strParseStrFromList(&string, &distanceMode, gDistanceModeKeys, DISTANCE_COUNT);
                partyMemberDescription->distanceMode[distanceMode] = true;
            }
        }

        if (configGetString(&config, section, "run_away_mode", &string)) {
            while (*string != '\0') {
                int runAwayMode;
                strParseStrFromList(&string, &runAwayMode, gRunAwayModeKeys, RUN_AWAY_MODE_COUNT);
                partyMemberDescription->runAwayMode[runAwayMode] = true;
            }
        }

        if (configGetString(&config, section, "disposition", &string)) {
            while (*string != '\0') {
                int disposition;
                strParseStrFromList(&string, &disposition, gDispositionKeys, DISPOSITION_COUNT);
                partyMemberDescription->disposition[disposition] = true;
            }
        }

        int levelUpEvery;
        if (configGetInt(&config, section, "level_up_every", &levelUpEvery)) {
            partyMemberDescription->level_up_every = levelUpEvery;

            int levelMinimum;
            if (configGetInt(&config, section, "level_minimum", &levelMinimum)) {
                partyMemberDescription->level_minimum = levelMinimum;
            }

            if (configGetString(&config, section, "level_pids", &string)) {
                while (*string != '\0' && partyMemberDescription->level_pids_num < PARTY_MEMBER_MAX_LEVEL) {
                    int levelPid;
                    strParseInt(&string, &levelPid);
                    partyMemberDescription->level_pids[partyMemberDescription->level_pids_num] = levelPid;
                    partyMemberDescription->level_pids_num++;
                }
            }
        }
    }

    configFree(&config);

    return 0;

err:

    configFree(&config);

    return -1;
}

// 0x4940E4
void partyMembersReset()
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        _partyMemberLevelUpInfoList[index].field_0 = 0;
        _partyMemberLevelUpInfoList[index].field_4 = 0;
        _partyMemberLevelUpInfoList[index].field_8 = 0;
    }
}

// 0x494134
void partyMembersExit()
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        _partyMemberLevelUpInfoList[index].field_0 = 0;
        _partyMemberLevelUpInfoList[index].field_4 = 0;
        _partyMemberLevelUpInfoList[index].field_8 = 0;
    }

    gPartyMemberDescriptionsLength = 0;

    if (gPartyMemberPids != nullptr) {
        internal_free(gPartyMemberPids);
        gPartyMemberPids = nullptr;
    }

    if (gPartyMembers != nullptr) {
        internal_free(gPartyMembers);
        gPartyMembers = nullptr;
    }

    if (gPartyMemberDescriptions != nullptr) {
        internal_free(gPartyMemberDescriptions);
        gPartyMemberDescriptions = nullptr;
    }

    if (_partyMemberLevelUpInfoList != nullptr) {
        internal_free(_partyMemberLevelUpInfoList);
        _partyMemberLevelUpInfoList = nullptr;
    }
}

// 0x4941F0
static int partyMemberGetDescription(Object* object, PartyMemberDescription** partyMemberDescriptionPtr)
{
    for (int index = 1; index < gPartyMemberDescriptionsLength; index++) {
        if (gPartyMemberPids[index] == object->pid) {
            *partyMemberDescriptionPtr = &(gPartyMemberDescriptions[index]);
            return 0;
        }
    }

    return -1;
}

// 0x49425C
static void partyMemberDescriptionInit(PartyMemberDescription* partyMemberDescription)
{
    for (int index = 0; index < AREA_ATTACK_MODE_COUNT; index++) {
        partyMemberDescription->areaAttackMode[index] = 0;
    }

    for (int index = 0; index < RUN_AWAY_MODE_COUNT; index++) {
        partyMemberDescription->runAwayMode[index] = 0;
    }

    for (int index = 0; index < BEST_WEAPON_COUNT; index++) {
        partyMemberDescription->bestWeapon[index] = 0;
    }

    for (int index = 0; index < DISTANCE_COUNT; index++) {
        partyMemberDescription->distanceMode[index] = 0;
    }

    for (int index = 0; index < ATTACK_WHO_COUNT; index++) {
        partyMemberDescription->attackWho[index] = 0;
    }

    for (int index = 0; index < CHEM_USE_COUNT; index++) {
        partyMemberDescription->chemUse[index] = 0;
    }

    for (int index = 0; index < DISPOSITION_COUNT; index++) {
        partyMemberDescription->disposition[index] = 0;
    }

    partyMemberDescription->level_minimum = 0;
    partyMemberDescription->level_up_every = 0;
    partyMemberDescription->level_pids_num = 0;

    partyMemberDescription->level_pids[0] = -1;

    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        _partyMemberLevelUpInfoList[index].field_0 = 0;
        _partyMemberLevelUpInfoList[index].field_4 = 0;
        _partyMemberLevelUpInfoList[index].field_8 = 0;
    }
}

// partyMemberAdd
// 0x494378
int partyMemberAdd(Object* object)
{
    if (gPartyMembersLength >= gPartyMemberDescriptionsLength + 20) {
        return -1;
    }

    for (int index = 0; index < gPartyMembersLength; index++) {
        PartyMemberListItem* partyMember = &(gPartyMembers[index]);
        if (partyMember->object == object || partyMember->object->pid == object->pid) {
            return 0;
        }
    }

    if (_partyStatePrepped) {
        debugPrint("\npartyMemberAdd DENIED: %s\n", critterGetName(object));
        return -1;
    }

    PartyMemberListItem* partyMember = &(gPartyMembers[gPartyMembersLength]);
    partyMember->object = object;
    partyMember->script = nullptr;
    partyMember->vars = nullptr;

    object->id = (object->pid & 0xFFFFFF) + 18000;
    object->flags |= (OBJECT_NO_REMOVE | OBJECT_NO_SAVE);

    gPartyMembersLength++;

    Script* script;
    if (scriptGetScript(object->sid, &script) != -1) {
        script->flags |= (SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);
        script->field_1C = object->id;

        object->sid = ((object->pid & 0xFFFFFF) + 18000) | (object->sid & 0xFF000000);
        script->sid = object->sid;
    }

    critterSetTeam(object, 0);
    queueRemoveEventsByType(object, EVENT_TYPE_SCRIPT);

    if (_gdialogActive()) {
        if (object == gGameDialogSpeaker) {
            _gdialogUpdatePartyStatus();
        }
    }

    return 0;
}

// partyMemberRemove
// 0x4944DC
int partyMemberRemove(Object* object)
{
    if (gPartyMembersLength == 0) {
        return -1;
    }

    if (object == nullptr) {
        return -1;
    }

    int index;
    for (index = 1; index < gPartyMembersLength; index++) {
        PartyMemberListItem* partyMember = &(gPartyMembers[index]);
        if (partyMember->object == object) {
            break;
        }
    }

    if (index == gPartyMembersLength) {
        return -1;
    }

    if (_partyStatePrepped) {
        debugPrint("\npartyMemberRemove DENIED: %s\n", critterGetName(object));
        return -1;
    }

    if (index < gPartyMembersLength - 1) {
        gPartyMembers[index].object = gPartyMembers[gPartyMembersLength - 1].object;
    }

    object->flags &= ~(OBJECT_NO_REMOVE | OBJECT_NO_SAVE);

    gPartyMembersLength--;

    Script* script;
    if (scriptGetScript(object->sid, &script) != -1) {
        script->flags &= ~(SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);
    }

    queueRemoveEventsByType(object, EVENT_TYPE_SCRIPT);

    if (_gdialogActive()) {
        if (object == gGameDialogSpeaker) {
            _gdialogUpdatePartyStatus();
        }
    }

    return 0;
}

// 0x49460C
int _partyMemberPrepSave()
{
    _partyStatePrepped = 1;

    for (int index = 0; index < gPartyMembersLength; index++) {
        PartyMemberListItem* ptr = &(gPartyMembers[index]);

        if (index > 0) {
            ptr->object->flags &= ~(OBJECT_NO_REMOVE | OBJECT_NO_SAVE);
        }

        Script* script;
        if (scriptGetScript(ptr->object->sid, &script) != -1) {
            script->flags &= ~(SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);
        }
    }

    return 0;
}

// 0x49466C
int _partyMemberUnPrepSave()
{
    for (int index = 0; index < gPartyMembersLength; index++) {
        PartyMemberListItem* ptr = &(gPartyMembers[index]);

        if (index > 0) {
            ptr->object->flags |= (OBJECT_NO_REMOVE | OBJECT_NO_SAVE);
        }

        Script* script;
        if (scriptGetScript(ptr->object->sid, &script) != -1) {
            script->flags |= (SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);
        }
    }

    _partyStatePrepped = 0;

    return 0;
}

// 0x4946CC
int partyMembersSave(File* stream)
{
    if (fileWriteInt32(stream, gPartyMembersLength) == -1) return -1;
    if (fileWriteInt32(stream, _partyMemberItemCount) == -1) return -1;

    for (int index = 1; index < gPartyMembersLength; index++) {
        PartyMemberListItem* partyMember = &(gPartyMembers[index]);
        if (fileWriteInt32(stream, partyMember->object->id) == -1) return -1;
    }

    for (int index = 1; index < gPartyMemberDescriptionsLength; index++) {
        STRU_519DBC* ptr = &(_partyMemberLevelUpInfoList[index]);
        if (fileWriteInt32(stream, ptr->field_0) == -1) return -1;
        if (fileWriteInt32(stream, ptr->field_4) == -1) return -1;
        if (fileWriteInt32(stream, ptr->field_8) == -1) return -1;
    }

    return 0;
}

// 0x4947AC
int _partyMemberPrepLoad()
{
    if (_partyStatePrepped) {
        return -1;
    }

    _partyStatePrepped = 1;

    for (int index = 0; index < gPartyMembersLength; index++) {
        PartyMemberListItem* ptr_519DA8 = &(gPartyMembers[index]);
        if (_partyMemberPrepLoadInstance(ptr_519DA8) != 0) {
            return -1;
        }
    }

    return 0;
}

// partyMemberPrepLoadInstance
// 0x49480C
static int _partyMemberPrepLoadInstance(PartyMemberListItem* a1)
{
    Object* obj = a1->object;

    if (obj == nullptr) {
        debugPrint("\n  Error!: partyMemberPrepLoadInstance: No Critter Object!");
        a1->script = nullptr;
        a1->vars = nullptr;
        a1->next = nullptr;
        return 0;
    }

    if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
        obj->data.critter.combat.whoHitMe = nullptr;
    }

    Script* script;
    if (scriptGetScript(obj->sid, &script) == -1) {
        debugPrint("\n  Error!: partyMemberPrepLoadInstance: Can't find script!");
        debugPrint("\n          partyMemberPrepLoadInstance: script was: (%s)", critterGetName(obj));
        a1->script = nullptr;
        a1->vars = nullptr;
        a1->next = nullptr;
        return 0;
    }

    a1->script = (Script*)internal_malloc(sizeof(*script));
    if (a1->script == nullptr) {
        showMesageBox("\n  Error!: partyMemberPrepLoad: Out of memory!");
        exit(1);
    }

    memcpy(a1->script, script, sizeof(*script));

    if (script->localVarsCount != 0 && script->localVarsOffset != -1) {
        a1->vars = (int*)internal_malloc(sizeof(*a1->vars) * script->localVarsCount);
        if (a1->vars == nullptr) {
            showMesageBox("\n  Error!: partyMemberPrepLoad: Out of memory!");
            exit(1);
        }

        if (gMapLocalVars != nullptr) {
            memcpy(a1->vars, gMapLocalVars + script->localVarsOffset, sizeof(int) * script->localVarsCount);
        } else {
            debugPrint("\nWarning: partyMemberPrepLoadInstance: No map_local_vars found, but script references them!");
            memset(a1->vars, 0, sizeof(int) * script->localVarsCount);
        }
    }

    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        _partyMemberItemSave(inventoryItem->item);
    }

    script->flags &= ~(SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);

    scriptRemove(script->sid);

    if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
        _dude_stand(obj, obj->rotation, -1);
    }

    return 0;
}

// partyMemberRecoverLoad
// 0x4949C4
int _partyMemberRecoverLoad()
{
    if (_partyStatePrepped != 1) {
        debugPrint("\npartyMemberRecoverLoad DENIED");
        return -1;
    }

    debugPrint("\n");

    for (int index = 0; index < gPartyMembersLength; index++) {
        if (_partyMemberRecoverLoadInstance(&(gPartyMembers[index])) != 0) {
            return -1;
        }

        debugPrint("[Party Member %d]: %s\n", index, critterGetName(gPartyMembers[index].object));
    }

    PartyMemberListItem* v6 = _itemSaveListHead;
    while (v6 != nullptr) {
        _itemSaveListHead = v6->next;

        _partyMemberItemRecover(v6);
        internal_free(v6);

        v6 = _itemSaveListHead;
    }

    _partyStatePrepped = 0;

    if (!_isLoadingGame()) {
        partyFixMultipleMembers();
    }

    return 0;
}

// partyMemberRecoverLoadInstance
// 0x494A88
static int _partyMemberRecoverLoadInstance(PartyMemberListItem* a1)
{
    if (a1->script == nullptr) {
        showMesageBox("\n  Error!: partyMemberRecoverLoadInstance: No script!");
        return 0;
    }

    int scriptType = SCRIPT_TYPE_CRITTER;
    if (PID_TYPE(a1->object->pid) != OBJ_TYPE_CRITTER) {
        scriptType = SCRIPT_TYPE_ITEM;
    }

    int v1 = -1;
    if (scriptAdd(&v1, scriptType) == -1) {
        showMesageBox("\n  Error!: partyMemberRecoverLoad: Can't create script!");
        exit(1);
    }

    Script* script;
    if (scriptGetScript(v1, &script) == -1) {
        showMesageBox("\n  Error!: partyMemberRecoverLoad: Can't find script!");
        exit(1);
    }

    memcpy(script, a1->script, sizeof(*script));

    int sid = (scriptType << 24) | ((a1->object->pid & 0xFFFFFF) + 18000);
    a1->object->sid = sid;
    script->sid = sid;

    script->flags &= ~(SCRIPT_FLAG_0x01 | SCRIPT_FLAG_0x04);

    internal_free(a1->script);
    a1->script = nullptr;

    script->flags |= (SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);

    if (a1->vars != nullptr) {
        script->localVarsOffset = _map_malloc_local_var(script->localVarsCount);
        memcpy(gMapLocalVars + script->localVarsOffset, a1->vars, sizeof(int) * script->localVarsCount);
    }

    return 0;
}

// 0x494BBC
int partyMembersLoad(File* stream)
{
    int* partyMemberObjectIds = (int*)internal_malloc(sizeof(*partyMemberObjectIds) * (gPartyMemberDescriptionsLength + 20));
    if (partyMemberObjectIds == nullptr) {
        return -1;
    }

    // FIXME: partyMemberObjectIds is never free'd in this function, obviously memory leak.

    if (fileReadInt32(stream, &gPartyMembersLength) == -1) return -1;
    if (fileReadInt32(stream, &_partyMemberItemCount) == -1) return -1;

    gPartyMembers->object = gDude;

    if (gPartyMembersLength != 0) {
        for (int index = 1; index < gPartyMembersLength; index++) {
            if (fileReadInt32(stream, &(partyMemberObjectIds[index])) == -1) return -1;
        }

        for (int index = 1; index < gPartyMembersLength; index++) {
            int objectId = partyMemberObjectIds[index];

            Object* object = objectFindFirst();
            while (object != nullptr) {
                if (object->id == objectId) {
                    break;
                }
                object = objectFindNext();
            }

            if (object != nullptr) {
                gPartyMembers[index].object = object;
            } else {
                debugPrint("Couldn't find party member on map...trying to load anyway.\n");
                if (index + 1 >= gPartyMembersLength) {
                    partyMemberObjectIds[index] = 0;
                } else {
                    memcpy(&(partyMemberObjectIds[index]), &(partyMemberObjectIds[index + 1]), sizeof(*partyMemberObjectIds) * (gPartyMembersLength - (index + 1)));
                }

                index--;
                gPartyMembersLength--;
            }
        }

        if (_partyMemberUnPrepSave() == -1) {
            return -1;
        }
    }

    partyFixMultipleMembers();

    for (int index = 1; index < gPartyMemberDescriptionsLength; index++) {
        STRU_519DBC* ptr_519DBC = &(_partyMemberLevelUpInfoList[index]);

        if (fileReadInt32(stream, &(ptr_519DBC->field_0)) == -1) return -1;
        if (fileReadInt32(stream, &(ptr_519DBC->field_4)) == -1) return -1;
        if (fileReadInt32(stream, &(ptr_519DBC->field_8)) == -1) return -1;
    }

    return 0;
}

// 0x494D7C
void _partyMemberClear()
{
    if (_partyStatePrepped) {
        _partyMemberUnPrepSave();
    }

    for (int index = gPartyMembersLength; index > 1; index--) {
        partyMemberRemove(gPartyMembers[1].object);
    }

    gPartyMembersLength = 1;

    _scr_remove_all();
    _partyMemberClearItemList();

    _partyStatePrepped = 0;
}

// 0x494DD0
int _partyMemberSyncPosition()
{
    int clockwiseRotation = (gDude->rotation + 2) % ROTATION_COUNT;
    int counterClockwiseRotation = (gDude->rotation + 4) % ROTATION_COUNT;

    int n = 0;
    int distance = 2;
    for (int index = 1; index < gPartyMembersLength; index++) {
        PartyMemberListItem* partyMember = &(gPartyMembers[index]);
        Object* partyMemberObj = partyMember->object;
        if ((partyMemberObj->flags & OBJECT_HIDDEN) == 0 && PID_TYPE(partyMemberObj->pid) == OBJ_TYPE_CRITTER) {
            int rotation;
            if ((n % 2) != 0) {
                rotation = clockwiseRotation;
            } else {
                rotation = counterClockwiseRotation;
            }

            int tile = tileGetTileInDirection(gDude->tile, rotation, distance / 2);
            _objPMAttemptPlacement(partyMemberObj, tile, gDude->elevation);

            distance++;
            n++;
        }
    }

    return 0;
}

// Heals party members according to their healing rate.
//
// 0x494EB8
int _partyMemberRestingHeal(int a1)
{
    int v1 = a1 / 3;
    if (v1 == 0) {
        return 0;
    }

    for (int index = 0; index < gPartyMembersLength; index++) {
        PartyMemberListItem* partyMember = &(gPartyMembers[index]);
        if (PID_TYPE(partyMember->object->pid) == OBJ_TYPE_CRITTER) {
            int healingRate = critterGetStat(partyMember->object, STAT_HEALING_RATE);
            critterAdjustHitPoints(partyMember->object, v1 * healingRate);
        }
    }

    return 1;
}

// 0x494F24
Object* partyMemberFindByPid(int pid)
{
    for (int index = 0; index < gPartyMembersLength; index++) {
        Object* object = gPartyMembers[index].object;
        if (object->pid == pid) {
            return object;
        }
    }

    return nullptr;
}

// 0x494F64
bool _isPotentialPartyMember(Object* object)
{
    for (int index = 0; index < gPartyMembersLength; index++) {
        PartyMemberListItem* partyMember = &(gPartyMembers[index]);
        if (partyMember->object->pid == gPartyMemberPids[index]) {
            return true;
        }
    }

    return false;
}

// Returns `true` if specified object is a party member.
//
// 0x494FC4
bool objectIsPartyMember(Object* object)
{
    if (object == nullptr) {
        return false;
    }

    if (object->id < 18000) {
        return false;
    }

    bool isPartyMember = false;

    for (int index = 0; index < gPartyMembersLength; index++) {
        if (gPartyMembers[index].object == object) {
            isPartyMember = true;
            break;
        }
    }

    return isPartyMember;
}

// Returns number of active critters in the party.
//
// 0x495010
int _getPartyMemberCount()
{
    int count = gPartyMembersLength;

    for (int index = 1; index < gPartyMembersLength; index++) {
        Object* object = gPartyMembers[index].object;

        if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER || critterIsDead(object) || (object->flags & OBJECT_HIDDEN) != 0) {
            count--;
        }
    }

    return count;
}

// 0x495070
static int _partyMemberNewObjID()
{
    Object* object;

    do {
        _curID++;

        object = objectFindFirst();
        while (object != nullptr) {
            if (object->id == _curID) {
                break;
            }

            Inventory* inventory = &(object->data.inventory);

            int index;
            for (index = 0; index < inventory->length; index++) {
                InventoryItem* inventoryItem = &(inventory->items[index]);
                Object* item = inventoryItem->item;
                if (item->id == _curID) {
                    break;
                }

                if (_partyMemberNewObjIDRecurseFind(item, _curID)) {
                    break;
                }
            }

            if (index < inventory->length) {
                break;
            }

            object = objectFindNext();
        }
    } while (object != nullptr);

    _curID++;

    return _curID;
}

// 0x4950F4
static int _partyMemberNewObjIDRecurseFind(Object* obj, int objectId)
{
    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (inventoryItem->item->id == objectId) {
            return 1;
        }

        if (_partyMemberNewObjIDRecurseFind(inventoryItem->item, objectId)) {
            return 1;
        }
    }

    return 0;
}

// 0x495140
int _partyMemberPrepItemSaveAll()
{
    for (int partyMemberIndex = 0; partyMemberIndex < gPartyMembersLength; partyMemberIndex++) {
        PartyMemberListItem* partyMember = &(gPartyMembers[partyMemberIndex]);

        Inventory* inventory = &(partyMember->object->data.inventory);
        for (int inventoryItemIndex = 0; inventoryItemIndex < inventory->length; inventoryItemIndex++) {
            InventoryItem* inventoryItem = &(inventory->items[inventoryItemIndex]);
            _partyMemberPrepItemSave(inventoryItem->item);
        }
    }

    return 0;
}

// partyMemberPrepItemSaveAll
static int _partyMemberPrepItemSave(Object* object)
{
    if (object->sid != -1) {
        Script* script;
        if (scriptGetScript(object->sid, &script) == -1) {
            showMesageBox("\n  Error!: partyMemberPrepItemSaveAll: Can't find script!");
            exit(1);
        }

        script->flags |= (SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);
    }

    Inventory* inventory = &(object->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        _partyMemberPrepItemSave(inventoryItem->item);
    }

    return 0;
}

// 0x495234
static int _partyMemberItemSave(Object* object)
{
    if (object->sid != -1) {
        Script* script;
        if (scriptGetScript(object->sid, &script) == -1) {
            showMesageBox("\n  Error!: partyMemberItemSave: Can't find script!");
            exit(1);
        }

        if (object->id < 20000) {
            script->field_1C = _partyMemberNewObjID();
            object->id = script->field_1C;
        }

        PartyMemberListItem* node = (PartyMemberListItem*)internal_malloc(sizeof(*node));
        if (node == nullptr) {
            showMesageBox("\n  Error!: partyMemberItemSave: Out of memory!");
            exit(1);
        }

        node->object = object;

        node->script = (Script*)internal_malloc(sizeof(*script));
        if (node->script == nullptr) {
            showMesageBox("\n  Error!: partyMemberItemSave: Out of memory!");
            exit(1);
        }

        memcpy(node->script, script, sizeof(*script));

        if (script->localVarsCount != 0 && script->localVarsOffset != -1) {
            node->vars = (int*)internal_malloc(sizeof(*node->vars) * script->localVarsCount);
            if (node->vars == nullptr) {
                showMesageBox("\n  Error!: partyMemberItemSave: Out of memory!");
                exit(1);
            }

            memcpy(node->vars, gMapLocalVars + script->localVarsOffset, sizeof(int) * script->localVarsCount);
        } else {
            node->vars = nullptr;
        }

        PartyMemberListItem* temp = _itemSaveListHead;
        _itemSaveListHead = node;
        node->next = temp;
    }

    Inventory* inventory = &(object->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        _partyMemberItemSave(inventoryItem->item);
    }

    return 0;
}

// partyMemberItemRecover
// 0x495388
static int _partyMemberItemRecover(PartyMemberListItem* a1)
{
    int sid = -1;
    if (scriptAdd(&sid, SCRIPT_TYPE_ITEM) == -1) {
        showMesageBox("\n  Error!: partyMemberItemRecover: Can't create script!");
        exit(1);
    }

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        showMesageBox("\n  Error!: partyMemberItemRecover: Can't find script!");
        exit(1);
    }

    memcpy(script, a1->script, sizeof(*script));

    a1->object->sid = _partyMemberItemCount | (SCRIPT_TYPE_ITEM << 24);
    script->sid = _partyMemberItemCount | (SCRIPT_TYPE_ITEM << 24);

    script->program = nullptr;
    script->flags &= ~(SCRIPT_FLAG_0x01 | SCRIPT_FLAG_0x04 | SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);

    _partyMemberItemCount++;

    internal_free(a1->script);
    a1->script = nullptr;

    if (a1->vars != nullptr) {
        script->localVarsOffset = _map_malloc_local_var(script->localVarsCount);
        memcpy(gMapLocalVars + script->localVarsOffset, a1->vars, sizeof(int) * script->localVarsCount);
    }

    return 0;
}

// 0x4954C4
static int _partyMemberClearItemList()
{
    while (_itemSaveListHead != nullptr) {
        PartyMemberListItem* node = _itemSaveListHead;
        _itemSaveListHead = _itemSaveListHead->next;

        if (node->script != nullptr) {
            internal_free(node->script);
        }

        if (node->vars != nullptr) {
            internal_free(node->vars);
        }

        internal_free(node);
    }

    _partyMemberItemCount = 20000;

    return 0;
}

// Returns best skill of the specified party member.
//
// 0x495520
int partyMemberGetBestSkill(Object* object)
{
    int bestSkill = SKILL_SMALL_GUNS;

    if (object == nullptr) {
        return bestSkill;
    }

    if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
        return bestSkill;
    }

    int bestValue = 0;
    for (int skill = 0; skill < SKILL_COUNT; skill++) {
        int value = skillGetValue(object, skill);
        if (value > bestValue) {
            bestSkill = skill;
            bestValue = value;
        }
    }

    return bestSkill;
}

// Returns party member with highest skill level.
//
// 0x495560
Object* partyMemberGetBestInSkill(int skill)
{
    int bestValue = 0;
    Object* bestPartyMember = nullptr;

    for (int index = 0; index < gPartyMembersLength; index++) {
        Object* object = gPartyMembers[index].object;
        if ((object->flags & OBJECT_HIDDEN) == 0 && PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            int value = skillGetValue(object, skill);
            if (value > bestValue) {
                bestValue = value;
                bestPartyMember = object;
            }
        }
    }

    return bestPartyMember;
}

// Returns highest skill level in party.
//
// 0x4955C8
int partyGetBestSkillValue(int skill)
{
    int bestValue = 0;

    for (int index = 0; index < gPartyMembersLength; index++) {
        Object* object = gPartyMembers[index].object;
        if ((object->flags & OBJECT_HIDDEN) == 0 && PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            int value = skillGetValue(object, skill);
            if (value > bestValue) {
                bestValue = value;
            }
        }
    }

    return bestValue;
}

// 0x495620
static int partyFixMultipleMembers()
{
    debugPrint("\n\n\n[Party Members]:");

    // NOTE: Original code is slightly different (uses two nested loops).
    int critterCount = 0;
    Object* obj = objectFindFirst();
    while (obj != nullptr) {
        bool isPartyMember = false;
        for (int index = 1; index < gPartyMemberDescriptionsLength; index++) {
            if (obj->pid == gPartyMemberPids[index]) {
                isPartyMember = true;
                break;
            }
        }

        if (isPartyMember) {
            debugPrint("\n   PM: %s", critterGetName(obj));

            bool remove = false;
            if (obj->sid == -1) {
                remove = true;
            } else {
                // NOTE: Uninline.
                Object* partyMember = partyMemberFindByPid(obj->pid);
                if (partyMember != nullptr && partyMember != obj) {
                    if (partyMember->sid == obj->sid) {
                        obj->sid = -1;
                    }
                    remove = true;
                }
            }

            if (remove) {
                // NOTE: Uninline.
                if (obj != partyMemberFindByPid(obj->pid)) {
                    debugPrint("\nDestroying evil critter doppleganger!");

                    if (obj->sid != -1) {
                        scriptRemove(obj->sid);
                        obj->sid = -1;
                    } else {
                        if (queueRemoveEventsByType(obj, EVENT_TYPE_SCRIPT) == -1) {
                            debugPrint("\nERROR Removing Timed Events on FIX remove!!\n");
                        }
                    }

                    _combat_delete_critter(obj);

                    objectDestroy(obj, nullptr);

                    // Start over.
                    critterCount = 0;
                    obj = objectFindFirst();
                    continue;
                } else {
                    debugPrint("\nError: Attempting to destroy evil critter doppleganger FAILED!");
                }
            }
        }

        obj = objectFindNext();
    }

    for (int index = 0; index < gPartyMembersLength; index++) {
        PartyMemberListItem* partyMember = &(gPartyMembers[index]);

        Script* script;
        if (scriptGetScript(partyMember->object->sid, &script) != -1) {
            script->owner = partyMember->object;
        } else {
            debugPrint("\nError: Failed to fix party member critter scripts!");
        }
    }

    debugPrint("\nTotal Critter Count: %d\n\n", critterCount);

    return 0;
}

// 0x495870
void _partyMemberSaveProtos()
{
    for (int index = 1; index < gPartyMemberDescriptionsLength; index++) {
        int pid = gPartyMemberPids[index];
        if (pid != -1) {
            _proto_save_pid(pid);
        }
    }
}

// 0x4958B0
bool partyMemberSupportsDisposition(Object* critter, int disposition)
{
    if (critter == nullptr) {
        return false;
    }

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (disposition == -1 || disposition > 5) {
        return false;
    }

    PartyMemberDescription* partyMemberDescription;
    if (partyMemberGetDescription(critter, &partyMemberDescription) == -1) {
        return false;
    }

    return partyMemberDescription->disposition[disposition + 1];
}

// 0x495920
bool partyMemberSupportsAreaAttackMode(Object* object, int areaAttackMode)
{
    if (object == nullptr) {
        return false;
    }

    if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (areaAttackMode >= AREA_ATTACK_MODE_COUNT) {
        return false;
    }

    PartyMemberDescription* partyMemberDescription;
    if (partyMemberGetDescription(object, &partyMemberDescription) == -1) {
        return false;
    }

    return partyMemberDescription->areaAttackMode[areaAttackMode];
}

// 0x495980
bool partyMemberSupportsRunAwayMode(Object* object, int runAwayMode)
{
    if (object == nullptr) {
        return false;
    }

    if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (runAwayMode >= RUN_AWAY_MODE_COUNT) {
        return false;
    }

    PartyMemberDescription* partyMemberDescription;
    if (partyMemberGetDescription(object, &partyMemberDescription) == -1) {
        return false;
    }

    return partyMemberDescription->runAwayMode[runAwayMode + 1];
}

// 0x4959E0
bool partyMemberSupportsBestWeapon(Object* object, int bestWeapon)
{
    if (object == nullptr) {
        return false;
    }

    if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (bestWeapon >= BEST_WEAPON_COUNT) {
        return false;
    }

    PartyMemberDescription* partyMemberDescription;
    if (partyMemberGetDescription(object, &partyMemberDescription) == -1) {
        return false;
    }

    return partyMemberDescription->bestWeapon[bestWeapon];
}

// 0x495A40
bool partyMemberSupportsDistance(Object* object, int distanceMode)
{
    if (object == nullptr) {
        return false;
    }

    if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (distanceMode >= DISTANCE_COUNT) {
        return false;
    }

    PartyMemberDescription* partyMemberDescription;
    if (partyMemberGetDescription(object, &partyMemberDescription) == -1) {
        return false;
    }

    return partyMemberDescription->distanceMode[distanceMode];
}

// 0x495AA0
bool partyMemberSupportsAttackWho(Object* object, int attackWho)
{
    if (object == nullptr) {
        return false;
    }

    if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (attackWho >= ATTACK_WHO_COUNT) {
        return false;
    }

    PartyMemberDescription* partyMemberDescription;
    if (partyMemberGetDescription(object, &partyMemberDescription) == -1) {
        return false;
    }

    return partyMemberDescription->attackWho[attackWho];
}

// 0x495B00
bool partyMemberSupportsChemUse(Object* object, int chemUse)
{
    if (object == nullptr) {
        return false;
    }

    if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (chemUse >= CHEM_USE_COUNT) {
        return false;
    }

    PartyMemberDescription* partyMemberDescription;
    if (partyMemberGetDescription(object, &partyMemberDescription) == -1) {
        return false;
    }

    return partyMemberDescription->chemUse[chemUse];
}

// partyMemberIncLevels
// 0x495B60
int _partyMemberIncLevels()
{
    int i;
    PartyMemberListItem* ptr;
    Object* obj;
    PartyMemberDescription* party_member;
    const char* name;
    int j;
    int v0;
    STRU_519DBC* ptr_519DBC;
    int v24;
    char* text;
    MessageListItem msg;
    char str[260];
    Rect v19;

    v0 = -1;
    for (i = 1; i < gPartyMembersLength; i++) {
        ptr = &(gPartyMembers[i]);
        obj = ptr->object;

        if (partyMemberGetDescription(obj, &party_member) == -1) {
            // SFALL: NPC level fix.
            continue;
        }

        if (PID_TYPE(obj->pid) != OBJ_TYPE_CRITTER) {
            continue;
        }

        name = critterGetName(obj);
        debugPrint("\npartyMemberIncLevels: %s", name);

        if (party_member->level_up_every == 0) {
            continue;
        }

        for (j = 1; j < gPartyMemberDescriptionsLength; j++) {
            if (gPartyMemberPids[j] == obj->pid) {
                v0 = j;
            }
        }

        if (v0 == -1) {
            continue;
        }

        if (pcGetStat(PC_STAT_LEVEL) < party_member->level_minimum) {
            continue;
        }

        ptr_519DBC = &(_partyMemberLevelUpInfoList[v0]);

        if (ptr_519DBC->field_0 >= party_member->level_pids_num) {
            continue;
        }

        ptr_519DBC->field_4++;

        v24 = ptr_519DBC->field_4 % party_member->level_pids_num;
        debugPrint("pm: levelMod: %d, Lvl: %d, Early: %d, Every: %d", v24, ptr_519DBC->field_4, ptr_519DBC->field_8, party_member->level_up_every);

        if (v24 != 0 || ptr_519DBC->field_8 == 0) {
            if (ptr_519DBC->field_8 == 0) {
                if (v24 == 0 || randomBetween(0, 100) <= 100 * v24 / party_member->level_up_every) {
                    ptr_519DBC->field_0++;
                    if (v24 != 0) {
                        ptr_519DBC->field_8 = 1;
                    }

                    if (_partyMemberCopyLevelInfo(obj, party_member->level_pids[ptr_519DBC->field_0]) == -1) {
                        return -1;
                    }

                    name = critterGetName(obj);
                    // %s has gained in some abilities.
                    text = getmsg(&gMiscMessageList, &msg, 9000);
                    snprintf(str, sizeof(str), text, name);
                    displayMonitorAddMessage(str);

                    debugPrint(str);

                    // Individual message
                    msg.num = 9000 + 10 * v0 + ptr_519DBC->field_0 - 1;
                    if (messageListGetItem(&gMiscMessageList, &msg)) {
                        name = critterGetName(obj);
                        snprintf(str, sizeof(str), msg.text, name);
                        textObjectAdd(obj, str, 101, _colorTable[0x7FFF], _colorTable[0], &v19);
                        tileWindowRefreshRect(&v19, obj->elevation);
                    }
                }
            }
        } else {
            ptr_519DBC->field_8 = 0;
        }
    }

    return 0;
}

// 0x495EA8
static int _partyMemberCopyLevelInfo(Object* critter, int a2)
{
    if (critter == nullptr) {
        return -1;
    }

    if (a2 == -1) {
        return -1;
    }

    Proto* proto1;
    if (protoGetProto(critter->pid, &proto1) == -1) {
        return -1;
    }

    Proto* proto2;
    if (protoGetProto(a2, &proto2) == -1) {
        return -1;
    }

    Object* item2 = critterGetItem2(critter);
    _invenUnwieldFunc(critter, 1, 0);

    Object* armor = critterGetArmor(critter);
    _adjust_ac(critter, armor, nullptr);
    itemRemove(critter, armor, 1);

    int maxHp = critterGetStat(critter, STAT_MAXIMUM_HIT_POINTS);
    critterAdjustHitPoints(critter, maxHp);

    for (int stat = 0; stat < SPECIAL_STAT_COUNT; stat++) {
        proto1->critter.data.baseStats[stat] = proto2->critter.data.baseStats[stat];
    }

    for (int stat = 0; stat < SPECIAL_STAT_COUNT; stat++) {
        proto1->critter.data.bonusStats[stat] = proto2->critter.data.bonusStats[stat];
    }

    for (int skill = 0; skill < SKILL_COUNT; skill++) {
        proto1->critter.data.skills[skill] = proto2->critter.data.skills[skill];
    }

    critter->data.critter.hp = critterGetStat(critter, STAT_MAXIMUM_HIT_POINTS);

    if (armor != nullptr) {
        itemAdd(critter, armor, 1);
        _inven_wield(critter, armor, 0);
    }

    if (item2 != nullptr) {
        // SFALL: Fix for party member's equipped weapon being placed in the
        // incorrect item slot after leveling up.
        _invenWieldFunc(critter, item2, 1, false);
    }

    return 0;
}

// Returns `true` if any party member that can be healed thru the rest is
// wounded.
//
// This function is used to determine if any party member needs healing thru
// the "Rest until party healed", therefore it excludes robots in the party
// (they cannot be healed by resting) and dude (he/she has it's own "Rest
// until healed" option).
//
// 0x496058
bool partyIsAnyoneCanBeHealedByRest()
{
    for (int index = 1; index < gPartyMembersLength; index++) {
        PartyMemberListItem* ptr = &(gPartyMembers[index]);
        Object* object = ptr->object;

        if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) continue;
        if (critterIsDead(object)) continue;
        if ((object->flags & OBJECT_HIDDEN) != 0) continue;
        if (critterGetKillType(object) == KILL_TYPE_ROBOT) continue;

        int currentHp = critterGetHitPoints(object);
        int maximumHp = critterGetStat(object, STAT_MAXIMUM_HIT_POINTS);
        if (currentHp < maximumHp) {
            return true;
        }
    }

    return false;
}

// Returns maximum amount of damage of any party member that can be healed thru
// the rest.
//
// 0x4960DC
int partyGetMaxWoundToHealByRest()
{
    int maxWound = 0;

    for (int index = 1; index < gPartyMembersLength; index++) {
        PartyMemberListItem* ptr = &(gPartyMembers[index]);
        Object* object = ptr->object;

        if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) continue;
        if (critterIsDead(object)) continue;
        if ((object->flags & OBJECT_HIDDEN) != 0) continue;
        if (critterGetKillType(object) == KILL_TYPE_ROBOT) continue;

        int currentHp = critterGetHitPoints(object);
        int maximumHp = critterGetStat(object, STAT_MAXIMUM_HIT_POINTS);
        int wound = maximumHp - currentHp;
        if (wound > 0) {
            if (wound > maxWound) {
                maxWound = wound;
            }
        }
    }

    return maxWound;
}

std::vector<Object*> get_all_party_members_objects(bool include_hidden)
{
    std::vector<Object*> value;
    value.reserve(gPartyMembersLength);
    for (int index = 0; index < gPartyMembersLength; index++) {
        auto object = gPartyMembers[index].object;
        if (include_hidden
            || (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER
                && !critterIsDead(object)
                && (object->flags & OBJECT_HIDDEN) == 0)) {
            value.push_back(object);
        }
    }
    return value;
}

} // namespace fallout
