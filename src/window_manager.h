#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include "geometry.h"

#include <stddef.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define MAX_WINDOW_COUNT (50)

// The maximum number of radio groups.
#define RADIO_GROUP_LIST_CAPACITY (64)

// The maximum number of buttons in one radio group.
#define RADIO_GROUP_BUTTON_LIST_CAPACITY (64)

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
    WINDOW_FLAG_0x01 = 0x01,
    WINDOW_FLAG_0x02 = 0x02,
    WINDOW_FLAG_0x04 = 0x04,
    WINDOW_HIDDEN = 0x08,
    WINDOW_FLAG_0x10 = 0x10,
    WINDOW_FLAG_0x20 = 0x20,
    WINDOW_FLAG_0x40 = 0x40,
    WINDOW_FLAG_0x80 = 0x80,
    WINDOW_FLAG_0x0100 = 0x0100,
} WindowFlags;

typedef enum ButtonFlags {
    BUTTON_FLAG_0x01 = 0x01,
    BUTTON_FLAG_0x02 = 0x02,
    BUTTON_FLAG_0x04 = 0x04,
    BUTTON_FLAG_DISABLED = 0x08,
    BUTTON_FLAG_0x10 = 0x10,
    BUTTON_FLAG_TRANSPARENT = 0x20,
    BUTTON_FLAG_0x40 = 0x40,
    BUTTON_FLAG_0x010000 = 0x010000,
    BUTTON_FLAG_0x020000 = 0x020000,
    BUTTON_FLAG_0x040000 = 0x040000,
    BUTTON_FLAG_RIGHT_MOUSE_BUTTON_CONFIGURED = 0x080000,
} ButtonFlags;

typedef struct struc_176 {
    int field_0;
    int field_4;
    int field_8;
    int field_C;
    int field_10;
    int field_14;
    int field_18;
    int field_1C;
    int field_20;
} struc_176;

typedef struct struc_177 {
    int win;
    Rect rect;
    int entriesCount;
    struc_176 entries[15];
    int field_234;
    int field_238;
} struc_177;

typedef void WindowBlitProc(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);

typedef struct Button Button;
typedef struct RadioGroup RadioGroup;

typedef struct Window {
    int id;
    int flags;
    Rect rect;
    int width;
    int height;
    int field_20;
    // rand
    int field_24;
    // rand
    int field_28;
    unsigned char* buffer;
    Button* buttonListHead;
    Button* field_34;
    Button* field_38;
    struc_177* field_3C;
    WindowBlitProc* blitProc;
} Window;

typedef void ButtonCallback(int btn, int keyCode);

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
    unsigned char* mouseUpImage;
    unsigned char* mouseDownImage;
    unsigned char* mouseHoverImage;
    unsigned char* field_3C;
    unsigned char* field_40;
    unsigned char* field_44;
    unsigned char* currentImage;
    unsigned char* mask;
    ButtonCallback* mouseEnterProc;
    ButtonCallback* mouseExitProc;
    ButtonCallback* leftMouseDownProc;
    ButtonCallback* leftMouseUpProc;
    ButtonCallback* rightMouseDownProc;
    ButtonCallback* rightMouseUpProc;
    ButtonCallback* onPressed;
    ButtonCallback* onUnpressed;
    RadioGroup* radioGroup;
    Button* prev;
    Button* next;
} Button;

typedef struct RadioGroup {
    int field_0;
    int field_4;
    void (*field_8)(int);
    int buttonsLength;
    Button* buttons[RADIO_GROUP_BUTTON_LIST_CAPACITY];
} RadioGroup;

typedef int(VideoSystemInitProc)();
typedef void(VideoSystemExitProc)();

extern char _path_patches[];

extern bool _GNW95_already_running;
extern HANDLE _GNW95_title_mutex;
extern bool gWindowSystemInitialized;
extern int _GNW_wcolor[6];
extern unsigned char* _screen_buffer;
extern bool _insideWinExit;
extern int _last_button_winID;

