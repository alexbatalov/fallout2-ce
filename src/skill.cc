#include "skill.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "actions.h"
#include "color.h"
#include "combat.h"
#include "critter.h"
#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "interface.h"
#include "item.h"
#include "message.h"
#include "object.h"
#include "palette.h"
#include "party_member.h"
#include "perk.h"
#include "pipboy.h"
#include "platform_compat.h"
#include "proto.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "stat.h"
#include "trait.h"

namespace fallout {

#define SKILLS_MAX_USES_PER_DAY (3)

#define REPAIRABLE_DAMAGE_FLAGS_LENGTH (5)
#define HEALABLE_DAMAGE_FLAGS_LENGTH (5)

typedef struct SkillDescription {
    char* name;
    char* description;
    char* attributes;
    int frmId;
    int defaultValue;
    int statModifier;
    int stat1;
    int stat2;
    int field_20;
    int experience;
    int field_28;
} SkillDescription;

static void _show_skill_use_messages(Object* obj, int skill, Object* a3, int a4, int a5);
static int skillGetFreeUsageSlot(int skill);
static int skill_use_slot_clear();

// Damage flags which can be repaired using "Repair" skill.
//
// 0x4AA2F0
static const int gRepairableDamageFlags[REPAIRABLE_DAMAGE_FLAGS_LENGTH] = {
    DAM_BLIND,
    DAM_CRIP_ARM_LEFT,
    DAM_CRIP_ARM_RIGHT,
    DAM_CRIP_LEG_RIGHT,
    DAM_CRIP_LEG_LEFT,
};

// Damage flags which can be healed using "Doctor" skill.
//
// 0x4AA304
static const int gHealableDamageFlags[HEALABLE_DAMAGE_FLAGS_LENGTH] = {
    DAM_BLIND,
    DAM_CRIP_ARM_LEFT,
    DAM_CRIP_ARM_RIGHT,
    DAM_CRIP_LEG_RIGHT,
    DAM_CRIP_LEG_LEFT,
};

// 0x51D118
static SkillDescription gSkillDescriptions[SKILL_COUNT] = {
    { nullptr, nullptr, nullptr, 28, 5, 4, STAT_AGILITY, STAT_INVALID, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 29, 0, 2, STAT_AGILITY, STAT_INVALID, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 30, 0, 2, STAT_AGILITY, STAT_INVALID, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 31, 30, 2, STAT_AGILITY, STAT_STRENGTH, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 32, 20, 2, STAT_AGILITY, STAT_STRENGTH, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 33, 0, 4, STAT_AGILITY, STAT_INVALID, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 34, 0, 2, STAT_PERCEPTION, STAT_INTELLIGENCE, 1, 25, 0 },
    { nullptr, nullptr, nullptr, 35, 5, 1, STAT_PERCEPTION, STAT_INTELLIGENCE, 1, 50, 0 },
    { nullptr, nullptr, nullptr, 36, 5, 3, STAT_AGILITY, STAT_INVALID, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 37, 10, 1, STAT_PERCEPTION, STAT_AGILITY, 1, 25, 1 },
    { nullptr, nullptr, nullptr, 38, 0, 3, STAT_AGILITY, STAT_INVALID, 1, 25, 1 },
    { nullptr, nullptr, nullptr, 39, 10, 1, STAT_PERCEPTION, STAT_AGILITY, 1, 25, 1 },
    { nullptr, nullptr, nullptr, 40, 0, 4, STAT_INTELLIGENCE, STAT_INVALID, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 41, 0, 3, STAT_INTELLIGENCE, STAT_INVALID, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 42, 0, 5, STAT_CHARISMA, STAT_INVALID, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 43, 0, 4, STAT_CHARISMA, STAT_INVALID, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 44, 0, 5, STAT_LUCK, STAT_INVALID, 1, 0, 0 },
    { nullptr, nullptr, nullptr, 45, 0, 2, STAT_ENDURANCE, STAT_INTELLIGENCE, 1, 100, 0 },
};

// 0x51D430
int _gIsSteal = 0;

// Something about stealing, base value?
//
// 0x51D434
int _gStealCount = 0;

// 0x51D438
int _gStealSize = 0;

// 0x667F98
static int _timesSkillUsed[SKILL_COUNT][SKILLS_MAX_USES_PER_DAY];

// 0x668070
static int gTaggedSkills[NUM_TAGGED_SKILLS];

// skill.msg
//
// 0x668080
static MessageList gSkillsMessageList;

// 0x4AA318
int skillsInit()
{
    if (!messageListInit(&gSkillsMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "skill.msg");

    if (!messageListLoad(&gSkillsMessageList, path)) {
        return -1;
    }

    for (int skill = 0; skill < SKILL_COUNT; skill++) {
        MessageListItem messageListItem;

        messageListItem.num = 100 + skill;
        if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
            gSkillDescriptions[skill].name = messageListItem.text;
        }

        messageListItem.num = 200 + skill;
        if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
            gSkillDescriptions[skill].description = messageListItem.text;
        }

        messageListItem.num = 300 + skill;
        if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
            gSkillDescriptions[skill].attributes = messageListItem.text;
        }
    }

    for (int index = 0; index < NUM_TAGGED_SKILLS; index++) {
        gTaggedSkills[index] = -1;
    }

    // NOTE: Uninline.
    skill_use_slot_clear();

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_SKILL, &gSkillsMessageList);

    return 0;
}

