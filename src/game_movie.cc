#include "game_movie.h"

#include "color.h"
#include "core.h"
#include "cycle.h"
#include "debug.h"
#include "game.h"
#include "game_config.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "movie.h"
#include "movie_effect.h"
#include "palette.h"
#include "platform_compat.h"
#include "text_font.h"
#include "widget.h"
#include "window_manager.h"

#include <stdio.h>
#include <string.h>

#define GAME_MOVIE_WINDOW_WIDTH 640
#define GAME_MOVIE_WINDOW_HEIGHT 480

static char* gameMovieBuildSubtitlesFilePath(char* movieFilePath);

// 0x50352A
static const float flt_50352A = 0.032258064f;

// 0x518DA0
static const char* gMovieFileNames[MOVIE_COUNT] = {
    "iplogo.mve",
    "intro.mve",
    "elder.mve",
    "vsuit.mve",
    "afailed.mve",
    "adestroy.mve",
    "car.mve",
    "cartucci.mve",
    "timeout.mve",
    "tanker.mve",
    "enclave.mve",
    "derrick.mve",
    "artimer1.mve",
    "artimer2.mve",
    "artimer3.mve",
    "artimer4.mve",
    "credits.mve",
};

// 0x518DE4
static const char* gMoviePaletteFilePaths[MOVIE_COUNT] = {
    NULL,
    "art\\cuts\\introsub.pal",
    "art\\cuts\\eldersub.pal",
    NULL,
    "art\\cuts\\artmrsub.pal",
    NULL,
    NULL,
    NULL,
    "art\\cuts\\artmrsub.pal",
    NULL,
    NULL,
    NULL,
    "art\\cuts\\artmrsub.pal",
    "art\\cuts\\artmrsub.pal",
    "art\\cuts\\artmrsub.pal",
    "art\\cuts\\artmrsub.pal",
    "art\\cuts\\crdtssub.pal",
};

// 0x518E28
static bool gGameMovieIsPlaying = false;

// 0x518E2C
static bool gGameMovieFaded = false;

// 0x596C78
static unsigned char gGameMoviesSeen[MOVIE_COUNT];

// 0x596C89
static char gGameMovieSubtitlesFilePath[COMPAT_MAX_PATH];

// gmovie_init
// 0x44E5C0
int gameMoviesInit()
{
    int v1 = 0;
    if (backgroundSoundIsEnabled()) {
        v1 = backgroundSoundGetVolume();
    }

    movieSetVolume(v1);

    movieSetBuildSubtitleFilePathProc(gameMovieBuildSubtitlesFilePath);

    memset(gGameMoviesSeen, 0, sizeof(gGameMoviesSeen));

    gGameMovieIsPlaying = false;
    gGameMovieFaded = false;

    return 0;
}

// 0x44E60C
void gameMoviesReset()
{
    memset(gGameMoviesSeen, 0, sizeof(gGameMoviesSeen));

    gGameMovieIsPlaying = false;
    gGameMovieFaded = false;
}

// 0x44E638
int gameMoviesLoad(File* stream)
{
    if (fileRead(gGameMoviesSeen, sizeof(*gGameMoviesSeen), MOVIE_COUNT, stream) != MOVIE_COUNT) {
        return -1;
    }

    return 0;
}

// 0x44E664
int gameMoviesSave(File* stream)
{
    if (fileWrite(gGameMoviesSeen, sizeof(*gGameMoviesSeen), MOVIE_COUNT, stream) != MOVIE_COUNT) {
        return -1;
    }

    return 0;
}

