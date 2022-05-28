#include "main.h"

#include "autorun.h"
#include "character_selector.h"
#include "color.h"
#include "core.h"
#include "credits.h"
#include "cycle.h"
#include "db.h"
#include "debug.h"
#include "draw.h"
#include "endgame.h"
#include "game.h"
#include "game_config.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "loadsave.h"
#include "map.h"
#include "object.h"
#include "options.h"
#include "palette.h"
#include "platform_compat.h"
#include "random.h"
#include "scripts.h"
#include "sfall_config.h"
#include "selfrun.h"
#include "text_font.h"
#include "version.h"
#include "window_manager.h"
#include "word_wrap.h"
#include "world_map.h"

#define MAIN_MENU_WINDOW_WIDTH 640
#define MAIN_MENU_WINDOW_HEIGHT 480

#define DEATH_WINDOW_WIDTH 640
#define DEATH_WINDOW_HEIGHT 480

// 0x5194C8
char _mainMap[] = "artemple.map";

// 0x5194D8
int _main_game_paused = 0;

// 0x5194DC
char** _main_selfrun_list = NULL;

// 0x5194E0
int _main_selfrun_count = 0;

// 0x5194E4
int _main_selfrun_index = 0;

// 0x5194E8
bool _main_show_death_scene = false;

// 0x5194F0
int gMainMenuWindow = -1;

// 0x5194F4
unsigned char* gMainMenuWindowBuffer = NULL;

// 0x5194F8
unsigned char* gMainMenuBackgroundFrmData = NULL;

// 0x5194FC
unsigned char* gMainMenuButtonUpFrmData = NULL;

// 0x519500
unsigned char* gMainMenuButtonDownFrmData = NULL;

// 0x519504
bool _in_main_menu = false;

// 0x519508
bool gMainMenuWindowInitialized = false;

// 0x51950C
unsigned int gMainMenuScreensaverDelay = 120000;

// 0x519510
const int gMainMenuButtonKeyBindings[MAIN_MENU_BUTTON_COUNT] = {
    KEY_LOWERCASE_I, // intro
    KEY_LOWERCASE_N, // new game
    KEY_LOWERCASE_L, // load game
    KEY_LOWERCASE_O, // options
    KEY_LOWERCASE_C, // credits
    KEY_LOWERCASE_E, // exit
};

// 0x519528
const int _return_values[MAIN_MENU_BUTTON_COUNT] = {
    MAIN_MENU_INTRO,
    MAIN_MENU_NEW_GAME,
    MAIN_MENU_LOAD_GAME,
    MAIN_MENU_OPTIONS,
    MAIN_MENU_CREDITS,
    MAIN_MENU_EXIT,
};

// 0x614838
bool _main_death_voiceover_done;

// 0x614840
int gMainMenuButtons[MAIN_MENU_BUTTON_COUNT];

// 0x614858
bool gMainMenuWindowHidden;

// 0x61485C
CacheEntry* gMainMenuButtonUpFrmHandle;

// 0x614860
CacheEntry* gMainMenuButtonDownFrmHandle;

// 0x614864
CacheEntry* gMainMenuBackgroundFrmHandle;

