#include "mouse.h"

#include "color.h"
#include "dinput.h"
#include "input.h"
#include "kb.h"
#include "memory.h"
#include "svga.h"
#include "touch.h"
#include "vcr.h"

namespace fallout {

static void mousePrepareDefaultCursor();
static void _mouse_anim();
static void _mouse_clip();

// The default mouse cursor buffer.
//
// Initially it contains color codes, which will be replaced at startup
// according to loaded palette.
//
// Available color codes:
// - 0: transparent
// - 1: white
// - 15: black
//
// 0x51E250
static unsigned char gMouseDefaultCursor[MOUSE_DEFAULT_CURSOR_SIZE] = {
    // clang-format off
    1,  1,  1,  1,  1,  1,  1, 0,
    1, 15, 15, 15, 15, 15,  1, 0,
    1, 15, 15, 15, 15,  1,  1, 0,
    1, 15, 15, 15, 15,  1,  1, 0,
    1, 15, 15, 15, 15, 15,  1, 1,
    1, 15,  1,  1, 15, 15, 15, 1,
    1,  1,  1,  1,  1, 15, 15, 1,
    0,  0,  0,  0,  1,  1,  1, 1,
    // clang-format on
};

// 0x51E290
static int _mouse_idling = 0;

// 0x51E294
static unsigned char* gMouseCursorData = nullptr;

// 0x51E298
static unsigned char* _mouse_shape = nullptr;

// 0x51E29C
static unsigned char* _mouse_fptr = nullptr;

// 0x51E2A0
double gMouseSensitivity = 1.0;

// 0x51E2AC
static int last_buttons = 0;

// 0x6AC790
static bool gCursorIsHidden;

// 0x6AC794
static int _raw_x;

// 0x6AC798
static int gMouseCursorHeight;

// 0x6AC79C
static int _raw_y;

// 0x6AC7A0
static int _raw_buttons;

// 0x6AC7A4
static int gMouseCursorY;

// 0x6AC7A8
static int gMouseCursorX;

// 0x6AC7AC
static int _mouse_disabled;

// 0x6AC7B0
static int gMouseEvent;

// 0x6AC7B4
static unsigned int _mouse_speed;

// 0x6AC7B8
static int _mouse_curr_frame;

// 0x6AC7BC
static bool gMouseInitialized;

// 0x6AC7C0
static int gMouseCursorPitch;

// 0x6AC7C4
static int gMouseCursorWidth;

// 0x6AC7C8
static int _mouse_num_frames;

// 0x6AC7CC
static int _mouse_hoty;

// 0x6AC7D0
static int _mouse_hotx;

// 0x6AC7D4
static unsigned int _mouse_idle_start_time;

// 0x6AC7D8
WindowDrawingProc2* _mouse_blit_trans;

// 0x6AC7DC
WINDOWDRAWINGPROC _mouse_blit;

// 0x6AC7E0
static char _mouse_trans;

static int gMouseWheelX = 0;
static int gMouseWheelY = 0;

// 0x4C9F40
int mouseInit()
{
    gMouseInitialized = false;
    _mouse_disabled = 0;

    gCursorIsHidden = true;

    mousePrepareDefaultCursor();

    if (mouseSetFrame(nullptr, 0, 0, 0, 0, 0, 0) == -1) {
        return -1;
    }

    if (!mouseDeviceAcquire()) {
        return -1;
    }

    gMouseInitialized = true;
    gMouseCursorX = _scr_size.right / 2;
    gMouseCursorY = _scr_size.bottom / 2;
    _raw_x = _scr_size.right / 2;
    _raw_y = _scr_size.bottom / 2;
    _mouse_idle_start_time = getTicks();

    return 0;
}

// 0x4C9FD8
void mouseFree()
{
    mouseDeviceUnacquire();

    if (gMouseCursorData != nullptr) {
        internal_free(gMouseCursorData);
        gMouseCursorData = nullptr;
    }

    if (_mouse_fptr != nullptr) {
        tickersRemove(_mouse_anim);
        _mouse_fptr = nullptr;
    }
}

// 0x4CA01C
static void mousePrepareDefaultCursor()
{
    for (int index = 0; index < 64; index++) {
        switch (gMouseDefaultCursor[index]) {
        case 0:
            gMouseDefaultCursor[index] = _colorTable[0];
            break;
        case 1:
            gMouseDefaultCursor[index] = _colorTable[8456];
            break;
        case 15:
            gMouseDefaultCursor[index] = _colorTable[32767];
            break;
        }
    }
}

// 0x4CA0AC
int mouseSetFrame(unsigned char* a1, int width, int height, int pitch, int a5, int a6, char a7)
{
    Rect rect;
    unsigned char* v9;
    int v11, v12;
    int v7, v8;

    v7 = a5;
    v8 = a6;
    v9 = a1;

    if (a1 == nullptr) {
        // NOTE: Original code looks tail recursion optimization.
        return mouseSetFrame(gMouseDefaultCursor, MOUSE_DEFAULT_CURSOR_WIDTH, MOUSE_DEFAULT_CURSOR_HEIGHT, MOUSE_DEFAULT_CURSOR_WIDTH, 1, 1, _colorTable[0]);
    }

    bool cursorWasHidden = gCursorIsHidden;
    if (!gCursorIsHidden && gMouseInitialized) {
        gCursorIsHidden = true;
        mouseGetRect(&rect);
        windowRefreshAll(&rect);
    }

    if (width != gMouseCursorWidth || height != gMouseCursorHeight) {
        unsigned char* buf = (unsigned char*)internal_malloc(width * height);
        if (buf == nullptr) {
            if (!cursorWasHidden) {
                mouseShowCursor();
            }
            return -1;
        }

        if (gMouseCursorData != nullptr) {
            internal_free(gMouseCursorData);
        }

        gMouseCursorData = buf;
    }

    gMouseCursorWidth = width;
    gMouseCursorHeight = height;
    gMouseCursorPitch = pitch;
    _mouse_shape = v9;
    _mouse_trans = a7;

    if (_mouse_fptr) {
        tickersRemove(_mouse_anim);
        _mouse_fptr = nullptr;
    }

    v11 = _mouse_hotx - v7;
    _mouse_hotx = v7;

    gMouseCursorX += v11;

    v12 = _mouse_hoty - v8;
    _mouse_hoty = v8;

    gMouseCursorY += v12;

    _mouse_clip();

    if (!cursorWasHidden) {
        mouseShowCursor();
    }

    _raw_x = gMouseCursorX;
    _raw_y = gMouseCursorY;

    return 0;
}

// NOTE: Looks like this code is not reachable.
//
// 0x4CA2D0
static void _mouse_anim()
{
    // 0x51E2A8
    static unsigned int ticker = 0;

    if (getTicksSince(ticker) >= _mouse_speed) {
        ticker = getTicks();

        if (++_mouse_curr_frame == _mouse_num_frames) {
            _mouse_curr_frame = 0;
        }

        _mouse_shape = gMouseCursorWidth * _mouse_curr_frame * gMouseCursorHeight + _mouse_fptr;

        if (!gCursorIsHidden) {
            mouseShowCursor();
        }
    }
}

// 0x4CA34C
void mouseShowCursor()
{
    int i;
    unsigned char* v2;
    int v7, v8;
    int v9, v10;
    int v4;
    unsigned char v6;
    int v3;

    v2 = gMouseCursorData;
    if (gMouseInitialized) {
        if (!_mouse_blit_trans || !gCursorIsHidden) {
            _win_get_mouse_buf(gMouseCursorData);
            v2 = gMouseCursorData;
            v3 = 0;

            for (i = 0; i < gMouseCursorHeight; i++) {
                for (v4 = 0; v4 < gMouseCursorWidth; v4++) {
                    v6 = _mouse_shape[i * gMouseCursorPitch + v4];
                    if (v6 != _mouse_trans) {
                        v2[v3] = v6;
                    }
                    v3++;
                }
            }
        }

        if (gMouseCursorX >= _scr_size.left) {
            if (gMouseCursorWidth + gMouseCursorX - 1 <= _scr_size.right) {
                v8 = gMouseCursorWidth;
                v7 = 0;
            } else {
                v7 = 0;
                v8 = _scr_size.right - gMouseCursorX + 1;
            }
        } else {
            v7 = _scr_size.left - gMouseCursorX;
            v8 = gMouseCursorWidth - (_scr_size.left - gMouseCursorX);
        }

        if (gMouseCursorY >= _scr_size.top) {
            if (gMouseCursorHeight + gMouseCursorY - 1 <= _scr_size.bottom) {
                v9 = 0;
                v10 = gMouseCursorHeight;
            } else {
                v9 = 0;
                v10 = _scr_size.bottom - gMouseCursorY + 1;
            }
        } else {
            v9 = _scr_size.top - gMouseCursorY;
            v10 = gMouseCursorHeight - (_scr_size.top - gMouseCursorY);
        }

        gMouseCursorData = v2;
        if (_mouse_blit_trans && gCursorIsHidden) {
            _mouse_blit_trans(_mouse_shape, gMouseCursorPitch, gMouseCursorHeight, v7, v9, v8, v10, v7 + gMouseCursorX, v9 + gMouseCursorY, _mouse_trans);
        } else {
            _mouse_blit(gMouseCursorData, gMouseCursorWidth, gMouseCursorHeight, v7, v9, v8, v10, v7 + gMouseCursorX, v9 + gMouseCursorY);
        }

        v2 = gMouseCursorData;
        gCursorIsHidden = false;
    }
    gMouseCursorData = v2;
}

// 0x4CA534
void mouseHideCursor()
{
    Rect rect;

    if (gMouseInitialized) {
        if (!gCursorIsHidden) {
            rect.left = gMouseCursorX;
            rect.top = gMouseCursorY;
            rect.right = gMouseCursorX + gMouseCursorWidth - 1;
            rect.bottom = gMouseCursorY + gMouseCursorHeight - 1;

            gCursorIsHidden = true;
            windowRefreshAll(&rect);
        }
    }
}

// 0x4CA59C
void _mouse_info()
{
    if (!gMouseInitialized) {
        return;
    }

    if (gCursorIsHidden) {
        return;
    }

    if (_mouse_disabled) {
        return;
    }

    Gesture gesture;
    if (touch_get_gesture(&gesture)) {
        static int prevx;
        static int prevy;

        switch (gesture.type) {
        case kTap:
            if (gesture.numberOfTouches == 1) {
                _mouse_set_position(gesture.x, gesture.y);
                _mouse_simulate_input(0, 0, MOUSE_STATE_LEFT_BUTTON_DOWN);
            } else if (gesture.numberOfTouches == 2) {
                _mouse_set_position(gesture.x, gesture.y);
                _mouse_simulate_input(0, 0, MOUSE_STATE_RIGHT_BUTTON_DOWN);
            }
            break;
        case kLongPress:
        case kPan:
            if (gesture.state == kBegan) {
                prevx = gesture.x;
                prevy = gesture.y;
                _mouse_set_position(gesture.x, gesture.y);
            }

            if (gesture.type == kLongPress) {
                if (gesture.numberOfTouches == 1) {
                    _mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, MOUSE_STATE_LEFT_BUTTON_DOWN);
                } else if (gesture.numberOfTouches == 2) {
                    _mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, MOUSE_STATE_RIGHT_BUTTON_DOWN);
                }
            } else if (gesture.type == kPan) {
                if (gesture.numberOfTouches == 1) {
                    _mouse_set_position(gesture.x, gesture.y);
                    // _mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, 0); TODO if people want cursor to scroll with their finger
                } else if (gesture.numberOfTouches == 2) {
                    gMouseWheelX = (prevx - gesture.x) / 2;
                    gMouseWheelY = (gesture.y - prevy) / 2;

                    if (gMouseWheelX != 0 || gMouseWheelY != 0) {
                        gMouseEvent |= MOUSE_EVENT_WHEEL;
                        _raw_buttons |= MOUSE_EVENT_WHEEL;
                    }
                }
            }

            prevx = gesture.x;
            prevy = gesture.y;
            break;
        }

