#include "movie.h"

#include "color.h"
#include "core.h"
#include "db.h"
#include "debug.h"
#include "draw.h"
#include "game_config.h"
#include "geometry.h"
#include "memory_manager.h"
#include "movie_effect.h"
#include "movie_lib.h"
#include "platform_compat.h"
#include "pointer_registry.h"
#include "sound.h"
#include "text_font.h"
#include "window_manager.h"

#include <string.h>

#include <SDL.h>

typedef struct MovieSubtitleListNode {
    int num;
    char* text;
    struct MovieSubtitleListNode* next;
} MovieSubtitleListNode;

static void* movieMallocImpl(size_t size);
static void movieFreeImpl(void* ptr);
static bool movieReadImpl(int fileHandle, void* buf, int count);
static void movieDirectImpl(SDL_Surface* a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9);
static void movieBufferedImpl(SDL_Surface* a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9);
static int _movieScaleSubRectAlpha(int a1);
static int _blitAlpha(int win, unsigned char* a2, int a3, int a4, int a5);
static int _blitNormal(int win, int a2, int a3, int a4, int a5);
static void movieSetPaletteEntriesImpl(unsigned char* palette, int start, int end);
static int _noop();
static void _cleanupMovie(int a1);
static void _cleanupLast();
static File* movieOpen(char* filePath);
static void movieLoadSubtitles(char* filePath);
static void movieRenderSubtitles();
static int _movieStart(int win, char* filePath, int (*a3)());
static bool _localMovieCallback();
static int _stepMovie();

// 0x5195B8
static int gMovieWindow = -1;

// 0x5195BC
static int gMovieSubtitlesFont = -1;

// 0x5195E0
static MovieSetPaletteEntriesProc* gMovieSetPaletteEntriesProc = _setSystemPaletteEntries;

// 0x5195E4
static int gMovieSubtitlesColorR = 31;

// 0x5195E8
static int gMovieSubtitlesColorG = 31;

// 0x5195EC
static int gMovieSubtitlesColorB = 31;

// 0x638E10
static Rect gMovieWindowRect;

// 0x638E20
static Rect _movieRect;

// 0x638E30
static void (*_movieCallback)();

// 0x638E38
static MovieSetPaletteProc* gMoviePaletteProc;

// NOTE: Some kind of callback which was intended to change movie file path
// in place during opening movie file to find subsitutions. This callback is
// never set.
//
// 0x638E3C
static int (*_failedOpenFunc)(char* filePath);

// 0x638E40
static MovieBuildSubtitleFilePathProc* gMovieBuildSubtitleFilePathProc;

// 0x638E48
static int _subtitleW;

// 0x638E4C
static int _lastMovieBH;

// 0x638E50
static int _lastMovieBW;

// 0x638E54
static int _lastMovieSX;

// 0x638E58
static int _lastMovieSY;

// 0x638E5C
static int _movieScaleFlag;

// 0x638E64
static int _lastMovieH;

// 0x638E68
static int _lastMovieW;

// 0x638E6C
static int _lastMovieX;

// 0x638E70
static int _lastMovieY;

// 0x638E74
static MovieSubtitleListNode* gMovieSubtitleHead;

// 0x638E78
static unsigned int gMovieFlags;

// 0x638E7C
static int _movieAlphaFlag;

// 0x638E80
static bool _movieSubRectFlag;

// 0x638E84
static int _movieH;

// 0x638E88
static int _movieOffset;

// 0x638E8C
static void (*_movieCaptureFrameFunc)(void*, int, int, int, int, int);

// 0x638E90
static unsigned char* _lastMovieBuffer;

// 0x638E94
static int _movieW;

// 0x638E98
static void (*_movieFrameGrabFunc)();

// 0x638EA0
static int _subtitleH;

// 0x638EA4
static int _running;

// 0x638EA8
static File* gMovieFileStream;

// 0x638EAC
static unsigned char* _alphaWindowBuf;

// 0x638EB0
static int _movieX;

// 0x638EB4
static int _movieY;

// 0x638EBC
static File* _alphaHandle;

// 0x638EC0
static unsigned char* _alphaBuf;

