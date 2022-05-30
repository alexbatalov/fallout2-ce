#include "core.h"

#include "config.h"
#include "color.h"
#include "dinput.h"
#include "draw.h"
#include "interface.h"
#include "memory.h"
#include "mmx.h"
#include "text_font.h"
#include "win32.h"
#include "window_manager.h"
#include "window_manager_private.h"

#include <limits.h>
#include <string.h>
#include <SDL.h>
#if _WIN32
#include <SDL_syswm.h>
#endif

// NOT USED.
void (*_idle_func)() = NULL;

// NOT USED.
void (*_focus_func)(int) = NULL;

// 0x51E23C
int gKeyboardKeyRepeatRate = 80;

// 0x51E240
int gKeyboardKeyRepeatDelay = 500;

// The default mouse cursor buffer.
//
// Initially it contains color codes, which will be replaced at startup
// according to loaded palette.
//
// Available color codes:
// - 0: transparent
// - 1: white
// - 15:  black
//
// 0x51E250
unsigned char gMouseDefaultCursor[MOUSE_DEFAULT_CURSOR_SIZE] = {
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
int _mouse_idling = 0;

// 0x51E294
unsigned char* gMouseCursorData = NULL;

// 0x51E298
unsigned char* _mouse_shape = NULL;

// 0x51E29C
unsigned char* _mouse_fptr = NULL;

// 0x51E2A0
double gMouseSensitivity = 1.0;

// 0x51E2A8
unsigned int _ticker_ = 0;

// 0x51E2AC
int gMouseButtonsState = 0;

// NOTE: This value is never set, so it's impossible to understand it's
// meaning.
//
// 0x51E2C4
void (*_update_palette_func)() = NULL;

// 0x51E2C8
bool gMmxEnabled = true;

// 0x51E2CC
bool gMmxProbed = false;

// 0x51E2D0
unsigned char _kb_installed = 0;

// 0x51E2D4
bool gKeyboardDisabled = false;

// 0x51E2D8
bool gKeyboardNumpadDisabled = false;

// 0x51E2DC
bool gKeyboardNumlockDisabled = false;

// 0x51E2E0
int gKeyboardEventQueueWriteIndex = 0;

// 0x51E2E4
int gKeyboardEventQueueReadIndex = 0;

// 0x51E2E8
short word_51E2E8 = 0;

// 0x51E2EA
int gModifierKeysState = 0;

// TODO: It's _kb_next_ascii_English_US (not implemented yet).
//
// 0x51E2EC
int (*_kb_scan_to_ascii)() = keyboardDequeueLogicalKeyCode;

// 0x51E2F0
STRUCT_51E2F0* _vcr_buffer = NULL;

// number of entries in _vcr_buffer
// 0x51E2F4
int _vcr_buffer_index = 0;

// 0x51E2F8
int _vcr_state = 2;

// 0x51E2FC
int _vcr_time = 0;

// 0x51E300
int _vcr_counter = 0;

// 0x51E304
int _vcr_terminate_flags = 0;

// 0x51E308
int _vcr_terminated_condition = 0;

// 0x51E30C
int _vcr_start_time = 0;

// 0x51E310
int _vcr_registered_atexit = 0;

// 0x51E314
File* _vcr_file = NULL;

// 0x51E318
int _vcr_buffer_end = 0;

// A map of SDL_SCANCODE_* constants normalized for QWERTY keyboard.
//
// 0x6ABC70
int gNormalizedQwertyKeys[SDL_NUM_SCANCODES];

// Ring buffer of input events.
//
// Looks like this buffer does not support overwriting of values. Once the
// buffer is full it will not overwrite values until they are dequeued.
//
// 0x6ABD70
InputEvent gInputEventQueue[40];

// 0x6ABF50
STRUCT_6ABF50 _GNW95_key_time_stamps[SDL_NUM_SCANCODES];

// 0x6AC750
int _input_mx;

// 0x6AC754
int _input_my;

// 0x6AC75C
bool gPaused;

// 0x6AC760
int gScreenshotKeyCode;

// 0x6AC764
int _using_msec_timer;

// 0x6AC768
int gPauseKeyCode;

// 0x6AC76C
ScreenshotHandler* gScreenshotHandler;

// 0x6AC770
int gInputEventQueueReadIndex;

// 0x6AC774
unsigned char* gScreenshotBuffer;

// 0x6AC778
PauseHandler* gPauseHandler;

// 0x6AC77C
int gInputEventQueueWriteIndex;

// 0x6AC780
bool gRunLoopDisabled;

// 0x6AC784
TickerListNode* gTickerListHead;

// 0x6AC788
unsigned int gTickerLastTimestamp;

// 0x6AC790
bool gCursorIsHidden;

// x (1)
// 0x6AC794
int _raw_x;

// 0x6AC798
int gMouseCursorHeight;

// y (1)
// 0x6AC79C
int _raw_y;

// mouse event (1)
// 0x6AC7A0
int _raw_buttons;

// 0x6AC7A4
int gMouseCursorY;

// 0x6AC7A8
int gMouseCursorX;

// 0x6AC7AC
int _mouse_disabled;

// 0x6AC7B0
int gMouseEvent;

// 0x6AC7B4
unsigned int _mouse_speed;

// 0x6AC7B8
int _mouse_curr_frame;

// 0x6AC7BC
bool gMouseInitialized;

// 0x6AC7C0
int gMouseCursorPitch;

// 0x6AC7C4
int gMouseCursorWidth;

// 0x6AC7C8
int _mouse_num_frames;

// 0x6AC7CC
int _mouse_hoty;

// 0x6AC7D0
int _mouse_hotx;

// 0x6AC7D4
unsigned int _mouse_idle_start_time;

// 0x6AC7D8
WindowDrawingProc2* _mouse_blit_trans;

// 0x6AC7DC
WINDOWDRAWINGPROC _mouse_blit;

// 0x6AC7E0
unsigned char _mouse_trans;

// 0x6AC7E4
int gMouseRightButtonDownTimestamp;

// 0x6AC7E8
int gMouseLeftButtonDownTimestamp;

// 0x6AC7EC
int gMousePreviousEvent;

// 0x6AC7F0
unsigned short gSixteenBppPalette[256];

// screen rect
Rect _scr_size;

// 0x6ACA00
int gGreenMask;

// 0x6ACA04
int gRedMask;

// 0x6ACA08
int gBlueMask;

// 0x6ACA0C
int gBlueShift;

// 0x6ACA10
int gRedShift;

// 0x6ACA14
int gGreenShift;

// 0x6ACA18
void (*_scr_blit)(unsigned char* src, int src_pitch, int a3, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y) = _GNW95_ShowRect;

// 0x6ACA1C
void (*_zero_mem)() = NULL;

// 0x6ACA20
bool gMmxSupported;

// FIXME: This buffer was supposed to be used as temporary place to store
// current palette while switching video modes (changing resolution). However
// the original game does not have UI to change video mode. Even if it did this
// buffer it too small to hold the entire palette, which require 256 * 3 bytes.
//
// 0x6ACA24
unsigned char gLastVideoModePalette[268];

// Ring buffer of keyboard events.
//
// 0x6ACB30
KeyboardEvent gKeyboardEventsQueue[64];

// A map of logical key configurations for physical scan codes [SDL_SCANCODE_*].
//
// 0x6ACC30
LogicalKeyEntry gLogicalKeyEntries[SDL_NUM_SCANCODES];

// A state of physical keys [SDL_SCANCODE_*] currently pressed.
//
// 0 - key is not pressed.
// 1 - key pressed.
//
// 0x6AD830
unsigned char gPressedPhysicalKeys[SDL_NUM_SCANCODES];

// 0x6AD930
unsigned int _kb_idle_start_time;

// 0x6AD934
KeyboardEvent gLastKeyboardEvent;

// 0x6AD938
int gKeyboardLayout;

// The number of keys currently pressed.
//
// 0x6AD93C
unsigned char gPressedPhysicalKeysCount;

SDL_Window* gSdlWindow = NULL;
SDL_Surface* gSdlWindowSurface = NULL;
SDL_Surface* gSdlSurface = NULL;

// 0x4C8A70
int coreInit(int a1)
{
    if (!directInputInit()) {
        return -1;
    }

    if (keyboardInit() == -1) {
        return -1;
    }

    if (mouseInit() == -1) {
        return -1;
    }

    if (_GNW95_input_init() == -1) {
        return -1;
    }

    buildNormalizedQwertyKeys();
    _GNW95_clear_time_stamps();

    _using_msec_timer = a1;
    gInputEventQueueWriteIndex = 0;
    gInputEventQueueReadIndex = -1;
    _input_mx = -1;
    _input_my = -1;
    gRunLoopDisabled = 0;
    gPaused = false;
    gPauseKeyCode = KEY_ALT_P;
    gPauseHandler = pauseHandlerDefaultImpl;
    gScreenshotHandler = screenshotHandlerDefaultImpl;
    gTickerListHead = NULL;
    gScreenshotKeyCode = KEY_ALT_C;

    return 0;
}

// 0x4C8B40
void coreExit()
{
    _GNW95_input_init();
    mouseFree();
    keyboardFree();
    directInputFree();

    TickerListNode* curr = gTickerListHead;
    while (curr != NULL) {
        TickerListNode* next = curr->next;
        internal_free(curr);
        curr = next;
    }
}

// 0x4C8B78
int _get_input()
{
    int v3;

    _GNW95_process_message();

    if (!gProgramIsActive) {
        _GNW95_lost_focus();
    }

    _process_bk();

    v3 = dequeueInputEvent();
    if (v3 == -1 && mouseGetEvent() & 0x33) {
        mouseGetPosition(&_input_mx, &_input_my);
        return -2;
    } else {
        return _GNW_check_menu_bars(v3);
    }

    return -1;
}

// 0x4C8BDC
void _process_bk()
{
    int v1;

    tickersExecute();

    if (_vcr_update() != 3) {
        _mouse_info();
    }

    v1 = _win_check_all_buttons();
    if (v1 != -1) {
        enqueueInputEvent(v1);
        return;
    }

    v1 = _kb_getch();
    if (v1 != -1) {
        enqueueInputEvent(v1);
        return;
    }
}

// 0x4C8C04
void enqueueInputEvent(int a1)
{
    if (a1 == -1) {
        return;
    }

    if (a1 == gPauseKeyCode) {
        pauseGame();
        return;
    }

    if (a1 == gScreenshotKeyCode) {
        takeScreenshot();
        return;
    }

    if (gInputEventQueueWriteIndex == gInputEventQueueReadIndex) {
        return;
    }

    InputEvent* inputEvent = &(gInputEventQueue[gInputEventQueueWriteIndex]);
    inputEvent->logicalKey = a1;

    mouseGetPosition(&(inputEvent->mouseX), &(inputEvent->mouseY));

    gInputEventQueueWriteIndex++;

    if (gInputEventQueueWriteIndex == 40) {
        gInputEventQueueWriteIndex = 0;
        return;
    }

    if (gInputEventQueueReadIndex == -1) {
        gInputEventQueueReadIndex = 0;
    }
}

// 0x4C8C9C
int dequeueInputEvent()
{
    if (gInputEventQueueReadIndex == -1) {
        return -1;
    }

    InputEvent* inputEvent = &(gInputEventQueue[gInputEventQueueReadIndex]);
    int eventCode = inputEvent->logicalKey;
    _input_mx = inputEvent->mouseX;
    _input_my = inputEvent->mouseY;

    gInputEventQueueReadIndex++;

    if (gInputEventQueueReadIndex == 40) {
        gInputEventQueueReadIndex = 0;
    }

    if (gInputEventQueueReadIndex == gInputEventQueueWriteIndex) {
        gInputEventQueueReadIndex = -1;
        gInputEventQueueWriteIndex = 0;
    }

    return eventCode;
}

// 0x4C8D04
void inputEventQueueReset()
{
    gInputEventQueueReadIndex = -1;
    gInputEventQueueWriteIndex = 0;
}

// 0x4C8D1C
void tickersExecute()
{
    if (gPaused) {
        return;
    }

    if (gRunLoopDisabled) {
        return;
    }

    gTickerLastTimestamp = SDL_GetTicks();

    TickerListNode* curr = gTickerListHead;
    TickerListNode** currPtr = &(gTickerListHead);

    while (curr != NULL) {
        TickerListNode* next = curr->next;
        if (curr->flags & 1) {
            *currPtr = next;

            internal_free(curr);
        } else {
            curr->proc();
            currPtr = &(curr->next);
        }
        curr = next;
    }
}

// 0x4C8D74
void tickersAdd(TickerProc* proc)
{
    TickerListNode* curr = gTickerListHead;
    while (curr != NULL) {
        if (curr->proc == proc) {
            if ((curr->flags & 0x01) != 0) {
                curr->flags &= ~0x01;
                return;
            }
        }
        curr = curr->next;
    }

    curr = (TickerListNode*)internal_malloc(sizeof(*curr));
    curr->flags = 0;
    curr->proc = proc;
    curr->next = gTickerListHead;
    gTickerListHead = curr;
}

// 0x4C8DC4
void tickersRemove(TickerProc* proc)
{
    TickerListNode* curr = gTickerListHead;
    while (curr != NULL) {
        if (curr->proc == proc) {
            curr->flags |= 0x01;
            return;
        }
        curr = curr->next;
    }
}

// 0x4C8DE4
void tickersEnable()
{
    gRunLoopDisabled = false;
}

// 0x4C8DF0
void tickersDisable()
{
    gRunLoopDisabled = true;
}

// 0x4C8DFC
void pauseGame()
{
    if (!gPaused) {
        gPaused = true;

        int win = gPauseHandler();

        while (_get_input() != KEY_ESCAPE) {
        }

        gPaused = false;
        windowDestroy(win);
    }
}

// 0x4C8E38
int pauseHandlerDefaultImpl()
{
    int len;
    int v1;
    int v2;
    int win;
    unsigned char* buf;
    int v6;
    int v7;

    len = fontGetStringWidth("Paused") + 32;
    v1 = fontGetLineHeight();
    v2 = 3 * v1 + 16;

    win = windowCreate((_scr_size.right - _scr_size.left + 1 - len) / 2, (_scr_size.bottom - _scr_size.top + 1 - v2) / 2, len, v2, 256, 20);
    if (win == -1) {
        return -1;
    }

    windowDrawBorder(win);
    buf = windowGetBuffer(win);
    fontDrawText(buf + 8 * len + 16, "Paused", len, len, _colorTable[31744]);

    v6 = v2 - 8 - v1;
    v7 = fontGetStringWidth("Done");
    // TODO: Incomplete.
    // _win_register_text_button(win, (len - v7 - 16) / 2, v6 - 6, -1, -1, -1, 27, "Done", 0);

    windowRefresh(win);

    return win;
}

// 0x4C8F34
void pauseHandlerConfigure(int keyCode, PauseHandler* handler)
{
    gPauseKeyCode = keyCode;

    if (handler == NULL) {
        handler = pauseHandlerDefaultImpl;
    }

    gPauseHandler = handler;
}

// 0x4C8F4C
void takeScreenshot()
{
    int width = _scr_size.right - _scr_size.left + 1;
    int height = _scr_size.bottom - _scr_size.top + 1;
    gScreenshotBuffer = (unsigned char*)internal_malloc(width * height);
    if (gScreenshotBuffer == NULL) {
        return;
    }

    WINDOWDRAWINGPROC v0 = _scr_blit;
    _scr_blit = screenshotBlitter;

    WINDOWDRAWINGPROC v2 = _mouse_blit;
    _mouse_blit = screenshotBlitter;

    WindowDrawingProc2* v1 = _mouse_blit_trans;
    _mouse_blit_trans = NULL;

    windowRefreshAll(&_scr_size);

    _mouse_blit_trans = v1;
    _mouse_blit = v2;
    _scr_blit = v0;

    unsigned char* palette = _getSystemPalette();
    gScreenshotHandler(width, height, gScreenshotBuffer, palette);
    internal_free(gScreenshotBuffer);
}

// 0x4C8FF0
void screenshotBlitter(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int width, int height, int destX, int destY)
{
    int destWidth = _scr_size.right - _scr_size.left + 1;
    blitBufferToBuffer(src + srcPitch * srcY + srcX, width, height, srcPitch, gScreenshotBuffer + destWidth * destY + destX, destWidth);
}

// 0x4C9048
int screenshotHandlerDefaultImpl(int width, int height, unsigned char* data, unsigned char* palette)
{
    char fileName[16];
    FILE* stream;
    int index;
    unsigned int intValue;
    unsigned short shortValue;

    for (index = 0; index < 100000; index++) {
        sprintf(fileName, "scr%.5d.bmp", index);
        
        stream = fopen(fileName, "rb");
        if (stream == NULL) {
            break;
        }

        fclose(stream);
    }

    if (index == 100000) {
        return -1;
    }

    stream = fopen(fileName, "wb");
    if (stream == NULL) {
        return -1;
    }

    // bfType
    shortValue = 0x4D42;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // bfSize
    // 14 - sizeof(BITMAPFILEHEADER)
    // 40 - sizeof(BITMAPINFOHEADER)
    // 1024 - sizeof(RGBQUAD) * 256
    intValue = width * height + 14 + 40 + 1024;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // bfReserved1
    shortValue = 0;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // bfReserved2
    shortValue = 0;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // bfOffBits
    intValue = 14 + 40 + 1024;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biSize
    intValue = 40;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biWidth
    intValue = width;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biHeight
    intValue = height;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biPlanes
    shortValue = 1;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // biBitCount
    shortValue = 8;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // biCompression
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);
    
    // biSizeImage
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biXPelsPerMeter
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biYPelsPerMeter
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biClrUsed
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biClrImportant
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    for (int index = 0; index < 256; index++) {
        unsigned char rgbReserved = 0;
        unsigned char rgbRed = palette[index * 3] << 2;
        unsigned char rgbGreen = palette[index * 3 + 1] << 2;
        unsigned char rgbBlue = palette[index * 3 + 2] << 2;

        fwrite(&rgbBlue, sizeof(rgbBlue), 1, stream);
        fwrite(&rgbGreen, sizeof(rgbGreen), 1, stream);
        fwrite(&rgbRed, sizeof(rgbRed), 1, stream);
        fwrite(&rgbReserved, sizeof(rgbReserved), 1, stream);
    }

    for (int y = height - 1; y >= 0; y--) {
        unsigned char* dataPtr = data + y * width;
        fwrite(dataPtr, 1, width, stream);
    }

    fflush(stream);
    fclose(stream);

    return 0;
}