        return;
    }

    int x;
    int y;
    int buttons = 0;

    MouseData mouseData;
    if (mouseDeviceGetData(&mouseData)) {
        x = mouseData.x;
        y = mouseData.y;

        if (mouseData.buttons[0] == 1) {
            buttons |= MOUSE_STATE_LEFT_BUTTON_DOWN;
        }

        if (mouseData.buttons[1] == 1) {
            buttons |= MOUSE_STATE_RIGHT_BUTTON_DOWN;
        }
    } else {
        x = 0;
        y = 0;
    }

    // Adjust for mouse senstivity.
    x = (int)(x * gMouseSensitivity);
    y = (int)(y * gMouseSensitivity);

    if (gVcrState == VCR_STATE_PLAYING) {
        if (((gVcrTerminateFlags & VCR_TERMINATE_ON_MOUSE_PRESS) != 0 && buttons != 0)
            || ((gVcrTerminateFlags & VCR_TERMINATE_ON_MOUSE_MOVE) != 0 && (x != 0 || y != 0))) {
            gVcrPlaybackCompletionReason = VCR_PLAYBACK_COMPLETION_REASON_TERMINATED;
            vcrStop();
            return;
        }
        x = 0;
        y = 0;
        buttons = last_buttons;
    }

    _mouse_simulate_input(x, y, buttons);

    // TODO: Move to `_mouse_simulate_input`.
    // TODO: Record wheel event in VCR.
    gMouseWheelX = mouseData.wheelX;
    gMouseWheelY = mouseData.wheelY;

    if (gMouseWheelX != 0 || gMouseWheelY != 0) {
        gMouseEvent |= MOUSE_EVENT_WHEEL;
        _raw_buttons |= MOUSE_EVENT_WHEEL;
    }
}

