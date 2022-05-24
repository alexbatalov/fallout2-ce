#ifndef CORE_H
#define CORE_H

#include "db.h"
#include "dinput.h"
#include "geometry.h"
#include "window.h"

#include <SDL.h>

#include <stdbool.h>

#define MOUSE_DEFAULT_CURSOR_WIDTH 8
#define MOUSE_DEFAULT_CURSOR_HEIGHT 8
#define MOUSE_DEFAULT_CURSOR_SIZE (MOUSE_DEFAULT_CURSOR_WIDTH * MOUSE_DEFAULT_CURSOR_HEIGHT)

#define MOUSE_STATE_LEFT_BUTTON_DOWN 0x01
#define MOUSE_STATE_RIGHT_BUTTON_DOWN 0x02

#define MOUSE_EVENT_LEFT_BUTTON_DOWN 0x01
#define MOUSE_EVENT_RIGHT_BUTTON_DOWN 0x02
#define MOUSE_EVENT_LEFT_BUTTON_REPEAT 0x04
#define MOUSE_EVENT_RIGHT_BUTTON_REPEAT 0x08
#define MOUSE_EVENT_LEFT_BUTTON_UP 0x10
#define MOUSE_EVENT_RIGHT_BUTTON_UP 0x20
#define MOUSE_EVENT_ANY_BUTTON_DOWN (MOUSE_EVENT_LEFT_BUTTON_DOWN | MOUSE_EVENT_RIGHT_BUTTON_DOWN)
#define MOUSE_EVENT_ANY_BUTTON_REPEAT (MOUSE_EVENT_LEFT_BUTTON_REPEAT | MOUSE_EVENT_RIGHT_BUTTON_REPEAT)
#define MOUSE_EVENT_ANY_BUTTON_UP (MOUSE_EVENT_LEFT_BUTTON_UP | MOUSE_EVENT_RIGHT_BUTTON_UP)
#define MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT (MOUSE_EVENT_LEFT_BUTTON_DOWN | MOUSE_EVENT_LEFT_BUTTON_REPEAT)
#define MOUSE_EVENT_RIGHT_BUTTON_DOWN_REPEAT (MOUSE_EVENT_RIGHT_BUTTON_DOWN | MOUSE_EVENT_RIGHT_BUTTON_REPEAT)

#define BUTTON_REPEAT_TIME 250

#define KEY_STATE_UP 0
#define KEY_STATE_DOWN 1
#define KEY_STATE_REPEAT 2

#define MODIFIER_KEY_STATE_NUM_LOCK 0x01
#define MODIFIER_KEY_STATE_CAPS_LOCK 0x02
#define MODIFIER_KEY_STATE_SCROLL_LOCK 0x04

#define KEYBOARD_EVENT_MODIFIER_CAPS_LOCK 0x0001
#define KEYBOARD_EVENT_MODIFIER_NUM_LOCK 0x0002
#define KEYBOARD_EVENT_MODIFIER_SCROLL_LOCK 0x0004
#define KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT 0x0008
#define KEYBOARD_EVENT_MODIFIER_RIGHT_SHIFT 0x0010
#define KEYBOARD_EVENT_MODIFIER_LEFT_ALT 0x0020
#define KEYBOARD_EVENT_MODIFIER_RIGHT_ALT 0x0040
#define KEYBOARD_EVENT_MODIFIER_LEFT_CONTROL 0x0080
#define KEYBOARD_EVENT_MODIFIER_RIGHT_CONTROL 0x0100
#define KEYBOARD_EVENT_MODIFIER_ANY_SHIFT (KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT | KEYBOARD_EVENT_MODIFIER_RIGHT_SHIFT)
#define KEYBOARD_EVENT_MODIFIER_ANY_ALT (KEYBOARD_EVENT_MODIFIER_LEFT_ALT | KEYBOARD_EVENT_MODIFIER_RIGHT_ALT)
#define KEYBOARD_EVENT_MODIFIER_ANY_CONTROL (KEYBOARD_EVENT_MODIFIER_LEFT_CONTROL | KEYBOARD_EVENT_MODIFIER_RIGHT_CONTROL)

#define KEY_QUEUE_SIZE 64