// 0x4C9358
void screenshotHandlerConfigure(int keyCode, ScreenshotHandler* handler)
{
    gScreenshotKeyCode = keyCode;

    if (handler == NULL) {
        handler = screenshotHandlerDefaultImpl;
    }

    gScreenshotHandler = handler;
}

// 0x4C9370
unsigned int _get_time()
{
    return SDL_GetTicks();
}

// 0x4C937C
void coreDelayProcessingEvents(unsigned int delay)
{
    // NOTE: Uninline.
    unsigned int start = _get_time();
    unsigned int end = _get_time();

    // NOTE: Uninline.
    unsigned int diff = getTicksBetween(end, start);
    while (diff < delay) {
        _process_bk();

        end = _get_time();

        // NOTE: Uninline.
        diff = getTicksBetween(end, start);
    }
}

// 0x4C93B8
void coreDelay(unsigned int ms)
{
    unsigned int start = SDL_GetTicks();
    unsigned int diff;
    do {
        // NOTE: Uninline
        diff = getTicksSince(start);
    } while (diff < ms);
}

// 0x4C93E0
unsigned int getTicksSince(unsigned int start)
{
    unsigned int end = SDL_GetTicks();

    // NOTE: Uninline.
    return getTicksBetween(end, start);
}

// 0x4C9400
unsigned int getTicksBetween(unsigned int end, unsigned int start)
{
    if (start > end) {
        return INT_MAX;
    } else {
        return end - start;
    }
}

// 0x4C9410
unsigned int _get_bk_time()
{
    return gTickerLastTimestamp;
}

// 0x4C9490
void buildNormalizedQwertyKeys()
{
    int* keys = gNormalizedQwertyKeys;
    int k;

    keys[SDL_SCANCODE_ESCAPE] = SDL_SCANCODE_ESCAPE;
    keys[SDL_SCANCODE_1] = SDL_SCANCODE_1;
    keys[SDL_SCANCODE_2] = SDL_SCANCODE_2;
    keys[SDL_SCANCODE_3] = SDL_SCANCODE_3;
    keys[SDL_SCANCODE_4] = SDL_SCANCODE_4;
    keys[SDL_SCANCODE_5] = SDL_SCANCODE_5;
    keys[SDL_SCANCODE_6] = SDL_SCANCODE_6;
    keys[SDL_SCANCODE_7] = SDL_SCANCODE_7;
    keys[SDL_SCANCODE_8] = SDL_SCANCODE_8;
    keys[SDL_SCANCODE_9] = SDL_SCANCODE_9;
    keys[SDL_SCANCODE_0] = SDL_SCANCODE_0;

    switch (gKeyboardLayout) {
    case 0:
        k = SDL_SCANCODE_MINUS;
        break;
    case 1:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }
    keys[SDL_SCANCODE_MINUS] = k;

    switch (gKeyboardLayout) {
    case 1:
        k = SDL_SCANCODE_0;
        break;
    default:
        k = SDL_SCANCODE_EQUALS;
        break;
    }
    keys[SDL_SCANCODE_EQUALS] = k;

    keys[SDL_SCANCODE_BACKSPACE] = SDL_SCANCODE_BACKSPACE;
    keys[SDL_SCANCODE_TAB] = SDL_SCANCODE_TAB;

    switch (gKeyboardLayout) {
    case 1:
        k = SDL_SCANCODE_A;
        break;
    default:
        k = SDL_SCANCODE_Q;
        break;
    }
    keys[SDL_SCANCODE_Q] = k;

    switch (gKeyboardLayout) {
    case 1:
        k = SDL_SCANCODE_Z;
        break;
    default:
        k = SDL_SCANCODE_W;
        break;
    }
    keys[SDL_SCANCODE_W] = k;

    keys[SDL_SCANCODE_E] = SDL_SCANCODE_E;
    keys[SDL_SCANCODE_R] = SDL_SCANCODE_R;
    keys[SDL_SCANCODE_T] = SDL_SCANCODE_T;

    switch (gKeyboardLayout) {
    case 0:
    case 1:
    case 3:
    case 4:
        k = SDL_SCANCODE_Y;
        break;
    default:
        k = SDL_SCANCODE_Z;
        break;
    }
    keys[SDL_SCANCODE_Y] = k;

    keys[SDL_SCANCODE_U] = SDL_SCANCODE_U;
    keys[SDL_SCANCODE_I] = SDL_SCANCODE_I;
    keys[SDL_SCANCODE_O] = SDL_SCANCODE_O;
    keys[SDL_SCANCODE_P] = SDL_SCANCODE_P;

    switch (gKeyboardLayout) {
    case 0:
    case 3:
    case 4:
        k = SDL_SCANCODE_LEFTBRACKET;
        break;
    case 1:
        k = SDL_SCANCODE_5;
        break;
    default:
        k = SDL_SCANCODE_8;
        break;
    }
    keys[SDL_SCANCODE_LEFTBRACKET] = k;

    switch (gKeyboardLayout) {
    case 0:
    case 3:
    case 4:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    case 1:
        k = SDL_SCANCODE_MINUS;
        break;
    default:
        k = SDL_SCANCODE_9;
        break;
    }
    keys[SDL_SCANCODE_RIGHTBRACKET] = k;

    keys[SDL_SCANCODE_RETURN] = SDL_SCANCODE_RETURN;
    keys[SDL_SCANCODE_LCTRL] = SDL_SCANCODE_LCTRL;

    switch (gKeyboardLayout) {
    case 1:
        k = SDL_SCANCODE_Q;
        break;
    default:
        k = SDL_SCANCODE_A;
        break;
    }
    keys[SDL_SCANCODE_A] = k;

    keys[SDL_SCANCODE_S] = SDL_SCANCODE_S;
    keys[SDL_SCANCODE_D] = SDL_SCANCODE_D;
    keys[SDL_SCANCODE_F] = SDL_SCANCODE_F;
    keys[SDL_SCANCODE_G] = SDL_SCANCODE_G;
    keys[SDL_SCANCODE_H] = SDL_SCANCODE_H;
    keys[SDL_SCANCODE_J] = SDL_SCANCODE_J;
    keys[SDL_SCANCODE_K] = SDL_SCANCODE_K;
    keys[SDL_SCANCODE_L] = SDL_SCANCODE_L;

    switch (gKeyboardLayout) {
    case 0:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }
    keys[SDL_SCANCODE_SEMICOLON] = k;

    switch (gKeyboardLayout) {
    case 0:
        k = SDL_SCANCODE_APOSTROPHE;
        break;
    case 1:
        k = SDL_SCANCODE_4;
        break;
    default:
        k = SDL_SCANCODE_MINUS;
        break;
    }
    keys[SDL_SCANCODE_APOSTROPHE] = k;

    switch (gKeyboardLayout) {
    case 0:
        k = SDL_SCANCODE_GRAVE;
        break;
    case 1:
        k = SDL_SCANCODE_2;
        break;
    case 3:
    case 4:
        k = 0;
        break;
    default:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    }
    keys[SDL_SCANCODE_GRAVE] = k;

    keys[SDL_SCANCODE_LSHIFT] = SDL_SCANCODE_LSHIFT;

    switch (gKeyboardLayout) {
    case 0:
        k = SDL_SCANCODE_BACKSLASH;
        break;
    case 1:
        k = SDL_SCANCODE_8;
        break;
    case 3:
    case 4:
        k = SDL_SCANCODE_GRAVE;
        break;
    default:
        k = SDL_SCANCODE_Y;
        break;
    }
    keys[SDL_SCANCODE_BACKSLASH] = k;

    switch (gKeyboardLayout) {
    case 0:
    case 3:
    case 4:
        k = SDL_SCANCODE_Z;
        break;
    case 1:
        k = SDL_SCANCODE_W;
        break;
    default:
        k = SDL_SCANCODE_Y;
        break;
    }
    keys[SDL_SCANCODE_Z] = k;

    keys[SDL_SCANCODE_X] = SDL_SCANCODE_X;
    keys[SDL_SCANCODE_C] = SDL_SCANCODE_C;
    keys[SDL_SCANCODE_V] = SDL_SCANCODE_V;
    keys[SDL_SCANCODE_B] = SDL_SCANCODE_B;
    keys[SDL_SCANCODE_N] = SDL_SCANCODE_N;

    switch (gKeyboardLayout) {
    case 1:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_M;
        break;
    }
    keys[SDL_SCANCODE_M] = k;

    switch (gKeyboardLayout) {
    case 1:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }
    keys[SDL_SCANCODE_COMMA] = k;

    switch (gKeyboardLayout) {
    case 1:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }
    keys[SDL_SCANCODE_PERIOD] = k;

    switch (gKeyboardLayout) {
    case 0:
        k = SDL_SCANCODE_SLASH;
        break;
    case 1:
        k = SDL_SCANCODE_PERIOD;
        break;
    default:
        k = SDL_SCANCODE_7;
        break;
    }
    keys[SDL_SCANCODE_SLASH] = k;

    keys[SDL_SCANCODE_RSHIFT] = SDL_SCANCODE_RSHIFT;
    keys[SDL_SCANCODE_KP_MULTIPLY] = SDL_SCANCODE_KP_MULTIPLY;
    keys[SDL_SCANCODE_SPACE] = SDL_SCANCODE_SPACE;
    keys[SDL_SCANCODE_LALT] = SDL_SCANCODE_LALT;
    keys[SDL_SCANCODE_CAPSLOCK] = SDL_SCANCODE_CAPSLOCK;
    keys[SDL_SCANCODE_F1] = SDL_SCANCODE_F1;
    keys[SDL_SCANCODE_F2] = SDL_SCANCODE_F2;
    keys[SDL_SCANCODE_F3] = SDL_SCANCODE_F3;
    keys[SDL_SCANCODE_F4] = SDL_SCANCODE_F4;
    keys[SDL_SCANCODE_F5] = SDL_SCANCODE_F5;
    keys[SDL_SCANCODE_F6] = SDL_SCANCODE_F6;
    keys[SDL_SCANCODE_F7] = SDL_SCANCODE_F7;
    keys[SDL_SCANCODE_F8] = SDL_SCANCODE_F8;
    keys[SDL_SCANCODE_F9] = SDL_SCANCODE_F9;
    keys[SDL_SCANCODE_F10] = SDL_SCANCODE_F10;
    keys[SDL_SCANCODE_NUMLOCKCLEAR] = SDL_SCANCODE_NUMLOCKCLEAR;
    keys[SDL_SCANCODE_SCROLLLOCK] = SDL_SCANCODE_SCROLLLOCK;
    keys[SDL_SCANCODE_KP_7] = SDL_SCANCODE_KP_7;
    keys[SDL_SCANCODE_KP_9] = SDL_SCANCODE_KP_9;
    keys[SDL_SCANCODE_KP_8] = SDL_SCANCODE_KP_8;
    keys[SDL_SCANCODE_KP_MINUS] = SDL_SCANCODE_KP_MINUS;
    keys[SDL_SCANCODE_KP_4] = SDL_SCANCODE_KP_4;
    keys[SDL_SCANCODE_KP_5] = SDL_SCANCODE_KP_5;
    keys[SDL_SCANCODE_KP_6] = SDL_SCANCODE_KP_6;
    keys[SDL_SCANCODE_KP_PLUS] = SDL_SCANCODE_KP_PLUS;
    keys[SDL_SCANCODE_KP_1] = SDL_SCANCODE_KP_1;
    keys[SDL_SCANCODE_KP_2] = SDL_SCANCODE_KP_2;
    keys[SDL_SCANCODE_KP_3] = SDL_SCANCODE_KP_3;
    keys[SDL_SCANCODE_KP_0] = SDL_SCANCODE_KP_0;
    keys[SDL_SCANCODE_KP_DECIMAL] = SDL_SCANCODE_KP_DECIMAL;
    keys[SDL_SCANCODE_F11] = SDL_SCANCODE_F11;
    keys[SDL_SCANCODE_F12] = SDL_SCANCODE_F12;
    keys[SDL_SCANCODE_F13] = -1;
    keys[SDL_SCANCODE_F14] = -1;
    keys[SDL_SCANCODE_F15] = -1;
    //keys[DIK_KANA] = -1;
    //keys[DIK_CONVERT] = -1;
    //keys[DIK_NOCONVERT] = -1;
    //keys[DIK_YEN] = -1;
    keys[SDL_SCANCODE_KP_EQUALS] = -1;
    //keys[DIK_PREVTRACK] = -1;
    //keys[DIK_AT] = -1;
    //keys[DIK_COLON] = -1;
    //keys[DIK_UNDERLINE] = -1;
    //keys[DIK_KANJI] = -1;
    keys[SDL_SCANCODE_STOP] = -1;
    //keys[DIK_AX] = -1;
    //keys[DIK_UNLABELED] = -1;
    keys[SDL_SCANCODE_KP_ENTER] = SDL_SCANCODE_KP_ENTER;
    keys[SDL_SCANCODE_RCTRL] = SDL_SCANCODE_RCTRL;
    keys[SDL_SCANCODE_KP_COMMA] = -1;
    keys[SDL_SCANCODE_KP_DIVIDE] = SDL_SCANCODE_KP_DIVIDE;
    //keys[DIK_SYSRQ] = 84;
    keys[SDL_SCANCODE_RALT] = SDL_SCANCODE_RALT;
    keys[SDL_SCANCODE_HOME] = SDL_SCANCODE_HOME;
    keys[SDL_SCANCODE_UP] = SDL_SCANCODE_UP;
    keys[SDL_SCANCODE_PRIOR] = SDL_SCANCODE_PRIOR;
    keys[SDL_SCANCODE_LEFT] = SDL_SCANCODE_LEFT;
    keys[SDL_SCANCODE_RIGHT] = SDL_SCANCODE_RIGHT;
    keys[SDL_SCANCODE_END] = SDL_SCANCODE_END;
    keys[SDL_SCANCODE_DOWN] = SDL_SCANCODE_DOWN;
    keys[SDL_SCANCODE_PAGEDOWN] = SDL_SCANCODE_PAGEDOWN;
    keys[SDL_SCANCODE_INSERT] = SDL_SCANCODE_INSERT;
    keys[SDL_SCANCODE_DELETE] = SDL_SCANCODE_DELETE;
    keys[SDL_SCANCODE_LGUI] = -1;
    keys[SDL_SCANCODE_RGUI] = -1;
    keys[SDL_SCANCODE_APPLICATION] = -1;
}

