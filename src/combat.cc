#include "combat.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat_ai.h"
#include "critter.h"
#include "db.h"
#include "debug.h"
#include "display_monitor.h"
#include "draw.h"
#include "elevator.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "input.h"
#include "interface.h"
#include "item.h"
#include "kb.h"
#include "loadsave.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "pipboy.h"
#include "platform_compat.h"
#include "proto.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "sfall_config.h"
#include "sfall_global_scripts.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "trait.h"
#include "window_manager.h"

namespace fallout {

#define CALLED_SHOT_WINDOW_Y (20)
#define CALLED_SHOT_WINDOW_WIDTH (504)
#define CALLED_SHOT_WINDOW_HEIGHT (309)

typedef enum DamageCalculationType {
    DAMAGE_CALCULATION_TYPE_VANILLA = 0,
    DAMAGE_CALCULATION_TYPE_GLOVZ = 1,
    DAMAGE_CALCULATION_TYPE_GLOVZ_WITH_DAMAGE_MULTIPLIER_TWEAK = 2,
    DAMAGE_CALCULATION_TYPE_YAAM = 5,
} DamageCalculationType;

typedef struct CombatAiInfo {
    Object* friendlyDead;
    Object* lastTarget;
    Object* lastItem;
    int lastMove;
} CombatAiInfo;

typedef struct UnarmedHitDescription {
    int requiredLevel;
    int requiredSkill;
    int requiredStats[PRIMARY_STAT_COUNT];
    int minDamage;
    int maxDamage;
    int bonusDamage;
    int bonusCriticalChance;
    int actionPointCost;
    bool isPenetrate;
    bool isSecondary;
} UnarmedHitDescription;

typedef struct DamageCalculationContext {
    Attack* attack;
    int* damagePtr;
    int ammoQuantity;
    int damageResistance;
    int damageThreshold;
    int damageBonus;
    int bonusDamageMultiplier;
    int combatDifficultyDamageModifier;
} DamageCalculationContext;

static bool _combat_safety_invalidate_weapon_func(Object* attacker, Object* weapon, int hitMode, Object* defender, int* safeDistancePtr, Object* attackerFriend);
static void _combatInitAIInfoList();
static int aiInfoCopy(int srcIndex, int destIndex);
static int _combatAIInfoSetLastMove(Object* object, int move);
static void _combat_begin(Object* a1);
static void _combat_begin_extra(Object* a1);
static void _combat_update_critters_in_los(bool a1);
static void _combat_over();
static void _combat_add_noncoms();
static int _compare_faster(const void* a1, const void* a2);
static void _combat_sequence_init(Object* a1, Object* a2);
static void _combat_sequence();
static void combatAttemptEnd();
static int _combat_input();
static void _combat_set_move_all();
static int _combat_turn(Object* a1, bool a2);
static bool _combat_should_end();
static bool _check_ranged_miss(Attack* attack);
static int _shoot_along_path(Attack* attack, int endTile, int rounds, int anim);
static int _compute_spray(Attack* attack, int accuracy, int* roundsHitMainTargetPtr, int* roundsSpentPtr, int anim);
static int attackComputeEnhancedKnockout(Attack* attack);
static int attackCompute(Attack* attack);
static int attackComputeCriticalHit(Attack* a1);
static int _attackFindInvalidFlags(Object* a1, Object* a2);
static int attackComputeCriticalFailure(Attack* attack);
static void _do_random_cripple(int* flagsPtr);
static int attackDetermineToHit(Object* attacker, int tile, Object* defender, int hitLocation, int hitMode, bool useDistance);
static void attackComputeDamage(Attack* attack, int ammoQuantity, int a3);
static void _check_for_death(Object* a1, int a2, int* a3);
static void _set_new_results(Object* a1, int a2);
static void _damage_object(Object* a1, int damage, bool animated, int a4, Object* a5);
static void combatCopyDamageAmountDescription(char* dest, size_t size, Object* critter_obj, int damage);
static void combatAddDamageFlagsDescription(char* a1, int flags, Object* a3);
static void _combat_standup(Object* a1);
static void _print_tohit(unsigned char* dest, int dest_pitch, int a3);
static char* hitLocationGetName(Object* critter, int hitLocation);
static void _draw_loc_off(int a1, int a2);
static void _draw_loc_on_(int a1, int a2);
static void _draw_loc_(int eventCode, int color);
static int calledShotSelectHitLocation(Object* critter, int* hitLocation, int hitMode);

static void criticalsInit();
static void criticalsReset();
static void criticalsExit();
static void burstModInit();
static int burstModComputeRounds(int totalRounds, int* centerRoundsPtr, int* leftRoundsPtr, int* rightRoundsPtr);
static void unarmedInit();
static void unarmedInitVanilla();
static void unarmedInitCustom();
static int unarmedGetHitModeInRange(int firstHitMode, int lastHitMode, bool isSecondary);
static void damageModInit();
static void damageModCalculateGlovz(DamageCalculationContext* context);
static int damageModGlovzDivRound(int dividend, int divisor);
static void damageModCalculateYaam(DamageCalculationContext* context);

// 0x500B50
static char _a_1[] = ".";

// 0x51093C
static int _combat_turn_running = 0;

// 0x510940
int _combatNumTurns = 0;

// 0x510944
unsigned int gCombatState = COMBAT_STATE_0x02;

// 0x510948
static CombatAiInfo* _aiInfoList = NULL;

// 0x51094C
static STRUCT_664980* _gcsd = NULL;

// 0x510950
static bool _combat_call_display = false;

// Accuracy modifiers for hit locations.
//
// 0x510954
static int hit_location_penalty_default[HIT_LOCATION_COUNT] = {
    -40,
    -30,
    -30,
    0,
    -20,
    -20,
    -60,
    -30,
    0,
};

static int hit_location_penalty[HIT_LOCATION_COUNT];

// Critical hit tables for every kill type.
//
// 0x510978
static CriticalHitDescription gCriticalHitTables[SFALL_KILL_TYPE_COUNT][HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT] = {
    // KILL_TYPE_MAN
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5002, 5003 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5002, 5003 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5004, 5003 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, STAT_LUCK, 0, DAM_BLIND, 5005, 5006 },
            { 6, DAM_DEAD, -1, 0, 0, 5007, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5008, 5000 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 5009, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_LEFT, 5010, 5011 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5012, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5012, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5013, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5008, 5000 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 5009, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_RIGHT, 5014, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5015, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5015, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5013, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5016, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5017, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5019, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5019, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5020, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5021, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5023, 5000 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5025, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5025, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5026, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5023, 5000 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5025, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5025, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5026, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, STAT_LUCK, 4, DAM_BLIND, 5027, 5028 },
            { 4, DAM_BYPASS, STAT_LUCK, 3, DAM_BLIND, 5029, 5028 },
            { 6, DAM_BYPASS, STAT_LUCK, 2, DAM_BLIND, 5029, 5028 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5030, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5031, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5032, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5033, 5000 },
            { 3, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5034, 5035 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5035, 5036 },
            { 3, DAM_KNOCKED_OUT, -1, 0, 0, 5036, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5035, 5036 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5037, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5016, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5017, 5000 },
            { 4, 0, -1, 0, 0, 5018, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5019, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5020, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5021, 5000 },
        },
    },
    // KILL_TYPE_WOMAN
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5101, 5100 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5102, 5103 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5102, 5103 },
            { 6, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5104, 5103 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, STAT_LUCK, 0, DAM_BLIND, 5105, 5106 },
            { 6, DAM_DEAD, -1, 0, 0, 5107, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5108, 5100 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 5109, 5100 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_LEFT, 5110, 5111 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_LEFT, 5110, 5111 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5112, 5100 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5113, 5100 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5108, 5100 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 5109, 5100 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_RIGHT, 5114, 5100 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_RIGHT, 5114, 5100 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5115, 5100 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5113, 5100 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5116, 5100 },
            { 3, DAM_BYPASS, -1, 0, 0, 5117, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5119, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5119, 5100 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5120, 5100 },
            { 6, DAM_DEAD, -1, 0, 0, 5121, 5100 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5123, 5100 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5123, 5124 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 5123, 5124 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5125, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5125, 5126 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5126, 5100 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5123, 5100 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5123, 5124 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 5123, 5124 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5125, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5125, 5126 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5126, 5100 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, STAT_LUCK, 4, DAM_BLIND, 5127, 5128 },
            { 4, DAM_BYPASS, STAT_LUCK, 3, DAM_BLIND, 5129, 5128 },
            { 6, DAM_BYPASS, STAT_LUCK, 2, DAM_BLIND, 5129, 5128 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5130, 5100 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5131, 5100 },
            { 8, DAM_DEAD, -1, 0, 0, 5132, 5100 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5133, 5100 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5133, 5134 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5134, 5135 },
            { 3, DAM_KNOCKED_OUT, -1, 0, 0, 5135, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5134, 5135 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5135, 5100 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5116, 5100 },
            { 3, DAM_BYPASS, -1, 0, 0, 5117, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5119, 5100 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5119, 5100 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5120, 5100 },
            { 6, DAM_DEAD, -1, 0, 0, 5121, 5100 },
        },
    },
    // KILL_TYPE_CHILD
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5200, 5201 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, -2, DAM_KNOCKED_OUT, 5202, 5203 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, -2, DAM_KNOCKED_OUT, 5202, 5203 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5203, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5203, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5204, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5205, 5000 },
            { 4, DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5206, 5207 },
            { 4, DAM_LOSE_TURN, STAT_ENDURANCE, -2, DAM_CRIP_ARM_LEFT, 5206, 5207 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5209, 5000 },
            { 4, DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5206, 5207 },
            { 4, DAM_LOSE_TURN, STAT_ENDURANCE, -2, DAM_CRIP_ARM_RIGHT, 5206, 5207 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5208, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5210, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5211, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5212, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5212, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5213, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5214, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5215, 5000 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, DAM_CRIP_ARM_RIGHT | DAM_BLIND | DAM_ON_FIRE | DAM_EXPLODE, 5000, 0 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, DAM_CRIP_ARM_RIGHT | DAM_BLIND | DAM_ON_FIRE | DAM_EXPLODE, 5000, 0 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5215, 5000 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, DAM_CRIP_ARM_RIGHT | DAM_BLIND | DAM_ON_FIRE | DAM_EXPLODE, 5000, 0 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, DAM_CRIP_ARM_RIGHT | DAM_BLIND | DAM_ON_FIRE | DAM_EXPLODE, 5000, 0 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5217, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, STAT_LUCK, 5, DAM_BLIND, 5218, 5219 },
            { 4, DAM_BYPASS, STAT_LUCK, 2, DAM_BLIND, 5220, 5221 },
            { 6, DAM_BYPASS, STAT_LUCK, -1, DAM_BLIND, 5220, 5221 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5222, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5223, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5224, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5225, 5000 },
            { 3, 0, -1, 0, 0, 5225, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5226, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5226, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5226, 5000 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5226, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5210, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5211, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5211, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5212, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5213, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5214, 5000 },
        },
    },
    // KILL_TYPE_SUPER_MUTANT
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5300, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, -1, DAM_KNOCKED_DOWN, 5301, 5302 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -4, DAM_KNOCKED_DOWN, 5301, 5302 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5302, 5303 },
            { 6, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5302, 5303 },
            { 6, DAM_DEAD, -1, 0, 0, 5304, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_LOSE_TURN, 5300, 5306 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -1, DAM_CRIP_ARM_LEFT, 5307, 5308 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -3, DAM_CRIP_ARM_LEFT, 5307, 5308 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5308, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5308, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_LOSE_TURN, 5300, 5006 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -1, DAM_CRIP_ARM_RIGHT, 5307, 5309 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -3, DAM_CRIP_ARM_RIGHT, 5307, 5309 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5309, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5309, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5301, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5302, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5302, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5310, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5311, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5300, 5312 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 5312, 5313 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5313, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5314, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5315, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5300, 5312 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 5312, 5313 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 5313, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5314, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5315, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5300, 5000 },
            { 4, DAM_BYPASS, STAT_LUCK, 5, DAM_BLIND, 5316, 5317 },
            { 6, DAM_BYPASS, STAT_LUCK, 3, DAM_BLIND, 5316, 5317 },
            { 6, DAM_BYPASS | DAM_LOSE_TURN, STAT_LUCK, 0, DAM_BLIND, 5318, 5319 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5320, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5321, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, 0, STAT_LUCK, 0, DAM_BYPASS, 5300, 5017 },
            { 3, DAM_BYPASS, STAT_ENDURANCE, -2, DAM_KNOCKED_DOWN, 5301, 5302 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5312, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5302, 5303 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5303, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5300, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5301, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5302, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5302, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5310, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5311, 5000 },
        },
    },
    // KILL_TYPE_GHOUL
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5400, 5003 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -1, DAM_KNOCKED_OUT, 5400, 5003 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -2, DAM_KNOCKED_OUT, 5004, 5005 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, STAT_STRENGTH, 0, 0, 5005, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5401, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5016, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_DROP | DAM_LOSE_TURN, 5001, 5402 },
            { 4, DAM_DROP | DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5402, 5012 },
            { 4, DAM_BYPASS | DAM_DROP | DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5403, 5404 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS | DAM_DROP, -1, 0, 0, 5404, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS | DAM_DROP, -1, 0, 0, 5404, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5016, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_DROP | DAM_LOSE_TURN, 5001, 5402 },
            { 4, DAM_DROP | DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5402, 5015 },
            { 4, DAM_BYPASS | DAM_DROP | DAM_LOSE_TURN, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5403, 5404 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS | DAM_DROP, -1, 0, 0, 5404, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS | DAM_DROP, -1, 0, 0, 5404, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5017, 5000 },
            { 3, 0, -1, 0, 0, 5018, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5004, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5003, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5007, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5023 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5024, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5024, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5026, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5023 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 5024, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5024, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5026, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, STAT_LUCK, 3, DAM_BLIND, 5001, 5405 },
            { 4, DAM_BYPASS, STAT_LUCK, 0, DAM_BLIND, 5406, 5407 },
            { 6, DAM_BYPASS, STAT_LUCK, -3, DAM_BLIND, 5406, 5407 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5030, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5031, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5408, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_LUCK, 0, DAM_BYPASS, 5001, 5033 },
            { 3, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5033, 5035 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5004, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5035, 5036 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5036, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5017, 5000 },
            { 3, 0, -1, 0, 0, 5018, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5004, 5000 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 5003, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5007, 5000 },
        },
    },
    // KILL_TYPE_BRAHMIN
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 5, 0, STAT_ENDURANCE, 2, DAM_KNOCKED_DOWN, 5016, 5500 },
            { 5, 0, STAT_ENDURANCE, -1, DAM_KNOCKED_DOWN, 5016, 5500 },
            { 6, DAM_KNOCKED_OUT, STAT_STRENGTH, 0, 0, 5501, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5502, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5016, 5503 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5016, 5503 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5503, 5000 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5503, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5016, 5503 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5016, 5503 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5503, 5000 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5503, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5504, 5000 },
            { 3, 0, -1, 0, 0, 5504, 5000 },
            { 4, 0, -1, 0, 0, 5504, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5505, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5505, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5506, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5016, 5503 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5016, 5503 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5503, 5000 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5503, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5016, 5503 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5016, 5503 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5503, 5000 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5503, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, STAT_LUCK, 0, DAM_BLIND, 5029, 5507 },
            { 6, DAM_BYPASS, STAT_LUCK, -3, DAM_BLIND, 5029, 5507 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5508, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5509, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5510, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5511, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5511, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5512, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5512, 5000 },
            { 6, DAM_BYPASS, -1, 0, 0, 5513, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5504, 5000 },
            { 3, 0, -1, 0, 0, 5504, 5000 },
            { 4, 0, -1, 0, 0, 5504, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5505, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5505, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5506, 5000 },
        },
    },
    // KILL_TYPE_RADSCORPION
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 3, DAM_KNOCKED_DOWN, 5001, 5600 },
            { 5, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5001, 5600 },
            { 5, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5001, 5600 },
            { 6, DAM_KNOCKED_DOWN, -1, 0, 0, 5600, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5601, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5016, 5602 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5602, 5000 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5602, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, 2, DAM_CRIP_ARM_RIGHT, 5016, 5603 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5016, 5603 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5603, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5604, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5605, 5000 },
            { 4, DAM_BYPASS, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5605, 5606 },
            { 4, DAM_DEAD, -1, 0, 0, 5607, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 2, 0, 5001, 5600 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5600, 5608 },
            { 4, DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5609, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5608, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 5608, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 2, 0, 5001, 5600 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5600, 5008 },
            { 4, DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5609, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5608, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 5608, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_AGILITY, 3, DAM_BLIND, 5001, 5610 },
            { 6, 0, STAT_AGILITY, 0, DAM_BLIND, 5016, 5610 },
            { 6, 0, STAT_AGILITY, -3, DAM_BLIND, 5016, 5610 },
            { 8, 0, STAT_AGILITY, -3, DAM_BLIND, 5611, 5612 },
            { 8, DAM_DEAD, -1, 0, 0, 5613, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5614, 5000 },
            { 3, 0, -1, 0, 0, 5614, 5000 },
            { 4, 0, -1, 0, 0, 5614, 5000 },
            { 4, DAM_KNOCKED_OUT, -1, 0, 0, 5615, 5000 },
            { 4, DAM_KNOCKED_OUT, -1, 0, 0, 5615, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5616, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 5604, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5605, 5000 },
            { 4, DAM_BYPASS, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5605, 5606 },
            { 4, DAM_DEAD, -1, 0, 0, 5607, 5000 },
        },
    },
    // KILL_TYPE_RAT
    {
        // HIT_LOCATION_HEAD
        {
            { 4, DAM_BYPASS, -1, 0, 0, 5700, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5700, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5701, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5701, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5701, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5701, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
            { 3, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
            { 3, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5703, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
            { 3, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
            { 3, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5705, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5706, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5708, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
            { 3, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
            { 3, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5709, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
            { 3, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
            { 3, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 5710, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, DAM_BYPASS, -1, 0, 0, 5711, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5712, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5712, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5712, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5712, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5712, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5711, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5711, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5712, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 5712, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5706, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5707, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5708, 5000 },
        },
    },
    // KILL_TYPE_FLOATER
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 5, 0, STAT_AGILITY, -3, DAM_KNOCKED_DOWN, 5016, 5800 },
            { 5, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 5800, 5801 },
            { 6, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 5800, 5801 },
            { 6, DAM_DEAD, -1, 0, 0, 5802, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_LOSE_TURN, 5001, 5803 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_LOSE_TURN, 5001, 5803 },
            { 3, DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5804, 5000 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5804, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5805, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_LOSE_TURN, 5001, 5803 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_LOSE_TURN, 5001, 5803 },
            { 3, DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5804, 5000 },
            { 4, DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5804, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5805, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 3, 0, STAT_AGILITY, -2, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5800, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5804, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5805, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 1, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 4, 0, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -1, DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT, 5800, 5806 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT, 5804, 5806 },
            { 6, DAM_DEAD | DAM_ON_FIRE, -1, 0, 0, 5807, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_LOSE_TURN, -1, 0, 0, 5803, 5000 },
            { 4, DAM_LOSE_TURN, -1, 0, 0, 5803, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT | DAM_LOSE_TURN, -1, 0, 0, 5808, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT | DAM_LOSE_TURN, -1, 0, 0, 5808, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5809, 5000 },
            { 5, 0, STAT_ENDURANCE, 0, DAM_BLIND, 5016, 5810 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_BLIND, 5809, 5810 },
            { 6, DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5810, 5000 },
            { 6, DAM_KNOCKED_DOWN | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5801, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 3, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 3, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5800, 5000 },
            { 3, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5800, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 3, 0, STAT_AGILITY, -2, DAM_KNOCKED_DOWN, 5001, 5800 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 5800, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5804, 5000 },
            { 4, DAM_DEAD, -1, 0, 0, 5805, 5000 },
        },
    },
    // KILL_TYPE_CENTAUR
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5016, 5900 },
            { 5, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5016, 5900 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5901, 5900 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5901, 5900 },
            { 6, DAM_DEAD, -1, 0, 0, 5902, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_LOSE_TURN, 5016, 5903 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5016, 5904 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 5904, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 5905, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_LOSE_TURN, 5016, 5903 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5016, 5904 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 5904, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 5905, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5901, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 2, 0, 5901, 5900 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5900, 5000 },
            { 5, DAM_DEAD, -1, 0, 0, 5902, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5900, 5000 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5900, 5906 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5906, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 5906, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_LOSE_TURN, -1, 0, 0, 5907, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 5900, 5000 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5900, 5906 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 5906, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 5906, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_LOSE_TURN, -1, 0, 0, 5907, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 1, DAM_BLIND, 5001, 5908 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -1, DAM_BLIND, 5901, 5908 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5909, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 5910, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 5911, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 2, 0, -1, 0, 0, 5912, 5000 },
            { 2, 0, -1, 0, 0, 5912, 5000 },
            { 2, 0, -1, 0, 0, 5912, 5000 },
            { 2, 0, -1, 0, 0, 5912, 5000 },
            { 2, 0, -1, 0, 0, 5912, 5000 },
            { 2, 0, -1, 0, 0, 5912, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5901, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 2, 0, 5901, 5900 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5900, 5000 },
            { 5, DAM_DEAD, -1, 0, 0, 5902, 5000 },
        },
    },
    // KILL_TYPE_ROBOT
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 6000, 5000 },
            { 4, 0, -1, 0, 0, 6000, 5000 },
            { 5, 0, -1, 0, 0, 6000, 5000 },
            { 5, DAM_KNOCKED_DOWN, -1, 0, 0, 6001, 5000 },
            { 6, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6002, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6003, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 6000, 6004 },
            { 3, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_LEFT, 6000, 6004 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 6004, 5000 },
            { 4, DAM_CRIP_ARM_LEFT, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 6004, 6005 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 6000, 6004 },
            { 3, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_RIGHT, 6000, 6004 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 6004, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 6004, 6005 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6006, 5000 },
            { 4, 0, -1, 0, 0, 6007, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6008, 5000 },
            { 6, DAM_BYPASS, -1, 0, 0, 6009, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6010, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 4, 0, -1, 0, 0, 6007, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 6000, 6004 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_LEG_RIGHT, 6007, 6004 },
            { 4, DAM_CRIP_LEG_RIGHT, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 6004, 6011 },
            { 4, DAM_CRIP_LEG_RIGHT, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, 6004, 6012 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 4, 0, -1, 0, 0, 6007, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 6000, 6004 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_LEG_LEFT, 6007, 6004 },
            { 4, DAM_CRIP_LEG_LEFT, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 6004, 6011 },
            { 4, DAM_CRIP_LEG_LEFT, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, 6004, 6012 },
        },
        // HIT_LOCATION_EYES
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_BLIND, 6000, 6013 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_BLIND, 6000, 6013 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_BLIND, 6000, 6013 },
            { 3, 0, STAT_ENDURANCE, -6, DAM_BLIND, 6000, 6013 },
            { 3, DAM_BLIND, -1, 0, 0, 6013, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, 0, STAT_ENDURANCE, -1, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 6000, 6002 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 6000, 6002 },
            { 3, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, STAT_ENDURANCE, 0, 0, 6002, 6003 },
            { 3, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, STAT_ENDURANCE, -4, 0, 6002, 6003 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 6000, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6006, 5000 },
            { 4, 0, -1, 0, 0, 6007, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6008, 5000 },
            { 6, DAM_BYPASS, -1, 0, 0, 6009, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6010, 5000 },
        },
    },
    // KILL_TYPE_DOG
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5016, 6100 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5016, 6100 },
            { 4, 0, STAT_ENDURANCE, -6, DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT, 5016, 6101 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6100, 6102 },
            { 4, DAM_DEAD, -1, 0, 0, 6103, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, -1, DAM_CRIP_LEG_LEFT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -5, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, 5001, 6105 },
            { 3, DAM_CRIP_LEG_LEFT, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 6104, 6105 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 6105, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, -1, DAM_CRIP_LEG_RIGHT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -5, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, 5001, 6105 },
            { 3, DAM_CRIP_LEG_RIGHT, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 6104, 6105 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 6105, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 5001, 6100 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_AGILITY, -3, DAM_KNOCKED_DOWN, 5016, 6100 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 6100, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6103, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, STAT_ENDURANCE, 1, DAM_CRIP_LEG_RIGHT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_LEG_RIGHT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, 5001, 6105 },
            { 3, DAM_CRIP_LEG_RIGHT, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 6104, 6105 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 6105, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, STAT_ENDURANCE, 1, DAM_CRIP_LEG_LEFT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_LEG_LEFT, 5001, 6104 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, 5001, 6105 },
            { 3, DAM_CRIP_LEG_LEFT, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 6104, 6105 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 6105, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 5018, 5000 },
            { 6, DAM_BYPASS, -1, 0, 0, 5018, 5000 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, 3, DAM_BLIND, 5018, 6106 },
            { 8, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_BLIND, 5018, 6106 },
            { 8, DAM_DEAD, -1, 0, 0, 6107, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, -2, DAM_KNOCKED_DOWN, 5001, 6100 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_AGILITY, -5, DAM_KNOCKED_DOWN, 5016, 6100 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 6100, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6103, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_AGILITY, -1, DAM_KNOCKED_DOWN, 5001, 6100 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_AGILITY, -3, DAM_KNOCKED_DOWN, 5016, 6100 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 6100, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6103, 5000 },
        },
    },
    // KILL_TYPE_MANTIS
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5001, 6200 },
            { 5, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5016, 6200 },
            { 5, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -1, DAM_KNOCKED_OUT, 6200, 6201 },
            { 6, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6200, 6201 },
            { 6, DAM_DEAD, -1, 0, 0, 6202, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5001, 6203 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5001, 6203 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_LEFT, 5001, 6203 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_LEFT, 5016, 6203 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_LEFT, 5016, 6203 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_LOSE_TURN, -1, 0, 0, 6204, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5001, 6203 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5001, 6203 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_RIGHT, 5001, 6203 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_RIGHT, 5016, 6203 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_RIGHT, 5016, 6203 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_LOSE_TURN, -1, 0, 0, 6204, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 1000, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_BYPASS, 5001, 6205 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_BYPASS, 5001, 6205 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_BYPASS, 5016, 6205 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_BYPASS, 5016, 6205 },
            { 6, DAM_DEAD, -1, 0, 0, 6206, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 6201 },
            { 3, 0, STAT_AGILITY, -2, DAM_KNOCKED_DOWN, 5001, 6201 },
            { 4, 0, STAT_AGILITY, -4, DAM_KNOCKED_DOWN, 5001, 6201 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 6201, 6203 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 6201, 6203 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 6207, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 6201 },
            { 3, 0, STAT_AGILITY, -3, DAM_KNOCKED_DOWN, 5001, 6201 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -2, DAM_CRIP_LEG_LEFT, 6201, 6208 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -2, DAM_CRIP_LEG_LEFT, 6201, 6208 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -5, DAM_CRIP_LEG_LEFT, 6201, 6208 },
            { 3, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 6208, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_LOSE_TURN, 6205, 6209 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_LOSE_TURN, 6205, 6209 },
            { 6, DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -3, DAM_BLIND, 6209, 6210 },
            { 8, DAM_KNOCKED_DOWN | DAM_BYPASS | DAM_LOSE_TURN, STAT_ENDURANCE, -3, DAM_BLIND, 6209, 6210 },
            { 8, DAM_DEAD, -1, 0, 0, 6202, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6205, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6209, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 1000, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_BYPASS, 5001, 6205 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_BYPASS, 5001, 6205 },
            { 4, 0, STAT_ENDURANCE, -2, DAM_BYPASS, 5016, 6205 },
            { 4, 0, STAT_ENDURANCE, -4, DAM_BYPASS, 5016, 6205 },
            { 6, DAM_DEAD, -1, 0, 0, 6206, 5000 },
        },
    },
    // KILL_TYPE_DEATH_CLAW
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 5016, 5023 },
            { 5, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 5016, 5023 },
            { 5, 0, STAT_ENDURANCE, -5, DAM_KNOCKED_DOWN, 5016, 5023 },
            { 6, 0, STAT_ENDURANCE, -4, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5016, 5004 },
            { 6, 0, STAT_ENDURANCE, -5, DAM_KNOCKED_DOWN | DAM_LOSE_TURN, 5016, 5004 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 5001, 5011 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_LEFT, 5001, 5011 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_LEFT, 5001, 5011 },
            { 3, 0, STAT_ENDURANCE, -6, DAM_CRIP_ARM_LEFT, 5001, 5011 },
            { 3, 0, STAT_ENDURANCE, -8, DAM_CRIP_ARM_LEFT, 5001, 5011 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 5001, 5014 },
            { 3, 0, STAT_ENDURANCE, -2, DAM_CRIP_ARM_RIGHT, 5001, 5014 },
            { 3, 0, STAT_ENDURANCE, -4, DAM_CRIP_ARM_RIGHT, 5001, 5014 },
            { 3, 0, STAT_ENDURANCE, -6, DAM_CRIP_ARM_RIGHT, 5001, 5014 },
            { 3, 0, STAT_ENDURANCE, -8, DAM_CRIP_ARM_RIGHT, 5001, 5014 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, -1, DAM_BYPASS, 5001, 6300 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, -1, DAM_BYPASS, 5016, 6300 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5004, 5000 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5005, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5004 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5001, 5004 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -2, DAM_CRIP_LEG_RIGHT, 5001, 5004 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -4, DAM_CRIP_LEG_RIGHT, 5016, 5022 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -5, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -6, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5001, 5004 },
            { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 5001, 5004 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -2, DAM_CRIP_LEG_RIGHT, 5001, 5004 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -4, DAM_CRIP_LEG_RIGHT, 5016, 5022 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -5, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -6, DAM_CRIP_LEG_RIGHT, 5023, 5024 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_LOSE_TURN, 5001, 6301 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -6, DAM_LOSE_TURN, 6300, 6301 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -2, DAM_BLIND, 6301, 6302 },
            { 8, DAM_BLIND | DAM_BYPASS, -1, 0, 0, 6302, 5000 },
            { 8, DAM_BLIND | DAM_BYPASS, -1, 0, 0, 6302, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5001, 5000 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 5, 0, STAT_AGILITY, 0, DAM_KNOCKED_DOWN, 5016, 5004 },
            { 5, 0, STAT_AGILITY, -3, DAM_KNOCKED_DOWN, 5016, 5004 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 5001, 5000 },
            { 3, 0, STAT_ENDURANCE, -1, DAM_BYPASS, 5001, 6300 },
            { 4, 0, -1, 0, 0, 5016, 5000 },
            { 4, 0, STAT_ENDURANCE, -1, DAM_BYPASS, 5016, 6300 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 5004, 5000 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 5005, 5000 },
        },
    },
    // KILL_TYPE_PLANT
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 6405, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6400, 5000 },
            { 5, 0, -1, 0, 0, 6401, 5000 },
            { 5, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_LOSE_TURN, 6402, 6403 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -6, DAM_LOSE_TURN, 6402, 6403 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6400, 5000 },
            { 4, 0, -1, 0, 0, 6401, 5000 },
            { 4, 0, -1, 0, 0, 6401, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, -1, 0, 0, 6405, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6400, 5000 },
            { 5, 0, -1, 0, 0, 6401, 5000 },
            { 5, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
            { 6, DAM_BYPASS, STAT_ENDURANCE, -4, DAM_BLIND, 6402, 6406 },
            { 6, DAM_BLIND | DAM_BYPASS, -1, 0, 0, 6406, 6404 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_LOSE_TURN, 6402, 6403 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -6, DAM_LOSE_TURN, 6402, 6403 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, 0, -1, 0, 0, 6405, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6400, 5000 },
            { 4, 0, -1, 0, 0, 6401, 5000 },
            { 4, 0, -1, 0, 0, 6401, 5000 },
            { 4, DAM_BYPASS, -1, 0, 0, 6402, 5000 },
        },
    },
    // KILL_TYPE_GECKO
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 6701, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6700, 5003 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6700, 5003 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6700, 5003 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, STAT_LUCK, 0, DAM_BLIND, 6700, 5006 },
            { 6, DAM_DEAD, -1, 0, 0, 6700, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 6702, 5000 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 6702, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_LEFT, 6702, 5011 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6702, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6702, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6702, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 6702, 5000 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 6702, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_RIGHT, 6702, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6702, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6702, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6702, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 6701, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6701, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6704, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6704, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 6704, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6704, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6705, 5000 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 6705, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 6705, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6705, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6705, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6705, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6705, 5000 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 6705, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 6705, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6705, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6705, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6705, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, STAT_LUCK, 4, DAM_BLIND, 6700, 5028 },
            { 4, DAM_BYPASS, STAT_LUCK, 3, DAM_BLIND, 6700, 5028 },
            { 6, DAM_BYPASS, STAT_LUCK, 2, DAM_BLIND, 6700, 5028 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 6700, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 6700, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 6700, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 6703, 5000 },
            { 3, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 6703, 5035 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6703, 5036 },
            { 3, DAM_KNOCKED_OUT, -1, 0, 0, 6703, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6703, 5036 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 6703, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 6700, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6700, 5000 },
            { 4, 0, -1, 0, 0, 6700, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6700, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 6700, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6700, 5000 },
        },
    },
    // KILL_TYPE_ALIEN
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 6801, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6800, 5003 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6800, 5003 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6803, 5003 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, STAT_LUCK, 0, DAM_BLIND, 6804, 5006 },
            { 6, DAM_DEAD, -1, 0, 0, 6804, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 6806, 5000 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 6806, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_LEFT, 6806, 5011 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6806, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6806, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6806, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 6806, 5000 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 6806, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_RIGHT, 6806, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6806, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6806, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6806, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 6800, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6800, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6800, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6800, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 6800, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6800, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6805, 5000 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 6805, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 6805, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6805, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6805, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6805, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6805, 5000 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 6805, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 6805, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6805, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6805, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6805, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, STAT_LUCK, 4, DAM_BLIND, 6803, 5028 },
            { 4, DAM_BYPASS, STAT_LUCK, 3, DAM_BLIND, 6803, 5028 },
            { 6, DAM_BYPASS, STAT_LUCK, 2, DAM_BLIND, 6803, 5028 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 6803, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 6803, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 6804, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 6801, 5000 },
            { 3, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 6801, 5035 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6801, 5036 },
            { 3, DAM_KNOCKED_OUT, -1, 0, 0, 6801, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6804, 5036 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 6804, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 6800, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6800, 5000 },
            { 4, 0, -1, 0, 0, 6800, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6800, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 6800, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6800, 5000 },
        },
    },
    // KILL_TYPE_GIANT_ANT
    {
        // HIT_LOCATION_HEAD
        {
            { 4, 0, -1, 0, 0, 6901, 5000 },
            { 4, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6901, 5003 },
            { 5, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6902, 5003 },
            { 5, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6902, 5003 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, STAT_LUCK, 0, DAM_BLIND, 6902, 5006 },
            { 6, DAM_DEAD, -1, 0, 0, 6902, 5000 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 6906, 5000 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 6906, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_LEFT, 6906, 5011 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6906, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6906, 5000 },
            { 4, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6906, 5000 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 6906, 5000 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 6906, 5000 },
            { 4, 0, STAT_ENDURANCE, -3, DAM_CRIP_ARM_RIGHT, 6906, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6906, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6906, 5000 },
            { 4, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6906, 5000 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 6900, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6900, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6904, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6904, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 6904, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6904, 5000 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6905, 5000 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 6905, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 6905, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6905, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6905, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6905, 5000 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6905, 5000 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 6905, 5024 },
            { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 6905, 5024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6905, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6905, 5026 },
            { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6905, 5000 },
        },
        // HIT_LOCATION_EYES
        {
            { 4, 0, STAT_LUCK, 4, DAM_BLIND, 6900, 5028 },
            { 4, DAM_BYPASS, STAT_LUCK, 3, DAM_BLIND, 6906, 5028 },
            { 6, DAM_BYPASS, STAT_LUCK, 2, DAM_BLIND, 6901, 5028 },
            { 6, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 6901, 5000 },
            { 8, DAM_KNOCKED_OUT | DAM_BLIND | DAM_BYPASS, -1, 0, 0, 6901, 5000 },
            { 8, DAM_DEAD, -1, 0, 0, 6901, 5000 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 6900, 5000 },
            { 3, DAM_BYPASS, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 6900, 5035 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_KNOCKED_OUT, 6900, 5036 },
            { 3, DAM_KNOCKED_OUT, -1, 0, 0, 6903, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_OUT, 6903, 5036 },
            { 4, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 6903, 5000 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 6900, 5000 },
            { 3, DAM_BYPASS, -1, 0, 0, 6900, 5000 },
            { 4, 0, -1, 0, 0, 6904, 5000 },
            { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6904, 5000 },
            { 6, DAM_KNOCKED_OUT | DAM_BYPASS, -1, 0, 0, 6904, 5000 },
            { 6, DAM_DEAD, -1, 0, 0, 6904, 5000 },
        },
    },
    // KILL_TYPE_BIG_BAD_BOSS
    {
        // HIT_LOCATION_HEAD
        {
            { 3, 0, -1, 0, 0, 7101, 7100 },
            { 3, 0, -1, 0, 0, 7102, 7103 },
            { 4, 0, -1, 0, 0, 7102, 7103 },
            { 4, DAM_LOSE_TURN, -1, 0, 0, 7104, 7103 },
            { 5, DAM_KNOCKED_DOWN, STAT_LUCK, 0, DAM_BLIND, 7105, 7106 },
            { 6, DAM_KNOCKED_DOWN, -1, 0, 0, 7105, 7100 },
        },
        // HIT_LOCATION_LEFT_ARM
        {
            { 3, 0, -1, 0, 0, 7106, 7100 },
            { 3, 0, -1, 0, 0, 7106, 7100 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 7106, 7011 },
            { 4, DAM_LOSE_TURN, -1, 0, 0, 7106, 7100 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 7106, 7100 },
            { 4, DAM_CRIP_ARM_LEFT, -1, 0, 0, 7106, 7100 },
        },
        // HIT_LOCATION_RIGHT_ARM
        {
            { 3, 0, -1, 0, 0, 7106, 7100 },
            { 3, 0, -1, 0, 0, 7106, 7100 },
            { 3, DAM_LOSE_TURN, -1, 0, 0, 7106, 7100 },
            { 4, DAM_LOSE_TURN, -1, 0, 0, 7106, 7100 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 7106, 7100 },
            { 4, DAM_CRIP_ARM_RIGHT, -1, 0, 0, 7106, 7100 },
        },
        // HIT_LOCATION_TORSO
        {
            { 3, 0, -1, 0, 0, 7106, 7100 },
            { 3, 0, -1, 0, 0, 7106, 7100 },
            { 3, 0, -1, 0, 0, 7106, 7100 },
            { 4, 0, -1, 0, 0, 7106, 7100 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 7106, 7100 },
            { 5, DAM_KNOCKED_DOWN, -1, 0, 0, 7106, 7100 },
        },
        // HIT_LOCATION_RIGHT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 7106, 7100 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 7106, 7106 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_RIGHT, 7060, 7106 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 7106, 7100 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT, -1, 0, 0, 7106, 7106 },
            { 4, DAM_CRIP_LEG_RIGHT, -1, 0, 0, 7106, 7100 },
        },
        // HIT_LOCATION_LEFT_LEG
        {
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 7106, 7100 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 7106, 7024 },
            { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, -3, DAM_CRIP_LEG_LEFT, 7106, 7024 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 7106, 7100 },
            { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT, -1, 0, 0, 7106, 7106 },
            { 4, DAM_CRIP_LEG_LEFT, -1, 0, 0, 7106, 7100 },
        },
        // HIT_LOCATION_EYES
        {
            { 3, 0, -1, 0, 0, 7106, 7106 },
            { 3, 0, -1, 0, 0, 7106, 7106 },
            { 4, 0, STAT_LUCK, 2, DAM_BLIND, 7106, 7106 },
            { 4, DAM_LOSE_TURN, -1, 0, 0, 7106, 7100 },
            { 5, DAM_BLIND | DAM_LOSE_TURN, -1, 0, 0, 7106, 7100 },
            { 5, DAM_BLIND | DAM_LOSE_TURN, -1, 0, 0, 7106, 7100 },
        },
        // HIT_LOCATION_GROIN
        {
            { 3, 0, -1, 0, 0, 7106, 7100 },
            { 3, 0, STAT_ENDURANCE, -3, DAM_KNOCKED_DOWN, 7106, 7106 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 7106, 7106 },
            { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 7106, 7100 },
            { 4, 0, -1, 0, 0, 7106, 7106 },
            { 4, DAM_KNOCKED_DOWN, -1, 0, 0, 7106, 7100 },
        },
        // HIT_LOCATION_UNCALLED
        {
            { 3, 0, -1, 0, 0, 7106, 7100 },
            { 3, 0, -1, 0, 0, 7106, 7100 },
            { 4, 0, -1, 0, 0, 7106, 7100 },
            { 4, 0, -1, 0, 0, 7106, 7100 },
            { 5, DAM_KNOCKED_DOWN, -1, 0, 0, 7106, 7100 },
            { 5, DAM_KNOCKED_DOWN, -1, 0, 0, 7106, 7100 },
        },
    },
};

