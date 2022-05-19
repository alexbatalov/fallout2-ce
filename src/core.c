#include "core.h"

#include "color.h"
#include "dinput.h"
#include "draw.h"
#include "memory.h"
#include "mmx.h"
#include "text_font.h"
#include "window_manager.h"
#include "window_manager_private.h"

// NOT USED.
void (*_idle_func)() = NULL;

// NOT USED.
void (*_focus_func)(int) = NULL;

// 0x51E23C
int gKeyboardKeyRepeatRate = 80;

// 0x51E240
int gKeyboardKeyRepeatDelay = 500;

// 0x51E244
bool _keyboard_hooked = false;

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

// 0x51E2B0
LPDIRECTDRAW gDirectDraw = NULL;

// 0x51E2B4
LPDIRECTDRAWSURFACE gDirectDrawSurface1 = NULL;

// 0x51E2B8
LPDIRECTDRAWSURFACE gDirectDrawSurface2 = NULL;

// 0x51E2BC
LPDIRECTDRAWPALETTE gDirectDrawPalette = NULL;

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

// A map of DIK_* constants normalized for QWERTY keyboard.
//
// 0x6ABC70
unsigned char gNormalizedQwertyKeys[256];

// Ring buffer of input events.
//
// Looks like this buffer does not support overwriting of values. Once the
// buffer is full it will not overwrite values until they are dequeued.
//
// 0x6ABD70
InputEvent gInputEventQueue[40];

// 0x6ABF50
STRUCT_6ABF50 _GNW95_key_time_stamps[256];

// 0x6AC750
int _input_mx;

// 0x6AC754
int _input_my;

// 0x6AC758
HHOOK _GNW95_keyboardHandle;

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

// A map of logical key configurations for physical scan codes [DIK_*].
//
// 0x6ACC30
LogicalKeyEntry gLogicalKeyEntries[256];

// A state of physical keys [DIK_*] currently pressed.
//
// 0 - key is not pressed.
// 1 - key pressed.
//
// 0x6AD830
unsigned char gPressedPhysicalKeys[256];

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

    _GNW95_hook_input(1);
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
    _GNW95_hook_keyboard(0);
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

#pragma warning(suppress : 28159)
    gTickerLastTimestamp = GetTickCount();

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

    curr = internal_malloc(sizeof(*curr));
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
    gScreenshotBuffer = internal_malloc(width * height);
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
#pragma warning(suppress : 28159)
    return GetTickCount();
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
#pragma warning(suppress : 28159)
    unsigned int start = GetTickCount();
    unsigned int diff;
    do {
        // NOTE: Uninline
        diff = getTicksSince(start);
    } while (diff < ms);
}

// 0x4C93E0
unsigned int getTicksSince(unsigned int start)
{
#pragma warning(suppress : 28159)
    unsigned int end = GetTickCount();

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
    unsigned char* keys = gNormalizedQwertyKeys;
    int k;

    keys[DIK_ESCAPE] = DIK_ESCAPE;
    keys[DIK_1] = DIK_1;
    keys[DIK_2] = DIK_2;
    keys[DIK_3] = DIK_3;
    keys[DIK_4] = DIK_4;
    keys[DIK_5] = DIK_5;
    keys[DIK_6] = DIK_6;
    keys[DIK_7] = DIK_7;
    keys[DIK_8] = DIK_8;
    keys[DIK_9] = DIK_9;
    keys[DIK_0] = DIK_0;

    switch (gKeyboardLayout) {
    case 0:
        k = DIK_MINUS;
        break;
    case 1:
        k = DIK_6;
        break;
    default:
        k = DIK_SLASH;
        break;
    }
    keys[DIK_MINUS] = k;

    switch (gKeyboardLayout) {
    case 1:
        k = DIK_0;
        break;
    default:
        k = DIK_EQUALS;
        break;
    }
    keys[DIK_EQUALS] = k;

    keys[DIK_BACK] = DIK_BACK;
    keys[DIK_TAB] = DIK_TAB;

    switch (gKeyboardLayout) {
    case 1:
        k = DIK_A;
        break;
    default:
        k = DIK_Q;
        break;
    }
    keys[DIK_Q] = k;

    switch (gKeyboardLayout) {
    case 1:
        k = DIK_Z;
        break;
    default:
        k = DIK_W;
        break;
    }
    keys[DIK_W] = k;

    keys[DIK_E] = DIK_E;
    keys[DIK_R] = DIK_R;
    keys[DIK_T] = DIK_T;

    switch (gKeyboardLayout) {
    case 0:
    case 1:
    case 3:
    case 4:
        k = DIK_Y;
        break;
    default:
        k = DIK_Z;
        break;
    }
    keys[DIK_Y] = k;

    keys[DIK_U] = DIK_U;
    keys[DIK_I] = DIK_I;
    keys[DIK_O] = DIK_O;
    keys[DIK_P] = DIK_P;

    switch (gKeyboardLayout) {
    case 0:
    case 3:
    case 4:
        k = DIK_LBRACKET;
        break;
    case 1:
        k = DIK_5;
        break;
    default:
        k = DIK_8;
        break;
    }
    keys[DIK_LBRACKET] = k;

    switch (gKeyboardLayout) {
    case 0:
    case 3:
    case 4:
        k = DIK_RBRACKET;
        break;
    case 1:
        k = DIK_MINUS;
        break;
    default:
        k = DIK_9;
        break;
    }
    keys[DIK_RBRACKET] = k;

    keys[DIK_RETURN] = DIK_RETURN;
    keys[DIK_LCONTROL] = DIK_LCONTROL;

    switch (gKeyboardLayout) {
    case 1:
        k = DIK_Q;
        break;
    default:
        k = DIK_A;
        break;
    }
    keys[DIK_A] = k;

    keys[DIK_S] = DIK_S;
    keys[DIK_D] = DIK_D;
    keys[DIK_F] = DIK_F;
    keys[DIK_G] = DIK_G;
    keys[DIK_H] = DIK_H;
    keys[DIK_J] = DIK_J;
    keys[DIK_K] = DIK_K;
    keys[DIK_L] = DIK_L;

    switch (gKeyboardLayout) {
    case 0:
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_COMMA;
        break;
    }
    keys[DIK_SEMICOLON] = k;

    switch (gKeyboardLayout) {
    case 0:
        k = DIK_APOSTROPHE;
        break;
    case 1:
        k = DIK_4;
        break;
    default:
        k = DIK_MINUS;
        break;
    }
    keys[DIK_APOSTROPHE] = k;

    switch (gKeyboardLayout) {
    case 0:
        k = DIK_GRAVE;
        break;
    case 1:
        k = DIK_2;
        break;
    case 3:
    case 4:
        k = 0;
        break;
    default:
        k = DIK_RBRACKET;
        break;
    }
    keys[DIK_GRAVE] = k;

    keys[DIK_LSHIFT] = DIK_LSHIFT;

    switch (gKeyboardLayout) {
    case 0:
        k = DIK_BACKSLASH;
        break;
    case 1:
        k = DIK_8;
        break;
    case 3:
    case 4:
        k = DIK_GRAVE;
        break;
    default:
        k = DIK_Y;
        break;
    }
    keys[DIK_BACKSLASH] = k;

    switch (gKeyboardLayout) {
    case 0:
    case 3:
    case 4:
        k = DIK_Z;
        break;
    case 1:
        k = DIK_W;
        break;
    default:
        k = DIK_Y;
        break;
    }
    keys[DIK_Z] = k;

    keys[DIK_X] = DIK_X;
    keys[DIK_C] = DIK_C;
    keys[DIK_V] = DIK_V;
    keys[DIK_B] = DIK_B;
    keys[DIK_N] = DIK_N;

    switch (gKeyboardLayout) {
    case 1:
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_M;
        break;
    }
    keys[DIK_M] = k;

    switch (gKeyboardLayout) {
    case 1:
        k = DIK_M;
        break;
    default:
        k = DIK_COMMA;
        break;
    }
    keys[DIK_COMMA] = k;

    switch (gKeyboardLayout) {
    case 1:
        k = DIK_COMMA;
        break;
    default:
        k = DIK_PERIOD;
        break;
    }
    keys[DIK_PERIOD] = k;

    switch (gKeyboardLayout) {
    case 0:
        k = DIK_SLASH;
        break;
    case 1:
        k = DIK_PERIOD;
        break;
    default:
        k = DIK_7;
        break;
    }
    keys[DIK_SLASH] = k;

    keys[DIK_RSHIFT] = DIK_RSHIFT;
    keys[DIK_MULTIPLY] = DIK_MULTIPLY;
    keys[DIK_SPACE] = DIK_SPACE;
    keys[DIK_LMENU] = DIK_LMENU;
    keys[DIK_CAPITAL] = DIK_CAPITAL;
    keys[DIK_F1] = DIK_F1;
    keys[DIK_F2] = DIK_F2;
    keys[DIK_F3] = DIK_F3;
    keys[DIK_F4] = DIK_F4;
    keys[DIK_F5] = DIK_F5;
    keys[DIK_F6] = DIK_F6;
    keys[DIK_F7] = DIK_F7;
    keys[DIK_F8] = DIK_F8;
    keys[DIK_F9] = DIK_F9;
    keys[DIK_F10] = DIK_F10;
    keys[DIK_NUMLOCK] = DIK_NUMLOCK;
    keys[DIK_SCROLL] = DIK_SCROLL;
    keys[DIK_NUMPAD7] = DIK_NUMPAD7;
    keys[DIK_NUMPAD9] = DIK_NUMPAD9;
    keys[DIK_NUMPAD8] = DIK_NUMPAD8;
    keys[DIK_SUBTRACT] = DIK_SUBTRACT;
    keys[DIK_NUMPAD4] = DIK_NUMPAD4;
    keys[DIK_NUMPAD5] = DIK_NUMPAD5;
    keys[DIK_NUMPAD6] = DIK_NUMPAD6;
    keys[DIK_ADD] = DIK_ADD;
    keys[DIK_NUMPAD1] = DIK_NUMPAD1;
    keys[DIK_NUMPAD2] = DIK_NUMPAD2;
    keys[DIK_NUMPAD3] = DIK_NUMPAD3;
    keys[DIK_NUMPAD0] = DIK_NUMPAD0;
    keys[DIK_DECIMAL] = DIK_DECIMAL;
    keys[DIK_F11] = DIK_F11;
    keys[DIK_F12] = DIK_F12;
    keys[DIK_F13] = -1;
    keys[DIK_F14] = -1;
    keys[DIK_F15] = -1;
    keys[DIK_KANA] = -1;
    keys[DIK_CONVERT] = -1;
    keys[DIK_NOCONVERT] = -1;
    keys[DIK_YEN] = -1;
    keys[DIK_NUMPADEQUALS] = -1;
    keys[DIK_PREVTRACK] = -1;
    keys[DIK_AT] = -1;
    keys[DIK_COLON] = -1;
    keys[DIK_UNDERLINE] = -1;
    keys[DIK_KANJI] = -1;
    keys[DIK_STOP] = -1;
    keys[DIK_AX] = -1;
    keys[DIK_UNLABELED] = -1;
    keys[DIK_NUMPADENTER] = DIK_NUMPADENTER;
    keys[DIK_RCONTROL] = DIK_RCONTROL;
    keys[DIK_NUMPADCOMMA] = -1;
    keys[DIK_DIVIDE] = DIK_DIVIDE;
    keys[DIK_SYSRQ] = 84;
    keys[DIK_RMENU] = DIK_RMENU;
    keys[DIK_HOME] = DIK_HOME;
    keys[DIK_UP] = DIK_UP;
    keys[DIK_PRIOR] = DIK_PRIOR;
    keys[DIK_LEFT] = DIK_LEFT;
    keys[DIK_RIGHT] = DIK_RIGHT;
    keys[DIK_END] = DIK_END;
    keys[DIK_DOWN] = DIK_DOWN;
    keys[DIK_NEXT] = DIK_NEXT;
    keys[DIK_INSERT] = DIK_INSERT;
    keys[DIK_DELETE] = DIK_DELETE;
    keys[DIK_LWIN] = -1;
    keys[DIK_RWIN] = -1;
    keys[DIK_APPS] = -1;
}

