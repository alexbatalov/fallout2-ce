#include "sfall_metarules.h"

#include <algorithm>
#include <math.h>
#include <memory>
#include <string.h>
#include <string>

#include "color.h"
#include "combat.h"
#include "config.h" // For Config, configInit, configFree
#include "dbox.h"
#include "debug.h"
#include "game.h"
#include "game_dialog.h"
#include "game_mouse.h"
#include "interface.h"
#include "interpreter.h"
#include "inventory.h"
#include "memory.h"
#include "object.h"
#include "platform_compat.h"
#include "scripts.h"
#include "sfall_arrays.h" // For CreateTempArray, SetArray
#include "sfall_ini.h"
#include "text_font.h"
#include "tile.h"
#include "window.h"
#include "worldmap.h"

namespace fallout {

static void mf_car_gas_amount(Program* program, int args);
static void mf_combat_data(Program* program, int args);
static void mf_critter_inven_obj2(Program* program, int args);
static void mf_dialog_obj(Program* program, int args);
static void mf_get_cursor_mode(Program* program, int args);
static void mf_get_flags(Program* program, int args);
static void mf_get_object_data(Program* program, int args);
static void mf_get_text_width(Program* program, int args);
static void mf_intface_redraw(Program* program, int args);
static void mf_loot_obj(Program* program, int args);
static void mf_message_box(Program* program, int args);
static void mf_metarule_exist(Program* program, int args);
static void mf_opcode_exists(Program* program, int args);
static void mf_outlined_object(Program* program, int args);
static void mf_set_cursor_mode(Program* program, int args);
static void mf_set_flags(Program* program, int args);
static void mf_set_outline(Program* program, int args);
static void mf_show_window(Program* program, int args);
static void mf_tile_refresh_display(Program* program, int args);
static void mf_string_compare(Program* program, int args);
static void mf_string_find(Program* program, int args);
static void mf_string_to_case(Program* program, int args);
static void mf_string_format(Program* program, int args);
static void mf_floor2(Program* program, int args);

// ref. https://github.com/sfall-team/sfall/blob/42556141127895c27476cd5242a73739cbb0fade/sfall/Modules/Scripting/Handlers/Metarule.cpp#L72
// Note: metarules should pop arguments off the stack in natural order
const MetaruleInfo kMetarules[] = {
    // {"add_extra_msg_file",        mf_add_extra_msg_file,        1, 2, -1, {ARG_STRING, ARG_INT}},
    // {"add_iface_tag",             mf_add_iface_tag,             0, 0},
    // {"add_g_timer_event",         mf_add_g_timer_event,         2, 2, -1, {ARG_INT, ARG_INT}},
    // {"add_trait",                 mf_add_trait,                 1, 1, -1, {ARG_INT}},
    // {"art_cache_clear",           mf_art_cache_flush,           0, 0},
    // {"art_frame_data",            mf_art_frame_data,            1, 3,  0, {ARG_INTSTR, ARG_INT, ARG_INT}},
    // {"attack_is_aimed",           mf_attack_is_aimed,           0, 0},
    { "car_gas_amount", mf_car_gas_amount, 0, 0 },
    { "combat_data", mf_combat_data, 0, 0 },
    // {"create_win",                mf_create_win,                5, 6, -1, {ARG_STRING, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    { "critter_inven_obj2", mf_critter_inven_obj2, 2, 2 }, // XXX: likely parameter order mismatch
    // {"dialog_message",            mf_dialog_message,            1, 1, -1, {ARG_STRING}},
    { "dialog_obj", mf_dialog_obj, 0, 0 },
    // {"display_stats",             mf_display_stats,             0, 0}, // refresh
    // {"draw_image",                mf_draw_image,                1, 5, -1, {ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"draw_image_scaled",         mf_draw_image_scaled,         1, 6, -1, {ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"exec_map_update_scripts",   mf_exec_map_update_scripts,   0, 0},
    { "floor2", mf_floor2, 1, 1 },
    // {"get_can_rest_on_map",       mf_get_rest_on_map,           2, 2, -1, {ARG_INT, ARG_INT}},
    // {"get_combat_free_move",      mf_get_combat_free_move,      0, 0},
    // {"get_current_inven_size",    mf_get_current_inven_size,    1, 1,  0, {ARG_OBJECT}},
    { "get_cursor_mode", mf_get_cursor_mode, 0, 0 },
    { "get_flags", mf_get_flags, 1, 1 },
    // {"get_ini_config",            mf_get_ini_config,            2, 2,  0, {ARG_STRING, ARG_INT}},
    { "get_ini_section", mf_get_ini_section, 2, 2 },
    { "get_ini_sections", mf_get_ini_sections, 1, 1 },
    // {"get_inven_ap_cost",         mf_get_inven_ap_cost,         0, 0},
    // {"get_map_enter_position",    mf_get_map_enter_position,    0, 0},
    // {"get_metarule_table",        mf_get_metarule_table,        0, 0},
    // {"get_object_ai_data",        mf_get_object_ai_data,        2, 2, -1, {ARG_OBJECT, ARG_INT}},
    { "get_object_data", mf_get_object_data, 2, 2 }, // XXX: likely parameter order mismatch
    // {"get_outline",               mf_get_outline,               1, 1,  0, {ARG_OBJECT}},
    // {"get_sfall_arg_at",          mf_get_sfall_arg_at,          1, 1,  0, {ARG_INT}},
    // {"get_stat_max",              mf_get_stat_max,              1, 2,  0, {ARG_INT, ARG_INT}},
    // {"get_stat_min",              mf_get_stat_min,              1, 2,  0, {ARG_INT, ARG_INT}},
    // {"get_string_pointer",        mf_get_string_pointer,        1, 1,  0, {ARG_STRING}}, // note: deprecated; do not implement
    // {"get_terrain_name",          mf_get_terrain_name,          0, 2, -1, {ARG_INT, ARG_INT}},
    { "get_text_width", mf_get_text_width, 1, 1 },
    // {"get_window_attribute",      mf_get_window_attribute,      1, 2, -1, {ARG_INT, ARG_INT}},
    // {"has_fake_perk_npc",         mf_has_fake_perk_npc,         2, 2,  0, {ARG_OBJECT, ARG_STRING}},
    // {"has_fake_trait_npc",        mf_has_fake_trait_npc,        2, 2,  0, {ARG_OBJECT, ARG_STRING}},
    // {"hide_window",               mf_hide_window,               0, 1, -1, {ARG_STRING}},
    // {"interface_art_draw",        mf_interface_art_draw,        4, 6, -1, {ARG_INT, ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"interface_overlay",         mf_interface_overlay,         2, 6, -1, {ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"interface_print",           mf_interface_print,           5, 6, -1, {ARG_STRING, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"intface_hide",              mf_intface_hide,              0, 0},
    // {"intface_is_hidden",         mf_intface_is_hidden,         0, 0},
    { "intface_redraw", mf_intface_redraw, 0, 1 },
    // {"intface_show",              mf_intface_show,              0, 0},
    // {"inventory_redraw",          mf_inventory_redraw,          0, 1, -1, {ARG_INT}},
    // {"item_make_explosive",       mf_item_make_explosive,       3, 4, -1, {ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"item_weight",               mf_item_weight,               1, 1,  0, {ARG_OBJECT}},
    // {"lock_is_jammed",            mf_lock_is_jammed,            1, 1,  0, {ARG_OBJECT}},
    { "loot_obj", mf_loot_obj, 0, 0 },
    { "message_box", mf_message_box, 1, 4 }, // {ARG_STRING, ARG_INT, ARG_INT, ARG_INT}},
    { "metarule_exist", mf_metarule_exist, 1, 1 },
    // {"npc_engine_level_up",       mf_npc_engine_level_up,       1, 1},
    // {"obj_is_openable",           mf_obj_is_openable,           1, 1,  0, {ARG_OBJECT}},
    // {"obj_under_cursor",          mf_obj_under_cursor,          2, 2,  0, {ARG_INT, ARG_INT}},
    // {"objects_in_radius",         mf_objects_in_radius,         3, 4,  0, {ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    { "outlined_object", mf_outlined_object, 0, 0 },
    // {"real_dude_obj",             mf_real_dude_obj,             0, 0},
    // {"reg_anim_animate_and_move", mf_reg_anim_animate_and_move, 4, 4, -1, {ARG_OBJECT, ARG_INT, ARG_INT, ARG_INT}},
    // {"remove_timer_event",        mf_remove_timer_event,        0, 1, -1, {ARG_INT}},
    // {"set_spray_settings",        mf_set_spray_settings,        4, 4, -1, {ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"set_can_rest_on_map",       mf_set_rest_on_map,           3, 3, -1, {ARG_INT, ARG_INT, ARG_INT}},
    // {"set_car_intface_art",       mf_set_car_intface_art,       1, 1, -1, {ARG_INT}},
    // {"set_combat_free_move",      mf_set_combat_free_move,      1, 1, -1, {ARG_INT}},
    { "set_cursor_mode", mf_set_cursor_mode, 1, 1 },
    // {"set_drugs_data",            mf_set_drugs_data,            3, 3, -1, {ARG_INT, ARG_INT, ARG_INT}},
    // {"set_dude_obj",              mf_set_dude_obj,              1, 1, -1, {ARG_INT}},
    // {"set_fake_perk_npc",         mf_set_fake_perk_npc,         5, 5, -1, {ARG_OBJECT, ARG_STRING, ARG_INT, ARG_INT, ARG_STRING}},
    // {"set_fake_trait_npc",        mf_set_fake_trait_npc,        5, 5, -1, {ARG_OBJECT, ARG_STRING, ARG_INT, ARG_INT, ARG_STRING}},
    { "set_flags", mf_set_flags, 2, 2 },
    // {"set_iface_tag_text",        mf_set_iface_tag_text,        3, 3, -1, {ARG_INT, ARG_STRING, ARG_INT}},
    { "set_ini_setting", mf_set_ini_setting, 2, 2 },
    // {"set_map_enter_position",    mf_set_map_enter_position,    3, 3, -1, {ARG_INT, ARG_INT, ARG_INT}},
    // {"set_object_data",           mf_set_object_data,           3, 3, -1, {ARG_OBJECT, ARG_INT, ARG_INT}},
    { "set_outline", mf_set_outline, 2, 2 },
    // {"set_quest_failure_value",   mf_set_quest_failure_value,   2, 2, -1, {ARG_INT, ARG_INT}},
    // {"set_rest_heal_time",        mf_set_rest_heal_time,        1, 1, -1, {ARG_INT}},
    // {"set_worldmap_heal_time",    mf_set_worldmap_heal_time,    1, 1, -1, {ARG_INT}},
    // {"set_rest_mode",             mf_set_rest_mode,             1, 1, -1, {ARG_INT}},
    // {"set_scr_name",              mf_set_scr_name,              0, 1, -1, {ARG_STRING}},
    // {"set_selectable_perk_npc",   mf_set_selectable_perk_npc,   5, 5, -1, {ARG_OBJECT, ARG_STRING, ARG_INT, ARG_INT, ARG_STRING}},
    // {"set_terrain_name",          mf_set_terrain_name,          3, 3, -1, {ARG_INT, ARG_INT, ARG_STRING}},
    // {"set_town_title",            mf_set_town_title,            2, 2, -1, {ARG_INT, ARG_STRING}},
    // {"set_unique_id",             mf_set_unique_id,             1, 2, -1, {ARG_OBJECT, ARG_INT}},
    // {"set_unjam_locks_time",      mf_set_unjam_locks_time,      1, 1, -1, {ARG_INT}},
    // {"set_window_flag",           mf_set_window_flag,           3, 3, -1, {ARG_INTSTR, ARG_INT, ARG_INT}},
    { "show_window", mf_show_window, 0, 1 },
    // {"signal_close_game",         mf_signal_close_game,         0, 0},
    // {"spatial_radius",            mf_spatial_radius,            1, 1,  0, {ARG_OBJECT}},
    { "string_compare", mf_string_compare, 2, 3 }, // {ARG_STRING, ARG_STRING, ARG_INT}},
    { "string_find", mf_string_find, 2, 3 }, // {ARG_STRING, ARG_STRING, ARG_INT}},
    { "string_format", mf_string_format, 2, 8 },
    { "string_to_case", mf_string_to_case, 2, 2 }, // {ARG_STRING, ARG_INT}}
    // {"tile_by_position",          mf_tile_by_position,          2, 2, -1, {ARG_INT, ARG_INT}},
    { "tile_refresh_display", mf_tile_refresh_display, 0, 0 },
    // {"unjam_lock",                mf_unjam_lock,                1, 1, -1, {ARG_OBJECT}},
    // {"unwield_slot",              mf_unwield_slot,              2, 2, -1, {ARG_OBJECT, ARG_INT}},
    // {"win_fill_color",            mf_win_fill_color,            0, 5, -1, {ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    { "opcode_exists", mf_opcode_exists, 1, 1 },
};
const std::size_t kMetarulesCount = sizeof(kMetarules) / sizeof(kMetarules[0]);

constexpr int kMetarulesMax = sizeof(kMetarules) / sizeof(kMetarules[0]);

void mf_car_gas_amount(Program* program, int args)
{
    programStackPushInteger(program, wmCarGasAmount());
}

void mf_combat_data(Program* program, int args)
{
    if (isInCombat()) {
        programStackPushPointer(program, combat_get_data());
    } else {
        programStackPushPointer(program, nullptr);
    }
}

void mf_critter_inven_obj2(Program* program, int args)
{
    int slot = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    switch (slot) {
    case 0:
        programStackPushPointer(program, critterGetArmor(obj));
        break;
    case 1:
        programStackPushPointer(program, critterGetItem2(obj));
        break;
    case 2:
        programStackPushPointer(program, critterGetItem1(obj));
        break;
    case -2:
        programStackPushInteger(program, obj->data.inventory.length);
        break;
    default:
        programFatalError("mf_critter_inven_obj2: invalid type");
    }
}

void mf_dialog_obj(Program* program, int args)
{
    if (GameMode::isInGameMode(GameMode::kDialog)) {
        programStackPushPointer(program, gGameDialogSpeaker);
    } else {
        programStackPushPointer(program, nullptr);
    }
}

void mf_get_cursor_mode(Program* program, int args)
{
    programStackPushInteger(program, gameMouseGetMode());
}

void mf_get_flags(Program* program, int args)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    programStackPushInteger(program, object->flags);
}

