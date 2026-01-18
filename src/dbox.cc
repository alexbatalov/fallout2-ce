#include "dbox.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "art.h"
#include "character_editor.h"
#include "color.h"
#include "debug.h"
#include "delay.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "message.h"
#include "mouse.h"
#include "platform_compat.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"
#include "word_wrap.h"

namespace fallout {

#define FILE_DIALOG_LINE_COUNT 12

#define FILE_DIALOG_DOUBLE_CLICK_DELAY 32

#define LOAD_FILE_DIALOG_DONE_BUTTON_X 58
#define LOAD_FILE_DIALOG_DONE_BUTTON_Y 187

#define LOAD_FILE_DIALOG_DONE_LABEL_X 79
#define LOAD_FILE_DIALOG_DONE_LABEL_Y 187

#define LOAD_FILE_DIALOG_CANCEL_BUTTON_X 163
#define LOAD_FILE_DIALOG_CANCEL_BUTTON_Y 187

#define LOAD_FILE_DIALOG_CANCEL_LABEL_X 182
#define LOAD_FILE_DIALOG_CANCEL_LABEL_Y 187

#define SAVE_FILE_DIALOG_DONE_BUTTON_X 58
#define SAVE_FILE_DIALOG_DONE_BUTTON_Y 214

#define SAVE_FILE_DIALOG_DONE_LABEL_X 79
#define SAVE_FILE_DIALOG_DONE_LABEL_Y 213

#define SAVE_FILE_DIALOG_CANCEL_BUTTON_X 163
#define SAVE_FILE_DIALOG_CANCEL_BUTTON_Y 214

#define SAVE_FILE_DIALOG_CANCEL_LABEL_X 182
#define SAVE_FILE_DIALOG_CANCEL_LABEL_Y 213

#define FILE_DIALOG_TITLE_X 49
#define FILE_DIALOG_TITLE_Y 16

#define FILE_DIALOG_SCROLL_BUTTON_X 36
#define FILE_DIALOG_SCROLL_BUTTON_Y 44

#define FILE_DIALOG_FILE_LIST_X 55
#define FILE_DIALOG_FILE_LIST_Y 49
#define FILE_DIALOG_FILE_LIST_WIDTH 190
#define FILE_DIALOG_FILE_LIST_HEIGHT 124

typedef enum DialogType {
    DIALOG_TYPE_MEDIUM,
    DIALOG_TYPE_LARGE,
    DIALOG_TYPE_COUNT,
} DialogType;

typedef enum FileDialogFrm {
    FILE_DIALOG_FRM_BACKGROUND,
    FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL,
    FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED,
    FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_NORMAL,
    FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED,
    FILE_DIALOG_FRM_SCROLL_UP_ARROW_NORMAL,
    FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED,
    FILE_DIALOG_FRM_COUNT,
} FileDialogFrm;

typedef enum FileDialogScrollDirection {
    FILE_DIALOG_SCROLL_DIRECTION_NONE,
    FILE_DIALOG_SCROLL_DIRECTION_UP,
    FILE_DIALOG_SCROLL_DIRECTION_DOWN,
} FileDialogScrollDirection;

static void fileDialogRenderFileList(unsigned char* buffer, char** fileList, int pageOffset, int fileListLength, int selectedIndex, int pitch);

// 0x5108C8
static const int gDialogBoxBackgroundFrmIds[DIALOG_TYPE_COUNT] = {
    218, // MEDIALOG.FRM - Medium generic dialog box
    217, // LGDIALOG.FRM - Large generic dialog box
};

// 0x5108D0
static const int _ytable[DIALOG_TYPE_COUNT] = {
    23,
    27,
};

// 0x5108D8
static const int _xtable[DIALOG_TYPE_COUNT] = {
    29,
    29,
};

// 0x5108E0
static const int _doneY[DIALOG_TYPE_COUNT] = {
    81,
    98,
};

// 0x5108E8
static const int _doneX[DIALOG_TYPE_COUNT] = {
    51,
    37,
};

// 0x5108F0
static const int _dblines[DIALOG_TYPE_COUNT] = {
    5,
    6,
};

// 0x510900
static int gLoadFileDialogFrmIds[FILE_DIALOG_FRM_COUNT] = {
    224, // loadbox.frm - character editor
    8, // lilredup.frm - little red button up
    9, // lilreddn.frm - little red button down
    181, // dnarwoff.frm - character editor
    182, // dnarwon.frm - character editor
    199, // uparwoff.frm - character editor
    200, // uparwon.frm - character editor
};

// 0x51091C
static int gSaveFileDialogFrmIds[FILE_DIALOG_FRM_COUNT] = {
    225, // savebox.frm - character editor
    8, // lilredup.frm - little red button up
    9, // lilreddn.frm - little red button down
    181, // dnarwoff.frm - character editor
    182, // dnarwon.frm - character editor
    199, // uparwoff.frm - character editor
    200, // uparwon.frm - character editor
};

// 0x41CF20
int showDialogBox(const char* title, const char** body, int bodyLength, int x, int y, int titleColor, const char* a8, int bodyColor, int flags)
{
    MessageList messageList;
    MessageListItem messageListItem;
    int savedFont = fontGetCurrent();

    bool initializedButtons = false;

    bool hasTwoButtons = false;
    if (a8 != nullptr) {
        // TODO: a8 seems to be a way to customize "NO", but is unused.
        hasTwoButtons = true;
    }

    bool hasTitle = false;
    if (title != nullptr) {
        hasTitle = true;
    }

    if ((flags & DIALOG_BOX_YES_NO) != 0) {
        hasTwoButtons = true;
        flags |= DIALOG_BOX_LARGE;
        flags &= ~DIALOG_BOX_NO_BUTTONS;
    }

    int maximumLineWidth = 0;
    if (hasTitle) {
        maximumLineWidth = fontGetStringWidth(title);
    }

    int linesCount = 0;
    for (int index = 0; index < bodyLength; index++) {
        // NOTE: Originally there is no `max` macro.
        maximumLineWidth = std::max(fontGetStringWidth(body[index]), maximumLineWidth);
        linesCount++;
    }

    int dialogType;
    if ((flags & DIALOG_BOX_LARGE) != 0 || hasTwoButtons) {
        dialogType = DIALOG_TYPE_LARGE;
    } else if ((flags & DIALOG_BOX_MEDIUM) != 0) {
        dialogType = DIALOG_TYPE_MEDIUM;
    } else {
        if (hasTitle) {
            linesCount++;
        }

        dialogType = maximumLineWidth > 168 || linesCount > 5
            ? DIALOG_TYPE_LARGE
            : DIALOG_TYPE_MEDIUM;
    }

    FrmImage backgroundFrmImage;
    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, gDialogBoxBackgroundFrmIds[dialogType], 0, 0, 0);
    if (!backgroundFrmImage.lock(backgroundFid)) {
        fontSetCurrent(savedFont);
        return -1;
    }