// 0x48099C
int falloutMain(int argc, char** argv)
{
    if (!autorunMutexCreate()) {
        return 1;
    }

    if (!falloutInit(argc, argv)) {
        return 1;
    }

    gameMoviePlay(MOVIE_IPLOGO, GAME_MOVIE_FADE_IN);
    gameMoviePlay(MOVIE_INTRO, 0);
    gameMoviePlay(MOVIE_CREDITS, 0);

    FpsLimiter fpsLimiter;

    if (mainMenuWindowInit() == 0) {
        bool done = false;
        while (!done) {
            keyboardReset();
            _gsound_background_play_level_music("07desert", 11);
            mainMenuWindowUnhide(1);

            mouseShowCursor();
            int mainMenuRc = mainMenuWindowHandleEvents(fpsLimiter);
            mouseHideCursor();

            switch (mainMenuRc) {
            case MAIN_MENU_INTRO:
                mainMenuWindowHide(true);
                gameMoviePlay(MOVIE_INTRO, GAME_MOVIE_PAUSE_MUSIC);
                gameMoviePlay(MOVIE_CREDITS, 0);
                break;
            case MAIN_MENU_NEW_GAME:
                mainMenuWindowHide(true);
                mainMenuWindowFree();
                if (characterSelectorOpen() == 2) {
                    gameMoviePlay(MOVIE_ELDER, GAME_MOVIE_STOP_MUSIC);
                    randomSeedPrerandom(-1);

                    // SFALL: Override starting map.                    
                    char* mapName = NULL;
                    if (configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_STARTING_MAP_KEY, &mapName)) {
                        if (*mapName == '\0') {
                            mapName = NULL;
                        }
                    }

                    char* mapNameCopy = strdup(mapName != NULL ? mapName : _mainMap);
                    _main_load_new(mapNameCopy);
                    free(mapNameCopy);

                    mainLoop(fpsLimiter);
                    paletteFadeTo(gPaletteWhite);
                    objectHide(gDude, NULL);
                    _map_exit();
                    gameReset();
                    if (_main_show_death_scene != 0) {
                        showDeath();
                        _main_show_death_scene = 0;
                    }
                }

                mainMenuWindowInit();

                break;
            case MAIN_MENU_LOAD_GAME:
                if (1) {
                    int win = windowCreate(0, 0, screenGetWidth(), screenGetHeight(), _colorTable[0], WINDOW_FLAG_0x10 | WINDOW_FLAG_0x04);
                    mainMenuWindowHide(true);
                    mainMenuWindowFree();
                    _game_user_wants_to_quit = 0;
                    gDude->flags &= ~OBJECT_FLAG_0x08;
                    _main_show_death_scene = 0;
                    objectShow(gDude, NULL);
                    mouseHideCursor();
                    _map_init();
                    gameMouseSetCursor(MOUSE_CURSOR_NONE);
                    mouseShowCursor();
                    colorPaletteLoad("color.pal");
                    paletteFadeTo(_cmap);
                    int loadGameRc = lsgLoadGame(LOAD_SAVE_MODE_FROM_MAIN_MENU);
                    if (loadGameRc == -1) {
                        debugPrint("\n ** Error running LoadGame()! **\n");
                    } else if (loadGameRc != 0) {
                        windowDestroy(win);
                        win = -1;
                        mainLoop(fpsLimiter);
                    }
                    paletteFadeTo(gPaletteWhite);
                    if (win != -1) {
                        windowDestroy(win);
                    }
                    objectHide(gDude, NULL);
                    _map_exit();
                    gameReset();
                    if (_main_show_death_scene != 0) {
                        showDeath();
                        _main_show_death_scene = 0;
                    }
                    mainMenuWindowInit();
                }
                break;
            case MAIN_MENU_TIMEOUT:
                debugPrint("Main menu timed-out\n");
                // FALLTHROUGH
            case MAIN_MENU_3:
                // _main_selfrun_play();
                break;
            case MAIN_MENU_OPTIONS:
                mainMenuWindowHide(false);
                mouseShowCursor();
                showOptionsWithInitialKeyCode(112);
                gameMouseSetCursor(MOUSE_CURSOR_ARROW);
                mouseShowCursor();
                mainMenuWindowUnhide(0);
                break;
            case MAIN_MENU_CREDITS:
                mainMenuWindowHide(true);
                creditsOpen("credits.txt", -1, false);
                break;
            case MAIN_MENU_QUOTES:
                // NOTE: There is a strange cmp at 0x480C50. Both operands are
                // zero, set before the loop and do not modify afterwards. For
                // clarity this condition is omitted.
                mainMenuWindowHide(true);
                creditsOpen("quotes.txt", -1, true);
                break;
            case MAIN_MENU_EXIT:
            case -1:
                done = true;
                mainMenuWindowHide(true);
                mainMenuWindowFree();
                backgroundSoundDelete();
                break;
            case MAIN_MENU_SELFRUN:
                // _main_selfrun_record();
                break;
            }
        }
    }

    backgroundSoundDelete();
    _main_selfrun_exit();
    gameExit();

    autorunMutexClose();

    return 0;
}

