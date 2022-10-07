#include "main.h"

#include <ctype.h>
#include <limits.h>
#include <string.h>

#include "art.h"
#include "autorun.h"
#include "character_selector.h"
#include "color.h"
#include "credits.h"
#include "cycle.h"
#include "db.h"
#include "debug.h"
#include "draw.h"
#include "endgame.h"
#include "game.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "loadsave.h"
#include "map.h"
#include "mouse.h"
#include "object.h"
#include "options.h"
#include "palette.h"
#include "platform_compat.h"
#include "proto.h"
#include "random.h"
#include "scripts.h"
#include "selfrun.h"
#include "settings.h"
#include "sfall_config.h"
#include "svga.h"
#include "text_font.h"
#include "version.h"
#include "window.h"
#include "window_manager.h"
#include "window_manager_private.h"
#include "word_wrap.h"
#include "worldmap.h"

namespace fallout {

#define MAIN_MENU_WINDOW_WIDTH 640
#define MAIN_MENU_WINDOW_HEIGHT 480

#define DEATH_WINDOW_WIDTH 640
#define DEATH_WINDOW_HEIGHT 480

typedef enum MainMenuButton {
    MAIN_MENU_BUTTON_INTRO,
    MAIN_MENU_BUTTON_NEW_GAME,
    MAIN_MENU_BUTTON_LOAD_GAME,
    MAIN_MENU_BUTTON_OPTIONS,
    MAIN_MENU_BUTTON_CREDITS,
    MAIN_MENU_BUTTON_EXIT,
    MAIN_MENU_BUTTON_COUNT,
} MainMenuButton;

typedef enum MainMenuOption {
    MAIN_MENU_INTRO,
    MAIN_MENU_NEW_GAME,
    MAIN_MENU_LOAD_GAME,
    MAIN_MENU_SCREENSAVER,
    MAIN_MENU_TIMEOUT,
    MAIN_MENU_CREDITS,
    MAIN_MENU_QUOTES,
    MAIN_MENU_EXIT,
    MAIN_MENU_SELFRUN,
    MAIN_MENU_OPTIONS,
} MainMenuOption;

static bool falloutInit(int argc, char** argv);
static int main_reset_system();
static void main_exit_system();
static int _main_load_new(char* fname);
static int main_loadgame_new();
static void main_unload_new();
static void mainLoop();
static void _main_selfrun_exit();
static void _main_selfrun_record();
static void _main_selfrun_play();
static void showDeath();
static void _main_death_voiceover_callback();
static int _mainDeathGrabTextFile(const char* fileName, char* dest);
static int _mainDeathWordWrap(char* text, int width, short* beginnings, short* count);
static int mainMenuWindowInit();
static void mainMenuWindowFree();
static void mainMenuWindowHide(bool animate);
static void mainMenuWindowUnhide(bool animate);
static int _main_menu_is_enabled();
static int mainMenuWindowHandleEvents();
static int main_menu_fatal_error();
static void main_menu_play_sound(const char* fileName);

// 0x5194C8
static char _mainMap[] = "artemple.map";

// 0x5194D8
static int _main_game_paused = 0;

// 0x5194DC
static char** _main_selfrun_list = NULL;

// 0x5194E0
static int _main_selfrun_count = 0;

// 0x5194E4
static int _main_selfrun_index = 0;

// 0x5194E8
static bool _main_show_death_scene = false;

// A switch to pick selfrun vs. intro video for screensaver:
// - `false` - will play next selfrun recording
// - `true` - will play intro video
//
// This value will alternate on every attempt, even if there are no selfrun
// recordings.
//
// 0x5194EC
static bool gMainMenuScreensaverCycle = false;

// 0x5194F0
static int gMainMenuWindow = -1;

// 0x5194F4
static unsigned char* gMainMenuWindowBuffer = NULL;

// 0x519504
static bool _in_main_menu = false;

// 0x519508
static bool gMainMenuWindowInitialized = false;

// 0x51950C
static unsigned int gMainMenuScreensaverDelay = 120000;

// 0x519510
static const int gMainMenuButtonKeyBindings[MAIN_MENU_BUTTON_COUNT] = {
    KEY_LOWERCASE_I, // intro
    KEY_LOWERCASE_N, // new game
    KEY_LOWERCASE_L, // load game
    KEY_LOWERCASE_O, // options
    KEY_LOWERCASE_C, // credits
    KEY_LOWERCASE_E, // exit
};

// 0x519528
static const int _return_values[MAIN_MENU_BUTTON_COUNT] = {
    MAIN_MENU_INTRO,
    MAIN_MENU_NEW_GAME,
    MAIN_MENU_LOAD_GAME,
    MAIN_MENU_OPTIONS,
    MAIN_MENU_CREDITS,
    MAIN_MENU_EXIT,
};

// 0x614838
static bool _main_death_voiceover_done;

// 0x614840
static int gMainMenuButtons[MAIN_MENU_BUTTON_COUNT];

// 0x614858
static bool gMainMenuWindowHidden;

static FrmImage _mainMenuBackgroundFrmImage;
static FrmImage _mainMenuButtonNormalFrmImage;
static FrmImage _mainMenuButtonPressedFrmImage;

// 0x48099C
int falloutMain(int argc, char** argv)
{
    if (!autorunMutexCreate()) {
        return 1;
    }

    if (!falloutInit(argc, argv)) {
        return 1;
    }

    // SFALL: Allow to skip intro movies
    int skipOpeningMovies;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_SKIP_OPENING_MOVIES_KEY, &skipOpeningMovies);
    if (skipOpeningMovies < 1) {
        gameMoviePlay(MOVIE_IPLOGO, GAME_MOVIE_FADE_IN);
        gameMoviePlay(MOVIE_INTRO, 0);
        gameMoviePlay(MOVIE_CREDITS, 0);
    }