// Player's criticals effects.
//
// 0x5179B0
static CriticalHitDescription gPlayerCriticalHitTable[HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT] = {
    {
        { 3, 0, -1, 0, 0, 6500, 5000 },
        { 3, DAM_BYPASS, STAT_ENDURANCE, 3, DAM_KNOCKED_DOWN, 6501, 6503 },
        { 3, DAM_BYPASS, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 6501, 6503 },
        { 3, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_ENDURANCE, 2, DAM_KNOCKED_OUT, 6503, 6502 },
        { 3, DAM_KNOCKED_OUT | DAM_BYPASS, STAT_LUCK, 2, DAM_BLIND, 6502, 6504 },
        { 6, DAM_BYPASS, STAT_ENDURANCE, -2, DAM_DEAD, 6501, 6505 },
    },
    {
        { 2, 0, -1, 0, 0, 6506, 5000 },
        { 2, DAM_LOSE_TURN, -1, 0, 0, 6507, 5000 },
        { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_LEFT, 6508, 6509 },
        { 3, DAM_BYPASS, -1, 0, 0, 6501, 5000 },
        { 3, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6510, 5000 },
        { 3, DAM_CRIP_ARM_LEFT | DAM_BYPASS, -1, 0, 0, 6510, 5000 },
    },
    {
        { 2, 0, -1, 0, 0, 6506, 5000 },
        { 2, DAM_LOSE_TURN, -1, 0, 0, 6507, 5000 },
        { 3, 0, STAT_ENDURANCE, 0, DAM_CRIP_ARM_RIGHT, 6508, 6509 },
        { 3, DAM_BYPASS, -1, 0, 0, 6501, 5000 },
        { 3, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6511, 5000 },
        { 3, DAM_CRIP_ARM_RIGHT | DAM_BYPASS, -1, 0, 0, 6511, 5000 },
    },
    {
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, DAM_BYPASS, -1, 0, 0, 6508, 5000 },
        { 3, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6503, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6503, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_LUCK, 2, DAM_DEAD, 6503, 6513 },
    },
    {
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6514, 5000 },
        { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_RIGHT, 6514, 6515 },
        { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6516, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6516, 5000 },
        { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_RIGHT | DAM_BYPASS, -1, 0, 0, 6517, 5000 },
    },
    {
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6514, 5000 },
        { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 0, DAM_CRIP_LEG_LEFT, 6514, 6515 },
        { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6516, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6516, 5000 },
        { 4, DAM_KNOCKED_OUT | DAM_CRIP_LEG_LEFT | DAM_BYPASS, -1, 0, 0, 6517, 5000 },
    },
    {
        { 3, 0, -1, 0, 0, 6518, 5000 },
        { 3, 0, STAT_LUCK, 3, DAM_BLIND, 6518, 6519 },
        { 3, DAM_BYPASS, STAT_LUCK, 3, DAM_BLIND, 6501, 6519 },
        { 4, DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 6520, 5000 },
        { 4, DAM_BLIND | DAM_BYPASS | DAM_LOSE_TURN, -1, 0, 0, 6521, 5000 },
        { 6, DAM_DEAD, -1, 0, 0, 6522, 5000 },
    },
    {
        { 3, 0, -1, 0, 0, 6523, 5000 },
        { 3, 0, STAT_ENDURANCE, 0, DAM_KNOCKED_DOWN, 6523, 6524 },
        { 3, DAM_KNOCKED_DOWN, -1, 0, 0, 6524, 5000 },
        { 3, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 4, DAM_KNOCKED_OUT, 6524, 6525 },
        { 4, DAM_KNOCKED_DOWN, STAT_ENDURANCE, 2, DAM_KNOCKED_OUT, 6524, 6525 },
        { 4, DAM_KNOCKED_OUT, -1, 0, 0, 6526, 5000 },
    },
    {
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, 0, -1, 0, 0, 6512, 5000 },
        { 3, DAM_BYPASS, -1, 0, 0, 6508, 5000 },
        { 3, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6503, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, -1, 0, 0, 6503, 5000 },
        { 4, DAM_KNOCKED_DOWN | DAM_BYPASS, STAT_LUCK, 2, DAM_DEAD, 6503, 6513 },
    },
};

// 0x517F98
static int _combat_end_due_to_load = 0;

// 0x517F9C
static bool _combat_cleanup_enabled = false;

// Provides effects caused by failing weapons.
//
// 0x517FA0
static const int _cf_table[WEAPON_CRITICAL_FAILURE_TYPE_COUNT][WEAPON_CRITICAL_FAILURE_EFFECT_COUNT] = {
    { 0, DAM_LOSE_TURN, DAM_LOSE_TURN, DAM_HURT_SELF | DAM_KNOCKED_DOWN, DAM_CRIP_RANDOM },
    { 0, DAM_LOSE_TURN, DAM_DROP, DAM_RANDOM_HIT, DAM_HIT_SELF },
    { 0, DAM_LOSE_AMMO, DAM_DROP, DAM_RANDOM_HIT, DAM_DESTROY },
    { DAM_LOSE_TURN, DAM_LOSE_TURN | DAM_LOSE_AMMO, DAM_DROP | DAM_LOSE_TURN, DAM_RANDOM_HIT, DAM_EXPLODE | DAM_LOSE_TURN },
    { DAM_DUD, DAM_DROP, DAM_DROP | DAM_HURT_SELF, DAM_RANDOM_HIT, DAM_EXPLODE },
    { DAM_LOSE_TURN, DAM_DUD, DAM_DESTROY, DAM_RANDOM_HIT, DAM_EXPLODE | DAM_LOSE_TURN | DAM_KNOCKED_DOWN },
    { 0, DAM_LOSE_TURN, DAM_RANDOM_HIT, DAM_DESTROY, DAM_EXPLODE | DAM_LOSE_TURN | DAM_ON_FIRE },
};

// 0x51802C
static const int _call_ty[4] = {
    122,
    188,
    251,
    316,
};

// 0x51803C
static const int _hit_loc_left[4] = {
    HIT_LOCATION_HEAD,
    HIT_LOCATION_EYES,
    HIT_LOCATION_RIGHT_ARM,
    HIT_LOCATION_RIGHT_LEG,
};

// 0x51804C
static const int _hit_loc_right[4] = {
    HIT_LOCATION_TORSO,
    HIT_LOCATION_GROIN,
    HIT_LOCATION_LEFT_ARM,
    HIT_LOCATION_LEFT_LEG,
};

// 0x56D2B0
static Attack _main_ctd;

// combat.msg
//
// 0x56D368
static MessageList gCombatMessageList;

// 0x56D370
static Object* gCalledShotCritter;

// 0x56D374
static int gCalledShotWindow;

// 0x56D378
static int _combat_elev;

// 0x56D37C
static int _list_total;

// Probably last who_hit_me of obj_dude
//
// 0x56D380
static Object* _combat_ending_guy;

// 0x56D384
static int _list_noncom;

// 0x56D388
static Object* _combat_turn_obj;

// target_highlight
//
// 0x56D38C
static int _combat_highlight;

// 0x56D390
static Object** _combat_list;

// 0x56D394
static int _list_com;

// Experience received for killing critters during current combat.
//
// 0x56D398
static int _combat_exps;

// bonus action points from BONUS_MOVE perk.
//
// 0x56D39C
int _combat_free_move;

// 0x56D3A0
static Attack _shoot_ctd;

// 0x56D458
static Attack _explosion_ctd;

static CriticalHitDescription gBaseCriticalHitTables[SFALL_KILL_TYPE_COUNT][HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];
static CriticalHitDescription gBasePlayerCriticalHitTable[HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];

static const char* gCritDataMemberKeys[CRIT_DATA_MEMBER_COUNT] = {
    "DamageMultiplier",
    "EffectFlags",
    "StatCheck",
    "StatMod",
    "FailureEffect",
    "Message",
    "FailMessage",
};

static bool gBurstModEnabled = false;
static int gBurstModCenterMultiplier = SFALL_CONFIG_BURST_MOD_DEFAULT_CENTER_MULTIPLIER;
static int gBurstModCenterDivisor = SFALL_CONFIG_BURST_MOD_DEFAULT_CENTER_DIVISOR;
static int gBurstModTargetMultiplier = SFALL_CONFIG_BURST_MOD_DEFAULT_TARGET_MULTIPLIER;
static int gBurstModTargetDivisor = SFALL_CONFIG_BURST_MOD_DEFAULT_TARGET_DIVISOR;
static UnarmedHitDescription gUnarmedHitDescriptions[HIT_MODE_COUNT];
static int gDamageCalculationType;
static bool gBonusHthDamageFix;
static bool gDisplayBonusDamage;

// combat_init
// 0x420CC0
int combatInit()
{
    int max_action_points;
    char path[COMPAT_MAX_PATH];

    _combat_turn_running = 0;
    _combatNumTurns = 0;
    _combat_list = 0;
    _aiInfoList = 0;
    _list_com = 0;
    _list_noncom = 0;
    _list_total = 0;
    _gcsd = 0;
    _combat_call_display = 0;
    gCombatState = COMBAT_STATE_0x02;

    max_action_points = critterGetStat(gDude, STAT_MAXIMUM_ACTION_POINTS);

    _combat_free_move = 0;
    _combat_ending_guy = NULL;
    _combat_end_due_to_load = 0;

    gDude->data.critter.combat.ap = max_action_points;

    _combat_cleanup_enabled = 0;

    if (!messageListInit(&gCombatMessageList)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "combat.msg");

    if (!messageListLoad(&gCombatMessageList, path)) {
        return -1;
    }

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_COMBAT, &gCombatMessageList);

    // SFALL
    criticalsInit();
    burstModInit();
    unarmedInit();
    damageModInit();
    combat_reset_hit_location_penalty();

    return 0;
}