// 0x4C9C20
int _GNW95_input_init()
{
    return 0;
}

// 0x4C9CF0
void _GNW95_process_message()
{
    // We need to process event loop even if program is not active or keyboard
    // is disabled, because if we ignore it, we'll never be able to reactivate
    // it again.

    KeyboardData keyboardData;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            // The data is accumulated in SDL itself and will be processed
            // in `_mouse_info`.
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (!keyboardIsDisabled()) {
                keyboardData.key = e.key.keysym.scancode;
                keyboardData.down = (e.key.state & SDL_PRESSED) != 0;
                _GNW95_process_key(&keyboardData);
            }
            break;
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_EXPOSED:
                windowRefreshAll(&_scr_size);
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                // TODO: Recreate gSdlSurface in case size really changed (i.e.
                // not alt-tabbing in fullscreen mode).
                gSdlWindowSurface = SDL_GetWindowSurface(gSdlWindow);
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                gProgramIsActive = true;
                windowRefreshAll(&_scr_size);
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                gProgramIsActive = false;
                break;
            }
            break;
        case SDL_QUIT:
            exit(EXIT_SUCCESS);
            break;
        }
    }

    if (gProgramIsActive && !keyboardIsDisabled()) {
        // NOTE: Uninline
        int tick = _get_time();

        for (int key = 0; key < SDL_NUM_SCANCODES; key++) {
            STRUCT_6ABF50* ptr = &(_GNW95_key_time_stamps[key]);
            if (ptr->tick != -1) {
                int elapsedTime = ptr->tick > tick ? INT_MAX : tick - ptr->tick;
                int delay = ptr->repeatCount == 0 ? gKeyboardKeyRepeatDelay : gKeyboardKeyRepeatRate;
                if (elapsedTime > delay) {
                    keyboardData.key = key;
                    keyboardData.down = 1;
                    _GNW95_process_key(&keyboardData);

                    ptr->tick = tick;
                    ptr->repeatCount++;
                }
            }
        }
    }
}

// 0x4C9DF0
void _GNW95_clear_time_stamps()
{
    for (int index = 0; index < SDL_NUM_SCANCODES; index++) {
        _GNW95_key_time_stamps[index].tick = -1;
        _GNW95_key_time_stamps[index].repeatCount = 0;
    }
}

// 0x4C9E14
void _GNW95_process_key(KeyboardData* data)
{
    data->key = gNormalizedQwertyKeys[data->key];

    if (_vcr_state == 1) {
        if (_vcr_terminate_flags & 1) {
            _vcr_terminated_condition = 2;
            _vcr_stop();
        }
    } else {
        STRUCT_6ABF50* ptr = &(_GNW95_key_time_stamps[data->key]);
        if (data->down == 1) {
            ptr->tick = _get_time();
            ptr->repeatCount = 0;
        } else {
            ptr->tick = -1;
        }

        _kb_simulate_key(data);
    }
}

// 0x4C9EEC
void _GNW95_lost_focus()
{
    if (_focus_func != NULL) {
        _focus_func(0);
    }

    while (!gProgramIsActive) {
        _GNW95_process_message();

        if (_idle_func != NULL) {
            _idle_func();
        }
    }

    if (_focus_func != NULL) {
        _focus_func(1);
    }
}

