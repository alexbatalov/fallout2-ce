#include "mapper/mp_proto.h"

#include <string.h>

#include "art.h"
#include "color.h"
#include "combat_ai.h"
#include "critter.h"
#include "input.h"
#include "kb.h"
#include "mapper/mp_targt.h"
#include "memory.h"
#include "proto.h"
#include "svga.h"
#include "window_manager.h"
#include "window_manager_private.h"

namespace fallout {

#define CRITTER_FLAG_COUNT 10

#define YES 0
#define NO 1

static int proto_choose_container_flags(Proto* proto);
static int proto_subdata_setup_int_button(const char* title, int key, int value, int min_value, int max_value, int* y, int a7);
static int proto_subdata_setup_fid_button(const char* title, int key, int fid, int* y, int a5);
static int proto_subdata_setup_pid_button(const char* title, int key, int pid, int* y, int a5);
static void proto_critter_flags_redraw(int win, int pid);
static int proto_critter_flags_modify(int pid);
static int mp_pick_kill_type();

static char kYes[] = "YES";
static char kNo[] = "NO";

// 0x53DAFC
static char default_proto_builder_name[36] = "EVERTS SCOTTY";

// 0x559924
char* proto_builder_name = default_proto_builder_name;

// 0x559B94
static const char* wall_light_strs[] = {
    "North/South",
    "East/West",
    "North Corner",
    "South Corner",
    "East Corner",
    "West Corner",
};

// 0x559C50
static char* yesno[] = {
    kYes,
    kNo,
};

// 0x559C58
int edit_window_color = 1;

// 0x559C60
bool can_modify_protos = false;

// 0x559C68
static int subwin = -1;

// 0x559C6C
static int critFlagList[CRITTER_FLAG_COUNT] = {
    CRITTER_NO_STEAL,
    CRITTER_NO_DROP,
    CRITTER_NO_LIMBS,
    CRITTER_NO_AGE,
    CRITTER_NO_HEAL,
    CRITTER_INVULNERABLE,
    CRITTER_FLAT,
    CRITTER_SPECIAL_DEATH,
    CRITTER_LONG_LIMBS,
    CRITTER_NO_KNOCKBACK,
};

// 0x559C94
static const char* critFlagStrs[CRITTER_FLAG_COUNT] = {
    "_Steal",
    "_Drop",
    "_Limbs",
    "_Ages",
    "_Heal",
    "Invuln.,",
    "_Flattens",
    "Special",
    "Rng",
    "_Knock",
};

// 0x4922F8
void init_mapper_protos()
{
    edit_window_color = _colorTable[10570];
    can_modify_protos = target_overriden();
}

// 0x492840
int proto_choose_container_flags(Proto* proto)
{
    int win = windowCreate(320,
        185,
        220,
        205,
        edit_window_color,
        WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    _win_register_text_button(win,
        10,
        11,
        -1,
        -1,
        -1,
        '1',
        "Magic Hands Grnd",
        0);

    if ((proto->item.data.container.openFlags & 0x1) != 0) {
        windowDrawText(win,
            yesno[YES],
            50,
            125,
            15,
            _colorTable[32747] | 0x10000);
    } else {
        windowDrawText(win,
            yesno[NO],
            50,
            125,
            15,
            _colorTable[32747] | 0x10000);
    }

    _win_register_text_button(win,
        10,
        32,
        -1,
        -1,
        -1,
        '2',
        "Cannot Pick Up",
        0);

    if (_proto_action_can_pickup(proto->pid)) {
        windowDrawText(win,
            yesno[YES],
            50,
            125,
            36,
            _colorTable[32747] | 0x10000);
    } else {
        windowDrawText(win,
            yesno[NO],
            50,
            125,
            36,
            _colorTable[32747] | 0x10000);
    }

    windowDrawBorder(win);
    windowRefresh(win);

    while (1) {
        sharedFpsLimiter.mark();

        int input = inputGetInput();
        if (input == KEY_ESCAPE
            || input == KEY_BAR
            || input == KEY_RETURN) {
            break;
        }

        if (input == '1') {
            proto->item.data.container.openFlags ^= 0x1;

            if ((proto->item.data.container.openFlags & 0x1) != 0) {
                windowDrawText(win,
                    yesno[YES],
                    50,
                    125,
                    15,
                    _colorTable[32747] | 0x10000);
            } else {
                windowDrawText(win,
                    yesno[NO],
                    50,
                    125,
                    15,
                    _colorTable[32747] | 0x10000);
            }

            windowRefresh(win);
        } else if (input == '2') {
            proto->item.extendedFlags ^= 0x8000;

            if (_proto_action_can_pickup(proto->pid)) {
                windowDrawText(win,
                    yesno[YES],
                    50,
                    125,
                    36,
                    _colorTable[32747] | 0x10000);
            } else {
                windowDrawText(win,
                    yesno[NO],
                    50,
                    125,
                    36,
                    _colorTable[32747] | 0x10000);
            }

            windowRefresh(win);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);

    return 0;
}

// 0x492A3C
int proto_subdata_setup_int_button(const char* title, int key, int value, int min_value, int max_value, int* y, int a7)
{
    char text[36];
    int button_x;
    int value_offset_x;

    button_x = 10;
    value_offset_x = 90;

    if (a7 == 9) {
        *y -= 189;
    }

    if (a7 > 8) {
        button_x = 165;
        value_offset_x -= 16;
    }

    _win_register_text_button(subwin,
        button_x,
        *y,
        -1,
        -1,
        -1,
        key,
        title,
        0);

    if (value >= min_value && value < max_value) {
        sprintf(text, "%d", value);
        windowDrawText(subwin,
            text,
            38,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[32747] | 0x10000);
    } else {
        windowDrawText(subwin,
            "<ERROR>",
            38,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[31744] | 0x10000);
    }

    *y += 21;

    return 0;
}

// 0x492B28
int proto_subdata_setup_fid_button(const char* title, int key, int fid, int* y, int a5)
{
    char text[36];
    char* pch;
    int button_x;
    int value_offset_x;

    button_x = 10;
    value_offset_x = 90;

    if (a5 == 9) {
        *y -= 189;
    }

    if (a5 > 8) {
        button_x = 165;
        value_offset_x -= 16;
    }

    _win_register_text_button(subwin,
        button_x,
        *y,
        -1,
        -1,
        -1,
        key,
        title,
        0);

    if (art_list_str(fid, text) != -1) {
        pch = strchr(text, '.');
        if (pch != NULL) {
            *pch = '\0';
        }

        windowDrawText(subwin,
            text,
            80,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[32747] | 0x10000);
    } else {
        windowDrawText(subwin,
            "None",
            80,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[992] | 0x10000);
    }

    *y += 21;

    return 0;
}

// 0x492C20
int proto_subdata_setup_pid_button(const char* title, int key, int pid, int* y, int a5)
{
    int button_x;
    int value_offset_x;

    button_x = 10;
    value_offset_x = 90;

    if (a5 == 9) {
        *y -= 189;
    }

    if (a5 > 8) {
        button_x = 165;
        value_offset_x = 74;
    }

    _win_register_text_button(subwin,
        button_x,
        *y,
        -1,
        -1,
        -1,
        key,
        title,
        0);

    if (pid != -1) {
        windowDrawText(subwin,
            protoGetName(pid),
            49,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[32747] | 0x10000);
    } else {
        windowDrawText(subwin,
            "None",
            49,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[992] | 0x10000);
    }

    *y += 21;

    return 0;
}

// 0x495438
const char* proto_wall_light_str(int flags)
{
    if ((flags & 0x8000000) != 0) {
        return wall_light_strs[1];
    }

    if ((flags & 0x10000000) != 0) {
        return wall_light_strs[2];
    }

    if ((flags & 0x20000000) != 0) {
        return wall_light_strs[3];
    }

    if ((flags & 0x40000000) != 0) {
        return wall_light_strs[4];
    }

    if ((flags & 0x80000000) != 0) {
        return wall_light_strs[5];
    }

    return wall_light_strs[0];
}

// 0x4960B8
void proto_critter_flags_redraw(int win, int pid)
{
    int index;
    int color;
    int x = 110;

    for (index = 0; index < CRITTER_FLAG_COUNT; index++) {
        if (_critter_flag_check(pid, critFlagList[index])) {
            color = _colorTable[992];
        } else {
            color = _colorTable[10570];
        }

        windowDrawText(win, critFlagStrs[index], 44, x, 195, color | 0x10000);
        x += 48;
    }
}

// 0x496120
int proto_critter_flags_modify(int pid)
{
    Proto* proto;
    int rc;
    int flags = 0;
    int index;

    if (protoGetProto(pid, &proto) == -1) {
        return -1;
    }

    rc = win_yes_no("Can't be stolen from?", 340, 200, _colorTable[15855]);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_STEAL;
    }

    rc = win_yes_no("Can't Drop items?", 340, 200, _colorTable[15855]);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_DROP;
    }

    rc = win_yes_no("Can't lose limbs?", 340, 200, _colorTable[15855]);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_LIMBS;
    }