    if (mainMenuWindowInit() == 0) {
        bool done = false;
        while (!done) {
            keyboardReset();
            _gsound_background_play_level_music("07desert", 11);
            mainMenuWindowUnhide(1);

            mouseShowCursor();
            int mainMenuRc = mainMenuWindowHandleEvents();
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

                    char* mapNameCopy = compat_strdup(mapName != NULL ? mapName : _mainMap);
                    _main_load_new(mapNameCopy);
                    free(mapNameCopy);

                    mainLoop();
                    paletteFadeTo(gPaletteWhite);

                    // NOTE: Uninline.
                    main_unload_new();

                    // NOTE: Uninline.
                    main_reset_system();

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

                    // NOTE: Uninline.
                    main_loadgame_new();

                    colorPaletteLoad("color.pal");
                    paletteFadeTo(_cmap);
                    int loadGameRc = lsgLoadGame(LOAD_SAVE_MODE_FROM_MAIN_MENU);
                    if (loadGameRc == -1) {
                        debugPrint("\n ** Error running LoadGame()! **\n");
                    } else if (loadGameRc != 0) {
                        windowDestroy(win);
                        win = -1;
                        mainLoop();
                    }
                    paletteFadeTo(gPaletteWhite);
                    if (win != -1) {
                        windowDestroy(win);
                    }

                    // NOTE: Uninline.
                    main_unload_new();

                    // NOTE: Uninline.
                    main_reset_system();

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
            case MAIN_MENU_SCREENSAVER:
                _main_selfrun_play();
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
                _main_selfrun_record();
                break;
            }
        }
    }

    // NOTE: Uninline.
    main_exit_system();

    autorunMutexClose();

    return 0;
}

// 0x480CC0
static bool falloutInit(int argc, char** argv)
{
    if (gameInitWithOptions("FALLOUT II", false, 0, 0, argc, argv) == -1) {
        return false;
    }

    if (_main_selfrun_list != NULL) {
        _main_selfrun_exit();
    }

    if (selfrunInitFileList(&_main_selfrun_list, &_main_selfrun_count) == 0) {
        _main_selfrun_index = 0;
    }

    return true;
}

// NOTE: Inlined.
//
// 0x480D0C
static int main_reset_system()
{
    gameReset();

    return 1;
}

// NOTE: Inlined.
//
// 0x480D18
static void main_exit_system()
{
    backgroundSoundDelete();

    // NOTE: Uninline.
    _main_selfrun_exit();

    gameExit();
}

// 0x480D4C
static int _main_load_new(char* mapFileName)
{
    _game_user_wants_to_quit = 0;
    _main_show_death_scene = 0;
    gDude->flags &= ~OBJECT_FLAT;
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
    wmMapMusicStart();
    paletteFadeTo(gPaletteWhite);
    windowDestroy(win);
    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);
    return 0;
}

