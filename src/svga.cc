#include "svga.h"

#include <limits.h>
#include <string.h>

#include <SDL.h>

#include "audio_engine.h"
#include "color.h"
#include "config.h"
#include "dinput.h"
#include "draw.h"
#include "interface.h"
#include "kb.h"
#include "memory.h"
#include "mmx.h"
#include "mouse.h"
#include "text_font.h"
#include "vcr.h"
#include "win32.h"
#include "window_manager.h"
#include "window_manager_private.h"

namespace fallout {

static bool createRenderer(int width, int height);
static void destroyRenderer();

// NOTE: This value is never set, so it's impossible to understand it's
// meaning.
//
// 0x51E2C4
void (*_update_palette_func)() = NULL;

// 0x51E2C8
bool gMmxEnabled = true;

// 0x51E2CC
bool gMmxProbed = false;

// 0x6AC7F0
unsigned short gSixteenBppPalette[256];

// screen rect
Rect _scr_size;

// 0x6ACA00
int gGreenMask;

// 0x6ACA04
int gRedMask;

// 0x6ACA08
int gBlueMask;

// 0x6ACA0C
int gBlueShift;

// 0x6ACA10
int gRedShift;

// 0x6ACA14
int gGreenShift;

// 0x6ACA18
void (*_scr_blit)(unsigned char* src, int src_pitch, int a3, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y) = _GNW95_ShowRect;

// 0x6ACA1C
void (*_zero_mem)() = NULL;

// 0x6ACA20
bool gMmxSupported;

// FIXME: This buffer was supposed to be used as temporary place to store
// current palette while switching video modes (changing resolution). However
// the original game does not have UI to change video mode. Even if it did this
// buffer it too small to hold the entire palette, which require 256 * 3 bytes.
//
// 0x6ACA24
unsigned char gLastVideoModePalette[268];

SDL_Window* gSdlWindow = NULL;
SDL_Surface* gSdlSurface = NULL;
SDL_Renderer* gSdlRenderer = NULL;
SDL_Texture* gSdlTexture = NULL;
SDL_Surface* gSdlTextureSurface = NULL;

// TODO: Remove once migration to update-render cycle is completed.
FpsLimiter sharedFpsLimiter;

// 0x4CACD0
void mmxSetEnabled(bool a1)
{
    if (!gMmxProbed) {
        gMmxSupported = mmxIsSupported();
        gMmxProbed = true;
    }

    if (gMmxSupported) {
        gMmxEnabled = a1;
    }
}

// 0x4CAD08
int _init_mode_320_200()
{
    return _GNW95_init_mode_ex(320, 200, 8);
}

// 0x4CAD40
int _init_mode_320_400()
{
    return _GNW95_init_mode_ex(320, 400, 8);
}

// 0x4CAD5C
int _init_mode_640_480_16()
{
    return -1;
}

// 0x4CAD64
int _init_mode_640_480()
{
    return _init_vesa_mode(640, 480);
}

// 0x4CAD94
int _init_mode_640_400()
{
    return _init_vesa_mode(640, 400);
}

// 0x4CADA8
int _init_mode_800_600()
{
    return _init_vesa_mode(800, 600);
}

// 0x4CADBC
int _init_mode_1024_768()
{
    return _init_vesa_mode(1024, 768);
}

// 0x4CADD0
int _init_mode_1280_1024()
{
    return _init_vesa_mode(1280, 1024);
}

// 0x4CADF8
void _get_start_mode_()
{
}

// 0x4CADFC
void _zero_vid_mem()
{
    if (_zero_mem) {
        _zero_mem();
    }
}

// 0x4CAE1C
int _GNW95_init_mode_ex(int width, int height, int bpp)
{
    bool fullscreen = true;

    Config resolutionConfig;
    if (configInit(&resolutionConfig)) {
        if (configRead(&resolutionConfig, "f2_res.ini", false)) {
            int screenWidth;
            if (configGetInt(&resolutionConfig, "MAIN", "SCR_WIDTH", &screenWidth)) {
                width = screenWidth;
            }

            int screenHeight;
            if (configGetInt(&resolutionConfig, "MAIN", "SCR_HEIGHT", &screenHeight)) {
                height = screenHeight;
            }

            bool windowed;
            if (configGetBool(&resolutionConfig, "MAIN", "WINDOWED", &windowed)) {
                fullscreen = !windowed;
            }

            configGetBool(&resolutionConfig, "IFACE", "IFACE_BAR_MODE", &gInterfaceBarMode);
        }
        configFree(&resolutionConfig);
    }

    if (_GNW95_init_window(width, height, fullscreen) == -1) {
        return -1;
    }

    if (directDrawInit(width, height, bpp) == -1) {
        return -1;
    }

    _scr_size.left = 0;
    _scr_size.top = 0;
    _scr_size.right = width - 1;
    _scr_size.bottom = height - 1;

    mmxSetEnabled(true);

    if (bpp == 8) {
        _mouse_blit_trans = NULL;
        _scr_blit = _GNW95_ShowRect;
        _zero_mem = _GNW95_zero_vid_mem;
        _mouse_blit = _GNW95_ShowRect;
    } else {
        _zero_mem = NULL;
        _mouse_blit = _GNW95_MouseShowRect16;
        _mouse_blit_trans = _GNW95_MouseShowTransRect16;
        _scr_blit = _GNW95_ShowRect16;
    }

    return 0;
}

// 0x4CAECC
int _init_vesa_mode(int width, int height)
{
    return _GNW95_init_mode_ex(width, height, 8);
}

// 0x4CAEDC
int _GNW95_init_window(int width, int height, bool fullscreen)
{
    if (gSdlWindow == NULL) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            return -1;
        }

        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