// 0x4AA448
void skillsReset()
{
    for (int index = 0; index < NUM_TAGGED_SKILLS; index++) {
        gTaggedSkills[index] = -1;
    }

    // NOTE: Uninline.
    skill_use_slot_clear();
}

// 0x4AA478
void skillsExit()
{
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_SKILL, nullptr);
    messageListFree(&gSkillsMessageList);
}

// 0x4AA488
int skillsLoad(File* stream)
{
    return fileReadInt32List(stream, gTaggedSkills, NUM_TAGGED_SKILLS);
}

// 0x4AA4A8
int skillsSave(File* stream)
{
    return fileWriteInt32List(stream, gTaggedSkills, NUM_TAGGED_SKILLS);
}

// 0x4AA4C8
void protoCritterDataResetSkills(CritterProtoData* data)
{
    for (int skill = 0; skill < SKILL_COUNT; skill++) {
        data->skills[skill] = 0;
    }
}

// 0x4AA4E4
void skillsSetTagged(int* skills, int count)
{
    for (int index = 0; index < count; index++) {
        gTaggedSkills[index] = skills[index];
    }
}

// 0x4AA508
void skillsGetTagged(int* skills, int count)
{
    for (int index = 0; index < count; index++) {
        skills[index] = gTaggedSkills[index];
    }
}

// 0x4AA52C
bool skillIsTagged(int skill)
{
    return skill == gTaggedSkills[0]
        || skill == gTaggedSkills[1]
        || skill == gTaggedSkills[2]
        || skill == gTaggedSkills[3];
}

// 0x4AA558
int skillGetValue(Object* critter, int skill)
{
    if (!skillIsValid(skill)) {
        return -5;
    }

    int baseValue = skillGetBaseValue(critter, skill);
    if (baseValue < 0) {
        return baseValue;
    }

    SkillDescription* skillDescription = &(gSkillDescriptions[skill]);

    int v7 = critterGetStat(critter, skillDescription->stat1);
    if (skillDescription->stat2 != -1) {
        v7 += critterGetStat(critter, skillDescription->stat2);
    }

    int value = skillDescription->defaultValue + skillDescription->statModifier * v7 + baseValue * skillDescription->field_20;

    if (critter == gDude) {
        if (skillIsTagged(skill)) {
            value += baseValue * skillDescription->field_20;

            if (!perkGetRank(critter, PERK_TAG) || skill != gTaggedSkills[3]) {
                value += 20;
            }
        }

        value += traitGetSkillModifier(skill);
        value += perkGetSkillModifier(critter, skill);
        value += skillGetGameDifficultyModifier(skill);
    }

    if (value > 300) {
        value = 300;
    }

    return value;
}

// 0x4AA654
int skillGetDefaultValue(int skill)
{
    return skillIsValid(skill) ? gSkillDescriptions[skill].defaultValue : -5;
}

// 0x4AA680
int skillGetBaseValue(Object* obj, int skill)
{
    if (!skillIsValid(skill)) {
        return 0;
    }

    Proto* proto;
    protoGetProto(obj->pid, &proto);

    return proto->critter.data.skills[skill];
}