    // Maintain original position in original resolution, otherwise center it.
    x += (screenGetWidth() - 640) / 2;
    y += (screenGetHeight() - 480) / 2;
    int win = windowCreate(x,
        y,
        backgroundFrmImage.getWidth(),
        backgroundFrmImage.getHeight(),
        256,
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        fontSetCurrent(savedFont);
        return -1;
    }

    unsigned char* windowBuf = windowGetBuffer(win);
    memcpy(windowBuf, backgroundFrmImage.getData(), backgroundFrmImage.getWidth() * backgroundFrmImage.getHeight());

    FrmImage doneBoxFrmImage;
    FrmImage buttonNormalFrmImage;
    FrmImage buttonPressedFrmImage;

    if ((flags & DIALOG_BOX_NO_BUTTONS) == 0) {
        int doneBoxFid = buildFid(OBJ_TYPE_INTERFACE, 209, 0, 0, 0);
        if (!doneBoxFrmImage.lock(doneBoxFid)) {
            fontSetCurrent(savedFont);
            windowDestroy(win);
            return -1;
        }

        int pressedFid = buildFid(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
        if (!buttonPressedFrmImage.lock(pressedFid)) {
            fontSetCurrent(savedFont);
            windowDestroy(win);
            return -1;
        }

        int normalFid = buildFid(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
        if (!buttonNormalFrmImage.lock(normalFid)) {
            fontSetCurrent(savedFont);
            windowDestroy(win);
            return -1;
        }

        int v27 = hasTwoButtons
            ? _doneX[dialogType]
            : (backgroundFrmImage.getWidth() - doneBoxFrmImage.getWidth()) / 2;
        blitBufferToBuffer(doneBoxFrmImage.getData(),
            doneBoxFrmImage.getWidth(),
            doneBoxFrmImage.getHeight(),
            doneBoxFrmImage.getWidth(),
            windowBuf + backgroundFrmImage.getWidth() * _doneY[dialogType] + v27,
            backgroundFrmImage.getWidth());

        if (!messageListInit(&messageList)) {
            fontSetCurrent(savedFont);
            windowDestroy(win);
            return -1;
        }

        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s%s", asc_5186C8, "DBOX.MSG");

        if (!messageListLoad(&messageList, path)) {
            fontSetCurrent(savedFont);
            // FIXME: Window is not removed.
            return -1;
        }

        fontSetCurrent(103);

        // 100 - DONE
        // 101 - YES
        messageListItem.num = (flags & DIALOG_BOX_YES_NO) == 0 ? 100 : 101;
        if (messageListGetItem(&messageList, &messageListItem)) {
            fontDrawText(windowBuf + backgroundFrmImage.getWidth() * (_doneY[dialogType] + 3) + v27 + 35,
                messageListItem.text,
                backgroundFrmImage.getWidth(),
                backgroundFrmImage.getWidth(),
                _colorTable[18979]);
        }

        int btn = buttonCreate(win,
            v27 + 13,
            _doneY[dialogType] + 4,
            buttonPressedFrmImage.getWidth(),
            buttonPressedFrmImage.getHeight(),
            -1,
            -1,
            -1,
            500,
            buttonNormalFrmImage.getData(),
            buttonPressedFrmImage.getData(),
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
        }

        initializedButtons = true;
    }

    if (hasTwoButtons && dialogType == DIALOG_TYPE_LARGE) {
        if (initializedButtons) {
            if ((flags & DIALOG_BOX_YES_NO) != 0) {
                a8 = getmsg(&messageList, &messageListItem, 102); // 102 - NO
            }

            fontSetCurrent(103);

            blitBufferToBufferTrans(doneBoxFrmImage.getData(),
                doneBoxFrmImage.getWidth(),
                doneBoxFrmImage.getHeight(),
                doneBoxFrmImage.getWidth(),
                windowBuf + backgroundFrmImage.getWidth() * _doneY[dialogType] + _doneX[dialogType] + doneBoxFrmImage.getWidth() + 24,
                backgroundFrmImage.getWidth());

            fontDrawText(windowBuf + backgroundFrmImage.getWidth() * (_doneY[dialogType] + 3) + _doneX[dialogType] + doneBoxFrmImage.getWidth() + 59,
                a8,
                backgroundFrmImage.getWidth(),
                backgroundFrmImage.getWidth(),
                _colorTable[18979]);

            int btn = buttonCreate(win,
                doneBoxFrmImage.getWidth() + _doneX[dialogType] + 37,
                _doneY[dialogType] + 4,
                buttonPressedFrmImage.getWidth(),
                buttonPressedFrmImage.getHeight(),
                -1,
                -1,
                -1,
                501,
                buttonNormalFrmImage.getData(),
                buttonPressedFrmImage.getData(),
                nullptr,
                BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }
        } else {
            int doneBoxFid = buildFid(OBJ_TYPE_INTERFACE, 209, 0, 0, 0);
            if (!doneBoxFrmImage.lock(doneBoxFid)) {
                fontSetCurrent(savedFont);
                windowDestroy(win);
                return -1;
            }

            int pressedFid = buildFid(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
            if (!buttonPressedFrmImage.lock(pressedFid)) {
                fontSetCurrent(savedFont);
                windowDestroy(win);
                return -1;
            }

            int normalFid = buildFid(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
            if (!buttonNormalFrmImage.lock(normalFid)) {
                fontSetCurrent(savedFont);
                windowDestroy(win);
                return -1;
            }

            if (!messageListInit(&messageList)) {
                fontSetCurrent(savedFont);
                windowDestroy(win);
                return -1;
            }

            char path[COMPAT_MAX_PATH];
            snprintf(path, sizeof(path), "%s%s", asc_5186C8, "DBOX.MSG");

            if (!messageListLoad(&messageList, path)) {
                fontSetCurrent(savedFont);
                windowDestroy(win);
                return -1;
            }

            blitBufferToBufferTrans(doneBoxFrmImage.getData(),
                doneBoxFrmImage.getWidth(),
                doneBoxFrmImage.getHeight(),
                doneBoxFrmImage.getWidth(),
                windowBuf + backgroundFrmImage.getWidth() * _doneY[dialogType] + _doneX[dialogType],
                backgroundFrmImage.getWidth());

            fontSetCurrent(103);

            fontDrawText(windowBuf + backgroundFrmImage.getWidth() * (_doneY[dialogType] + 3) + _doneX[dialogType] + 35,
                a8, backgroundFrmImage.getWidth(), backgroundFrmImage.getWidth(), _colorTable[18979]);

            int btn = buttonCreate(win,
                _doneX[dialogType] + 13,
                _doneY[dialogType] + 4,
                buttonPressedFrmImage.getWidth(),
                buttonPressedFrmImage.getHeight(),
                -1,
                -1,
                -1,
                501,
                buttonNormalFrmImage.getData(),
                buttonPressedFrmImage.getData(),
                nullptr,
                BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }

            initializedButtons = true;
        }
    }

    fontSetCurrent(101);

    int nextY = _ytable[dialogType];
    int maxY = _ytable[dialogType] + _dblines[dialogType] * fontGetLineHeight();

    if ((flags & DIALOG_BOX_NO_VERTICAL_CENTERING) == 0) {
        int numberOfLines = 0;

        if (hasTitle) {
            numberOfLines++;
        }

        for (int index = 0; index < bodyLength; index++) {
            short beginnings[WORD_WRAP_MAX_COUNT];
            short subLineCount;
            int maxWidth = backgroundFrmImage.getWidth() - _xtable[dialogType] * 2;
            if (wordWrap(body[index], maxWidth, beginnings, &subLineCount) == 0) {
                numberOfLines += subLineCount - 1;
            }
        }

        if (numberOfLines > _dblines[dialogType]) {
            numberOfLines = _dblines[dialogType];
        }

        nextY += (_dblines[dialogType] - numberOfLines) * fontGetLineHeight() / 2;
    }

    if (hasTitle) {
        if ((flags & DIALOG_BOX_NO_HORIZONTAL_CENTERING) != 0) {
            fontDrawText(windowBuf + backgroundFrmImage.getWidth() * nextY + _xtable[dialogType],
                title,
                backgroundFrmImage.getWidth(),
                backgroundFrmImage.getWidth(),
                titleColor);
        } else {
            int length = fontGetStringWidth(title);
            fontDrawText(windowBuf + backgroundFrmImage.getWidth() * nextY + (backgroundFrmImage.getWidth() - length) / 2,
                title,
                backgroundFrmImage.getWidth(),
                backgroundFrmImage.getWidth(),
                titleColor);
        }
        nextY += fontGetLineHeight();
    }

    for (int index = 0; index < bodyLength && nextY < maxY; index++) {
        int width = fontGetStringWidth(body[index]);
        int maxWidth = backgroundFrmImage.getWidth() - _xtable[dialogType] * 2;
        if (width <= maxWidth) {
            if ((flags & DIALOG_BOX_NO_HORIZONTAL_CENTERING) != 0) {
                fontDrawText(windowBuf + backgroundFrmImage.getWidth() * nextY + _xtable[dialogType],
                    body[index],
                    backgroundFrmImage.getWidth(),
                    backgroundFrmImage.getWidth(),
                    bodyColor);
            } else {
                int length = fontGetStringWidth(body[index]);
                fontDrawText(windowBuf + backgroundFrmImage.getWidth() * nextY + (backgroundFrmImage.getWidth() - length) / 2,
                    body[index],
                    backgroundFrmImage.getWidth(),
                    backgroundFrmImage.getWidth(),
                    bodyColor);
            }
            nextY += fontGetLineHeight();
        } else {
            short beginnings[WORD_WRAP_MAX_COUNT];
            short count;
            if (wordWrap(body[index], maxWidth, beginnings, &count) != 0) {
                debugPrint("\nError: dialog_out");
            }

            for (int beginningIndex = 1; beginningIndex < count && nextY < maxY; beginningIndex++) {
                int subLineLength = beginnings[beginningIndex] - beginnings[beginningIndex - 1];
                if (subLineLength >= 260) {
                    subLineLength = 259;
                }

                char string[260];
                strncpy(string, body[index] + beginnings[beginningIndex - 1], subLineLength);
                string[subLineLength] = '\0';

                // Remove trailing space as it affects width calculation.
                if (subLineLength > 0 && string[subLineLength - 1] == ' ') {
                    string[subLineLength - 1] = '\0';
                    subLineLength -= 1;
                }

                if ((flags & DIALOG_BOX_NO_HORIZONTAL_CENTERING) != 0) {
                    fontDrawText(windowBuf + backgroundFrmImage.getWidth() * nextY + _xtable[dialogType],
                        string,
                        backgroundFrmImage.getWidth(),
                        backgroundFrmImage.getWidth(),
                        bodyColor);
                } else {
                    int length = fontGetStringWidth(string);
                    fontDrawText(windowBuf + backgroundFrmImage.getWidth() * nextY + (backgroundFrmImage.getWidth() - length) / 2,
                        string,
                        backgroundFrmImage.getWidth(),
                        backgroundFrmImage.getWidth(),
                        bodyColor);
                }
                nextY += fontGetLineHeight();
            }
        }
    }

    windowRefresh(win);

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        if (keyCode == 500) {
            rc = 1;
        } else if (keyCode == KEY_RETURN) {
            soundPlayFile("ib1p1xx1");
            rc = 1;
        } else if (keyCode == KEY_ESCAPE || keyCode == 501) {
            rc = 0;
        } else {
            if ((flags & DIALOG_BOX_YES_NO) != 0) {
                if (keyCode == KEY_UPPERCASE_Y || keyCode == KEY_LOWERCASE_Y) {
                    rc = 1;
                } else if (keyCode == KEY_UPPERCASE_N || keyCode == KEY_LOWERCASE_N) {
                    rc = 0;
                }
            }
        }

        if (_game_user_wants_to_quit != 0) {
            rc = 1;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);
    fontSetCurrent(savedFont);

    if (initializedButtons) {
        messageListFree(&messageList);
    }

    return rc;
}

// 0x41DE90
int showLoadFileDialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags)
{
    int oldFont = fontGetCurrent();

    bool isScrollable = false;
    if (fileListLength > FILE_DIALOG_LINE_COUNT) {
        isScrollable = true;
    }

    int selectedFileIndex = 0;
    int pageOffset = 0;
    int maxPageOffset = fileListLength - (FILE_DIALOG_LINE_COUNT + 1);
    if (maxPageOffset < 0) {
        maxPageOffset = fileListLength - 1;
        if (maxPageOffset < 0) {
            maxPageOffset = 0;
        }
    }

    FrmImage frmImages[FILE_DIALOG_FRM_COUNT];

    for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gLoadFileDialogFrmIds[index], 0, 0, 0);
        if (!frmImages[index].lock(fid)) {
            return -1;
        }
    }

    int backgroundWidth = frmImages[FILE_DIALOG_FRM_BACKGROUND].getWidth();
    int backgroundHeight = frmImages[FILE_DIALOG_FRM_BACKGROUND].getHeight();

    // Maintain original position in original resolution, otherwise center it.
    x += (screenGetWidth() - 640) / 2;
    y += (screenGetHeight() - 480) / 2;
    int win = windowCreate(x, y, backgroundWidth, backgroundHeight, 256, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(win);
    memcpy(windowBuffer, frmImages[FILE_DIALOG_FRM_BACKGROUND].getData(), backgroundWidth * backgroundHeight);

    MessageList messageList;
    MessageListItem messageListItem;

    if (!messageListInit(&messageList)) {
        windowDestroy(win);
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "DBOX.MSG");

    if (!messageListLoad(&messageList, path)) {
        windowDestroy(win);
        return -1;
    }

    fontSetCurrent(103);

    // DONE
    const char* done = getmsg(&messageList, &messageListItem, 100);
    fontDrawText(windowBuffer + LOAD_FILE_DIALOG_DONE_LABEL_Y * backgroundWidth + LOAD_FILE_DIALOG_DONE_LABEL_X, done, backgroundWidth, backgroundWidth, _colorTable[18979]);

    // CANCEL
    const char* cancel = getmsg(&messageList, &messageListItem, 103);
    fontDrawText(windowBuffer + LOAD_FILE_DIALOG_CANCEL_LABEL_Y * backgroundWidth + LOAD_FILE_DIALOG_CANCEL_LABEL_X, cancel, backgroundWidth, backgroundWidth, _colorTable[18979]);

    int doneBtn = buttonCreate(win,
        LOAD_FILE_DIALOG_DONE_BUTTON_X,
        LOAD_FILE_DIALOG_DONE_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        500,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int cancelBtn = buttonCreate(win,
        LOAD_FILE_DIALOG_CANCEL_BUTTON_X,
        LOAD_FILE_DIALOG_CANCEL_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        501,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int scrollUpBtn = buttonCreate(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getHeight(),
        -1,
        505,
        506,
        505,
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int scrollDownButton = buttonCreate(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y + frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getHeight(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getHeight(),
        -1,
        503,
        504,
        503,
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    buttonCreate(
        win,
        FILE_DIALOG_FILE_LIST_X,
        FILE_DIALOG_FILE_LIST_Y,
        FILE_DIALOG_FILE_LIST_WIDTH,
        FILE_DIALOG_FILE_LIST_HEIGHT,
        -1,
        -1,
        -1,
        502,
        nullptr,
        nullptr,
        nullptr,
        0);

    if (title != nullptr) {
        fontDrawText(windowBuffer + backgroundWidth * FILE_DIALOG_TITLE_Y + FILE_DIALOG_TITLE_X, title, backgroundWidth, backgroundWidth, _colorTable[18979]);
    }

    fontSetCurrent(101);

    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
    windowRefresh(win);

    int doubleClickSelectedFileIndex = -2;
    int doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int tick = getTicks();
        int keyCode = inputGetInput();
        int scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_NONE;
        int scrollCounter = 0;
        bool isScrolling = false;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == 500) {
            if (fileListLength != 0) {
                strncpy(dest, fileList[selectedFileIndex + pageOffset], 16);
                rc = 0;
            } else {
                rc = 1;
            }
        } else if (keyCode == 501 || keyCode == KEY_ESCAPE) {
            rc = 1;
        } else if (keyCode == 502 && fileListLength != 0) {
            int mouseX;
            int mouseY;
            mouseGetPosition(&mouseX, &mouseY);

            int selectedLine = (mouseY - y - FILE_DIALOG_FILE_LIST_Y) / fontGetLineHeight();
            if (selectedLine - 1 < 0) {
                selectedLine = 0;
            }

            if (isScrollable || selectedLine < fileListLength) {
                if (selectedLine >= FILE_DIALOG_LINE_COUNT) {
                    selectedLine = FILE_DIALOG_LINE_COUNT - 1;
                }
            } else {
                selectedLine = fileListLength - 1;
            }

            selectedFileIndex = selectedLine;
            if (selectedFileIndex == doubleClickSelectedFileIndex) {
                soundPlayFile("ib1p1xx1");
                strncpy(dest, fileList[selectedFileIndex + pageOffset], 16);
                rc = 0;
            }

            doubleClickSelectedFileIndex = selectedFileIndex;
            fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
        } else if (keyCode == 506) {
            scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_UP;
        } else if (keyCode == 504) {
            scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_DOWN;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                pageOffset--;
                if (pageOffset < 0) {
                    selectedFileIndex--;
                    if (selectedFileIndex < 0) {
                        selectedFileIndex = 0;
                    }
                    pageOffset = 0;
                }
                fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            case KEY_ARROW_DOWN:
                if (isScrollable) {
                    pageOffset++;
                    // FIXME: Should be >= maxPageOffset (as in save dialog).
                    // Otherwise out of bounds index is considered selected.
                    if (pageOffset > maxPageOffset) {
                        selectedFileIndex++;
                        // FIXME: Should be >= FILE_DIALOG_LINE_COUNT (as in
                        // save dialog). Otherwise out of bounds index is
                        // considered selected.
                        if (selectedFileIndex > FILE_DIALOG_LINE_COUNT) {
                            selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                        }
                        pageOffset = maxPageOffset;
                    }
                } else {
                    selectedFileIndex++;
                    if (selectedFileIndex > maxPageOffset) {
                        selectedFileIndex = maxPageOffset;
                    }
                }
                fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            case KEY_HOME:
                selectedFileIndex = 0;
                pageOffset = 0;
                fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            case KEY_END:
                if (isScrollable) {
                    selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                    pageOffset = maxPageOffset;
                } else {
                    selectedFileIndex = maxPageOffset;
                    pageOffset = 0;
                }
                fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            }
        }

        if (scrollDirection != FILE_DIALOG_SCROLL_DIRECTION_NONE) {
            unsigned int scrollDelay = 4;
            doubleClickSelectedFileIndex = -2;
            while (1) {
                unsigned int scrollTick = getTicks();
                scrollCounter += 1;
                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollDelay += 1;
                        if (scrollDelay > 24) {
                            scrollDelay = 24;
                        }
                    }

                    if (scrollDirection == FILE_DIALOG_SCROLL_DIRECTION_UP) {
                        pageOffset--;
                        if (pageOffset < 0) {
                            selectedFileIndex--;
                            if (selectedFileIndex < 0) {
                                selectedFileIndex = 0;
                            }
                            pageOffset = 0;
                        }
                    } else {
                        if (isScrollable) {
                            pageOffset++;
                            if (pageOffset > maxPageOffset) {
                                selectedFileIndex++;
                                if (selectedFileIndex >= FILE_DIALOG_LINE_COUNT) {
                                    selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                                }
                                pageOffset = maxPageOffset;
                            }
                        } else {
                            selectedFileIndex++;
                            if (selectedFileIndex > maxPageOffset) {
                                selectedFileIndex = maxPageOffset;
                            }
                        }
                    }

                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    windowRefresh(win);
                }

                unsigned int delay = (scrollCounter > 14.4) ? 1000 / scrollDelay : 1000 / 24;

                delay_ms(delay - (getTicks() - scrollTick));

                if (_game_user_wants_to_quit != 0) {
                    rc = 1;
                    break;
                }

                int keyCode = inputGetInput();
                if (keyCode == 505 || keyCode == 503) {
                    break;
                }

                renderPresent();
            }
        } else {
            windowRefresh(win);

            doubleClickTimer--;
            if (doubleClickTimer == 0) {
                doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;
                doubleClickSelectedFileIndex = -2;
            }

            delay_ms(1000 / 24 - (getTicks() - tick));
        }

        if (_game_user_wants_to_quit) {
            rc = 1;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);

    messageListFree(&messageList);
    fontSetCurrent(oldFont);

    return rc;
}

// 0x41EA78
int showSaveFileDialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags)
{
    int oldFont = fontGetCurrent();

    bool isScrollable = false;
    if (fileListLength > FILE_DIALOG_LINE_COUNT) {
        isScrollable = true;
    }

    int selectedFileIndex = 0;
    int pageOffset = 0;
    int maxPageOffset = fileListLength - (FILE_DIALOG_LINE_COUNT + 1);
    if (maxPageOffset < 0) {
        maxPageOffset = fileListLength - 1;
        if (maxPageOffset < 0) {
            maxPageOffset = 0;
        }
    }

    FrmImage frmImages[FILE_DIALOG_FRM_COUNT];

    for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gSaveFileDialogFrmIds[index], 0, 0, 0);
        if (!frmImages[index].lock(fid)) {
            return -1;
        }
    }

