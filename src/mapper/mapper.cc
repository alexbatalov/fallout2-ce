#include "mapper/mapper.h"

#include <ctype.h>

#include "animation.h"
#include "art.h"
#include "color.h"
#include "draw.h"
#include "game_mouse.h"
#include "inventory.h"
#include "mapper/mp_proto.h"
#include "object.h"
#include "proto.h"
#include "settings.h"
#include "svga.h"
#include "window_manager.h"

namespace fallout {

static int categoryUnhide();
static bool proto_user_is_librarian();
static void redraw_toolname();
static void clear_toolname();
static void update_high_obj_name(Object* obj);
static int mapper_mark_exit_grid();
static void mapper_mark_all_exit_grids();

// 0x559748
MapTransition mapInfo = { -1, -1, 0, 0 };

// 0x5598AC
static int categoryWin = -1;

// 0x5598B0
static bool categoryIsHidden = false;

// 0x6EC4A8
unsigned char* tool;

// 0x6EC4AC
int tool_win;

// 0x487774
int categoryUnhide()
{
    if (categoryWin == -1) {
        return -1;
    }

    windowShow(categoryWin);
    categoryIsHidden = false;

    return 0;
}

// 0x487784
bool proto_user_is_librarian()
{
    if (!settings.mapper.librarian) {
        return false;
    }

    can_modify_protos = true;
    return true;
}

// 0x48B16C
void print_toolbar_name(int object_type)
{
    Rect rect;
    char name[80];

    rect.left = 0;
    rect.top = 0;
    rect.right = 0;
    rect.bottom = 22;
    bufferFill(tool + 2 + 2 * (_scr_size.right - _scr_size.left) + 2,
        96,
        _scr_size.right - _scr_size.left + 1,
        19,
        _colorTable[21140]);

    sprintf(name, "%s", artGetObjectTypeName(object_type));
    name[0] = toupper(name[0]);
    windowDrawText(tool_win, name, 0, 7, 7, _colorTable[32747] | 0x2000000);
    windowRefreshRect(tool_win, &rect);
}

// 0x48B230
void redraw_toolname()
{
    Rect rect;

    rect.left = _scr_size.right - _scr_size.left - 149;
    rect.top = 60;
    rect.right = _scr_size.right - _scr_size.left + 1;
    rect.bottom = 95;
    windowRefreshRect(tool_win, &rect);
}

// 0x48B278
void clear_toolname()
{
    windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 60, 260);
    windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 70, 260);
    windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 80, 260);
    redraw_toolname();
}

// 0x48B5BC
void update_high_obj_name(Object* obj)
{
    Proto* proto;

    if (protoGetProto(obj->pid, &proto) != -1) {
        windowDrawText(tool_win, protoGetName(obj->pid), 120, _scr_size.right - _scr_size.left - 149, 60, 260);
        windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 70, 260);
        windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 80, 260);
        redraw_toolname();
    }
}

// 0x48C604
int mapper_inven_unwield(Object* obj, int right_hand)
{
    Object* item;
    int fid;

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);

    if (right_hand) {
        item = critterGetItem2(obj);
    } else {
        item = critterGetItem1(obj);
    }

    if (item != NULL) {
        item->flags &= ~OBJECT_IN_ANY_HAND;
    }

    animationRegisterAnimate(obj, ANIM_PUT_AWAY, 0);

    fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, 0, 0, (obj->fid & 0x70000000) >> 28);
    animationRegisterSetFid(obj, fid, 0);

    return reg_anim_end();
}

// 0x48C678
int mapper_mark_exit_grid()
{
    int y;
    int x;
    int tile;
    Object* obj;

    for (y = -2000; y != 2000; y += 200) {
        for (x = -10; x < 10; x++) {
            tile = gGameMouseBouncingCursor->tile + y + x;

            obj = objectFindFirstAtElevation(gElevation);
            while (obj != NULL) {
                if (isExitGridPid(obj->pid)) {
                    obj->data.misc.map = mapInfo.map;
                    obj->data.misc.tile = mapInfo.tile;
                    obj->data.misc.elevation = mapInfo.elevation;
                    obj->data.misc.rotation = mapInfo.rotation;
                }
                obj = objectFindNextAtElevation();
            }
        }
    }

    return 0;
}

// 0x48C704
void mapper_mark_all_exit_grids()
{
    Object* obj;

    obj = objectFindFirstAtElevation(gElevation);
    while (obj != NULL) {
        if (isExitGridPid(obj->pid)) {
            obj->data.misc.map = mapInfo.map;
            obj->data.misc.tile = mapInfo.tile;
            obj->data.misc.elevation = mapInfo.elevation;
            obj->data.misc.rotation = mapInfo.rotation;
        }
        obj = objectFindNextAtElevation();
    }
}

} // namespace fallout