// 0x4AA6BC
int skillAdd(Object* obj, int skill)
{
    if (obj != gDude) {
        return -5;
    }

    if (!skillIsValid(skill)) {
        return -5;
    }

    Proto* proto;
    protoGetProto(obj->pid, &proto);

    int unspentSp = pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS);
    if (unspentSp <= 0) {
        return -4;
    }

    int skillValue = skillGetValue(obj, skill);
    if (skillValue >= 300) {
        return -3;
    }

    // NOTE: Uninline.
    int requiredSp = skillsGetCost(skillValue);

    if (unspentSp < requiredSp) {
        return -4;
    }

    int rc = pcSetStat(PC_STAT_UNSPENT_SKILL_POINTS, unspentSp - requiredSp);
    if (rc == 0) {
        proto->critter.data.skills[skill] += 1;
    }

    return rc;
}

// 0x4AA7F8
int skillAddForce(Object* obj, int skill)
{
    if (obj != gDude) {
        return -5;
    }

    if (!skillIsValid(skill)) {
        return -5;
    }

    Proto* proto;
    protoGetProto(obj->pid, &proto);

    if (skillGetValue(obj, skill) >= 300) {
        return -3;
    }

    proto->critter.data.skills[skill] += 1;

    return 0;
}

// Returns the cost of raising skill value in skill points.
//
// 0x4AA87C
int skillsGetCost(int skillValue)
{
    if (skillValue >= 201) {
        return 6;
    } else if (skillValue >= 176) {
        return 5;
    } else if (skillValue >= 151) {
        return 4;
    } else if (skillValue >= 126) {
        return 3;
    } else if (skillValue >= 101) {
        return 2;
    } else {
        return 1;
    }
}

// Decrements specified skill value by one, returning appropriate amount as
// unspent skill points.
//
// 0x4AA8C4
int skillSub(Object* critter, int skill)
{
    if (critter != gDude) {
        return -5;
    }

    if (!skillIsValid(skill)) {
        return -5;
    }

    Proto* proto;
    protoGetProto(critter->pid, &proto);

    if (proto->critter.data.skills[skill] <= 0) {
        return -2;
    }

    int unspentSp = pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS);
    int skillValue = skillGetValue(critter, skill) - 1;

    // NOTE: Uninline.
    int requiredSp = skillsGetCost(skillValue);

    int newUnspentSp = unspentSp + requiredSp;
    int rc = pcSetStat(PC_STAT_UNSPENT_SKILL_POINTS, newUnspentSp);
    if (rc != 0) {
        return rc;
    }

    proto->critter.data.skills[skill] -= 1;

    if (skillIsTagged(skill)) {
        int oldSkillCost = skillsGetCost(skillValue);
        int newSkillCost = skillsGetCost(skillGetValue(critter, skill));
        if (oldSkillCost != newSkillCost) {
            rc = pcSetStat(PC_STAT_UNSPENT_SKILL_POINTS, newUnspentSp - 1);
            if (rc != 0) {
                return rc;
            }
        }
    }

    if (proto->critter.data.skills[skill] < 0) {
        proto->critter.data.skills[skill] = 0;
    }

    return 0;
}

// Decrements specified skill value by one.
//
// 0x4AAA34
int skillSubForce(Object* obj, int skill)
{
    Proto* proto;

    if (obj != gDude) {
        return -5;
    }

    if (!skillIsValid(skill)) {
        return -5;
    }

    protoGetProto(obj->pid, &proto);

    if (proto->critter.data.skills[skill] <= 0) {
        return -2;
    }

    proto->critter.data.skills[skill] -= 1;

    return 0;
}

// 0x4AAAA4
int skillRoll(Object* critter, int skill, int modifier, int* howMuch)
{
    if (!skillIsValid(skill)) {
        return ROLL_FAILURE;
    }

    if (critter == gDude && skill != SKILL_STEAL) {
        Object* partyMember = partyMemberGetBestInSkill(skill);
        if (partyMember != nullptr) {
            if (partyMemberGetBestSkill(partyMember) == skill) {
                critter = partyMember;
            }
        }
    }

    int skillValue = skillGetValue(critter, skill);

    if (critter == gDude && skill == SKILL_STEAL) {
        if (dudeHasState(DUDE_STATE_SNEAKING)) {
            if (dudeIsSneaking()) {
                skillValue += 30;
            }
        }
    }

    int criticalChance = critterGetStat(critter, STAT_CRITICAL_CHANCE);
    return randomRoll(skillValue + modifier, criticalChance, howMuch);
}

