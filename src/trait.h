#ifndef TRAIT_H
#define TRAIT_H

#include "db.h"
#include "trait_defs.h"

namespace fallout {

int traitsInit();
void traitsReset();
void traitsExit();
int traitsLoad(File* stream);
int traitsSave(File* stream);
void traitsSetSelected(int trait1, int trait2);
void traitsGetSelected(int* trait1, int* trait2);
char* traitGetName(int trait);
char* traitGetDescription(int trait);
int traitGetFrmId(int trait);
bool traitIsSelected(int trait);
int traitGetStatModifier(int stat);
int traitGetSkillModifier(int skill);

} // namespace fallout

#endif /* TRAIT_H */
