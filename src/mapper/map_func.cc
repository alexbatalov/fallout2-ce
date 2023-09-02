#include "mapper/map_func.h"

#include "memory.h"
#include "proto.h"
#include "svga.h"
#include "window_manager.h"

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
