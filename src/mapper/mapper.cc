#include "mapper/mapper.h"

#include "object.h"
#include "proto.h"

namespace fallout {

static void mapper_mark_all_exit_grids();

// 0x559748
MapTransition mapInfo = { -1, -1, 0, 0 };

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
