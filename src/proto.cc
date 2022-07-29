#include "proto.h"

#include "art.h"
#include "character_editor.h"
#include "combat.h"
#include "config.h"
#include "critter.h"
#include "debug.h"
#include "dialog.h"
#include "game.h"
#include "game_config.h"
#include "game_movie.h"
#include "interface.h"
#include "map.h"
#include "memory.h"
#include "object.h"
#include "perk.h"
#include "skill.h"
#include "stat.h"
#include "trait.h"

#include <stdio.h>
#include <string.h>

static int _proto_critter_init(Proto* a1, int a2);
static int objectCritterCombatDataRead(CritterCombatData* data, File* stream);
static int objectCritterCombatDataWrite(CritterCombatData* data, File* stream);
static int _proto_update_gen(Object* obj);
static int _proto_header_load();
static int protoItemDataRead(ItemProtoData* item_data, int type, File* stream);
static int protoSceneryDataRead(SceneryProtoData* scenery_data, int type, File* stream);
static int protoRead(Proto* buf, File* stream);
static int protoItemDataWrite(ItemProtoData* item_data, int type, File* stream);
static int protoSceneryDataWrite(SceneryProtoData* scenery_data, int type, File* stream);
static int protoWrite(Proto* buf, File* stream);
static int _proto_load_pid(int pid, Proto** out_proto);
static int _proto_find_free_subnode(int type, Proto** out_ptr);
static void _proto_remove_some_list(int type);
static void _proto_remove_list(int type);
static int _proto_new_id(int a1);
static int _proto_max_id(int a1);

// 0x50CF3C
static char _aProto_0[] = "proto\\";

// 0x50D1B0
static char _aDrugStatSpecia[] = "Drug Stat (Special)";

// 0x50D1C4
static char _aNone_1[] = "None";

// 0x51C18C
char _cd_path_base[COMPAT_MAX_PATH];

// 0x51C290
static ProtoList _protoLists[11] = {
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

// 0x51C340
static const size_t _proto_sizes[11] = {
    sizeof(ItemProto), // 0x84
    sizeof(CritterProto), // 0x1A0
    sizeof(SceneryProto), // 0x38
    sizeof(WallProto), // 0x24
    sizeof(TileProto), // 0x1C
    sizeof(MiscProto), // 0x1C
    0,
    0,
    0,
    0,
    0,
};

// 0x51C36C
static int _protos_been_initialized = 0;

// obj_dude_proto
// 0x51C370
static CritterProto gDudeProto = {
    0x1000000,
    -1,
    0x1000001,
    0,
    0,
    0x20000000,
    0,
    -1,
    0,
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 18, 0, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0, 23, 0 },
    { 0 },
    { 0 },
    0,
    0,
    0,
    0,
    -1,
    0,
    0,
};

// 0x51C534
static char* _proto_path_base = _aProto_0;

// 0x51C538
static int _init_true = 0;

// 0x51C53C
static int _retval = 0;

// 0x66452C
static char* _mp_perk_code_None;

// 0x664530
static char* _mp_perk_code_strs[PERK_COUNT];

// 0x66470C
static char* _mp_critter_stats_list;

// 0x664710
static char* _critter_stats_list_None;

// 0x664714
static char* _critter_stats_list_strs[STAT_COUNT];

// Message list by object type
// 0 - pro_item.msg
// 1 - pro_crit.msg
// 2 - pro_scen.msg
// 3 - pro_wall.msg
// 4 - pro_tile.msg
// 5 - pro_misc.msg
//
// 0x6647AC
static MessageList _proto_msg_files[6];

// 0x6647DC
static char* gRaceTypeNames[RACE_TYPE_COUNT];

// 0x6647E4
static char* gSceneryTypeNames[SCENERY_TYPE_COUNT];

// proto.msg
//
// 0x6647FC
MessageList gProtoMessageList;

// 0x664804
static char* gMaterialTypeNames[MATERIAL_TYPE_COUNT];

// "<None>" from proto.msg
//
// 0x664824
char* _proto_none_str;

// 0x664828
static char* gBodyTypeNames[BODY_TYPE_COUNT];

// 0x664834
static char* gItemTypeNames[ITEM_TYPE_COUNT];

// 0x66484C
static char* gDamageTypeNames[DAMAGE_TYPE_COUNT];

// 0x66486C
static char* gCaliberTypeNames[CALIBER_TYPE_COUNT];

// Perk names.
//
// 0x6648B8
static char** _perk_code_strs;

// Stat names.
//
// 0x6648BC
static char** _critter_stats_list;

// NOTE: Inlined.
void _proto_make_path(char* path, int pid)
{
    strcpy(path, _cd_path_base);
    strcat(path, _proto_path_base);
    if (pid != -1) {
        strcat(path, artGetObjectTypeName(PID_TYPE(pid)));
    }
}

// Append proto file name to proto_path from proto.lst.
//
// 0x49E758
int _proto_list_str(int pid, char* proto_path)
{
    if (pid == -1) {
        return -1;
    }

    if (proto_path == NULL) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    _proto_make_path(path, pid);
    strcat(path, "\\");
    strcat(path, artGetObjectTypeName(PID_TYPE(pid)));
    strcat(path, ".lst");

    File* stream = fileOpen(path, "rt");

    int i = 1;
    char string[256];
    while (fileReadString(string, sizeof(string), stream)) {
        if (i == (pid & 0xFFFFFF)) {
            break;
        }

        i++;
    }

    fileClose(stream);

    if (i != (pid & 0xFFFFFF)) {
        return -1;
    }

    char* pch = strchr(string, ' ');
    if (pch != NULL) {
        *pch = '\0';
    }

    pch = strchr(string, '\n');
    if (pch != NULL) {
        *pch = '\0';
    }

    strcpy(proto_path, string);

    return 0;
}

// 0x49E99C
bool _proto_action_can_use(int pid)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return false;
    }

    if ((proto->item.extendedFlags & 0x0800) != 0) {
        return true;
    }

    if (PID_TYPE(pid) == OBJ_TYPE_ITEM && proto->item.type == ITEM_TYPE_CONTAINER) {
        return true;
    }

    return false;
}

// 0x49E9DC
bool _proto_action_can_use_on(int pid)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return false;
    }

    if ((proto->item.extendedFlags & 0x1000) != 0) {
        return true;
    }

    if (PID_TYPE(pid) == OBJ_TYPE_ITEM && proto->item.type == ITEM_TYPE_DRUG) {
        return true;
    }

    return false;
}

// 0x49EA24
bool _proto_action_can_talk_to(int pid)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return false;
    }

    if (PID_TYPE(pid) == OBJ_TYPE_CRITTER) {
        return true;
    }

    if (proto->critter.extendedFlags & 0x4000) {
        return true;
    }

    return false;
}

// Likely returns true if item with given pid can be picked up.
//
// 0x49EA5C
int _proto_action_can_pickup(int pid)
{
    if (PID_TYPE(pid) != OBJ_TYPE_ITEM) {
        return false;
    }

    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return false;
    }

    if (proto->item.type == ITEM_TYPE_CONTAINER) {
        return (proto->item.extendedFlags & 0x8000) != 0;
    }

    return true;
}

