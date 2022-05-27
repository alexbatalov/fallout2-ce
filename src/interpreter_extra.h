#ifndef INTERPRETER_EXTRA_H
#define INTERPRETER_EXTRA_H

#include "game_movie.h"
#include "geometry.h"
#include "interpreter.h"
#include "obj_types.h"

typedef enum ScriptError {
    SCRIPT_ERROR_NOT_IMPLEMENTED,
    SCRIPT_ERROR_OBJECT_IS_NULL,
    SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID,
    SCRIPT_ERROR_FOLLOWS,
    SCRIPT_ERROR_COUNT,
} ScriptError;

typedef enum Metarule {
    METARULE_SIGNAL_END_GAME = 13,
    METARULE_FIRST_RUN = 14,
    METARULE_ELEVATOR = 15,
    METARULE_PARTY_COUNT = 16,
    METARULE_AREA_KNOWN = 17,
    METARULE_WHO_ON_DRUGS = 18,
    METARULE_MAP_KNOWN = 19,
    METARULE_IS_LOADGAME = 22,
    METARULE_CAR_CURRENT_TOWN = 30,
    METARULE_GIVE_CAR_TO_PARTY = 31,
    METARULE_GIVE_CAR_GAS = 32,
    METARULE_SKILL_CHECK_TAG = 40,
    METARULE_DROP_ALL_INVEN = 42,
    METARULE_INVEN_UNWIELD_WHO = 43,
    METARULE_GET_WORLDMAP_XPOS = 44,
    METARULE_GET_WORLDMAP_YPOS = 45,
    METARULE_CURRENT_TOWN = 46,
    METARULE_LANGUAGE_FILTER = 47,
    METARULE_VIOLENCE_FILTER = 48,
    METARULE_WEAPON_DAMAGE_TYPE = 49,
    METARULE_CRITTER_BARTERS = 50,
    METARULE_CRITTER_KILL_TYPE = 51,
    METARULE_SET_CAR_CARRY_AMOUNT = 52,
    METARULE_GET_CAR_CARRY_AMOUNT = 53,
} Metarule;

typedef enum Metarule3 {
    METARULE3_CLR_FIXED_TIMED_EVENTS = 100,
    METARULE3_MARK_SUBTILE = 101,
    METARULE3_SET_WM_MUSIC = 102,
    METARULE3_GET_KILL_COUNT = 103,
    METARULE3_MARK_MAP_ENTRANCE = 104,
    METARULE3_WM_SUBTILE_STATE = 105,
    METARULE3_TILE_GET_NEXT_CRITTER = 106,
    METARULE3_ART_SET_BASE_FID_NUM = 107,
    METARULE3_TILE_SET_CENTER = 108,
    // chem use preference
    METARULE3_109 = 109,
    // probably true if car is out of fuel
    METARULE3_110 = 110,
    // probably returns city index
    METARULE3_111 = 111,
} Metarule3;

typedef enum CritterTrait {
    CRITTER_TRAIT_PERK = 0,
    CRITTER_TRAIT_OBJECT = 1,
    CRITTER_TRAIT_TRAIT = 2,
} CritterTrait;

typedef enum CritterTraitObject {
    CRITTER_TRAIT_OBJECT_AI_PACKET = 5,
    CRITTER_TRAIT_OBJECT_TEAM = 6,
    CRITTER_TRAIT_OBJECT_ROTATION = 10,
    CRITTER_TRAIT_OBJECT_IS_INVISIBLE = 666,
    CRITTER_TRAIT_OBJECT_GET_INVENTORY_WEIGHT = 669,
} CritterTraitObject;

// See [opGetCritterState].
typedef enum CritterState {
    CRITTER_STATE_NORMAL = 0x00,
    CRITTER_STATE_DEAD = 0x01,
    CRITTER_STATE_PRONE = 0x02,
} CritterState;

enum {
    INVEN_TYPE_WORN = 0,
    INVEN_TYPE_RIGHT_HAND = 1,
    INVEN_TYPE_LEFT_HAND = 2,
    INVEN_TYPE_INV_COUNT = -2,
};

