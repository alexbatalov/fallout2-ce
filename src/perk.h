#ifndef PERK_H
#define PERK_H

#include "db.h"
#include "obj_types.h"
#include "perk_defs.h"

namespace fallout {

int perksInit();
void perksReset();
void perksExit();
int perksLoad(File* stream);
int perksSave(File* stream);
int perkAdd(Object* critter, int perk);
int perkAddForce(Object* critter, int perk);
int perkRemove(Object* critter, int perk);
int perkGetAvailablePerks(Object* critter, int* perks);
int perkGetRank(Object* critter, int perk);
char* perkGetName(int perk);
char* perkGetDescription(int perk);
int perkGetFrmId(int perk);
void perkAddEffect(Object* critter, int perk);
void perkRemoveEffect(Object* critter, int perk);
int perkGetSkillModifier(Object* critter, int skill);

// Returns true if perk is valid.
static inline bool perkIsValid(int perk)
{
    return perk >= 0 && perk < PERK_COUNT;
}

// Returns true if critter has at least one rank in specified perk.
//
// NOTE: Most perks have only 1 rank, which means dude either have perk, or
// not.
//
// On the other hand, there are several places in editor, where they made two
// consequtive calls to [perkGetRank], first to check for presence, then get
// the actual value for displaying. So a macro could exist, or this very
// function, but due to similarity to [perkGetRank] it could have been
// collapsed by compiler.
static inline bool perkHasRank(Object* critter, int perk)
{
    return perkGetRank(critter, perk) != 0;
}

} // namespace fallout

#endif /* PERK_H */