    rc = win_yes_no("Dead Bodies Can't Age?", 340, 200, _colorTable[15855]);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_AGE;
    }

    rc = win_yes_no("Can't Heal by Aging?", 340, 200, _colorTable[15855]);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_HEAL;
    }

    rc = win_yes_no("Is Invlunerable????", 340, 200, _colorTable[15855]);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_INVULNERABLE;
    }

    rc = win_yes_no("Can't Flatten on Death?", 340, 200, _colorTable[15855]);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_FLAT;
    }

    rc = win_yes_no("Has Special Death?", 340, 200, _colorTable[15855]);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_SPECIAL_DEATH;
    }

    rc = win_yes_no("Has Extra Hand-To-Hand Range?", 340, 200, _colorTable[15855]);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_LONG_LIMBS;
    }

    rc = win_yes_no("Can't be knocked back?", 340, 200, _colorTable[15855]);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_KNOCKBACK;
    }

    if (!can_modify_protos) {
        win_timed_msg("Can't modify protos!", _colorTable[31744] | 0x10000);
        return -1;
    }

    for (index = 0; index < CRITTER_FLAG_COUNT; index++) {
        if ((critFlagList[index] & flags) != 0) {
            critter_flag_set(pid, critFlagList[index]);
        } else {
            critter_flag_unset(pid, critFlagList[index]);
        }
    }

    return 0;
}

// 0x497520
int mp_pick_kill_type()
{
    char* names[KILL_TYPE_COUNT];
    int index;

    for (index = 0; index < KILL_TYPE_COUNT; index++) {
        names[index] = killTypeGetName(index);
    }

    return _win_list_select("Kill Type",
        names,
        KILL_TYPE_COUNT,
        NULL,
        50,
        100,
        _colorTable[15855]);
}

// 0x497568
int proto_pick_ai_packet(int* value)
{
    int count;
    char** names;
    int index;
    int rc;

    count = combat_ai_num();
    if (count <= 0) {
        return -1;
    }

    names = (char**)internal_malloc(sizeof(char*) * count);
    for (index = 0; index < count; index++) {
        names[index] = (char*)internal_malloc(strlen(combat_ai_name(index)) + 1);
        strcpy(names[index], combat_ai_name(index));
    }

    rc = _win_list_select("AI Packet",
        names,
        count,
        NULL,
        50,
        100,
        _colorTable[15855]);
    if (rc != -1) {
        *value = rc;
    }

    for (index = 0; index < count; index++) {
        internal_free(names[index]);
    }

    internal_free(names);
    return 0;
}

} // namespace fallout
