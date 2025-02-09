#include "sfall_opcodes.h"

#include <string.h>

#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "dbox.h"
#include "debug.h"
#include "game.h"
#include "input.h"
#include "interface.h"
#include "interpreter.h"
#include "item.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "object.h"
#include "party_member.h"
#include "proto.h"
#include "scripts.h"
#include "sfall_arrays.h"
#include "sfall_global_scripts.h"
#include "sfall_global_vars.h"
#include "sfall_ini.h"
#include "sfall_kb_helpers.h"
#include "sfall_lists.h"
#include "sfall_metarules.h"
#include "stat.h"
#include "svga.h"
#include "tile.h"
#include "worldmap.h"

namespace fallout {

typedef enum ExplosionMetarule {
    EXPL_FORCE_EXPLOSION_PATTERN = 1,
    EXPL_FORCE_EXPLOSION_ART = 2,
    EXPL_FORCE_EXPLOSION_RADIUS = 3,
    EXPL_FORCE_EXPLOSION_DMGTYPE = 4,
    EXPL_STATIC_EXPLOSION_RADIUS = 5,
    EXPL_GET_EXPLOSION_DAMAGE = 6,
    EXPL_SET_DYNAMITE_EXPLOSION_DAMAGE = 7,
    EXPL_SET_PLASTIC_EXPLOSION_DAMAGE = 8,
    EXPL_SET_EXPLOSION_MAX_TARGET = 9,
} ExplosionMetarule;

static constexpr int kVersionMajor = 4;
static constexpr int kVersionMinor = 3;
static constexpr int kVersionPatch = 4;

// read_byte
static void opReadByte(Program* program)
{
    int addr = programStackPopInteger(program);

    int value = 0;
    switch (addr) {
    case 0x56D38C:
        value = combatGetTargetHighlight();
        break;
    default:
        debugPrint("%s: attempt to 'read_byte' at 0x%x", program->name, addr);
        break;
    }

    programStackPushInteger(program, value);
}

// set_pc_base_stat
static void op_set_pc_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBaseStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    critterSetBaseStat(gDude, stat, value);
}

// set_pc_extra_stat
static void opSetPcBonusStat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBonusStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    critterSetBonusStat(gDude, stat, value);
}

// get_pc_base_stat
static void op_get_pc_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall obtains value directly from
    // dude's proto. This can have unforeseen consequences when dealing with
    // current stats.
    int stat = programStackPopInteger(program);
    programStackPushInteger(program, critterGetBaseStat(gDude, stat));
}

// get_pc_extra_stat
static void opGetPcBonusStat(Program* program)
{
    int stat = programStackPopInteger(program);
    int value = critterGetBonusStat(gDude, stat);
    programStackPushInteger(program, value);
}

// tap_key
static void op_tap_key(Program* program)
{
    int key = programStackPopInteger(program);
    sfall_kb_press_key(key);
}

// get_year
static void op_get_year(Program* program)
{
    int year;
    gameTimeGetDate(nullptr, nullptr, &year);
    programStackPushInteger(program, year);
}

// game_loaded
static void op_game_loaded(Program* program)
{
    bool loaded = sfall_gl_scr_is_loaded(program);
    programStackPushInteger(program, loaded ? 1 : 0);
}

// set_global_script_repeat
static void op_set_global_script_repeat(Program* program)
{
    int frames = programStackPopInteger(program);
    sfall_gl_scr_set_repeat(program, frames);
}

// key_pressed
static void op_key_pressed(Program* program)
{
    int key = programStackPopInteger(program);
    bool pressed = sfall_kb_is_key_pressed(key);
    programStackPushInteger(program, pressed ? 1 : 0);
}

// in_world_map
static void op_in_world_map(Program* program)
{
    programStackPushInteger(program, GameMode::isInGameMode(GameMode::kWorldmap) ? 1 : 0);
}

// force_encounter
static void op_force_encounter(Program* program)
{
    int map = programStackPopInteger(program);
    wmForceEncounter(map, 0);
}

