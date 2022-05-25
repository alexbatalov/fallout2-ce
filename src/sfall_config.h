#ifndef SFALL_CONFIG_H
#define SFALL_CONFIG_H

#include "config.h"

#include <stdbool.h>

#define SFALL_CONFIG_FILE_NAME "ddraw.ini"

#define SFALL_CONFIG_MISC_KEY "Misc"

#define SFALL_CONFIG_STARTING_MAP_KEY "StartingMap"

extern bool gSfallConfigInitialized;
extern Config gSfallConfig;

bool sfallConfigInit(int argc, char** argv);
void sfallConfigExit();

#endif /* SFALL_CONFIG_H */
