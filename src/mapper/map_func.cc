#include "mapper/map_func.h"

#include "proto.h"
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
