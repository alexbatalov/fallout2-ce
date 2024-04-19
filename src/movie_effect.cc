#include "movie_effect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "debug.h"
#include "memory.h"
#include "movie.h"
#include "palette.h"
#include "platform_compat.h"

namespace fallout {

typedef enum MovieEffectType {
    MOVIE_EFFECT_TYPE_NONE = 0,
    MOVIE_EFFECT_TYPE_FADE_IN = 1,
    MOVIE_EFFECT_TYPE_FADE_OUT = 2,
} MovieEffectFadeType;

typedef struct MovieEffect {
    int startFrame;
    int endFrame;
    int steps;
    unsigned char fadeType;
    // range 0-63
    unsigned char r;
    // range 0-63
    unsigned char g;
    // range 0-63
    unsigned char b;
    struct MovieEffect* next;
} MovieEffect;

static void _moviefx_callback_func(int frame);
static void _moviefx_palette_func(unsigned char* palette, int start, int end);
static void movieEffectsClear();

// 0x5195F0
static bool gMovieEffectsInitialized = false;

// 0x5195F4
static MovieEffect* gMovieEffectHead = nullptr;

// 0x638EC4
static unsigned char _source_palette[768];

// 0x6391C4
static bool _inside_fade;

// 0x487CC0
int movieEffectsInit()
{
    if (gMovieEffectsInitialized) {
        return -1;
    }

    memset(_source_palette, 0, sizeof(_source_palette));

    gMovieEffectsInitialized = true;

    return 0;
}

// 0x487CF4
void movieEffectsReset()
{
    if (!gMovieEffectsInitialized) {
        return;
    }

    movieSetPaletteProc(nullptr);
    _movieSetPaletteFunc(nullptr);
    movieEffectsClear();

    _inside_fade = false;

    memset(_source_palette, 0, sizeof(_source_palette));
}

// 0x487D30
void movieEffectsExit()
{
    if (!gMovieEffectsInitialized) {
        return;
    }

    movieSetPaletteProc(nullptr);
    _movieSetPaletteFunc(nullptr);
    movieEffectsClear();

    _inside_fade = false;

    memset(_source_palette, 0, sizeof(_source_palette));
}

// 0x487D7C
int movieEffectsLoad(const char* filePath)
{
    if (!gMovieEffectsInitialized) {
        return -1;
    }

    movieSetPaletteProc(nullptr);
    _movieSetPaletteFunc(nullptr);
    movieEffectsClear();
    _inside_fade = false;
    memset(_source_palette, 0, sizeof(_source_palette));

    if (filePath == nullptr) {
        return -1;
    }

    Config config;
    if (!configInit(&config)) {
        return -1;
    }

    int rc = -1;

    char path[COMPAT_MAX_PATH];
    strcpy(path, filePath);

    char* pch = strrchr(path, '.');
    if (pch != nullptr) {
        *pch = '\0';
    }

    strcpy(path + strlen(path), ".cfg");

    int* movieEffectFrameList;

    if (!configRead(&config, path, true)) {
        goto out;
    }

    int movieEffectsLength;
    if (!configGetInt(&config, "info", "total_effects", &movieEffectsLength)) {
        goto out;
    }

    movieEffectFrameList = (int*)internal_malloc(sizeof(*movieEffectFrameList) * movieEffectsLength);
    if (movieEffectFrameList == nullptr) {
        goto out;
    }

    bool frameListRead;
    if (movieEffectsLength >= 2) {
        frameListRead = configGetIntList(&config, "info", "effect_frames", movieEffectFrameList, movieEffectsLength);
    } else {
        frameListRead = configGetInt(&config, "info", "effect_frames", &(movieEffectFrameList[0]));
    }

    if (frameListRead) {
        int movieEffectsCreated = 0;
        for (int index = 0; index < movieEffectsLength; index++) {
            char section[20];
            compat_itoa(movieEffectFrameList[index], section, 10);

            char* fadeTypeString;
            if (!configGetString(&config, section, "fade_type", &fadeTypeString)) {
                continue;
            }

            int fadeType = MOVIE_EFFECT_TYPE_NONE;
            if (compat_stricmp(fadeTypeString, "in") == 0) {
                fadeType = MOVIE_EFFECT_TYPE_FADE_IN;
            } else if (compat_stricmp(fadeTypeString, "out") == 0) {
                fadeType = MOVIE_EFFECT_TYPE_FADE_OUT;
            }

            if (fadeType == MOVIE_EFFECT_TYPE_NONE) {
                continue;
            }

            int fadeColor[3];
            if (!configGetIntList(&config, section, "fade_color", fadeColor, 3)) {
                continue;
            }

            int steps;
            if (!configGetInt(&config, section, "fade_steps", &steps)) {
                continue;
            }

            MovieEffect* movieEffect = (MovieEffect*)internal_malloc(sizeof(*movieEffect));
            if (movieEffect == nullptr) {
                continue;
            }

            memset(movieEffect, 0, sizeof(*movieEffect));
            movieEffect->startFrame = movieEffectFrameList[index];
            movieEffect->endFrame = movieEffect->startFrame + steps - 1;
            movieEffect->steps = steps;
            movieEffect->fadeType = fadeType & 0xFF;
            movieEffect->r = fadeColor[0] & 0xFF;
            movieEffect->g = fadeColor[1] & 0xFF;
            movieEffect->b = fadeColor[2] & 0xFF;

            if (movieEffect->startFrame <= 1) {
                _inside_fade = true;
            }

            movieEffect->next = gMovieEffectHead;
            gMovieEffectHead = movieEffect;

            movieEffectsCreated++;
        }

        if (movieEffectsCreated != 0) {
            movieSetPaletteProc(_moviefx_callback_func);
            _movieSetPaletteFunc(_moviefx_palette_func);
            rc = 0;
        }
    }

    internal_free(movieEffectFrameList);

out:

    configFree(&config);

    return rc;
}

// 0x4880F0
void _moviefx_stop()
{
    if (!gMovieEffectsInitialized) {
        return;
    }

    movieSetPaletteProc(nullptr);
    _movieSetPaletteFunc(nullptr);

    movieEffectsClear();

    _inside_fade = false;
    memset(_source_palette, 0, sizeof(_source_palette));
}

// 0x488144
static void _moviefx_callback_func(int frame)
{
    MovieEffect* movieEffect = gMovieEffectHead;
    while (movieEffect != nullptr) {
        if (frame >= movieEffect->startFrame && frame <= movieEffect->endFrame) {
            break;
        }
        movieEffect = movieEffect->next;
    }

    if (movieEffect != nullptr) {
        unsigned char palette[768];
        int step = frame - movieEffect->startFrame + 1;

        if (movieEffect->fadeType == MOVIE_EFFECT_TYPE_FADE_IN) {
            for (int index = 0; index < 256; index++) {
                palette[index * 3] = movieEffect->r - (step * (movieEffect->r - _source_palette[index * 3]) / movieEffect->steps);
                palette[index * 3 + 1] = movieEffect->g - (step * (movieEffect->g - _source_palette[index * 3 + 1]) / movieEffect->steps);
                palette[index * 3 + 2] = movieEffect->b - (step * (movieEffect->b - _source_palette[index * 3 + 2]) / movieEffect->steps);
            }
        } else {
            for (int index = 0; index < 256; index++) {
                palette[index * 3] = _source_palette[index * 3] - (step * (_source_palette[index * 3] - movieEffect->r) / movieEffect->steps);
                palette[index * 3 + 1] = _source_palette[index * 3 + 1] - (step * (_source_palette[index * 3 + 1] - movieEffect->g) / movieEffect->steps);
                palette[index * 3 + 2] = _source_palette[index * 3 + 2] - (step * (_source_palette[index * 3 + 2] - movieEffect->b) / movieEffect->steps);
            }
        }

        paletteSetEntries(palette);
    }

    _inside_fade = movieEffect != nullptr;
}

// 0x4882AC
static void _moviefx_palette_func(unsigned char* palette, int start, int end)
{
    memcpy(_source_palette + 3 * start, palette, 3 * (end - start + 1));

    if (!_inside_fade) {
        paletteSetEntriesInRange(palette, start, end);
    }
}

// 0x488310
static void movieEffectsClear()
{
    MovieEffect* movieEffect = gMovieEffectHead;
    while (movieEffect != nullptr) {
        MovieEffect* next = movieEffect->next;
        internal_free(movieEffect);
        movieEffect = next;
    }

    gMovieEffectHead = nullptr;
}

} // namespace fallout
