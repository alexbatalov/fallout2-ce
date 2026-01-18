#include "sfall_opcodes.h"

#include <math.h>
#include <string.h>

#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "critter.h"
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
static void op_read_byte(Program* program)
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

static void op_set_critter_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBaseStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    critterSetBaseStat(obj, stat, value);
}

// set_pc_extra_stat
static void op_set_pc_bonus_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBonusStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    critterSetBonusStat(gDude, stat, value);
}

static void op_set_critter_extra_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBonusStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    critterSetBonusStat(obj, stat, value);
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

static void op_get_critter_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall obtains value directly from
    // dude's proto. This can have unforeseen consequences when dealing with
    // current stats.
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    programStackPushInteger(program, critterGetBaseStat(obj, stat));
}

// get_pc_extra_stat
static void op_get_pc_bonus_stat(Program* program)
{
    int stat = programStackPopInteger(program);
    int value = critterGetBonusStat(gDude, stat);
    programStackPushInteger(program, value);
}

static void op_get_critter_extra_stat(Program* program)
{
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    int value = critterGetBonusStat(obj, stat);
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

// get_world_map_x_pos
static void op_get_world_map_x_pos(Program* program)
{
    int x;
    wmGetPartyWorldPos(&x, nullptr);
    programStackPushInteger(program, x);
}

// get_world_map_y_pos
static void op_get_world_map_y_pos(Program* program)
{
    int y;
    wmGetPartyWorldPos(nullptr, &y);
    programStackPushInteger(program, y);
}

// set_map_time_multi
void op_set_map_time_multi(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    wmSetScriptWorldMapMulti(value.asFloat());
}

// active_hand
static void op_active_hand(Program* program)
{
    programStackPushInteger(program, interfaceGetCurrentHand());
}

// toggle_active_hand
static void op_toggle_active_hand(Program* program)
{
    interfaceBarSwapHands(true);
}

// set_global_script_type
static void op_set_global_script_type(Program* program)
{
    int type = programStackPopInteger(program);
    sfall_gl_scr_set_type(program, type);
}

// set_sfall_global
static void op_set_sfall_global(Program* program)
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
static void op_get_sfall_global_int(Program* program)
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

// get_game_mode
static void op_get_game_mode(Program* program)
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

// sin
static void op_sin(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, sinf(programValue.asFloat()));
}

// cos
static void op_cos(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, cosf(programValue.asFloat()));
}

// tan
static void op_tan(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, tanf(programValue.asFloat()));
}

// arctan
static void op_arctan(Program* program)
{
    ProgramValue xValue = programStackPopValue(program);
    ProgramValue yValue = programStackPopValue(program);
    programStackPushFloat(program, atan2f(yValue.asFloat(), xValue.asFloat()));
}

// pow (^)
static void op_power(Program* program)
{
    ProgramValue expValue = programStackPopValue(program);
    ProgramValue baseValue = programStackPopValue(program);

    // CE: Implementation is slightly different, check.
    float result = powf(baseValue.asFloat(), expValue.asFloat());

    if (baseValue.isInt() && expValue.isInt()) {
        // Note: this will truncate the result if power is negative.  Keeping it to match sfall.
        programStackPushInteger(program, static_cast<int>(result));
    } else {
        programStackPushFloat(program, result);
    }
}

// log
static void op_log(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, logf(programValue.asFloat()));
}

// ceil
static void op_ceil(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushInteger(program, static_cast<int>(ceilf(programValue.asFloat())));
}

// exp
static void op_exponent(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, expf(programValue.asFloat()));
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
static void op_list_begin(Program* program)
{
    int listType = programStackPopInteger(program);
    int listId = sfallListsCreate(listType);
    programStackPushInteger(program, listId);
}

// list_next
static void op_list_next(Program* program)
{
    int listId = programStackPopInteger(program);
    Object* obj = sfallListsGetNext(listId);
    programStackPushPointer(program, obj);
}

// list_end
static void op_list_end(Program* program)
{
    int listId = programStackPopInteger(program);
    sfallListsDestroy(listId);
}

// sfall_ver_major
static void op_get_version_major(Program* program)
{
    programStackPushInteger(program, kVersionMajor);
}

// sfall_ver_minor
static void op_get_version_minor(Program* program)
{
    programStackPushInteger(program, kVersionMinor);
}

// sfall_ver_build
static void op_get_version_patch(Program* program)
{
    programStackPushInteger(program, kVersionPatch);
}

// get_weapon_ammo_pid
static void op_get_weapon_ammo_pid(Program* program)
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
static void op_set_weapon_ammo_pid(Program* program)
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
static void op_get_weapon_ammo_count(Program* program)
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
static void op_set_weapon_ammo_count(Program* program)
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
static void op_get_mouse_x(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);
    programStackPushInteger(program, x);
}

