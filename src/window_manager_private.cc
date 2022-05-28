#include "window_manager_private.h"

#include "core.h"
#include "memory.h"
#include "text_font.h"
#include "window_manager.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

// 0x51E414
int _wd = -1;

// 0x51E418
int _curr_menu = 0;

// 0x51E41C
bool _tm_watch_active = false;

// 0x6B2340
STRUCT_6B2340 _tm_location[5];

// 0x6B2368
int _tm_text_x;

// 0x6B236C
int _tm_h;

// 0x6B2370
STRUCT_6B2370 _tm_queue[5];

// 0x6B23AC
int _tm_persistence;

// 0x6B23B0
int _scr_center_x;

// 0x6B23B4
int _tm_text_y;

// 0x6B23B8
int _tm_kill;

// 0x6B23BC
int _tm_add;

// x
//
// 0x6B23C0
int _curry;

// y
//
// 0x6B23C4
int _currx;

// 0x6B23D0
char gProgramWindowTitle[256];

// 0x4DC30C
int _win_debug(char* a1)
{
    if (!gWindowSystemInitialized) {
        return -1;
    }

    // TODO: Incomplete.

    windowRefresh(_wd);

    return 0;
}

// 0x4DC65C
void _win_debug_delete()
{
    windowDestroy(_wd);
    _wd = -1;
}

// 0x4DC674
int _win_register_menu_bar(int win, int x, int y, int width, int height, int a6, int a7)
{
    Window* window = windowGetWindow(win);

    if (!gWindowSystemInitialized) {
        return -1;
    }

    if (window == NULL) {
        return -1;
    }

    if (window->field_3C != NULL) {
        return -1;
    }

    int right = x + width;
    if (right > window->width) {
        return -1;
    }

    int bottom = y + height;
    if (bottom > window->height) {
        return -1;
    }

    struc_177* v14 = window->field_3C = (struc_177*)internal_malloc(sizeof(struc_177));
    if (v14 == NULL) {
        return -1;
    }

    v14->win = win;
    v14->rect.left = x;
    v14->rect.top = y;
    v14->rect.right = right - 1;
    v14->rect.bottom = bottom - 1;
    v14->entriesCount = 0;
    v14->field_234 = a6;
    v14->field_238 = a7;

    windowFill(win, x, y, width, height, a7);
    windowDrawRect(win, x, y, right - 1, bottom - 1, a6);

    return 0;
}

// 0x4DC768
int _win_register_menu_pulldown(int win, int x, char* str, int a4)
{
    Window* window = windowGetWindow(win);

    if (!gWindowSystemInitialized) {
        return -1;
    }

    if (window == NULL) {
        return -1;
    }

    struc_177* field_3C = window->field_3C;
    if (field_3C == NULL) {
        return -1;
    }

    if (window->field_3C->entriesCount == 15) {
        return -1;
    }

    int btn = buttonCreate(win,
        field_3C->rect.left + x,
        (field_3C->rect.top + field_3C->rect.bottom - fontGetLineHeight()) / 2,
        fontGetStringWidth(str),
        fontGetLineHeight(),
        -1,
        -1,
        a4,
        -1,
        NULL,
        NULL,
        NULL,
        0);
    if (btn == -1) {
        return -1;
    }

    // TODO: Incomplete.

    return 0;
}

// 0x4DCA30
int _win_width_needed(char** fileNameList, int fileNameListLength)
{
    int maxWidth = 0;

    for (int index = 0; index < fileNameListLength; index++) {
        int width = fontGetStringWidth(fileNameList[index]);
        if (width > maxWidth) {
            maxWidth = width;
        }
    }

    return maxWidth;
}

// 0x4DC930
int _GNW_process_menu(struc_177* ptr, int i)
{
    // TODO: Incomplete

    return 0;
}