// set_world_map_pos
static void op_set_world_map_pos(Program* program)
{
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    wmSetPartyWorldPos(x, y);
}

// active_hand
static void opGetCurrentHand(Program* program)
{
    programStackPushInteger(program, interfaceGetCurrentHand());
}

// set_global_script_type
static void op_set_global_script_type(Program* program)
{
    int type = programStackPopInteger(program);
    sfall_gl_scr_set_type(program, type);
}

// set_sfall_global
static void opSetGlobalVar(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    ProgramValue variable = programStackPopValue(program);

    if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* key = programGetString(program, variable.opcode, variable.integerValue);
        sfall_gl_vars_store(key, value.integerValue);
    } else if (variable.opcode == VALUE_TYPE_INT) {
        sfall_gl_vars_store(variable.integerValue, value.integerValue);
    }
}

// get_sfall_global_int
static void opGetGlobalInt(Program* program)
{
    ProgramValue variable = programStackPopValue(program);

    int value = 0;
    if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* key = programGetString(program, variable.opcode, variable.integerValue);
        sfall_gl_vars_fetch(key, value);
    } else if (variable.opcode == VALUE_TYPE_INT) {
        sfall_gl_vars_fetch(variable.integerValue, value);
    }

    programStackPushInteger(program, value);
}

// get_ini_setting
static void op_get_ini_setting(Program* program)
{
    const char* string = programStackPopString(program);

    int value;
    if (sfall_ini_get_int(string, &value)) {
        programStackPushInteger(program, value);
    } else {
        programStackPushInteger(program, -1);
    }
}

// get_game_mode
static void opGetGameMode(Program* program)
{
    programStackPushInteger(program, GameMode::getCurrentGameMode());
}

// get_uptime
static void op_get_uptime(Program* program)
{
    programStackPushInteger(program, getTicks());
}

// set_car_current_town
static void op_set_car_current_town(Program* program)
{
    int area = programStackPopInteger(program);
    wmCarSetCurrentArea(area);
}

// get_bodypart_hit_modifier
static void op_get_bodypart_hit_modifier(Program* program)
{
    int hit_location = programStackPopInteger(program);
    programStackPushInteger(program, combat_get_hit_location_penalty(hit_location));
}

// set_bodypart_hit_modifier
static void op_set_bodypart_hit_modifier(Program* program)
{
    int penalty = programStackPopInteger(program);
    int hit_location = programStackPopInteger(program);
    combat_set_hit_location_penalty(hit_location, penalty);
}

// get_ini_string
static void op_get_ini_string(Program* program)
{
    const char* string = programStackPopString(program);

    char value[256];
    if (sfall_ini_get_string(string, value, sizeof(value))) {
        programStackPushString(program, value);
    } else {
        programStackPushInteger(program, -1);
    }
}

// sqrt
static void op_sqrt(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, sqrtf(programValue.asFloat()));
}

// abs
static void op_abs(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);

    if (programValue.isInt()) {
        programStackPushInteger(program, abs(programValue.integerValue));
    } else {
        programStackPushFloat(program, abs(programValue.asFloat()));
    }
}

// get_script
static void op_get_script(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    programStackPushInteger(program, obj->scriptIndex + 1);
}

// get_proto_data
static void op_get_proto_data(Program* program)
{
    size_t offset = static_cast<size_t>(programStackPopInteger(program));
    int pid = programStackPopInteger(program);

    Proto* proto;
    if (protoGetProto(pid, &proto) != 0) {
        debugPrint("op_get_proto_data: bad proto %d", pid);
        programStackPushInteger(program, -1);
        return;
    }

    // CE: Make sure the requested offset is within memory bounds and is
    // properly aligned.
    if (offset + sizeof(int) > proto_size(PID_TYPE(pid)) || offset % sizeof(int) != 0) {
        debugPrint("op_get_proto_data: bad offset %d", offset);
        programStackPushInteger(program, -1);
        return;
    }

    int value = *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(proto) + offset);
    programStackPushInteger(program, value);
}