// NOTE: Inlined.
//
// 0x480DF8
static int main_loadgame_new()
{
    _game_user_wants_to_quit = 0;
    _main_show_death_scene = 0;

    gDude->flags &= ~OBJECT_FLAT;

    objectShow(gDude, NULL);
    mouseHideCursor();

    _map_init();

    gameMouseSetCursor(MOUSE_CURSOR_NONE);
    mouseShowCursor();

    return 0;
}

// 0x480E34
static void main_unload_new()
{
    objectHide(gDude, NULL);
    _map_exit();
}

// 0x480E48
static void mainLoop()
{
    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        mouseShowCursor();
    }

    _main_game_paused = 0;

    scriptsEnable();

    while (_game_user_wants_to_quit == 0) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
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

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    scriptsDisable();

    if (cursorWasHidden) {
        mouseHideCursor();
    }
}

// 0x480F38
static void _main_selfrun_exit()
{
    if (_main_selfrun_list != NULL) {
        selfrunFreeFileList(&_main_selfrun_list);
    }

    _main_selfrun_count = 0;
    _main_selfrun_index = 0;
    _main_selfrun_list = NULL;
}

// 0x480F64
static void _main_selfrun_record()
{
    SelfrunData selfrunData;
    bool ready = false;

    char** fileList;
    int fileListLength = fileNameListInit("maps\\*.map", &fileList, 0, 0);
    if (fileListLength != 0) {
        int selectedFileIndex = _win_list_select("Select Map", fileList, fileListLength, 0, 80, 80, 0x10000 | 0x100 | 4);
        if (selectedFileIndex != -1) {
            // NOTE: It's size is likely 13 chars (on par with SelfrunData
            // fields), but due to the padding it takes 16 chars on stack.
            char recordingName[SELFRUN_RECORDING_FILE_NAME_LENGTH];
            recordingName[0] = '\0';
            if (_win_get_str(recordingName, sizeof(recordingName) - 2, "Enter name for recording (8 characters max, no extension):", 100, 100) == 0) {
                memset(&selfrunData, 0, sizeof(selfrunData));
                if (selfrunPrepareRecording(recordingName, fileList[selectedFileIndex], &selfrunData) == 0) {
                    ready = true;
                }
            }
        }
        fileNameListFree(&fileList, 0);
    }

    if (ready) {
        mainMenuWindowHide(true);
        mainMenuWindowFree();
        backgroundSoundDelete();
        randomSeedPrerandom(0xBEEFFEED);

        // NOTE: Uninline.
        main_reset_system();

        _proto_dude_init("premade\\combat.gcd");
        _main_load_new(selfrunData.mapFileName);
        selfrunRecordingLoop(&selfrunData);
        paletteFadeTo(gPaletteWhite);

        // NOTE: Uninline.
        main_unload_new();

        // NOTE: Uninline.
        main_reset_system();

        mainMenuWindowInit();

        if (_main_selfrun_list != NULL) {
            _main_selfrun_exit();
        }

        if (selfrunInitFileList(&_main_selfrun_list, &_main_selfrun_count) == 0) {
            _main_selfrun_index = 0;
        }
    }
}

// 0x48109C
static void _main_selfrun_play()
{
    if (!gMainMenuScreensaverCycle && _main_selfrun_count > 0) {
        SelfrunData selfrunData;
        if (selfrunPreparePlayback(_main_selfrun_list[_main_selfrun_index], &selfrunData) == 0) {
            mainMenuWindowHide(true);
            mainMenuWindowFree();
            backgroundSoundDelete();
            randomSeedPrerandom(0xBEEFFEED);

            // NOTE: Uninline.
            main_reset_system();

            _proto_dude_init("premade\\combat.gcd");
            _main_load_new(selfrunData.mapFileName);
            selfrunPlaybackLoop(&selfrunData);
            paletteFadeTo(gPaletteWhite);

            // NOTE: Uninline.
            main_unload_new();

            // NOTE: Uninline.
            main_reset_system();

            mainMenuWindowInit();
        }

        _main_selfrun_index++;
        if (_main_selfrun_index >= _main_selfrun_count) {
            _main_selfrun_index = 0;
        }
    } else {
        mainMenuWindowHide(true);
        gameMoviePlay(MOVIE_INTRO, GAME_MOVIE_PAUSE_MUSIC);
    }

    gMainMenuScreensaverCycle = !gMainMenuScreensaverCycle;
}