// 0x49EAA4
char* protoGetMessage(int pid, int message)
{
    char* v1 = _proto_none_str;

    Proto* proto;
    if (protoGetProto(pid, &proto) != -1) {
        if (proto->messageId != -1) {
            MessageList* messageList = &(_proto_msg_files[PID_TYPE(pid)]);

            MessageListItem messageListItem;
            messageListItem.num = proto->messageId + message;
            if (messageListGetItem(messageList, &messageListItem)) {
                v1 = messageListItem.text;
            }
        }
    }

    return v1;
}

// 0x49EAFC
char* protoGetName(int pid)
{
    if (pid == 0x1000000) {
        return critterGetName(gDude);
    }

    return protoGetMessage(pid, PROTOTYPE_MESSAGE_NAME);
}

// 0x49EB1C
char* protoGetDescription(int pid)
{
    return protoGetMessage(pid, PROTOTYPE_MESSAGE_DESCRIPTION);
}

// 0x49EDB4
static int _proto_critter_init(Proto* a1, int a2)
{
    if (!_protos_been_initialized) {
        return -1;
    }

    int v1 = a2 & 0xFFFFFF;

    a1->pid = -1;
    a1->messageId = 100 * v1;
    a1->fid = buildFid(OBJ_TYPE_CRITTER, v1 - 1, 0, 0, 0);
    a1->critter.lightDistance = 0;
    a1->critter.lightIntensity = 0;
    a1->critter.flags = 0x20000000;
    a1->critter.extendedFlags = 0x6000;
    a1->critter.sid = -1;
    a1->critter.data.flags = 0;
    a1->critter.data.bodyType = 0;
    a1->critter.headFid = -1;
    a1->critter.aiPacket = 1;
    if (!artExists(a1->fid)) {
        a1->fid = buildFid(OBJ_TYPE_CRITTER, 0, 0, 0, 0);
    }

    CritterProtoData* data = &(a1->critter.data);
    data->experience = 60;
    data->killType = 0;
    data->damageType = 0;
    protoCritterDataResetStats(data);
    protoCritterDataResetSkills(data);

    return 0;
}

// 0x49EEA4
void objectDataReset(Object* obj)
{
    // NOTE: Original code is slightly different. It uses loop to zero object
    // data byte by byte.
    memset(&(obj->data), 0, sizeof(obj->data));
}

// 0x49EEB8
static int objectCritterCombatDataRead(CritterCombatData* data, File* stream)
{
    if (fileReadInt32(stream, &(data->damageLastTurn)) == -1) return -1;
    if (fileReadInt32(stream, &(data->maneuver)) == -1) return -1;
    if (fileReadInt32(stream, &(data->ap)) == -1) return -1;
    if (fileReadInt32(stream, &(data->results)) == -1) return -1;
    if (fileReadInt32(stream, &(data->aiPacket)) == -1) return -1;
    if (fileReadInt32(stream, &(data->team)) == -1) return -1;
    if (fileReadInt32(stream, &(data->whoHitMeCid)) == -1) return -1;

    return 0;
}

// 0x49EF40
static int objectCritterCombatDataWrite(CritterCombatData* data, File* stream)
{
    if (fileWriteInt32(stream, data->damageLastTurn) == -1) return -1;
    if (fileWriteInt32(stream, data->maneuver) == -1) return -1;
    if (fileWriteInt32(stream, data->ap) == -1) return -1;
    if (fileWriteInt32(stream, data->results) == -1) return -1;
    if (fileWriteInt32(stream, data->aiPacket) == -1) return -1;
    if (fileWriteInt32(stream, data->team) == -1) return -1;
    if (fileWriteInt32(stream, data->whoHitMeCid) == -1) return -1;

    return 0;
}

// 0x49F004
int objectDataRead(Object* obj, File* stream)
{
    Proto* proto;

    Inventory* inventory = &(obj->data.inventory);
    if (fileReadInt32(stream, &(inventory->length)) == -1) return -1;
    if (fileReadInt32(stream, &(inventory->capacity)) == -1) return -1;
    // TODO: See below.
    if (fileReadInt32(stream, (int*)&(inventory->items)) == -1) return -1;

    if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
        if (fileReadInt32(stream, &(obj->data.critter.field_0)) == -1) return -1;
        if (objectCritterCombatDataRead(&(obj->data.critter.combat), stream) == -1) return -1;
        if (fileReadInt32(stream, &(obj->data.critter.hp)) == -1) return -1;
        if (fileReadInt32(stream, &(obj->data.critter.radiation)) == -1) return -1;
        if (fileReadInt32(stream, &(obj->data.critter.poison)) == -1) return -1;
    } else {
        if (fileReadInt32(stream, &(obj->data.flags)) == -1) return -1;

        if (obj->data.flags == 0xCCCCCCCC) {
            debugPrint("\nNote: Reading pud: updated_flags was un-Set!");
            obj->data.flags = 0;
        }

        switch (PID_TYPE(obj->pid)) {
        case OBJ_TYPE_ITEM:
            if (protoGetProto(obj->pid, &proto) == -1) return -1;

            switch (proto->item.type) {
            case ITEM_TYPE_WEAPON:
                if (fileReadInt32(stream, &(obj->data.item.weapon.ammoQuantity)) == -1) return -1;
                if (fileReadInt32(stream, &(obj->data.item.weapon.ammoTypePid)) == -1) return -1;
                break;
            case ITEM_TYPE_AMMO:
                if (fileReadInt32(stream, &(obj->data.item.ammo.quantity)) == -1) return -1;
                break;
            case ITEM_TYPE_MISC:
                if (fileReadInt32(stream, &(obj->data.item.misc.charges)) == -1) return -1;
                break;
            case ITEM_TYPE_KEY:
                if (fileReadInt32(stream, &(obj->data.item.key.keyCode)) == -1) return -1;
                break;
            default:
                break;
            }

            break;
        case OBJ_TYPE_SCENERY:
            if (protoGetProto(obj->pid, &proto) == -1) return -1;

            switch (proto->scenery.type) {
            case SCENERY_TYPE_DOOR:
                if (fileReadInt32(stream, &(obj->data.scenery.door.openFlags)) == -1) return -1;
                break;
            case SCENERY_TYPE_STAIRS:
                if (fileReadInt32(stream, &(obj->data.scenery.stairs.destinationBuiltTile)) == -1) return -1;
                if (fileReadInt32(stream, &(obj->data.scenery.stairs.destinationMap)) == -1) return -1;
                break;
            case SCENERY_TYPE_ELEVATOR:
                if (fileReadInt32(stream, &(obj->data.scenery.elevator.type)) == -1) return -1;
                if (fileReadInt32(stream, &(obj->data.scenery.elevator.level)) == -1) return -1;
                break;
            case SCENERY_TYPE_LADDER_UP:
                if (gMapHeader.version == 19) {
                    if (fileReadInt32(stream, &(obj->data.scenery.ladder.destinationBuiltTile)) == -1) return -1;
                } else {
                    if (fileReadInt32(stream, &(obj->data.scenery.ladder.destinationMap)) == -1) return -1;
                    if (fileReadInt32(stream, &(obj->data.scenery.ladder.destinationBuiltTile)) == -1) return -1;
                }
                break;
            case SCENERY_TYPE_LADDER_DOWN:
                if (gMapHeader.version == 19) {
                    if (fileReadInt32(stream, &(obj->data.scenery.ladder.destinationBuiltTile)) == -1) return -1;
                } else {
                    if (fileReadInt32(stream, &(obj->data.scenery.ladder.destinationMap)) == -1) return -1;
                    if (fileReadInt32(stream, &(obj->data.scenery.ladder.destinationBuiltTile)) == -1) return -1;
                }
                break;
            }

            break;
        case OBJ_TYPE_MISC:
            if (obj->pid >= 0x5000010 && obj->pid <= 0x5000017) {
                if (fileReadInt32(stream, &(obj->data.misc.map)) == -1) return -1;
                if (fileReadInt32(stream, &(obj->data.misc.tile)) == -1) return -1;
                if (fileReadInt32(stream, &(obj->data.misc.elevation)) == -1) return -1;
                if (fileReadInt32(stream, &(obj->data.misc.rotation)) == -1) return -1;
            }
            break;
        }
    }

    return 0;
}