// 0x4C9BB4
void _GNW95_hook_input(int a1)
{
    _GNW95_hook_keyboard(a1);

    if (a1) {
        mouseDeviceAcquire();
    } else {
        mouseDeviceUnacquire();
    }
}

// 0x4C9C20
int _GNW95_input_init()
{
    return 0;
}

// 0x4C9C28
int _GNW95_hook_keyboard(int a1)
{
    if (a1 == _keyboard_hooked) {
        return 0;
    }

    if (!a1) {
        keyboardDeviceUnacquire();

        UnhookWindowsHookEx(_GNW95_keyboardHandle);

        keyboardReset();

        _keyboard_hooked = a1;

        return 0;
    }

    if (keyboardDeviceAcquire()) {
        _GNW95_keyboardHandle = SetWindowsHookExA(WH_KEYBOARD, _GNW95_keyboard_hook, 0, GetCurrentThreadId());
        keyboardReset();
        _keyboard_hooked = a1;

        return 0;
    }

    return -1;
}

// 0x4C9C4C
LRESULT CALLBACK _GNW95_keyboard_hook(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0) {
        if (wParam == VK_DELETE && lParam & 0x20000000 && GetAsyncKeyState(VK_CONTROL) & 0x80000000)
            return 0;

        if (wParam == VK_ESCAPE && GetAsyncKeyState(VK_CONTROL) & 0x80000000)
            return 0;

        if (wParam == VK_RETURN && lParam & 0x20000000)
            return 0;

        if (wParam == VK_NUMLOCK || wParam == VK_CAPITAL || wParam == VK_SCROLL) {
            // TODO: Get rid of this goto.
            goto next;
        }

        return 1;
    }

next:

    return CallNextHookEx(_GNW95_keyboardHandle, nCode, wParam, lParam);
}