typedef enum FloatingMessageType {
    FLOATING_MESSAGE_TYPE_WARNING = -2,
    FLOATING_MESSAGE_TYPE_COLOR_SEQUENCE = -1,
    FLOATING_MESSAGE_TYPE_NORMAL = 0,
    FLOATING_MESSAGE_TYPE_BLACK,
    FLOATING_MESSAGE_TYPE_RED,
    FLOATING_MESSAGE_TYPE_GREEN,
    FLOATING_MESSAGE_TYPE_BLUE,
    FLOATING_MESSAGE_TYPE_PURPLE,
    FLOATING_MESSAGE_TYPE_NEAR_WHITE,
    FLOATING_MESSAGE_TYPE_LIGHT_RED,
    FLOATING_MESSAGE_TYPE_YELLOW,
    FLOATING_MESSAGE_TYPE_WHITE,
    FLOATING_MESSAGE_TYPE_GREY,
    FLOATING_MESSAGE_TYPE_DARK_GREY,
    FLOATING_MESSAGE_TYPE_LIGHT_GREY,
    FLOATING_MESSAGE_TYPE_COUNT,
} FloatingMessageType;

typedef enum OpRegAnimFunc {
    OP_REG_ANIM_FUNC_BEGIN = 1,
    OP_REG_ANIM_FUNC_CLEAR = 2,
    OP_REG_ANIM_FUNC_END = 3,
} OpRegAnimFunc;

extern char _Error_0[];
extern char _aCritter[];

extern const int dword_453F90[3];
extern const unsigned short word_453F9C[MOVIE_COUNT];
extern Rect stru_453FC0;

extern const char* _dbg_error_strs[SCRIPT_ERROR_COUNT];
extern const int _ftList[11];
extern char* _errStr;
extern int _last_color;
extern char* _strName;

extern int gGameDialogReactionOrFidget;

