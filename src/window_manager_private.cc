#include "window_manager_private.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "color.h"
#include "draw.h"
#include "input.h"
#include "kb.h"
#include "memory.h"
#include "mouse.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"

namespace fallout {

/// Maximum number of timed messages.
static constexpr int kTimedMsgs = 5;

static int get_num_i(int win, int* value, int max_chars_wcursor, bool clear, bool allow_negative, int x, int y);
static void tm_watch_msgs();
static void tm_kill_msg();
static void tm_kill_out_of_order(int queueIndex);
static void tm_click_response(int btn, int keyCode);
static bool tm_index_active(int queueIndex);

// 0x51E414
static int _wd = -1;

// 0x51E418
static MenuBar* _curr_menu = nullptr;

// 0x51E41C
static bool tm_watch_active = false;

// NOTE: Also anonymous in the original code.
//
// 0x6B2340
static struct {
    bool taken;
    int y;
} tm_location[kTimedMsgs];

// 0x6B2368
static int tm_text_x;

// 0x6B236C
static int tm_h;

// NOTE: Also anonymous in the original code.
//
// 0x6B2370
static struct {
    unsigned int created;
    int id;
    int location;
} tm_queue[kTimedMsgs];

// 0x6B23AC
static unsigned int tm_persistence;

// 0x6B23B0
static int scr_center_x;

// 0x6B23B4
static int tm_text_y;

// 0x6B23B8
static int tm_kill;

// 0x6B23BC
static int tm_add;

// x
//
// 0x6B23C0
static int _curry;

// y
//
// 0x6B23C4
static int _currx;

// 0x6B23D0
char gProgramWindowTitle[256];

// 0x4DA6C0
int _win_list_select(const char* title, char** fileList, int fileListLength, ListSelectionHandler* callback, int x, int y, int color)
{
    return _win_list_select_at(title, fileList, fileListLength, callback, x, y, color, 0);
}

// 0x4DA70C
int _win_list_select_at(const char* title, char** items, int itemsLength, ListSelectionHandler* callback, int x, int y, int color, int start)
{
    if (!gWindowSystemInitialized) {
        return -1;
    }

    int listViewWidth = _win_width_needed(items, itemsLength);
    int windowWidth = listViewWidth + 16;

    int titleWidth = fontGetStringWidth(title);
    if (titleWidth > windowWidth) {
        windowWidth = titleWidth;
        listViewWidth = titleWidth - 16;
    }

    windowWidth += 20;

    int win;
    int windowHeight;
    int listViewCapacity = 10;
    for (int heightMultiplier = 13; heightMultiplier > 8; heightMultiplier--) {
        windowHeight = heightMultiplier * fontGetLineHeight() + 22;
        win = windowCreate(x, y, windowWidth, windowHeight, 256, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
        if (win != -1) {
            break;
        }
        listViewCapacity--;
    }

    if (win == -1) {
        return -1;
    }

    Window* window = windowGetWindow(win);
    Rect* windowRect = &(window->rect);
    unsigned char* windowBuffer = window->buffer;

    bufferDrawRect(windowBuffer,
        windowWidth,
        0,
        0,
        windowWidth - 1,
        windowHeight - 1,
        _colorTable[0]);
    bufferDrawRectShadowed(windowBuffer,
        windowWidth,
        1,
        1,
        windowWidth - 2,
        windowHeight - 2,
        _colorTable[_GNW_wcolor[1]],
        _colorTable[_GNW_wcolor[2]]);

    bufferFill(windowBuffer + windowWidth * 5 + 5,
        windowWidth - 11,
        fontGetLineHeight() + 3,
        windowWidth,
        _colorTable[_GNW_wcolor[0]]);

    fontDrawText(windowBuffer + windowWidth / 2 + 8 * windowWidth - fontGetStringWidth(title) / 2,
        title,
        windowWidth,
        windowWidth,
        _colorTable[_GNW_wcolor[3]]);

    bufferDrawRectShadowed(windowBuffer,
        windowWidth,
        5,
        5,
        windowWidth - 6,
        fontGetLineHeight() + 8,
        _colorTable[_GNW_wcolor[2]],
        _colorTable[_GNW_wcolor[1]]);

    int listViewX = 8;
    int listViewY = fontGetLineHeight() + 16;
    unsigned char* listViewBuffer = windowBuffer + windowWidth * listViewY + listViewX;
    int listViewMaxY = listViewCapacity * fontGetLineHeight() + listViewY;

    bufferFill(listViewBuffer + windowWidth * (-2) + (-3),
        listViewWidth + listViewX - 2,
        listViewCapacity * fontGetLineHeight() + 2,
        windowWidth,
        _colorTable[_GNW_wcolor[0]]);

    int scrollOffset = start;
    if (start < 0 || start >= itemsLength) {
        scrollOffset = 0;
    }

    // Relative to `scrollOffset`.
    int selectedItemIndex;
    if (itemsLength - scrollOffset < listViewCapacity) {
        int newScrollOffset = itemsLength - listViewCapacity;
        if (newScrollOffset < 0) {
            newScrollOffset = 0;
        }
        int oldScrollOffset = scrollOffset;
        scrollOffset = newScrollOffset;
        selectedItemIndex = oldScrollOffset - newScrollOffset;
    } else {
        selectedItemIndex = 0;
    }

    _win_text(win,
        items + start,
        itemsLength < listViewCapacity ? itemsLength : listViewCapacity,
        listViewWidth,
        listViewX,
        listViewY,
        color | 0x2000000);

    _lighten_buf(listViewBuffer + windowWidth * selectedItemIndex * fontGetLineHeight(),
        listViewWidth,
        fontGetLineHeight(),
        windowWidth);

    bufferDrawRectShadowed(windowBuffer,
        windowWidth,
        5,
        listViewY - 3,
        listViewWidth + 10,
        listViewMaxY,
        _colorTable[_GNW_wcolor[2]],
        _colorTable[_GNW_wcolor[1]]);

    _win_register_text_button(win,
        windowWidth - 25,
        listViewY - 3,
        -1,
        -1,
        KEY_ARROW_UP,
        -1,
        "\x18",
        0);

    _win_register_text_button(win,
        windowWidth - 25,
        listViewMaxY - fontGetLineHeight() - 5,
        -1,
        -1,
        KEY_ARROW_DOWN,
        -1,
        "\x19",
        0);

    _win_register_text_button(win,
        windowWidth / 2 - 32,
        windowHeight - 8 - fontGetLineHeight() - 6,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        "Done",
        0);

    int scrollbarX = windowWidth - 21;
    int scrollbarY = listViewY + fontGetLineHeight() + 7;
    int scrollbarKnobSize = 14;
    int scrollbarHeight = listViewMaxY - scrollbarY;
    unsigned char* scrollbarBuffer = windowBuffer + windowWidth * scrollbarY + scrollbarX;

    bufferFill(scrollbarBuffer,
        scrollbarKnobSize + 1,
        scrollbarHeight - fontGetLineHeight() - 8,
        windowWidth,
        _colorTable[_GNW_wcolor[0]]);

    buttonCreate(win,
        scrollbarX,
        scrollbarY,
        scrollbarKnobSize + 1,
        scrollbarHeight - fontGetLineHeight() - 8,
        -1,
        -1,
        2048,
        -1,
        nullptr,
        nullptr,
        nullptr,
        0);

    bufferDrawRectShadowed(windowBuffer,
        windowWidth,
        windowWidth - 22,
        scrollbarY - 1,
        scrollbarX + scrollbarKnobSize + 1,
        listViewMaxY - fontGetLineHeight() - 9,
        _colorTable[_GNW_wcolor[2]],
        _colorTable[_GNW_wcolor[1]]);
    bufferDrawRectShadowed(windowBuffer,
        windowWidth,
        scrollbarX,
        scrollbarY,
        scrollbarX + scrollbarKnobSize,
        scrollbarY + scrollbarKnobSize,
        _colorTable[_GNW_wcolor[1]],
        _colorTable[_GNW_wcolor[2]]);

    _lighten_buf(scrollbarBuffer, scrollbarKnobSize, scrollbarKnobSize, windowWidth);

    for (int index = 0; index < listViewCapacity; index++) {
        buttonCreate(win,
            listViewX,
            listViewY + index * fontGetLineHeight(),
            listViewWidth,
            fontGetLineHeight(),
            512 + index,
            -1,
            1024 + index,
            -1,
            nullptr,
            nullptr,
            nullptr,
            0);
    }

    buttonCreate(win,
        0,
        0,
        windowWidth,
        fontGetLineHeight() + 8,
        -1,
        -1,
        -1,
        -1,
        nullptr,
        nullptr,
        nullptr,
        BUTTON_DRAG_HANDLE);

    windowRefresh(win);

    int absoluteSelectedItemIndex = -1;

    // Relative to `scrollOffset`.
    int previousSelectedItemIndex = -1;
    while (1) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        int mouseX;
        int mouseY;
        mouseGetPosition(&mouseX, &mouseY);

        if (keyCode == KEY_RETURN || (keyCode >= 1024 && keyCode < listViewCapacity + 1024)) {
            if (selectedItemIndex != -1) {
                absoluteSelectedItemIndex = scrollOffset + selectedItemIndex;
                if (absoluteSelectedItemIndex < itemsLength) {
                    if (callback == nullptr) {
                        break;
                    }

                    callback(items, absoluteSelectedItemIndex);
                }
                absoluteSelectedItemIndex = -1;
            }
        } else if (keyCode == 2048) {
            if (window->rect.top + scrollbarY > mouseY) {
                keyCode = KEY_PAGE_UP;
            } else if (window->rect.top + scrollbarKnobSize + scrollbarY < mouseY) {
                keyCode = KEY_PAGE_DOWN;
            }
        }

        if (keyCode == KEY_ESCAPE) {
            break;
        }

        if (keyCode >= 512 && keyCode < listViewCapacity + 512) {
            int itemIndex = keyCode - 512;
            if (itemIndex != selectedItemIndex && itemIndex < itemsLength) {
                previousSelectedItemIndex = selectedItemIndex;
                selectedItemIndex = itemIndex;
                keyCode = -3;
            } else {
                continue;
            }
        } else {
            switch (keyCode) {
            case KEY_HOME:
                if (scrollOffset > 0) {
                    keyCode = -4;
                    scrollOffset = 0;
                }
                break;
            case KEY_ARROW_UP:
                if (selectedItemIndex > 0) {
                    keyCode = -3;
                    previousSelectedItemIndex = selectedItemIndex;
                    selectedItemIndex -= 1;
                } else {
                    if (scrollOffset > 0) {
                        keyCode = -4;
                        scrollOffset -= 1;
                    }
                }
                break;
            case KEY_PAGE_UP:
                if (scrollOffset > 0) {
                    scrollOffset -= listViewCapacity;
                    if (scrollOffset < 0) {
                        scrollOffset = 0;
                    }
                    keyCode = -4;
                }
                break;
            case KEY_END:
                if (scrollOffset < itemsLength - listViewCapacity) {
                    keyCode = -4;
                    scrollOffset = itemsLength - listViewCapacity;
                }
                break;
            case KEY_ARROW_DOWN:
                if (selectedItemIndex < listViewCapacity - 1 && selectedItemIndex < itemsLength - 1) {
                    keyCode = -3;
                    previousSelectedItemIndex = selectedItemIndex;
                    selectedItemIndex += 1;
                } else {
                    if (scrollOffset + listViewCapacity < itemsLength) {
                        keyCode = -4;
                        scrollOffset += 1;
                    }
                }
                break;
            case KEY_PAGE_DOWN:
                if (scrollOffset < itemsLength - listViewCapacity) {
                    scrollOffset += listViewCapacity;
                    if (scrollOffset > itemsLength - listViewCapacity) {
                        scrollOffset = itemsLength - listViewCapacity;
                    }
                    keyCode = -4;
                }
                break;
            default:
                if (itemsLength > listViewCapacity) {
                    if ((keyCode >= 'a' && keyCode <= 'z')
                        || (keyCode >= 'A' && keyCode <= 'Z')) {
                        int found = _find_first_letter(keyCode, items, itemsLength);
                        if (found != -1) {
                            scrollOffset = found;
                            if (scrollOffset > itemsLength - listViewCapacity) {
                                scrollOffset = itemsLength - listViewCapacity;
                            }
                            keyCode = -4;
                            selectedItemIndex = found - scrollOffset;
                        }
                    }
                }
                break;
            }
        }

        if (keyCode == -4) {
            bufferFill(listViewBuffer,
                listViewWidth,
                listViewMaxY - listViewY,
                windowWidth,
                _colorTable[_GNW_wcolor[0]]);

            _win_text(win,
                items + scrollOffset,
                itemsLength < listViewCapacity ? itemsLength : listViewCapacity,
                listViewWidth,
                listViewX,
                listViewY,
                color | 0x2000000);

            _lighten_buf(listViewBuffer + windowWidth * selectedItemIndex * fontGetLineHeight(),
                listViewWidth,
                fontGetLineHeight(),
                windowWidth);

            if (itemsLength > listViewCapacity) {
                bufferFill(windowBuffer + windowWidth * scrollbarY + scrollbarX,
                    scrollbarKnobSize + 1,
                    scrollbarKnobSize + 1,
                    windowWidth,
                    _colorTable[_GNW_wcolor[0]]);

                scrollbarY = (scrollOffset * (listViewMaxY - listViewY - 2 * fontGetLineHeight() - 16 - scrollbarKnobSize - 1)) / (itemsLength - listViewCapacity)
                    + listViewY + fontGetLineHeight() + 7;

                bufferDrawRectShadowed(windowBuffer,
                    windowWidth,
                    scrollbarX,
                    scrollbarY,
                    scrollbarX + scrollbarKnobSize,
                    scrollbarY + scrollbarKnobSize,
                    _colorTable[_GNW_wcolor[1]],
                    _colorTable[_GNW_wcolor[2]]);

                _lighten_buf(windowBuffer + windowWidth * scrollbarY + scrollbarX,
                    scrollbarKnobSize,
                    scrollbarKnobSize,
                    windowWidth);

                _GNW_win_refresh(window, windowRect, nullptr);
            }
        } else if (keyCode == -3) {
            Rect itemRect;
            itemRect.left = windowRect->left + listViewX;
            itemRect.right = itemRect.left + listViewWidth;

            if (previousSelectedItemIndex != -1) {
                itemRect.top = windowRect->top + listViewY + previousSelectedItemIndex * fontGetLineHeight();
                itemRect.bottom = itemRect.top + fontGetLineHeight();

                bufferFill(listViewBuffer + windowWidth * previousSelectedItemIndex * fontGetLineHeight(),
                    listViewWidth,
                    fontGetLineHeight(),
                    windowWidth,
                    _colorTable[_GNW_wcolor[0]]);

                int textColor;
                if ((color & 0xFF00) != 0) {
                    int colorIndex = (color & 0xFF) - 1;
                    textColor = (color & ~0xFFFF) | _colorTable[_GNW_wcolor[colorIndex]];
                } else {
                    textColor = color;
                }

                fontDrawText(listViewBuffer + windowWidth * previousSelectedItemIndex * fontGetLineHeight(),
                    items[scrollOffset + previousSelectedItemIndex],
                    windowWidth,
                    windowWidth,
                    textColor);

                _GNW_win_refresh(window, &itemRect, nullptr);
            }

            if (selectedItemIndex != -1) {
                itemRect.top = windowRect->top + listViewY + selectedItemIndex * fontGetLineHeight();
                itemRect.bottom = itemRect.top + fontGetLineHeight();

                _lighten_buf(listViewBuffer + windowWidth * selectedItemIndex * fontGetLineHeight(),
                    listViewWidth,
                    fontGetLineHeight(),
                    windowWidth);

                _GNW_win_refresh(window, &itemRect, nullptr);
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);

    return absoluteSelectedItemIndex;
}

// 0x4DB478
int _win_get_str(char* dest, int length, const char* title, int x, int y)
{
    if (!gWindowSystemInitialized) {
        return -1;
    }

    int titleWidth = fontGetStringWidth(title) + 12;
    if (titleWidth < fontGetMonospacedCharacterWidth() * length) {
        titleWidth = fontGetMonospacedCharacterWidth() * length;
    }

    int windowWidth = titleWidth + 16;
    if (windowWidth < 160) {
        windowWidth = 160;
    }

    int windowHeight = 5 * fontGetLineHeight() + 16;

    int win = windowCreate(x, y, windowWidth, windowHeight, 256, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    windowDrawBorder(win);

    unsigned char* windowBuffer = windowGetBuffer(win);

    bufferFill(windowBuffer + windowWidth * (fontGetLineHeight() + 14) + 14,
        windowWidth - 28,
        fontGetLineHeight() + 2,
        windowWidth,
        _colorTable[_GNW_wcolor[0]]);
    fontDrawText(windowBuffer + windowWidth * 8 + 8, title, windowWidth, windowWidth, _colorTable[_GNW_wcolor[4]]);

    bufferDrawRectShadowed(windowBuffer,
        windowWidth,
        14,
        fontGetLineHeight() + 14,
        windowWidth - 14,
        2 * fontGetLineHeight() + 16,
        _colorTable[_GNW_wcolor[2]],
        _colorTable[_GNW_wcolor[1]]);

    _win_register_text_button(win,
        windowWidth / 2 - 72,
        windowHeight - 8 - fontGetLineHeight() - 6,
        -1,
        -1,
        -1,
        KEY_RETURN,
        "Done",
        0);

    _win_register_text_button(win,
        windowWidth / 2 + 8,
        windowHeight - 8 - fontGetLineHeight() - 6,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        "Cancel",
        0);

    windowRefresh(win);

    int rc = _win_input_str(win,
        dest,
        length,
        16,
        fontGetLineHeight() + 16,
        _colorTable[_GNW_wcolor[3]],
        _colorTable[_GNW_wcolor[0]]);

    windowDestroy(win);

    return rc;
}

// 0x4DB920
int win_yes_no(const char* question, int x, int y, int color)
{
    if (!gWindowSystemInitialized) {
        return -1;
    }

    int height = 3 * fontGetLineHeight() + 16;
    int width = std::max(fontGetStringWidth(question) + 16, 144) + 16;

    int win = windowCreate(x, y, width, height, 0x100, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    windowDrawBorder(win);

    unsigned char* windowBuffer = windowGetWindow(win)->buffer;

    int textColor;
    if ((color & 0xFF00) != 0) {
        int colorIndex = (color & 0xFF) - 1;
        textColor = (color & ~0xFFFF) | _colorTable[_GNW_wcolor[colorIndex]];
    } else {
        textColor = color;
    }

    fontDrawText(windowBuffer + 8 * width + 16,
        question,
        width,
        width,
        textColor);

    _win_register_text_button(win,
        width / 2 - 64,
        height - fontGetLineHeight() - 14,
        -1,
        -1,
        -1,
        'y',
        "Yes",
        0);
    _win_register_text_button(win,
        width / 2 + 16,
        height - fontGetLineHeight() - 14,
        -1,
        -1,
        -1,
        'n',
        "No",
        0);
    windowRefresh(win);

    // NOTE: Original code is slightly different but does the same thing.
    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        switch (inputGetInput()) {
        case KEY_ESCAPE:
        case 'n':
        case 'N':
            rc = 0;
            break;
        case KEY_RETURN:
        case KEY_SPACE:
        case 'y':
        case 'Y':
            rc = 1;
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);

    return rc;
}

// 0x4DBA98
int _win_msg(const char* string, int x, int y, int color)
{
    if (!gWindowSystemInitialized) {
        return -1;
    }

    int windowHeight = 3 * fontGetLineHeight() + 16;

    int windowWidth = fontGetStringWidth(string) + 16;
    if (windowWidth < 80) {
        windowWidth = 80;
    }

    windowWidth += 16;

    int win = windowCreate(x, y, windowWidth, windowHeight, 256, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    windowDrawBorder(win);

    Window* window = windowGetWindow(win);
    unsigned char* windowBuffer = window->buffer;

    int textColor;
    if ((color & 0xFF00) != 0) {
        int colorIndex = (color & 0xFF) - 1;
        textColor = (color & ~0xFFFF) | _colorTable[_GNW_wcolor[colorIndex]];
    } else {
        textColor = color;
    }

    fontDrawText(windowBuffer + windowWidth * 8 + 16, string, windowWidth, windowWidth, textColor);

    _win_register_text_button(win,
        windowWidth / 2 - 32,
        windowHeight - 8 - fontGetLineHeight() - 6,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        "Done",
        0);

    windowRefresh(win);

    while (inputGetInput() != KEY_ESCAPE) {
        sharedFpsLimiter.mark();
        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);

    return 0;
}

// 0x4DBBC4
int _win_pull_down(char** items, int itemsLength, int x, int y, int color)
{
    if (!gWindowSystemInitialized) {
        return -1;
    }

    Rect rect;
    int win = _create_pull_down(items, itemsLength, x, y, color, _colorTable[_GNW_wcolor[0]], &rect);
    if (win == -1) {
        return -1;
    }

    return process_pull_down(win, &rect, items, itemsLength, color, _colorTable[_GNW_wcolor[0]], nullptr, -1);
}

// 0x4DBC34
int _create_pull_down(char** stringList, int stringListLength, int x, int y, int foregroundColor, int backgroundColor, Rect* rect)
{
    int windowHeight = stringListLength * fontGetLineHeight() + 16;
    int windowWidth = _win_width_needed(stringList, stringListLength) + 4;
    if (windowHeight < 2 || windowWidth < 2) {
        return -1;
    }

    int win = windowCreate(x, y, windowWidth, windowHeight, backgroundColor, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    _win_text(win, stringList, stringListLength, windowWidth - 4, 2, 8, foregroundColor);
    windowDrawRect(win, 0, 0, windowWidth - 1, windowHeight - 1, _colorTable[0]);
    windowDrawRect(win, 1, 1, windowWidth - 2, windowHeight - 2, foregroundColor);
    windowRefresh(win);
    windowGetRect(win, rect);

    return win;
}

// 0x4DC30C
int _win_debug(char* string)
{
    if (!gWindowSystemInitialized) {
        return -1;
    }

    // CE: Debug window metrics were designed for default DOS-style font (0).
    // We don't expect caller to properly set one, so without manually forcing
    // it debug window might contain mixed fonts.
    int oldFont = fontGetCurrent();
    fontSetCurrent(0);

    int lineHeight = fontGetLineHeight();

    if (_wd == -1) {
        _wd = windowCreate(80, 80, 300, 192, 256, WINDOW_MOVE_ON_TOP);
        if (_wd == -1) {
            return -1;
        }

        windowDrawBorder(_wd);

        Window* window = windowGetWindow(_wd);
        unsigned char* windowBuffer = window->buffer;

        windowFill(_wd, 8, 8, 284, lineHeight, 0x100 | 1);

        windowDrawText(_wd,
            "Debug",
            0,
            (300 - fontGetStringWidth("Debug")) / 2,
            8,
            0x2000000 | 0x100 | 4);

        bufferDrawRectShadowed(windowBuffer,
            300,
            8,
            8,
            291,
            lineHeight + 8,
            _colorTable[_GNW_wcolor[2]],
            _colorTable[_GNW_wcolor[1]]);

        windowFill(_wd, 9, 26, 282, 135, 0x100 | 1);

        bufferDrawRectShadowed(windowBuffer,
            300,
            8,
            25,
            291,
            lineHeight + 145,
            _colorTable[_GNW_wcolor[2]],
            _colorTable[_GNW_wcolor[1]]);

        _currx = 9;
        _curry = 26;

        int btn = _win_register_text_button(_wd,
            (300 - fontGetStringWidth("Close")) / 2,
            192 - 8 - lineHeight - 6,
            -1,
            -1,
            -1,
            -1,
            "Close",
            0);
        buttonSetMouseCallbacks(btn, nullptr, nullptr, nullptr, _win_debug_delete);

        buttonCreate(_wd,
            8,
            8,
            284,
            lineHeight,
            -1,
            -1,
            -1,
            -1,
            nullptr,
            nullptr,
            nullptr,
            BUTTON_DRAG_HANDLE);
    }

    char temp[2];
    temp[1] = '\0';

    char* pch = string;
    while (*pch != '\0') {
        int characterWidth = fontGetCharacterWidth(*pch);
        if (*pch == '\n' || _currx + characterWidth > 291) {
            _currx = 9;
            _curry += lineHeight;
        }

        while (160 - _curry < lineHeight) {
            Window* window = windowGetWindow(_wd);
            unsigned char* windowBuffer = window->buffer;
            blitBufferToBuffer(windowBuffer + lineHeight * 300 + 300 * 26 + 9,
                282,
                134 - lineHeight - 1,
                300,
                windowBuffer + 300 * 26 + 9,
                300);
            _curry -= lineHeight;
            windowFill(_wd, 9, _curry, 282, lineHeight, 0x100 | 1);
        }

        if (*pch != '\n') {
            temp[0] = *pch;
            windowDrawText(_wd, temp, 0, _currx, _curry, 0x2000000 | 0x100 | 4);
            _currx += characterWidth + fontGetLetterSpacing();
        }

        pch++;
    }

    windowRefresh(_wd);

    // CE: Restore font.
    fontSetCurrent(oldFont);

    return 0;
}

// 0x4DC65C
void _win_debug_delete(int btn, int keyCode)
{
    windowDestroy(_wd);
    _wd = -1;
}

// 0x4DC674
int _win_register_menu_bar(int win, int x, int y, int width, int height, int foregroundColor, int backgroundColor)
{
    Window* window = windowGetWindow(win);

    if (!gWindowSystemInitialized) {
        return -1;
    }

    if (window == nullptr) {
        return -1;
    }

    if (window->menuBar != nullptr) {
        return -1;
    }

    int right = x + width;
    if (right > window->width) {
        return -1;
    }

    int bottom = y + height;
    if (bottom > window->height) {
        return -1;
    }

    MenuBar* menuBar = window->menuBar = (MenuBar*)internal_malloc(sizeof(MenuBar));
    if (menuBar == nullptr) {
        return -1;
    }

    menuBar->win = win;
    menuBar->rect.left = x;
    menuBar->rect.top = y;
    menuBar->rect.right = right - 1;
    menuBar->rect.bottom = bottom - 1;
    menuBar->pulldownsLength = 0;
    menuBar->foregroundColor = foregroundColor;
    menuBar->backgroundColor = backgroundColor;

    windowFill(win, x, y, width, height, backgroundColor);
    windowDrawRect(win, x, y, right - 1, bottom - 1, foregroundColor);

    return 0;
}

// 0x4DC768
int _win_register_menu_pulldown(int win, int x, char* title, int keyCode, int itemsLength, char** items, int foregroundColor, int backgroundColor)
{
    Window* window = windowGetWindow(win);

    if (!gWindowSystemInitialized) {
        return -1;
    }

    if (window == nullptr) {
        return -1;
    }

    MenuBar* menuBar = window->menuBar;
    if (menuBar == nullptr) {
        return -1;
    }

    if (window->menuBar->pulldownsLength == 15) {
        return -1;
    }

    int titleX = menuBar->rect.left + x;
    int titleY = (menuBar->rect.top + menuBar->rect.bottom - fontGetLineHeight()) / 2;
    int btn = buttonCreate(win,
        titleX,
        titleY,
        fontGetStringWidth(title),
        fontGetLineHeight(),
        -1,
        -1,
        keyCode,
        -1,
        nullptr,
        nullptr,
        nullptr,
        0);
    if (btn == -1) {
        return -1;
    }

    windowDrawText(win, title, 0, titleX, titleY, window->menuBar->foregroundColor | 0x2000000);

    MenuPulldown* pulldown = &(window->menuBar->pulldowns[window->menuBar->pulldownsLength]);
    pulldown->rect.left = titleX;
    pulldown->rect.top = titleY;
    pulldown->rect.right = fontGetStringWidth(title) + titleX - 1;
    pulldown->rect.bottom = fontGetLineHeight() + titleY - 1;
    pulldown->keyCode = keyCode;
    pulldown->itemsLength = itemsLength;
    pulldown->items = items;
    pulldown->foregroundColor = foregroundColor;
    pulldown->backgroundColor = backgroundColor;

    window->menuBar->pulldownsLength++;

    return 0;
}

// 0x4DC8D0
void _win_delete_menu_bar(int win)
{
    Window* window = windowGetWindow(win);

    if (!gWindowSystemInitialized) {
        return;
    }

    if (window == nullptr) {
        return;
    }

    if (window->menuBar == nullptr) {
        return;
    }

    windowFill(win,
        window->menuBar->rect.left,
        window->menuBar->rect.top,
        rectGetWidth(&(window->menuBar->rect)),
        rectGetHeight(&(window->menuBar->rect)),
        window->color);

    internal_free(window->menuBar);
    window->menuBar = nullptr;
}

// 0x4DC9F0
int _find_first_letter(int ch, char** stringList, int stringListLength)
{
    if (ch >= 'A' && ch <= 'Z') {
        ch += ' ';
    }

    for (int index = 0; index < stringListLength; index++) {
        char* string = stringList[index];
        if (string[0] == ch || string[0] == ch - ' ') {
            return index;
        }
    }

    return -1;
}

// 0x4DCA30
int _win_width_needed(char** fileNameList, int fileNameListLength)
{
    int maxWidth = 0;

    for (int index = 0; index < fileNameListLength; index++) {
        int width = fontGetStringWidth(fileNameList[index]);
        if (width > maxWidth) {
            maxWidth = width;
        }
    }

    return maxWidth;
}

// 0x4DCA5C
int _win_input_str(int win, char* dest, int maxLength, int x, int y, int textColor, int backgroundColor)
{
    Window* window = windowGetWindow(win);
    unsigned char* buffer = window->buffer + window->width * y + x;

    size_t cursorPos = strlen(dest);
    dest[cursorPos] = '_';
    dest[cursorPos + 1] = '\0';

    int lineHeight = fontGetLineHeight();
    int stringWidth = fontGetStringWidth(dest);
    bufferFill(buffer, stringWidth, lineHeight, window->width, backgroundColor);
    fontDrawText(buffer, dest, stringWidth, window->width, textColor);

    Rect dirtyRect;
    dirtyRect.left = window->rect.left + x;
    dirtyRect.top = window->rect.top + y;
    dirtyRect.right = dirtyRect.left + stringWidth;
    dirtyRect.bottom = dirtyRect.top + lineHeight;
    _GNW_win_refresh(window, &dirtyRect, nullptr);

    // NOTE: This loop is slightly different compared to other input handling
    // loops. Cursor position is managed inside an incrementing loop. Cursor is
    // decremented in the loop body when key is not handled.
    bool isFirstKey = true;
    for (; cursorPos <= maxLength; cursorPos++) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        if (keyCode != -1) {
            if (keyCode == KEY_ESCAPE) {
                dest[cursorPos] = '\0';
                return -1;
            }

            if (keyCode == KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    stringWidth = fontGetStringWidth(dest);

                    if (isFirstKey) {
                        bufferFill(buffer, stringWidth, lineHeight, window->width, backgroundColor);

                        dirtyRect.left = window->rect.left + x;
                        dirtyRect.top = window->rect.top + y;
                        dirtyRect.right = dirtyRect.left + stringWidth;
                        dirtyRect.bottom = dirtyRect.top + lineHeight;
                        _GNW_win_refresh(window, &dirtyRect, nullptr);

                        dest[0] = '_';
                        dest[1] = '\0';
                        cursorPos = 1;
                    } else {
                        dest[cursorPos] = ' ';
                        dest[cursorPos - 1] = '_';
                    }

                    bufferFill(buffer, stringWidth, lineHeight, window->width, backgroundColor);
                    fontDrawText(buffer, dest, stringWidth, window->width, textColor);

                    dirtyRect.left = window->rect.left + x;
                    dirtyRect.top = window->rect.top + y;
                    dirtyRect.right = dirtyRect.left + stringWidth;
                    dirtyRect.bottom = dirtyRect.top + lineHeight;
                    _GNW_win_refresh(window, &dirtyRect, nullptr);

                    dest[cursorPos] = '\0';
                    cursorPos -= 2;

                    isFirstKey = false;
                } else {
                    cursorPos--;
                }
            } else if (keyCode == KEY_RETURN) {
                break;
            } else {
                if (cursorPos == maxLength) {
                    cursorPos = maxLength - 1;
                } else {
                    if (keyCode > 0 && keyCode < 256) {
                        dest[cursorPos] = keyCode;
                        dest[cursorPos + 1] = '_';
                        dest[cursorPos + 2] = '\0';

                        int stringWidth = fontGetStringWidth(dest);
                        bufferFill(buffer, stringWidth, lineHeight, window->width, backgroundColor);
                        fontDrawText(buffer, dest, stringWidth, window->width, textColor);

                        dirtyRect.left = window->rect.left + x;
                        dirtyRect.top = window->rect.top + y;
                        dirtyRect.right = dirtyRect.left + stringWidth;
                        dirtyRect.bottom = dirtyRect.top + lineHeight;
                        _GNW_win_refresh(window, &dirtyRect, nullptr);

                        isFirstKey = false;
                    } else {
                        cursorPos--;
                    }
                }
            }
        } else {
            cursorPos--;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    dest[cursorPos] = '\0';

    return 0;
}

// 0x4DCD68
int win_get_num_i(int* value, int min, int max, bool clear, const char* title, int x, int y)
{
    if (!gWindowSystemInitialized) {
        return -1;
    }

    if (max < min) {
        return -1;
    }

    if (*value < min) {
        *value = min;
    } else if (*value > max) {
        *value = max;
    }

    int original = *value;
    int max_chars_wcursor = _calc_max_field_chars_wcursor(min, max);
    if (max_chars_wcursor == -1) {
        return -1;
    }

    int v2 = fontGetMonospacedCharacterWidth() * max_chars_wcursor;

    int width = fontGetStringWidth(title);
    if (width < v2) {
        width = v2;
    }

    width += 16;
    if (width < 160) {
        width = 160;
    }

    int height = 5 * fontGetLineHeight() + 16;
    int v3 = (width - v2) / 2;
    int v4 = fontGetLineHeight();
    int v5 = fontGetLineHeight() + 2;

    int win = windowCreate(x, y, width, height, 0x100, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    windowDrawBorder(win);
    windowFill(win, v3, v4 + 14, v2, v5, 0x100 | 1);
    windowDrawText(win, title, width - 16, 8, 8, 0x100 | 5);

    bufferDrawRectShadowed(windowGetBuffer(win),
        width,
        v3 - 2,
        v4 + 12,
        v3 + v2 + 1,
        v4 + 14 + v5 - 1,
        _colorTable[_GNW_wcolor[2]],
        _colorTable[_GNW_wcolor[1]]);

    _win_register_text_button(win,
        width / 2 - 72,
        height - fontGetLineHeight() - 14,
        -1,
        -1,
        -1,
        KEY_RETURN,
        "Done",
        0);

    _win_register_text_button(win,
        width / 2 + 16,
        height - fontGetLineHeight() - 14,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        "Cancel",
        0);

    char* hint = (char*)internal_malloc(80);
    if (hint == nullptr) {
        return -1;
    }

    sprintf(hint, "Please enter a number between %d and %d.", min, max);
    windowRefresh(win);

    int rc;
    while (1) {
        rc = get_num_i(win, value, max_chars_wcursor, clear, min < 0, v3, v4 + 14);
        if (*value >= min && *value <= max) {
            break;
        }

        if (rc == -1) {
            break;
        }

        _win_msg(hint, x - 70, y + 100, 0x100 | 6);
        *value = original;
    }

    internal_free(hint);
    windowDestroy(win);

    return rc;
}

// 0x4DBD04
int process_pull_down(int win, Rect* rect, char** items, int itemsLength, int foregroundColor, int backgroundColor, MenuBar* menuBar, int pulldownIndex)
{
    if (menuBar != nullptr) {
        unsigned char* parentWindowBuffer = windowGetWindow(menuBar->win)->buffer;
        MenuPulldown* pulldown = &(menuBar->pulldowns[pulldownIndex]);

        int x = pulldown->rect.left;
        int y = pulldown->rect.top;
        int width = pulldown->rect.right - x + 1;
        int height = pulldown->rect.bottom - y + 1;

        int color1 = menuBar->foregroundColor;
        if ((color1 & 0xFF00) != 0) {
            int colorIndex = (color1 & 0xFF) - 1;
            color1 = (color1 & ~0xFFFF) | _colorTable[_GNW_wcolor[colorIndex]];
        }

        int color2 = menuBar->backgroundColor;
        if ((color2 & 0xFF00) != 0) {
            int colorIndex = (color2 & 0xFF) - 1;
            color2 = (color2 & ~0xFFFF) | _colorTable[_GNW_wcolor[colorIndex]];
        }

        _swap_color_buf(parentWindowBuffer + width * y + x,
            width,
            height,
            windowGetWidth(menuBar->win),
            color1,
            color2);
        windowRefreshRect(menuBar->win, &(pulldown->rect));
    }

    unsigned char* windowBuffer = windowGetWindow(win)->buffer;
    int width = rectGetWidth(rect);
    int height = rectGetHeight(rect);

    int focusedIndex = -1;
    int rc;
    int mx1;
    int my1;
    int mx2;
    int my2;
    int input;

    mouseGetPosition(&mx1, &my1);

    while (1) {
        sharedFpsLimiter.mark();

        input = inputGetInput();
        if (input != -1) {
            break;
        }

        mouseGetPosition(&mx2, &my2);

        if (mx2 < mx1 - 4
            || mx2 > mx1 + 4
            || my2 < my1 - 4
            || my2 > my1 + 4) {
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    while (1) {
        sharedFpsLimiter.mark();

        mouseGetPosition(&mx2, &my2);

        if (input == -2) {
            if (menuBar != nullptr) {
                if (_mouse_click_in(menuBar->rect.left, menuBar->rect.top, menuBar->rect.right, menuBar->rect.bottom)) {
                    int index;
                    for (index = 0; index < menuBar->pulldownsLength; index++) {
                        MenuPulldown* pulldown = &(menuBar->pulldowns[index]);
                        if (_mouse_click_in(pulldown->rect.left, pulldown->rect.top, pulldown->rect.right, pulldown->rect.bottom)) {
                            break;
                        }
                    }

                    if (index < menuBar->pulldownsLength && index != pulldownIndex) {
                        rc = -2 - index;
                        break;
                    }
                }
            }
        }

        if ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_UP) != 0
            || ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0
                && (mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) == 0)) {
            if (_mouse_click_in(rect->left, rect->top + 8, rect->right, rect->bottom - 9)) {
                rc = focusedIndex;
            } else {
                rc = -1;
            }
            break;
        }

        bool done = false;
        switch (input) {
        case KEY_ESCAPE:
            rc = -1;
            done = true;
            break;
        case KEY_RETURN:
            rc = focusedIndex;
            done = true;
            break;
        case KEY_ARROW_LEFT:
            if (menuBar != nullptr && pulldownIndex > 0) {
                rc = -2 - (pulldownIndex - 1);
                done = true;
            }
            break;
        case KEY_ARROW_RIGHT:
            if (menuBar != nullptr && pulldownIndex < menuBar->pulldownsLength - 1) {
                rc = -2 - (pulldownIndex + 1);
                done = true;
            }
            break;
        case KEY_ARROW_UP:
            while (focusedIndex > 0) {
                focusedIndex--;

                if (items[focusedIndex][0] != '\0') {
                    break;
                }
            }
            input = -3;
            break;
        case KEY_ARROW_DOWN:
            while (focusedIndex < itemsLength - 1) {
                focusedIndex++;

                if (items[focusedIndex][0] != '\0') {
                    break;
                }
            }
            input = -3;
            break;
        default:
            if (mx2 != mx1 || my2 != my1) {
                if (_mouse_click_in(rect->left, rect->top + 8, rect->right, rect->bottom - 9)) {
                    input = (my2 - rect->top - 8) / fontGetLineHeight();
                    if (input != -1) {
                        focusedIndex = items[input][0] != '\0' ? input : -1;
                        input = -3;
                    }
                }

                mx1 = mx2;
                my1 = my2;
            }
            break;
        }

        if (done) {
            break;
        }

        if (input == -3) {
            windowFill(win, 2, 8, width - 4, height - 16, backgroundColor);
            _win_text(win, items, itemsLength, width - 4, 2, 8, foregroundColor);

            if (focusedIndex != -1) {
                _lighten_buf(windowBuffer + (focusedIndex * fontGetLineHeight() + 8) * width + 2,
                    width - 4,
                    fontGetLineHeight(),
                    width);
            }

            windowRefresh(win);
        }

        input = inputGetInput();

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (menuBar != nullptr) {
        unsigned char* parentWindowBuffer = windowGetWindow(menuBar->win)->buffer;
        MenuPulldown* pulldown = &(menuBar->pulldowns[pulldownIndex]);

        int x = pulldown->rect.left;
        int y = pulldown->rect.top;
        int width = pulldown->rect.right - x + 1;
        int height = pulldown->rect.bottom - y + 1;

        int color1 = menuBar->foregroundColor;
        if ((color1 & 0xFF00) != 0) {
            int colorIndex = (color1 & 0xFF) - 1;
            color1 = (color1 & ~0xFFFF) | _colorTable[_GNW_wcolor[colorIndex]];
        }

        int color2 = menuBar->backgroundColor;
        if ((color2 & 0xFF00) != 0) {
            int colorIndex = (color2 & 0xFF) - 1;
            color2 = (color2 & ~0xFFFF) | _colorTable[_GNW_wcolor[colorIndex]];
        }

        _swap_color_buf(parentWindowBuffer + width * y + x,
            width,
            height,
            windowGetWidth(menuBar->win),
            color1,
            color2);
        windowRefreshRect(menuBar->win, &(pulldown->rect));

        renderPresent();
    }

    windowDestroy(win);

    return rc;
}

// 0x4DC930
int _GNW_process_menu(MenuBar* menuBar, int pulldownIndex)
{
    if (_curr_menu != nullptr) {
        return -1;
    }

    _curr_menu = menuBar;

    int keyCode;
    Rect rect;
    do {
        MenuPulldown* pulldown = &(menuBar->pulldowns[pulldownIndex]);
        int win = _create_pull_down(pulldown->items,
            pulldown->itemsLength,
            pulldown->rect.left,
            menuBar->rect.bottom + 1,
            pulldown->foregroundColor,
            pulldown->backgroundColor,
            &rect);
        if (win == -1) {
            _curr_menu = nullptr;
            return -1;
        }

        keyCode = process_pull_down(win, &rect, pulldown->items, pulldown->itemsLength, pulldown->foregroundColor, pulldown->backgroundColor, menuBar, pulldownIndex);
        if (keyCode < -1) {
            pulldownIndex = -2 - keyCode;
        }
    } while (keyCode < -1);

    if (keyCode != -1) {
        inputEventQueueReset();
        enqueueInputEvent(keyCode);
        keyCode = menuBar->pulldowns[pulldownIndex].keyCode;
    }

    _curr_menu = nullptr;

    return keyCode;
}

// Calculates max length of string needed to represent `value` or `value2`.
//
// 0x4DD03C
size_t _calc_max_field_chars_wcursor(int value1, int value2)
{
    char* str = (char*)internal_malloc(17);
    if (str == nullptr) {
        return -1;
    }

    snprintf(str, 17, "%d", value1);
    size_t len1 = strlen(str);

    snprintf(str, 17, "%d", value2);
    size_t len2 = strlen(str);

    internal_free(str);

    return std::max(len1, len2) + 1;
}

// 0x4DD0AC
int get_num_i(int win, int* value, int max_chars_wcursor, bool clear, bool allow_negative, int x, int y)
{
    bool first_press = false;

    Window* window = windowGetWindow(win);
    if (window == nullptr) {
        return -1;
    }

    int original = *value;

    int width = max_chars_wcursor * fontGetMonospacedCharacterWidth();
    int height = fontGetLineHeight();

    char* string = (char*)internal_malloc(max_chars_wcursor + 1);

    if (clear) {
        string[0] = '\0';
    } else {
        snprintf(string,
            max_chars_wcursor + 1,
            "%d",
            *value);
    }

    int cursorPos = strlen(string);
    string[cursorPos] = '_';
    string[cursorPos + 1] = '\0';

    windowDrawText(win, string, width, x, y, 0x100 | 4);

    Rect rect;
    rect.left = x;
    rect.top = y;
    rect.right = x + width;
    rect.bottom = y + height;
    windowRefreshRect(win, &rect);

    bool done = false;
    while (cursorPos <= max_chars_wcursor && !done) {
        sharedFpsLimiter.mark();

        int input = inputGetInput();
        if (input == KEY_RETURN) {
            done = true;
        } else if (input == KEY_BACKSPACE) {
            if (cursorPos > 0) {
                int stringWidth = fontGetStringWidth(string);
                if (first_press) {
                    string[0] = '_';
                    string[1] = '\0';
                    cursorPos = 1;
                    first_press = false;
                } else {
                    string[cursorPos - 1] = '_';
                    string[cursorPos] = '\0';
                    cursorPos--;
                }

                windowFill(win, x, y, stringWidth, height, 0x100 | 1);
                windowDrawText(win, string, width, x, y, 0x100 | 4);
                windowRefreshRect(win, &rect);
            }
        } else if (input == KEY_ESCAPE) {
            *value = original;
            internal_free(string);

            return -1;
        } else if (input == KEY_ARROW_LEFT) {
            if (cursorPos > 0) {
                int stringWidth = fontGetStringWidth(string);
                string[cursorPos - 1] = '_';
                string[cursorPos] = '\0';
                windowFill(win, x, y, stringWidth, height, 0x100 | 1);
                windowDrawText(win, string, width, x, y, 0x100 | 4);
                windowRefreshRect(win, &rect);

                first_press = false;
                cursorPos--;
            }
        } else {
            if (cursorPos != max_chars_wcursor - 1) {
                if ((input == '-' && allow_negative)
                    || (input >= '0' && input <= '9')) {
                    string[cursorPos] = input;
                    string[cursorPos + 1] = '_';
                    string[cursorPos + 2] = '\0';

                    int stringWidth = fontGetStringWidth(string);
                    windowFill(win, x, y, stringWidth, height, 0x100 | 1);
                    windowDrawText(win, string, width, x, y, 0x100 | 4);
                    windowRefreshRect(win, &rect);

                    first_press = false;
                    cursorPos++;
                }
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    *value = atoi(string);
    internal_free(string);

    return 0;
}

// 0x4DD3EC
void _GNW_intr_init()
{
    int spacing;
    int top;
    int index;

    tm_persistence = 3000;
    tm_add = 0;
    tm_kill = -1;
    scr_center_x = _scr_size.right / 2;

    if (_scr_size.bottom >= 479) {
        tm_text_y = 16;
        tm_text_x = 16;
    } else {
        tm_text_y = 10;
        tm_text_x = 10;
    }

    tm_h = 2 * tm_text_y + fontGetLineHeight();

    spacing = _scr_size.bottom / 8;
    top = _scr_size.bottom / 4;

    for (index = 0; index < kTimedMsgs; index++) {
        tm_location[index].y = spacing * index + top;
        tm_location[index].taken = false;
    }
}

// 0x4DD4A4
void _GNW_intr_exit()
{
    tickersRemove(tm_watch_msgs);
    while (tm_kill != -1) {
        tm_kill_msg();
    }
}

// 0x4DD4C8
int win_timed_msg(const char* msg, int color)
{
    if (!gWindowSystemInitialized) {
        return -1;
    }

    if (tm_add == tm_kill) {
        return -1;
    }

    int width = fontGetStringWidth(msg) + 2 * tm_text_x;
    int x = scr_center_x - width / 2;
    int height = tm_h;
    int y;

    int index;
    for (index = 0; index < kTimedMsgs; index++) {
        if (!tm_location[index].taken) {
            tm_location[index].taken = true;
            y = tm_location[index].y;
            break;
        }
    }

    if (index == kTimedMsgs) {
        return -1;
    }

    int win = windowCreate(x, y, width, height, 0x100 | 1, WINDOW_MOVE_ON_TOP);
    windowDrawBorder(win);
    windowDrawText(win, msg, 0, tm_text_x, tm_text_y, color);

    int btn = buttonCreate(win,
        0,
        0,
        width,
        height,
        -1,
        -1,
        -1,
        -1,
        nullptr,
        nullptr,
        nullptr,
        0);
    buttonSetMouseCallbacks(btn, nullptr, nullptr, nullptr, tm_click_response);

    windowRefresh(win);

    tm_queue[tm_add].id = win;
    tm_queue[tm_add].created = getTicks();
    tm_queue[tm_add].location = index;

    tm_add++;
    if (tm_add == kTimedMsgs) {
        tm_add = 0;
    } else if (tm_kill == -1) {
        tm_kill = 0;
        tickersAdd(tm_watch_msgs);
    }

    return 0;
}

// 0x4DD66C
void tm_watch_msgs()
{
    if (tm_watch_active) {
        return;
    }

    tm_watch_active = true;
    while (tm_kill != -1) {
        if (getTicksSince(tm_queue[tm_kill].created) < tm_persistence) {
            break;
        }

        tm_kill_msg();
    }
    tm_watch_active = false;
}

// 0x4DD6C0
void tm_kill_msg()
{
    if (tm_kill != -1) {
        windowDestroy(tm_queue[tm_kill].id);
        tm_location[tm_queue[tm_kill].location].taken = false;

        tm_kill++;
        if (tm_kill == kTimedMsgs) {
            tm_kill = 0;
        }

        if (tm_kill == tm_add) {
            tm_add = 0;
            tm_kill = -1;
            tickersRemove(tm_watch_msgs);
        }
    }
}

// 0x4DD744
void tm_kill_out_of_order(int queue_index)
{
    int copy_from;
    int copy_to;

    if (tm_kill == -1) {
        return;
    }

    if (!tm_index_active(queue_index)) {
        return;
    }

    windowDestroy(tm_queue[queue_index].id);

    tm_location[tm_queue[queue_index].location].taken = false;

    copy_to = queue_index;
    while (copy_to != tm_kill) {
        copy_from = copy_to - 1;
        if (copy_from < 0) {
            copy_from = kTimedMsgs - 1;
        }

        tm_queue[copy_to] = tm_queue[copy_from];
        copy_to = copy_from;
    }

    tm_kill++;
    if (tm_kill == kTimedMsgs) {
        tm_kill = 0;
    }

    if (tm_add == tm_kill) {
        tm_add = 0;
        tm_kill = -1;
        tickersRemove(tm_watch_msgs);
    }
}

// 0x4DD82C
void tm_click_response(int btn, int keyCode)
{
    int win;
    int queue_index;

    if (tm_kill == -1) {
        return;
    }

    win = buttonGetWindowId(btn);
    queue_index = tm_kill;
    while (win != tm_queue[queue_index].id) {
        queue_index++;
        if (queue_index == kTimedMsgs) {
            queue_index = 0;
        }

        if (queue_index == tm_kill || !tm_index_active(queue_index)) {
            return;
        }
    }

    tm_kill_out_of_order(queue_index);
}

// 0x4DD870
bool tm_index_active(int queue_index)
{
    if (tm_kill == tm_add) {
        return true;
    }

    if (tm_kill < tm_add) {
        return queue_index >= tm_kill && queue_index < tm_add;
    }

    return queue_index < tm_add || queue_index >= tm_kill;
}

} // namespace fallout