// set_proto_data
static void op_set_proto_data(Program* program)
{
    int value = programStackPopInteger(program);
    size_t offset = static_cast<size_t>(programStackPopInteger(program));
    int pid = programStackPopInteger(program);

    Proto* proto;
    if (protoGetProto(pid, &proto) != 0) {
        debugPrint("op_set_proto_data: bad proto %d", pid);
        programStackPushInteger(program, -1);
        return;
    }

    // CE: Make sure the requested offset is within memory bounds and is
    // properly aligned.
    if (offset + sizeof(int) > proto_size(PID_TYPE(pid)) || offset % sizeof(int) != 0) {
        debugPrint("op_set_proto_data: bad offset %d", offset);
        programStackPushInteger(program, -1);
        return;
    }

    *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(proto) + offset) = value;
}

// set_self
static void op_set_self(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int sid = scriptGetSid(program);

    Script* scr;
    if (scriptGetScript(sid, &scr) == 0) {
        scr->overriddenSelf = obj;
    }
}

// list_begin
static void opListBegin(Program* program)
{
    int listType = programStackPopInteger(program);
    int listId = sfallListsCreate(listType);
    programStackPushInteger(program, listId);
}

// list_next
static void opListNext(Program* program)
{
    int listId = programStackPopInteger(program);
    Object* obj = sfallListsGetNext(listId);
    programStackPushPointer(program, obj);
}

// list_end
static void opListEnd(Program* program)
{
    int listId = programStackPopInteger(program);
    sfallListsDestroy(listId);
}

// sfall_ver_major
static void opGetVersionMajor(Program* program)
{
    programStackPushInteger(program, kVersionMajor);
}

// sfall_ver_minor
static void opGetVersionMinor(Program* program)
{
    programStackPushInteger(program, kVersionMinor);
}

// sfall_ver_build
static void opGetVersionPatch(Program* program)
{
    programStackPushInteger(program, kVersionPatch);
}

// get_weapon_ammo_pid
static void opGetWeaponAmmoPid(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int pid = -1;
    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_WEAPON:
                pid = weaponGetAmmoTypePid(obj);
                break;
            case ITEM_TYPE_MISC:
                pid = miscItemGetPowerTypePid(obj);
                break;
            }
        }
    }

    programStackPushInteger(program, pid);
}

// There are two problems with this function.
//
// 1. Sfall's implementation changes ammo PID of misc items, which is impossible
// since it's stored in proto, not in the object.
// 2. Changing weapon's ammo PID is done without checking for ammo
// quantity/capacity which can probably lead to bad things.
//
// set_weapon_ammo_pid
static void opSetWeaponAmmoPid(Program* program)
{
    int ammoTypePid = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_WEAPON:
                obj->data.item.weapon.ammoTypePid = ammoTypePid;
                break;
            }
        }
    }
}

// get_weapon_ammo_count
static void opGetWeaponAmmoCount(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    // CE: Implementation is different.
    int ammoQuantityOrCharges = 0;
    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_AMMO:
            case ITEM_TYPE_WEAPON:
                ammoQuantityOrCharges = ammoGetQuantity(obj);
                break;
            case ITEM_TYPE_MISC:
                ammoQuantityOrCharges = miscItemGetCharges(obj);
                break;
            }
        }
    }

    programStackPushInteger(program, ammoQuantityOrCharges);
}

// set_weapon_ammo_count
static void opSetWeaponAmmoCount(Program* program)
{
    int ammoQuantityOrCharges = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    // CE: Implementation is different.
    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_AMMO:
            case ITEM_TYPE_WEAPON:
                ammoSetQuantity(obj, ammoQuantityOrCharges);
                break;
            case ITEM_TYPE_MISC:
                miscItemSetCharges(obj, ammoQuantityOrCharges);
                break;
            }
        }
    }
}

