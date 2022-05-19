#include "reaction.h"

#include "scripts.h"

// 0x4A29D0
int reactionSetValue(Object* critter, int value)
{
    scriptSetLocalVar(critter->sid, 0, value);
    return 0;
}

// 0x4A29E8
int reactionTranslateValue(int a1)
{
    if (a1 > 10) {
        return NPC_REACTION_GOOD;
    } else if (a1 > -10) {
        return NPC_REACTION_NEUTRAL;
    } else if (a1 > -25) {
        return NPC_REACTION_BAD;
    } else if (a1 > -50) {
        return NPC_REACTION_BAD;
    } else if (a1 > -75) {
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
    int reactionValue;

    if (scriptGetLocalVar(critter->sid, 0, &reactionValue) == -1) {
        return -1;
    }

    return reactionValue;
}
