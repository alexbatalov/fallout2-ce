#ifndef PERK_H
#define PERK_H

#include "db.h"
#include "message.h"
#include "obj_types.h"
#include "perk_defs.h"
#include "stat_defs.h"

typedef struct PerkDescription {
    char* name;
    char* description;
    int frmId;
    int maxRank;
    int minLevel;
    int stat;
    int statModifier;
    int param1;
    int value1;
    int field_24;
    int param2;
    int value2;
    int stats[PRIMARY_STAT_COUNT];
} PerkDescription;

typedef struct PerkRankData {
    int ranks[PERK_COUNT];
} PerkRankData;

extern PerkDescription gPerkDescriptions[PERK_COUNT];
extern PerkRankData* gPartyMemberPerkRanks;
extern int gHereAndNowBonusExperience;

extern MessageList gPerksMessageList;

int perksInit();
void perksReset();
void perksExit();
int perksLoad(File* stream);
int perksSave(File* stream);
PerkRankData* perkGetRankData(Object* critter);
bool perkCanAdd(Object* critter, int perk);
void perkResetRanks();
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

#endif /* PERK_H */