// get_mouse_x
static void opGetMouseX(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);
    programStackPushInteger(program, x);
}

// get_mouse_y
static void opGetMouseY(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);
    programStackPushInteger(program, y);
}

// get_mouse_buttons
static void op_get_mouse_buttons(Program* program)
{
    // CE: Implementation is slightly different - it does not handle middle
    // mouse button.
    programStackPushInteger(program, mouse_get_last_buttons());
}

// get_screen_width
static void opGetScreenWidth(Program* program)
{
    programStackPushInteger(program, screenGetWidth());
}

// get_screen_height
static void opGetScreenHeight(Program* program)
{
    programStackPushInteger(program, screenGetHeight());
}

// create_message_window
static void op_create_message_window(Program* program)
{
    static bool showing = false;

    if (showing) {
        return;
    }

    const char* string = programStackPopString(program);
    if (string == nullptr || string[0] == '\0') {
        return;
    }

    char* copy = internal_strdup(string);

    const char* body[4];
    int count = 0;

    char* pch = strchr(copy, '\n');
    while (pch != nullptr && count < 4) {
        *pch = '\0';
        body[count++] = pch + 1;
        pch = strchr(pch + 1, '\n');
    }

    showing = true;
    showDialogBox(copy,
        body,
        count,
        192,
        116,
        _colorTable[32328],
        nullptr,
        _colorTable[32328],
        DIALOG_BOX_LARGE);
    showing = false;

    internal_free(copy);
}

// get_attack_type
static void op_get_attack_type(Program* program)
{
    int hit_mode;
    if (interface_get_current_attack_mode(&hit_mode)) {
        programStackPushInteger(program, hit_mode);
    } else {
        programStackPushInteger(program, -1);
    }
}

// force_encounter_with_flags
static void op_force_encounter_with_flags(Program* program)
{
    unsigned int flags = programStackPopInteger(program);
    int map = programStackPopInteger(program);
    wmForceEncounter(map, flags);
}

// list_as_array
static void op_list_as_array(Program* program)
{
    int type = programStackPopInteger(program);
    int arrayId = ListAsArray(type);
    programStackPushInteger(program, arrayId);
}

// atoi
static void opParseInt(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, static_cast<int>(strtol(string, nullptr, 0)));
}

// atof
static void op_atof(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushFloat(program, static_cast<float>(atof(string)));
}

// tile_under_cursor
static void op_tile_under_cursor(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);

    int tile = tileFromScreenXY(x, y, gElevation);
    programStackPushInteger(program, tile);
}

// substr
static void opSubstr(Program* program)
{
    auto length = programStackPopInteger(program);
    auto startPos = programStackPopInteger(program);
    const char* str = programStackPopString(program);

    char buf[5120] = { 0 };

    int len = strlen(str);

    if (startPos < 0) {
        startPos += len; // start from end
        if (startPos < 0) {
            startPos = 0;
        }
    }

    if (length < 0) {
        length += len - startPos; // cutoff at end
        if (length == 0) {
            programStackPushString(program, buf);
            return;
        }
        length = abs(length); // length can't be negative
    }

    // check position
    if (startPos >= len) {
        // start position is out of string length, return empty string
        programStackPushString(program, buf);
        return;
    }

    if (length == 0 || length + startPos > len) {
        length = len - startPos; // set the correct length, the length of characters goes beyond the end of the string
    }

    if (length > sizeof(buf) - 1) {
        length = sizeof(buf) - 1;
    }

    memcpy(buf, &str[startPos], length);
    buf[length] = '\0';
    programStackPushString(program, buf);
}

// strlen
static void opGetStringLength(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, static_cast<int>(strlen(string)));
}

