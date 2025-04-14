#include "main.h"

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

extern void* internal_malloc(size_t size);
extern void internal_free(void* ptr);

// 0x5194C8
static char _mainMap[] = "artemple.map";

// 0x5194D8
static int _main_game_paused = 0;

// 0x5194E8
static bool _main_show_death_scene = false;

// 0x614838
static bool _main_death_voiceover_done;

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
                gameMoviePlay(MOVIE_INTRO, GAME_MOVIE_STOP_MUSIC);
                gameMoviePlay(MOVIE_CREDITS, 0);
                break;
            case MAIN_MENU_NEW_GAME:
                mainMenuWindowHide(true);
                mainMenuWindowFree();
                if (characterSelectorOpen() == 2) {
                    gameMoviePlay(MOVIE_ELDER, GAME_MOVIE_STOP_MUSIC);
                    randomSeedPrerandom(-1);

                    // SFALL: Override starting map.
                    char* mapName = nullptr;
                    if (configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_STARTING_MAP_KEY, &mapName)) {
                        if (*mapName == '\0') {
                            mapName = nullptr;
                        }
                    }

                    char* mapNameCopy = compat_strdup(mapName != nullptr ? mapName : _mainMap);
                    _main_load_new(mapNameCopy);
                    free(mapNameCopy);

                    // SFALL: AfterNewGameStartHook.
                    sfall_gl_scr_exec_start_proc();

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
                    int win = windowCreate(0, 0, screenGetWidth(), screenGetHeight(), _colorTable[0], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
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
                mainMenuWindowHide(true);
                gameMoviePlay(MOVIE_INTRO, GAME_MOVIE_PAUSE_MUSIC);
                break;
            case MAIN_MENU_OPTIONS:
                mainMenuWindowHide(true);
                doPreferences(true);
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
    _game_user_wants_to_quit = 0;
    _main_show_death_scene = 0;
    gDude->flags &= ~OBJECT_FLAT;
    objectShow(gDude, nullptr);
    mouseHideCursor();

    int win = windowCreate(0, 0, screenGetWidth(), screenGetHeight(), _colorTable[0], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
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
static void showDeath() {
    artCacheFlush();
    colorCycleDisable();
    gameMouseSetCursor(MOUSE_CURSOR_NONE);

    bool wasCursorHidden = cursorIsHidden();
    if (wasCursorHidden) {
        mouseShowCursor();
    }

    // Read stretching setting from f2_res.ini
    int deathScreenStretchMode = 0;
    Config config;
    if (configInit(&config)) {
        if (configRead(&config, "f2_res.ini", false)) {
            configGetInt(&config, "STATIC_SCREENS", "DEATH_SCRN_SIZE", &deathScreenStretchMode);
        }
        configFree(&config);
    }

    // Get current screen dimensions.
    int screenWidth = screenGetWidth();
    int screenHeight = screenGetHeight();

    // Load the DEATH image.
    FrmImage deathFrmImage;
    int deathFid = buildFid(OBJ_TYPE_INTERFACE, 309, 0, 0, 0);
    if (!deathFrmImage.lock(deathFid)) {
        return;
    }

    // Get pointer to the death screen pixel data.
    unsigned char* deathData = deathFrmImage.getData();
    int deathScreenWidth = DEATH_WINDOW_WIDTH;
    int deathScreenHeight = DEATH_WINDOW_HEIGHT;

    unsigned char* finalBuffer = deathData;
    int finalWidth = deathScreenWidth;
    int finalHeight = deathScreenHeight;

    // Handle optional stretching if needed.
    if (deathScreenStretchMode != 0 || screenWidth < deathScreenWidth || screenHeight < deathScreenHeight) {
        int scaledWidth;
        int scaledHeight;

        // Stretch full screen for setting 2.
        if (deathScreenStretchMode == 2) {
            scaledWidth = screenWidth;
            scaledHeight = screenHeight;
        } else {
            // Scale while maintaining aspect ratio for setting 1.
            if (screenHeight * deathScreenWidth >= screenWidth * deathScreenHeight) {
                scaledWidth = screenWidth;
                scaledHeight = (screenWidth * deathScreenHeight + (deathScreenWidth)) / deathScreenWidth;
            } else {
                scaledWidth = (screenHeight * deathScreenWidth + (deathScreenHeight)) / deathScreenHeight;
                scaledHeight = screenHeight;
            }
        }

        // Allocate memory for the scaled image.
        unsigned char* scaled = reinterpret_cast<unsigned char*>(internal_malloc((scaledWidth + 1) * (scaledHeight + 1)));
        if (scaled != nullptr) {
            // Perform stretching.
            blitBufferToBufferStretch(deathData, deathScreenWidth, deathScreenHeight, deathScreenWidth, scaled, scaledWidth, scaledHeight, scaledWidth);
            
            // Fix rightmost edge artifacts by copying the last good pixel horizontally.
            for (int y = 0; y < scaledHeight; y++) {
                scaled[y * scaledWidth + (scaledWidth - 1)] = scaled[y * scaledWidth + (scaledWidth - 2)];
            }

            // Fix bottom row artifacts by copying the last good pixel vertically.
            for (int x = 0; x < scaledWidth; x++) {
                scaled[(scaledHeight - 1) * scaledWidth + x] = scaled[(scaledHeight - 2) * scaledWidth + x];
            }

            finalBuffer = scaled;
            finalWidth = scaledWidth;
            finalHeight = scaledHeight;
        }
    }

    // Center the death window on screen.
    int windowX = (screenWidth - finalWidth) / 2;
    int windowY = (screenHeight - finalHeight) / 2;
    int win = windowCreate(windowX, windowY, finalWidth, finalHeight, 0, WINDOW_MOVE_ON_TOP);

    if (win != -1) {
        do {
            unsigned char* windowBuffer = windowGetBuffer(win);
            if (windowBuffer == nullptr) {
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

            // Draw death image to window.
            blitBufferToBuffer(finalBuffer, finalWidth, finalHeight, finalWidth, windowBuffer, finalWidth);
            deathFrmImage.unlock();

            // Get death ending filename.
            const char* deathFileName = endgameDeathEndingGetFileName();

            // If subtitles are enabled, load and display them.
            if (settings.preferences.subtitles) {
                char text[512];
                if (_mainDeathGrabTextFile(deathFileName, text) == 0) {
                    debugPrint("\n((ShowDeath)): %s\n", text);

                    short lineStartOffsets[WORD_WRAP_MAX_COUNT];
                    short lineCount;

                    // Set subtitle box width based on final screen width.
                    int subtitleWidth = (finalWidth >= 640) ? 560 : finalWidth - 80;
                    if (subtitleWidth < 100) {
                        subtitleWidth = finalWidth - 20;
                    }

                    // Word wrap subtitle text.
                    if (_mainDeathWordWrap(text, subtitleWidth, lineStartOffsets, &lineCount) == 0) {
                        int lineHeight = fontGetLineHeight();
                        int subtitleHeight = lineHeight * lineCount + 2;

                        // Center subtitles near bottom of screen.
                        int subtitleX = (finalWidth - subtitleWidth) / 2;
                        int subtitleY = finalHeight - subtitleHeight - 8;

                        unsigned char* subtitleBuffer = windowBuffer + finalWidth * subtitleY + subtitleX;

                        bufferFill(subtitleBuffer - finalWidth * 1, subtitleWidth, subtitleHeight, finalWidth, 0);

                        // Draw each subtitle line.
                        for (int i = 0; i < lineCount; i++) {
                            fontDrawText(subtitleBuffer, text + lineStartOffsets[i], subtitleWidth, finalWidth, _colorTable[32767]);
                            subtitleBuffer += finalWidth * lineHeight;
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
                // If voiceover missing, fallback to a short delay.
                delay = 3000;
            } else {
                delay = UINT_MAX;
            }

            _gsound_speech_play_preloaded();
            inputBlockForTocks(100);

            // Wait for user input, voiceover completion, or timeout.
            unsigned int startTime = getTicks();
            int keyCode;
            do {
                sharedFpsLimiter.mark();
                keyCode = inputGetInput();
                renderPresent();
                sharedFpsLimiter.throttle();
            } while (keyCode == -1 && !_main_death_voiceover_done && getTicksSince(startTime) < delay);

            // Clean up voiceover.
            speechSetEndCallback(nullptr);
            speechDelete();

            while (mouseGetEvent() != 0) {
                sharedFpsLimiter.mark();
                inputGetInput();
                renderPresent();
                sharedFpsLimiter.throttle();
            }

            if (keyCode == -1) {
                // Pause briefly if no input was received.
                inputPauseForTocks(500);
            }

            // Fade out screen to black.
            paletteFadeTo(gPaletteBlack);
            colorPaletteLoad("color.pal");

        } while (0);

        // Destroy death window.
        windowDestroy(win);
    }

    if (wasCursorHidden) {
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