// get_mouse_y
static void op_get_mouse_y(Program* program)
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
static void op_get_screen_width(Program* program)
{
    programStackPushInteger(program, screenGetWidth());
}

// get_screen_height
static void op_get_screen_height(Program* program)
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
static void op_parse_int(Program* program)
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
static void op_substr(Program* program)
{
    auto length = programStackPopInteger(program);
    auto startPos = programStackPopInteger(program);
    const char* str = programStackPopString(program);

    char buf[5120] = { 0 };

    int len = static_cast<int>(strlen(str));

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
static void op_get_string_length(Program* program)
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

// message_str_game
static void op_get_message(Program* program)
{
    int messageId = programStackPopInteger(program);
    int messageListId = programStackPopInteger(program);
    char* text = messageListRepositoryGetMsg(messageListId, messageId);
    programStackPushString(program, text);
}

// array_key
static void op_get_array_key(Program* program)
{
    auto index = programStackPopInteger(program);
    auto arrayId = programStackPopInteger(program);
    auto value = GetArrayKey(arrayId, index, program);
    programStackPushValue(program, value);
}

// create_array
static void op_create_array(Program* program)
{
    auto flags = programStackPopInteger(program);
    auto len = programStackPopInteger(program);
    auto arrayId = CreateArray(len, flags);
    programStackPushInteger(program, arrayId);
}

// temp_array
static void op_temp_array(Program* program)
{
    auto flags = programStackPopInteger(program);
    auto len = programStackPopInteger(program);
    auto arrayId = CreateTempArray(len, flags);
    programStackPushInteger(program, arrayId);
}

// fix_array
static void op_fix_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    FixArray(arrayId);
}

// string_split
static void op_string_split(Program* program)
{
    auto split = programStackPopString(program);
    auto str = programStackPopString(program);
    auto arrayId = StringSplit(str, split);
    programStackPushInteger(program, arrayId);
}

// set_array
static void op_set_array(Program* program)
{
    auto value = programStackPopValue(program);
    auto key = programStackPopValue(program);
    auto arrayId = programStackPopInteger(program);
    SetArray(arrayId, key, value, true, program);
}

// arrayexpr
static void op_stack_array(Program* program)
{
    auto value = programStackPopValue(program);
    auto key = programStackPopValue(program);
    auto returnValue = StackArray(key, value, program);
    programStackPushInteger(program, returnValue);
}

// scan_array
static void op_scan_array(Program* program)
{
    auto value = programStackPopValue(program);
    auto arrayId = programStackPopInteger(program);
    auto returnValue = ScanArray(arrayId, value, program);
    programStackPushValue(program, returnValue);
}

// get_array
static void op_get_array(Program* program)
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
static void op_free_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    FreeArray(arrayId);
}

// len_array
static void op_len_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    programStackPushInteger(program, LenArray(arrayId));
}

// resize_array
static void op_resize_array(Program* program)
{
    auto newLen = programStackPopInteger(program);
    auto arrayId = programStackPopInteger(program);
    ResizeArray(arrayId, newLen);
}

// party_member_list
static void op_party_member_list(Program* program)
{
    auto includeHidden = programStackPopInteger(program);
    auto objects = get_all_party_members_objects(includeHidden);
    auto arrayId = CreateTempArray(static_cast<int>(objects.size()), SFALL_ARRAYFLAG_RESERVED);
    for (int i = 0; i < LenArray(arrayId); i++) {
        SetArray(arrayId, ProgramValue { i }, ProgramValue { objects[i] }, false, program);
    }
    programStackPushInteger(program, arrayId);
}