// 0x420DA0
void combatReset()
{
    int max_action_points;

    _combat_turn_running = 0;
    _combatNumTurns = 0;
    _combat_list = 0;
    _aiInfoList = 0;
    _list_com = 0;
    _list_noncom = 0;
    _list_total = 0;
    _gcsd = 0;
    _combat_call_display = 0;
    gCombatState = COMBAT_STATE_0x02;

    max_action_points = critterGetStat(gDude, STAT_MAXIMUM_ACTION_POINTS);

    _combat_free_move = 0;
    _combat_ending_guy = NULL;

    gDude->data.critter.combat.ap = max_action_points;

    // SFALL
    criticalsReset();
    combat_reset_hit_location_penalty();
}

// 0x420E14
void combatExit()
{
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_COMBAT, nullptr);
    messageListFree(&gCombatMessageList);

    // SFALL
    criticalsExit();
}

// 0x420E24
int _find_cid(int a1, int cid, Object** critterList, int critterListLength)
{
    int index;

    for (index = a1; index < critterListLength; index++) {
        if (critterList[index]->cid == cid) {
            break;
        }
    }

    return index;
}

// 0x420E4C
int combatLoad(File* stream)
{
    if (fileReadUInt32(stream, &gCombatState) == -1) return -1;

    if (!isInCombat()) {
        Object* obj = objectFindFirst();
        while (obj != NULL) {
            if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
                if (obj->data.critter.combat.whoHitMeCid == -1) {
                    obj->data.critter.combat.whoHitMe = NULL;
                }
            }
            obj = objectFindNext();
        }
        return 0;
    }

    if (fileReadInt32(stream, &_combat_turn_running) == -1) return -1;
    if (fileReadInt32(stream, &_combat_free_move) == -1) return -1;
    if (fileReadInt32(stream, &_combat_exps) == -1) return -1;
    if (fileReadInt32(stream, &_list_com) == -1) return -1;
    if (fileReadInt32(stream, &_list_noncom) == -1) return -1;
    if (fileReadInt32(stream, &_list_total) == -1) return -1;

    if (objectListCreate(-1, gElevation, OBJ_TYPE_CRITTER, &_combat_list) != _list_total) {
        objectListFree(_combat_list);
        return -1;
    }

    if (fileReadInt32(stream, &(gDude->cid)) == -1) return -1;

    for (int index = 0; index < _list_total; index++) {
        if (_combat_list[index]->data.critter.combat.whoHitMeCid == -1) {
            _combat_list[index]->data.critter.combat.whoHitMe = NULL;
        } else {
            // NOTE: Uninline.
            int found = _find_cid(0, _combat_list[index]->data.critter.combat.whoHitMeCid, _combat_list, _list_total);
            if (found == _list_total) {
                _combat_list[index]->data.critter.combat.whoHitMe = NULL;
            } else {
                _combat_list[index]->data.critter.combat.whoHitMe = _combat_list[found];
            }
        }
    }

    for (int index = 0; index < _list_total; index++) {
        int cid;
        if (fileReadInt32(stream, &cid) == -1) return -1;

        // NOTE: Uninline.
        int found = _find_cid(index, cid, _combat_list, _list_total);
        if (found == _list_total) {
            return -1;
        }

        Object* obj = _combat_list[index];
        _combat_list[index] = _combat_list[found];
        _combat_list[found] = obj;
    }

    for (int index = 0; index < _list_total; index++) {
        _combat_list[index]->cid = index;
    }

    if (_aiInfoList != NULL) {
        internal_free(_aiInfoList);
    }

    _aiInfoList = (CombatAiInfo*)internal_malloc(sizeof(*_aiInfoList) * _list_total);
    if (_aiInfoList == NULL) {
        return -1;
    }

    for (int index = 0; index < _list_total; index++) {
        CombatAiInfo* aiInfo = &(_aiInfoList[index]);

        int friendlyId;
        if (fileReadInt32(stream, &friendlyId) == -1) return -1;

        if (friendlyId == -1) {
            aiInfo->friendlyDead = NULL;
        } else {
            // SFALL: Fix incorrect object type search when loading a game in
            // combat mode.
            aiInfo->friendlyDead = objectTypedFindById(friendlyId, OBJ_TYPE_CRITTER);
            if (aiInfo->friendlyDead == NULL) return -1;
        }

        int targetId;
        if (fileReadInt32(stream, &targetId) == -1) return -1;

        if (targetId == -1) {
            aiInfo->lastTarget = NULL;
        } else {
            // SFALL: Fix incorrect object type search when loading a game in
            // combat mode.
            aiInfo->lastTarget = objectTypedFindById(targetId, OBJ_TYPE_CRITTER);
            if (aiInfo->lastTarget == NULL) return -1;
        }

        int itemId;
        if (fileReadInt32(stream, &itemId) == -1) return -1;

        if (itemId == -1) {
            aiInfo->lastItem = NULL;
        } else {
            // SFALL: Fix incorrect object type search when loading a game in
            // combat mode.
            aiInfo->lastItem = objectTypedFindById(itemId, OBJ_TYPE_ITEM);
            if (aiInfo->lastItem == NULL) return -1;
        }

        if (fileReadInt32(stream, &(aiInfo->lastMove)) == -1) return -1;
    }

    _combat_begin_extra(gDude);

    return 0;
}

// 0x421244
int combatSave(File* stream)
{
    if (fileWriteInt32(stream, gCombatState) == -1) return -1;

    if (!isInCombat()) return 0;

    if (fileWriteInt32(stream, _combat_turn_running) == -1) return -1;
    if (fileWriteInt32(stream, _combat_free_move) == -1) return -1;
    if (fileWriteInt32(stream, _combat_exps) == -1) return -1;
    if (fileWriteInt32(stream, _list_com) == -1) return -1;
    if (fileWriteInt32(stream, _list_noncom) == -1) return -1;
    if (fileWriteInt32(stream, _list_total) == -1) return -1;
    if (fileWriteInt32(stream, gDude->cid) == -1) return -1;

    for (int index = 0; index < _list_total; index++) {
        if (fileWriteInt32(stream, _combat_list[index]->cid) == -1) return -1;
    }

    if (_aiInfoList == NULL) {
        return -1;
    }

    for (int index = 0; index < _list_total; index++) {
        CombatAiInfo* aiInfo = &(_aiInfoList[index]);

        if (fileWriteInt32(stream, aiInfo->friendlyDead != NULL ? aiInfo->friendlyDead->id : -1) == -1) return -1;
        if (fileWriteInt32(stream, aiInfo->lastTarget != NULL ? aiInfo->lastTarget->id : -1) == -1) return -1;
        if (fileWriteInt32(stream, aiInfo->lastItem != NULL ? aiInfo->lastItem->id : -1) == -1) return -1;
        if (fileWriteInt32(stream, aiInfo->lastMove) == -1) return -1;
    }

    return 0;
}

// 0x4213E8
bool _combat_safety_invalidate_weapon(Object* attacker, Object* weapon, int hitMode, Object* defender, int* safeDistancePtr)
{
    return _combat_safety_invalidate_weapon_func(attacker, weapon, hitMode, defender, safeDistancePtr, NULL);
}

// 0x4213FC
static bool _combat_safety_invalidate_weapon_func(Object* attacker, Object* weapon, int hitMode, Object* defender, int* safeDistancePtr, Object* attackerFriend)
{
    if (safeDistancePtr != NULL) {
        *safeDistancePtr = 0;
    }

    if (attacker->pid == PROTO_ID_0x10001E0) {
        return false;
    }

    int intelligence = critterGetStat(attacker, STAT_INTELLIGENCE);
    int team = attacker->data.critter.combat.team;
    int damageRadius = weaponGetDamageRadius(weapon, hitMode);
    int maxDamage;
    weaponGetDamageMinMax(weapon, NULL, &maxDamage);
    int damageType = weaponGetDamageType(attacker, weapon);

    if (damageRadius > 0) {
        if (intelligence < 5) {
            damageRadius -= 5 - intelligence;
            if (damageRadius < 0) {
                damageRadius = 0;
            }
        }

        if (attackerFriend != NULL) {
            if (objectGetDistanceBetween(defender, attackerFriend) < damageRadius) {
                debugPrint("Friendly was in the way!");
                return true;
            }
        }

        for (int index = 0; index < _list_total; index++) {
            Object* candidate = _combat_list[index];
            if (candidate->data.critter.combat.team == team
                && candidate != attacker
                && candidate != defender
                && !critterIsDead(candidate)) {
                int v14 = objectGetDistanceBetween(defender, candidate);
                if (v14 < damageRadius && candidate != candidate->data.critter.combat.whoHitMe) {
                    int damageThreshold = critterGetStat(candidate, STAT_DAMAGE_THRESHOLD + damageType);
                    int damageResistance = critterGetStat(candidate, STAT_DAMAGE_RESISTANCE + damageType);
                    if (damageResistance * (maxDamage - damageThreshold) / 100 > 0) {
                        return true;
                    }
                }
            }
        }

        if (objectGetDistanceBetween(defender, attacker) <= damageRadius) {
            if (safeDistancePtr != NULL) {
                *safeDistancePtr = damageRadius - objectGetDistanceBetween(defender, attacker) + 1;
                return false;
            }

            return true;
        }

        return false;
    }

    int anim = weaponGetAnimationForHitMode(weapon, hitMode);
    if (anim != ANIM_FIRE_BURST && anim != ANIM_FIRE_CONTINUOUS) {
        return false;
    }

    Attack attack;
    attackInit(&attack, attacker, defender, hitMode, HIT_LOCATION_TORSO);

    int accuracy = attackDetermineToHit(attacker, attacker->tile, defender, HIT_LOCATION_TORSO, hitMode, true);
    int roundsHitMainTarget;
    int roundsSpent;
    _compute_spray(&attack, accuracy, &roundsHitMainTarget, &roundsSpent, anim);

    if (attackerFriend != NULL) {
        for (int index = 0; index < attack.extrasLength; index++) {
            if (attack.extras[index] == attackerFriend) {
                debugPrint("Friendly was in the way!");
                return true;
            }
        }
    }

    for (int index = 0; index < attack.extrasLength; index++) {
        Object* candidate = attack.extras[index];
        if (candidate->data.critter.combat.team == team
            && candidate != attacker
            && candidate != defender
            && !critterIsDead(candidate)
            && candidate != candidate->data.critter.combat.whoHitMe) {
            int damageThreshold = critterGetStat(candidate, STAT_DAMAGE_THRESHOLD + damageType);
            int damageResistance = critterGetStat(candidate, STAT_DAMAGE_RESISTANCE + damageType);
            if (damageResistance * (maxDamage - damageThreshold) / 100 > 0) {
                return true;
            }
        }
    }

    return false;
}

// 0x4217BC
bool _combatTestIncidentalHit(Object* attacker, Object* defender, Object* attackerFriend, Object* weapon)
{
    return _combat_safety_invalidate_weapon_func(attacker, weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY, defender, NULL, attackerFriend);
}

// 0x4217D4
Object* _combat_whose_turn()
{
    if (isInCombat()) {
        return _combat_turn_obj;
    } else {
        return NULL;
    }
}

// 0x4217E8
void _combat_data_init(Object* obj)
{
    obj->data.critter.combat.damageLastTurn = 0;
    obj->data.critter.combat.results = 0;
}

// NOTE: Inlined.
//
// 0x4217FC
static void _combatInitAIInfoList()
{
    int index;

    for (index = 0; index < _list_total; index++) {
        _aiInfoList[index].friendlyDead = NULL;
        _aiInfoList[index].lastTarget = NULL;
        _aiInfoList[index].lastItem = NULL;
        _aiInfoList[index].lastMove = 0;
    }
}

// 0x421850
static int aiInfoCopy(int srcIndex, int destIndex)
{
    CombatAiInfo* src = &_aiInfoList[srcIndex];
    CombatAiInfo* dest = &_aiInfoList[destIndex];

    dest->friendlyDead = src->friendlyDead;
    dest->lastTarget = src->lastTarget;
    dest->lastItem = src->lastItem;
    dest->lastMove = src->lastMove;

    return 0;
}

// 0x421880
Object* aiInfoGetFriendlyDead(Object* obj)
{
    if (!isInCombat()) {
        return NULL;
    }

    if (obj == NULL) {
        return NULL;
    }

    if (obj->cid == -1) {
        return NULL;
    }

    return _aiInfoList[obj->cid].friendlyDead;
}

// 0x4218AC
int aiInfoSetFriendlyDead(Object* a1, Object* a2)
{
    if (!isInCombat()) {
        return 0;
    }

    if (a1 == NULL) {
        return -1;
    }

    if (a1->cid == -1) {
        return -1;
    }

    if (a1 == a2) {
        return -1;
    }

    _aiInfoList[a1->cid].friendlyDead = a2;

    return 0;
}

// 0x4218EC
Object* aiInfoGetLastTarget(Object* obj)
{
    if (!isInCombat()) {
        return NULL;
    }

    if (obj == NULL) {
        return NULL;
    }

    if (obj->cid == -1) {
        return NULL;
    }

    return _aiInfoList[obj->cid].lastTarget;
}

// 0x421918
int aiInfoSetLastTarget(Object* a1, Object* a2)
{
    if (!isInCombat()) {
        return 0;
    }

    if (a1 == NULL) {
        return -1;
    }

    if (a1->cid == -1) {
        return -1;
    }

    if (a1 == a2) {
        return -1;
    }

    if (critterIsDead(a2)) {
        a2 = NULL;
    }

    _aiInfoList[a1->cid].lastTarget = a2;

    return 0;
}

// 0x42196C
Object* aiInfoGetLastItem(Object* obj)
{
    int v1;

    if (!isInCombat()) {
        return NULL;
    }

    if (obj == NULL) {
        return NULL;
    }

    v1 = obj->cid;
    if (v1 == -1) {
        return NULL;
    }

    return _aiInfoList[v1].lastItem;
}

// 0x421998
int aiInfoSetLastItem(Object* obj, Object* a2)
{
    int v2;

    if (!isInCombat()) {
        return 0;
    }

    if (obj == NULL) {
        return -1;
    }

    v2 = obj->cid;
    if (v2 == -1) {
        return -1;
    }

    _aiInfoList[v2].lastItem = NULL;

    return 0;
}

// NOTE: Inlined.
//
// 0x421A00
static int _combatAIInfoSetLastMove(Object* object, int move)
{
    if (!isInCombat()) {
        return 0;
    }

    if (object == NULL) {
        return -1;
    }

    if (object->cid == -1) {
        return -1;
    }

    _aiInfoList[object->cid].lastMove = move;

    return 0;
}

// 0x421A34
static void _combat_begin(Object* a1)
{
    _combat_turn_running = 0;
    animationStop();
    tickersRemove(_dude_fidget);
    _combat_elev = gElevation;

    if (!isInCombat()) {
        _combatNumTurns = 0;
        _combat_exps = 0;
        _combat_list = NULL;
        _list_total = objectListCreate(-1, _combat_elev, OBJ_TYPE_CRITTER, &_combat_list);
        _list_noncom = _list_total;
        _list_com = 0;
        _aiInfoList = (CombatAiInfo*)internal_malloc(sizeof(*_aiInfoList) * _list_total);
        if (_aiInfoList == NULL) {
            return;
        }

        // NOTE: Uninline.
        _combatInitAIInfoList();

        Object* v1 = NULL;
        for (int index = 0; index < _list_total; index++) {
            Object* critter = _combat_list[index];
            CritterCombatData* combatData = &(critter->data.critter.combat);
            combatData->maneuver &= CRITTER_MANEUVER_ENGAGING;
            combatData->damageLastTurn = 0;
            combatData->whoHitMe = NULL;
            combatData->ap = 0;
            critter->cid = index;

            // NOTE: Uninline.
            _combatAIInfoSetLastMove(critter, 0);

            scriptSetObjects(critter->sid, NULL, NULL);
            scriptSetFixedParam(critter->sid, 0);
            if (critter->pid == 0x1000098) {
                if (!critterIsDead(critter)) {
                    v1 = critter;
                }
            }
        }

        gCombatState |= COMBAT_STATE_0x01;

        tileWindowRefresh();
        gameUiDisable(0);
        gameMouseSetCursor(MOUSE_CURSOR_WAIT_WATCH);
        _combat_ending_guy = NULL;
        _combat_begin_extra(a1);
        _caiTeamCombatInit(_combat_list, _list_total);
        interfaceBarEndButtonsShow(true);
        _gmouse_enable_scrolling();

        if (v1 != NULL && !_isLoadingGame()) {
            int fid = buildFid(FID_TYPE(v1->fid),
                100,
                FID_ANIM_TYPE(v1->fid),
                (v1->fid & 0xF000) >> 12,
                (v1->fid & 0x70000000) >> 28);

            reg_anim_clear(v1);
            reg_anim_begin(ANIMATION_REQUEST_RESERVED);
            animationRegisterAnimate(v1, ANIM_UP_STAIRS_RIGHT, -1);
            animationRegisterSetFid(v1, fid, -1);
            reg_anim_end();

            while (animationIsBusy(v1)) {
                inputGetInput();
            }
        }
    }
}

// 0x421C8C
static void _combat_begin_extra(Object* a1)
{
    for (int index = 0; index < _list_total; index++) {
        _combat_update_critter_outline_for_los(_combat_list[index], 0);
    }

    attackInit(&_main_ctd, a1, NULL, 4, 3);

    _combat_turn_obj = a1;

    _combat_ai_begin(_list_total, _combat_list);

    _combat_highlight = settings.preferences.target_highlight;
}

// NOTE: Inlined.
//
// 0x421D18
static void _combat_update_critters_in_los(bool a1)
{
    int index;

    for (index = 0; index < _list_total; index++) {
        _combat_update_critter_outline_for_los(_combat_list[index], a1);
    }
}

// Something with outlining.
//
// 0x421D50
void _combat_update_critter_outline_for_los(Object* critter, bool a2)
{
    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return;
    }

    if (critter == gDude) {
        return;
    }

    if (critterIsDead(critter)) {
        return;
    }

    bool v5 = false;
    if (!_combat_is_shot_blocked(gDude, gDude->tile, critter->tile, critter, 0)) {
        v5 = true;
    }

    if (v5) {
        int outlineType = critter->outline & OUTLINE_TYPE_MASK;
        if (outlineType != OUTLINE_TYPE_HOSTILE && outlineType != OUTLINE_TYPE_FRIENDLY) {
            int newOutlineType = gDude->data.critter.combat.team == critter->data.critter.combat.team
                ? OUTLINE_TYPE_FRIENDLY
                : OUTLINE_TYPE_HOSTILE;
            objectDisableOutline(critter, NULL);
            objectClearOutline(critter, NULL);
            objectSetOutline(critter, newOutlineType, NULL);
            if (a2) {
                objectEnableOutline(critter, NULL);
            } else {
                objectDisableOutline(critter, NULL);
            }
        } else {
            if (critter->outline != 0 && (critter->outline & OUTLINE_DISABLED) == 0) {
                if (!a2) {
                    objectDisableOutline(critter, NULL);
                }
            } else {
                if (a2) {
                    objectEnableOutline(critter, NULL);
                }
            }
        }
    } else {
        int v7 = objectGetDistanceBetween(gDude, critter);
        int v8 = critterGetStat(gDude, STAT_PERCEPTION) * 5;
        if ((critter->flags & OBJECT_TRANS_GLASS) != 0) {
            v8 /= 2;
        }

        if (v7 <= v8) {
            v5 = true;
        }

        int outlineType = critter->outline & OUTLINE_TYPE_MASK;
        if (outlineType != OUTLINE_TYPE_32) {
            objectDisableOutline(critter, NULL);
            objectClearOutline(critter, NULL);

            if (v5) {
                objectSetOutline(critter, OUTLINE_TYPE_32, NULL);

                if (a2) {
                    objectEnableOutline(critter, NULL);
                } else {
                    objectDisableOutline(critter, NULL);
                }
            }
        } else {
            if (critter->outline != 0 && (critter->outline & OUTLINE_DISABLED) == 0) {
                if (!a2) {
                    objectDisableOutline(critter, NULL);
                }
            } else {
                if (a2) {
                    objectEnableOutline(critter, NULL);
                }
            }
        }
    }
}

// Probably complete combat sequence.
//
// 0x421EFC
static void _combat_over()
{
    if (_game_user_wants_to_quit == 0) {
        for (int index = 0; index < _list_com; index++) {
            Object* critter = _combat_list[index];
            if (critter != gDude) {
                // SFALL: Fix to prevent dead NPCs from reloading their weapons.
                if ((critter->data.critter.combat.results & DAM_DEAD) == 0) {
                    aiAttemptWeaponReload(critter, 0);
                }
            }
        }
    }

    tickersAdd(_dude_fidget);

    for (int index = 0; index < _list_noncom + _list_com; index++) {
        Object* critter = _combat_list[index];
        critter->data.critter.combat.damageLastTurn = 0;
        critter->data.critter.combat.maneuver = CRITTER_MANEUVER_NONE;
    }

    for (int index = 0; index < _list_total; index++) {
        Object* critter = _combat_list[index];
        critter->data.critter.combat.ap = 0;
        objectClearOutline(critter, NULL);
        critter->data.critter.combat.whoHitMe = NULL;

        scriptSetObjects(critter->sid, NULL, NULL);
        scriptSetFixedParam(critter->sid, 0);

        if (critter->pid == 0x1000098 && !critterIsDead(critter) && !_isLoadingGame()) {
            int fid = buildFid(FID_TYPE(critter->fid),
                99,
                FID_ANIM_TYPE(critter->fid),
                (critter->fid & 0xF000) >> 12,
                (critter->fid & 0x70000000) >> 28);
            reg_anim_clear(critter);
            reg_anim_begin(ANIMATION_REQUEST_RESERVED);
            animationRegisterAnimate(critter, ANIM_UP_STAIRS_RIGHT, -1);
            animationRegisterSetFid(critter, fid, -1);
            reg_anim_end();

            while (animationIsBusy(critter)) {
                inputGetInput();
            }
        }
    }

    tileWindowRefresh();

    int leftItemAction;
    int rightItemAction;
    interfaceGetItemActions(&leftItemAction, &rightItemAction);
    interfaceUpdateItems(true, leftItemAction, rightItemAction);

    gDude->data.critter.combat.ap = critterGetStat(gDude, STAT_MAXIMUM_ACTION_POINTS);

    interfaceRenderActionPoints(0, 0);

    if (_game_user_wants_to_quit == 0) {
        _combat_give_exps(_combat_exps);
    }

    _combat_exps = 0;

    gCombatState &= ~(COMBAT_STATE_0x01 | COMBAT_STATE_0x02);
    gCombatState |= COMBAT_STATE_0x02;

    if (_list_total != 0) {
        objectListFree(_combat_list);

        if (_aiInfoList != NULL) {
            internal_free(_aiInfoList);
        }
        _aiInfoList = NULL;
    }

    _list_total = 0;

    _combat_ai_over();
    gameUiEnable();
    gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
    interfaceRenderArmorClass(true);

    if (_critter_is_prone(gDude) && !critterIsDead(gDude) && _combat_ending_guy == NULL) {
        queueRemoveEventsByType(gDude, EVENT_TYPE_KNOCKOUT);
        knockoutEventProcess(gDude, NULL);
    }
}

// 0x422194
void _combat_over_from_load()
{
    _combat_over();
    gCombatState = 0;
    _combat_end_due_to_load = 1;
}

// Give exp for destroying critter.
//
// 0x4221B4
void _combat_give_exps(int exp_points)
{
    MessageListItem v7;
    MessageListItem v9;
    int current_hp;
    int max_hp;
    char text[132];

    if (exp_points <= 0) {
        return;
    }

    if (critterIsDead(gDude)) {
        return;
    }

    // SFALL: Display actual xp received.
    int xpGained;
    pcAddExperience(exp_points, &xpGained);

    v7.num = 621; // %s you earn %d exp. points.
    if (!messageListGetItem(&gProtoMessageList, &v7)) {
        return;
    }

    v9.num = randomBetween(0, 3) + 622; // generate prefix for message

    current_hp = critterGetStat(gDude, STAT_CURRENT_HIT_POINTS);
    max_hp = critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS);
    if (current_hp == max_hp && randomBetween(0, 100) > 65) {
        v9.num = 626; // Best possible prefix: For destroying your enemies without taking a scratch,
    }

    if (!messageListGetItem(&gProtoMessageList, &v9)) {
        return;
    }

    snprintf(text, sizeof(text), v7.text, v9.text, xpGained);
    displayMonitorAddMessage(text);
}

// 0x4222A8
static void _combat_add_noncoms()
{
    _combatai_notify_friends(gDude);

    for (int index = _list_com; index < _list_com + _list_noncom; index++) {
        Object* obj = _combat_list[index];
        if (_combatai_want_to_join(obj)) {
            obj->data.critter.combat.maneuver = CRITTER_MANEUVER_NONE;

            Object** objectPtr1 = &(_combat_list[index]);
            Object** objectPtr2 = &(_combat_list[_list_com]);
            Object* t = *objectPtr1;
            *objectPtr1 = *objectPtr2;
            *objectPtr2 = t;

            _list_com += 1;
            _list_noncom -= 1;

            int actionPoints = 0;
            if (obj != gDude) {
                actionPoints = critterGetStat(obj, STAT_MAXIMUM_ACTION_POINTS);
            }

            if (_gcsd != NULL) {
                actionPoints += _gcsd->actionPointsBonus;
            }

            obj->data.critter.combat.ap = actionPoints;

            _combat_turn(obj, false);
        }
    }
}

