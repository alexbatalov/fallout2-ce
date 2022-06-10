#ifndef SFALL_CONFIG_H
#define SFALL_CONFIG_H

#include "config.h"

#define SFALL_CONFIG_FILE_NAME "ddraw.ini"

#define SFALL_CONFIG_MISC_KEY "Misc"

#define SFALL_CONFIG_DUDE_NATIVE_LOOK_JUMPSUIT_MALE_KEY "MaleDefaultModel"
#define SFALL_CONFIG_DUDE_NATIVE_LOOK_JUMPSUIT_FEMALE_KEY "FemaleDefaultModel"
#define SFALL_CONFIG_DUDE_NATIVE_LOOK_TRIBAL_MALE_KEY "MaleStartModel"
#define SFALL_CONFIG_DUDE_NATIVE_LOOK_TRIBAL_FEMALE_KEY "FemaleStartModel"
#define SFALL_CONFIG_MAIN_MENU_BIG_FONT_COLOR_KEY "MainMenuBigFontColour"
#define SFALL_CONFIG_MAIN_MENU_FONT_COLOR_KEY "MainMenuFontColour"
#define SFALL_CONFIG_SKIP_OPENING_MOVIES_KEY "SkipOpeningMovies"
#define SFALL_CONFIG_STARTING_MAP_KEY "StartingMap"
#define SFALL_CONFIG_KARMA_FRMS_KEY "KarmaFRMs"
#define SFALL_CONFIG_KARMA_POINTS_KEY "KarmaPoints"
#define SFALL_CONFIG_DISPLAY_KARMA_CHANGES_KEY "DisplayKarmaChanges"

extern bool gSfallConfigInitialized;
extern Config gSfallConfig;

bool sfallConfigInit(int argc, char** argv);
void sfallConfigExit();

#endif /* SFALL_CONFIG_H */