static SDL_Surface* gMovieSdlSurface = NULL;
static int gMovieFileStreamPointerKey = 0;

// 0x4865FC
static void* movieMallocImpl(size_t size)
{
    return internal_malloc_safe(size, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 209
}

// 0x486614
static void movieFreeImpl(void* ptr)
{
    internal_free_safe(ptr, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 213
}

// 0x48662C
static bool movieReadImpl(int fileHandle, void* buf, int count)
{
    return fileRead(buf, 1, count, (File*)intToPtr(fileHandle)) == count;
}

// 0x486654
static void movieDirectImpl(SDL_Surface* surface, int srcWidth, int srcHeight, int srcX, int srcY, int destWidth, int destHeight, int a8, int a9)
{
    int v14;
    int v15;

    SDL_Rect srcRect;
    srcRect.x = srcX;
    srcRect.y = srcY;
    srcRect.w = srcWidth;
    srcRect.h = srcHeight;

    v14 = gMovieWindowRect.right - gMovieWindowRect.left;
    v15 = gMovieWindowRect.right - gMovieWindowRect.left + 1;

    SDL_Rect destRect;

    if (_movieScaleFlag) {
        if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x08) != 0) {
            destRect.y = (gMovieWindowRect.bottom - gMovieWindowRect.top + 1 - destHeight) / 2;
            destRect.x = (v15 - 4 * srcWidth / 3) / 2;
        } else {
            destRect.y = _movieY + gMovieWindowRect.top;
            destRect.x = gMovieWindowRect.left + _movieX;
        }

        destRect.w = 4 * srcWidth / 3;
        destRect.h = destHeight;
    } else {
        if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x08) != 0) {
            destRect.y = (gMovieWindowRect.bottom - gMovieWindowRect.top + 1 - destHeight) / 2;
            destRect.x = (v15 - destWidth) / 2;
        } else {
            destRect.y = _movieY + gMovieWindowRect.top;
            destRect.x = gMovieWindowRect.left + _movieX;
        }
        destRect.w = destWidth;
        destRect.h = destHeight;
    }

    _lastMovieSX = srcX;
    _lastMovieSY = srcY;
    _lastMovieX = destRect.x;
    _lastMovieY = destRect.y;
    _lastMovieBH = srcHeight;
    _lastMovieW = destRect.w;
    gMovieSdlSurface = surface;
    _lastMovieBW = srcWidth;
    _lastMovieH = destRect.h;

    // The code above assumes `gMovieWindowRect` is always at (0,0) which is not
    // the case in HRP. For blitting purposes we have to adjust it relative to
    // the actual origin. We do it here because the variables above need to stay
    // in movie window coordinate space (for proper subtitles positioning).
    destRect.x += gMovieWindowRect.left;
    destRect.y += gMovieWindowRect.top;

    if (_movieCaptureFrameFunc != NULL) {
        if (SDL_LockSurface(surface) == 0) {
            _movieCaptureFrameFunc(surface->pixels, srcWidth, destRect.x, destRect.y, destRect.w, destRect.h);
            SDL_UnlockSurface(surface);
        }
    }

    // TODO: This is a super-ugly hack. The reason is that surfaces managed by
    // MVE does not have palette. If we blit from these internal surfaces into
    // backbuffer surface (with palette set), all we get is shiny white box.
    SDL_SetSurfacePalette(surface, gSdlSurface->format->palette);
    SDL_BlitSurface(surface, &srcRect, gSdlSurface, &destRect);
    SDL_BlitSurface(gSdlSurface, NULL, gSdlWindowSurface, NULL);
    SDL_UpdateWindowSurface(gSdlWindow);    
}

