#include "combat_ai.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "combat.h"
#include "config.h"
#include "critter.h"
#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "interface.h"
#include "item.h"
#include "light.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "party_member.h"
#include "platform_compat.h"
#include "proto.h"
#include "proto_instance.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "text_object.h"
#include "tile.h"

namespace fallout {

#define AI_PACKET_CHEM_PRIMARY_DESIRE_COUNT (3)

#define AI_MESSAGE_SIZE 260

static constexpr int kChemUseStimsWhenHurtLittleHpRatio = 60;
static constexpr int kChemUseStimsWhenHurtLotsHpRatio = 30;
static constexpr int kChemUseStimsHpRatio = 50;

static constexpr int kChemUseSometimesChance = 25;
static constexpr int kChemUseAnytimeChance = 75;
static constexpr int kChemUseAlwaysChance = 100;

static constexpr int kRandomDrugPickingArraySize = 3;

typedef struct AiMessageRange {
    int start;
    int end;
} AiMessageRange;

typedef struct AiPacket {
    char* name;
    int packet_num;
    int max_dist;
    int min_to_hit;
    int min_hp;
    int aggression;
    int hurt_too_much;
    int secondary_freq;
    int called_freq;
    int font;
    int color;
    int outline_color;
    int chance;
    AiMessageRange run;
    AiMessageRange move;
    AiMessageRange attack;
    AiMessageRange miss;
    AiMessageRange hit[HIT_LOCATION_SPECIFIC_COUNT];
    int area_attack_mode;
    int run_away_mode;
    int best_weapon;
    int distance;
    int attack_who;
    int chem_use;
    int chem_primary_desire[AI_PACKET_CHEM_PRIMARY_DESIRE_COUNT];
    int disposition;
    char* body_type;
    char* general_type;
} AiPacket;

typedef struct AiRetargetData {
    Object* source;
    Object* target;
    Object* critterList[100];
    int ratingList[100];
    int critterCount;
    int sourceTeam;
    int sourceRating;
    bool notSameTile;
    int* tiles;
    int currentTileIndex;
    int sourceIntelligence;
} AiRetargetData;

static void _parse_hurt_str(char* str, int* out_value);
static int _cai_match_str_to_list(const char* str, const char** list, int count, int* out_value);
static void aiPacketInit(AiPacket* ai);
static int aiPacketRead(File* stream, AiPacket* ai);
static int aiPacketWrite(File* stream, AiPacket* ai);
static AiPacket* aiGetPacket(Object* obj);
static AiPacket* aiGetPacketByNum(int aiPacketNum);
static int _ai_magic_hands(Object* a1, Object* a2, int num);
static int _ai_check_drugs(Object* critter);
static void _ai_run_away(Object* a1, Object* a2);
static int _ai_move_away(Object* a1, Object* a2, int a3);
static bool _ai_find_friend(Object* a1, int a2, int a3);
static int _compare_nearer(const void* a1, const void* a2);
static void _ai_sort_list_distance(Object** critterList, int length, Object* origin);
static int _compare_strength(const void* a1, const void* a2);
static void _ai_sort_list_strength(Object** critterList, int length);
static int _compare_weakness(const void* a1, const void* a2);
static void _ai_sort_list_weakness(Object** critterList, int length);
static Object* _ai_find_nearest_team(Object* a1, Object* a2, int flags);
static Object* _ai_find_nearest_team_in_combat(Object* a1, Object* a2, int flags);
static int aiFindAttackers(Object* critter, Object** whoHitMePtr, Object** whoHitFriendPtr, Object** whoHitByFriendPtr);
static Object* _ai_danger_source(Object* a1);
static bool aiHaveAmmo(Object* critter, Object* weapon, Object** ammoPtr);
static bool _caiHasWeapPrefType(AiPacket* ai, int attackType);
static Object* _ai_best_weapon(Object* a1, Object* a2, Object* a3, Object* a4);
static bool _ai_can_use_weapon(Object* critter, Object* weapon, int hitMode);
static bool aiCanUseItem(Object* obj, Object* a2);
static Object* _ai_search_environ(Object* critter, int itemType);
static Object* _ai_retrieve_object(Object* critter, Object* item);
static int _ai_pick_hit_mode(Object* attacker, Object* weapon, Object* defender);
static int _ai_move_steps_closer(Object* a1, Object* a2, int actionPoints, bool taunt);
static int _ai_move_closer(Object* a1, Object* a2, bool taunt);
static int _cai_retargetTileFromFriendlyFire(Object* source, Object* target, int* tilePtr);
static int _cai_retargetTileFromFriendlyFireSubFunc(AiRetargetData* aiRetargetData, int tile);
static bool _cai_attackWouldIntersect(Object* attacker, Object* defender, Object* attackerFriend, int tile, int* distance);
static int _ai_switch_weapons(Object* a1, int* hitMode, Object** weapon, Object* a4);
static int _ai_called_shot(Object* attacker, Object* defender, int hitMode);
static int _ai_attack(Object* attacker, Object* defender, int hitMode);
static int _ai_try_attack(Object* a1, Object* a2);
static int _cai_get_min_hp(AiPacket* ai);
static int _ai_print_msg(Object* critter, int type);
static int _combatai_rating(Object* obj);
static int aiMessageListInit();
static int aiMessageListFree();

// 0x51805C
static Object* _combat_obj = nullptr;

// 0x518060
static int gAiPacketsLength = 0;

// 0x518064
static AiPacket* gAiPackets = nullptr;

// 0x518068
static bool gAiInitialized = false;

// 0x51806C
const char* gAreaAttackModeKeys[AREA_ATTACK_MODE_COUNT] = {
    "always",
    "sometimes",
    "be_sure",
    "be_careful",
    "be_absolutely_sure",
};

// 0x5180D0
const char* gAttackWhoKeys[ATTACK_WHO_COUNT] = {
    "whomever_attacking_me",
    "strongest",
    "weakest",
    "whomever",
    "closest",
};

// 0x51809C
const char* gBestWeaponKeys[BEST_WEAPON_COUNT] = {
    "no_pref",
    "melee",
    "melee_over_ranged",
    "ranged_over_melee",
    "ranged",
    "unarmed",
    "unarmed_over_thrown",
    "random",
};

// 0x5180E4
const char* gChemUseKeys[CHEM_USE_COUNT] = {
    "clean",
    "stims_when_hurt_little",
    "stims_when_hurt_lots",
    "sometimes",
    "anytime",
    "always",
};

// 0x5180BC
const char* gDistanceModeKeys[DISTANCE_COUNT] = {
    "stay_close",
    "charge",
    "snipe",
    "on_your_own",
    "stay",
};

// 0x518080
const char* gRunAwayModeKeys[RUN_AWAY_MODE_COUNT] = {
    "none",
    "coward",
    "finger_hurts",
    "bleeding",
    "not_feeling_good",
    "tourniquet",
    "never",
};

// 0x5180FC
const char* gDispositionKeys[DISPOSITION_COUNT] = {
    "none",
    "custom",
    "coward",
    "defensive",
    "aggressive",
    "berserk",
};

// 0x518114
const char* gHurtTooMuchKeys[HURT_COUNT] = {
    "blind",
    "crippled",
    "crippled_legs",
    "crippled_arms",
};

// hurt_too_much
//
// 0x518124
static const int _rmatchHurtVals[5] = {
    DAM_BLIND,
    DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT | DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT,
    DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT,
    DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT,
    0,
};

// Hit points in percent to choose run away mode.
//
// 0x518138
static const int _hp_run_away_value[6] = {
    0,
    25,
    40,
    60,
    75,
    100,
};

// 0x518150
static Object* _attackerTeamObj = nullptr;

// 0x518154
static Object* _targetTeamObj = nullptr;

// 0x518158
static const int _weapPrefOrderings[BEST_WEAPON_COUNT + 1][ATTACK_TYPE_COUNT] = {
    { ATTACK_TYPE_RANGED, ATTACK_TYPE_THROW, ATTACK_TYPE_MELEE, ATTACK_TYPE_UNARMED, 0 },
    { ATTACK_TYPE_RANGED, ATTACK_TYPE_THROW, ATTACK_TYPE_MELEE, ATTACK_TYPE_UNARMED, 0 }, // BEST_WEAPON_NO_PREF
    { ATTACK_TYPE_MELEE, 0, 0, 0, 0 }, // BEST_WEAPON_MELEE
    { ATTACK_TYPE_MELEE, ATTACK_TYPE_RANGED, 0, 0, 0 }, // BEST_WEAPON_MELEE_OVER_RANGED
    { ATTACK_TYPE_RANGED, ATTACK_TYPE_MELEE, 0, 0, 0 }, // BEST_WEAPON_RANGED_OVER_MELEE
    { ATTACK_TYPE_RANGED, 0, 0, 0, 0 }, // BEST_WEAPON_RANGED
    { ATTACK_TYPE_UNARMED, 0, 0, 0, 0 }, // BEST_WEAPON_UNARMED
    { ATTACK_TYPE_UNARMED, ATTACK_TYPE_THROW, 0, 0, 0 }, // BEST_WEAPON_UNARMED_OVER_THROW
    { 0, 0, 0, 0, 0 }, // BEST_WEAPON_RANDOM
};

// 0x518220
static int gLanguageFilter = -1;

// ai.msg
//
// 0x56D510
static MessageList gCombatAiMessageList;

// 0x56D518
static char _target_str[AI_MESSAGE_SIZE];

// 0x56D61C
static int _curr_crit_num;

// 0x56D620
static Object** _curr_crit_list;

// 0x56D624
static char _attack_str[AI_MESSAGE_SIZE];

// parse hurt_too_much
static void _parse_hurt_str(char* str, int* valuePtr)
{
    *valuePtr = 0;

    str = compat_strlwr(str);
    while (*str) {
        str += strspn(str, " ");

        size_t delimeterPos = strcspn(str, ",");
        char tmp = str[delimeterPos];
        str[delimeterPos] = '\0';

        int index;
        for (index = 0; index < HURT_COUNT; index++) {
            if (strcmp(str, gHurtTooMuchKeys[index]) == 0) {
                *valuePtr |= _rmatchHurtVals[index];
                break;
            }
        }

        if (index == HURT_COUNT) {
            debugPrint("Unrecognized flag: %s\n", str);
        }

        str[delimeterPos] = tmp;

        if (tmp == '\0') {
            break;
        }

        str += delimeterPos + 1;
    }
}

// parse behaviour entry
static int _cai_match_str_to_list(const char* str, const char** list, int count, int* valuePtr)
{
    *valuePtr = -1;
    for (int index = 0; index < count; index++) {
        if (compat_stricmp(str, list[index]) == 0) {
            *valuePtr = index;
        }
    }

    return 0;
}

// 0x426FE0
static void aiPacketInit(AiPacket* ai)
{
    ai->name = nullptr;

    ai->area_attack_mode = -1;
    ai->run_away_mode = -1;
    ai->best_weapon = -1;
    ai->distance = -1;
    ai->attack_who = -1;
    ai->chem_use = -1;

    for (int index = 0; index < AI_PACKET_CHEM_PRIMARY_DESIRE_COUNT; index++) {
        ai->chem_primary_desire[index] = -1;
    }

    ai->disposition = -1;
}

// ai_init
// 0x42703C
int aiInit()
{
    int index;

    if (aiMessageListInit() == -1) {
        return -1;
    }

    gAiPacketsLength = 0;

    Config config;
    if (!configInit(&config)) {
        return -1;
    }

    if (!configRead(&config, "data\\ai.txt", true)) {
        return -1;
    }

    gAiPackets = (AiPacket*)internal_malloc(sizeof(*gAiPackets) * config.entriesLength);
    if (gAiPackets == nullptr) {
        goto err;
    }

    for (index = 0; index < config.entriesLength; index++) {
        aiPacketInit(&(gAiPackets[index]));
    }

    for (index = 0; index < config.entriesLength; index++) {
        DictionaryEntry* sectionEntry = &(config.entries[index]);
        AiPacket* ai = &(gAiPackets[index]);
        char* stringValue;

        ai->name = internal_strdup(sectionEntry->key);

        if (!configGetInt(&config, sectionEntry->key, "packet_num", &(ai->packet_num))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "max_dist", &(ai->max_dist))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "min_to_hit", &(ai->min_to_hit))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "min_hp", &(ai->min_hp))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "aggression", &(ai->aggression))) goto err;

        if (configGetString(&config, sectionEntry->key, "hurt_too_much", &stringValue)) {
            _parse_hurt_str(stringValue, &(ai->hurt_too_much));
        }

        if (!configGetInt(&config, sectionEntry->key, "secondary_freq", &(ai->secondary_freq))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "called_freq", &(ai->called_freq))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "font", &(ai->font))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "color", &(ai->color))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "outline_color", &(ai->outline_color))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "chance", &(ai->chance))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "run_start", &(ai->run.start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "run_end", &(ai->run.end))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "move_start", &(ai->move.start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "move_end", &(ai->move.end))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "attack_start", &(ai->attack.start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "attack_end", &(ai->attack.end))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "miss_start", &(ai->miss.start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "miss_end", &(ai->miss.end))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_head_start", &(ai->hit[HIT_LOCATION_HEAD].start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_head_end", &(ai->hit[HIT_LOCATION_HEAD].end))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_left_arm_start", &(ai->hit[HIT_LOCATION_LEFT_ARM].start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_left_arm_end", &(ai->hit[HIT_LOCATION_LEFT_ARM].end))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_right_arm_start", &(ai->hit[HIT_LOCATION_RIGHT_ARM].start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_right_arm_end", &(ai->hit[HIT_LOCATION_RIGHT_ARM].end))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_torso_start", &(ai->hit[HIT_LOCATION_TORSO].start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_torso_end", &(ai->hit[HIT_LOCATION_TORSO].end))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_right_leg_start", &(ai->hit[HIT_LOCATION_RIGHT_LEG].start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_right_leg_end", &(ai->hit[HIT_LOCATION_RIGHT_LEG].end))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_left_leg_start", &(ai->hit[HIT_LOCATION_LEFT_LEG].start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_left_leg_end", &(ai->hit[HIT_LOCATION_LEFT_LEG].end))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_eyes_start", &(ai->hit[HIT_LOCATION_EYES].start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_eyes_end", &(ai->hit[HIT_LOCATION_EYES].end))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_groin_start", &(ai->hit[HIT_LOCATION_GROIN].start))) goto err;
        if (!configGetInt(&config, sectionEntry->key, "hit_groin_end", &(ai->hit[HIT_LOCATION_GROIN].end))) goto err;

        ai->hit[HIT_LOCATION_GROIN].end++;

        if (configGetString(&config, sectionEntry->key, "area_attack_mode", &stringValue)) {
            _cai_match_str_to_list(stringValue, gAreaAttackModeKeys, AREA_ATTACK_MODE_COUNT, &(ai->area_attack_mode));
        } else {
            ai->area_attack_mode = -1;
        }

        if (configGetString(&config, sectionEntry->key, "run_away_mode", &stringValue)) {
            _cai_match_str_to_list(stringValue, gRunAwayModeKeys, RUN_AWAY_MODE_COUNT, &(ai->run_away_mode));

            if (ai->run_away_mode >= 0) {
                ai->run_away_mode--;
            }
        }

        if (configGetString(&config, sectionEntry->key, "best_weapon", &stringValue)) {
            _cai_match_str_to_list(stringValue, gBestWeaponKeys, BEST_WEAPON_COUNT, &(ai->best_weapon));
        }

        if (configGetString(&config, sectionEntry->key, "distance", &stringValue)) {
            _cai_match_str_to_list(stringValue, gDistanceModeKeys, DISTANCE_COUNT, &(ai->distance));
        }

        if (configGetString(&config, sectionEntry->key, "attack_who", &stringValue)) {
            _cai_match_str_to_list(stringValue, gAttackWhoKeys, ATTACK_WHO_COUNT, &(ai->attack_who));
        }

        if (configGetString(&config, sectionEntry->key, "chem_use", &stringValue)) {
            _cai_match_str_to_list(stringValue, gChemUseKeys, CHEM_USE_COUNT, &(ai->chem_use));
        }

        configGetIntList(&config, sectionEntry->key, "chem_primary_desire", ai->chem_primary_desire, AI_PACKET_CHEM_PRIMARY_DESIRE_COUNT);

        if (configGetString(&config, sectionEntry->key, "disposition", &stringValue)) {
            _cai_match_str_to_list(stringValue, gDispositionKeys, DISPOSITION_COUNT, &(ai->disposition));
            ai->disposition--;
        }

        if (configGetString(&config, sectionEntry->key, "body_type", &stringValue)) {
            ai->body_type = internal_strdup(stringValue);
        } else {
            ai->body_type = nullptr;
        }

        if (configGetString(&config, sectionEntry->key, "general_type", &stringValue)) {
            ai->general_type = internal_strdup(stringValue);
        } else {
            ai->general_type = nullptr;
        }
    }

    if (index < config.entriesLength) {
        goto err;
    }

    gAiPacketsLength = config.entriesLength;

    configFree(&config);

    gAiInitialized = true;

    return 0;

