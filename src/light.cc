#include "light.h"

#include <algorithm>

#include "map_defs.h"
#include "object.h"
#include "perk.h"
#include "tile.h"

namespace fallout {

// 20% of max light per "Night Vision" rank
#define LIGHT_LEVEL_NIGHT_VISION_BONUS (65536 / 5)

// 0x51923C
static int gAmbientIntensity = LIGHT_INTENSITY_MAX;

// light intensity per elevation per tile
// 0x59E994
static int gTileIntensity[ELEVATION_COUNT][HEX_GRID_SIZE];

// 0x47A8F0
int lightInit()
{
    lightResetTileIntensity();
    return 0;
}

// 0x47A8F0
void lightReset()
{
    lightResetTileIntensity();
}

// 0x47A8F0
void lightExit()
{
    lightResetTileIntensity();
}

// 0x47A8F8
int lightGetAmbientIntensity()
{
    return gAmbientIntensity;
}

// 0x47A908
void lightSetAmbientIntensity(int intensity, bool shouldUpdateScreen)
{
    int adjustedIntensity = intensity + perkGetRank(gDude, PERK_NIGHT_VISION) * LIGHT_LEVEL_NIGHT_VISION_BONUS;
    int normalizedIntensity = std::clamp(adjustedIntensity, LIGHT_INTENSITY_MIN, LIGHT_INTENSITY_MAX);

    int oldAmbientIntensity = gAmbientIntensity;
    gAmbientIntensity = normalizedIntensity;

    if (shouldUpdateScreen) {
        if (oldAmbientIntensity != normalizedIntensity) {
            tileWindowRefresh();
        }
    }
}

// 0x47A980
int lightGetTileIntensity(int elevation, int tile)
{
    if (!elevationIsValid(elevation)) {
        return 0;
    }

    if (!hexGridTileIsValid(tile)) {
        return 0;
    }

    return std::min(gTileIntensity[elevation][tile], LIGHT_INTENSITY_MAX);
}

// 0x47A9C4
int lightGetTrueTileIntensity(int elevation, int tile)
{
    if (!elevationIsValid(elevation)) {
        return 0;
    }

    if (!hexGridTileIsValid(tile)) {
        return 0;
    }

    return gTileIntensity[elevation][tile];
}

// 0x47A9EC
void lightSetTileIntensity(int elevation, int tile, int intensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }

    gTileIntensity[elevation][tile] = intensity;
}

// 0x47AA10
void lightIncreaseTileIntensity(int elevation, int tile, int intensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }

    gTileIntensity[elevation][tile] += intensity;
}

// 0x47AA48
void lightDecreaseTileIntensity(int elevation, int tile, int intensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }

    gTileIntensity[elevation][tile] -= intensity;
}

// 0x47AA84
void lightResetTileIntensity()
{
    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
            gTileIntensity[elevation][tile] = 655;
        }
    }
}

} // namespace fallout