// 0x4AAB9C
char* skillGetName(int skill)
{
    return skillIsValid(skill) ? gSkillDescriptions[skill].name : nullptr;
}

// 0x4AABC0
char* skillGetDescription(int skill)
{
    return skillIsValid(skill) ? gSkillDescriptions[skill].description : nullptr;
}

// 0x4AABE4
char* skillGetAttributes(int skill)
{
    return skillIsValid(skill) ? gSkillDescriptions[skill].attributes : nullptr;
}

// 0x4AAC08
int skillGetFrmId(int skill)
{
    return skillIsValid(skill) ? gSkillDescriptions[skill].frmId : 0;
}

// 0x4AAC2C
static void _show_skill_use_messages(Object* obj, int skill, Object* a3, int a4, int criticalChanceModifier)
{
    if (obj != gDude) {
        return;
    }

    if (a4 <= 0) {
        return;
    }

    SkillDescription* skillDescription = &(gSkillDescriptions[skill]);

    int baseExperience = skillDescription->experience;
    if (baseExperience == 0) {
        return;
    }

    if (skillDescription->field_28 && criticalChanceModifier < 0) {
        baseExperience += abs(criticalChanceModifier);
    }

    int xpToAdd = a4 * baseExperience;

    int before = pcGetStat(PC_STAT_EXPERIENCE);

    if (pcAddExperience(xpToAdd) == 0 && a4 > 0) {
        MessageListItem messageListItem;
        messageListItem.num = 505; // You earn %d XP for honing your skills
        if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
            int after = pcGetStat(PC_STAT_EXPERIENCE);

            char text[60];
            snprintf(text, sizeof(text), messageListItem.text, after - before);
            displayMonitorAddMessage(text);
        }
    }
}

