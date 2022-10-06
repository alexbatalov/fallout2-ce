#include "perk.h"

#include <stdio.h>

#include "debug.h"
#include "game.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "party_member.h"
#include "platform_compat.h"
#include "skill.h"
#include "stat.h"

namespace fallout {

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

static PerkRankData* perkGetRankData(Object* critter);
static bool perkCanAdd(Object* critter, int perk);
static void perkResetRanks();

// 0x519DCC
static PerkDescription gPerkDescriptions[PERK_COUNT] = {
    { NULL, NULL, 72, 1, 3, -1, 0, -1, 0, 0, -1, 0, 0, 5, 0, 0, 0, 0, 0 },
    { NULL, NULL, 73, 1, 15, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 6, 0 },
    { NULL, NULL, 74, 3, 3, 11, 2, -1, 0, 0, -1, 0, 6, 0, 0, 0, 0, 6, 0 },
    { NULL, NULL, 75, 2, 6, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { NULL, NULL, 76, 2, 6, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 6, 6 },
    { NULL, NULL, 77, 1, 15, -1, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 6, 7, 0 },
    { NULL, NULL, 78, 3, 3, 13, 2, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { NULL, NULL, 79, 3, 3, 14, 2, -1, 0, 0, -1, 0, 0, 0, 6, 0, 0, 0, 0 },
    { NULL, NULL, 80, 3, 6, 15, 5, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 6 },
    { NULL, NULL, 81, 1, 3, -1, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { NULL, NULL, 82, 3, 3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 6, 0, 0, 0 },
    { NULL, NULL, 83, 2, 6, 31, 15, -1, 0, 0, -1, 0, 0, 0, 6, 0, 4, 0, 0 },
    { NULL, NULL, 84, 3, 3, 24, 10, -1, 0, 0, -1, 0, 0, 0, 6, 0, 0, 0, 6 },
    { NULL, NULL, 85, 3, 3, 12, 50, -1, 0, 0, -1, 0, 6, 0, 6, 0, 0, 0, 0 },
    { NULL, NULL, 86, 1, 9, -1, 0, -1, 0, 0, -1, 0, 0, 7, 0, 0, 6, 0, 0 },
    { NULL, NULL, 87, 1, 6, -1, 0, 8, 50, 0, -1, 0, 0, 0, 0, 0, 0, 6, 0 },
    { NULL, NULL, 88, 1, 3, -1, 0, 17, 40, 0, -1, 0, 0, 0, 6, 0, 6, 0, 0 },
    { NULL, NULL, 89, 1, 12, -1, 0, 15, 75, 0, -1, 0, 0, 0, 0, 7, 0, 0, 0 },
    { NULL, NULL, 90, 3, 6, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 6, 0, 0 },
    { NULL, NULL, 91, 2, 3, -1, 0, 6, 40, 0, -1, 0, 0, 7, 0, 0, 5, 6, 0 },
    { NULL, NULL, 92, 1, 6, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 8 },
    { NULL, NULL, 93, 1, 9, 16, 20, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 4, 6 },
    { NULL, NULL, 94, 1, 6, -1, 0, -1, 0, 0, -1, 0, 0, 7, 0, 0, 5, 0, 0 },
    { NULL, NULL, 95, 1, 24, -1, 0, 3, 80, 0, -1, 0, 8, 0, 0, 0, 0, 8, 0 },
    { NULL, NULL, 96, 1, 24, -1, 0, 0, 80, 0, -1, 0, 0, 8, 0, 0, 0, 8, 0 },
    { NULL, NULL, 97, 1, 18, -1, 0, 8, 80, 2, 3, 80, 0, 0, 0, 0, 0, 10, 0 },
    { NULL, NULL, 98, 2, 12, 8, 1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { NULL, NULL, 99, 1, 310, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 100, 2, 12, -1, 0, -1, 0, 0, -1, 0, 0, 0, 4, 0, 0, 0, 0 },
    { NULL, NULL, 101, 1, 9, 9, 5, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 6, 0 },
    { NULL, NULL, 102, 2, 6, 32, 25, -1, 0, 0, -1, 0, 0, 0, 3, 0, 0, 0, 0 },
    { NULL, NULL, 103, 1, 12, -1, 0, 13, 40, 1, 12, 40, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 104, 1, 12, -1, 0, 6, 40, 1, 7, 40, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 105, 1, 12, -1, 0, 10, 50, 2, 9, 50, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 106, 1, 9, -1, 0, 14, 50, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 107, 3, 6, -1, 0, -1, 0, 0, -1, 0, -9, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 108, 1, 310, -1, 0, -1, 0, 0, -1, 0, 0, 4, 0, 0, 0, 0, 0 },
    { NULL, NULL, 109, 1, 15, -1, 0, 10, 80, 0, -1, 0, 0, 0, 0, 0, 0, 8, 0 },
    { NULL, NULL, 110, 1, 6, -1, 0, 8, 60, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 111, 1, 12, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 10, 0, 0, 0 },
    { NULL, NULL, 112, 1, 310, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 8 },
    { NULL, NULL, 113, 1, 9, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 114, 1, 310, -1, 0, -1, 0, 0, -1, 0, 0, 0, 5, 0, 0, 0, 0 },
    { NULL, NULL, 115, 2, 6, -1, 0, 17, 40, 0, -1, 0, 0, 0, 6, 0, 0, 0, 0 },
    { NULL, NULL, 116, 1, 310, -1, 0, 17, 25, 0, -1, 0, 0, 0, 0, 0, 5, 0, 0 },
    { NULL, NULL, 117, 1, 3, -1, 0, -1, 0, 0, -1, 0, 0, 7, 0, 0, 0, 0, 0 },
    { NULL, NULL, 118, 1, 9, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 4 },
    { NULL, NULL, 119, 1, 6, -1, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { NULL, NULL, 120, 1, 3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { NULL, NULL, 121, 3, 3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 4, 0, 0 },
    { NULL, NULL, 122, 3, 3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 4, 0, 0 },
    { NULL, NULL, 123, 1, 12, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 124, 1, 9, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 125, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 126, -1, 1, -1, 0, -1, 0, 0, -1, 0, -2, 0, -2, 0, 0, -3, 0 },
    { NULL, NULL, 127, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, -3, -2, 0 },
    { NULL, NULL, 128, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, -2, 0, 0 },
    { NULL, NULL, 129, -1, 1, 31, -20, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 130, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 131, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 132, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 133, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 134, -1, 1, 31, 30, -1, 0, 0, -1, 0, 3, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 135, -1, 1, 31, 20, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 136, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 137, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 138, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 139, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 140, -1, 1, 31, 60, -1, 0, 0, -1, 0, 4, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 141, -1, 1, 31, 75, -1, 0, 0, -1, 0, 4, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 136, -1, 1, 8, -1, -1, 0, 0, -1, 0, -1, -1, 0, 0, 0, 0, 0 },
    { NULL, NULL, 149, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, -2, 0, 0, -1, 0, -1 },
    { NULL, NULL, 154, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 2, 0, 0, 0 },
    { NULL, NULL, 158, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 157, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 157, -1, 1, 3, -1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 168, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 168, -1, 1, 3, -1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 172, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 155, 1, 6, -1, 0, -1, 0, 0, -1, 0, -10, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 156, 1, 3, -1, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { NULL, NULL, 122, 1, 3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 6, 0, 0 },
    { NULL, NULL, 39, 1, 9, -1, 0, 11, 75, 0, -1, 0, 0, 0, 0, 0, 0, 4, 0 },
    { NULL, NULL, 44, 1, 6, -1, 0, 16, 50, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 0, 1, 12, -1, 0, -1, 0, 0, -1, 0, -10, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 1, 1, 12, -1, 0, -1, 0, 0, -1, 0, 0, -10, 0, 0, 0, 0, 0 },
    { NULL, NULL, 2, 1, 12, -1, 0, -1, 0, 0, -1, 0, 0, 0, -10, 0, 0, 0, 0 },
    { NULL, NULL, 3, 1, 12, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, -10, 0, 0, 0 },
    { NULL, NULL, 4, 1, 12, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, -10, 0, 0 },
    { NULL, NULL, 5, 1, 12, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, -10, 0 },
    { NULL, NULL, 6, 1, 12, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -10 },
    { NULL, NULL, 160, 1, 6, -1, 0, 10, 50, 2, 0x4000000, 50, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 161, 1, 3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 159, 1, 12, -1, 0, 3, 75, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 163, 1, 3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 5, 0, 0, 5, 0 },
    { NULL, NULL, 162, 1, 9, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 6, 0, 0, 0 },
    { NULL, NULL, 164, 1, 9, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 5 },
    { NULL, NULL, 165, 1, 12, -1, 0, 7, 60, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 166, 1, 6, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, -10, 0, 0, 0 },
    { NULL, NULL, 43, 1, 6, -1, 0, 15, 50, 2, 14, 50, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 167, 1, 6, 12, 50, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 169, 1, 9, -1, 0, 1, 75, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 170, 1, 6, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { NULL, NULL, 121, 1, 6, -1, 0, 15, 50, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 171, 1, 3, -1, 0, -1, 0, 0, -1, 0, 6, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 38, 1, 3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 173, 1, 12, -1, 0, -1, 0, 0, -1, 0, -7, 0, 0, 0, 0, 5, 0 },
    { NULL, NULL, 104, -1, 1, -1, 0, 7, 75, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 142, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 142, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 52, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 52, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 104, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 104, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 35, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 35, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 154, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 154, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, NULL, 64, -1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
};

// An array of perk ranks for each party member.
//
// 0x51C120
static PerkRankData* gPartyMemberPerkRanks = NULL;

// Amount of experience points granted when player selected "Here and now"
// perk.
//
// 0x51C124
static int gHereAndNowBonusExperience = 0;

// perk.msg
//
// 0x6642D4
static MessageList gPerksMessageList;

// 0x4965A0
int perksInit()
{
    gPartyMemberPerkRanks = (PerkRankData*)internal_malloc(sizeof(*gPartyMemberPerkRanks) * gPartyMemberDescriptionsLength);
    if (gPartyMemberPerkRanks == NULL) {
        return -1;
    }

    perkResetRanks();

    if (!messageListInit(&gPerksMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s%s", asc_5186C8, "perk.msg");

    if (!messageListLoad(&gPerksMessageList, path)) {
        return -1;
    }

    for (int perk = 0; perk < PERK_COUNT; perk++) {
        MessageListItem messageListItem;

        messageListItem.num = 101 + perk;
        if (messageListGetItem(&gPerksMessageList, &messageListItem)) {
            gPerkDescriptions[perk].name = messageListItem.text;
        }

        messageListItem.num = 1101 + perk;
        if (messageListGetItem(&gPerksMessageList, &messageListItem)) {
            gPerkDescriptions[perk].description = messageListItem.text;
        }
    }

    return 0;
}

// 0x4966B0
void perksReset()
{
    perkResetRanks();
}

// 0x4966B8
void perksExit()
{
    messageListFree(&gPerksMessageList);

    if (gPartyMemberPerkRanks != NULL) {
        internal_free(gPartyMemberPerkRanks);
        gPartyMemberPerkRanks = NULL;
    }
}

// 0x4966E4
int perksLoad(File* stream)
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        PerkRankData* ranksData = &(gPartyMemberPerkRanks[index]);
        for (int perk = 0; perk < PERK_COUNT; perk++) {
            if (fileReadInt32(stream, &(ranksData->ranks[perk])) == -1) {
                return -1;
            }
        }
    }

    return 0;
}

// 0x496738
int perksSave(File* stream)
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        PerkRankData* ranksData = &(gPartyMemberPerkRanks[index]);
        for (int perk = 0; perk < PERK_COUNT; perk++) {
            if (fileWriteInt32(stream, ranksData->ranks[perk]) == -1) {
                return -1;
            }
        }
    }

    return 0;
}

// perkGetLevelData
// 0x49678C
static PerkRankData* perkGetRankData(Object* critter)
{
    if (critter == gDude) {
        return gPartyMemberPerkRanks;
    }

    for (int index = 1; index < gPartyMemberDescriptionsLength; index++) {
        if (critter->pid == gPartyMemberPids[index]) {
            return gPartyMemberPerkRanks + index;
        }
    }

    debugPrint("\nError: perkGetLevelData: Can't find party member match!");

    return gPartyMemberPerkRanks;
}

// 0x49680C
static bool perkCanAdd(Object* critter, int perk)
{
    if (!perkIsValid(perk)) {
        return false;
    }

    PerkDescription* perkDescription = &(gPerkDescriptions[perk]);

    if (perkDescription->maxRank == -1) {
        return false;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    if (ranksData->ranks[perk] >= perkDescription->maxRank) {
        return false;
    }

    if (critter == gDude) {
        if (pcGetStat(PC_STAT_LEVEL) < perkDescription->minLevel) {
            return false;
        }
    }

    bool v1 = true;

    int param1 = perkDescription->param1;
    if (param1 != -1) {
        bool isVariable = false;
        if ((param1 & 0x4000000) != 0) {
            isVariable = true;
            param1 &= ~0x4000000;
        }

        int value1 = perkDescription->value1;
        if (value1 < 0) {
            if (isVariable) {
                if (gameGetGlobalVar(param1) >= value1) {
                    v1 = false;
                }
            } else {
                if (skillGetValue(critter, param1) >= -value1) {
                    v1 = false;
                }
            }
        } else {
            if (isVariable) {
                if (gameGetGlobalVar(param1) < value1) {
                    v1 = false;
                }
            } else {
                if (skillGetValue(critter, param1) < value1) {
                    v1 = false;
                }
            }
        }
    }

    if (!v1 || perkDescription->field_24 == 2) {
        if (perkDescription->field_24 == 0) {
            return false;
        }

        if (!v1 && perkDescription->field_24 == 2) {
            return false;
        }

        int param2 = perkDescription->param2;
        bool isVariable = false;
        if (param2 != -1) {
            if ((param2 & 0x4000000) != 0) {
                isVariable = true;
                param2 &= ~0x4000000;
            }
        }

        if (param2 == -1) {
            return false;
        }

        int value2 = perkDescription->value2;
        if (value2 < 0) {
            if (isVariable) {
                if (gameGetGlobalVar(param2) >= value2) {
                    return false;
                }
            } else {
                if (skillGetValue(critter, param2) >= -value2) {
                    return false;
                }
            }
        } else {
            if (isVariable) {
                if (gameGetGlobalVar(param2) < value2) {
                    return false;
                }
            } else {
                if (skillGetValue(critter, param2) < value2) {
                    return false;
                }
            }
        }
    }

    for (int stat = 0; stat < PRIMARY_STAT_COUNT; stat++) {
        if (perkDescription->stats[stat] < 0) {
            if (critterGetStat(critter, stat) >= -perkDescription->stats[stat]) {
                return false;
            }
        } else {
            if (critterGetStat(critter, stat) < perkDescription->stats[stat]) {
                return false;
            }
        }
    }

    return true;
}

// Resets party member perks.
//
// 0x496A0C
static void perkResetRanks()
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        PerkRankData* ranksData = &(gPartyMemberPerkRanks[index]);
        for (int perk = 0; perk < PERK_COUNT; perk++) {
            ranksData->ranks[perk] = 0;
        }
    }
}

// 0x496A5C
int perkAdd(Object* critter, int perk)
{
    if (!perkIsValid(perk)) {
        return -1;
    }

    if (!perkCanAdd(critter, perk)) {
        return -1;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    ranksData->ranks[perk] += 1;

    perkAddEffect(critter, perk);

    return 0;
}

// perk_add_force
// 0x496A9C
int perkAddForce(Object* critter, int perk)
{
    if (!perkIsValid(perk)) {
        return -1;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    int value = ranksData->ranks[perk];

    int maxRank = gPerkDescriptions[perk].maxRank;

    if (maxRank != -1 && value >= maxRank) {
        return -1;
    }

    ranksData->ranks[perk] += 1;

    perkAddEffect(critter, perk);

    return 0;
}

// perk_sub
// 0x496AFC
int perkRemove(Object* critter, int perk)
{
    if (!perkIsValid(perk)) {
        return -1;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    int value = ranksData->ranks[perk];

    if (value < 1) {
        return -1;
    }

    ranksData->ranks[perk] -= 1;

    perkRemoveEffect(critter, perk);

    return 0;
}

// Returns perks available to pick.
//
// 0x496B44
int perkGetAvailablePerks(Object* critter, int* perks)
{
    int count = 0;
    for (int perk = 0; perk < PERK_COUNT; perk++) {
        if (perkCanAdd(critter, perk)) {
            perks[count] = perk;
            count++;
        }
    }
    return count;
}

// has_perk
// 0x496B78
int perkGetRank(Object* critter, int perk)
{
    if (!perkIsValid(perk)) {
        return 0;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    return ranksData->ranks[perk];
}

// 0x496B90
char* perkGetName(int perk)
{
    if (!perkIsValid(perk)) {
        return NULL;
    }
    return gPerkDescriptions[perk].name;
}

// 0x496BB4
char* perkGetDescription(int perk)
{
    if (!perkIsValid(perk)) {
        return NULL;
    }
    return gPerkDescriptions[perk].description;
}

// 0x496BD8
int perkGetFrmId(int perk)
{
    if (!perkIsValid(perk)) {
        return 0;
    }
    return gPerkDescriptions[perk].frmId;
}

// perk_add_effect
// 0x496BFC
void perkAddEffect(Object* critter, int perk)
{
    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        debugPrint("\nERROR: perk_add_effect: Was called on non-critter!");
        return;
    }

    if (!perkIsValid(perk)) {
        return;
    }

    PerkDescription* perkDescription = &(gPerkDescriptions[perk]);

    if (perkDescription->stat != -1) {
        int value = critterGetBonusStat(critter, perkDescription->stat);
        critterSetBonusStat(critter, perkDescription->stat, value + perkDescription->statModifier);
    }

    if (perk == PERK_HERE_AND_NOW) {
        PerkRankData* ranksData = perkGetRankData(critter);
        ranksData->ranks[PERK_HERE_AND_NOW] -= 1;

        int level = pcGetStat(PC_STAT_LEVEL);

        gHereAndNowBonusExperience = pcGetExperienceForLevel(level + 1) - pcGetStat(PC_STAT_EXPERIENCE);
        pcAddExperienceWithOptions(gHereAndNowBonusExperience, false);

        ranksData->ranks[PERK_HERE_AND_NOW] += 1;
    }

    if (perkDescription->maxRank == -1) {
        for (int stat = 0; stat < PRIMARY_STAT_COUNT; stat++) {
            int value = critterGetBonusStat(critter, stat);
            critterSetBonusStat(critter, stat, value + perkDescription->stats[stat]);
        }
    }
}

// perk_remove_effect
// 0x496CE0
void perkRemoveEffect(Object* critter, int perk)
{
    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        debugPrint("\nERROR: perk_remove_effect: Was called on non-critter!");
        return;
    }

    if (!perkIsValid(perk)) {
        return;
    }

    PerkDescription* perkDescription = &(gPerkDescriptions[perk]);

    if (perkDescription->stat != -1) {
        int value = critterGetBonusStat(critter, perkDescription->stat);
        critterSetBonusStat(critter, perkDescription->stat, value - perkDescription->statModifier);
    }

    if (perk == PERK_HERE_AND_NOW) {
        int xp = pcGetStat(PC_STAT_EXPERIENCE);
        pcSetStat(PC_STAT_EXPERIENCE, xp - gHereAndNowBonusExperience);
    }

    if (perkDescription->maxRank == -1) {
        for (int stat = 0; stat < PRIMARY_STAT_COUNT; stat++) {
            int value = critterGetBonusStat(critter, stat);
            critterSetBonusStat(critter, stat, value - perkDescription->stats[stat]);
        }
    }
}

// Returns modifier to specified skill accounting for perks.
//
// 0x496DD0
int perkGetSkillModifier(Object* critter, int skill)
{
    int modifier = 0;

    switch (skill) {
    case SKILL_FIRST_AID:
        if (perkHasRank(critter, PERK_MEDIC)) {
            modifier += 10;
        }

        if (perkHasRank(critter, PERK_VAULT_CITY_TRAINING)) {
            modifier += 5;
        }

        break;
    case SKILL_DOCTOR:
        if (perkHasRank(critter, PERK_MEDIC)) {
            modifier += 10;
        }

        if (perkHasRank(critter, PERK_LIVING_ANATOMY)) {
            modifier += 10;
        }

        if (perkHasRank(critter, PERK_VAULT_CITY_TRAINING)) {
            modifier += 5;
        }

        break;
    case SKILL_SNEAK:
        if (perkHasRank(critter, PERK_GHOST)) {
            int lightIntensity = objectGetLightIntensity(gDude);
            if (lightIntensity > 45875) {
                modifier += 20;
            }
        }
        // FALLTHROUGH
    case SKILL_LOCKPICK:
    case SKILL_STEAL:
    case SKILL_TRAPS:
        if (perkHasRank(critter, PERK_THIEF)) {
            modifier += 10;
        }

        if (skill == SKILL_LOCKPICK || skill == SKILL_STEAL) {
            if (perkHasRank(critter, PERK_MASTER_THIEF)) {
                modifier += 15;
            }
        }

        if (skill == SKILL_STEAL) {
            if (perkHasRank(critter, PERK_HARMLESS)) {
                modifier += 20;
            }
        }

        break;
    case SKILL_SCIENCE:
    case SKILL_REPAIR:
        if (perkHasRank(critter, PERK_MR_FIXIT)) {
            modifier += 10;
        }

        break;
    case SKILL_SPEECH:
        if (perkHasRank(critter, PERK_SPEAKER)) {
            modifier += 20;
        }

        if (perkHasRank(critter, PERK_EXPERT_EXCREMENT_EXPEDITOR)) {
            modifier += 5;
        }

        // FALLTHROUGH
    case SKILL_BARTER:
        if (perkHasRank(critter, PERK_NEGOTIATOR)) {
            modifier += 10;
        }

        if (skill == SKILL_BARTER) {
            if (perkHasRank(critter, PERK_SALESMAN)) {
                modifier += 20;
            }
        }

        break;
    case SKILL_GAMBLING:
        if (perkHasRank(critter, PERK_GAMBLER)) {
            modifier += 20;
        }

        break;
    case SKILL_OUTDOORSMAN:
        if (perkHasRank(critter, PERK_RANGER)) {
            modifier += 15;
        }

        if (perkHasRank(critter, PERK_SURVIVALIST)) {
            modifier += 25;
        }

        break;
    }

    return modifier;
}

} // namespace fallout