err:

    if (gAiPackets != nullptr) {
        for (index = 0; index < config.entriesLength; index++) {
            AiPacket* ai = &(gAiPackets[index]);
            if (ai->name != nullptr) {
                internal_free(ai->name);
            }

            // FIXME: leaking ai->body_type and ai->general_type, does not matter
            // because it halts further processing
        }
        internal_free(gAiPackets);
    }

    debugPrint("Error processing ai.txt");

    configFree(&config);

    return -1;
}

// 0x4279F8
void aiReset()
{
}

// 0x4279FC
int aiExit()
{
    for (int index = 0; index < gAiPacketsLength; index++) {
        AiPacket* ai = &(gAiPackets[index]);

        if (ai->name != nullptr) {
            internal_free(ai->name);
            ai->name = nullptr;
        }

        if (ai->general_type != nullptr) {
            internal_free(ai->general_type);
            ai->general_type = nullptr;
        }

        if (ai->body_type != nullptr) {
            internal_free(ai->body_type);
            ai->body_type = nullptr;
        }
    }

    internal_free(gAiPackets);
    gAiPacketsLength = 0;

    gAiInitialized = false;

    // NOTE: Uninline.
    if (aiMessageListFree() != 0) {
        return -1;
    }

    return 0;
}

// 0x427AD8
int aiLoad(File* stream)
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        int pid = gPartyMemberPids[index];
        if (pid != -1 && PID_TYPE(pid) == OBJ_TYPE_CRITTER) {
            Proto* proto;
            if (protoGetProto(pid, &proto) == -1) {
                return -1;
            }

            AiPacket* ai = aiGetPacketByNum(proto->critter.aiPacket);
            if (ai->disposition == 0) {
                aiPacketRead(stream, ai);
            }
        }
    }

    return 0;
}

// 0x427B50
int aiSave(File* stream)
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        int pid = gPartyMemberPids[index];
        if (pid != -1 && PID_TYPE(pid) == OBJ_TYPE_CRITTER) {
            Proto* proto;
            if (protoGetProto(pid, &proto) == -1) {
                return -1;
            }

            AiPacket* ai = aiGetPacketByNum(proto->critter.aiPacket);
            if (ai->disposition == 0) {
                aiPacketWrite(stream, ai);
            }
        }
    }

    return 0;
}

// 0x427BC8
static int aiPacketRead(File* stream, AiPacket* ai)
{
    if (fileReadInt32(stream, &(ai->packet_num)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->max_dist)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->min_to_hit)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->min_hp)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->aggression)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->hurt_too_much)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->secondary_freq)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->called_freq)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->font)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->color)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->outline_color)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->chance)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->run.start)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->run.end)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->move.start)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->move.end)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->attack.start)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->attack.end)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->miss.start)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->miss.end)) == -1) return -1;

    for (int index = 0; index < HIT_LOCATION_SPECIFIC_COUNT; index++) {
        AiMessageRange* range = &(ai->hit[index]);
        if (fileReadInt32(stream, &(range->start)) == -1) return -1;
        if (fileReadInt32(stream, &(range->end)) == -1) return -1;
    }

    if (fileReadInt32(stream, &(ai->area_attack_mode)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->best_weapon)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->distance)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->attack_who)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->chem_use)) == -1) return -1;
    if (fileReadInt32(stream, &(ai->run_away_mode)) == -1) return -1;

    for (int index = 0; index < AI_PACKET_CHEM_PRIMARY_DESIRE_COUNT; index++) {
        if (fileReadInt32(stream, &(ai->chem_primary_desire[index])) == -1) return -1;
    }

    return 0;
}

// 0x427E1C
static int aiPacketWrite(File* stream, AiPacket* ai)
{
    if (fileWriteInt32(stream, ai->packet_num) == -1) return -1;
    if (fileWriteInt32(stream, ai->max_dist) == -1) return -1;
    if (fileWriteInt32(stream, ai->min_to_hit) == -1) return -1;
    if (fileWriteInt32(stream, ai->min_hp) == -1) return -1;
    if (fileWriteInt32(stream, ai->aggression) == -1) return -1;
    if (fileWriteInt32(stream, ai->hurt_too_much) == -1) return -1;
    if (fileWriteInt32(stream, ai->secondary_freq) == -1) return -1;
    if (fileWriteInt32(stream, ai->called_freq) == -1) return -1;
    if (fileWriteInt32(stream, ai->font) == -1) return -1;
    if (fileWriteInt32(stream, ai->color) == -1) return -1;
    if (fileWriteInt32(stream, ai->outline_color) == -1) return -1;
    if (fileWriteInt32(stream, ai->chance) == -1) return -1;
    if (fileWriteInt32(stream, ai->run.start) == -1) return -1;
    if (fileWriteInt32(stream, ai->run.end) == -1) return -1;
    if (fileWriteInt32(stream, ai->move.start) == -1) return -1;
    if (fileWriteInt32(stream, ai->move.end) == -1) return -1;
    if (fileWriteInt32(stream, ai->attack.start) == -1) return -1;
    if (fileWriteInt32(stream, ai->attack.end) == -1) return -1;
    if (fileWriteInt32(stream, ai->miss.start) == -1) return -1;
    if (fileWriteInt32(stream, ai->miss.end) == -1) return -1;

    for (int index = 0; index < HIT_LOCATION_SPECIFIC_COUNT; index++) {
        AiMessageRange* range = &(ai->hit[index]);
        if (fileWriteInt32(stream, range->start) == -1) return -1;
        if (fileWriteInt32(stream, range->end) == -1) return -1;
    }

    if (fileWriteInt32(stream, ai->area_attack_mode) == -1) return -1;
    if (fileWriteInt32(stream, ai->best_weapon) == -1) return -1;
    if (fileWriteInt32(stream, ai->distance) == -1) return -1;
    if (fileWriteInt32(stream, ai->attack_who) == -1) return -1;
    if (fileWriteInt32(stream, ai->chem_use) == -1) return -1;
    if (fileWriteInt32(stream, ai->run_away_mode) == -1) return -1;

    for (int index = 0; index < AI_PACKET_CHEM_PRIMARY_DESIRE_COUNT; index++) {
        // TODO: Check, probably writes chem_primary_desire[0] three times,
        // might be a bug in original source code.
        if (fileWriteInt32(stream, ai->chem_primary_desire[index]) == -1) return -1;
    }

    return 0;
}

// 0x428058
int combat_ai_num()
{
    return gAiPacketsLength;
}

// 0x428060
char* combat_ai_name(int packet_num)
{
    int index;

    if (packet_num < 0 || packet_num >= gAiPacketsLength) {
        return nullptr;
    }

    for (index = 0; index < gAiPacketsLength; index++) {
        if (gAiPackets[index].packet_num == packet_num) {
            return gAiPackets[index].name;
        }
    }

    return nullptr;
}

// Get ai from object
//
// 0x4280B4
static AiPacket* aiGetPacket(Object* obj)
{
    // NOTE: Uninline.
    AiPacket* ai = aiGetPacketByNum(obj->data.critter.combat.aiPacket);
    return ai;
}

// get ai packet by num
//
// 0x42811C
static AiPacket* aiGetPacketByNum(int aiPacketId)
{
    for (int index = 0; index < gAiPacketsLength; index++) {
        AiPacket* ai = &(gAiPackets[index]);
        if (aiPacketId == ai->packet_num) {
            return ai;
        }
    }

    debugPrint("Missing AI Packet\n");

    return gAiPackets;
}

// 0x428184
int aiGetAreaAttackMode(Object* obj)
{
    AiPacket* ai = aiGetPacket(obj);
    return ai->area_attack_mode;
}

// 0x428190
int aiGetRunAwayMode(Object* obj)
{
    int runAwayMode = -1;

    AiPacket* ai = aiGetPacket(obj);
    if (ai->run_away_mode != -1) {
        return ai->run_away_mode;
    }

    int hpRatio = 100 * ai->min_hp / critterGetStat(obj, STAT_MAXIMUM_HIT_POINTS);
    for (int index = 0; index < 6; index++) {
        if (hpRatio >= _hp_run_away_value[index]) {
            runAwayMode = index;
        }
    }

    return runAwayMode;
}

// 0x4281FC
int aiGetBestWeapon(Object* obj)
{
    AiPacket* ai = aiGetPacket(obj);
    return ai->best_weapon;
}

// 0x428208
int aiGetDistance(Object* obj)
{
    AiPacket* ai = aiGetPacket(obj);
    return ai->distance;
}

// 0x428214
int aiGetAttackWho(Object* obj)
{
    AiPacket* ai = aiGetPacket(obj);
    return ai->attack_who;
}

// 0x428220
int aiGetChemUse(Object* obj)
{
    AiPacket* ai = aiGetPacket(obj);
    return ai->chem_use;
}

// 0x42822C
int aiSetAreaAttackMode(Object* critter, int areaAttackMode)
{
    if (areaAttackMode >= AREA_ATTACK_MODE_COUNT) {
        return -1;
    }

    AiPacket* ai = aiGetPacket(critter);
    ai->area_attack_mode = areaAttackMode;
    return 0;
}

// 0x428248
int aiSetRunAwayMode(Object* obj, int runAwayMode)
{
    if (runAwayMode >= 6) {
        return -1;
    }

    AiPacket* ai = aiGetPacket(obj);
    ai->run_away_mode = runAwayMode;

    int maximumHp = critterGetStat(obj, STAT_MAXIMUM_HIT_POINTS);
    ai->min_hp = maximumHp - maximumHp * _hp_run_away_value[runAwayMode] / 100;

    int currentHp = critterGetStat(obj, STAT_CURRENT_HIT_POINTS);
    const char* name = critterGetName(obj);

    debugPrint("\n%s minHp = %d; curHp = %d", name, ai->min_hp, currentHp);

    return 0;
}

// 0x4282D0
int aiSetBestWeapon(Object* critter, int bestWeapon)
{
    if (bestWeapon >= BEST_WEAPON_COUNT) {
        return -1;
    }

    AiPacket* ai = aiGetPacket(critter);
    ai->best_weapon = bestWeapon;
    return 0;
}

// 0x4282EC
int aiSetDistance(Object* critter, int distance)
{
    if (distance >= DISTANCE_COUNT) {
        return -1;
    }

    AiPacket* ai = aiGetPacket(critter);
    ai->distance = distance;
    return 0;
}

// 0x428308
int aiSetAttackWho(Object* critter, int attackWho)
{
    if (attackWho >= ATTACK_WHO_COUNT) {
        return -1;
    }

    AiPacket* ai = aiGetPacket(critter);
    ai->attack_who = attackWho;
    return 0;
}

// 0x428324
int aiSetChemUse(Object* critter, int chemUse)
{
    if (chemUse >= CHEM_USE_COUNT) {
        return -1;
    }

    AiPacket* ai = aiGetPacket(critter);
    ai->chem_use = chemUse;
    return 0;
}

// 0x428340
int aiGetDisposition(Object* obj)
{
    if (obj == nullptr) {
        return 0;
    }

    AiPacket* ai = aiGetPacket(obj);
    return ai->disposition;
}

// 0x428354
int aiSetDisposition(Object* obj, int disposition)
{
    if (obj == nullptr) {
        return -1;
    }

    if (disposition == -1 || disposition >= 5) {
        return -1;
    }

    AiPacket* ai = aiGetPacket(obj);
    obj->data.critter.combat.aiPacket = ai->packet_num - (disposition - ai->disposition);

    return 0;
}

// 0x428398
static int _ai_magic_hands(Object* critter, Object* item, int num)
{
    reg_anim_begin(ANIMATION_REQUEST_RESERVED);

    animationRegisterAnimate(critter, ANIM_MAGIC_HANDS_MIDDLE, 0);

    if (reg_anim_end() == 0) {
        if (isInCombat()) {
            _combat_turn_run();
        }
    }

    if (num != -1) {
        MessageListItem messageListItem;
        messageListItem.num = num;
        if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
            const char* critterName = objectGetName(critter);

            char text[200];
            if (item != nullptr) {
                const char* itemName = objectGetName(item);
                snprintf(text, sizeof(text), "%s %s %s.", critterName, messageListItem.text, itemName);
            } else {
                snprintf(text, sizeof(text), "%s %s.", critterName, messageListItem.text);
            }

            displayMonitorAddMessage(text);
        }
    }

    return 0;
}