// 0x49F428
int objectDataWrite(Object* obj, File* stream)
{
    Proto* proto;

    ObjectData* data = &(obj->data);
    if (fileWriteInt32(stream, data->inventory.length) == -1) return -1;
    if (fileWriteInt32(stream, data->inventory.capacity) == -1) return -1;
    // TODO: Why do we need to write address of pointer? That probably means
    // this field is shared with something else.
    if (fileWriteInt32(stream, (intptr_t)data->inventory.items) == -1) return -1;

    if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
        if (fileWriteInt32(stream, data->flags) == -1) return -1;
        if (objectCritterCombatDataWrite(&(obj->data.critter.combat), stream) == -1) return -1;
        if (fileWriteInt32(stream, data->critter.hp) == -1) return -1;
        if (fileWriteInt32(stream, data->critter.radiation) == -1) return -1;
        if (fileWriteInt32(stream, data->critter.poison) == -1) return -1;
    } else {
        if (fileWriteInt32(stream, data->flags) == -1) return -1;

        switch (PID_TYPE(obj->pid)) {
        case OBJ_TYPE_ITEM:
            if (protoGetProto(obj->pid, &proto) == -1) return -1;

            switch (proto->item.type) {
            case ITEM_TYPE_WEAPON:
                if (fileWriteInt32(stream, data->item.weapon.ammoQuantity) == -1) return -1;
                if (fileWriteInt32(stream, data->item.weapon.ammoTypePid) == -1) return -1;
                break;
            case ITEM_TYPE_AMMO:
                if (fileWriteInt32(stream, data->item.ammo.quantity) == -1) return -1;
                break;
            case ITEM_TYPE_MISC:
                if (fileWriteInt32(stream, data->item.misc.charges) == -1) return -1;
                break;
            case ITEM_TYPE_KEY:
                if (fileWriteInt32(stream, data->item.key.keyCode) == -1) return -1;
                break;
            }
            break;
        case OBJ_TYPE_SCENERY:
            if (protoGetProto(obj->pid, &proto) == -1) return -1;

            switch (proto->scenery.type) {
            case SCENERY_TYPE_DOOR:
                if (fileWriteInt32(stream, data->scenery.door.openFlags) == -1) return -1;
                break;
            case SCENERY_TYPE_STAIRS:
                if (fileWriteInt32(stream, data->scenery.stairs.destinationBuiltTile) == -1) return -1;
                if (fileWriteInt32(stream, data->scenery.stairs.destinationMap) == -1) return -1;
                break;
            case SCENERY_TYPE_ELEVATOR:
                if (fileWriteInt32(stream, data->scenery.elevator.type) == -1) return -1;
                if (fileWriteInt32(stream, data->scenery.elevator.level) == -1) return -1;
                break;
            case SCENERY_TYPE_LADDER_UP:
                if (fileWriteInt32(stream, data->scenery.ladder.destinationMap) == -1) return -1;
                if (fileWriteInt32(stream, data->scenery.ladder.destinationBuiltTile) == -1) return -1;
                break;
            case SCENERY_TYPE_LADDER_DOWN:
                if (fileWriteInt32(stream, data->scenery.ladder.destinationMap) == -1) return -1;
                if (fileWriteInt32(stream, data->scenery.ladder.destinationBuiltTile) == -1) return -1;
                break;
            default:
                break;
            }
            break;
        case OBJ_TYPE_MISC:
            if (obj->pid >= 0x5000010 && obj->pid <= 0x5000017) {
                if (fileWriteInt32(stream, data->misc.map) == -1) return -1;
                if (fileWriteInt32(stream, data->misc.tile) == -1) return -1;
                if (fileWriteInt32(stream, data->misc.elevation) == -1) return -1;
                if (fileWriteInt32(stream, data->misc.rotation) == -1) return -1;
            }
            break;
        default:
            break;
        }
    }

    return 0;
}

// 0x49F73C
static int _proto_update_gen(Object* obj)
{
    Proto* proto;

    if (!_protos_been_initialized) {
        return -1;
    }

    ObjectData* data = &(obj->data);
    data->inventory.length = 0;
    data->inventory.capacity = 0;
    data->inventory.items = NULL;

    if (protoGetProto(obj->pid, &proto) == -1) {
        return -1;
    }

    switch (PID_TYPE(obj->pid)) {
    case OBJ_TYPE_ITEM:
        switch (proto->item.type) {
        case ITEM_TYPE_CONTAINER:
            data->flags = 0;
            break;
        case ITEM_TYPE_WEAPON:
            data->item.weapon.ammoQuantity = proto->item.data.weapon.ammoCapacity;
            data->item.weapon.ammoTypePid = proto->item.data.weapon.ammoTypePid;
            break;
        case ITEM_TYPE_AMMO:
            data->item.ammo.quantity = proto->item.data.ammo.quantity;
            break;
        case ITEM_TYPE_MISC:
            data->item.misc.charges = proto->item.data.misc.charges;
            break;
        case ITEM_TYPE_KEY:
            data->item.key.keyCode = proto->item.data.key.keyCode;
            break;
        }
        break;
    case OBJ_TYPE_SCENERY:
        switch (proto->scenery.type) {
        case SCENERY_TYPE_DOOR:
            data->scenery.door.openFlags = proto->scenery.data.door.openFlags;
            break;
        case SCENERY_TYPE_STAIRS:
            data->scenery.stairs.destinationBuiltTile = proto->scenery.data.stairs.field_0;
            data->scenery.stairs.destinationMap = proto->scenery.data.stairs.field_4;
            break;
        case SCENERY_TYPE_ELEVATOR:
            data->scenery.elevator.type = proto->scenery.data.elevator.type;
            data->scenery.elevator.level = proto->scenery.data.elevator.level;
            break;
        case SCENERY_TYPE_LADDER_UP:
        case SCENERY_TYPE_LADDER_DOWN:
            data->scenery.ladder.destinationMap = proto->scenery.data.ladder.field_0;
            break;
        }
        break;
    case OBJ_TYPE_MISC:
        if (obj->pid >= 0x5000010 && obj->pid <= 0x5000017) {
            data->misc.tile = -1;
            data->misc.elevation = 0;
            data->misc.rotation = 0;
            data->misc.map = -1;
        }
        break;
    default:
        break;
    }

    return 0;
}

