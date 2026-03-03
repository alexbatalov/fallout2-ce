#include "palette.h"
#ifdef NXDK
#include "xbox_debug.h"
#endif
#include <string.h>

#include "color.h"
#include "cycle.h"
#include "debug.h"
#include "game_sound.h"
#include "input.h"
#include "svga.h"

namespace fallout {

static void _palette_reset_();

// 0x6639D0
static unsigned char gPalette[256 * 3];

// 0x663CD0
unsigned char gPaletteWhite[256 * 3];

// 0x663FD0
unsigned char gPaletteBlack[256 * 3];

// 0x6642D0
static int gPaletteFadeSteps;

// 0x493A00
void paletteInit()
{
#ifdef NXDK

    if (gPaletteBlack) {
        memset(gPaletteBlack, 0, 256 * 3);
        DbgPrint("[paletteInit] gPaletteBlack cleared\n");
    } else {
        DbgPrint("[paletteInit] ERROR: gPaletteBlack is NULL\n");
    }

    if (gPaletteWhite) {
        memset(gPaletteWhite, 63, 256 * 3);
        DbgPrint("[paletteInit] gPaletteWhite filled\n");
    } else {
        DbgPrint("[paletteInit] ERROR: gPaletteWhite is NULL\n");
    }

    if (gPalette && _cmap) {
        memcpy(gPalette, _cmap, 256 * 3);
        DbgPrint("[paletteInit] gPalette memcpy from _cmap successful\n");
    } else {
        if (!gPalette) DbgPrint("[paletteInit] ERROR: gPalette is NULL\n");
        if (!_cmap)     DbgPrint("[paletteInit] ERROR: _cmap is NULL\n");

        // Optional fallback
        if (gPalette && !_cmap) {
            DbgPrint("[paletteInit] Fallback: generating grayscale palette\n");
            for (int i = 0; i < 256; i++) {
                gPalette[i * 3 + 0] = i / 4;
                gPalette[i * 3 + 1] = i / 4;
                gPalette[i * 3 + 2] = i / 4;
            }
        }
    }
#else
    memset(gPaletteBlack, 0, 256 * 3);
    memset(gPaletteWhite, 63, 256 * 3);
    memcpy(gPalette, _cmap, 256 * 3);
#endif

    unsigned int tick = getTicks();
#ifdef NXDK
    DbgPrint("[paletteInit] Tick start = %u\n", tick);
#endif

    if (backgroundSoundIsEnabled()) {
#ifdef NXDK
        DbgPrint("[paletteInit] backgroundSoundIsEnabled() = TRUE\n");
#endif
    }
    if (speechIsEnabled()) {
#ifdef NXDK
        DbgPrint("[paletteInit] speechIsEnabled() = TRUE\n");
#endif
    }

    if (backgroundSoundIsEnabled() || speechIsEnabled()) {
        colorPaletteSetTransitionCallback(soundContinueAll);
#ifdef NXDK
        DbgPrint("[paletteInit] soundContinueAll set as fade callback\n");
#endif
    }

#ifdef NXDK
    DbgPrint("[paletteInit] Calling colorPaletteFadeBetween (gPalette → gPalette, steps = 60)\n");
#endif
    colorPaletteFadeBetween(gPalette, gPalette, 60);
#ifdef NXDK
    DbgPrint("[paletteInit] Fade complete\n");
#endif

    colorPaletteSetTransitionCallback(nullptr);
#ifdef NXDK
    DbgPrint("[paletteInit] Fade callback cleared\n");
#endif

    unsigned int actualFadeDuration = getTicksSince(tick);
#ifdef NXDK
    DbgPrint("[paletteInit] Tick end = %u\n", getTicks());
    DbgPrint("[paletteInit] Elapsed = %u ms\n", actualFadeDuration);
#endif

    gPaletteFadeSteps = 60 * 700 / actualFadeDuration;
#ifdef NXDK
    DbgPrint("[paletteInit] gPaletteFadeSteps = %d\n", gPaletteFadeSteps);
#endif

    debugPrint("\nFade time is %u\nFade steps are %d\n", actualFadeDuration, gPaletteFadeSteps);

#ifdef NXDK
    DbgPrint("==== [paletteInit] END ====\n");
#endif
}


// NOTE: Collapsed.
//
// 0x493AD0
static void _palette_reset_()
{
}

// NOTE: Uncollapsed 0x493AD0.
void paletteReset()
{
    _palette_reset_();
}

// NOTE: Uncollapsed 0x493AD0.
void paletteExit()
{
    _palette_reset_();
}

// 0x493AD4
void paletteFadeTo(unsigned char* palette)
{
    bool colorCycleWasEnabled = colorCycleEnabled();
    colorCycleDisable();

    if (backgroundSoundIsEnabled() || speechIsEnabled()) {
        colorPaletteSetTransitionCallback(soundContinueAll);
    }

    colorPaletteFadeBetween(gPalette, palette, gPaletteFadeSteps);
    colorPaletteSetTransitionCallback(nullptr);

    memcpy(gPalette, palette, 768);

    if (colorCycleWasEnabled) {
        colorCycleEnable();
    }
}

// 0x493B48
void paletteSetEntries(unsigned char* palette)
{
    memcpy(gPalette, palette, sizeof(gPalette));
    _setSystemPalette(palette);
}

// 0x493B78
void paletteSetEntriesInRange(unsigned char* palette, int start, int end)
{
    memcpy(gPalette + 3 * start, palette, 3 * (end - start + 1));
    _setSystemPaletteEntries(palette, start, end);
}

} // namespace fallout