// ai using drugs
// 0x428480
static int _ai_check_drugs(Object* critter)
{
    if (critterGetBodyType(critter) != BODY_TYPE_BIPED) {
        return 0;
    }

    bool searchCompleted = false;
    bool drugUsed = false;
    int drugCount = 0;
    Object* lastItem = aiInfoGetLastItem(critter);
    if (lastItem == nullptr) {
        AiPacket* ai = aiGetPacket(critter);
        if (ai == nullptr) {
            return 0;
        }

        int hpRatio = kChemUseStimsHpRatio;
        int chemUseChance = 0;
        switch (ai->chem_use) {
        case CHEM_USE_CLEAN:
            return 0;
        case CHEM_USE_STIMS_WHEN_HURT_LITTLE:
            hpRatio = kChemUseStimsWhenHurtLittleHpRatio;
            break;
        case CHEM_USE_STIMS_WHEN_HURT_LOTS:
            hpRatio = kChemUseStimsWhenHurtLotsHpRatio;
            break;
        case CHEM_USE_SOMETIMES:
            if ((_combatNumTurns % 3) == 0) {
                chemUseChance = kChemUseSometimesChance;
            }
            break;
        case CHEM_USE_ANYTIME:
            if ((_combatNumTurns % 3) == 0) {
                chemUseChance = kChemUseAnytimeChance;
            }
            break;
        case CHEM_USE_ALWAYS:
            chemUseChance = kChemUseAlwaysChance;
            break;
        }

        int minHp = critterGetStat(critter, STAT_MAXIMUM_HIT_POINTS) * hpRatio / 100;
        int inventoryItemIndex = -1;
        while (critterGetStat(critter, STAT_CURRENT_HIT_POINTS) < minHp && critter->data.critter.combat.ap >= 2) {
            Object* drug = _inven_find_type(critter, ITEM_TYPE_DRUG, &inventoryItemIndex);
            if (drug == nullptr) {
                searchCompleted = true;
                break;
            }

            int drugPid = drug->pid;
            if (itemIsHealing(drugPid)) {
                if (itemRemove(critter, drug, 1) == 0) {
                    if (_item_d_take_drug(critter, drug) == -1) {
                        itemAdd(critter, drug, 1);
                    } else {
                        _ai_magic_hands(critter, drug, 5000);
                        _obj_connect(drug, critter->tile, critter->elevation, nullptr);
                        _obj_destroy(drug);
                        drugUsed = true;
                    }

                    if (critter->data.critter.combat.ap < 2) {
                        critter->data.critter.combat.ap = 0;
                    } else {
                        critter->data.critter.combat.ap -= 2;
                    }

                    inventoryItemIndex = -1;
                }
            }
        }

        if (!drugUsed) {
            if (chemUseChance > 0 && randomBetween(0, 100) < chemUseChance) {
                // CE: Slightly improve and randomize drug picking.
                inventoryItemIndex = -1;
                searchCompleted = false;

                Object* primaryDrugs[kRandomDrugPickingArraySize];
                int primaryDrugsCount = 0;

                Object* secondaryDrugs[kRandomDrugPickingArraySize];
                int secondaryDrugsCount = 0;

                // Collect drugs into buckets - primary desires and everything
                // else.
                //
                // Strictly speaking NPCs can have more drugs than buckets can
                // store. Together with `CHEM_USE_ALWAYS` it can lead to
                // unexpected behaviour. In vanilla NPCs will eat drugs on their
                // turns while they have action points. With this implementation
                // NPCs will eat at most 2x `kRandomDrugPickingArraySize` drugs
                // every turn (exhausting both primary and secondary buckets)
                // and then continue to other activities (assuming they have
                // action points to do so). In practice this sitation is very
                // rare if even exists in vanilla or popular mods.
                //
                // The alternative is to use a pair of `std::vector`s and let
                // them manage the memory.
                while (true) {
                    Object* drug = _inven_find_type(critter, ITEM_TYPE_DRUG, &inventoryItemIndex);
                    if (drug == nullptr) {
                        searchCompleted = true;
                        break;
                    }

                    if (!itemIsHealing(drug->pid)) {
                        bool isPrimary = false;
                        for (int index = 0; index < AI_PACKET_CHEM_PRIMARY_DESIRE_COUNT; index++) {
                            if (ai->chem_primary_desire[index] == drug->pid) {
                                isPrimary = true;
                                break;
                            }
                        }

                        if (isPrimary) {
                            if (primaryDrugsCount < kRandomDrugPickingArraySize) {
                                primaryDrugs[primaryDrugsCount++] = drug;
                            }
                        } else {
                            if (secondaryDrugsCount < kRandomDrugPickingArraySize) {
                                secondaryDrugs[secondaryDrugsCount++] = drug;
                            }
                        }
                    }
                }

                // Consume drugs one-by-one in random order with primary desires
                // first.
                while (critter->data.critter.combat.ap >= 2) {
                    Object** availableDrugs;
                    int* availableDrugsCountPtr;

                    if (primaryDrugsCount > 0) {
                        availableDrugs = primaryDrugs;
                        availableDrugsCountPtr = &primaryDrugsCount;
                    } else if (secondaryDrugsCount > 0) {
                        availableDrugs = secondaryDrugs;
                        availableDrugsCountPtr = &secondaryDrugsCount;
                    } else {
                        break;
                    }

                    int index = randomBetween(0, *availableDrugsCountPtr - 1);
                    Object* drug = availableDrugs[index];
                    availableDrugs[index] = availableDrugs[*availableDrugsCountPtr - 1];
                    *availableDrugsCountPtr -= 1;

                    if (itemRemove(critter, drug, 1) == 0) {
                        if (_item_d_take_drug(critter, drug) == -1) {
                            itemAdd(critter, drug, 1);
                        } else {
                            _ai_magic_hands(critter, drug, 5000);
                            _obj_connect(drug, critter->tile, critter->elevation, nullptr);
                            _obj_destroy(drug);
                            drugUsed = true;
                            drugCount += 1;
                        }

                        if (critter->data.critter.combat.ap < 2) {
                            critter->data.critter.combat.ap = 0;
                        } else {
                            critter->data.critter.combat.ap -= 2;
                        }

                        if (ai->chem_use == CHEM_USE_SOMETIMES || (ai->chem_use == CHEM_USE_ANYTIME && drugCount >= 2)) {
                            break;
                        }
                    }
                }
            }
        }
    }

    if (lastItem != nullptr || (!drugUsed && searchCompleted)) {
        do {
            if (lastItem == nullptr) {
                lastItem = _ai_search_environ(critter, ITEM_TYPE_DRUG);

                if (lastItem == nullptr) {
                    lastItem = _ai_search_environ(critter, ITEM_TYPE_MISC);

                    if (lastItem == nullptr) {
                        break;
                    }
                }
            }

            lastItem = _ai_retrieve_object(critter, lastItem);
            if (lastItem == nullptr) {
                break;
            }

            if (itemRemove(critter, lastItem, 1) == 0) {
                if (_item_d_take_drug(critter, lastItem) == -1) {
                    itemAdd(critter, lastItem, 1);
                } else {
                    _ai_magic_hands(critter, lastItem, 5000);
                    _obj_connect(lastItem, critter->tile, critter->elevation, nullptr);
                    _obj_destroy(lastItem);
                    lastItem = nullptr;
                }

                if (critter->data.critter.combat.ap < 2) {
                    critter->data.critter.combat.ap = 0;
                } else {
                    critter->data.critter.combat.ap -= 2;
                }
            }
        } while (lastItem != nullptr && critter->data.critter.combat.ap >= 2);
    }

    return 0;
}

// 0x428868
static void _ai_run_away(Object* a1, Object* a2)
{
    if (a2 == nullptr) {
        a2 = gDude;
    }

    CritterCombatData* combatData = &(a1->data.critter.combat);

    AiPacket* ai = aiGetPacket(a1);
    int distance = objectGetDistanceBetween(a1, a2);
    if (distance < ai->max_dist) {
        combatData->maneuver |= CRITTER_MANUEVER_FLEEING;

        int rotation = tileGetRotationTo(a2->tile, a1->tile);

        int destination;
        int actionPoints = combatData->ap;
        for (; actionPoints > 0; actionPoints -= 1) {
            destination = tileGetTileInDirection(a1->tile, rotation, actionPoints);
            if (_make_path(a1, a1->tile, destination, nullptr, 1) > 0) {
                break;
            }

            destination = tileGetTileInDirection(a1->tile, (rotation + 1) % ROTATION_COUNT, actionPoints);
            if (_make_path(a1, a1->tile, destination, nullptr, 1) > 0) {
                break;
            }

            destination = tileGetTileInDirection(a1->tile, (rotation + 5) % ROTATION_COUNT, actionPoints);
            if (_make_path(a1, a1->tile, destination, nullptr, 1) > 0) {
                break;
            }
        }

        if (actionPoints > 0) {
            reg_anim_begin(ANIMATION_REQUEST_RESERVED);
            _combatai_msg(a1, nullptr, AI_MESSAGE_TYPE_RUN, 0);
            animationRegisterRunToTile(a1, destination, a1->elevation, combatData->ap, 0);
            if (reg_anim_end() == 0) {
                _combat_turn_run();
            }
        }
    } else {
        combatData->maneuver |= CRITTER_MANEUVER_DISENGAGING;
    }
}

// 0x42899C
static int _ai_move_away(Object* a1, Object* a2, int a3)
{
    if (aiGetPacket(a1)->distance == DISTANCE_STAY) {
        return -1;
    }

    if (objectGetDistanceBetween(a1, a2) <= a3) {
        int actionPoints = a1->data.critter.combat.ap;
        if (a3 < actionPoints) {
            actionPoints = a3;
        }

        int rotation = tileGetRotationTo(a2->tile, a1->tile);

        int destination;
        int actionPointsLeft = actionPoints;
        for (; actionPointsLeft > 0; actionPointsLeft -= 1) {
            destination = tileGetTileInDirection(a1->tile, rotation, actionPointsLeft);
            if (_make_path(a1, a1->tile, destination, nullptr, 1) > 0) {
                break;
            }

            destination = tileGetTileInDirection(a1->tile, (rotation + 1) % ROTATION_COUNT, actionPointsLeft);
            if (_make_path(a1, a1->tile, destination, nullptr, 1) > 0) {
                break;
            }

            destination = tileGetTileInDirection(a1->tile, (rotation + 5) % ROTATION_COUNT, actionPointsLeft);
            if (_make_path(a1, a1->tile, destination, nullptr, 1) > 0) {
                break;
            }
        }

        if (actionPoints > 0) {
            reg_anim_begin(ANIMATION_REQUEST_RESERVED);
            animationRegisterMoveToTile(a1, destination, a1->elevation, actionPoints, 0);
            if (reg_anim_end() == 0) {
                _combat_turn_run();
            }
        }
    }

    return 0;
}

// 0x428AC4
static bool _ai_find_friend(Object* a1, int a2, int a3)
{
    Object* v1 = _ai_find_nearest_team(a1, a1, 1);
    if (v1 == nullptr) {
        return false;
    }

    int distance = objectGetDistanceBetween(a1, v1);
    if (distance > a2) {
        return false;
    }

    if (a3 > distance) {
        int v2 = objectGetDistanceBetween(a1, v1) - a3;
        _ai_move_steps_closer(a1, v1, v2, false);
    }

    return true;
}

// Compare objects by distance to origin.
//
// 0x428B1C
static int _compare_nearer(const void* a1, const void* a2)
{
    Object* object1 = *(Object**)a1;
    Object* object2 = *(Object**)a2;

    if (object1 == nullptr && object2 == nullptr) {
        return 0;
    } else if (object1 != nullptr && object2 == nullptr) {
        return -1;
    } else if (object1 == nullptr && object2 != nullptr) {
        return 1;
    }

    int distance1 = objectGetDistanceBetween(object1, _combat_obj);
    int distance2 = objectGetDistanceBetween(object2, _combat_obj);

    if (distance1 < distance2) {
        return -1;
    } else if (distance1 > distance2) {
        return 1;
    } else {
        return 0;
    }
}

// NOTE: Inlined.
//
// 0x428B74
static void _ai_sort_list_distance(Object** critterList, int length, Object* origin)
{
    _combat_obj = origin;
    qsort(critterList, length, sizeof(*critterList), _compare_nearer);
}

// qsort compare function - melee then ranged.
//
// 0x428B8C
static int _compare_strength(const void* a1, const void* a2)
{
    Object* object1 = *(Object**)a1;
    Object* object2 = *(Object**)a2;

    if (object1 == nullptr && object2 == nullptr) {
        return 0;
    } else if (object1 != nullptr && object2 == nullptr) {
        return -1;
    } else if (object1 == nullptr && object2 != nullptr) {
        return 1;
    }

    int rating1 = _combatai_rating(object1);
    int rating2 = _combatai_rating(object2);

    if (rating1 < rating2) {
        return -1;
    } else if (rating1 > rating2) {
        return 1;
    } else {
        return 0;
    }
}

// NOTE: Inlined.
//
// 0x428BD0
static void _ai_sort_list_strength(Object** critterList, int length)
{
    qsort(critterList, length, sizeof(*critterList), _compare_strength);
}

// qsort compare unction - ranged then melee
//
// 0x428BE4
static int _compare_weakness(const void* a1, const void* a2)
{
    Object* object1 = *(Object**)a1;
    Object* object2 = *(Object**)a2;

    if (object1 == nullptr && object2 == nullptr) {
        return 0;
    } else if (object1 != nullptr && object2 == nullptr) {
        return -1;
    } else if (object1 == nullptr && object2 != nullptr) {
        return 1;
    }

    int rating1 = _combatai_rating(object1);
    int rating2 = _combatai_rating(object2);

    if (rating1 < rating2) {
        return 1;
    } else if (rating1 > rating2) {
        return -1;
    } else {
        return 0;
    }
}

// NOTE: Inlined.
//
// 0x428C28
static void _ai_sort_list_weakness(Object** critterList, int length)
{
    qsort(critterList, length, sizeof(*critterList), _compare_weakness);
}