// 0x49F8A0
int _proto_update_init(Object* obj)
{
    if (!_protos_been_initialized) {
        return -1;
    }

    if (obj == NULL) {
        return -1;
    }

    if (obj->pid == -1) {
        return -1;
    }

    for (int i = 0; i < 14; i++) {
        obj->field_2C_array[i] = 0;
    }

    if (PID_TYPE(obj->pid) != OBJ_TYPE_CRITTER) {
        return _proto_update_gen(obj);
    }

    ObjectData* data = &(obj->data);
    data->inventory.length = 0;
    data->inventory.capacity = 0;
    data->inventory.items = NULL;
    _combat_data_init(obj);
    data->critter.hp = critterGetStat(obj, STAT_MAXIMUM_HIT_POINTS);
    data->critter.combat.ap = critterGetStat(obj, STAT_MAXIMUM_ACTION_POINTS);
    critterUpdateDerivedStats(obj);
    obj->data.critter.combat.whoHitMe = NULL;

    Proto* proto;
    if (protoGetProto(obj->pid, &proto) != -1) {
        data->critter.combat.aiPacket = proto->critter.aiPacket;
        data->critter.combat.team = proto->critter.team;
    }

    return 0;
}

// 0x49F984
int _proto_dude_update_gender()
{
    Proto* proto;
    if (protoGetProto(0x1000000, &proto) == -1) {
        return -1;
    }

    int nativeLook = DUDE_NATIVE_LOOK_TRIBAL;
    if (gameMovieIsSeen(MOVIE_VSUIT)) {
        nativeLook = DUDE_NATIVE_LOOK_JUMPSUIT;
    }

    int frmId;
    if (critterGetStat(gDude, STAT_GENDER) == GENDER_MALE) {
        frmId = _art_vault_person_nums[nativeLook][GENDER_MALE];
    } else {
        frmId = _art_vault_person_nums[nativeLook][GENDER_FEMALE];
    }

    _art_vault_guy_num = frmId;

    if (critterGetArmor(gDude) == NULL) {
        int v1 = 0;
        if (critterGetItem2(gDude) != NULL || critterGetItem1(gDude) != NULL) {
            v1 = (gDude->fid & 0xF000) >> 12;
        }

        int fid = buildFid(OBJ_TYPE_CRITTER, _art_vault_guy_num, 0, v1, 0);
        objectSetFid(gDude, fid, NULL);
    }

    proto->fid = buildFid(OBJ_TYPE_CRITTER, _art_vault_guy_num, 0, 0, 0);

    return 0;
}

// proto_dude_init
// 0x49FA64
int _proto_dude_init(const char* path)
{
    gDudeProto.fid = buildFid(OBJ_TYPE_CRITTER, _art_vault_guy_num, 0, 0, 0);

    if (_init_true) {
        _obj_inven_free(&(gDude->data.inventory));
    }

    _init_true = 1;

    Proto* proto;
    if (protoGetProto(0x1000000, &proto) == -1) {
        return -1;
    }

    protoGetProto(gDude->pid, &proto);

    _proto_update_init(gDude);
    gDude->data.critter.combat.aiPacket = 0;
    gDude->data.critter.combat.team = 0;
    _ResetPlayer();

    if (gcdLoad(path) == -1) {
        _retval = -1;
    }

    proto->critter.data.baseStats[STAT_DAMAGE_RESISTANCE_EMP] = 100;
    proto->critter.data.bodyType = 0;
    proto->critter.data.experience = 0;
    proto->critter.data.killType = 0;
    proto->critter.data.damageType = 0;

    _proto_dude_update_gender();
    _inven_reset_dude();

    if ((gDude->flags & OBJECT_FLAT) != 0) {
        _obj_toggle_flat(gDude, NULL);
    }

    if ((gDude->flags & OBJECT_NO_BLOCK) != 0) {
        gDude->flags &= ~OBJECT_NO_BLOCK;
    }

    critterUpdateDerivedStats(gDude);
    critterAdjustHitPoints(gDude, 10000);

    if (_retval) {
        debugPrint("\n ** Error in proto_dude_init()! **\n");
    }

    return 0;
}

