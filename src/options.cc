#include "options.h"

#include "art.h"
#include "color.h"
#include "cycle.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "graph_lib.h"
#include "input.h"
#include "kb.h"
#include "loadsave.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "preferences.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

#define OPTIONS_WINDOW_BUTTONS_COUNT (10)

typedef enum PauseWindowFrm {
    PAUSE_WINDOW_FRM_BACKGROUND,
    PAUSE_WINDOW_FRM_DONE_BOX,
    PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP,
    PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN,
    PAUSE_WINDOW_FRM_COUNT,
} PauseWindowFrm;

typedef enum OptionsWindowFrm {
    OPTIONS_WINDOW_FRM_BACKGROUND,
    OPTIONS_WINDOW_FRM_BUTTON_ON,
    OPTIONS_WINDOW_FRM_BUTTON_OFF,
    OPTIONS_WINDOW_FRM_COUNT,
} OptionsWindowFrm;

static int optionsWindowInit();
static int optionsWindowFree();
static void _ShadeScreen(bool a1);

// 0x48FC0C
static const int gPauseWindowFrmIds[PAUSE_WINDOW_FRM_COUNT] = {
    208, // charwin.frm - character editor
    209, // donebox.frm - character editor
    8, // lilredup.frm - little red button up
    9, // lilreddn.frm - little red button down
};

// 0x5197C0
static const int gOptionsWindowFrmIds[OPTIONS_WINDOW_FRM_COUNT] = {
    220, // opbase.frm - character editor
    222, // opbtnon.frm - character editor
    221, // opbtnoff.frm - character editor
};

// 0x6637E8
static MessageList gPreferencesMessageList;

// 0x663840
static MessageListItem gPreferencesMessageListItem;

// 0x663878
static unsigned char* _opbtns[OPTIONS_WINDOW_BUTTONS_COUNT];

// 0x6638FC
static bool gOptionsWindowGameMouseObjectsWasVisible;

// 0x663900
static int gOptionsWindow;

// 0x663908
static unsigned char* gOptionsWindowBuffer;

// 0x66398C
static int gOptionsWindowOldFont;

// 0x663994
static bool gOptionsWindowIsoWasEnabled;

static FrmImage _optionsFrmImages[OPTIONS_WINDOW_FRM_COUNT];

// 0x48FC50
int showOptions()
{
    ScopedGameMode gm(GameMode::kOptions);

    if (optionsWindowInit() == -1) {
        debugPrint("\nOPTION MENU: Error loading option dialog data!\n");
        return -1;
    }

    touch_set_touchscreen_mode(true);

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        if (keyCode == KEY_ESCAPE || keyCode == 504 || _game_user_wants_to_quit != 0) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_RETURN:
            case KEY_UPPERCASE_O:
            case KEY_LOWERCASE_O:
            case KEY_UPPERCASE_D:
            case KEY_LOWERCASE_D:
                soundPlayFile("ib1p1xx1");
                rc = 0;
                break;
            case KEY_UPPERCASE_S:
            case KEY_LOWERCASE_S:
            case 500:
                if (lsgSaveGame(LOAD_SAVE_MODE_NORMAL) == 1) {
                    rc = 1;
                }
                break;
            case KEY_UPPERCASE_L:
            case KEY_LOWERCASE_L:
            case 501:
                if (lsgLoadGame(LOAD_SAVE_MODE_NORMAL) == 1) {
                    rc = 1;
                }
                break;
            case KEY_UPPERCASE_P:
            case KEY_LOWERCASE_P:
                soundPlayFile("ib1p1xx1");
                // FALLTHROUGH
            case 502:
                // PREFERENCES
                doPreferences(false);
                break;
            case KEY_PLUS:
            case KEY_EQUAL:
                brightnessIncrease();
                break;
            case KEY_UNDERSCORE:
            case KEY_MINUS:
                brightnessDecrease();
                break;
            case KEY_F12:
                takeScreenshot();
                break;
            case KEY_UPPERCASE_E:
            case KEY_LOWERCASE_E:
            case KEY_CTRL_Q:
            case KEY_CTRL_X:
            case KEY_F10:
            case 503:
                showQuitConfirmationDialog();
                break;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    optionsWindowFree();

    return rc;
}

