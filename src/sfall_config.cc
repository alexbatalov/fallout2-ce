#include "sfall_config.h"

#include <stdio.h>
#include <string.h>

#define SFALL_CONFIG_FILE_NAME "ddraw.ini"

bool gSfallConfigInitialized = false;
Config gSfallConfig;

bool sfallConfigInit(int argc, char** argv)
{
    if (gSfallConfigInitialized) {
        return false;
    }

    if (!configInit(&gSfallConfig)) {
        return false;
    }

    // Initialize defaults.
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_STARTING_MAP_KEY, "");

    char path[FILENAME_MAX];
    char* executable = argv[0];
    char* ch = strrchr(executable, '\\');
    if (ch != NULL) {
        *ch = '\0';
        sprintf(path, "%s\\%s", executable, SFALL_CONFIG_FILE_NAME);
        *ch = '\\';
    } else {
        strcpy(path, SFALL_CONFIG_FILE_NAME);
    }

    configRead(&gSfallConfig, path, false);

    configParseCommandLineArguments(&gSfallConfig, argc, argv);

    gSfallConfigInitialized = true;

    return true;
}

void sfallConfigExit()
{
    if (gSfallConfigInitialized) {
        configFree(&gSfallConfig);
        gSfallConfigInitialized = false;
    }
}