void scriptPredefinedError(Program* program, const char* name, int error);
void scriptError(const char* format, ...);
int tileIsVisible(int tile);
int _correctFidForRemovedItem(Object* a1, Object* a2, int a3);
void opGiveExpPoints(Program* program);
void opScrReturn(Program* program);
void opPlaySfx(Program* program);
void opSetMapStart(Program* program);
void opOverrideMapStart(Program* program);
void opHasSkill(Program* program);
void opUsingSkill(Program* program);
void opRollVsSkill(Program* program);
void opSkillContest(Program* program);
void opDoCheck(Program* program);
void opSuccess(Program* program);
void opCritical(Program* program);
void opHowMuch(Program* program);
void opMarkAreaKnown(Program* program);
void opReactionInfluence(Program* program);
void opRandom(Program* program);
void opRollDice(Program* program);
void opMoveTo(Program* program);
void opCreateObject(Program* program);
void opDestroyObject(Program* program);
void opDisplayMsg(Program* program);
void opScriptOverrides(Program* program);
void opObjectIsCarryingObjectWithPid(Program* program);
void opTileContainsObjectWithPid(Program* program);
void opGetSelf(Program* program);
void opGetSource(Program* program);
void opGetTarget(Program* program);
void opGetDude(Program* program);
void opGetObjectBeingUsed(Program* program);
void opGetLocalVar(Program* program);
void opSetLocalVar(Program* program);
void opGetMapVar(Program* program);
void opSetMapVar(Program* program);
void opGetGlobalVar(Program* program);
void opSetGlobalVar(Program* program);
void opGetScriptAction(Program* program);
void opGetObjectType(Program* program);
void opGetItemType(Program* program);
void opGetCritterStat(Program* program);
void opSetCritterStat(Program* program);
void opAnimateStand(Program* program);
void opAnimateStandReverse(Program* program);
void opAnimateMoveObjectToTile(Program* program);
void opTileInTileRect(Program* program);
void opMakeDayTime(Program* program);
void opTileDistanceBetween(Program* program);
void opTileDistanceBetweenObjects(Program* program);
void opGetObjectTile(Program* program);
void opGetTileInDirection(Program* program);
void opPickup(Program* program);
void opDrop(Program* program);
void opAddObjectToInventory(Program* program);
void opRemoveObjectFromInventory(Program* program);
void opWieldItem(Program* program);
void opUseObject(Program* program);
void opObjectCanSeeObject(Program* program);
void opAttackComplex(Program* program);
void opStartGameDialog(Program* program);
void opEndGameDialog(Program* program);
void opGameDialogReaction(Program* program);
void opMetarule3(Program* program);
void opSetMapMusic(Program* program);
void opSetObjectVisibility(Program* program);
void opLoadMap(Program* program);
void opWorldmapCitySetPos(Program* program);
void opSetExitGrids(Program* program);
void opAnimBusy(Program* program);
void opCritterHeal(Program* program);
void opSetLightLevel(Program* program);
void opGetGameTime(Program* program);
void opGetGameTimeInSeconds(Program* program);
void opGetObjectElevation(Program* program);
void opKillCritter(Program* program);
int _correctDeath(Object* critter, int anim, bool a3);
void opKillCritterType(Program* program);
void opCritterDamage(Program* program);
void opAddTimerEvent(Program* program);
void opRemoveTimerEvent(Program* program);
void opGameTicks(Program* program);
void opHasTrait(Program* program);
void opObjectCanHearObject(Program* program);
void opGameTimeHour(Program* program);
void opGetFixedParam(Program* program);
void opTileIsVisible(Program* program);
void opGameDialogSystemEnter(Program* program);
void opGetActionBeingUsed(Program* program);
void opGetCritterState(Program* program);
void opGameTimeAdvance(Program* program);
void opRadiationIncrease(Program* program);
void opRadiationDecrease(Program* program);
void opCritterAttemptPlacement(Program* program);
void opGetObjectPid(Program* program);
void opGetCurrentMap(Program* program);
void opCritterAddTrait(Program* program);
void opCritterRemoveTrait(Program* program);
void opGetProtoData(Program* program);
void opGetMessageString(Program* program);
void opCritterGetInventoryObject(Program* program);
void opSetObjectLightLevel(Program* program);
void opWorldmap(Program* program);
void _op_inven_cmds(Program* program);
void opFloatMessage(Program* program);
void opMetarule(Program* program);
void opAnim(Program* program);
void opObjectCarryingObjectByPid(Program* program);
void opRegAnimFunc(Program* program);
void opRegAnimAnimate(Program* program);
void opRegAnimAnimateReverse(Program* program);
void opRegAnimObjectMoveToObject(Program* program);
void opRegAnimObjectRunToObject(Program* program);
void opRegAnimObjectMoveToTile(Program* program);
void opRegAnimObjectRunToTile(Program* program);
void opPlayGameMovie(Program* program);
void opAddMultipleObjectsToInventory(Program* program);
void opRemoveMultipleObjectsFromInventory(Program* program);
void opGetMonth(Program* program);
void opGetDay(Program* program);
void opExplosion(Program* program);
void opGetDaysSinceLastVisit(Program* program);
void _op_gsay_start(Program* program);
void _op_gsay_end(Program* program);
void _op_gsay_reply(Program* program);
void _op_gsay_option(Program* program);
void _op_gsay_message(Program* program);
void _op_giq_option(Program* program);
void opPoison(Program* program);
void opGetPoison(Program* program);
void opPartyAdd(Program* program);
void opPartyRemove(Program* program);
void opRegAnimAnimateForever(Program* program);
void opCritterInjure(Program* program);
void opCombatIsInitialized(Program* program);
void _op_gdialog_barter(Program* program);
void opGetGameDifficulty(Program* program);
void opGetRunningBurningGuy(Program* program);
void _op_inven_unwield(Program* program);
void opObjectIsLocked(Program* program);
void opObjectLock(Program* program);
void opObjectUnlock(Program* program);
void opObjectIsOpen(Program* program);
void opObjectOpen(Program* program);
void opObjectClose(Program* program);
void opGameUiDisable(Program* program);
void opGameUiEnable(Program* program);
void opGameUiIsDisabled(Program* program);
void opFadeOut(Program* program);
void opFadeIn(Program* program);
void opItemCapsTotal(Program* program);
void opItemCapsAdjust(Program* program);
void _op_anim_action_frame(Program* program);
void opRegAnimPlaySfx(Program* program);
void opCritterModifySkill(Program* program);
void opSfxBuildCharName(Program* program);
void opSfxBuildAmbientName(Program* program);
void opSfxBuildInterfaceName(Program* program);
void opSfxBuildItemName(Program* program);
void opSfxBuildWeaponName(Program* program);
void opSfxBuildSceneryName(Program* program);
void opSfxBuildOpenName(Program* program);
void opAttackSetup(Program* program);
void opDestroyMultipleObjects(Program* program);
void opUseObjectOnObject(Program* program);
void opEndgameSlideshow(Program* program);
void opMoveObjectInventoryToObject(Program* program);
void opEndgameMovie(Program* program);
void opGetObjectFid(Program* program);
void opGetFidAnim(Program* program);
void opGetPartyMember(Program* program);
void opGetRotationToTile(Program* program);
void opJamLock(Program* program);
void opGameDialogSetBarterMod(Program* program);
void opGetCombatDifficulty(Program* program);
void opObjectOnScreen(Program* program);
void opCritterIsFleeing(Program* program);
void opCritterSetFleeState(Program* program);
void opTerminateCombat(Program* program);
void opDebugMessage(Program* program);
void opCritterStopAttacking(Program* program);
void opTileGetObjectWithPid(Program* program);
void opGetObjectName(Program* program);
void opGetPcStat(Program* program);
void _intExtraClose_();
void _initIntExtra();
void _intExtraRemoveProgramReferences_();

#endif /* INTERPRETER_EXTRA_H */