typedef enum Key {
    KEY_ESCAPE = '\x1b',
    KEY_TAB = '\x09',
    KEY_BACKSPACE = '\x08',
    KEY_RETURN = '\r',

    KEY_SPACE = ' ',
    KEY_EXCLAMATION = '!',
    KEY_QUOTE = '"',
    KEY_NUMBER_SIGN = '#',
    KEY_DOLLAR = '$',
    KEY_PERCENT = '%',
    KEY_AMPERSAND = '&',
    KEY_SINGLE_QUOTE = '\'',
    KEY_PAREN_LEFT = '(',
    KEY_PAREN_RIGHT = ')',
    KEY_ASTERISK = '*',
    KEY_PLUS = '+',
    KEY_COMMA = ',',
    KEY_MINUS = '-',
    KEY_DOT = '.',
    KEY_SLASH = '/',
    KEY_0 = '0',
    KEY_1 = '1',
    KEY_2 = '2',
    KEY_3 = '3',
    KEY_4 = '4',
    KEY_5 = '5',
    KEY_6 = '6',
    KEY_7 = '7',
    KEY_8 = '8',
    KEY_9 = '9',
    KEY_COLON = ':',
    KEY_SEMICOLON = ';',
    KEY_LESS = '<',
    KEY_EQUAL = '=',
    KEY_GREATER = '>',
    KEY_QUESTION = '?',
    KEY_AT = '@',
    KEY_UPPERCASE_A = 'A',
    KEY_UPPERCASE_B = 'B',
    KEY_UPPERCASE_C = 'C',
    KEY_UPPERCASE_D = 'D',
    KEY_UPPERCASE_E = 'E',
    KEY_UPPERCASE_F = 'F',
    KEY_UPPERCASE_G = 'G',
    KEY_UPPERCASE_H = 'H',
    KEY_UPPERCASE_I = 'I',
    KEY_UPPERCASE_J = 'J',
    KEY_UPPERCASE_K = 'K',
    KEY_UPPERCASE_L = 'L',
    KEY_UPPERCASE_M = 'M',
    KEY_UPPERCASE_N = 'N',
    KEY_UPPERCASE_O = 'O',
    KEY_UPPERCASE_P = 'P',
    KEY_UPPERCASE_Q = 'Q',
    KEY_UPPERCASE_R = 'R',
    KEY_UPPERCASE_S = 'S',
    KEY_UPPERCASE_T = 'T',
    KEY_UPPERCASE_U = 'U',
    KEY_UPPERCASE_V = 'V',
    KEY_UPPERCASE_W = 'W',
    KEY_UPPERCASE_X = 'X',
    KEY_UPPERCASE_Y = 'Y',
    KEY_UPPERCASE_Z = 'Z',

    KEY_BRACKET_LEFT = '[',
    KEY_BACKSLASH = '\\',
    KEY_BRACKET_RIGHT = ']',
    KEY_CARET = '^',
    KEY_UNDERSCORE = '_',

    KEY_GRAVE = '`',
    KEY_LOWERCASE_A = 'a',
    KEY_LOWERCASE_B = 'b',
    KEY_LOWERCASE_C = 'c',
    KEY_LOWERCASE_D = 'd',
    KEY_LOWERCASE_E = 'e',
    KEY_LOWERCASE_F = 'f',
    KEY_LOWERCASE_G = 'g',
    KEY_LOWERCASE_H = 'h',
    KEY_LOWERCASE_I = 'i',
    KEY_LOWERCASE_J = 'j',
    KEY_LOWERCASE_K = 'k',
    KEY_LOWERCASE_L = 'l',
    KEY_LOWERCASE_M = 'm',
    KEY_LOWERCASE_N = 'n',
    KEY_LOWERCASE_O = 'o',
    KEY_LOWERCASE_P = 'p',
    KEY_LOWERCASE_Q = 'q',
    KEY_LOWERCASE_R = 'r',
    KEY_LOWERCASE_S = 's',
    KEY_LOWERCASE_T = 't',
    KEY_LOWERCASE_U = 'u',
    KEY_LOWERCASE_V = 'v',
    KEY_LOWERCASE_W = 'w',
    KEY_LOWERCASE_X = 'x',
    KEY_LOWERCASE_Y = 'y',
    KEY_LOWERCASE_Z = 'z',
    KEY_BRACE_LEFT = '{',
    KEY_BAR = '|',
    KEY_BRACE_RIGHT = '}',
    KEY_TILDE = '~',
    KEY_DEL = 127,

    KEY_136 = 136,
    KEY_146 = 146,
    KEY_149 = 149,
    KEY_150 = 150,
    KEY_151 = 151,
    KEY_152 = 152,
    KEY_161 = 161,
    KEY_163 = 163,
    KEY_164 = 164,
    KEY_166 = 166,
    KEY_168 = 168,
    KEY_167 = 167,
    KEY_170 = 170,
    KEY_172 = 172,
    KEY_176 = 176,
    KEY_178 = 178,
    KEY_179 = 179,
    KEY_180 = 180,
    KEY_181 = 181,
    KEY_186 = 186,
    KEY_191 = 191,
    KEY_196 = 196,
    KEY_199 = 199,
    KEY_209 = 209,
    KEY_214 = 214,
    KEY_215 = 215,
    KEY_220 = 220,
    KEY_223 = 223,
    KEY_224 = 224,
    KEY_228 = 228,
    KEY_231 = 231,
    KEY_232 = 232,
    KEY_233 = 233,
    KEY_241 = 241,
    KEY_246 = 246,
    KEY_247 = 247,
    KEY_249 = 249,
    KEY_252 = 252,

    KEY_ALT_Q = 272,
    KEY_ALT_W = 273,
    KEY_ALT_E = 274,
    KEY_ALT_R = 275,
    KEY_ALT_T = 276,
    KEY_ALT_Y = 277,
    KEY_ALT_U = 278,
    KEY_ALT_I = 279,
    KEY_ALT_O = 280,
    KEY_ALT_P = 281,
    KEY_ALT_A = 286,
    KEY_ALT_S = 287,
    KEY_ALT_D = 288,
    KEY_ALT_F = 289,
    KEY_ALT_G = 290,
    KEY_ALT_H = 291,
    KEY_ALT_J = 292,
    KEY_ALT_K = 293,
    KEY_ALT_L = 294,
    KEY_ALT_Z = 300,
    KEY_ALT_X = 301,
    KEY_ALT_C = 302,
    KEY_ALT_V = 303,
    KEY_ALT_B = 304,
    KEY_ALT_N = 305,
    KEY_ALT_M = 306,

    KEY_CTRL_Q = 17,
    KEY_CTRL_W = 23,
    KEY_CTRL_E = 5,
    KEY_CTRL_R = 18,
    KEY_CTRL_T = 20,
    KEY_CTRL_Y = 25,
    KEY_CTRL_U = 21,
    KEY_CTRL_I = 9,
    KEY_CTRL_O = 15,
    KEY_CTRL_P = 16,
    KEY_CTRL_A = 1,
    KEY_CTRL_S = 19,
    KEY_CTRL_D = 4,
    KEY_CTRL_F = 6,
    KEY_CTRL_G = 7,
    KEY_CTRL_H = 8,
    KEY_CTRL_J = 10,
    KEY_CTRL_K = 11,
    KEY_CTRL_L = 12,
    KEY_CTRL_Z = 26,
    KEY_CTRL_X = 24,
    KEY_CTRL_C = 3,
    KEY_CTRL_V = 22,
    KEY_CTRL_B = 2,
    KEY_CTRL_N = 14,
    KEY_CTRL_M = 13,

    KEY_F1 = 315,
    KEY_F2 = 316,
    KEY_F3 = 317,
    KEY_F4 = 318,
    KEY_F5 = 319,
    KEY_F6 = 320,
    KEY_F7 = 321,
    KEY_F8 = 322,
    KEY_F9 = 323,
    KEY_F10 = 324,
    KEY_F11 = 389,
    KEY_F12 = 390,

    KEY_SHIFT_F1 = 340,
    KEY_SHIFT_F2 = 341,
    KEY_SHIFT_F3 = 342,
    KEY_SHIFT_F4 = 343,
    KEY_SHIFT_F5 = 344,
    KEY_SHIFT_F6 = 345,
    KEY_SHIFT_F7 = 346,
    KEY_SHIFT_F8 = 347,
    KEY_SHIFT_F9 = 348,
    KEY_SHIFT_F10 = 349,
    KEY_SHIFT_F11 = 391,
    KEY_SHIFT_F12 = 392,

    KEY_CTRL_F1 = 350,
    KEY_CTRL_F2 = 351,
    KEY_CTRL_F3 = 352,
    KEY_CTRL_F4 = 353,
    KEY_CTRL_F5 = 354,
    KEY_CTRL_F6 = 355,
    KEY_CTRL_F7 = 356,
    KEY_CTRL_F8 = 357,
    KEY_CTRL_F9 = 358,
    KEY_CTRL_F10 = 359,
    KEY_CTRL_F11 = 393,
    KEY_CTRL_F12 = 394,

    KEY_ALT_F1 = 360,
    KEY_ALT_F2 = 361,
    KEY_ALT_F3 = 362,
    KEY_ALT_F4 = 363,
    KEY_ALT_F5 = 364,
    KEY_ALT_F6 = 365,
    KEY_ALT_F7 = 366,
    KEY_ALT_F8 = 367,
    KEY_ALT_F9 = 368,
    KEY_ALT_F10 = 369,
    KEY_ALT_F11 = 395,
    KEY_ALT_F12 = 396,

    KEY_HOME = 327,
    KEY_CTRL_HOME = 375,
    KEY_ALT_HOME = 407,

    KEY_PAGE_UP = 329,
    KEY_CTRL_PAGE_UP = 388,
    KEY_ALT_PAGE_UP = 409,

    KEY_INSERT = 338,
    KEY_CTRL_INSERT = 402,
    KEY_ALT_INSERT = 418,

    KEY_DELETE = 339,
    KEY_CTRL_DELETE = 403,
    KEY_ALT_DELETE = 419,

    KEY_END = 335,
    KEY_CTRL_END = 373,
    KEY_ALT_END = 415,

    KEY_PAGE_DOWN = 337,
    KEY_ALT_PAGE_DOWN = 417,
    KEY_CTRL_PAGE_DOWN = 374,

    KEY_ARROW_UP = 328,
    KEY_CTRL_ARROW_UP = 397,
    KEY_ALT_ARROW_UP = 408,

    KEY_ARROW_DOWN = 336,
    KEY_CTRL_ARROW_DOWN = 401,
    KEY_ALT_ARROW_DOWN = 416,

    KEY_ARROW_LEFT = 331,
    KEY_CTRL_ARROW_LEFT = 371,
    KEY_ALT_ARROW_LEFT = 411,

    KEY_ARROW_RIGHT = 333,
    KEY_CTRL_ARROW_RIGHT = 372,
    KEY_ALT_ARROW_RIGHT = 413,

    KEY_CTRL_BACKSLASH = 192,

    KEY_NUMBERPAD_5 = 332,
    KEY_CTRL_NUMBERPAD_5 = 399,
    KEY_ALT_NUMBERPAD_5 = 9999,

    KEY_FIRST_INPUT_CHARACTER = KEY_SPACE,
    KEY_LAST_INPUT_CHARACTER = KEY_LOWERCASE_Z,
} Key;

