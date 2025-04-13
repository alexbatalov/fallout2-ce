#include "reaction.h"

#include "scripts.h"

namespace fallout {

// 0x4A29D0
int reactionSetValue(Object* critter, int value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_INT;
    programValue.integerValue = value;
    scriptSetLocalVar(critter->sid, 0, programValue);
    return 0;
}

// 0x4A29E8
int reactionTranslateValue(int value)
{
    if (value > 10) {
        return NPC_REACTION_GOOD;
    } else if (value > -10) {
        return NPC_REACTION_NEUTRAL;
    } else if (value > -25) {
        return NPC_REACTION_BAD;
    } else if (value > -50) {
        return NPC_REACTION_BAD;
    } else if (value > -75) {
        return NPC_REACTION_BAD;
    } else {
        return NPC_REACTION_BAD;
    }
}

// 0x4A29F0
int _reaction_influence_()
{
    return 0;
}

// 0x4A2B28
int reactionGetValue(Object* critter)
{
    ProgramValue programValue;

    if (scriptGetLocalVar(critter->sid, 0, programValue) == -1) {
        return -1;
    }

    return programValue.integerValue;
}

} // namespace fallout