// metarule2_explosions
static void op_explosions_metarule(Program* program)
{
    int param2 = programStackPopInteger(program);
    int param1 = programStackPopInteger(program);
    int metarule = programStackPopInteger(program);

    switch (metarule) {
    case EXPL_FORCE_EXPLOSION_PATTERN:
        if (param1 != 0) {
            explosionSetPattern(2, 4);
        } else {
            explosionSetPattern(0, 6);
        }
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_ART:
        explosionSetFrm(param1);
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_RADIUS:
        explosionSetRadius(param1);
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_DMGTYPE:
        explosionSetDamageType(param1);
        programStackPushInteger(program, 0);
        break;
    case EXPL_STATIC_EXPLOSION_RADIUS:
        weaponSetGrenadeExplosionRadius(param1);
        weaponSetRocketExplosionRadius(param2);
        programStackPushInteger(program, 0);
        break;
    case EXPL_GET_EXPLOSION_DAMAGE:
        if (1) {
            int minDamage;
            int maxDamage;
            explosiveGetDamage(param1, &minDamage, &maxDamage);

            ArrayId arrayId = CreateTempArray(2, 0);
            SetArray(arrayId, ProgramValue { 0 }, ProgramValue { minDamage }, false, program);
            SetArray(arrayId, ProgramValue { 1 }, ProgramValue { maxDamage }, false, program);

            programStackPushInteger(program, arrayId);
        }
        break;
    case EXPL_SET_DYNAMITE_EXPLOSION_DAMAGE:
        explosiveSetDamage(PROTO_ID_DYNAMITE_I, param1, param2);
        break;
    case EXPL_SET_PLASTIC_EXPLOSION_DAMAGE:
        explosiveSetDamage(PROTO_ID_PLASTIC_EXPLOSIVES_I, param1, param2);
        break;
    case EXPL_SET_EXPLOSION_MAX_TARGET:
        explosionSetMaxTargets(param1);
        break;
    }
}

// pow (^)
static void op_power(Program* program)
{
    ProgramValue expValue = programStackPopValue(program);
    ProgramValue baseValue = programStackPopValue(program);

    // CE: Implementation is slightly different, check.
    float result = powf(baseValue.asFloat(), expValue.asFloat());

    if (baseValue.isInt() && expValue.isInt()) {
        programStackPushInteger(program, static_cast<int>(result));
    } else {
        programStackPushFloat(program, result);
    }
}

// message_str_game
static void opGetMessage(Program* program)
{
    int messageId = programStackPopInteger(program);
    int messageListId = programStackPopInteger(program);
    char* text = messageListRepositoryGetMsg(messageListId, messageId);
    programStackPushString(program, text);
}

// array_key
static void opGetArrayKey(Program* program)
{
    auto index = programStackPopInteger(program);
    auto arrayId = programStackPopInteger(program);
    auto value = GetArrayKey(arrayId, index, program);
    programStackPushValue(program, value);
}

// create_array
static void opCreateArray(Program* program)
{
    auto flags = programStackPopInteger(program);
    auto len = programStackPopInteger(program);
    auto arrayId = CreateArray(len, flags);
    programStackPushInteger(program, arrayId);
}

// temp_array
static void opTempArray(Program* program)
{
    auto flags = programStackPopInteger(program);
    auto len = programStackPopInteger(program);
    auto arrayId = CreateTempArray(len, flags);
    programStackPushInteger(program, arrayId);
}

// fix_array
static void opFixArray(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    FixArray(arrayId);
}

// string_split
static void opStringSplit(Program* program)
{
    auto split = programStackPopString(program);
    auto str = programStackPopString(program);
    auto arrayId = StringSplit(str, split);
    programStackPushInteger(program, arrayId);
}

// set_array
static void opSetArray(Program* program)
{
    auto value = programStackPopValue(program);
    auto key = programStackPopValue(program);
    auto arrayId = programStackPopInteger(program);
    SetArray(arrayId, key, value, true, program);
}

// arrayexpr
static void opStackArray(Program* program)
{
    auto value = programStackPopValue(program);
    auto key = programStackPopValue(program);
    auto returnValue = StackArray(key, value, program);
    programStackPushInteger(program, returnValue);
}

