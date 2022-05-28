#include "skilldex.h"

#include "color.h"
#include "core.h"
#include "cycle.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "interface.h"
#include "map.h"
#include "memory.h"
#include "object.h"
#include "platform_compat.h"
#include "skill.h"
#include "text_font.h"
#include "window_manager.h"

#include <stdio.h>
#include <string.h>

#define SKILLDEX_WINDOW_RIGHT_MARGIN 4
#define SKILLDEX_WINDOW_BOTTOM_MARGIN 6

// 0x51D43C
bool gSkilldexWindowIsoWasEnabled = false;

// 0x51D440
const int gSkilldexFrmIds[SKILLDEX_FRM_COUNT] = {
    121,
    119,
    120,
    8,
    9,
    170,
};

// Maps Skilldex options into skills.
//
// 0x51D458
const int gSkilldexSkills[SKILLDEX_SKILL_COUNT] = {
    SKILL_SNEAK,
    SKILL_LOCKPICK,
    SKILL_STEAL,
    SKILL_TRAPS,
    SKILL_FIRST_AID,
    SKILL_DOCTOR,
    SKILL_SCIENCE,
    SKILL_REPAIR,
};

// 0x668088
Size gSkilldexFrmSizes[SKILLDEX_FRM_COUNT];

// 0x6680B8
unsigned char* gSkilldexButtonsData[SKILLDEX_SKILL_BUTTON_BUFFER_COUNT];

// skilldex.msg
// 0x6680F8
MessageList gSkilldexMessageList;

// 0x668100
MessageListItem gSkilldexMessageListItem;

// 0x668110
unsigned char* gSkilldexFrmData[SKILLDEX_FRM_COUNT];

// 0x668128
CacheEntry* gSkilldexFrmHandles[SKILLDEX_FRM_COUNT];

// 0x668140
int gSkilldexWindow;

// 0x668144
unsigned char* gSkilldexWindowBuffer;

// 0x668148
int gSkilldexWindowOldFont;

// skilldex_select
// 0x4ABFD0
int skilldexOpen()
{
    if (skilldexWindowInit() == -1) {
        debugPrint("\n ** Error loading skilldex dialog data! **\n");
        return -1;
    }

    int rc = -1;
    while (rc == -1) {
        int keyCode = _get_input();

        if (keyCode == KEY_ESCAPE || keyCode == 500 || _game_user_wants_to_quit != 0) {
            rc = 0;
        } else if (keyCode == KEY_RETURN) {
            soundPlayFile("ib1p1xx1");
            rc = 0;
        } else if (keyCode >= 501 && keyCode <= 509) {
            rc = keyCode - 500;
        }
    }

    if (rc != 0) {
        coreDelay(1000 / 9);
    }

    skilldexWindowFree();

    return rc;
}