typedef enum KeyboardLayout {
    KEYBOARD_LAYOUT_QWERTY,
    KEYBOARD_LAYOUT_FRENCH,
    KEYBOARD_LAYOUT_GERMAN,
    KEYBOARD_LAYOUT_ITALIAN,
    KEYBOARD_LAYOUT_SPANISH,
} KeyboardLayout;

typedef struct STRUCT_6ABF50 {
    // Time when appropriate key was pressed down or -1 if it's up.
    int tick;
    int repeatCount;
} STRUCT_6ABF50;

typedef struct InputEvent {
    // This is either logical key or input event id, which can be either
    // character code pressed or some other numbers used throughout the
    // game interface.
    int logicalKey;
    int mouseX;
    int mouseY;
} InputEvent;

typedef void TickerProc();

typedef struct TickerListNode {
    int flags;
    TickerProc* proc;
    struct TickerListNode* next;
} TickerListNode;

typedef struct STRUCT_51E2F0 {
    int type;
    int field_4;
    int field_8;
    union {
        struct {
            int type_1_field_C; // mouse x
            int type_1_field_10; // mouse y
            int type_1_field_14; // keyboard layout
        };
        struct {
            short type_2_field_C;
        };
        struct {
            int dx;
            int dy;
            int buttons;
        };
    };
} STRUCT_51E2F0;