// type_of
static void op_type_of(Program* program)
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
static void op_round(Program* program)
{
    float floatValue = programStackPopValue(program).asFloat();
    programStackPushInteger(program, static_cast<int>(lroundf(floatValue)));
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
static void op_art_exists(Program* program)
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

// sfall_func6
static void op_sfall_func7(Program* program)
{
    sfall_metarule(program, 7);
}

// sfall_func6
static void op_sfall_func8(Program* program)
{
    sfall_metarule(program, 8);
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

static void op_sprintf(Program* program)
{
    auto arg1 = programStackPopValue(program);
    auto arg2 = programStackPopString(program);
    programStackPushValue(program, arg1);
    programStackPushString(program, arg2);
    sprintf_lite(program, 2, "op_sprintf");
}

static void op_charcode(Program* program)
{
    const char* str = programStackPopString(program);
    if (str != nullptr && str[0] != '\0') {
        programStackPushInteger(program, static_cast<int>(str[0]));
    } else {
        programStackPushInteger(program, 0);
    }
}

static void op_show_iface_tag(fallout::Program* program)
{
    int tag = fallout::programStackPopInteger(program);

    switch (tag) {
    case DudeState::DUDE_STATE_SNEAKING:
    case DudeState::DUDE_STATE_LEVEL_UP_AVAILABLE:
    case DudeState::DUDE_STATE_ADDICTED:
        dudeEnableState(tag);
        break;
    default:
        debugPrint("op_show_iface_tag: custom tag %d is not handled", tag);
    }
}

static void op_hide_iface_tag(fallout::Program* program)
{
    int tag = fallout::programStackPopInteger(program);

    switch (tag) {
    case DudeState::DUDE_STATE_SNEAKING:
    case DudeState::DUDE_STATE_LEVEL_UP_AVAILABLE:
    case DudeState::DUDE_STATE_ADDICTED:
        dudeDisableState(tag);
        break;
    default:
        debugPrint("op_hide_iface_tag: custom tag %d is not handled", tag);
    }
}

static void op_is_iface_tag_active(fallout::Program* program)
{
    int tag = fallout::programStackPopInteger(program);
    bool isActive = false;

    switch (tag) {
    case DudeState::DUDE_STATE_SNEAKING:
    case DudeState::DUDE_STATE_LEVEL_UP_AVAILABLE:
    case DudeState::DUDE_STATE_ADDICTED:
        isActive = fallout::dudeHasState(tag);
        break;
    case 1: // POISONED
        isActive = critterGetPoison(gDude) > POISON_INDICATOR_THRESHOLD;
        break;
    case 2: // RADIATED
        isActive = critterGetRadiation(gDude) > RADATION_INDICATOR_THRESHOLD;
        break;
    default:
        debugPrint("op_is_iface_tag_active: custom tag %d is not handled", tag);
    }

    fallout::programStackPushInteger(program, isActive ? 1 : 0);
}

// Note: opcodes should pop arguments off the stack in reverse order
void sfallOpcodesInit()
{
    // ref. https://github.com/sfall-team/sfall/blob/71ecec3d405bd5e945f157954618b169e60068fe/artifacts/scripting/sfall%20opcode%20list.txt#L145
    // Note: we can't really implement these since address space is different.
    // We can potentially special case some of them, but we should try to avoid that.
    // 0x8156 - int   read_byte(int address)
    interpreterRegisterOpcode(0x8156, op_read_byte);
    // 0x8157 - int   read_short(int address)
    // 0x8158 - int   read_int(int address)
    // 0x8159 - string read_string(int address)

    // ^ 0x81cf - void  write_byte(int address, int value)
    // ^ 0x81d0 - void  write_short(int address, int value)
    // ^ 0x81d1 - void  write_int(int address, int value)
    // ^ 0x821b - void  write_string(int address, string value)

    // ^ 0x81d2 - void  call_offset_v0(int address)
    // ^ 0x81d3 - void  call_offset_v1(int address, int arg1)
    // ^ 0x81d4 - void  call_offset_v2(int address, int arg1, int arg2)
    // ^ 0x81d5 - void  call_offset_v3(int address, int arg1, int arg2, int arg3)
    // ^ 0x81d6 - void  call_offset_v4(int address, int arg1, int arg2, int arg3, int arg4)
    // ^ 0x81d7 - int   call_offset_r0(int address)
    // ^ 0x81d8 - int   call_offset_r1(int address, int arg1)
    // ^ 0x81d9 - int   call_offset_r2(int address, int arg1, int arg2)
    // ^ 0x81da - int   call_offset_r3(int address, int arg1, int arg2, int arg3)
    // ^ 0x81db - int   call_offset_r4(int address, int arg1, int arg2, int arg3, int arg4)

    // 0x815a - void set_pc_base_stat(int StatID, int value)
    interpreterRegisterOpcode(0x815A, op_set_pc_base_stat);
    // 0x815b - void set_pc_extra_stat(int StatID, int value)
    interpreterRegisterOpcode(0x815B, op_set_pc_bonus_stat);
    // 0x815c - int  get_pc_base_stat(int StatID)
    interpreterRegisterOpcode(0x815C, op_get_pc_base_stat);
    // 0x815d - int  get_pc_extra_stat(int StatID)
    interpreterRegisterOpcode(0x815D, op_get_pc_bonus_stat);

    // 0x815e - void set_critter_base_stat(object, int StatID, int value)
    interpreterRegisterOpcode(0x815E, op_set_critter_base_stat);
    // 0x815f - void set_critter_extra_stat(object, int StatID, int value)
    interpreterRegisterOpcode(0x815F, op_set_critter_extra_stat);
    // 0x8160 - int  get_critter_base_stat(object, int StatID)
    interpreterRegisterOpcode(0x8160, op_get_critter_base_stat);
    // 0x8161 - int  get_critter_extra_stat(object, int StatID)
    interpreterRegisterOpcode(0x8161, op_get_critter_extra_stat);
    // 0x8242 - void set_critter_skill_points(int critter, int skill, int value)
    // 0x8243 - int  get_critter_skill_points(int critter, int skill)
    // 0x8244 - void set_available_skill_points(int value)
    // 0x8245 - int  get_available_skill_points()
    // 0x8246 - void mod_skill_points_per_level(int value)

    // 0x81b4 - void set_stat_max(int stat, int value)
    // 0x81b5 - void set_stat_min(int stat, int value)
    // 0x81b7 - void set_pc_stat_max(int stat, int value)
    // 0x81b8 - void set_pc_stat_min(int stat, int value)
    // 0x81b9 - void set_npc_stat_max(int stat, int value)
    // 0x81ba - void set_npc_stat_min(int stat, int value)

    // 0x816b - int  input_funcs_available() // deprecated; do not implement
    // 0x816c - int  key_pressed(int dxScancode)
    interpreterRegisterOpcode(0x816C, op_key_pressed);
    // 0x8162 - void tap_key(int dxScancode)
    interpreterRegisterOpcode(0x8162, op_tap_key);
    // 0x821c - int  get_mouse_x()
    interpreterRegisterOpcode(0x821C, op_get_mouse_x);
    // 0x821d - int  get_mouse_y()
    interpreterRegisterOpcode(0x821D, op_get_mouse_y);
    // 0x821e - int  get_mouse_buttons()
    interpreterRegisterOpcode(0x821E, op_get_mouse_buttons);
    // 0x821f - int  get_window_under_mouse()

    // 0x8163 - int get_year()
    interpreterRegisterOpcode(0x8163, op_get_year);

    // 0x8164 - bool game_loaded()
    interpreterRegisterOpcode(0x8164, op_game_loaded);

    // 0x8165 - bool graphics_funcs_available()
    // 0x8166 - int  load_shader(string path)
    // 0x8167 - void free_shader(int ID)
    // 0x8168 - void activate_shader(int ID)
    // 0x8169 - void deactivate_shader(int ID)
    // 0x816d - void set_shader_int(int ID, string param, int value)
    // 0x816e - void set_shader_float(int ID, string param, float value)
    // 0x816f - void set_shader_vector(int ID, string param, float f1, float f2, float f3, float f4)
    // 0x81ad - int get_shader_version()
    // 0x81ae - void set_shader_mode(int mode)
    // 0x81b0 - void force_graphics_refresh(bool enabled)
    // 0x81b1 - int get_shader_texture(int ID, int texture)
    // 0x81b2 - void set_shader_texture(int ID, string param, int texID)

    // 0x816a - void set_global_script_repeat(int frames)
    interpreterRegisterOpcode(0x816A, op_set_global_script_repeat);
    // 0x819b - void set_global_script_type(int type)
    interpreterRegisterOpcode(0x819B, op_set_global_script_type);
    // 0x819c - int available_global_script_types()

    // 0x8170 - bool in_world_map()
    interpreterRegisterOpcode(0x8170, op_in_world_map);

    // 0x8171 - void force_encounter(int map)
    interpreterRegisterOpcode(0x8171, op_force_encounter);
    // 0x8229 - void force_encounter_with_flags(int map, int flags)
    interpreterRegisterOpcode(0x8229, op_force_encounter_with_flags);
    // 0x822a - void set_map_time_multi(float multi)
    interpreterRegisterOpcode(0x822A, op_set_map_time_multi);

    // 0x8172 - void set_world_map_pos(int x, int y)
    interpreterRegisterOpcode(0x8172, op_set_world_map_pos);
    // 0x8173 - int get_world_map_x_pos()
    interpreterRegisterOpcode(0x8173, op_get_world_map_x_pos);
    // 0x8174 - int get_world_map_y_pos()
    interpreterRegisterOpcode(0x8174, op_get_world_map_y_pos);

    // 0x8175 - void set_dm_model(string name)
    // 0x8176 - void set_df_model(string name)
    // 0x8177 - void set_movie_path(string filename, int movieid)

    // 0x8178 - void set_perk_image(int perkID, int value)
    // 0x8179 - void set_perk_ranks(int perkID, int value)
    // 0x817a - void set_perk_level(int perkID, int value)
    // 0x817b - void set_perk_stat(int perkID, int value)
    // 0x817c - void set_perk_stat_mag(int perkID, int value)
    // 0x817d - void set_perk_skill1(int perkID, int value)
    // 0x817e - void set_perk_skill1_mag(int perkID, int value)
    // 0x817f - void set_perk_type(int perkID, int value)
    // 0x8180 - void set_perk_skill2(int perkID, int value)
    // 0x8181 - void set_perk_skill2_mag(int perkID, int value)
    // 0x8182 - void set_perk_str(int perkID, int value)
    // 0x8183 - void set_perk_per(int perkID, int value)
    // 0x8184 - void set_perk_end(int perkID, int value)
    // 0x8185 - void set_perk_chr(int perkID, int value)
    // 0x8196 - void set_perk_int(int perkID, int value)
    // 0x8187 - void set_perk_agl(int perkID, int value)
    // 0x8188 - void set_perk_lck(int perkID, int value)
    // 0x8189 - void set_perk_name(int perkID, string value)
    // 0x818a - void set_perk_desc(int perkID, string value)
    // 0x8247 - void set_perk_freq(int value)

    // 0x818b - void set_pipboy_available(int available)

    // 0x818c - int get_kill_counter(int critterType)
    // 0x818d - void mod_kill_counter(int critterType, int amount)

    // 0x818e - int get_perk_owed()
    // 0x818f - void set_perk_owed(int value)
    // 0x8190 - int get_perk_available(int perk)

    // 0x8191 - int get_critter_current_ap(object critter)
    // 0x8192 - void set_critter_current_ap(object critter, int ap)

    // 0x8193 - int  active_hand()
    interpreterRegisterOpcode(0x8193, op_active_hand);
    // 0x8194 - void toggle_active_hand()
    interpreterRegisterOpcode(0x8194, op_toggle_active_hand);

    // 0x8195 - void set_weapon_knockback(object weapon, int type, int/float value)
    // 0x8196 - void set_target_knockback(object critter, int type, int/float value)
    // 0x8197 - void set_attacker_knockback(object critter, int type, int/float value)
    // 0x8198 - void remove_weapon_knockback(object weapon)
    // 0x8199 - void remove_target_knockback(object critter)
    // 0x819a - void remove_attacker_knockback(object critter)

    // 0x819d - void  set_sfall_global(string/int varname, int/float value)
    interpreterRegisterOpcode(0x819D, op_set_sfall_global);
    // 0x819e - int   get_sfall_global_int(string/int varname)
    interpreterRegisterOpcode(0x819E, op_get_sfall_global_int);
    // 0x819f - float get_sfall_global_float(string/int varname)
    // 0x822d - int   create_array(int element_count, int flags)
    interpreterRegisterOpcode(0x822D, op_create_array);
    // 0x822e - void  set_array(int array, any element, any value)
    interpreterRegisterOpcode(0x822E, op_set_array);
    // 0x822f - any   get_array(int array, any element)
    interpreterRegisterOpcode(0x822F, op_get_array);
    // 0x8230 - void  free_array(int array)
    interpreterRegisterOpcode(0x8230, op_free_array);
    // 0x8231 - int   len_array(int array)
    interpreterRegisterOpcode(0x8231, op_len_array);
    // 0x8232 - void  resize_array(int array, int new_element_count)
    interpreterRegisterOpcode(0x8232, op_resize_array);
    // 0x8233 - int   temp_array(int element_count, int flags)
    interpreterRegisterOpcode(0x8233, op_temp_array);
    // 0x8234 - void  fix_array(int array)
    interpreterRegisterOpcode(0x8234, op_fix_array);
    // 0x8239 - int   scan_array(int array, int/float var)
    interpreterRegisterOpcode(0x8239, op_scan_array);
    // 0x8254 - void  save_array(any key, int array)
    // 0x8255 - int   load_array(any key)
    // 0x8256 - int   array_key(int array, int index)
    interpreterRegisterOpcode(0x8256, op_get_array_key);
    // 0x8257 - int   arrayexpr(any key, any value)
    interpreterRegisterOpcode(0x8257, op_stack_array);

    // 0x81a0 - void set_pickpocket_max(int percentage)
    // 0x81a1 - void set_hit_chance_max(int percentage)
    // 0x81a2 - void set_skill_max(int value)
    // 0x81aa - void set_xp_mod(int percentage)
    // 0x81ab - void set_perk_level_mod(int levels)

    // 0x81c5 - void set_critter_hit_chance_mod(object, int max, int mod)
    // 0x81c6 - void set_base_hit_chance_mod(int max, int mod)
    // 0x81c7 - void set_critter_skill_mod(object, int max)
    // 0x81c8 - void set_base_skill_mod(int max)
    // 0x81c9 - void set_critter_pickpocket_mod(object, int max, int mod)
    // 0x81ca - void set_base_pickpocket_mod(int max, int mod)

    // note: these are deprecated; do not implement
    // 0x81a3 - int  eax_available()
    // 0x81a4 - void set_eax_environment(int environment)

    // 0x81a5 - void inc_npc_level(int pid/string name)
    // 0x8241 - int  get_npc_level(int pid/string name)

    // 0x81a6 - int get_viewport_x()
    // 0x81a7 - int get_viewport_y()
    // 0x81a8 - void set_viewport_x(int view_x)
    // 0x81a9 - void set_viewport_y(int view_y)

    // 0x81ac - int   get_ini_setting(string setting)
    interpreterRegisterOpcode(0x81AC, op_get_ini_setting);
    // 0x81eb - string get_ini_string(string setting)
    interpreterRegisterOpcode(0x81EB, op_get_ini_string);

    // 0x81af - int get_game_mode()
    interpreterRegisterOpcode(0x81AF, op_get_game_mode);

    // 0x81b3 - int get_uptime()
    interpreterRegisterOpcode(0x81B3, op_get_uptime);

    // 0x81b6 - void set_car_current_town(int town)
    interpreterRegisterOpcode(0x81B6, op_set_car_current_town);

    // 0x81bb - void set_fake_perk(string name, int level, int image, string desc)
    // 0x81bc - void set_fake_trait(string name, int active, int image, string desc)
    // 0x81bd - void set_selectable_perk(string name, int active, int image, string desc)
    // 0x81be - void set_perkbox_title(string title)
    // 0x81bf - void hide_real_perks()
    // 0x81c0 - void show_real_perks()
    // 0x81c1 - int has_fake_perk(string name/int extraPerkID)
    // 0x81c2 - int has_fake_trait(string name)
    // 0x81c3 - void perk_add_mode(int type)
    // 0x81c4 - void clear_selectable_perks()
    // 0x8225 - void remove_trait(int traitID)

    // 0x81cb - void set_pyromaniac_mod(int bonus)
    // 0x81cc - void apply_heaveho_fix()
    // 0x81cd - void set_swiftlearner_mod(int bonus)
    // 0x81ce - void set_hp_per_level_mod(int mod)

    // 0x81dc - void show_iface_tag(int tag)
    interpreterRegisterOpcode(0x81DC, op_show_iface_tag);
    // 0x81dd - void hide_iface_tag(int tag)
    interpreterRegisterOpcode(0x81DD, op_hide_iface_tag);
    // 0x81de - int  is_iface_tag_active(int tag)
    interpreterRegisterOpcode(0x81DE, op_is_iface_tag_active);

    // 0x81df - int  get_bodypart_hit_modifier(int bodypart)
    interpreterRegisterOpcode(0x81DF, op_get_bodypart_hit_modifier);
    // 0x81e0 - void set_bodypart_hit_modifier(int bodypart, int value)
    interpreterRegisterOpcode(0x81E0, op_set_bodypart_hit_modifier);

    // 0x81e1 - void set_critical_table(int crittertype, int bodypart, int level, int valuetype, int value)
    // 0x81e2 - int  get_critical_table(int crittertype, int bodypart, int level, int valuetype)
    // 0x81e3 - void reset_critical_table(int crittertype, int bodypart, int level, int valuetype)

    // 0x81e4 - int   get_sfall_arg()
    // 0x823c - array get_sfall_args()
    // 0x823d - void  set_sfall_arg(int argnum, int value)
    // 0x81e5 - void  set_sfall_return(any value)
    // 0x81ea - int   init_hook()

    // 0x81e6 - void set_unspent_ap_bonus(int multiplier)
    // 0x81e7 - int  get_unspent_ap_bonus()
    // 0x81e8 - void set_unspent_ap_perk_bonus(int multiplier)
    // 0x81e9 - int  get_unspent_ap_perk_bonus()

    // 0x81ec - float sqrt(float)
    interpreterRegisterOpcode(0x81EC, op_sqrt);
    // 0x81ed - int/float abs(int/float)
    interpreterRegisterOpcode(0x81ED, op_abs);
    // 0x81ee - float sin(float)
    interpreterRegisterOpcode(0x81EE, op_sin);
    // 0x81ef - float cos(float)
    interpreterRegisterOpcode(0x81EF, op_cos);
    // 0x81f0 - float tan(float)
    interpreterRegisterOpcode(0x81F0, op_tan);
    // 0x81f1 - float arctan(float x, float y)
    interpreterRegisterOpcode(0x81F1, op_arctan);
    // 0x8263 - ^ operator (exponentiation)
    interpreterRegisterOpcode(0x8263, op_power);
    // 0x8264 - float log(float)
    interpreterRegisterOpcode(0x8264, op_log);
    // 0x8265 - float exponent(float)
    interpreterRegisterOpcode(0x8265, op_exponent);
    // 0x8266 - int ceil(float)
    interpreterRegisterOpcode(0x8266, op_ceil);
    // 0x8267 - int round(float)
    interpreterRegisterOpcode(0x8267, op_round);
    // 0x827f - div operator (unsigned integer division)
    interpreterRegisterOpcode(0x827F, op_div);

    // 0x81f2 - void set_palette(string path)

    // 0x81f3 - void remove_script(object)
    // 0x81f4 - void set_script(object, int scriptid)
    // 0x81f5 - int get_script(object)
    interpreterRegisterOpcode(0x81F5, op_get_script);

    // 0x81f6 - int nb_create_char() // deprecated; do not implement

    // 0x81f7 - int   fs_create(string path, int size)
    // 0x81f8 - int   fs_copy(string path, string source)
    // 0x81f9 - int   fs_find(string path)
    // 0x81fa - void  fs_write_byte(int id, int data)
    // 0x81fb - void  fs_write_short(int id, int data)
    // 0x81fc - void  fs_write_int(int id, int data)
    // 0x81fd - void  fs_write_float(int id, int data)
    // 0x81fe - void  fs_write_string(int id, string data)
    // 0x8208 - void  fs_write_bstring(int id, string data)
    // 0x8209 - int   fs_read_byte(int id)
    // 0x820a - int   fs_read_short(int id)
    // 0x820b - int   fs_read_int(int id)
    // 0x820c - float fs_read_float(int id)
    // 0x81ff - void  fs_delete(int id)
    // 0x8200 - int   fs_size(int id)
    // 0x8201 - int   fs_pos(int id)
    // 0x8202 - void  fs_seek(int id, int pos)
    // 0x8203 - void  fs_resize(int id, int size)

    // 0x8204 - int  get_proto_data(int pid, int offset)
    interpreterRegisterOpcode(0x8204, op_get_proto_data);
    // 0x8205 - void set_proto_data(int pid, int offset, int value)
    interpreterRegisterOpcode(0x8205, op_set_proto_data);

    // 0x8206 - void set_self(object)
    interpreterRegisterOpcode(0x8206, op_set_self);
    // 0x8207 - void register_hook(int hook)

    // 0x820d - int   list_begin(int type)
    interpreterRegisterOpcode(0x820D, op_list_begin);
    // 0x820e - int   list_next(int listid)
    interpreterRegisterOpcode(0x820E, op_list_next);
    // 0x820f - void  list_end(int listid)
    interpreterRegisterOpcode(0x820F, op_list_end);
    // 0x8236 - array list_as_array(int type)
    interpreterRegisterOpcode(0x8236, op_list_as_array);

    // 0x8210 - int sfall_ver_major()
    interpreterRegisterOpcode(0x8210, op_get_version_major);
    // 0x8211 - int sfall_ver_minor()
    interpreterRegisterOpcode(0x8211, op_get_version_minor);
    // 0x8212 - int sfall_ver_build()
    interpreterRegisterOpcode(0x8212, op_get_version_patch);

    // 0x8213 - void hero_select_win(int)
    // 0x8214 - void set_hero_race(int style)
    // 0x8215 - void set_hero_style(int style)

    // 0x8216 - void set_critter_burst_disable(object critter, int disable)

    // 0x8217 - int  get_weapon_ammo_pid(object weapon)
    interpreterRegisterOpcode(0x8217, op_get_weapon_ammo_pid);
    // 0x8218 - void set_weapon_ammo_pid(object weapon, int pid)
    interpreterRegisterOpcode(0x8218, op_set_weapon_ammo_pid);
    // 0x8219 - int  get_weapon_ammo_count(object weapon)
    interpreterRegisterOpcode(0x8219, op_get_weapon_ammo_count);
    // 0x821a - void set_weapon_ammo_count(object weapon, int count)
    interpreterRegisterOpcode(0x821A, op_set_weapon_ammo_count);

    // 0x8220 - int get_screen_width()
    interpreterRegisterOpcode(0x8220, op_get_screen_width);
    // 0x8221 - int get_screen_height()
    interpreterRegisterOpcode(0x8221, op_get_screen_height);

    // 0x8222 - void stop_game()
    // 0x8223 - void resume_game()
    // 0x8224 - void create_message_window(string message)
    interpreterRegisterOpcode(0x8224, op_create_message_window);

    // 0x8226 - int get_light_level()

    // 0x8227 - void refresh_pc_art()

    // 0x8228 - int get_attack_type()
    interpreterRegisterOpcode(0x8228, op_get_attack_type);

    // 0x822b - int  play_sfall_sound(string file, int mode)
    // 0x822c - void stop_sfall_sound(int soundID)

    // 0x8235 - array string_split(string string, string split)
    interpreterRegisterOpcode(0x8235, op_string_split);
    // 0x8237 - int   atoi(string string)
    interpreterRegisterOpcode(0x8237, op_parse_int);
    // 0x8238 - float atof(string string)
    interpreterRegisterOpcode(0x8238, op_atof);
    // 0x824e - string substr(string string, int start, int length)
    interpreterRegisterOpcode(0x824E, op_substr);
    // 0x824f - int   strlen(string string)
    interpreterRegisterOpcode(0x824F, op_get_string_length);
    // 0x8250 - string sprintf(string format, any value)
    interpreterRegisterOpcode(0x8250, op_sprintf);
    // 0x8251 - int   charcode(string string)
    interpreterRegisterOpcode(0x8251, op_charcode);
    // 0x8253 - int   typeof(any value)
    interpreterRegisterOpcode(0x8253, op_type_of);

    // 0x823a - int get_tile_fid(int tileData)

    // 0x823b - int modified_ini() // deprecated: do not implement

    // 0x823e - void force_aimed_shots(int pid)
    // 0x823f - void disable_aimed_shots(int pid)

    // 0x8240 - void mark_movie_played(int id)

    // 0x8248 - object get_last_target(object critter)
    // 0x8249 - object get_last_attacker(object critter)
    // 0x824a - void block_combat(int enable)

    // 0x824b - int tile_under_cursor()
    interpreterRegisterOpcode(0x824B, op_tile_under_cursor);
    // 0x824c - int gdialog_get_barter_mod()
    // 0x824d - void set_inven_ap_cost(int cost)

    // 0x825c - void reg_anim_combat_check(int enable)
    // 0x825a - void reg_anim_destroy(object object)
    // 0x825b - void reg_anim_animate_and_hide(object object, int animID, int delay)
    // 0x825d - void reg_anim_light(object object, int radius, int delay)
    // 0x825e - void reg_anim_change_fid(object object, int FID, int delay)
    // 0x825f - void reg_anim_take_out(object object, int holdFrameID, int delay)
    // 0x8260 - void reg_anim_turn_towards(object object, int tile/targetObj, int delay)

    // 0x8261 - int metarule2_explosions(object object)
    interpreterRegisterOpcode(0x8261, op_explosions_metarule);

    // 0x8262 - void register_hook_proc(int hook, procedure proc)

    // 0x826b - string message_str_game(int fileId, int messageId)
    interpreterRegisterOpcode(0x826B, op_get_message);
    // 0x826c - int sneak_success()
    // 0x826d - int tile_light(int elevation, int tileNum)
    // 0x826e - object obj_blocking_line(object objFrom, int tileTo, int blockingType)
    interpreterRegisterOpcode(0x826E, op_make_straight_path);
    // 0x826f - object obj_blocking_tile(int tileNum, int elevation, int blockingType)
    interpreterRegisterOpcode(0x826F, op_obj_blocking_at);
    // 0x8270 - array tile_get_objs(int tileNum, int elevation)
    // 0x8271 - array party_member_list(int includeHidden)
    interpreterRegisterOpcode(0x8271, op_party_member_list);
    // 0x8272 - array path_find_to(object objFrom, int tileTo, int blockingType)
    // 0x8273 - object create_spatial(int scriptID, int tile, int elevation, int radius)
    // 0x8274 - int art_exists(int artFID)
    interpreterRegisterOpcode(0x8274, op_art_exists);
    // 0x8275 - int obj_is_carrying_obj(object invenObj, object itemObj)

    // 0x8276 - any sfall_func0(string funcName)
    interpreterRegisterOpcode(0x8276, op_sfall_func0);
    // 0x8277 - any sfall_func1(string funcName, arg1)
    interpreterRegisterOpcode(0x8277, op_sfall_func1);
    // 0x8278 - any sfall_func2(string funcName, arg1, arg2)
    interpreterRegisterOpcode(0x8278, op_sfall_func2);
    // 0x8279 - any sfall_func3(string funcName, arg1, arg2, arg3)
    interpreterRegisterOpcode(0x8279, op_sfall_func3);
    // 0x827a - any sfall_func4(string funcName, arg1, arg2, arg3, arg4)
    interpreterRegisterOpcode(0x827A, op_sfall_func4);
    // 0x827b - any sfall_func5(string funcName, arg1, arg2, arg3, arg4, arg5)
    interpreterRegisterOpcode(0x827B, op_sfall_func5);
    // 0x827c - any sfall_func6(string funcName, arg1, arg2, arg3, arg4, arg5, arg6)
    interpreterRegisterOpcode(0x827C, op_sfall_func6);
    // 0x8280 - any sfall_func7(string funcName, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
    interpreterRegisterOpcode(0x8280, op_sfall_func7);
    // 0x8281 - any sfall_func8(string funcName, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
    interpreterRegisterOpcode(0x8281, op_sfall_func8);

    // 0x827d - void register_hook_proc_spec(int hook, procedure proc)
    // 0x827e - void reg_anim_callback(procedure proc)
}

void sfallOpcodesExit()
{
}

} // namespace fallout