// 0x4C9F40
int mouseInit()
{
    gMouseInitialized = false;
    _mouse_disabled = 0;

    gCursorIsHidden = true;

    mousePrepareDefaultCursor();

    if (mouseSetFrame(NULL, 0, 0, 0, 0, 0, 0) == -1) {
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
    _mouse_idle_start_time = _get_time();

    return 0;
}

// 0x4C9FD8
void mouseFree()
{
    mouseDeviceUnacquire();

    if (gMouseCursorData != NULL) {
        internal_free(gMouseCursorData);
        gMouseCursorData = NULL;
    }

    if (_mouse_fptr != NULL) {
        tickersRemove(_mouse_anim);
        _mouse_fptr = NULL;
    }
}

// 0x4CA01C
void mousePrepareDefaultCursor()
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
int mouseSetFrame(unsigned char* a1, int width, int height, int pitch, int a5, int a6, int a7)
{
    Rect rect;
    unsigned char* v9;
    int v11, v12;
    int v7, v8;

    v7 = a5;
    v8 = a6;
    v9 = a1;

    if (a1 == NULL) {
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
        if (buf == NULL) {
            if (!cursorWasHidden) {
                mouseShowCursor();
            }
            return -1;
        }

        if (gMouseCursorData != NULL) {
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
        _mouse_fptr = NULL;
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
void _mouse_anim()
{
    if (getTicksSince(_ticker_) >= _mouse_speed) {
        _ticker_ = _get_time();

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

    if (_vcr_state == 1) {
        if (((_vcr_terminate_flags & 4) && buttons) || ((_vcr_terminate_flags & 2) && (x || y))) {
            _vcr_terminated_condition = 2;
            _vcr_stop();
            return;
        }
        x = 0;
        y = 0;
        buttons = gMouseButtonsState;
    }

    _mouse_simulate_input(x, y, buttons);
}

// 0x4CA698
void _mouse_simulate_input(int delta_x, int delta_y, int buttons)
{
    if (!gMouseInitialized || gCursorIsHidden) {
        return;
    }

    if (delta_x || delta_y || buttons != gMouseButtonsState) {
        if (_vcr_state == 0) {
            if (_vcr_buffer_index == 4095) {
                _vcr_dump_buffer();
            }

            STRUCT_51E2F0* ptr = &(_vcr_buffer[_vcr_buffer_index]);
            ptr->type = 3;
            ptr->field_4 = _vcr_time;
            ptr->field_8 = _vcr_counter;
            ptr->dx = delta_x;
            ptr->dy = delta_y;
            ptr->buttons = buttons;

            _vcr_buffer_index++;
        }
    } else {
        if (gMouseButtonsState == 0) {
            if (!_mouse_idling) {
                _mouse_idle_start_time = _get_time();
                _mouse_idling = 1;
            }

            gMouseButtonsState = 0;
            _raw_buttons = 0;
            gMouseEvent = 0;

            return;
        }
    }

    _mouse_idling = 0;
    gMouseButtonsState = buttons;
    gMousePreviousEvent = gMouseEvent;
    gMouseEvent = 0;

    if ((gMousePreviousEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT) != 0) {
        if ((buttons & 0x01) != 0) {
            gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_REPEAT;

            if (getTicksSince(gMouseLeftButtonDownTimestamp) > BUTTON_REPEAT_TIME) {
                gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
                gMouseLeftButtonDownTimestamp = _get_time();
            }
        } else {
            gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_UP;
        }
    } else {
        if ((buttons & 0x01) != 0) {
            gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
            gMouseLeftButtonDownTimestamp = _get_time();
        }
    }

    if ((gMousePreviousEvent & MOUSE_EVENT_RIGHT_BUTTON_DOWN_REPEAT) != 0) {
        if ((buttons & 0x02) != 0) {
            gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_REPEAT;
            if (getTicksSince(gMouseRightButtonDownTimestamp) > BUTTON_REPEAT_TIME) {
                gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
                gMouseRightButtonDownTimestamp = _get_time();
            }
        } else {
            gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_UP;
        }
    } else {
        if (buttons & 0x02) {
            gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
            gMouseRightButtonDownTimestamp = _get_time();
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
void _mouse_clip()
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
    if (value > 0 && value < 2.0) {
        gMouseSensitivity = value;
    }
}

// 0x4CACD0
void mmxSetEnabled(bool a1)
{
    if (!gMmxProbed) {
        gMmxSupported = mmxIsSupported();
        gMmxProbed = true;
    }

    if (gMmxSupported) {
        gMmxEnabled = a1;
    }
}

// 0x4CAD08
int _init_mode_320_200()
{
    return _GNW95_init_mode_ex(320, 200, 8);
}

// 0x4CAD40
int _init_mode_320_400()
{
    return _GNW95_init_mode_ex(320, 400, 8);
}

// 0x4CAD5C
int _init_mode_640_480_16()
{
    return -1;
}

// 0x4CAD64
int _init_mode_640_480()
{
    return _init_vesa_mode(640, 480);
}

// 0x4CAD94
int _init_mode_640_400()
{
    return _init_vesa_mode(640, 400);
}

// 0x4CADA8
int _init_mode_800_600()
{
    return _init_vesa_mode(800, 600);
}

// 0x4CADBC
int _init_mode_1024_768()
{
    return _init_vesa_mode(1024, 768);
}

// 0x4CADD0
int _init_mode_1280_1024()
{
    return _init_vesa_mode(1280, 1024);
}

// 0x4CADF8
void _get_start_mode_()
{
}

// 0x4CADFC
void _zero_vid_mem()
{
    if (_zero_mem) {
        _zero_mem();
    }
}

// 0x4CAE1C
int _GNW95_init_mode_ex(int width, int height, int bpp)
{
    bool fullscreen = true;

    Config resolutionConfig;
    if (configInit(&resolutionConfig)) {
        if (configRead(&resolutionConfig, "f2_res.ini", false)) {
            int screenWidth;
            if (configGetInt(&resolutionConfig, "MAIN", "SCR_WIDTH", &screenWidth)) {
                width = screenWidth;
            }

            int screenHeight;
            if (configGetInt(&resolutionConfig, "MAIN", "SCR_HEIGHT", &screenHeight)) {
                height = screenHeight;
            }

            bool windowed;
            if (configGetBool(&resolutionConfig, "MAIN", "WINDOWED", &windowed)) {
                fullscreen = !windowed;
            }

            configGetBool(&resolutionConfig, "IFACE", "IFACE_BAR_MODE", &gInterfaceBarMode);
        }
        configFree(&resolutionConfig);
    }

    if (_GNW95_init_window(width, height, fullscreen) == -1) {
        return -1;
    }

    if (directDrawInit(width, height, bpp) == -1) {
        return -1;
    }

    _scr_size.left = 0;
    _scr_size.top = 0;
    _scr_size.right = width - 1;
    _scr_size.bottom = height - 1;

    mmxSetEnabled(true);

    if (bpp == 8) {
        _mouse_blit_trans = NULL;
        _scr_blit = _GNW95_ShowRect;
        _zero_mem = _GNW95_zero_vid_mem;
        _mouse_blit = _GNW95_ShowRect;
    } else {
         _zero_mem = NULL;
         _mouse_blit = _GNW95_MouseShowRect16;
         _mouse_blit_trans = _GNW95_MouseShowTransRect16;
         _scr_blit = _GNW95_ShowRect16;
    }

    return 0;
}

// 0x4CAECC
int _init_vesa_mode(int width, int height)
{
    return _GNW95_init_mode_ex(width, height, 8);
}

// 0x4CAEDC
int _GNW95_init_window(int width, int height, bool fullscreen)
{
    if (gSdlWindow == NULL) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            return -1;
        }

        gSdlWindow = SDL_CreateWindow(gProgramWindowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
        if (gSdlWindow == NULL) {
            return -1;
        }

        gSdlWindowSurface = SDL_GetWindowSurface(gSdlWindow);
        if (gSdlWindowSurface == NULL) {
            SDL_DestroyWindow(gSdlWindow);
            gSdlWindow = NULL;
            return -1;
        }

#if _WIN32
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);

        if (!SDL_GetWindowWMInfo(gSdlWindow, &info)) {
            SDL_DestroyWindow(gSdlWindow);
            gSdlWindow = NULL;
            return -1;
        }

        // Needed for DirectSound.
        gProgramWindow = info.info.win.window;
#endif
    }

    return 0;
}

// calculate shift for mask
// 0x4CAF50
int getShiftForBitMask(int mask)
{
    int shift = 0;

    if ((mask & 0xFFFF0000) != 0) {
        shift |= 16;
        mask &= 0xFFFF0000;
    }

    if ((mask & 0xFF00FF00) != 0) {
        shift |= 8;
        mask &= 0xFF00FF00;
    }

    if ((mask & 0xF0F0F0F0) != 0) {
        shift |= 4;
        mask &= 0xF0F0F0F0;
    }

    if ((mask & 0xCCCCCCCC) != 0) {
        shift |= 2;
        mask &= 0xCCCCCCCC;
    }

    if ((mask & 0xAAAAAAAA) != 0) {
        shift |= 1;
    }

    return shift;
}

// 0x4CAF9C
int directDrawInit(int width, int height, int bpp)
{
    if (gSdlSurface != NULL) {
        unsigned char* palette = directDrawGetPalette();
        directDrawFree();

        if (directDrawInit(width, height, bpp) == -1) {
            return -1;
        }

        directDrawSetPalette(palette);

        return 0;
    }

    gSdlSurface = SDL_CreateRGBSurface(0, width, height, bpp, 0, 0, 0, 0);

    if (bpp == 8) {
        SDL_Color colors[256];
        for (int index = 0; index < 256; index++) {
            colors[index].r = index;
            colors[index].g = index;
            colors[index].b = index;
            colors[index].a = 255;
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);
    } else {
        gRedMask = gSdlSurface->format->Rmask;
        gGreenMask = gSdlSurface->format->Gmask;
        gBlueMask = gSdlSurface->format->Bmask;

        gRedShift = gSdlSurface->format->Rshift;
        gGreenShift = gSdlSurface->format->Gshift;
        gBlueShift = gSdlSurface->format->Bshift;
    }

    return 0;
}

// 0x4CB1B0
void directDrawFree()
{
    if (gSdlSurface != NULL) {
        SDL_FreeSurface(gSdlSurface);
        gSdlSurface = NULL;
    }
}

// 0x4CB310
void directDrawSetPaletteInRange(unsigned char* palette, int start, int count)
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
        SDL_Color colors[256];

        if (count != 0) {
            for (int index = 0; index < count; index++) {
                colors[index].r = palette[index * 3] << 2;
                colors[index].g = palette[index * 3 + 1] << 2;
                colors[index].b = palette[index * 3 + 2] << 2;
                colors[index].a = 255;
            }
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, start, count);
        SDL_BlitSurface(gSdlSurface, NULL, gSdlWindowSurface, NULL);
        SDL_UpdateWindowSurface(gSdlWindow);
    } else {
        for (int index = start; index < start + count; index++) {
            unsigned short r = palette[0] << 2;
            unsigned short g = palette[1] << 2;
            unsigned short b = palette[2] << 2;
            palette += 3;

            r = gRedShift > 0 ? (r << gRedShift) : (r >> -gRedShift);
            r &= gRedMask;

            g = gGreenShift > 0 ? (g << gGreenShift) : (g >> -gGreenShift);
            g &= gGreenMask;

            b = gBlueShift > 0 ? (b << gBlueShift) : (b >> -gBlueShift);
            b &= gBlueMask;

            unsigned short rgb = r | g | b;
            gSixteenBppPalette[index] = rgb;
        }

        windowRefreshAll(&_scr_size);
    }

    if (_update_palette_func != NULL) {
        _update_palette_func();
    }
}

// 0x4CB568
void directDrawSetPalette(unsigned char* palette)
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
            SDL_Color colors[256];

        for (int index = 0; index < 256; index++) {
            colors[index].r = palette[index * 3] << 2;
            colors[index].g = palette[index * 3 + 1] << 2;
            colors[index].b = palette[index * 3 + 2] << 2;
            colors[index].a = 255;
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);
        SDL_BlitSurface(gSdlSurface, NULL, gSdlWindowSurface, NULL);
        SDL_UpdateWindowSurface(gSdlWindow);
    } else {
        for (int index = 0; index < 256; index++) {
            unsigned short r = palette[index * 3] << 2;
            unsigned short g = palette[index * 3 + 1] << 2;
            unsigned short b = palette[index * 3 + 2] << 2;

            r = gRedShift > 0 ? (r << gRedShift) : (r >> -gRedShift);
            r &= gRedMask;

            g = gGreenShift > 0 ? (g << gGreenShift) : (g >> -gGreenShift);
            g &= gGreenMask;

            b = gBlueShift > 0 ? (b << gBlueShift) : (b >> -gBlueShift);
            b &= gBlueMask;

            unsigned short rgb = r | g | b;
            gSixteenBppPalette[index] = rgb;
        }

        windowRefreshAll(&_scr_size);
    }


    if (_update_palette_func != NULL) {
        _update_palette_func();
    }
}

// 0x4CB68C
unsigned char* directDrawGetPalette()
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
        SDL_Color* colors = gSdlSurface->format->palette->colors;

        for (int index = 0; index < 256; index++) {
            SDL_Color* color = &(colors[index]);
            gLastVideoModePalette[index * 3] = color->r >> 2;
            gLastVideoModePalette[index * 3 + 1] = color->g >> 2;
            gLastVideoModePalette[index * 3 + 2] = color->b >> 2;
        }

        return gLastVideoModePalette;
    }

    int redShift = gRedShift + 2;
    int greenShift = gGreenShift + 2;
    int blueShift = gBlueShift + 2;

    for (int index = 0; index < 256; index++) {
        unsigned short rgb = gSixteenBppPalette[index];

        unsigned short r = redShift > 0 ? ((rgb & gRedMask) >> redShift) : ((rgb & gRedMask) << -redShift);
        unsigned short g = greenShift > 0 ? ((rgb & gGreenMask) >> greenShift) : ((rgb & gGreenMask) << -greenShift);
        unsigned short b = blueShift > 0 ? ((rgb & gBlueMask) >> blueShift) : ((rgb & gBlueMask) << -blueShift);

        gLastVideoModePalette[index * 3] = (r >> 2) & 0xFF;
        gLastVideoModePalette[index * 3 + 1] = (g >> 2) & 0xFF;
        gLastVideoModePalette[index * 3 + 2] = (b >> 2) & 0xFF;
    }

    return gLastVideoModePalette;
}

// 0x4CB850
void _GNW95_ShowRect(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    SDL_LockSurface(gSdlSurface);
    blitBufferToBuffer(src + srcPitch * srcY + srcX, srcWidth, srcHeight, srcPitch, (unsigned char*)gSdlSurface->pixels + gSdlSurface->pitch * destY + destX, gSdlSurface->pitch);
    SDL_UnlockSurface(gSdlSurface);

    SDL_Rect srcRect;
    srcRect.x = destX;
    srcRect.y = destY;
    srcRect.w = srcWidth;
    srcRect.h = srcHeight;

    SDL_Rect destRect;
    destRect.x = destX;
    destRect.y = destY;
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlWindowSurface, &destRect);
    SDL_UpdateWindowSurface(gSdlWindow);
}

// 0x4CB93C
void _GNW95_MouseShowRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    if (!gProgramIsActive) {
        return;
    }

    SDL_LockSurface(gSdlSurface);
    unsigned char* dest = (unsigned char*)gSdlSurface->pixels + gSdlSurface->pitch * destY + 2 * destX;

    src += srcPitch * srcY + srcX;

    for (int y = 0; y < srcHeight; y++) {
        unsigned short* destPtr = (unsigned short*)dest;
        unsigned char* srcPtr = src;
        for (int x = 0; x < srcWidth; x++) {
            *destPtr = gSixteenBppPalette[*srcPtr];
            destPtr++;
            srcPtr++;
        }

        dest += gSdlSurface->pitch;
        src += srcPitch;
    }

    SDL_UnlockSurface(gSdlSurface);

    SDL_Rect srcRect;
    srcRect.x = destX;
    srcRect.y = destY;
    srcRect.w = srcWidth;
    srcRect.h = srcHeight;

    SDL_Rect destRect;
    destRect.x = destX;
    destRect.y = destY;
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlWindowSurface, &destRect);
    SDL_UpdateWindowSurface(gSdlWindow);
}

// 0x4CBA44
void _GNW95_ShowRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    _GNW95_MouseShowRect16(src, srcPitch, a3, srcX, srcY, srcWidth, srcHeight, destX, destY);
}

// 0x4CBAB0
void _GNW95_MouseShowTransRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY, unsigned char keyColor)
{
    if (!gProgramIsActive) {
        return;
    }

    SDL_LockSurface(gSdlSurface);
    unsigned char* dest = (unsigned char*)gSdlSurface->pixels + gSdlSurface->pitch * destY + 2 * destX;

    src += srcPitch * srcY + srcX;

    for (int y = 0; y < srcHeight; y++) {
        unsigned short* destPtr = (unsigned short*)dest;
        unsigned char* srcPtr = src;
        for (int x = 0; x < srcWidth; x++) {
            if (*srcPtr != keyColor) {
                *destPtr = gSixteenBppPalette[*srcPtr];
            }
            destPtr++;
            srcPtr++;
        }

        dest += gSdlSurface->pitch;
        src += srcPitch;
    }

    SDL_UnlockSurface(gSdlSurface);

    SDL_Rect srcRect;
    srcRect.x = destX;
    srcRect.y = destY;
    srcRect.w = srcWidth;
    srcRect.h = srcHeight;

    SDL_Rect destRect;
    destRect.x = destX;
    destRect.y = destY;
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlWindowSurface, &destRect);
    SDL_UpdateWindowSurface(gSdlWindow);
}

// Clears drawing surface.
//
// 0x4CBBC8
void _GNW95_zero_vid_mem()
{
    if (!gProgramIsActive) {
        return;
    }

    SDL_LockSurface(gSdlSurface);

    unsigned char* surface = (unsigned char*)gSdlSurface->pixels;
    for (unsigned int y = 0; y < gSdlSurface->h; y++) {
        memset(surface, 0, gSdlSurface->w);
        surface += gSdlSurface->pitch;
    }

    SDL_UnlockSurface(gSdlSurface);

    SDL_BlitSurface(gSdlSurface, NULL, gSdlWindowSurface, NULL);
    SDL_UpdateWindowSurface(gSdlWindow);
}

// 0x4CBC90
int keyboardInit()
{
    if (_kb_installed) {
        return -1;
    }

    _kb_installed = 1;
    gPressedPhysicalKeysCount = 0;

    memset(gPressedPhysicalKeys, 0, 256);

    gKeyboardEventQueueWriteIndex = 0;
    gKeyboardEventQueueReadIndex = 0;

    keyboardDeviceReset();
    _GNW95_clear_time_stamps();
    _kb_init_lock_status();
    keyboardSetLayout(KEYBOARD_LAYOUT_QWERTY);

    _kb_idle_start_time = _get_time();

    return 0;
}

// 0x4CBD00
void keyboardFree()
{
    if (_kb_installed) {
        _kb_installed = 0;
    }
}

// 0x4CBDA8
void keyboardReset()
{
    if (_kb_installed) {
        gPressedPhysicalKeysCount = 0;

        memset(&gPressedPhysicalKeys, 0, 256);

        gKeyboardEventQueueWriteIndex = 0;
        gKeyboardEventQueueReadIndex = 0;
    }

    keyboardDeviceReset();
    _GNW95_clear_time_stamps();
}

int _kb_getch()
{
    int rc = -1;

    if (_kb_installed != 0) {
        rc = _kb_scan_to_ascii();
    }

    return rc;
}

// 0x4CBE00
void keyboardDisable()
{
    gKeyboardDisabled = true;
}

// 0x4CBE0C
void keyboardEnable()
{
    gKeyboardDisabled = false;
}

// 0x4CBE18
int keyboardIsDisabled()
{
    return gKeyboardDisabled;
}

// 0x4CBE74
void keyboardSetLayout(int keyboardLayout)
{
    int oldKeyboardLayout = gKeyboardLayout;
    gKeyboardLayout = keyboardLayout;

    switch (keyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        _kb_scan_to_ascii = _kb_next_ascii_English_US;
        keyboardBuildQwertyConfiguration();
        break;
    // case KEYBOARD_LAYOUT_FRENCH:
    //    _kb_scan_to_ascii = sub_4CC5BC;
    //    _kb_map_ascii_French();
    //    break;
    // case KEYBOARD_LAYOUT_GERMAN:
    //    _kb_scan_to_ascii = sub_4CC94C;
    //    _kb_map_ascii_German();
    //    break;
    // case KEYBOARD_LAYOUT_ITALIAN:
    //    _kb_scan_to_ascii = sub_4CCE14;
    //    _kb_map_ascii_Italian();
    //    break;
    // case KEYBOARD_LAYOUT_SPANISH:
    //    _kb_scan_to_ascii = sub_4CD0E0;
    //    _kb_map_ascii_Spanish();
    //    break;
    default:
        gKeyboardLayout = oldKeyboardLayout;
        break;
    }
}

