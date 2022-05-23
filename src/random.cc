#include "random.h"

#include "debug.h"
#include "scripts.h"

#include <limits.h>
#include <stdlib.h>

// clang-format off
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
// clang-format on

// 0x50D4BA
const double dbl_50D4BA = 36.42;

// 0x50D4C2
const double dbl_50D4C2 = 4000;

// 0x51C694
int _iy = 0;

// 0x6648D0
int _iv[32];

// 0x664950
int _idum;

// 0x4A2FE0
void Random::init()
{
    unsigned int randomSeed = getSeed();
    srand(randomSeed);

    int pseudorandomSeed = Int32();
    seedPrerandomInternal(pseudorandomSeed);

    validatePrerandom();
}

// Note: Collapsed.
//
// 0x4A2FFC
int Random::rollReset()
{
    return 0;
}

// NOTE: Uncollapsed 0x4A2FFC.
void Random::reset()
{
    rollReset();
}

// NOTE: Uncollapsed 0x4A2FFC.
void Random::exit()
{
    rollReset();
}

// NOTE: Uncollapsed 0x4A2FFC.
int Random::save(File* stream)
{
    return rollReset();
}

// NOTE: Uncollapsed 0x4A2FFC.
int Random::load(File* stream)
{
    return rollReset();
}

// Rolls d% against [difficulty].
//
// 0x4A3000
int Random::roll(int difficulty, int criticalSuccessModifier, int* howMuchPtr)
{
    int delta = difficulty - Random::between(1, 100);
    int result = translateRoll(delta, criticalSuccessModifier);

    if (howMuchPtr != NULL) {
        *howMuchPtr = delta;
    }

    return result;
}

// Translates raw d% result into [Roll] constants, possibly upgrading to
// criticals (starting from day 2).
//
// 0x4A3030
int Random::translateRoll(int delta, int criticalSuccessModifier)
{
    int gameTime = gameTimeGetTime();

    int roll;
    if (delta < 0) {
        roll = Random::Roll::FAILURE;

        if ((gameTime / GAME_TIME_TICKS_PER_DAY) >= 1) {
            // 10% to become critical failure.
            if (Random::between(1, 100) <= -delta / 10) {
                roll = Random::Roll::CRITICAL_FAILURE;
            }
        }
    } else {
        roll = Random::Roll::SUCCESS;

        if ((gameTime / GAME_TIME_TICKS_PER_DAY) >= 1) {
            // 10% + modifier to become critical success.
            if (Random::between(1, 100) <= delta / 10 + criticalSuccessModifier) {
                roll = Random::Roll::CRITICAL_SUCCESS;
            }
        }
    }

    return roll;
}

// 0x4A30C0
int Random::between(int min, int max)
{
    int result;

    if (min <= max) {
        result = min + getRandom(max - min + 1);
    } else {
        result = max + getRandom(min - max + 1);
    }

    if (result < min || result > max) {
        debugPrint("Random number %d is not in range %d to %d", result, min, max);
        result = min;
    }

    return result;
}

// 0x4A30FC
int Random::getRandom(int max)
{
    int v1 = 16807 * (_idum % 127773) - 2836 * (_idum / 127773);

    if (v1 < 0) {
        v1 += 0x7FFFFFFF;
    }

    if (v1 < 0) {
        v1 += 0x7FFFFFFF;
    }

    int v2 = _iy & 0x1F;
    int v3 = _iv[v2];
    _iv[v2] = v1;
    _iy = v3;
    _idum = v1;

    return v3 % max;
}

// 0x4A31A0
void Random::seedPrerandom(int seed)
{
    if (seed == -1) {
        // NOTE: Uninline.
        seed = Int32();
    }

    seedPrerandomInternal(seed);
}

// 0x4A31C4
int Random::Int32()
{
    int high = rand() << 16;
    int low = rand();

    return (high + low) & INT_MAX;
}

// 0x4A31E0
void Random::seedPrerandomInternal(int seed)
{
    int num = seed;
    if (num < 1) {
        num = 1;
    }

    for (int index = 40; index > 0; index--) {
        num = 16807 * (num % 127773) - 2836 * (num / 127773);

        if (num < 0) {
            num &= INT_MAX;
        }

        if (index < 32) {
            _iv[index] = num;
        }
    }

    _iy = _iv[0];
    _idum = num;
}

// Provides seed for random number generator.
//
// 0x4A3258
unsigned int Random::getSeed()
{
    return timeGetTime();
}

// 0x4A3264
void Random::validatePrerandom()
{
    int results[25];

    for (int index = 0; index < 25; index++) {
        results[index] = 0;
    }

    for (int attempt = 0; attempt < 100000; attempt++) {
        int value = Random::between(1, 25);
        if (value - 1 < 0) {
            debugPrint("I made a negative number %d\n", value - 1);
        }

        results[value - 1]++;
    }

    double v1 = 0.0;

    for (int index = 0; index < 25; index++) {
        double v2 = ((double)results[index] - dbl_50D4C2) * ((double)results[index] - dbl_50D4C2) / dbl_50D4C2;
        v1 += v2;
    }

    debugPrint("Chi squared is %f, P = %f at 0.05\n", v1, dbl_50D4C2);

    if (v1 < dbl_50D4BA) {
        debugPrint("Sequence is random, 95%% confidence.\n");
    } else {
        debugPrint("Warning! Sequence is not random, 95%% confidence.\n");
    }
}