// proto_data_member
// 0x49FFD8
int protoGetDataMember(int pid, int member, ProtoDataMemberValue* value)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return -1;
    }

    switch (PID_TYPE(pid)) {
    case OBJ_TYPE_ITEM:
        switch (member) {
        case ITEM_DATA_MEMBER_PID:
            value->integerValue = proto->pid;
            break;
        case ITEM_DATA_MEMBER_NAME:
            // NOTE: uninline
            value->stringValue = protoGetName(proto->scenery.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case ITEM_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = protoGetDescription(proto->pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case ITEM_DATA_MEMBER_FID:
            value->integerValue = proto->fid;
            break;
        case ITEM_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->item.lightDistance;
            break;
        case ITEM_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->item.lightIntensity;
            break;
        case ITEM_DATA_MEMBER_FLAGS:
            value->integerValue = proto->item.flags;
            break;
        case ITEM_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->item.extendedFlags;
            break;
        case ITEM_DATA_MEMBER_SID:
            value->integerValue = proto->item.sid;
            break;
        case ITEM_DATA_MEMBER_TYPE:
            value->integerValue = proto->item.type;
            break;
        case ITEM_DATA_MEMBER_MATERIAL:
            value->integerValue = proto->item.material;
            break;
        case ITEM_DATA_MEMBER_SIZE:
            value->integerValue = proto->item.size;
            break;
        case ITEM_DATA_MEMBER_WEIGHT:
            value->integerValue = proto->item.weight;
            break;
        case ITEM_DATA_MEMBER_COST:
            value->integerValue = proto->item.cost;
            break;
        case ITEM_DATA_MEMBER_INVENTORY_FID:
            value->integerValue = proto->item.inventoryFid;
            break;
        case ITEM_DATA_MEMBER_WEAPON_RANGE:
            if (proto->item.type == ITEM_TYPE_WEAPON) {
                value->integerValue = proto->item.data.weapon.maxRange1;
            }
            break;
        default:
            debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_CRITTER:
        switch (member) {
        case CRITTER_DATA_MEMBER_PID:
            value->integerValue = proto->critter.pid;
            break;
        case CRITTER_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = protoGetName(proto->critter.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case CRITTER_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = protoGetDescription(proto->critter.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case CRITTER_DATA_MEMBER_FID:
            value->integerValue = proto->critter.fid;
            break;
        case CRITTER_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->critter.lightDistance;
            break;
        case CRITTER_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->critter.lightIntensity;
            break;
        case CRITTER_DATA_MEMBER_FLAGS:
            value->integerValue = proto->critter.flags;
            break;
        case CRITTER_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->critter.extendedFlags;
            break;
        case CRITTER_DATA_MEMBER_SID:
            value->integerValue = proto->critter.sid;
            break;
        case CRITTER_DATA_MEMBER_HEAD_FID:
            value->integerValue = proto->critter.headFid;
            break;
        case CRITTER_DATA_MEMBER_BODY_TYPE:
            value->integerValue = proto->critter.data.bodyType;
            break;
        default:
            debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_SCENERY:
        switch (member) {
        case SCENERY_DATA_MEMBER_PID:
            value->integerValue = proto->scenery.pid;
            break;
        case SCENERY_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = protoGetName(proto->scenery.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case SCENERY_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = protoGetDescription(proto->scenery.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case SCENERY_DATA_MEMBER_FID:
            value->integerValue = proto->scenery.fid;
            break;
        case SCENERY_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->scenery.lightDistance;
            break;
        case SCENERY_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->scenery.lightIntensity;
            break;
        case SCENERY_DATA_MEMBER_FLAGS:
            value->integerValue = proto->scenery.flags;
            break;
        case SCENERY_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->scenery.extendedFlags;
            break;
        case SCENERY_DATA_MEMBER_SID:
            value->integerValue = proto->scenery.sid;
            break;
        case SCENERY_DATA_MEMBER_TYPE:
            value->integerValue = proto->scenery.type;
            break;
        case SCENERY_DATA_MEMBER_MATERIAL:
            value->integerValue = proto->scenery.field_2C;
            break;
        default:
            debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_WALL:
        switch (member) {
        case WALL_DATA_MEMBER_PID:
            value->integerValue = proto->wall.pid;
            break;
        case WALL_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = protoGetName(proto->wall.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case WALL_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = protoGetDescription(proto->wall.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case WALL_DATA_MEMBER_FID:
            value->integerValue = proto->wall.fid;
            break;
        case WALL_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->wall.lightDistance;
            break;
        case WALL_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->wall.lightIntensity;
            break;
        case WALL_DATA_MEMBER_FLAGS:
            value->integerValue = proto->wall.flags;
            break;
        case WALL_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->wall.extendedFlags;
            break;
        case WALL_DATA_MEMBER_SID:
            value->integerValue = proto->wall.sid;
            break;
        case WALL_DATA_MEMBER_MATERIAL:
            value->integerValue = proto->wall.material;
            break;
        default:
            debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_TILE:
        debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
        break;
    case OBJ_TYPE_MISC:
        switch (member) {
        case MISC_DATA_MEMBER_PID:
            value->integerValue = proto->misc.pid;
            break;
        case MISC_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = protoGetName(proto->misc.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case MISC_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = protoGetDescription(proto->misc.pid);
            // FIXME: Errornously report type as int, should be string.
            return PROTO_DATA_MEMBER_TYPE_INT;
        case MISC_DATA_MEMBER_FID:
            value->integerValue = proto->misc.fid;
            return 1;
        case MISC_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->misc.lightDistance;
            return 1;
        case MISC_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->misc.lightIntensity;
            break;
        case MISC_DATA_MEMBER_FLAGS:
            value->integerValue = proto->misc.flags;
            break;
        case MISC_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->misc.extendedFlags;
            break;
        default:
            debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    }

    return PROTO_DATA_MEMBER_TYPE_INT;
}

// proto_init
// 0x4A0390
int protoInit()
{
    char* master_patches;
    int len;
    MessageListItem messageListItem;
    char path[COMPAT_MAX_PATH];
    int i;

    if (!configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &master_patches)) {
        return -1;
    }

    sprintf(path, "%s\\proto", master_patches);
    len = strlen(path);

    compat_mkdir(path);

    strcpy(path + len, "\\critters");
    compat_mkdir(path);

    strcpy(path + len, "\\items");
    compat_mkdir(path);

    // TODO: Get rid of cast.
    _proto_critter_init((Proto*)&gDudeProto, 0x1000000);

    gDudeProto.pid = 0x1000000;
    gDudeProto.fid = buildFid(OBJ_TYPE_CRITTER, 1, 0, 0, 0);

    gDude->pid = 0x1000000;
    gDude->sid = 1;

    for (i = 0; i < 6; i++) {
        _proto_remove_list(i);
    }

    _proto_header_load();

    _protos_been_initialized = 1;

    _proto_dude_init("premade\\player.gcd");

    for (i = 0; i < 6; i++) {
        if (!messageListInit(&(_proto_msg_files[i]))) {
            debugPrint("\nError: Initing proto message files!");
            return -1;
        }
    }

    for (i = 0; i < 6; i++) {
        sprintf(path, "%spro_%.4s%s", asc_5186C8, artGetObjectTypeName(i), ".msg");

        if (!messageListLoad(&(_proto_msg_files[i]), path)) {
            debugPrint("\nError: Loading proto message files!");
            return -1;
        }
    }

    _mp_critter_stats_list = _aDrugStatSpecia;
    _critter_stats_list = _critter_stats_list_strs;
    _critter_stats_list_None = _aNone_1;
    for (i = 0; i < STAT_COUNT; i++) {
        _critter_stats_list_strs[i] = statGetName(i);
        if (_critter_stats_list_strs[i] == NULL) {
            debugPrint("\nError: Finding stat names!");
            return -1;
        }
    }

    _mp_perk_code_None = _aNone_1;
    _perk_code_strs = _mp_perk_code_strs;
    for (i = 0; i < PERK_COUNT; i++) {
        _mp_perk_code_strs[i] = perkGetName(i);
        if (_mp_perk_code_strs[i] == NULL) {
            debugPrint("\nError: Finding perk names!");
            return -1;
        }
    }

    if (!messageListInit(&gProtoMessageList)) {
        debugPrint("\nError: Initing main proto message file!");
        return -1;
    }

    sprintf(path, "%sproto.msg", asc_5186C8);

    if (!messageListLoad(&gProtoMessageList, path)) {
        debugPrint("\nError: Loading main proto message file!");
        return -1;
    }

    _proto_none_str = getmsg(&gProtoMessageList, &messageListItem, 10);

    // material type names
    for (i = 0; i < MATERIAL_TYPE_COUNT; i++) {
        gMaterialTypeNames[i] = getmsg(&gProtoMessageList, &messageListItem, 100 + i);
    }

    // item type names
    for (i = 0; i < ITEM_TYPE_COUNT; i++) {
        gItemTypeNames[i] = getmsg(&gProtoMessageList, &messageListItem, 150 + i);
    }

    // scenery type names
    for (i = 0; i < SCENERY_TYPE_COUNT; i++) {
        gSceneryTypeNames[i] = getmsg(&gProtoMessageList, &messageListItem, 200 + i);
    }

    // damage code types
    for (i = 0; i < DAMAGE_TYPE_COUNT; i++) {
        gDamageTypeNames[i] = getmsg(&gProtoMessageList, &messageListItem, 250 + i);
    }

    // caliber types
    for (i = 0; i < CALIBER_TYPE_COUNT; i++) {
        gCaliberTypeNames[i] = getmsg(&gProtoMessageList, &messageListItem, 300 + i);
    }

    // race types
    for (i = 0; i < RACE_TYPE_COUNT; i++) {
        gRaceTypeNames[i] = getmsg(&gProtoMessageList, &messageListItem, 350 + i);
    }

    // body types
    for (i = 0; i < BODY_TYPE_COUNT; i++) {
        gBodyTypeNames[i] = getmsg(&gProtoMessageList, &messageListItem, 400 + i);
    }

    return 0;
}

// 0x4A0814
void protoReset()
{
    int i;

    // TODO: Get rid of cast.
    _proto_critter_init((Proto*)&gDudeProto, 0x1000000);
    gDudeProto.pid = 0x1000000;
    gDudeProto.fid = buildFid(OBJ_TYPE_CRITTER, 1, 0, 0, 0);

    gDude->pid = 0x1000000;
    gDude->sid = -1;
    gDude->flags &= ~OBJECT_FLAG_0xFC000;

    for (i = 0; i < 6; i++) {
        _proto_remove_list(i);
    }

    _proto_header_load();

    _protos_been_initialized = 1;
    _proto_dude_init("premade\\player.gcd");
}

// 0x4A0898
void protoExit()
{
    int i;

    for (i = 0; i < 6; i++) {
        _proto_remove_list(i);
    }

    for (i = 0; i < 6; i++) {
        messageListFree(&(_proto_msg_files[i]));
    }

    messageListFree(&gProtoMessageList);
}

// Count .pro lines in .lst files.
//
// 0x4A08E0
static int _proto_header_load()
{
    for (int index = 0; index < 6; index++) {
        ProtoList* ptr = &(_protoLists[index]);
        ptr->head = NULL;
        ptr->tail = NULL;
        ptr->length = 0;
        ptr->max_entries_num = 1;

        char path[COMPAT_MAX_PATH];
        _proto_make_path(path, index << 24);
        strcat(path, "\\");
        strcat(path, artGetObjectTypeName(index));
        strcat(path, ".lst");

        File* stream = fileOpen(path, "rt");
        if (stream == NULL) {
            return -1;
        }

        int ch = '\0';
        while (1) {
            ch = fileReadChar(stream);
            if (ch == -1) {
                break;
            }

            if (ch == '\n') {
                ptr->max_entries_num++;
            }
        }

        if (ch != '\n') {
            ptr->max_entries_num++;
        }

        fileClose(stream);
    }

    return 0;
}

// 0x4A0AEC
static int protoItemDataRead(ItemProtoData* item_data, int type, File* stream)
{
    switch (type) {
    case ITEM_TYPE_ARMOR:
        if (fileReadInt32(stream, &(item_data->armor.armorClass)) == -1) return -1;
        if (fileReadInt32List(stream, item_data->armor.damageResistance, 7) == -1) return -1;
        if (fileReadInt32List(stream, item_data->armor.damageThreshold, 7) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->armor.perk)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->armor.maleFid)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->armor.femaleFid)) == -1) return -1;

        return 0;
    case ITEM_TYPE_CONTAINER:
        if (fileReadInt32(stream, &(item_data->container.maxSize)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->container.openFlags)) == -1) return -1;

        return 0;
    case ITEM_TYPE_DRUG:
        if (fileReadInt32(stream, &(item_data->drug.stat[0])) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->drug.stat[1])) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->drug.stat[2])) == -1) return -1;
        if (fileReadInt32List(stream, item_data->drug.amount, 3) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->drug.duration1)) == -1) return -1;
        if (fileReadInt32List(stream, item_data->drug.amount1, 3) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->drug.duration2)) == -1) return -1;
        if (fileReadInt32List(stream, item_data->drug.amount2, 3) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->drug.addictionChance)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->drug.withdrawalEffect)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->drug.withdrawalOnset)) == -1) return -1;

        return 0;
    case ITEM_TYPE_WEAPON:
        if (fileReadInt32(stream, &(item_data->weapon.animationCode)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.minDamage)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.maxDamage)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.damageType)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.maxRange1)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.maxRange2)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.projectilePid)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.minStrength)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.actionPointCost1)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.actionPointCost2)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.criticalFailureType)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.perk)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.rounds)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.caliber)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.ammoTypePid)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.ammoCapacity)) == -1) return -1;
        if (fileReadUInt8(stream, &(item_data->weapon.soundCode)) == -1) return -1;

        return 0;
    case ITEM_TYPE_AMMO:
        if (fileReadInt32(stream, &(item_data->ammo.caliber)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->ammo.quantity)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->ammo.armorClassModifier)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->ammo.damageResistanceModifier)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->ammo.damageMultiplier)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->ammo.damageDivisor)) == -1) return -1;

        return 0;
    case ITEM_TYPE_MISC:
        if (fileReadInt32(stream, &(item_data->misc.powerTypePid)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->misc.powerType)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->misc.charges)) == -1) return -1;

        return 0;
    case ITEM_TYPE_KEY:
        if (fileReadInt32(stream, &(item_data->key.keyCode)) == -1) return -1;

        return 0;
    }

    return 0;
}