// gmovie_play
// 0x44E690
int gameMoviePlay(int movie, int flags)
{
    gGameMovieIsPlaying = true;

    const char* movieFileName = gMovieFileNames[movie];
    debugPrint("\nPlaying movie: %s\n", movieFileName);

    char* language;
    if (!configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_LANGUAGE_KEY, &language)) {
        debugPrint("\ngmovie_play() - Error: Unable to determine language!\n");
        gGameMovieIsPlaying = false;
        return -1;
    }

    char movieFilePath[COMPAT_MAX_PATH];
    int movieFileSize;
    bool movieFound = false;

    if (compat_stricmp(language, ENGLISH) != 0) {
        sprintf(movieFilePath, "art\\%s\\cuts\\%s", language, gMovieFileNames[movie]);
        movieFound = dbGetFileSize(movieFilePath, &movieFileSize) == 0;
    }

    if (!movieFound) {
        sprintf(movieFilePath, "art\\cuts\\%s", gMovieFileNames[movie]);
        movieFound = dbGetFileSize(movieFilePath, &movieFileSize) == 0;
    }

    if (!movieFound) {
        debugPrint("\ngmovie_play() - Error: Unable to open %s\n", gMovieFileNames[movie]);
        gGameMovieIsPlaying = false;
        return -1;
    }

    if ((flags & GAME_MOVIE_FADE_IN) != 0) {
        paletteFadeTo(gPaletteBlack);
        gGameMovieFaded = true;
    }

    int gameMovieWindowX = (screenGetWidth() - GAME_MOVIE_WINDOW_WIDTH) / 2;
    int gameMovieWindowY = (screenGetHeight() - GAME_MOVIE_WINDOW_HEIGHT) / 2;
    int win = windowCreate(gameMovieWindowX,
        gameMovieWindowY,
        GAME_MOVIE_WINDOW_WIDTH, 
        GAME_MOVIE_WINDOW_HEIGHT,
        0,
        WINDOW_FLAG_0x10);
    if (win == -1) {
        gGameMovieIsPlaying = false;
        return -1;
    }

    if ((flags & GAME_MOVIE_STOP_MUSIC) != 0) {
        backgroundSoundDelete();
    } else if ((flags & GAME_MOVIE_PAUSE_MUSIC) != 0) {
        backgroundSoundPause();
    }

    windowRefresh(win);

    bool subtitlesEnabled = false;
    int v1 = 4;
    configGetBool(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_SUBTITLES_KEY, &subtitlesEnabled);
    if (subtitlesEnabled) {
        char* subtitlesFilePath = gameMovieBuildSubtitlesFilePath(movieFilePath);

        int subtitlesFileSize;
        if (dbGetFileSize(subtitlesFilePath, &subtitlesFileSize) == 0) {
            v1 = 12;
        } else {
            subtitlesEnabled = false;
        }
    }

    movieSetFlags(v1);

    int oldTextColor;
    int oldFont;
    if (subtitlesEnabled) {
        const char* subtitlesPaletteFilePath;
        if (gMoviePaletteFilePaths[movie] != NULL) {
            subtitlesPaletteFilePath = gMoviePaletteFilePaths[movie];
        } else {
            subtitlesPaletteFilePath = "art\\cuts\\subtitle.pal";
        }

        colorPaletteLoad(subtitlesPaletteFilePath);

        oldTextColor = widgetGetTextColor();
        widgetSetTextColor(1.0, 1.0, 1.0);

        oldFont = fontGetCurrent();
        widgetSetFont(101);
    }

    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        gameMouseSetCursor(MOUSE_CURSOR_NONE);
        mouseShowCursor();
    }

    while (mouseGetEvent() != 0) {
        _mouse_info();
    }

    mouseHideCursor();
    colorCycleDisable();

    movieEffectsLoad(movieFilePath);

    _zero_vid_mem();
    _movieRun(win, movieFilePath);

    int v11 = 0;
    int buttons;
    do {
        if (!_moviePlaying() || _game_user_wants_to_quit || _get_input() != -1) {
            break;
        }

        int x;
        int y;
        _mouse_get_raw_state(&x, &y, &buttons);

        v11 |= buttons;
    } while ((v11 & 1) == 0 && (v11 & 2) == 0 || (buttons & 1) != 0 || (buttons & 2) != 0);

    _movieStop();
    _moviefx_stop();
    _movieUpdate();
    paletteSetEntries(gPaletteBlack);

    gGameMoviesSeen[movie] = 1;

    colorCycleEnable();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    if (!cursorWasHidden) {
        mouseShowCursor();
    }

    if (subtitlesEnabled) {
        colorPaletteLoad("color.pal");

        widgetSetFont(oldFont);

        float r = (float)((_Color2RGB_(oldTextColor) & 0x7C00) >> 10) * flt_50352A;
        float g = (float)((_Color2RGB_(oldTextColor) & 0x3E0) >> 5) * flt_50352A;
        float b = (float)(_Color2RGB_(oldTextColor) & 0x1F) * flt_50352A;
        widgetSetTextColor(r, g, b);
    }

    windowDestroy(win);

    if ((flags & GAME_MOVIE_PAUSE_MUSIC) != 0) {
        backgroundSoundResume();
    }

    if ((flags & GAME_MOVIE_FADE_OUT) != 0) {
        if (!subtitlesEnabled) {
            colorPaletteLoad("color.pal");
        }

        paletteFadeTo(_cmap);
        gGameMovieFaded = false;
    }

    gGameMovieIsPlaying = false;
    return 0;
}

// 0x44EAE4
void gameMovieFadeOut()
{
    if (gGameMovieFaded) {
        paletteFadeTo(_cmap);
        gGameMovieFaded = false;
    }
}

// 0x44EB04
bool gameMovieIsSeen(int movie)
{
    return gGameMoviesSeen[movie] == 1;
}

// 0x44EB14
bool gameMovieIsPlaying()
{
    return gGameMovieIsPlaying;
}

// 0x44EB1C
static char* gameMovieBuildSubtitlesFilePath(char* movieFilePath)
{
    char* language;
    configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_LANGUAGE_KEY, &language);

    char* path = movieFilePath;

    char* separator = strrchr(path, '\\');
    if (separator != NULL) {
        path = separator + 1;
    }

    sprintf(gGameMovieSubtitlesFilePath, "text\\%s\\cuts\\%s", language, path);

    char* pch = strrchr(gGameMovieSubtitlesFilePath, '.');
    if (*pch != '\0') {
        *pch = '\0';
    }

    strcpy(gGameMovieSubtitlesFilePath + strlen(gGameMovieSubtitlesFilePath), ".SVE");

    return gGameMovieSubtitlesFilePath;
}
