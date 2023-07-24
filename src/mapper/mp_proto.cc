#include "mapper/mp_proto.h"

#include <string.h>

#include "color.h"
#include "combat_ai.h"
#include "memory.h"
#include "window_manager_private.h"

namespace fallout {

// 0x559C60
bool can_modify_protos = false;

// 0x4922F8
void init_mapper_protos()
{
    // TODO: Incomplete.
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
