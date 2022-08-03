#include "elevator.h"

#include "art.h"
#include "core.h"
#include "cycle.h"
#include "debug.h"
#include "draw.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "geometry.h"
#include "interface.h"
#include "map.h"
#include "pipboy.h"
#include "scripts.h"
#include "sfall_config.h"
#include "window_manager.h"

#include <ctype.h>
#include <string.h>

#include <algorithm>

// The maximum number of elevator levels.
#define ELEVATOR_LEVEL_MAX (4)

// NOTE: There are two variables which hold background data used in the
// elevator window - [gElevatorBackgroundFrmData] and [gElevatorPanelFrmData].
// For unknown reason they are using -1 to denote that they are not set
// (instead of using NULL).
#define ELEVATOR_BACKGROUND_NULL ((unsigned char*)(-1))

// Max number of elevators that can be loaded from elevators.ini. This limit is
// emposed by Sfall.
#define ELEVATORS_MAX 50

typedef enum ElevatorFrm {
    ELEVATOR_FRM_BUTTON_DOWN,
    ELEVATOR_FRM_BUTTON_UP,
    ELEVATOR_FRM_GAUGE,
    ELEVATOR_FRM_COUNT,
} ElevatorFrm;

typedef struct ElevatorBackground {
    int backgroundFrmId;
    int panelFrmId;
} ElevatorBackground;

typedef struct ElevatorDescription {
    int map;
    int elevation;
    int tile;
} ElevatorDescription;

static int elevatorWindowInit(int elevator);
static void elevatorWindowFree();
static int elevatorGetLevelFromKeyCode(int elevator, int keyCode);

// 0x43E950
static const int gElevatorFrmIds[ELEVATOR_FRM_COUNT] = {
    141, // ebut_in.frm - map elevator screen
    142, // ebut_out.frm - map elevator screen
    149, // gaj000.frm - map elevator screen
};