// 0x48118C
static void showDeath()
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
            FrmImage backgroundFrmImage;
            int fid = buildFid(OBJ_TYPE_INTERFACE, 309, 0, 0, 0);
            if (!backgroundFrmImage.lock(fid)) {
                break;
            }

            while (mouseGetEvent() != 0) {
                sharedFpsLimiter.mark();

                inputGetInput();

                renderPresent();
                sharedFpsLimiter.throttle();
            }

            keyboardReset();
            inputEventQueueReset();

            blitBufferToBuffer(backgroundFrmImage.getData(), 640, 480, 640, windowBuffer, 640);
            backgroundFrmImage.unlock();

            const char* deathFileName = endgameDeathEndingGetFileName();

            if (settings.preferences.subtitles) {
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
            inputBlockForTocks(100);

            unsigned int time = getTicks();
            int keyCode;
            do {
                sharedFpsLimiter.mark();

                keyCode = inputGetInput();

                renderPresent();
                sharedFpsLimiter.throttle();
            } while (keyCode == -1 && !_main_death_voiceover_done && getTicksSince(time) < delay);

            speechSetEndCallback(NULL);

            speechDelete();

            while (mouseGetEvent() != 0) {
                sharedFpsLimiter.mark();

                inputGetInput();

                renderPresent();
                sharedFpsLimiter.throttle();
            }

            if (keyCode == -1) {
                inputPauseForTocks(500);
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
static void _main_death_voiceover_callback()
{
    _main_death_voiceover_done = true;
}

// Read endgame subtitle.
//
// 0x4814B4
static int _mainDeathGrabTextFile(const char* fileName, char* dest)
{
    const char* p = strrchr(fileName, '\\');
    if (p == NULL) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    sprintf(path, "text\\%s\\cuts\\%s%s", settings.system.language.c_str(), p + 1, ".TXT");

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
static int _mainDeathWordWrap(char* text, int width, short* beginnings, short* count)
{
    while (true) {
        char* sep = strchr(text, ':');
        if (sep == NULL) {
            break;
        }

        if (sep - 1 < text) {
            break;
        }
        sep[0] = ' ';
        sep[-1] = ' ';
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
static int mainMenuWindowInit()
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
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    gMainMenuWindowBuffer = windowGetBuffer(gMainMenuWindow);

    // mainmenu.frm
    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 140, 0, 0, 0);
    if (!_mainMenuBackgroundFrmImage.lock(backgroundFid)) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    blitBufferToBuffer(_mainMenuBackgroundFrmImage.getData(), 640, 480, 640, gMainMenuWindowBuffer, 640);
    _mainMenuBackgroundFrmImage.unlock();

    int oldFont = fontGetCurrent();
    fontSetCurrent(100);

    // SFALL: Allow to change font color/flags of copyright/version text
    //        It's the last byte ('3C' by default) that picks the colour used. The first byte supplies additional flags for this option
    //        0x010000 - change the color for version string only
    //        0x020000 - underline text (only for the version string)
    //        0x040000 - monospace font (only for the version string)
    int fontSettings = _colorTable[21091], fontSettingsSFall = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_FONT_COLOR_KEY, &fontSettingsSFall);
    if (fontSettingsSFall && !(fontSettingsSFall & 0x010000))
        fontSettings = fontSettingsSFall & 0xFF;

    // SFALL: Allow to move copyright text
    int offsetX = 0, offsetY = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_X_KEY, &offsetX);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_Y_KEY, &offsetY);

    // Copyright.
    msg.num = 20;
    if (messageListGetItem(&gMiscMessageList, &msg)) {
        windowDrawText(gMainMenuWindow, msg.text, 0, offsetX + 15, offsetY + 460, fontSettings | 0x06000000);
    }

    // SFALL: Make sure font settings are applied when using 0x010000 flag
    if (fontSettingsSFall)
        fontSettings = fontSettingsSFall;

    // TODO: Allow to move version text
    // Version.
    char version[VERSION_MAX];
    versionGetVersion(version);
    len = fontGetStringWidth(version);
    windowDrawText(gMainMenuWindow, version, 0, 615 - len, 460, fontSettings | 0x06000000);

    // menuup.frm
    fid = buildFid(OBJ_TYPE_INTERFACE, 299, 0, 0, 0);
    if (!_mainMenuButtonNormalFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    // menudown.frm
    fid = buildFid(OBJ_TYPE_INTERFACE, 300, 0, 0, 0);
    if (!_mainMenuButtonPressedFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = -1;
    }

    // SFALL: Allow to move menu buttons
    offsetX = offsetY = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_X_KEY, &offsetX);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_Y_KEY, &offsetY);

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = buttonCreate(gMainMenuWindow,
            offsetX + 30,
            offsetY + 19 + index * 42 - index,
            26,
            26,
            -1,
            -1,
            1111,
            gMainMenuButtonKeyBindings[index],
            _mainMenuButtonNormalFrmImage.getData(),
            _mainMenuButtonPressedFrmImage.getData(),
            0,
            BUTTON_FLAG_TRANSPARENT);
        if (gMainMenuButtons[index] == -1) {
            // NOTE: Uninline.
            return main_menu_fatal_error();
        }

        buttonSetMask(gMainMenuButtons[index], _mainMenuButtonNormalFrmImage.getData());
    }

    fontSetCurrent(104);

    // SFALL: Allow to change font color of buttons
    fontSettings = _colorTable[21091];
    fontSettingsSFall = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_BIG_FONT_COLOR_KEY, &fontSettingsSFall);
    if (fontSettingsSFall)
        fontSettings = fontSettingsSFall & 0xFF;

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        msg.num = 9 + index;
        if (messageListGetItem(&gMiscMessageList, &msg)) {
            len = fontGetStringWidth(msg.text);
            fontDrawText(gMainMenuWindowBuffer + offsetX + 640 * (offsetY + 42 * index - index + 20) + 126 - (len / 2), msg.text, 640 - (126 - (len / 2)) - 1, 640, fontSettings);
        }
    }

    fontSetCurrent(oldFont);

    gMainMenuWindowInitialized = true;
    gMainMenuWindowHidden = true;

    return 0;
}