// 0x4CBEEC
int keyboardGetLayout()
{
    return gKeyboardLayout;
}

// TODO: Key type is likely short.
void _kb_simulate_key(KeyboardData* data)
{
    if (_vcr_state == 0) {
        if (_vcr_buffer_index != 4095) {
            STRUCT_51E2F0* ptr = &(_vcr_buffer[_vcr_buffer_index]);
            ptr->type = 2;
            ptr->type_2_field_C = data->key & 0xFFFF;
            ptr->field_4 = _vcr_time;
            ptr->field_8 = _vcr_counter;
            _vcr_buffer_index++;
        }
    }

    _kb_idle_start_time = _get_bk_time();

    int key = data->key;
    int keyState = data->down == 1 ? KEY_STATE_DOWN : KEY_STATE_UP;

    int physicalKey = key;

    if (keyState != KEY_STATE_UP && gPressedPhysicalKeys[physicalKey] != KEY_STATE_UP) {
        keyState = KEY_STATE_REPEAT;
    }

    if (gPressedPhysicalKeys[physicalKey] != keyState) {
        gPressedPhysicalKeys[physicalKey] = keyState;
        if (keyState == KEY_STATE_DOWN) {
            gPressedPhysicalKeysCount++;
        } else if (keyState == KEY_STATE_UP) {
            gPressedPhysicalKeysCount--;
        }
    }

    if (keyState != KEY_STATE_UP) {
        gLastKeyboardEvent.scanCode = physicalKey;
        gLastKeyboardEvent.modifiers = 0;

        if (physicalKey == SDL_SCANCODE_CAPSLOCK) {
            if (gPressedPhysicalKeys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && gPressedPhysicalKeys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                // TODO: Missing check for QWERTY keyboard layout.
                if ((gModifierKeysState & MODIFIER_KEY_STATE_CAPS_LOCK) != 0) {
                    // TODO: There is some strange code checking for _kb_layout, check in
                    // debugger.
                    gModifierKeysState &= ~MODIFIER_KEY_STATE_CAPS_LOCK;
                } else {
                    gModifierKeysState |= MODIFIER_KEY_STATE_CAPS_LOCK;
                }
            }
        } else if (physicalKey == SDL_SCANCODE_NUMLOCKCLEAR) {
            if (gPressedPhysicalKeys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && gPressedPhysicalKeys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                if ((gModifierKeysState & MODIFIER_KEY_STATE_NUM_LOCK) != 0) {
                    gModifierKeysState &= ~MODIFIER_KEY_STATE_NUM_LOCK;
                } else {
                    gModifierKeysState |= MODIFIER_KEY_STATE_NUM_LOCK;
                }
            }
        } else if (physicalKey == SDL_SCANCODE_SCROLLLOCK) {
            if (gPressedPhysicalKeys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && gPressedPhysicalKeys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                if ((gModifierKeysState & MODIFIER_KEY_STATE_SCROLL_LOCK) != 0) {
                    gModifierKeysState &= ~MODIFIER_KEY_STATE_SCROLL_LOCK;
                } else {
                    gModifierKeysState |= MODIFIER_KEY_STATE_SCROLL_LOCK;
                }
            }
        } else if ((physicalKey == SDL_SCANCODE_LSHIFT || physicalKey == SDL_SCANCODE_RSHIFT) && (gModifierKeysState & MODIFIER_KEY_STATE_CAPS_LOCK) != 0 && gKeyboardLayout != 0) {
            if (gPressedPhysicalKeys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && gPressedPhysicalKeys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                if (gModifierKeysState & MODIFIER_KEY_STATE_CAPS_LOCK) {
                    gModifierKeysState &= ~MODIFIER_KEY_STATE_CAPS_LOCK;
                } else {
                    gModifierKeysState |= MODIFIER_KEY_STATE_CAPS_LOCK;
                }
            }
        }

        if (gModifierKeysState != 0) {
            if ((gModifierKeysState & MODIFIER_KEY_STATE_NUM_LOCK) != 0 && !gKeyboardNumlockDisabled) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_NUM_LOCK;
            }

            if ((gModifierKeysState & MODIFIER_KEY_STATE_CAPS_LOCK) != 0) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_CAPS_LOCK;
            }

            if ((gModifierKeysState & MODIFIER_KEY_STATE_SCROLL_LOCK) != 0) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_SCROLL_LOCK;
            }
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_LSHIFT] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_RSHIFT] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_SHIFT;
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_LALT] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_ALT;
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_RALT] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_ALT;
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_LCTRL] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_CONTROL;
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_RCTRL] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_CONTROL;
        }

        if (((gKeyboardEventQueueWriteIndex + 1) & 0x3F) != gKeyboardEventQueueReadIndex) {
            gKeyboardEventsQueue[gKeyboardEventQueueWriteIndex] = gLastKeyboardEvent;
            gKeyboardEventQueueWriteIndex++;
            gKeyboardEventQueueWriteIndex &= 0x3F;
        }
    }
}

// 0x4CC2F0
int _kb_next_ascii_English_US()
{
    KeyboardEvent* keyboardEvent;
    if (keyboardPeekEvent(0, &keyboardEvent) != 0) {
        return -1;
    }

    if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_CAPS_LOCK) != 0) {
        int a = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_A : SDL_SCANCODE_Q);
        int m = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_M : SDL_SCANCODE_SEMICOLON);
        int q = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_Q : SDL_SCANCODE_A);
        int w = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_W : SDL_SCANCODE_Z);

        int y;
        switch (gKeyboardLayout) {
        case KEYBOARD_LAYOUT_QWERTY:
        case KEYBOARD_LAYOUT_FRENCH:
        case KEYBOARD_LAYOUT_ITALIAN:
        case KEYBOARD_LAYOUT_SPANISH:
            y = SDL_SCANCODE_Y;
            break;
        default:
            // GERMAN
            y = SDL_SCANCODE_Z;
            break;
        }

        int z;
        switch (gKeyboardLayout) {
        case KEYBOARD_LAYOUT_QWERTY:
        case KEYBOARD_LAYOUT_ITALIAN:
        case KEYBOARD_LAYOUT_SPANISH:
            z = SDL_SCANCODE_Z;
            break;
        case KEYBOARD_LAYOUT_FRENCH:
            z = SDL_SCANCODE_W;
            break;
        default:
            // GERMAN
            z = SDL_SCANCODE_Y;
            break;
        }

        int scanCode = keyboardEvent->scanCode;
        if (scanCode == a
            || scanCode == SDL_SCANCODE_B
            || scanCode == SDL_SCANCODE_C
            || scanCode == SDL_SCANCODE_D
            || scanCode == SDL_SCANCODE_E
            || scanCode == SDL_SCANCODE_F
            || scanCode == SDL_SCANCODE_G
            || scanCode == SDL_SCANCODE_H
            || scanCode == SDL_SCANCODE_I
            || scanCode == SDL_SCANCODE_J
            || scanCode == SDL_SCANCODE_K
            || scanCode == SDL_SCANCODE_L
            || scanCode == m
            || scanCode == SDL_SCANCODE_N
            || scanCode == SDL_SCANCODE_O
            || scanCode == SDL_SCANCODE_P
            || scanCode == q
            || scanCode == SDL_SCANCODE_R
            || scanCode == SDL_SCANCODE_S
            || scanCode == SDL_SCANCODE_T
            || scanCode == SDL_SCANCODE_U
            || scanCode == SDL_SCANCODE_V
            || scanCode == w
            || scanCode == SDL_SCANCODE_X
            || scanCode == y
            || scanCode == z) {
            if (keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) {
                keyboardEvent->modifiers &= ~KEYBOARD_EVENT_MODIFIER_ANY_SHIFT;
            } else {
                keyboardEvent->modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
            }
        }
    }

    return keyboardDequeueLogicalKeyCode();
}

// 0x4CDA4C
int keyboardDequeueLogicalKeyCode()
{
    KeyboardEvent* keyboardEvent;
    if (keyboardPeekEvent(0, &keyboardEvent) != 0) {
        return -1;
    }

    switch (keyboardEvent->scanCode) {
    case SDL_SCANCODE_KP_DIVIDE:
    case SDL_SCANCODE_KP_MULTIPLY:
    case SDL_SCANCODE_KP_MINUS:
    case SDL_SCANCODE_KP_PLUS:
    case SDL_SCANCODE_KP_ENTER:
        if (gKeyboardNumpadDisabled) {
            if (gKeyboardEventQueueReadIndex != gKeyboardEventQueueWriteIndex) {
                gKeyboardEventQueueReadIndex++;
                gKeyboardEventQueueReadIndex &= (KEY_QUEUE_SIZE - 1);
            }
            return -1;
        }
        break;
    case SDL_SCANCODE_KP_0:
    case SDL_SCANCODE_KP_1:
    case SDL_SCANCODE_KP_2:
    case SDL_SCANCODE_KP_3:
    case SDL_SCANCODE_KP_4:
    case SDL_SCANCODE_KP_5:
    case SDL_SCANCODE_KP_6:
    case SDL_SCANCODE_KP_7:
    case SDL_SCANCODE_KP_8:
    case SDL_SCANCODE_KP_9:
        if (gKeyboardNumpadDisabled) {
            if (gKeyboardEventQueueReadIndex != gKeyboardEventQueueWriteIndex) {
                gKeyboardEventQueueReadIndex++;
                gKeyboardEventQueueReadIndex &= (KEY_QUEUE_SIZE - 1);
            }
            return -1;
        }

        if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_ALT) == 0 && (keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_NUM_LOCK) != 0) {
            if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) != 0) {
                keyboardEvent->modifiers &= ~KEYBOARD_EVENT_MODIFIER_ANY_SHIFT;
            } else {
                keyboardEvent->modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
            }
        }

        break;
    }

    int logicalKey = -1;

    LogicalKeyEntry* logicalKeyDescription = &(gLogicalKeyEntries[keyboardEvent->scanCode]);
    if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_CONTROL) != 0) {
        logicalKey = logicalKeyDescription->ctrl;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_RIGHT_ALT) != 0) {
        logicalKey = logicalKeyDescription->rmenu;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_LEFT_ALT) != 0) {
        logicalKey = logicalKeyDescription->lmenu;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) != 0) {
        logicalKey = logicalKeyDescription->shift;
    } else {
        logicalKey = logicalKeyDescription->unmodified;
    }

    if (gKeyboardEventQueueReadIndex != gKeyboardEventQueueWriteIndex) {
        gKeyboardEventQueueReadIndex++;
        gKeyboardEventQueueReadIndex &= (KEY_QUEUE_SIZE - 1);
    }

    return logicalKey;
}

