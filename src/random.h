#ifndef RANDOM_H
#define RANDOM_H

#include "db.h"

namespace fallout {

typedef enum Roll {
    ROLL_CRITICAL_FAILURE,
    ROLL_FAILURE,
    ROLL_SUCCESS,
    ROLL_CRITICAL_SUCCESS,
} Roll;

void randomInit();
void randomReset();
void randomExit();
int randomSave(File* stream);
int randomLoad(File* stream);
int randomRoll(int difficulty, int criticalSuccessModifier, int* howMuchPtr);
int randomBetween(int min, int max);
void randomSeedPrerandom(int seed);

} // namespace fallout

#endif /* RANDOM_H */
