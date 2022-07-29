#include "selfrun.h"

#include "core.h"
#include "db.h"
#include "game.h"
#include "game_config.h"
#include "platform_compat.h"

#include <stdlib.h>

// 0x51C8D8
int gSelfrunState = SELFRUN_STATE_TURNED_OFF;

// 0x4A8BE0
int selfrunInitFileList(char*** fileListPtr, int* fileListLengthPtr)
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
int selfrunFreeFileList(char*** fileListPtr)
{
    if (fileListPtr == NULL) {
        return -1;
    }

    fileNameListFree(fileListPtr, 0);

    return 0;
}

// 0x4A8C28
int selfrunPreparePlayback(const char* fileName, SelfrunData* selfrunData)
{
    if (fileName == NULL) {
        return -1;
    }

    if (selfrunData == NULL) {
        return -1;
    }

    if (vcrGetState() != VCR_STATE_TURNED_OFF) {
        return -1;
    }

    if (gSelfrunState != SELFRUN_STATE_TURNED_OFF) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s%s", "selfrun\\", fileName);

    if (selfrunReadData(path, selfrunData) != 0) {
        return -1;
    }

    gSelfrunState = SELFRUN_STATE_PLAYING;

    return 0;
}

// 0x4A8C88
void selfrunPlaybackLoop(SelfrunData* selfrunData)
{
    if (gSelfrunState == SELFRUN_STATE_PLAYING) {
        char path[COMPAT_MAX_PATH];
        sprintf(path, "%s%s", "selfrun\\", selfrunData->recordingFileName);

        if (vcrPlay(path, VCR_TERMINATE_ON_KEY_PRESS | VCR_TERMINATE_ON_MOUSE_PRESS, selfrunPlaybackCompleted)) {
            bool cursorWasHidden = cursorIsHidden();
            if (cursorWasHidden) {
                mouseShowCursor();
            }

            while (gSelfrunState == SELFRUN_STATE_PLAYING) {
                int keyCode = _get_input();
                if (keyCode != selfrunData->stopKeyCode) {
                    gameHandleKey(keyCode, false);
                }
            }

            while (mouseGetEvent() != 0) {
                _get_input();
            }

            if (cursorWasHidden) {
                mouseHideCursor();
            }
        }
    }
}

// 0x4A8D28
int selfrunPrepareRecording(const char* recordingName, const char* mapFileName, SelfrunData* selfrunData)
{
    if (recordingName == NULL) {
        return -1;
    }

    if (mapFileName == NULL) {
        return -1;
    }

    if (vcrGetState() != VCR_STATE_TURNED_OFF) {
        return -1;
    }

    if (gSelfrunState != SELFRUN_STATE_TURNED_OFF) {
        return -1;
    }

    sprintf(selfrunData->recordingFileName, "%s%s", recordingName, ".vcr");
    strcpy(selfrunData->mapFileName, mapFileName);

    selfrunData->stopKeyCode = KEY_CTRL_R;

    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s%s%s", "selfrun\\", recordingName, ".sdf");

    if (selfrunWriteData(path, selfrunData) != 0) {
        return -1;
    }

    gSelfrunState = SELFRUN_STATE_RECORDING;

    return 0;
}

// 0x4A8DDC
void selfrunRecordingLoop(SelfrunData* selfrunData)
{
    if (gSelfrunState == SELFRUN_STATE_RECORDING) {
        char path[COMPAT_MAX_PATH];
        sprintf(path, "%s%s", "selfrun\\", selfrunData->recordingFileName);
        if (vcrRecord(path)) {
            if (!cursorIsHidden()) {
                mouseShowCursor();
            }

            bool done = false;
            while (!done) {
                int keyCode = _get_input();
                if (keyCode == selfrunData->stopKeyCode) {
                    vcrStop();
                    _game_user_wants_to_quit = 2;
                    done = true;
                } else {
                    gameHandleKey(keyCode, false);
                }
            }
        }
        gSelfrunState = SELFRUN_STATE_TURNED_OFF;
    }
}

// 0x4A8E74
void selfrunPlaybackCompleted(int reason)
{
    _game_user_wants_to_quit = 2;
    gSelfrunState = SELFRUN_STATE_TURNED_OFF;
}

// 0x4A8E8C
int selfrunReadData(const char* path, SelfrunData* selfrunData)
{
    if (path == NULL) {
        return -1;
    }

    if (selfrunData == NULL) {
        return -1;
    }

    File* stream = fileOpen(path, "rb");
    if (stream == NULL) {
        return -1;
    }

    int rc = -1;
    if (fileReadFixedLengthString(stream, selfrunData->recordingFileName, SELFRUN_RECORDING_FILE_NAME_LENGTH) == 0
        && fileReadFixedLengthString(stream, selfrunData->mapFileName, SELFRUN_MAP_FILE_NAME_LENGTH) == 0
        && fileReadInt32(stream, &(selfrunData->stopKeyCode)) == 0) {
        rc = 0;
    }

    fileClose(stream);

    return rc;
}

// 0x4A8EF4
int selfrunWriteData(const char* path, SelfrunData* selfrunData)
{
    if (path == NULL) {
        return -1;
    }

    if (selfrunData == NULL) {
        return -1;
    }

    char* masterPatches;
    configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &masterPatches);

    char selfrunDirectoryPath[COMPAT_MAX_PATH];
    sprintf(selfrunDirectoryPath, "%s\\%s", masterPatches, "selfrun\\");

    compat_mkdir(selfrunDirectoryPath);

    File* stream = fileOpen(path, "wb");
    if (stream == NULL) {
        return -1;
    }

    int rc = -1;
    if (fileWriteFixedLengthString(stream, selfrunData->recordingFileName, SELFRUN_RECORDING_FILE_NAME_LENGTH) == 0
        && fileWriteFixedLengthString(stream, selfrunData->mapFileName, SELFRUN_MAP_FILE_NAME_LENGTH) == 0
        && fileWriteInt32(stream, selfrunData->stopKeyCode) == 0) {
        rc = 0;
    }

    fileClose(stream);

    return rc;
}