// 0x486900
static void movieBufferedImpl(SDL_Surface* a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)
{
    int v13;

    if (gMovieWindow == -1) {
        return;
    }

    _lastMovieBW = a2;
    gMovieSdlSurface = a1;
    _lastMovieBH = a2;
    _lastMovieW = a6;
    _lastMovieH = a7;
    _lastMovieX = a4;
    _lastMovieY = a5;
    _lastMovieSX = a4;
    _lastMovieSY = a5;

    if (SDL_LockSurface(a1) != 0) {
        return;
    }

    if (_movieCaptureFrameFunc != NULL) {
        // TODO: Ignore, _movieCaptureFrameFunc is never set.
        // _movieCaptureFrameFunc()
    }

    if (_movieFrameGrabFunc != NULL) {
        // TODO: Ignore, _movieFrameGrabFunc is never set.
        // _movieFrameGrabFunc();
    } else {
        v13 = 4 * _movieSubRectFlag + 8 * _movieScaleFlag + 16 * _movieAlphaFlag;
        // TODO: Incomplete.
    }

    SDL_UnlockSurface(a1);
}

// 0x486C74
static int _movieScaleSubRectAlpha(int a1)
{
    gMovieFlags |= 1;
    return 0;
}

// 0x486C80
static int _blitAlpha(int win, unsigned char* a2, int a3, int a4, int a5)
{
    unsigned char* buf;
    int offset;

    offset = windowGetWidth(win) * _movieY + _movieX;
    buf = windowGetBuffer(win);

    // TODO: Incomplete.
    // _alphaBltBuf(a2, a3, a4, a5, _alphaWindowBuf, _alphaBuf, buf + offset, windowGetWidth(win));

    return 1;
}

// 0x486D84
static int _blitNormal(int win, int a2, int a3, int a4, int a5)
{
    unsigned char* buf;
    int offset;

    offset = windowGetWidth(win) * _movieY + _movieX;
    buf = windowGetBuffer(win);

    // TODO: Incomplete.
    // _drawScaled(buf + offset, _movieW, _movieH, windowGetWidth(win), a2, a3, a4, a5);

    return 1;
}

// 0x486DDC
static void movieSetPaletteEntriesImpl(unsigned char* palette, int start, int end)
{
    if (end != 0) {
        gMovieSetPaletteEntriesProc(palette + start * 3, start, end + start - 1);
    }
}

// 0x486E08
static int _noop()
{
    return 0;
}

// initMovie
// 0x486E0C
void movieInit()
{
    movieLibSetMemoryProcs(movieMallocImpl, movieFreeImpl);
    movieLibSetPaletteEntriesProc(movieSetPaletteEntriesImpl);
    _MVE_sfSVGA(640, 480, 480, 0, 0, 0, 0, 0, 0);
    movieLibSetReadProc(movieReadImpl);
}