    int backgroundWidth = frmImages[FILE_DIALOG_FRM_BACKGROUND].getWidth();
    int backgroundHeight = frmImages[FILE_DIALOG_FRM_BACKGROUND].getHeight();

    // Maintain original position in original resolution, otherwise center it.
    x += (screenGetWidth() - 640) / 2;
    y += (screenGetHeight() - 480) / 2;
    int win = windowCreate(x, y, backgroundWidth, backgroundHeight, 256, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(win);
    memcpy(windowBuffer, frmImages[FILE_DIALOG_FRM_BACKGROUND].getData(), backgroundWidth * backgroundHeight);

    MessageList messageList;
    MessageListItem messageListItem;

    if (!messageListInit(&messageList)) {
        windowDestroy(win);
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "DBOX.MSG");

    if (!messageListLoad(&messageList, path)) {
        windowDestroy(win);
        return -1;
    }

    fontSetCurrent(103);

    // DONE
    const char* done = getmsg(&messageList, &messageListItem, 100);
    fontDrawText(windowBuffer + backgroundWidth * SAVE_FILE_DIALOG_DONE_LABEL_Y + SAVE_FILE_DIALOG_DONE_LABEL_X, done, backgroundWidth, backgroundWidth, _colorTable[18979]);

    // CANCEL
    const char* cancel = getmsg(&messageList, &messageListItem, 103);
    fontDrawText(windowBuffer + backgroundWidth * SAVE_FILE_DIALOG_CANCEL_LABEL_Y + SAVE_FILE_DIALOG_CANCEL_LABEL_X, cancel, backgroundWidth, backgroundWidth, _colorTable[18979]);

    int doneBtn = buttonCreate(win,
        SAVE_FILE_DIALOG_DONE_BUTTON_X,
        SAVE_FILE_DIALOG_DONE_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        500,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int cancelBtn = buttonCreate(win,
        SAVE_FILE_DIALOG_CANCEL_BUTTON_X,
        SAVE_FILE_DIALOG_CANCEL_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        501,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int scrollUpBtn = buttonCreate(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getHeight(),
        -1,
        505,
        506,
        505,
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int scrollDownButton = buttonCreate(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y + frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getHeight(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getHeight(),
        -1,
        503,
        504,
        503,
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    buttonCreate(
        win,
        FILE_DIALOG_FILE_LIST_X,
        FILE_DIALOG_FILE_LIST_Y,
        FILE_DIALOG_FILE_LIST_WIDTH,
        FILE_DIALOG_FILE_LIST_HEIGHT,
        -1,
        -1,
        -1,
        502,
        nullptr,
        nullptr,
        nullptr,
        0);

    if (title != nullptr) {
        fontDrawText(windowBuffer + backgroundWidth * FILE_DIALOG_TITLE_Y + FILE_DIALOG_TITLE_X, title, backgroundWidth, backgroundWidth, _colorTable[18979]);
    }

    fontSetCurrent(101);

    int cursorHeight = fontGetLineHeight();
    int cursorWidth = fontGetStringWidth("_") - 4;
    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);

    int fileNameLength = 0;
    char* pch = dest;
    while (*pch != '\0' && *pch != '.') {
        fileNameLength++;
        if (fileNameLength >= 12) {
            break;
        }
    }
    dest[fileNameLength] = '\0';

    char fileNameCopy[32];
    strncpy(fileNameCopy, dest, 32);

    size_t fileNameCopyLength = strlen(fileNameCopy);
    fileNameCopy[fileNameCopyLength + 1] = '\0';
    fileNameCopy[fileNameCopyLength] = ' ';

    unsigned char* fileNameBufferPtr = windowBuffer + backgroundWidth * 190 + 57;

    bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, backgroundWidth, 100);
    fontDrawText(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, _colorTable[992]);

    windowRefresh(win);

    beginTextInput();

    int blinkingCounter = 3;
    bool blink = false;

    int doubleClickSelectedFileIndex = -2;
    int doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int tick = getTicks();
        int keyCode = inputGetInput();
        int scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_NONE;
        int scrollCounter = 0;
        bool isScrolling = false;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == 500) {
            rc = 0;
        } else if (keyCode == KEY_RETURN) {
            soundPlayFile("ib1p1xx1");
            rc = 0;
        } else if (keyCode == 501 || keyCode == KEY_ESCAPE) {
            rc = 1;
        } else if ((keyCode == KEY_DELETE || keyCode == KEY_BACKSPACE) && fileNameCopyLength > 0) {
            bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, backgroundWidth, 100);
            fileNameCopy[fileNameCopyLength - 1] = ' ';
            fileNameCopy[fileNameCopyLength] = '\0';
            fontDrawText(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, _colorTable[992]);
            fileNameCopyLength--;
            windowRefresh(win);
        } else if (keyCode < KEY_FIRST_INPUT_CHARACTER || keyCode > KEY_LAST_INPUT_CHARACTER || fileNameCopyLength >= 8) {
            if (keyCode == 502 && fileListLength != 0) {
                int mouseX;
                int mouseY;
                mouseGetPosition(&mouseX, &mouseY);

                int selectedLine = (mouseY - y - FILE_DIALOG_FILE_LIST_Y) / fontGetLineHeight();
                if (selectedLine - 1 < 0) {
                    selectedLine = 0;
                }

                if (isScrollable || selectedLine < fileListLength) {
                    if (selectedLine >= FILE_DIALOG_LINE_COUNT) {
                        selectedLine = FILE_DIALOG_LINE_COUNT - 1;
                    }
                } else {
                    selectedLine = fileListLength - 1;
                }

                selectedFileIndex = selectedLine;
                if (selectedFileIndex == doubleClickSelectedFileIndex) {
                    soundPlayFile("ib1p1xx1");
                    strncpy(dest, fileList[selectedFileIndex + pageOffset], 16);

                    int index;
                    for (index = 0; index < 12; index++) {
                        if (dest[index] == '.' || dest[index] == '\0') {
                            break;
                        }
                    }

                    dest[index] = '\0';
                    rc = 2;
                } else {
                    doubleClickSelectedFileIndex = selectedFileIndex;
                    bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, backgroundWidth, 100);
                    strncpy(fileNameCopy, fileList[selectedFileIndex + pageOffset], 16);

                    int index;
                    for (index = 0; index < 12; index++) {
                        if (fileNameCopy[index] == '.' || fileNameCopy[index] == '\0') {
                            break;
                        }
                    }

                    fileNameCopy[index] = '\0';
                    fileNameCopyLength = strlen(fileNameCopy);
                    fileNameCopy[fileNameCopyLength] = ' ';
                    fileNameCopy[fileNameCopyLength + 1] = '\0';

                    fontDrawText(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, _colorTable[992]);
                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                }
            } else if (keyCode == 506) {
                scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_UP;
            } else if (keyCode == 504) {
                scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_DOWN;
            } else {
                switch (keyCode) {
                case KEY_ARROW_UP:
                    pageOffset--;
                    if (pageOffset < 0) {
                        selectedFileIndex--;
                        if (selectedFileIndex < 0) {
                            selectedFileIndex = 0;
                        }
                        pageOffset = 0;
                    }
                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                case KEY_ARROW_DOWN:
                    if (isScrollable) {
                        pageOffset++;
                        if (pageOffset >= maxPageOffset) {
                            selectedFileIndex++;
                            if (selectedFileIndex >= FILE_DIALOG_LINE_COUNT) {
                                selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                            }
                            pageOffset = maxPageOffset;
                        }
                    } else {
                        selectedFileIndex++;
                        if (selectedFileIndex > maxPageOffset) {
                            selectedFileIndex = maxPageOffset;
                        }
                    }
                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                case KEY_HOME:
                    selectedFileIndex = 0;
                    pageOffset = 0;
                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                case KEY_END:
                    if (isScrollable) {
                        selectedFileIndex = 11;
                        pageOffset = maxPageOffset;
                    } else {
                        selectedFileIndex = maxPageOffset;
                        pageOffset = 0;
                    }
                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                }
            }
        } else if (_isdoschar(keyCode)) {
            bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, backgroundWidth, 100);

            fileNameCopy[fileNameCopyLength] = keyCode & 0xFF;
            fileNameCopy[fileNameCopyLength + 1] = ' ';
            fileNameCopy[fileNameCopyLength + 2] = '\0';
            fontDrawText(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, _colorTable[992]);
            fileNameCopyLength++;

            windowRefresh(win);
        }

