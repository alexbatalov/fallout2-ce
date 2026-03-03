#include "main.h"

#include <limits.h>
#include <string.h>
#include "xbox_debug.h"
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
#include "mainmenu.h"
#include "map.h"
#include "mouse.h"
#include "object.h"
#include "palette.h"
#include "platform_compat.h"
#include "preferences.h"
#include "proto.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "sfall_config.h"
#include "sfall_global_scripts.h"
#include "svga.h"
#include "text_font.h"
#include "window.h"
#include "window_manager.h"
#include "window_manager_private.h"
#include "word_wrap.h"
#include "worldmap.h"

namespace fallout {

#define DEATH_WINDOW_WIDTH 640
#define DEATH_WINDOW_HEIGHT 480

static bool falloutInit(int argc, char** argv);
static int main_reset_system();
static void main_exit_system();
static int _main_load_new(char* fname);
static int main_loadgame_new();
static void main_unload_new();
static void mainLoop();
static void showDeath();
static void _main_death_voiceover_callback();
static int _mainDeathGrabTextFile(const char* fileName, char* dest);
static int _mainDeathWordWrap(char* text, int width, short* beginnings, short* count);

// 0x5194C8
static char _mainMap[] = "artemple.map";

// 0x5194D8
static int _main_game_paused = 0;

// 0x5194E8
static bool _main_show_death_scene = false;

// 0x614838
static bool _main_death_voiceover_done;

// 0x48099C
// 0x48099C
int falloutMain(int argc, char** argv)
{
    DbgPrint("falloutMain: started\n");

    if (!autorunMutexCreate()) {
        DbgPrint("falloutMain: autorunMutexCreate failed\n");
        return 1;
    }

    DbgPrint("falloutMain: autorunMutexCreate succeeded\n");

    if (!falloutInit(argc, argv)) {
        DbgPrint("falloutMain: falloutInit failed\n");
        return 1;
    }

    DbgPrint("falloutMain: falloutInit succeeded\n");

    int skipOpeningMovies;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_SKIP_OPENING_MOVIES_KEY, &skipOpeningMovies);
    DbgPrint("falloutMain: skipOpeningMovies = %d\n", skipOpeningMovies);

    if (skipOpeningMovies < 1) {
        DbgPrint("falloutMain: playing intro movies\n");
        gameMoviePlay(MOVIE_IPLOGO, GAME_MOVIE_FADE_IN);
        gameMoviePlay(MOVIE_INTRO, 0);
        gameMoviePlay(MOVIE_CREDITS, 0);
    }