// Compares critters by sequence.
//
// 0x4223C8
static int _compare_faster(const void* a1, const void* a2)
{
    Object* v1 = *(Object**)a1;
    Object* v2 = *(Object**)a2;

    int sequence1 = critterGetStat(v1, STAT_SEQUENCE);
    int sequence2 = critterGetStat(v2, STAT_SEQUENCE);
    if (sequence1 > sequence2) {
        return -1;
    } else if (sequence1 < sequence2) {
        return 1;
    }

    int luck1 = critterGetStat(v1, STAT_LUCK);
    int luck2 = critterGetStat(v2, STAT_LUCK);
    if (luck1 > luck2) {
        return -1;
    } else if (luck1 < luck2) {
        return 1;
    }

    return 0;
}

// 0x42243C
static void _combat_sequence_init(Object* a1, Object* a2)
{
    int next = 0;
    if (a1 != NULL) {
        for (int index = 0; index < _list_total; index++) {
            Object* obj = _combat_list[index];
            if (obj == a1) {
                Object* temp = _combat_list[next];
                _combat_list[index] = temp;
                _combat_list[next] = obj;
                next += 1;
                break;
            }
        }
    }

    if (a2 != NULL) {
        for (int index = 0; index < _list_total; index++) {
            Object* obj = _combat_list[index];
            if (obj == a2) {
                Object* temp = _combat_list[next];
                _combat_list[index] = temp;
                _combat_list[next] = obj;
                next += 1;
                break;
            }
        }
    }

    if (a1 != gDude && a2 != gDude) {
        for (int index = 0; index < _list_total; index++) {
            Object* obj = _combat_list[index];
            if (obj == gDude) {
                Object* temp = _combat_list[next];
                _combat_list[index] = temp;
                _combat_list[next] = obj;
                next += 1;
                break;
            }
        }
    }

    _list_com = next;
    _list_noncom -= next;

    if (a1 != NULL) {
        _critter_set_who_hit_me(a1, a2);
    }

    if (a2 != NULL) {
        _critter_set_who_hit_me(a2, a1);
    }
}

// 0x422580
static void _combat_sequence()
{
    _combat_add_noncoms();

    int count = _list_com;

    for (int index = 0; index < count; index++) {
        Object* critter = _combat_list[index];
        if ((critter->data.critter.combat.results & DAM_DEAD) != 0) {
            _combat_list[index] = _combat_list[count - 1];
            _combat_list[count - 1] = critter;

            _combat_list[count - 1] = _combat_list[_list_noncom + count - 1];
            _combat_list[_list_noncom + count - 1] = critter;

            index -= 1;
            count -= 1;
        }
    }

    for (int index = 0; index < count; index++) {
        Object* critter = _combat_list[index];
        if (critter != gDude) {
            if ((critter->data.critter.combat.results & DAM_KNOCKED_OUT) != 0
                || critter->data.critter.combat.maneuver == CRITTER_MANEUVER_DISENGAGING) {
                critter->data.critter.combat.maneuver &= ~CRITTER_MANEUVER_ENGAGING;
                _list_noncom += 1;

                _combat_list[index] = _combat_list[count - 1];
                _combat_list[count - 1] = critter;

                count -= 1;
                index -= 1;
            }
        }
    }

    if (count != 0) {
        _list_com = count;
        qsort(_combat_list, count, sizeof(*_combat_list), _compare_faster);
        count = _list_com;
    }

    _list_com = count;

    gameTimeAddSeconds(5);
}

// 0x422694
static void combatAttemptEnd()
{
    if (_combat_elev == gDude->elevation) {
        MessageListItem messageListItem;
        int dudeTeam = gDude->data.critter.combat.team;

        for (int index = 0; index < _list_com; index++) {
            Object* critter = _combat_list[index];
            if (critter != gDude) {
                int critterTeam = critter->data.critter.combat.team;
                Object* critterWhoHitMe = critter->data.critter.combat.whoHitMe;
                if (critterTeam != dudeTeam || (critterWhoHitMe != NULL && critterWhoHitMe->data.critter.combat.team == critterTeam)) {
                    if (!_combatai_want_to_stop(critter)) {
                        messageListItem.num = 103;
                        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                            displayMonitorAddMessage(messageListItem.text);
                        }
                        return;
                    }
                }
            }
        }

        for (int index = _list_com; index < _list_com + _list_noncom; index++) {
            Object* critter = _combat_list[index];
            if (critter != gDude) {
                int critterTeam = critter->data.critter.combat.team;
                Object* critterWhoHitMe = critter->data.critter.combat.whoHitMe;
                if (critterTeam != dudeTeam || (critterWhoHitMe != NULL && critterWhoHitMe->data.critter.combat.team == critterTeam)) {
                    if (_combatai_want_to_join(critter)) {
                        messageListItem.num = 103;
                        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                            displayMonitorAddMessage(messageListItem.text);
                        }
                        return;
                    }
                }
            }
        }
    }

    gCombatState |= COMBAT_STATE_0x08;
    _caiTeamCombatExit();
}

// 0x4227DC
void _combat_turn_run()
{
    while (_combat_turn_running > 0) {
        sharedFpsLimiter.mark();

        inputGetInput();

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// 0x4227F4
static int _combat_input()
{
    ScopedGameMode gm(GameMode::kPlayerTurn);

    while ((gCombatState & COMBAT_STATE_0x02) != 0) {
        sharedFpsLimiter.mark();

        if ((gCombatState & COMBAT_STATE_0x08) != 0) {
            break;
        }

        if ((gDude->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD | DAM_LOSE_TURN)) != 0) {
            break;
        }

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        if (_combat_end_due_to_load != 0) {
            break;
        }

        int keyCode = inputGetInput();

        // SFALL: CombatLoopHook.
        sfall_gl_scr_process_main();

        if (_action_explode_running()) {
            // NOTE: Uninline.
            _combat_turn_run();
        }

        if (gDude->data.critter.combat.ap <= 0 && _combat_free_move <= 0) {
            break;
        }

        if (keyCode == KEY_SPACE) {
            break;
        }

        if (keyCode == KEY_RETURN) {
            combatAttemptEnd();
        } else {
            _scripts_check_state_in_combat();
            gameHandleKey(keyCode, true);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    int v4 = _game_user_wants_to_quit;
    if (_game_user_wants_to_quit == 1) {
        _game_user_wants_to_quit = 0;
    }

    if ((gCombatState & COMBAT_STATE_0x08) != 0) {
        gCombatState &= ~COMBAT_STATE_0x08;
        return -1;
    }

    if (_game_user_wants_to_quit != 0 || v4 != 0 || _combat_end_due_to_load != 0) {
        return -1;
    }

    _scripts_check_state_in_combat();

    return 0;
}

// 0x422914
static void _combat_set_move_all()
{
    for (int index = 0; index < _list_com; index++) {
        Object* object = _combat_list[index];

        int actionPoints = critterGetStat(object, STAT_MAXIMUM_ACTION_POINTS);

        if (_gcsd) {
            actionPoints += _gcsd->actionPointsBonus;
        }

        object->data.critter.combat.ap = actionPoints;

        // NOTE: Uninline.
        _combatAIInfoSetLastMove(object, 0);
    }
}

// 0x42299C
static int _combat_turn(Object* a1, bool a2)
{
    _combat_turn_obj = a1;

    attackInit(&_main_ctd, a1, NULL, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);

    if ((a1->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD | DAM_LOSE_TURN)) != 0) {
        a1->data.critter.combat.results &= ~DAM_LOSE_TURN;
    } else {
        if (a1 == gDude) {
            keyboardReset();
            interfaceRenderArmorClass(true);
            _combat_free_move = 2 * perkGetRank(gDude, PERK_BONUS_MOVE);
            interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
        } else {
            soundContinueAll();
        }

        bool scriptOverrides = false;
        if (a1->sid != -1) {
            scriptSetObjects(a1->sid, NULL, NULL);
            scriptSetFixedParam(a1->sid, 4);
            scriptExecProc(a1->sid, SCRIPT_PROC_COMBAT);

            Script* scr;
            if (scriptGetScript(a1->sid, &scr) != -1) {
                scriptOverrides = scr->scriptOverrides;
            }

            if (_game_user_wants_to_quit == 1) {
                return -1;
            }
        }

        if (!scriptOverrides) {
            if (!a2 && _critter_is_prone(a1)) {
                _combat_standup(a1);
            }

            if (a1 == gDude) {
                gameUiEnable();
                _gmouse_3d_refresh();

                if (_gcsd != NULL) {
                    _combat_attack_this(_gcsd->defender);
                }

                if (!a2) {
                    gCombatState |= 0x02;
                }

                interfaceBarEndButtonsRenderGreenLights();

                // NOTE: Uninline.
                _combat_update_critters_in_los(false);

                if (_combat_highlight != 0) {
                    _combat_outline_on();
                }

                if (_combat_input() == -1) {
                    gameUiDisable(1);
                    gameMouseSetCursor(MOUSE_CURSOR_WAIT_WATCH);
                    a1->data.critter.combat.damageLastTurn = 0;
                    interfaceBarEndButtonsRenderRedLights();
                    _combat_outline_off();
                    interfaceRenderActionPoints(-1, -1);
                    interfaceRenderArmorClass(true);
                    _combat_free_move = 0;
                    return -1;
                }
            } else {
                Rect rect;
                if (objectEnableOutline(a1, &rect) == 0) {
                    tileWindowRefreshRect(&rect, a1->elevation);
                }

                _combat_ai(a1, _gcsd != NULL ? _gcsd->defender : NULL);
            }
        }

        // NOTE: Uninline.
        _combat_turn_run();

        if (a1 == gDude) {
            gameUiDisable(1);
            gameMouseSetCursor(MOUSE_CURSOR_WAIT_WATCH);
            interfaceBarEndButtonsRenderRedLights();
            _combat_outline_off();
            interfaceRenderActionPoints(-1, -1);
            _combat_turn_obj = NULL;
            interfaceRenderArmorClass(true);
            _combat_turn_obj = gDude;
        } else {
            Rect rect;
            if (objectDisableOutline(a1, &rect) == 0) {
                tileWindowRefreshRect(&rect, a1->elevation);
            }
        }
    }

    if ((gDude->data.critter.combat.results & DAM_DEAD) != 0) {
        return -1;
    }

    if (a1 != gDude || _combat_elev == gDude->elevation) {
        _combat_free_move = 0;
        return 0;
    }

    return -1;
}

// 0x422C60
static bool _combat_should_end()
{
    if (_list_com <= 1) {
        return true;
    }

    int index;
    for (index = 0; index < _list_com; index++) {
        if (_combat_list[index] == gDude) {
            break;
        }
    }

    if (index == _list_com) {
        return true;
    }

    int team = gDude->data.critter.combat.team;

    for (index = 0; index < _list_com; index++) {
        Object* critter = _combat_list[index];
        if (critter->data.critter.combat.team != team) {
            break;
        }

        Object* critterWhoHitMe = critter->data.critter.combat.whoHitMe;
        if (critterWhoHitMe != NULL && critterWhoHitMe->data.critter.combat.team == team) {
            break;
        }
    }

    if (index == _list_com) {
        return true;
    }

    return false;
}

// 0x422D2C
void _combat(STRUCT_664980* attack)
{
    ScopedGameMode gm(GameMode::kCombat);

    if (attack == NULL
        || (attack->attacker == NULL || attack->attacker->elevation == gElevation)
        || (attack->defender == NULL || attack->defender->elevation == gElevation)) {
        int v3 = gCombatState & 0x01;

        _combat_begin(NULL);

        int v6;

        // TODO: Not sure.
        if (v3 != 0) {
            if (_combat_turn(gDude, true) == -1) {
                v6 = -1;
            } else {
                int index;
                for (index = 0; index < _list_com; index++) {
                    if (_combat_list[index] == gDude) {
                        break;
                    }
                }
                v6 = index + 1;
            }
            _gcsd = NULL;
        } else {
            Object* v3;
            Object* v9;
            if (attack != NULL) {
                v3 = attack->defender;
                v9 = attack->attacker;
            } else {
                v3 = NULL;
                v9 = NULL;
            }
            _combat_sequence_init(v9, v3);
            _gcsd = attack;
            v6 = 0;
        }

        do {
            if (v6 == -1) {
                break;
            }

            _combat_set_move_all();

            for (; v6 < _list_com; v6++) {
                if (_combat_turn(_combat_list[v6], false) == -1) {
                    break;
                }

                if (_combat_ending_guy != NULL) {
                    break;
                }

                _gcsd = NULL;
            }

            if (v6 < _list_com) {
                break;
            }

            _combat_sequence();
            v6 = 0;
            _combatNumTurns += 1;
        } while (!_combat_should_end());

        if (_combat_end_due_to_load) {
            gameUiEnable();
            gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
        } else {
            _gmouse_disable_scrolling();
            interfaceBarEndButtonsHide(true);
            _gmouse_enable_scrolling();
            _combat_over();
            scriptsExecMapUpdateProc();
        }

        _combat_end_due_to_load = 0;

        if (_game_user_wants_to_quit == 1) {
            _game_user_wants_to_quit = 0;
        }
    }
}

// 0x422EC4
void attackInit(Attack* attack, Object* attacker, Object* defender, int hitMode, int hitLocation)
{
    attack->attacker = attacker;
    attack->hitMode = hitMode;
    attack->weapon = critterGetWeaponForHitMode(attacker, hitMode);
    attack->attackHitLocation = HIT_LOCATION_TORSO;
    attack->attackerDamage = 0;
    attack->attackerFlags = 0;
    attack->ammoQuantity = 0;
    attack->criticalMessageId = -1;
    attack->defender = defender;
    attack->tile = defender != NULL ? defender->tile : -1;
    attack->defenderHitLocation = hitLocation;
    attack->defenderDamage = 0;
    attack->defenderFlags = 0;
    attack->defenderKnockback = 0;
    attack->extrasLength = 0;
    attack->oops = defender;
}

// 0x422F3C
int _combat_attack(Object* attacker, Object* defender, int hitMode, int hitLocation)
{
    if (attacker != gDude && hitMode == HIT_MODE_PUNCH && randomBetween(1, 4) == 1) {
        int fid = buildFid(OBJ_TYPE_CRITTER, attacker->fid & 0xFFF, ANIM_KICK_LEG, (attacker->fid & 0xF000) >> 12, (attacker->fid & 0x70000000) >> 28);
        if (artExists(fid)) {
            hitMode = HIT_MODE_KICK;
        }
    }

    attackInit(&_main_ctd, attacker, defender, hitMode, hitLocation);
    debugPrint("computing attack...\n");

    if (attackCompute(&_main_ctd) == -1) {
        return -1;
    }

    if (_gcsd != NULL) {
        _main_ctd.defenderDamage += _gcsd->damageBonus;

        if (_main_ctd.defenderDamage < _gcsd->minDamage) {
            _main_ctd.defenderDamage = _gcsd->minDamage;
        }

        if (_main_ctd.defenderDamage > _gcsd->maxDamage) {
            _main_ctd.defenderDamage = _gcsd->maxDamage;
        }

        if (_gcsd->field_1C) {
            // FIXME: looks like a bug, two different fields are used to set
            // one field.
            _main_ctd.defenderFlags = _gcsd->field_20;
            _main_ctd.defenderFlags = _gcsd->field_24;
        }
    }

    bool aiming;
    if (_main_ctd.defenderHitLocation == HIT_LOCATION_TORSO || _main_ctd.defenderHitLocation == HIT_LOCATION_UNCALLED) {
        if (attacker == gDude) {
            interfaceGetCurrentHitMode(&hitMode, &aiming);
        } else {
            aiming = false;
        }
    } else {
        aiming = true;
    }

    int actionPoints = weaponGetActionPointCost(attacker, _main_ctd.hitMode, aiming);
    debugPrint("sequencing attack...\n");

    if (_action_attack(&_main_ctd) == -1) {
        return -1;
    }

    if (actionPoints > attacker->data.critter.combat.ap) {
        attacker->data.critter.combat.ap = 0;
    } else {
        attacker->data.critter.combat.ap -= actionPoints;
    }

    if (attacker == gDude) {
        interfaceRenderActionPoints(attacker->data.critter.combat.ap, _combat_free_move);
        _critter_set_who_hit_me(attacker, defender);
    }

    // SFALL
    explosionSettingsReset();

    _combat_call_display = 1;
    _combat_cleanup_enabled = 1;
    aiInfoSetLastTarget(attacker, defender);
    debugPrint("running attack...\n");

    return 0;
}

// Returns tile one step closer from [attacker] to [target]
//
// 0x423104
int _combat_bullet_start(const Object* attacker, const Object* target)
{
    int rotation = tileGetRotationTo(attacker->tile, target->tile);
    return tileGetTileInDirection(attacker->tile, rotation, 1);
}

// 0x423128
static bool _check_ranged_miss(Attack* attack)
{
    int range = weaponGetRange(attack->attacker, attack->hitMode);
    int to = _tile_num_beyond(attack->attacker->tile, attack->defender->tile, range);

    int roll = ROLL_FAILURE;
    Object* critter = attack->attacker;
    if (critter != NULL) {
        int curr = attack->attacker->tile;
        while (curr != to) {
            _make_straight_path_func(attack->attacker, curr, to, NULL, &critter, 32, _obj_shoot_blocking_at);
            if (critter != NULL) {
                if ((critter->flags & OBJECT_SHOOT_THRU) == 0) {
                    if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
                        roll = ROLL_SUCCESS;
                        break;
                    }

                    if (critter != attack->defender) {
                        int v6 = attackDetermineToHit(attack->attacker, attack->attacker->tile, critter, attack->defenderHitLocation, attack->hitMode, true) / 3;
                        if (critterIsDead(critter)) {
                            v6 = 5;
                        }

                        if (randomBetween(1, 100) <= v6) {
                            roll = ROLL_SUCCESS;
                            break;
                        }
                    }

                    curr = critter->tile;
                }
            }

            if (critter == NULL) {
                break;
            }
        }
    }

    attack->defenderHitLocation = HIT_LOCATION_TORSO;

    if (roll < ROLL_SUCCESS || critter == NULL || (critter->flags & OBJECT_SHOOT_THRU) == 0) {
        return false;
    }

    attack->defender = critter;
    attack->tile = critter->tile;
    attack->attackerFlags |= DAM_HIT;
    attack->defenderHitLocation = HIT_LOCATION_TORSO;
    attackComputeDamage(attack, 1, 2);
    return true;
}

// 0x423284
static int _shoot_along_path(Attack* attack, int endTile, int rounds, int anim)
{
    int remainingRounds = rounds;
    int roundsHitMainTarget = 0;
    int currentTile = attack->attacker->tile;

    Object* critter = attack->attacker;
    while (critter != NULL) {
        if ((remainingRounds <= 0 && anim != ANIM_FIRE_CONTINUOUS) || currentTile == endTile || attack->extrasLength >= 6) {
            break;
        }

        _make_straight_path_func(attack->attacker, currentTile, endTile, NULL, &critter, 32, _obj_shoot_blocking_at);

        if (critter != NULL) {
            if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
                break;
            }

            int accuracy = attackDetermineToHit(attack->attacker, attack->attacker->tile, critter, HIT_LOCATION_TORSO, attack->hitMode, true);
            if (anim == ANIM_FIRE_CONTINUOUS) {
                remainingRounds = 1;
            }

            int roundsHit = 0;
            while (randomBetween(1, 100) <= accuracy && remainingRounds > 0) {
                remainingRounds -= 1;
                roundsHit += 1;
            }

            if (roundsHit != 0) {
                if (critter == attack->defender) {
                    roundsHitMainTarget += roundsHit;
                } else {
                    int index;
                    for (index = 0; index < attack->extrasLength; index += 1) {
                        if (critter == attack->extras[index]) {
                            break;
                        }
                    }

                    attack->extrasHitLocation[index] = HIT_LOCATION_TORSO;
                    attack->extras[index] = critter;
                    attackInit(&_shoot_ctd, attack->attacker, critter, attack->hitMode, HIT_LOCATION_TORSO);
                    _shoot_ctd.attackerFlags |= DAM_HIT;
                    attackComputeDamage(&_shoot_ctd, roundsHit, 2);

                    if (index == attack->extrasLength) {
                        attack->extrasDamage[index] = _shoot_ctd.defenderDamage;
                        attack->extrasFlags[index] = _shoot_ctd.defenderFlags;
                        attack->extrasKnockback[index] = _shoot_ctd.defenderKnockback;
                        attack->extrasLength++;
                    } else {
                        if (anim == ANIM_FIRE_BURST) {
                            attack->extrasDamage[index] += _shoot_ctd.defenderDamage;
                            attack->extrasFlags[index] |= _shoot_ctd.defenderFlags;
                            attack->extrasKnockback[index] += _shoot_ctd.defenderKnockback;
                        }
                    }
                }
            }

            currentTile = critter->tile;
        }
    }

    if (anim == ANIM_FIRE_CONTINUOUS) {
        roundsHitMainTarget = 0;
    }

    return roundsHitMainTarget;
}

// 0x423488
static int _compute_spray(Attack* attack, int accuracy, int* roundsHitMainTargetPtr, int* roundsSpentPtr, int anim)
{
    *roundsHitMainTargetPtr = 0;

    int ammoQuantity = ammoGetQuantity(attack->weapon);
    int burstRounds = weaponGetBurstRounds(attack->weapon);
    if (burstRounds < ammoQuantity) {
        ammoQuantity = burstRounds;
    }

    *roundsSpentPtr = ammoQuantity;

    int criticalChance = critterGetStat(attack->attacker, STAT_CRITICAL_CHANCE);
    int roll = randomRoll(accuracy, criticalChance, NULL);

    if (roll == ROLL_CRITICAL_FAILURE) {
        return roll;
    }

    if (roll == ROLL_CRITICAL_SUCCESS) {
        accuracy += 20;
    }

    int leftRounds;
    int mainTargetRounds;
    int centerRounds;
    int rightRounds;
    if (anim == ANIM_FIRE_BURST) {
        // SFALL: Burst mod.
        if (gBurstModEnabled) {
            mainTargetRounds = burstModComputeRounds(ammoQuantity, &centerRounds, &leftRounds, &rightRounds);
        } else {
            centerRounds = ammoQuantity / 3;
            if (centerRounds == 0) {
                centerRounds = 1;
            }

            leftRounds = ammoQuantity / 3;
            rightRounds = ammoQuantity - centerRounds - leftRounds;
            mainTargetRounds = centerRounds / 2;
            if (mainTargetRounds == 0) {
                mainTargetRounds = 1;
                centerRounds -= 1;
            }
        }
    } else {
        leftRounds = 1;
        mainTargetRounds = 1;
        centerRounds = 1;
        rightRounds = 1;
    }

    for (int index = 0; index < mainTargetRounds; index += 1) {
        if (randomRoll(accuracy, 0, NULL) >= ROLL_SUCCESS) {
            *roundsHitMainTargetPtr += 1;
        }
    }

    if (*roundsHitMainTargetPtr == 0 && _check_ranged_miss(attack)) {
        *roundsHitMainTargetPtr = 1;
    }

    int range = weaponGetRange(attack->attacker, attack->hitMode);
    int mainTargetEndTile = _tile_num_beyond(attack->attacker->tile, attack->defender->tile, range);
    *roundsHitMainTargetPtr += _shoot_along_path(attack, mainTargetEndTile, centerRounds - *roundsHitMainTargetPtr, anim);

    int centerTile;
    if (objectGetDistanceBetween(attack->attacker, attack->defender) <= 3) {
        centerTile = _tile_num_beyond(attack->attacker->tile, attack->defender->tile, 3);
    } else {
        centerTile = attack->defender->tile;
    }

    int rotation = tileGetRotationTo(centerTile, attack->attacker->tile);

    int leftTile = tileGetTileInDirection(centerTile, (rotation + 1) % ROTATION_COUNT, 1);
    int leftEndTile = _tile_num_beyond(attack->attacker->tile, leftTile, range);
    *roundsHitMainTargetPtr += _shoot_along_path(attack, leftEndTile, leftRounds, anim);

    int rightTile = tileGetTileInDirection(centerTile, (rotation + 5) % ROTATION_COUNT, 1);
    int rightEndTile = _tile_num_beyond(attack->attacker->tile, rightTile, range);
    *roundsHitMainTargetPtr += _shoot_along_path(attack, rightEndTile, rightRounds, anim);

    if (roll != ROLL_FAILURE || (*roundsHitMainTargetPtr <= 0 && attack->extrasLength <= 0)) {
        if (roll >= ROLL_SUCCESS && *roundsHitMainTargetPtr == 0 && attack->extrasLength == 0) {
            roll = ROLL_FAILURE;
        }
    } else {
        roll = ROLL_SUCCESS;
    }

    return roll;
}

// 0x423714
static int attackComputeEnhancedKnockout(Attack* attack)
{
    if (weaponGetPerk(attack->weapon) == PERK_WEAPON_ENHANCED_KNOCKOUT) {
        int difficulty = critterGetStat(attack->attacker, STAT_STRENGTH) - 8;
        int chance = randomBetween(1, 100);
        if (chance <= difficulty) {
            Object* weapon = NULL;
            if (attack->defender != gDude) {
                weapon = critterGetWeaponForHitMode(attack->defender, HIT_MODE_RIGHT_WEAPON_PRIMARY);
            }

            if (!(_attackFindInvalidFlags(attack->defender, weapon) & 1)) {
                attack->defenderFlags |= DAM_KNOCKED_OUT;
            }
        }
    }

    return 0;
}