// 0x4CA698
void _mouse_simulate_input(int delta_x, int delta_y, int buttons)
{
    // 0x6AC7E4
    static unsigned int previousRightButtonTimestamp;

    // 0x6AC7E8
    static unsigned int previousLeftButtonTimestamp;

    // 0x6AC7EC
    static int previousEvent;

    if (!gMouseInitialized || gCursorIsHidden) {
        return;
    }

    if (delta_x || delta_y || buttons != last_buttons) {
        if (gVcrState == 0) {
            if (_vcr_buffer_index == VCR_BUFFER_CAPACITY - 1) {
                vcrDump();
            }

            VcrEntry* vcrEntry = &(_vcr_buffer[_vcr_buffer_index]);
            vcrEntry->type = VCR_ENTRY_TYPE_MOUSE_EVENT;
            vcrEntry->time = _vcr_time;
            vcrEntry->counter = _vcr_counter;
            vcrEntry->mouseEvent.dx = delta_x;
            vcrEntry->mouseEvent.dy = delta_y;
            vcrEntry->mouseEvent.buttons = buttons;

            _vcr_buffer_index++;
        }
    } else {
        if (last_buttons == 0) {
            if (!_mouse_idling) {
                _mouse_idle_start_time = getTicks();
                _mouse_idling = 1;
            }

            last_buttons = 0;
            _raw_buttons = 0;
            gMouseEvent = 0;

            return;
        }
    }

    _mouse_idling = 0;
    last_buttons = buttons;
    previousEvent = gMouseEvent;
    gMouseEvent = 0;

    if ((previousEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT) != 0) {
        if ((buttons & 0x01) != 0) {
            gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_REPEAT;

            if (getTicksSince(previousLeftButtonTimestamp) > BUTTON_REPEAT_TIME) {
                gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
                previousLeftButtonTimestamp = getTicks();
            }
        } else {
            gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_UP;
        }
    } else {
        if ((buttons & 0x01) != 0) {
            gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
            previousLeftButtonTimestamp = getTicks();
        }
    }

    if ((previousEvent & MOUSE_EVENT_RIGHT_BUTTON_DOWN_REPEAT) != 0) {
        if ((buttons & 0x02) != 0) {
            gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_REPEAT;
            if (getTicksSince(previousRightButtonTimestamp) > BUTTON_REPEAT_TIME) {
                gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
                previousRightButtonTimestamp = getTicks();
            }
        } else {
            gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_UP;
        }
    } else {
        if (buttons & 0x02) {
            gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
            previousRightButtonTimestamp = getTicks();
        }
    }

    _raw_buttons = gMouseEvent;

    if (delta_x != 0 || delta_y != 0) {
        Rect mouseRect;
        mouseRect.left = gMouseCursorX;
        mouseRect.top = gMouseCursorY;
        mouseRect.right = gMouseCursorWidth + gMouseCursorX - 1;
        mouseRect.bottom = gMouseCursorHeight + gMouseCursorY - 1;

        gMouseCursorX += delta_x;
        gMouseCursorY += delta_y;
        _mouse_clip();

        windowRefreshAll(&mouseRect);

        mouseShowCursor();

        _raw_x = gMouseCursorX;
        _raw_y = gMouseCursorY;
    }
}

