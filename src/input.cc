#include <string>
#include <queue>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "input.h"
#include "audio_engine.h"
#include "color.h"
#include "delay.h"
#include "dinput.h"
#include "draw.h"
#include "kb.h"
#include "memory.h"
#include "mouse.h"
#include "svga.h"
#include "text_font.h"
#include "touch.h"
#include "vcr.h"
#include "win32.h"

#ifdef __SWITCH__
#include "worldmap.h"
#endif

namespace fallout {

#ifdef __SWITCH__
int lastControllerTime = 0;

const int CONTROLLER_R_DEADZONE = 8000;

int touchMouseDeltaX = 0;
int touchMouseDeltaY = 0;
static bool textInputActive = false;
static std::string textInputBuffer;
static std::queue<SDL_TextInputEvent> textInputQueue;

PadState pad;

enum class HidControllerButtons {
    KEY_A,
    KEY_B,
    KEY_X,
    KEY_Y,
    KEY_PLUS,
    KEY_MINUS,
    KEY_LSTICK,
    KEY_RSTICK,
    KEY_DPAD_UP,
    KEY_DPAD_DOWN,
    KEY_L,
    KEY_R,
    KEY_ZL,
    KEY_ZR
};

#endif

typedef struct InputEvent {
    // This is either logical key or input event id, which can be either
    // character code pressed or some other numbers used throughout the
    // game interface.
    int logicalKey;
    int mouseX;
    int mouseY;
} InputEvent;

typedef struct RepeatInfo {
    int tick;
    int repeatCount;
} RepeatInfo;

typedef struct TickerListNode {
    int flags;
    TickerProc* proc;
    struct TickerListNode* next;
} TickerListNode;

static int dequeueInputEvent();
static void pauseGame();
static int pauseHandlerDefaultImpl();
static void screenshotBlitter(unsigned char* src, int src_pitch, int a3, int x, int y, int width, int height, int dest_x, int dest_y);
static void buildNormalizedQwertyKeys();
static void _GNW95_process_key(KeyboardData* data);

static void idleImpl();

// 0x51E234
static IdleFunc* _idle_func = nullptr;

// 0x51E238
static FocusFunc* _focus_func = nullptr;

// 0x51E23C
static int gKeyboardKeyRepeatRate = 80;

// 0x51E240
static int gKeyboardKeyRepeatDelay = 500;

// A map of SDL_SCANCODE_* constants normalized for QWERTY keyboard.
//
// 0x6ABC70
static int gNormalizedQwertyKeys[SDL_NUM_SCANCODES];

// Ring buffer of input events.
//
// Looks like this buffer does not support overwriting of values. Once the
// buffer is full it will not overwrite values until they are dequeued.
//
// 0x6ABD70
static InputEvent gInputEventQueue[40];

// 0x6ABF50
static RepeatInfo _GNW95_key_time_stamps[SDL_NUM_SCANCODES];

// 0x6AC750
static int _input_mx;

// 0x6AC754
static int _input_my;

// 0x6AC75C
static bool gPaused;

// 0x6AC760
static int gScreenshotKeyCode;

// 0x6AC764
static int _using_msec_timer;

// 0x6AC768
static int gPauseKeyCode;

// 0x6AC76C
static ScreenshotHandler* gScreenshotHandler;

// 0x6AC770
static int gInputEventQueueReadIndex;

// 0x6AC774
static unsigned char* gScreenshotBuffer;

// 0x6AC778
static PauseHandler* gPauseHandler;

// 0x6AC77C
static int gInputEventQueueWriteIndex;

// 0x6AC780
static bool gRunLoopDisabled;

// 0x6AC784
static TickerListNode* gTickerListHead;

// 0x6AC788
static unsigned int gTickerLastTimestamp;

// 0x4C8A70
int inputInit(int a1)
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
    gTickerListHead = nullptr;
    gScreenshotKeyCode = KEY_ALT_C;

    // SFALL: Set idle function.
    // CE: Prevents frying CPU when window is not focused.
    inputSetIdleFunc(idleImpl);

    #ifdef __SWITCH__
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    #endif