// 0x4CDC08
void keyboardBuildQwertyConfiguration()
{
    int k;

    for (k = 0; k < SDL_NUM_SCANCODES; k++) {
        gLogicalKeyEntries[k].field_0 = -1;
        gLogicalKeyEntries[k].unmodified = -1;
        gLogicalKeyEntries[k].shift = -1;
        gLogicalKeyEntries[k].lmenu = -1;
        gLogicalKeyEntries[k].rmenu = -1;
        gLogicalKeyEntries[k].ctrl = -1;
    }

    gLogicalKeyEntries[SDL_SCANCODE_ESCAPE].unmodified = KEY_ESCAPE;
    gLogicalKeyEntries[SDL_SCANCODE_ESCAPE].shift = KEY_ESCAPE;
    gLogicalKeyEntries[SDL_SCANCODE_ESCAPE].lmenu = KEY_ESCAPE;
    gLogicalKeyEntries[SDL_SCANCODE_ESCAPE].rmenu = KEY_ESCAPE;
    gLogicalKeyEntries[SDL_SCANCODE_ESCAPE].ctrl = KEY_ESCAPE;

    gLogicalKeyEntries[SDL_SCANCODE_F1].unmodified = KEY_F1;
    gLogicalKeyEntries[SDL_SCANCODE_F1].shift = KEY_SHIFT_F1;
    gLogicalKeyEntries[SDL_SCANCODE_F1].lmenu = KEY_ALT_F1;
    gLogicalKeyEntries[SDL_SCANCODE_F1].rmenu = KEY_ALT_F1;
    gLogicalKeyEntries[SDL_SCANCODE_F1].ctrl = KEY_CTRL_F1;

    gLogicalKeyEntries[SDL_SCANCODE_F2].unmodified = KEY_F2;
    gLogicalKeyEntries[SDL_SCANCODE_F2].shift = KEY_SHIFT_F2;
    gLogicalKeyEntries[SDL_SCANCODE_F2].lmenu = KEY_ALT_F2;
    gLogicalKeyEntries[SDL_SCANCODE_F2].rmenu = KEY_ALT_F2;
    gLogicalKeyEntries[SDL_SCANCODE_F2].ctrl = KEY_CTRL_F2;

    gLogicalKeyEntries[SDL_SCANCODE_F3].unmodified = KEY_F3;
    gLogicalKeyEntries[SDL_SCANCODE_F3].shift = KEY_SHIFT_F3;
    gLogicalKeyEntries[SDL_SCANCODE_F3].lmenu = KEY_ALT_F3;
    gLogicalKeyEntries[SDL_SCANCODE_F3].rmenu = KEY_ALT_F3;
    gLogicalKeyEntries[SDL_SCANCODE_F3].ctrl = KEY_CTRL_F3;

    gLogicalKeyEntries[SDL_SCANCODE_F4].unmodified = KEY_F4;
    gLogicalKeyEntries[SDL_SCANCODE_F4].shift = KEY_SHIFT_F4;
    gLogicalKeyEntries[SDL_SCANCODE_F4].lmenu = KEY_ALT_F4;
    gLogicalKeyEntries[SDL_SCANCODE_F4].rmenu = KEY_ALT_F4;
    gLogicalKeyEntries[SDL_SCANCODE_F4].ctrl = KEY_CTRL_F4;

    gLogicalKeyEntries[SDL_SCANCODE_F5].unmodified = KEY_F5;
    gLogicalKeyEntries[SDL_SCANCODE_F5].shift = KEY_SHIFT_F5;
    gLogicalKeyEntries[SDL_SCANCODE_F5].lmenu = KEY_ALT_F5;
    gLogicalKeyEntries[SDL_SCANCODE_F5].rmenu = KEY_ALT_F5;
    gLogicalKeyEntries[SDL_SCANCODE_F5].ctrl = KEY_CTRL_F5;

    gLogicalKeyEntries[SDL_SCANCODE_F6].unmodified = KEY_F6;
    gLogicalKeyEntries[SDL_SCANCODE_F6].shift = KEY_SHIFT_F6;
    gLogicalKeyEntries[SDL_SCANCODE_F6].lmenu = KEY_ALT_F6;
    gLogicalKeyEntries[SDL_SCANCODE_F6].rmenu = KEY_ALT_F6;
    gLogicalKeyEntries[SDL_SCANCODE_F6].ctrl = KEY_CTRL_F6;

    gLogicalKeyEntries[SDL_SCANCODE_F7].unmodified = KEY_F7;
    gLogicalKeyEntries[SDL_SCANCODE_F7].shift = KEY_SHIFT_F7;
    gLogicalKeyEntries[SDL_SCANCODE_F7].lmenu = KEY_ALT_F7;
    gLogicalKeyEntries[SDL_SCANCODE_F7].rmenu = KEY_ALT_F7;
    gLogicalKeyEntries[SDL_SCANCODE_F7].ctrl = KEY_CTRL_F7;

    gLogicalKeyEntries[SDL_SCANCODE_F8].unmodified = KEY_F8;
    gLogicalKeyEntries[SDL_SCANCODE_F8].shift = KEY_SHIFT_F8;
    gLogicalKeyEntries[SDL_SCANCODE_F8].lmenu = KEY_ALT_F8;
    gLogicalKeyEntries[SDL_SCANCODE_F8].rmenu = KEY_ALT_F8;
    gLogicalKeyEntries[SDL_SCANCODE_F8].ctrl = KEY_CTRL_F8;

    gLogicalKeyEntries[SDL_SCANCODE_F9].unmodified = KEY_F9;
    gLogicalKeyEntries[SDL_SCANCODE_F9].shift = KEY_SHIFT_F9;
    gLogicalKeyEntries[SDL_SCANCODE_F9].lmenu = KEY_ALT_F9;
    gLogicalKeyEntries[SDL_SCANCODE_F9].rmenu = KEY_ALT_F9;
    gLogicalKeyEntries[SDL_SCANCODE_F9].ctrl = KEY_CTRL_F9;

    gLogicalKeyEntries[SDL_SCANCODE_F10].unmodified = KEY_F10;
    gLogicalKeyEntries[SDL_SCANCODE_F10].shift = KEY_SHIFT_F10;
    gLogicalKeyEntries[SDL_SCANCODE_F10].lmenu = KEY_ALT_F10;
    gLogicalKeyEntries[SDL_SCANCODE_F10].rmenu = KEY_ALT_F10;
    gLogicalKeyEntries[SDL_SCANCODE_F10].ctrl = KEY_CTRL_F10;

    gLogicalKeyEntries[SDL_SCANCODE_F11].unmodified = KEY_F11;
    gLogicalKeyEntries[SDL_SCANCODE_F11].shift = KEY_SHIFT_F11;
    gLogicalKeyEntries[SDL_SCANCODE_F11].lmenu = KEY_ALT_F11;
    gLogicalKeyEntries[SDL_SCANCODE_F11].rmenu = KEY_ALT_F11;
    gLogicalKeyEntries[SDL_SCANCODE_F11].ctrl = KEY_CTRL_F11;

    gLogicalKeyEntries[SDL_SCANCODE_F12].unmodified = KEY_F12;
    gLogicalKeyEntries[SDL_SCANCODE_F12].shift = KEY_SHIFT_F12;
    gLogicalKeyEntries[SDL_SCANCODE_F12].lmenu = KEY_ALT_F12;
    gLogicalKeyEntries[SDL_SCANCODE_F12].rmenu = KEY_ALT_F12;
    gLogicalKeyEntries[SDL_SCANCODE_F12].ctrl = KEY_CTRL_F12;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_GRAVE;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_2;
        break;
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = 0;
        break;
    default:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_GRAVE;
    gLogicalKeyEntries[k].shift = KEY_TILDE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_1].unmodified = KEY_1;
    gLogicalKeyEntries[SDL_SCANCODE_1].shift = KEY_EXCLAMATION;
    gLogicalKeyEntries[SDL_SCANCODE_1].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_2].unmodified = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_2].shift = KEY_AT;
    gLogicalKeyEntries[SDL_SCANCODE_2].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_3].unmodified = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_3].shift = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[SDL_SCANCODE_3].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_4].unmodified = KEY_4;
    gLogicalKeyEntries[SDL_SCANCODE_4].shift = KEY_DOLLAR;
    gLogicalKeyEntries[SDL_SCANCODE_4].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_4].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_4].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_5].unmodified = KEY_5;
    gLogicalKeyEntries[SDL_SCANCODE_5].shift = KEY_PERCENT;
    gLogicalKeyEntries[SDL_SCANCODE_5].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_5].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_5].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_6].unmodified = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_6].shift = KEY_CARET;
    gLogicalKeyEntries[SDL_SCANCODE_6].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_7].unmodified = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_7].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[SDL_SCANCODE_7].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_8].unmodified = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_8].shift = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_8].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_9].unmodified = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_9].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_9].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_0].unmodified = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_0].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_0].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_MINUS;
    gLogicalKeyEntries[k].shift = KEY_UNDERSCORE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_EQUALS;
        break;
    default:
        k = SDL_SCANCODE_0;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_EQUAL;
    gLogicalKeyEntries[k].shift = KEY_PLUS;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_BACKSPACE].unmodified = KEY_BACKSPACE;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSPACE].shift = KEY_BACKSPACE;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSPACE].lmenu = KEY_BACKSPACE;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSPACE].rmenu = KEY_BACKSPACE;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSPACE].ctrl = KEY_DEL;

    gLogicalKeyEntries[SDL_SCANCODE_TAB].unmodified = KEY_TAB;
    gLogicalKeyEntries[SDL_SCANCODE_TAB].shift = KEY_TAB;
    gLogicalKeyEntries[SDL_SCANCODE_TAB].lmenu = KEY_TAB;
    gLogicalKeyEntries[SDL_SCANCODE_TAB].rmenu = KEY_TAB;
    gLogicalKeyEntries[SDL_SCANCODE_TAB].ctrl = KEY_TAB;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_A;
        break;
    default:
        k = SDL_SCANCODE_Q;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Q;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Q;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Q;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_Q;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Q;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_Z;
        break;
    default:
        k = SDL_SCANCODE_W;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_W;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_W;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_W;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_W;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_W;

    gLogicalKeyEntries[SDL_SCANCODE_E].unmodified = KEY_LOWERCASE_E;
    gLogicalKeyEntries[SDL_SCANCODE_E].shift = KEY_UPPERCASE_E;
    gLogicalKeyEntries[SDL_SCANCODE_E].lmenu = KEY_ALT_E;
    gLogicalKeyEntries[SDL_SCANCODE_E].rmenu = KEY_ALT_E;
    gLogicalKeyEntries[SDL_SCANCODE_E].ctrl = KEY_CTRL_E;

    gLogicalKeyEntries[SDL_SCANCODE_R].unmodified = KEY_LOWERCASE_R;
    gLogicalKeyEntries[SDL_SCANCODE_R].shift = KEY_UPPERCASE_R;
    gLogicalKeyEntries[SDL_SCANCODE_R].lmenu = KEY_ALT_R;
    gLogicalKeyEntries[SDL_SCANCODE_R].rmenu = KEY_ALT_R;
    gLogicalKeyEntries[SDL_SCANCODE_R].ctrl = KEY_CTRL_R;

    gLogicalKeyEntries[SDL_SCANCODE_T].unmodified = KEY_LOWERCASE_T;
    gLogicalKeyEntries[SDL_SCANCODE_T].shift = KEY_UPPERCASE_T;
    gLogicalKeyEntries[SDL_SCANCODE_T].lmenu = KEY_ALT_T;
    gLogicalKeyEntries[SDL_SCANCODE_T].rmenu = KEY_ALT_T;
    gLogicalKeyEntries[SDL_SCANCODE_T].ctrl = KEY_CTRL_T;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_Y;
        break;
    default:
        k = SDL_SCANCODE_Z;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Y;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Y;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Y;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_Y;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Y;

    gLogicalKeyEntries[SDL_SCANCODE_U].unmodified = KEY_LOWERCASE_U;
    gLogicalKeyEntries[SDL_SCANCODE_U].shift = KEY_UPPERCASE_U;
    gLogicalKeyEntries[SDL_SCANCODE_U].lmenu = KEY_ALT_U;
    gLogicalKeyEntries[SDL_SCANCODE_U].rmenu = KEY_ALT_U;
    gLogicalKeyEntries[SDL_SCANCODE_U].ctrl = KEY_CTRL_U;

    gLogicalKeyEntries[SDL_SCANCODE_I].unmodified = KEY_LOWERCASE_I;
    gLogicalKeyEntries[SDL_SCANCODE_I].shift = KEY_UPPERCASE_I;
    gLogicalKeyEntries[SDL_SCANCODE_I].lmenu = KEY_ALT_I;
    gLogicalKeyEntries[SDL_SCANCODE_I].rmenu = KEY_ALT_I;
    gLogicalKeyEntries[SDL_SCANCODE_I].ctrl = KEY_CTRL_I;

    gLogicalKeyEntries[SDL_SCANCODE_O].unmodified = KEY_LOWERCASE_O;
    gLogicalKeyEntries[SDL_SCANCODE_O].shift = KEY_UPPERCASE_O;
    gLogicalKeyEntries[SDL_SCANCODE_O].lmenu = KEY_ALT_O;
    gLogicalKeyEntries[SDL_SCANCODE_O].rmenu = KEY_ALT_O;
    gLogicalKeyEntries[SDL_SCANCODE_O].ctrl = KEY_CTRL_O;

    gLogicalKeyEntries[SDL_SCANCODE_P].unmodified = KEY_LOWERCASE_P;
    gLogicalKeyEntries[SDL_SCANCODE_P].shift = KEY_UPPERCASE_P;
    gLogicalKeyEntries[SDL_SCANCODE_P].lmenu = KEY_ALT_P;
    gLogicalKeyEntries[SDL_SCANCODE_P].rmenu = KEY_ALT_P;
    gLogicalKeyEntries[SDL_SCANCODE_P].ctrl = KEY_CTRL_P;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_LEFTBRACKET;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_5;
        break;
    default:
        k = SDL_SCANCODE_8;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[k].shift = KEY_BRACE_LEFT;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_MINUS;
        break;
    default:
        k = SDL_SCANCODE_9;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[k].shift = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_BACKSLASH;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_8;
        break;
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_GRAVE;
        break;
    default:
        k = SDL_SCANCODE_MINUS;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_BACKSLASH;
    gLogicalKeyEntries[k].shift = KEY_BAR;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_BACKSLASH;

    gLogicalKeyEntries[SDL_SCANCODE_CAPSLOCK].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_CAPSLOCK].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_CAPSLOCK].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_CAPSLOCK].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_CAPSLOCK].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_Q;
        break;
    default:
        k = SDL_SCANCODE_A;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_A;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_A;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_A;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_A;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_A;

    gLogicalKeyEntries[SDL_SCANCODE_S].unmodified = KEY_LOWERCASE_S;
    gLogicalKeyEntries[SDL_SCANCODE_S].shift = KEY_UPPERCASE_S;
    gLogicalKeyEntries[SDL_SCANCODE_S].lmenu = KEY_ALT_S;
    gLogicalKeyEntries[SDL_SCANCODE_S].rmenu = KEY_ALT_S;
    gLogicalKeyEntries[SDL_SCANCODE_S].ctrl = KEY_CTRL_S;

    gLogicalKeyEntries[SDL_SCANCODE_D].unmodified = KEY_LOWERCASE_D;
    gLogicalKeyEntries[SDL_SCANCODE_D].shift = KEY_UPPERCASE_D;
    gLogicalKeyEntries[SDL_SCANCODE_D].lmenu = KEY_ALT_D;
    gLogicalKeyEntries[SDL_SCANCODE_D].rmenu = KEY_ALT_D;
    gLogicalKeyEntries[SDL_SCANCODE_D].ctrl = KEY_CTRL_D;

    gLogicalKeyEntries[SDL_SCANCODE_F].unmodified = KEY_LOWERCASE_F;
    gLogicalKeyEntries[SDL_SCANCODE_F].shift = KEY_UPPERCASE_F;
    gLogicalKeyEntries[SDL_SCANCODE_F].lmenu = KEY_ALT_F;
    gLogicalKeyEntries[SDL_SCANCODE_F].rmenu = KEY_ALT_F;
    gLogicalKeyEntries[SDL_SCANCODE_F].ctrl = KEY_CTRL_F;

    gLogicalKeyEntries[SDL_SCANCODE_G].unmodified = KEY_LOWERCASE_G;
    gLogicalKeyEntries[SDL_SCANCODE_G].shift = KEY_UPPERCASE_G;
    gLogicalKeyEntries[SDL_SCANCODE_G].lmenu = KEY_ALT_G;
    gLogicalKeyEntries[SDL_SCANCODE_G].rmenu = KEY_ALT_G;
    gLogicalKeyEntries[SDL_SCANCODE_G].ctrl = KEY_CTRL_G;

    gLogicalKeyEntries[SDL_SCANCODE_H].unmodified = KEY_LOWERCASE_H;
    gLogicalKeyEntries[SDL_SCANCODE_H].shift = KEY_UPPERCASE_H;
    gLogicalKeyEntries[SDL_SCANCODE_H].lmenu = KEY_ALT_H;
    gLogicalKeyEntries[SDL_SCANCODE_H].rmenu = KEY_ALT_H;
    gLogicalKeyEntries[SDL_SCANCODE_H].ctrl = KEY_CTRL_H;

    gLogicalKeyEntries[SDL_SCANCODE_J].unmodified = KEY_LOWERCASE_J;
    gLogicalKeyEntries[SDL_SCANCODE_J].shift = KEY_UPPERCASE_J;
    gLogicalKeyEntries[SDL_SCANCODE_J].lmenu = KEY_ALT_J;
    gLogicalKeyEntries[SDL_SCANCODE_J].rmenu = KEY_ALT_J;
    gLogicalKeyEntries[SDL_SCANCODE_J].ctrl = KEY_CTRL_J;

    gLogicalKeyEntries[SDL_SCANCODE_K].unmodified = KEY_LOWERCASE_K;
    gLogicalKeyEntries[SDL_SCANCODE_K].shift = KEY_UPPERCASE_K;
    gLogicalKeyEntries[SDL_SCANCODE_K].lmenu = KEY_ALT_K;
    gLogicalKeyEntries[SDL_SCANCODE_K].rmenu = KEY_ALT_K;
    gLogicalKeyEntries[SDL_SCANCODE_K].ctrl = KEY_CTRL_K;

    gLogicalKeyEntries[SDL_SCANCODE_L].unmodified = KEY_LOWERCASE_L;
    gLogicalKeyEntries[SDL_SCANCODE_L].shift = KEY_UPPERCASE_L;
    gLogicalKeyEntries[SDL_SCANCODE_L].lmenu = KEY_ALT_L;
    gLogicalKeyEntries[SDL_SCANCODE_L].rmenu = KEY_ALT_L;
    gLogicalKeyEntries[SDL_SCANCODE_L].ctrl = KEY_CTRL_L;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_SEMICOLON;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_APOSTROPHE;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_3;
        break;
    default:
        k = SDL_SCANCODE_2;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[k].shift = KEY_QUOTE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RETURN].unmodified = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_RETURN].shift = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_RETURN].lmenu = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_RETURN].rmenu = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_RETURN].ctrl = KEY_CTRL_J;

    gLogicalKeyEntries[SDL_SCANCODE_LSHIFT].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LSHIFT].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LSHIFT].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LSHIFT].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LSHIFT].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_Z;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_W;
        break;
    default:
        k = SDL_SCANCODE_Y;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Z;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Z;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Z;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_Z;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Z;

    gLogicalKeyEntries[SDL_SCANCODE_X].unmodified = KEY_LOWERCASE_X;
    gLogicalKeyEntries[SDL_SCANCODE_X].shift = KEY_UPPERCASE_X;
    gLogicalKeyEntries[SDL_SCANCODE_X].lmenu = KEY_ALT_X;
    gLogicalKeyEntries[SDL_SCANCODE_X].rmenu = KEY_ALT_X;
    gLogicalKeyEntries[SDL_SCANCODE_X].ctrl = KEY_CTRL_X;

    gLogicalKeyEntries[SDL_SCANCODE_C].unmodified = KEY_LOWERCASE_C;
    gLogicalKeyEntries[SDL_SCANCODE_C].shift = KEY_UPPERCASE_C;
    gLogicalKeyEntries[SDL_SCANCODE_C].lmenu = KEY_ALT_C;
    gLogicalKeyEntries[SDL_SCANCODE_C].rmenu = KEY_ALT_C;
    gLogicalKeyEntries[SDL_SCANCODE_C].ctrl = KEY_CTRL_C;

    gLogicalKeyEntries[SDL_SCANCODE_V].unmodified = KEY_LOWERCASE_V;
    gLogicalKeyEntries[SDL_SCANCODE_V].shift = KEY_UPPERCASE_V;
    gLogicalKeyEntries[SDL_SCANCODE_V].lmenu = KEY_ALT_V;
    gLogicalKeyEntries[SDL_SCANCODE_V].rmenu = KEY_ALT_V;
    gLogicalKeyEntries[SDL_SCANCODE_V].ctrl = KEY_CTRL_V;

    gLogicalKeyEntries[SDL_SCANCODE_B].unmodified = KEY_LOWERCASE_B;
    gLogicalKeyEntries[SDL_SCANCODE_B].shift = KEY_UPPERCASE_B;
    gLogicalKeyEntries[SDL_SCANCODE_B].lmenu = KEY_ALT_B;
    gLogicalKeyEntries[SDL_SCANCODE_B].rmenu = KEY_ALT_B;
    gLogicalKeyEntries[SDL_SCANCODE_B].ctrl = KEY_CTRL_B;

    gLogicalKeyEntries[SDL_SCANCODE_N].unmodified = KEY_LOWERCASE_N;
    gLogicalKeyEntries[SDL_SCANCODE_N].shift = KEY_UPPERCASE_N;
    gLogicalKeyEntries[SDL_SCANCODE_N].lmenu = KEY_ALT_N;
    gLogicalKeyEntries[SDL_SCANCODE_N].rmenu = KEY_ALT_N;
    gLogicalKeyEntries[SDL_SCANCODE_N].ctrl = KEY_CTRL_N;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_M;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_M;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_M;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_M;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_M;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_M;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_LESS;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_GREATER;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_SLASH;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_PERIOD;
        break;
    default:
        k = SDL_SCANCODE_7;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_SLASH;
    gLogicalKeyEntries[k].shift = KEY_QUESTION;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RSHIFT].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RSHIFT].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RSHIFT].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RSHIFT].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RSHIFT].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_LCTRL].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LCTRL].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LCTRL].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LCTRL].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LCTRL].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_LALT].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LALT].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LALT].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LALT].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LALT].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_SPACE].unmodified = KEY_SPACE;
    gLogicalKeyEntries[SDL_SCANCODE_SPACE].shift = KEY_SPACE;
    gLogicalKeyEntries[SDL_SCANCODE_SPACE].lmenu = KEY_SPACE;
    gLogicalKeyEntries[SDL_SCANCODE_SPACE].rmenu = KEY_SPACE;
    gLogicalKeyEntries[SDL_SCANCODE_SPACE].ctrl = KEY_SPACE;

    gLogicalKeyEntries[SDL_SCANCODE_RALT].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RALT].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RALT].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RALT].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RALT].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RCTRL].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RCTRL].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RCTRL].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RCTRL].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RCTRL].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_INSERT].unmodified = KEY_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_INSERT].shift = KEY_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_INSERT].lmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_INSERT].rmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_INSERT].ctrl = KEY_CTRL_INSERT;

    gLogicalKeyEntries[SDL_SCANCODE_HOME].unmodified = KEY_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_HOME].shift = KEY_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_HOME].lmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_HOME].rmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_HOME].ctrl = KEY_CTRL_HOME;

    gLogicalKeyEntries[SDL_SCANCODE_PRIOR].unmodified = KEY_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_PRIOR].shift = KEY_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_PRIOR].lmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_PRIOR].rmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_PRIOR].ctrl = KEY_CTRL_PAGE_UP;

    gLogicalKeyEntries[SDL_SCANCODE_DELETE].unmodified = KEY_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_DELETE].shift = KEY_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_DELETE].lmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_DELETE].rmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_DELETE].ctrl = KEY_CTRL_DELETE;

    gLogicalKeyEntries[SDL_SCANCODE_END].unmodified = KEY_END;
    gLogicalKeyEntries[SDL_SCANCODE_END].shift = KEY_END;
    gLogicalKeyEntries[SDL_SCANCODE_END].lmenu = KEY_ALT_END;
    gLogicalKeyEntries[SDL_SCANCODE_END].rmenu = KEY_ALT_END;
    gLogicalKeyEntries[SDL_SCANCODE_END].ctrl = KEY_CTRL_END;

    gLogicalKeyEntries[SDL_SCANCODE_PAGEDOWN].unmodified = KEY_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_PAGEDOWN].shift = KEY_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_PAGEDOWN].lmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_PAGEDOWN].rmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_PAGEDOWN].ctrl = KEY_CTRL_PAGE_DOWN;

    gLogicalKeyEntries[SDL_SCANCODE_UP].unmodified = KEY_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_UP].shift = KEY_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_UP].lmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_UP].rmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_UP].ctrl = KEY_CTRL_ARROW_UP;

    gLogicalKeyEntries[SDL_SCANCODE_DOWN].unmodified = KEY_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_DOWN].shift = KEY_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_DOWN].lmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_DOWN].rmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_DOWN].ctrl = KEY_CTRL_ARROW_DOWN;

    gLogicalKeyEntries[SDL_SCANCODE_LEFT].unmodified = KEY_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFT].shift = KEY_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFT].lmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFT].rmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFT].ctrl = KEY_CTRL_ARROW_LEFT;

    gLogicalKeyEntries[SDL_SCANCODE_RIGHT].unmodified = KEY_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHT].shift = KEY_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHT].lmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHT].rmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHT].ctrl = KEY_CTRL_ARROW_RIGHT;

    gLogicalKeyEntries[SDL_SCANCODE_NUMLOCKCLEAR].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_NUMLOCKCLEAR].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_NUMLOCKCLEAR].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_NUMLOCKCLEAR].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_NUMLOCKCLEAR].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].unmodified = KEY_SLASH;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].shift = KEY_SLASH;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].ctrl = 3;

    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].unmodified = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].shift = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_MINUS].unmodified = KEY_MINUS;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MINUS].shift = KEY_MINUS;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MINUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MINUS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MINUS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_7].unmodified = KEY_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_KP_7].shift = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_KP_7].lmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_KP_7].rmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_KP_7].ctrl = KEY_CTRL_HOME;

    gLogicalKeyEntries[SDL_SCANCODE_KP_8].unmodified = KEY_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_8].shift = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_KP_8].lmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_8].rmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_8].ctrl = KEY_CTRL_ARROW_UP;

    gLogicalKeyEntries[SDL_SCANCODE_KP_9].unmodified = KEY_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_9].shift = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_KP_9].lmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_9].rmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_9].ctrl = KEY_CTRL_PAGE_UP;

    gLogicalKeyEntries[SDL_SCANCODE_KP_PLUS].unmodified = KEY_PLUS;
    gLogicalKeyEntries[SDL_SCANCODE_KP_PLUS].shift = KEY_PLUS;
    gLogicalKeyEntries[SDL_SCANCODE_KP_PLUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_PLUS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_PLUS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_4].unmodified = KEY_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_4].shift = KEY_4;
    gLogicalKeyEntries[SDL_SCANCODE_KP_4].lmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_4].rmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_4].ctrl = KEY_CTRL_ARROW_LEFT;

    gLogicalKeyEntries[SDL_SCANCODE_KP_5].unmodified = KEY_NUMBERPAD_5;
    gLogicalKeyEntries[SDL_SCANCODE_KP_5].shift = KEY_5;
    gLogicalKeyEntries[SDL_SCANCODE_KP_5].lmenu = KEY_ALT_NUMBERPAD_5;
    gLogicalKeyEntries[SDL_SCANCODE_KP_5].rmenu = KEY_ALT_NUMBERPAD_5;
    gLogicalKeyEntries[SDL_SCANCODE_KP_5].ctrl = KEY_CTRL_NUMBERPAD_5;

    gLogicalKeyEntries[SDL_SCANCODE_KP_6].unmodified = KEY_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_6].shift = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_KP_6].lmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_6].rmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_6].ctrl = KEY_CTRL_ARROW_RIGHT;

    gLogicalKeyEntries[SDL_SCANCODE_KP_1].unmodified = KEY_END;
    gLogicalKeyEntries[SDL_SCANCODE_KP_1].shift = KEY_1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_1].lmenu = KEY_ALT_END;
    gLogicalKeyEntries[SDL_SCANCODE_KP_1].rmenu = KEY_ALT_END;
    gLogicalKeyEntries[SDL_SCANCODE_KP_1].ctrl = KEY_CTRL_END;

    gLogicalKeyEntries[SDL_SCANCODE_KP_2].unmodified = KEY_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_2].shift = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_KP_2].lmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_2].rmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_2].ctrl = KEY_CTRL_ARROW_DOWN;

    gLogicalKeyEntries[SDL_SCANCODE_KP_3].unmodified = KEY_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_3].shift = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_KP_3].lmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_3].rmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_3].ctrl = KEY_CTRL_PAGE_DOWN;

    gLogicalKeyEntries[SDL_SCANCODE_KP_ENTER].unmodified = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_ENTER].shift = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_ENTER].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_ENTER].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_ENTER].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_0].unmodified = KEY_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_0].shift = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_KP_0].lmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_0].rmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_0].ctrl = KEY_CTRL_INSERT;

    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].unmodified = KEY_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].shift = KEY_DOT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].rmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].ctrl = KEY_CTRL_DELETE;
}