// 0x4C9CF0
void _GNW95_process_message()
{
    if (gProgramIsActive && !keyboardIsDisabled()) {
        KeyboardData data;
        while (keyboardDeviceGetData(&data)) {
            _GNW95_process_key(&data);
        }

        // NOTE: Uninline
        int tick = _get_time();

        for (int key = 0; key < 256; key++) {
            STRUCT_6ABF50* ptr = &(_GNW95_key_time_stamps[key]);
            if (ptr->tick != -1) {
                int elapsedTime = ptr->tick > tick ? INT_MAX : tick - ptr->tick;
                int delay = ptr->repeatCount == 0 ? gKeyboardKeyRepeatDelay : gKeyboardKeyRepeatRate;
                if (elapsedTime > delay) {
                    data.key = key;
                    data.down = 1;
                    _GNW95_process_key(&data);

                    ptr->tick = tick;
                    ptr->repeatCount++;
                }
            }
        }
    }

    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, 0)) {
        if (GetMessageA(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
}

// 0x4C9DF0
void _GNW95_clear_time_stamps()
{
    for (int index = 0; index < 256; index++) {
        _GNW95_key_time_stamps[index].tick = -1;
        _GNW95_key_time_stamps[index].repeatCount = 0;
    }
}

// 0x4C9E14
void _GNW95_process_key(KeyboardData* data)
{
    short key = data->key & 0xFF;

    switch (key) {
    case DIK_NUMPADENTER:
    case DIK_RCONTROL:
    case DIK_DIVIDE:
    case DIK_RMENU:
    case DIK_HOME:
    case DIK_UP:
    case DIK_PRIOR:
    case DIK_LEFT:
    case DIK_RIGHT:
    case DIK_END:
    case DIK_DOWN:
    case DIK_NEXT:
    case DIK_INSERT:
    case DIK_DELETE:
        key |= 0x0100;
        break;
    }

    int qwertyKey = gNormalizedQwertyKeys[data->key & 0xFF];

    if (_vcr_state == 1) {
        if (_vcr_terminate_flags & 1) {
            _vcr_terminated_condition = 2;
            _vcr_stop();
        }
    } else {
        if ((key & 0x0100) != 0) {
            _kb_simulate_key(224);
            qwertyKey -= 0x80;
        }

        STRUCT_6ABF50* ptr = &(_GNW95_key_time_stamps[data->key & 0xFF]);
        if (data->down == 1) {
            ptr->tick = _get_time();
            ptr->repeatCount = 0;
        } else {
            qwertyKey |= 0x80;
            ptr->tick = -1;
        }

        _kb_simulate_key(qwertyKey);
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
        unsigned char* buf = internal_malloc(width * height);
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
    if (_GNW95_init_window() == -1) {
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
int _GNW95_init_window()
{
    if (gProgramWindow == NULL) {
        int width = GetSystemMetrics(SM_CXSCREEN);
        int height = GetSystemMetrics(SM_CYSCREEN);

        gProgramWindow = CreateWindowExA(WS_EX_TOPMOST, "GNW95 Class", gProgramWindowTitle, WS_POPUP | WS_VISIBLE | WS_SYSMENU, 0, 0, width, height, NULL, NULL, gInstance, NULL);
        if (gProgramWindow == NULL) {
            return -1;
        }

        UpdateWindow(gProgramWindow);
        SetFocus(gProgramWindow);
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
    if (gDirectDraw != NULL) {
        unsigned char* palette = directDrawGetPalette();
        directDrawFree();

        if (directDrawInit(width, height, bpp) == -1) {
            return -1;
        }

        directDrawSetPalette(palette);

        return 0;
    }

    if (gDirectDrawCreateProc(NULL, &gDirectDraw, NULL) != DD_OK) {
        return -1;
    }

    if (IDirectDraw_SetCooperativeLevel(gDirectDraw, gProgramWindow, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN) != DD_OK) {
        return -1;
    }

    if (IDirectDraw_SetDisplayMode(gDirectDraw, width, height, bpp) != DD_OK) {
        return -1;
    }

    DDSURFACEDESC ddsd;
    memset(&ddsd, 0, sizeof(DDSURFACEDESC));

    ddsd.dwSize = sizeof(DDSURFACEDESC);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

    if (IDirectDraw_CreateSurface(gDirectDraw, &ddsd, &gDirectDrawSurface1, NULL) != DD_OK) {
        return -1;
    }

    gDirectDrawSurface2 = gDirectDrawSurface1;

    if (bpp == 8) {
        PALETTEENTRY pe[256];
        for (int index = 0; index < 256; index++) {
            pe[index].peRed = index;
            pe[index].peGreen = index;
            pe[index].peBlue = index;
            pe[index].peFlags = 0;
        }

        if (IDirectDraw_CreatePalette(gDirectDraw, DDPCAPS_8BIT | DDPCAPS_ALLOW256, pe, &gDirectDrawPalette, NULL) != DD_OK) {
            return -1;
        }

        if (IDirectDrawSurface_SetPalette(gDirectDrawSurface1, gDirectDrawPalette) != DD_OK) {
            return -1;
        }

        return 0;
    } else {
        DDPIXELFORMAT ddpf;
        ddpf.dwSize = sizeof(DDPIXELFORMAT);

        if (IDirectDrawSurface_GetPixelFormat(gDirectDrawSurface1, &ddpf) != DD_OK) {
            return -1;
        }

        gRedMask = ddpf.dwRBitMask;
        gGreenMask = ddpf.dwGBitMask;
        gBlueMask = ddpf.dwBBitMask;

        gRedShift = getShiftForBitMask(gRedMask) - 7;
        gGreenShift = getShiftForBitMask(gGreenMask) - 7;
        gBlueShift = getShiftForBitMask(gBlueMask) - 7;

        return 0;
    }
}

// 0x4CB1B0
void directDrawFree()
{
    if (gDirectDraw != NULL) {
        IDirectDraw_RestoreDisplayMode(gDirectDraw);

        if (gDirectDrawSurface1 != NULL) {
            IDirectDrawSurface_Release(gDirectDrawSurface1);
            gDirectDrawSurface1 = NULL;
            gDirectDrawSurface2 = NULL;
        }

        if (gDirectDrawPalette != NULL) {
            IDirectDrawPalette_Release(gDirectDrawPalette);
            gDirectDrawPalette = NULL;
        }

        IDirectDraw_Release(gDirectDraw);
        gDirectDraw = NULL;
    }
}

// 0x4CB310
void directDrawSetPaletteInRange(unsigned char* palette, int start, int count)
{
    if (gDirectDrawPalette != NULL) {
        PALETTEENTRY entries[256];

        if (count != 0) {
            for (int index = 0; index < count; index++) {
                entries[index].peRed = palette[index * 3] << 2;
                entries[index].peGreen = palette[index * 3 + 1] << 2;
                entries[index].peBlue = palette[index * 3 + 2] << 2;
                entries[index].peFlags = PC_NOCOLLAPSE;
            }
        }

        IDirectDrawPalette_SetEntries(gDirectDrawPalette, 0, start, count, entries);
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
    if (gDirectDrawPalette != NULL) {
        PALETTEENTRY entries[256];

        for (int index = 0; index < 256; index++) {
            entries[index].peRed = palette[index * 3] << 2;
            entries[index].peGreen = palette[index * 3 + 1] << 2;
            entries[index].peBlue = palette[index * 3 + 2] << 2;
            entries[index].peFlags = PC_NOCOLLAPSE;
        }

        IDirectDrawPalette_SetEntries(gDirectDrawPalette, 0, 0, 256, entries);
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
    if (gDirectDrawPalette != NULL) {
        PALETTEENTRY paletteEntries[256];
        if (IDirectDrawPalette_GetEntries(gDirectDrawPalette, 0, 0, 256, paletteEntries) != DD_OK) {
            return NULL;
        }

        for (int index = 0; index < 256; index++) {
            PALETTEENTRY* paletteEntry = &(paletteEntries[index]);
            gLastVideoModePalette[index * 3] = paletteEntry->peRed >> 2;
            gLastVideoModePalette[index * 3 + 1] = paletteEntry->peGreen >> 2;
            gLastVideoModePalette[index * 3 + 2] = paletteEntry->peBlue >> 2;
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
    DDSURFACEDESC ddsd;
    HRESULT hr;

    if (!gProgramIsActive) {
        return;
    }

    while (1) {
        ddsd.dwSize = sizeof(DDSURFACEDESC);

        hr = IDirectDrawSurface_Lock(gDirectDrawSurface1, NULL, &ddsd, 1, NULL);
        if (hr == DD_OK) {
            break;
        }

        if (hr == DDERR_SURFACELOST) {
            if (IDirectDrawSurface_Restore(gDirectDrawSurface2) != DD_OK) {
                return;
            }
        }
    }

    blitBufferToBuffer(src + srcPitch * srcY + srcX, srcWidth, srcHeight, srcPitch, (unsigned char*)ddsd.lpSurface + ddsd.lPitch * destY + destX, ddsd.lPitch);

    IDirectDrawSurface_Unlock(gDirectDrawSurface1, ddsd.lpSurface);
}

// 0x4CB93C
void _GNW95_MouseShowRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    DDSURFACEDESC ddsd;
    HRESULT hr;

    if (!gProgramIsActive) {
        return;
    }

    while (1) {
        ddsd.dwSize = sizeof(ddsd);

        hr = IDirectDrawSurface_Lock(gDirectDrawSurface1, NULL, &ddsd, 1, NULL);
        if (hr == DD_OK) {
            break;
        }

        if (hr == DDERR_SURFACELOST) {
            if (IDirectDrawSurface_Restore(gDirectDrawSurface2) != DD_OK) {
                return;
            }
        }
    }

    unsigned char* dest = (unsigned char*)ddsd.lpSurface + ddsd.lPitch * destY + 2 * destX;

    src += srcPitch * srcY + srcX;

    for (int y = 0; y < srcHeight; y++) {
        unsigned short* destPtr = (unsigned short*)dest;
        unsigned char* srcPtr = src;
        for (int x = 0; x < srcWidth; x++) {
            *destPtr = gSixteenBppPalette[*srcPtr];
            destPtr++;
            srcPtr++;
        }

        dest += ddsd.lPitch;
        src += srcPitch;
    }

    IDirectDrawSurface_Unlock(gDirectDrawSurface1, ddsd.lpSurface);
}

// 0x4CBA44
void _GNW95_ShowRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    _GNW95_MouseShowRect16(src, srcPitch, a3, srcX, srcY, srcWidth, srcHeight, destX, destY);
}

// 0x4CBAB0
void _GNW95_MouseShowTransRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY, unsigned char keyColor)
{
    DDSURFACEDESC ddsd;
    HRESULT hr;

    if (!gProgramIsActive) {
        return;
    }

    while (1) {
        ddsd.dwSize = sizeof(ddsd);

        hr = IDirectDrawSurface_Lock(gDirectDrawSurface1, NULL, &ddsd, 1, NULL);
        if (hr == DD_OK) {
            break;
        }

        if (hr == DDERR_SURFACELOST) {
            if (IDirectDrawSurface_Restore(gDirectDrawSurface2) != DD_OK) {
                return;
            }
        }
    }

    unsigned char* dest = (unsigned char*)ddsd.lpSurface + ddsd.lPitch * destY + 2 * destX;

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

        dest += ddsd.lPitch;
        src += srcPitch;
    }

    IDirectDrawSurface_Unlock(gDirectDrawSurface1, ddsd.lpSurface);
}

// Clears drawing surface.
//
// 0x4CBBC8
void _GNW95_zero_vid_mem()
{
    DDSURFACEDESC ddsd;
    HRESULT hr;
    unsigned char* surface;
    
    if (!gProgramIsActive) {
        return;
    }

    while (1) {
        ddsd.dwSize = sizeof(DDSURFACEDESC);

        hr = IDirectDrawSurface_Lock(gDirectDrawSurface1, NULL, &ddsd, 1, NULL);
        if (hr == DD_OK) {
            break;
        }

        if (hr == DDERR_SURFACELOST) {
            if (IDirectDrawSurface_Restore(gDirectDrawSurface2) != DD_OK) {
                return;
            }
        }
    }

    surface = (unsigned char*)ddsd.lpSurface;
    for (unsigned int y = 0; y < ddsd.dwHeight; y++) {
        memset(surface, 0, ddsd.dwWidth);
        surface += ddsd.lPitch;
    }

    IDirectDrawSurface_Unlock(gDirectDrawSurface1, ddsd.lpSurface);
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
void _kb_simulate_key(int key)
{
    if (_vcr_state == 0) {
        if (_vcr_buffer_index != 4095) {
            STRUCT_51E2F0* ptr = &(_vcr_buffer[_vcr_buffer_index]);
            ptr->type = 2;
            ptr->type_2_field_C = key & 0xFFFF;
            ptr->field_4 = _vcr_time;
            ptr->field_8 = _vcr_counter;
            _vcr_buffer_index++;
        }
    }

    _kb_idle_start_time = _get_bk_time();

    if (key == 224) {
        word_51E2E8 = 0x80;
    } else {
        int keyState;
        if (key & 0x80) {
            key &= ~0x80;
            keyState = KEY_STATE_UP;
        } else {
            keyState = KEY_STATE_DOWN;
        }

        int physicalKey = key | word_51E2E8;

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
            gLastKeyboardEvent.scanCode = physicalKey & 0xFF;
            gLastKeyboardEvent.modifiers = 0;

            if (physicalKey == DIK_CAPITAL) {
                if (gPressedPhysicalKeys[DIK_LCONTROL] == KEY_STATE_UP && gPressedPhysicalKeys[DIK_RCONTROL] == KEY_STATE_UP) {
                    // TODO: Missing check for QWERTY keyboard layout.
                    if ((gModifierKeysState & MODIFIER_KEY_STATE_CAPS_LOCK) != 0) {
                        // TODO: There is some strange code checking for _kb_layout, check in
                        // debugger.
                        gModifierKeysState &= ~MODIFIER_KEY_STATE_CAPS_LOCK;
                    } else {
                        gModifierKeysState |= MODIFIER_KEY_STATE_CAPS_LOCK;
                    }
                }
            } else if (physicalKey == DIK_NUMLOCK) {
                if (gPressedPhysicalKeys[DIK_LCONTROL] == KEY_STATE_UP && gPressedPhysicalKeys[DIK_RCONTROL] == KEY_STATE_UP) {
                    if ((gModifierKeysState & MODIFIER_KEY_STATE_NUM_LOCK) != 0) {
                        gModifierKeysState &= ~MODIFIER_KEY_STATE_NUM_LOCK;
                    } else {
                        gModifierKeysState |= MODIFIER_KEY_STATE_NUM_LOCK;
                    }
                }
            } else if (physicalKey == DIK_SCROLL) {
                if (gPressedPhysicalKeys[DIK_LCONTROL] == KEY_STATE_UP && gPressedPhysicalKeys[DIK_RCONTROL] == KEY_STATE_UP) {
                    if ((gModifierKeysState & MODIFIER_KEY_STATE_SCROLL_LOCK) != 0) {
                        gModifierKeysState &= ~MODIFIER_KEY_STATE_SCROLL_LOCK;
                    } else {
                        gModifierKeysState |= MODIFIER_KEY_STATE_SCROLL_LOCK;
                    }
                }
            } else if ((physicalKey == DIK_LSHIFT || physicalKey == DIK_RSHIFT) && (gModifierKeysState & MODIFIER_KEY_STATE_CAPS_LOCK) != 0 && gKeyboardLayout != 0) {
                if (gPressedPhysicalKeys[DIK_LCONTROL] == KEY_STATE_UP && gPressedPhysicalKeys[DIK_RCONTROL] == KEY_STATE_UP) {
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

            if (gPressedPhysicalKeys[DIK_LSHIFT] != KEY_STATE_UP) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
            }

            if (gPressedPhysicalKeys[DIK_RSHIFT] != KEY_STATE_UP) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_SHIFT;
            }

            if (gPressedPhysicalKeys[DIK_LMENU] != KEY_STATE_UP) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_ALT;
            }

            if (gPressedPhysicalKeys[DIK_RMENU] != KEY_STATE_UP) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_ALT;
            }

            if (gPressedPhysicalKeys[DIK_LCONTROL] != KEY_STATE_UP) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_CONTROL;
            }

            if (gPressedPhysicalKeys[DIK_RCONTROL] != KEY_STATE_UP) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_CONTROL;
            }

            if (((gKeyboardEventQueueWriteIndex + 1) & 0x3F) != gKeyboardEventQueueReadIndex) {
                gKeyboardEventsQueue[gKeyboardEventQueueWriteIndex] = gLastKeyboardEvent;
                gKeyboardEventQueueWriteIndex++;
                gKeyboardEventQueueWriteIndex &= 0x3F;
            }
        }

        word_51E2E8 = 0;
    }

    if (gPressedPhysicalKeys[198] != KEY_STATE_UP) {
        // NOTE: Uninline
        keyboardReset();
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
        unsigned char a = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? DIK_A : DIK_Q);
        unsigned char m = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? DIK_M : DIK_SEMICOLON);
        unsigned char q = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? DIK_Q : DIK_A);
        unsigned char w = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? DIK_W : DIK_Z);

        unsigned char y;
        switch (gKeyboardLayout) {
        case KEYBOARD_LAYOUT_QWERTY:
        case KEYBOARD_LAYOUT_FRENCH:
        case KEYBOARD_LAYOUT_ITALIAN:
        case KEYBOARD_LAYOUT_SPANISH:
            y = DIK_Y;
            break;
        default:
            // GERMAN
            y = DIK_Z;
            break;
        }

        unsigned char z;
        switch (gKeyboardLayout) {
        case KEYBOARD_LAYOUT_QWERTY:
        case KEYBOARD_LAYOUT_ITALIAN:
        case KEYBOARD_LAYOUT_SPANISH:
            z = DIK_Z;
            break;
        case KEYBOARD_LAYOUT_FRENCH:
            z = DIK_W;
            break;
        default:
            // GERMAN
            z = DIK_Y;
            break;
        }

        unsigned char scanCode = keyboardEvent->scanCode;
        if (scanCode == a
            || scanCode == DIK_B
            || scanCode == DIK_C
            || scanCode == DIK_D
            || scanCode == DIK_E
            || scanCode == DIK_F
            || scanCode == DIK_G
            || scanCode == DIK_H
            || scanCode == DIK_I
            || scanCode == DIK_J
            || scanCode == DIK_K
            || scanCode == DIK_L
            || scanCode == m
            || scanCode == DIK_N
            || scanCode == DIK_O
            || scanCode == DIK_P
            || scanCode == q
            || scanCode == DIK_R
            || scanCode == DIK_S
            || scanCode == DIK_T
            || scanCode == DIK_U
            || scanCode == DIK_V
            || scanCode == w
            || scanCode == DIK_X
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
    case DIK_DIVIDE:
    case DIK_MULTIPLY:
    case DIK_SUBTRACT:
    case DIK_ADD:
    case DIK_NUMPADENTER:
        if (gKeyboardNumpadDisabled) {
            if (gKeyboardEventQueueReadIndex != gKeyboardEventQueueWriteIndex) {
                gKeyboardEventQueueReadIndex++;
                gKeyboardEventQueueReadIndex &= (KEY_QUEUE_SIZE - 1);
            }
            return -1;
        }
        break;
    case DIK_NUMPAD0:
    case DIK_NUMPAD1:
    case DIK_NUMPAD2:
    case DIK_NUMPAD3:
    case DIK_NUMPAD4:
    case DIK_NUMPAD5:
    case DIK_NUMPAD6:
    case DIK_NUMPAD7:
    case DIK_NUMPAD8:
    case DIK_NUMPAD9:
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

    for (k = 0; k < 256; k++) {
        gLogicalKeyEntries[k].field_0 = -1;
        gLogicalKeyEntries[k].unmodified = -1;
        gLogicalKeyEntries[k].shift = -1;
        gLogicalKeyEntries[k].lmenu = -1;
        gLogicalKeyEntries[k].rmenu = -1;
        gLogicalKeyEntries[k].ctrl = -1;
    }

    gLogicalKeyEntries[DIK_ESCAPE].unmodified = KEY_ESCAPE;
    gLogicalKeyEntries[DIK_ESCAPE].shift = KEY_ESCAPE;
    gLogicalKeyEntries[DIK_ESCAPE].lmenu = KEY_ESCAPE;
    gLogicalKeyEntries[DIK_ESCAPE].rmenu = KEY_ESCAPE;
    gLogicalKeyEntries[DIK_ESCAPE].ctrl = KEY_ESCAPE;

    gLogicalKeyEntries[DIK_F1].unmodified = KEY_F1;
    gLogicalKeyEntries[DIK_F1].shift = KEY_SHIFT_F1;
    gLogicalKeyEntries[DIK_F1].lmenu = KEY_ALT_F1;
    gLogicalKeyEntries[DIK_F1].rmenu = KEY_ALT_F1;
    gLogicalKeyEntries[DIK_F1].ctrl = KEY_CTRL_F1;

    gLogicalKeyEntries[DIK_F2].unmodified = KEY_F2;
    gLogicalKeyEntries[DIK_F2].shift = KEY_SHIFT_F2;
    gLogicalKeyEntries[DIK_F2].lmenu = KEY_ALT_F2;
    gLogicalKeyEntries[DIK_F2].rmenu = KEY_ALT_F2;
    gLogicalKeyEntries[DIK_F2].ctrl = KEY_CTRL_F2;

    gLogicalKeyEntries[DIK_F3].unmodified = KEY_F3;
    gLogicalKeyEntries[DIK_F3].shift = KEY_SHIFT_F3;
    gLogicalKeyEntries[DIK_F3].lmenu = KEY_ALT_F3;
    gLogicalKeyEntries[DIK_F3].rmenu = KEY_ALT_F3;
    gLogicalKeyEntries[DIK_F3].ctrl = KEY_CTRL_F3;

    gLogicalKeyEntries[DIK_F4].unmodified = KEY_F4;
    gLogicalKeyEntries[DIK_F4].shift = KEY_SHIFT_F4;
    gLogicalKeyEntries[DIK_F4].lmenu = KEY_ALT_F4;
    gLogicalKeyEntries[DIK_F4].rmenu = KEY_ALT_F4;
    gLogicalKeyEntries[DIK_F4].ctrl = KEY_CTRL_F4;

    gLogicalKeyEntries[DIK_F5].unmodified = KEY_F5;
    gLogicalKeyEntries[DIK_F5].shift = KEY_SHIFT_F5;
    gLogicalKeyEntries[DIK_F5].lmenu = KEY_ALT_F5;
    gLogicalKeyEntries[DIK_F5].rmenu = KEY_ALT_F5;
    gLogicalKeyEntries[DIK_F5].ctrl = KEY_CTRL_F5;

    gLogicalKeyEntries[DIK_F6].unmodified = KEY_F6;
    gLogicalKeyEntries[DIK_F6].shift = KEY_SHIFT_F6;
    gLogicalKeyEntries[DIK_F6].lmenu = KEY_ALT_F6;
    gLogicalKeyEntries[DIK_F6].rmenu = KEY_ALT_F6;
    gLogicalKeyEntries[DIK_F6].ctrl = KEY_CTRL_F6;

    gLogicalKeyEntries[DIK_F7].unmodified = KEY_F7;
    gLogicalKeyEntries[DIK_F7].shift = KEY_SHIFT_F7;
    gLogicalKeyEntries[DIK_F7].lmenu = KEY_ALT_F7;
    gLogicalKeyEntries[DIK_F7].rmenu = KEY_ALT_F7;
    gLogicalKeyEntries[DIK_F7].ctrl = KEY_CTRL_F7;

    gLogicalKeyEntries[DIK_F8].unmodified = KEY_F8;
    gLogicalKeyEntries[DIK_F8].shift = KEY_SHIFT_F8;
    gLogicalKeyEntries[DIK_F8].lmenu = KEY_ALT_F8;
    gLogicalKeyEntries[DIK_F8].rmenu = KEY_ALT_F8;
    gLogicalKeyEntries[DIK_F8].ctrl = KEY_CTRL_F8;

    gLogicalKeyEntries[DIK_F9].unmodified = KEY_F9;
    gLogicalKeyEntries[DIK_F9].shift = KEY_SHIFT_F9;
    gLogicalKeyEntries[DIK_F9].lmenu = KEY_ALT_F9;
    gLogicalKeyEntries[DIK_F9].rmenu = KEY_ALT_F9;
    gLogicalKeyEntries[DIK_F9].ctrl = KEY_CTRL_F9;

    gLogicalKeyEntries[DIK_F10].unmodified = KEY_F10;
    gLogicalKeyEntries[DIK_F10].shift = KEY_SHIFT_F10;
    gLogicalKeyEntries[DIK_F10].lmenu = KEY_ALT_F10;
    gLogicalKeyEntries[DIK_F10].rmenu = KEY_ALT_F10;
    gLogicalKeyEntries[DIK_F10].ctrl = KEY_CTRL_F10;

    gLogicalKeyEntries[DIK_F11].unmodified = KEY_F11;
    gLogicalKeyEntries[DIK_F11].shift = KEY_SHIFT_F11;
    gLogicalKeyEntries[DIK_F11].lmenu = KEY_ALT_F11;
    gLogicalKeyEntries[DIK_F11].rmenu = KEY_ALT_F11;
    gLogicalKeyEntries[DIK_F11].ctrl = KEY_CTRL_F11;

    gLogicalKeyEntries[DIK_F12].unmodified = KEY_F12;
    gLogicalKeyEntries[DIK_F12].shift = KEY_SHIFT_F12;
    gLogicalKeyEntries[DIK_F12].lmenu = KEY_ALT_F12;
    gLogicalKeyEntries[DIK_F12].rmenu = KEY_ALT_F12;
    gLogicalKeyEntries[DIK_F12].ctrl = KEY_CTRL_F12;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = DIK_GRAVE;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_2;
        break;
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = 0;
        break;
    default:
        k = DIK_RBRACKET;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_GRAVE;
    gLogicalKeyEntries[k].shift = KEY_TILDE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[DIK_1].unmodified = KEY_1;
    gLogicalKeyEntries[DIK_1].shift = KEY_EXCLAMATION;
    gLogicalKeyEntries[DIK_1].lmenu = -1;
    gLogicalKeyEntries[DIK_1].rmenu = -1;
    gLogicalKeyEntries[DIK_1].ctrl = -1;

    gLogicalKeyEntries[DIK_2].unmodified = KEY_2;
    gLogicalKeyEntries[DIK_2].shift = KEY_AT;
    gLogicalKeyEntries[DIK_2].lmenu = -1;
    gLogicalKeyEntries[DIK_2].rmenu = -1;
    gLogicalKeyEntries[DIK_2].ctrl = -1;

    gLogicalKeyEntries[DIK_3].unmodified = KEY_3;
    gLogicalKeyEntries[DIK_3].shift = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[DIK_3].lmenu = -1;
    gLogicalKeyEntries[DIK_3].rmenu = -1;
    gLogicalKeyEntries[DIK_3].ctrl = -1;

    gLogicalKeyEntries[DIK_4].unmodified = KEY_4;
    gLogicalKeyEntries[DIK_4].shift = KEY_DOLLAR;
    gLogicalKeyEntries[DIK_4].lmenu = -1;
    gLogicalKeyEntries[DIK_4].rmenu = -1;
    gLogicalKeyEntries[DIK_4].ctrl = -1;

    gLogicalKeyEntries[DIK_5].unmodified = KEY_5;
    gLogicalKeyEntries[DIK_5].shift = KEY_PERCENT;
    gLogicalKeyEntries[DIK_5].lmenu = -1;
    gLogicalKeyEntries[DIK_5].rmenu = -1;
    gLogicalKeyEntries[DIK_5].ctrl = -1;

    gLogicalKeyEntries[DIK_6].unmodified = KEY_6;
    gLogicalKeyEntries[DIK_6].shift = KEY_CARET;
    gLogicalKeyEntries[DIK_6].lmenu = -1;
    gLogicalKeyEntries[DIK_6].rmenu = -1;
    gLogicalKeyEntries[DIK_6].ctrl = -1;

    gLogicalKeyEntries[DIK_7].unmodified = KEY_7;
    gLogicalKeyEntries[DIK_7].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[DIK_7].lmenu = -1;
    gLogicalKeyEntries[DIK_7].rmenu = -1;
    gLogicalKeyEntries[DIK_7].ctrl = -1;

    gLogicalKeyEntries[DIK_8].unmodified = KEY_8;
    gLogicalKeyEntries[DIK_8].shift = KEY_ASTERISK;
    gLogicalKeyEntries[DIK_8].lmenu = -1;
    gLogicalKeyEntries[DIK_8].rmenu = -1;
    gLogicalKeyEntries[DIK_8].ctrl = -1;

    gLogicalKeyEntries[DIK_9].unmodified = KEY_9;
    gLogicalKeyEntries[DIK_9].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[DIK_9].lmenu = -1;
    gLogicalKeyEntries[DIK_9].rmenu = -1;
    gLogicalKeyEntries[DIK_9].ctrl = -1;

    gLogicalKeyEntries[DIK_0].unmodified = KEY_0;
    gLogicalKeyEntries[DIK_0].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[DIK_0].lmenu = -1;
    gLogicalKeyEntries[DIK_0].rmenu = -1;
    gLogicalKeyEntries[DIK_0].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = DIK_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_6;
        break;
    default:
        k = DIK_SLASH;
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
        k = DIK_EQUALS;
        break;
    default:
        k = DIK_0;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_EQUAL;
    gLogicalKeyEntries[k].shift = KEY_PLUS;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[DIK_BACK].unmodified = KEY_BACKSPACE;
    gLogicalKeyEntries[DIK_BACK].shift = KEY_BACKSPACE;
    gLogicalKeyEntries[DIK_BACK].lmenu = KEY_BACKSPACE;
    gLogicalKeyEntries[DIK_BACK].rmenu = KEY_BACKSPACE;
    gLogicalKeyEntries[DIK_BACK].ctrl = KEY_DEL;

    gLogicalKeyEntries[DIK_TAB].unmodified = KEY_TAB;
    gLogicalKeyEntries[DIK_TAB].shift = KEY_TAB;
    gLogicalKeyEntries[DIK_TAB].lmenu = KEY_TAB;
    gLogicalKeyEntries[DIK_TAB].rmenu = KEY_TAB;
    gLogicalKeyEntries[DIK_TAB].ctrl = KEY_TAB;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_A;
        break;
    default:
        k = DIK_Q;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Q;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Q;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Q;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_Q;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Q;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_Z;
        break;
    default:
        k = DIK_W;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_W;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_W;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_W;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_W;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_W;

    gLogicalKeyEntries[DIK_E].unmodified = KEY_LOWERCASE_E;
    gLogicalKeyEntries[DIK_E].shift = KEY_UPPERCASE_E;
    gLogicalKeyEntries[DIK_E].lmenu = KEY_ALT_E;
    gLogicalKeyEntries[DIK_E].rmenu = KEY_ALT_E;
    gLogicalKeyEntries[DIK_E].ctrl = KEY_CTRL_E;

    gLogicalKeyEntries[DIK_R].unmodified = KEY_LOWERCASE_R;
    gLogicalKeyEntries[DIK_R].shift = KEY_UPPERCASE_R;
    gLogicalKeyEntries[DIK_R].lmenu = KEY_ALT_R;
    gLogicalKeyEntries[DIK_R].rmenu = KEY_ALT_R;
    gLogicalKeyEntries[DIK_R].ctrl = KEY_CTRL_R;

    gLogicalKeyEntries[DIK_T].unmodified = KEY_LOWERCASE_T;
    gLogicalKeyEntries[DIK_T].shift = KEY_UPPERCASE_T;
    gLogicalKeyEntries[DIK_T].lmenu = KEY_ALT_T;
    gLogicalKeyEntries[DIK_T].rmenu = KEY_ALT_T;
    gLogicalKeyEntries[DIK_T].ctrl = KEY_CTRL_T;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = DIK_Y;
        break;
    default:
        k = DIK_Z;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Y;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Y;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Y;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_Y;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Y;

    gLogicalKeyEntries[DIK_U].unmodified = KEY_LOWERCASE_U;
    gLogicalKeyEntries[DIK_U].shift = KEY_UPPERCASE_U;
    gLogicalKeyEntries[DIK_U].lmenu = KEY_ALT_U;
    gLogicalKeyEntries[DIK_U].rmenu = KEY_ALT_U;
    gLogicalKeyEntries[DIK_U].ctrl = KEY_CTRL_U;

    gLogicalKeyEntries[DIK_I].unmodified = KEY_LOWERCASE_I;
    gLogicalKeyEntries[DIK_I].shift = KEY_UPPERCASE_I;
    gLogicalKeyEntries[DIK_I].lmenu = KEY_ALT_I;
    gLogicalKeyEntries[DIK_I].rmenu = KEY_ALT_I;
    gLogicalKeyEntries[DIK_I].ctrl = KEY_CTRL_I;

    gLogicalKeyEntries[DIK_O].unmodified = KEY_LOWERCASE_O;
    gLogicalKeyEntries[DIK_O].shift = KEY_UPPERCASE_O;
    gLogicalKeyEntries[DIK_O].lmenu = KEY_ALT_O;
    gLogicalKeyEntries[DIK_O].rmenu = KEY_ALT_O;
    gLogicalKeyEntries[DIK_O].ctrl = KEY_CTRL_O;

    gLogicalKeyEntries[DIK_P].unmodified = KEY_LOWERCASE_P;
    gLogicalKeyEntries[DIK_P].shift = KEY_UPPERCASE_P;
    gLogicalKeyEntries[DIK_P].lmenu = KEY_ALT_P;
    gLogicalKeyEntries[DIK_P].rmenu = KEY_ALT_P;
    gLogicalKeyEntries[DIK_P].ctrl = KEY_CTRL_P;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = DIK_LBRACKET;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_5;
        break;
    default:
        k = DIK_8;
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
        k = DIK_RBRACKET;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_MINUS;
        break;
    default:
        k = DIK_9;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[k].shift = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = DIK_BACKSLASH;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_8;
        break;
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = DIK_GRAVE;
        break;
    default:
        k = DIK_MINUS;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_BACKSLASH;
    gLogicalKeyEntries[k].shift = KEY_BAR;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_BACKSLASH;

    gLogicalKeyEntries[DIK_CAPITAL].unmodified = -1;
    gLogicalKeyEntries[DIK_CAPITAL].shift = -1;
    gLogicalKeyEntries[DIK_CAPITAL].lmenu = -1;
    gLogicalKeyEntries[DIK_CAPITAL].rmenu = -1;
    gLogicalKeyEntries[DIK_CAPITAL].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_Q;
        break;
    default:
        k = DIK_A;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_A;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_A;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_A;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_A;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_A;

    gLogicalKeyEntries[DIK_S].unmodified = KEY_LOWERCASE_S;
    gLogicalKeyEntries[DIK_S].shift = KEY_UPPERCASE_S;
    gLogicalKeyEntries[DIK_S].lmenu = KEY_ALT_S;
    gLogicalKeyEntries[DIK_S].rmenu = KEY_ALT_S;
    gLogicalKeyEntries[DIK_S].ctrl = KEY_CTRL_S;

    gLogicalKeyEntries[DIK_D].unmodified = KEY_LOWERCASE_D;
    gLogicalKeyEntries[DIK_D].shift = KEY_UPPERCASE_D;
    gLogicalKeyEntries[DIK_D].lmenu = KEY_ALT_D;
    gLogicalKeyEntries[DIK_D].rmenu = KEY_ALT_D;
    gLogicalKeyEntries[DIK_D].ctrl = KEY_CTRL_D;

    gLogicalKeyEntries[DIK_F].unmodified = KEY_LOWERCASE_F;
    gLogicalKeyEntries[DIK_F].shift = KEY_UPPERCASE_F;
    gLogicalKeyEntries[DIK_F].lmenu = KEY_ALT_F;
    gLogicalKeyEntries[DIK_F].rmenu = KEY_ALT_F;
    gLogicalKeyEntries[DIK_F].ctrl = KEY_CTRL_F;

    gLogicalKeyEntries[DIK_G].unmodified = KEY_LOWERCASE_G;
    gLogicalKeyEntries[DIK_G].shift = KEY_UPPERCASE_G;
    gLogicalKeyEntries[DIK_G].lmenu = KEY_ALT_G;
    gLogicalKeyEntries[DIK_G].rmenu = KEY_ALT_G;
    gLogicalKeyEntries[DIK_G].ctrl = KEY_CTRL_G;

    gLogicalKeyEntries[DIK_H].unmodified = KEY_LOWERCASE_H;
    gLogicalKeyEntries[DIK_H].shift = KEY_UPPERCASE_H;
    gLogicalKeyEntries[DIK_H].lmenu = KEY_ALT_H;
    gLogicalKeyEntries[DIK_H].rmenu = KEY_ALT_H;
    gLogicalKeyEntries[DIK_H].ctrl = KEY_CTRL_H;

    gLogicalKeyEntries[DIK_J].unmodified = KEY_LOWERCASE_J;
    gLogicalKeyEntries[DIK_J].shift = KEY_UPPERCASE_J;
    gLogicalKeyEntries[DIK_J].lmenu = KEY_ALT_J;
    gLogicalKeyEntries[DIK_J].rmenu = KEY_ALT_J;
    gLogicalKeyEntries[DIK_J].ctrl = KEY_CTRL_J;

    gLogicalKeyEntries[DIK_K].unmodified = KEY_LOWERCASE_K;
    gLogicalKeyEntries[DIK_K].shift = KEY_UPPERCASE_K;
    gLogicalKeyEntries[DIK_K].lmenu = KEY_ALT_K;
    gLogicalKeyEntries[DIK_K].rmenu = KEY_ALT_K;
    gLogicalKeyEntries[DIK_K].ctrl = KEY_CTRL_K;

    gLogicalKeyEntries[DIK_L].unmodified = KEY_LOWERCASE_L;
    gLogicalKeyEntries[DIK_L].shift = KEY_UPPERCASE_L;
    gLogicalKeyEntries[DIK_L].lmenu = KEY_ALT_L;
    gLogicalKeyEntries[DIK_L].rmenu = KEY_ALT_L;
    gLogicalKeyEntries[DIK_L].ctrl = KEY_CTRL_L;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_SEMICOLON;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = DIK_APOSTROPHE;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_3;
        break;
    default:
        k = DIK_2;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[k].shift = KEY_QUOTE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[DIK_RETURN].unmodified = KEY_RETURN;
    gLogicalKeyEntries[DIK_RETURN].shift = KEY_RETURN;
    gLogicalKeyEntries[DIK_RETURN].lmenu = KEY_RETURN;
    gLogicalKeyEntries[DIK_RETURN].rmenu = KEY_RETURN;
    gLogicalKeyEntries[DIK_RETURN].ctrl = KEY_CTRL_J;

    gLogicalKeyEntries[DIK_LSHIFT].unmodified = -1;
    gLogicalKeyEntries[DIK_LSHIFT].shift = -1;
    gLogicalKeyEntries[DIK_LSHIFT].lmenu = -1;
    gLogicalKeyEntries[DIK_LSHIFT].rmenu = -1;
    gLogicalKeyEntries[DIK_LSHIFT].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = DIK_Z;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_W;
        break;
    default:
        k = DIK_Y;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Z;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Z;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Z;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_Z;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Z;

    gLogicalKeyEntries[DIK_X].unmodified = KEY_LOWERCASE_X;
    gLogicalKeyEntries[DIK_X].shift = KEY_UPPERCASE_X;
    gLogicalKeyEntries[DIK_X].lmenu = KEY_ALT_X;
    gLogicalKeyEntries[DIK_X].rmenu = KEY_ALT_X;
    gLogicalKeyEntries[DIK_X].ctrl = KEY_CTRL_X;

    gLogicalKeyEntries[DIK_C].unmodified = KEY_LOWERCASE_C;
    gLogicalKeyEntries[DIK_C].shift = KEY_UPPERCASE_C;
    gLogicalKeyEntries[DIK_C].lmenu = KEY_ALT_C;
    gLogicalKeyEntries[DIK_C].rmenu = KEY_ALT_C;
    gLogicalKeyEntries[DIK_C].ctrl = KEY_CTRL_C;

    gLogicalKeyEntries[DIK_V].unmodified = KEY_LOWERCASE_V;
    gLogicalKeyEntries[DIK_V].shift = KEY_UPPERCASE_V;
    gLogicalKeyEntries[DIK_V].lmenu = KEY_ALT_V;
    gLogicalKeyEntries[DIK_V].rmenu = KEY_ALT_V;
    gLogicalKeyEntries[DIK_V].ctrl = KEY_CTRL_V;

    gLogicalKeyEntries[DIK_B].unmodified = KEY_LOWERCASE_B;
    gLogicalKeyEntries[DIK_B].shift = KEY_UPPERCASE_B;
    gLogicalKeyEntries[DIK_B].lmenu = KEY_ALT_B;
    gLogicalKeyEntries[DIK_B].rmenu = KEY_ALT_B;
    gLogicalKeyEntries[DIK_B].ctrl = KEY_CTRL_B;

    gLogicalKeyEntries[DIK_N].unmodified = KEY_LOWERCASE_N;
    gLogicalKeyEntries[DIK_N].shift = KEY_UPPERCASE_N;
    gLogicalKeyEntries[DIK_N].lmenu = KEY_ALT_N;
    gLogicalKeyEntries[DIK_N].rmenu = KEY_ALT_N;
    gLogicalKeyEntries[DIK_N].ctrl = KEY_CTRL_N;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_M;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_M;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_M;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_M;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_M;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_M;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_M;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_LESS;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_COMMA;
        break;
    default:
        k = DIK_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_GREATER;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = DIK_SLASH;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_PERIOD;
        break;
    default:
        k = DIK_7;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_SLASH;
    gLogicalKeyEntries[k].shift = KEY_QUESTION;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[DIK_RSHIFT].unmodified = -1;
    gLogicalKeyEntries[DIK_RSHIFT].shift = -1;
    gLogicalKeyEntries[DIK_RSHIFT].lmenu = -1;
    gLogicalKeyEntries[DIK_RSHIFT].rmenu = -1;
    gLogicalKeyEntries[DIK_RSHIFT].ctrl = -1;

    gLogicalKeyEntries[DIK_LCONTROL].unmodified = -1;
    gLogicalKeyEntries[DIK_LCONTROL].shift = -1;
    gLogicalKeyEntries[DIK_LCONTROL].lmenu = -1;
    gLogicalKeyEntries[DIK_LCONTROL].rmenu = -1;
    gLogicalKeyEntries[DIK_LCONTROL].ctrl = -1;

    gLogicalKeyEntries[DIK_LMENU].unmodified = -1;
    gLogicalKeyEntries[DIK_LMENU].shift = -1;
    gLogicalKeyEntries[DIK_LMENU].lmenu = -1;
    gLogicalKeyEntries[DIK_LMENU].rmenu = -1;
    gLogicalKeyEntries[DIK_LMENU].ctrl = -1;

    gLogicalKeyEntries[DIK_SPACE].unmodified = KEY_SPACE;
    gLogicalKeyEntries[DIK_SPACE].shift = KEY_SPACE;
    gLogicalKeyEntries[DIK_SPACE].lmenu = KEY_SPACE;
    gLogicalKeyEntries[DIK_SPACE].rmenu = KEY_SPACE;
    gLogicalKeyEntries[DIK_SPACE].ctrl = KEY_SPACE;

    gLogicalKeyEntries[DIK_RMENU].unmodified = -1;
    gLogicalKeyEntries[DIK_RMENU].shift = -1;
    gLogicalKeyEntries[DIK_RMENU].lmenu = -1;
    gLogicalKeyEntries[DIK_RMENU].rmenu = -1;
    gLogicalKeyEntries[DIK_RMENU].ctrl = -1;

    gLogicalKeyEntries[DIK_RCONTROL].unmodified = -1;
    gLogicalKeyEntries[DIK_RCONTROL].shift = -1;
    gLogicalKeyEntries[DIK_RCONTROL].lmenu = -1;
    gLogicalKeyEntries[DIK_RCONTROL].rmenu = -1;
    gLogicalKeyEntries[DIK_RCONTROL].ctrl = -1;

    gLogicalKeyEntries[DIK_INSERT].unmodified = KEY_INSERT;
    gLogicalKeyEntries[DIK_INSERT].shift = KEY_INSERT;
    gLogicalKeyEntries[DIK_INSERT].lmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[DIK_INSERT].rmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[DIK_INSERT].ctrl = KEY_CTRL_INSERT;

    gLogicalKeyEntries[DIK_HOME].unmodified = KEY_HOME;
    gLogicalKeyEntries[DIK_HOME].shift = KEY_HOME;
    gLogicalKeyEntries[DIK_HOME].lmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[DIK_HOME].rmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[DIK_HOME].ctrl = KEY_CTRL_HOME;

    gLogicalKeyEntries[DIK_PRIOR].unmodified = KEY_PAGE_UP;
    gLogicalKeyEntries[DIK_PRIOR].shift = KEY_PAGE_UP;
    gLogicalKeyEntries[DIK_PRIOR].lmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[DIK_PRIOR].rmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[DIK_PRIOR].ctrl = KEY_CTRL_PAGE_UP;

    gLogicalKeyEntries[DIK_DELETE].unmodified = KEY_DELETE;
    gLogicalKeyEntries[DIK_DELETE].shift = KEY_DELETE;
    gLogicalKeyEntries[DIK_DELETE].lmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[DIK_DELETE].rmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[DIK_DELETE].ctrl = KEY_CTRL_DELETE;

    gLogicalKeyEntries[DIK_END].unmodified = KEY_END;
    gLogicalKeyEntries[DIK_END].shift = KEY_END;
    gLogicalKeyEntries[DIK_END].lmenu = KEY_ALT_END;
    gLogicalKeyEntries[DIK_END].rmenu = KEY_ALT_END;
    gLogicalKeyEntries[DIK_END].ctrl = KEY_CTRL_END;

    gLogicalKeyEntries[DIK_NEXT].unmodified = KEY_PAGE_DOWN;
    gLogicalKeyEntries[DIK_NEXT].shift = KEY_PAGE_DOWN;
    gLogicalKeyEntries[DIK_NEXT].lmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[DIK_NEXT].rmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[DIK_NEXT].ctrl = KEY_CTRL_PAGE_DOWN;

    gLogicalKeyEntries[DIK_UP].unmodified = KEY_ARROW_UP;
    gLogicalKeyEntries[DIK_UP].shift = KEY_ARROW_UP;
    gLogicalKeyEntries[DIK_UP].lmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[DIK_UP].rmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[DIK_UP].ctrl = KEY_CTRL_ARROW_UP;

    gLogicalKeyEntries[DIK_DOWN].unmodified = KEY_ARROW_DOWN;
    gLogicalKeyEntries[DIK_DOWN].shift = KEY_ARROW_DOWN;
    gLogicalKeyEntries[DIK_DOWN].lmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[DIK_DOWN].rmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[DIK_DOWN].ctrl = KEY_CTRL_ARROW_DOWN;

    gLogicalKeyEntries[DIK_LEFT].unmodified = KEY_ARROW_LEFT;
    gLogicalKeyEntries[DIK_LEFT].shift = KEY_ARROW_LEFT;
    gLogicalKeyEntries[DIK_LEFT].lmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[DIK_LEFT].rmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[DIK_LEFT].ctrl = KEY_CTRL_ARROW_LEFT;

    gLogicalKeyEntries[DIK_RIGHT].unmodified = KEY_ARROW_RIGHT;
    gLogicalKeyEntries[DIK_RIGHT].shift = KEY_ARROW_RIGHT;
    gLogicalKeyEntries[DIK_RIGHT].lmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[DIK_RIGHT].rmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[DIK_RIGHT].ctrl = KEY_CTRL_ARROW_RIGHT;

    gLogicalKeyEntries[DIK_NUMLOCK].unmodified = -1;
    gLogicalKeyEntries[DIK_NUMLOCK].shift = -1;
    gLogicalKeyEntries[DIK_NUMLOCK].lmenu = -1;
    gLogicalKeyEntries[DIK_NUMLOCK].rmenu = -1;
    gLogicalKeyEntries[DIK_NUMLOCK].ctrl = -1;

    gLogicalKeyEntries[DIK_DIVIDE].unmodified = KEY_SLASH;
    gLogicalKeyEntries[DIK_DIVIDE].shift = KEY_SLASH;
    gLogicalKeyEntries[DIK_DIVIDE].lmenu = -1;
    gLogicalKeyEntries[DIK_DIVIDE].rmenu = -1;
    gLogicalKeyEntries[DIK_DIVIDE].ctrl = 3;

    gLogicalKeyEntries[DIK_MULTIPLY].unmodified = KEY_ASTERISK;
    gLogicalKeyEntries[DIK_MULTIPLY].shift = KEY_ASTERISK;
    gLogicalKeyEntries[DIK_MULTIPLY].lmenu = -1;
    gLogicalKeyEntries[DIK_MULTIPLY].rmenu = -1;
    gLogicalKeyEntries[DIK_MULTIPLY].ctrl = -1;

    gLogicalKeyEntries[DIK_SUBTRACT].unmodified = KEY_MINUS;
    gLogicalKeyEntries[DIK_SUBTRACT].shift = KEY_MINUS;
    gLogicalKeyEntries[DIK_SUBTRACT].lmenu = -1;
    gLogicalKeyEntries[DIK_SUBTRACT].rmenu = -1;
    gLogicalKeyEntries[DIK_SUBTRACT].ctrl = -1;

    gLogicalKeyEntries[DIK_NUMPAD7].unmodified = KEY_HOME;
    gLogicalKeyEntries[DIK_NUMPAD7].shift = KEY_7;
    gLogicalKeyEntries[DIK_NUMPAD7].lmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[DIK_NUMPAD7].rmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[DIK_NUMPAD7].ctrl = KEY_CTRL_HOME;

    gLogicalKeyEntries[DIK_NUMPAD8].unmodified = KEY_ARROW_UP;
    gLogicalKeyEntries[DIK_NUMPAD8].shift = KEY_8;
    gLogicalKeyEntries[DIK_NUMPAD8].lmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[DIK_NUMPAD8].rmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[DIK_NUMPAD8].ctrl = KEY_CTRL_ARROW_UP;

    gLogicalKeyEntries[DIK_NUMPAD9].unmodified = KEY_PAGE_UP;
    gLogicalKeyEntries[DIK_NUMPAD9].shift = KEY_9;
    gLogicalKeyEntries[DIK_NUMPAD9].lmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[DIK_NUMPAD9].rmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[DIK_NUMPAD9].ctrl = KEY_CTRL_PAGE_UP;

    gLogicalKeyEntries[DIK_ADD].unmodified = KEY_PLUS;
    gLogicalKeyEntries[DIK_ADD].shift = KEY_PLUS;
    gLogicalKeyEntries[DIK_ADD].lmenu = -1;
    gLogicalKeyEntries[DIK_ADD].rmenu = -1;
    gLogicalKeyEntries[DIK_ADD].ctrl = -1;

    gLogicalKeyEntries[DIK_NUMPAD4].unmodified = KEY_ARROW_LEFT;
    gLogicalKeyEntries[DIK_NUMPAD4].shift = KEY_4;
    gLogicalKeyEntries[DIK_NUMPAD4].lmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[DIK_NUMPAD4].rmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[DIK_NUMPAD4].ctrl = KEY_CTRL_ARROW_LEFT;

    gLogicalKeyEntries[DIK_NUMPAD5].unmodified = KEY_NUMBERPAD_5;
    gLogicalKeyEntries[DIK_NUMPAD5].shift = KEY_5;
    gLogicalKeyEntries[DIK_NUMPAD5].lmenu = KEY_ALT_NUMBERPAD_5;
    gLogicalKeyEntries[DIK_NUMPAD5].rmenu = KEY_ALT_NUMBERPAD_5;
    gLogicalKeyEntries[DIK_NUMPAD5].ctrl = KEY_CTRL_NUMBERPAD_5;

    gLogicalKeyEntries[DIK_NUMPAD6].unmodified = KEY_ARROW_RIGHT;
    gLogicalKeyEntries[DIK_NUMPAD6].shift = KEY_6;
    gLogicalKeyEntries[DIK_NUMPAD6].lmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[DIK_NUMPAD6].rmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[DIK_NUMPAD6].ctrl = KEY_CTRL_ARROW_RIGHT;

    gLogicalKeyEntries[DIK_NUMPAD1].unmodified = KEY_END;
    gLogicalKeyEntries[DIK_NUMPAD1].shift = KEY_1;
    gLogicalKeyEntries[DIK_NUMPAD1].lmenu = KEY_ALT_END;
    gLogicalKeyEntries[DIK_NUMPAD1].rmenu = KEY_ALT_END;
    gLogicalKeyEntries[DIK_NUMPAD1].ctrl = KEY_CTRL_END;

    gLogicalKeyEntries[DIK_NUMPAD2].unmodified = KEY_ARROW_DOWN;
    gLogicalKeyEntries[DIK_NUMPAD2].shift = KEY_2;
    gLogicalKeyEntries[DIK_NUMPAD2].lmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[DIK_NUMPAD2].rmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[DIK_NUMPAD2].ctrl = KEY_CTRL_ARROW_DOWN;

    gLogicalKeyEntries[DIK_NUMPAD3].unmodified = KEY_PAGE_DOWN;
    gLogicalKeyEntries[DIK_NUMPAD3].shift = KEY_3;
    gLogicalKeyEntries[DIK_NUMPAD3].lmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[DIK_NUMPAD3].rmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[DIK_NUMPAD3].ctrl = KEY_CTRL_PAGE_DOWN;

    gLogicalKeyEntries[DIK_NUMPADENTER].unmodified = KEY_RETURN;
    gLogicalKeyEntries[DIK_NUMPADENTER].shift = KEY_RETURN;
    gLogicalKeyEntries[DIK_NUMPADENTER].lmenu = -1;
    gLogicalKeyEntries[DIK_NUMPADENTER].rmenu = -1;
    gLogicalKeyEntries[DIK_NUMPADENTER].ctrl = -1;

    gLogicalKeyEntries[DIK_NUMPAD0].unmodified = KEY_INSERT;
    gLogicalKeyEntries[DIK_NUMPAD0].shift = KEY_0;
    gLogicalKeyEntries[DIK_NUMPAD0].lmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[DIK_NUMPAD0].rmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[DIK_NUMPAD0].ctrl = KEY_CTRL_INSERT;

    gLogicalKeyEntries[DIK_DECIMAL].unmodified = KEY_DELETE;
    gLogicalKeyEntries[DIK_DECIMAL].shift = KEY_DOT;
    gLogicalKeyEntries[DIK_DECIMAL].lmenu = -1;
    gLogicalKeyEntries[DIK_DECIMAL].rmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[DIK_DECIMAL].ctrl = KEY_CTRL_DELETE;
}