// 0x480CC0
bool falloutInit(int argc, char** argv)
{
    if (gameInitWithOptions("FALLOUT II", false, 0, 0, argc, argv) == -1) {
        return false;
    }

    if (_main_selfrun_list != NULL) {
        _main_selfrun_exit();
    }

    if (_selfrun_get_list(&_main_selfrun_list, &_main_selfrun_count) == 0) {
        _main_selfrun_index = 0;
    }

    return true;
}

// 0x480D4C
int _main_load_new(char* mapFileName)
{
    _game_user_wants_to_quit = 0;
    _main_show_death_scene = 0;
    gDude->flags &= ~OBJECT_FLAG_0x08;
    objectShow(gDude, NULL);
    mouseHideCursor();

    int win = windowCreate(0, 0, screenGetWidth(), screenGetHeight(), _colorTable[0], WINDOW_FLAG_0x10 | WINDOW_FLAG_0x04);
    windowRefresh(win);

    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);
    _map_init();
    gameMouseSetCursor(MOUSE_CURSOR_NONE);
    mouseShowCursor();
    mapLoadByName(mapFileName);
    worldmapStartMapMusic();
    paletteFadeTo(gPaletteWhite);
    windowDestroy(win);
    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);
    return 0;
}

// 0x480E48
void mainLoop(FpsLimiter& fpsLimiter)
{
    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        mouseShowCursor();
    }

    _main_game_paused = 0;

    scriptsEnable();

    while (_game_user_wants_to_quit == 0) {
        fpsLimiter.mark();

        int keyCode = _get_input();
        gameHandleKey(keyCode, false);

        scriptsHandleRequests();

        mapHandleTransition();

        if (_main_game_paused != 0) {
            _main_game_paused = 0;
        }

        if ((gDude->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
            endgameSetupDeathEnding(ENDGAME_DEATH_ENDING_REASON_DEATH);
            _main_show_death_scene = 1;
            _game_user_wants_to_quit = 2;
        }

        fpsLimiter.throttle();
    }

    scriptsDisable();

    if (cursorWasHidden) {
        mouseHideCursor();
    }
}

// 0x480F38
void _main_selfrun_exit()
{
    if (_main_selfrun_list != NULL) {
        _selfrun_free_list(&_main_selfrun_list);
    }

    _main_selfrun_count = 0;
    _main_selfrun_index = 0;
    _main_selfrun_list = NULL;
}