    if (mainMenuWindowInit() == 0) {
        DbgPrint("falloutMain: mainMenuWindowInit succeeded\n");

        bool done = false;
        while (!done) {
            DbgPrint("falloutMain: top of main loop\n");

            keyboardReset();
            _gsound_background_play_level_music("07desert", 11);
            mainMenuWindowUnhide(1);

            mouseShowCursor();
            int mainMenuRc = mainMenuWindowHandleEvents();
            DbgPrint("falloutMain: mainMenuWindowHandleEvents returned %d\n", mainMenuRc);
            mouseHideCursor();

            switch (mainMenuRc) {
            case MAIN_MENU_INTRO:
                DbgPrint("falloutMain: MAIN_MENU_INTRO\n");
                mainMenuWindowHide(true);
                gameMoviePlay(MOVIE_INTRO, GAME_MOVIE_STOP_MUSIC);
                gameMoviePlay(MOVIE_CREDITS, 0);
                break;
            case MAIN_MENU_NEW_GAME:
                DbgPrint("falloutMain: MAIN_MENU_NEW_GAME\n");
                mainMenuWindowHide(true);
                mainMenuWindowFree();

                if (characterSelectorOpen() == 2) {
                    DbgPrint("falloutMain: characterSelectorOpen returned 2\n");
                    gameMoviePlay(MOVIE_ELDER, GAME_MOVIE_STOP_MUSIC);
                    randomSeedPrerandom(-1);

                    char* mapName = nullptr;
                    if (configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_STARTING_MAP_KEY, &mapName)) {
                        if (*mapName == '\0') {
                            mapName = nullptr;
                        }
                    }

                    DbgPrint("falloutMain: using map %s\n", mapName != nullptr ? mapName : _mainMap);
                    char* mapNameCopy = compat_strdup(mapName != nullptr ? mapName : _mainMap);
                    _main_load_new(mapNameCopy);
                    free(mapNameCopy);

                    DbgPrint("falloutMain: mainLoop begin\n");
                    sfall_gl_scr_exec_start_proc();
                    mainLoop();
                    DbgPrint("falloutMain: mainLoop finished\n");

                    paletteFadeTo(gPaletteWhite);
                    main_unload_new();
                    main_reset_system();

                    if (_main_show_death_scene != 0) {
                        DbgPrint("falloutMain: showing death scene\n");
                        showDeath();
                        _main_show_death_scene = 0;
                    }
                }

                DbgPrint("falloutMain: reinitializing main menu\n");
                mainMenuWindowInit();
                break;

            case MAIN_MENU_LOAD_GAME:
                DbgPrint("falloutMain: MAIN_MENU_LOAD_GAME\n");
                {
                    int win = windowCreate(0, 0, screenGetWidth(), screenGetHeight(), _colorTable[0], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
                    mainMenuWindowHide(true);
                    mainMenuWindowFree();

                    main_loadgame_new();
                    
                    colorPaletteLoad("color.pal");
                    paletteFadeTo(_cmap);

                    int loadGameRc = lsgLoadGame(LOAD_SAVE_MODE_FROM_MAIN_MENU);
                    DbgPrint("falloutMain: lsgLoadGame returned %d\n", loadGameRc);

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

                    main_unload_new();
                    main_reset_system();

                    if (_main_show_death_scene != 0) {
                        DbgPrint("falloutMain: showing death scene after load\n");
                        showDeath();
                        _main_show_death_scene = 0;
                    }

                    mainMenuWindowInit();
                }
                break;

            case MAIN_MENU_TIMEOUT:
                DbgPrint("falloutMain: MAIN_MENU_TIMEOUT\n");
                debugPrint("Main menu timed-out\n");
                // FALLTHROUGH
            case MAIN_MENU_SCREENSAVER:
                DbgPrint("falloutMain: MAIN_MENU_SCREENSAVER\n");
                mainMenuWindowHide(true);
                gameMoviePlay(MOVIE_INTRO, GAME_MOVIE_PAUSE_MUSIC);
                break;

            case MAIN_MENU_OPTIONS:
                DbgPrint("falloutMain: MAIN_MENU_OPTIONS\n");
                mainMenuWindowHide(true);
                doPreferences(true);
                break;

            case MAIN_MENU_CREDITS:
                DbgPrint("falloutMain: MAIN_MENU_CREDITS\n");
                mainMenuWindowHide(true);
                creditsOpen("credits.txt", -1, false);
                break;

            case MAIN_MENU_QUOTES:
                DbgPrint("falloutMain: MAIN_MENU_QUOTES\n");
                mainMenuWindowHide(true);
                creditsOpen("quotes.txt", -1, true);
                break;

            case MAIN_MENU_EXIT:
            case -1:
                DbgPrint("falloutMain: MAIN_MENU_EXIT or -1\n");
                done = true;
                mainMenuWindowHide(true);
                mainMenuWindowFree();
                backgroundSoundDelete();
                break;

            case MAIN_MENU_SELFRUN:
                DbgPrint("falloutMain: MAIN_MENU_SELFRUN (unhandled)\n");
                break;

            default:
                DbgPrint("falloutMain: unknown menu result %d\n", mainMenuRc);
                break;
            }
        }
    } else {
        DbgPrint("falloutMain: mainMenuWindowInit failed\n");
    }

    main_exit_system();
    autorunMutexClose();

    DbgPrint("falloutMain: exiting cleanly\n");
    return 0;
}


