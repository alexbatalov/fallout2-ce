#include "sfall_config.h"

#include "platform_compat.h"

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
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DUDE_NATIVE_LOOK_JUMPSUIT_MALE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DUDE_NATIVE_LOOK_JUMPSUIT_FEMALE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DUDE_NATIVE_LOOK_TRIBAL_MALE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DUDE_NATIVE_LOOK_TRIBAL_FEMALE_KEY, "");
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_BIG_FONT_COLOR_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_X_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_Y_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_FONT_COLOR_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_X_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_Y_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_SKIP_OPENING_MOVIES_KEY, 0);
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_STARTING_MAP_KEY, "");
    configSetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DISPLAY_KARMA_CHANGES_KEY, false);

    char path[COMPAT_MAX_PATH];
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