// 0x48FE14
static int optionsWindowInit()
{
    gOptionsWindowOldFont = fontGetCurrent();

    if (!messageListInit(&gPreferencesMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "options.msg");
    if (!messageListLoad(&gPreferencesMessageList, path)) {
        return -1;
    }

    for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gOptionsWindowFrmIds[index], 0, 0, 0);
        if (!_optionsFrmImages[index].lock(fid)) {
            while (--index >= 0) {
                _optionsFrmImages[index].unlock();
            }

            messageListFree(&gPreferencesMessageList);

            return -1;
        }
    }

    int cycle = 0;
    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
        _opbtns[index] = (unsigned char*)internal_malloc(_optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight() + 1024);
        if (_opbtns[index] == nullptr) {
            while (--index >= 0) {
                internal_free(_opbtns[index]);
            }

            for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
                _optionsFrmImages[index].unlock();
            }

            messageListFree(&gPreferencesMessageList);

            return -1;
        }

        cycle = cycle ^ 1;

        memcpy(_opbtns[index], _optionsFrmImages[cycle + 1].getData(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight());
    }

    int optionsWindowX = (screenGetWidth() - _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getWidth()) / 2;
    int optionsWindowY = (screenGetHeight() - _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getHeight()) / 2 - 60;
    gOptionsWindow = windowCreate(optionsWindowX,
        optionsWindowY,
        _optionsFrmImages[0].getWidth(),
        _optionsFrmImages[0].getHeight(),
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);

    if (gOptionsWindow == -1) {
        for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
            internal_free(_opbtns[index]);
        }

        for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
            _optionsFrmImages[index].unlock();
        }

        messageListFree(&gPreferencesMessageList);

        return -1;
    }

    gOptionsWindowIsoWasEnabled = isoDisable();

    gOptionsWindowGameMouseObjectsWasVisible = gameMouseObjectsIsVisible();
    if (gOptionsWindowGameMouseObjectsWasVisible) {
        gameMouseObjectsHide();
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    gOptionsWindowBuffer = windowGetBuffer(gOptionsWindow);
    memcpy(gOptionsWindowBuffer, _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getData(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getWidth() * _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getHeight());

    fontSetCurrent(103);

    int textY = (_optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight() - fontGetLineHeight()) / 2 + 1;
    int buttonY = 17;

    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index += 2) {
        char text[128];

        const char* msg = getmsg(&gPreferencesMessageList, &gPreferencesMessageListItem, index / 2);
        strcpy(text, msg);

        int textX = (_optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() - fontGetStringWidth(text)) / 2;
        if (textX < 0) {
            textX = 0;
        }

        fontDrawText(_opbtns[index] + _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * textY + textX, text, _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), _colorTable[18979]);
        fontDrawText(_opbtns[index + 1] + _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * textY + textX, text, _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), _colorTable[14723]);

        int btn = buttonCreate(gOptionsWindow,
            13,
            buttonY,
            _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(),
            _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight(),
            -1,
            -1,
            -1,
            index / 2 + 500,
            _opbtns[index],
            _opbtns[index + 1],
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_lrg_butt_press, _gsound_lrg_butt_release);
        }

        buttonY += _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight() + 3;
    }

    fontSetCurrent(101);

    windowRefresh(gOptionsWindow);

    return 0;
}

// 0x490244
static int optionsWindowFree()
{
    windowDestroy(gOptionsWindow);
    fontSetCurrent(gOptionsWindowOldFont);
    messageListFree(&gPreferencesMessageList);

    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
        internal_free(_opbtns[index]);
    }

    for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
        _optionsFrmImages[index].unlock();
    }

    if (gOptionsWindowGameMouseObjectsWasVisible) {
        gameMouseObjectsShow();
    }

    if (gOptionsWindowIsoWasEnabled) {
        isoEnable();
    }

    touch_set_touchscreen_mode(false);

    return 0;
}

