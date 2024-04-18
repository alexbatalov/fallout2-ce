#ifndef SKILL_H
#define SKILL_H

#include "db.h"
#include "obj_types.h"
#include "proto_types.h"
#include "skill_defs.h"

namespace fallout {

extern int _gIsSteal;
extern int _gStealCount;
extern int _gStealSize;

int skillsInit();
void skillsReset();
void skillsExit();
int skillsLoad(File* stream);
int skillsSave(File* stream);
void protoCritterDataResetSkills(CritterProtoData* data);
void skillsSetTagged(int* skills, int count);
void skillsGetTagged(int* skills, int count);
bool skillIsTagged(int skill);
int skillGetValue(Object* critter, int skill);
int skillGetDefaultValue(int skill);
int skillGetBaseValue(Object* critter, int skill);
int skillAdd(Object* critter, int skill);
int skillAddForce(Object* critter, int skill);
int skillsGetCost(int a1);
int skillSub(Object* critter, int skill);
int skillSubForce(Object* critter, int skill);
int skillRoll(Object* critter, int skill, int a3, int* a4);
char* skillGetName(int skill);
char* skillGetDescription(int skill);
char* skillGetAttributes(int skill);
int skillGetFrmId(int skill);
int skillUse(Object* obj, Object* target, int skill, int criticalChanceModifier);
int skillsPerformStealing(Object* a1, Object* a2, Object* item, bool isPlanting);
int skillGetGameDifficultyModifier(int skill);
int skillUpdateLastUse(int skill);
int skillsUsageSave(File* stream);
int skillsUsageLoad(File* stream);
char* skillsGetGenericResponse(Object* critter, bool isDude);

// Returns true if skill is valid.
static inline bool skillIsValid(int skill)
{
    return skill >= 0 && skill < SKILL_COUNT;
}

} // namespace fallout

#endif /* SKILL_H */
