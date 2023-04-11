#include "cycle.h"

#include "color.h"
#include "input.h"
#include "palette.h"
#include "settings.h"
#include "svga.h"

namespace fallout {

static constexpr unsigned int kSlowCyclePeriod = 1000 / 5;
static constexpr unsigned int kMediumCyclePeriod = 1000 / 7;
static constexpr unsigned int kFastCyclePeriod = 1000 / 10;
static constexpr unsigned int kVeryFastCyclePeriod = 1000 / 30;

// 0x51843C
static int gColorCycleSpeedFactor = 1;

// TODO: Convert colors to RGB.
// clang-format off

// Green.
//
// 0x518440
static unsigned char slime[12] = {
    0, 108, 0,
    11, 115, 7,
    27, 123, 15,
    43, 131, 27,
};

// Light gray?
//
// 0x51844C
static unsigned char shoreline[18] = {
    83, 63, 43,
    75, 59, 43,
    67, 55, 39,
    63, 51, 39,
    55, 47, 35,
    51, 43, 35,
};

// Orange.
//
// 0x51845E
static unsigned char fire_slow[15] = {
    255, 0, 0,
    215, 0, 0,
    147, 43, 11,
    255, 119, 0,
    255, 59, 0,
};

// Red.
//
// 0x51846D
static unsigned char fire_fast[15] = {
    71, 0, 0,
    123, 0, 0,
    179, 0, 0,
    123, 0, 0,
    71, 0, 0,
};

// Light blue.
//
// 0x51847C
static unsigned char monitors[15] = {
    107, 107, 111,
    99, 103, 127,
    87, 107, 143,
    0, 147, 163,
    107, 187, 255,
};

// clang-format on

// 0x51848C
static bool gColorCycleInitialized = false;

// 0x518490
static bool gColorCycleEnabled = false;

// 0x56D7D0
static unsigned int last_cycle_fast;

// 0x56D7D4
static unsigned int last_cycle_slow;

// 0x56D7D8
static unsigned int last_cycle_medium;

// 0x56D7DC
static unsigned int last_cycle_very_fast;

// 0x42E780
void colorCycleInit()
{
    if (gColorCycleInitialized) {
        return;
    }

    if (!settings.system.color_cycling) {
        return;
    }

    for (int index = 0; index < 12; index++) {
        slime[index] >>= 2;
    }

    for (int index = 0; index < 18; index++) {
        shoreline[index] >>= 2;
    }

    for (int index = 0; index < 15; index++) {
        fire_slow[index] >>= 2;
    }

    for (int index = 0; index < 15; index++) {
        fire_fast[index] >>= 2;
    }

    for (int index = 0; index < 15; index++) {
        monitors[index] >>= 2;
    }

    tickersAdd(colorCycleTicker);

    gColorCycleInitialized = true;
    gColorCycleEnabled = true;

    cycleSetSpeedFactor(settings.system.cycle_speed_factor);
}

// 0x42E8CC
void colorCycleReset()
{
    if (gColorCycleInitialized) {
        last_cycle_slow = 0;
        last_cycle_medium = 0;
        last_cycle_fast = 0;
        last_cycle_very_fast = 0;
        tickersAdd(colorCycleTicker);
        gColorCycleEnabled = true;
    }
}

// 0x42E90C
void colorCycleFree()
{
    if (gColorCycleInitialized) {
        tickersRemove(colorCycleTicker);
        gColorCycleInitialized = false;
        gColorCycleEnabled = false;
    }
}

// 0x42E930
void colorCycleDisable()
{
    gColorCycleEnabled = false;
}

// 0x42E93C
void colorCycleEnable()
{
    gColorCycleEnabled = true;
}

// 0x42E948
bool colorCycleEnabled()
{
    return gColorCycleEnabled;
}

// 0x42E950
void cycleSetSpeedFactor(int value)
{
    gColorCycleSpeedFactor = value;
    settings.system.cycle_speed_factor = value;
}

// 0x42E97C
void colorCycleTicker()
{
    // 0x518494
    static int slime_start = 0;

    // 0x518498
    static int shoreline_start = 0;

    // 0x51849C
    static int fire_slow_start = 0;

    // 0x5184A0
    static int fire_fast_start = 0;

    // 0x5184A4
    static int monitors_start = 0;

    // 0x5184A8
    static unsigned char bobber_red = 0;

    // 0x5184A9
    static signed char bobber_diff = -4;

    if (!gColorCycleEnabled) {
        return;
    }

    bool changed = false;

    unsigned char* palette = _getSystemPalette();
    unsigned int time = getTicks();

    if (getTicksBetween(time, last_cycle_slow) >= kSlowCyclePeriod * gColorCycleSpeedFactor) {
        changed = true;
        last_cycle_slow = time;

        int paletteIndex = 229 * 3;

        for (int index = slime_start; index < 12; index++) {
            palette[paletteIndex++] = slime[index];
        }

        for (int index = 0; index < slime_start; index++) {
            palette[paletteIndex++] = slime[index];
        }

        slime_start -= 3;
        if (slime_start < 0) {
            slime_start = 9;
        }

        paletteIndex = 248 * 3;

        for (int index = shoreline_start; index < 18; index++) {
            palette[paletteIndex++] = shoreline[index];
        }

        for (int index = 0; index < shoreline_start; index++) {
            palette[paletteIndex++] = shoreline[index];
        }

        shoreline_start -= 3;
        if (shoreline_start < 0) {
            shoreline_start = 15;
        }

        paletteIndex = 238 * 3;

        for (int index = fire_slow_start; index < 15; index++) {
            palette[paletteIndex++] = fire_slow[index];
        }

        for (int index = 0; index < fire_slow_start; index++) {
            palette[paletteIndex++] = fire_slow[index];
        }

        fire_slow_start -= 3;
        if (fire_slow_start < 0) {
            fire_slow_start = 12;
        }
    }

    if (getTicksBetween(time, last_cycle_medium) >= kMediumCyclePeriod * gColorCycleSpeedFactor) {
        changed = true;
        last_cycle_medium = time;

        int paletteIndex = 243 * 3;

        for (int index = fire_fast_start; index < 15; index++) {
            palette[paletteIndex++] = fire_fast[index];
        }

        for (int index = 0; index < fire_fast_start; index++) {
            palette[paletteIndex++] = fire_fast[index];
        }

        fire_fast_start -= 3;
        if (fire_fast_start < 0) {
            fire_fast_start = 12;
        }
    }

    if (getTicksBetween(time, last_cycle_fast) >= kFastCyclePeriod * gColorCycleSpeedFactor) {
        changed = true;
        last_cycle_fast = time;

        int paletteIndex = 233 * 3;

        for (int index = monitors_start; index < 15; index++) {
            palette[paletteIndex++] = monitors[index];
        }

        for (int index = 0; index < monitors_start; index++) {
            palette[paletteIndex++] = monitors[index];
        }

        monitors_start -= 3;

        if (monitors_start < 0) {
            monitors_start = 12;
        }
    }

    if (getTicksBetween(time, last_cycle_very_fast) >= kVeryFastCyclePeriod * gColorCycleSpeedFactor) {
        changed = true;
        last_cycle_very_fast = time;

        if (bobber_red == 0 || bobber_red == 60) {
            bobber_diff = -bobber_diff;
        }

        bobber_red += bobber_diff;

        int paletteIndex = 254 * 3;
        palette[paletteIndex++] = bobber_red;
        palette[paletteIndex++] = 0;
        palette[paletteIndex++] = 0;
    }

    if (changed) {
        paletteSetEntriesInRange(palette + 229 * 3, 229, 255);
    }
}

} // namespace fallout