        if (scrollDirection != FILE_DIALOG_SCROLL_DIRECTION_NONE) {
            unsigned int scrollDelay = 4;
            doubleClickSelectedFileIndex = -2;
            while (1) {
                unsigned int scrollTick = getTicks();
                scrollCounter += 1;
                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollDelay += 1;
                        if (scrollDelay > 24) {
                            scrollDelay = 24;
                        }
                    }

                    if (scrollDirection == FILE_DIALOG_SCROLL_DIRECTION_UP) {
                        pageOffset--;
                        if (pageOffset < 0) {
                            selectedFileIndex--;
                            if (selectedFileIndex < 0) {
                                selectedFileIndex = 0;
                            }
                            pageOffset = 0;
                        }
                    } else {
                        if (isScrollable) {
                            pageOffset++;
                            if (pageOffset > maxPageOffset) {
                                selectedFileIndex++;
                                if (selectedFileIndex >= FILE_DIALOG_LINE_COUNT) {
                                    selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                                }
                                pageOffset = maxPageOffset;
                            }
                        } else {
                            selectedFileIndex++;
                            if (selectedFileIndex > maxPageOffset) {
                                selectedFileIndex = maxPageOffset;
                            }
                        }
                    }

                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    windowRefresh(win);
                }

                // NOTE: Original code is slightly different. For unknown reason
                // entire blinking stuff is placed into two different branches,
                // which only differs by amount of delay. Probably result of
                // using large blinking macro as there are no traces of inlined
                // function.
                blinkingCounter -= 1;
                if (blinkingCounter == 0) {
                    blinkingCounter = 3;

                    int color = blink ? 100 : _colorTable[992];
                    blink = !blink;

                    bufferFill(fileNameBufferPtr + fontGetStringWidth(fileNameCopy) - cursorWidth, cursorWidth, cursorHeight - 2, backgroundWidth, color);
                }