// 0x4D0400
void keyboardBuildFrenchConfiguration()
{
    int k;

    keyboardBuildQwertyConfiguration();

    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].unmodified = KEY_178;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_1].unmodified = KEY_AMPERSAND;
    gLogicalKeyEntries[SDL_SCANCODE_1].shift = KEY_1;
    gLogicalKeyEntries[SDL_SCANCODE_1].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_2].unmodified = KEY_233;
    gLogicalKeyEntries[SDL_SCANCODE_2].shift = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_2].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].rmenu = KEY_152;
    gLogicalKeyEntries[SDL_SCANCODE_2].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_3].unmodified = KEY_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_3].shift = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_3].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].rmenu = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[SDL_SCANCODE_3].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_4].unmodified = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_4].shift = KEY_4;
    gLogicalKeyEntries[SDL_SCANCODE_4].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_4].rmenu = KEY_BRACE_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_4].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_5].unmodified = KEY_PAREN_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_5].shift = KEY_5;
    gLogicalKeyEntries[SDL_SCANCODE_5].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_5].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_5].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_6].unmodified = KEY_150;
    gLogicalKeyEntries[SDL_SCANCODE_6].shift = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_6].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].rmenu = KEY_166;
    gLogicalKeyEntries[SDL_SCANCODE_6].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_7].unmodified = KEY_232;
    gLogicalKeyEntries[SDL_SCANCODE_7].shift = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_7].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].rmenu = KEY_GRAVE;
    gLogicalKeyEntries[SDL_SCANCODE_7].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_8].unmodified = KEY_UNDERSCORE;
    gLogicalKeyEntries[SDL_SCANCODE_8].shift = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_8].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].rmenu = KEY_BACKSLASH;
    gLogicalKeyEntries[SDL_SCANCODE_8].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_9].unmodified = KEY_231;
    gLogicalKeyEntries[SDL_SCANCODE_9].shift = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_9].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].rmenu = KEY_136;
    gLogicalKeyEntries[SDL_SCANCODE_9].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_0].unmodified = KEY_224;
    gLogicalKeyEntries[SDL_SCANCODE_0].shift = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_0].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].rmenu = KEY_AT;
    gLogicalKeyEntries[SDL_SCANCODE_0].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_MINUS].unmodified = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].shift = KEY_176;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_EQUALS;
        break;
    default:
        k = SDL_SCANCODE_0;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_EQUAL;
    gLogicalKeyEntries[k].shift = KEY_PLUS;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].unmodified = KEY_136;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].shift = KEY_168;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].unmodified = KEY_DOLLAR;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].shift = KEY_163;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].rmenu = KEY_164;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].unmodified = KEY_249;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].shift = KEY_PERCENT;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].unmodified = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].shift = KEY_181;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    //gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    //gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    //gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    //gLogicalKeyEntries[DIK_OEM_102].rmenu = -1;
    //gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_QUESTION;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_SEMICOLON;
    gLogicalKeyEntries[k].shift = KEY_DOT;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        // FIXME: Probably error, maps semicolon to colon on QWERTY keyboards.
        // Semicolon is already mapped above, so I bet it should be SDL_SCANCODE_COLON.
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COLON;
    gLogicalKeyEntries[k].shift = KEY_SLASH;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_SLASH].unmodified = KEY_EXCLAMATION;
    gLogicalKeyEntries[SDL_SCANCODE_SLASH].shift = KEY_167;
    gLogicalKeyEntries[SDL_SCANCODE_SLASH].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SLASH].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SLASH].ctrl = -1;
}

