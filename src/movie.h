#ifndef MOVIE_H
#define MOVIE_H

#include "geometry.h"

namespace fallout {

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

typedef char* MovieBuildSubtitleFilePathProc(char* movieFilePath);
typedef void MovieSetPaletteEntriesProc(unsigned char* palette, int start, int end);
typedef void MovieSetPaletteProc(int frame);
typedef void(MovieFrameGrabProc)(unsigned char* data, int width, int height, int pitch);
typedef void(MovieCaptureFrameProc)(unsigned char* data, int width, int height, int pitch, int movieX, int movieY, int movieWidth, int movieHeight);
typedef int(MovieBlitFunc)(int win, unsigned char* data, int width, int height, int pitch);
typedef void(MoviePreDrawFunc)(int win, Rect* rect);
typedef void(MovieStartFunc)(int win);
typedef void(MovieEndFunc)(int win, int x, int y, int width, int height);
typedef int(MovieFailedOpenFunc)(char* path);

void movieInit();
void movieExit();
void _movieStop();
int movieSetFlags(int a1);
void _movieSetPaletteFunc(MovieSetPaletteEntriesProc* proc);
void movieSetPaletteProc(MovieSetPaletteProc* proc);
int _movieRun(int win, char* filePath);
int _movieRunRect(int win, char* filePath, int a3, int a4, int a5, int a6);
void movieSetBuildSubtitleFilePathProc(MovieBuildSubtitleFilePathProc* proc);
void movieSetVolume(int volume);
void _movieUpdate();
int _moviePlaying();

} // namespace fallout

#endif /* MOVIE_H */
