#include "light.h"

#include "map_defs.h"
#include "perk.h"
#include "tile.h"
#include "object.h"

#include <math.h>

// 20% of max light per "Night Vision" rank
#define LIGHT_LEVEL_NIGHT_VISION_BONUS (65536 / 5)

// 0x51923C
static int gLightLevel = LIGHT_LEVEL_MAX;

// light intensity per elevation per tile
// 0x59E994
static int gLightIntensity[ELEVATION_COUNT][HEX_GRID_SIZE];

// 0x47A8F0
int lightInit()
{
    lightResetIntensity();
    return 0;
}

// 0x47A8F8
int lightGetLightLevel()
{
    return gLightLevel;
}

// 0x47A908
void lightSetLightLevel(int lightLevel, bool shouldUpdateScreen)
{
    int normalizedLightLevel = lightLevel + perkGetRank(gDude, PERK_NIGHT_VISION) * LIGHT_LEVEL_NIGHT_VISION_BONUS;

    if (normalizedLightLevel < LIGHT_LEVEL_MIN) {
        normalizedLightLevel = LIGHT_LEVEL_MIN;
    }

    if (normalizedLightLevel > LIGHT_LEVEL_MAX) {
        normalizedLightLevel = LIGHT_LEVEL_MAX;
    }

    int oldLightLevel = gLightLevel;
    gLightLevel = normalizedLightLevel;

    if (shouldUpdateScreen) {
        if (oldLightLevel != normalizedLightLevel) {
            tileWindowRefresh();
        }
    }
}

// TODO: Looks strange - it tries to clamp intensity as light level?
int _light_get_tile(int elevation, int tile)
{
    if (!elevationIsValid(elevation)) {
        return 0;
    }

    if (!hexGridTileIsValid(tile)) {
        return 0;
    }

    int result = gLightIntensity[elevation][tile];

    if (result >= 0x10000) {
        result = 0x10000;
    }

    return result;
}

// 0x47A9C4
int lightGetIntensity(int elevation, int tile)
{
    if (!elevationIsValid(elevation)) {
        return 0;
    }

    if (!hexGridTileIsValid(tile)) {
        return 0;
    }

    return gLightIntensity[elevation][tile];
}

// 0x47A9EC
void lightSetIntensity(int elevation, int tile, int lightIntensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }

    gLightIntensity[elevation][tile] = lightIntensity;
}

// 0x47AA10
void lightIncreaseIntensity(int elevation, int tile, int lightIntensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }
    
    gLightIntensity[elevation][tile] += lightIntensity;
}

// 0x47AA48
void lightDecreaseIntensity(int elevation, int tile, int lightIntensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }
    
    gLightIntensity[elevation][tile] -= lightIntensity;
}

// 0x47AA84
void lightResetIntensity()
{
    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
            gLightIntensity[elevation][tile] = 655;
        }
    }
}
