#include "trait.h"

#include <stdio.h>

#include "game.h"
#include "message.h"
#include "object.h"
#include "platform_compat.h"
#include "skill.h"
#include "stat.h"

namespace fallout {

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

// 0x66BE38
static MessageList gTraitsMessageList;

// List of selected traits.
//
// 0x66BE40
static int gSelectedTraits[TRAITS_MAX_SELECTED_COUNT];

// 0x51DB84
static TraitDescription gTraitDescriptions[TRAIT_COUNT] = {
    { nullptr, nullptr, 55 },
    { nullptr, nullptr, 56 },
    { nullptr, nullptr, 57 },
    { nullptr, nullptr, 58 },
    { nullptr, nullptr, 59 },
    { nullptr, nullptr, 60 },
    { nullptr, nullptr, 61 },
    { nullptr, nullptr, 62 },
    { nullptr, nullptr, 63 },
    { nullptr, nullptr, 64 },
    { nullptr, nullptr, 65 },
    { nullptr, nullptr, 66 },
    { nullptr, nullptr, 67 },
    { nullptr, nullptr, 94 },
    { nullptr, nullptr, 69 },
    { nullptr, nullptr, 70 },
};

// 0x4B39F0
int traitsInit()
{
    if (!messageListInit(&gTraitsMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "trait.msg");

    if (!messageListLoad(&gTraitsMessageList, path)) {
        return -1;
    }

    for (int trait = 0; trait < TRAIT_COUNT; trait++) {
        MessageListItem messageListItem;

        messageListItem.num = 100 + trait;
        if (messageListGetItem(&gTraitsMessageList, &messageListItem)) {
            gTraitDescriptions[trait].name = messageListItem.text;
        }

        messageListItem.num = 200 + trait;
        if (messageListGetItem(&gTraitsMessageList, &messageListItem)) {
            gTraitDescriptions[trait].description = messageListItem.text;
        }
    }

    // NOTE: Uninline.
    traitsReset();

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_TRAIT, &gTraitsMessageList);

    return true;
}

// 0x4B3ADC
void traitsReset()
{
    for (int index = 0; index < TRAITS_MAX_SELECTED_COUNT; index++) {
        gSelectedTraits[index] = -1;
    }
}

// 0x4B3AF8
void traitsExit()
{
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_TRAIT, nullptr);
    messageListFree(&gTraitsMessageList);
}

// Loads trait system state from save game.
//
// 0x4B3B08
int traitsLoad(File* stream)
{
    return fileReadInt32List(stream, gSelectedTraits, TRAITS_MAX_SELECTED_COUNT);
}

// Saves trait system state to save game.
//
// 0x4B3B28
int traitsSave(File* stream)
{
    return fileWriteInt32List(stream, gSelectedTraits, TRAITS_MAX_SELECTED_COUNT);
}

// Sets selected traits.
//
// 0x4B3B48
void traitsSetSelected(int trait1, int trait2)
{
    gSelectedTraits[0] = trait1;
    gSelectedTraits[1] = trait2;
}

// Returns selected traits.
//
// 0x4B3B54
void traitsGetSelected(int* trait1, int* trait2)
{
    *trait1 = gSelectedTraits[0];
    *trait2 = gSelectedTraits[1];
}

// Returns a name of the specified trait, or `NULL` if the specified trait is
// out of range.
//
// 0x4B3B68
char* traitGetName(int trait)
{
    return trait >= 0 && trait < TRAIT_COUNT ? gTraitDescriptions[trait].name : nullptr;
}

// Returns a description of the specified trait, or `NULL` if the specified
// trait is out of range.
//
// 0x4B3B88
char* traitGetDescription(int trait)
{
    return trait >= 0 && trait < TRAIT_COUNT ? gTraitDescriptions[trait].description : nullptr;
}

// Return an art ID of the specified trait, or `0` if the specified trait is
// out of range.
//
// 0x4B3BA8
int traitGetFrmId(int trait)
{
    return trait >= 0 && trait < TRAIT_COUNT ? gTraitDescriptions[trait].frmId : 0;
}