void mf_get_object_data(Program* program, int args)
{
    size_t offset = static_cast<size_t>(programStackPopInteger(program));
    void* ptr = programStackPopPointer(program);

    if (offset % 4 != 0) {
        programFatalError("mf_get_object_data: bad offset %d", offset);
    }

    int value = *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(ptr) + offset);
    programStackPushInteger(program, value);
}

void mf_get_text_width(Program* program, int args)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, fontGetStringWidth(string));
}

void mf_intface_redraw(Program* program, int args)
{
    if (args == 0) {
        interfaceBarRefresh();
    } else {
        // TODO: Incomplete.
        programFatalError("mf_intface_redraw: not implemented");
    }

    programStackPushInteger(program, -1);
}

void mf_loot_obj(Program* program, int args)
{
    if (GameMode::isInGameMode(GameMode::kInventory)) {
        programStackPushPointer(program, inven_get_current_target_obj());
    } else {
        programStackPushPointer(program, nullptr);
    }
}

void mf_metarule_exist(Program* program, int args)
{
    const char* metarule = programStackPopString(program);

    for (int index = 0; index < kMetarulesMax; index++) {
        if (strcmp(kMetarules[index].name, metarule) == 0) {
            programStackPushInteger(program, 1);
            return;
        }
    }

    programStackPushInteger(program, 0);
}