// 0x428C3C
static Object* _ai_find_nearest_team(Object* a1, Object* a2, int flags)
{
    if (a2 == nullptr) {
        return nullptr;
    }

    int team = a2->data.critter.combat.team;

    if (_curr_crit_num == 0) {
        return nullptr;
    }

    // NOTE: Uninline.
    _ai_sort_list_distance(_curr_crit_list, _curr_crit_num, a1);

    for (int index = 0; index < _curr_crit_num; index++) {
        Object* obj = _curr_crit_list[index];
        if (a1 != obj
            && (obj->data.critter.combat.results & DAM_DEAD) == 0
            && (((flags & 0x02) && team != obj->data.critter.combat.team)
                || ((flags & 0x01) && team == obj->data.critter.combat.team))) {
            return obj;
        }
    }

    return nullptr;
}

// 0x428CF4
static Object* _ai_find_nearest_team_in_combat(Object* a1, Object* a2, int flags)
{
    if (a2 == nullptr) {
        return nullptr;
    }

    int team = a2->data.critter.combat.team;

    if (_curr_crit_num == 0) {
        return nullptr;
    }

    // NOTE: Uninline.
    _ai_sort_list_distance(_curr_crit_list, _curr_crit_num, a1);

    for (int index = 0; index < _curr_crit_num; index++) {
        Object* obj = _curr_crit_list[index];
        if (obj != a1
            && (obj->data.critter.combat.results & DAM_DEAD) == 0
            && (((flags & 0x02) != 0 && team != obj->data.critter.combat.team)
                || ((flags & 0x01) != 0 && team == obj->data.critter.combat.team))) {
            if (obj->data.critter.combat.whoHitMe != nullptr) {
                return obj;
            }
        }
    }

    return nullptr;
}

// 0x428DB0
static int aiFindAttackers(Object* critter, Object** whoHitMePtr, Object** whoHitFriendPtr, Object** whoHitByFriendPtr)
{
    if (whoHitMePtr != nullptr) {
        *whoHitMePtr = nullptr;
    }

    if (whoHitFriendPtr != nullptr) {
        *whoHitFriendPtr = nullptr;
    }

    if (whoHitByFriendPtr != nullptr) {
        *whoHitByFriendPtr = nullptr;
    }

    if (_curr_crit_num == 0) {
        return 0;
    }

    // NOTE: Uninline.
    _ai_sort_list_distance(_curr_crit_list, _curr_crit_num, critter);

    int foundTargetCount = 0;
    int team = critter->data.critter.combat.team;

    // SFALL: Add `continue` to fix for one candidate being reported in more
    // than one category.
    for (int index = 0; foundTargetCount < 3 && index < _curr_crit_num; index++) {
        Object* candidate = _curr_crit_list[index];
        if (candidate != critter) {
            if (whoHitMePtr != nullptr && *whoHitMePtr == nullptr) {
                if ((candidate->data.critter.combat.results & DAM_DEAD) == 0
                    && candidate->data.critter.combat.whoHitMe == critter) {
                    foundTargetCount++;
                    *whoHitMePtr = candidate;
                    continue;
                }
            }

            if (whoHitFriendPtr != nullptr && *whoHitFriendPtr == nullptr) {
                if (team == candidate->data.critter.combat.team) {
                    Object* whoHitCandidate = candidate->data.critter.combat.whoHitMe;
                    if (whoHitCandidate != nullptr
                        && whoHitCandidate != critter
                        && team != whoHitCandidate->data.critter.combat.team
                        && (whoHitCandidate->data.critter.combat.results & DAM_DEAD) == 0) {
                        foundTargetCount++;
                        *whoHitFriendPtr = whoHitCandidate;
                        continue;
                    }
                }
            }

            if (whoHitByFriendPtr != nullptr && *whoHitByFriendPtr == nullptr) {
                if (candidate->data.critter.combat.team != team
                    && (candidate->data.critter.combat.results & DAM_DEAD) == 0) {
                    Object* whoHitCandidate = candidate->data.critter.combat.whoHitMe;
                    if (whoHitCandidate != nullptr
                        && whoHitCandidate->data.critter.combat.team == team) {
                        foundTargetCount++;
                        *whoHitByFriendPtr = candidate;
                        continue;
                    }
                }
            }
        }
    }

    return 0;
}

// ai_danger_source
// 0x428F4C
static Object* _ai_danger_source(Object* a1)
{
    if (a1 == nullptr) {
        return nullptr;
    }

    bool ignoreFleeingCritters = false;
    int attackWho;

    Object* targets[4];
    targets[0] = nullptr;

    if (objectIsPartyMember(a1)) {
        int disposition = aiGetDisposition(a1);

        switch (disposition + 1) {
        case DISPOSITION_CUSTOM:
        case DISPOSITION_COWARD:
        case DISPOSITION_DEFENSIVE:
        case DISPOSITION_AGGRESSIVE:
            ignoreFleeingCritters = true;
            break;
        case DISPOSITION_NONE:
        case DISPOSITION_BERKSERK:
            ignoreFleeingCritters = false;
            break;
        }

        if (ignoreFleeingCritters && aiGetDistance(a1) == DISTANCE_CHARGE) {
            ignoreFleeingCritters = false;
        }

        attackWho = aiGetAttackWho(a1);
        switch (attackWho) {
        case ATTACK_WHO_WHOMEVER_ATTACKING_ME:
            if (1) {
                // CE: Slightly improve "Whomever is attacking me" targeting.
                //
                // First attempt to continue attack our previous target. This
                // prevents jumping from target to target thus spending precious
                // action points on meaningless movements.
                Object* candidate = aiInfoGetLastTarget(a1);
                if (candidate != nullptr) {
                    // Check if candidate is still a valid target:
                    // - not dead and not knocked out
                    // - not on the same team
                    // - still attacking dude
                    Object* critter = candidate;
                    if ((critter->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0
                        || a1->data.critter.combat.team == critter->data.critter.combat.team
                        || aiInfoGetLastTarget(critter) != gDude) {
                        candidate = nullptr;
                    }

                    // NOTE: I'm not sure if we need to revalidate candidate's
                    // reachability and shot conditions.

                    // Do not chase fleeing critter.
                    if (ignoreFleeingCritters && critterIsFleeing(critter)) {
                        candidate = nullptr;
                    }
                }

                if (candidate == nullptr) {
                    // Previous target is invalid. Pick nearest candidate who's
                    // attacking dude.
                    _ai_sort_list_distance(_curr_crit_list, _curr_crit_num, a1);

                    for (int index = 0; index < _curr_crit_num; index++) {
                        Object* critter = _curr_crit_list[index];
                        if (critter != a1) {
                            // Check if it's valid target (same conditions as
                            // above).
                            if ((critter->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0
                                || a1->data.critter.combat.team == critter->data.critter.combat.team
                                || aiInfoGetLastTarget(critter) != gDude) {
                                continue;
                            }

                            // Make sure critter is reachable.
                            if (pathfinderFindPath(a1, a1->tile, critter->tile, nullptr, 0, _obj_blocking_at) == 0) {
                                continue;
                            }

                            // Check if we can attack it. No ammo and out of
                            // range are ok results, since we can reload weapon
                            // move closer if necessary.
                            int badShot = _combat_check_bad_shot(a1, critter, HIT_MODE_RIGHT_WEAPON_PRIMARY, false);
                            if (badShot != COMBAT_BAD_SHOT_OK
                                && badShot != COMBAT_BAD_SHOT_NO_AMMO
                                && badShot != COMBAT_BAD_SHOT_OUT_OF_RANGE) {
                                continue;
                            }

                            // Do not chase fleeing critter.
                            if (ignoreFleeingCritters && critterIsFleeing(critter)) {
                                continue;
                            }

                            candidate = critter;
                            break;
                        }
                    }
                }

                if (candidate != nullptr) {
                    return candidate;
                }
            }
            break;
        case ATTACK_WHO_STRONGEST:
        case ATTACK_WHO_WEAKEST:
        case ATTACK_WHO_CLOSEST:
            a1->data.critter.combat.whoHitMe = nullptr;
            break;
        default:
            break;
        }
    } else {
        attackWho = -1;
    }

    Object* whoHitMe = a1->data.critter.combat.whoHitMe;
    if (whoHitMe == nullptr || a1 == whoHitMe) {
        targets[0] = nullptr;
    } else {
        if ((whoHitMe->data.critter.combat.results & DAM_DEAD) == 0) {
            if (attackWho == ATTACK_WHO_WHOMEVER || attackWho == -1) {
                return whoHitMe;
            }
        } else {
            if (whoHitMe->data.critter.combat.team != a1->data.critter.combat.team) {
                targets[0] = _ai_find_nearest_team(a1, whoHitMe, 1);
            } else {
                targets[0] = nullptr;
            }
        }
    }

    aiFindAttackers(a1, &(targets[1]), &(targets[2]), &(targets[3]));

    if (ignoreFleeingCritters) {
        for (int index = 0; index < 4; index++) {
            if (targets[index] != nullptr && critterIsFleeing(targets[index])) {
                targets[index] = nullptr;
            }
        }
    }

    switch (attackWho) {
    case ATTACK_WHO_STRONGEST:
        // NOTE: Uninline.
        _ai_sort_list_strength(targets, 4);
        break;
    case ATTACK_WHO_WEAKEST:
        // NOTE: Uninline.
        _ai_sort_list_weakness(targets, 4);
        break;
    default:
        // NOTE: Uninline.
        _ai_sort_list_distance(targets, 4, a1);
        break;
    }

    for (int index = 0; index < 4; index++) {
        Object* candidate = targets[index];
        if (candidate != nullptr && isWithinPerception(a1, candidate)) {
            if (pathfinderFindPath(a1, a1->tile, candidate->tile, nullptr, 0, _obj_blocking_at) != 0
                || _combat_check_bad_shot(a1, candidate, HIT_MODE_RIGHT_WEAPON_PRIMARY, false) == COMBAT_BAD_SHOT_OK) {
                return candidate;
            }
            debugPrint("\nai_danger_source: I couldn't get at my target!  Picking alternate!");
        }
    }

    return nullptr;
}

// 0x4291C4
int _caiSetupTeamCombat(Object* attackerTeam, Object* defenderTeam)
{
    Object* obj = objectFindFirstAtElevation(attackerTeam->elevation);
    while (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER && obj != gDude) {
            obj->data.critter.combat.maneuver |= CRITTER_MANEUVER_ENGAGING;
        }
        obj = objectFindNextAtElevation();
    }

    _attackerTeamObj = attackerTeam;
    _targetTeamObj = defenderTeam;

    return 0;
}

// 0x_429210
int _caiTeamCombatInit(Object** crittersList, int crittersListLength)
{
    if (crittersList == nullptr) {
        return -1;
    }

    if (crittersListLength == 0) {
        return 0;
    }

    if (_attackerTeamObj == nullptr) {
        return 0;
    }

    int attackerTeam = _attackerTeamObj->data.critter.combat.team;
    int defenderTeam = _targetTeamObj->data.critter.combat.team;

    for (int index = 0; index < crittersListLength; index++) {
        int team = crittersList[index]->data.critter.combat.team;
        if (team == attackerTeam) {
            crittersList[index]->data.critter.combat.whoHitMe = _ai_find_nearest_team(crittersList[index], _targetTeamObj, 1);
        } else if (team == defenderTeam) {
            crittersList[index]->data.critter.combat.whoHitMe = _ai_find_nearest_team(crittersList[index], _attackerTeamObj, 1);
        }
    }

    _attackerTeamObj = nullptr;
    _targetTeamObj = nullptr;

    return 0;
}

// 0x4292C0
void _caiTeamCombatExit()
{
    _targetTeamObj = nullptr;
    _attackerTeamObj = nullptr;
}

// 0x4292D4
static bool aiHaveAmmo(Object* critter, Object* weapon, Object** ammoPtr)
{
    if (ammoPtr != nullptr) {
        *ammoPtr = nullptr;
    }

    if (weapon->pid == PROTO_ID_SOLAR_SCORCHER) {
        return lightGetAmbientIntensity() > LIGHT_INTENSITY_MAX * 0.95;
    }

    int inventoryItemIndex = -1;

    while (1) {
        Object* ammo = _inven_find_type(critter, ITEM_TYPE_AMMO, &inventoryItemIndex);
        if (ammo == nullptr) {
            break;
        }

        if (weaponCanBeReloadedWith(weapon, ammo)) {
            if (ammoPtr != nullptr) {
                *ammoPtr = ammo;
            }
            return true;
        }

        if (weaponGetAnimationCode(weapon)) {
            if (weaponGetRange(critter, HIT_MODE_RIGHT_WEAPON_PRIMARY) < 3) {
                _inven_unwield(critter, HAND_RIGHT);
            }
        } else {
            _inven_unwield(critter, HAND_RIGHT);
        }
    }

    return false;
}

// 0x42938C
static bool _caiHasWeapPrefType(AiPacket* ai, int attackType)
{
    int bestWeapon = ai->best_weapon + 1;

    for (int index = 0; index < ATTACK_TYPE_COUNT; index++) {
        if (attackType == _weapPrefOrderings[bestWeapon][index]) {
            return true;
        }
    }

    return false;
}

// 0x4293BC
static Object* _ai_best_weapon(Object* attacker, Object* weapon1, Object* weapon2, Object* defender)
{
    if (attacker == nullptr) {
        return nullptr;
    }

    AiPacket* ai = aiGetPacket(attacker);
    if (ai->best_weapon == BEST_WEAPON_RANDOM) {
        return randomBetween(1, 100) <= 50 ? weapon1 : weapon2;
    }
    int minDamage;
    int maxDamage;

    bool ignoreWeapon1 = false;
    int order2 = 999;
    int order1 = 999;
    int avgDamage1 = 0;

    Attack attack;
    attackInit(&attack, attacker, defender, HIT_MODE_RIGHT_WEAPON_PRIMARY, HIT_LOCATION_TORSO);

    int attackType1;
    int distance;
    int attackType2;
    int avgDamage2 = 0;

    bool ignoreWeapon2 = false;

    // NOTE: weaponClass1 and weaponClass2 both use ESI but they are not
    // initialized. I'm not sure if this is right, but at least it doesn't
    // crash.
    attackType1 = -1;
    attackType2 = -1;

    if (weapon1 != nullptr) {
        attackType1 = weaponGetAttackTypeForHitMode(weapon1, HIT_MODE_RIGHT_WEAPON_PRIMARY);
        if (weaponGetDamageMinMax(weapon1, &minDamage, &maxDamage) == -1) {
            return nullptr;
        }

        // SFALL: Fix avg damage calculation.
        avgDamage1 = (maxDamage + minDamage) / 2;
        if (weaponGetDamageRadius(weapon1, HIT_MODE_RIGHT_WEAPON_PRIMARY) > 0 && defender != nullptr) {
            attack.weapon = weapon1;
            _compute_explosion_on_extras(&attack, 0, weaponIsGrenade(weapon1), 1);
            avgDamage1 *= attack.extrasLength + 1;
        }

        // SFALL: Fix for the incorrect item being checked.
        if (weaponGetPerk(weapon1) != -1) {
            // SFALL: Lower weapon score multiplier for having perk.
            avgDamage1 *= 2;
        }

        if (defender != nullptr) {
            if (_combat_safety_invalidate_weapon(attacker, weapon1, HIT_MODE_RIGHT_WEAPON_PRIMARY, defender, nullptr)) {
                ignoreWeapon1 = true;
            }
        }

        if (itemIsHidden(weapon1)) {
            return weapon1;
        }
    } else {
        distance = objectGetDistanceBetween(attacker, defender);
        if (weaponGetRange(attacker, HIT_MODE_PUNCH) >= distance) {
            attackType1 = ATTACK_TYPE_UNARMED;
        }
    }

    if (!ignoreWeapon1) {
        for (int index = 0; index < ATTACK_TYPE_COUNT; index++) {
            if (_weapPrefOrderings[ai->best_weapon + 1][index] == attackType1) {
                order1 = index;
                break;
            }
        }
    }

    if (weapon2 != nullptr) {
        attackType2 = weaponGetAttackTypeForHitMode(weapon2, HIT_MODE_RIGHT_WEAPON_PRIMARY);
        if (weaponGetDamageMinMax(weapon2, &minDamage, &maxDamage) == -1) {
            return nullptr;
        }

        // SFALL: Fix avg damage calculation.
        avgDamage2 = (maxDamage + minDamage) / 2;
        if (weaponGetDamageRadius(weapon2, HIT_MODE_RIGHT_WEAPON_PRIMARY) > 0 && defender != nullptr) {
            attack.weapon = weapon2;
            _compute_explosion_on_extras(&attack, 0, weaponIsGrenade(weapon2), 1);
            avgDamage2 *= attack.extrasLength + 1;
        }

        if (weaponGetPerk(weapon2) != -1) {
            // SFALL: Lower weapon score multiplier for having perk.
            avgDamage2 *= 2;
        }

        if (defender != nullptr) {
            if (_combat_safety_invalidate_weapon(attacker, weapon2, HIT_MODE_RIGHT_WEAPON_PRIMARY, defender, nullptr)) {
                ignoreWeapon2 = true;
            }
        }

        if (itemIsHidden(weapon2)) {
            return weapon2;
        }
    } else {
        if (distance == 0) {
            distance = objectGetDistanceBetween(attacker, weapon1);
        }

        if (weaponGetRange(attacker, HIT_MODE_PUNCH) >= distance) {
            attackType2 = ATTACK_TYPE_UNARMED;
        }
    }

    if (!ignoreWeapon2) {
        for (int index = 0; index < ATTACK_TYPE_COUNT; index++) {
            if (_weapPrefOrderings[ai->best_weapon + 1][index] == attackType2) {
                order2 = index;
                break;
            }
        }
    }

    if (order1 == order2) {
        if (order1 == 999) {
            return nullptr;
        }

        if (abs(avgDamage2 - avgDamage1) <= 5) {
            return itemGetCost(weapon2) > itemGetCost(weapon1) ? weapon2 : weapon1;
        }

        return avgDamage2 > avgDamage1 ? weapon2 : weapon1;
    }

    if (weapon1 != nullptr && weapon1->pid == PROTO_ID_FLARE && weapon2 != nullptr) {
        return weapon2;
    }

    if (weapon2 != nullptr && weapon2->pid == PROTO_ID_FLARE && weapon1 != nullptr) {
        return weapon1;
    }

    if ((ai->best_weapon == -1 || ai->best_weapon >= BEST_WEAPON_UNARMED_OVER_THROW)
        && abs(avgDamage2 - avgDamage1) > 5) {
        return avgDamage2 > avgDamage1 ? weapon2 : weapon1;
    }

    return order1 > order2 ? weapon2 : weapon1;
}

// 0x4298EC
static bool _ai_can_use_weapon(Object* critter, Object* weapon, int hitMode)
{
    int damageFlags = critter->data.critter.combat.results;
    if ((damageFlags & DAM_CRIP_ARM_LEFT) != 0 && (damageFlags & DAM_CRIP_ARM_RIGHT) != 0) {
        return false;
    }

    if ((damageFlags & DAM_CRIP_ARM_ANY) != 0 && weaponIsTwoHanded(weapon)) {
        return false;
    }

    int rotation = critter->rotation + 1;
    int animationCode = weaponGetAnimationCode(weapon);
    int weaponAnimationCode = weaponGetAnimationForHitMode(weapon, hitMode);
    int fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, weaponAnimationCode, animationCode, rotation);
    if (!artExists(fid)) {
        return false;
    }

    int skill = weaponGetSkillForHitMode(weapon, hitMode);
    AiPacket* ai = aiGetPacket(critter);
    if (skillGetValue(critter, skill) < ai->min_to_hit) {
        return false;
    }

    int attackType = weaponGetAttackTypeForHitMode(weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY);
    return _caiHasWeapPrefType(ai, attackType) != 0;
}

