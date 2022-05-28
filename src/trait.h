#ifndef TRAIT_H
#define TRAIT_H

#include "db.h"
#include "message.h"
#include "trait_defs.h"

// Provides metadata about traits.
typedef struct TraitDescription {
    // The name of trait.
    char* name;

    // The description of trait.
    //
    // The description is only used in character editor to inform player about
    // effects of this trait.
    char* description;

    // Identifier of art in [intrface.lst].
    int frmId;
} TraitDescription;

extern MessageList gTraitsMessageList;
extern int gSelectedTraits[TRAITS_MAX_SELECTED_COUNT];
extern TraitDescription gTraitDescriptions[TRAIT_COUNT];

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

#endif /* TRAIT_H */
