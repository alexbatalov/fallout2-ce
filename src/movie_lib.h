#ifndef MOVIE_LIB_H
#define MOVIE_LIB_H

#include <stddef.h>

namespace fallout {

typedef void*(MveMallocFunc)(size_t size);
typedef void(MveFreeFunc)(void* ptr);
typedef bool(MveReadFunc)(void* handle, void* buffer, int count);
typedef void(MveShowFrameFunc)(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y);
typedef void(MveSetPaletteFunc)(unsigned char* palette, int start, int count);

void MveSetMemory(MveMallocFunc* malloc_func, MveFreeFunc* free_func);
void MveSetIO(MveReadFunc* read_func);
void MveSetVolume(int volume);
void MveSetScreenSize(int width, int height);
void MveSetShowFrame(MveShowFrameFunc* proc);
void MveSetPalette(MveSetPaletteFunc* set_palette_func);
void MVE_rmFrameCounts(int* frame_count_ptr, int* frame_drop_count_ptr);
int MVE_rmPrepMovie(void* handle, int dx, int dy, unsigned char track);
int _MVE_rmStepMovie();
void MVE_rmEndMovie();
void MVE_ReleaseMem();

} // namespace fallout

#endif /* MOVIE_LIB_H */