// 0x42378C
static int attackCompute(Attack* attack)
{
    int range = weaponGetRange(attack->attacker, attack->hitMode);
    int distance = objectGetDistanceBetween(attack->attacker, attack->defender);

    if (range < distance) {
        return -1;
    }

    int anim = critterGetAnimationForHitMode(attack->attacker, attack->hitMode);
    int accuracy = attackDetermineToHit(attack->attacker, attack->attacker->tile, attack->defender, attack->defenderHitLocation, attack->hitMode, true);

    bool isGrenade = false;
    int damageType = weaponGetDamageType(attack->attacker, attack->weapon);
    // SFALL
    if (anim == ANIM_THROW_ANIM && (damageType == explosionGetDamageType() || damageType == DAMAGE_TYPE_PLASMA || damageType == DAMAGE_TYPE_EMP)) {
        isGrenade = true;
    }

    if (attack->defenderHitLocation == HIT_LOCATION_UNCALLED) {
        attack->defenderHitLocation = HIT_LOCATION_TORSO;
    }

    int attackType = weaponGetAttackTypeForHitMode(attack->weapon, attack->hitMode);
    int ammoQuantity = 1;
    int damageMultiplier = 2;
    int v26 = 1;

    int roll;

    if (anim == ANIM_FIRE_BURST || anim == ANIM_FIRE_CONTINUOUS) {
        roll = _compute_spray(attack, accuracy, &ammoQuantity, &v26, anim);
    } else {
        int chance = critterGetStat(attack->attacker, STAT_CRITICAL_CHANCE);
        roll = randomRoll(accuracy, chance - hit_location_penalty[attack->defenderHitLocation], NULL);
    }

    if (roll == ROLL_FAILURE) {
        if (traitIsSelected(TRAIT_JINXED) || perkHasRank(gDude, PERK_JINXED)) {
            if (randomBetween(0, 1) == 1) {
                roll = ROLL_CRITICAL_FAILURE;
            }
        }
    }

    if (roll == ROLL_SUCCESS) {
        if ((attackType == ATTACK_TYPE_MELEE || attackType == ATTACK_TYPE_UNARMED) && attack->attacker == gDude) {
            if (perkHasRank(attack->attacker, PERK_SLAYER)) {
                roll = ROLL_CRITICAL_SUCCESS;
            }

            if (perkHasRank(gDude, PERK_SILENT_DEATH)
                && !_is_hit_from_front(gDude, attack->defender)
                && dudeHasState(DUDE_STATE_SNEAKING)
                && gDude != attack->defender->data.critter.combat.whoHitMe) {
                damageMultiplier = 4;
            }

            // SFALL
            int bonusCriticalChance = unarmedGetBonusCriticalChance(attack->hitMode);
            if (bonusCriticalChance != 0) {
                if (randomBetween(1, 100) <= bonusCriticalChance) {
                    roll = ROLL_CRITICAL_SUCCESS;
                }
            }
        }
    }

    if (attackType == ATTACK_TYPE_RANGED) {
        attack->ammoQuantity = v26;

        if (roll == ROLL_SUCCESS && attack->attacker == gDude) {
            if (perkGetRank(gDude, PERK_SNIPER) != 0) {
                int d10 = randomBetween(1, 10);
                int luck = critterGetStat(gDude, STAT_LUCK);
                if (d10 <= luck) {
                    roll = ROLL_CRITICAL_SUCCESS;
                }
            }
        }
    } else {
        if (ammoGetCapacity(attack->weapon) > 0) {
            attack->ammoQuantity = 1;
        }
    }

    if (_item_w_compute_ammo_cost(attack->weapon, &(attack->ammoQuantity)) == -1) {
        return -1;
    }

    switch (roll) {
    case ROLL_CRITICAL_SUCCESS:
        damageMultiplier = attackComputeCriticalHit(attack);

        // SFALL: Fix Silent Death bonus not being applied to critical hits.
        if ((attackType == ATTACK_TYPE_MELEE || attackType == ATTACK_TYPE_UNARMED) && attack->attacker == gDude) {
            if (perkHasRank(gDude, PERK_SILENT_DEATH)
                && !_is_hit_from_front(gDude, attack->defender)
                && dudeHasState(DUDE_STATE_SNEAKING)
                && gDude != attack->defender->data.critter.combat.whoHitMe) {
                damageMultiplier *= 2;
            }
        }
        // FALLTHROUGH
    case ROLL_SUCCESS:
        attack->attackerFlags |= DAM_HIT;
        attackComputeEnhancedKnockout(attack);
        attackComputeDamage(attack, ammoQuantity, damageMultiplier);
        break;
    case ROLL_FAILURE:
        if (attackType == ATTACK_TYPE_RANGED || attackType == ATTACK_TYPE_THROW) {
            _check_ranged_miss(attack);
        }
        break;
    case ROLL_CRITICAL_FAILURE:
        attackComputeCriticalFailure(attack);
        break;
    }

    if (attackType == ATTACK_TYPE_RANGED || attackType == ATTACK_TYPE_THROW) {
        if ((attack->attackerFlags & (DAM_HIT | DAM_CRITICAL)) == 0) {
            int tile;
            if (isGrenade) {
                int throwDistance = randomBetween(1, distance / 2);
                if (throwDistance == 0) {
                    throwDistance = 1;
                }

                int rotation = randomBetween(0, 5);
                tile = tileGetTileInDirection(attack->defender->tile, rotation, throwDistance);
            } else {
                tile = _tile_num_beyond(attack->attacker->tile, attack->defender->tile, range);
            }

            attack->tile = tile;

            Object* accidentalTarget = attack->defender;
            _make_straight_path_func(accidentalTarget, attack->defender->tile, attack->tile, NULL, &accidentalTarget, 32, _obj_shoot_blocking_at);
            if (accidentalTarget != NULL && accidentalTarget != attack->defender) {
                attack->tile = accidentalTarget->tile;
            } else {
                accidentalTarget = _obj_blocking_at(NULL, attack->tile, attack->defender->elevation);
            }

            if (accidentalTarget != NULL && (accidentalTarget->flags & OBJECT_SHOOT_THRU) == 0) {
                attack->attackerFlags |= DAM_HIT;
                attack->defender = accidentalTarget;
                attackComputeDamage(attack, 1, 2);
            }
        }
    }

    // SFALL
    if ((damageType == explosionGetDamageType() || isGrenade) && ((attack->attackerFlags & DAM_HIT) != 0 || (attack->attackerFlags & DAM_CRITICAL) == 0)) {
        _compute_explosion_on_extras(attack, 0, isGrenade, 0);
    } else {
        if ((attack->attackerFlags & DAM_EXPLODE) != 0) {
            _compute_explosion_on_extras(attack, 1, isGrenade, 0);
        }
    }

    attackComputeDeathFlags(attack);

    return 0;
}

// compute_explosion_on_extras
// 0x423C10
void _compute_explosion_on_extras(Attack* attack, bool isFromAttacker, bool isGrenade, bool noDamage)
{
    Object* targetObj;

    if (isFromAttacker) {
        targetObj = attack->attacker;
    } else {
        if ((attack->attackerFlags & DAM_HIT) != 0) {
            targetObj = attack->defender;
        } else {
            targetObj = NULL;
        }
    }

    int explosionTile;
    if (targetObj != NULL) {
        explosionTile = targetObj->tile;
    } else {
        explosionTile = attack->tile;
    }

    if (explosionTile == -1) {
        debugPrint("\nError: compute_explosion_on_extras: Called with bad target/tileNum");
        return;
    }

    int ringTileIdx;
    int radius = 0;
    int rotation = 0;
    int tile = -1;
    int ringFirstTile = explosionTile;

    // SFALL
    int maxTargets = explosionGetMaxTargets();
    // Check adjacent tiles for possible targets, going ring-by-ring
    while (attack->extrasLength < maxTargets) {
        if (radius != 0 && (tile == -1 || (tile = tileGetTileInDirection(tile, rotation, 1)) != ringFirstTile)) {
            ringTileIdx++;
            if (ringTileIdx % radius == 0) { // the larger the radius, the slower we rotate
                rotation += 1;
                if (rotation == ROTATION_COUNT) {
                    rotation = ROTATION_NE;
                }
            }
        } else {
            radius++; // go to the next ring
            if (isGrenade && weaponGetGrenadeExplosionRadius(attack->weapon) < radius) {
                tile = -1;
            } else if (isGrenade || weaponGetRocketExplosionRadius(attack->weapon) >= radius) {
                tile = tileGetTileInDirection(ringFirstTile, ROTATION_NE, 1);
            } else {
                tile = -1;
            }

            ringFirstTile = tile;
            rotation = ROTATION_SE;
            ringTileIdx = 0;
        }

        if (tile == -1) {
            break;
        }

        Object* obstacle = _obj_blocking_at(targetObj, tile, attack->attacker->elevation);
        if (obstacle != NULL
            && FID_TYPE(obstacle->fid) == OBJ_TYPE_CRITTER
            && (obstacle->data.critter.combat.results & DAM_DEAD) == 0
            && (obstacle->flags & OBJECT_SHOOT_THRU) == 0
            && !_combat_is_shot_blocked(obstacle, obstacle->tile, explosionTile, NULL, NULL)) {
            if (obstacle == attack->attacker) {
                attack->attackerFlags &= ~DAM_HIT;
                attackComputeDamage(attack, 1, 2);
                attack->attackerFlags |= DAM_HIT;
                attack->attackerFlags |= DAM_BACKWASH;
            } else {
                int index;
                for (index = 0; index < attack->extrasLength; index++) {
                    if (attack->extras[index] == obstacle) {
                        break;
                    }
                }

                if (index == attack->extrasLength) {
                    attack->extrasHitLocation[index] = HIT_LOCATION_TORSO;
                    attack->extras[index] = obstacle;
                    attackInit(&_explosion_ctd, attack->attacker, obstacle, attack->hitMode, HIT_LOCATION_TORSO);
                    if (!noDamage) {
                        _explosion_ctd.attackerFlags |= DAM_HIT;
                        attackComputeDamage(&_explosion_ctd, 1, 2);
                    }

                    attack->extrasDamage[index] = _explosion_ctd.defenderDamage;
                    attack->extrasFlags[index] = _explosion_ctd.defenderFlags;
                    attack->extrasKnockback[index] = _explosion_ctd.defenderKnockback;
                    attack->extrasLength += 1;
                }
            }
        }
    }
}

// 0x423EB4
static int attackComputeCriticalHit(Attack* attack)
{
    Object* defender = attack->defender;
    if (defender != NULL && _critter_flag_check(defender->pid, CRITTER_INVULNERABLE)) {
        return 2;
    }

    if (defender != NULL && PID_TYPE(defender->pid) != OBJ_TYPE_CRITTER) {
        return 2;
    }

    attack->attackerFlags |= DAM_CRITICAL;

    int chance = randomBetween(1, 100);

    chance += critterGetStat(attack->attacker, STAT_BETTER_CRITICALS);

    int effect;
    if (chance <= 20)
        effect = 0;
    else if (chance <= 45)
        effect = 1;
    else if (chance <= 70)
        effect = 2;
    else if (chance <= 90)
        effect = 3;
    else if (chance <= 100)
        effect = 4;
    else
        effect = 5;

    CriticalHitDescription* criticalHitDescription;
    if (defender == gDude) {
        criticalHitDescription = &(gPlayerCriticalHitTable[attack->defenderHitLocation][effect]);
    } else {
        int killType = critterGetKillType(defender);
        criticalHitDescription = &(gCriticalHitTables[killType][attack->defenderHitLocation][effect]);
    }

    attack->defenderFlags |= criticalHitDescription->flags;

    // NOTE: Original code is slightly different, it does not set message in
    // advance, instead using "else" statement.
    attack->criticalMessageId = criticalHitDescription->messageId;

    if (criticalHitDescription->massiveCriticalStat != -1) {
        if (statRoll(defender, criticalHitDescription->massiveCriticalStat, criticalHitDescription->massiveCriticalStatModifier, NULL) <= ROLL_FAILURE) {
            attack->defenderFlags |= criticalHitDescription->massiveCriticalFlags;
            attack->criticalMessageId = criticalHitDescription->massiveCriticalMessageId;
        }
    }

    if ((attack->defenderFlags & DAM_CRIP_RANDOM) != 0) {
        // NOTE: Uninline.
        _do_random_cripple(&(attack->defenderFlags));
    }

    if (weaponGetPerk(attack->weapon) == PERK_WEAPON_ENHANCED_KNOCKOUT) {
        attack->defenderFlags |= DAM_KNOCKED_OUT;
    }

    Object* weapon = NULL;
    if (defender != gDude) {
        weapon = critterGetWeaponForHitMode(defender, HIT_MODE_RIGHT_WEAPON_PRIMARY);
    }

    int flags = _attackFindInvalidFlags(defender, weapon);
    attack->defenderFlags &= ~flags;

    return criticalHitDescription->damageMultiplier;
}

// 0x424088
static int _attackFindInvalidFlags(Object* critter, Object* item)
{
    int flags = 0;

    if (critter != NULL && PID_TYPE(critter->pid) == OBJ_TYPE_CRITTER && _critter_flag_check(critter->pid, CRITTER_NO_DROP)) {
        flags |= DAM_DROP;
    }

    if (item != NULL && itemIsHidden(item)) {
        flags |= DAM_DROP;
    }

    return flags;
}

// 0x4240DC
static int attackComputeCriticalFailure(Attack* attack)
{
    attack->attackerFlags &= ~DAM_HIT;

    if (attack->attacker != NULL && _critter_flag_check(attack->attacker->pid, CRITTER_INVULNERABLE)) {
        return 0;
    }

    if (attack->attacker == gDude) {
        // SFALL: Remove criticals time limits.
        bool criticalsTimeLimitsRemoved = false;
        configGetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_REMOVE_CRITICALS_TIME_LIMITS_KEY, &criticalsTimeLimitsRemoved);

        unsigned int gameTime = gameTimeGetTime();
        if (!criticalsTimeLimitsRemoved && gameTime / GAME_TIME_TICKS_PER_DAY < 6) {
            return 0;
        }
    }

    int attackType = weaponGetAttackTypeForHitMode(attack->weapon, attack->hitMode);
    int criticalFailureTableIndex = weaponGetCriticalFailureType(attack->weapon);
    if (criticalFailureTableIndex == -1) {
        criticalFailureTableIndex = 0;
    }

    int chance = randomBetween(1, 100) - 5 * (critterGetStat(attack->attacker, STAT_LUCK) - 5);

    int effect;
    if (chance <= 20)
        effect = 0;
    else if (chance <= 50)
        effect = 1;
    else if (chance <= 75)
        effect = 2;
    else if (chance <= 95)
        effect = 3;
    else
        effect = 4;

    int flags = _cf_table[criticalFailureTableIndex][effect];
    if (flags == 0) {
        return 0;
    }

    attack->attackerFlags |= DAM_CRITICAL;
    attack->attackerFlags |= flags;

    int v17 = _attackFindInvalidFlags(attack->attacker, attack->weapon);
    attack->attackerFlags &= ~v17;

    if ((attack->attackerFlags & DAM_HIT_SELF) != 0) {
        int ammoQuantity = attackType == ATTACK_TYPE_RANGED ? attack->ammoQuantity : 1;
        attackComputeDamage(attack, ammoQuantity, 2);
    } else if ((attack->attackerFlags & DAM_EXPLODE) != 0) {
        attackComputeDamage(attack, 1, 2);
    }

    if ((attack->attackerFlags & DAM_LOSE_TURN) != 0) {
        attack->attacker->data.critter.combat.ap = 0;
    }

    if ((attack->attackerFlags & DAM_LOSE_AMMO) != 0) {
        if (attackType == ATTACK_TYPE_RANGED) {
            attack->ammoQuantity = ammoGetQuantity(attack->weapon);
        } else {
            attack->attackerFlags &= ~DAM_LOSE_AMMO;
        }
    }

    if ((attack->attackerFlags & DAM_CRIP_RANDOM) != 0) {
        // NOTE: Uninline.
        _do_random_cripple(&(attack->attackerFlags));
    }

    if ((attack->attackerFlags & DAM_RANDOM_HIT) != 0) {
        attack->defender = _combat_ai_random_target(attack);
        if (attack->defender != NULL) {
            attack->attackerFlags |= DAM_HIT;
            attack->defenderHitLocation = HIT_LOCATION_TORSO;
            attack->attackerFlags &= ~DAM_CRITICAL;

            int ammoQuantity = attackType == ATTACK_TYPE_RANGED ? attack->ammoQuantity : 1;
            attackComputeDamage(attack, ammoQuantity, 2);
        } else {
            attack->defender = attack->oops;
        }

        if (attack->defender != NULL) {
            attack->tile = attack->defender->tile;
        }
    }

    return 0;
}

// 0x42432C
static void _do_random_cripple(int* flagsPtr)
{
    *flagsPtr &= ~DAM_CRIP_RANDOM;

    switch (randomBetween(0, 3)) {
    case 0:
        *flagsPtr |= DAM_CRIP_LEG_LEFT;
        break;
    case 1:
        *flagsPtr |= DAM_CRIP_LEG_RIGHT;
        break;
    case 2:
        *flagsPtr |= DAM_CRIP_ARM_LEFT;
        break;
    case 3:
        *flagsPtr |= DAM_CRIP_ARM_RIGHT;
        break;
    }
}

// 0x42436C
int _determine_to_hit(Object* attacker, Object* defender, int hitLocation, int hitMode)
{
    return attackDetermineToHit(attacker, attacker->tile, defender, hitLocation, hitMode, true);
}

// 0x424380
int _determine_to_hit_no_range(Object* attacker, Object* defender, int hitLocation, int hitMode, unsigned char* a5)
{
    return attackDetermineToHit(attacker, attacker->tile, defender, hitLocation, hitMode, false);
}

// 0x424394
int _determine_to_hit_from_tile(Object* attacker, int tile, Object* defender, int hitLocation, int hitMode)
{
    return attackDetermineToHit(attacker, tile, defender, hitLocation, hitMode, true);
}

// determine_to_hit
// 0x4243A8
static int attackDetermineToHit(Object* attacker, int tile, Object* defender, int hitLocation, int hitMode, bool useDistance)
{
    Object* weapon = critterGetWeaponForHitMode(attacker, hitMode);

    bool targetIsCritter = defender != NULL
        ? FID_TYPE(defender->fid) == OBJ_TYPE_CRITTER
        : false;

    bool isRangedWeapon = false;

    int toHit;
    if (weapon == NULL || isUnarmedHitMode(hitMode)) {
        toHit = skillGetValue(attacker, SKILL_UNARMED);
    } else {
        toHit = weaponGetSkillValue(attacker, hitMode);

        int attackType = weaponGetAttackTypeForHitMode(weapon, hitMode);
        if (attackType == ATTACK_TYPE_RANGED || attackType == ATTACK_TYPE_THROW) {
            isRangedWeapon = true;

            int perceptionBonusMult = 0;
            int minEffectiveDist = 0;

            int weaponPerk = weaponGetPerk(weapon);
            switch (weaponPerk) {
            case PERK_WEAPON_LONG_RANGE:
                perceptionBonusMult = 4;
                break;
            case PERK_WEAPON_SCOPE_RANGE:
                perceptionBonusMult = 5;
                minEffectiveDist = 8;
                break;
            default:
                perceptionBonusMult = 2;
                break;
            }

            int perception = critterGetStat(attacker, STAT_PERCEPTION);

            // SFALL: Fix Sharpshooter.
            if (attacker == gDude) {
                perception += 2 * perkGetRank(gDude, PERK_SHARPSHOOTER);
            }

            int distanceMod = 0;
            // SFALL: Fix for `determine_to_hit_func` function taking distance
            // into account when called from `determine_to_hit_no_range`.
            if (defender != NULL && useDistance) {
                distanceMod = objectGetDistanceBetweenTiles(attacker, tile, defender, defender->tile);
            } else {
                distanceMod = 0;
            }

            if (distanceMod >= minEffectiveDist) {
                int perceptionBonus = attacker == gDude
                    ? perceptionBonusMult * (perception - 2)
                    : perceptionBonusMult * perception;

                distanceMod -= perceptionBonus;
            } else {
                distanceMod += minEffectiveDist;
            }

            if (distanceMod < -2 * perception) {
                distanceMod = -2 * perception;
            }

            if (distanceMod >= 0) {
                if ((attacker->data.critter.combat.results & DAM_BLIND) != 0) {
                    distanceMod *= -12;
                } else {
                    distanceMod *= -4;
                }
            } else {
                distanceMod *= -4;
            }

            if (useDistance || distanceMod > 0) {
                toHit += distanceMod;
            }

            int numCrittersInLof = 0;

            if (defender != NULL && useDistance) {
                _combat_is_shot_blocked(attacker, tile, defender->tile, defender, &numCrittersInLof);
            }

            toHit -= 10 * numCrittersInLof;
        }

        if (attacker == gDude && traitIsSelected(TRAIT_ONE_HANDER)) {
            if (weaponIsTwoHanded(weapon)) {
                toHit -= 40;
            } else {
                toHit += 20;
            }
        }

        int minStrength = weaponGetMinStrengthRequired(weapon);
        int minStrengthMod = minStrength - critterGetStat(attacker, STAT_STRENGTH);
        if (attacker == gDude && perkGetRank(gDude, PERK_WEAPON_HANDLING) != 0) {
            minStrengthMod -= 3;
        }

        if (minStrengthMod > 0) {
            toHit -= 20 * minStrengthMod;
        }

        if (weaponGetPerk(weapon) == PERK_WEAPON_ACCURATE) {
            toHit += 20;
        }
    }

    if (targetIsCritter && defender != NULL) {
        int armorClass = critterGetStat(defender, STAT_ARMOR_CLASS);
        armorClass += weaponGetAmmoArmorClassModifier(weapon);
        if (armorClass < 0) {
            armorClass = 0;
        }

        toHit -= armorClass;
    }

    if (isRangedWeapon) {
        toHit += hit_location_penalty[hitLocation];
    } else {
        toHit += hit_location_penalty[hitLocation] / 2;
    }

    if (defender != NULL && (defender->flags & OBJECT_MULTIHEX) != 0) {
        toHit += 15;
    }

    if (attacker == gDude) {
        int lightIntensity;
        if (defender != NULL) {
            lightIntensity = objectGetLightIntensity(defender);
            if (weaponGetPerk(weapon) == PERK_WEAPON_NIGHT_SIGHT) {
                lightIntensity = 65536;
            }
        } else {
            lightIntensity = 0;
        }

        if (lightIntensity <= 26214)
            toHit -= 40;
        else if (lightIntensity <= 39321)
            toHit -= 25;
        else if (lightIntensity <= 52428)
            toHit -= 10;
    }

    if (_gcsd != NULL) {
        toHit += _gcsd->accuracyBonus;
    }

    if ((attacker->data.critter.combat.results & DAM_BLIND) != 0) {
        toHit -= 25;
    }

    if (targetIsCritter && defender != NULL && (defender->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0) {
        toHit += 40;
    }

    if (attacker->data.critter.combat.team != gDude->data.critter.combat.team) {
        switch (settings.preferences.combat_difficulty) {
        case 0:
            toHit -= 20;
            break;
        case 2:
            toHit += 20;
            break;
        }
    }

    if (toHit > 95) {
        toHit = 95;
    }

    if (toHit < -100) {
        debugPrint("Whoa! Bad skill value in determine_to_hit!\n");
    }

    return toHit;
}

// 0x4247B8
static void attackComputeDamage(Attack* attack, int ammoQuantity, int bonusDamageMultiplier)
{
    int* damagePtr;
    Object* critter;
    int* flagsPtr;
    int* knockbackDistancePtr;

    if ((attack->attackerFlags & DAM_HIT) != 0) {
        damagePtr = &(attack->defenderDamage);
        critter = attack->defender;
        flagsPtr = &(attack->defenderFlags);
        knockbackDistancePtr = &(attack->defenderKnockback);
    } else {
        damagePtr = &(attack->attackerDamage);
        critter = attack->attacker;
        flagsPtr = &(attack->attackerFlags);
        knockbackDistancePtr = NULL;
    }

    *damagePtr = 0;

    if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
        return;
    }

    int damageType = weaponGetDamageType(attack->attacker, attack->weapon);
    int damageThreshold = critterGetStat(critter, STAT_DAMAGE_THRESHOLD + damageType);
    int damageResistance = critterGetStat(critter, STAT_DAMAGE_RESISTANCE + damageType);

    if ((*flagsPtr & DAM_BYPASS) != 0 && damageType != DAMAGE_TYPE_EMP) {
        damageThreshold = 20 * damageThreshold / 100;
        damageResistance = 20 * damageResistance / 100;
    } else {
        // SFALL
        if (weaponGetPerk(attack->weapon) == PERK_WEAPON_PENETRATE
            || unarmedIsPenetrating(attack->hitMode)) {
            damageThreshold = 20 * damageThreshold / 100;
        }

        if (attack->attacker == gDude && traitIsSelected(TRAIT_FINESSE)) {
            damageResistance += 30;
        }
    }

    int damageBonus;
    if (attack->attacker == gDude && weaponGetAttackTypeForHitMode(attack->weapon, attack->hitMode) == ATTACK_TYPE_RANGED) {
        damageBonus = 2 * perkGetRank(gDude, PERK_BONUS_RANGED_DAMAGE);
    } else {
        damageBonus = 0;
    }

    int combatDifficultyDamageModifier = 100;
    if (attack->attacker->data.critter.combat.team != gDude->data.critter.combat.team) {
        switch (settings.preferences.combat_difficulty) {
        case COMBAT_DIFFICULTY_EASY:
            combatDifficultyDamageModifier = 75;
            break;
        case COMBAT_DIFFICULTY_HARD:
            combatDifficultyDamageModifier = 125;
            break;
        }
    }

    // SFALL: Damage mod.
    DamageCalculationContext context;
    context.attack = attack;
    context.damagePtr = damagePtr;
    context.damageResistance = damageResistance;
    context.damageThreshold = damageThreshold;
    context.damageBonus = damageBonus;
    context.bonusDamageMultiplier = bonusDamageMultiplier;
    context.combatDifficultyDamageModifier = combatDifficultyDamageModifier;

    if (gDamageCalculationType == DAMAGE_CALCULATION_TYPE_GLOVZ || gDamageCalculationType == DAMAGE_CALCULATION_TYPE_GLOVZ_WITH_DAMAGE_MULTIPLIER_TWEAK) {
        damageModCalculateGlovz(&context);
    } else if (gDamageCalculationType == DAMAGE_CALCULATION_TYPE_YAAM) {
        damageModCalculateYaam(&context);
    } else {
        damageResistance += weaponGetAmmoDamageResistanceModifier(attack->weapon);
        if (damageResistance > 100) {
            damageResistance = 100;
        } else if (damageResistance < 0) {
            damageResistance = 0;
        }

        int damageMultiplier = bonusDamageMultiplier * weaponGetAmmoDamageMultiplier(attack->weapon);
        int damageDivisor = weaponGetAmmoDamageDivisor(attack->weapon);

        for (int index = 0; index < ammoQuantity; index++) {
            int damage = weaponGetDamage(attack->attacker, attack->hitMode);

            damage += damageBonus;

            damage *= damageMultiplier;

            if (damageDivisor != 0) {
                damage /= damageDivisor;
            }

            // TODO: Why we're halving it?
            damage /= 2;

            damage *= combatDifficultyDamageModifier;
            damage /= 100;

            damage -= damageThreshold;

            if (damage > 0) {
                damage -= damage * damageResistance / 100;
            }

            if (damage > 0) {
                *damagePtr += damage;
            }
        }
    }

    if (attack->attacker == gDude) {
        if (perkGetRank(attack->attacker, PERK_LIVING_ANATOMY) != 0) {
            int kt = critterGetKillType(attack->defender);
            if (kt != KILL_TYPE_ROBOT && kt != KILL_TYPE_ALIEN) {
                *damagePtr += 5;
            }
        }

        if (perkGetRank(attack->attacker, PERK_PYROMANIAC) != 0) {
            if (weaponGetDamageType(attack->attacker, attack->weapon) == DAMAGE_TYPE_FIRE) {
                *damagePtr += 5;
            }
        }
    }

    if (knockbackDistancePtr != NULL
        && (critter->flags & OBJECT_MULTIHEX) == 0
        && (damageType == DAMAGE_TYPE_EXPLOSION || attack->weapon == NULL || weaponGetAttackTypeForHitMode(attack->weapon, attack->hitMode) == ATTACK_TYPE_MELEE)
        && PID_TYPE(critter->pid) == OBJ_TYPE_CRITTER
        && !_critter_flag_check(critter->pid, CRITTER_NO_KNOCKBACK)) {
        bool shouldKnockback = true;
        bool hasStonewall = false;
        if (critter == gDude) {
            if (perkGetRank(critter, PERK_STONEWALL) != 0) {
                int chance = randomBetween(0, 100);
                hasStonewall = true;
                if (chance < 50) {
                    shouldKnockback = false;
                }
            }
        }

        if (shouldKnockback) {
            int knockbackDistanceDivisor = weaponGetPerk(attack->weapon) == PERK_WEAPON_KNOCKBACK ? 5 : 10;

            *knockbackDistancePtr = *damagePtr / knockbackDistanceDivisor;

            if (hasStonewall) {
                *knockbackDistancePtr /= 2;
            }
        }
    }
}

