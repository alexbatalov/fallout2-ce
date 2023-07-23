#include "mapper/mapper.h"

#include "game_mouse.h"
#include "object.h"
#include "proto.h"

namespace fallout {

static int mapper_mark_exit_grid();
static void mapper_mark_all_exit_grids();

// 0x559748
MapTransition mapInfo = { -1, -1, 0, 0 };

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