// skill_use
// 0x4AAD08
int skillUse(Object* obj, Object* a2, int skill, int criticalChanceModifier)
{
    MessageListItem messageListItem;
    char text[60];

    bool giveExp = true;
    int currentHp = critterGetStat(a2, STAT_CURRENT_HIT_POINTS);
    int maximumHp = critterGetStat(a2, STAT_MAXIMUM_HIT_POINTS);

    int hpToHeal = 0;
    int maximumHpToHeal = 0;
    int minimumHpToHeal = 0;

    if (obj == gDude) {
        if (skill == SKILL_FIRST_AID || skill == SKILL_DOCTOR) {
            int healerRank = perkGetRank(obj, PERK_HEALER);
            minimumHpToHeal = 4 * healerRank;
            maximumHpToHeal = 10 * healerRank;
        }
    }

    int criticalChance = critterGetStat(obj, STAT_CRITICAL_CHANCE) + criticalChanceModifier;

    int damageHealingAttempts = 1;
    int v1 = 0;
    int v2 = 0;

    switch (skill) {
    case SKILL_FIRST_AID:
        if (skillGetFreeUsageSlot(SKILL_FIRST_AID) == -1) {
            // 590: You've taxed your ability with that skill. Wait a while.
            // 591: You're too tired.
            // 592: The strain might kill you.
            messageListItem.num = 590 + randomBetween(0, 2);
            if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }

            return -1;
        }

        if (critterIsDead(a2)) {
            // 512: You can't heal the dead.
            // 513: Let the dead rest in peace.
            // 514: It's dead, get over it.
            messageListItem.num = 512 + randomBetween(0, 2);
            if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                debugPrint(messageListItem.text);
            }

            break;
        }

        if (currentHp < maximumHp) {
            paletteFadeTo(gPaletteBlack);

            int roll;
            if (critterGetBodyType(a2) == BODY_TYPE_ROBOTIC) {
                roll = ROLL_FAILURE;
            } else {
                roll = skillRoll(obj, skill, criticalChance, &hpToHeal);
            }

            if (roll == ROLL_SUCCESS || roll == ROLL_CRITICAL_SUCCESS) {
                hpToHeal = randomBetween(minimumHpToHeal + 1, maximumHpToHeal + 5);
                critterAdjustHitPoints(a2, hpToHeal);

                if (obj == gDude) {
                    // You heal %d hit points.
                    messageListItem.num = 500;
                    if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                        return -1;
                    }

                    if (maximumHp - currentHp < hpToHeal) {
                        hpToHeal = maximumHp - currentHp;
                    }

                    snprintf(text, sizeof(text), messageListItem.text, hpToHeal);
                    displayMonitorAddMessage(text);
                }

                a2->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;

                skillUpdateLastUse(SKILL_FIRST_AID);

                v1 = 1;

                if (a2 == gDude) {
                    interfaceRenderHitPoints(true);
                }
            } else {
                // You fail to do any healing.
                messageListItem.num = 503;
                if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                    return -1;
                }

                snprintf(text, sizeof(text), messageListItem.text, hpToHeal);
                displayMonitorAddMessage(text);
            }

            scriptsExecMapUpdateProc();
            paletteFadeTo(_cmap);
        } else {
            if (obj == gDude) {
                // 501: You look healty already
                // 502: %s looks healthy already
                messageListItem.num = (a2 == gDude ? 501 : 502);
                if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                    return -1;
                }

                if (a2 == gDude) {
                    strcpy(text, messageListItem.text);
                } else {
                    snprintf(text, sizeof(text), messageListItem.text, objectGetName(a2));
                }

                displayMonitorAddMessage(text);
                giveExp = false;
            }
        }

        if (obj == gDude) {
            gameTimeAddSeconds(1800);
        }

        break;
    case SKILL_DOCTOR:
        if (skillGetFreeUsageSlot(SKILL_DOCTOR) == -1) {
            // 590: You've taxed your ability with that skill. Wait a while.
            // 591: You're too tired.
            // 592: The strain might kill you.
            messageListItem.num = 590 + randomBetween(0, 2);
            if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }

            return -1;
        }

        if (critterIsDead(a2)) {
            // 512: You can't heal the dead.
            // 513: Let the dead rest in peace.
            // 514: It's dead, get over it.
            messageListItem.num = 512 + randomBetween(0, 2);
            if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
            break;
        }

        if (currentHp < maximumHp || critterIsCrippled(a2)) {
            paletteFadeTo(gPaletteBlack);

            if (critterGetBodyType(a2) != BODY_TYPE_ROBOTIC && critterIsCrippled(a2)) {
                int flags[HEALABLE_DAMAGE_FLAGS_LENGTH];
                memcpy(flags, gHealableDamageFlags, sizeof(gHealableDamageFlags));

                for (int index = 0; index < HEALABLE_DAMAGE_FLAGS_LENGTH; index++) {
                    if ((a2->data.critter.combat.results & flags[index]) != 0) {
                        damageHealingAttempts++;

                        int roll = skillRoll(obj, skill, criticalChance, &hpToHeal);

                        // 530: damaged eye
                        // 531: crippled left arm
                        // 532: crippled right arm
                        // 533: crippled right leg
                        // 534: crippled left leg
                        messageListItem.num = 530 + index;
                        if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                            return -1;
                        }

                        MessageListItem prefix;

                        if (roll == ROLL_SUCCESS || roll == ROLL_CRITICAL_SUCCESS) {
                            a2->data.critter.combat.results &= ~flags[index];
                            a2->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;

                            // 520: You heal your %s.
                            // 521: You heal the %s.
                            prefix.num = (a2 == gDude ? 520 : 521);

                            skillUpdateLastUse(SKILL_DOCTOR);

                            v1 = 1;
                            v2 = 1;
                        } else {
                            // 525: You fail to heal your %s.
                            // 526: You fail to heal the %s.
                            prefix.num = (a2 == gDude ? 525 : 526);
                        }

                        if (!messageListGetItem(&gSkillsMessageList, &prefix)) {
                            return -1;
                        }

                        snprintf(text, sizeof(text), prefix.text, messageListItem.text);
                        displayMonitorAddMessage(text);
                        _show_skill_use_messages(obj, skill, a2, v1, criticalChanceModifier);

                        giveExp = false;
                    }
                }
            }

            int roll;
            if (critterGetBodyType(a2) == BODY_TYPE_ROBOTIC) {
                roll = ROLL_FAILURE;
            } else {
                int skillValue = skillGetValue(obj, skill);
                roll = randomRoll(skillValue, criticalChance, &hpToHeal);
            }

            if (roll == ROLL_SUCCESS || roll == ROLL_CRITICAL_SUCCESS) {
                hpToHeal = randomBetween(minimumHpToHeal + 4, maximumHpToHeal + 10);
                critterAdjustHitPoints(a2, hpToHeal);

                if (obj == gDude) {
                    // You heal %d hit points.
                    messageListItem.num = 500;
                    if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                        return -1;
                    }

                    if (maximumHp - currentHp < hpToHeal) {
                        hpToHeal = maximumHp - currentHp;
                    }
                    snprintf(text, sizeof(text), messageListItem.text, hpToHeal);
                    displayMonitorAddMessage(text);
                }

                if (!v2) {
                    skillUpdateLastUse(SKILL_DOCTOR);
                }

                a2->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;

                if (a2 == gDude) {
                    interfaceRenderHitPoints(true);
                }

                v1 = 1;
                _show_skill_use_messages(obj, skill, a2, v1, criticalChanceModifier);
                scriptsExecMapUpdateProc();
                paletteFadeTo(_cmap);

                giveExp = false;
            } else {
                // You fail to do any healing.
                messageListItem.num = 503;
                if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                    return -1;
                }

                snprintf(text, sizeof(text), messageListItem.text, hpToHeal);
                displayMonitorAddMessage(text);

                scriptsExecMapUpdateProc();
                paletteFadeTo(_cmap);
            }
        } else {
            if (obj == gDude) {
                // 501: You look healty already
                // 502: %s looks healthy already
                messageListItem.num = (a2 == gDude ? 501 : 502);
                if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                    return -1;
                }

                if (a2 == gDude) {
                    strcpy(text, messageListItem.text);
                } else {
                    snprintf(text, sizeof(text), messageListItem.text, objectGetName(a2));
                }

                displayMonitorAddMessage(text);

                giveExp = false;
            }
        }

        if (obj == gDude) {
            gameTimeAddSeconds(3600 * damageHealingAttempts);
        }

        break;
    case SKILL_SNEAK:
    case SKILL_LOCKPICK:
        break;
    case SKILL_STEAL:
        scriptsRequestStealing(obj, a2);
        break;
    case SKILL_TRAPS:
        messageListItem.num = 551; // You fail to find any traps.
        if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }

        return -1;
    case SKILL_SCIENCE:
        messageListItem.num = 552; // You fail to learn anything.
        if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }

        return -1;
    case SKILL_REPAIR:
        if (critterGetBodyType(a2) != BODY_TYPE_ROBOTIC) {
            // You cannot repair that.
            messageListItem.num = 553;
            if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
            return -1;
        }

        if (skillGetFreeUsageSlot(SKILL_REPAIR) == -1) {
            // 590: You've taxed your ability with that skill. Wait a while.
            // 591: You're too tired.
            // 592: The strain might kill you.
            messageListItem.num = 590 + randomBetween(0, 2);
            if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
            return -1;
        }

        if (critterIsDead(a2)) {
            // You got it?
            messageListItem.num = 1101;
            if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
            break;
        }

        if (currentHp < maximumHp || critterIsCrippled(a2)) {
            int flags[REPAIRABLE_DAMAGE_FLAGS_LENGTH];
            memcpy(flags, gRepairableDamageFlags, sizeof(gRepairableDamageFlags));

            paletteFadeTo(gPaletteBlack);

            for (int index = 0; index < REPAIRABLE_DAMAGE_FLAGS_LENGTH; index++) {
                if ((a2->data.critter.combat.results & flags[index]) != 0) {
                    damageHealingAttempts++;

                    int roll = skillRoll(obj, skill, criticalChance, &hpToHeal);

                    // 530: damaged eye
                    // 531: crippled left arm
                    // 532: crippled right arm
                    // 533: crippled right leg
                    // 534: crippled left leg
                    messageListItem.num = 530 + index;
                    if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                        return -1;
                    }

                    MessageListItem prefix;

                    if (roll == ROLL_SUCCESS || roll == ROLL_CRITICAL_SUCCESS) {
                        a2->data.critter.combat.results &= ~flags[index];
                        a2->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;

                        // 520: You heal your %s.
                        // 521: You heal the %s.
                        prefix.num = (a2 == gDude ? 520 : 521);
                        skillUpdateLastUse(SKILL_REPAIR);

                        v1 = 1;
                        v2 = 1;
                    } else {
                        // 525: You fail to heal your %s.
                        // 526: You fail to heal the %s.
                        prefix.num = (a2 == gDude ? 525 : 526);
                    }

                    if (!messageListGetItem(&gSkillsMessageList, &prefix)) {
                        return -1;
                    }

                    snprintf(text, sizeof(text), prefix.text, messageListItem.text);
                    displayMonitorAddMessage(text);

                    _show_skill_use_messages(obj, skill, a2, v1, criticalChanceModifier);
                    giveExp = false;
                }
            }

            int skillValue = skillGetValue(obj, skill);
            int roll = randomRoll(skillValue, criticalChance, &hpToHeal);

            if (roll == ROLL_SUCCESS || roll == ROLL_CRITICAL_SUCCESS) {
                hpToHeal = randomBetween(minimumHpToHeal + 4, maximumHpToHeal + 10);
                critterAdjustHitPoints(a2, hpToHeal);

                if (obj == gDude) {
                    // You heal %d hit points.
                    messageListItem.num = 500;
                    if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                        return -1;
                    }

                    if (maximumHp - currentHp < hpToHeal) {
                        hpToHeal = maximumHp - currentHp;
                    }
                    snprintf(text, sizeof(text), messageListItem.text, hpToHeal);
                    displayMonitorAddMessage(text);
                }

                if (!v2) {
                    skillUpdateLastUse(SKILL_REPAIR);
                }

                a2->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;

                if (a2 == gDude) {
                    interfaceRenderHitPoints(true);
                }

                v1 = 1;
                _show_skill_use_messages(obj, skill, a2, v1, criticalChanceModifier);
                scriptsExecMapUpdateProc();
                paletteFadeTo(_cmap);

                giveExp = false;
            } else {
                // You fail to do any healing.
                messageListItem.num = 503;
                if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                    return -1;
                }

                snprintf(text, sizeof(text), messageListItem.text, hpToHeal);
                displayMonitorAddMessage(text);

                scriptsExecMapUpdateProc();
                paletteFadeTo(_cmap);
            }
        } else {
            if (obj == gDude) {
                // 501: You look healty already
                // 502: %s looks healthy already
                messageListItem.num = (a2 == gDude ? 501 : 502);
                if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
                    return -1;
                }

                snprintf(text, sizeof(text), messageListItem.text, objectGetName(a2));
                displayMonitorAddMessage(text);

                giveExp = false;
            }
        }

        if (obj == gDude) {
            gameTimeAddSeconds(1800 * damageHealingAttempts);
        }

        break;
    default:
        messageListItem.num = 510; // skill_use: invalid skill used.
        if (messageListGetItem(&gSkillsMessageList, &messageListItem)) {
            debugPrint(messageListItem.text);
        }

        return -1;
    }

    if (giveExp) {
        _show_skill_use_messages(obj, skill, a2, v1, criticalChanceModifier);
    }

    if (skill == SKILL_FIRST_AID || skill == SKILL_DOCTOR) {
        scriptsExecMapUpdateProc();
    }

    return 0;
}