        if (fullscreen) {
            windowFlags |= SDL_WINDOW_FULLSCREEN;
        }

        gSdlWindow = SDL_CreateWindow(gProgramWindowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, windowFlags);
        if (gSdlWindow == NULL) {
            return -1;
        }

        if (!createRenderer(width, height)) {
            destroyRenderer();

            SDL_DestroyWindow(gSdlWindow);
            gSdlWindow = NULL;

            return -1;
        }
    }

    return 0;
}

// calculate shift for mask
// 0x4CAF50
int getShiftForBitMask(int mask)
{
    int shift = 0;

    if ((mask & 0xFFFF0000) != 0) {
        shift |= 16;
        mask &= 0xFFFF0000;
    }

    if ((mask & 0xFF00FF00) != 0) {
        shift |= 8;
        mask &= 0xFF00FF00;
    }

    if ((mask & 0xF0F0F0F0) != 0) {
        shift |= 4;
        mask &= 0xF0F0F0F0;
    }

    if ((mask & 0xCCCCCCCC) != 0) {
        shift |= 2;
        mask &= 0xCCCCCCCC;
    }

    if ((mask & 0xAAAAAAAA) != 0) {
        shift |= 1;
    }

    return shift;
}

// 0x4CAF9C
int directDrawInit(int width, int height, int bpp)
{
    if (gSdlSurface != NULL) {
        unsigned char* palette = directDrawGetPalette();
        directDrawFree();

        if (directDrawInit(width, height, bpp) == -1) {
            return -1;
        }

        directDrawSetPalette(palette);

        return 0;
    }

    gSdlSurface = SDL_CreateRGBSurface(0, width, height, bpp, 0, 0, 0, 0);

    if (bpp == 8) {
        SDL_Color colors[256];
        for (int index = 0; index < 256; index++) {
            colors[index].r = index;
            colors[index].g = index;
            colors[index].b = index;
            colors[index].a = 255;
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);
    } else {
        gRedMask = gSdlSurface->format->Rmask;
        gGreenMask = gSdlSurface->format->Gmask;
        gBlueMask = gSdlSurface->format->Bmask;

        gRedShift = gSdlSurface->format->Rshift;
        gGreenShift = gSdlSurface->format->Gshift;
        gBlueShift = gSdlSurface->format->Bshift;
    }

    return 0;
}

// 0x4CB1B0
void directDrawFree()
{
    if (gSdlSurface != NULL) {
        SDL_FreeSurface(gSdlSurface);
        gSdlSurface = NULL;
    }
}

