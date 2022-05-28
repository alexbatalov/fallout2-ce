#include "dbox.h"

#include "art.h"
#include "color.h"
#include "core.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "message.h"
#include "platform_compat.h"
#include "text_font.h"
#include "window_manager.h"
#include "word_wrap.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

// 0x5108C8
const int gDialogBoxBackgroundFrmIds[DIALOG_TYPE_COUNT] = {
    218, // MEDIALOG.FRM - Medium generic dialog box
    217, // LGDIALOG.FRM - Large generic dialog box
};

// 0x5108D0
const int _ytable[DIALOG_TYPE_COUNT] = {
    23,
    27,
};

// 0x5108D8
const int _xtable[DIALOG_TYPE_COUNT] = {
    29,
    29,
};

// 0x5108E0
const int _doneY[DIALOG_TYPE_COUNT] = {
    81,
    98,
};

// 0x5108E8
const int _doneX[DIALOG_TYPE_COUNT] = {
    51,
    37,
};

// 0x5108F0
const int _dblines[DIALOG_TYPE_COUNT] = {
    5,
    6,
};

// 0x510900
int _flgids[7] = {
    224, // loadbox.frm - character editor
    8, // lilredup.frm - little red button up
    9, // lilreddn.frm - little red button down
    181, // dnarwoff.frm - character editor
    182, // dnarwon.frm - character editor
    199, // uparwoff.frm - character editor
    200, // uparwon.frm - character editor
};