// 0x4A0ED0
static int protoSceneryDataRead(SceneryProtoData* scenery_data, int type, File* stream)
{
    switch (type) {
    case SCENERY_TYPE_DOOR:
        if (fileReadInt32(stream, &(scenery_data->door.openFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(scenery_data->door.keyCode)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_STAIRS:
        if (fileReadInt32(stream, &(scenery_data->stairs.field_0)) == -1) return -1;
        if (fileReadInt32(stream, &(scenery_data->stairs.field_4)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_ELEVATOR:
        if (fileReadInt32(stream, &(scenery_data->elevator.type)) == -1) return -1;
        if (fileReadInt32(stream, &(scenery_data->elevator.level)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_LADDER_UP:
    case SCENERY_TYPE_LADDER_DOWN:
        if (fileReadInt32(stream, &(scenery_data->ladder.field_0)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_GENERIC:
        if (fileReadInt32(stream, &(scenery_data->generic.field_0)) == -1) return -1;

        return 0;
    }

    return 0;
}

// read .pro file
// 0x4A0FA0
static int protoRead(Proto* proto, File* stream)
{
    if (fileReadInt32(stream, &(proto->pid)) == -1) return -1;
    if (fileReadInt32(stream, &(proto->messageId)) == -1) return -1;
    if (fileReadInt32(stream, &(proto->fid)) == -1) return -1;

    switch (PID_TYPE(proto->pid)) {
    case OBJ_TYPE_ITEM:
        if (fileReadInt32(stream, &(proto->item.lightDistance)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->item.lightIntensity)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.flags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.extendedFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.sid)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.type)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.material)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.size)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->item.weight)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.cost)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.inventoryFid)) == -1) return -1;
        if (fileReadUInt8(stream, &(proto->item.field_80)) == -1) return -1;
        if (protoItemDataRead(&(proto->item.data), proto->item.type, stream) == -1) return -1;

        return 0;
    case OBJ_TYPE_CRITTER:
        if (fileReadInt32(stream, &(proto->critter.lightDistance)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->critter.lightIntensity)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->critter.flags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->critter.extendedFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->critter.sid)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->critter.headFid)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->critter.aiPacket)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->critter.team)) == -1) return -1;

        if (protoCritterDataRead(stream, &(proto->critter.data)) == -1) return -1;

        return 0;
    case OBJ_TYPE_SCENERY:
        if (fileReadInt32(stream, &(proto->scenery.lightDistance)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->scenery.lightIntensity)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->scenery.flags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->scenery.extendedFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->scenery.sid)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->scenery.type)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->scenery.field_2C)) == -1) return -1;
        if (fileReadUInt8(stream, &(proto->scenery.field_34)) == -1) return -1;
        if (protoSceneryDataRead(&(proto->scenery.data), proto->scenery.type, stream) == -1) return -1;
        return 0;
    case OBJ_TYPE_WALL:
        if (fileReadInt32(stream, &(proto->wall.lightDistance)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->wall.lightIntensity)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->wall.flags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->wall.extendedFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->wall.sid)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->wall.material)) == -1) return -1;

        return 0;
    case OBJ_TYPE_TILE:
        if (fileReadInt32(stream, &(proto->tile.flags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->tile.extendedFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->tile.sid)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->tile.material)) == -1) return -1;

        return 0;
    case OBJ_TYPE_MISC:
        if (fileReadInt32(stream, &(proto->misc.lightDistance)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->misc.lightIntensity)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->misc.flags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->misc.extendedFlags)) == -1) return -1;

        return 0;
    }

    return -1;
}