extern int gOrderedWindowIds[MAX_WINDOW_COUNT];
extern Window* gWindows[MAX_WINDOW_COUNT];
extern VideoSystemExitProc* gVideoSystemExitProc;
extern int gWindowsLength;
extern int _window_flags;
extern bool _buffering;
extern int _bk_color;
extern VideoSystemInitProc* gVideoSystemInitProc;
extern int _doing_refresh_all;
extern void* _GNW_texture;
extern RadioGroup gRadioGroups[RADIO_GROUP_LIST_CAPACITY];

int windowManagerInit(VideoSystemInitProc* videoSystemInitProc, VideoSystemExitProc* videoSystemExitProc, int a3);
void windowManagerExit(void);
int windowCreate(int x, int y, int width, int height, int a4, int flags);
void windowDestroy(int win);
void windowFree(int win);
void _win_buffering(bool a1);
void windowDrawBorder(int win);
void windowDrawText(int win, char* str, int a3, int x, int y, int a6);
void windowDrawLine(int win, int left, int top, int right, int bottom, int color);
void windowDrawRect(int win, int left, int top, int right, int bottom, int color);
void windowFill(int win, int x, int y, int width, int height, int a6);
void windowUnhide(int win);
void windowHide(int win);
void _win_move(int win_index, int x, int y);
void windowRefresh(int win);
void windowRefreshRect(int win, const Rect* rect);
void _GNW_win_refresh(Window* window, Rect* rect, unsigned char* a3);
void windowRefreshAll(Rect* rect);
void _win_clip(Window* window, RectListNode** rect, unsigned char* a3);
void _win_drag(int win);
void _win_get_mouse_buf(unsigned char* a1);
void _refresh_all(Rect* rect, unsigned char* a2);
Window* windowGetWindow(int win);
unsigned char* windowGetBuffer(int win);
int windowGetAtPoint(int x, int y);
int windowGetWidth(int win);
int windowGetHeight(int win);
int windowGetRect(int win, Rect* rect);
int _win_check_all_buttons();
Button* buttonGetButton(int btn, Window** out_win);
int _GNW_check_menu_bars(int a1);
void _win_text(int win, char** fileNameList, int fileNameListLength, int maxWidth, int x, int y, int flags);
void programWindowSetTitle(const char* title);
int paletteOpenFileImpl(const char* path, int flags);
int paletteReadFileImpl(int fd, void* buf, size_t count);
int paletteCloseFileImpl(int fd);
bool showMesageBox(const char* str);
int buttonCreate(int win, int x, int y, int width, int height, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, unsigned char* up, unsigned char* dn, unsigned char* hover, int flags);
int _win_register_button_disable(int btn, unsigned char* up, unsigned char* down, unsigned char* hover);
int _win_register_button_image(int btn, unsigned char* up, unsigned char* down, unsigned char* hover, int a5);
int buttonSetMouseCallbacks(int btn, ButtonCallback* mouseEnterProc, ButtonCallback* mouseExitProc, ButtonCallback* mouseDownProc, ButtonCallback* mouseUpProc);
int buttonSetRightMouseCallbacks(int btn, int rightMouseDownEventCode, int rightMouseUpEventCode, ButtonCallback* rightMouseDownProc, ButtonCallback* rightMouseUpProc);
int buttonSetCallbacks(int btn, ButtonCallback* onPressed, ButtonCallback* onUnpressed);
int buttonSetMask(int btn, unsigned char* mask);
Button* buttonCreateInternal(int win, int x, int y, int width, int height, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, int flags, unsigned char* up, unsigned char* dn, unsigned char* hover);
bool _win_button_down(int btn);
int _GNW_check_buttons(Window* window, int* out_a2);
bool _button_under_mouse(Button* button, Rect* rect);
int buttonGetWindowId(int btn);
int _win_last_button_winID();
int buttonDestroy(int btn);
void buttonFree(Button* ptr);
int buttonEnable(int btn);
int buttonDisable(int btn);
int _win_set_button_rest_state(int btn, bool a2, int a3);
int _win_group_check_buttons(int a1, int* a2, int a3, void (*a4)(int));
int _win_group_radio_buttons(int a1, int* a2);
int _button_check_group(Button* button);
void _button_draw(Button* button, Window* window, unsigned char* data, int a4, Rect* a5, int a6);
void _GNW_button_refresh(Window* window, Rect* rect);
int _win_button_press_and_release(int btn);

#endif /* WINDOW_MANAGER_H */
