#ifndef GAME_H
#define GAME_H

#include "game_vars.h"
#include "message.h"

extern char _aGame_0[];

extern bool gGameUiDisabled;
extern int _game_state_cur;
extern bool gIsMapper;
extern int* gGameGlobalVars;
extern int gGameGlobalVarsLength;
extern const char* asc_5186C8;
extern int _game_user_wants_to_quit;

extern MessageList gMiscMessageList;
extern int _master_db_handle;
extern int _critter_db_handle;

int gameInitWithOptions(const char* windowTitle, bool isMapper, int a3, int a4, int argc, char** argv);
void gameReset();
void gameExit();
int gameHandleKey(int eventCode, bool isInCombatMode);
void gameUiDisable(int a1);
void gameUiEnable();
bool gameUiIsDisabled();
int gameGetGlobalVar(int var);
int gameSetGlobalVar(int var, int value);
int gameLoadGlobalVars();
int globalVarsRead(const char* path, const char* section, int* out_vars_num, int** out_vars);
int _game_state();
int _game_state_request(int a1);
void _game_state_update();
int gameTakeScreenshot(int width, int height, unsigned char* buffer, unsigned char* palette);
void gameFreeGlobalVars();
void showHelp();
int showQuitConfirmationDialog();
int gameDbInit();
void showSplash();

#endif /* GAME_H */