// 0x48118C
void showDeath()
{
    artCacheFlush();
    colorCycleDisable();
    gameMouseSetCursor(MOUSE_CURSOR_NONE);

    bool oldCursorIsHidden = cursorIsHidden();
    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    int deathWindowX = (screenGetWidth() - DEATH_WINDOW_WIDTH) / 2;
    int deathWindowY = (screenGetHeight() - DEATH_WINDOW_HEIGHT) / 2;
    int win = windowCreate(deathWindowX,
        deathWindowY,
        DEATH_WINDOW_WIDTH,
        DEATH_WINDOW_HEIGHT,
        0,
        WINDOW_FLAG_0x04);
    if (win != -1) {
        do {
            unsigned char* windowBuffer = windowGetBuffer(win);
            if (windowBuffer == NULL) {
                break;
            }

            // DEATH.FRM
            CacheEntry* backgroundHandle;
            int fid = buildFid(6, 309, 0, 0, 0);
            unsigned char* background = artLockFrameData(fid, 0, 0, &backgroundHandle);
            if (background == NULL) {
                break;
            }

            while (mouseGetEvent() != 0) {
                _get_input();
            }

            keyboardReset();
            inputEventQueueReset();

            blitBufferToBuffer(background, 640, 480, 640, windowBuffer, 640);
            artUnlock(backgroundHandle);

            const char* deathFileName = endgameDeathEndingGetFileName();

            int subtitles = 0;
            configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_SUBTITLES_KEY, &subtitles);
            if (subtitles != 0) {
                char text[512];
                if (_mainDeathGrabTextFile(deathFileName, text) == 0) {
                    debugPrint("\n((ShowDeath)): %s\n", text);

                    short beginnings[WORD_WRAP_MAX_COUNT];
                    short count;
                    if (_mainDeathWordWrap(text, 560, beginnings, &count) == 0) {
                        unsigned char* p = windowBuffer + 640 * (480 - fontGetLineHeight() * count - 8);
                        bufferFill(p - 602, 564, fontGetLineHeight() * count + 2, 640, 0);
                        p += 40;
                        for (int index = 0; index < count; index++) {
                            fontDrawText(p, text + beginnings[index], 560, 640, _colorTable[32767]);
                            p += 640 * fontGetLineHeight();
                        }
                    }
                }
            }

            windowRefresh(win);

            colorPaletteLoad("art\\intrface\\death.pal");
            paletteFadeTo(_cmap);

            _main_death_voiceover_done = false;
            speechSetEndCallback(_main_death_voiceover_callback);

            unsigned int delay;
            if (speechLoad(deathFileName, 10, 14, 15) == -1) {
                delay = 3000;
            } else {
                delay = UINT_MAX;
            }

            _gsound_speech_play_preloaded();

            // SFALL: Fix the playback of the speech sound file for the death
            // screen.
            coreDelay(100);

            unsigned int time = _get_time();
            int keyCode;
            do {
                keyCode = _get_input();
            } while (keyCode == -1 && !_main_death_voiceover_done && getTicksSince(time) < delay);

            speechSetEndCallback(NULL);

            speechDelete();

            while (mouseGetEvent() != 0) {
                _get_input();
            }

            if (keyCode == -1) {
                coreDelayProcessingEvents(500);
            }

            paletteFadeTo(gPaletteBlack);
            colorPaletteLoad("color.pal");
        } while (0);
        windowDestroy(win);
    }

    if (oldCursorIsHidden) {
        mouseHideCursor();
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    colorCycleEnable();
}

// 0x4814A8
void _main_death_voiceover_callback()
{
    _main_death_voiceover_done = true;
}

// Read endgame subtitle.
//
// 0x4814B4
int _mainDeathGrabTextFile(const char* fileName, char* dest)
{
    const char* p = strrchr(fileName, '\\');
    if (p == NULL) {
        return -1;
    }

    char* language = NULL;
    if (!configGetString(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_KEY, &language)) {
        debugPrint("MAIN: Error grabing language for ending. Defaulting to english.\n");
        language = _aEnglish_2;
    }

    char path[COMPAT_MAX_PATH];
    sprintf(path, "text\\%s\\cuts\\%s%s", language, p + 1, ".TXT");

    File* stream = fileOpen(path, "rt");
    if (stream == NULL) {
        return -1;
    }

    while (true) {
        int c = fileReadChar(stream);
        if (c == -1) {
            break;
        }

        if (c == '\n') {
            c = ' ';
        }

        *dest++ = (c & 0xFF);
    }

    fileClose(stream);

    *dest = '\0';

    return 0;
}

// 0x481598
int _mainDeathWordWrap(char* text, int width, short* beginnings, short* count)
{
    // TODO: Probably wrong.
    while (true) {
        char* p = text;
        while (*p != ':') {
            if (*p != '\0') {
                p++;
                if (*p == ':') {
                    break;
                }
                if (*p != '\0') {
                    continue;
                }
            }
            p = NULL;
            break;
        }

        if (p == NULL) {
            break;
        }

        if (p - 1 < text) {
            break;
        }
        p[0] = ' ';
        p[-1] = ' ';
    }

    if (wordWrap(text, width, beginnings, count) == -1) {
        return -1;
    }

    // TODO: Probably wrong.
    *count -= 1;

    for (int index = 1; index < *count; index++) {
        char* p = text + beginnings[index];
        while (p >= text && *p != ' ') {
            p--;
            beginnings[index]--;
        }

        if (p != NULL) {
            *p = '\0';
            beginnings[index]++;
        }
    }

    return 0;
}

