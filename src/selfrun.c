#include "selfrun.h"

#include "db.h"
#include "game.h"

#include <stdlib.h>

// 0x51C8D8
int _selfrun_state = 0;

// 0x4A8BE0
int _selfrun_get_list(char*** fileListPtr, int* fileListLengthPtr)
{
    if (fileListPtr == NULL) {
        return -1;
    }

    if (fileListLengthPtr == NULL) {
        return -1;
    }

    *fileListLengthPtr = fileNameListInit("selfrun\\*.sdf", fileListPtr, 0, 0);

    return 0;
}

// 0x4A8C10
int _selfrun_free_list(char*** fileListPtr)
{
    if (fileListPtr == NULL) {
        return -1;
    }

    fileNameListFree(fileListPtr, 0);

    return 0;
}

// 0x4A8E74
void _selfrun_playback_callback()
{
    _game_user_wants_to_quit = 2;
    _selfrun_state = 0;
}