void mf_opcode_exists(Program* program, int args)
{
    int opcode = programStackPopInteger(program);
    int opcodeIndex = opcode & 0x3FFF;
    if (opcodeIndex < 0 || opcodeIndex >= OPCODE_MAX_COUNT) {
        programStackPushInteger(program, 0);
        return;
    }
    auto opcodeHandler = gInterpreterOpcodeHandlers[opcodeIndex];
    int opcodeExists = opcodeHandler != nullptr ? 1 : 0;
    programStackPushInteger(program, opcodeExists);
}

void mf_outlined_object(Program* program, int args)
{
    programStackPushPointer(program, gmouse_get_outlined_object());
}

void mf_set_cursor_mode(Program* program, int args)
{
    int mode = programStackPopInteger(program);
    gameMouseSetMode(mode);
    programStackPushInteger(program, -1);
}

void mf_set_flags(Program* program, int args)
{
    int flags = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    object->flags = flags;

    programStackPushInteger(program, -1);
}

void mf_set_outline(Program* program, int args)
{
    int outline = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    object->outline = outline;
    programStackPushInteger(program, -1);
}

void mf_show_window(Program* program, int args)
{
    if (args == 0) {
        _windowShow();
    } else if (args == 1) {
        const char* windowName = programStackPopString(program);
        if (!_windowShowNamed(windowName)) {
            debugPrint("show_window: window '%s' is not found", windowName);
        }
    }

    programStackPushInteger(program, -1);
}

