#ifndef REACTION_H
#define REACTION_H

#include "obj_types.h"

namespace fallout {

typedef enum NpcReaction {
    NPC_REACTION_BAD,
    NPC_REACTION_NEUTRAL,
    NPC_REACTION_GOOD,
} NpcReaction;

int reactionSetValue(Object* critter, int value);
int reactionTranslateValue(int value);
int _reaction_influence_();
int reactionGetValue(Object* critter);

} // namespace fallout

#endif /* REACTION_H */
