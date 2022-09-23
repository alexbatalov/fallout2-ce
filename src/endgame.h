#ifndef ENDGAME_H
#define ENDGAME_H

namespace fallout {

typedef enum EndgameDeathEndingReason {
    // Dude died.
    ENDGAME_DEATH_ENDING_REASON_DEATH = 0,

    // 13 years passed.
    ENDGAME_DEATH_ENDING_REASON_TIMEOUT = 2,
} EndgameDeathEndingReason;

extern char _aEnglish_2[];

void endgamePlaySlideshow();
void endgamePlayMovie();
int endgameDeathEndingInit();
int endgameDeathEndingExit();
void endgameSetupDeathEnding(int reason);
char* endgameDeathEndingGetFileName();

} // namespace fallout

#endif /* ENDGAME_H */
