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
#define SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_X_KEY "MainMenuCreditsOffsetX"
#define SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_Y_KEY "MainMenuCreditsOffsetY"
#define SFALL_CONFIG_MAIN_MENU_FONT_COLOR_KEY "MainMenuFontColour"
#define SFALL_CONFIG_MAIN_MENU_OFFSET_X_KEY "MainMenuOffsetX"
#define SFALL_CONFIG_MAIN_MENU_OFFSET_Y_KEY "MainMenuOffsetY"
#define SFALL_CONFIG_SKIP_OPENING_MOVIES_KEY "SkipOpeningMovies"
#define SFALL_CONFIG_STARTING_MAP_KEY "StartingMap"
#define SFALL_CONFIG_KARMA_FRMS_KEY "KarmaFRMs"
#define SFALL_CONFIG_KARMA_POINTS_KEY "KarmaPoints"
#define SFALL_CONFIG_DISPLAY_KARMA_CHANGES_KEY "DisplayKarmaChanges"
#define SFALL_CONFIG_OVERRIDE_CRITICALS_MODE_KEY "OverrideCriticalTable"
#define SFALL_CONFIG_OVERRIDE_CRITICALS_FILE_KEY "OverrideCriticalFile"
#define SFALL_CONFIG_REMOVE_CRITICALS_TIME_LIMITS_KEY "RemoveCriticalTimelimits"
#define SFALL_CONFIG_BOOKS_FILE_KEY "BooksFile"
#define SFALL_CONFIG_ELEVATORS_FILE_KEY "ElevatorsFile"
#define SFALL_CONFIG_CONSOLE_OUTPUT_FILE_KEY "ConsoleOutputPath"

extern bool gSfallConfigInitialized;
extern Config gSfallConfig;

bool sfallConfigInit(int argc, char** argv);
void sfallConfigExit();

#endif /* SFALL_CONFIG_H */
