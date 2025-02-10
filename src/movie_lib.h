#ifndef MOVIE_LIB_H
#define MOVIE_LIB_H

#include <SDL.h>

namespace fallout {

typedef void*(MveMallocFunc)(size_t size);
typedef void(MveFreeFunc)(void* ptr);
typedef bool(MveReadFunc)(void* handle, void* buffer, int count);
typedef void(MovieShowFrameProc)(SDL_Surface*, int, int, int, int, int, int, int, int);

void MveSetMemory(MveMallocFunc* malloc_func, MveFreeFunc* free_func);
void MveSetIO(MveReadFunc* read_func);
void MveSetVolume(int volume);
void MveSetScreenSize(int width, int height);
void _MVE_sfCallbacks(MovieShowFrameProc* proc);
void movieLibSetPaletteEntriesProc(void (*fn)(unsigned char*, int, int));
void _MVE_rmCallbacks(int (*fn)());
void _sub_4F4BB(int a1);
void MVE_rmFrameCounts(int* frame_count_ptr, int* frame_drop_count_ptr);
int _MVE_rmPrepMovie(void* handle, int a2, int a3, char a4);
int _MVE_rmStepMovie();
void _MVE_rmEndMovie();
void _MVE_ReleaseMem();

} // namespace fallout

#endif /* MOVIE_LIB_H */