                // FIXME: Missing windowRefresh makes blinking useless.

                unsigned int delay = (scrollCounter > 14.4) ? 1000 / scrollDelay : 1000 / 24;
                delay_ms(delay - (getTicks() - scrollTick));

                if (_game_user_wants_to_quit != 0) {
                    rc = 1;
                    break;
                }

                int key = inputGetInput();
                if (key == 505 || key == 503) {
                    break;
                }

                renderPresent();
            }
        } else {
            blinkingCounter -= 1;
            if (blinkingCounter == 0) {
                blinkingCounter = 3;

                int color = blink ? 100 : _colorTable[992];
                blink = !blink;

                bufferFill(fileNameBufferPtr + fontGetStringWidth(fileNameCopy) - cursorWidth, cursorWidth, cursorHeight - 2, backgroundWidth, color);
            }

            windowRefresh(win);

            doubleClickTimer--;
            if (doubleClickTimer == 0) {
                doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;
                doubleClickSelectedFileIndex = -2;
            }

            delay_ms(1000 / 24 - (getTicks() - tick));
        }

        if (_game_user_wants_to_quit != 0) {
            rc = 1;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    endTextInput();

    if (rc == 0) {
        if (fileNameCopyLength != 0) {
            fileNameCopy[fileNameCopyLength] = '\0';
            strcpy(dest, fileNameCopy);
        } else {
            rc = 1;
        }
    } else {
        if (rc == 2) {
            rc = 0;
        }
    }

    windowDestroy(win);
    messageListFree(&messageList);
    fontSetCurrent(oldFont);

    return rc;
}

// 0x41FBDC
static void fileDialogRenderFileList(unsigned char* buffer, char** fileList, int pageOffset, int fileListLength, int selectedIndex, int pitch)
{
    int lineHeight = fontGetLineHeight();
    int y = FILE_DIALOG_FILE_LIST_Y;
    bufferFill(buffer + y * pitch + FILE_DIALOG_FILE_LIST_X, FILE_DIALOG_FILE_LIST_WIDTH, FILE_DIALOG_FILE_LIST_HEIGHT, pitch, 100);
    if (fileListLength != 0) {
        if (fileListLength - pageOffset > FILE_DIALOG_LINE_COUNT) {
            fileListLength = FILE_DIALOG_LINE_COUNT;
        }

        for (int index = 0; index < fileListLength; index++) {
            int color = index == selectedIndex ? _colorTable[32747] : _colorTable[992];
            fontDrawText(buffer + pitch * y + FILE_DIALOG_FILE_LIST_X, fileList[pageOffset + index], FILE_DIALOG_FILE_LIST_WIDTH, pitch, color);
            y += lineHeight;
        }
    }
}

} // namespace fallout