// 0x4ABBE4
int skillsPerformStealing(Object* a1, Object* a2, Object* item, bool isPlanting)
{
    int howMuch;

    int stealModifier = _gStealCount;
    stealModifier--;
    stealModifier = -stealModifier;

    if (a1 != gDude || !perkHasRank(a1, PERK_PICKPOCKET)) {
        // -4% per item size
        stealModifier -= 4 * itemGetSize(item);

        if (FID_TYPE(a2->fid) == OBJ_TYPE_CRITTER) {
            // check facing: -25% if face to face
            if (_is_hit_from_front(a1, a2)) {
                stealModifier -= 25;
            }
        }
    }

    if ((a2->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0) {
        stealModifier += 20;
    }

    int stealChance = stealModifier + skillGetValue(a1, SKILL_STEAL);
    if (stealChance > 95) {
        stealChance = 95;
    }

    int stealRoll;
    if (a1 == gDude && objectIsPartyMember(a2)) {
        stealRoll = ROLL_CRITICAL_SUCCESS;
    } else {
        int criticalChance = critterGetStat(a1, STAT_CRITICAL_CHANCE);
        stealRoll = randomRoll(stealChance, criticalChance, &howMuch);
    }

    int catchRoll;
    if (stealRoll == ROLL_CRITICAL_SUCCESS) {
        catchRoll = ROLL_CRITICAL_FAILURE;
    } else if (stealRoll == ROLL_CRITICAL_FAILURE) {
        catchRoll = ROLL_SUCCESS;
    } else {
        int catchChance;
        if (PID_TYPE(a2->pid) == OBJ_TYPE_CRITTER) {
            catchChance = skillGetValue(a2, SKILL_STEAL) - stealModifier;
        } else {
            catchChance = 30 - stealModifier;
        }

        catchRoll = randomRoll(catchChance, 0, &howMuch);
    }

    MessageListItem messageListItem;
    char text[60];

    if (catchRoll != ROLL_SUCCESS && catchRoll != ROLL_CRITICAL_SUCCESS) {
        // 571: You steal the %s.
        // 573: You plant the %s.
        messageListItem.num = isPlanting ? 573 : 571;
        if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
            return -1;
        }

        snprintf(text, sizeof(text), messageListItem.text, objectGetName(item));
        displayMonitorAddMessage(text);

        return 1;
    } else {
        // 570: You're caught stealing the %s.
        // 572: You're caught planting the %s.
        messageListItem.num = isPlanting ? 572 : 570;
        if (!messageListGetItem(&gSkillsMessageList, &messageListItem)) {
            return -1;
        }

        snprintf(text, sizeof(text), messageListItem.text, objectGetName(item));
        displayMonitorAddMessage(text);

        return 0;
    }
}