// 0x4299A0
Object* _ai_search_inven_weap(Object* critter, bool checkRequiredActionPoints, Object* defender)
{
    int bodyType = critterGetBodyType(critter);
    if (bodyType != BODY_TYPE_BIPED
        && bodyType != BODY_TYPE_ROBOTIC
        && critter->pid != PROTO_ID_0x1000098) {
        return nullptr;
    }

    int token = -1;
    Object* bestWeapon = nullptr;
    Object* rightHandWeapon = critterGetItem2(critter);
    while (true) {
        Object* weapon = _inven_find_type(critter, ITEM_TYPE_WEAPON, &token);
        if (weapon == nullptr) {
            break;
        }

        if (weapon == rightHandWeapon) {
            continue;
        }

        if (checkRequiredActionPoints) {
            if (weaponGetPrimaryActionPointCost(weapon) > critter->data.critter.combat.ap) {
                continue;
            }
        }

        if (!_ai_can_use_weapon(critter, weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY)) {
            continue;
        }

        if (weaponGetAttackTypeForHitMode(weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY) == ATTACK_TYPE_RANGED) {
            if (ammoGetQuantity(weapon) == 0) {
                if (!aiHaveAmmo(critter, weapon, nullptr)) {
                    continue;
                }
            }
        }

        bestWeapon = _ai_best_weapon(critter, bestWeapon, weapon, defender);
    }

    return bestWeapon;
}

// Finds new best armor (other than what's already equipped) based on the armor score.
//
// 0x429A6C
Object* _ai_search_inven_armor(Object* critter)
{
    if (!objectIsPartyMember(critter)) {
        return nullptr;
    }

    // Calculate armor score - it's a unitless combination of armor class and bonuses across
    // all damage types.
    int armorScore = 0;
    Object* armor = critterGetArmor(critter);
    if (armor != nullptr) {
        armorScore = armorGetArmorClass(armor);

        for (int damageType = 0; damageType < DAMAGE_TYPE_COUNT; damageType++) {
            armorScore += armorGetDamageResistance(armor, damageType);
            armorScore += armorGetDamageThreshold(armor, damageType);
        }
    } else {
        armorScore = 0;
    }

    Object* bestArmor = nullptr;

    int inventoryItemIndex = -1;
    while (true) {
        Object* candidate = _inven_find_type(critter, ITEM_TYPE_ARMOR, &inventoryItemIndex);
        if (candidate == nullptr) {
            break;
        }

        if (armor != candidate) {
            int candidateScore = armorGetArmorClass(candidate);
            for (int damageType = 0; damageType < DAMAGE_TYPE_COUNT; damageType++) {
                candidateScore += armorGetDamageResistance(candidate, damageType);
                candidateScore += armorGetDamageThreshold(candidate, damageType);
            }

            if (candidateScore > armorScore) {
                armorScore = candidateScore;
                bestArmor = candidate;
            }
        }
    }

    return bestArmor;
}

// Returns true if critter can use given item.
//
// That means the item is one of it's primary desires,
// or it's a humanoid being with intelligence at least 3,
// and the iteam is a something healing.
//
// 0x429B44
static bool aiCanUseItem(Object* critter, Object* item)
{
    if (critter == nullptr) {
        return false;
    }

    if (item == nullptr) {
        return false;
    }

    AiPacket* ai = aiGetPacketByNum(critter->data.critter.combat.aiPacket);
    if (ai == nullptr) {
        return false;
    }

    for (int index = 0; index < AI_PACKET_CHEM_PRIMARY_DESIRE_COUNT; index++) {
        if (item->pid == ai->chem_primary_desire[index]) {
            return true;
        }
    }

    if (critterGetBodyType(critter) != BODY_TYPE_BIPED) {
        return false;
    }

    int killType = critterGetKillType(critter);
    if (killType != KILL_TYPE_MAN
        && killType != KILL_TYPE_WOMAN
        && killType != KILL_TYPE_SUPER_MUTANT
        && killType != KILL_TYPE_GHOUL
        && killType != KILL_TYPE_CHILD) {
        return false;
    }

    if (critterGetStat(critter, STAT_INTELLIGENCE) < 3) {
        return false;
    }

    // SFALL: Check healing items.
    if (!itemIsHealing(item->pid)) {
        return false;
    }

    // CE: Make sure critter actually need healing item.
    //
    // Sfall has similar fix implemented differently in `ai_check_drugs`. It
    // does so after healing item is returned from `ai_search_environ`, so it
    // does not a have a chance to look for other items.
    int hpRatio = kChemUseStimsHpRatio;
    switch (aiGetChemUse(critter)) {
    case CHEM_USE_CLEAN:
        hpRatio = 0;
        break;
    case CHEM_USE_STIMS_WHEN_HURT_LITTLE:
        hpRatio = kChemUseStimsWhenHurtLittleHpRatio;
        break;
    case CHEM_USE_STIMS_WHEN_HURT_LOTS:
        hpRatio = kChemUseStimsWhenHurtLotsHpRatio;
        break;
    }

    int currentHp = critterGetStat(critter, STAT_CURRENT_HIT_POINTS);
    int stimsHp = critterGetStat(critter, STAT_MAXIMUM_HIT_POINTS) * hpRatio / 100;
    if (currentHp > stimsHp) {
        return false;
    }

    return true;
}

// Find best item type to use?
//
// 0x429C18
static Object* _ai_search_environ(Object* critter, int itemType)
{
    if (critterGetBodyType(critter) != BODY_TYPE_BIPED) {
        return nullptr;
    }

    Object** objects;
    int count = objectListCreate(-1, gElevation, OBJ_TYPE_ITEM, &objects);
    if (count == 0) {
        return nullptr;
    }

    // NOTE: Uninline.
    _ai_sort_list_distance(objects, count, critter);

    int perception = critterGetStat(critter, STAT_PERCEPTION) + 5;
    Object* item2 = critterGetItem2(critter);

    Object* foundItem = nullptr;

    for (int index = 0; index < count; index++) {
        Object* item = objects[index];
        int distance = objectGetDistanceBetween(critter, item);
        if (distance > perception) {
            break;
        }

        if (itemGetType(item) == itemType) {
            switch (itemType) {
            case ITEM_TYPE_WEAPON:
                if (_ai_can_use_weapon(critter, item, HIT_MODE_RIGHT_WEAPON_PRIMARY)) {
                    foundItem = item;
                }
                break;
            case ITEM_TYPE_AMMO:
                if (weaponCanBeReloadedWith(item2, item)) {
                    foundItem = item;
                }
                break;
            case ITEM_TYPE_DRUG:
            case ITEM_TYPE_MISC:
                if (aiCanUseItem(critter, item)) {
                    foundItem = item;
                }
                break;
            }

            if (foundItem != nullptr) {
                break;
            }
        }
    }

    objectListFree(objects);

    return foundItem;
}

// 0x429D60
static Object* _ai_retrieve_object(Object* critter, Object* item)
{
    if (actionPickUp(critter, item) != 0) {
        return nullptr;
    }

    // Run animation so that NPC can actually move and pick up the item.
    _combat_turn_run();

    // Find the item in NPC's inventory. This step is needed because NPC could
    // not get the item on this turn.
    Object* retrievedItem = _inven_find_id(critter, item->id);

    if (retrievedItem != nullptr || item->owner != nullptr) {
        // Either NPC have the item, or someone else have picked it up.
        item = nullptr;
    }

    // Save item NPC wants to pick up on the next turn.
    aiInfoSetLastItem(retrievedItem, item);

    return retrievedItem;
}

