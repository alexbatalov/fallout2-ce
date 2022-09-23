#ifndef REACTION_H
#define REACTION_H

#include "obj_types.h"

namespace fallout {

typedef enum NpcReaction {
    NPC_REACTION_BAD,
    NPC_REACTION_NEUTRAL,
    NPC_REACTION_GOOD,
} NpcReaction;

int reactionSetValue(Object* critter, int a2);
int reactionTranslateValue(int a1);
int _reaction_influence_();
int reactionGetValue(Object* critter);

} // namespace fallout

#endif /* REACTION_H */
