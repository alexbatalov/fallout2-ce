#ifndef FALLOUT_SVGA_H_
#define FALLOUT_SVGA_H_

#include <SDL.h>

#include "fps_limiter.h"
#include "geometry.h"

namespace fallout {

extern Rect _scr_size;
extern void (*_scr_blit)(unsigned char* src, int src_pitch, int a3, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y);
extern void (*_zero_mem)();

extern SDL_Window* gSdlWindow;
extern SDL_Surface* gSdlSurface;
extern SDL_Renderer* gSdlRenderer;
extern SDL_Texture* gSdlTexture;
extern SDL_Surface* gSdlTextureSurface;
extern FpsLimiter sharedFpsLimiter;

int _init_mode_320_200();
int _init_mode_320_400();
int _init_mode_640_480_16();
int _init_mode_640_480();
int _init_mode_640_400();
int _init_mode_800_600();
int _init_mode_1024_768();
int _init_mode_1280_1024();
void _get_start_mode_();
void _zero_vid_mem();
int _GNW95_init_mode_ex(int width, int height, int bpp);
void adjustScaling(int& width, int& height, int& scale);
int _init_vesa_mode(int width, int height);
int _GNW95_init_window(int width, int height, bool fullscreen, int scale);
int directDrawInit(int width, int height, int bpp);
void directDrawFree();
void directDrawSetPaletteInRange(unsigned char* a1, int a2, int a3);
void directDrawSetPalette(unsigned char* palette);
unsigned char* directDrawGetPalette();
void _GNW95_ShowRect(unsigned char* src, int src_pitch, int a3, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y);
void _GNW95_zero_vid_mem();

int screenGetWidth();
int screenGetHeight();
int screenGetVisibleHeight();
void handleWindowSizeChanged();
void renderPresent();

} // namespace fallout

#endif /* FALLOUT_SVGA_H_ */