// 0x4ABDEC
int skillGetGameDifficultyModifier(int skill)
{
    switch (skill) {
    case SKILL_FIRST_AID:
    case SKILL_DOCTOR:
    case SKILL_SNEAK:
    case SKILL_LOCKPICK:
    case SKILL_STEAL:
    case SKILL_TRAPS:
    case SKILL_SCIENCE:
    case SKILL_REPAIR:
    case SKILL_SPEECH:
    case SKILL_BARTER:
    case SKILL_GAMBLING:
    case SKILL_OUTDOORSMAN:
        if (1) {
            int gameDifficulty = settings.preferences.game_difficulty;

            if (gameDifficulty == GAME_DIFFICULTY_HARD) {
                return -10;
            } else if (gameDifficulty == GAME_DIFFICULTY_EASY) {
                return 20;
            }
        }
        break;
    }

    return 0;
}

// 0x4ABE44
static int skillGetFreeUsageSlot(int skill)
{
    for (int slot = 0; slot < SKILLS_MAX_USES_PER_DAY; slot++) {
        if (_timesSkillUsed[skill][slot] == 0) {
            return slot;
        }
    }

    unsigned int time = gameTimeGetTime();
    int hoursSinceLastUsage = (time - _timesSkillUsed[skill][0]) / GAME_TIME_TICKS_PER_HOUR;
    if (hoursSinceLastUsage <= 24) {
        return -1;
    }

    return SKILLS_MAX_USES_PER_DAY - 1;
}

