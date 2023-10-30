#include "palette.h"

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
    memset(gPaletteBlack, 0, 256 * 3);
    memset(gPaletteWhite, 63, 256 * 3);
    memcpy(gPalette, _cmap, 256 * 3);

    unsigned int tick = getTicks();
    if (backgroundSoundIsEnabled() || speechIsEnabled()) {
        colorPaletteSetTransitionCallback(soundContinueAll);
    }

    colorPaletteFadeBetween(gPalette, gPalette, 60);

    colorPaletteSetTransitionCallback(nullptr);

    // Actual fade duration will never be 0 since |colorPaletteFadeBetween| uses
    // frame rate throttling.
    unsigned int actualFadeDuration = getTicksSince(tick);

    // Calculate fade steps needed to perform fading in about 700 ms.
    gPaletteFadeSteps = 60 * 700 / actualFadeDuration;

    debugPrint("\nFade time is %u\nFade steps are %d\n", actualFadeDuration, gPaletteFadeSteps);
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