static_assert(sizeof(STRUCT_51E2F0) == 24, "wrong size");

typedef struct LogicalKeyEntry {
    short field_0;
    short unmodified;
    short shift;
    short lmenu;
    short rmenu;
    short ctrl;
} LogicalKeyEntry;

typedef struct KeyboardEvent {
    int scanCode;
    unsigned short modifiers;
} KeyboardEvent;

typedef int(PauseHandler)();
typedef int(ScreenshotHandler)(int width, int height, unsigned char* buffer, unsigned char* palette);

extern void (*_idle_func)();
extern void (*_focus_func)(int);
extern int gKeyboardKeyRepeatRate;
extern int gKeyboardKeyRepeatDelay;
extern bool _keyboard_hooked;
extern unsigned char gMouseDefaultCursor[MOUSE_DEFAULT_CURSOR_SIZE];
extern int _mouse_idling;
extern unsigned char* gMouseCursorData;
extern unsigned char* _mouse_shape;
extern unsigned char* _mouse_fptr;
extern double gMouseSensitivity;
extern unsigned int _ticker_;
extern int gMouseButtonsState;

extern void (*_update_palette_func)();
extern bool gMmxEnabled;
extern bool gMmxProbed;

extern unsigned char _kb_installed;
extern bool gKeyboardDisabled;
extern bool gKeyboardNumpadDisabled;
extern bool gKeyboardNumlockDisabled;
extern int gKeyboardEventQueueWriteIndex;
extern int gKeyboardEventQueueReadIndex;
extern short word_51E2E8;
extern int gModifierKeysState;
extern int (*_kb_scan_to_ascii)();
extern STRUCT_51E2F0* _vcr_buffer;
extern int _vcr_buffer_index;
extern int _vcr_state;
extern int _vcr_time;
extern int _vcr_counter;
extern int _vcr_terminate_flags;
extern int _vcr_terminated_condition;
extern int _vcr_start_time;
extern int _vcr_registered_atexit;
extern File* _vcr_file;
extern int _vcr_buffer_end;