// 0x424BAC
void attackComputeDeathFlags(Attack* attack)
{
    _check_for_death(attack->attacker, attack->attackerDamage, &(attack->attackerFlags));
    _check_for_death(attack->defender, attack->defenderDamage, &(attack->defenderFlags));

    for (int index = 0; index < attack->extrasLength; index++) {
        _check_for_death(attack->extras[index], attack->extrasDamage[index], &(attack->extrasFlags[index]));
    }
}

// 0x424C04
void _apply_damage(Attack* attack, bool animated)
{
    Object* attacker = attack->attacker;
    bool attackerIsCritter = attacker != NULL && FID_TYPE(attacker->fid) == OBJ_TYPE_CRITTER;
    bool v5 = attack->defender != attack->oops;

    if (attackerIsCritter && (attacker->data.critter.combat.results & DAM_DEAD) == 0) {
        _set_new_results(attacker, attack->attackerFlags);
        // TODO: Not sure about "attack->defender == attack->oops".
        _damage_object(attacker, attack->attackerDamage, animated, attack->defender == attack->oops, attacker);
    }

    Object* v7 = attack->oops;
    if (v7 != NULL && v7 != attack->defender) {
        _combatai_notify_onlookers(v7);
    }

    Object* defender = attack->defender;
    bool defenderIsCritter = defender != NULL && FID_TYPE(defender->fid) == OBJ_TYPE_CRITTER;

    if (!defenderIsCritter && !v5) {
        bool v9 = objectIsPartyMember(attack->defender) && objectIsPartyMember(attack->attacker) ? false : true;
        if (v9) {
            if (defender != NULL) {
                if (defender->sid != -1) {
                    scriptSetFixedParam(defender->sid, attack->attackerDamage);
                    scriptSetObjects(defender->sid, attack->attacker, attack->weapon);
                    scriptExecProc(defender->sid, SCRIPT_PROC_DAMAGE);
                }
            }
        }
    }

    if (defenderIsCritter && (defender->data.critter.combat.results & DAM_DEAD) == 0) {
        _set_new_results(defender, attack->defenderFlags);

        if (defenderIsCritter) {
            if (defenderIsCritter) {
                if ((defender->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
                    if (!v5 || defender != gDude) {
                        _critter_set_who_hit_me(defender, attack->attacker);
                    }
                } else if (defender == attack->oops || defender->data.critter.combat.team != attack->attacker->data.critter.combat.team) {
                    _combatai_check_retaliation(defender, attack->attacker);
                }
            }
        }

        scriptSetObjects(defender->sid, attack->attacker, attack->weapon);
        _damage_object(defender, attack->defenderDamage, animated, attack->defender != attack->oops, attacker);

        if (defenderIsCritter) {
            _combatai_notify_onlookers(defender);
        }

        if (attack->defenderDamage >= 0 && (attack->attackerFlags & DAM_HIT) != 0) {
            scriptSetObjects(attack->attacker->sid, NULL, attack->defender);
            scriptSetFixedParam(attack->attacker->sid, 2);
            scriptExecProc(attack->attacker->sid, SCRIPT_PROC_COMBAT);
        }
    }

    for (int index = 0; index < attack->extrasLength; index++) {
        Object* obj = attack->extras[index];
        if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER && (obj->data.critter.combat.results & DAM_DEAD) == 0) {
            _set_new_results(obj, attack->extrasFlags[index]);

            if (defenderIsCritter) {
                if ((obj->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
                    _critter_set_who_hit_me(obj, attack->attacker);
                } else if (obj->data.critter.combat.team != attack->attacker->data.critter.combat.team) {
                    _combatai_check_retaliation(obj, attack->attacker);
                }
            }

            scriptSetObjects(obj->sid, attack->attacker, attack->weapon);
            // TODO: Not sure about defender == oops.
            _damage_object(obj, attack->extrasDamage[index], animated, attack->defender == attack->oops, attack->attacker);
            _combatai_notify_onlookers(obj);

            if (attack->extrasDamage[index] >= 0) {
                if ((attack->attackerFlags & DAM_HIT) != 0) {
                    scriptSetObjects(attack->attacker->sid, NULL, obj);
                    scriptSetFixedParam(attack->attacker->sid, 2);
                    scriptExecProc(attack->attacker->sid, SCRIPT_PROC_COMBAT);
                }
            }
        }
    }
}

// 0x424EE8
static void _check_for_death(Object* object, int damage, int* flags)
{
    if (object == NULL || !_critter_flag_check(object->pid, CRITTER_INVULNERABLE)) {
        if (object == NULL || PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            if (damage > 0) {
                if (critterGetHitPoints(object) - damage <= 0) {
                    *flags |= DAM_DEAD;
                }
            }
        }
    }
}

// 0x424F2C
static void _set_new_results(Object* critter, int flags)
{
    if (critter == NULL) {
        return;
    }

    if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
        return;
    }

    if (_critter_flag_check(critter->pid, CRITTER_INVULNERABLE)) {
        return;
    }

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return;
    }

    if ((flags & DAM_DEAD) != 0) {
        queueRemoveEvents(critter);
    } else if ((flags & DAM_KNOCKED_OUT) != 0) {
        // SFALL: Fix multiple knockout events.
        queueRemoveEventsByType(critter, EVENT_TYPE_KNOCKOUT);

        int endurance = critterGetStat(critter, STAT_ENDURANCE);
        queueAddEvent(10 * (35 - 3 * endurance), critter, NULL, EVENT_TYPE_KNOCKOUT);
    }

    if (critter == gDude && (flags & DAM_CRIP_ARM_ANY) != 0) {
        critter->data.critter.combat.results |= flags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_CRIP | DAM_DEAD | DAM_LOSE_TURN);

        int leftItemAction;
        int rightItemAction;
        interfaceGetItemActions(&leftItemAction, &rightItemAction);
        interfaceUpdateItems(true, leftItemAction, rightItemAction);
    } else {
        critter->data.critter.combat.results |= flags & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_CRIP | DAM_DEAD | DAM_LOSE_TURN);
    }
}

// 0x425020
static void _damage_object(Object* a1, int damage, bool animated, int a4, Object* a5)
{
    if (a1 == NULL) {
        return;
    }

    if (FID_TYPE(a1->fid) != OBJ_TYPE_CRITTER) {
        return;
    }

    if (_critter_flag_check(a1->pid, CRITTER_INVULNERABLE)) {
        return;
    }

    if (damage <= 0) {
        return;
    }

    critterAdjustHitPoints(a1, -damage);

    if (a1 == gDude) {
        interfaceRenderHitPoints(animated);
    }

    a1->data.critter.combat.damageLastTurn += damage;

    if (!a4) {
        // TODO: Not sure about this one.
        if (!objectIsPartyMember(a1) || !objectIsPartyMember(a5)) {
            scriptSetFixedParam(a1->sid, damage);
            scriptExecProc(a1->sid, SCRIPT_PROC_DAMAGE);
        }
    }

    if ((a1->data.critter.combat.results & DAM_DEAD) != 0) {
        scriptSetObjects(a1->sid, a1->data.critter.combat.whoHitMe, NULL);
        scriptExecProc(a1->sid, SCRIPT_PROC_DESTROY);
        itemDestroyAllHidden(a1);

        if (a1 != gDude) {
            Object* whoHitMe = a1->data.critter.combat.whoHitMe;
            if (whoHitMe == gDude || (whoHitMe != NULL && whoHitMe->data.critter.combat.team == gDude->data.critter.combat.team)) {
                bool scriptOverrides = false;
                Script* scr;
                if (scriptGetScript(a1->sid, &scr) != -1) {
                    scriptOverrides = scr->scriptOverrides;
                }

                if (!scriptOverrides) {
                    _combat_exps += critterGetExp(a1);
                    killsIncByType(critterGetKillType(a1));
                }
            }
        }

        if (a1->sid != -1) {
            scriptRemove(a1->sid);
            a1->sid = -1;
        }

        partyMemberRemove(a1);
    }
}

// Print attack description to monitor.
//
// 0x425170
void _combat_display(Attack* attack)
{
    MessageListItem messageListItem;

    if (attack->attacker == gDude) {
        Object* weapon = critterGetWeaponForHitMode(attack->attacker, attack->hitMode);
        int strengthRequired = weaponGetMinStrengthRequired(weapon);

        if (perkGetRank(attack->attacker, PERK_WEAPON_HANDLING) != 0) {
            strengthRequired -= 3;
        }

        if (weapon != NULL) {
            if (strengthRequired > critterGetStat(gDude, STAT_STRENGTH)) {
                // You are not strong enough to use this weapon properly.
                messageListItem.num = 107;
                if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                    displayMonitorAddMessage(messageListItem.text);
                }
            }
        }
    }

    Object* mainCritter;
    if ((attack->attackerFlags & DAM_HIT) != 0) {
        mainCritter = attack->defender;
    } else {
        mainCritter = attack->attacker;
    }

    char* mainCritterName = _a_1;

    char you[20];
    you[0] = '\0';
    if (critterGetStat(gDude, STAT_GENDER) == GENDER_MALE) {
        // You (male)
        messageListItem.num = 506;
    } else {
        // You (female)
        messageListItem.num = 556;
    }

    if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
        strcpy(you, messageListItem.text);
    }

    int baseMessageId;
    if (mainCritter == gDude) {
        mainCritterName = you;
        if (critterGetStat(gDude, STAT_GENDER) == GENDER_MALE) {
            baseMessageId = 500;
        } else {
            baseMessageId = 550;
        }
    } else if (mainCritter != NULL) {
        mainCritterName = objectGetName(mainCritter);
        if (critterGetStat(mainCritter, STAT_GENDER) == GENDER_MALE) {
            baseMessageId = 600;
        } else {
            baseMessageId = 700;
        }
    }

    char text[280];
    if (attack->defender != NULL
        && attack->oops != NULL
        && attack->defender != attack->oops
        && (attack->attackerFlags & DAM_HIT) != 0) {
        if (FID_TYPE(attack->defender->fid) == OBJ_TYPE_CRITTER) {
            if (attack->oops == gDude) {
                // 608 (male) - Oops! %s was hit instead of you!
                // 708 (female) - Oops! %s was hit instead of you!
                messageListItem.num = baseMessageId + 8;
                if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                    snprintf(text, sizeof(text), messageListItem.text, mainCritterName);
                }
            } else {
                // 509 (male) - Oops! %s were hit instead of %s!
                // 559 (female) - Oops! %s were hit instead of %s!
                const char* name = objectGetName(attack->oops);
                messageListItem.num = baseMessageId + 9;
                if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                    snprintf(text, sizeof(text), messageListItem.text, mainCritterName, name);
                }
            }
        } else {
            if (attack->attacker == gDude) {
                if (critterGetStat(attack->attacker, STAT_GENDER) == GENDER_MALE) {
                    // (male) %s missed
                    messageListItem.num = 515;
                } else {
                    // (female) %s missed
                    messageListItem.num = 565;
                }

                if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                    snprintf(text, sizeof(text), messageListItem.text, you);
                }
            } else {
                const char* name = objectGetName(attack->attacker);
                if (critterGetStat(attack->attacker, STAT_GENDER) == GENDER_MALE) {
                    // (male) %s missed
                    messageListItem.num = 615;
                } else {
                    // (female) %s missed
                    messageListItem.num = 715;
                }

                if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                    snprintf(text, sizeof(text), messageListItem.text, name);
                }
            }
        }

        strcat(text, ".");

        displayMonitorAddMessage(text);
    }

    if ((attack->attackerFlags & DAM_HIT) != 0) {
        Object* v21 = attack->defender;
        if (v21 != NULL && (v21->data.critter.combat.results & DAM_DEAD) == 0) {
            text[0] = '\0';

            if (FID_TYPE(v21->fid) == OBJ_TYPE_CRITTER) {
                if (attack->defenderHitLocation == HIT_LOCATION_TORSO) {
                    if ((attack->attackerFlags & DAM_CRITICAL) != 0) {
                        switch (attack->defenderDamage) {
                        case 0:
                            // 528 - %s were critically hit for no damage
                            messageListItem.num = baseMessageId + 28;
                            break;
                        case 1:
                            // 524 - %s were critically hit for 1 hit point
                            messageListItem.num = baseMessageId + 24;
                            break;
                        default:
                            // 520 - %s were critically hit for %d hit points
                            messageListItem.num = baseMessageId + 20;
                            break;
                        }

                        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                            if (attack->defenderDamage <= 1) {
                                snprintf(text, sizeof(text), messageListItem.text, mainCritterName);
                            } else {
                                snprintf(text, sizeof(text), messageListItem.text, mainCritterName, attack->defenderDamage);
                            }
                        }
                    } else {
                        combatCopyDamageAmountDescription(text, sizeof(text), v21, attack->defenderDamage);
                    }
                } else {
                    const char* hitLocationName = hitLocationGetName(v21, attack->defenderHitLocation);
                    if (hitLocationName != NULL) {
                        if ((attack->attackerFlags & DAM_CRITICAL) != 0) {
                            switch (attack->defenderDamage) {
                            case 0:
                                // 525 - %s were critically hit in %s for no damage
                                messageListItem.num = baseMessageId + 25;
                                break;
                            case 1:
                                // 521 - %s were critically hit in %s for 1 damage
                                messageListItem.num = baseMessageId + 21;
                                break;
                            default:
                                // 511 - %s were critically hit in %s for %d hit points
                                messageListItem.num = baseMessageId + 11;
                                break;
                            }
                        } else {
                            switch (attack->defenderDamage) {
                            case 0:
                                // 526 - %s were hit in %s for no damage
                                messageListItem.num = baseMessageId + 26;
                                break;
                            case 1:
                                // 522 - %s were hit in %s for 1 damage
                                messageListItem.num = baseMessageId + 22;
                                break;
                            default:
                                // 512 - %s were hit in %s for %d hit points
                                messageListItem.num = baseMessageId + 12;
                                break;
                            }
                        }

                        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                            if (attack->defenderDamage <= 1) {
                                snprintf(text, sizeof(text), messageListItem.text, mainCritterName, hitLocationName);
                            } else {
                                snprintf(text, sizeof(text), messageListItem.text, mainCritterName, hitLocationName, attack->defenderDamage);
                            }
                        }
                    }
                }

                if (settings.preferences.combat_messages && (attack->attackerFlags & DAM_CRITICAL) != 0 && attack->criticalMessageId != -1) {
                    messageListItem.num = attack->criticalMessageId;
                    if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                        strcat(text, messageListItem.text);
                    }

                    if ((attack->defenderFlags & DAM_DEAD) != 0) {
                        strcat(text, ".");
                        displayMonitorAddMessage(text);

                        if (attack->defender == gDude) {
                            if (critterGetStat(attack->defender, STAT_GENDER) == GENDER_MALE) {
                                // were killed
                                messageListItem.num = 207;
                            } else {
                                // were killed
                                messageListItem.num = 257;
                            }
                        } else {
                            if (critterGetStat(attack->defender, STAT_GENDER) == GENDER_MALE) {
                                // was killed
                                messageListItem.num = 307;
                            } else {
                                // was killed
                                messageListItem.num = 407;
                            }
                        }

                        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                            snprintf(text, sizeof(text), "%s %s", mainCritterName, messageListItem.text);
                        }
                    }
                } else {
                    combatAddDamageFlagsDescription(text, attack->defenderFlags, attack->defender);
                }

                strcat(text, ".");

                displayMonitorAddMessage(text);
            }
        }
    }

    if (attack->attacker != NULL && (attack->attacker->data.critter.combat.results & DAM_DEAD) == 0) {
        if ((attack->attackerFlags & DAM_HIT) == 0) {
            if ((attack->attackerFlags & DAM_CRITICAL) != 0) {
                switch (attack->attackerDamage) {
                case 0:
                    // 514 - %s critically missed
                    messageListItem.num = baseMessageId + 14;
                    break;
                case 1:
                    // 533 - %s critically missed and took 1 hit point
                    messageListItem.num = baseMessageId + 33;
                    break;
                default:
                    // 534 - %s critically missed and took %d hit points
                    messageListItem.num = baseMessageId + 34;
                    break;
                }
            } else {
                // 515 - %s missed
                messageListItem.num = baseMessageId + 15;
            }

            if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                if (attack->attackerDamage <= 1) {
                    snprintf(text, sizeof(text), messageListItem.text, mainCritterName);
                } else {
                    snprintf(text, sizeof(text), messageListItem.text, mainCritterName, attack->attackerDamage);
                }
            }

            combatAddDamageFlagsDescription(text, attack->attackerFlags, attack->attacker);

            strcat(text, ".");

            displayMonitorAddMessage(text);
        }

        if ((attack->attackerFlags & DAM_HIT) != 0 || (attack->attackerFlags & DAM_CRITICAL) == 0) {
            if (attack->attackerDamage > 0) {
                combatCopyDamageAmountDescription(text, sizeof(text), attack->attacker, attack->attackerDamage);
                combatAddDamageFlagsDescription(text, attack->attackerFlags, attack->attacker);
                strcat(text, ".");
                displayMonitorAddMessage(text);
            }
        }
    }

    for (int index = 0; index < attack->extrasLength; index++) {
        Object* critter = attack->extras[index];
        if ((critter->data.critter.combat.results & DAM_DEAD) == 0) {
            combatCopyDamageAmountDescription(text, sizeof(text), critter, attack->extrasDamage[index]);
            combatAddDamageFlagsDescription(text, attack->extrasFlags[index], critter);
            strcat(text, ".");

            displayMonitorAddMessage(text);
        }
    }
}

// 0x425A9C
static void combatCopyDamageAmountDescription(char* dest, size_t size, Object* critter, int damage)
{
    MessageListItem messageListItem;
    char text[40];
    char* name;

    int messageId;
    if (critter == gDude) {
        text[0] = '\0';

        if (critterGetStat(gDude, STAT_GENDER) == GENDER_MALE) {
            messageId = 500;
        } else {
            messageId = 550;
        }

        // 506 - You
        messageListItem.num = messageId + 6;
        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
            strcpy(text, messageListItem.text);
        }

        name = text;
    } else {
        name = objectGetName(critter);

        if (critterGetStat(critter, STAT_GENDER) == GENDER_MALE) {
            messageId = 600;
        } else {
            messageId = 700;
        }
    }

    switch (damage) {
    case 0:
        // 627 - %s was hit for no damage
        messageId += 27;
        break;
    case 1:
        // 623 - %s was hit for 1 hit point
        messageId += 23;
        break;
    default:
        // 613 - %s was hit for %d hit points
        messageId += 13;
        break;
    }

    messageListItem.num = messageId;
    if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
        if (damage <= 1) {
            snprintf(dest, size, messageListItem.text, name);
        } else {
            snprintf(dest, size, messageListItem.text, name, damage);
        }
    }
}

// 0x425BA4
static void combatAddDamageFlagsDescription(char* dest, int flags, Object* critter)
{
    MessageListItem messageListItem;

    int num;
    if (critter == gDude) {
        if (critterGetStat(critter, STAT_GENDER) == GENDER_MALE) {
            num = 200;
        } else {
            num = 250;
        }
    } else {
        if (critterGetStat(critter, STAT_GENDER) == GENDER_MALE) {
            num = 300;
        } else {
            num = 400;
        }
    }

    if (flags == 0) {
        return;
    }

    if ((flags & DAM_DEAD) != 0) {
        // " and "
        messageListItem.num = 108;
        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
            strcat(dest, messageListItem.text);
        }

        // were killed
        messageListItem.num = num + 7;
        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
            strcat(dest, messageListItem.text);
        }

        return;
    }

    int bit = 1;
    int flagsListLength = 0;
    int flagsList[32];
    for (int index = 0; index < 32; index++) {
        if (bit != DAM_CRITICAL && bit != DAM_HIT && (bit & flags) != 0) {
            flagsList[flagsListLength++] = index;
        }
        bit <<= 1;
    }

    if (flagsListLength != 0) {
        for (int index = 0; index < flagsListLength - 1; index++) {
            strcat(dest, ", ");

            messageListItem.num = num + flagsList[index];
            if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
                strcat(dest, messageListItem.text);
            }
        }

        // " and "
        messageListItem.num = 108;
        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
            strcat(dest, messageListItem.text);
        }

        messageListItem.num = num + flagsList[flagsListLength - 1];
        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
            strcat(dest, messageListItem.text);
        }
    }
}

// 0x425E3C
void _combat_anim_begin()
{
    if (++_combat_turn_running == 1 && gDude == _main_ctd.attacker) {
        gameUiDisable(1);
        gameMouseSetCursor(26);
        if (_combat_highlight == 2) {
            _combat_outline_off();
        }
    }
}

// 0x425E80
void _combat_anim_finished()
{
    _combat_turn_running -= 1;
    if (_combat_turn_running != 0) {
        return;
    }

    if (gDude == _main_ctd.attacker) {
        gameUiEnable();
    }

    if (_combat_cleanup_enabled) {
        _combat_cleanup_enabled = false;

        Object* weapon = critterGetWeaponForHitMode(_main_ctd.attacker, _main_ctd.hitMode);
        if (weapon != NULL) {
            if (ammoGetCapacity(weapon) > 0) {
                int ammoQuantity = ammoGetQuantity(weapon);
                ammoSetQuantity(weapon, ammoQuantity - _main_ctd.ammoQuantity);

                if (_main_ctd.attacker == gDude) {
                    _intface_update_ammo_lights();
                }
            }
        }

        if (_combat_call_display) {
            _combat_display(&_main_ctd);
            _combat_call_display = false;
        }

        _apply_damage(&_main_ctd, true);

        Object* attacker = _main_ctd.attacker;
        if (attacker == gDude && _combat_highlight == 2) {
            _combat_outline_on();
        }

        if (_scr_end_combat()) {
            if ((gDude->data.critter.combat.results & DAM_KNOCKED_OUT) != 0) {
                if (attacker->data.critter.combat.team == gDude->data.critter.combat.team) {
                    _combat_ending_guy = gDude->data.critter.combat.whoHitMe;
                } else {
                    _combat_ending_guy = attacker;
                }
            }
        }

        attackInit(&_main_ctd, _main_ctd.attacker, NULL, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);

        if ((attacker->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0) {
            if ((attacker->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD | DAM_LOSE_TURN)) == 0) {
                _combat_standup(attacker);
            }
        }
    }
}