// scan_array
static void opScanArray(Program* program)
{
    auto value = programStackPopValue(program);
    auto arrayId = programStackPopInteger(program);
    auto returnValue = ScanArray(arrayId, value, program);
    programStackPushValue(program, returnValue);
}

// get_array
static void opGetArray(Program* program)
{
    auto key = programStackPopValue(program);
    auto arrayId = programStackPopValue(program);

    if (arrayId.isInt()) {
        auto value = GetArray(arrayId.integerValue, key, program);
        programStackPushValue(program, value);
    } else if (arrayId.isString() && key.isInt()) {
        auto pos = key.asInt();
        auto str = programGetString(program, arrayId.opcode, arrayId.integerValue);

        char buf[2] = { 0 };
        if (pos < strlen(str)) {
            buf[0] = str[pos];
            programStackPushString(program, buf);
        } else {
            programStackPushString(program, buf);
        }
    }
}

// free_array
static void opFreeArray(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    FreeArray(arrayId);
}

// len_array
static void opLenArray(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    programStackPushInteger(program, LenArray(arrayId));
}

// resize_array
static void opResizeArray(Program* program)
{
    auto newLen = programStackPopInteger(program);
    auto arrayId = programStackPopInteger(program);
    ResizeArray(arrayId, newLen);
}

// party_member_list
static void opPartyMemberList(Program* program)
{
    auto includeHidden = programStackPopInteger(program);
    auto objects = get_all_party_members_objects(includeHidden);
    auto arrayId = CreateTempArray(objects.size(), SFALL_ARRAYFLAG_RESERVED);
    for (int i = 0; i < LenArray(arrayId); i++) {
        SetArray(arrayId, ProgramValue { i }, ProgramValue { objects[i] }, false, program);
    }
    programStackPushInteger(program, arrayId);
}

// type_of
static void opTypeOf(Program* program)
{
    auto value = programStackPopValue(program);
    if (value.isInt()) {
        programStackPushInteger(program, 1);
    } else if (value.isFloat()) {
        programStackPushInteger(program, 2);
    } else {
        programStackPushInteger(program, 3);
    };
}

// round
static void opRound(Program* program)
{
    float floatValue = programStackPopFloat(program);
    int integerValue = static_cast<int>(floatValue);
    float mod = floatValue - static_cast<float>(integerValue);
    if (abs(mod) >= 0.5) {
        integerValue += mod > 0.0 ? 1 : -1;
    }
    programStackPushInteger(program, integerValue);
}

enum BlockType {
    BLOCKING_TYPE_BLOCK,
    BLOCKING_TYPE_SHOOT,
    BLOCKING_TYPE_AI,
    BLOCKING_TYPE_SIGHT,
    BLOCKING_TYPE_SCROLL,
};

PathBuilderCallback* get_blocking_func(int type)
{
    switch (type) {
    case BLOCKING_TYPE_SHOOT:
        return _obj_shoot_blocking_at;
    case BLOCKING_TYPE_AI:
        return _obj_ai_blocking_at;
    case BLOCKING_TYPE_SIGHT:
        return _obj_sight_blocking_at;
    default:
        return _obj_blocking_at;
    }
}

// obj_blocking_line
static void op_make_straight_path(Program* program)
{
    int type = programStackPopInteger(program);
    int dest = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int flags = type == BLOCKING_TYPE_SHOOT ? 32 : 0;

    Object* obstacle = nullptr;
    _make_straight_path_func(object, object->tile, dest, nullptr, &obstacle, flags, get_blocking_func(type));
    programStackPushPointer(program, obstacle);
}

// obj_blocking_tile
static void op_obj_blocking_at(Program* program)
{
    int type = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);

    PathBuilderCallback* func = get_blocking_func(type);
    Object* obstacle = func(nullptr, tile, elevation);
    if (obstacle != nullptr) {
        if (type == BLOCKING_TYPE_SHOOT) {
            if ((obstacle->flags & OBJECT_SHOOT_THRU) != 0) {
                obstacle = nullptr;
            }
        }
    }
    programStackPushPointer(program, obstacle);
}