// 0x4902B0
int showPause(bool a1)
{
    bool gameMouseWasVisible;
    if (!a1) {
        gOptionsWindowIsoWasEnabled = isoDisable();
        colorCycleDisable();

        gameMouseWasVisible = gameMouseObjectsIsVisible();
        if (gameMouseWasVisible) {
            gameMouseObjectsHide();
        }
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    _ShadeScreen(a1);

    FrmImage frmImages[PAUSE_WINDOW_FRM_COUNT];
    for (int index = 0; index < PAUSE_WINDOW_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gPauseWindowFrmIds[index], 0, 0, 0);
        if (!frmImages[index].lock(fid)) {
            debugPrint("\n** Error loading pause window graphics! **\n");
            return -1;
        }
    }

    if (!messageListInit(&gPreferencesMessageList)) {
        // FIXME: Leaking graphics.
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "options.msg");
    if (!messageListLoad(&gPreferencesMessageList, path)) {
        // FIXME: Leaking graphics.
        return -1;
    }

    int pauseWindowX = (screenGetWidth() - frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth()) / 2;
    int pauseWindowY = (screenGetHeight() - frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getHeight()) / 2;

    if (a1) {
        pauseWindowX -= 65;
        pauseWindowY -= 24;
    } else {
        pauseWindowY -= 54;
    }

    int window = windowCreate(pauseWindowX,
        pauseWindowY,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getHeight(),
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (window == -1) {
        messageListFree(&gPreferencesMessageList);

        debugPrint("\n** Error opening pause window! **\n");
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(window);
    memcpy(windowBuffer,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getData(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getHeight());

    blitBufferToBufferTrans(frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getData(),
        frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getHeight(),
        frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getWidth(),
        windowBuffer + frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * 42 + 13,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth());

    gOptionsWindowOldFont = fontGetCurrent();
    fontSetCurrent(103);

    char* messageItemText;

    messageItemText = getmsg(&gPreferencesMessageList, &gPreferencesMessageListItem, 300);
    fontDrawText(windowBuffer + frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * 45 + 52,
        messageItemText,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        _colorTable[18979]);

    fontSetCurrent(104);

    messageItemText = getmsg(&gPreferencesMessageList, &gPreferencesMessageListItem, 301);
    strcpy(path, messageItemText);

    int length = fontGetStringWidth(path);
    fontDrawText(windowBuffer + frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * 10 + 2 + (frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() - length) / 2,
        path,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        _colorTable[18979]);

    int doneBtn = buttonCreate(window,
        26,
        46,
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].getHeight(),
        -1,
        -1,
        -1,
        504,
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].getData(),
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(window);

    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        switch (keyCode) {
        case KEY_PLUS:
        case KEY_EQUAL:
            brightnessIncrease();
            break;
        case KEY_MINUS:
        case KEY_UNDERSCORE:
            brightnessDecrease();
            break;
        default:
            if (keyCode != -1 && keyCode != -2) {
                done = true;
            }

            if (_game_user_wants_to_quit != 0) {
                done = true;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (!a1) {
        tileWindowRefresh();
    }

    windowDestroy(window);
    messageListFree(&gPreferencesMessageList);

    if (!a1) {
        if (gameMouseWasVisible) {
            gameMouseObjectsShow();
        }

        if (gOptionsWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();

        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    }

    fontSetCurrent(gOptionsWindowOldFont);

    return 0;
}

// 0x490748
static void _ShadeScreen(bool a1)
{
    if (a1) {
        mouseHideCursor();
    } else {
        mouseHideCursor();
        tileWindowRefresh();

        int windowWidth = windowGetWidth(gIsoWindow);
        int windowHeight = windowGetHeight(gIsoWindow);
        unsigned char* windowBuffer = windowGetBuffer(gIsoWindow);
        grayscalePaletteApply(windowBuffer, windowWidth, windowHeight, windowWidth);

        windowRefresh(gIsoWindow);
    }

    mouseShowCursor();
}

// init_options_menu
// 0x4928B8
int _init_options_menu()
{
    preferencesInit();

    grayscalePaletteUpdate(0, 255);

    return 0;
}

} // namespace fallout