// 0x486E98
static void _cleanupMovie(int a1)
{
    if (!_running) {
        return;
    }

    // TODO: Probably can be ignored.
    // if (_endMovieFunc) {
    //     _endMovieFunc(_movieW, _movieX, _movieH);
    // }

    int frame;
    int dropped;
    _MVE_rmFrameCounts(&frame, &dropped);
    debugPrint("Frames %d, dropped %d\n", frame, dropped);

    if (_lastMovieBuffer != NULL) {
        internal_free_safe(_lastMovieBuffer, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 787
        _lastMovieBuffer = NULL;
    }

    if (gMovieSdlSurface != NULL) {
        if (SDL_LockSurface(gMovieSdlSurface) == 0) {
            _lastMovieBuffer = (unsigned char*)internal_malloc_safe(_lastMovieBH * _lastMovieBW, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 802
            blitBufferToBuffer((unsigned char*)gMovieSdlSurface->pixels + gMovieSdlSurface->pitch * _lastMovieSX + _lastMovieSY, _lastMovieBW, _lastMovieBH, gMovieSdlSurface->pitch, _lastMovieBuffer, _lastMovieBW);
            SDL_UnlockSurface(gMovieSdlSurface);
        } else {
            debugPrint("Couldn't lock movie surface\n");
        }

        gMovieSdlSurface = NULL;
    }

    if (a1) {
        _MVE_rmEndMovie();
    }

    _MVE_ReleaseMem();

    fileClose(gMovieFileStream);

    if (_alphaWindowBuf != NULL) {
        blitBufferToBuffer(_alphaWindowBuf, _movieW, _movieH, _movieW, windowGetBuffer(gMovieWindow) + _movieY * windowGetWidth(gMovieWindow) + _movieX, windowGetWidth(gMovieWindow));
        windowRefreshRect(gMovieWindow, &_movieRect);
    }

    if (_alphaHandle != NULL) {
        fileClose(_alphaHandle);
        _alphaHandle = NULL;
    }

    if (_alphaBuf != NULL) {
        internal_free_safe(_alphaBuf, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 840
        _alphaBuf = NULL;
    }

    if (_alphaWindowBuf != NULL) {
        internal_free_safe(_alphaWindowBuf, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 845
        _alphaWindowBuf = NULL;
    }

    while (gMovieSubtitleHead != NULL) {
        MovieSubtitleListNode* next = gMovieSubtitleHead->next;
        internal_free_safe(gMovieSubtitleHead->text, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 851
        internal_free_safe(gMovieSubtitleHead, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 852
        gMovieSubtitleHead = next;
    }

    _running = 0;
    _movieSubRectFlag = 0;
    _movieScaleFlag = 0;
    _movieAlphaFlag = 0;
    gMovieFlags = 0;
    gMovieWindow = -1;
}

// 0x48711C
void movieExit()
{
    _cleanupMovie(1);

    if (_lastMovieBuffer) {
        internal_free_safe(_lastMovieBuffer, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 869
        _lastMovieBuffer = NULL;
    }
}

// 0x487150
void _movieStop()
{
    if (_running) {
        gMovieFlags |= MOVIE_EXTENDED_FLAG_0x02;
    }
}

// 0x487164
int movieSetFlags(int flags)
{
    if ((flags & MOVIE_FLAG_0x04) != 0) {
        gMovieFlags |= MOVIE_EXTENDED_FLAG_0x04 | MOVIE_EXTENDED_FLAG_0x08;
    } else {
        gMovieFlags &= ~MOVIE_EXTENDED_FLAG_0x08;
        if ((flags & MOVIE_FLAG_0x02) != 0) {
            gMovieFlags |= MOVIE_EXTENDED_FLAG_0x04;
        } else {
            gMovieFlags &= ~MOVIE_EXTENDED_FLAG_0x04;
        }
    }

    if ((flags & MOVIE_FLAG_0x01) != 0) {
        _movieScaleFlag = 1;

        if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x04) != 0) {
            _sub_4F4BB(3);
        }
    } else {
        _movieScaleFlag = 0;

        if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x04) != 0) {
            _sub_4F4BB(4);
        } else {
            gMovieFlags &= ~MOVIE_EXTENDED_FLAG_0x08;
        }
    }

    if ((flags & MOVIE_FLAG_0x08) != 0) {
        gMovieFlags |= MOVIE_EXTENDED_FLAG_0x10;
    } else {
        gMovieFlags &= ~MOVIE_EXTENDED_FLAG_0x10;
    }

    return 0;
}

// 0x48725C
void _movieSetPaletteFunc(MovieSetPaletteEntriesProc* proc)
{
    gMovieSetPaletteEntriesProc = proc != NULL ? proc : _setSystemPaletteEntries;
}

// 0x487274
void movieSetPaletteProc(MovieSetPaletteProc* proc)
{
    gMoviePaletteProc = proc;
}

// 0x4872E8
static void _cleanupLast()
{
    if (_lastMovieBuffer != NULL) {
        internal_free_safe(_lastMovieBuffer, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 981
        _lastMovieBuffer = NULL;
    }

    gMovieSdlSurface = NULL;
}

// 0x48731C
static File* movieOpen(char* filePath)
{
    gMovieFileStream = fileOpen(filePath, "rb");
    if (gMovieFileStream == NULL) {
        if (_failedOpenFunc == NULL) {
            debugPrint("Couldn't find movie file %s\n", filePath);
            return 0;
        }

        while (gMovieFileStream == NULL && _failedOpenFunc(filePath) != 0) {
            gMovieFileStream = fileOpen(filePath, "rb");
        }
    }
    return gMovieFileStream;
}

// 0x487380
static void movieLoadSubtitles(char* filePath)
{
    _subtitleW = windowGetWidth(gMovieWindow);
    _subtitleH = fontGetLineHeight() + 4;

    if (gMovieBuildSubtitleFilePathProc != NULL) {
        filePath = gMovieBuildSubtitleFilePathProc(filePath);
    }

    char path[COMPAT_MAX_PATH];
    strcpy(path, filePath);

    debugPrint("Opening subtitle file %s\n", path);
    File* stream = fileOpen(path, "r");
    if (stream == NULL) {
        debugPrint("Couldn't open subtitle file %s\n", path);
        gMovieFlags &= ~MOVIE_EXTENDED_FLAG_0x10;
        return;
    }

    MovieSubtitleListNode* prev = NULL;
    int subtitleCount = 0;
    while (!fileEof(stream)) {
        char string[260];
        string[0] = '\0';
        fileReadString(string, 259, stream);
        if (*string == '\0') {
            break;
        }

        MovieSubtitleListNode* subtitle = (MovieSubtitleListNode*)internal_malloc_safe(sizeof(*subtitle), __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1050
        subtitle->next = NULL;

        subtitleCount++;

        char* pch;

        pch = string;
        while (*pch != '\0' && *pch != '\n') {
            pch++;
        }

        if (*pch != '\0') {
            *pch = '\0';
        }

        pch = string;
        while (*pch != '\0' && *pch != '\r') {
            pch++;
        }

        if (*pch != '\0') {
            *pch = '\0';
        }

        pch = string;
        while (*pch != '\0' && *pch != ':') {
            pch++;
        }

        if (*pch != '\0') {
            *pch = '\0';
            subtitle->num = atoi(string);
            subtitle->text = strdup_safe(pch + 1, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1058

            if (prev != NULL) {
                prev->next = subtitle;
            } else {
                gMovieSubtitleHead = subtitle;
            }

            prev = subtitle;
        } else {
            debugPrint("subtitle: couldn't parse %s\n", string);
        }
    }

    fileClose(stream);

    debugPrint("Read %d subtitles\n", subtitleCount);
}

// 0x48755C
static void movieRenderSubtitles()
{
    if (gMovieSubtitleHead == NULL) {
        return;
    }

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x10) == 0) {
        return;
    }

    int v1 = fontGetLineHeight();
    int v2 = (480 - _lastMovieH - _lastMovieY - v1) / 2 + _lastMovieH + _lastMovieY;

    if (_subtitleH + v2 > _windowGetYres()) {
        _subtitleH = _windowGetYres() - v2;
    }

    int frame;
    int dropped;
    _MVE_rmFrameCounts(&frame, &dropped);

    while (gMovieSubtitleHead != NULL) {
        if (frame < gMovieSubtitleHead->num) {
            break;
        }

        MovieSubtitleListNode* next = gMovieSubtitleHead->next;

        windowFill(gMovieWindow, 0, v2, _subtitleW, _subtitleH, 0);

        int oldFont;
        if (gMovieSubtitlesFont != -1) {
            oldFont = fontGetCurrent();
            fontSetCurrent(gMovieSubtitlesFont);
        }

        int colorIndex = (gMovieSubtitlesColorR << 10) | (gMovieSubtitlesColorG << 5) | gMovieSubtitlesColorB;
        _windowWrapLine(gMovieWindow, gMovieSubtitleHead->text, _subtitleW, _subtitleH, 0, v2, _colorTable[colorIndex] | 0x2000000, TEXT_ALIGNMENT_CENTER);

        Rect rect;
        rect.right = _subtitleW;
        rect.top = v2;
        rect.bottom = v2 + _subtitleH;
        rect.left = 0;
        windowRefreshRect(gMovieWindow, &rect);

        internal_free_safe(gMovieSubtitleHead->text, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1108
        internal_free_safe(gMovieSubtitleHead, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1109

        gMovieSubtitleHead = next;

        if (gMovieSubtitlesFont != -1) {
            fontSetCurrent(oldFont);
        }
    }
}

// 0x487710
static int _movieStart(int win, char* filePath, int (*a3)())
{
    int v15;
    int v16;
    int v17;

    if (_running) {
        return 1;
    }

    _cleanupLast();

    gMovieFileStream = movieOpen(filePath);
    if (gMovieFileStream == NULL) {
        return 1;
    }

    gMovieFileStreamPointerKey = ptrToInt(gMovieFileStream);

    gMovieWindow = win;
    _running = 1;
    gMovieFlags &= ~MOVIE_EXTENDED_FLAG_0x01;

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x10) != 0) {
        movieLoadSubtitles(filePath);
    }

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x04) != 0) {
        debugPrint("Direct ");
        windowGetRect(gMovieWindow, &gMovieWindowRect);
        debugPrint("Playing at (%d, %d)  ", _movieX + gMovieWindowRect.left, _movieY + gMovieWindowRect.top);
        _MVE_rmCallbacks(a3);
        _MVE_sfCallbacks(movieDirectImpl);

        v17 = 0;
        v16 = _movieY + gMovieWindowRect.top;
        v15 = _movieX + gMovieWindowRect.left;
    } else {
        debugPrint("Buffered ");
        _MVE_rmCallbacks(a3);
        _MVE_sfCallbacks(movieBufferedImpl);
        v17 = 0;
        v16 = 0;
        v15 = 0;
    }

    _MVE_rmPrepMovie(gMovieFileStreamPointerKey, v15, v16, v17);

    if (_movieScaleFlag) {
        debugPrint("scaled\n");
    } else {
        debugPrint("not scaled\n");
    }

    // TODO: Probably can be ignored, never set.
    // if (_startMovieFunc) {
    //     _startMovieFunc();
    // }

    if (_alphaHandle != NULL) {
        // TODO: Probably can be ignored, never set.
        abort();
    }

    _movieRect.left = _movieX;
    _movieRect.top = _movieY;
    _movieRect.right = _movieW + _movieX;
    _movieRect.bottom = _movieH + _movieY;

    return 0;
}

// 0x487964
static bool _localMovieCallback()
{
    movieRenderSubtitles();

    if (_movieCallback != NULL) {
        _movieCallback();
    }

    return _get_input() != -1;
}

// 0x487AC8
int _movieRun(int win, char* filePath)
{
    if (_running) {
        return 1;
    }

    _movieX = 0;
    _movieY = 0;
    _movieOffset = 0;
    _movieW = windowGetWidth(win);
    _movieH = windowGetHeight(win);
    _movieSubRectFlag = 0;
    return _movieStart(win, filePath, _noop);
}

// 0x487B1C
int _movieRunRect(int win, char* filePath, int a3, int a4, int a5, int a6)
{
    if (_running) {
        return 1;
    }

    _movieX = a3;
    _movieY = a4;
    _movieOffset = a3 + a4 * windowGetWidth(win);
    _movieW = a5;
    _movieH = a6;
    _movieSubRectFlag = 1;

    return _movieStart(win, filePath, _noop);
}

// 0x487B7C
static int _stepMovie()
{
    if (_alphaHandle != NULL) {
        int size;
        fileReadInt32(_alphaHandle, &size);
        fileRead(_alphaBuf, 1, size, _alphaHandle);
    }

    int v1 = _MVE_rmStepMovie();
    if (v1 != -1) {
        movieRenderSubtitles();
    }

    return v1;
}

// 0x487BC8
void movieSetBuildSubtitleFilePathProc(MovieBuildSubtitleFilePathProc* proc)
{
    gMovieBuildSubtitleFilePathProc = proc;
}

// 0x487BD0
void movieSetVolume(int volume)
{
    int normalizedVolume = _soundVolumeHMItoDirectSound(volume);
    movieLibSetVolume(normalizedVolume);
}

// 0x487BEC
void _movieUpdate()
{
    if (!_running) {
        return;
    }

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x02) != 0) {
        debugPrint("Movie aborted\n");
        _cleanupMovie(1);
        return;
    }

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x01) != 0) {
        debugPrint("Movie error\n");
        _cleanupMovie(1);
        return;
    }

    if (_stepMovie() == -1) {
        _cleanupMovie(1);
        return;
    }

    if (gMoviePaletteProc != NULL) {
        int frame;
        int dropped;
        _MVE_rmFrameCounts(&frame, &dropped);
        gMoviePaletteProc(frame);
    }
}

// 0x487C88
int _moviePlaying()
{
    return _running;
}
