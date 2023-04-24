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

typedef struct STRUCT_6B2340 {
    int field_0;
    int field_4;
} STRUCT_6B2340;

typedef struct STRUCT_6B2370 {
    int field_0;
    // win
    int field_4;
    int field_8;
} STRUCT_6B2370;

// 0x51E414
static int _wd = -1;

// 0x51E418
static MenuBar* _curr_menu = NULL;

// 0x51E41C
static bool _tm_watch_active = false;

// 0x6B2340
static STRUCT_6B2340 _tm_location[5];

// 0x6B2368
static int _tm_text_x;

// 0x6B236C
static int _tm_h;

// 0x6B2370
static STRUCT_6B2370 _tm_queue[5];

// 0x6B23AC
static unsigned int _tm_persistence;

// 0x6B23B0
static int _scr_center_x;

// 0x6B23B4
static int _tm_text_y;

// 0x6B23B8
static int _tm_kill;

// 0x6B23BC
static int _tm_add;

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
        NULL,
        NULL,
        NULL,
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
            NULL,
            NULL,
            NULL,
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
        NULL,
        NULL,
        NULL,
        BUTTON_FLAG_0x10);

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
                    if (callback == NULL) {
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

                _GNW_win_refresh(window, windowRect, NULL);
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

                _GNW_win_refresh(window, &itemRect, NULL);
            }

            if (selectedItemIndex != -1) {
                itemRect.top = windowRect->top + listViewY + selectedItemIndex * fontGetLineHeight();
                itemRect.bottom = itemRect.top + fontGetLineHeight();

                _lighten_buf(listViewBuffer + windowWidth * selectedItemIndex * fontGetLineHeight(),
                    listViewWidth,
                    fontGetLineHeight(),
                    windowWidth);

                _GNW_win_refresh(window, &itemRect, NULL);
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

    _win_input_str(win,
        dest,
        length,
        16,
        fontGetLineHeight() + 16,
        _colorTable[_GNW_wcolor[3]],
        _colorTable[_GNW_wcolor[0]]);

    windowDestroy(win);

    return 0;
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
        int index = (color & 0xFF) - 1;
        textColor = _colorTable[_GNW_wcolor[index]];
        textColor |= color & ~0xFFFF;
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

    return process_pull_down(win, &rect, items, itemsLength, color, _colorTable[_GNW_wcolor[0]], NULL, -1);
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
        buttonSetMouseCallbacks(btn, NULL, NULL, NULL, _win_debug_delete);

        buttonCreate(_wd,
            8,
            8,
            284,
            lineHeight,
            -1,
            -1,
            -1,
            -1,
            NULL,
            NULL,
            NULL,
            BUTTON_FLAG_0x10);
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

    if (window == NULL) {
        return -1;
    }

    if (window->menuBar != NULL) {
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
    if (menuBar == NULL) {
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

    if (window == NULL) {
        return -1;
    }

    MenuBar* menuBar = window->menuBar;
    if (menuBar == NULL) {
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
        NULL,
        NULL,
        NULL,
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

    if (window == NULL) {
        return;
    }

    if (window->menuBar == NULL) {
        return;
    }

    windowFill(win,
        window->menuBar->rect.left,
        window->menuBar->rect.top,
        rectGetWidth(&(window->menuBar->rect)),
        rectGetHeight(&(window->menuBar->rect)),
        window->color);

    internal_free(window->menuBar);
    window->menuBar = NULL;
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
    _GNW_win_refresh(window, &dirtyRect, NULL);

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
                        _GNW_win_refresh(window, &dirtyRect, NULL);

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
                    _GNW_win_refresh(window, &dirtyRect, NULL);

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
                        _GNW_win_refresh(window, &dirtyRect, NULL);

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

// 0x4DBD04
int process_pull_down(int win, Rect* rect, char** items, int itemsLength, int foregroundColor, int backgroundColor, MenuBar* menuBar, int pulldownIndex)
{
    // TODO: Incomplete.
    return -1;
}

// 0x4DC930
int _GNW_process_menu(MenuBar* menuBar, int pulldownIndex)
{
    if (_curr_menu != NULL) {
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
            _curr_menu = NULL;
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

    _curr_menu = NULL;

    return keyCode;
}

// Calculates max length of string needed to represent `value` or `value2`.
//
// 0x4DD03C
size_t _calc_max_field_chars_wcursor(int value1, int value2)
{
    char* str = (char*)internal_malloc(17);
    if (str == NULL) {
        return -1;
    }

    snprintf(str, 17, "%d", value1);
    size_t len1 = strlen(str);

    snprintf(str, 17, "%d", value2);
    size_t len2 = strlen(str);

    internal_free(str);

    return std::max(len1, len2) + 1;
}

// 0x4DD3EC
void _GNW_intr_init()
{
    int v1, v2;
    int i;

    _tm_persistence = 3000;
    _tm_add = 0;
    _tm_kill = -1;
    _scr_center_x = _scr_size.right / 2;

    if (_scr_size.bottom >= 479) {
        _tm_text_y = 16;
        _tm_text_x = 16;
    } else {
        _tm_text_y = 10;
        _tm_text_x = 10;
    }

    _tm_h = 2 * _tm_text_y + fontGetLineHeight();

    v1 = _scr_size.bottom >> 3;
    v2 = _scr_size.bottom >> 2;

    for (i = 0; i < 5; i++) {
        _tm_location[i].field_4 = v1 * i + v2;
        _tm_location[i].field_0 = 0;
    }
}

// 0x4DD4A4
void _GNW_intr_exit()
{
    tickersRemove(_tm_watch_msgs);
    while (_tm_kill != -1) {
        _tm_kill_msg();
    }
}

// 0x4DD66C
void _tm_watch_msgs()
{
    if (_tm_watch_active) {
        return;
    }

    _tm_watch_active = 1;
    while (_tm_kill != -1) {
        if (getTicksSince(_tm_queue[_tm_kill].field_0) < _tm_persistence) {
            break;
        }

        _tm_kill_msg();
    }
    _tm_watch_active = 0;
}

// 0x4DD6C0
void _tm_kill_msg()
{
    int v0;

    v0 = _tm_kill;
    if (v0 != -1) {
        windowDestroy(_tm_queue[_tm_kill].field_4);
        _tm_location[_tm_queue[_tm_kill].field_8].field_0 = 0;

        if (v0 == 5) {
            v0 = 0;
        }

        if (v0 == _tm_add) {
            _tm_add = 0;
            _tm_kill = -1;
            tickersRemove(_tm_watch_msgs);
            v0 = _tm_kill;
        }
    }

    _tm_kill = v0;
}

// 0x4DD744
void _tm_kill_out_of_order(int queueIndex)
{
    int v7;
    int v6;

    if (_tm_kill == -1) {
        return;
    }

    if (!_tm_index_active(queueIndex)) {
        return;
    }

    windowDestroy(_tm_queue[queueIndex].field_4);

    _tm_location[_tm_queue[queueIndex].field_8].field_0 = 0;

    if (queueIndex != _tm_kill) {
        v6 = queueIndex;
        do {
            v7 = v6 - 1;
            if (v7 < 0) {
                v7 = 4;
            }

            memcpy(&(_tm_queue[v6]), &(_tm_queue[v7]), sizeof(STRUCT_6B2370));
            v6 = v7;
        } while (v7 != _tm_kill);
    }

    if (++_tm_kill == 5) {
        _tm_kill = 0;
    }

    if (_tm_add == _tm_kill) {
        _tm_add = 0;
        _tm_kill = -1;
        tickersRemove(_tm_watch_msgs);
    }
}

// 0x4DD82C
void _tm_click_response(int btn)
{
    int win;
    int queueIndex;

    if (_tm_kill == -1) {
        return;
    }

    win = buttonGetWindowId(btn);
    queueIndex = _tm_kill;
    while (win != _tm_queue[queueIndex].field_4) {
        queueIndex++;
        if (queueIndex == 5) {
            queueIndex = 0;
        }

        if (queueIndex == _tm_kill || !_tm_index_active(queueIndex))
            return;
    }

    _tm_kill_out_of_order(queueIndex);
}

// 0x4DD870
int _tm_index_active(int queueIndex)
{
    if (_tm_kill != _tm_add) {
        if (_tm_kill >= _tm_add) {
            if (queueIndex >= _tm_add && queueIndex < _tm_kill)
                return 0;
        } else if (queueIndex < _tm_kill || queueIndex >= _tm_add) {
            return 0;
        }
    }
    return 1;
}

} // namespace fallout
