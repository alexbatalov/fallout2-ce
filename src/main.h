#ifndef MAIN_H
#define MAIN_H

#include "art.h"
#include "fps_limiter.h"

typedef enum MainMenuButton {
    MAIN_MENU_BUTTON_INTRO,
    MAIN_MENU_BUTTON_NEW_GAME,
    MAIN_MENU_BUTTON_LOAD_GAME,
    MAIN_MENU_BUTTON_OPTIONS,
    MAIN_MENU_BUTTON_CREDITS,
    MAIN_MENU_BUTTON_EXIT,
    MAIN_MENU_BUTTON_COUNT,
} MainMenuButton;

typedef enum MainMenuOption {
    MAIN_MENU_INTRO,
    MAIN_MENU_NEW_GAME,
    MAIN_MENU_LOAD_GAME,
    MAIN_MENU_3,
    MAIN_MENU_TIMEOUT,
    MAIN_MENU_CREDITS,
    MAIN_MENU_QUOTES,
    MAIN_MENU_EXIT,
    MAIN_MENU_SELFRUN,
    MAIN_MENU_OPTIONS,
} MainMenuOption;

extern char _mainMap[];
extern int _main_game_paused;
extern char** _main_selfrun_list;
extern int _main_selfrun_count;
extern int _main_selfrun_index;
extern bool _main_show_death_scene;
extern int mainMenuWindowHandle;
extern unsigned char* mainMenuWindowBuf;
extern unsigned char* gMainMenuBackgroundFrmData;
extern unsigned char* gMainMenuButtonUpFrmData;
extern unsigned char* gMainMenuButtonDownFrmData;
extern bool _in_main_menu;
extern bool gMainMenuWindowInitialized;
extern unsigned int gMainMenuScreensaverDelay;
extern const int gMainMenuButtonKeyBindings[MAIN_MENU_BUTTON_COUNT];
extern const int _return_values[MAIN_MENU_BUTTON_COUNT];

extern bool _main_death_voiceover_done;
extern int gMainMenuButtons[MAIN_MENU_BUTTON_COUNT];
extern bool gMainMenuWindowHidden;
extern CacheEntry* gMainMenuButtonUpFrmHandle;
extern CacheEntry* gMainMenuButtonDownFrmHandle;
extern CacheEntry* gMainMenuBackgroundFrmHandle;

int falloutMain(int argc, char** argv);
bool falloutInit(int argc, char** argv);
int _main_load_new(char* fname);
void mainLoop(FpsLimiter& fpsLimiter);
void _main_selfrun_exit();
void showDeath();
void _main_death_voiceover_callback();
int _mainDeathGrabTextFile(const char* fileName, char* dest);
int _mainDeathWordWrap(char* text, int width, short* beginnings, short* count);
int mainMenuWindowInit();
void mainMenuWindowFree();
void mainMenuWindowHide(bool animate);
void mainMenuWindowUnhide(bool animate);
int _main_menu_is_enabled();
int mainMenuWindowHandleEvents(FpsLimiter& fpsLimiter);

#endif /* MAIN_H */