// 0x43E95C
static ElevatorBackground gElevatorBackgrounds[ELEVATORS_MAX] = {
    { 143, -1 },
    { 143, 150 },
    { 144, -1 },
    { 144, 145 },
    { 146, -1 },
    { 146, 147 },
    { 146, -1 },
    { 146, 151 },
    { 148, -1 },
    { 146, -1 },
    { 146, -1 },
    { 146, 147 },
    { 388, -1 },
    { 143, 150 },
    { 148, -1 },
    { 148, -1 },
    { 148, -1 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
};

// Number of levels for eleveators.
//
// 0x43EA1C
static int gElevatorLevels[ELEVATORS_MAX] = {
    4,
    2,
    3,
    2,
    3,
    2,
    3,
    3,
    3,
    3,
    3,
    2,
    4,
    2,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

// 0x43EA7C
static ElevatorDescription gElevatorDescriptions[ELEVATORS_MAX][ELEVATOR_LEVEL_MAX] = {
    {
        { 14, 0, 18940 },
        { 14, 1, 18936 },
        { 15, 0, 21340 },
        { 15, 1, 21340 },
    },
    {
        { 13, 0, 20502 },
        { 14, 0, 14912 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 33, 0, 12498 },
        { 33, 1, 20094 },
        { 34, 0, 17312 },
        { 0, 0, -1 },
    },
    {
        { 34, 0, 16140 },
        { 34, 1, 16140 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 49, 0, 14920 },
        { 49, 1, 15120 },
        { 50, 0, 12944 },
        { 0, 0, -1 },
    },
    {
        { 50, 0, 24520 },
        { 50, 1, 25316 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 42, 0, 22526 },
        { 42, 1, 22526 },
        { 42, 2, 22526 },
        { 0, 0, -1 },
    },
    {
        { 42, 2, 14086 },
        { 43, 0, 14086 },
        { 43, 2, 14086 },
        { 0, 0, -1 },
    },
    {
        { 40, 0, 14104 },
        { 40, 1, 22504 },
        { 40, 2, 17312 },
        { 0, 0, -1 },
    },
    {
        { 9, 0, 13704 },
        { 9, 1, 23302 },
        { 9, 2, 17308 },
        { 0, 0, -1 },
    },
    {
        { 28, 0, 19300 },
        { 28, 1, 19300 },
        { 28, 2, 20110 },
        { 0, 0, -1 },
    },
    {
        { 28, 2, 20118 },
        { 29, 0, 21710 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 28, 0, 20122 },
        { 28, 1, 20124 },
        { 28, 2, 20940 },
        { 29, 0, 22540 },
    },
    {
        { 12, 1, 16052 },
        { 12, 2, 14480 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 6, 0, 14104 },
        { 6, 1, 22504 },
        { 6, 2, 17312 },
        { 0, 0, -1 },
    },
    {
        { 30, 0, 14104 },
        { 30, 1, 22504 },
        { 30, 2, 17312 },
        { 0, 0, -1 },
    },
    {
        { 36, 0, 13704 },
        { 36, 1, 23302 },
        { 36, 2, 17308 },
        { 0, 0, -1 },
    },
    {
        { 39, 0, 17285 },
        { 36, 0, 19472 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 109, 2, 10701 },
        { 109, 1, 10705 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 109, 2, 14697 },
        { 109, 1, 15099 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 109, 2, 23877 },
        { 109, 1, 25481 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 109, 2, 26130 },
        { 109, 1, 24721 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 137, 0, 23953 },
        { 148, 1, 16526 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 62, 0, 13901 },
        { 63, 1, 17923 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
};

// NOTE: These values are also used as key bindings.
//
// 0x43EEFC
static char gElevatorLevelLabels[ELEVATORS_MAX][ELEVATOR_LEVEL_MAX] = {
    { '1', '2', '3', '4' },
    { 'G', '1', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '6', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '\0', '\0' },
    { '1', '2', '3', '4' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
};

// 0x51862C
static const char* gElevatorSoundEffects[ELEVATOR_LEVEL_MAX - 1][ELEVATOR_LEVEL_MAX] = {
    {
        "ELV1_1",
        "ELV1_1",
        "ERROR",
        "ERROR",
    },
    {
        "ELV1_2",
        "ELV1_2",
        "ELV1_1",
        "ERROR",
    },
    {
        "ELV1_3",
        "ELV1_3",
        "ELV2_3",
        "ELV1_1",
    },
};

// 0x570A2C
static Size gElevatorFrmSizes[ELEVATOR_FRM_COUNT];

// 0x570A44
static int gElevatorBackgroundFrmWidth;

// 0x570A48
static int gElevatorBackgroundFrmHeight;

// 0x570A4C
static int gElevatorPanelFrmWidth;

// 0x570A50
static int gElevatorPanelFrmHeight;

// 0x570A54
static int gElevatorWindow;

// 0x570A58
static CacheEntry* gElevatorFrmHandles[ELEVATOR_FRM_COUNT];

// 0x570A64
static CacheEntry* gElevatorBackgroundFrmHandle;

// 0x570A68
static CacheEntry* gElevatorPanelFrmHandle;

// 0x570A6C
static unsigned char* gElevatorWindowBuffer;

// 0x570A70
static bool gElevatorWindowIsoWasEnabled;

// 0x570A74
static unsigned char* gElevatorFrmData[ELEVATOR_FRM_COUNT];

// 0x570A80
static unsigned char* gElevatorBackgroundFrmData;

// 0x570A84
static unsigned char* gElevatorPanelFrmData;

// Presents elevator dialog for player to pick a desired level.
//
// 0x43EF5C
int elevatorSelectLevel(int elevator, int* mapPtr, int* elevationPtr, int* tilePtr)
{
    if (elevator < 0 || elevator >= ELEVATORS_MAX) {
        return -1;
    }

    // SFALL
    if (elevatorWindowInit(elevator) == -1) {
        return -1;
    }

    const ElevatorDescription* elevatorDescription = gElevatorDescriptions[elevator];

    int index;
    for (index = 0; index < ELEVATOR_LEVEL_MAX; index++) {
        if (elevatorDescription[index].map == *mapPtr) {
            break;
        }
    }

    if (index < ELEVATOR_LEVEL_MAX) {
        if (elevatorDescription[*elevationPtr + index].tile != -1) {
            *elevationPtr += index;
        }
    }

    if (elevator == ELEVATOR_SIERRA_2) {
        if (*elevationPtr <= 2) {
            *elevationPtr -= 2;
        } else {
            *elevationPtr -= 3;
        }
    } else if (elevator == ELEVATOR_MILITARY_BASE_LOWER) {
        if (*elevationPtr >= 2) {
            *elevationPtr -= 2;
        }
    } else if (elevator == ELEVATOR_MILITARY_BASE_UPPER && *elevationPtr == 4) {
        *elevationPtr -= 2;
    }

    if (*elevationPtr > 3) {
        *elevationPtr -= 3;
    }

    debugPrint("\n the start elev level %d\n", *elevationPtr);

    int v18 = (gElevatorFrmSizes[ELEVATOR_FRM_GAUGE].width * gElevatorFrmSizes[ELEVATOR_FRM_GAUGE].height) / 13;
    float v42 = 12.0f / (float)(gElevatorLevels[elevator] - 1);
    blitBufferToBuffer(
        gElevatorFrmData[ELEVATOR_FRM_GAUGE] + v18 * (int)((float)(*elevationPtr) * v42),
        gElevatorFrmSizes[ELEVATOR_FRM_GAUGE].width,
        gElevatorFrmSizes[ELEVATOR_FRM_GAUGE].height / 13,
        gElevatorFrmSizes[ELEVATOR_FRM_GAUGE].width,
        gElevatorWindowBuffer + gElevatorBackgroundFrmWidth * 41 + 121,
        gElevatorBackgroundFrmWidth);
    windowRefresh(gElevatorWindow);

    bool done = false;
    int keyCode;
    while (!done) {
        keyCode = _get_input();
        if (keyCode == KEY_ESCAPE) {
            done = true;
        }

        if (keyCode >= 500 && keyCode < 504) {
            done = true;
        }

        if (keyCode > 0 && keyCode < 500) {
            int level = elevatorGetLevelFromKeyCode(elevator, keyCode);
            if (level != 0) {
                keyCode = 500 + level - 1;
                done = true;
            }
        }
    }

    if (keyCode != KEY_ESCAPE) {
        keyCode -= 500;

        if (*elevationPtr != keyCode) {
            float v43 = (float)(gElevatorLevels[elevator] - 1) / 12.0f;

            unsigned int delay = (unsigned int)(v43 * 276.92307);

            if (keyCode < *elevationPtr) {
                v43 = -v43;
            }

            int numberOfLevelsTravelled = keyCode - *elevationPtr;
            if (numberOfLevelsTravelled < 0) {
                numberOfLevelsTravelled = -numberOfLevelsTravelled;
            }

            soundPlayFile(gElevatorSoundEffects[gElevatorLevels[elevator] - 2][numberOfLevelsTravelled]);

            float v41 = (float)keyCode * v42;
            float v44 = (float)(*elevationPtr) * v42;
            do {
                unsigned int tick = _get_time();
                v44 += v43;
                blitBufferToBuffer(
                    gElevatorFrmData[ELEVATOR_FRM_GAUGE] + v18 * (int)v44,
                    gElevatorFrmSizes[ELEVATOR_FRM_GAUGE].width,
                    gElevatorFrmSizes[ELEVATOR_FRM_GAUGE].height / 13,
                    gElevatorFrmSizes[ELEVATOR_FRM_GAUGE].width,
                    gElevatorWindowBuffer + gElevatorBackgroundFrmWidth * 41 + 121,
                    gElevatorBackgroundFrmWidth);

                windowRefresh(gElevatorWindow);

                while (getTicksSince(tick) < delay) {
                }
            } while ((v43 <= 0.0 || v44 < v41) && (v43 > 0.0 || v44 > v41));

            coreDelayProcessingEvents(200);
        }
    }

    elevatorWindowFree();

    if (keyCode != KEY_ESCAPE) {
        const ElevatorDescription* description = &(elevatorDescription[keyCode]);
        *mapPtr = description->map;
        *elevationPtr = description->elevation;
        *tilePtr = description->tile;
    }

    return 0;
}

// 0x43F324
static int elevatorWindowInit(int elevator)
{
    gElevatorWindowIsoWasEnabled = isoDisable();
    colorCycleDisable();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    gameMouseObjectsHide();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    scriptsDisable();

    int index;
    for (index = 0; index < ELEVATOR_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gElevatorFrmIds[index], 0, 0, 0);
        gElevatorFrmData[index] = artLockFrameDataReturningSize(fid, &(gElevatorFrmHandles[index]), &(gElevatorFrmSizes[index].width), &(gElevatorFrmSizes[index].height));
        if (gElevatorFrmData[index] == NULL) {
            break;
        }
    }

    if (index != ELEVATOR_FRM_COUNT) {
        for (int reversedIndex = index - 1; reversedIndex >= 0; reversedIndex--) {
            artUnlock(gElevatorFrmHandles[reversedIndex]);
        }

        if (gElevatorWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    gElevatorPanelFrmData = ELEVATOR_BACKGROUND_NULL;
    gElevatorBackgroundFrmData = ELEVATOR_BACKGROUND_NULL;

    const ElevatorBackground* elevatorBackground = &(gElevatorBackgrounds[elevator]);
    bool backgroundsLoaded = true;

    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, elevatorBackground->backgroundFrmId, 0, 0, 0);
    gElevatorBackgroundFrmData = artLockFrameDataReturningSize(backgroundFid, &gElevatorBackgroundFrmHandle, &gElevatorBackgroundFrmWidth, &gElevatorBackgroundFrmHeight);
    if (gElevatorBackgroundFrmData != NULL) {
        if (elevatorBackground->panelFrmId != -1) {
            int panelFid = buildFid(OBJ_TYPE_INTERFACE, elevatorBackground->panelFrmId, 0, 0, 0);
            gElevatorPanelFrmData = artLockFrameDataReturningSize(panelFid, &gElevatorPanelFrmHandle, &gElevatorPanelFrmWidth, &gElevatorPanelFrmHeight);
            if (gElevatorPanelFrmData == NULL) {
                gElevatorPanelFrmData = ELEVATOR_BACKGROUND_NULL;
                backgroundsLoaded = false;
            }
        }
    } else {
        gElevatorBackgroundFrmData = ELEVATOR_BACKGROUND_NULL;
        backgroundsLoaded = false;
    }

    if (!backgroundsLoaded) {
        if (gElevatorBackgroundFrmData != ELEVATOR_BACKGROUND_NULL) {
            artUnlock(gElevatorBackgroundFrmHandle);
        }

        if (gElevatorPanelFrmData != ELEVATOR_BACKGROUND_NULL) {
            artUnlock(gElevatorPanelFrmHandle);
        }

        for (int index = 0; index < ELEVATOR_FRM_COUNT; index++) {
            artUnlock(gElevatorFrmHandles[index]);
        }

        if (gElevatorWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    int elevatorWindowX = (screenGetWidth() - gElevatorBackgroundFrmWidth) / 2;
    int elevatorWindowY = (screenGetHeight() - INTERFACE_BAR_HEIGHT - 1 - gElevatorBackgroundFrmHeight) / 2;
    gElevatorWindow = windowCreate(
        elevatorWindowX,
        elevatorWindowY,
        gElevatorBackgroundFrmWidth,
        gElevatorBackgroundFrmHeight,
        256,
        WINDOW_FLAG_0x10 | WINDOW_FLAG_0x02);
    if (gElevatorWindow == -1) {
        if (gElevatorBackgroundFrmData != ELEVATOR_BACKGROUND_NULL) {
            artUnlock(gElevatorBackgroundFrmHandle);
        }

        if (gElevatorPanelFrmData != ELEVATOR_BACKGROUND_NULL) {
            artUnlock(gElevatorPanelFrmHandle);
        }

        for (int index = 0; index < ELEVATOR_FRM_COUNT; index++) {
            artUnlock(gElevatorFrmHandles[index]);
        }

        if (gElevatorWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    gElevatorWindowBuffer = windowGetBuffer(gElevatorWindow);
    memcpy(gElevatorWindowBuffer, (unsigned char*)gElevatorBackgroundFrmData, gElevatorBackgroundFrmWidth * gElevatorBackgroundFrmHeight);

    if (gElevatorPanelFrmData != ELEVATOR_BACKGROUND_NULL) {
        blitBufferToBuffer((unsigned char*)gElevatorPanelFrmData,
            gElevatorPanelFrmWidth,
            gElevatorPanelFrmHeight,
            gElevatorPanelFrmWidth,
            gElevatorWindowBuffer + gElevatorBackgroundFrmWidth * (gElevatorBackgroundFrmHeight - gElevatorPanelFrmHeight),
            gElevatorBackgroundFrmWidth);
    }

    int y = 40;
    for (int level = 0; level < gElevatorLevels[elevator]; level++) {
        int btn = buttonCreate(gElevatorWindow,
            13,
            y,
            gElevatorFrmSizes[ELEVATOR_FRM_BUTTON_DOWN].width,
            gElevatorFrmSizes[ELEVATOR_FRM_BUTTON_DOWN].height,
            -1,
            -1,
            -1,
            500 + level,
            gElevatorFrmData[ELEVATOR_FRM_BUTTON_UP],
            gElevatorFrmData[ELEVATOR_FRM_BUTTON_DOWN],
            NULL,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_red_butt_press, NULL);
        }
        y += 60;
    }

    return 0;
}

// 0x43F6D0
static void elevatorWindowFree()
{
    windowDestroy(gElevatorWindow);

    if (gElevatorBackgroundFrmData != ELEVATOR_BACKGROUND_NULL) {
        artUnlock(gElevatorBackgroundFrmHandle);
    }

    if (gElevatorPanelFrmData != ELEVATOR_BACKGROUND_NULL) {
        artUnlock(gElevatorPanelFrmHandle);
    }

    for (int index = 0; index < ELEVATOR_FRM_COUNT; index++) {
        artUnlock(gElevatorFrmHandles[index]);
    }

    scriptsEnable();

    if (gElevatorWindowIsoWasEnabled) {
        isoEnable();
    }

    colorCycleEnable();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
}

// 0x43F73C
static int elevatorGetLevelFromKeyCode(int elevator, int keyCode)
{
    // TODO: Check if result is really unused?
    toupper(keyCode);

    for (int index = 0; index < ELEVATOR_LEVEL_MAX; index++) {
        char c = gElevatorLevelLabels[elevator][index];
        if (c == '\0') {
            break;
        }

        if (c == (char)(keyCode & 0xFF)) {
            return index + 1;
        }
    }
    return 0;
}

void elevatorsInit()
{
    char* elevatorsFileName;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_ELEVATORS_FILE_KEY, &elevatorsFileName);
    if (elevatorsFileName != NULL && *elevatorsFileName == '\0') {
        elevatorsFileName = NULL;
    }

    if (elevatorsFileName != NULL) {
        Config elevatorsConfig;
        if (configInit(&elevatorsConfig)) {
            if (configRead(&elevatorsConfig, elevatorsFileName, false)) {
                char sectionKey[4];
                char key[32];
                for (int index = 0; index < ELEVATORS_MAX; index++) {
                    sprintf(sectionKey, "%d", index);

                    if (index >= ELEVATOR_COUNT) {
                        int levels = 0;
                        configGetInt(&elevatorsConfig, sectionKey, "ButtonCount", &levels);
                        gElevatorLevels[index] = std::clamp(levels, 2, ELEVATOR_LEVEL_MAX);
                    }

                    configGetInt(&elevatorsConfig, sectionKey, "MainFrm", &(gElevatorBackgrounds[index].backgroundFrmId));
                    configGetInt(&elevatorsConfig, sectionKey, "ButtonsFrm", &(gElevatorBackgrounds[index].panelFrmId));

                    for (int level = 0; level < ELEVATOR_LEVEL_MAX; level++) {
                        sprintf(key, "ID%d", level + 1);
                        configGetInt(&elevatorsConfig, sectionKey, key, &(gElevatorDescriptions[index][level].map));

                        sprintf(key, "Elevation%d", level + 1);
                        configGetInt(&elevatorsConfig, sectionKey, key, &(gElevatorDescriptions[index][level].elevation));

                        sprintf(key, "Tile%d", level + 1);
                        configGetInt(&elevatorsConfig, sectionKey, key, &(gElevatorDescriptions[index][level].tile));
                    }
                }

                // NOTE: Sfall implementation is slightly different. It uses one
                // loop and stores `type` value in a separate lookup table. This
                // value is then used in the certain places to remap from
                // requested elevator to the new one.
                for (int index = 0; index < ELEVATORS_MAX; index++) {
                    sprintf(sectionKey, "%d", index);

                    int type;
                    if (configGetInt(&elevatorsConfig, sectionKey, "Image", &type)) {
                        type = std::clamp(type, 0, ELEVATORS_MAX - 1);
                        if (index != type) {
                            memcpy(&(gElevatorBackgrounds[index]), &(gElevatorBackgrounds[type]), sizeof(*gElevatorBackgrounds));
                            memcpy(&(gElevatorLevels[index]), &(gElevatorLevels[type]), sizeof(*gElevatorLevels));
                            memcpy(&(gElevatorLevelLabels[index]), &(gElevatorLevelLabels[type]), sizeof(*gElevatorLevelLabels));
                        }
                    }
                }
            }

            configFree(&elevatorsConfig);
        }
    }
}