// 0x429DB4
static int _ai_pick_hit_mode(Object* attacker, Object* weapon, Object* defender)
{
    if (weapon == nullptr) {
        return HIT_MODE_PUNCH;
    }

    if (itemGetType(weapon) != ITEM_TYPE_WEAPON) {
        return HIT_MODE_PUNCH;
    }

    int attackType = weaponGetAttackTypeForHitMode(weapon, HIT_MODE_RIGHT_WEAPON_SECONDARY);
    int intelligence = critterGetStat(attacker, STAT_INTELLIGENCE);
    if (attackType == ATTACK_TYPE_NONE || !_ai_can_use_weapon(attacker, weapon, HIT_MODE_RIGHT_WEAPON_SECONDARY)) {
        return HIT_MODE_RIGHT_WEAPON_PRIMARY;
    }

    bool useSecondaryMode = false;

    AiPacket* ai = aiGetPacket(attacker);
    if (ai == nullptr) {
        return HIT_MODE_PUNCH;
    }

    if (ai->area_attack_mode != -1) {
        switch (ai->area_attack_mode) {
        case AREA_ATTACK_MODE_ALWAYS:
            useSecondaryMode = true;
            break;
        case AREA_ATTACK_MODE_SOMETIMES:
            if (randomBetween(1, ai->secondary_freq) == 1) {
                useSecondaryMode = true;
            }
            break;
        case AREA_ATTACK_MODE_BE_SURE:
            if (_determine_to_hit(attacker, defender, HIT_LOCATION_TORSO, HIT_MODE_RIGHT_WEAPON_SECONDARY) >= 85
                && !_combat_safety_invalidate_weapon(attacker, weapon, 3, defender, nullptr)) {
                useSecondaryMode = true;
            }
            break;
        case AREA_ATTACK_MODE_BE_CAREFUL:
            if (_determine_to_hit(attacker, defender, HIT_LOCATION_TORSO, HIT_MODE_RIGHT_WEAPON_SECONDARY) >= 50
                && !_combat_safety_invalidate_weapon(attacker, weapon, 3, defender, nullptr)) {
                useSecondaryMode = true;
            }
            break;
        case AREA_ATTACK_MODE_BE_ABSOLUTELY_SURE:
            if (_determine_to_hit(attacker, defender, HIT_LOCATION_TORSO, HIT_MODE_RIGHT_WEAPON_SECONDARY) >= 95
                && !_combat_safety_invalidate_weapon(attacker, weapon, 3, defender, nullptr)) {
                useSecondaryMode = true;
            }
            break;
        }
    } else {
        if (intelligence < 6 || objectGetDistanceBetween(attacker, defender) < 10) {
            if (randomBetween(1, ai->secondary_freq) == 1) {
                useSecondaryMode = true;
            }
        }
    }

    if (useSecondaryMode) {
        if (!_caiHasWeapPrefType(ai, attackType)) {
            useSecondaryMode = false;
        }
    }

    // SFALL: Add a check for the weapon range and the AP cost when AI is
    // choosing weapon attack modes.
    if (useSecondaryMode) {
        if (objectGetDistanceBetween(attacker, defender) > weaponGetRange(attacker, HIT_MODE_RIGHT_WEAPON_SECONDARY)) {
            useSecondaryMode = false;
        }
    }

    if (useSecondaryMode) {
        if (attacker->data.critter.combat.ap < weaponGetActionPointCost(attacker, HIT_MODE_RIGHT_WEAPON_SECONDARY, false)) {
            useSecondaryMode = false;
        }
    }

    if (useSecondaryMode) {
        if (attackType != ATTACK_TYPE_THROW
            || _ai_search_inven_weap(attacker, false, defender) != nullptr
            || statRoll(attacker, STAT_INTELLIGENCE, 0, nullptr) <= 1) {
            return HIT_MODE_RIGHT_WEAPON_SECONDARY;
        }
    }

    return HIT_MODE_RIGHT_WEAPON_PRIMARY;
}

// 0x429FC8
static int _ai_move_steps_closer(Object* critter, Object* target, int actionPoints, bool taunt)
{
    if (actionPoints <= 0) {
        return -1;
    }

    int distance = aiGetDistance(critter);
    if (distance == DISTANCE_STAY) {
        return -1;
    }

    if (distance == DISTANCE_STAY_CLOSE) {
        if (target != gDude) {
            int currentDistance = objectGetDistanceBetween(critter, gDude);
            if (currentDistance > 5
                && objectGetDistanceBetween(target, gDude) > 5
                && currentDistance + actionPoints > 5) {
                return -1;
            }
        }
    }

    if (objectGetDistanceBetween(critter, target) <= 1) {
        return -1;
    }

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);

    if (taunt) {
        _combatai_msg(critter, nullptr, AI_MESSAGE_TYPE_MOVE, 0);
    }

    Object* initialTarget = target;

    bool shouldUnhide;
    if ((target->flags & OBJECT_MULTIHEX) != 0) {
        shouldUnhide = true;
        target->flags |= OBJECT_HIDDEN;
    } else {
        shouldUnhide = false;
    }

    if (pathfinderFindPath(critter, critter->tile, target->tile, nullptr, 0, _obj_blocking_at) == 0) {
        _moveBlockObj = nullptr;
        if (pathfinderFindPath(critter, critter->tile, target->tile, nullptr, 0, _obj_ai_blocking_at) == 0
            && _moveBlockObj != nullptr
            && PID_TYPE(_moveBlockObj->pid) == OBJ_TYPE_CRITTER) {
            if (shouldUnhide) {
                target->flags &= ~OBJECT_HIDDEN;
            }

            target = _moveBlockObj;
            if ((target->flags & OBJECT_MULTIHEX) != 0) {
                shouldUnhide = true;
                target->flags |= OBJECT_HIDDEN;
            } else {
                shouldUnhide = false;
            }
        }
    }

    if (shouldUnhide) {
        target->flags &= ~OBJECT_HIDDEN;
    }

    int tile = target->tile;
    if (target == initialTarget) {
        _cai_retargetTileFromFriendlyFire(critter, target, &tile);
    }

    if (actionPoints >= critterGetStat(critter, STAT_MAXIMUM_ACTION_POINTS) / 2 && artCritterFidShouldRun(critter->fid)) {
        if ((target->flags & OBJECT_MULTIHEX) != 0) {
            animationRegisterRunToObject(critter, target, actionPoints, 0);
        } else {
            animationRegisterRunToTile(critter, tile, critter->elevation, actionPoints, 0);
        }
    } else {
        if ((target->flags & OBJECT_MULTIHEX) != 0) {
            animationRegisterMoveToObject(critter, target, actionPoints, 0);
        } else {
            animationRegisterMoveToTile(critter, tile, critter->elevation, actionPoints, 0);
        }
    }

    if (reg_anim_end() != 0) {
        return -1;
    }

    _combat_turn_run();

    return 0;
}

// NOTE: Inlined.
//
// 0x42A1C0
static int _ai_move_closer(Object* a1, Object* a2, bool taunt)
{
    return _ai_move_steps_closer(a1, a2, a1->data.critter.combat.ap, taunt);
}

// 0x42A1D4
static int _cai_retargetTileFromFriendlyFire(Object* source, Object* target, int* tilePtr)
{
    if (source == nullptr) {
        return -1;
    }

    if (target == nullptr) {
        return -1;
    }

    if (tilePtr == nullptr) {
        return -1;
    }

    if (*tilePtr == -1) {
        return -1;
    }

    if (_curr_crit_num == 0) {
        return -1;
    }

    int tiles[32];

    AiRetargetData aiRetargetData;
    aiRetargetData.source = source;
    aiRetargetData.target = target;
    aiRetargetData.sourceTeam = source->data.critter.combat.team;
    aiRetargetData.sourceRating = _combatai_rating(source);
    aiRetargetData.critterCount = 0;
    aiRetargetData.tiles = tiles;
    aiRetargetData.notSameTile = *tilePtr != source->tile;
    aiRetargetData.currentTileIndex = 0;
    aiRetargetData.sourceIntelligence = critterGetStat(source, STAT_INTELLIGENCE);

    for (int index = 0; index < 32; index++) {
        tiles[index] = -1;
    }

    for (int index = 0; index < _curr_crit_num; index++) {
        Object* obj = _curr_crit_list[index];
        if ((obj->data.critter.combat.results & DAM_DEAD) == 0
            && obj->data.critter.combat.team == aiRetargetData.sourceTeam
            && aiInfoGetLastTarget(obj) == aiRetargetData.target
            && obj != aiRetargetData.source) {
            int rating = _combatai_rating(obj);
            if (rating >= aiRetargetData.sourceRating) {
                aiRetargetData.critterList[aiRetargetData.critterCount] = obj;
                aiRetargetData.ratingList[aiRetargetData.critterCount] = rating;
                aiRetargetData.critterCount += 1;
            }
        }
    }

    // NOTE: Uninline.
    _ai_sort_list_distance(aiRetargetData.critterList, aiRetargetData.critterCount, source);

    if (_cai_retargetTileFromFriendlyFireSubFunc(&aiRetargetData, *tilePtr) == 0) {
        int minDistance = 99999;
        int minDistanceIndex = -1;

        for (int index = 0; index < 32; index++) {
            int tile = tiles[index];
            if (tile == -1) {
                break;
            }

            if (_obj_blocking_at(nullptr, tile, source->elevation) == nullptr) {
                int distance = tileDistanceBetween(*tilePtr, tile);
                if (distance < minDistance) {
                    minDistance = distance;
                    minDistanceIndex = index;
                }
            }
        }

        if (minDistanceIndex != -1) {
            *tilePtr = tiles[minDistanceIndex];
        }
    }

    return 0;
}

// 0x42A410
static int _cai_retargetTileFromFriendlyFireSubFunc(AiRetargetData* aiRetargetData, int tile)
{
    if (aiRetargetData->sourceIntelligence <= 0) {
        return 0;
    }

    int distance = 1;

    for (int index = 0; index < aiRetargetData->critterCount; index++) {
        Object* obj = aiRetargetData->critterList[index];
        if (_cai_attackWouldIntersect(obj, aiRetargetData->target, aiRetargetData->source, tile, &distance)) {
            debugPrint("In the way!");

            aiRetargetData->tiles[aiRetargetData->currentTileIndex] = tileGetTileInDirection(tile, (obj->rotation + 1) % ROTATION_COUNT, distance);
            aiRetargetData->tiles[aiRetargetData->currentTileIndex + 1] = tileGetTileInDirection(tile, (obj->rotation + 5) % ROTATION_COUNT, distance);

            aiRetargetData->sourceIntelligence -= 2;
            aiRetargetData->currentTileIndex += 2;
            break;
        }
    }

    return 0;
}

// 0x42A518
static bool _cai_attackWouldIntersect(Object* attacker, Object* defender, Object* attackerFriend, int tile, int* distance)
{
    int hitMode = HIT_MODE_RIGHT_WEAPON_PRIMARY;
    bool aiming = false;
    if (attacker == gDude) {
        interfaceGetCurrentHitMode(&hitMode, &aiming);
    }

    Object* weapon = critterGetWeaponForHitMode(attacker, hitMode);
    if (weapon == nullptr) {
        return false;
    }

    if (weaponGetRange(attacker, hitMode) < 1) {
        return false;
    }

    Object* object = nullptr;
    _make_straight_path_func(attacker, attacker->tile, defender->tile, nullptr, &object, 32, _obj_shoot_blocking_at);
    if (object != attackerFriend) {
        if (!_combatTestIncidentalHit(attacker, defender, attackerFriend, weapon)) {
            return false;
        }
    }

    return true;
}

// 0x42A5B8
static int _ai_switch_weapons(Object* attacker, int* hitMode, Object** weapon, Object* defender)
{
    *weapon = nullptr;
    *hitMode = HIT_MODE_PUNCH;

    Object* bestWeapon = _ai_search_inven_weap(attacker, true, defender);
    if (bestWeapon != nullptr) {
        *weapon = bestWeapon;
        *hitMode = _ai_pick_hit_mode(attacker, bestWeapon, defender);
    } else {
        Object* nearbyWeapon = _ai_search_environ(attacker, ITEM_TYPE_WEAPON);
        if (nearbyWeapon == nullptr) {
            if (weaponGetActionPointCost(attacker, *hitMode, 0) <= attacker->data.critter.combat.ap) {
                return 0;
            }

            return -1;
        }

        Object* retrievedWeapon = _ai_retrieve_object(attacker, nearbyWeapon);
        if (retrievedWeapon != nullptr) {
            *weapon = retrievedWeapon;
            *hitMode = _ai_pick_hit_mode(attacker, retrievedWeapon, defender);
        }
    }

    if (*weapon != nullptr) {
        _inven_wield(attacker, *weapon, 1);
        _combat_turn_run();
        if (weaponGetActionPointCost(attacker, *hitMode, 0) <= attacker->data.critter.combat.ap) {
            return 0;
        }
    }

    return -1;
}

// 0x42A670
static int _ai_called_shot(Object* attacker, Object* defender, int hitMode)
{
    int hitLocation = HIT_LOCATION_TORSO;

    if (weaponGetActionPointCost(attacker, hitMode, true) <= attacker->data.critter.combat.ap) {
        if (critterCanAim(attacker, hitMode)) {
            AiPacket* ai = aiGetPacket(attacker);
            if (randomBetween(1, ai->called_freq) == 1) {
                int intelligenceRequired;
                switch (settings.preferences.combat_difficulty) {
                case COMBAT_DIFFICULTY_EASY:
                    intelligenceRequired = 7;
                    break;
                case COMBAT_DIFFICULTY_NORMAL:
                    intelligenceRequired = 5;
                    break;
                case COMBAT_DIFFICULTY_HARD:
                    intelligenceRequired = 3;
                    break;
                }

                if (critterGetStat(attacker, STAT_INTELLIGENCE) >= intelligenceRequired) {
                    hitLocation = randomBetween(0, HIT_LOCATION_SPECIFIC_COUNT);
                    int chanceToHit = _determine_to_hit(attacker, defender, hitMode, hitLocation);
                    if (chanceToHit < ai->min_to_hit) {
                        hitLocation = HIT_LOCATION_TORSO;
                    }
                }
            }
        }
    }

    return hitLocation;
}

// 0x42A748
static int _ai_attack(Object* attacker, Object* defender, int hitMode)
{
    if (attacker->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) {
        return -1;
    }

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);
    animationRegisterRotateToTile(attacker, defender->tile);
    reg_anim_end();
    _combat_turn_run();

    int hitLocation = _ai_called_shot(attacker, defender, hitMode);
    if (_combat_attack(attacker, defender, hitMode, hitLocation)) {
        return -1;
    }

    _combat_turn_run();

    return 0;
}

