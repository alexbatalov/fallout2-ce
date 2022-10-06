#include "cycle.h"

#include "color.h"
#include "input.h"
#include "palette.h"
#include "settings.h"
#include "svga.h"

namespace fallout {

#define COLOR_CYCLE_PERIOD_1 (200U)
#define COLOR_CYCLE_PERIOD_2 (142U)
#define COLOR_CYCLE_PERIOD_3 (100U)
#define COLOR_CYCLE_PERIOD_4 (33U)

// 0x51843C
static int gColorCycleSpeedFactor = 1;

// TODO: Convert colors to RGB.
// clang-format off

// Green.
//
// 0x518440
static unsigned char _slime[12] = {
    0, 108, 0,
    11, 115, 7,
    27, 123, 15,
    43, 131, 27,
};

// Light gray?
//
// 0x51844C
static unsigned char _shoreline[18] = {
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
static unsigned char _fire_slow[15] = {
    255, 0, 0,
    215, 0, 0,
    147, 43, 11,
    255, 119, 0,
    255, 59, 0,
};

// Red.
//
// 0x51846D
static unsigned char _fire_fast[15] = {
    71, 0, 0,
    123, 0, 0,
    179, 0, 0,
    123, 0, 0,
    71, 0, 0,
};

// Light blue.
//
// 0x51847C
static unsigned char _monitors[15] = {
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

// 0x518494
static int _slime_start = 0;

// 0x518498
static int _shoreline_start = 0;

// 0x51849C
static int _fire_slow_start = 0;

// 0x5184A0
static int _fire_fast_start = 0;

// 0x5184A4
static int _monitors_start = 0;

// 0x5184A8
static unsigned char _bobber_red = 0;

// 0x5184A9
static signed char _bobber_diff = -4;

// 0x56D7D0
static unsigned int gColorCycleTimestamp3;

// 0x56D7D4
static unsigned int gColorCycleTimestamp1;

// 0x56D7D8
static unsigned int gColorCycleTimestamp2;

// 0x56D7DC
static unsigned int gColorCycleTimestamp4;

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
        _slime[index] >>= 2;
    }

    for (int index = 0; index < 18; index++) {
        _shoreline[index] >>= 2;
    }

    for (int index = 0; index < 15; index++) {
        _fire_slow[index] >>= 2;
    }

    for (int index = 0; index < 15; index++) {
        _fire_fast[index] >>= 2;
    }

    for (int index = 0; index < 15; index++) {
        _monitors[index] >>= 2;
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
        gColorCycleTimestamp1 = 0;
        gColorCycleTimestamp2 = 0;
        gColorCycleTimestamp3 = 0;
        gColorCycleTimestamp4 = 0;
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
    if (!gColorCycleEnabled) {
        return;
    }

    bool changed = false;

    unsigned char* palette = _getSystemPalette();
    unsigned int time = getTicks();

    if (getTicksBetween(time, gColorCycleTimestamp1) >= COLOR_CYCLE_PERIOD_1 * gColorCycleSpeedFactor) {
        changed = true;
        gColorCycleTimestamp1 = time;

        int paletteIndex = 229 * 3;

        for (int index = _slime_start; index < 12; index++) {
            palette[paletteIndex++] = _slime[index];
        }

        for (int index = 0; index < _slime_start; index++) {
            palette[paletteIndex++] = _slime[index];
        }

        _slime_start -= 3;
        if (_slime_start < 0) {
            _slime_start = 9;
        }

        paletteIndex = 248 * 3;

        for (int index = _shoreline_start; index < 18; index++) {
            palette[paletteIndex++] = _shoreline[index];
        }

        for (int index = 0; index < _shoreline_start; index++) {
            palette[paletteIndex++] = _shoreline[index];
        }

        _shoreline_start -= 3;
        if (_shoreline_start < 0) {
            _shoreline_start = 15;
        }

        paletteIndex = 238 * 3;

        for (int index = _fire_slow_start; index < 15; index++) {
            palette[paletteIndex++] = _fire_slow[index];
        }

        for (int index = 0; index < _fire_slow_start; index++) {
            palette[paletteIndex++] = _fire_slow[index];
        }

        _fire_slow_start -= 3;
        if (_fire_slow_start < 0) {
            _fire_slow_start = 12;
        }
    }

    if (getTicksBetween(time, gColorCycleTimestamp2) >= COLOR_CYCLE_PERIOD_2 * gColorCycleSpeedFactor) {
        changed = true;
        gColorCycleTimestamp2 = time;

        int paletteIndex = 243 * 3;

        for (int index = _fire_fast_start; index < 15; index++) {
            palette[paletteIndex++] = _fire_fast[index];
        }

        for (int index = 0; index < _fire_fast_start; index++) {
            palette[paletteIndex++] = _fire_fast[index];
        }

        _fire_fast_start -= 3;
        if (_fire_fast_start < 0) {
            _fire_fast_start = 12;
        }
    }

    if (getTicksBetween(time, gColorCycleTimestamp3) >= COLOR_CYCLE_PERIOD_3 * gColorCycleSpeedFactor) {
        changed = true;
        gColorCycleTimestamp3 = time;

        int paletteIndex = 233 * 3;

        for (int index = _monitors_start; index < 15; index++) {
            palette[paletteIndex++] = _monitors[index];
        }

        for (int index = 0; index < _monitors_start; index++) {
            palette[paletteIndex++] = _monitors[index];
        }

        _monitors_start -= 3;

        if (_monitors_start < 0) {
            _monitors_start = 12;
        }
    }

    if (getTicksBetween(time, gColorCycleTimestamp4) >= COLOR_CYCLE_PERIOD_4 * gColorCycleSpeedFactor) {
        changed = true;
        gColorCycleTimestamp4 = time;

        if (_bobber_red == 0 || _bobber_red == 60) {
            _bobber_diff = -_bobber_diff;
        }

        _bobber_red += _bobber_diff;

        int paletteIndex = 254 * 3;
        palette[paletteIndex++] = _bobber_red;
        palette[paletteIndex++] = 0;
        palette[paletteIndex++] = 0;
    }

    if (changed) {
        paletteSetEntriesInRange(palette + 229 * 3, 229, 255);
    }
}

} // namespace fallout