// 0x481650
int mainMenuWindowInit()
{
    int fid;
    MessageListItem msg;
    int len;

    if (gMainMenuWindowInitialized) {
        return 0;
    }

    colorPaletteLoad("color.pal");

    int mainMenuWindowX = (screenGetWidth() - MAIN_MENU_WINDOW_WIDTH) / 2;
    int mainMenuWindowY = (screenGetHeight() - MAIN_MENU_WINDOW_HEIGHT) / 2;
    gMainMenuWindow = windowCreate(mainMenuWindowX,
        mainMenuWindowY,
        MAIN_MENU_WINDOW_WIDTH,
        MAIN_MENU_WINDOW_HEIGHT,
        0,
        WINDOW_HIDDEN | WINDOW_FLAG_0x04);
    if (gMainMenuWindow == -1) {
        mainMenuWindowFree();
        return -1;
    }

    gMainMenuWindowBuffer = windowGetBuffer(gMainMenuWindow);

    // mainmenu.frm
    int backgroundFid = buildFid(6, 140, 0, 0, 0);
    gMainMenuBackgroundFrmData = artLockFrameData(backgroundFid, 0, 0, &gMainMenuBackgroundFrmHandle);
    if (gMainMenuBackgroundFrmData == NULL) {
        mainMenuWindowFree();
        return -1;
    }

    blitBufferToBuffer(gMainMenuBackgroundFrmData, 640, 480, 640, gMainMenuWindowBuffer, 640);
    artUnlock(gMainMenuBackgroundFrmHandle);

    int oldFont = fontGetCurrent();
    fontSetCurrent(100);

    // Copyright.
    msg.num = 20;
    if (messageListGetItem(&gMiscMessageList, &msg)) {
        windowDrawText(gMainMenuWindow, msg.text, 0, 15, 460, _colorTable[21091] | 0x6000000);
    }

    // Version.
    char version[VERSION_MAX];
    versionGetVersion(version);
    len = fontGetStringWidth(version);
    windowDrawText(gMainMenuWindow, version, 0, 615 - len, 460, _colorTable[21091] | 0x6000000);

    // menuup.frm
    fid = buildFid(6, 299, 0, 0, 0);
    gMainMenuButtonUpFrmData = artLockFrameData(fid, 0, 0, &gMainMenuButtonUpFrmHandle);
    if (gMainMenuButtonUpFrmData == NULL) {
        mainMenuWindowFree();
        return -1;
    }

    // menudown.frm
    fid = buildFid(6, 300, 0, 0, 0);
    gMainMenuButtonDownFrmData = artLockFrameData(fid, 0, 0, &gMainMenuButtonDownFrmHandle);
    if (gMainMenuButtonDownFrmData == NULL) {
        mainMenuWindowFree();
        return -1;
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = -1;
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = buttonCreate(gMainMenuWindow, 30, 19 + index * 42 - index, 26, 26, -1, -1, 1111, gMainMenuButtonKeyBindings[index], gMainMenuButtonUpFrmData, gMainMenuButtonDownFrmData, 0, 32);
        if (gMainMenuButtons[index] == -1) {
            mainMenuWindowFree();
            return -1;
        }

        buttonSetMask(gMainMenuButtons[index], gMainMenuButtonUpFrmData);
    }

    fontSetCurrent(104);

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        msg.num = 9 + index;
        if (messageListGetItem(&gMiscMessageList, &msg)) {
            len = fontGetStringWidth(msg.text);
            fontDrawText(gMainMenuWindowBuffer + 640 * (42 * index - index + 20) + 126 - (len / 2), msg.text, 640 - (126 - (len / 2)) - 1, 640, _colorTable[21091]);
        }
    }

    fontSetCurrent(oldFont);

    gMainMenuWindowInitialized = true;
    gMainMenuWindowHidden = true;

    return 0;
}