extern int gNormalizedQwertyKeys[SDL_NUM_SCANCODES];
extern InputEvent gInputEventQueue[40];
extern STRUCT_6ABF50 _GNW95_key_time_stamps[SDL_NUM_SCANCODES];
extern int _input_mx;
extern int _input_my;
extern HHOOK _GNW95_keyboardHandle;
extern bool gPaused;
extern int gScreenshotKeyCode;
extern int _using_msec_timer;
extern int gPauseKeyCode;
extern ScreenshotHandler* gScreenshotHandler;
extern int gInputEventQueueReadIndex;
extern unsigned char* gScreenshotBuffer;
extern PauseHandler* gPauseHandler;
extern int gInputEventQueueWriteIndex;
extern bool gRunLoopDisabled;
extern TickerListNode* gTickerListHead;
extern unsigned int gTickerLastTimestamp;
extern bool gCursorIsHidden;
extern int _raw_x;
extern int gMouseCursorHeight;
extern int _raw_y;
extern int _raw_buttons;
extern int gMouseCursorY;
extern int gMouseCursorX;
extern int _mouse_disabled;
extern int gMouseEvent;
extern unsigned int _mouse_speed;
extern int _mouse_curr_frame;
extern bool gMouseInitialized;
extern int gMouseCursorPitch;
extern int gMouseCursorWidth;
extern int _mouse_num_frames;
extern int _mouse_hoty;
extern int _mouse_hotx;
extern unsigned int _mouse_idle_start_time;
extern WindowDrawingProc2* _mouse_blit_trans;
extern WINDOWDRAWINGPROC _mouse_blit;
extern unsigned char _mouse_trans;
extern int gMouseRightButtonDownTimestamp;
extern int gMouseLeftButtonDownTimestamp;
extern int gMousePreviousEvent;
extern unsigned short gSixteenBppPalette[256];
extern Rect _scr_size;
extern int gRedMask;
extern int gGreenMask;
extern int gBlueMask;
extern int gBlueShift;
extern int gRedShift;
extern int gGreenShift;
extern void (*_scr_blit)(unsigned char* src, int src_pitch, int a3, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y);
extern void (*_zero_mem)();
extern bool gMmxSupported;
extern unsigned char gLastVideoModePalette[268];
extern KeyboardEvent gKeyboardEventsQueue[KEY_QUEUE_SIZE];
extern LogicalKeyEntry gLogicalKeyEntries[SDL_NUM_SCANCODES];
extern unsigned char gPressedPhysicalKeys[SDL_NUM_SCANCODES];
extern unsigned int _kb_idle_start_time;
extern KeyboardEvent gLastKeyboardEvent;
extern int gKeyboardLayout;
extern unsigned char gPressedPhysicalKeysCount;

extern SDL_Window* gSdlWindow;
extern SDL_Surface* gSdlWindowSurface;
extern SDL_Surface* gSdlSurface;