    return 0;
}

// 0x4C8B40
void inputExit()
{
    _GNW95_input_init();
    mouseFree();
    keyboardFree();
    directInputFree();

    TickerListNode* curr = gTickerListHead;
    while (curr != nullptr) {
        TickerListNode* next = curr->next;
        internal_free(curr);
        curr = next;
    }
}

// 0x4C8B78
int inputGetInput()
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

// 0x4C8BC8
void get_input_position(int* x, int* y)
{
    *x = _input_mx;
    *y = _input_my;
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
static int dequeueInputEvent()
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

    while (curr != nullptr) {
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
    while (curr != nullptr) {
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
    while (curr != nullptr) {
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
static void pauseGame()
{
    if (!gPaused) {
        gPaused = true;

        int win = gPauseHandler();

        while (inputGetInput() != KEY_ESCAPE) {
        }

        gPaused = false;
        windowDestroy(win);
    }
}

// 0x4C8E38
static int pauseHandlerDefaultImpl()
{
    int windowWidth = fontGetStringWidth("Paused") + 32;
    int windowHeight = 3 * fontGetLineHeight() + 16;

    int win = windowCreate((rectGetWidth(&_scr_size) - windowWidth) / 2,
        (rectGetHeight(&_scr_size) - windowHeight) / 2,
        windowWidth,
        windowHeight,
        256,
        WINDOW_FLAG_0x40 | WINDOW_FLAG_0x40); // Replace with valid flags
                                                //TODO: wtf is this

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

    if (handler == nullptr) {
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
    if (gScreenshotBuffer == nullptr) {
        return;
    }

    WINDOWDRAWINGPROC v0 = _scr_blit;
    _scr_blit = screenshotBlitter;

    WINDOWDRAWINGPROC v2 = _mouse_blit;
    _mouse_blit = screenshotBlitter;

    WindowDrawingProc2* v1 = _mouse_blit_trans;
    _mouse_blit_trans = nullptr;

    windowRefreshAll(&_scr_size);

    _mouse_blit_trans = v1;
    _mouse_blit = v2;
    _scr_blit = v0;

    unsigned char* palette = _getSystemPalette();
    gScreenshotHandler(width, height, gScreenshotBuffer, palette);
    internal_free(gScreenshotBuffer);
}

// 0x4C8FF0
static void screenshotBlitter(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int width, int height, int destX, int destY)
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
        snprintf(fileName, sizeof(fileName), "scr%.5d.bmp", index);

        stream = compat_fopen(fileName, "rb");
        if (stream == nullptr) {
            break;
        }

        fclose(stream);
    }

    if (index == 100000) {
        return -1;
    }

    stream = compat_fopen(fileName, "wb");
    if (stream == nullptr) {
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

    if (handler == nullptr) {
        handler = screenshotHandlerDefaultImpl;
    }

    gScreenshotHandler = handler;
}

// 0x4C9370
unsigned int getTicks()
{
    return SDL_GetTicks();
}

// 0x4C937C
void inputPauseForTocks(unsigned int delay)
{
    // NOTE: Uninline.
    unsigned int start = getTicks();
    unsigned int end = getTicks();

    // NOTE: Uninline.
    unsigned int diff = getTicksBetween(end, start);
    while (diff < delay) {
        _process_bk();

        end = getTicks();

        // NOTE: Uninline.
        diff = getTicksBetween(end, start);
    }
}

// 0x4C93B8
void inputBlockForTocks(unsigned int ms)
{
    delay_ms(ms);
}

// 0x4C93E0
unsigned int getTicksSince(unsigned int start)
{
    unsigned int end = SDL_GetTicks();

    // NOTE: Uninline.
    return getTicksBetween(end, start);
}

unsigned int getTicksBetween(unsigned int end, unsigned int start) {
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
static void buildNormalizedQwertyKeys()
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
    // keys[DIK_KANA] = -1;
    // keys[DIK_CONVERT] = -1;
    // keys[DIK_NOCONVERT] = -1;
    // keys[DIK_YEN] = -1;
    keys[SDL_SCANCODE_KP_EQUALS] = -1;
    // keys[DIK_PREVTRACK] = -1;
    // keys[DIK_AT] = -1;
    // keys[DIK_COLON] = -1;
    // keys[DIK_UNDERLINE] = -1;
    // keys[DIK_KANJI] = -1;
    keys[SDL_SCANCODE_STOP] = -1;
    // keys[DIK_AX] = -1;
    // keys[DIK_UNLABELED] = -1;
    keys[SDL_SCANCODE_KP_ENTER] = SDL_SCANCODE_KP_ENTER;
    keys[SDL_SCANCODE_RCTRL] = SDL_SCANCODE_RCTRL;
    keys[SDL_SCANCODE_KP_COMMA] = -1;
    keys[SDL_SCANCODE_KP_DIVIDE] = SDL_SCANCODE_KP_DIVIDE;
    // keys[DIK_SYSRQ] = 84;
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
            touch_handle_start(&(e.tfinger));
            break;
        case SDL_FINGERMOTION:
            touch_handle_move(&(e.tfinger));
            break;
        case SDL_FINGERUP:
            touch_handle_end(&(e.tfinger));
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (!keyboardIsDisabled() && !textInputActive) {
                keyboardData.key = e.key.keysym.scancode;
                keyboardData.down = (e.key.state & SDL_PRESSED) != 0;
                _GNW95_process_key(&keyboardData);
            }
            break;
        case SDL_TEXTINPUT:
            #ifdef __SWITCH__
            handleTextInputEvent(e.text);
            #endif
            break;
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_EXPOSED:
                windowRefreshAll(&_scr_size);
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                handleWindowSizeChanged();
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

    touch_process_gesture();

    if (!textInputActive && gProgramIsActive && !keyboardIsDisabled()) {
        // NOTE: Uninline
        int tick = getTicks();

        for (int key = 0; key < SDL_NUM_SCANCODES; key++) {
            RepeatInfo* ptr = &(_GNW95_key_time_stamps[key]);
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

#ifdef __SWITCH__
    padUpdate(&pad);
    uint64_t kDown = padGetButtonsDown(&pad);
    uint64_t kUp = padGetButtonsUp(&pad);
    uint64_t kHeld = padGetButtons(&pad);

    handleSwitchControllerEvents(kDown, kUp, kHeld);
    processTextInputQueue();
#endif
}

void _GNW95_clear_time_stamps()
{
    for (int index = 0; index < SDL_NUM_SCANCODES; index++) {
        _GNW95_key_time_stamps[index].tick = -1;
        _GNW95_key_time_stamps[index].repeatCount = 0;
    }
}

// 0x4C9E14
static void _GNW95_process_key(KeyboardData* data)
{
    // Use originally pressed scancode, not qwerty-remapped one, for tracking
    // timestamps, see usage from |_GNW95_process_message|.
    int scanCode = data->key;

    data->key = gNormalizedQwertyKeys[data->key];

    if (gVcrState == VCR_STATE_PLAYING) {
        if ((gVcrTerminateFlags & VCR_TERMINATE_ON_KEY_PRESS) != 0) {
            gVcrPlaybackCompletionReason = VCR_PLAYBACK_COMPLETION_REASON_TERMINATED;
            vcrStop();
        }
    } else {
        RepeatInfo* ptr = &(_GNW95_key_time_stamps[scanCode]);
        if (data->down == 1) {
            ptr->tick = getTicks();
            ptr->repeatCount = 0;
        } else {
            ptr->tick = -1;
        }

        // Ignore keys which were remapped to -1.
        if (data->key == -1) {
            return;
        }

        _kb_simulate_key(data);
    }
}

// 0x4C9EEC
void _GNW95_lost_focus()
{
    if (_focus_func != nullptr) {
        _focus_func(false);
    }

    while (!gProgramIsActive) {
        _GNW95_process_message();

        if (_idle_func != nullptr) {
            _idle_func();
        }
    }

    if (_focus_func != nullptr) {
        _focus_func(true);
    }
}

static void idleImpl()
{
    SDL_Delay(125);
}

void handleControllerAxisEvent(const HidAnalogStickState& rightStick)
{
    if (textInputActive) return;

    //todo: gotta be a better way to do this lmao..
    const uint32_t currentTime = SDL_GetTicks();
    lastControllerTime = currentTime;

    KeyboardData keyboardData;

    // Priority: Up > Down > Right > Left
    if (rightStick.y > CONTROLLER_R_DEADZONE && abs(rightStick.y) > abs(rightStick.x)) {
        keyboardData.key = SDL_SCANCODE_UP;
        keyboardData.down = 1;
        _GNW95_process_key(&keyboardData);
        
        keyboardData.key = SDL_SCANCODE_DOWN;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
        keyboardData.key = SDL_SCANCODE_LEFT;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
        keyboardData.key = SDL_SCANCODE_RIGHT;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
    } 
    else if (rightStick.y < -CONTROLLER_R_DEADZONE && abs(rightStick.y) > abs(rightStick.x)) {
        keyboardData.key = SDL_SCANCODE_DOWN;
        keyboardData.down = 1;
        _GNW95_process_key(&keyboardData);

        keyboardData.key = SDL_SCANCODE_UP;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
        keyboardData.key = SDL_SCANCODE_LEFT;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
        keyboardData.key = SDL_SCANCODE_RIGHT;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
    } 
    else if (rightStick.x > CONTROLLER_R_DEADZONE) {
        keyboardData.key = SDL_SCANCODE_RIGHT;
        keyboardData.down = 1;
        _GNW95_process_key(&keyboardData);

        keyboardData.key = SDL_SCANCODE_UP;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
        keyboardData.key = SDL_SCANCODE_DOWN;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
        keyboardData.key = SDL_SCANCODE_LEFT;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
    } 
    else if (rightStick.x < -CONTROLLER_R_DEADZONE) {
        keyboardData.key = SDL_SCANCODE_LEFT;
        keyboardData.down = 1;
        _GNW95_process_key(&keyboardData);

        keyboardData.key = SDL_SCANCODE_UP;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
        keyboardData.key = SDL_SCANCODE_DOWN;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
        keyboardData.key = SDL_SCANCODE_RIGHT;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
    } 
    else {
        keyboardData.key = SDL_SCANCODE_UP;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
        keyboardData.key = SDL_SCANCODE_DOWN;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
        keyboardData.key = SDL_SCANCODE_LEFT;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
        keyboardData.key = SDL_SCANCODE_RIGHT;
        keyboardData.down = 0;
        _GNW95_process_key(&keyboardData);
    }
}

void handleTextInputEvent(const SDL_TextInputEvent& textEvent) {
    textInputQueue.push(textEvent);
}

void processTextInputQueue() {
    while (!textInputQueue.empty()) {
        SDL_TextInputEvent textEvent = textInputQueue.front();
        textInputQueue.pop();

        for (const char* ch = textEvent.text; *ch != '\0'; ++ch) {
            SDL_Scancode scancode = mapCharToScancode(*ch);
            if (scancode == SDL_SCANCODE_UNKNOWN) {
                continue;
            }

            simulateKeyEvent(scancode, *ch);
        }
    }
}

SDL_Scancode mapCharToScancode(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return static_cast<SDL_Scancode>(SDL_SCANCODE_A + (ch - 'a'));
    }

    if (ch >= 'A' && ch <= 'Z') {
        return static_cast<SDL_Scancode>(SDL_SCANCODE_A + (ch - 'A'));
    }

    if (ch >= '0' && ch <= '9') {
        return static_cast<SDL_Scancode>(SDL_SCANCODE_0 + (ch - '0'));
    }

    switch (ch) {
        case ' ': return SDL_SCANCODE_SPACE;
        case '-': return SDL_SCANCODE_MINUS;
        case '=': return SDL_SCANCODE_EQUALS;
        case '[': return SDL_SCANCODE_LEFTBRACKET;
        case ']': return SDL_SCANCODE_RIGHTBRACKET;
        case '\\': return SDL_SCANCODE_BACKSLASH;
        case ';': return SDL_SCANCODE_SEMICOLON;
        case '\'': return SDL_SCANCODE_APOSTROPHE;
        case ',': return SDL_SCANCODE_COMMA;
        case '.': return SDL_SCANCODE_PERIOD;
        case '/': return SDL_SCANCODE_SLASH;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

void simulateKeyEvent(SDL_Scancode scancode, char ch) {
    KeyboardData keyboardData;
    bool isUppercase = (ch >= 'A' && ch <= 'Z');

    if (isUppercase) {
        keyboardData.key = SDL_SCANCODE_LSHIFT;
        keyboardData.down = true;
        _GNW95_process_key(&keyboardData);
    }

    keyboardData.key = scancode;
    keyboardData.down = true;
    _GNW95_process_key(&keyboardData);

    keyboardData.down = false;
    _GNW95_process_key(&keyboardData);

    if (isUppercase) {
        keyboardData.key = SDL_SCANCODE_LSHIFT;
        keyboardData.down = false;
        _GNW95_process_key(&keyboardData);
    }
}

void handleControllerButtonEvent(HidControllerButtons button, bool pressed) {
    if (textInputActive) return;
    
    KeyboardData keyboardData;

    switch (button) {
    case HidControllerButtons::KEY_A:
        // start combat
        keyboardData.key = SDL_SCANCODE_A;
        keyboardData.down = pressed ? 1 : 0;
        _GNW95_process_key(&keyboardData);
        break;
    case HidControllerButtons::KEY_B:
        // end turn
        keyboardData.key = SDL_SCANCODE_SPACE;
        keyboardData.down = pressed ? 1 : 0;
        _GNW95_process_key(&keyboardData);
        break;
    case HidControllerButtons::KEY_X:
        // Skilldex
        keyboardData.key = SDL_SCANCODE_S;
        keyboardData.down = pressed ? 1 : 0;
        _GNW95_process_key(&keyboardData);
        break;
    case HidControllerButtons::KEY_Y:
        // Inventory
        keyboardData.key = SDL_SCANCODE_I;
        keyboardData.down = pressed ? 1 : 0;
        _GNW95_process_key(&keyboardData);
        break;
    case HidControllerButtons::KEY_PLUS:
        // Pause menu
        keyboardData.key = SDL_SCANCODE_ESCAPE;
        keyboardData.down = pressed ? 1 : 0;
        _GNW95_process_key(&keyboardData);
        break;
    case HidControllerButtons::KEY_MINUS:
        // Character menu
        keyboardData.key = SDL_SCANCODE_C;
        keyboardData.down = pressed ? 1 : 0;
        _GNW95_process_key(&keyboardData);
        break;
    case HidControllerButtons::KEY_R:
        // Toggle cursor speedup
        cursorSpeedup = pressed ? 2.0f : 1.0f;
        break;
    case HidControllerButtons::KEY_LSTICK:
        // Toggle sneak mode
        keyboardData.key = SDL_SCANCODE_1;
        keyboardData.down = pressed ? 1 : 0;
        _GNW95_process_key(&keyboardData);
        break;
    case HidControllerButtons::KEY_RSTICK:
        // End combat
        keyboardData.key = SDL_SCANCODE_KP_ENTER;
        keyboardData.down = pressed ? 1 : 0;
        _GNW95_process_key(&keyboardData);
        break;
    case HidControllerButtons::KEY_DPAD_UP:
        // Pipboy 2000
        keyboardData.key = SDL_SCANCODE_P;
        keyboardData.down = pressed ? 1 : 0;
        _GNW95_process_key(&keyboardData);
        break;
    case HidControllerButtons::KEY_DPAD_DOWN:
        // Center on player character
        keyboardData.key = SDL_SCANCODE_HOME;
        keyboardData.down = pressed ? 1 : 0;
        _GNW95_process_key(&keyboardData);
        break;
    default:
        break;
    }
}

void handleSwitchControllerEvents(uint64_t kDown, uint64_t kUp, uint64_t kHeld) {
    
    // Map Nintendo Switch buttons to game actions
    if (kDown & HidNpadButton_A) handleControllerButtonEvent(HidControllerButtons::KEY_A, true);
    if (kUp & HidNpadButton_A) handleControllerButtonEvent(HidControllerButtons::KEY_A, false);

    if (kDown & HidNpadButton_B) handleControllerButtonEvent(HidControllerButtons::KEY_B, true);
    if (kUp & HidNpadButton_B) handleControllerButtonEvent(HidControllerButtons::KEY_B, false);

    if (kDown & HidNpadButton_X) handleControllerButtonEvent(HidControllerButtons::KEY_X, true);
    if (kUp & HidNpadButton_X) handleControllerButtonEvent(HidControllerButtons::KEY_X, false);

    if (kDown & HidNpadButton_Y) handleControllerButtonEvent(HidControllerButtons::KEY_Y, true);
    if (kUp & HidNpadButton_Y) handleControllerButtonEvent(HidControllerButtons::KEY_Y, false);

    if (kDown & HidNpadButton_Plus) handleControllerButtonEvent(HidControllerButtons::KEY_PLUS, true);
    if (kUp & HidNpadButton_Plus) handleControllerButtonEvent(HidControllerButtons::KEY_PLUS, false);

    if (kDown & HidNpadButton_Minus) handleControllerButtonEvent(HidControllerButtons::KEY_MINUS, true);
    if (kUp & HidNpadButton_Minus) handleControllerButtonEvent(HidControllerButtons::KEY_MINUS, false);

    if (kDown & HidNpadButton_StickL) handleControllerButtonEvent(HidControllerButtons::KEY_LSTICK, true);
    if (kUp & HidNpadButton_StickL) handleControllerButtonEvent(HidControllerButtons::KEY_LSTICK, false);

    if (kDown & HidNpadButton_StickR) handleControllerButtonEvent(HidControllerButtons::KEY_RSTICK, true);
    if (kUp & HidNpadButton_StickR) handleControllerButtonEvent(HidControllerButtons::KEY_RSTICK, false);

    if (kDown & HidNpadButton_Up) handleControllerButtonEvent(HidControllerButtons::KEY_DPAD_UP, true);
    if (kUp & HidNpadButton_Up) handleControllerButtonEvent(HidControllerButtons::KEY_DPAD_UP, false); 

    if (kDown & HidNpadButton_Down) handleControllerButtonEvent(HidControllerButtons::KEY_DPAD_DOWN, true);
    if (kUp & HidNpadButton_Down) handleControllerButtonEvent(HidControllerButtons::KEY_DPAD_DOWN, false);

    if (kDown & HidNpadButton_L) handleControllerButtonEvent(HidControllerButtons::KEY_L, true);
    if (kUp & HidNpadButton_L) handleControllerButtonEvent(HidControllerButtons::KEY_L, false);

    if (kDown & HidNpadButton_R) handleControllerButtonEvent(HidControllerButtons::KEY_R, true);
    if (kUp & HidNpadButton_R) handleControllerButtonEvent(HidControllerButtons::KEY_R, false);

    if (kDown & HidNpadButton_ZL) handleControllerButtonEvent(HidControllerButtons::KEY_ZL, true);
    if (kUp & HidNpadButton_ZL) handleControllerButtonEvent(HidControllerButtons::KEY_ZL, false);

    if (kDown & HidNpadButton_ZR) handleControllerButtonEvent(HidControllerButtons::KEY_ZR, true);
    if (kUp & HidNpadButton_ZR) handleControllerButtonEvent(HidControllerButtons::KEY_ZR, false);

    HidAnalogStickState rightStick = padGetStickPos(&pad, 1);

    handleControllerAxisEvent(rightStick);
}

void beginTextInput() {
    textInputActive = true;
    textInputBuffer.clear();

    // Clear the text input queue
    while (!textInputQueue.empty()) {
        textInputQueue.pop();
    }

    // Start SDL text input
    SDL_StartTextInput();
}

void endTextInput() {
    // Stop SDL text input
    SDL_StopTextInput();
    textInputActive = false;
}
} // namespace fallout