// art_exists
static void opArtExists(Program* program)
{
    int fid = programStackPopInteger(program);
    programStackPushInteger(program, artExists(fid));
}

// sfall_func0
static void op_sfall_func0(Program* program)
{
    sfall_metarule(program, 0);
}

// sfall_func1
static void op_sfall_func1(Program* program)
{
    sfall_metarule(program, 1);
}

// sfall_func2
static void op_sfall_func2(Program* program)
{
    sfall_metarule(program, 2);
}

// sfall_func3
static void op_sfall_func3(Program* program)
{
    sfall_metarule(program, 3);
}

// sfall_func4
static void op_sfall_func4(Program* program)
{
    sfall_metarule(program, 4);
}

// sfall_func5
static void op_sfall_func5(Program* program)
{
    sfall_metarule(program, 5);
}

// sfall_func6
static void op_sfall_func6(Program* program)
{
    sfall_metarule(program, 6);
}

// div (/)
static void op_div(Program* program)
{
    ProgramValue divisorValue = programStackPopValue(program);
    ProgramValue dividendValue = programStackPopValue(program);

    if (divisorValue.integerValue == 0) {
        debugPrint("Division by zero");

        // TODO: Looks like execution is not halted in Sfall's div, check.
        programStackPushInteger(program, 0);
        return;
    }

    if (dividendValue.isFloat() || divisorValue.isFloat()) {
        programStackPushFloat(program, dividendValue.asFloat() / divisorValue.asFloat());
    } else {
        // Unsigned divison.
        programStackPushInteger(program, static_cast<unsigned int>(dividendValue.integerValue) / static_cast<unsigned int>(divisorValue.integerValue));
    }
}