void mf_tile_refresh_display(Program* program, int args)
{
    tileWindowRefresh();
    programStackPushInteger(program, -1);
}

// compares strings case-insensitive with specifics for Fallout
// from sfall: https://github.com/sfall-team/sfall/blob/71ecec3d405bd5e945f157954618b169e60068fe/sfall/Modules/Scripting/Handlers/Utils.cpp#L34
static bool FalloutStringCompare(const char* str1, const char* str2, long codePage)
{
    while (true) {
        unsigned char c1 = *str1;
        unsigned char c2 = *str2;

        if (c1 == 0 && c2 == 0) return true; // end - strings are equal
        if (c1 == 0 || c2 == 0) return false; // strings are not equal
        str1++;
        str2++;
        if (c1 == c2) continue;

        if (codePage == 866) {
            // replace Russian 'x' with English (Fallout specific)
            if (c1 == 229) c1 -= 229 - 'x';
            if (c2 == 229) c2 -= 229 - 'x';
        }

        // 0 - 127 (standard ASCII)
        // upper to lower case
        if (c1 >= 'A' && c1 <= 'Z') c1 |= 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 |= 32;
        if (c1 == c2) continue;
        if (c1 < 128 || c2 < 128) return false;

        // 128 - 255 (international/extended)
        switch (codePage) {
        case 866:
            if (c1 != 149 && c2 != 149) { // code used for the 'bullet' character in Fallout font (the Russian letter 'X' uses Latin letter)
                // upper to lower case
                if (c1 >= 128 && c1 <= 159) {
                    c1 |= 32;
                } else if (c1 >= 224 && c1 <= 239) {
                    c1 -= 48; // shift lower range
                } else if (c1 == 240) {
                    c1++;
                }
                if (c2 >= 128 && c2 <= 159) {
                    c2 |= 32;
                } else if (c2 >= 224 && c2 <= 239) {
                    c2 -= 48; // shift lower range
                } else if (c2 == 240) {
                    c2++;
                }
            }
            break;
        case 1251:
            // upper to lower case
            if (c1 >= 0xC0 && c1 <= 0xDF) c1 |= 32;
            if (c2 >= 0xC0 && c2 <= 0xDF) c2 |= 32;
            if (c1 == 0xA8) c1 += 16;
            if (c2 == 0xA8) c2 += 16;
            break;
        case 1250:
        case 1252:
            if (c1 != 0xD7 && c1 != 0xF7 && c2 != 0xD7 && c2 != 0xF7) {
                if (c1 >= 0xC0 && c1 <= 0xDE) c1 |= 32;
                if (c2 >= 0xC0 && c2 <= 0xDE) c2 |= 32;
            }
            break;
        }
        if (c1 != c2) return false; // strings are not equal
    }
}

