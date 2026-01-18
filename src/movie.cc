#include "movie.h"

#include <string.h>

#include <SDL.h>

#include "color.h"
#include "db.h"
#include "debug.h"
#include "draw.h"
#include "geometry.h"
#include "input.h"
#include "memory_manager.h"
#include "movie_effect.h"
#include "movie_lib.h"
#include "platform_compat.h"
#include "sound.h"
#include "svga.h"
#include "text_font.h"
#include "window.h"
#include "window_manager.h"

namespace fallout {

typedef struct MovieSubtitleListNode {
    int num;
    char* text;
    struct MovieSubtitleListNode* next;
} MovieSubtitleListNode;

static void* movieMallocImpl(size_t size);
static void movieFreeImpl(void* ptr);
static bool movieReadImpl(void* handle, void* buf, int count);
static void movieDirectImpl(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y);
static void movieBufferedImpl(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y);
static int _movieScaleSubRect(int win, unsigned char* data, int width, int height, int pitch);
static int _movieScaleSubRectAlpha(int win, unsigned char* data, int width, int height, int pitch);
static int _movieScaleWindowAlpha(int win, unsigned char* data, int width, int height, int pitch);
static int _blitAlpha(int win, unsigned char* data, int width, int height, int pitch);
static int _movieScaleWindow(int win, unsigned char* data, int width, int height, int pitch);
static int _blitNormal(int win, unsigned char* data, int width, int height, int pitch);
static void movieSetPaletteEntriesImpl(unsigned char* palette, int start, int end);
static void _cleanupMovie(int a1);
static void _cleanupLast();
static File* movieOpen(char* filePath);
static void movieLoadSubtitles(char* filePath);
static void movieRenderSubtitles();
static int _movieStart(int win, char* filePath);
static bool _localMovieCallback();
static int _stepMovie();

// 0x5195B8
static int gMovieWindow = -1;

// 0x5195BC
static int gMovieSubtitlesFont = -1;

// 0x5195C0
static MovieBlitFunc* gMovieBlitFuncs[2][2][2] = {
    {
        {
            _blitNormal,
            _blitNormal,
        },
        {
            _movieScaleWindow,
            _movieScaleSubRect,
        },
    },
    {
        {
            _blitAlpha,
            _blitAlpha,
        },
        {
            _movieScaleWindowAlpha,
            _movieScaleSubRectAlpha,
        },
    },
};

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

// 0x638E90
static unsigned char* _lastMovieBuffer;

// 0x638E94
static int _movieW;

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

static unsigned char* MVE_lastBuffer = nullptr;

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
static bool movieReadImpl(void* handle, void* buf, int count)
{
    return fileRead(buf, 1, count, (File*)handle) == count;
}

// 0x486654
static void movieDirectImpl(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y)
{
    int v14;
    int v15;

    SDL_Rect srcRect;
    srcRect.x = src_x;
    srcRect.y = src_y;
    srcRect.w = src_width;
    srcRect.h = src_height;

    v14 = gMovieWindowRect.right - gMovieWindowRect.left;
    v15 = gMovieWindowRect.right - gMovieWindowRect.left + 1;

    SDL_Rect destRect;

    if (_movieScaleFlag) {
        if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x08) != 0) {
            destRect.y = (gMovieWindowRect.bottom - gMovieWindowRect.top + 1 - dst_height) / 2;
            destRect.x = (v15 - 4 * src_width / 3) / 2;
        } else {
            destRect.y = _movieY + gMovieWindowRect.top;
            destRect.x = gMovieWindowRect.left + _movieX;
        }