// 0x480CC0
static bool falloutInit(int argc, char** argv)
{
    if (gameInitWithOptions("FALLOUT II", false, 0, 0, argc, argv) == -1) {
        return false;
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

    gameExit();
}

// 0x480D4C
static int _main_load_new(char* mapFileName)
{
#ifdef NXDK
    DbgPrint("[_main_load_new] Called with mapFileName = %s\n", mapFileName);
#endif

    _game_user_wants_to_quit = 0;
    _main_show_death_scene = 0;

#ifdef NXDK
    DbgPrint("[_main_load_new] Reset _game_user_wants_to_quit and _main_show_death_scene\n");
#endif

    gDude->flags &= ~OBJECT_FLAT;
#ifdef NXDK
    DbgPrint("[_main_load_new] Cleared OBJECT_FLAT from gDude->flags\n");
#endif

    objectShow(gDude, nullptr);
#ifdef NXDK
    DbgPrint("[_main_load_new] Called objectShow(gDude)\n");
#endif

    mouseHideCursor();
#ifdef NXDK
    DbgPrint("[_main_load_new] Called mouseHideCursor()\n");
#endif

    int win = windowCreate(0, 0, screenGetWidth(), screenGetHeight(), _colorTable[0], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
#ifdef NXDK
    DbgPrint("[_main_load_new] Created modal window: win = %d\n", win);
#endif

    windowRefresh(win);
#ifdef NXDK
    DbgPrint("[_main_load_new] Refreshed window %d\n", win);
#endif

    colorPaletteLoad("color.pal");
#ifdef NXDK
    DbgPrint("[_main_load_new] colorPaletteLoad(\"color.pal\") completed\n");
#endif

    paletteFadeTo(_cmap);
#ifdef NXDK
    DbgPrint("[_main_load_new] paletteFadeTo(_cmap) completed\n");
#endif

    _map_init();
#ifdef NXDK
    DbgPrint("[_main_load_new] _map_init() completed\n");
#endif

    gameMouseSetCursor(MOUSE_CURSOR_NONE);
#ifdef NXDK
    DbgPrint("[_main_load_new] gameMouseSetCursor(MOUSE_CURSOR_NONE)\n");
#endif

    mouseShowCursor();
#ifdef NXDK
    DbgPrint("[_main_load_new] mouseShowCursor()\n");
#endif

    mapLoadByName(mapFileName);
#ifdef NXDK
    DbgPrint("[_main_load_new] mapLoadByName(%s) completed\n", mapFileName);
#endif

    wmMapMusicStart();
#ifdef NXDK
    DbgPrint("[_main_load_new] wmMapMusicStart() completed\n");
#endif

    paletteFadeTo(gPaletteWhite);
#ifdef NXDK
    DbgPrint("[_main_load_new] paletteFadeTo(gPaletteWhite) completed\n");
#endif

    windowDestroy(win);
#ifdef NXDK
    DbgPrint("[_main_load_new] windowDestroy(%d) completed\n", win);
#endif

    colorPaletteLoad("color.pal");
#ifdef NXDK
    DbgPrint("[_main_load_new] colorPaletteLoad(\"color.pal\") (post-map) completed\n");
#endif

    paletteFadeTo(_cmap);
#ifdef NXDK
    DbgPrint("[_main_load_new] paletteFadeTo(_cmap) (post-map) completed\n");
    DbgPrint("[_main_load_new] Done. Returning 0.\n");
#endif

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

    objectShow(gDude, nullptr);
    mouseHideCursor();

    _map_init();

    gameMouseSetCursor(MOUSE_CURSOR_NONE);
    mouseShowCursor();

    return 0;
}

// 0x480E34
static void main_unload_new()
{
    objectHide(gDude, nullptr);
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

        // SFALL: MainLoopHook.
        sfall_gl_scr_process_main();

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
        WINDOW_MOVE_ON_TOP);
    if (win != -1) {
        do {
            unsigned char* windowBuffer = windowGetBuffer(win);
            if (windowBuffer == nullptr) {
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

            speechSetEndCallback(nullptr);

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
    if (p == nullptr) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "text\\%s\\cuts\\%s%s", settings.system.language.c_str(), p + 1, ".TXT");

    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
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
        if (sep == nullptr) {
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

        if (p != nullptr) {
            *p = '\0';
            beginnings[index]++;
        }
    }

    return 0;
}

} // namespace fallout