void mf_string_compare(Program* program, int args)
{
    // compare str1 to str3 case insensitively
    // if args == 3, use FalloutStringCompare
    const char* str1 = programStackPopString(program);
    const char* str2 = programStackPopString(program);
    int codePage = 0;
    if (args == 3) {
        codePage = programStackPopInteger(program);
    }
    bool result = false;
    if (args < 3) {
        // default case-insensitive comparison
        result = compat_stricmp(str1, str2) == 0;
    } else {
        // Fallout specific case-insensitive comparison
        result = FalloutStringCompare(str1, str2, codePage);
    }
    if (result) {
        programStackPushInteger(program, 1); // strings are equal
    } else {
        programStackPushInteger(program, 0); // strings are not equal
    }
}

void mf_string_find(Program* program, int args)
{
    const char* str = programStackPopString(program);
    const char* substr = programStackPopString(program);
    int startPos = 0;

    if (args == 3) {
        startPos = programStackPopInteger(program);
    }

    if (startPos < 0 || startPos >= strlen(str)) {
        debugPrint("string_find: invalid start position %d", startPos);
        programStackPushInteger(program, -1);
        return;
    }

    const char* found = strstr(str + startPos, substr);
    if (found) {
        programStackPushInteger(program, static_cast<int>(found - str));
    } else {
        programStackPushInteger(program, -1);
    }
}

