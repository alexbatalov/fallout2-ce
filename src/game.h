#ifndef GAME_H
#define GAME_H

#include "game_vars.h"
#include "message.h"
#include "touch.h"

namespace fallout {

typedef enum GameState {
    GAME_STATE_0,
    GAME_STATE_1,
    GAME_STATE_2,
    GAME_STATE_3,
    GAME_STATE_4,
    GAME_STATE_5,
} GameState;

extern int* gGameGlobalVars;
extern int gGameGlobalVarsLength;
extern const char* asc_5186C8;
extern int _game_user_wants_to_quit;

extern MessageList gMiscMessageList;

int gameInitWithOptions(const char* windowTitle, bool isMapper, int font, int flags, int argc, char** argv);
void gameReset();
void gameExit();
int gameHandleKey(int eventCode, bool isInCombatMode);
void gameUiDisable(int allowScrolling);
void gameUiEnable();
bool gameUiIsDisabled();
int gameGetGlobalVar(int var);
int gameSetGlobalVar(int var, int value);
int globalVarsRead(const char* path, const char* section, int* variablesListLengthPtr, int** variablesListPtr);
int gameGetState();
int gameRequestState(int newGameState);
void gameUpdateState();
int showQuitConfirmationDialog();

int gameShowDeathDialog(const char* message);
void* gameGetGlobalPointer(int var);
int gameSetGlobalPointer(int var, void* value);

class GameMode {
public:
    enum Flags {
        kWorldmap = 0x1,
        kDialog = 0x4,
        kOptions = 0x8,
        kSaveGame = 0x10,
        kLoadGame = 0x20,
        kCombat = 0x40,
        kPreferences = 0x80,
        kHelp = 0x100,
        kEditor = 0x200,
        kPipboy = 0x400,
        kPlayerTurn = 0x800,
        kInventory = 0x1000,
        kAutomap = 0x2000,
        kSkilldex = 0x4000,
        kUseOn = 0x8000,
        kLoot = 0x10000,
        kBarter = 0x20000,
        kHero = 0x40000,
        kDialogReview = 0x80000,
        kCounter = 0x100000,
        kSpecial = 0x80000000,
    };

    static void enterGameMode(int gameMode);
    static void exitGameMode(int gameMode);
    static bool isInGameMode(int gameMode);
    static int getCurrentGameMode() { return currentGameMode; }

private:
    static int currentGameMode;
};

class ScopedGameMode {
public:
    ScopedGameMode(int gameMode);
    ~ScopedGameMode();

private:
    int gameMode;
};

} // namespace fallout

#endif /* GAME_H */
