#ifndef FALLOUT_LOOP_H_
#define FALLOUT_LOOP_H_

#include <cstdint>

namespace fallout {

enum LoopFlag : unsigned long {
    WORLDMAP           = 1 << 0,  // 0x1
//  RESERVED           = 1 << 1,  // 0x2 (unused)
    DIALOG             = 1 << 2,  // 0x4
    ESCMENU            = 1 << 3,  // 0x8
    SAVEGAME           = 1 << 4,  // 0x10
    LOADGAME           = 1 << 5,  // 0x20
    COMBAT             = 1 << 6,  // 0x40
    OPTIONS            = 1 << 7,  // 0x80
    HELP               = 1 << 8,  // 0x100
    CHARSCREEN         = 1 << 9,  // 0x200
    PIPBOY             = 1 << 10, // 0x400
    COMBAT_PLAYER_TURN = 1 << 11, // 0x800
    INVENTORY          = 1 << 12, // 0x1000
    AUTOMAP            = 1 << 13, // 0x2000
    SKILLDEX           = 1 << 14, // 0x4000
    USE_INTERFACE      = 1 << 15, // 0x8000
    LOOT_INTERFACE     = 1 << 16, // 0x10000
    BARTER             = 1 << 17, // 0x20000
//  HERO_WINDOW        = 1 << 18, // 0x40000 Hero Appearance mod
    DIALOG_REVIEW      = 1 << 19, // 0x80000
    COUNTER_WINDOW     = 1 << 20, // 0x100000 Counter window for moving multiple items or setting a timer

//  SPECIAL            = 1UL << 31  // 0x80000000 Additional special flag for all modes
};

// low-level API
uint32_t loopCurrentFlags();
bool loopGetFlag(LoopFlag flag);
void loopSetFlag(LoopFlag flag);
void loopClearFlag(LoopFlag flag);

// high-level predicates
bool loopIsInWorldMap();
bool loopIsInCombatEnemyTurn();
bool loopIsInCombatWaitingForPlayerAction(); // with no menus open
bool loopIsOutsideCombatWaitingForPlayerAction(); // with no menus open

} // namespace fallout

#endif /* FALLOUT_LOOP_H_ */
