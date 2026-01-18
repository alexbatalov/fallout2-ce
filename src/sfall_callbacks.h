#ifndef SFALL_BEHAVIOURS_H
#define SFALL_BEHAVIOURS_H

namespace fallout {

void sfallOnBeforeGameInit();
void sfallOnGameInit();
void sfallOnAfterGameInit();
void sfallOnGameExit();
void sfallOnGameReset();
void sfallOnBeforeGameStart();
void sfallOnAfterGameStarted();
void sfallOnAfterNewGame();
void sfallOnGameModeChange();
void sfallOnBeforeGameClose();
void sfallOnCombatStart();
void sfallOnCombatEnd();
void sfallOnBeforeMapLoad();

} // namespace fallout

#endif // SFALL_BEHAVIOURS_H