void mf_string_to_case(Program* program, int args)
{
    auto buf = programStackPopString(program);
    std::string s(buf);
    auto caseType = programStackPopInteger(program);
    if (caseType == 1) {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    } else if (caseType == 0) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    } else {
        debugPrint("string_to_case: invalid case type %d", caseType);
    }
    programStackPushString(program, s.c_str());
}

void mf_string_format(Program* program, int args)
{
    sprintf_lite(program, args, "string_format");
}

void mf_floor2(Program* program, int args)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushInteger(program, static_cast<int>(floor(programValue.asFloat())));
}

void sprintf_lite(Program* program, int args, const char* infoOpcodeName)
{
    auto format = programStackPopString(program); // Pop the format string

    ProgramValue formatArgs[7]; // 8 arguments total, 1 for format string

    for (int index = 0; index < args - 1; index++) {
        formatArgs[index] = programStackPopValue(program);
    }

    int fmtLen = static_cast<int>(strlen(format));
    if (fmtLen == 0) {
        programStackPushString(program, "");
        return;
    }
    if (fmtLen > 1024) {
        debugPrint("%s(): format string exceeds maximum length of 1024 characters.", infoOpcodeName);
        programStackPushString(program, "Error");
        return;
    }
    int newFmtLen = fmtLen;

    for (int i = 0; i < fmtLen; i++) {
        if (format[i] == '%') newFmtLen++; // will possibly be escaped, need space for that
    }

    // parse format to make it safe
    auto newFmt = std::make_unique<char[]>(newFmtLen + 1);

    bool conversion = false;
    int j = 0;
    int valIdx = 0;

    char out[5120 + 1] = { 0 };
    int bufCount = sizeof(out) - 1;
    char* outBuf = out;

    int numArgs = args; // From 2 to 8

    for (int i = 0; i < fmtLen; i++) {
        char c = format[i];
        if (!conversion) {
            // Start conversion.
            if (c == '%') conversion = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '%') {
            int partLen;
            if (c == '%') {
                // escaped % sign, just copy newFmt up to (and including) the leading % sign
                newFmt[j] = '\0';
                // strncpy_s(outBuf, bufCount, newFmt, j);
                strncpy(outBuf, newFmt.get(), std::min(j, bufCount - 1));
                partLen = j;
            } else {
                // ignore size prefixes
                if (c == 'h' || c == 'l' || c == 'j' || c == 'z' || c == 't' || c == 'w' || c == 'L' || c == 'I') continue;
                // Type specifier, perform conversion.
                if (++valIdx == numArgs) {
                    debugPrint("%s() - format string contains more conversions than passed arguments (%d): %s",
                        infoOpcodeName, numArgs - 1, format);
                }
                const auto& arg = formatArgs[std::min(valIdx - 1, numArgs - 2)];

                // ctx.arg(valIdx < numArgs ? valIdx : numArgs - 1);
                if (c == 'S' || c == 'Z') {
                    c = 's'; // don't allow wide strings
                }
                if ((c == 's' && !arg.isString()) || // don't allow treating non-string values as string pointers
                    c == 'n') // don't allow "n" specifier
                {
                    c = 'd';
                }
                newFmt[j++] = c;
                newFmt[j] = '\0';
                partLen = arg.isFloat()
                    ? snprintf(outBuf, bufCount, newFmt.get(), arg.floatValue)
                    : arg.isInt()    ? snprintf(outBuf, bufCount, newFmt.get(), arg.integerValue)
                    : arg.isString() ? snprintf(outBuf, bufCount, newFmt.get(),
                                           programGetString(program, arg.opcode, arg.integerValue))
                                     : snprintf(outBuf, bufCount, newFmt.get(), "<UNSUPPORTED TYPE>");
            }
            outBuf += partLen;
            bufCount -= partLen;
            conversion = false;
            j = 0;
            if (bufCount <= 0) {
                break;
            }
            continue;
        }
        newFmt[j++] = c;
    }
    // Copy the remainder of the string.
    if (bufCount > 0) {
        newFmt[j] = '\0';
        // strcpy_s(outBuf, bufCount, newFmt);
        if (strlen(newFmt.get()) < bufCount) {
            strcpy(outBuf, newFmt.get());
        } else {
            strncpy(outBuf, newFmt.get(), bufCount - 1);
            outBuf[bufCount - 1] = '\0'; // Ensure null-termination
        }
    }

    programStackPushString(program, out);
}

