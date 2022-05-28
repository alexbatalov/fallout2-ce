#ifndef LIGHT_H
#define LIGHT_H

#include "map_defs.h"

#define LIGHT_LEVEL_MIN (65536 / 4)
#define LIGHT_LEVEL_MAX 65536

// 20% of max light per "Night Vision" rank
#define LIGHT_LEVEL_NIGHT_VISION_BONUS (65536 / 5)

typedef void AdjustLightIntensityProc(int elevation, int tile, int intensity);

extern int gLightLevel;
extern int gLightIntensity[ELEVATION_COUNT][HEX_GRID_SIZE];

int lightInit();
int lightGetLightLevel();
void lightSetLightLevel(int lightLevel, bool shouldUpdateScreen);
int _light_get_tile(int elevation, int tile);
int lightGetIntensity(int elevation, int tile);
void lightSetIntensity(int elevation, int tile, int intensity);
void lightIncreaseIntensity(int elevation, int tile, int intensity);
void lightDecreaseIntensity(int elevation, int tile, int intensity);
void lightResetIntensity();

#endif /* LIGHT_H */
