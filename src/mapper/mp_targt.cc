#include "mapper/mp_targt.h"

#include <string.h>

#include "art.h"
#include "game.h"
#include "map.h"
#include "mapper/mp_proto.h"
#include "proto.h"
#include "window_manager_private.h"

namespace fallout {

#define TARGET_DAT "target.dat"

typedef struct TargetList {
    int field_0;
    int field_4;
    int field_8;
} TargetList;

// 0x53F354
static char default_target_path_base[] = "\\fallout2\\dev\\proto\\";

// 0x559CC4
static TargetList targetlist = { 0 };

// 0x559CD0
static char* target_path_base = default_target_path_base;

// 0x559DBC
static bool tgt_overriden = false;

// 0x49B2F0
void target_override_protection()
{
    char* name;

    tgt_overriden = true;

    name = getenv("MAIL_NAME");
    if (name != NULL) {
        // NOTE: Unsafe, backing buffer is 32 byte max.
        strcpy(proto_builder_name, strupr(name));
    } else {
        strcpy(proto_builder_name, "UNKNOWN");
    }
}

// 0x49B2F0
bool target_overriden()
{
    return tgt_overriden;
}

// 0x49B34C
void target_make_path(char* path, int pid)
{
    if (_cd_path_base[0] != '\0' && _cd_path_base[1] == ':') {
        strncpy(path, _cd_path_base, 2);
        strcat(path, target_path_base);
    } else {
        strcpy(path, target_path_base);
    }

    if (pid != -1) {
        strcat(path, artGetObjectTypeName(PID_TYPE(pid)));
    }
}

// 0x49B424
int target_init()
{
    // TODO: Incomplete.

    return 0;
}

// 0x49B434
int target_exit()
{
    // TODO: Incomplete.

    return 0;
}

// 0x49B454
int target_header_save()
{
    char path[COMPAT_MAX_PATH];
    FILE* stream;

    target_make_path(path, -1);
    strcat(path, TARGET_DAT);

    stream = fopen(path, "wb");
    if (stream == NULL) {
        return -1;
    }

    if (fwrite(&targetlist, sizeof(targetlist), 1, stream) != 1) {
        // FIXME: Leaking `stream`.
        return -1;
    }

    fclose(stream);
    return 0;
}

// 0x49B4E8
int target_header_load()
{
    char path[COMPAT_MAX_PATH];
    FILE* stream;

    target_make_path(path, -1);
    strcat(path, TARGET_DAT);

    stream = fopen(path, "rb");
    if (stream == NULL) {
        return -1;
    }

    if (fread(&targetlist, sizeof(targetlist), 1, stream) != 1) {
        // FIXME: Leaking `stream`.
        return -1;
    }

    targetlist.field_0 = 0;
    targetlist.field_4 = 0;

    fclose(stream);
    return 0;
}

// 0x49BD98
int pick_rot()
{
    int value;
    win_get_num_i(&value,
        -1,
        5,
        false,
        "Rotation",
        100,
        100);
    return value;
}

// 0x49BDD0
int target_pick_global_var(int* value_ptr)
{
    int value;
    int rc;

    if (gGameGlobalVarsLength == 0) {
        return -1;
    }

    rc = win_get_num_i(&value,
        0,
        gGameGlobalVarsLength - 1,
        false,
        "Global Variable Index #:",
        100,
        100);
    if (rc == -1) {
        return -1;
    }

    *value_ptr = value;
    return 0;
}

// 0x49BE20
int target_pick_map_var(int* value_ptr)
{
    int value;
    int rc;

    if (gMapGlobalVarsLength == 0) {
        return -1;
    }

    rc = win_get_num_i(&value,
        0,
        gMapGlobalVarsLength - 1,
        false,
        "Map Variable Index #:",
        100,
        100);
    if (rc == -1) {
        return -1;
    }

    *value_ptr = value;
    return 0;
}

// 0x49BE70
int target_pick_local_var(int* value_ptr)
{
    int value;
    int rc;

    if (gMapLocalVarsLength == 0) {
        return -1;
    }

    rc = win_get_num_i(&value,
        0,
        gMapLocalVarsLength - 1,
        false,
        "Local Variable Index #:",
        100,
        100);
    if (rc == -1) {
        return -1;
    }

    *value_ptr = value;
    return 0;
}

} // namespace fallout