// 0x4CB310
void directDrawSetPaletteInRange(unsigned char* palette, int start, int count)
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
        SDL_Color colors[256];

        if (count != 0) {
            for (int index = 0; index < count; index++) {
                colors[index].r = palette[index * 3] << 2;
                colors[index].g = palette[index * 3 + 1] << 2;
                colors[index].b = palette[index * 3 + 2] << 2;
                colors[index].a = 255;
            }
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, start, count);
        SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
    } else {
        for (int index = start; index < start + count; index++) {
            unsigned short r = palette[0] << 2;
            unsigned short g = palette[1] << 2;
            unsigned short b = palette[2] << 2;
            palette += 3;

            r = gRedShift > 0 ? (r << gRedShift) : (r >> -gRedShift);
            r &= gRedMask;

            g = gGreenShift > 0 ? (g << gGreenShift) : (g >> -gGreenShift);
            g &= gGreenMask;

            b = gBlueShift > 0 ? (b << gBlueShift) : (b >> -gBlueShift);
            b &= gBlueMask;

            unsigned short rgb = r | g | b;
            gSixteenBppPalette[index] = rgb;
        }

        windowRefreshAll(&_scr_size);
    }

    if (_update_palette_func != NULL) {
        _update_palette_func();
    }
}

// 0x4CB568
void directDrawSetPalette(unsigned char* palette)
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
        SDL_Color colors[256];

        for (int index = 0; index < 256; index++) {
            colors[index].r = palette[index * 3] << 2;
            colors[index].g = palette[index * 3 + 1] << 2;
            colors[index].b = palette[index * 3 + 2] << 2;
            colors[index].a = 255;
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);
        SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
    } else {
        for (int index = 0; index < 256; index++) {
            unsigned short r = palette[index * 3] << 2;
            unsigned short g = palette[index * 3 + 1] << 2;
            unsigned short b = palette[index * 3 + 2] << 2;

            r = gRedShift > 0 ? (r << gRedShift) : (r >> -gRedShift);
            r &= gRedMask;

            g = gGreenShift > 0 ? (g << gGreenShift) : (g >> -gGreenShift);
            g &= gGreenMask;

            b = gBlueShift > 0 ? (b << gBlueShift) : (b >> -gBlueShift);
            b &= gBlueMask;

            unsigned short rgb = r | g | b;
            gSixteenBppPalette[index] = rgb;
        }

        windowRefreshAll(&_scr_size);
    }

    if (_update_palette_func != NULL) {
        _update_palette_func();
    }
}

// 0x4CB68C
unsigned char* directDrawGetPalette()
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
        SDL_Color* colors = gSdlSurface->format->palette->colors;

        for (int index = 0; index < 256; index++) {
            SDL_Color* color = &(colors[index]);
            gLastVideoModePalette[index * 3] = color->r >> 2;
            gLastVideoModePalette[index * 3 + 1] = color->g >> 2;
            gLastVideoModePalette[index * 3 + 2] = color->b >> 2;
        }

        return gLastVideoModePalette;
    }

    int redShift = gRedShift + 2;
    int greenShift = gGreenShift + 2;
    int blueShift = gBlueShift + 2;

    for (int index = 0; index < 256; index++) {
        unsigned short rgb = gSixteenBppPalette[index];

        unsigned short r = redShift > 0 ? ((rgb & gRedMask) >> redShift) : ((rgb & gRedMask) << -redShift);
        unsigned short g = greenShift > 0 ? ((rgb & gGreenMask) >> greenShift) : ((rgb & gGreenMask) << -greenShift);
        unsigned short b = blueShift > 0 ? ((rgb & gBlueMask) >> blueShift) : ((rgb & gBlueMask) << -blueShift);

        gLastVideoModePalette[index * 3] = (r >> 2) & 0xFF;
        gLastVideoModePalette[index * 3 + 1] = (g >> 2) & 0xFF;
        gLastVideoModePalette[index * 3 + 2] = (b >> 2) & 0xFF;
    }

    return gLastVideoModePalette;
}

// 0x4CB850
void _GNW95_ShowRect(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    blitBufferToBuffer(src + srcPitch * srcY + srcX, srcWidth, srcHeight, srcPitch, (unsigned char*)gSdlSurface->pixels + gSdlSurface->pitch * destY + destX, gSdlSurface->pitch);

    SDL_Rect srcRect;
    srcRect.x = destX;
    srcRect.y = destY;
    srcRect.w = srcWidth;
    srcRect.h = srcHeight;

    SDL_Rect destRect;
    destRect.x = destX;
    destRect.y = destY;
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlTextureSurface, &destRect);
}

