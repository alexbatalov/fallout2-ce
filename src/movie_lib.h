#ifndef MOVIE_LIB_H
#define MOVIE_LIB_H

#include <SDL.h>

#include "memory_defs.h"

namespace fallout {

typedef bool MovieReadProc(int fileHandle, void* buffer, int count);
typedef void(MovieShowFrameProc)(SDL_Surface*, int, int, int, int, int, int, int, int);

void movieLibSetMemoryProcs(MallocProc* mallocProc, FreeProc* freeProc);
void movieLibSetReadProc(MovieReadProc* readProc);
void movieLibSetVolume(int volume);
void movieLibSetPan(int pan);
void _MVE_sfSVGA(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9);
void _MVE_sfCallbacks(MovieShowFrameProc* proc);
void movieLibSetPaletteEntriesProc(void (*fn)(unsigned char*, int, int));
void _MVE_rmCallbacks(int (*fn)());
void _sub_4F4BB(int a1);
void _MVE_rmFrameCounts(int* a1, int* a2);
int _MVE_rmPrepMovie(int fileHandle, int a2, int a3, char a4);
int _MVE_rmStepMovie();
void _MVE_rmEndMovie();
void _MVE_ReleaseMem();

} // namespace fallout

#endif /* MOVIE_LIB_H */
