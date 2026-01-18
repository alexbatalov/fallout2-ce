#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <stddef.h>

#include "geometry.h"

namespace fallout {

// The maximum number of buttons in one radio group.
#define BUTTON_GROUP_BUTTON_LIST_CAPACITY (64)

typedef enum WindowManagerErr {
    WINDOW_MANAGER_OK = 0,
    WINDOW_MANAGER_ERR_INITIALIZING_VIDEO_MODE = 1,
    WINDOW_MANAGER_ERR_NO_MEMORY = 2,
    WINDOW_MANAGER_ERR_INITIALIZING_TEXT_FONTS = 3,
    WINDOW_MANAGER_ERR_WINDOW_SYSTEM_ALREADY_INITIALIZED = 4,
    WINDOW_MANAGER_ERR_WINDOW_SYSTEM_NOT_INITIALIZED = 5,
    WINDOW_MANAGER_ERR_CURRENT_WINDOWS_TOO_BIG = 6,
    WINDOW_MANAGER_ERR_INITIALIZING_DEFAULT_DATABASE = 7,

    // Unknown fatal error.
    //
    // NOTE: When this error code returned from window system initialization, the
    // game simply exits without any debug message. There is no way to figure out
    // it's meaning.
    WINDOW_MANAGER_ERR_8 = 8,
    WINDOW_MANAGER_ERR_ALREADY_RUNNING = 9,
    WINDOW_MANAGER_ERR_TITLE_NOT_SET = 10,
    WINDOW_MANAGER_ERR_INITIALIZING_INPUT = 11,
} WindowManagerErr;

typedef enum WindowFlags {
    // Use system window flags which are set during game startup and does not
    // change afterwards.
    WINDOW_USE_DEFAULTS = 0x1,
    WINDOW_DONT_MOVE_TOP = 0x2,
    WINDOW_MOVE_ON_TOP = 0x4,
    WINDOW_HIDDEN = 0x8,
    // Sfall calls this Exclusive.
    WINDOW_MODAL = 0x10,
    WINDOW_TRANSPARENT = 0x20,
    WINDOW_FLAG_0x40 = 0x40,

    /// Specifies that the window is draggable by clicking and moving anywhere
    /// in its background.
    WINDOW_DRAGGABLE_BY_BACKGROUND = 0x80,
    WINDOW_MANAGED = 0x100,
} WindowFlags;

typedef enum ButtonFlags {
    BUTTON_FLAG_0x01 = 0x01,
    BUTTON_FLAG_0x02 = 0x02,
    BUTTON_FLAG_0x04 = 0x04,
    BUTTON_FLAG_DISABLED = 0x08,

    /// Specifies that the button is a drag handle for parent window.
    BUTTON_DRAG_HANDLE = 0x10,
    BUTTON_FLAG_TRANSPARENT = 0x20,
    BUTTON_FLAG_0x40 = 0x40,
    BUTTON_FLAG_GRAPHIC = 0x010000,
    BUTTON_FLAG_CHECKED = 0x020000,
    BUTTON_FLAG_RADIO = 0x040000,
    BUTTON_FLAG_RIGHT_MOUSE_BUTTON_CONFIGURED = 0x080000,
} ButtonFlags;

typedef struct MenuPulldown {
    Rect rect;
    int keyCode;
    int itemsLength;
    char** items;
    int foregroundColor;
    int backgroundColor;
} MenuPulldown;

typedef struct MenuBar {
    int win;
    Rect rect;
    int pulldownsLength;
    MenuPulldown pulldowns[15];
    int foregroundColor;
    int backgroundColor;
} MenuBar;

typedef void WindowBlitProc(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);

typedef struct Button Button;
typedef struct ButtonGroup ButtonGroup;

typedef struct Window {
    int id;
    int flags;
    Rect rect;
    int width;
    int height;
    int color;
    int tx;
    int ty;
    unsigned char* buffer;
    Button* buttonListHead;
    Button* hoveredButton;
    Button* clickedButton;
    MenuBar* menuBar;
    WindowBlitProc* blitProc;
} Window;

typedef void ButtonCallback(int btn, int keyCode);
typedef void RadioButtonCallback(int btn);

typedef struct Button {
    int id;
    int flags;
    Rect rect;
    int mouseEnterEventCode;
    int mouseExitEventCode;
    int lefMouseDownEventCode;
    int leftMouseUpEventCode;
    int rightMouseDownEventCode;
    int rightMouseUpEventCode;
    unsigned char* normalImage;
    unsigned char* pressedImage;
    unsigned char* hoverImage;
    unsigned char* disabledNormalImage;
    unsigned char* disabledPressedImage;
    unsigned char* disabledHoverImage;
    unsigned char* currentImage;
    unsigned char* mask;
    ButtonCallback* mouseEnterProc;
    ButtonCallback* mouseExitProc;
    ButtonCallback* leftMouseDownProc;
    ButtonCallback* leftMouseUpProc;
    ButtonCallback* rightMouseDownProc;
    ButtonCallback* rightMouseUpProc;
    ButtonCallback* pressSoundFunc;
    ButtonCallback* releaseSoundFunc;
    ButtonGroup* buttonGroup;
    Button* prev;
    Button* next;
} Button;

typedef struct ButtonGroup {
    int maxChecked;
    int currChecked;
    RadioButtonCallback* func;
    int buttonsLength;
    Button* buttons[BUTTON_GROUP_BUTTON_LIST_CAPACITY];
} ButtonGroup;

typedef int(VideoSystemInitProc)();
typedef void(VideoSystemExitProc)();

extern bool gWindowSystemInitialized;
extern int _GNW_wcolor[6];

int windowManagerInit(VideoSystemInitProc* videoSystemInitProc, VideoSystemExitProc* videoSystemExitProc, int flags);
void windowManagerExit(void);
int windowCreate(int x, int y, int width, int height, int color, int flags);
void windowDestroy(int win);
void windowDrawBorder(int win);
void windowDrawText(int win, const char* str, int a3, int x, int y, int a6);
void _win_text(int win, char** fileNameList, int fileNameListLength, int maxWidth, int x, int y, int flags);
void windowDrawLine(int win, int left, int top, int right, int bottom, int color);
void windowDrawRect(int win, int left, int top, int right, int bottom, int color);
void windowFill(int win, int x, int y, int width, int height, int color);
void windowShow(int win);
void windowHide(int win);
void windowRefresh(int win);
void windowRefreshRect(int win, const Rect* rect);
void _GNW_win_refresh(Window* window, Rect* rect, unsigned char* a3);
void windowRefreshAll(Rect* rect);
void _win_get_mouse_buf(unsigned char* dest);
Window* windowGetWindow(int win);
unsigned char* windowGetBuffer(int win);
int windowGetAtPoint(int x, int y);
int windowGetWidth(int win);
int windowGetHeight(int win);
int windowGetRect(int win, Rect* rect);
int _win_check_all_buttons();
int _GNW_check_menu_bars(int input);
void programWindowSetTitle(const char* title);
bool showMesageBox(const char* str);
int buttonCreate(int win, int x, int y, int width, int height, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, unsigned char* up, unsigned char* dn, unsigned char* hover, int flags);
int _win_register_text_button(int win, int x, int y, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, const char* title, int flags);
int _win_register_button_disable(int btn, unsigned char* up, unsigned char* down, unsigned char* hover);
int _win_register_button_image(int btn, unsigned char* up, unsigned char* down, unsigned char* hover, bool draw);
int buttonSetMouseCallbacks(int btn, ButtonCallback* mouseEnterProc, ButtonCallback* mouseExitProc, ButtonCallback* mouseDownProc, ButtonCallback* mouseUpProc);
int buttonSetRightMouseCallbacks(int btn, int rightMouseDownEventCode, int rightMouseUpEventCode, ButtonCallback* rightMouseDownProc, ButtonCallback* rightMouseUpProc);
int buttonSetCallbacks(int btn, ButtonCallback* pressSoundFunc, ButtonCallback* releaseSoundFunc);
int buttonSetMask(int btn, unsigned char* mask);
bool _win_button_down(int btn);
int buttonGetWindowId(int btn);
int _win_last_button_winID();
int buttonDestroy(int btn);
int buttonEnable(int btn);
int buttonDisable(int btn);
int _win_set_button_rest_state(int btn, bool checked, int flags);
int _win_group_radio_buttons(int buttonCount, int* btns);
int _win_button_press_and_release(int btn);

} // namespace fallout

#endif /* WINDOW_MANAGER_H */
