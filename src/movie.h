#ifndef MOVIE_H
#define MOVIE_H

#include "db.h"
#include "geometry.h"

#include <SDL.h>

typedef enum MovieFlags {
    MOVIE_FLAG_0x01 = 0x01,
    MOVIE_FLAG_0x02 = 0x02,
    MOVIE_FLAG_0x04 = 0x04,
    MOVIE_FLAG_0x08 = 0x08,
} MovieFlags;

typedef enum MovieExtendedFlags {
    MOVIE_EXTENDED_FLAG_0x01 = 0x01,
    MOVIE_EXTENDED_FLAG_0x02 = 0x02,
    MOVIE_EXTENDED_FLAG_0x04 = 0x04,
    MOVIE_EXTENDED_FLAG_0x08 = 0x08,
    MOVIE_EXTENDED_FLAG_0x10 = 0x10,
} MovieExtendedFlags;

typedef struct MovieSubtitleListNode {
    int num;
    char* text;
    struct MovieSubtitleListNode* next;
} MovieSubtitleListNode;

typedef char* MovieBuildSubtitleFilePathProc(char* movieFilePath);
typedef void MovieSetPaletteEntriesProc(unsigned char* palette, int start, int end);
typedef void MovieSetPaletteProc(int frame);

extern int gMovieWindow;
extern int gMovieSubtitlesFont;
extern MovieSetPaletteEntriesProc* gMovieSetPaletteEntriesProc;
extern int gMovieSubtitlesColorR;
extern int gMovieSubtitlesColorG;
extern int gMovieSubtitlesColorB;

extern Rect gMovieWindowRect;
extern Rect _movieRect;
extern void (*_movieCallback)();
extern MovieSetPaletteProc* gMoviePaletteProc;
extern int (*_failedOpenFunc)(char* filePath);
extern MovieBuildSubtitleFilePathProc* gMovieBuildSubtitleFilePathProc;
extern int _subtitleW;
extern int _lastMovieBH;
extern int _lastMovieBW;
extern int _lastMovieSX;
extern int _lastMovieSY;
extern int _movieScaleFlag;
extern int _lastMovieH;
extern int _lastMovieW;
extern int _lastMovieX;
extern int _lastMovieY;
extern MovieSubtitleListNode* gMovieSubtitleHead;
extern unsigned int gMovieFlags;
extern int _movieAlphaFlag;
extern bool _movieSubRectFlag;
extern int _movieH;
extern int _movieOffset;
extern void (*_movieCaptureFrameFunc)(void*, int, int, int, int, int);
extern unsigned char* _lastMovieBuffer;
extern int _movieW;
extern void (*_movieFrameGrabFunc)();
extern int _subtitleH;
extern int _running;
extern File* gMovieFileStream;
extern unsigned char* _alphaWindowBuf;
extern int _movieX;
extern int _movieY;
extern bool gMovieDirectSoundInitialized;
extern File* _alphaHandle;
extern unsigned char* _alphaBuf;

extern SDL_Surface* gMovieSdlSurface;

void* movieMallocImpl(size_t size);
void movieFreeImpl(void* ptr);
bool movieReadImpl(int fileHandle, void* buf, int count);
void movieDirectImpl(SDL_Surface* a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9);
void movieBufferedImpl(SDL_Surface* a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9);
int _movieScaleSubRectAlpha(int a1);
int _blitAlpha(int win, unsigned char* a2, int a3, int a4, int a5);
int _blitNormal(int win, int a2, int a3, int a4, int a5);
void movieSetPaletteEntriesImpl(unsigned char* palette, int start, int end);
int _noop();
void movieInit();
void _cleanupMovie(int a1);
void movieExit();
void _movieStop();
int movieSetFlags(int a1);
void _movieSetPaletteFunc(MovieSetPaletteEntriesProc* proc);
void movieSetPaletteProc(MovieSetPaletteProc* proc);
void _cleanupLast();
File* movieOpen(char* filePath);
void movieLoadSubtitles(char* filePath);
void movieRenderSubtitles();
int _movieStart(int win, char* filePath, int (*a3)());
bool _localMovieCallback();
int _movieRun(int win, char* filePath);
int _movieRunRect(int win, char* filePath, int a3, int a4, int a5, int a6);
int _stepMovie();
void movieSetBuildSubtitleFilePathProc(MovieBuildSubtitleFilePathProc* proc);
void movieSetVolume(int volume);
void _movieUpdate();
int _moviePlaying();

#endif /* MOVIE_H */