// 0x4D0400
void keyboardBuildFrenchConfiguration()
{
    int k;

    keyboardBuildQwertyConfiguration();

    gLogicalKeyEntries[DIK_GRAVE].unmodified = KEY_178;
    gLogicalKeyEntries[DIK_GRAVE].shift = -1;
    gLogicalKeyEntries[DIK_GRAVE].lmenu = -1;
    gLogicalKeyEntries[DIK_GRAVE].rmenu = -1;
    gLogicalKeyEntries[DIK_GRAVE].ctrl = -1;

    gLogicalKeyEntries[DIK_1].unmodified = KEY_AMPERSAND;
    gLogicalKeyEntries[DIK_1].shift = KEY_1;
    gLogicalKeyEntries[DIK_1].lmenu = -1;
    gLogicalKeyEntries[DIK_1].rmenu = -1;
    gLogicalKeyEntries[DIK_1].ctrl = -1;

    gLogicalKeyEntries[DIK_2].unmodified = KEY_233;
    gLogicalKeyEntries[DIK_2].shift = KEY_2;
    gLogicalKeyEntries[DIK_2].lmenu = -1;
    gLogicalKeyEntries[DIK_2].rmenu = KEY_152;
    gLogicalKeyEntries[DIK_2].ctrl = -1;

    gLogicalKeyEntries[DIK_3].unmodified = KEY_QUOTE;
    gLogicalKeyEntries[DIK_3].shift = KEY_3;
    gLogicalKeyEntries[DIK_3].lmenu = -1;
    gLogicalKeyEntries[DIK_3].rmenu = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[DIK_3].ctrl = -1;

    gLogicalKeyEntries[DIK_4].unmodified = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[DIK_4].shift = KEY_4;
    gLogicalKeyEntries[DIK_4].lmenu = -1;
    gLogicalKeyEntries[DIK_4].rmenu = KEY_BRACE_LEFT;
    gLogicalKeyEntries[DIK_4].ctrl = -1;

    gLogicalKeyEntries[DIK_5].unmodified = KEY_PAREN_LEFT;
    gLogicalKeyEntries[DIK_5].shift = KEY_5;
    gLogicalKeyEntries[DIK_5].lmenu = -1;
    gLogicalKeyEntries[DIK_5].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[DIK_5].ctrl = -1;

    gLogicalKeyEntries[DIK_6].unmodified = KEY_150;
    gLogicalKeyEntries[DIK_6].shift = KEY_6;
    gLogicalKeyEntries[DIK_6].lmenu = -1;
    gLogicalKeyEntries[DIK_6].rmenu = KEY_166;
    gLogicalKeyEntries[DIK_6].ctrl = -1;

    gLogicalKeyEntries[DIK_7].unmodified = KEY_232;
    gLogicalKeyEntries[DIK_7].shift = KEY_7;
    gLogicalKeyEntries[DIK_7].lmenu = -1;
    gLogicalKeyEntries[DIK_7].rmenu = KEY_GRAVE;
    gLogicalKeyEntries[DIK_7].ctrl = -1;

    gLogicalKeyEntries[DIK_8].unmodified = KEY_UNDERSCORE;
    gLogicalKeyEntries[DIK_8].shift = KEY_8;
    gLogicalKeyEntries[DIK_8].lmenu = -1;
    gLogicalKeyEntries[DIK_8].rmenu = KEY_BACKSLASH;
    gLogicalKeyEntries[DIK_8].ctrl = -1;

    gLogicalKeyEntries[DIK_9].unmodified = KEY_231;
    gLogicalKeyEntries[DIK_9].shift = KEY_9;
    gLogicalKeyEntries[DIK_9].lmenu = -1;
    gLogicalKeyEntries[DIK_9].rmenu = KEY_136;
    gLogicalKeyEntries[DIK_9].ctrl = -1;

    gLogicalKeyEntries[DIK_0].unmodified = KEY_224;
    gLogicalKeyEntries[DIK_0].shift = KEY_0;
    gLogicalKeyEntries[DIK_0].lmenu = -1;
    gLogicalKeyEntries[DIK_0].rmenu = KEY_AT;
    gLogicalKeyEntries[DIK_0].ctrl = -1;

    gLogicalKeyEntries[DIK_MINUS].unmodified = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[DIK_MINUS].shift = KEY_176;
    gLogicalKeyEntries[DIK_MINUS].lmenu = -1;
    gLogicalKeyEntries[DIK_MINUS].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[DIK_MINUS].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_EQUALS;
        break;
    default:
        k = DIK_0;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_EQUAL;
    gLogicalKeyEntries[k].shift = KEY_PLUS;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[DIK_LBRACKET].unmodified = KEY_136;
    gLogicalKeyEntries[DIK_LBRACKET].shift = KEY_168;
    gLogicalKeyEntries[DIK_LBRACKET].lmenu = -1;
    gLogicalKeyEntries[DIK_LBRACKET].rmenu = -1;
    gLogicalKeyEntries[DIK_LBRACKET].ctrl = -1;

    gLogicalKeyEntries[DIK_RBRACKET].unmodified = KEY_DOLLAR;
    gLogicalKeyEntries[DIK_RBRACKET].shift = KEY_163;
    gLogicalKeyEntries[DIK_RBRACKET].lmenu = -1;
    gLogicalKeyEntries[DIK_RBRACKET].rmenu = KEY_164;
    gLogicalKeyEntries[DIK_RBRACKET].ctrl = -1;

    gLogicalKeyEntries[DIK_APOSTROPHE].unmodified = KEY_249;
    gLogicalKeyEntries[DIK_APOSTROPHE].shift = KEY_PERCENT;
    gLogicalKeyEntries[DIK_APOSTROPHE].lmenu = -1;
    gLogicalKeyEntries[DIK_APOSTROPHE].rmenu = -1;
    gLogicalKeyEntries[DIK_APOSTROPHE].ctrl = -1;

    gLogicalKeyEntries[DIK_BACKSLASH].unmodified = KEY_ASTERISK;
    gLogicalKeyEntries[DIK_BACKSLASH].shift = KEY_181;
    gLogicalKeyEntries[DIK_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[DIK_BACKSLASH].rmenu = -1;
    gLogicalKeyEntries[DIK_BACKSLASH].ctrl = -1;

    gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    gLogicalKeyEntries[DIK_OEM_102].rmenu = -1;
    gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_M;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_QUESTION;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_COMMA;
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
        // Semicolon is already mapped above, so I bet it should be DIK_COLON.
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COLON;
    gLogicalKeyEntries[k].shift = KEY_SLASH;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[DIK_SLASH].unmodified = KEY_EXCLAMATION;
    gLogicalKeyEntries[DIK_SLASH].shift = KEY_167;
    gLogicalKeyEntries[DIK_SLASH].lmenu = -1;
    gLogicalKeyEntries[DIK_SLASH].rmenu = -1;
    gLogicalKeyEntries[DIK_SLASH].ctrl = -1;
}

// 0x4D0C54
void keyboardBuildGermanConfiguration()
{
    int k;

    keyboardBuildQwertyConfiguration();

    gLogicalKeyEntries[DIK_GRAVE].unmodified = KEY_136;
    gLogicalKeyEntries[DIK_GRAVE].shift = KEY_186;
    gLogicalKeyEntries[DIK_GRAVE].lmenu = -1;
    gLogicalKeyEntries[DIK_GRAVE].rmenu = -1;
    gLogicalKeyEntries[DIK_GRAVE].ctrl = -1;

    gLogicalKeyEntries[DIK_2].unmodified = KEY_2;
    gLogicalKeyEntries[DIK_2].shift = KEY_QUOTE;
    gLogicalKeyEntries[DIK_2].lmenu = -1;
    gLogicalKeyEntries[DIK_2].rmenu = KEY_178;
    gLogicalKeyEntries[DIK_2].ctrl = -1;

    gLogicalKeyEntries[DIK_3].unmodified = KEY_3;
    gLogicalKeyEntries[DIK_3].shift = KEY_167;
    gLogicalKeyEntries[DIK_3].lmenu = -1;
    gLogicalKeyEntries[DIK_3].rmenu = KEY_179;
    gLogicalKeyEntries[DIK_3].ctrl = -1;

    gLogicalKeyEntries[DIK_6].unmodified = KEY_6;
    gLogicalKeyEntries[DIK_6].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[DIK_6].lmenu = -1;
    gLogicalKeyEntries[DIK_6].rmenu = -1;
    gLogicalKeyEntries[DIK_6].ctrl = -1;

    gLogicalKeyEntries[DIK_7].unmodified = KEY_7;
    gLogicalKeyEntries[DIK_7].shift = KEY_166;
    gLogicalKeyEntries[DIK_7].lmenu = -1;
    gLogicalKeyEntries[DIK_7].rmenu = KEY_BRACE_LEFT;
    gLogicalKeyEntries[DIK_7].ctrl = -1;

    gLogicalKeyEntries[DIK_8].unmodified = KEY_8;
    gLogicalKeyEntries[DIK_8].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[DIK_8].lmenu = -1;
    gLogicalKeyEntries[DIK_8].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[DIK_8].ctrl = -1;

    gLogicalKeyEntries[DIK_9].unmodified = KEY_9;
    gLogicalKeyEntries[DIK_9].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[DIK_9].lmenu = -1;
    gLogicalKeyEntries[DIK_9].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[DIK_9].ctrl = -1;

    gLogicalKeyEntries[DIK_0].unmodified = KEY_0;
    gLogicalKeyEntries[DIK_0].shift = KEY_EQUAL;
    gLogicalKeyEntries[DIK_0].lmenu = -1;
    gLogicalKeyEntries[DIK_0].rmenu = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[DIK_0].ctrl = -1;

    gLogicalKeyEntries[DIK_MINUS].unmodified = KEY_223;
    gLogicalKeyEntries[DIK_MINUS].shift = KEY_QUESTION;
    gLogicalKeyEntries[DIK_MINUS].lmenu = -1;
    gLogicalKeyEntries[DIK_MINUS].rmenu = KEY_BACKSLASH;
    gLogicalKeyEntries[DIK_MINUS].ctrl = -1;

    gLogicalKeyEntries[DIK_EQUALS].unmodified = KEY_180;
    gLogicalKeyEntries[DIK_EQUALS].shift = KEY_GRAVE;
    gLogicalKeyEntries[DIK_EQUALS].lmenu = -1;
    gLogicalKeyEntries[DIK_EQUALS].rmenu = -1;
    gLogicalKeyEntries[DIK_EQUALS].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_A;
        break;
    default:
        k = DIK_Q;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Q;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Q;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Q;
    gLogicalKeyEntries[k].rmenu = KEY_AT;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Q;

    gLogicalKeyEntries[DIK_LBRACKET].unmodified = KEY_252;
    gLogicalKeyEntries[DIK_LBRACKET].shift = KEY_220;
    gLogicalKeyEntries[DIK_LBRACKET].lmenu = -1;
    gLogicalKeyEntries[DIK_LBRACKET].rmenu = -1;
    gLogicalKeyEntries[DIK_LBRACKET].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_EQUALS;
        break;
    default:
        k = DIK_RBRACKET;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_PLUS;
    gLogicalKeyEntries[k].shift = KEY_ASTERISK;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = KEY_152;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[DIK_SEMICOLON].unmodified = KEY_246;
    gLogicalKeyEntries[DIK_SEMICOLON].shift = KEY_214;
    gLogicalKeyEntries[DIK_SEMICOLON].lmenu = -1;
    gLogicalKeyEntries[DIK_SEMICOLON].rmenu = -1;
    gLogicalKeyEntries[DIK_SEMICOLON].ctrl = -1;

    gLogicalKeyEntries[DIK_APOSTROPHE].unmodified = KEY_228;
    gLogicalKeyEntries[DIK_APOSTROPHE].shift = KEY_196;
    gLogicalKeyEntries[DIK_APOSTROPHE].lmenu = -1;
    gLogicalKeyEntries[DIK_APOSTROPHE].rmenu = -1;
    gLogicalKeyEntries[DIK_APOSTROPHE].ctrl = -1;

    gLogicalKeyEntries[DIK_BACKSLASH].unmodified = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[DIK_BACKSLASH].shift = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[DIK_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[DIK_BACKSLASH].rmenu = -1;
    gLogicalKeyEntries[DIK_BACKSLASH].ctrl = -1;

    gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    gLogicalKeyEntries[DIK_OEM_102].rmenu = KEY_166;
    gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_M;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_M;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_M;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_M;
    gLogicalKeyEntries[k].rmenu = KEY_181;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_M;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_M;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_SEMICOLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_COMMA;
        break;
    default:
        k = DIK_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = DIK_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_6;
        break;
    default:
        k = DIK_SLASH;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_150;
    gLogicalKeyEntries[k].shift = KEY_151;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[DIK_DIVIDE].unmodified = KEY_247;
    gLogicalKeyEntries[DIK_DIVIDE].shift = KEY_247;
    gLogicalKeyEntries[DIK_DIVIDE].lmenu = -1;
    gLogicalKeyEntries[DIK_DIVIDE].rmenu = -1;
    gLogicalKeyEntries[DIK_DIVIDE].ctrl = -1;

    gLogicalKeyEntries[DIK_MULTIPLY].unmodified = KEY_215;
    gLogicalKeyEntries[DIK_MULTIPLY].shift = KEY_215;
    gLogicalKeyEntries[DIK_MULTIPLY].lmenu = -1;
    gLogicalKeyEntries[DIK_MULTIPLY].rmenu = -1;
    gLogicalKeyEntries[DIK_MULTIPLY].ctrl = -1;

    gLogicalKeyEntries[DIK_DECIMAL].unmodified = KEY_DELETE;
    gLogicalKeyEntries[DIK_DECIMAL].shift = KEY_COMMA;
    gLogicalKeyEntries[DIK_DECIMAL].lmenu = -1;
    gLogicalKeyEntries[DIK_DECIMAL].rmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[DIK_DECIMAL].ctrl = KEY_CTRL_DELETE;
}

// 0x4D1758
void keyboardBuildItalianConfiguration()
{
    int k;

    keyboardBuildQwertyConfiguration();

    gLogicalKeyEntries[DIK_GRAVE].unmodified = KEY_BACKSLASH;
    gLogicalKeyEntries[DIK_GRAVE].shift = KEY_BAR;
    gLogicalKeyEntries[DIK_GRAVE].lmenu = -1;
    gLogicalKeyEntries[DIK_GRAVE].rmenu = -1;
    gLogicalKeyEntries[DIK_GRAVE].ctrl = -1;

    gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    gLogicalKeyEntries[DIK_OEM_102].rmenu = -1;
    gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    gLogicalKeyEntries[DIK_1].unmodified = KEY_1;
    gLogicalKeyEntries[DIK_1].shift = KEY_EXCLAMATION;
    gLogicalKeyEntries[DIK_1].lmenu = -1;
    gLogicalKeyEntries[DIK_1].rmenu = -1;
    gLogicalKeyEntries[DIK_1].ctrl = -1;

    gLogicalKeyEntries[DIK_2].unmodified = KEY_2;
    gLogicalKeyEntries[DIK_2].shift = KEY_QUOTE;
    gLogicalKeyEntries[DIK_2].lmenu = -1;
    gLogicalKeyEntries[DIK_2].rmenu = -1;
    gLogicalKeyEntries[DIK_2].ctrl = -1;

    gLogicalKeyEntries[DIK_3].unmodified = KEY_3;
    gLogicalKeyEntries[DIK_3].shift = KEY_163;
    gLogicalKeyEntries[DIK_3].lmenu = -1;
    gLogicalKeyEntries[DIK_3].rmenu = -1;
    gLogicalKeyEntries[DIK_3].ctrl = -1;

    gLogicalKeyEntries[DIK_6].unmodified = KEY_6;
    gLogicalKeyEntries[DIK_6].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[DIK_6].lmenu = -1;
    gLogicalKeyEntries[DIK_6].rmenu = -1;
    gLogicalKeyEntries[DIK_6].ctrl = -1;

    gLogicalKeyEntries[DIK_7].unmodified = KEY_7;
    gLogicalKeyEntries[DIK_7].shift = KEY_SLASH;
    gLogicalKeyEntries[DIK_7].lmenu = -1;
    gLogicalKeyEntries[DIK_7].rmenu = -1;
    gLogicalKeyEntries[DIK_7].ctrl = -1;

    gLogicalKeyEntries[DIK_8].unmodified = KEY_8;
    gLogicalKeyEntries[DIK_8].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[DIK_8].lmenu = -1;
    gLogicalKeyEntries[DIK_8].rmenu = -1;
    gLogicalKeyEntries[DIK_8].ctrl = -1;

    gLogicalKeyEntries[DIK_9].unmodified = KEY_9;
    gLogicalKeyEntries[DIK_9].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[DIK_9].lmenu = -1;
    gLogicalKeyEntries[DIK_9].rmenu = -1;
    gLogicalKeyEntries[DIK_9].ctrl = -1;

    gLogicalKeyEntries[DIK_0].unmodified = KEY_0;
    gLogicalKeyEntries[DIK_0].shift = KEY_EQUAL;
    gLogicalKeyEntries[DIK_0].lmenu = -1;
    gLogicalKeyEntries[DIK_0].rmenu = -1;
    gLogicalKeyEntries[DIK_0].ctrl = -1;

    gLogicalKeyEntries[DIK_MINUS].unmodified = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[DIK_MINUS].shift = KEY_QUESTION;
    gLogicalKeyEntries[DIK_MINUS].lmenu = -1;
    gLogicalKeyEntries[DIK_MINUS].rmenu = -1;
    gLogicalKeyEntries[DIK_MINUS].ctrl = -1;

    gLogicalKeyEntries[DIK_LBRACKET].unmodified = KEY_232;
    gLogicalKeyEntries[DIK_LBRACKET].shift = KEY_233;
    gLogicalKeyEntries[DIK_LBRACKET].lmenu = -1;
    gLogicalKeyEntries[DIK_LBRACKET].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[DIK_LBRACKET].ctrl = -1;

    gLogicalKeyEntries[DIK_RBRACKET].unmodified = KEY_PLUS;
    gLogicalKeyEntries[DIK_RBRACKET].shift = KEY_ASTERISK;
    gLogicalKeyEntries[DIK_RBRACKET].lmenu = -1;
    gLogicalKeyEntries[DIK_RBRACKET].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[DIK_RBRACKET].ctrl = -1;

    gLogicalKeyEntries[DIK_BACKSLASH].unmodified = KEY_249;
    gLogicalKeyEntries[DIK_BACKSLASH].shift = KEY_167;
    gLogicalKeyEntries[DIK_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[DIK_BACKSLASH].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[DIK_BACKSLASH].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_M;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_SEMICOLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_COMMA;
        break;
    default:
        k = DIK_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = DIK_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_6;
        break;
    default:
        k = DIK_SLASH;
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

    gLogicalKeyEntries[DIK_1].unmodified = KEY_1;
    gLogicalKeyEntries[DIK_1].shift = KEY_EXCLAMATION;
    gLogicalKeyEntries[DIK_1].lmenu = -1;
    gLogicalKeyEntries[DIK_1].rmenu = KEY_BAR;
    gLogicalKeyEntries[DIK_1].ctrl = -1;

    gLogicalKeyEntries[DIK_2].unmodified = KEY_2;
    gLogicalKeyEntries[DIK_2].shift = KEY_QUOTE;
    gLogicalKeyEntries[DIK_2].lmenu = -1;
    gLogicalKeyEntries[DIK_2].rmenu = KEY_AT;
    gLogicalKeyEntries[DIK_2].ctrl = -1;

    gLogicalKeyEntries[DIK_3].unmodified = KEY_3;
    gLogicalKeyEntries[DIK_3].shift = KEY_149;
    gLogicalKeyEntries[DIK_3].lmenu = -1;
    gLogicalKeyEntries[DIK_3].rmenu = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[DIK_3].ctrl = -1;

    gLogicalKeyEntries[DIK_6].unmodified = KEY_6;
    gLogicalKeyEntries[DIK_6].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[DIK_6].lmenu = -1;
    gLogicalKeyEntries[DIK_6].rmenu = KEY_172;
    gLogicalKeyEntries[DIK_6].ctrl = -1;

    gLogicalKeyEntries[DIK_7].unmodified = KEY_7;
    gLogicalKeyEntries[DIK_7].shift = KEY_SLASH;
    gLogicalKeyEntries[DIK_7].lmenu = -1;
    gLogicalKeyEntries[DIK_7].rmenu = -1;
    gLogicalKeyEntries[DIK_7].ctrl = -1;

    gLogicalKeyEntries[DIK_8].unmodified = KEY_8;
    gLogicalKeyEntries[DIK_8].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[DIK_8].lmenu = -1;
    gLogicalKeyEntries[DIK_8].rmenu = -1;
    gLogicalKeyEntries[DIK_8].ctrl = -1;

    gLogicalKeyEntries[DIK_9].unmodified = KEY_9;
    gLogicalKeyEntries[DIK_9].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[DIK_9].lmenu = -1;
    gLogicalKeyEntries[DIK_9].rmenu = -1;
    gLogicalKeyEntries[DIK_9].ctrl = -1;

    gLogicalKeyEntries[DIK_0].unmodified = KEY_0;
    gLogicalKeyEntries[DIK_0].shift = KEY_EQUAL;
    gLogicalKeyEntries[DIK_0].lmenu = -1;
    gLogicalKeyEntries[DIK_0].rmenu = -1;
    gLogicalKeyEntries[DIK_0].ctrl = -1;

    gLogicalKeyEntries[DIK_MINUS].unmodified = KEY_146;
    gLogicalKeyEntries[DIK_MINUS].shift = KEY_QUESTION;
    gLogicalKeyEntries[DIK_MINUS].lmenu = -1;
    gLogicalKeyEntries[DIK_MINUS].rmenu = -1;
    gLogicalKeyEntries[DIK_MINUS].ctrl = -1;

    gLogicalKeyEntries[DIK_EQUALS].unmodified = KEY_161;
    gLogicalKeyEntries[DIK_EQUALS].shift = KEY_191;
    gLogicalKeyEntries[DIK_EQUALS].lmenu = -1;
    gLogicalKeyEntries[DIK_EQUALS].rmenu = -1;
    gLogicalKeyEntries[DIK_EQUALS].ctrl = -1;

    gLogicalKeyEntries[DIK_GRAVE].unmodified = KEY_176;
    gLogicalKeyEntries[DIK_GRAVE].shift = KEY_170;
    gLogicalKeyEntries[DIK_GRAVE].lmenu = -1;
    gLogicalKeyEntries[DIK_GRAVE].rmenu = KEY_BACKSLASH;
    gLogicalKeyEntries[DIK_GRAVE].ctrl = -1;

    gLogicalKeyEntries[DIK_LBRACKET].unmodified = KEY_GRAVE;
    gLogicalKeyEntries[DIK_LBRACKET].shift = KEY_CARET;
    gLogicalKeyEntries[DIK_LBRACKET].lmenu = -1;
    gLogicalKeyEntries[DIK_LBRACKET].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[DIK_LBRACKET].ctrl = -1;

    gLogicalKeyEntries[DIK_RBRACKET].unmodified = KEY_PLUS;
    gLogicalKeyEntries[DIK_RBRACKET].shift = KEY_ASTERISK;
    gLogicalKeyEntries[DIK_RBRACKET].lmenu = -1;
    gLogicalKeyEntries[DIK_RBRACKET].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[DIK_RBRACKET].ctrl = -1;

    gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    gLogicalKeyEntries[DIK_OEM_102].rmenu = -1;
    gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    gLogicalKeyEntries[DIK_SEMICOLON].unmodified = KEY_241;
    gLogicalKeyEntries[DIK_SEMICOLON].shift = KEY_209;
    gLogicalKeyEntries[DIK_SEMICOLON].lmenu = -1;
    gLogicalKeyEntries[DIK_SEMICOLON].rmenu = -1;
    gLogicalKeyEntries[DIK_SEMICOLON].ctrl = -1;

    gLogicalKeyEntries[DIK_APOSTROPHE].unmodified = KEY_168;
    gLogicalKeyEntries[DIK_APOSTROPHE].shift = KEY_180;
    gLogicalKeyEntries[DIK_APOSTROPHE].lmenu = -1;
    gLogicalKeyEntries[DIK_APOSTROPHE].rmenu = KEY_BRACE_LEFT;
    gLogicalKeyEntries[DIK_APOSTROPHE].ctrl = -1;

    gLogicalKeyEntries[DIK_BACKSLASH].unmodified = KEY_231;
    gLogicalKeyEntries[DIK_BACKSLASH].shift = KEY_199;
    gLogicalKeyEntries[DIK_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[DIK_BACKSLASH].rmenu = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[DIK_BACKSLASH].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_M;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_SEMICOLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_COMMA;
        break;
    default:
        k = DIK_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = DIK_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = DIK_6;
        break;
    default:
        k = DIK_SLASH;
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
    if (GetKeyState(VK_CAPITAL) & 1) {
        gModifierKeysState |= MODIFIER_KEY_STATE_CAPS_LOCK;
    }

    if (GetKeyState(VK_NUMLOCK) & 1) {
        gModifierKeysState |= MODIFIER_KEY_STATE_NUM_LOCK;
    }

    if (GetKeyState(VK_SCROLL) & 1) {
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

    _vcr_buffer = internal_malloc(sizeof(*_vcr_buffer) * 4096);
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
int _vcr_stop(void)
{
    if (_vcr_state == 0 || _vcr_state == 1) {
        _vcr_state |= 0x80000000;
    }

    keyboardReset();

    return 1;
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