int coreInit(int a1);
void coreExit();
int _get_input();
void _process_bk();
void enqueueInputEvent(int a1);
int dequeueInputEvent();
void inputEventQueueReset();
void tickersExecute();
void tickersAdd(TickerProc* fn);
void tickersRemove(TickerProc* fn);
void tickersEnable();
void tickersDisable();
void pauseGame();
int pauseHandlerDefaultImpl();
void pauseHandlerConfigure(int keyCode, PauseHandler* fn);
void takeScreenshot();
void screenshotBlitter(unsigned char* src, int src_pitch, int a3, int x, int y, int width, int height, int dest_x, int dest_y);
int screenshotHandlerDefaultImpl(int width, int height, unsigned char* data, unsigned char* palette);
void screenshotHandlerConfigure(int keyCode, ScreenshotHandler* handler);
unsigned int _get_time();
void coreDelayProcessingEvents(unsigned int ms);
void coreDelay(unsigned int ms);
unsigned int getTicksSince(unsigned int a1);
unsigned int getTicksBetween(unsigned int a1, unsigned int a2);
unsigned int _get_bk_time();
void buildNormalizedQwertyKeys();
int _GNW95_input_init();
void _GNW95_process_message();
void _GNW95_clear_time_stamps();
void _GNW95_process_key(KeyboardData* data);
void _GNW95_lost_focus();
int mouseInit();
void mouseFree();
void mousePrepareDefaultCursor();
int mouseSetFrame(unsigned char* a1, int width, int height, int pitch, int a5, int a6, int a7);
void _mouse_anim();
void mouseShowCursor();
void mouseHideCursor();
void _mouse_info();
void _mouse_simulate_input(int delta_x, int delta_y, int buttons);
bool _mouse_in(int left, int top, int right, int bottom);
bool _mouse_click_in(int left, int top, int right, int bottom);
void mouseGetRect(Rect* rect);
void mouseGetPosition(int* out_x, int* out_y);
void _mouse_set_position(int a1, int a2);
void _mouse_clip();
int mouseGetEvent();
bool cursorIsHidden();
void _mouse_get_raw_state(int* out_x, int* out_y, int* out_buttons);
void mouseSetSensitivity(double value);
void mmxSetEnabled(bool a1);
int _init_mode_320_200();
int _init_mode_320_400();
int _init_mode_640_480_16();
int _init_mode_640_480();
int _init_mode_640_400();
int _init_mode_800_600();
int _init_mode_1024_768();
int _init_mode_1280_1024();
void _get_start_mode_();
void _zero_vid_mem();
int _GNW95_init_mode_ex(int width, int height, int bpp);
int _init_vesa_mode(int width, int height);
int _GNW95_init_window(int width, int height, bool fullscreen);
int getShiftForBitMask(int mask);
int directDrawInit(int width, int height, int bpp);
void directDrawFree();
void directDrawSetPaletteInRange(unsigned char* a1, int a2, int a3);
void directDrawSetPalette(unsigned char* palette);
unsigned char* directDrawGetPalette();
void _GNW95_ShowRect(unsigned char* src, int src_pitch, int a3, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y);
void _GNW95_MouseShowRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY);
void _GNW95_ShowRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY);
void _GNW95_MouseShowTransRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY, unsigned char keyColor);
void _GNW95_zero_vid_mem();
int keyboardInit();
void keyboardFree();
void keyboardReset();
int _kb_getch();
void keyboardDisable();
void keyboardEnable();
int keyboardIsDisabled();
void keyboardSetLayout(int new_language);
int keyboardGetLayout();
void _kb_simulate_key(KeyboardData* data);
int _kb_next_ascii_English_US();
int keyboardDequeueLogicalKeyCode();
void keyboardBuildQwertyConfiguration();
void keyboardBuildFrenchConfiguration();
void keyboardBuildGermanConfiguration();
void keyboardBuildItalianConfiguration();
void keyboardBuildSpanishConfiguration();
void _kb_init_lock_status();
int keyboardPeekEvent(int index, KeyboardEvent** keyboardEventPtr);
bool _vcr_record(const char* fileName);
void _vcr_stop();
int _vcr_status();
int _vcr_update();
bool _vcr_clear_buffer();
int _vcr_dump_buffer();
bool _vcr_save_record(STRUCT_51E2F0* ptr, File* stream);
bool _vcr_load_record(STRUCT_51E2F0* ptr, File* stream);

int screenGetWidth();
int screenGetHeight();
int screenGetVisibleHeight();
void mouseGetPositionInWindow(int win, int* x, int* y);
bool mouseHitTestInWindow(int win, int left, int top, int right, int bottom);

#endif /* CORE_H */