// 0x4CA8C8
bool _mouse_in(int left, int top, int right, int bottom)
{
    if (!gMouseInitialized) {
        return false;
    }

    return gMouseCursorHeight + gMouseCursorY > top
        && right >= gMouseCursorX
        && gMouseCursorWidth + gMouseCursorX > left
        && bottom >= gMouseCursorY;
}

// 0x4CA934
bool _mouse_click_in(int left, int top, int right, int bottom)
{
    if (!gMouseInitialized) {
        return false;
    }

    return _mouse_hoty + gMouseCursorY >= top
        && _mouse_hotx + gMouseCursorX <= right
        && _mouse_hotx + gMouseCursorX >= left
        && _mouse_hoty + gMouseCursorY <= bottom;
}

// 0x4CA9A0
void mouseGetRect(Rect* rect)
{
    rect->left = gMouseCursorX;
    rect->top = gMouseCursorY;
    rect->right = gMouseCursorWidth + gMouseCursorX - 1;
    rect->bottom = gMouseCursorHeight + gMouseCursorY - 1;
}

// 0x4CA9DC
void mouseGetPosition(int* xPtr, int* yPtr)
{
    *xPtr = _mouse_hotx + gMouseCursorX;
    *yPtr = _mouse_hoty + gMouseCursorY;
}