// 0x4AC054
int skilldexWindowInit()
{
    gSkilldexWindowOldFont = fontGetCurrent();
    gSkilldexWindowIsoWasEnabled = false;

    gameMouseObjectsHide();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    if (!messageListInit(&gSkilldexMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s%s", asc_5186C8, "skilldex.msg");

    if (!messageListLoad(&gSkilldexMessageList, path)) {
        return -1;
    }

    int frmIndex;
    for (frmIndex = 0; frmIndex < SKILLDEX_FRM_COUNT; frmIndex++) {
        int fid = buildFid(6, gSkilldexFrmIds[frmIndex], 0, 0, 0);
        gSkilldexFrmData[frmIndex] = artLockFrameDataReturningSize(fid, &(gSkilldexFrmHandles[frmIndex]), &(gSkilldexFrmSizes[frmIndex].width), &(gSkilldexFrmSizes[frmIndex].height));
        if (gSkilldexFrmData[frmIndex] == NULL) {
            break;
        }
    }

    if (frmIndex < SKILLDEX_FRM_COUNT) {
        while (--frmIndex >= 0) {
            artUnlock(gSkilldexFrmHandles[frmIndex]);
        }

        messageListFree(&gSkilldexMessageList);

        return -1;
    }

    bool cycle = false;
    int buttonDataIndex;
    for (buttonDataIndex = 0; buttonDataIndex < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT; buttonDataIndex++) {
        gSkilldexButtonsData[buttonDataIndex] = (unsigned char*)internal_malloc(gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_ON].height * gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_ON].width + 512);
        if (gSkilldexButtonsData[buttonDataIndex] == NULL) {
            break;
        }

        // NOTE: Original code uses bitwise XOR.
        cycle = !cycle;

        unsigned char* data;
        int size;
        if (cycle) {
            size = gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_OFF].width * gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_OFF].height;
            data = gSkilldexFrmData[SKILLDEX_FRM_BUTTON_OFF];
        } else {
            size = gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_ON].width * gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_ON].height;
            data = gSkilldexFrmData[SKILLDEX_FRM_BUTTON_ON];
        }

        memcpy(gSkilldexButtonsData[buttonDataIndex], data, size);
    }

    if (buttonDataIndex < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT) {
        while (--buttonDataIndex >= 0) {
            internal_free(gSkilldexButtonsData[buttonDataIndex]);
        }

        for (int index = 0; index < SKILLDEX_FRM_COUNT; index++) {
            artUnlock(gSkilldexFrmHandles[index]);
        }

        messageListFree(&gSkilldexMessageList);

        return -1;
    }

    // Maintain original position relative to centered interface bar.
    int skilldexWindowX = (screenGetWidth() - 640) / 2 + 640 - gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width - SKILLDEX_WINDOW_RIGHT_MARGIN;
    int skilldexWindowY = screenGetHeight() - INTERFACE_BAR_HEIGHT - 1 - gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].height - SKILLDEX_WINDOW_BOTTOM_MARGIN;
    gSkilldexWindow = windowCreate(skilldexWindowX,
        skilldexWindowY,
        gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width,
        gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].height,
        256,
        WINDOW_FLAG_0x10 | WINDOW_FLAG_0x02);
    if (gSkilldexWindow == -1) {
        for (int index = 0; index < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT; index++) {
            internal_free(gSkilldexButtonsData[index]);
        }

        for (int index = 0; index < SKILLDEX_FRM_COUNT; index++) {
            artUnlock(gSkilldexFrmHandles[index]);
        }

        messageListFree(&gSkilldexMessageList);

        return -1;
    }

    gSkilldexWindowIsoWasEnabled = isoDisable();

    colorCycleDisable();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    gSkilldexWindowBuffer = windowGetBuffer(gSkilldexWindow);
    memcpy(gSkilldexWindowBuffer,
        gSkilldexFrmData[SKILLDEX_FRM_BACKGROUND],
        gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width * gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].height);

    fontSetCurrent(103);

    // Render "SKILLDEX" title.
    char* title = getmsg(&gSkilldexMessageList, &gSkilldexMessageListItem, 100);
    fontDrawText(gSkilldexWindowBuffer + 14 * gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width + 55,
        title,
        gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width,
        gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width,
        _colorTable[18979]);

    // Render skill values.
    int valueY = 48;
    for (int index = 0; index < SKILLDEX_SKILL_COUNT; index++) {
        int value = skillGetValue(gDude, gSkilldexSkills[index]);
        if (value == -1) {
            value = 0;
        }

        int hundreds = value / 100;
        blitBufferToBuffer(gSkilldexFrmData[SKILLDEX_FRM_BIG_NUMBERS] + 14 * hundreds,
            14,
            24,
            336,
            gSkilldexWindowBuffer + gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width * valueY + 110,
            gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width);

        int tens = (value % 100) / 10;
        blitBufferToBuffer(gSkilldexFrmData[SKILLDEX_FRM_BIG_NUMBERS] + 14 * tens,
            14,
            24,
            336,
            gSkilldexWindowBuffer + gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width * valueY + 124,
            gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width);

        int ones = (value % 100) % 10;
        blitBufferToBuffer(gSkilldexFrmData[SKILLDEX_FRM_BIG_NUMBERS] + 14 * ones,
            14,
            24,
            336,
            gSkilldexWindowBuffer + gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width * valueY + 138,
            gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width);

        valueY += 36;
    }

    // Render skill buttons.
    int lineHeight = fontGetLineHeight();

    int buttonY = 45;
    int nameY = ((gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_OFF].height - lineHeight) / 2) + 1;
    for (int index = 0; index < SKILLDEX_SKILL_COUNT; index++) {
        char name[MESSAGE_LIST_ITEM_FIELD_MAX_SIZE];
        strcpy(name, getmsg(&gSkilldexMessageList, &gSkilldexMessageListItem, 102 + index));

        int nameX = ((gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_OFF].width - fontGetStringWidth(name)) / 2) + 1;
        if (nameX < 0) {
            nameX = 0;
        }

        fontDrawText(gSkilldexButtonsData[index * 2] + gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_ON].width * nameY + nameX,
            name,
            gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_ON].width,
            gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_ON].width,
            _colorTable[18979]);

        fontDrawText(gSkilldexButtonsData[index * 2 + 1] + gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_OFF].width * nameY + nameX,
            name,
            gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_OFF].width,
            gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_OFF].width,
            _colorTable[14723]);

        int btn = buttonCreate(gSkilldexWindow,
            15,
            buttonY,
            gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_OFF].width,
            gSkilldexFrmSizes[SKILLDEX_FRM_BUTTON_OFF].height,
            -1,
            -1,
            -1,
            501 + index,
            gSkilldexButtonsData[index * 2],
            gSkilldexButtonsData[index * 2 + 1],
            NULL,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_lrg_butt_press, _gsound_lrg_butt_release);
        }

        buttonY += 36;
    }

    // Render "CANCEL" button.
    char* cancel = getmsg(&gSkilldexMessageList, &gSkilldexMessageListItem, 101);
    fontDrawText(gSkilldexWindowBuffer + gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width * 337 + 72,
        cancel,
        gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width,
        gSkilldexFrmSizes[SKILLDEX_FRM_BACKGROUND].width,
        _colorTable[18979]);

    int cancelBtn = buttonCreate(gSkilldexWindow,
        48,
        338,
        gSkilldexFrmSizes[SKILLDEX_FRM_LITTLE_RED_BUTTON_UP].width,
        gSkilldexFrmSizes[SKILLDEX_FRM_LITTLE_RED_BUTTON_UP].height,
        -1,
        -1,
        -1,
        500,
        gSkilldexFrmData[SKILLDEX_FRM_LITTLE_RED_BUTTON_UP],
        gSkilldexFrmData[SKILLDEX_FRM_LITTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(gSkilldexWindow);

    return 0;
}

// 0x4AC67C
void skilldexWindowFree()
{
    windowDestroy(gSkilldexWindow);

    for (int index = 0; index < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT; index++) {
        internal_free(gSkilldexButtonsData[index]);
    }

    for (int index = 0; index < SKILLDEX_FRM_COUNT; index++) {
        artUnlock(gSkilldexFrmHandles[index]);
    }

    messageListFree(&gSkilldexMessageList);

    fontSetCurrent(gSkilldexWindowOldFont);

    if (gSkilldexWindowIsoWasEnabled) {
        isoEnable();
    }

    colorCycleEnable();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
}