        destRect.w = 4 * src_width / 3;
        destRect.h = dst_height;
    } else {
        if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x08) != 0) {
            destRect.y = (gMovieWindowRect.bottom - gMovieWindowRect.top + 1 - dst_height) / 2;
            destRect.x = (v15 - dst_width) / 2;
        } else {
            destRect.y = _movieY + gMovieWindowRect.top;
            destRect.x = gMovieWindowRect.left + _movieX;
        }
        destRect.w = dst_width;
        destRect.h = dst_height;
    }

    _lastMovieSX = src_x;
    _lastMovieSY = src_y;
    _lastMovieX = destRect.x;
    _lastMovieY = destRect.y;
    _lastMovieBH = src_height;
    _lastMovieW = destRect.w;
    MVE_lastBuffer = pixels;
    _lastMovieBW = src_width;
    _lastMovieH = destRect.h;

    // The code above assumes `gMovieWindowRect` is always at (0,0) which is not
    // the case in HRP. For blitting purposes we have to adjust it relative to
    // the actual origin. We do it here because the variables above need to stay
    // in movie window coordinate space (for proper subtitles positioning).
    destRect.x += gMovieWindowRect.left;
    destRect.y += gMovieWindowRect.top;

    _scr_blit(pixels, src_width, src_height, src_x, src_y, dst_width, dst_height, dst_x, dst_y);
    renderPresent();
}

// 0x486900
static void movieBufferedImpl(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y)
{
    if (gMovieWindow == -1) {
        return;
    }

    _lastMovieBW = src_width;
    MVE_lastBuffer = pixels;
    _lastMovieBH = src_width;
    _lastMovieW = dst_width;
    _lastMovieH = dst_height;
    _lastMovieX = src_x;
    _lastMovieY = src_y;
    _lastMovieSX = src_x;
    _lastMovieSY = src_y;

    MovieBlitFunc* func = gMovieBlitFuncs[_movieAlphaFlag][_movieScaleFlag][_movieSubRectFlag];
    if (func(gMovieWindow, pixels, src_width, src_height, src_width) != 0) {
        windowRefreshRect(gMovieWindow, &_movieRect);
    }
}

// 0x486B68
int _movieScaleSubRect(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = windowGetWidth(win);
    unsigned char* windowBuffer = windowGetBuffer(win) + windowWidth * _movieY + _movieX;
    if (width * 4 / 3 > _movieW) {
        gMovieFlags |= 0x01;
        return 0;
    }

    int v1 = width / 3;
    for (int y = 0; y < height; y++) {
        int x;
        for (x = 0; x < v1; x++) {
            unsigned int value = data[0];
            value |= data[1] << 8;
            value |= data[2] << 16;
            value |= data[2] << 24;

            *(unsigned int*)windowBuffer = value;

            windowBuffer += 4;
            data += 3;
        }

        for (x = x * 3; x < width; x++) {
            *windowBuffer++ = *data++;
        }

        data += pitch - width;
        windowBuffer += windowWidth - _movieW;
    }

    return 1;
}

// 0x486C74
int _movieScaleSubRectAlpha(int win, unsigned char* data, int width, int height, int pitch)
{
    gMovieFlags |= 1;
    return 0;
}

// NOTE: Uncollapsed 0x486C74.
int _movieScaleWindowAlpha(int win, unsigned char* data, int width, int height, int pitch)
{
    gMovieFlags |= 1;
    return 0;
}

// 0x486C80
int _blitAlpha(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = windowGetWidth(win);
    unsigned char* windowBuffer = windowGetBuffer(win);
    _alphaBltBuf(data, width, height, pitch, _alphaWindowBuf, _alphaBuf, windowBuffer + windowWidth * _movieY + _movieX, windowWidth);
    return 1;
}

// 0x486CD4
int _movieScaleWindow(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = windowGetWidth(win);
    if (width != 3 * windowWidth / 4) {
        gMovieFlags |= 1;
        return 0;
    }

    unsigned char* windowBuffer = windowGetBuffer(win);
    for (int y = 0; y < height; y++) {
        int scaledWidth = width / 3;
        for (int x = 0; x < scaledWidth; x++) {
            unsigned int value = data[0];
            value |= data[1] << 8;
            value |= data[2] << 16;
            value |= data[3] << 24;

            *(unsigned int*)windowBuffer = value;

            windowBuffer += 4;
            data += 3;
        }
        data += pitch - width;
    }

    return 1;
}