// 0x4CAA04
void _mouse_set_position(int a1, int a2)
{
    gMouseCursorX = a1 - _mouse_hotx;
    gMouseCursorY = a2 - _mouse_hoty;
    _raw_y = a2 - _mouse_hoty;
    _raw_x = a1 - _mouse_hotx;
    _mouse_clip();
}

// 0x4CAA38
static void _mouse_clip()
{
    if (_mouse_hotx + gMouseCursorX < _scr_size.left) {
        gMouseCursorX = _scr_size.left - _mouse_hotx;
    } else if (_mouse_hotx + gMouseCursorX > _scr_size.right) {
        gMouseCursorX = _scr_size.right - _mouse_hotx;
    }

    if (_mouse_hoty + gMouseCursorY < _scr_size.top) {
        gMouseCursorY = _scr_size.top - _mouse_hoty;
    } else if (_mouse_hoty + gMouseCursorY > _scr_size.bottom) {
        gMouseCursorY = _scr_size.bottom - _mouse_hoty;
    }
}

// 0x4CAAA0
int mouseGetEvent()
{
    return gMouseEvent;
}

// 0x4CAAA8
bool cursorIsHidden()
{
    return gCursorIsHidden;
}

// 0x4CAB5C
void _mouse_get_raw_state(int* out_x, int* out_y, int* out_buttons)
{
    MouseData mouseData;
    if (!mouseDeviceGetData(&mouseData)) {
        mouseData.x = 0;
        mouseData.y = 0;
        mouseData.buttons[0] = (gMouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0;
        mouseData.buttons[1] = (gMouseEvent & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0;
    }

    _raw_buttons = 0;
    _raw_x += mouseData.x;
    _raw_y += mouseData.y;

    if (mouseData.buttons[0] != 0) {
        _raw_buttons |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
    }

    if (mouseData.buttons[1] != 0) {
        _raw_buttons |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
    }

    *out_x = _raw_x;
    *out_y = _raw_y;
    *out_buttons = _raw_buttons;
}

// 0x4CAC3C
void mouseSetSensitivity(double value)
{
    if (value >= 1.0 && value <= 2.5) {
        gMouseSensitivity = value;
    }
}

void mouseGetPositionInWindow(int win, int* x, int* y)
{
    mouseGetPosition(x, y);

    Window* window = windowGetWindow(win);
    if (window != nullptr) {
        *x -= window->rect.left;
        *y -= window->rect.top;
    }
}

bool mouseHitTestInWindow(int win, int left, int top, int right, int bottom)
{
    Window* window = windowGetWindow(win);
    if (window != nullptr) {
        left += window->rect.left;
        top += window->rect.top;
        right += window->rect.left;
        bottom += window->rect.top;
    }

    return _mouse_click_in(left, top, right, bottom);
}

void mouseGetWheel(int* x, int* y)
{
    *x = gMouseWheelX;
    *y = gMouseWheelY;
}

void convertMouseWheelToArrowKey(int* keyCodePtr)
{
    if (*keyCodePtr == -1) {
        if ((mouseGetEvent() & MOUSE_EVENT_WHEEL) != 0) {
            int wheelX;
            int wheelY;
            mouseGetWheel(&wheelX, &wheelY);

            if (wheelY > 0) {
                *keyCodePtr = KEY_ARROW_UP;
            } else if (wheelY < 0) {
                *keyCodePtr = KEY_ARROW_DOWN;
            }
        }
    }
}

int mouse_get_last_buttons()
{
    return last_buttons;
}

} // namespace fallout