// 0x4ABEB8
int skillUpdateLastUse(int skill)
{
    int slot = skillGetFreeUsageSlot(skill);
    if (slot == -1) {
        return -1;
    }

    if (_timesSkillUsed[skill][slot] != 0) {
        for (int i = 0; i < slot; i++) {
            _timesSkillUsed[skill][i] = _timesSkillUsed[skill][i + 1];
        }
    }

    _timesSkillUsed[skill][slot] = gameTimeGetTime();

    return 0;
}

// NOTE: Inlined.
//
// 0x4ABF24
int skill_use_slot_clear()
{
    memset(_timesSkillUsed, 0, sizeof(_timesSkillUsed));
    return 0;
}

// 0x4ABF3C
int skillsUsageSave(File* stream)
{
    return fileWriteInt32List(stream, (int*)_timesSkillUsed, SKILL_COUNT * SKILLS_MAX_USES_PER_DAY);
}

// 0x4ABF5C
int skillsUsageLoad(File* stream)
{
    return fileReadInt32List(stream, (int*)_timesSkillUsed, SKILL_COUNT * SKILLS_MAX_USES_PER_DAY);
}

// 0x4ABF7C
char* skillsGetGenericResponse(Object* critter, bool isDude)
{
    int baseMessageId;
    int count;

    if (isDude) {
        baseMessageId = 1100;
        count = 4;
    } else {
        baseMessageId = 1000;
        count = 5;
    }

    int messageId = randomBetween(0, count);

    MessageListItem messageListItem;
    char* msg = getmsg(&gSkillsMessageList, &messageListItem, baseMessageId + messageId);
    return msg;
}

} // namespace fallout