// 0x425FBC
static void _combat_standup(Object* a1)
{
    int v2;

    v2 = 3;
    if (a1 == gDude && perkGetRank(a1, PERK_QUICK_RECOVERY)) {
        v2 = 1;
    }

    if (v2 > a1->data.critter.combat.ap) {
        a1->data.critter.combat.ap = 0;
    } else {
        a1->data.critter.combat.ap -= v2;
    }

    if (a1 == gDude) {
        interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
    }

    _dude_standup(a1);

    // NOTE: Uninline.
    _combat_turn_run();
}

// Render two digits.
//
// 0x42603C
static void _print_tohit(unsigned char* dest, int destPitch, int accuracy)
{
    FrmImage numbersFrmImage;
    int numbersFid = buildFid(OBJ_TYPE_INTERFACE, 82, 0, 0, 0);
    if (!numbersFrmImage.lock(numbersFid)) {
        return;
    }

    if (accuracy >= 0) {
        blitBufferToBuffer(numbersFrmImage.getData() + 9 * (accuracy % 10), 9, 17, 360, dest + 9, destPitch);
        blitBufferToBuffer(numbersFrmImage.getData() + 9 * (accuracy / 10), 9, 17, 360, dest, destPitch);
    } else {
        blitBufferToBuffer(numbersFrmImage.getData() + 108, 6, 17, 360, dest + 9, destPitch);
        blitBufferToBuffer(numbersFrmImage.getData() + 108, 6, 17, 360, dest, destPitch);
    }
}

// 0x42612C
static char* hitLocationGetName(Object* critter, int hitLocation)
{
    MessageListItem messageListItem;
    messageListItem.num = 1000 + 10 * _art_alias_num(critter->fid & 0xFFF) + hitLocation;
    if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
        return messageListItem.text;
    }

    return NULL;
}

// 0x4261B4
static void _draw_loc_off(int a1, int a2)
{
    _draw_loc_(a2, _colorTable[992]);
}

// 0x4261C0
static void _draw_loc_on_(int a1, int a2)
{
    _draw_loc_(a2, _colorTable[31744]);
}

// 0x4261CC
static void _draw_loc_(int eventCode, int color)
{
    color |= 0x3000000;

    if (eventCode >= 4) {
        char* name = hitLocationGetName(gCalledShotCritter, _hit_loc_right[eventCode - 4]);
        int width = fontGetStringWidth(name);
        windowDrawText(gCalledShotWindow, name, 0, 431 - width, _call_ty[eventCode - 4] - 86, color);
    } else {
        char* name = hitLocationGetName(gCalledShotCritter, _hit_loc_left[eventCode]);
        windowDrawText(gCalledShotWindow, name, 0, 74, _call_ty[eventCode] - 86, color);
    }
}

// 0x426218
static int calledShotSelectHitLocation(Object* critter, int* hitLocation, int hitMode)
{
    if (critter == NULL) {
        return 0;
    }

    if (critter->pid >> 24 != OBJ_TYPE_CRITTER) {
        return 0;
    }

    gCalledShotCritter = critter;

    // The default value is 68, which centers called shot window given it's
    // width (68 - 504 - 68).
    int calledShotWindowX = (screenGetWidth() - CALLED_SHOT_WINDOW_WIDTH) / 2;
    // Center vertically for HRP, otherwise maintain original location (20).
    int calledShotWindowY = screenGetHeight() != 480
        ? (screenGetHeight() - INTERFACE_BAR_HEIGHT - 1 - CALLED_SHOT_WINDOW_HEIGHT) / 2
        : CALLED_SHOT_WINDOW_Y;
    gCalledShotWindow = windowCreate(calledShotWindowX,
        calledShotWindowY,
        CALLED_SHOT_WINDOW_WIDTH,
        CALLED_SHOT_WINDOW_HEIGHT,
        _colorTable[0],
        WINDOW_MODAL);
    if (gCalledShotWindow == -1) {
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(gCalledShotWindow);

    FrmImage backgroundFrm;
    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 118, 0, 0, 0);
    if (!backgroundFrm.lock(backgroundFid)) {
        windowDestroy(gCalledShotWindow);
        return -1;
    }

    blitBufferToBuffer(backgroundFrm.getData(),
        CALLED_SHOT_WINDOW_WIDTH,
        CALLED_SHOT_WINDOW_HEIGHT,
        CALLED_SHOT_WINDOW_WIDTH,
        windowBuffer,
        CALLED_SHOT_WINDOW_WIDTH);

    FrmImage critterFrm;
    int critterFid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, ANIM_CALLED_SHOT_PIC, 0, 0);
    if (critterFrm.lock(critterFid)) {
        blitBufferToBuffer(critterFrm.getData(),
            170,
            225,
            170,
            windowBuffer + CALLED_SHOT_WINDOW_WIDTH * 31 + 168,
            CALLED_SHOT_WINDOW_WIDTH);
    }

    FrmImage cancelButtonNormalFrmImage;
    int cancelButtonNormalFid = buildFid(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    if (!cancelButtonNormalFrmImage.lock(cancelButtonNormalFid)) {
        windowDestroy(gCalledShotWindow);
        return -1;
    }

    FrmImage cancelButtonPressedFrmImage;
    int cancelButtonPressedFid = buildFid(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    if (!cancelButtonPressedFrmImage.lock(cancelButtonPressedFid)) {
        windowDestroy(gCalledShotWindow);
        return -1;
    }

    // Cancel button
    int cancelBtn = buttonCreate(gCalledShotWindow,
        210,
        268,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        cancelButtonNormalFrmImage.getData(),
        cancelButtonPressedFrmImage.getData(),
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    for (int index = 0; index < 4; index++) {
        int probability;
        int btn;

        probability = _determine_to_hit(gDude, critter, _hit_loc_left[index], hitMode);
        _print_tohit(windowBuffer + CALLED_SHOT_WINDOW_WIDTH * (_call_ty[index] - 86) + 33, CALLED_SHOT_WINDOW_WIDTH, probability);

        btn = buttonCreate(gCalledShotWindow, 33, _call_ty[index] - 90, 128, 20, index, index, -1, index, NULL, NULL, NULL, 0);
        buttonSetMouseCallbacks(btn, _draw_loc_on_, _draw_loc_off, NULL, NULL);
        _draw_loc_(index, _colorTable[992]);

        probability = _determine_to_hit(gDude, critter, _hit_loc_right[index], hitMode);
        _print_tohit(windowBuffer + CALLED_SHOT_WINDOW_WIDTH * (_call_ty[index] - 86) + 453, CALLED_SHOT_WINDOW_WIDTH, probability);

        btn = buttonCreate(gCalledShotWindow, 341, _call_ty[index] - 90, 128, 20, index + 4, index + 4, -1, index + 4, NULL, NULL, NULL, 0);
        buttonSetMouseCallbacks(btn, _draw_loc_on_, _draw_loc_off, NULL, NULL);
        _draw_loc_(index + 4, _colorTable[992]);
    }

    windowRefresh(gCalledShotWindow);

    bool gameUiWasDisabled = gameUiIsDisabled();
    if (gameUiWasDisabled) {
        gameUiEnable();
    }

    _gmouse_disable(0);
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    int eventCode;
    while (true) {
        sharedFpsLimiter.mark();

        eventCode = inputGetInput();

        if (eventCode == KEY_ESCAPE) {
            break;
        }

        if (eventCode >= 0 && eventCode < HIT_LOCATION_COUNT) {
            break;
        }

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    _gmouse_enable();

    if (gameUiWasDisabled) {
        gameUiDisable(0);
    }

    fontSetCurrent(oldFont);

    windowDestroy(gCalledShotWindow);

    if (eventCode == KEY_ESCAPE) {
        return -1;
    }

    *hitLocation = eventCode < 4 ? _hit_loc_left[eventCode] : _hit_loc_right[eventCode - 4];

    soundPlayFile("icsxxxx1");

    return 0;
}

// check for possibility of performing attacking
// 0x426614
int _combat_check_bad_shot(Object* attacker, Object* defender, int hitMode, bool aiming)
{
    int range = 1;
    int tile = -1;
    if (defender != NULL) {
        tile = defender->tile;
        range = objectGetDistanceBetween(attacker, defender);
        if ((defender->data.critter.combat.results & DAM_DEAD) != 0) {
            return COMBAT_BAD_SHOT_ALREADY_DEAD;
        }
    }

    Object* weapon = critterGetWeaponForHitMode(attacker, hitMode);
    if (weapon != NULL) {
        if ((attacker->data.critter.combat.results & DAM_CRIP_ARM_LEFT) != 0
            && (attacker->data.critter.combat.results & DAM_CRIP_ARM_RIGHT) != 0) {
            return COMBAT_BAD_SHOT_BOTH_ARMS_CRIPPLED;
        }

        if ((attacker->data.critter.combat.results & DAM_CRIP_ARM_ANY) != 0) {
            if (weaponIsTwoHanded(weapon)) {
                return COMBAT_BAD_SHOT_ARM_CRIPPLED;
            }
        }
    }

    if (weaponGetActionPointCost(attacker, hitMode, aiming) > attacker->data.critter.combat.ap) {
        return COMBAT_BAD_SHOT_NOT_ENOUGH_AP;
    }

    if (weaponGetRange(attacker, hitMode) < range) {
        return COMBAT_BAD_SHOT_OUT_OF_RANGE;
    }

    int attackType = weaponGetAttackTypeForHitMode(weapon, hitMode);

    if (ammoGetCapacity(weapon) > 0) {
        if (ammoGetQuantity(weapon) == 0) {
            return COMBAT_BAD_SHOT_NO_AMMO;
        }
    }

    if (attackType == ATTACK_TYPE_RANGED
        || attackType == ATTACK_TYPE_THROW
        || weaponGetRange(attacker, hitMode) > 1) {
        if (_combat_is_shot_blocked(attacker, attacker->tile, tile, defender, NULL)) {
            return COMBAT_BAD_SHOT_AIM_BLOCKED;
        }
    }

    return COMBAT_BAD_SHOT_OK;
}

// 0x426744
bool _combat_to_hit(Object* target, int* accuracy)
{
    int hitMode;
    bool aiming;
    if (interfaceGetCurrentHitMode(&hitMode, &aiming) == -1) {
        return false;
    }

    if (_combat_check_bad_shot(gDude, target, hitMode, aiming) != COMBAT_BAD_SHOT_OK) {
        return false;
    }

    *accuracy = attackDetermineToHit(gDude, gDude->tile, target, HIT_LOCATION_UNCALLED, hitMode, true);

    return true;
}

// 0x4267CC
void _combat_attack_this(Object* a1)
{
    if (a1 == NULL) {
        return;
    }

    if ((gCombatState & 0x02) == 0) {
        return;
    }

    int hitMode;
    bool aiming;
    if (interfaceGetCurrentHitMode(&hitMode, &aiming) == -1) {
        return;
    }

    MessageListItem messageListItem;
    Object* item;
    char formattedText[80];
    const char* sfx;

    int rc = _combat_check_bad_shot(gDude, a1, hitMode, aiming);
    switch (rc) {
    case COMBAT_BAD_SHOT_NO_AMMO:
        item = critterGetWeaponForHitMode(gDude, hitMode);
        messageListItem.num = 101; // Out of ammo.
        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }

        sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_OUT_OF_AMMO, item, hitMode, NULL);
        soundPlayFile(sfx);
        return;
    case COMBAT_BAD_SHOT_OUT_OF_RANGE:
        messageListItem.num = 102; // Target out of range.
        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
        return;
    case COMBAT_BAD_SHOT_NOT_ENOUGH_AP:
        item = critterGetWeaponForHitMode(gDude, hitMode);
        messageListItem.num = 100; // You need %d action points.
        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
            int actionPointsRequired = weaponGetActionPointCost(gDude, hitMode, aiming);
            snprintf(formattedText, sizeof(formattedText), messageListItem.text, actionPointsRequired);
            displayMonitorAddMessage(formattedText);
        }
        return;
    case COMBAT_BAD_SHOT_ALREADY_DEAD:
        return;
    case COMBAT_BAD_SHOT_AIM_BLOCKED:
        messageListItem.num = 104; // Your aim is blocked.
        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
        return;
    case COMBAT_BAD_SHOT_ARM_CRIPPLED:
        messageListItem.num = 106; // You cannot use two-handed weapons with a crippled arm.
        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
        return;
    case COMBAT_BAD_SHOT_BOTH_ARMS_CRIPPLED:
        messageListItem.num = 105; // You cannot use weapons with both arms crippled.
        if (messageListGetItem(&gCombatMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
        return;
    }

    if (!isInCombat()) {
        STRUCT_664980 stru;
        stru.attacker = gDude;
        stru.defender = a1;
        stru.actionPointsBonus = 0;
        stru.accuracyBonus = 0;
        stru.damageBonus = 0;
        stru.minDamage = 0;
        stru.maxDamage = INT_MAX;
        stru.field_1C = 0;
        _combat(&stru);
        return;
    }

    if (!aiming) {
        _combat_attack(gDude, a1, hitMode, HIT_LOCATION_UNCALLED);
        return;
    }

    if (aiming != 1) {
        debugPrint("Bad called shot value %d\n", aiming);
    }

    int hitLocation;
    if (calledShotSelectHitLocation(a1, &hitLocation, hitMode) != -1) {
        _combat_attack(gDude, a1, hitMode, hitLocation);
    }
}

// Highlights critters.
//
// 0x426AA8
void _combat_outline_on()
{
    if (settings.preferences.target_highlight == TARGET_HIGHLIGHT_OFF) {
        return;
    }

    if (gameMouseGetMode() != GAME_MOUSE_MODE_CROSSHAIR) {
        return;
    }

    if (isInCombat()) {
        for (int index = 0; index < _list_total; index++) {
            _combat_update_critter_outline_for_los(_combat_list[index], 1);
        }
    } else {
        Object** critterList;
        int critterListLength = objectListCreate(-1, gElevation, OBJ_TYPE_CRITTER, &critterList);
        for (int index = 0; index < critterListLength; index++) {
            Object* critter = critterList[index];
            if (critter != gDude && (critter->data.critter.combat.results & DAM_DEAD) == 0) {
                _combat_update_critter_outline_for_los(critter, 1);
            }
        }

        if (critterListLength != 0) {
            objectListFree(critterList);
        }
    }

    // NOTE: Uninline.
    _combat_update_critters_in_los(true);

    tileWindowRefresh();
}

// 0x426BC0
void _combat_outline_off()
{
    int i;
    int v5;
    Object** v9;

    if (gCombatState & 1) {
        for (i = 0; i < _list_total; i++) {
            objectDisableOutline(_combat_list[i], NULL);
        }
    } else {
        v5 = objectListCreate(-1, gElevation, 1, &v9);
        for (i = 0; i < v5; i++) {
            objectDisableOutline(v9[i], NULL);
            objectClearOutline(v9[i], NULL);
        }
        if (v5) {
            objectListFree(v9);
        }
    }

    tileWindowRefresh();
}

// 0x426C64
void _combat_highlight_change()
{
    int targetHighlight = settings.preferences.target_highlight;
    if (targetHighlight != _combat_highlight && isInCombat()) {
        if (targetHighlight != 0) {
            if (_combat_highlight == 0) {
                _combat_outline_on();
            }
        } else {
            _combat_outline_off();
        }
    }

    _combat_highlight = targetHighlight;
}

// Checks if line of fire to the target object is blocked or not. Optionally calculate number of critters on the line of fire.
//
// 0x426CC4
bool _combat_is_shot_blocked(Object* sourceObj, int from, int to, Object* targetObj, int* numCrittersOnLof)
{
    if (numCrittersOnLof != NULL) {
        *numCrittersOnLof = 0;
    }

    Object* obstacle = sourceObj;
    int current = from;
    while (obstacle != NULL && current != to) {
        _make_straight_path_func(sourceObj, current, to, 0, &obstacle, 32, _obj_shoot_blocking_at);
        if (obstacle != NULL) {
            if (FID_TYPE(obstacle->fid) != OBJ_TYPE_CRITTER && obstacle != targetObj) {
                return true;
            }

            if (numCrittersOnLof != NULL && obstacle != targetObj && targetObj != NULL) {
                // SFALL: Fix for combat_is_shot_blocked_ engine
                // function not taking the flags of critters in the
                // line of fire into account when calculating the hit
                // chance penalty of ranged attacks in
                // determine_to_hit_func_ engine function.
                if ((obstacle->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_DOWN | DAM_KNOCKED_OUT)) == 0) {
                    *numCrittersOnLof += 1;

                    if ((obstacle->flags & OBJECT_MULTIHEX) != 0) {
                        *numCrittersOnLof += 1;
                    }
                }
            }

            if ((obstacle->flags & OBJECT_MULTIHEX) != 0) {
                // SFALL: Fix obtaining the next tile from a multihex object.
                // This bug does not cause any noticeable error in the function.
                current = obstacle->tile;
                if (current != to) {
                    int rotation = tileGetRotationTo(current, to);
                    current = tileGetTileInDirection(current, rotation, 1);
                }
            } else {
                current = obstacle->tile;
            }
        }
    }

    return false;
}

// 0x426D94
int _combat_player_knocked_out_by()
{
    if ((gDude->data.critter.combat.results & DAM_DEAD) != 0) {
        return -1;
    }

    if (_combat_ending_guy == NULL) {
        return -1;
    }

    return _combat_ending_guy->data.critter.combat.team;
}

// 0x426DB8
int _combat_explode_scenery(Object* a1, Object* a2)
{
    _scr_explode_scenery(a1, a1->tile, weaponGetRocketExplosionRadius(NULL), a1->elevation);
    return 0;
}

// 0x426DDC
void _combat_delete_critter(Object* obj)
{
    // TODO: Check entire function.
    if (!isInCombat()) {
        return;
    }

    if (_list_total == 0) {
        return;
    }

    int i;
    for (i = 0; i < _list_total; i++) {
        if (obj == _combat_list[i]) {
            break;
        }
    }

    if (i == _list_total) {
        return;
    }

    while (i < (_list_total - 1)) {
        _combat_list[i] = _combat_list[i + 1];
        aiInfoCopy(i + 1, i);
        i++;
    }

    _list_total--;

    _combat_list[_list_total] = obj;

    if (i >= _list_com) {
        if (i < (_list_noncom + _list_com)) {
            _list_noncom--;
        }
    } else {
        _list_com--;
    }

    obj->data.critter.combat.ap = 0;
    objectClearOutline(obj, NULL);

    obj->data.critter.combat.whoHitMe = NULL;
    _combatai_delete_critter(obj);
}

// 0x426EC4
void _combatKillCritterOutsideCombat(Object* critter_obj, char* msg)
{
    if (critter_obj != gDude) {
        displayMonitorAddMessage(msg);
        scriptExecProc(critter_obj->sid, SCRIPT_PROC_DESTROY);
        critterKill(critter_obj, -1, 1);
    }
}

int combatGetTargetHighlight()
{
    return _combat_highlight;
}

static void criticalsInit()
{
    int mode = 2;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_OVERRIDE_CRITICALS_MODE_KEY, &mode);
    if (mode < 0 || mode > 3) {
        mode = 0;
    }

    if (mode == 2 || mode == 3) {
        // Men
        criticalsSetValue(KILL_TYPE_MAN, HIT_LOCATION_UNCALLED, 2, CRIT_DATA_MEMBER_FLAGS, DAM_KNOCKED_DOWN | DAM_BYPASS);
        criticalsSetValue(KILL_TYPE_MAN, HIT_LOCATION_UNCALLED, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 5019);

        // Children
        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_RIGHT_LEG, 1, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, 0);
        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_RIGHT_LEG, 1, CRIT_DATA_MEMBER_MESSAGE_ID, 5216);
        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_RIGHT_LEG, 1, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 5000);

        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_RIGHT_LEG, 2, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, 0);
        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_RIGHT_LEG, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 5216);
        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_RIGHT_LEG, 2, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 5000);

        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_LEFT_LEG, 1, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, 0);
        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_LEFT_LEG, 1, CRIT_DATA_MEMBER_MESSAGE_ID, 5216);
        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_LEFT_LEG, 1, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 5000);

        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_LEFT_LEG, 2, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, 0);
        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_LEFT_LEG, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 5216);
        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_LEFT_LEG, 2, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 5000);

        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_UNCALLED, 1, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 4);
        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_UNCALLED, 2, CRIT_DATA_MEMBER_FLAGS, DAM_KNOCKED_DOWN | DAM_BYPASS);
        criticalsSetValue(KILL_TYPE_CHILD, HIT_LOCATION_UNCALLED, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 5212);

        // Super Mutants
        criticalsSetValue(KILL_TYPE_SUPER_MUTANT, HIT_LOCATION_LEFT_LEG, 1, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 5306);

        // Ghouls
        criticalsSetValue(KILL_TYPE_GHOUL, HIT_LOCATION_HEAD, 4, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT, -1);

        // Brahmin
        criticalsSetValue(KILL_TYPE_BRAHMIN, HIT_LOCATION_HEAD, 4, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT, -1);

        // Radscorpions
        criticalsSetValue(KILL_TYPE_RADSCORPION, HIT_LOCATION_RIGHT_LEG, 1, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, DAM_KNOCKED_DOWN);

        criticalsSetValue(KILL_TYPE_RADSCORPION, HIT_LOCATION_LEFT_LEG, 1, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, DAM_KNOCKED_DOWN);
        criticalsSetValue(KILL_TYPE_RADSCORPION, HIT_LOCATION_LEFT_LEG, 2, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 5608);

        // Centaurs
        criticalsSetValue(KILL_TYPE_CENTAUR, HIT_LOCATION_TORSO, 3, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, DAM_KNOCKED_DOWN);

        criticalsSetValue(KILL_TYPE_CENTAUR, HIT_LOCATION_UNCALLED, 3, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, DAM_KNOCKED_DOWN);

        // Deathclaws
        criticalsSetValue(KILL_TYPE_DEATH_CLAW, HIT_LOCATION_LEFT_LEG, 1, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, DAM_CRIP_LEG_LEFT);
        criticalsSetValue(KILL_TYPE_DEATH_CLAW, HIT_LOCATION_LEFT_LEG, 2, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, DAM_CRIP_LEG_LEFT);
        criticalsSetValue(KILL_TYPE_DEATH_CLAW, HIT_LOCATION_LEFT_LEG, 3, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, DAM_CRIP_LEG_LEFT);
        criticalsSetValue(KILL_TYPE_DEATH_CLAW, HIT_LOCATION_LEFT_LEG, 4, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, DAM_CRIP_LEG_LEFT);
        criticalsSetValue(KILL_TYPE_DEATH_CLAW, HIT_LOCATION_LEFT_LEG, 5, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS, DAM_CRIP_LEG_LEFT);

        // Geckos
        criticalsSetValue(KILL_TYPE_GECKO, HIT_LOCATION_UNCALLED, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 6701);
        criticalsSetValue(KILL_TYPE_GECKO, HIT_LOCATION_UNCALLED, 1, CRIT_DATA_MEMBER_MESSAGE_ID, 6701);
        criticalsSetValue(KILL_TYPE_GECKO, HIT_LOCATION_UNCALLED, 2, CRIT_DATA_MEMBER_FLAGS, DAM_KNOCKED_DOWN | DAM_BYPASS);
        criticalsSetValue(KILL_TYPE_GECKO, HIT_LOCATION_UNCALLED, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 6704);
        criticalsSetValue(KILL_TYPE_GECKO, HIT_LOCATION_UNCALLED, 3, CRIT_DATA_MEMBER_MESSAGE_ID, 6704);
        criticalsSetValue(KILL_TYPE_GECKO, HIT_LOCATION_UNCALLED, 4, CRIT_DATA_MEMBER_MESSAGE_ID, 6704);
        criticalsSetValue(KILL_TYPE_GECKO, HIT_LOCATION_UNCALLED, 5, CRIT_DATA_MEMBER_MESSAGE_ID, 6704);

        // Aliens
        criticalsSetValue(16, HIT_LOCATION_UNCALLED, 2, CRIT_DATA_MEMBER_FLAGS, DAM_KNOCKED_DOWN | DAM_BYPASS);

        // Giant Ants
        criticalsSetValue(17, HIT_LOCATION_UNCALLED, 2, CRIT_DATA_MEMBER_FLAGS, DAM_KNOCKED_DOWN | DAM_BYPASS);

        // Big Bad Boss
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 5001);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_HEAD, 1, CRIT_DATA_MEMBER_MESSAGE_ID, 5001);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_HEAD, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 5001);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_HEAD, 3, CRIT_DATA_MEMBER_MESSAGE_ID, 7105);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_HEAD, 4, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_HEAD, 4, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 7104);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_HEAD, 5, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);

        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_ARM, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 5008);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_ARM, 1, CRIT_DATA_MEMBER_MESSAGE_ID, 5008);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_ARM, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 5009);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_ARM, 3, CRIT_DATA_MEMBER_MESSAGE_ID, 5009);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_ARM, 4, CRIT_DATA_MEMBER_MESSAGE_ID, 7102);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_ARM, 5, CRIT_DATA_MEMBER_MESSAGE_ID, 7102);

        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_ARM, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 5008);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_ARM, 1, CRIT_DATA_MEMBER_MESSAGE_ID, 5008);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_ARM, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 5009);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_ARM, 3, CRIT_DATA_MEMBER_MESSAGE_ID, 5009);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_ARM, 4, CRIT_DATA_MEMBER_MESSAGE_ID, 7102);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_ARM, 5, CRIT_DATA_MEMBER_MESSAGE_ID, 7102);

        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_TORSO, 4, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_TORSO, 5, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);

        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_LEG, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 5023);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_LEG, 1, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_LEG, 1, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 7103);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_LEG, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_LEG, 2, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 7103);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_LEG, 3, CRIT_DATA_MEMBER_MESSAGE_ID, 7103);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_LEG, 4, CRIT_DATA_MEMBER_MESSAGE_ID, 7103);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_RIGHT_LEG, 5, CRIT_DATA_MEMBER_MESSAGE_ID, 7103);

        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_LEG, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 5023);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_LEG, 1, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_LEG, 1, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 7103);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_LEG, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_LEG, 2, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 7103);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_LEG, 3, CRIT_DATA_MEMBER_MESSAGE_ID, 7103);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_LEG, 4, CRIT_DATA_MEMBER_MESSAGE_ID, 7103);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_LEFT_LEG, 5, CRIT_DATA_MEMBER_MESSAGE_ID, 7103);

        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_EYES, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 5027);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_EYES, 1, CRIT_DATA_MEMBER_MESSAGE_ID, 5027);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_EYES, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 5027);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_EYES, 3, CRIT_DATA_MEMBER_MESSAGE_ID, 5027);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_EYES, 4, CRIT_DATA_MEMBER_MESSAGE_ID, 7104);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_EYES, 5, CRIT_DATA_MEMBER_MESSAGE_ID, 7104);

        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_GROIN, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 5033);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_GROIN, 1, CRIT_DATA_MEMBER_MESSAGE_ID, 5027);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_GROIN, 1, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID, 7101);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_GROIN, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_GROIN, 3, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_GROIN, 4, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_GROIN, 5, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);

        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_UNCALLED, 2, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 3);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_UNCALLED, 4, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 4);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_UNCALLED, 4, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);
        criticalsSetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_UNCALLED, 5, CRIT_DATA_MEMBER_MESSAGE_ID, 7101);
    }

    if (mode == 1 || mode == 3) {
        Config criticalsConfig;
        if (configInit(&criticalsConfig)) {
            char* criticalsConfigFilePath;
            configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_OVERRIDE_CRITICALS_FILE_KEY, &criticalsConfigFilePath);
            if (criticalsConfigFilePath != NULL && *criticalsConfigFilePath == '\0') {
                criticalsConfigFilePath = NULL;
            }

            if (configRead(&criticalsConfig, criticalsConfigFilePath, false)) {
                if (mode == 1) {
                    char sectionKey[16];

                    // Read original kill types (19) plus one for the player.
                    for (int killType = 0; killType < KILL_TYPE_COUNT + 1; killType++) {
                        for (int hitLocation = 0; hitLocation < HIT_LOCATION_COUNT; hitLocation++) {
                            for (int effect = 0; effect < CRTICIAL_EFFECT_COUNT; effect++) {
                                snprintf(sectionKey, sizeof(sectionKey), "c_%02d_%d_%d", killType, hitLocation, effect);

                                // Update player kill type if needed.
                                int newKillType = killType == KILL_TYPE_COUNT ? SFALL_KILL_TYPE_COUNT : killType;
                                for (int dataMember = 0; dataMember < CRIT_DATA_MEMBER_COUNT; dataMember++) {
                                    int value = criticalsGetValue(newKillType, hitLocation, effect, dataMember);
                                    if (configGetInt(&criticalsConfig, sectionKey, gCritDataMemberKeys[dataMember], &value)) {
                                        criticalsSetValue(newKillType, hitLocation, effect, dataMember, value);
                                    }
                                }
                            }
                        }
                    }
                } else if (mode == 3) {
                    char ktSectionKey[32];
                    char hitLocationSectionKey[32];
                    char key[32];

                    // Read Sfall kill types (38) plus one for the player.
                    for (int killType = 0; killType < SFALL_KILL_TYPE_COUNT + 1; killType++) {
                        snprintf(ktSectionKey, sizeof(ktSectionKey), "c_%02d", killType);

                        int enabled = 0;
                        configGetInt(&criticalsConfig, ktSectionKey, "Enabled", &enabled);
                        if (enabled == 0) {
                            continue;
                        }

                        for (int hitLocation = 0; hitLocation < HIT_LOCATION_COUNT; hitLocation++) {
                            if (enabled < 2) {
                                bool hitLocationChanged = false;

                                snprintf(key, sizeof(key), "Part_%d", hitLocation);
                                configGetBool(&criticalsConfig, ktSectionKey, key, &hitLocationChanged);

                                if (!hitLocationChanged) {
                                    continue;
                                }
                            }

                            snprintf(hitLocationSectionKey, sizeof(hitLocationSectionKey), "c_%02d_%d", killType, hitLocation);

                            for (int effect = 0; effect < CRTICIAL_EFFECT_COUNT; effect++) {
                                for (int dataMember = 0; dataMember < CRIT_DATA_MEMBER_COUNT; dataMember++) {
                                    int value = criticalsGetValue(killType, hitLocation, effect, dataMember);
                                    snprintf(key, sizeof(key), "e%d_%s", effect, gCritDataMemberKeys[dataMember]);
                                    if (configGetInt(&criticalsConfig, hitLocationSectionKey, key, &value)) {
                                        criticalsSetValue(killType, hitLocation, effect, dataMember, value);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            configFree(&criticalsConfig);
        }
    }

    memcpy(gBaseCriticalHitTables, gCriticalHitTables, sizeof(gCriticalHitTables));
    memcpy(gBasePlayerCriticalHitTable, gPlayerCriticalHitTable, sizeof(gPlayerCriticalHitTable));
}

static void criticalsReset()
{
    memcpy(gCriticalHitTables, gBaseCriticalHitTables, sizeof(gBaseCriticalHitTables));
    memcpy(gPlayerCriticalHitTable, gBasePlayerCriticalHitTable, sizeof(gBasePlayerCriticalHitTable));
}

static void criticalsExit()
{
    criticalsReset();
}

int criticalsGetValue(int killType, int hitLocation, int effect, int dataMember)
{
    if (killType == SFALL_KILL_TYPE_COUNT) {
        return gPlayerCriticalHitTable[hitLocation][effect].values[dataMember];
    } else {
        return gCriticalHitTables[killType][hitLocation][effect].values[dataMember];
    }
}

void criticalsSetValue(int killType, int hitLocation, int effect, int dataMember, int value)
{
    if (killType == SFALL_KILL_TYPE_COUNT) {
        gPlayerCriticalHitTable[hitLocation][effect].values[dataMember] = value;
    } else {
        gCriticalHitTables[killType][hitLocation][effect].values[dataMember] = value;
    }
}

void criticalsResetValue(int killType, int hitLocation, int effect, int dataMember)
{
    if (killType == SFALL_KILL_TYPE_COUNT) {
        gPlayerCriticalHitTable[hitLocation][effect].values[dataMember] = gBasePlayerCriticalHitTable[hitLocation][effect].values[dataMember];
    } else {
        gCriticalHitTables[killType][hitLocation][effect].values[dataMember] = gBaseCriticalHitTables[killType][hitLocation][effect].values[dataMember];
    }
}

static void burstModInit()
{
    configGetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BURST_MOD_ENABLED_KEY, &gBurstModEnabled);

    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BURST_MOD_CENTER_MULTIPLIER_KEY, &gBurstModCenterMultiplier);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BURST_MOD_CENTER_DIVISOR_KEY, &gBurstModCenterDivisor);
    if (gBurstModCenterDivisor < 1) {
        gBurstModCenterDivisor = 1;
    }
    if (gBurstModCenterMultiplier > gBurstModCenterDivisor) {
        gBurstModCenterMultiplier = gBurstModCenterDivisor;
    }

    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BURST_MOD_TARGET_MULTIPLIER_KEY, &gBurstModTargetMultiplier);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BURST_MOD_TARGET_DIVISOR_KEY, &gBurstModTargetDivisor);
    if (gBurstModTargetDivisor < 1) {
        gBurstModTargetDivisor = 1;
    }
    if (gBurstModTargetMultiplier > gBurstModTargetDivisor) {
        gBurstModTargetMultiplier = gBurstModTargetDivisor;
    }
}

