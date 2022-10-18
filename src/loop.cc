#include "loop.h"

namespace fallout {

static uint32_t loopFlags = 0;

// low-level API

uint32_t loopCurrentFlags() {
    return loopFlags;
}

bool loopGetFlag(LoopFlag flag) {
    return loopFlags & flag;
}

void loopSetFlag(LoopFlag flag) {
    loopFlags |= flag;
}

void loopClearFlag(LoopFlag flag) {
    loopFlags &= ~flag;
}

// high-level helpers

bool loopIsInWorldMap() {
    return loopFlags & LoopFlag::WORLDMAP;
}

bool loopIsInCombatEnemyTurn() {
    return loopFlags == LoopFlag::COMBAT;
}

// with no menus open
bool loopIsInCombatWaitingForPlayerAction() {
    return loopFlags == (LoopFlag::COMBAT | LoopFlag::COMBAT_PLAYER_TURN);
}

// with no menus open
bool loopIsOutsideCombatWaitingForPlayerAction() {
    return !loopFlags;
}

} // namespace fallout