// 0x42A7D8
static int _ai_try_attack(Object* attacker, Object* defender)
{
    _critter_set_who_hit_me(attacker, defender);

    CritterCombatData* combatData = &(attacker->data.critter.combat);
    bool taunt = true;

    Object* weapon = critterGetItem2(attacker);
    if (weapon != nullptr && itemGetType(weapon) != ITEM_TYPE_WEAPON) {
        weapon = nullptr;
    }

    int hitMode = _ai_pick_hit_mode(attacker, weapon, defender);
    int minToHit = aiGetPacket(attacker)->min_to_hit;

    int actionPoints = attacker->data.critter.combat.ap;
    int safeDistance = 0;
    int actionPointsToUse = 0;
    if (weapon != nullptr
        || (critterGetBodyType(defender) == BODY_TYPE_BIPED
            && ((defender->fid & 0xF000) >> 12 == 0)
            && artExists(buildFid(OBJ_TYPE_CRITTER, attacker->fid & 0xFFF, ANIM_THROW_PUNCH, 0, attacker->rotation + 1)))) {
        // SFALL: Check the safety of weapons based on the selected attack mode
        // instead of always the primary weapon hit mode.
        if (_combat_safety_invalidate_weapon(attacker, weapon, hitMode, defender, &safeDistance)) {
            _ai_switch_weapons(attacker, &hitMode, &weapon, defender);
        }
    } else {
        _ai_switch_weapons(attacker, &hitMode, &weapon, defender);
    }

    unsigned char rotations[800];

    Object* ammo = nullptr;
    for (int attempt = 0; attempt < 10; attempt++) {
        if ((combatData->results & (DAM_KNOCKED_OUT | DAM_DEAD | DAM_LOSE_TURN)) != 0) {
            break;
        }

        int reason = _combat_check_bad_shot(attacker, defender, hitMode, false);
        if (reason == COMBAT_BAD_SHOT_NO_AMMO) {
            // out of ammo
            if (aiHaveAmmo(attacker, weapon, &ammo)) {
                int remainingAmmoQuantity = weaponReload(weapon, ammo);
                if (remainingAmmoQuantity == 0 && ammo != nullptr) {
                    _obj_destroy(ammo);
                }

                if (remainingAmmoQuantity != -1) {
                    int volume = _gsound_compute_relative_volume(attacker);
                    const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_READY, weapon, hitMode, nullptr);
                    _gsound_play_sfx_file_volume(sfx, volume);
                    _ai_magic_hands(attacker, weapon, 5002);

                    // SFALL: Fix incorrect AP cost when AI reloads a weapon.
                    // CE: There is a commented out code which checks
                    // available action points before performing reload. Not
                    // sure why it was commented, probably needs additional
                    // testing.
                    int actionPointsRequired = weaponGetActionPointCost(attacker, HIT_MODE_RIGHT_WEAPON_RELOAD, false);
                    if (attacker->data.critter.combat.ap >= actionPointsRequired) {
                        attacker->data.critter.combat.ap -= actionPointsRequired;
                    } else {
                        attacker->data.critter.combat.ap = 0;
                    }
                }
            } else {
                ammo = _ai_search_environ(attacker, ITEM_TYPE_AMMO);
                if (ammo != nullptr) {
                    ammo = _ai_retrieve_object(attacker, ammo);
                    if (ammo != nullptr) {
                        int remainingAmmoQuantity = weaponReload(weapon, ammo);
                        if (remainingAmmoQuantity == 0) {
                            _obj_destroy(ammo);
                        }

                        if (remainingAmmoQuantity != -1) {
                            int volume = _gsound_compute_relative_volume(attacker);
                            const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_READY, weapon, hitMode, nullptr);
                            _gsound_play_sfx_file_volume(sfx, volume);
                            _ai_magic_hands(attacker, weapon, 5002);

                            // SFALL: Fix incorrect AP cost when AI reloads a
                            // weapon.
                            // CE: See note above, probably need to check
                            // available action points before performing
                            // reload.
                            int actionPointsRequired = weaponGetActionPointCost(attacker, HIT_MODE_RIGHT_WEAPON_RELOAD, false);
                            if (attacker->data.critter.combat.ap >= actionPointsRequired) {
                                attacker->data.critter.combat.ap -= actionPointsRequired;
                            } else {
                                attacker->data.critter.combat.ap = 0;
                            }
                        }
                    }
                } else {
                    int volume = _gsound_compute_relative_volume(attacker);
                    const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_OUT_OF_AMMO, weapon, hitMode, nullptr);
                    _gsound_play_sfx_file_volume(sfx, volume);
                    _ai_magic_hands(attacker, weapon, 5001);

                    if (_inven_unwield(attacker, 1) == 0) {
                        _combat_turn_run();
                    }

                    _ai_switch_weapons(attacker, &hitMode, &weapon, defender);
                }
            }
        } else if (reason == COMBAT_BAD_SHOT_NOT_ENOUGH_AP || reason == COMBAT_BAD_SHOT_ARM_CRIPPLED || reason == COMBAT_BAD_SHOT_BOTH_ARMS_CRIPPLED) {
            // 3 - not enough action points
            // 6 - crippled one arm for two-handed weapon
            // 7 - both hands crippled
            if (_ai_switch_weapons(attacker, &hitMode, &weapon, defender) == -1) {
                return -1;
            }
        } else if (reason == COMBAT_BAD_SHOT_OUT_OF_RANGE) {
            // target out of range
            int toHitNoRange = _determine_to_hit_no_range(attacker, defender, HIT_LOCATION_UNCALLED, hitMode, rotations);
            if (toHitNoRange < minToHit) {
                // hit chance is too low even at point blank range (not taking range into account)
                debugPrint("%s: FLEEING: Can't possibly Hit Target!", critterGetName(attacker));
                _ai_run_away(attacker, defender);
                return 0;
            }

            if (weapon != nullptr) {
                if (_ai_move_steps_closer(attacker, defender, actionPoints, taunt) == -1) {
                    return -1;
                }
                taunt = false;
            } else {
                if (_ai_switch_weapons(attacker, &hitMode, &weapon, defender) == -1 || weapon == nullptr) {
                    // NOTE: Uninline.
                    if (_ai_move_closer(attacker, defender, taunt) == -1) {
                        return -1;
                    }
                }
                taunt = false;
            }
        } else if (reason == COMBAT_BAD_SHOT_AIM_BLOCKED) {
            // aim is blocked
            if (_ai_move_steps_closer(attacker, defender, attacker->data.critter.combat.ap, taunt) == -1) {
                return -1;
            }
            taunt = false;
        } else if (reason == COMBAT_BAD_SHOT_OK) {
            int accuracy = _determine_to_hit(attacker, defender, HIT_LOCATION_UNCALLED, hitMode);
            if (safeDistance != 0) {
                if (_ai_move_away(attacker, defender, safeDistance) == -1) {
                    return -1;
                }
            }

            if (accuracy < minToHit) {
                int toHitNoRange = _determine_to_hit_no_range(attacker, defender, HIT_LOCATION_UNCALLED, hitMode, rotations);
                if (toHitNoRange < minToHit) {
                    debugPrint("%s: FLEEING: Can't possibly Hit Target!", critterGetName(attacker));
                    _ai_run_away(attacker, defender);
                    return 0;
                }

                if (actionPoints > 0) {
                    int pathLength = pathfinderFindPath(attacker, attacker->tile, defender->tile, rotations, 0, _obj_blocking_at);
                    if (pathLength == 0) {
                        actionPointsToUse = actionPoints;
                    } else {
                        if (pathLength < actionPoints) {
                            actionPoints = pathLength;
                        }

                        int tile = attacker->tile;
                        int index;
                        for (index = 0; index < actionPoints; index++) {
                            tile = tileGetTileInDirection(tile, rotations[index], 1);

                            actionPointsToUse++;

                            int toHit = _determine_to_hit_from_tile(attacker, tile, defender, HIT_LOCATION_UNCALLED, hitMode);
                            if (toHit >= minToHit) {
                                break;
                            }
                        }

                        if (index == actionPoints) {
                            actionPointsToUse = actionPoints;
                        }
                    }
                }

                if (_ai_move_steps_closer(attacker, defender, actionPointsToUse, taunt) == -1) {
                    debugPrint("%s: FLEEING: Can't possibly get closer to Target!", critterGetName(attacker));
                    _ai_run_away(attacker, defender);
                    return 0;
                }

                taunt = false;
                if (_ai_attack(attacker, defender, hitMode) == -1 || weaponGetActionPointCost(attacker, hitMode, 0) > attacker->data.critter.combat.ap) {
                    return -1;
                }
            } else {
                if (_ai_attack(attacker, defender, hitMode) == -1 || weaponGetActionPointCost(attacker, hitMode, 0) > attacker->data.critter.combat.ap) {
                    return -1;
                }
            }
        }
    }

    return -1;
}

// Something with using flare
//
// 0x42AE90
int _cAIPrepWeaponItem(Object* critter, Object* item)
{
    if (item != nullptr && critterGetStat(critter, STAT_INTELLIGENCE) >= 3 && item->pid == PROTO_ID_FLARE && lightGetAmbientIntensity() < LIGHT_INTENSITY_MAX * 0.85) {
        _protinst_use_item(critter, item);
    }
    return 0;
}

// 0x42AECC
void aiAttemptWeaponReload(Object* critter, int animate)
{
    Object* weapon = critterGetItem2(critter);
    if (weapon == nullptr) {
        return;
    }

    int ammoQuantity = ammoGetQuantity(weapon);
    int ammoCapacity = ammoGetCapacity(weapon);
    if (ammoQuantity < ammoCapacity) {
        Object* ammo;
        if (aiHaveAmmo(critter, weapon, &ammo)) {
            int rc = weaponReload(weapon, ammo);
            if (rc == 0) {
                _obj_destroy(ammo);
            }

            if (rc != -1 && objectIsPartyMember(critter)) {
                int volume = _gsound_compute_relative_volume(critter);
                const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_READY, weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY, nullptr);
                _gsound_play_sfx_file_volume(sfx, volume);

                if (animate) {
                    _ai_magic_hands(critter, weapon, 5002);
                }
            }
        }
    }
}

// 0x42AF78
void _combat_ai_begin(int a1, void* a2)
{
    _curr_crit_num = a1;

    if (a1 != 0) {
        _curr_crit_list = (Object**)internal_malloc(sizeof(Object*) * a1);
        if (_curr_crit_list) {
            memcpy(_curr_crit_list, a2, sizeof(Object*) * a1);
        } else {
            _curr_crit_num = 0;
        }
    }
}

// 0x42AFBC
void _combat_ai_over()
{
    if (_curr_crit_num) {
        internal_free(_curr_crit_list);
    }

    _curr_crit_num = 0;
}

// 0x42AFDC
int _cai_perform_distance_prefs(Object* a1, Object* a2)
{
    if (a1->data.critter.combat.ap <= 0) {
        return -1;
    }

    int distance = aiGetPacket(a1)->distance;

    if (a2 != nullptr) {
        if ((a2->data.critter.combat.ap & DAM_DEAD) != 0) {
            a2 = nullptr;
        }
    }

    switch (distance) {
    case DISTANCE_STAY_CLOSE:
        if (a1->data.critter.combat.whoHitMe != gDude) {
            int distance = objectGetDistanceBetween(a1, gDude);
            if (distance > 5) {
                _ai_move_steps_closer(a1, gDude, distance - 5, false);
            }
        }
        break;
    case DISTANCE_CHARGE:
        if (a2 != nullptr) {
            // NOTE: Uninline.
            _ai_move_closer(a1, a2, 1);
        }
        break;
    case DISTANCE_SNIPE:
        if (a2 != nullptr) {
            // SFALL: Fix AI behavior for "Snipe" distance preference.
            int distance = objectGetDistanceBetween(a1, a2);
            if (distance < 10) {
                int attackCost = weaponGetActionPointCost(a1, HIT_MODE_RIGHT_WEAPON_PRIMARY, false);
                int movementPoints = a1->data.critter.combat.ap - attackCost;
                if (movementPoints > 0) {
                    if (movementPoints + distance - 1 < 5) {
                        int attackerRating = _combatai_rating(a1);
                        int defenderRating = _combatai_rating(a2);
                        if (attackerRating < defenderRating) {
                            _ai_move_away(a1, a2, 10);
                        }
                    }
                } else {
                    _ai_move_away(a1, a2, 10);
                }
            }
        }
        break;
    }

    int tile = a1->tile;
    if (_cai_retargetTileFromFriendlyFire(a1, a2, &tile) == 0 && tile != a1->tile) {
        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        animationRegisterMoveToTile(a1, tile, a1->elevation, a1->data.critter.combat.ap, 0);
        if (reg_anim_end() != 0) {
            return -1;
        }
        _combat_turn_run();
    }

    return 0;
}

// 0x42B100
static int _cai_get_min_hp(AiPacket* ai)
{
    if (ai == nullptr) {
        return 0;
    }

    int run_away_mode = ai->run_away_mode;
    if (run_away_mode >= 0 && run_away_mode < RUN_AWAY_MODE_COUNT) {
        return _hp_run_away_value[run_away_mode];
    } else if (run_away_mode == -1) {
        return ai->min_hp;
    }

    return 0;
}

// 0x42B130
void _combat_ai(Object* a1, Object* a2)
{
    // 0x51820C
    static const int aiPartyMemberDistances[DISTANCE_COUNT] = {
        5,
        7,
        7,
        7,
        50000,
    };

    AiPacket* ai = aiGetPacket(a1);
    int hpRatio = _cai_get_min_hp(ai);
    if (ai->run_away_mode != -1) {
        int v7 = critterGetStat(a1, STAT_MAXIMUM_HIT_POINTS) * hpRatio / 100;
        int minimumHitPoints = critterGetStat(a1, STAT_MAXIMUM_HIT_POINTS) - v7;
        int currentHitPoints = critterGetStat(a1, STAT_CURRENT_HIT_POINTS);
        const char* name = critterGetName(a1);
        debugPrint("\n%s minHp = %d; curHp = %d", name, minimumHitPoints, currentHitPoints);
    }

    CritterCombatData* combatData = &(a1->data.critter.combat);
    if ((combatData->maneuver & CRITTER_MANUEVER_FLEEING) != 0
        || (combatData->results & ai->hurt_too_much) != 0
        || critterGetStat(a1, STAT_CURRENT_HIT_POINTS) < ai->min_hp) {
        debugPrint("%s: FLEEING: I'm Hurt!", critterGetName(a1));
        _ai_run_away(a1, a2);
        return;
    }

    if (_ai_check_drugs(a1)) {
        debugPrint("%s: FLEEING: I need DRUGS!", critterGetName(a1));
        _ai_run_away(a1, a2);
    } else {
        if (a2 == nullptr) {
            a2 = _ai_danger_source(a1);
        }

        _cai_perform_distance_prefs(a1, a2);

        if (a2 != nullptr) {
            _ai_try_attack(a1, a2);
        }
    }

    if (a2 != nullptr
        && (a2->data.critter.combat.results & DAM_DEAD) == 0
        && a1->data.critter.combat.ap != 0
        && objectGetDistanceBetween(a1, a2) > ai->max_dist) {
        Object* friendlyDead = aiInfoGetFriendlyDead(a1);
        if (friendlyDead != nullptr) {
            _ai_move_away(a1, friendlyDead, 10);
            aiInfoSetFriendlyDead(a1, nullptr);
        } else {
            int perception = critterGetStat(a1, STAT_PERCEPTION);
            if (!_ai_find_friend(a1, perception * 2, 5)) {
                combatData->maneuver |= CRITTER_MANEUVER_DISENGAGING;
            }
        }
    }

    if (a2 == nullptr && !objectIsPartyMember(a1)) {
        Object* whoHitMe = combatData->whoHitMe;
        if (whoHitMe != nullptr) {
            if ((whoHitMe->data.critter.combat.results & DAM_DEAD) == 0 && combatData->damageLastTurn > 0) {
                Object* friendlyDead = aiInfoGetFriendlyDead(a1);
                if (friendlyDead != nullptr) {
                    _ai_move_away(a1, friendlyDead, 10);
                    aiInfoSetFriendlyDead(a1, nullptr);
                } else {
                    debugPrint("%s: FLEEING: Somebody is shooting at me that I can't see!", critterGetName(a1));
                    _ai_run_away(a1, nullptr);
                }
            }
        }
    }

    Object* friendlyDead = aiInfoGetFriendlyDead(a1);
    if (friendlyDead != nullptr) {
        _ai_move_away(a1, friendlyDead, 10);
        if (objectGetDistanceBetween(a1, friendlyDead) >= 10) {
            aiInfoSetFriendlyDead(a1, nullptr);
        }
    }

    Object* nearestTeammate;
    int maxTeammateDistance = 5;
    if (a1->data.critter.combat.team != 0) {
        nearestTeammate = _ai_find_nearest_team_in_combat(a1, a1, 1);
    } else {
        nearestTeammate = gDude;
        if (objectIsPartyMember(a1)) {
            // NOTE: Uninline
            int distance = aiGetDistance(a1);
            if (distance != -1) {
                maxTeammateDistance = aiPartyMemberDistances[distance];
            }
        }
    }

    if (a2 == nullptr && nearestTeammate != nullptr && objectGetDistanceBetween(a1, nearestTeammate) > maxTeammateDistance) {
        int currentDistance = objectGetDistanceBetween(a1, nearestTeammate);
        _ai_move_steps_closer(a1, nearestTeammate, currentDistance - maxTeammateDistance, false);
    } else {
        if (a1->data.critter.combat.ap > 0) {
            debugPrint("\n>>>NOTE: %s had extra AP's to use!<<<", critterGetName(a1));
            _cai_perform_distance_prefs(a1, a2);
        }
    }
}