// 0x4A1390
static int protoItemDataWrite(ItemProtoData* item_data, int type, File* stream)
{
    switch (type) {
    case ITEM_TYPE_ARMOR:
        if (fileWriteInt32(stream, item_data->armor.armorClass) == -1) return -1;
        if (fileWriteInt32List(stream, item_data->armor.damageResistance, 7) == -1) return -1;
        if (fileWriteInt32List(stream, item_data->armor.damageThreshold, 7) == -1) return -1;
        if (fileWriteInt32(stream, item_data->armor.perk) == -1) return -1;
        if (fileWriteInt32(stream, item_data->armor.maleFid) == -1) return -1;
        if (fileWriteInt32(stream, item_data->armor.femaleFid) == -1) return -1;

        return 0;
    case ITEM_TYPE_CONTAINER:
        if (fileWriteInt32(stream, item_data->container.maxSize) == -1) return -1;
        if (fileWriteInt32(stream, item_data->container.openFlags) == -1) return -1;

        return 0;
    case ITEM_TYPE_DRUG:
        if (fileWriteInt32(stream, item_data->drug.stat[0]) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.stat[1]) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.stat[2]) == -1) return -1;
        if (fileWriteInt32List(stream, item_data->drug.amount, 3) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.duration1) == -1) return -1;
        if (fileWriteInt32List(stream, item_data->drug.amount1, 3) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.duration2) == -1) return -1;
        if (fileWriteInt32List(stream, item_data->drug.amount2, 3) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.addictionChance) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.withdrawalEffect) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.withdrawalOnset) == -1) return -1;

        return 0;
    case ITEM_TYPE_WEAPON:
        if (fileWriteInt32(stream, item_data->weapon.animationCode) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.maxDamage) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.minDamage) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.damageType) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.maxRange1) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.maxRange2) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.projectilePid) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.minStrength) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.actionPointCost1) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.actionPointCost2) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.criticalFailureType) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.perk) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.rounds) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.caliber) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.ammoTypePid) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.ammoCapacity) == -1) return -1;
        if (fileWriteUInt8(stream, item_data->weapon.soundCode) == -1) return -1;

        return 0;
    case ITEM_TYPE_AMMO:
        if (fileWriteInt32(stream, item_data->ammo.caliber) == -1) return -1;
        if (fileWriteInt32(stream, item_data->ammo.quantity) == -1) return -1;
        if (fileWriteInt32(stream, item_data->ammo.armorClassModifier) == -1) return -1;
        if (fileWriteInt32(stream, item_data->ammo.damageResistanceModifier) == -1) return -1;
        if (fileWriteInt32(stream, item_data->ammo.damageMultiplier) == -1) return -1;
        if (fileWriteInt32(stream, item_data->ammo.damageDivisor) == -1) return -1;

        return 0;
    case ITEM_TYPE_MISC:
        if (fileWriteInt32(stream, item_data->misc.powerTypePid) == -1) return -1;
        if (fileWriteInt32(stream, item_data->misc.powerType) == -1) return -1;
        if (fileWriteInt32(stream, item_data->misc.charges) == -1) return -1;

        return 0;
    case ITEM_TYPE_KEY:
        if (fileWriteInt32(stream, item_data->key.keyCode) == -1) return -1;

        return 0;
    }

    return 0;
}

// 0x4A16E4
static int protoSceneryDataWrite(SceneryProtoData* scenery_data, int type, File* stream)
{
    switch (type) {
    case SCENERY_TYPE_DOOR:
        if (fileWriteInt32(stream, scenery_data->door.openFlags) == -1) return -1;
        if (fileWriteInt32(stream, scenery_data->door.keyCode) == -1) return -1;

        return 0;
    case SCENERY_TYPE_STAIRS:
        if (fileWriteInt32(stream, scenery_data->stairs.field_0) == -1) return -1;
        if (fileWriteInt32(stream, scenery_data->stairs.field_4) == -1) return -1;

        return 0;
    case SCENERY_TYPE_ELEVATOR:
        if (fileWriteInt32(stream, scenery_data->elevator.type) == -1) return -1;
        if (fileWriteInt32(stream, scenery_data->elevator.level) == -1) return -1;

        return 0;
    case SCENERY_TYPE_LADDER_UP:
    case SCENERY_TYPE_LADDER_DOWN:
        if (fileWriteInt32(stream, scenery_data->ladder.field_0) == -1) return -1;

        return 0;
    case SCENERY_TYPE_GENERIC:
        if (fileWriteInt32(stream, scenery_data->generic.field_0) == -1) return -1;

        return 0;
    }

    return 0;
}

// 0x4A17B4
static int protoWrite(Proto* proto, File* stream)
{
    if (fileWriteInt32(stream, proto->pid) == -1) return -1;
    if (fileWriteInt32(stream, proto->messageId) == -1) return -1;
    if (fileWriteInt32(stream, proto->fid) == -1) return -1;

    switch (PID_TYPE(proto->pid)) {
    case OBJ_TYPE_ITEM:
        if (fileWriteInt32(stream, proto->item.lightDistance) == -1) return -1;
        if (_db_fwriteLong(stream, proto->item.lightIntensity) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.flags) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.extendedFlags) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.sid) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.type) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.material) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.size) == -1) return -1;
        if (_db_fwriteLong(stream, proto->item.weight) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.cost) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.inventoryFid) == -1) return -1;
        if (fileWriteUInt8(stream, proto->item.field_80) == -1) return -1;
        if (protoItemDataWrite(&(proto->item.data), proto->item.type, stream) == -1) return -1;

        return 0;
    case OBJ_TYPE_CRITTER:
        if (fileWriteInt32(stream, proto->critter.lightDistance) == -1) return -1;
        if (_db_fwriteLong(stream, proto->critter.lightIntensity) == -1) return -1;
        if (fileWriteInt32(stream, proto->critter.flags) == -1) return -1;
        if (fileWriteInt32(stream, proto->critter.extendedFlags) == -1) return -1;
        if (fileWriteInt32(stream, proto->critter.sid) == -1) return -1;
        if (fileWriteInt32(stream, proto->critter.headFid) == -1) return -1;
        if (fileWriteInt32(stream, proto->critter.aiPacket) == -1) return -1;
        if (fileWriteInt32(stream, proto->critter.team) == -1) return -1;
        if (protoCritterDataWrite(stream, &(proto->critter.data)) == -1) return -1;

        return 0;
    case OBJ_TYPE_SCENERY:
        if (fileWriteInt32(stream, proto->scenery.lightDistance) == -1) return -1;
        if (_db_fwriteLong(stream, proto->scenery.lightIntensity) == -1) return -1;
        if (fileWriteInt32(stream, proto->scenery.flags) == -1) return -1;
        if (fileWriteInt32(stream, proto->scenery.extendedFlags) == -1) return -1;
        if (fileWriteInt32(stream, proto->scenery.sid) == -1) return -1;
        if (fileWriteInt32(stream, proto->scenery.type) == -1) return -1;
        if (fileWriteInt32(stream, proto->scenery.field_2C) == -1) return -1;
        if (fileWriteUInt8(stream, proto->scenery.field_34) == -1) return -1;
        if (protoSceneryDataWrite(&(proto->scenery.data), proto->scenery.type, stream) == -1) return -1;
    case OBJ_TYPE_WALL:
        if (fileWriteInt32(stream, proto->wall.lightDistance) == -1) return -1;
        if (_db_fwriteLong(stream, proto->wall.lightIntensity) == -1) return -1;
        if (fileWriteInt32(stream, proto->wall.flags) == -1) return -1;
        if (fileWriteInt32(stream, proto->wall.extendedFlags) == -1) return -1;
        if (fileWriteInt32(stream, proto->wall.sid) == -1) return -1;
        if (fileWriteInt32(stream, proto->wall.material) == -1) return -1;

        return 0;
    case OBJ_TYPE_TILE:
        if (fileWriteInt32(stream, proto->tile.flags) == -1) return -1;
        if (fileWriteInt32(stream, proto->tile.extendedFlags) == -1) return -1;
        if (fileWriteInt32(stream, proto->tile.sid) == -1) return -1;
        if (fileWriteInt32(stream, proto->tile.material) == -1) return -1;

        return 0;
    case OBJ_TYPE_MISC:
        if (fileWriteInt32(stream, proto->misc.lightDistance) == -1) return -1;
        if (_db_fwriteLong(stream, proto->misc.lightIntensity) == -1) return -1;
        if (fileWriteInt32(stream, proto->misc.flags) == -1) return -1;
        if (fileWriteInt32(stream, proto->misc.extendedFlags) == -1) return -1;

        return 0;
    }

    return -1;
}