// 0x4CB93C
void _GNW95_MouseShowRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    if (!gProgramIsActive) {
        return;
    }

    unsigned char* dest = (unsigned char*)gSdlSurface->pixels + gSdlSurface->pitch * destY + 2 * destX;

    src += srcPitch * srcY + srcX;

    for (int y = 0; y < srcHeight; y++) {
        unsigned short* destPtr = (unsigned short*)dest;
        unsigned char* srcPtr = src;
        for (int x = 0; x < srcWidth; x++) {
            *destPtr = gSixteenBppPalette[*srcPtr];
            destPtr++;
            srcPtr++;
        }

        dest += gSdlSurface->pitch;
        src += srcPitch;
    }

    SDL_Rect srcRect;
    srcRect.x = destX;
    srcRect.y = destY;
    srcRect.w = srcWidth;
    srcRect.h = srcHeight;

    SDL_Rect destRect;
    destRect.x = destX;
    destRect.y = destY;
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlTextureSurface, &destRect);
}

// 0x4CBA44
void _GNW95_ShowRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    _GNW95_MouseShowRect16(src, srcPitch, a3, srcX, srcY, srcWidth, srcHeight, destX, destY);
}

// 0x4CBAB0
void _GNW95_MouseShowTransRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY, unsigned char keyColor)
{
    if (!gProgramIsActive) {
        return;
    }

    unsigned char* dest = (unsigned char*)gSdlSurface->pixels + gSdlSurface->pitch * destY + 2 * destX;

    src += srcPitch * srcY + srcX;

    for (int y = 0; y < srcHeight; y++) {
        unsigned short* destPtr = (unsigned short*)dest;
        unsigned char* srcPtr = src;
        for (int x = 0; x < srcWidth; x++) {
            if (*srcPtr != keyColor) {
                *destPtr = gSixteenBppPalette[*srcPtr];
            }
            destPtr++;
            srcPtr++;
        }

        dest += gSdlSurface->pitch;
        src += srcPitch;
    }

    SDL_Rect srcRect;
    srcRect.x = destX;
    srcRect.y = destY;
    srcRect.w = srcWidth;
    srcRect.h = srcHeight;

    SDL_Rect destRect;
    destRect.x = destX;
    destRect.y = destY;
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlTextureSurface, &destRect);
}

// Clears drawing surface.
//
// 0x4CBBC8
void _GNW95_zero_vid_mem()
{
    if (!gProgramIsActive) {
        return;
    }

    SDL_LockSurface(gSdlSurface);

    unsigned char* surface = (unsigned char*)gSdlSurface->pixels;
    for (int y = 0; y < gSdlSurface->h; y++) {
        memset(surface, 0, gSdlSurface->w);
        surface += gSdlSurface->pitch;
    }

    SDL_UnlockSurface(gSdlSurface);

    SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
}

int screenGetWidth()
{
    // TODO: Make it on par with _xres;
    return rectGetWidth(&_scr_size);
}

int screenGetHeight()
{
    // TODO: Make it on par with _yres.
    return rectGetHeight(&_scr_size);
}

int screenGetVisibleHeight()
{
    int windowBottomMargin = 0;

    if (!gInterfaceBarMode) {
        windowBottomMargin = INTERFACE_BAR_HEIGHT;
    }
    return screenGetHeight() - windowBottomMargin;
}

static bool createRenderer(int width, int height)
{
    gSdlRenderer = SDL_CreateRenderer(gSdlWindow, -1, 0);
    if (gSdlRenderer == NULL) {
        return false;
    }

    if (SDL_RenderSetLogicalSize(gSdlRenderer, width, height) != 0) {
        return false;
    }

    gSdlTexture = SDL_CreateTexture(gSdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (gSdlTexture == NULL) {
        return false;
    }

    Uint32 format;
    if (SDL_QueryTexture(gSdlTexture, &format, NULL, NULL, NULL) != 0) {
        return false;
    }

    gSdlTextureSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height, SDL_BITSPERPIXEL(format), format);
    if (gSdlTextureSurface == NULL) {
        return false;
    }

    return true;
}

static void destroyRenderer()
{
    if (gSdlTextureSurface != NULL) {
        SDL_FreeSurface(gSdlTextureSurface);
        gSdlTextureSurface = NULL;
    }

    if (gSdlTexture != NULL) {
        SDL_DestroyTexture(gSdlTexture);
        gSdlTexture = NULL;
    }

    if (gSdlRenderer != NULL) {
        SDL_DestroyRenderer(gSdlRenderer);
        gSdlRenderer = NULL;
    }
}

void handleWindowSizeChanged()
{
    destroyRenderer();
    createRenderer(screenGetWidth(), screenGetHeight());
}

void renderPresent()
{
    SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
    SDL_RenderPresent(gSdlRenderer);
}

} // namespace fallout