// 0x42B3FC
bool _combatai_want_to_join(Object* a1)
{
    _process_bk();

    if ((a1->flags & OBJECT_HIDDEN) != 0) {
        return false;
    }

    if (a1->elevation != gDude->elevation) {
        return false;
    }

    if ((a1->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        return false;
    }

    if (a1->data.critter.combat.damageLastTurn > 0) {
        return true;
    }

    if (a1->sid != -1) {
        scriptSetObjects(a1->sid, nullptr, nullptr);
        scriptSetFixedParam(a1->sid, 5);
        scriptExecProc(a1->sid, SCRIPT_PROC_COMBAT);
    }

    if ((a1->data.critter.combat.maneuver & CRITTER_MANEUVER_ENGAGING) != 0) {
        return true;
    }

    if ((a1->data.critter.combat.maneuver & CRITTER_MANEUVER_DISENGAGING) != 0) {
        return false;
    }

    if ((a1->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0) {
        return false;
    }

    if (_ai_danger_source(a1) == nullptr) {
        return false;
    }

    return true;
}

// 0x42B4A8
bool _combatai_want_to_stop(Object* a1)
{
    _process_bk();

    if ((a1->data.critter.combat.maneuver & CRITTER_MANEUVER_DISENGAGING) != 0) {
        return true;
    }

    if ((a1->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD)) != 0) {
        return true;
    }

    if ((a1->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0) {
        return true;
    }

    Object* enemy = _ai_danger_source(a1);
    return enemy == nullptr || !isWithinPerception(a1, enemy);
}

// 0x42B504
int critterSetTeam(Object* obj, int team)
{
    if (PID_TYPE(obj->pid) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    obj->data.critter.combat.team = team;

    if (obj->data.critter.combat.whoHitMeCid == -1) {
        _critter_set_who_hit_me(obj, nullptr);
        debugPrint("\nError: CombatData found with invalid who_hit_me!");
        return -1;
    }

    Object* whoHitMe = obj->data.critter.combat.whoHitMe;
    if (whoHitMe != nullptr) {
        if (whoHitMe->data.critter.combat.team == team) {
            _critter_set_who_hit_me(obj, nullptr);
        }
    }

    aiInfoSetLastTarget(obj, nullptr);

    if (isInCombat()) {
        bool outlineWasEnabled = obj->outline != 0 && (obj->outline & OUTLINE_DISABLED) == 0;

        objectClearOutline(obj, nullptr);

        int outlineType;
        if (obj->data.critter.combat.team == gDude->data.critter.combat.team) {
            outlineType = OUTLINE_TYPE_2;
        } else {
            outlineType = OUTLINE_TYPE_HOSTILE;
        }

        objectSetOutline(obj, outlineType, nullptr);

        if (outlineWasEnabled) {
            Rect rect;
            objectEnableOutline(obj, &rect);
            tileWindowRefreshRect(&rect, obj->elevation);
        }
    }

    return 0;
}

// 0x42B5D4
int critterSetAiPacket(Object* object, int aiPacket)
{
    if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    object->data.critter.combat.aiPacket = aiPacket;

    if (_isPotentialPartyMember(object)) {
        Proto* proto;
        if (protoGetProto(object->pid, &proto) == -1) {
            return -1;
        }

        proto->critter.aiPacket = aiPacket;
    }

    return 0;
}

// combatai_msg
// 0x42B634
int _combatai_msg(Object* critter, Attack* attack, int type, int delay)
{
    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    if (!settings.preferences.combat_taunts) {
        return -1;
    }

    if (critter == gDude) {
        return -1;
    }

    if ((critter->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        return -1;
    }

    AiPacket* ai = aiGetPacket(critter);

    debugPrint("%s is using %s packet with a %d%% chance to taunt\n", objectGetName(critter), ai->name, ai->chance);

    if (randomBetween(1, 100) > ai->chance) {
        return -1;
    }

    int start;
    int end;
    char* string;

    switch (type) {
    case AI_MESSAGE_TYPE_RUN:
        start = ai->run.start;
        end = ai->run.end;
        string = _attack_str;
        break;
    case AI_MESSAGE_TYPE_MOVE:
        start = ai->move.start;
        end = ai->move.end;
        string = _attack_str;
        break;
    case AI_MESSAGE_TYPE_ATTACK:
        start = ai->attack.start;
        end = ai->attack.end;
        string = _attack_str;
        break;
    case AI_MESSAGE_TYPE_MISS:
        start = ai->miss.start;
        end = ai->miss.end;
        string = _target_str;
        break;
    case AI_MESSAGE_TYPE_HIT:
        start = ai->hit[attack->defenderHitLocation].start;
        end = ai->hit[attack->defenderHitLocation].end;
        string = _target_str;
        break;
    default:
        return -1;
    }

    if (end < start) {
        return -1;
    }

    MessageListItem messageListItem;
    messageListItem.num = randomBetween(start, end);
    if (!messageListGetItem(&gCombatAiMessageList, &messageListItem)) {
        debugPrint("\nERROR: combatai_msg: Couldn't find message # %d for %s", messageListItem.num, critterGetName(critter));
        return -1;
    }

    debugPrint("%s said message %d\n", objectGetName(critter), messageListItem.num);
    snprintf(string, AI_MESSAGE_SIZE, "%s", messageListItem.text);

    // TODO: Get rid of casts.
    return animationRegisterCallback(critter, (void*)type, (AnimationCallback*)_ai_print_msg, delay);
}

// 0x42B80C
static int _ai_print_msg(Object* critter, int type)
{
    if (textObjectsGetCount() > 0) {
        return 0;
    }

    char* string;
    switch (type) {
    case AI_MESSAGE_TYPE_HIT:
    case AI_MESSAGE_TYPE_MISS:
        string = _target_str;
        break;
    default:
        string = _attack_str;
        break;
    }

    AiPacket* ai = aiGetPacket(critter);

    Rect rect;
    if (textObjectAdd(critter, string, ai->font, ai->color, ai->outline_color, &rect) == 0) {
        tileWindowRefreshRect(&rect, critter->elevation);
    }

    return 0;
}

// Returns random critter for attacking as a result of critical weapon failure.
//
// 0x42B868
Object* _combat_ai_random_target(Attack* attack)
{
    // Looks like this function does nothing because it's result is not used. I
    // suppose it was planned to use range as a condition below, but it was
    // later moved into 0x426614, but remained here.
    weaponGetRange(attack->attacker, attack->hitMode);

    Object* critter = nullptr;

    if (_curr_crit_num != 0) {
        // Randomize starting critter.
        int start = randomBetween(0, _curr_crit_num - 1);
        int index = start;
        while (true) {
            Object* obj = _curr_crit_list[index];
            if (obj != attack->attacker
                && obj != attack->defender
                && _can_see(attack->attacker, obj)
                && _combat_check_bad_shot(attack->attacker, obj, attack->hitMode, false) == COMBAT_BAD_SHOT_OK) {
                critter = obj;
                break;
            }

            index += 1;
            if (index == _curr_crit_num) {
                index = 0;
            }

            if (index == start) {
                break;
            }
        }
    }

    return critter;
}

// 0x42B90C
static int _combatai_rating(Object* obj)
{
    int melee_damage;
    Object* item;
    int weapon_damage_min;
    int weapon_damage_max;

    if (obj == nullptr) {
        return 0;
    }

    if (FID_TYPE(obj->fid) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    if ((obj->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        return 0;
    }

    melee_damage = critterGetStat(obj, STAT_MELEE_DAMAGE);

    item = critterGetItem2(obj);
    if (item != nullptr && itemGetType(item) == ITEM_TYPE_WEAPON && weaponGetDamageMinMax(item, &weapon_damage_min, &weapon_damage_max) != -1 && melee_damage < weapon_damage_max) {
        melee_damage = weapon_damage_max;
    }

    item = critterGetItem1(obj);
    if (item != nullptr && itemGetType(item) == ITEM_TYPE_WEAPON && weaponGetDamageMinMax(item, &weapon_damage_min, &weapon_damage_max) != -1 && melee_damage < weapon_damage_max) {
        melee_damage = weapon_damage_max;
    }

    return melee_damage + critterGetStat(obj, STAT_ARMOR_CLASS);
}

// 0x42B9D4
void _combatai_check_retaliation(Object* a1, Object* a2)
{
    Object* whoHitMe = a1->data.critter.combat.whoHitMe;
    if (whoHitMe != nullptr) {
        int candidateRating = _combatai_rating(a2);
        int whoHitMeRating = _combatai_rating(whoHitMe);
        if (candidateRating > whoHitMeRating) {
            _critter_set_who_hit_me(a1, a2);
        }
    } else {
        _critter_set_who_hit_me(a1, a2);
    }
}

// 0x42BA04
bool isWithinPerception(Object* critter, Object* target)
{
    if (target == nullptr) {
        return false;
    }

    int distance = objectGetDistanceBetween(target, critter);
    int perception = critterGetStat(critter, STAT_PERCEPTION);
    int sneak = skillGetValue(target, SKILL_SNEAK);
    if (_can_see(critter, target)) {
        int maxDistance = perception * 5;
        if ((target->flags & OBJECT_TRANS_GLASS) != 0) {
            maxDistance /= 2;
        }

        if (target == gDude) {
            if (dudeIsSneaking()) {
                maxDistance /= 4;
                if (sneak > 120) {
                    maxDistance -= 1;
                }
            } else if (dudeHasState(DUDE_STATE_SNEAKING)) {
                maxDistance = maxDistance * 2 / 3;
            }
        }

        if (distance <= maxDistance) {
            return true;
        }
    }

    int maxDistance;
    if (isInCombat()) {
        maxDistance = perception * 2;
    } else {
        maxDistance = perception;
    }

    if (target == gDude) {
        if (dudeIsSneaking()) {
            maxDistance /= 4;
            if (sneak > 120) {
                maxDistance -= 1;
            }
        } else if (dudeHasState(DUDE_STATE_SNEAKING)) {
            maxDistance = maxDistance * 2 / 3;
        }
    }

    if (distance <= maxDistance) {
        return true;
    }

    return false;
}

// Load combatai.msg and apply language filter.
//
// 0x42BB34
static int aiMessageListInit()
{
    if (!messageListInit(&gCombatAiMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "combatai.msg");

    if (!messageListLoad(&gCombatAiMessageList, path)) {
        return -1;
    }

    if (settings.preferences.language_filter) {
        messageListFilterBadwords(&gCombatAiMessageList);
    }

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_COMBAT_AI, &gCombatAiMessageList);

    return 0;
}

// NOTE: Inlined.
//
// 0x42BBD8
static int aiMessageListFree()
{
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_COMBAT_AI, nullptr);
    if (!messageListFree(&gCombatAiMessageList)) {
        return -1;
    }

    return 0;
}

// 0x42BBF0
void aiMessageListReloadIfNeeded()
{
    int languageFilter = static_cast<int>(settings.preferences.language_filter);

    if (languageFilter != gLanguageFilter) {
        gLanguageFilter = languageFilter;

        if (languageFilter == 1) {
            messageListFilterBadwords(&gCombatAiMessageList);
        } else {
            // NOTE: Uninline.
            aiMessageListFree();

            aiMessageListInit();
        }
    }
}

// 0x42BC60
void _combatai_notify_onlookers(Object* a1)
{
    for (int index = 0; index < _curr_crit_num; index++) {
        Object* obj = _curr_crit_list[index];
        if ((obj->data.critter.combat.maneuver & CRITTER_MANEUVER_ENGAGING) == 0) {
            if (isWithinPerception(obj, a1)) {
                obj->data.critter.combat.maneuver |= CRITTER_MANEUVER_ENGAGING;
                if ((a1->data.critter.combat.results & DAM_DEAD) != 0) {
                    if (!isWithinPerception(obj, a1->data.critter.combat.whoHitMe)) {
                        debugPrint("\nSomebody Died and I don't know why!  Run!!!");
                        aiInfoSetFriendlyDead(obj, a1);
                    }
                }
            }
        }
    }
}

// 0x42BCD4
void _combatai_notify_friends(Object* a1)
{
    int team = a1->data.critter.combat.team;

    for (int index = 0; index < _curr_crit_num; index++) {
        Object* obj = _curr_crit_list[index];
        if ((obj->data.critter.combat.maneuver & CRITTER_MANEUVER_ENGAGING) == 0 && team == obj->data.critter.combat.team) {
            if (isWithinPerception(obj, a1)) {
                obj->data.critter.combat.maneuver |= CRITTER_MANEUVER_ENGAGING;
            }
        }
    }
}

// 0x42BD28
void _combatai_delete_critter(Object* obj)
{
    // TODO: Check entire function.
    for (int i = 0; i < _curr_crit_num; i++) {
        if (obj == _curr_crit_list[i]) {
            _curr_crit_num--;
            _curr_crit_list[i] = _curr_crit_list[_curr_crit_num];
            _curr_crit_list[_curr_crit_num] = obj;
            break;
        }
    }
}

} // namespace fallout