// 0x51091C
int _flgids2[7] = {
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

    bool v86 = false;

    bool hasTwoButtons = false;
    if (a8 != NULL) {
        hasTwoButtons = true;
    }

    bool hasTitle = false;
    if (title != NULL) {
        hasTitle = true;
    }

    if ((flags & DIALOG_BOX_YES_NO) != 0) {
        hasTwoButtons = true;
        flags |= DIALOG_BOX_LARGE;
        flags &= ~DIALOG_BOX_0x20;
    }

    int maximumLineWidth = 0;
    if (hasTitle) {
        maximumLineWidth = fontGetStringWidth(title);
    }

    int linesCount = 0;
    for (int index = 0; index < bodyLength; index++) {
        // NOTE: Calls [fontGetStringWidth] twice because of [max] macro.
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

    CacheEntry* backgroundHandle;
    int backgroundWidth;
    int backgroundHeight;
    int fid = buildFid(6, gDialogBoxBackgroundFrmIds[dialogType], 0, 0, 0);
    unsigned char* background = artLockFrameDataReturningSize(fid, &backgroundHandle, &backgroundWidth, &backgroundHeight);
    if (background == NULL) {
        fontSetCurrent(savedFont);
        return -1;
    }

    // Maintain original position in original resolution, otherwise center it.
    if (screenGetWidth() != 640) x = (screenGetWidth() - backgroundWidth) / 2;
    if (screenGetHeight() != 480) y = (screenGetHeight() - backgroundHeight) / 2;
    int win = windowCreate(x, y, backgroundWidth, backgroundHeight, 256, WINDOW_FLAG_0x10 | WINDOW_FLAG_0x04);
    if (win == -1) {
        artUnlock(backgroundHandle);
        fontSetCurrent(savedFont);
        return -1;
    }

    unsigned char* windowBuf = windowGetBuffer(win);
    memcpy(windowBuf, background, backgroundWidth * backgroundHeight);

    CacheEntry* doneBoxHandle = NULL;
    unsigned char* doneBox = NULL;
    int doneBoxWidth;
    int doneBoxHeight;

    CacheEntry* downButtonHandle = NULL;
    unsigned char* downButton = NULL;
    int downButtonWidth;
    int downButtonHeight;

    CacheEntry* upButtonHandle = NULL;
    unsigned char* upButton = NULL;

    if ((flags & DIALOG_BOX_0x20) == 0) {
        int doneBoxFid = buildFid(6, 209, 0, 0, 0);
        doneBox = artLockFrameDataReturningSize(doneBoxFid, &doneBoxHandle, &doneBoxWidth, &doneBoxHeight);
        if (doneBox == NULL) {
            artUnlock(backgroundHandle);
            fontSetCurrent(savedFont);
            windowDestroy(win);
            return -1;
        }

        int downButtonFid = buildFid(6, 9, 0, 0, 0);
        downButton = artLockFrameDataReturningSize(downButtonFid, &downButtonHandle, &downButtonWidth, &downButtonHeight);
        if (downButton == NULL) {
            artUnlock(doneBoxHandle);
            artUnlock(backgroundHandle);
            fontSetCurrent(savedFont);
            windowDestroy(win);
            return -1;
        }

        int upButtonFid = buildFid(6, 8, 0, 0, 0);
        upButton = artLockFrameData(upButtonFid, 0, 0, &upButtonHandle);
        if (upButton == NULL) {
            artUnlock(downButtonHandle);
            artUnlock(doneBoxHandle);
            artUnlock(backgroundHandle);
            fontSetCurrent(savedFont);
            windowDestroy(win);
            return -1;
        }

        int v27 = hasTwoButtons ? _doneX[dialogType] : (backgroundWidth - doneBoxWidth) / 2;
        blitBufferToBuffer(doneBox, doneBoxWidth, doneBoxHeight, doneBoxWidth, windowBuf + backgroundWidth * _doneY[dialogType] + v27, backgroundWidth);

        if (!messageListInit(&messageList)) {
            artUnlock(upButtonHandle);
            artUnlock(downButtonHandle);
            artUnlock(doneBoxHandle);
            artUnlock(backgroundHandle);
            fontSetCurrent(savedFont);
            windowDestroy(win);
            return -1;
        }

        char path[COMPAT_MAX_PATH];
        sprintf(path, "%s%s", asc_5186C8, "DBOX.MSG");

        if (!messageListLoad(&messageList, path)) {
            artUnlock(upButtonHandle);
            artUnlock(downButtonHandle);
            artUnlock(doneBoxHandle);
            artUnlock(backgroundHandle);
            fontSetCurrent(savedFont);
            // FIXME: Window is not removed.
            return -1;
        }

        fontSetCurrent(103);

        // 100 - DONE
        // 101 - YES
        messageListItem.num = (flags & DIALOG_BOX_YES_NO) == 0 ? 100 : 101;
        if (messageListGetItem(&messageList, &messageListItem)) {
            fontDrawText(windowBuf + backgroundWidth * (_doneY[dialogType] + 3) + v27 + 35, messageListItem.text, backgroundWidth, backgroundWidth, _colorTable[18979]);
        }

        int btn = buttonCreate(win, v27 + 13, _doneY[dialogType] + 4, downButtonWidth, downButtonHeight, -1, -1, -1, 500, upButton, downButton, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
        }

        v86 = true;
    }

    if (hasTwoButtons && dialogType == DIALOG_TYPE_LARGE) {
        if (v86) {
            if ((flags & DIALOG_BOX_YES_NO) != 0) {
                a8 = getmsg(&messageList, &messageListItem, 102);
            }

            fontSetCurrent(103);

            blitBufferToBufferTrans(doneBox,
                doneBoxWidth,
                doneBoxHeight,
                doneBoxWidth,
                windowBuf + backgroundWidth * _doneY[dialogType] + _doneX[dialogType] + doneBoxWidth + 24,
                backgroundWidth);

            fontDrawText(windowBuf + backgroundWidth * (_doneY[dialogType] + 3) + _doneX[dialogType] + doneBoxWidth + 59,
                a8, backgroundWidth, backgroundWidth, _colorTable[18979]);

            int btn = buttonCreate(win,
                doneBoxWidth + _doneX[dialogType] + 37,
                _doneY[dialogType] + 4,
                downButtonWidth,
                downButtonHeight,
                -1, -1, -1, 501, upButton, downButton, 0, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }
        } else {
            int doneBoxFid = buildFid(6, 209, 0, 0, 0);
            unsigned char* doneBox = artLockFrameDataReturningSize(doneBoxFid, &doneBoxHandle, &doneBoxWidth, &doneBoxHeight);
            if (doneBox == NULL) {
                artUnlock(backgroundHandle);
                fontSetCurrent(savedFont);
                windowDestroy(win);
                return -1;
            }

            int downButtonFid = buildFid(6, 9, 0, 0, 0);
            unsigned char* downButton = artLockFrameDataReturningSize(downButtonFid, &downButtonHandle, &downButtonWidth, &downButtonHeight);
            if (downButton == NULL) {
                artUnlock(doneBoxHandle);
                artUnlock(backgroundHandle);
                fontSetCurrent(savedFont);
                windowDestroy(win);
                return -1;
            }

            int upButtonFid = buildFid(6, 8, 0, 0, 0);
            unsigned char* upButton = artLockFrameData(upButtonFid, 0, 0, &upButtonHandle);
            if (upButton == NULL) {
                artUnlock(downButtonHandle);
                artUnlock(doneBoxHandle);
                artUnlock(backgroundHandle);
                fontSetCurrent(savedFont);
                windowDestroy(win);
                return -1;
            }

            if (!messageListInit(&messageList)) {
                artUnlock(upButtonHandle);
                artUnlock(downButtonHandle);
                artUnlock(doneBoxHandle);
                artUnlock(backgroundHandle);
                fontSetCurrent(savedFont);
                windowDestroy(win);
                return -1;
            }

            char path[COMPAT_MAX_PATH];
            sprintf(path, "%s%s", asc_5186C8, "DBOX.MSG");

            if (!messageListLoad(&messageList, path)) {
                artUnlock(upButtonHandle);
                artUnlock(downButtonHandle);
                artUnlock(doneBoxHandle);
                artUnlock(backgroundHandle);
                fontSetCurrent(savedFont);
                windowDestroy(win);
                return -1;
            }

            blitBufferToBufferTrans(doneBox,
                doneBoxWidth,
                doneBoxHeight,
                doneBoxWidth,
                windowBuf + backgroundWidth * _doneY[dialogType] + _doneX[dialogType],
                backgroundWidth);

            fontSetCurrent(103);

            fontDrawText(windowBuf + backgroundWidth * (_doneY[dialogType] + 3) + _doneX[dialogType] + 35,
                a8, backgroundWidth, backgroundWidth, _colorTable[18979]);

            int btn = buttonCreate(win,
                _doneX[dialogType] + 13,
                _doneY[dialogType] + 4,
                downButtonWidth,
                downButtonHeight,
                -1,
                -1,
                -1,
                501,
                upButton,
                downButton,
                NULL,
                BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }

            v86 = true;
        }
    }

    fontSetCurrent(101);

    int v23 = _ytable[dialogType];

    if ((flags & DIALOG_BOX_NO_VERTICAL_CENTERING) == 0) {
        int v41 = _dblines[dialogType] * fontGetLineHeight() / 2 + v23;
        v23 = v41 - ((bodyLength + 1) * fontGetLineHeight() / 2);
    }

    if (hasTitle) {
        if ((flags & DIALOG_BOX_NO_HORIZONTAL_CENTERING) != 0) {
            fontDrawText(windowBuf + backgroundWidth * v23 + _xtable[dialogType], title, backgroundWidth, backgroundWidth, titleColor);
        } else {
            int length = fontGetStringWidth(title);
            fontDrawText(windowBuf + backgroundWidth * v23 + (backgroundWidth - length) / 2, title, backgroundWidth, backgroundWidth, titleColor);
        }
        v23 += fontGetLineHeight();
    }

    for (int v94 = 0; v94 < bodyLength; v94++) {
        int len = fontGetStringWidth(body[v94]);
        if (len <= backgroundWidth - 26) {
            if ((flags & DIALOG_BOX_NO_HORIZONTAL_CENTERING) != 0) {
                fontDrawText(windowBuf + backgroundWidth * v23 + _xtable[dialogType], body[v94], backgroundWidth, backgroundWidth, bodyColor);
            } else {
                int length = fontGetStringWidth(body[v94]);
                fontDrawText(windowBuf + backgroundWidth * v23 + (backgroundWidth - length) / 2, body[v94], backgroundWidth, backgroundWidth, bodyColor);
            }
            v23 += fontGetLineHeight();
        } else {
            short beginnings[WORD_WRAP_MAX_COUNT];
            short count;
            if (wordWrap(body[v94], backgroundWidth - 26, beginnings, &count) != 0) {
                debugPrint("\nError: dialog_out");
            }

            for (int v48 = 1; v48 < count; v48++) {
                int v51 = beginnings[v48] - beginnings[v48 - 1];
                if (v51 >= 260) {
                    v51 = 259;
                }

                char string[260];
                strncpy(string, body[v94] + beginnings[v48 - 1], v51);
                string[v51] = '\0';

                if ((flags & DIALOG_BOX_NO_HORIZONTAL_CENTERING) != 0) {
                    fontDrawText(windowBuf + backgroundWidth * v23 + _xtable[dialogType], string, backgroundWidth, backgroundWidth, bodyColor);
                } else {
                    int length = fontGetStringWidth(string);
                    fontDrawText(windowBuf + backgroundWidth * v23 + (backgroundWidth - length) / 2, string, backgroundWidth, backgroundWidth, bodyColor);
                }
                v23 += fontGetLineHeight();
            }
        }
    }

    windowRefresh(win);

    int rc = -1;
    while (rc == -1) {
        int keyCode = _get_input();

        if (keyCode == 500) {
            rc = 1;
        } else if (keyCode == KEY_RETURN) {
            soundPlayFile("ib1p1xx1");
            rc = 1;
        } else if (keyCode == KEY_ESCAPE || keyCode == 501) {
            rc = 0;
        } else {
            if ((flags & 0x10) != 0) {
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
    }

    windowDestroy(win);
    artUnlock(backgroundHandle);
    fontSetCurrent(savedFont);

    if (v86) {
        artUnlock(doneBoxHandle);
        artUnlock(downButtonHandle);
        artUnlock(upButtonHandle);
        messageListFree(&messageList);
    }

    return rc;
}

// 0x41EA78
int _save_file_dialog(char* a1, char** fileList, char* fileName, int fileListLength, int x, int y, int flags)
{
    int oldFont = fontGetCurrent();

    unsigned char* frmBuffers[7];
    CacheEntry* frmHandles[7];
    Size frmSizes[7];

    for (int index = 0; index < 7; index++) {
        int fid = buildFid(6, _flgids2[index], 0, 0, 0);
        frmBuffers[index] = artLockFrameDataReturningSize(fid, &(frmHandles[index]), &(frmSizes[index].width), &(frmSizes[index].height));
        if (frmBuffers[index] == NULL) {
            while (--index >= 0) {
                artUnlock(frmHandles[index]);
            }
            return -1;
        }
    }

    int win = windowCreate(x, y, frmSizes[0].width, frmSizes[0].height, 256, WINDOW_FLAG_0x10 | WINDOW_FLAG_0x04);
    if (win == -1) {
        for (int index = 0; index < 7; index++) {
            artUnlock(frmHandles[index]);
        }
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(win);
    memcpy(windowBuffer, frmBuffers[0], frmSizes[0].width * frmSizes[0].height);

    MessageList messageList;
    MessageListItem messageListItem;

    if (!messageListInit(&messageList)) {
        windowDestroy(win);

        for (int index = 0; index < 7; index++) {
            artUnlock(frmHandles[index]);
        }

        return -1;
    }

    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s%s", asc_5186C8, "DBOX.MSG");

    if (!messageListLoad(&messageList, path)) {
        windowDestroy(win);

        for (int index = 0; index < 7; index++) {
            artUnlock(frmHandles[index]);
        }

        return -1;
    }

    fontSetCurrent(103);

    // DONE
    const char* done = getmsg(&messageList, &messageListItem, 100);
    fontDrawText(windowBuffer + frmSizes[0].width * 213 + 79, done, frmSizes[0].width, frmSizes[0].width, _colorTable[18979]);

    // CANCEL
    const char* cancel = getmsg(&messageList, &messageListItem, 103);
    fontDrawText(windowBuffer + frmSizes[0].width * 213 + 182, cancel, frmSizes[0].width, frmSizes[0].width, _colorTable[18979]);

    int doneBtn = buttonCreate(win,
        58,
        214,
        frmSizes[2].width,
        frmSizes[2].height,
        -1,
        -1,
        -1,
        500,
        frmBuffers[1],
        frmBuffers[2],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int cancelBtn = buttonCreate(win,
        163,
        214,
        frmSizes[2].width,
        frmSizes[2].height,
        -1,
        -1,
        -1,
        501,
        frmBuffers[1],
        frmBuffers[2],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int scrollUpBtn = buttonCreate(win,
        36,
        44,
        frmSizes[6].width,
        frmSizes[6].height,
        -1,
        505,
        506,
        505,
        frmBuffers[5],
        frmBuffers[6],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int scrollDownButton = buttonCreate(win,
        36,
        44 + frmSizes[6].height,
        frmSizes[4].width,
        frmSizes[4].height,
        -1,
        503,
        504,
        503,
        frmBuffers[3],
        frmBuffers[4],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    buttonCreate(
        win,
        55,
        49,
        190,
        124,
        -1,
        -1,
        -1,
        502,
        NULL,
        NULL,
        NULL,
        0);

    if (a1 != NULL) {
        fontDrawText(windowBuffer + frmSizes[0].width * 16 + 49, a1, frmSizes[0].width, frmSizes[0].width, _colorTable[18979]);
    }
}

// 0x41FBDC
void _PrntFlist(unsigned char* buffer, char** fileList, int pageOffset, int fileListLength, int selectedIndex, int pitch)
{
    int lineHeight = fontGetLineHeight();
    int y = 49;
    bufferFill(buffer + y * pitch + 55, 190, 124, pitch, 100);
    if (fileListLength != 0) {
        if (fileListLength - pageOffset > 12) {
            fileListLength = 12;
        }

        for (int index = 0; index < fileListLength; index++) {
            int color = index == selectedIndex ? _colorTable[32747] : _colorTable[992];
            fontDrawText(buffer + y * index + 55, fileList[index], pitch, pitch, color);
            y += lineHeight;
        }
    }
}
