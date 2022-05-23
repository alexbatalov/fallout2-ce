#ifndef RANDOM_H
#define RANDOM_H

#include "db.h"

class Random {
public:
    enum Roll {
        CRITICAL_FAILURE,
        FAILURE,
        SUCCESS,
        CRITICAL_SUCCESS,
    };
    static void init();
    static void reset();
    static void exit();
    static int save(File* stream);
    static int load(File* stream);
    static int roll(int difficulty, int criticalSuccessModifier, int* howMuchPtr);
    static int between(int min, int max);
    static void seedPrerandom(int seed);

private:
    static int rollReset();
    static int translateRoll(int delta, int criticalSuccessModifier);
    static int getRandom(int max);
    static int Int32();
    static void seedPrerandomInternal(int seed);
    static unsigned int getSeed();
    static void validatePrerandom();
};

#endif /* RANDOM_H */
