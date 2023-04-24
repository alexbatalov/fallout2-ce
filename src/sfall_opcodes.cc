#include "sfall_opcodes.h"

#include "animation.h"
#include "art.h"
#include "combat.h"
#include "debug.h"
#include "game.h"
#include "input.h"
#include "interface.h"
#include "interpreter.h"
#include "item.h"
#include "message.h"
#include "mouse.h"
#include "object.h"
#include "proto.h"
#include "scripts.h"
#include "sfall_global_vars.h"
#include "sfall_lists.h"
#include "stat.h"
#include "svga.h"
#include "tile.h"
#include "worldmap.h"

namespace fallout {

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

// get_year
static void op_get_year(Program* program)
{
    int year;
    gameTimeGetDate(nullptr, nullptr, &year);
    programStackPushInteger(program, year);
}

// in_world_map
static void op_in_world_map(Program* program)
{
    programStackPushInteger(program, GameMode::isInGameMode(GameMode::kWorldmap) ? 1 : 0);
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

// set_sfall_global
static void opSetGlobalVar(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    ProgramValue variable = programStackPopValue(program);

    if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* key = programGetString(program, variable.opcode, variable.integerValue);
        sfallGlobalVarsStore(key, value.integerValue);
    } else if (variable.opcode == VALUE_TYPE_INT) {
        sfallGlobalVarsStore(variable.integerValue, value.integerValue);
    }
}

// get_sfall_global_int
static void opGetGlobalInt(Program* program)
{
    ProgramValue variable = programStackPopValue(program);

    int value = 0;
    if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* key = programGetString(program, variable.opcode, variable.integerValue);
        sfallGlobalVarsFetch(key, value);
    } else if (variable.opcode == VALUE_TYPE_INT) {
        sfallGlobalVarsFetch(variable.integerValue, value);
    }

    programStackPushInteger(program, value);
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

// strlen
static void opGetStringLength(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, static_cast<int>(strlen(string)));
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
    Object* obstacle = func(NULL, tile, elevation);
    if (obstacle != NULL) {
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
    interpreterRegisterOpcode(0x8163, op_get_year);
    interpreterRegisterOpcode(0x8170, op_in_world_map);
    interpreterRegisterOpcode(0x8172, op_set_world_map_pos);
    interpreterRegisterOpcode(0x8193, opGetCurrentHand);
    interpreterRegisterOpcode(0x819D, opSetGlobalVar);
    interpreterRegisterOpcode(0x819E, opGetGlobalInt);
    interpreterRegisterOpcode(0x81AF, opGetGameMode);
    interpreterRegisterOpcode(0x81B3, op_get_uptime);
    interpreterRegisterOpcode(0x81B6, op_set_car_current_town);
    interpreterRegisterOpcode(0x81DF, op_get_bodypart_hit_modifier);
    interpreterRegisterOpcode(0x81E0, op_set_bodypart_hit_modifier);
    interpreterRegisterOpcode(0x81EC, op_sqrt);
    interpreterRegisterOpcode(0x81ED, op_abs);
    interpreterRegisterOpcode(0x8204, op_get_proto_data);
    interpreterRegisterOpcode(0x8205, op_set_proto_data);
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
    interpreterRegisterOpcode(0x8228, op_get_attack_type);
    interpreterRegisterOpcode(0x8237, opParseInt);
    interpreterRegisterOpcode(0x8238, op_atof);
    interpreterRegisterOpcode(0x824B, op_tile_under_cursor);
    interpreterRegisterOpcode(0x824F, opGetStringLength);
    interpreterRegisterOpcode(0x8263, op_power);
    interpreterRegisterOpcode(0x826B, opGetMessage);
    interpreterRegisterOpcode(0x8267, opRound);
    interpreterRegisterOpcode(0x826E, op_make_straight_path);
    interpreterRegisterOpcode(0x826F, op_obj_blocking_at);
    interpreterRegisterOpcode(0x8274, opArtExists);
    interpreterRegisterOpcode(0x827F, op_div);
}

void sfallOpcodesExit()
{
}

} // namespace fallout