void sfallOpcodesInit()
{
    interpreterRegisterOpcode(0x8156, opReadByte);
    interpreterRegisterOpcode(0x815A, op_set_pc_base_stat);
    interpreterRegisterOpcode(0x815B, opSetPcBonusStat);
    interpreterRegisterOpcode(0x815C, op_get_pc_base_stat);
    interpreterRegisterOpcode(0x815D, opGetPcBonusStat);
    interpreterRegisterOpcode(0x8162, op_tap_key);
    interpreterRegisterOpcode(0x8163, op_get_year);
    interpreterRegisterOpcode(0x8164, op_game_loaded);
    interpreterRegisterOpcode(0x816A, op_set_global_script_repeat);
    interpreterRegisterOpcode(0x816C, op_key_pressed);
    interpreterRegisterOpcode(0x8170, op_in_world_map);
    interpreterRegisterOpcode(0x8171, op_force_encounter);
    interpreterRegisterOpcode(0x8172, op_set_world_map_pos);
    interpreterRegisterOpcode(0x8193, opGetCurrentHand);
    interpreterRegisterOpcode(0x819B, op_set_global_script_type);
    interpreterRegisterOpcode(0x819D, opSetGlobalVar);
    interpreterRegisterOpcode(0x819E, opGetGlobalInt);
    interpreterRegisterOpcode(0x81AC, op_get_ini_setting);
    interpreterRegisterOpcode(0x81AF, opGetGameMode);
    interpreterRegisterOpcode(0x81B3, op_get_uptime);
    interpreterRegisterOpcode(0x81B6, op_set_car_current_town);
    interpreterRegisterOpcode(0x81DF, op_get_bodypart_hit_modifier);
    interpreterRegisterOpcode(0x81E0, op_set_bodypart_hit_modifier);
    interpreterRegisterOpcode(0x81EB, op_get_ini_string);
    interpreterRegisterOpcode(0x81EC, op_sqrt);
    interpreterRegisterOpcode(0x81ED, op_abs);
    interpreterRegisterOpcode(0x81F5, op_get_script);
    interpreterRegisterOpcode(0x8204, op_get_proto_data);
    interpreterRegisterOpcode(0x8205, op_set_proto_data);
    interpreterRegisterOpcode(0x8206, op_set_self);
    interpreterRegisterOpcode(0x820D, opListBegin);
    interpreterRegisterOpcode(0x820E, opListNext);
    interpreterRegisterOpcode(0x820F, opListEnd);
    interpreterRegisterOpcode(0x8210, opGetVersionMajor);
    interpreterRegisterOpcode(0x8211, opGetVersionMinor);
    interpreterRegisterOpcode(0x8212, opGetVersionPatch);
    interpreterRegisterOpcode(0x8217, opGetWeaponAmmoPid);
    interpreterRegisterOpcode(0x8218, opSetWeaponAmmoPid);
    interpreterRegisterOpcode(0x8219, opGetWeaponAmmoCount);
    interpreterRegisterOpcode(0x821A, opSetWeaponAmmoCount);
    interpreterRegisterOpcode(0x821C, opGetMouseX);
    interpreterRegisterOpcode(0x821D, opGetMouseY);
    interpreterRegisterOpcode(0x821E, op_get_mouse_buttons);
    interpreterRegisterOpcode(0x8220, opGetScreenWidth);
    interpreterRegisterOpcode(0x8221, opGetScreenHeight);
    interpreterRegisterOpcode(0x8224, op_create_message_window);
    interpreterRegisterOpcode(0x8228, op_get_attack_type);
    interpreterRegisterOpcode(0x8229, op_force_encounter_with_flags);
    interpreterRegisterOpcode(0x822D, opCreateArray);
    interpreterRegisterOpcode(0x822E, opSetArray);
    interpreterRegisterOpcode(0x822F, opGetArray);
    interpreterRegisterOpcode(0x8230, opFreeArray);
    interpreterRegisterOpcode(0x8231, opLenArray);
    interpreterRegisterOpcode(0x8232, opResizeArray);
    interpreterRegisterOpcode(0x8233, opTempArray);
    interpreterRegisterOpcode(0x8234, opFixArray);
    interpreterRegisterOpcode(0x8235, opStringSplit);
    interpreterRegisterOpcode(0x8236, op_list_as_array);
    interpreterRegisterOpcode(0x8237, opParseInt);
    interpreterRegisterOpcode(0x8238, op_atof);
    interpreterRegisterOpcode(0x8239, opScanArray);
    interpreterRegisterOpcode(0x824B, op_tile_under_cursor);
    interpreterRegisterOpcode(0x824E, opSubstr);
    interpreterRegisterOpcode(0x824F, opGetStringLength);
    interpreterRegisterOpcode(0x8253, opTypeOf);
    interpreterRegisterOpcode(0x8256, opGetArrayKey);
    interpreterRegisterOpcode(0x8257, opStackArray);
    interpreterRegisterOpcode(0x8261, op_explosions_metarule);
    interpreterRegisterOpcode(0x8263, op_power);
    interpreterRegisterOpcode(0x8267, opRound);
    interpreterRegisterOpcode(0x826B, opGetMessage);
    interpreterRegisterOpcode(0x826E, op_make_straight_path);
    interpreterRegisterOpcode(0x826F, op_obj_blocking_at);
    interpreterRegisterOpcode(0x8271, opPartyMemberList);
    interpreterRegisterOpcode(0x8274, opArtExists);
    interpreterRegisterOpcode(0x8276, op_sfall_func0);
    interpreterRegisterOpcode(0x8277, op_sfall_func1);
    interpreterRegisterOpcode(0x8278, op_sfall_func2);
    interpreterRegisterOpcode(0x8279, op_sfall_func3);
    interpreterRegisterOpcode(0x827A, op_sfall_func4);
    interpreterRegisterOpcode(0x827B, op_sfall_func5);
    interpreterRegisterOpcode(0x827C, op_sfall_func6);
    interpreterRegisterOpcode(0x827F, op_div);
}

void sfallOpcodesExit()
{
}

} // namespace fallout