// 0x486D84
int _blitNormal(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = windowGetWidth(win);
    unsigned char* windowBuffer = windowGetBuffer(win);
    _drawScaled(windowBuffer + windowWidth * _movieY + _movieX, _movieW, _movieH, windowWidth, data, width, height, pitch);
    return 1;
}

// 0x486DDC
static void movieSetPaletteEntriesImpl(unsigned char* palette, int start, int end)
{
    if (end != 0) {
        gMovieSetPaletteEntriesProc(palette + start * 3, start, end + start - 1);
    }
}

// initMovie
// 0x486E0C
void movieInit()
{
    MveSetMemory(movieMallocImpl, movieFreeImpl);
    MveSetPalette(movieSetPaletteEntriesImpl);
    MveSetScreenSize(screenGetWidth(), screenGetHeight());
    MveSetIO(movieReadImpl);
}

// 0x486E98
static void _cleanupMovie(int a1)
{
    if (!_running) {
        return;
    }

    int frame;
    int dropped;
    MVE_rmFrameCounts(&frame, &dropped);
    debugPrint("Frames %d, dropped %d\n", frame, dropped);

    if (_lastMovieBuffer != nullptr) {
        internal_free_safe(_lastMovieBuffer, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 787
        _lastMovieBuffer = nullptr;
    }

    if (MVE_lastBuffer != nullptr) {
        _lastMovieBuffer = (unsigned char*)internal_malloc_safe(_lastMovieBH * _lastMovieBW, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 802
        memcpy(_lastMovieBuffer, MVE_lastBuffer, _lastMovieBW * _lastMovieBH);
        MVE_lastBuffer = nullptr;
    }

    if (a1) {
        MVE_rmEndMovie();
    }

    MVE_ReleaseMem();

    fileClose(gMovieFileStream);

    if (_alphaWindowBuf != nullptr) {
        blitBufferToBuffer(_alphaWindowBuf, _movieW, _movieH, _movieW, windowGetBuffer(gMovieWindow) + _movieY * windowGetWidth(gMovieWindow) + _movieX, windowGetWidth(gMovieWindow));
        windowRefreshRect(gMovieWindow, &_movieRect);
    }

    if (_alphaHandle != nullptr) {
        fileClose(_alphaHandle);
        _alphaHandle = nullptr;
    }

    if (_alphaBuf != nullptr) {
        internal_free_safe(_alphaBuf, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 840
        _alphaBuf = nullptr;
    }

    if (_alphaWindowBuf != nullptr) {
        internal_free_safe(_alphaWindowBuf, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 845
        _alphaWindowBuf = nullptr;
    }

    while (gMovieSubtitleHead != nullptr) {
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
        _lastMovieBuffer = nullptr;
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
    } else {
        _movieScaleFlag = 0;

        if ((gMovieFlags & MOVIE_EXTENDED_FLAG_0x04) == 0) {
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
    gMovieSetPaletteEntriesProc = proc != nullptr ? proc : _setSystemPaletteEntries;
}

// 0x487274
void movieSetPaletteProc(MovieSetPaletteProc* proc)
{
    gMoviePaletteProc = proc;
}

// 0x4872E8
static void _cleanupLast()
{
    if (_lastMovieBuffer != nullptr) {
        internal_free_safe(_lastMovieBuffer, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 981
        _lastMovieBuffer = nullptr;
    }

    MVE_lastBuffer = nullptr;
}

// 0x48731C
static File* movieOpen(char* filePath)
{
    gMovieFileStream = fileOpen(filePath, "rb");
    if (gMovieFileStream == nullptr) {
        debugPrint("Couldn't find movie file %s\n", filePath);
        return nullptr;
    }
    return gMovieFileStream;
}

// 0x487380
static void movieLoadSubtitles(char* filePath)
{
    _subtitleW = windowGetWidth(gMovieWindow);
    _subtitleH = fontGetLineHeight() + 4;

    if (gMovieBuildSubtitleFilePathProc != nullptr) {
        filePath = gMovieBuildSubtitleFilePathProc(filePath);
    }

    char path[COMPAT_MAX_PATH];
    strcpy(path, filePath);

    debugPrint("Opening subtitle file %s\n", path);
    File* stream = fileOpen(path, "r");
    if (stream == nullptr) {
        debugPrint("Couldn't open subtitle file %s\n", path);
        gMovieFlags &= ~MOVIE_EXTENDED_FLAG_0x10;
        return;
    }

    MovieSubtitleListNode* prev = nullptr;
    int subtitleCount = 0;
    while (!fileEof(stream)) {
        char string[260];
        string[0] = '\0';
        fileReadString(string, 259, stream);
        if (*string == '\0') {
            break;
        }

        MovieSubtitleListNode* subtitle = (MovieSubtitleListNode*)internal_malloc_safe(sizeof(*subtitle), __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1050
        subtitle->next = nullptr;

        subtitleCount++;

        char* pch;

        pch = strchr(string, '\n');
        if (pch != nullptr) {
            *pch = '\0';
        }

        pch = strchr(string, '\r');
        if (pch != nullptr) {
            *pch = '\0';
        }

        pch = strchr(string, ':');
        if (pch != nullptr) {
            *pch = '\0';
            subtitle->num = atoi(string);
            subtitle->text = strdup_safe(pch + 1, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1058

            if (prev != nullptr) {
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
    if (gMovieSubtitleHead == nullptr) {
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
    MVE_rmFrameCounts(&frame, &dropped);

    while (gMovieSubtitleHead != nullptr) {
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
static int _movieStart(int win, char* filePath)
{
    if (_running) {
        return 1;
    }

    _cleanupLast();

    gMovieFileStream = movieOpen(filePath);
    if (gMovieFileStream == nullptr) {
        return 1;
    }

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
        MveSetShowFrame(movieDirectImpl);
        MVE_rmPrepMovie(gMovieFileStream, _movieX + gMovieWindowRect.left, _movieY + gMovieWindowRect.top, 0);
    } else {
        debugPrint("Buffered ");
        MveSetShowFrame(movieBufferedImpl);
        MVE_rmPrepMovie(gMovieFileStream, 0, 0, 0);
    }

    if (_movieScaleFlag) {
        debugPrint("scaled\n");
    } else {
        debugPrint("not scaled\n");
    }

    if (_alphaHandle != nullptr) {
        int size;
        fileReadInt32(_alphaHandle, &size);

        short tmp;
        fileReadInt16(_alphaHandle, &tmp);
        fileReadInt16(_alphaHandle, &tmp);

        _alphaBuf = (unsigned char*)internal_malloc_safe(size, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1178
        _alphaWindowBuf = (unsigned char*)internal_malloc_safe(_movieH * _movieW, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1179

        unsigned char* windowBuffer = windowGetBuffer(gMovieWindow);
        blitBufferToBuffer(windowBuffer + windowGetWidth(gMovieWindow) * _movieY + _movieX,
            _movieW,
            _movieH,
            windowGetWidth(gMovieWindow),
            _alphaWindowBuf,
            _movieW);
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

    if (_movieCallback != nullptr) {
        _movieCallback();
    }

    return inputGetInput() != -1;
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
    return _movieStart(win, filePath);
}

// 0x487B1C
int _movieRunRect(int win, char* filePath, int x, int y, int w, int h)
{
    if (_running) {
        return 1;
    }

    _movieX = x;
    _movieY = y;
    _movieOffset = x + y * windowGetWidth(win);
    _movieW = w;
    _movieH = h;
    _movieSubRectFlag = 1;

    return _movieStart(win, filePath);
}

// 0x487B7C
static int _stepMovie()
{
    if (_alphaHandle != nullptr) {
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
    MveSetVolume(normalizedVolume);
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

    if (gMoviePaletteProc != nullptr) {
        int frame;
        int dropped;
        MVE_rmFrameCounts(&frame, &dropped);
        gMoviePaletteProc(frame);
    }
}

// 0x487C88
int _moviePlaying()
{
    return _running;
}

} // namespace fallout