// 0x4A1B30
int _proto_save_pid(int pid)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return -1;
    }

    char path[260];
    _proto_make_path(path, pid);
    strcat(path, "\\");

    _proto_list_str(pid, path + strlen(path));

    File* stream = fileOpen(path, "wb");
    if (stream == NULL) {
        return -1;
    }

    int rc = protoWrite(proto, stream);

    fileClose(stream);

    return rc;
}

// 0x4A1C3C
static int _proto_load_pid(int pid, Proto** protoPtr)
{
    char path[COMPAT_MAX_PATH];
    _proto_make_path(path, pid);
    strcat(path, "\\");

    if (_proto_list_str(pid, path + strlen(path)) == -1) {
        return -1;
    }

    File* stream = fileOpen(path, "rb");
    if (stream == NULL) {
        debugPrint("\nError: Can't fopen proto!\n");
        *protoPtr = NULL;
        return -1;
    }

    if (_proto_find_free_subnode(PID_TYPE(pid), protoPtr) == -1) {
        fileClose(stream);
        return -1;
    }

    if (protoRead(*protoPtr, stream) != 0) {
        fileClose(stream);
        return -1;
    }

    fileClose(stream);
    return 0;
}

// allocate memory for proto of given type and adds it to proto cache
static int _proto_find_free_subnode(int type, Proto** protoPtr)
{
    size_t size = (type >= 0 && type < 11) ? _proto_sizes[type] : 0;

    Proto* proto = (Proto*)internal_malloc(size);
    *protoPtr = proto;
    if (proto == NULL) {
        return -1;
    }

    ProtoList* protoList = &(_protoLists[type]);
    ProtoListExtent* protoListExtent = protoList->tail;

    if (protoList->head != NULL) {
        if (protoListExtent->length == PROTO_LIST_EXTENT_SIZE) {
            ProtoListExtent* newExtent = protoListExtent->next = (ProtoListExtent*)internal_malloc(sizeof(ProtoListExtent));
            if (protoListExtent == NULL) {
                internal_free(proto);
                *protoPtr = NULL;
                return -1;
            }

            newExtent->length = 0;
            newExtent->next = NULL;

            protoList->tail = newExtent;
            protoList->length++;

            protoListExtent = newExtent;
        }
    } else {
        protoListExtent = (ProtoListExtent*)internal_malloc(sizeof(ProtoListExtent));
        if (protoListExtent == NULL) {
            internal_free(proto);
            *protoPtr = NULL;
            return -1;
        }

        protoListExtent->next = NULL;
        protoListExtent->length = 0;

        protoList->length = 1;
        protoList->tail = protoListExtent;
        protoList->head = protoListExtent;
    }

    protoListExtent->proto[protoListExtent->length] = proto;
    protoListExtent->length++;

    return 0;
}

// Evict top most proto cache block.
//
// 0x4A2040
static void _proto_remove_some_list(int type)
{
    ProtoList* protoList = &(_protoLists[type]);
    ProtoListExtent* protoListExtent = protoList->head;
    if (protoListExtent != NULL) {
        protoList->length--;
        protoList->head = protoListExtent->next;

        for (int index = 0; index < protoListExtent->length; index++) {
            internal_free(protoListExtent->proto[index]);
        }

        internal_free(protoListExtent);
    }
}

// Clear proto cache of given type.
//
// 0x4A2094
static void _proto_remove_list(int type)
{
    ProtoList* protoList = &(_protoLists[type]);

    ProtoListExtent* curr = protoList->head;
    while (curr != NULL) {
        ProtoListExtent* next = curr->next;
        for (int index = 0; index < curr->length; index++) {
            internal_free(curr->proto[index]);
        }
        internal_free(curr);
        curr = next;
    }

    protoList->head = NULL;
    protoList->tail = NULL;
    protoList->length = 0;
}

// Clear all proto cache.
//
// 0x4A20F4
void _proto_remove_all()
{
    for (int index = 0; index < 6; index++) {
        _proto_remove_list(index);
    }
}

// proto_ptr
// 0x4A2108
int protoGetProto(int pid, Proto** protoPtr)
{
    *protoPtr = NULL;

    if (pid == -1) {
        return -1;
    }

    if (pid == 0x1000000) {
        *protoPtr = (Proto*)&gDudeProto;
        return 0;
    }

    ProtoList* protoList = &(_protoLists[PID_TYPE(pid)]);
    ProtoListExtent* protoListExtent = protoList->head;
    while (protoListExtent != NULL) {
        for (int index = 0; index < protoListExtent->length; index++) {
            Proto* proto = (Proto*)protoListExtent->proto[index];
            if (pid == proto->pid) {
                *protoPtr = proto;
                return 0;
            }
        }
        protoListExtent = protoListExtent->next;
    }

    if (protoList->head != NULL && protoList->tail != NULL) {
        if (PROTO_LIST_EXTENT_SIZE * protoList->length - (PROTO_LIST_EXTENT_SIZE - protoList->tail->length) > PROTO_LIST_MAX_ENTRIES) {
            _proto_remove_some_list(PID_TYPE(pid));
        }
    }

    return _proto_load_pid(pid, protoPtr);
}

// 0x4A21DC
static int _proto_new_id(int a1)
{
    int result = _protoLists[a1].max_entries_num;
    _protoLists[a1].max_entries_num = result + 1;

    return result;
}

// 0x4A2214
static int _proto_max_id(int a1)
{
    return _protoLists[a1].max_entries_num;
}

// 0x4A22C0
int _ResetPlayer()
{
    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    pcStatsReset();
    protoCritterDataResetStats(&(proto->critter.data));
    critterReset();
    characterEditorReset();
    protoCritterDataResetSkills(&(proto->critter.data));
    skillsReset();
    perksReset();
    traitsReset();
    critterUpdateDerivedStats(gDude);
    return 0;
}
