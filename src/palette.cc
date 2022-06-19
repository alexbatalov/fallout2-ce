#include "palette.h"

#include "color.h"
#include "core.h"
#include "cycle.h"
#include "debug.h"
#include "game_sound.h"

#include <string.h>

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
    memset(gPaletteBlack, 0, 256 * 3);
    memset(gPaletteWhite, 63, 256 * 3);
    memcpy(gPalette, _cmap, 256 * 3);

    unsigned int tick = _get_time();
    if (backgroundSoundIsEnabled() || speechIsEnabled()) {
        colorPaletteSetTransitionCallback(soundContinueAll);
    }

    colorPaletteFadeBetween(gPalette, gPalette, 60);

    colorPaletteSetTransitionCallback(NULL);

    unsigned int diff = getTicksSince(tick);

    // NOTE: Modern CPUs are super fast, so it's possible that less than 10ms
    // (the resolution of underlying GetTicks) is needed to fade between two
    // palettes, which leads to zero diff, which in turn leads to unpredictable
    // number of fade steps. To fix that the fallback value is used (46). This
    // value is commonly seen when running the game in 1 core VM.
    if (diff == 0) {
        diff = 46;
    }

    gPaletteFadeSteps = (int)(60.0 / (diff * (1.0 / 700.0)));

    debugPrint("\nFade time is %u\nFade steps are %d\n", diff, gPaletteFadeSteps);
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
    colorPaletteSetTransitionCallback(NULL);

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
