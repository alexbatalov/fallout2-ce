#include "core.h"

#include <limits.h>
#include <string.h>

#include <SDL.h>

#include "audio_engine.h"
#include "color.h"
#include "config.h"
#include "dinput.h"
#include "draw.h"
#include "interface.h"
#include "kb.h"
#include "memory.h"
#include "mmx.h"
#include "mouse.h"
#include "text_font.h"
#include "vcr.h"
#include "win32.h"
#include "window_manager.h"
#include "window_manager_private.h"

namespace fallout {

static void idleImpl();
static bool createRenderer(int width, int height);
static void destroyRenderer();

// 0x51E234
IdleFunc* _idle_func = NULL;

// 0x51E238
FocusFunc* _focus_func = NULL;

// 0x51E23C
int gKeyboardKeyRepeatRate = 80;

// 0x51E240
int gKeyboardKeyRepeatDelay = 500;

// NOTE: This value is never set, so it's impossible to understand it's
// meaning.
//
// 0x51E2C4
void (*_update_palette_func)() = NULL;

// 0x51E2C8
bool gMmxEnabled = true;

// 0x51E2CC
bool gMmxProbed = false;

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

SDL_Window* gSdlWindow = NULL;
SDL_Surface* gSdlSurface = NULL;
SDL_Renderer* gSdlRenderer = NULL;
SDL_Texture* gSdlTexture = NULL;
SDL_Surface* gSdlTextureSurface = NULL;

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

    // SFALL: Set idle function.
    // CE: Prevents frying CPU when window is not focused.
    inputSetIdleFunc(idleImpl);

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

    if (vcrUpdate() != 3) {
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
    int windowWidth = fontGetStringWidth("Paused") + 32;
    int windowHeight = 3 * fontGetLineHeight() + 16;

    int win = windowCreate((rectGetWidth(&_scr_size) - windowWidth) / 2,
        (rectGetHeight(&_scr_size) - windowHeight) / 2,
        windowWidth,
        windowHeight,
        256,
        WINDOW_FLAG_0x10 | WINDOW_FLAG_0x04);
    if (win == -1) {
        return -1;
    }

    windowDrawBorder(win);

    unsigned char* windowBuffer = windowGetBuffer(win);
    fontDrawText(windowBuffer + 8 * windowWidth + 16,
        "Paused",
        windowWidth,
        windowWidth,
        _colorTable[31744]);

    _win_register_text_button(win,
        (windowWidth - fontGetStringWidth("Done") - 16) / 2,
        windowHeight - 8 - fontGetLineHeight() - 6,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        "Done",
        0);

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

        stream = compat_fopen(fileName, "rb");
        if (stream == NULL) {
            break;
        }

        fclose(stream);
    }

    if (index == 100000) {
        return -1;
    }