// 0x481968
static void mainMenuWindowFree()
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

    _mainMenuButtonPressedFrmImage.unlock();
    _mainMenuButtonNormalFrmImage.unlock();

    if (gMainMenuWindow != -1) {
        windowDestroy(gMainMenuWindow);
    }

    gMainMenuWindowInitialized = false;
}

// 0x481A00
static void mainMenuWindowHide(bool animate)
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
static void mainMenuWindowUnhide(bool animate)
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
static int _main_menu_is_enabled()
{
    return 1;
}

// 0x481AEC
static int mainMenuWindowHandleEvents()
{
    _in_main_menu = true;

    bool oldCursorIsHidden = cursorIsHidden();
    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    unsigned int tick = getTicks();

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        for (int buttonIndex = 0; buttonIndex < MAIN_MENU_BUTTON_COUNT; buttonIndex++) {
            if (keyCode == gMainMenuButtonKeyBindings[buttonIndex] || keyCode == toupper(gMainMenuButtonKeyBindings[buttonIndex])) {
                // NOTE: Uninline.
                main_menu_play_sound("nmselec1");

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
                rc = MAIN_MENU_SCREENSAVER;
                continue;
            } else if (keyCode == 1111) {
                if (!(mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT)) {
                    // NOTE: Uninline.
                    main_menu_play_sound("nmselec0");
                }
                continue;
            }
        }

        if (keyCode == KEY_ESCAPE || _game_user_wants_to_quit == 3) {
            rc = MAIN_MENU_EXIT;

            // NOTE: Uninline.
            main_menu_play_sound("nmselec1");
            break;
        } else if (_game_user_wants_to_quit == 2) {
            _game_user_wants_to_quit = 0;
        } else {
            if (getTicksSince(tick) >= gMainMenuScreensaverDelay) {
                rc = MAIN_MENU_TIMEOUT;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (oldCursorIsHidden) {
        mouseHideCursor();
    }

    _in_main_menu = false;

    return rc;
}

// NOTE: Inlined.
//
// 0x481C88
static int main_menu_fatal_error()
{
    mainMenuWindowFree();

    return -1;
}

// NOTE: Inlined.
//
// 0x481C94
static void main_menu_play_sound(const char* fileName)
{
    soundPlayFile(fileName);
}

} // namespace fallout