// message_box
void mf_message_box(Program* program, int args)
{
    static int dialogShowCount = 0;

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

    int flags = DIALOG_BOX_LARGE | DIALOG_BOX_YES_NO;
    if (args > 1) {
        int flagParam = programStackPopInteger(program);
        if (flagParam != -1) {
            flags = flagParam;
        }
    }

    // note: most of the CE code uses colorTable indices, but this metarule expects palette values.
    // Default: yellow (145) = _colorTable[32328]
    int color1 = _colorTable[32328], color2 = _colorTable[32328];
    if (args > 2) {
        color1 = programStackPopInteger(program);
    }
    if (args > 3) {
        color2 = programStackPopInteger(program);
    }

    dialogShowCount++;
    scriptsDisable();
    int rc = showDialogBox(copy, body, count, 192, 116, color1, nullptr, color2, flags);
    if (--dialogShowCount == 0) {
        scriptsEnable();
    }

    programStackPushInteger(program, rc);
    internal_free(copy);
}

void sfall_metarule(Program* program, int args)
{
    static ProgramValue values[8];

    for (int index = 0; index < args; index++) {
        values[index] = programStackPopValue(program);
    }

    const char* metarule = programStackPopString(program);

    for (int index = 0; index < args; index++) {
        programStackPushValue(program, values[index]);
    }

    int metaruleIndex = -1;
    for (int index = 0; index < kMetarulesMax; index++) {
        if (strcmp(kMetarules[index].name, metarule) == 0) {
            metaruleIndex = index;
            break;
        }
    }

    if (metaruleIndex == -1) {
        programFatalError("op_sfall_func: '%s' is not implemented", metarule);
    }

    if (args < kMetarules[metaruleIndex].minArgs || args > kMetarules[metaruleIndex].maxArgs) {
        programFatalError("op_sfall_func: '%s': invalid number of args", metarule);
    }

    kMetarules[metaruleIndex].handler(program, args);
}

} // namespace fallout