    stream = compat_fopen(fileName, "wb");
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

// NOTE: Unused.
//
// 0x4C9418
void inputSetKeyboardKeyRepeatRate(int value)
{
    gKeyboardKeyRepeatRate = value;
}

// NOTE: Unused.
//
// 0x4C9420
int inputGetKeyboardKeyRepeatRate()
{
    return gKeyboardKeyRepeatRate;
}

// NOTE: Unused.
//
// 0x4C9428
void inputSetKeyboardKeyRepeatDelay(int value)
{
    gKeyboardKeyRepeatDelay = value;
}

// NOTE: Unused.
//
// 0x4C9430
int inputGetKeyboardKeyRepeatDelay()
{
    return gKeyboardKeyRepeatDelay;
}

// NOTE: Unused.
//
// 0x4C9438
void inputSetFocusFunc(FocusFunc* func)
{
    _focus_func = func;
}

// NOTE: Unused.
//
// 0x4C9440
FocusFunc* inputGetFocusFunc()
{
    return _focus_func;
}

// NOTE: Unused.
//
// 0x4C9448
void inputSetIdleFunc(IdleFunc* func)
{
    _idle_func = func;
}

// NOTE: Unused.
//
// 0x4C9450
IdleFunc* inputGetIdleFunc()
{
    return _idle_func;
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
        case SDL_MOUSEWHEEL:
            handleMouseEvent(&e);
            break;
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        case SDL_FINGERUP:
            handleTouchEvent(&e);
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
                destroyRenderer();
                createRenderer(screenGetWidth(), screenGetHeight());
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                gProgramIsActive = true;
                windowRefreshAll(&_scr_size);
                audioEngineResume();
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                gProgramIsActive = false;
                audioEnginePause();
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

    if (gVcrState == VCR_STATE_PLAYING) {
        if ((gVcrTerminateFlags & VCR_TERMINATE_ON_KEY_PRESS) != 0) {
            gVcrPlaybackCompletionReason = VCR_PLAYBACK_COMPLETION_REASON_TERMINATED;
            vcrStop();
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
        _focus_func(false);
    }

    while (!gProgramIsActive) {
        _GNW95_process_message();

        if (_idle_func != NULL) {
            _idle_func();
        }
    }

    if (_focus_func != NULL) {
        _focus_func(true);
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
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            return -1;
        }

        Uint32 windowFlags = SDL_WINDOW_OPENGL;

        if (fullscreen) {
            windowFlags |= SDL_WINDOW_FULLSCREEN;
        }

        gSdlWindow = SDL_CreateWindow(gProgramWindowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, windowFlags);
        if (gSdlWindow == NULL) {
            return -1;
        }

        if (!createRenderer(width, height)) {
            destroyRenderer();

            SDL_DestroyWindow(gSdlWindow);
            gSdlWindow = NULL;

            return -1;
        }
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
        SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
        SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
        SDL_RenderClear(gSdlRenderer);
        SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
        SDL_RenderPresent(gSdlRenderer);
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
        SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
        SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
        SDL_RenderClear(gSdlRenderer);
        SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
        SDL_RenderPresent(gSdlRenderer);
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
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlTextureSurface, &destRect);
    SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
    SDL_RenderPresent(gSdlRenderer);
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
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlTextureSurface, &destRect);
    SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
    SDL_RenderPresent(gSdlRenderer);
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
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlTextureSurface, &destRect);
    SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
    SDL_RenderPresent(gSdlRenderer);
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
    for (int y = 0; y < gSdlSurface->h; y++) {
        memset(surface, 0, gSdlSurface->w);
        surface += gSdlSurface->pitch;
    }

    SDL_UnlockSurface(gSdlSurface);

    SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
    SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
    SDL_RenderPresent(gSdlRenderer);
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

static void idleImpl()
{
    SDL_Delay(125);
}

static bool createRenderer(int width, int height)
{
    gSdlRenderer = SDL_CreateRenderer(gSdlWindow, -1, 0);
    if (gSdlRenderer == NULL) {
        return false;
    }

    if (SDL_RenderSetLogicalSize(gSdlRenderer, width, height) != 0) {
        return false;
    }

    gSdlTexture = SDL_CreateTexture(gSdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (gSdlTexture == NULL) {
        return false;
    }

    Uint32 format;
    if (SDL_QueryTexture(gSdlTexture, &format, NULL, NULL, NULL) != 0) {
        return false;
    }

    gSdlTextureSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height, SDL_BITSPERPIXEL(format), format);
    if (gSdlTextureSurface == NULL) {
        return false;
    }

    return true;
}

static void destroyRenderer()
{
    if (gSdlTextureSurface != NULL) {
        SDL_FreeSurface(gSdlTextureSurface);
        gSdlTextureSurface = NULL;
    }

    if (gSdlTexture != NULL) {
        SDL_DestroyTexture(gSdlTexture);
        gSdlTexture = NULL;
    }

    if (gSdlRenderer != NULL) {
        SDL_DestroyRenderer(gSdlRenderer);
        gSdlRenderer = NULL;
    }
}

} // namespace fallout
