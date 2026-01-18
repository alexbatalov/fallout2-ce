#include "skilldex.h"

#include <stdio.h>
#include <string.h>

#include "art.h"
#include "color.h"
#include "cycle.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "geometry.h"
#include "input.h"
#include "interface.h"
#include "kb.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "platform_compat.h"
#include "skill.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"

namespace fallout {

#define SKILLDEX_WINDOW_RIGHT_MARGIN 4
#define SKILLDEX_WINDOW_BOTTOM_MARGIN 6

#define SKILLDEX_SKILL_BUTTON_BUFFER_COUNT (SKILLDEX_SKILL_COUNT * 2)

typedef enum SkilldexFrm {
    SKILLDEX_FRM_BACKGROUND,
    SKILLDEX_FRM_BUTTON_ON,
    SKILLDEX_FRM_BUTTON_OFF,
    SKILLDEX_FRM_LITTLE_RED_BUTTON_UP,
    SKILLDEX_FRM_LITTLE_RED_BUTTON_DOWN,
    SKILLDEX_FRM_BIG_NUMBERS,
    SKILLDEX_FRM_COUNT,
} SkilldexFrm;

typedef enum SkilldexSkill {
    SKILLDEX_SKILL_SNEAK,
    SKILLDEX_SKILL_LOCKPICK,
    SKILLDEX_SKILL_STEAL,
    SKILLDEX_SKILL_TRAPS,
    SKILLDEX_SKILL_FIRST_AID,
    SKILLDEX_SKILL_DOCTOR,
    SKILLDEX_SKILL_SCIENCE,
    SKILLDEX_SKILL_REPAIR,
    SKILLDEX_SKILL_COUNT,
} SkilldexSkill;

static int skilldexWindowInit();
static void skilldexWindowFree();

// 0x51D43C
static bool gSkilldexWindowIsoWasEnabled = false;

// 0x51D440
static const int gSkilldexFrmIds[SKILLDEX_FRM_COUNT] = {
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
static const int gSkilldexSkills[SKILLDEX_SKILL_COUNT] = {
    SKILL_SNEAK,
    SKILL_LOCKPICK,
    SKILL_STEAL,
    SKILL_TRAPS,
    SKILL_FIRST_AID,
    SKILL_DOCTOR,
    SKILL_SCIENCE,
    SKILL_REPAIR,
};

// 0x6680B8
static unsigned char* gSkilldexButtonsData[SKILLDEX_SKILL_BUTTON_BUFFER_COUNT];

// skilldex.msg
// 0x6680F8
static MessageList gSkilldexMessageList;

// 0x668100
static MessageListItem gSkilldexMessageListItem;

// 0x668140
static int gSkilldexWindow;

// 0x668144
static unsigned char* gSkilldexWindowBuffer;

// 0x668148
static int gSkilldexWindowOldFont;

static FrmImage _skilldexFrmImages[SKILLDEX_FRM_COUNT];

// skilldex_select
// 0x4ABFD0
int skilldexOpen()
{
    ScopedGameMode gm(GameMode::kSkilldex);

    if (skilldexWindowInit() == -1) {
        debugPrint("\n ** Error loading skilldex dialog data! **\n");
        return -1;
    }

    touch_set_touchscreen_mode(true);

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        // SFALL: Close with 'S'.
        if (keyCode == KEY_ESCAPE || keyCode == KEY_UPPERCASE_S || keyCode == KEY_LOWERCASE_S || keyCode == 500 || _game_user_wants_to_quit != 0) {
            rc = 0;
        } else if (keyCode == KEY_RETURN) {
            soundPlayFile("ib1p1xx1");
            rc = 0;
        } else if (keyCode >= 501 && keyCode <= 509) {
            rc = keyCode - 500;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (rc != 0) {
        inputBlockForTocks(1000 / 9);
    }

    skilldexWindowFree();

    return rc;
}

// 0x4AC054
static int skilldexWindowInit()
{
    gSkilldexWindowOldFont = fontGetCurrent();
    gSkilldexWindowIsoWasEnabled = false;

    gameMouseObjectsHide();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    if (!messageListInit(&gSkilldexMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "skilldex.msg");

    if (!messageListLoad(&gSkilldexMessageList, path)) {
        return -1;
    }

    int frmIndex;
    for (frmIndex = 0; frmIndex < SKILLDEX_FRM_COUNT; frmIndex++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gSkilldexFrmIds[frmIndex], 0, 0, 0);
        if (!_skilldexFrmImages[frmIndex].lock(fid)) {
            break;
        }
    }

    if (frmIndex < SKILLDEX_FRM_COUNT) {
        while (--frmIndex >= 0) {
            _skilldexFrmImages[frmIndex].unlock();
        }

        messageListFree(&gSkilldexMessageList);

        return -1;
    }

    bool cycle = false;
    int buttonDataIndex;
    for (buttonDataIndex = 0; buttonDataIndex < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT; buttonDataIndex++) {
        gSkilldexButtonsData[buttonDataIndex] = (unsigned char*)internal_malloc(_skilldexFrmImages[SKILLDEX_FRM_BUTTON_ON].getHeight() * _skilldexFrmImages[SKILLDEX_FRM_BUTTON_ON].getWidth() + 512);
        if (gSkilldexButtonsData[buttonDataIndex] == nullptr) {
            break;
        }

        // NOTE: Original code uses bitwise XOR.
        cycle = !cycle;

        unsigned char* data;
        int size;
        if (cycle) {
            size = _skilldexFrmImages[SKILLDEX_FRM_BUTTON_OFF].getWidth() * _skilldexFrmImages[SKILLDEX_FRM_BUTTON_OFF].getHeight();
            data = _skilldexFrmImages[SKILLDEX_FRM_BUTTON_OFF].getData();
        } else {
            size = _skilldexFrmImages[SKILLDEX_FRM_BUTTON_ON].getWidth() * _skilldexFrmImages[SKILLDEX_FRM_BUTTON_ON].getHeight();
            data = _skilldexFrmImages[SKILLDEX_FRM_BUTTON_ON].getData();
        }

        memcpy(gSkilldexButtonsData[buttonDataIndex], data, size);
    }

    if (buttonDataIndex < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT) {
        while (--buttonDataIndex >= 0) {
            internal_free(gSkilldexButtonsData[buttonDataIndex]);
        }

        for (int index = 0; index < SKILLDEX_FRM_COUNT; index++) {
            _skilldexFrmImages[index].unlock();
        }

        messageListFree(&gSkilldexMessageList);

        return -1;
    }

    // Maintain original position relative to centered interface bar.
    int skilldexWindowX = (screenGetWidth() - gInterfaceBarWidth) / 2 + gInterfaceBarWidth - _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth() - SKILLDEX_WINDOW_RIGHT_MARGIN;
    int skilldexWindowY = screenGetHeight() - INTERFACE_BAR_HEIGHT - 1 - _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getHeight() - SKILLDEX_WINDOW_BOTTOM_MARGIN;
    gSkilldexWindow = windowCreate(skilldexWindowX,
        skilldexWindowY,
        _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth(),
        _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getHeight(),
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (gSkilldexWindow == -1) {
        for (int index = 0; index < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT; index++) {
            internal_free(gSkilldexButtonsData[index]);
        }

        for (int index = 0; index < SKILLDEX_FRM_COUNT; index++) {
            _skilldexFrmImages[index].unlock();
        }

        messageListFree(&gSkilldexMessageList);

        return -1;
    }

    gSkilldexWindowIsoWasEnabled = isoDisable();

    colorCycleDisable();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    gSkilldexWindowBuffer = windowGetBuffer(gSkilldexWindow);
    memcpy(gSkilldexWindowBuffer,
        _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getData(),
        _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth() * _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getHeight());

    fontSetCurrent(103);

    // Render "SKILLDEX" title.
    char* title = getmsg(&gSkilldexMessageList, &gSkilldexMessageListItem, 100);
    fontDrawText(gSkilldexWindowBuffer + 14 * _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth() + 55,
        title,
        _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth(),
        _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth(),
        _colorTable[18979]);

    // Render skill values.
    int valueY = 48;
    for (int index = 0; index < SKILLDEX_SKILL_COUNT; index++) {
        int value = skillGetValue(gDude, gSkilldexSkills[index]);

        // SFALL: Fix for negative values.
        //
        // NOTE: Sfall's fix is different. It simply renders 0 even if
        // calculated skill level is negative (which can be the case when
        // playing on Hard difficulty + low Agility). For unknown reason -5 is
        // the error code from `skillGetValue`, which is not handled here
        // because -5 is also a legitimate skill value.
        //
        // TODO: Provide other error code in `skillGetValue`.
        unsigned char* numbersFrmData = _skilldexFrmImages[SKILLDEX_FRM_BIG_NUMBERS].getData();
        if (value < 0) {
            // First half of the bignum.frm is white, second half is red.
            numbersFrmData += _skilldexFrmImages[SKILLDEX_FRM_BIG_NUMBERS].getWidth() / 2;
            value = -value;
        }

        int hundreds = value / 100;
        blitBufferToBuffer(numbersFrmData + 14 * hundreds,
            14,
            24,
            336,
            gSkilldexWindowBuffer + _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth() * valueY + 110,
            _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth());

        int tens = (value % 100) / 10;
        blitBufferToBuffer(numbersFrmData + 14 * tens,
            14,
            24,
            336,
            gSkilldexWindowBuffer + _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth() * valueY + 124,
            _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth());

        int ones = (value % 100) % 10;
        blitBufferToBuffer(numbersFrmData + 14 * ones,
            14,
            24,
            336,
            gSkilldexWindowBuffer + _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth() * valueY + 138,
            _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth());

        valueY += 36;
    }

    // Render skill buttons.
    int lineHeight = fontGetLineHeight();

    int buttonY = 45;
    int nameY = ((_skilldexFrmImages[SKILLDEX_FRM_BUTTON_OFF].getHeight() - lineHeight) / 2) + 1;
    for (int index = 0; index < SKILLDEX_SKILL_COUNT; index++) {
        char name[MESSAGE_LIST_ITEM_FIELD_MAX_SIZE];
        strcpy(name, getmsg(&gSkilldexMessageList, &gSkilldexMessageListItem, 102 + index));

        int nameX = ((_skilldexFrmImages[SKILLDEX_FRM_BUTTON_OFF].getWidth() - fontGetStringWidth(name)) / 2) + 1;
        if (nameX < 0) {
            nameX = 0;
        }

        fontDrawText(gSkilldexButtonsData[index * 2] + _skilldexFrmImages[SKILLDEX_FRM_BUTTON_ON].getWidth() * nameY + nameX,
            name,
            _skilldexFrmImages[SKILLDEX_FRM_BUTTON_ON].getWidth(),
            _skilldexFrmImages[SKILLDEX_FRM_BUTTON_ON].getWidth(),
            _colorTable[18979]);

        fontDrawText(gSkilldexButtonsData[index * 2 + 1] + _skilldexFrmImages[SKILLDEX_FRM_BUTTON_OFF].getWidth() * nameY + nameX,
            name,
            _skilldexFrmImages[SKILLDEX_FRM_BUTTON_OFF].getWidth(),
            _skilldexFrmImages[SKILLDEX_FRM_BUTTON_OFF].getWidth(),
            _colorTable[14723]);

        int btn = buttonCreate(gSkilldexWindow,
            15,
            buttonY,
            _skilldexFrmImages[SKILLDEX_FRM_BUTTON_OFF].getWidth(),
            _skilldexFrmImages[SKILLDEX_FRM_BUTTON_OFF].getHeight(),
            -1,
            -1,
            -1,
            501 + index,
            gSkilldexButtonsData[index * 2],
            gSkilldexButtonsData[index * 2 + 1],
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_lrg_butt_press, _gsound_lrg_butt_release);
        }

        buttonY += 36;
    }

    // Render "CANCEL" button.
    char* cancel = getmsg(&gSkilldexMessageList, &gSkilldexMessageListItem, 101);
    fontDrawText(gSkilldexWindowBuffer + _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth() * 337 + 72,
        cancel,
        _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth(),
        _skilldexFrmImages[SKILLDEX_FRM_BACKGROUND].getWidth(),
        _colorTable[18979]);

    int cancelBtn = buttonCreate(gSkilldexWindow,
        48,
        338,
        _skilldexFrmImages[SKILLDEX_FRM_LITTLE_RED_BUTTON_UP].getWidth(),
        _skilldexFrmImages[SKILLDEX_FRM_LITTLE_RED_BUTTON_UP].getHeight(),
        -1,
        -1,
        -1,
        500,
        _skilldexFrmImages[SKILLDEX_FRM_LITTLE_RED_BUTTON_UP].getData(),
        _skilldexFrmImages[SKILLDEX_FRM_LITTLE_RED_BUTTON_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(gSkilldexWindow);

    return 0;
}

// 0x4AC67C
static void skilldexWindowFree()
{
    windowDestroy(gSkilldexWindow);

    for (int index = 0; index < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT; index++) {
        internal_free(gSkilldexButtonsData[index]);
    }

    for (int index = 0; index < SKILLDEX_FRM_COUNT; index++) {
        _skilldexFrmImages[index].unlock();
    }

    messageListFree(&gSkilldexMessageList);

    fontSetCurrent(gSkilldexWindowOldFont);

    if (gSkilldexWindowIsoWasEnabled) {
        isoEnable();
    }

    colorCycleEnable();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    touch_set_touchscreen_mode(false);
}

} // namespace fallout