// 0x481968
void mainMenuWindowFree()
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        // FIXME: Why it tries to free only invalid buttons?
        if (gMainMenuButtons[index] == -1) {
            buttonDestroy(gMainMenuButtons[index]);
        }
    }

    if (gMainMenuButtonDownFrmData) {
        artUnlock(gMainMenuButtonDownFrmHandle);
        gMainMenuButtonDownFrmHandle = NULL;
        gMainMenuButtonDownFrmData = NULL;
    }

    if (gMainMenuButtonUpFrmData) {
        artUnlock(gMainMenuButtonUpFrmHandle);
        gMainMenuButtonUpFrmHandle = NULL;
        gMainMenuButtonUpFrmData = NULL;
    }

    if (gMainMenuWindow != -1) {
        windowDestroy(gMainMenuWindow);
    }

    gMainMenuWindowInitialized = false;
}

// 0x481A00
void mainMenuWindowHide(bool animate)
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    if (gMainMenuWindowHidden) {
        return;
    }

    soundContinueAll();

    if (animate) {
        paletteFadeTo(gPaletteBlack);
        soundContinueAll();
    }

    windowHide(gMainMenuWindow);

    gMainMenuWindowHidden = true;
}

// 0x481A48
void mainMenuWindowUnhide(bool animate)
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    if (!gMainMenuWindowHidden) {
        return;
    }

    windowUnhide(gMainMenuWindow);

    if (animate) {
        colorPaletteLoad("color.pal");
        paletteFadeTo(_cmap);
    }

    gMainMenuWindowHidden = false;
}

// 0x481AA8
int _main_menu_is_enabled()
{
    return 1;
}

// 0x481AEC
int mainMenuWindowHandleEvents(FpsLimiter& fpsLimiter)
{
    _in_main_menu = true;

    bool oldCursorIsHidden = cursorIsHidden();
    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    unsigned int tick = _get_time();

    int rc = -1;
    while (rc == -1) {
        fpsLimiter.mark();

        int keyCode = _get_input();

        for (int buttonIndex = 0; buttonIndex < MAIN_MENU_BUTTON_COUNT; buttonIndex++) {
            if (keyCode == gMainMenuButtonKeyBindings[buttonIndex] || keyCode == toupper(gMainMenuButtonKeyBindings[buttonIndex])) {
                soundPlayFile("nmselec1");

                rc = _return_values[buttonIndex];

                if (buttonIndex == MAIN_MENU_BUTTON_CREDITS && (gPressedPhysicalKeys[SDL_SCANCODE_RSHIFT] != KEY_STATE_UP || gPressedPhysicalKeys[SDL_SCANCODE_LSHIFT] != KEY_STATE_UP)) {
                    rc = MAIN_MENU_QUOTES;
                }

                break;
            }
        }

        if (rc == -1) {
            if (keyCode == KEY_CTRL_R) {
                rc = MAIN_MENU_SELFRUN;
                continue;
            } else if (keyCode == KEY_PLUS || keyCode == KEY_EQUAL) {
                brightnessIncrease();
            } else if (keyCode == KEY_MINUS || keyCode == KEY_UNDERSCORE) {
                brightnessDecrease();
            } else if (keyCode == KEY_UPPERCASE_D || keyCode == KEY_LOWERCASE_D) {
                rc = MAIN_MENU_3;
                continue;
            } else if (keyCode == 1111) {
                if (!(mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT)) {
                    soundPlayFile("nmselec0");
                }
                continue;
            }
        }

        if (keyCode == KEY_ESCAPE || _game_user_wants_to_quit == 3) {
            rc = MAIN_MENU_EXIT;
            soundPlayFile("nmselec1");
            break;
        } else if (_game_user_wants_to_quit == 2) {
            _game_user_wants_to_quit = 0;
        } else {
            if (getTicksSince(tick) >= gMainMenuScreensaverDelay) {
                rc = MAIN_MENU_TIMEOUT;
            }
        }

        fpsLimiter.throttle();
    }

    if (oldCursorIsHidden) {
        mouseHideCursor();
    }

    _in_main_menu = false;

    return rc;
}