// 0x4D0C54
void keyboardBuildGermanConfiguration()
{
    int k;

    keyboardBuildQwertyConfiguration();

    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].unmodified = KEY_136;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].shift = KEY_186;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_2].unmodified = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_2].shift = KEY_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_2].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].rmenu = KEY_178;
    gLogicalKeyEntries[SDL_SCANCODE_2].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_3].unmodified = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_3].shift = KEY_167;
    gLogicalKeyEntries[SDL_SCANCODE_3].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].rmenu = KEY_179;
    gLogicalKeyEntries[SDL_SCANCODE_3].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_6].unmodified = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_6].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[SDL_SCANCODE_6].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_7].unmodified = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_7].shift = KEY_166;
    gLogicalKeyEntries[SDL_SCANCODE_7].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].rmenu = KEY_BRACE_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_7].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_8].unmodified = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_8].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_8].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_8].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_9].unmodified = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_9].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_9].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_9].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_0].unmodified = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_0].shift = KEY_EQUAL;
    gLogicalKeyEntries[SDL_SCANCODE_0].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].rmenu = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_0].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_MINUS].unmodified = KEY_223;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].shift = KEY_QUESTION;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].rmenu = KEY_BACKSLASH;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].unmodified = KEY_180;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].shift = KEY_GRAVE;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_A;
        break;
    default:
        k = SDL_SCANCODE_Q;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Q;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Q;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Q;
    gLogicalKeyEntries[k].rmenu = KEY_AT;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Q;

    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].unmodified = KEY_252;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].shift = KEY_220;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_EQUALS;
        break;
    default:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_PLUS;
    gLogicalKeyEntries[k].shift = KEY_ASTERISK;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = KEY_152;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].unmodified = KEY_246;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].shift = KEY_214;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].unmodified = KEY_228;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].shift = KEY_196;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].unmodified = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].shift = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    //gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    //gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    //gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    //gLogicalKeyEntries[DIK_OEM_102].rmenu = KEY_166;
    //gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_M;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_M;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_M;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_M;
    gLogicalKeyEntries[k].rmenu = KEY_181;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_M;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_SEMICOLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_150;
    gLogicalKeyEntries[k].shift = KEY_151;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].unmodified = KEY_247;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].shift = KEY_247;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].unmodified = KEY_215;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].shift = KEY_215;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].unmodified = KEY_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].shift = KEY_COMMA;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].rmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].ctrl = KEY_CTRL_DELETE;
}

// 0x4D1758
void keyboardBuildItalianConfiguration()
{
    int k;

    keyboardBuildQwertyConfiguration();

    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].unmodified = KEY_BACKSLASH;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].shift = KEY_BAR;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].ctrl = -1;

    //gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    //gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    //gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    //gLogicalKeyEntries[DIK_OEM_102].rmenu = -1;
    //gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_1].unmodified = KEY_1;
    gLogicalKeyEntries[SDL_SCANCODE_1].shift = KEY_EXCLAMATION;
    gLogicalKeyEntries[SDL_SCANCODE_1].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_2].unmodified = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_2].shift = KEY_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_2].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_3].unmodified = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_3].shift = KEY_163;
    gLogicalKeyEntries[SDL_SCANCODE_3].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_6].unmodified = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_6].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[SDL_SCANCODE_6].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_7].unmodified = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_7].shift = KEY_SLASH;
    gLogicalKeyEntries[SDL_SCANCODE_7].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_8].unmodified = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_8].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_8].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_9].unmodified = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_9].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_9].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_0].unmodified = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_0].shift = KEY_EQUAL;
    gLogicalKeyEntries[SDL_SCANCODE_0].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_MINUS].unmodified = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].shift = KEY_QUESTION;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].unmodified = KEY_232;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].shift = KEY_233;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].unmodified = KEY_PLUS;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].shift = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].unmodified = KEY_249;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].shift = KEY_167;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_SEMICOLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_MINUS;
    gLogicalKeyEntries[k].shift = KEY_UNDERSCORE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;
}

// 0x4D1E24
void keyboardBuildSpanishConfiguration()
{
    int k;

    keyboardBuildQwertyConfiguration();

    gLogicalKeyEntries[SDL_SCANCODE_1].unmodified = KEY_1;
    gLogicalKeyEntries[SDL_SCANCODE_1].shift = KEY_EXCLAMATION;
    gLogicalKeyEntries[SDL_SCANCODE_1].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].rmenu = KEY_BAR;
    gLogicalKeyEntries[SDL_SCANCODE_1].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_2].unmodified = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_2].shift = KEY_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_2].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].rmenu = KEY_AT;
    gLogicalKeyEntries[SDL_SCANCODE_2].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_3].unmodified = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_3].shift = KEY_149;
    gLogicalKeyEntries[SDL_SCANCODE_3].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].rmenu = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[SDL_SCANCODE_3].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_6].unmodified = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_6].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[SDL_SCANCODE_6].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].rmenu = KEY_172;
    gLogicalKeyEntries[SDL_SCANCODE_6].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_7].unmodified = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_7].shift = KEY_SLASH;
    gLogicalKeyEntries[SDL_SCANCODE_7].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_8].unmodified = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_8].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_8].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_9].unmodified = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_9].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_9].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_0].unmodified = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_0].shift = KEY_EQUAL;
    gLogicalKeyEntries[SDL_SCANCODE_0].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_MINUS].unmodified = KEY_146;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].shift = KEY_QUESTION;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].unmodified = KEY_161;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].shift = KEY_191;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].unmodified = KEY_176;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].shift = KEY_170;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].rmenu = KEY_BACKSLASH;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].unmodified = KEY_GRAVE;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].shift = KEY_CARET;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].unmodified = KEY_PLUS;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].shift = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].ctrl = -1;

    //gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    //gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    //gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    //gLogicalKeyEntries[DIK_OEM_102].rmenu = -1;
    //gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].unmodified = KEY_241;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].shift = KEY_209;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].unmodified = KEY_168;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].shift = KEY_180;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].rmenu = KEY_BRACE_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].unmodified = KEY_231;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].shift = KEY_199;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].rmenu = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_SEMICOLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_MINUS;
    gLogicalKeyEntries[k].shift = KEY_UNDERSCORE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;
}

// 0x4D24F8
void _kb_init_lock_status()
{
    if ((SDL_GetModState() & KMOD_CAPS) != 0) {
        gModifierKeysState |= MODIFIER_KEY_STATE_CAPS_LOCK;
    }

    if ((SDL_GetModState() & KMOD_NUM) != 0) {
        gModifierKeysState |= MODIFIER_KEY_STATE_NUM_LOCK;
    }

    if ((SDL_GetModState() & KMOD_SCROLL) != 0) {
        gModifierKeysState |= MODIFIER_KEY_STATE_SCROLL_LOCK;
    }
}

// Get pointer to pending key event from the queue but do not consume it.
//
// 0x4D2614
int keyboardPeekEvent(int index, KeyboardEvent** keyboardEventPtr)
{
    int rc = -1;

    if (gKeyboardEventQueueReadIndex != gKeyboardEventQueueWriteIndex) {
        int end;
        if (gKeyboardEventQueueWriteIndex <= gKeyboardEventQueueReadIndex) {
            end = gKeyboardEventQueueWriteIndex + KEY_QUEUE_SIZE - gKeyboardEventQueueReadIndex - 1;
        } else {
            end = gKeyboardEventQueueWriteIndex - gKeyboardEventQueueReadIndex - 1;
        }

        if (index <= end) {
            int eventIndex = (gKeyboardEventQueueReadIndex + index) & (KEY_QUEUE_SIZE - 1);
            *keyboardEventPtr = &(gKeyboardEventsQueue[eventIndex]);
            rc = 0;
        }
    }

    return rc;
}

// 0x4D2680
bool _vcr_record(const char* fileName)
{
    if (_vcr_state != 2) {
        return false;
    }

    if (fileName == NULL) {
        return false;
    }

    if (_vcr_buffer != NULL) {
        return false;
    }

    _vcr_buffer = (STRUCT_51E2F0*)internal_malloc(sizeof(*_vcr_buffer) * 4096);
    if (_vcr_buffer == NULL) {
        return false;
    }

    _vcr_clear_buffer();

    _vcr_file = fileOpen(fileName, "wb");
    if (_vcr_file == NULL) {
        if (_vcr_buffer != NULL) {
            _vcr_clear_buffer();
            internal_free(_vcr_buffer);
            _vcr_buffer = NULL;
        }
        return false;
    }

    if (_vcr_registered_atexit == 0) {
        _vcr_registered_atexit = atexit(_vcr_stop);
    }

    STRUCT_51E2F0* entry = &(_vcr_buffer[_vcr_buffer_index]);
    entry->type = 1;
    entry->field_4 = 0;
    entry->field_8 = 0;
    entry->type_1_field_14 = keyboardGetLayout();

    while (mouseGetEvent() != 0) {
        _mouse_info();
    }

    mouseGetPosition(&(entry->type_1_field_C), &(entry->type_1_field_10));

    _vcr_counter = 1;
    _vcr_buffer_index++;
    _vcr_start_time = _get_time();
    keyboardReset();
    _vcr_state = 0;
    
    return true;
}

// 0x4D28F4
void _vcr_stop()
{
    if (_vcr_state == 0 || _vcr_state == 1) {
        _vcr_state |= 0x80000000;
    }

    keyboardReset();
}

// 0x4D2918
int _vcr_status()
{
    return _vcr_state;
}

// 0x4D2930
int _vcr_update()
{
    // TODO: Incomplete.
    return 0;
}

// 0x4D2CD0
bool _vcr_clear_buffer()
{
    if (_vcr_buffer == NULL) {
        return false;
    }

    _vcr_buffer_index = 0;

    return true;
}

// 0x4D2CF0
int _vcr_dump_buffer()
{
    if (!_vcr_buffer || !_vcr_file) {
        return 0;
    }

    for (int index = 0; index < _vcr_buffer_index; index++) {
        if (_vcr_save_record(&(_vcr_buffer[index]), _vcr_file)) {
            _vcr_buffer_index = 0;
            return 1;
        }
    }

    return 0;
}

// 0x4D2E00
bool _vcr_save_record(STRUCT_51E2F0* ptr, File* stream)
{
    if (_db_fwriteLong(stream, ptr->type) == -1) goto err;
    if (_db_fwriteLong(stream, ptr->field_4) == -1) goto err;
    if (_db_fwriteLong(stream, ptr->field_8) == -1) goto err;

    switch (ptr->type) {
    case 1:
        if (_db_fwriteLong(stream, ptr->type_1_field_C) == -1) goto err;
        if (_db_fwriteLong(stream, ptr->type_1_field_10) == -1) goto err;
        if (_db_fwriteLong(stream, ptr->type_1_field_14) == -1) goto err;

        return true;
    case 2:
        if (fileWriteInt16(stream, ptr->type_2_field_C) == -1) goto err;

        return true;
    case 3:
        if (_db_fwriteLong(stream, ptr->dx) == -1) goto err;
        if (_db_fwriteLong(stream, ptr->dy) == -1) goto err;
        if (_db_fwriteLong(stream, ptr->buttons) == -1) goto err;

        return true;
    }

err:

    return false;
}

// 0x4D2EE4
bool _vcr_load_record(STRUCT_51E2F0* ptr, File* stream)
{
    if (_db_freadInt(stream, &(ptr->type)) == -1) goto err;
    if (_db_freadInt(stream, &(ptr->field_4)) == -1) goto err;
    if (_db_freadInt(stream, &(ptr->field_8)) == -1) goto err;

    switch (ptr->type) {
    case 1:
        if (_db_freadInt(stream, &(ptr->type_1_field_C)) == -1) goto err;
        if (_db_freadInt(stream, &(ptr->type_1_field_10)) == -1) goto err;
        if (_db_freadInt(stream, &(ptr->type_1_field_14)) == -1) goto err;

        return true;
    case 2:
        if (fileReadInt16(stream, &(ptr->type_2_field_C)) == -1) goto err;

        return true;
    case 3:
        if (_db_freadInt(stream, &(ptr->dx)) == -1) goto err;
        if (_db_freadInt(stream, &(ptr->dy)) == -1) goto err;
        if (_db_freadInt(stream, &(ptr->buttons)) == -1) goto err;

        return true;
    }

err:

    return false;
}

int screenGetWidth()
{
    // TODO: Make it on par with _xres;
    return rectGetWidth(&_scr_size);
}

int screenGetHeight()
{
    // TODO: Make it on par with _yres.
    return rectGetHeight(&_scr_size);
}

int screenGetVisibleHeight()
{
    int windowBottomMargin = 0;

    if (!gInterfaceBarMode) {
        windowBottomMargin = INTERFACE_BAR_HEIGHT;
    }
    return screenGetHeight() - windowBottomMargin;
}

void mouseGetPositionInWindow(int win, int* x, int* y)
{
    mouseGetPosition(x, y);

    Window* window = windowGetWindow(win);
    if (window != NULL) {
        *x -= window->rect.left;
        *y -= window->rect.top;
    }
}

bool mouseHitTestInWindow(int win, int left, int top, int right, int bottom)
{
    Window* window = windowGetWindow(win);
    if (window != NULL) {
        left += window->rect.left;
        top += window->rect.top;
        right += window->rect.left;
        bottom += window->rect.top;
    }

    return _mouse_click_in(left, top, right, bottom);
}
