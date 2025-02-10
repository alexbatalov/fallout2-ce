#ifndef MOVIE_LIB_H
#define MOVIE_LIB_H

#include <SDL.h>

namespace fallout {

typedef void*(MveMallocFunc)(size_t size);
typedef void(MveFreeFunc)(void* ptr);
typedef bool(MveReadFunc)(void* handle, void* buffer, int count);
typedef void(MovieShowFrameProc)(SDL_Surface*, int, int, int, int, int, int, int, int);
typedef void(MveSetPaletteFunc)(unsigned char* palette, int start, int count);

void MveSetMemory(MveMallocFunc* malloc_func, MveFreeFunc* free_func);
void MveSetIO(MveReadFunc* read_func);
void MveSetVolume(int volume);
void MveSetScreenSize(int width, int height);
void _MVE_sfCallbacks(MovieShowFrameProc* proc);
void MveSetPalette(MveSetPaletteFunc* set_palette_func);
void _sub_4F4BB(int a1);
void MVE_rmFrameCounts(int* frame_count_ptr, int* frame_drop_count_ptr);
int MVE_rmPrepMovie(void* handle, int dx, int dy, unsigned char track);
int _MVE_rmStepMovie();
void _MVE_rmEndMovie();
void _MVE_ReleaseMem();

} // namespace fallout

#endif /* MOVIE_LIB_H */