static int burstModComputeRounds(int totalRounds, int* centerRoundsPtr, int* leftRoundsPtr, int* rightRoundsPtr)
{
    int totalRoundsMultiplied = totalRounds * gBurstModCenterMultiplier;
    int centerRounds = totalRoundsMultiplied / gBurstModCenterDivisor;
    if ((totalRoundsMultiplied % gBurstModCenterDivisor) != 0) {
        centerRounds++;
    }

    if (centerRounds == 0) {
        centerRounds++;
    }
    *centerRoundsPtr = centerRounds;

    int leftRounds = (totalRounds - centerRounds) / 2;
    *leftRoundsPtr = leftRounds;
    *rightRoundsPtr = totalRounds - centerRounds - leftRounds;

    int centerRoundsMultiplied = centerRounds * gBurstModTargetMultiplier;
    int mainTargetRounds = centerRoundsMultiplied / gBurstModTargetDivisor;
    if ((centerRoundsMultiplied % gBurstModTargetDivisor) != 0) {
        mainTargetRounds++;
    }

    return mainTargetRounds;
}

static void unarmedInit()
{
    unarmedInitVanilla();
    unarmedInitCustom();
}

static void unarmedInitVanilla()
{
    UnarmedHitDescription* hitDescription;

    // Punch
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_PUNCH]);
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->actionPointCost = 3;

    // Strong Punch
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_STRONG_PUNCH]);
    hitDescription->requiredSkill = 55;
    hitDescription->requiredStats[STAT_AGILITY] = 6;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 3;
    hitDescription->actionPointCost = 3;
    hitDescription->isPenetrate = false;
    hitDescription->isSecondary = false;

    // Hammer Punch
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_HAMMER_PUNCH]);
    hitDescription->requiredLevel = 6;
    hitDescription->requiredSkill = 75;
    hitDescription->requiredStats[STAT_STRENGTH] = 5;
    hitDescription->requiredStats[STAT_AGILITY] = 6;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 5;
    hitDescription->bonusCriticalChance = 5;
    hitDescription->actionPointCost = 3;

    // Lightning Punch
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_HAYMAKER]);
    hitDescription->requiredLevel = 9;
    hitDescription->requiredSkill = 100;
    hitDescription->requiredStats[STAT_STRENGTH] = 5;
    hitDescription->requiredStats[STAT_AGILITY] = 7;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 7;
    hitDescription->bonusCriticalChance = 15;
    hitDescription->actionPointCost = 3;

    // Chop Punch
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_JAB]);
    hitDescription->requiredLevel = 5;
    hitDescription->requiredSkill = 75;
    hitDescription->requiredStats[STAT_STRENGTH] = 5;
    hitDescription->requiredStats[STAT_AGILITY] = 7;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 3;
    hitDescription->bonusCriticalChance = 10;
    hitDescription->actionPointCost = 3;
    hitDescription->isSecondary = true;

    // Dragon Punch
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_PALM_STRIKE]);
    hitDescription->requiredLevel = 12;
    hitDescription->requiredSkill = 115;
    hitDescription->requiredStats[STAT_STRENGTH] = 5;
    hitDescription->requiredStats[STAT_AGILITY] = 7;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 7;
    hitDescription->bonusCriticalChance = 20;
    hitDescription->actionPointCost = 6;
    hitDescription->isPenetrate = true;
    hitDescription->isSecondary = true;

    // Force Punch
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_PIERCING_STRIKE]);
    hitDescription->requiredLevel = 16;
    hitDescription->requiredSkill = 130;
    hitDescription->requiredStats[STAT_STRENGTH] = 5;
    hitDescription->requiredStats[STAT_AGILITY] = 7;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 10;
    hitDescription->bonusCriticalChance = 40;
    hitDescription->actionPointCost = 8;
    hitDescription->isPenetrate = true;
    hitDescription->isSecondary = true;

    // Kick
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_KICK]);
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->actionPointCost = 3;

    // Strong Kick
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_STRONG_KICK]);
    hitDescription->requiredSkill = 40;
    hitDescription->requiredStats[STAT_AGILITY] = 6;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 5;
    hitDescription->actionPointCost = 4;

    // Snap Kick
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_SNAP_KICK]);
    hitDescription->requiredLevel = 6;
    hitDescription->requiredSkill = 60;
    hitDescription->requiredStats[STAT_AGILITY] = 6;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 7;
    hitDescription->actionPointCost = 4;

    // Roundhouse Kick
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_POWER_KICK]);
    hitDescription->requiredLevel = 9;
    hitDescription->requiredSkill = 80;
    hitDescription->requiredStats[STAT_STRENGTH] = 6;
    hitDescription->requiredStats[STAT_AGILITY] = 6;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 9;
    hitDescription->bonusCriticalChance = 5;
    hitDescription->actionPointCost = 4;

    // Kip Kick
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_HIP_KICK]);
    hitDescription->requiredLevel = 6;
    hitDescription->requiredSkill = 60;
    hitDescription->requiredStats[STAT_STRENGTH] = 6;
    hitDescription->requiredStats[STAT_AGILITY] = 7;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 7;
    hitDescription->actionPointCost = 7;
    hitDescription->isSecondary = true;

    // Jump Kick
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_HOOK_KICK]);
    hitDescription->requiredLevel = 12;
    hitDescription->requiredSkill = 100;
    hitDescription->requiredStats[STAT_STRENGTH] = 6;
    hitDescription->requiredStats[STAT_AGILITY] = 7;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 9;
    hitDescription->bonusCriticalChance = 10;
    hitDescription->actionPointCost = 7;
    hitDescription->isPenetrate = true;
    hitDescription->isSecondary = true;

    // Death Blossom Kick
    hitDescription = &(gUnarmedHitDescriptions[HIT_MODE_PIERCING_KICK]);
    hitDescription->requiredLevel = 15;
    hitDescription->requiredSkill = 125;
    hitDescription->requiredStats[STAT_STRENGTH] = 6;
    hitDescription->requiredStats[STAT_AGILITY] = 8;
    hitDescription->minDamage = 1;
    hitDescription->maxDamage = 2;
    hitDescription->bonusDamage = 12;
    hitDescription->bonusCriticalChance = 50;
    hitDescription->actionPointCost = 9;
    hitDescription->isPenetrate = true;
    hitDescription->isSecondary = true;
}

static void unarmedInitCustom()
{
    char* unarmedFileName = NULL;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_UNARMED_FILE_KEY, &unarmedFileName);
    if (unarmedFileName != NULL && *unarmedFileName == '\0') {
        unarmedFileName = NULL;
    }

    if (unarmedFileName == NULL) {
        return;
    }

    Config unarmedConfig;
    if (configInit(&unarmedConfig)) {
        if (configRead(&unarmedConfig, unarmedFileName, false)) {
            char section[4];
            char statKey[6];

            for (int hitMode = 0; hitMode < HIT_MODE_COUNT; hitMode++) {
                if (!isUnarmedHitMode(hitMode)) {
                    continue;
                }

                UnarmedHitDescription* hitDescription = &(gUnarmedHitDescriptions[hitMode]);
                snprintf(section, sizeof(section), "%d", hitMode);

                configGetInt(&unarmedConfig, section, "ReqLevel", &(hitDescription->requiredLevel));
                configGetInt(&unarmedConfig, section, "SkillLevel", &(hitDescription->requiredSkill));
                configGetInt(&unarmedConfig, section, "MinDamage", &(hitDescription->minDamage));
                configGetInt(&unarmedConfig, section, "MaxDamage", &(hitDescription->maxDamage));
                configGetInt(&unarmedConfig, section, "BonusDamage", &(hitDescription->bonusDamage));
                configGetInt(&unarmedConfig, section, "BonusCrit", &(hitDescription->bonusCriticalChance));
                configGetInt(&unarmedConfig, section, "APCost", &(hitDescription->actionPointCost));
                configGetBool(&unarmedConfig, section, "BonusDamage", &(hitDescription->isPenetrate));
                configGetBool(&unarmedConfig, section, "Secondary", &(hitDescription->isSecondary));

                for (int stat = 0; stat < PRIMARY_STAT_COUNT; stat++) {
                    snprintf(statKey, sizeof(statKey), "Stat%d", stat);
                    configGetInt(&unarmedConfig, section, statKey, &(hitDescription->requiredStats[stat]));
                }
            }
        }

        configFree(&unarmedConfig);
    }
}

int unarmedGetDamage(int hitMode, int* minDamagePtr, int* maxDamagePtr)
{
    UnarmedHitDescription* hitDescription = &(gUnarmedHitDescriptions[hitMode]);
    *minDamagePtr = hitDescription->minDamage;
    *maxDamagePtr = hitDescription->maxDamage;
    return hitDescription->bonusDamage;
}

int unarmedGetBonusCriticalChance(int hitMode)
{
    UnarmedHitDescription* hitDescription = &(gUnarmedHitDescriptions[hitMode]);
    return hitDescription->bonusCriticalChance;
}

int unarmedGetActionPointCost(int hitMode)
{
    UnarmedHitDescription* hitDescription = &(gUnarmedHitDescriptions[hitMode]);
    return hitDescription->actionPointCost;
}

bool unarmedIsPenetrating(int hitMode)
{
    UnarmedHitDescription* hitDescription = &(gUnarmedHitDescriptions[hitMode]);
    return hitDescription->isPenetrate;
}

int unarmedGetPunchHitMode(bool isSecondary)
{
    int hitMode = unarmedGetHitModeInRange(FIRST_ADVANCED_PUNCH_HIT_MODE, LAST_ADVANCED_PUNCH_HIT_MODE, isSecondary);
    if (hitMode == -1) {
        hitMode = HIT_MODE_PUNCH;
    }
    return hitMode;
}

int unarmedGetKickHitMode(bool isSecondary)
{
    int hitMode = unarmedGetHitModeInRange(FIRST_ADVANCED_KICK_HIT_MODE, LAST_ADVANCED_KICK_HIT_MODE, isSecondary);
    if (hitMode == -1) {
        hitMode = HIT_MODE_KICK;
    }
    return hitMode;
}

static int unarmedGetHitModeInRange(int firstHitMode, int lastHitMode, bool isSecondary)
{
    int hitMode = -1;

    int unarmed = skillGetValue(gDude, SKILL_UNARMED);
    int level = pcGetStat(PC_STAT_LEVEL);
    int stats[PRIMARY_STAT_COUNT];
    for (int stat = 0; stat < PRIMARY_STAT_COUNT; stat++) {
        stats[stat] = critterGetStat(gDude, stat);
    }

    for (int candidateHitMode = firstHitMode; candidateHitMode <= lastHitMode; candidateHitMode++) {
        UnarmedHitDescription* hitDescription = &(gUnarmedHitDescriptions[candidateHitMode]);
        if (isSecondary != hitDescription->isSecondary) {
            continue;
        }

        if (unarmed < hitDescription->requiredSkill) {
            continue;
        }

        if (level < hitDescription->requiredLevel) {
            continue;
        }

        bool missingStats = false;
        for (int stat = 0; stat < PRIMARY_STAT_COUNT; stat++) {
            if (stats[stat] < hitDescription->requiredStats[stat]) {
                missingStats = true;
                break;
            }
        }
        if (missingStats) {
            continue;
        }

        hitMode = candidateHitMode;
    }

    return hitMode;
}

static void damageModInit()
{
    gDamageCalculationType = DAMAGE_CALCULATION_TYPE_VANILLA;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DAMAGE_MOD_FORMULA_KEY, &gDamageCalculationType);

    gBonusHthDamageFix = true;
    configGetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BONUS_HTH_DAMAGE_FIX_KEY, &gBonusHthDamageFix);

    gDisplayBonusDamage = false;
    configGetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DISPLAY_BONUS_DAMAGE_KEY, &gDisplayBonusDamage);
}

bool damageModGetBonusHthDamageFix()
{
    return gBonusHthDamageFix;
}

bool damageModGetDisplayBonusDamage()
{
    return gDisplayBonusDamage;
}

static void damageModCalculateGlovz(DamageCalculationContext* context)
{
    int ammoX = weaponGetAmmoDamageMultiplier(context->attack->weapon);
    if (ammoX <= 0) {
        ammoX = 1;
    }

    int ammoY = weaponGetAmmoDamageDivisor(context->attack->weapon);
    if (ammoY <= 0) {
        ammoY = 1;
    }

    int ammoDamageResistance = weaponGetAmmoDamageResistanceModifier(context->attack->weapon);
    if (ammoDamageResistance > 0) {
        ammoDamageResistance = -ammoDamageResistance;
    }

    int calculatedDamageThreshold = context->damageThreshold;
    if (calculatedDamageThreshold > 0) {
        calculatedDamageThreshold = damageModGlovzDivRound(calculatedDamageThreshold, ammoY);
    }

    int calculatedDamageResistance = context->damageResistance;
    if (calculatedDamageResistance > 0) {
        if (context->combatDifficultyDamageModifier > 100) {
            calculatedDamageResistance -= 20;
        } else if (context->combatDifficultyDamageModifier < 100) {
            calculatedDamageResistance += 20;
        }

        calculatedDamageResistance += ammoDamageResistance;

        calculatedDamageResistance = damageModGlovzDivRound(calculatedDamageResistance, ammoX);

        if (calculatedDamageResistance >= 100) {
            return;
        }
    }

    for (int index = 0; index < context->ammoQuantity; index++) {
        int damage = weaponGetDamage(context->attack->attacker, context->attack->hitMode);

        damage += context->damageBonus;
        if (damage <= 0) {
            continue;
        }

        if (context->damageThreshold > 0) {
            damage -= calculatedDamageThreshold;
            if (damage <= 0) {
                continue;
            }
        }

        if (context->damageResistance > 0) {
            damage -= damageModGlovzDivRound(damage * calculatedDamageResistance, 100);
            if (damage <= 0) {
                continue;
            }
        }

        if (context->damageThreshold <= 0 && context->damageResistance <= 0) {
            if (ammoX > 1 && ammoY > 1) {
                damage += damageModGlovzDivRound(damage * 15, 100);
            } else if (ammoX > 1) {
                damage += damageModGlovzDivRound(damage * 20, 100);
            } else if (ammoY > 1) {
                damage += damageModGlovzDivRound(damage * 10, 100);
            }
        }

        if (gDamageCalculationType == DAMAGE_CALCULATION_TYPE_GLOVZ_WITH_DAMAGE_MULTIPLIER_TWEAK) {
            damage += damageModGlovzDivRound(damage * context->bonusDamageMultiplier * 25, 100);
        } else {
            damage += damage * context->bonusDamageMultiplier / 2;
        }

        if (damage > 0) {
            *context->damagePtr += damage;
        }
    }
}

static int damageModGlovzDivRound(int dividend, int divisor)
{
    if (dividend < divisor) {
        return dividend != divisor && dividend * 2 <= divisor ? 0 : 1;
    }

    int quotient = dividend / divisor;
    dividend %= divisor;

    if (dividend == 0) {
        return quotient;
    }

    dividend *= 2;

    if (dividend > divisor || (dividend == divisor && (quotient & 1) != 0)) {
        quotient += 1;
    }

    return quotient;
}

static void damageModCalculateYaam(DamageCalculationContext* context)
{
    int damageMultiplier = context->bonusDamageMultiplier * weaponGetAmmoDamageMultiplier(context->attack->weapon);
    int damageDivisor = weaponGetAmmoDamageDivisor(context->attack->weapon);

    int ammoDamageResistance = weaponGetAmmoDamageResistanceModifier(context->attack->weapon);

    int calculatedDamageThreshold = context->damageThreshold - ammoDamageResistance;
    int damageResistance = calculatedDamageThreshold;

    if (calculatedDamageThreshold >= 0) {
        damageResistance = 0;
    } else {
        calculatedDamageThreshold = 0;
        damageResistance *= 10;
    }

    int calculatedDamageResistance = context->damageResistance + damageResistance;
    if (calculatedDamageResistance < 0) {
        calculatedDamageResistance = 0;
    } else if (calculatedDamageResistance >= 100) {
        return;
    }

    for (int index = 0; index < context->ammoQuantity; index++) {
        int damage = weaponGetDamage(context->attack->weapon, context->attack->hitMode);
        damage += context->damageBonus;

        damage -= calculatedDamageThreshold;
        if (damage <= 0) {
            continue;
        }

        damage *= damageMultiplier;
        if (damageDivisor != 0) {
            damage /= damageDivisor;
        }

        damage /= 2;
        damage *= context->combatDifficultyDamageModifier;
        damage /= 100;

        damage -= damage * damageResistance / 100;

        if (damage > 0) {
            context->damagePtr += damage;
        }
    }
}

int combat_get_hit_location_penalty(int hit_location)
{
    if (hit_location >= 0 && hit_location < HIT_LOCATION_COUNT) {
        return hit_location_penalty[hit_location];
    } else {
        return 0;
    }
}

void combat_set_hit_location_penalty(int hit_location, int penalty)
{
    if (hit_location >= 0 && hit_location < HIT_LOCATION_COUNT) {
        hit_location_penalty[hit_location] = penalty;
    }
}

void combat_reset_hit_location_penalty()
{
    for (int hit_location = 0; hit_location < HIT_LOCATION_COUNT; hit_location++) {
        hit_location_penalty[hit_location] = hit_location_penalty_default[hit_location];
    }
}

Attack* combat_get_data()
{
    return &_main_ctd;
}

} // namespace fallout