// Returns `true` if the specified trait is selected.
//
// 0x4B3BC8
bool traitIsSelected(int trait)
{
    return gSelectedTraits[0] == trait || gSelectedTraits[1] == trait;
}

// Returns stat modifier depending on selected traits.
//
// 0x4B3C7C
int traitGetStatModifier(int stat)
{
    int modifier = 0;

    switch (stat) {
    case STAT_STRENGTH:
        if (traitIsSelected(TRAIT_GIFTED)) {
            modifier += 1;
        }
        if (traitIsSelected(TRAIT_BRUISER)) {
            modifier += 2;
        }
        break;
    case STAT_PERCEPTION:
        if (traitIsSelected(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_ENDURANCE:
        if (traitIsSelected(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_CHARISMA:
        if (traitIsSelected(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_INTELLIGENCE:
        if (traitIsSelected(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_AGILITY:
        if (traitIsSelected(TRAIT_GIFTED)) {
            modifier += 1;
        }
        if (traitIsSelected(TRAIT_SMALL_FRAME)) {
            modifier += 1;
        }
        break;
    case STAT_LUCK:
        if (traitIsSelected(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_MAXIMUM_ACTION_POINTS:
        if (traitIsSelected(TRAIT_BRUISER)) {
            modifier -= 2;
        }
        break;
    case STAT_ARMOR_CLASS:
        if (traitIsSelected(TRAIT_KAMIKAZE)) {
            modifier -= critterGetBaseStat(gDude, STAT_ARMOR_CLASS);
        }
        break;
    case STAT_MELEE_DAMAGE:
        if (traitIsSelected(TRAIT_HEAVY_HANDED)) {
            modifier += 4;
        }
        break;
    case STAT_CARRY_WEIGHT:
        if (traitIsSelected(TRAIT_SMALL_FRAME)) {
            modifier -= 10 * critterGetBaseStat(gDude, STAT_STRENGTH);
        }
        break;
    case STAT_SEQUENCE:
        if (traitIsSelected(TRAIT_KAMIKAZE)) {
            modifier += 5;
        }
        break;
    case STAT_HEALING_RATE:
        if (traitIsSelected(TRAIT_FAST_METABOLISM)) {
            modifier += 2;
        }
        break;
    case STAT_CRITICAL_CHANCE:
        if (traitIsSelected(TRAIT_FINESSE)) {
            modifier += 10;
        }
        break;
    case STAT_BETTER_CRITICALS:
        if (traitIsSelected(TRAIT_HEAVY_HANDED)) {
            modifier -= 30;
        }
        break;
    case STAT_RADIATION_RESISTANCE:
        if (traitIsSelected(TRAIT_FAST_METABOLISM)) {
            modifier -= critterGetBaseStat(gDude, STAT_RADIATION_RESISTANCE);
        }
        break;
    case STAT_POISON_RESISTANCE:
        if (traitIsSelected(TRAIT_FAST_METABOLISM)) {
            modifier -= critterGetBaseStat(gDude, STAT_POISON_RESISTANCE);
        }
        break;
    }

    return modifier;
}

// Returns skill modifier depending on selected traits.
//
// 0x4B40FC
int traitGetSkillModifier(int skill)
{
    int modifier = 0;

    if (traitIsSelected(TRAIT_GIFTED)) {
        modifier -= 10;
    }

    if (traitIsSelected(TRAIT_GOOD_NATURED)) {
        switch (skill) {
        case SKILL_SMALL_GUNS:
        case SKILL_BIG_GUNS:
        case SKILL_ENERGY_WEAPONS:
        case SKILL_UNARMED:
        case SKILL_MELEE_WEAPONS:
        case SKILL_THROWING:
            modifier -= 10;
            break;
        case SKILL_FIRST_AID:
        case SKILL_DOCTOR:
        case SKILL_SPEECH:
        case SKILL_BARTER:
            modifier += 15;
            break;
        }
    }

    return modifier;
}

} // namespace fallout
