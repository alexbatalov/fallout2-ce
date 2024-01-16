#ifndef STAT_H
#define STAT_H

#include "db.h"
#include "obj_types.h"
#include "proto_types.h"
#include "stat_defs.h"

namespace fallout {

#define STAT_ERR_INVALID_STAT (-5)

int statsInit();
int statsReset();
int statsExit();
int statsLoad(File* stream);
int statsSave(File* stream);
int critterGetStat(Object* critter, int stat);
int critterGetBaseStatWithTraitModifier(Object* critter, int stat);
int critterGetBaseStat(Object* critter, int stat);
int critterGetBonusStat(Object* critter, int stat);
int critterSetBaseStat(Object* critter, int stat, int value);
int critterIncBaseStat(Object* critter, int stat);
int critterDecBaseStat(Object* critter, int stat);
int critterSetBonusStat(Object* critter, int stat, int value);
void protoCritterDataResetStats(CritterProtoData* data);
void critterUpdateDerivedStats(Object* critter);
char* statGetName(int stat);
char* statGetDescription(int stat);
char* statGetValueDescription(int value);
int pcGetStat(int pcStat);
int pcSetStat(int pcStat, int value);
void pcStatsReset();
int pcGetExperienceForNextLevel();
int pcGetExperienceForLevel(int level);
char* pcStatGetName(int pcStat);
char* pcStatGetDescription(int pcStat);
int statGetFrmId(int stat);
int statRoll(Object* critter, int stat, int modifier, int* howMuch);
int pcAddExperience(int xp, int* xpGained = nullptr);
int pcAddExperienceWithOptions(int xp, bool a2, int* xpGained = nullptr);
int pcSetExperience(int a1);

static inline bool statIsValid(int stat)
{
    return stat >= 0 && stat < STAT_COUNT;
}

static inline bool pcStatIsValid(int pcStat)
{
    return pcStat >= 0 && pcStat < PC_STAT_COUNT;
}

} // namespace fallout

#endif /* STAT_H */