// Calculates max length of string needed to represent a1 or a2.
//
// 0x4DD03C
int _calc_max_field_chars_wcursor(int a1, int a2)
{
    char* str = (char*)internal_malloc(17);
    if (str == NULL) {
        return -1;
    }

    sprintf(str, "%d", a1);
    int len1 = strlen(str);

    sprintf(str, "%d", a2);
    int len2 = strlen(str);

    internal_free(str);

    return std::max(len1, len2) + 1;
}

// 0x4DD3EC
void _GNW_intr_init()
{
    int v1, v2;
    int i;

    _tm_persistence = 3000;
    _tm_add = 0;
    _tm_kill = -1;
    _scr_center_x = _scr_size.right / 2;

    if (_scr_size.bottom >= 479) {
        _tm_text_y = 16;
        _tm_text_x = 16;
    } else {
        _tm_text_y = 10;
        _tm_text_x = 10;
    }

    _tm_h = 2 * _tm_text_y + fontGetLineHeight();

    v1 = _scr_size.bottom >> 3;
    v2 = _scr_size.bottom >> 2;

    for (i = 0; i < 5; i++) {
        _tm_location[i].field_4 = v1 * i + v2;
        _tm_location[i].field_0 = 0;
    }
}

// 0x4DD4A4
void _GNW_intr_exit()
{
    tickersRemove(_tm_watch_msgs);
    while (_tm_kill != -1) {
        _tm_kill_msg();
    }
}

// 0x4DD66C
void _tm_watch_msgs()
{
    if (_tm_watch_active) {
        return;
    }

    _tm_watch_active = 1;
    while (_tm_kill != -1) {
        if (getTicksSince(_tm_queue[_tm_kill].field_0) < _tm_persistence) {
            break;
        }

        _tm_kill_msg();
    }
    _tm_watch_active = 0;
}

// 0x4DD6C0
void _tm_kill_msg()
{
    int v0;

    v0 = _tm_kill;
    if (v0 != -1) {
        windowDestroy(_tm_queue[_tm_kill].field_4);
        _tm_location[_tm_queue[_tm_kill].field_8].field_0 = 0;

        if (v0 == 5) {
            v0 = 0;
        }

        if (v0 == _tm_add) {
            _tm_add = 0;
            _tm_kill = -1;
            tickersRemove(_tm_watch_msgs);
            v0 = _tm_kill;
        }
    }

    _tm_kill = v0;
}

// 0x4DD744
void _tm_kill_out_of_order(int a1)
{
    int v7;
    int v6;

    if (_tm_kill == -1) {
        return;
    }

    if (!_tm_index_active(a1)) {
        return;
    }

    windowDestroy(_tm_queue[a1].field_4);

    _tm_location[_tm_queue[a1].field_8].field_0 = 0;

    if (a1 != _tm_kill) {
        v6 = a1;
        do {
            v7 = v6 - 1;
            if (v7 < 0) {
                v7 = 4;
            }

            memcpy(&(_tm_queue[v6]), &(_tm_queue[v7]), sizeof(STRUCT_6B2370));
            v6 = v7;
        } while (v7 != _tm_kill);
    }

    if (++_tm_kill == 5) {
        _tm_kill = 0;
    }

    if (_tm_add == _tm_kill) {
        _tm_add = 0;
        _tm_kill = -1;
        tickersRemove(_tm_watch_msgs);
    }
}

// 0x4DD82C
void _tm_click_response(int btn)
{
    int win;
    int v3;

    if (_tm_kill == -1) {
        return;
    }

    win = buttonGetWindowId(btn);
    v3 = _tm_kill;
    while (win != _tm_queue[v3].field_4) {
        v3++;
        if (v3 == 5) {
            v3 = 0;
        }

        if (v3 == _tm_kill || !_tm_index_active(v3))
            return;
    }

    _tm_kill_out_of_order(v3);
}

// 0x4DD870
int _tm_index_active(int a1)
{
    if (_tm_kill != _tm_add) {
        if (_tm_kill >= _tm_add) {
            if (a1 >= _tm_add && a1 < _tm_kill)
                return 0;
        } else if (a1 < _tm_kill || a1 >= _tm_add) {
            return 0;
        }
    }
    return 1;
}
