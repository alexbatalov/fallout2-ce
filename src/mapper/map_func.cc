#include "mapper/map_func.h"

#include "actions.h"
#include "color.h"
#include "game_mouse.h"
#include "input.h"
#include "map.h"
#include "memory.h"
#include "mouse.h"
#include "proto.h"
#include "svga.h"
#include "tile.h"
#include "window_manager.h"
#include "window_manager_private.h"

namespace fallout {

// 0x5595CC
static bool block_obj_view_on = false;

// 0x4825B0
void setup_map_dirs()
{
    // TODO: Incomplete.
}

// 0x4826B4
void copy_proto_lists()
{
    // TODO: Incomplete.
}

// 0x482708
void place_entrance_hex()
{
    int x;
    int y;
    int tile;

    while (inputGetInput() != -2) {
    }

    if ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
        if (_mouse_click_in(0, 0, _scr_size.right - _scr_size.left, _scr_size.bottom - _scr_size.top - 100)) {
            mouseGetPosition(&x, &y);

            tile = tileFromScreenXY(x, y, gElevation);
            if (tile != -1) {
                if (tileSetCenter(tile, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS) == 0) {
                    mapSetEnteringLocation(tile, gElevation, rotation);
                } else {
                    win_timed_msg("ERROR: Entrance out of range!", _colorTable[31744]);
                }
            }
        }
    }
}

// 0x4841C4
void pick_region(Rect* rect)
{
    Rect temp;
    int x;
    int y;

    gameMouseSetCursor(MOUSE_CURSOR_PLUS);
    gameMouseObjectsHide();

    while (1) {
        if (inputGetInput() == -2
            && (mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
            break;
        }
    }

    get_input_position(&x, &y);
    temp.left = x;
    temp.top = y;
    temp.right = x;
    temp.bottom = y;

    while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_UP) == 0) {
        inputGetInput();

        get_input_position(&x, &y);

        if (x != temp.right || y != temp.bottom) {
            erase_rect(rect);
            sort_rect(rect, &temp);
            draw_rect(rect, _colorTable[32747]);
        }
    }

    erase_rect(rect);
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    gameMouseObjectsShow();
}

// 0x484294
void sort_rect(Rect* a, Rect* b)
{
    if (b->right > b->left) {
        a->left = b->left;
        a->right = b->right;
    } else {
        a->left = b->right;
        a->right = b->left;
    }

    if (b->bottom > b->top) {
        a->top = b->top;
        a->bottom = b->bottom;
    } else {
        a->top = b->bottom;
        a->bottom = b->top;
    }
}

// 0x4842D4
void draw_rect(Rect* rect, unsigned char color)
{
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    int max_dimension;

    if (height < width) {
        max_dimension = width;
    } else {
        max_dimension = height;
    }

    unsigned char* buffer = (unsigned char*)internal_malloc(max_dimension);
    if (buffer != NULL) {
        memset(buffer, color, max_dimension);
        _scr_blit(buffer, width, 1, 0, 0, width, 1, rect->left, rect->top);
        _scr_blit(buffer, 1, height, 0, 0, 1, height, rect->left, rect->top);
        _scr_blit(buffer, width, 1, 0, 0, width, 1, rect->left, rect->bottom);
        _scr_blit(buffer, 1, height, 0, 0, 1, height, rect->right, rect->top);
        internal_free(buffer);
    }
}

// 0x4843A0
void erase_rect(Rect* rect)
{
    Rect r = *rect;

    r.bottom = rect->top;
    windowRefreshAll(&r);

    r.bottom = rect->bottom;
    r.left = rect->right;
    windowRefreshAll(&r);

    r.left = rect->left;
    r.top = rect->bottom;
    windowRefreshAll(&r);

    r.top = rect->top;
    r.right = rect->left;
    windowRefreshAll(&r);
}

// 0x484400
int toolbar_proto(int type, int id)
{
    if (id < proto_max_id(type)) {
        return (type << 24) | id;
    } else {
        return -1;
    }
}

// 0x485D44
bool map_toggle_block_obj_viewing_on()
{
    return block_obj_view_on;
}

} // namespace fallout
