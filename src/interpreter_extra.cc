#include "interpreter_extra.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "critter.h"
#include "debug.h"
#include "dialog.h"
#include "display_monitor.h"
#include "endgame.h"
#include "game.h"
#include "game_dialog.h"
#include "game_movie.h"
#include "game_sound.h"
#include "geometry.h"
#include "interface.h"
#include "item.h"
#include "light.h"
#include "loadsave.h"
#include "map.h"
#include "object.h"
#include "palette.h"
#include "party_member.h"
#include "perk.h"
#include "proto.h"
#include "proto_instance.h"
#include "queue.h"
#include "random.h"
#include "reaction.h"
#include "scripts.h"
#include "settings.h"
#include "sfall_opcodes.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "text_object.h"
#include "tile.h"
#include "trait.h"
#include "vcr.h"
#include "worldmap.h"

namespace fallout {

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

static void scriptPredefinedError(Program* program, const char* name, int error);
static void scriptError(const char* format, ...);
static int tileIsVisible(int tile);
static int _correctFidForRemovedItem(Object* a1, Object* a2, int a3);
static void opGiveExpPoints(Program* program);
static void opScrReturn(Program* program);
static void opPlaySfx(Program* program);
static void opSetMapStart(Program* program);
static void opOverrideMapStart(Program* program);
static void opHasSkill(Program* program);
static void opUsingSkill(Program* program);
static void opRollVsSkill(Program* program);
static void opSkillContest(Program* program);
static void opDoCheck(Program* program);
static void opSuccess(Program* program);
static void opCritical(Program* program);
static void opHowMuch(Program* program);
static void opMarkAreaKnown(Program* program);
static void opReactionInfluence(Program* program);
static void opRandom(Program* program);
static void opRollDice(Program* program);
static void opMoveTo(Program* program);
static void opCreateObject(Program* program);
static void opDestroyObject(Program* program);
static void opDisplayMsg(Program* program);
static void opScriptOverrides(Program* program);
static void opObjectIsCarryingObjectWithPid(Program* program);
static void opTileContainsObjectWithPid(Program* program);
static void opGetSelf(Program* program);
static void opGetSource(Program* program);
static void opGetTarget(Program* program);
static void opGetDude(Program* program);
static void opGetObjectBeingUsed(Program* program);
static void opGetLocalVar(Program* program);
static void opSetLocalVar(Program* program);
static void opGetMapVar(Program* program);
static void opSetMapVar(Program* program);
static void opGetGlobalVar(Program* program);
static void opSetGlobalVar(Program* program);
static void opGetScriptAction(Program* program);
static void opGetObjectType(Program* program);
static void opGetItemType(Program* program);
static void opGetCritterStat(Program* program);
static void opSetCritterStat(Program* program);
static void opAnimateStand(Program* program);
static void opAnimateStandReverse(Program* program);
static void opAnimateMoveObjectToTile(Program* program);
static void opTileInTileRect(Program* program);
static void opMakeDayTime(Program* program);
static void opTileDistanceBetween(Program* program);
static void opTileDistanceBetweenObjects(Program* program);
static void opGetObjectTile(Program* program);
static void opGetTileInDirection(Program* program);
static void opPickup(Program* program);
static void opDrop(Program* program);
static void opAddObjectToInventory(Program* program);
static void opRemoveObjectFromInventory(Program* program);
static void opWieldItem(Program* program);
static void opUseObject(Program* program);
static void opObjectCanSeeObject(Program* program);
static void opAttackComplex(Program* program);
static void opStartGameDialog(Program* program);
static void opEndGameDialog(Program* program);
static void opGameDialogReaction(Program* program);
static void opMetarule3(Program* program);
static void opSetMapMusic(Program* program);
static void opSetObjectVisibility(Program* program);
static void opLoadMap(Program* program);
static void opWorldmapCitySetPos(Program* program);
static void opSetExitGrids(Program* program);
static void opAnimBusy(Program* program);
static void opCritterHeal(Program* program);
static void opSetLightLevel(Program* program);
static void opGetGameTime(Program* program);
static void opGetGameTimeInSeconds(Program* program);
static void opGetObjectElevation(Program* program);
static void opKillCritter(Program* program);
static int _correctDeath(Object* critter, int anim, bool a3);
static void opKillCritterType(Program* program);
static void opCritterDamage(Program* program);
static void opAddTimerEvent(Program* program);
static void opRemoveTimerEvent(Program* program);
static void opGameTicks(Program* program);
static void opHasTrait(Program* program);
static void opObjectCanHearObject(Program* program);
static void opGameTimeHour(Program* program);
static void opGetFixedParam(Program* program);
static void opTileIsVisible(Program* program);
static void opGameDialogSystemEnter(Program* program);
static void opGetActionBeingUsed(Program* program);
static void opGetCritterState(Program* program);
static void opGameTimeAdvance(Program* program);
static void opRadiationIncrease(Program* program);
static void opRadiationDecrease(Program* program);
static void opCritterAttemptPlacement(Program* program);
static void opGetObjectPid(Program* program);
static void opGetCurrentMap(Program* program);
static void opCritterAddTrait(Program* program);
static void opCritterRemoveTrait(Program* program);
static void opGetProtoData(Program* program);
static void opGetMessageString(Program* program);
static void opCritterGetInventoryObject(Program* program);
static void opSetObjectLightLevel(Program* program);
static void opWorldmap(Program* program);
static void _op_inven_cmds(Program* program);
static void opFloatMessage(Program* program);
static void opMetarule(Program* program);
static void opAnim(Program* program);
static void opObjectCarryingObjectByPid(Program* program);
static void opRegAnimFunc(Program* program);
static void opRegAnimAnimate(Program* program);
static void opRegAnimAnimateReverse(Program* program);
static void opRegAnimObjectMoveToObject(Program* program);
static void opRegAnimObjectRunToObject(Program* program);
static void opRegAnimObjectMoveToTile(Program* program);
static void opRegAnimObjectRunToTile(Program* program);
static void opPlayGameMovie(Program* program);
static void opAddMultipleObjectsToInventory(Program* program);
static void opRemoveMultipleObjectsFromInventory(Program* program);
static void opGetMonth(Program* program);
static void opGetDay(Program* program);
static void opExplosion(Program* program);
static void opGetDaysSinceLastVisit(Program* program);
static void _op_gsay_start(Program* program);
static void _op_gsay_end(Program* program);
static void _op_gsay_reply(Program* program);
static void _op_gsay_option(Program* program);
static void _op_gsay_message(Program* program);
static void _op_giq_option(Program* program);
static void opPoison(Program* program);
static void opGetPoison(Program* program);
static void opPartyAdd(Program* program);
static void opPartyRemove(Program* program);
static void opRegAnimAnimateForever(Program* program);
static void opCritterInjure(Program* program);
static void opCombatIsInitialized(Program* program);
static void _op_gdialog_barter(Program* program);
static void opGetGameDifficulty(Program* program);
static void opGetRunningBurningGuy(Program* program);
static void _op_inven_unwield(Program* program);
static void opObjectIsLocked(Program* program);
static void opObjectLock(Program* program);
static void opObjectUnlock(Program* program);
static void opObjectIsOpen(Program* program);
static void opObjectOpen(Program* program);
static void opObjectClose(Program* program);
static void opGameUiDisable(Program* program);
static void opGameUiEnable(Program* program);
static void opGameUiIsDisabled(Program* program);
static void opGameFadeOut(Program* program);
static void opGameFadeIn(Program* program);
static void opItemCapsTotal(Program* program);
static void opItemCapsAdjust(Program* program);
static void _op_anim_action_frame(Program* program);
static void opRegAnimPlaySfx(Program* program);
static void opCritterModifySkill(Program* program);
static void opSfxBuildCharName(Program* program);
static void opSfxBuildAmbientName(Program* program);
static void opSfxBuildInterfaceName(Program* program);
static void opSfxBuildItemName(Program* program);
static void opSfxBuildWeaponName(Program* program);
static void opSfxBuildSceneryName(Program* program);
static void opSfxBuildOpenName(Program* program);
static void opAttackSetup(Program* program);
static void opDestroyMultipleObjects(Program* program);
static void opUseObjectOnObject(Program* program);
static void opEndgameSlideshow(Program* program);
static void opMoveObjectInventoryToObject(Program* program);
static void opEndgameMovie(Program* program);
static void opGetObjectFid(Program* program);
static void opGetFidAnim(Program* program);
static void opGetPartyMember(Program* program);
static void opGetRotationToTile(Program* program);
static void opJamLock(Program* program);
static void opGameDialogSetBarterMod(Program* program);
static void opGetCombatDifficulty(Program* program);
static void opObjectOnScreen(Program* program);
static void opCritterIsFleeing(Program* program);
static void opCritterSetFleeState(Program* program);
static void opTerminateCombat(Program* program);
static void opDebugMessage(Program* program);
static void opCritterStopAttacking(Program* program);
static void opTileGetObjectWithPid(Program* program);
static void opGetObjectName(Program* program);
static void opGetPcStat(Program* program);

// 0x504B0C
static char _aCritter[] = "<Critter>";

// 0x453FC0
static Rect stru_453FC0 = { 0, 0, 640, 480 };

// 0x518EC0
static const char* _dbg_error_strs[SCRIPT_ERROR_COUNT] = {
    "unimped",
    "obj is NULL",
    "can't match program to sid",
    "follows",
};

// Last message type during op_float_msg sequential.
//
// 0x518F00
static int _last_color = 1;

// 0x518F04
static char* _strName = _aCritter;

// NOTE: This value is a little bit odd. It's used to handle 2 operations:
// [opStartGameDialog] and [opGameDialogReaction]. It's not used outside those
// functions.
//
// When used inside [opStartGameDialog] this value stores [Fidget] constant
// (1 - Good, 4 - Neutral, 7 - Bad).
//
// When used inside [opGameDialogReaction] this value contains specified
// reaction (-1 - Good, 0 - Neutral, 1 - Bad).
//
// 0x5970D0
static int gGameDialogReactionOrFidget;

// 0x453FD0
static void scriptPredefinedError(Program* program, const char* name, int error)
{
    char string[260];

    snprintf(string, sizeof(string), "Script Error: %s: op_%s: %s", program->name, name, _dbg_error_strs[error]);

    debugPrint(string);
}

// 0x45400C
static void scriptError(const char* format, ...)
{
    char string[260];

    va_list argptr;
    va_start(argptr, format);
    vsnprintf(string, sizeof(string), format, argptr);
    va_end(argptr);

    debugPrint(string);
}

// 0x45404C
static int tileIsVisible(int tile)
{
    if (abs(gCenterTile - tile) % 200 < 5) {
        return 1;
    }

    if (abs(gCenterTile - tile) / 200 < 5) {
        return 1;
    }

    return 0;
}

// 0x45409C
static int _correctFidForRemovedItem(Object* a1, Object* a2, int flags)
{
    if (a1 == gDude) {
        bool animated = !gameUiIsDisabled();
        interfaceUpdateItems(animated, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }

    int fid = a1->fid;
    int v8 = (fid & 0xF000) >> 12;
    int newFid = -1;

    if ((flags & 0x03000000) != 0) {
        if (a1 == gDude) {
            if (interfaceGetCurrentHand()) {
                if ((flags & 0x02000000) != 0) {
                    v8 = 0;
                }
            } else {
                if ((flags & 0x01000000) != 0) {
                    v8 = 0;
                }
            }
        } else {
            if ((flags & 0x02000000) != 0) {
                v8 = 0;
            }
        }

        if (v8 == 0) {
            newFid = buildFid(FID_TYPE(fid), fid & 0xFFF, FID_ANIM_TYPE(fid), 0, (fid & 0x70000000) >> 28);
        }
    } else {
        if (a1 == gDude) {
            newFid = buildFid(FID_TYPE(fid), _art_vault_guy_num, FID_ANIM_TYPE(fid), v8, (fid & 0x70000000) >> 28);
        }

        _adjust_ac(a1, a2, NULL);
    }

    if (newFid != -1) {
        Rect rect;
        objectSetFid(a1, newFid, &rect);
        tileWindowRefreshRect(&rect, gElevation);
    }

    return 0;
}

// give_exp_points
// 0x4541C8
static void opGiveExpPoints(Program* program)
{
    int xp = programStackPopInteger(program);

    if (pcAddExperience(xp) != 0) {
        scriptError("\nScript Error: %s: op_give_exp_points: stat_pc_set failed");
    }
}

// scr_return
// 0x454238
static void opScrReturn(Program* program)
{
    int data = programStackPopInteger(program);

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        script->field_28 = data;
    }
}

// play_sfx
// 0x4542AC
static void opPlaySfx(Program* program)
{
    char* name = programStackPopString(program);

    soundPlayFile(name);
}

// set_map_start
// 0x454314
static void opSetMapStart(Program* program)
{
    int rotation = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    if (mapSetElevation(elevation) != 0) {
        scriptError("\nScript Error: %s: op_set_map_start: map_set_elevation failed", program->name);
        return;
    }

    int tile = 200 * y + x;
    if (tileSetCenter(tile, TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS) != 0) {
        scriptError("\nScript Error: %s: op_set_map_start: tile_set_center failed", program->name);
        return;
    }

    mapSetStart(tile, elevation, rotation);
}

// override_map_start
// 0x4543F4
static void opOverrideMapStart(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int rotation = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    char text[60];
    snprintf(text, sizeof(text), "OVERRIDE_MAP_START: x: %d, y: %d", x, y);
    debugPrint(text);

    int tile = 200 * y + x;
    int previousTile = gCenterTile;
    if (tile != -1) {
        if (objectSetRotation(gDude, rotation, NULL) != 0) {
            scriptError("\nError: %s: obj_set_rotation failed in override_map_start!", program->name);
        }

        if (objectSetLocation(gDude, tile, elevation, NULL) != 0) {
            scriptError("\nError: %s: obj_move_to_tile failed in override_map_start!", program->name);

            if (objectSetLocation(gDude, previousTile, elevation, NULL) != 0) {
                scriptError("\nError: %s: obj_move_to_tile RECOVERY Also failed!");
                exit(1);
            }
        }

        tileSetCenter(tile, TILE_SET_CENTER_REFRESH_WINDOW);
        tileWindowRefresh();
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// has_skill
// 0x454568
static void opHasSkill(Program* program)
{
    int skill = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int result = 0;
    if (object != NULL) {
        if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            result = skillGetValue(object, skill);
        }
    } else {
        scriptPredefinedError(program, "has_skill", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, result);
}

// using_skill
// 0x454634
static void opUsingSkill(Program* program)
{
    int skill = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    // NOTE: In the original source code this value is left uninitialized, that
    // explains why garbage is returned when using something else than dude and
    // SKILL_SNEAK as arguments.
    int result = 0;

    if (skill == SKILL_SNEAK && object == gDude) {
        result = dudeHasState(DUDE_STATE_SNEAKING);
    }

    programStackPushInteger(program, result);
}

// roll_vs_skill
// 0x4546E8
static void opRollVsSkill(Program* program)
{
    int modifier = programStackPopInteger(program);
    int skill = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int roll = ROLL_CRITICAL_FAILURE;
    if (object != NULL) {
        if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            int sid = scriptGetSid(program);

            Script* script;
            if (scriptGetScript(sid, &script) != -1) {
                roll = skillRoll(object, skill, modifier, &(script->howMuch));
            }
        }
    } else {
        scriptPredefinedError(program, "roll_vs_skill", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, roll);
}

// skill_contest
// 0x4547D4
static void opSkillContest(Program* program)
{
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    scriptPredefinedError(program, "skill_contest", SCRIPT_ERROR_NOT_IMPLEMENTED);
    programStackPushInteger(program, 0);
}

// do_check
// 0x454890
static void opDoCheck(Program* program)
{
    int mod = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int roll = 0;
    if (object != NULL) {
        int sid = scriptGetSid(program);

        Script* script;
        if (scriptGetScript(sid, &script) != -1) {
            switch (stat) {
            case STAT_STRENGTH:
            case STAT_PERCEPTION:
            case STAT_ENDURANCE:
            case STAT_CHARISMA:
            case STAT_INTELLIGENCE:
            case STAT_AGILITY:
            case STAT_LUCK:
                roll = statRoll(object, stat, mod, &(script->howMuch));
                break;
            default:
                scriptError("\nScript Error: %s: op_do_check: Stat out of range", program->name);
                break;
            }
        }
    } else {
        scriptPredefinedError(program, "do_check", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, roll);
}

// success
// 0x4549A8
static void opSuccess(Program* program)
{
    int data = programStackPopInteger(program);

    int result = -1;

    switch (data) {
    case ROLL_CRITICAL_FAILURE:
    case ROLL_FAILURE:
        result = 0;
        break;
    case ROLL_SUCCESS:
    case ROLL_CRITICAL_SUCCESS:
        result = 1;
        break;
    }

    programStackPushInteger(program, result);
}

// critical
// 0x454A44
static void opCritical(Program* program)
{
    int data = programStackPopInteger(program);

    int result = -1;

    switch (data) {
    case ROLL_CRITICAL_FAILURE:
    case ROLL_CRITICAL_SUCCESS:
        result = 1;
        break;
    case ROLL_FAILURE:
    case ROLL_SUCCESS:
        result = 0;
        break;
    }

    programStackPushInteger(program, result);
}

// how_much
// 0x454AD0
static void opHowMuch(Program* program)
{
    int data = programStackPopInteger(program);

    int result = 0;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        result = script->howMuch;
    } else {
        scriptPredefinedError(program, "how_much", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInteger(program, result);
}

// mark_area_known
// 0x454B6C
static void opMarkAreaKnown(Program* program)
{
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    // TODO: Provide meaningful names.
    if (data[2] == 0) {
        if (data[0] == CITY_STATE_INVISIBLE) {
            wmAreaSetVisibleState(data[1], 0, 1);
        } else {
            wmAreaSetVisibleState(data[1], 1, 1);
            wmAreaMarkVisitedState(data[1], data[0]);
        }
    } else if (data[2] == 1) {
        wmMapMarkVisited(data[1]);
    }
}

// reaction_influence
// 0x454C34
static void opReactionInfluence(Program* program)
{
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    int result = _reaction_influence_();
    programStackPushInteger(program, result);
}

// random
// 0x454CD4
static void opRandom(Program* program)
{
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    int result;
    if (vcrGetState() == VCR_STATE_TURNED_OFF) {
        result = randomBetween(data[1], data[0]);
    } else {
        result = (data[0] - data[1]) / 2;
    }

    programStackPushInteger(program, result);
}

// roll_dice
// 0x454D88
static void opRollDice(Program* program)
{
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    scriptPredefinedError(program, "roll_dice", SCRIPT_ERROR_NOT_IMPLEMENTED);

    programStackPushInteger(program, 0);
}

// move_to
// 0x454E28
static void opMoveTo(Program* program)
{
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int newTile;

    if (object != NULL) {
        if (object == gDude) {
            bool tileLimitingEnabled = tileScrollLimitingIsEnabled();
            bool tileBlockingEnabled = tileScrollBlockingIsEnabled();

            if (tileLimitingEnabled) {
                tileScrollLimitingDisable();
            }

            if (tileBlockingEnabled) {
                tileScrollBlockingDisable();
            }

            Rect rect;
            newTile = objectSetLocation(object, tile, elevation, &rect);
            if (newTile != -1) {
                tileSetCenter(object->tile, TILE_SET_CENTER_REFRESH_WINDOW);
            }

            if (tileLimitingEnabled) {
                tileScrollLimitingEnable();
            }

            if (tileBlockingEnabled) {
                tileScrollBlockingEnable();
            }
        } else {
            Rect before;
            objectGetRect(object, &before);

            if (object->elevation != elevation && PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                _combat_delete_critter(object);
            }

            Rect after;
            newTile = objectSetLocation(object, tile, elevation, &after);
            if (newTile != -1) {
                rectUnion(&before, &after, &before);
                tileWindowRefreshRect(&before, gElevation);
            }
        }
    } else {
        scriptPredefinedError(program, "move_to", SCRIPT_ERROR_OBJECT_IS_NULL);
        newTile = -1;
    }

    programStackPushInteger(program, newTile);
}

// create_object_sid
// 0x454FA8
static void opCreateObject(Program* program)
{
    int data[4];

    for (int arg = 0; arg < 4; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    int pid = data[3];
    int tile = data[2];
    int elevation = data[1];
    int sid = data[0];

    Object* object = NULL;

    if (_isLoadingGame() != 0) {
        debugPrint("\nError: attempt to Create critter in load/save-game: %s!", program->name);
        goto out;
    }

    if (pid == 0) {
        debugPrint("\nError: attempt to Create critter With PID of 0: %s!", program->name);
        goto out;
    }

    Proto* proto;
    if (protoGetProto(pid, &proto) != -1) {
        if (objectCreateWithFidPid(&object, proto->fid, pid) != -1) {
            if (tile == -1) {
                tile = 0;
            }

            Rect rect;
            if (objectSetLocation(object, tile, elevation, &rect) != -1) {
                tileWindowRefreshRect(&rect, object->elevation);
            }
        }
    } else {
        // SFALL: Prevent a crash when the proto is missing.
        goto out;
    }

    if (sid != -1) {
        int scriptType = 0;
        switch (PID_TYPE(object->pid)) {
        case OBJ_TYPE_CRITTER:
            scriptType = SCRIPT_TYPE_CRITTER;
            break;
        case OBJ_TYPE_ITEM:
        case OBJ_TYPE_SCENERY:
            scriptType = SCRIPT_TYPE_ITEM;
            break;
        }

        if (object->sid != -1) {
            scriptRemove(object->sid);
            object->sid = -1;
        }

        if (scriptAdd(&(object->sid), scriptType) == -1) {
            goto out;
        }

        Script* script;
        if (scriptGetScript(object->sid, &script) == -1) {
            goto out;
        }

        script->field_14 = sid - 1;

        if (scriptType == SCRIPT_TYPE_SPATIAL) {
            script->sp.built_tile = builtTileCreate(object->tile, object->elevation);
            script->sp.radius = 3;
        }

        object->id = scriptsNewObjectId();
        script->field_1C = object->id;
        script->owner = object;
        _scr_find_str_run_info(sid - 1, &(script->field_50), object->sid);
    };

out:

    programStackPushPointer(program, object);
}

// destroy_object
// 0x4551E4
static void opDestroyObject(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        scriptPredefinedError(program, "destroy_object", SCRIPT_ERROR_OBJECT_IS_NULL);
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        if (_isLoadingGame()) {
            debugPrint("\nError: attempt to destroy critter in load/save-game: %s!", program->name);
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }
    }

    bool isSelf = object == scriptGetSelf(program);

    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        _combat_delete_critter(object);
    }

    Object* owner = objectGetOwner(object);
    if (owner != NULL) {
        int quantity = itemGetQuantity(owner, object);
        itemRemove(owner, object, quantity);

        if (owner == gDude) {
            bool animated = !gameUiIsDisabled();
            interfaceUpdateItems(animated, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
        }

        _obj_connect(object, 1, 0, NULL);

        if (isSelf) {
            object->sid = -1;
            object->flags |= (OBJECT_HIDDEN | OBJECT_NO_SAVE);
        } else {
            reg_anim_clear(object);
            objectDestroy(object, NULL);
        }
    } else {
        reg_anim_clear(object);

        Rect rect;
        objectDestroy(object, &rect);
        tileWindowRefreshRect(&rect, gElevation);
    }

    program->flags &= ~PROGRAM_FLAG_0x20;

    if (isSelf) {
        program->flags |= PROGRAM_FLAG_0x0100;
    }
}

// display_msg
// 0x455388
static void opDisplayMsg(Program* program)
{
    char* string = programStackPopString(program);
    displayMonitorAddMessage(string);

    if (settings.debug.show_script_messages) {
        debugPrint("\n");
        debugPrint(string);
    }
}

// script_overrides
// 0x455430
static void opScriptOverrides(Program* program)
{
    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        script->scriptOverrides = 1;
    } else {
        scriptPredefinedError(program, "script_overrides", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }
}

// obj_is_carrying_obj_pid
// 0x455470
static void opObjectIsCarryingObjectWithPid(Program* program)
{
    int pid = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int result = 0;
    if (obj != NULL) {
        result = objectGetCarriedQuantityByPid(obj, pid);
    } else {
        scriptPredefinedError(program, "obj_is_carrying_obj_pid", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, result);
}

// tile_contains_obj_pid
// 0x455534
static void opTileContainsObjectWithPid(Program* program)
{
    int pid = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);

    int result = 0;

    Object* object = objectFindFirstAtLocation(elevation, tile);
    while (object) {
        if (object->pid == pid) {
            result = 1;
            break;
        }
        object = objectFindNextAtLocation();
    }

    programStackPushInteger(program, result);
}

// self_obj
// 0x455600
static void opGetSelf(Program* program)
{
    Object* object = scriptGetSelf(program);
    programStackPushPointer(program, object);
}

// source_obj
// 0x455624
static void opGetSource(Program* program)
{
    Object* object = NULL;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        object = script->source;
    } else {
        scriptPredefinedError(program, "source_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushPointer(program, object);
}

// target_obj
// 0x455678
static void opGetTarget(Program* program)
{
    Object* object = NULL;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        object = script->target;
    } else {
        scriptPredefinedError(program, "target_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushPointer(program, object);
}

// dude_obj
// 0x4556CC
static void opGetDude(Program* program)
{
    programStackPushPointer(program, gDude);
}

// NOTE: The implementation is the same as in [opGetTarget].
//
// obj_being_used_with
// 0x4556EC
static void opGetObjectBeingUsed(Program* program)
{
    Object* object = NULL;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        object = script->target;
    } else {
        scriptPredefinedError(program, "obj_being_used_with", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushPointer(program, object);
}

// local_var
// 0x455740
static void opGetLocalVar(Program* program)
{
    int data = programStackPopInteger(program);

    ProgramValue value;
    value.opcode = VALUE_TYPE_INT;
    value.integerValue = -1;

    int sid = scriptGetSid(program);
    scriptGetLocalVar(sid, data, value);

    programStackPushValue(program, value);
}

// set_local_var
// 0x4557C8
static void opSetLocalVar(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    int variable = programStackPopInteger(program);

    int sid = scriptGetSid(program);
    scriptSetLocalVar(sid, variable, value);
}

// map_var
// 0x455858
static void opGetMapVar(Program* program)
{
    int data = programStackPopInteger(program);

    ProgramValue value;
    if (mapGetGlobalVar(data, value) == -1) {
        value.opcode = VALUE_TYPE_INT;
        value.integerValue = -1;
    }

    programStackPushValue(program, value);
}

// set_map_var
// 0x4558C8
static void opSetMapVar(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    int variable = programStackPopInteger(program);

    mapSetGlobalVar(variable, value);
}

// global_var
// 0x455950
static void opGetGlobalVar(Program* program)
{
    int variable = programStackPopInteger(program);

    if (gGameGlobalVarsLength != 0) {
        void* ptr = gameGetGlobalPointer(variable);
        if (ptr != nullptr) {
            programStackPushPointer(program, ptr);
        } else {
            programStackPushInteger(program, gameGetGlobalVar(variable));
        }
    } else {
        scriptError("\nScript Error: %s: op_global_var: no global vars found!", program->name);
        programStackPushInteger(program, -1);
    }
}

// set_global_var
// 0x4559EC
// 0x80C6
static void opSetGlobalVar(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    int variable = programStackPopInteger(program);

    if (gGameGlobalVarsLength != 0) {
        if (value.opcode == VALUE_TYPE_PTR) {
            gameSetGlobalPointer(variable, value.pointerValue);
            gameSetGlobalVar(variable, 0);
        } else {
            gameSetGlobalPointer(variable, nullptr);
            gameSetGlobalVar(variable, value.integerValue);
        }
    } else {
        scriptError("\nScript Error: %s: op_set_global_var: no global vars found!", program->name);
    }
}

// script_action
// 0x455A90
static void opGetScriptAction(Program* program)
{
    int action = 0;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        action = script->action;
    } else {
        scriptPredefinedError(program, "script_action", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInteger(program, action);
}

// obj_type
// 0x455AE4
static void opGetObjectType(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int objectType = -1;
    if (object != NULL) {
        objectType = FID_TYPE(object->fid);
    }

    programStackPushInteger(program, objectType);
}

// obj_item_subtype
// 0x455B6C
static void opGetItemType(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int itemType = -1;
    if (obj != NULL) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            Proto* proto;
            if (protoGetProto(obj->pid, &proto) != -1) {
                itemType = itemGetType(obj);
            }
        }
    }

    programStackPushInteger(program, itemType);
}

// get_critter_stat
// 0x455C10
static void opGetCritterStat(Program* program)
{
    int stat = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int value = -1;
    if (object != NULL) {
        value = critterGetStat(object, stat);
    } else {
        scriptPredefinedError(program, "get_critter_stat", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, value);
}

// NOTE: Despite it's name it does not actually "set" stat, but "adjust". So
// it's last argument is amount of adjustment, not it's final value.
//
// set_critter_stat
// 0x455CCC
static void opSetCritterStat(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int result = 0;
    if (object != NULL) {
        if (object == gDude) {
            int currentValue = critterGetBaseStatWithTraitModifier(object, stat);
            critterSetBaseStat(object, stat, currentValue + value);
        } else {
            scriptPredefinedError(program, "set_critter_stat", SCRIPT_ERROR_FOLLOWS);
            debugPrint(" Can't modify anyone except obj_dude!");
            result = -1;
        }
    } else {
        scriptPredefinedError(program, "set_critter_stat", SCRIPT_ERROR_OBJECT_IS_NULL);
        result = -1;
    }

    programStackPushInteger(program, result);
}

// animate_stand_obj
// 0x455DC8
static void opAnimateStand(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    if (object == NULL) {
        int sid = scriptGetSid(program);

        Script* script;
        if (scriptGetScript(sid, &script) == -1) {
            scriptPredefinedError(program, "animate_stand_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
            return;
        }

        object = scriptGetSelf(program);
    }

    if (!isInCombat()) {
        reg_anim_begin(ANIMATION_REQUEST_UNRESERVED);
        animationRegisterAnimate(object, ANIM_STAND, 0);
        reg_anim_end();
    }
}

// animate_stand_reverse_obj
// 0x455E7C
static void opAnimateStandReverse(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    if (object == NULL) {
        int sid = scriptGetSid(program);

        Script* script;
        if (scriptGetScript(sid, &script) == -1) {
            scriptPredefinedError(program, "animate_stand_reverse_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
            return;
        }

        object = scriptGetSelf(program);
    }

    if (!isInCombat()) {
        reg_anim_begin(ANIMATION_REQUEST_UNRESERVED);
        animationRegisterAnimateReversed(object, ANIM_STAND, 0);
        reg_anim_end();
    }
}

// animate_move_obj_to_tile
// 0x455F30
static void opAnimateMoveObjectToTile(Program* program)
{
    int flags = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        scriptPredefinedError(program, "animate_move_obj_to_tile", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (tile <= -1) {
        return;
    }

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        scriptPredefinedError(program, "animate_move_obj_to_tile", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
        return;
    }

    if (!critterIsActive(object)) {
        return;
    }

    if (isInCombat()) {
        return;
    }

    if ((flags & 0x10) != 0) {
        reg_anim_clear(object);
        flags &= ~0x10;
    }

    reg_anim_begin(ANIMATION_REQUEST_UNRESERVED);

    if (flags == 0) {
        animationRegisterMoveToTile(object, tile, object->elevation, -1, 0);
    } else {
        animationRegisterRunToTile(object, tile, object->elevation, -1, 0);
    }

    reg_anim_end();
}

// tile_in_tile_rect
// 0x45607C
static void opTileInTileRect(Program* program)
{
    Point points[5];

    for (int arg = 0; arg < 5; arg++) {
        int value = programStackPopInteger(program);

        points[arg].x = value % 200;
        points[arg].y = value / 200;
    }

    int x = points[0].x;
    int y = points[0].y;

    int minX = points[1].x;
    int maxX = points[4].x;

    int minY = points[4].y;
    int maxY = points[1].y;

    int result = 0;
    if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
        result = 1;
    }

    programStackPushInteger(program, result);
}

// make_daytime
// 0x456170
static void opMakeDayTime(Program* program)
{
}

// tile_distance
// 0x456174
static void opTileDistanceBetween(Program* program)
{
    int tile2 = programStackPopInteger(program);
    int tile1 = programStackPopInteger(program);

    int distance;

    if (tile1 != -1 && tile2 != -1) {
        distance = tileDistanceBetween(tile1, tile2);
    } else {
        distance = 9999;
    }

    programStackPushInteger(program, distance);
}

// tile_distance_objs
// 0x456228
static void opTileDistanceBetweenObjects(Program* program)
{
    Object* object2 = static_cast<Object*>(programStackPopPointer(program));
    Object* object1 = static_cast<Object*>(programStackPopPointer(program));

    int distance = 9999;
    if (object1 != NULL && object2 != NULL) {
        if ((uintptr_t)object2 >= HEX_GRID_SIZE && (uintptr_t)object1 >= HEX_GRID_SIZE) {
            if (object1->elevation == object2->elevation) {
                if (object1->tile != -1 && object2->tile != -1) {
                    distance = tileDistanceBetween(object1->tile, object2->tile);
                }
            }
        } else {
            scriptPredefinedError(program, "tile_distance_objs", SCRIPT_ERROR_FOLLOWS);
            debugPrint(" Passed a tile # instead of an object!!!BADBADBAD!");
        }
    }

    programStackPushInteger(program, distance);
}

// tile_num
// 0x456324
static void opGetObjectTile(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int tile = -1;
    if (obj != NULL) {
        tile = obj->tile;
    } else {
        scriptPredefinedError(program, "tile_num", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, tile);
}

// tile_num_in_direction
// 0x4563B4
static void opGetTileInDirection(Program* program)
{
    int distance = programStackPopInteger(program);
    int rotation = programStackPopInteger(program);
    int origin = programStackPopInteger(program);

    int tile = -1;

    if (origin != -1) {
        if (rotation < ROTATION_COUNT) {
            if (distance != 0) {
                tile = tileGetTileInDirection(origin, rotation, distance);
                if (tile < -1) {
                    debugPrint("\nError: %s: op_tile_num_in_direction got #: %d", program->name, tile);
                    tile = -1;
                }
            }
        } else {
            scriptPredefinedError(program, "tile_num_in_direction", SCRIPT_ERROR_FOLLOWS);
            debugPrint(" rotation out of Range!");
        }
    } else {
        scriptPredefinedError(program, "tile_num_in_direction", SCRIPT_ERROR_FOLLOWS);
        debugPrint(" tileNum is -1!");
    }

    programStackPushInteger(program, tile);
}

// pickup_obj
// 0x4564D4
static void opPickup(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        return;
    }

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) == 1) {
        scriptPredefinedError(program, "pickup_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
        return;
    }

    Object* self = script->target;

    // SFALL: Override `self` via `op_set_self`.
    // CE: Implementation is different. Sfall integrates via `scriptGetSid` by
    // returning fake script with overridden `self` (and `target` in this case).
    if (script->overriddenSelf != nullptr) {
        self = script->overriddenSelf;
        script->overriddenSelf = nullptr;
    }

    if (self == NULL) {
        scriptPredefinedError(program, "pickup_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    actionPickUp(self, object);
}

// drop_obj
// 0x456580
static void opDrop(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        return;
    }

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        // FIXME: Should be SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID.
        scriptPredefinedError(program, "drop_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (script->target == NULL) {
        // FIXME: Should be SCRIPT_ERROR_OBJECT_IS_NULL.
        scriptPredefinedError(program, "drop_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
        return;
    }

    _obj_drop(script->target, object);
}

// add_obj_to_inven
// 0x45662C
static void opAddObjectToInventory(Program* program)
{
    Object* item = static_cast<Object*>(programStackPopPointer(program));
    Object* owner = static_cast<Object*>(programStackPopPointer(program));

    if (owner == NULL || item == NULL) {
        return;
    }

    if (item->owner == NULL) {
        if (itemAdd(owner, item, 1) == 0) {
            Rect rect;
            _obj_disconnect(item, &rect);
            tileWindowRefreshRect(&rect, item->elevation);
        }
    } else {
        scriptPredefinedError(program, "add_obj_to_inven", SCRIPT_ERROR_FOLLOWS);
        debugPrint(" Item was already attached to something else!");
    }
}

// rm_obj_from_inven
// 0x456708
static void opRemoveObjectFromInventory(Program* program)
{
    Object* item = static_cast<Object*>(programStackPopPointer(program));
    Object* owner = static_cast<Object*>(programStackPopPointer(program));

    if (owner == NULL || item == NULL) {
        return;
    }

    bool updateFlags = false;
    int flags = 0;

    if ((item->flags & OBJECT_EQUIPPED) != 0) {
        if ((item->flags & OBJECT_IN_LEFT_HAND) != 0) {
            flags |= OBJECT_IN_LEFT_HAND;
        }

        if ((item->flags & OBJECT_IN_RIGHT_HAND) != 0) {
            flags |= OBJECT_IN_RIGHT_HAND;
        }

        if ((item->flags & OBJECT_WORN) != 0) {
            flags |= OBJECT_WORN;
        }

        updateFlags = true;
    }

    if (itemRemove(owner, item, 1) == 0) {
        Rect rect;
        _obj_connect(item, 1, 0, &rect);
        tileWindowRefreshRect(&rect, item->elevation);

        if (updateFlags) {
            _correctFidForRemovedItem(owner, item, flags);
        }
    }
}

// wield_obj_critter
// 0x45681C
static void opWieldItem(Program* program)
{
    Object* item = static_cast<Object*>(programStackPopPointer(program));
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == NULL) {
        scriptPredefinedError(program, "wield_obj_critter", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (item == NULL) {
        scriptPredefinedError(program, "wield_obj_critter", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        scriptPredefinedError(program, "wield_obj_critter", SCRIPT_ERROR_FOLLOWS);
        debugPrint(" Only works for critters!  ERROR ERROR ERROR!");
        return;
    }

    int hand = HAND_RIGHT;

    bool shouldAdjustArmorClass = false;
    Object* oldArmor = NULL;
    Object* newArmor = NULL;
    if (critter == gDude) {
        if (interfaceGetCurrentHand() == HAND_LEFT) {
            hand = HAND_LEFT;
        }

        if (itemGetType(item) == ITEM_TYPE_ARMOR) {
            oldArmor = critterGetArmor(gDude);

            // SFALL
            shouldAdjustArmorClass = true;
            newArmor = item;
        }
    }

    if (_inven_wield(critter, item, hand) == -1) {
        scriptPredefinedError(program, "wield_obj_critter", SCRIPT_ERROR_FOLLOWS);
        debugPrint(" inven_wield failed!  ERROR ERROR ERROR!");
        return;
    }

    if (critter == gDude) {
        if (shouldAdjustArmorClass) {
            _adjust_ac(critter, oldArmor, newArmor);

            // SFALL
            interfaceRenderArmorClass(false);
        }

        bool animated = !gameUiIsDisabled();
        interfaceUpdateItems(animated, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }
}

// use_obj
// 0x4569D0
static void opUseObject(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        scriptPredefinedError(program, "use_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        // FIXME: Should be SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID.
        scriptPredefinedError(program, "use_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (script->target == NULL) {
        scriptPredefinedError(program, "use_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    Object* self = scriptGetSelf(program);
    if (PID_TYPE(self->pid) == OBJ_TYPE_CRITTER) {
        _action_use_an_object(script->target, object);
    } else {
        _obj_use(self, object);
    }
}

// obj_can_see_obj
// 0x456AC4
static void opObjectCanSeeObject(Program* program)
{
    Object* object2 = static_cast<Object*>(programStackPopPointer(program));
    Object* object1 = static_cast<Object*>(programStackPopPointer(program));

    bool canSee = false;

    if (object2 != nullptr && object1 != nullptr) {
        // SFALL: Check objects are on the same elevation.
        // CE: These checks are on par with |opObjectCanHearObject|.
        if (object2->elevation == object1->elevation) {
            if (object2->tile != -1 && object1->tile != -1) {
                if (isWithinPerception(object1, object2)) {
                    Object* obstacle;
                    _make_straight_path(object1, object1->tile, object2->tile, nullptr, &obstacle, 16);
                    if (obstacle == object2) {
                        canSee = true;
                    }
                }
            }
        }
    } else {
        scriptPredefinedError(program, "obj_can_see_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, canSee);
}

// attack_complex
// 0x456C00
static void opAttackComplex(Program* program)
{
    int data[8];

    for (int arg = 0; arg < 7; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    Object* target = static_cast<Object*>(programStackPopPointer(program));
    if (target == NULL) {
        scriptPredefinedError(program, "attack", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    program->flags |= PROGRAM_FLAG_0x20;

    Object* self = scriptGetSelf(program);
    if (self == NULL) {
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if (!critterIsActive(self) || (self->flags & OBJECT_HIDDEN) != 0) {
        debugPrint("\n   But is already Inactive (Dead/Stunned/Invisible)");
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if (!critterIsActive(target) || (target->flags & OBJECT_HIDDEN) != 0) {
        debugPrint("\n   But target is already dead or invisible");
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if ((target->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0) {
        debugPrint("\n   But target is AFRAID");
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if (_gdialogActive()) {
        // TODO: Might be an error, program flag is not removed.
        return;
    }

    if (isInCombat()) {
        CritterCombatData* combatData = &(self->data.critter.combat);
        if ((combatData->maneuver & CRITTER_MANEUVER_ENGAGING) == 0) {
            combatData->maneuver |= CRITTER_MANEUVER_ENGAGING;
            combatData->whoHitMe = target;
        }
    } else {
        STRUCT_664980 attack;
        attack.attacker = self;
        attack.defender = target;
        attack.actionPointsBonus = 0;
        attack.accuracyBonus = data[4];
        attack.damageBonus = 0;
        attack.minDamage = data[3];
        attack.maxDamage = data[2];

        // TODO: Something is probably broken here, why it wants
        // flags to be the same? Maybe because both of them
        // are applied to defender because of the bug in 0x422F3C?
        if (data[1] == data[0]) {
            attack.field_1C = 1;
            attack.field_24 = data[0];
            attack.field_20 = data[1];
        } else {
            attack.field_1C = 0;
        }

        scriptsRequestCombat(&attack);
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// start_gdialog
// 0x456DF0
static void opStartGameDialog(Program* program)
{
    int backgroundId = programStackPopInteger(program);
    int headId = programStackPopInteger(program);
    int reactionLevel = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    programStackPopInteger(program);

    if (isInCombat()) {
        return;
    }

    if (obj == NULL) {
        scriptPredefinedError(program, "start_gdialog", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    gGameDialogHeadFid = -1;
    if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
        Proto* proto;
        if (protoGetProto(obj->pid, &proto) == -1) {
            return;
        }
    }

    if (headId != -1) {
        gGameDialogHeadFid = buildFid(OBJ_TYPE_HEAD, headId, 0, 0, 0);
    }

    gameDialogSetBackground(backgroundId);
    gGameDialogReactionOrFidget = reactionLevel;

    if (gGameDialogHeadFid != -1) {
        int npcReactionValue = reactionGetValue(gGameDialogSpeaker);
        int npcReactionType = reactionTranslateValue(npcReactionValue);
        switch (npcReactionType) {
        case NPC_REACTION_BAD:
            gGameDialogReactionOrFidget = FIDGET_BAD;
            break;
        case NPC_REACTION_NEUTRAL:
            gGameDialogReactionOrFidget = FIDGET_NEUTRAL;
            break;
        case NPC_REACTION_GOOD:
            gGameDialogReactionOrFidget = FIDGET_GOOD;
            break;
        }
    }

    gGameDialogSid = scriptGetSid(program);
    gGameDialogSpeaker = scriptGetSelf(program);
    _gdialogInitFromScript(gGameDialogHeadFid, gGameDialogReactionOrFidget);
}

// end_dialogue
// 0x456F80
static void opEndGameDialog(Program* program)
{
    if (_gdialogExitFromScript() != -1) {
        gGameDialogSpeaker = NULL;
        gGameDialogSid = -1;
    }
}

// dialogue_reaction
// 0x456FA4
static void opGameDialogReaction(Program* program)
{
    int value = programStackPopInteger(program);

    gGameDialogReactionOrFidget = value;
    _talk_to_critter_reacts(value);
}

// metarule3
// 0x457110
static void opMetarule3(Program* program)
{
    ProgramValue param3 = programStackPopValue(program);
    ProgramValue param2 = programStackPopValue(program);
    ProgramValue param1 = programStackPopValue(program);
    int rule = programStackPopInteger(program);

    ProgramValue result;
    result.opcode = VALUE_TYPE_INT;
    result.integerValue = 0;

    switch (rule) {
    case METARULE3_CLR_FIXED_TIMED_EVENTS:
        if (1) {
            _scrSetQueueTestVals(static_cast<Object*>(param1.pointerValue), param2.integerValue);
            _queue_clear_type(EVENT_TYPE_SCRIPT, _scrQueueRemoveFixed);
        }
        break;
    case METARULE3_MARK_SUBTILE:
        result.integerValue = wmSubTileMarkRadiusVisited(param1.integerValue, param2.integerValue, param3.integerValue);
        break;
    case METARULE3_GET_KILL_COUNT:
        result.integerValue = killsGetByType(param1.integerValue);
        break;
    case METARULE3_MARK_MAP_ENTRANCE:
        result.integerValue = wmMapMarkMapEntranceState(param1.integerValue, param2.integerValue, param3.integerValue);
        break;
    case METARULE3_WM_SUBTILE_STATE:
        if (1) {
            int state;
            if (wmSubTileGetVisitedState(param1.integerValue, param2.integerValue, &state) == 0) {
                result.integerValue = state;
            }
        }
        break;
    case METARULE3_TILE_GET_NEXT_CRITTER:
        if (1) {
            int tile = param1.integerValue;
            int elevation = param2.integerValue;
            Object* previousCritter = static_cast<Object*>(param3.pointerValue);

            bool critterFound = previousCritter == NULL;

            Object* object = objectFindFirstAtLocation(elevation, tile);
            while (object != NULL) {
                if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                    if (critterFound) {
                        result.opcode = VALUE_TYPE_PTR;
                        result.pointerValue = object;
                        break;
                    }
                }

                if (object == previousCritter) {
                    critterFound = true;
                }

                object = objectFindNextAtLocation();
            }
        }
        break;
    case METARULE3_ART_SET_BASE_FID_NUM:
        if (1) {
            Object* obj = static_cast<Object*>(param1.pointerValue);
            int frmId = param2.integerValue;

            int fid = buildFid(FID_TYPE(obj->fid),
                frmId,
                FID_ANIM_TYPE(obj->fid),
                (obj->fid & 0xF000) >> 12,
                (obj->fid & 0x70000000) >> 28);

            Rect updatedRect;
            objectSetFid(obj, fid, &updatedRect);
            tileWindowRefreshRect(&updatedRect, gElevation);
        }
        break;
    case METARULE3_TILE_SET_CENTER:
        result.integerValue = tileSetCenter(param1.integerValue, TILE_SET_CENTER_REFRESH_WINDOW);
        break;
    case METARULE3_109:
        result.integerValue = aiGetChemUse(static_cast<Object*>(param1.pointerValue));
        break;
    case METARULE3_110:
        result.integerValue = wmCarIsOutOfGas() ? 1 : 0;
        break;
    case METARULE3_111:
        result.integerValue = _map_target_load_area();
        break;
    }

    programStackPushValue(program, result);
}

// set_map_music
// 0x45734C
static void opSetMapMusic(Program* program)
{
    char* string = programStackPopString(program);
    int mapIndex = programStackPopInteger(program);

    debugPrint("\nset_map_music: %d, %s", mapIndex, string);
    wmSetMapMusic(mapIndex, string);
}

// NOTE: Function name is a bit misleading. Last parameter is a boolean value
// where 1 or true makes object invisible, and value 0 (false) makes it visible
// again. So a better name for this function is opSetObjectInvisible.
//
//
// set_obj_visibility
// 0x45741C
static void opSetObjectVisibility(Program* program)
{
    int invisible = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj == NULL) {
        scriptPredefinedError(program, "set_obj_visibility", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (_isLoadingGame()) {
        debugPrint("Error: attempt to set_obj_visibility in load/save-game: %s!", program->name);
        return;
    }

    if (invisible != 0) {
        if ((obj->flags & OBJECT_HIDDEN) == 0) {
            if (isInCombat()) {
                objectDisableOutline(obj, NULL);
                objectClearOutline(obj, NULL);
            }

            Rect rect;
            if (objectHide(obj, &rect) != -1) {
                if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
                    obj->flags |= OBJECT_NO_BLOCK;
                }

                tileWindowRefreshRect(&rect, obj->elevation);
            }
        }
    } else {
        if ((obj->flags & OBJECT_HIDDEN) != 0) {
            if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
                obj->flags &= ~OBJECT_NO_BLOCK;
            }

            Rect rect;
            if (objectShow(obj, &rect) != -1) {
                tileWindowRefreshRect(&rect, obj->elevation);
            }
        }
    }
}

// load_map
// 0x45755C
static void opLoadMap(Program* program)
{
    int param = programStackPopInteger(program);
    ProgramValue mapIndexOrName = programStackPopValue(program);

    char* mapName = NULL;

    if ((mapIndexOrName.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        if ((mapIndexOrName.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            mapName = programGetString(program, mapIndexOrName.opcode, mapIndexOrName.integerValue);
        } else {
            programFatalError("script error: %s: invalid arg 1 to load_map", program->name);
        }
    }

    int mapIndex = -1;

    if (mapName != NULL) {
        gGameGlobalVars[GVAR_LOAD_MAP_INDEX] = param;
        mapIndex = wmMapMatchNameToIdx(mapName);
    } else {
        if (mapIndexOrName.integerValue >= 0) {
            gGameGlobalVars[GVAR_LOAD_MAP_INDEX] = param;
            mapIndex = mapIndexOrName.integerValue;
        }
    }

    if (mapIndex != -1) {
        MapTransition transition;
        transition.map = mapIndex;
        transition.elevation = -1;
        transition.tile = -1;
        transition.rotation = -1;
        mapSetTransition(&transition);
    }
}

// wm_area_set_pos
// 0x457680
static void opWorldmapCitySetPos(Program* program)
{
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    int city = programStackPopInteger(program);

    if (wmAreaSetWorldPos(city, x, y) == -1) {
        scriptPredefinedError(program, "wm_area_set_pos", SCRIPT_ERROR_FOLLOWS);
        debugPrint("Invalid Parameter!");
    }
}

// set_exit_grids
// 0x457730
static void opSetExitGrids(Program* program)
{
    int destinationRotation = programStackPopInteger(program);
    int destinationTile = programStackPopInteger(program);
    int destinationElevation = programStackPopInteger(program);
    int destinationMap = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);

    Object* object = objectFindFirstAtElevation(elevation);
    while (object != NULL) {
        if (object->pid >= FIRST_EXIT_GRID_PID && object->pid <= LAST_EXIT_GRID_PID) {
            object->data.misc.map = destinationMap;
            object->data.misc.tile = destinationTile;
            object->data.misc.elevation = destinationElevation;
        }
        object = objectFindNextAtElevation();
    }
}

// anim_busy
// 0x4577EC
static void opAnimBusy(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int rc = 0;
    if (object != NULL) {
        rc = animationIsBusy(object);
    } else {
        scriptPredefinedError(program, "anim_busy", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, rc);
}

// critter_heal
// 0x457880
static void opCritterHeal(Program* program)
{
    int amount = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    int rc = critterAdjustHitPoints(critter, amount);

    if (critter == gDude) {
        interfaceRenderHitPoints(true);
    }

    programStackPushInteger(program, rc);
}

// set_light_level
// 0x457934
static void opSetLightLevel(Program* program)
{
    // Maps light level to light intensity.
    //
    // Middle value is mapped one-to-one which corresponds to 50% light level
    // (cavern lighting). Light levels above (51-100%) and below (0-49%) is
    // calculated as percentage from two adjacent light values.
    //
    // 0x453F90
    static const int intensities[3] = {
        LIGHT_INTENSITY_MIN,
        (LIGHT_INTENSITY_MIN + LIGHT_INTENSITY_MAX) / 2,
        LIGHT_INTENSITY_MAX,
    };

    int data = programStackPopInteger(program);

    int lightLevel = data;

    if (data == 50) {
        lightSetAmbientIntensity(intensities[1], true);
        return;
    }

    int lightIntensity;
    if (data > 50) {
        lightIntensity = intensities[1] + data * (intensities[2] - intensities[1]) / 100;
    } else {
        lightIntensity = intensities[0] + data * (intensities[1] - intensities[0]) / 100;
    }

    lightSetAmbientIntensity(lightIntensity, true);
}

// game_time
// 0x4579F4
static void opGetGameTime(Program* program)
{
    int time = gameTimeGetTime();
    programStackPushInteger(program, time);
}

// game_time_in_seconds
// 0x457A18
static void opGetGameTimeInSeconds(Program* program)
{
    int time = gameTimeGetTime();
    programStackPushInteger(program, time / 10);
}

// elevation
// 0x457A44
static void opGetObjectElevation(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int elevation = 0;
    if (object != NULL) {
        elevation = object->elevation;
    } else {
        scriptPredefinedError(program, "elevation", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, elevation);
}

// kill_critter
// 0x457AD4
static void opKillCritter(Program* program)
{
    int deathFrame = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        scriptPredefinedError(program, "kill_critter", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (_isLoadingGame()) {
        debugPrint("\nError: attempt to destroy critter in load/save-game: %s!", program->name);
    }

    program->flags |= PROGRAM_FLAG_0x20;

    Object* self = scriptGetSelf(program);
    bool isSelf = self == object;

    reg_anim_clear(object);
    _combat_delete_critter(object);
    critterKill(object, deathFrame, 1);

    program->flags &= ~PROGRAM_FLAG_0x20;

    if (isSelf) {
        program->flags |= PROGRAM_FLAG_0x0100;
    }
}

// [forceBack] is to force fall back animation, otherwise it's fall front if it's present
static int _correctDeath(Object* critter, int anim, bool forceBack)
{
    if (anim >= ANIM_BIG_HOLE_SF && anim <= ANIM_FALL_FRONT_BLOOD_SF) {
        bool useStandardDeath = false;
        if (settings.preferences.violence_level < VIOLENCE_LEVEL_MAXIMUM_BLOOD) {
            useStandardDeath = true;
        } else {
            int fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, anim, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
            if (!artExists(fid)) {
                useStandardDeath = true;
            }
        }

        if (useStandardDeath) {
            if (forceBack) {
                anim = ANIM_FALL_BACK;
            } else {
                int fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, ANIM_FALL_FRONT, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
                if (artExists(fid)) {
                    anim = ANIM_FALL_FRONT;
                } else {
                    anim = ANIM_FALL_BACK;
                }
            }
        }
    }

    return anim;
}

// kill_critter_type
// 0x457CB4
static void opKillCritterType(Program* program)
{
    // 0x518ED0
    static const int ftList[] = {
        ANIM_FALL_BACK_BLOOD_SF,
        ANIM_BIG_HOLE_SF,
        ANIM_CHARRED_BODY_SF,
        ANIM_CHUNKS_OF_FLESH_SF,
        ANIM_FALL_FRONT_BLOOD_SF,
        ANIM_FALL_BACK_BLOOD_SF,
        ANIM_DANCING_AUTOFIRE_SF,
        ANIM_SLICED_IN_HALF_SF,
        ANIM_EXPLODED_TO_NOTHING_SF,
        ANIM_FALL_BACK_BLOOD_SF,
        ANIM_FALL_FRONT_BLOOD_SF,
    };

    int deathFrame = programStackPopInteger(program);
    int pid = programStackPopInteger(program);

    if (_isLoadingGame()) {
        debugPrint("\nError: attempt to destroy critter in load/save-game: %s!", program->name);
        return;
    }

    program->flags |= PROGRAM_FLAG_0x20;

    Object* previousObj = NULL;
    int count = 0;
    int ftIndex = 0;

    Object* obj = objectFindFirst();
    while (obj != NULL) {
        if (FID_ANIM_TYPE(obj->fid) < ANIM_FALL_BACK_SF) {
            if ((obj->flags & OBJECT_HIDDEN) == 0 && obj->pid == pid && !critterIsDead(obj)) {
                if (obj == previousObj || count > 200) {
                    scriptPredefinedError(program, "kill_critter_type", SCRIPT_ERROR_FOLLOWS);
                    debugPrint(" Infinite loop destroying critters!");
                    program->flags &= ~PROGRAM_FLAG_0x20;
                    return;
                }

                reg_anim_clear(obj);

                if (deathFrame != 0) {
                    _combat_delete_critter(obj);
                    if (deathFrame == 1) {
                        // Pick next animation from the |ftList|.
                        int anim = _correctDeath(obj, ftList[ftIndex], true);

                        // SFALL: Fix for incorrect death animation.
                        // CE: The fix is slightly different. Sfall passes
                        // |false| to |correctDeath| to disambiguate usage
                        // between this function and |opAnim|. On |false| it
                        // simply returns |ANIM_FALL_BACK_SF|. Instead of doing
                        // the same, check for standard death animations
                        // returned from |correctDeath| and convert them to
                        // appropariate single frame cousins.
                        switch (anim) {
                        case ANIM_FALL_BACK:
                            anim = ANIM_FALL_BACK_SF;
                            break;
                        case ANIM_FALL_FRONT:
                            anim = ANIM_FALL_FRONT_SF;
                            break;
                        }

                        critterKill(obj, anim, true);

                        ftIndex += 1;
                        if (ftIndex >= (sizeof(ftList) / sizeof(ftList[0]))) {
                            ftIndex = 0;
                        }
                    } else if (deathFrame >= FIRST_SF_DEATH_ANIM && deathFrame <= LAST_SF_DEATH_ANIM) {
                        // CE: In some cases user-space scripts randomize death
                        // frame between front/back animations but technically
                        // speaking a scripter can provide any single frame
                        // death animation which original code simply ignores.
                        critterKill(obj, deathFrame, true);
                    } else {
                        critterKill(obj, ANIM_FALL_BACK_SF, true);
                    }
                } else {
                    reg_anim_clear(obj);

                    Rect rect;
                    objectDestroy(obj, &rect);
                    tileWindowRefreshRect(&rect, gElevation);
                }

                previousObj = obj;
                count += 1;

                objectFindFirst();

                gMapHeader.lastVisitTime = gameTimeGetTime();
            }
        }

        obj = objectFindNext();
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// critter_dmg
// 0x457EB4
static void opCritterDamage(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int damageTypeWithFlags = programStackPopInteger(program);
    int amount = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        scriptPredefinedError(program, "critter_damage", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
        scriptPredefinedError(program, "critter_damage", SCRIPT_ERROR_FOLLOWS);
        debugPrint(" Can't call on non-critters!");
        return;
    }

    Object* self = scriptGetSelf(program);
    if (object->data.critter.combat.whoHitMeCid == -1) {
        object->data.critter.combat.whoHitMe = NULL;
    }

    bool animate = (damageTypeWithFlags & 0x200) == 0;
    bool bypassArmor = (damageTypeWithFlags & 0x100) != 0;
    int damageType = damageTypeWithFlags & ~(0x100 | 0x200);
    actionDamage(object->tile, object->elevation, amount, amount, damageType, animate, bypassArmor);

    program->flags &= ~PROGRAM_FLAG_0x20;

    if (self == object) {
        program->flags |= PROGRAM_FLAG_0x0100;
    }
}

// add_timer_event
// 0x457FF0
static void opAddTimerEvent(Program* program)
{
    int param = programStackPopInteger(program);
    int delay = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        scriptError("\nScript Error: %s: op_add_timer_event: pobj is NULL!", program->name);
        return;
    }

    scriptAddTimerEvent(object->sid, delay, param);
}

// rm_timer_event
// 0x458094
static void opRemoveTimerEvent(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        // FIXME: Should be op_rm_timer_event.
        scriptError("\nScript Error: %s: op_add_timer_event: pobj is NULL!");
        return;
    }

    queueRemoveEvents(object);
}

// Converts seconds into game ticks.
//
// game_ticks
// 0x458108
static void opGameTicks(Program* program)
{
    int ticks = programStackPopInteger(program);

    if (ticks < 0) {
        ticks = 0;
    }

    programStackPushInteger(program, ticks * 10);
}

// NOTE: The name of this function is misleading. It has (almost) nothing to do
// with player's "Traits" as a feature. Instead it's used to query many
// information of the critters using passed parameters. It's like "metarule" but
// for critters.
//
// 0x458180
// has_trait
static void opHasTrait(Program* program)
{
    int param = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    int type = programStackPopInteger(program);

    int result = 0;

    if (object != NULL) {
        switch (type) {
        case CRITTER_TRAIT_PERK:
            if (param < PERK_COUNT) {
                result = perkGetRank(object, param);
            } else {
                scriptError("\nScript Error: %s: op_has_trait: Perk out of range", program->name);
            }
            break;
        case CRITTER_TRAIT_OBJECT:
            switch (param) {
            case CRITTER_TRAIT_OBJECT_AI_PACKET:
                if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                    result = object->data.critter.combat.aiPacket;
                }
                break;
            case CRITTER_TRAIT_OBJECT_TEAM:
                if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                    result = object->data.critter.combat.team;
                }
                break;
            case CRITTER_TRAIT_OBJECT_ROTATION:
                result = object->rotation;
                break;
            case CRITTER_TRAIT_OBJECT_IS_INVISIBLE:
                result = (object->flags & OBJECT_HIDDEN) == 0;
                break;
            case CRITTER_TRAIT_OBJECT_GET_INVENTORY_WEIGHT:
                result = objectGetInventoryWeight(object);
                break;
            }
            break;
        case CRITTER_TRAIT_TRAIT:
            if (param < TRAIT_COUNT) {
                result = traitIsSelected(param);
            } else {
                scriptError("\nScript Error: %s: op_has_trait: Trait out of range", program->name);
            }
            break;
        default:
            scriptError("\nScript Error: %s: op_has_trait: Trait out of range", program->name);
            break;
        }
    } else {
        scriptPredefinedError(program, "has_trait", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, result);
}

// obj_can_hear_obj
// 0x45835C
static void opObjectCanHearObject(Program* program)
{
    Object* object2 = static_cast<Object*>(programStackPopPointer(program));
    Object* object1 = static_cast<Object*>(programStackPopPointer(program));

    bool canHear = false;

    // SFALL: Fix broken implementation.
    // CE: In Sfall this fix is available under "ObjCanHearObjFix" switch and
    // it's not enabled by default. Probably needs testing.
    if (object2 != nullptr && object1 != nullptr) {
        if (object2->elevation == object1->elevation) {
            if (object2->tile != -1 && object1->tile != -1) {
                if (isWithinPerception(object1, object2)) {
                    canHear = true;
                }
            }
        }
    }

    programStackPushInteger(program, canHear);
}

// game_time_hour
// 0x458438
static void opGameTimeHour(Program* program)
{
    int value = gameTimeGetHour();
    programStackPushInteger(program, value);
}

// fixed_param
// 0x45845C
static void opGetFixedParam(Program* program)
{
    int fixedParam = 0;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        fixedParam = script->fixedParam;
    } else {
        scriptPredefinedError(program, "fixed_param", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInteger(program, fixedParam);
}

// tile_is_visible
// 0x4584B0
static void opTileIsVisible(Program* program)
{
    int data = programStackPopInteger(program);

    int isVisible = 0;
    if (tileIsVisible(data)) {
        isVisible = 1;
    }

    programStackPushInteger(program, isVisible);
}

// dialogue_system_enter
// 0x458534
static void opGameDialogSystemEnter(Program* program)
{
    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        return;
    }

    Object* self = scriptGetSelf(program);
    if (PID_TYPE(self->pid) == OBJ_TYPE_CRITTER) {
        if (!critterIsActive(self)) {
            return;
        }
    }

    if (isInCombat()) {
        return;
    }

    if (gameRequestState(GAME_STATE_4) == -1) {
        return;
    }

    gGameDialogSpeaker = scriptGetSelf(program);
}

// action_being_used
// 0x458594
static void opGetActionBeingUsed(Program* program)
{
    int action = -1;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        action = script->actionBeingUsed;
    } else {
        scriptPredefinedError(program, "action_being_used", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInteger(program, action);
}

// critter_state
// 0x4585E8
static void opGetCritterState(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    int state = CRITTER_STATE_DEAD;
    if (critter != NULL && PID_TYPE(critter->pid) == OBJ_TYPE_CRITTER) {
        if (critterIsActive(critter)) {
            state = CRITTER_STATE_NORMAL;

            int anim = FID_ANIM_TYPE(critter->fid);
            if (anim >= ANIM_FALL_BACK_SF && anim <= ANIM_FALL_FRONT_SF) {
                state = CRITTER_STATE_PRONE;
            }

            state |= (critter->data.critter.combat.results & DAM_CRIP);
        } else {
            if (!critterIsDead(critter)) {
                state = CRITTER_STATE_PRONE;
            }
        }
    } else {
        scriptPredefinedError(program, "critter_state", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, state);
}

// game_time_advance
// 0x4586C8
static void opGameTimeAdvance(Program* program)
{
    int data = programStackPopInteger(program);

    int days = data / GAME_TIME_TICKS_PER_DAY;
    int remainder = data % GAME_TIME_TICKS_PER_DAY;

    for (int day = 0; day < days; day++) {
        gameTimeAddTicks(GAME_TIME_TICKS_PER_DAY);
        queueProcessEvents();
    }

    gameTimeAddTicks(remainder);
    queueProcessEvents();
}

// radiation_inc
// 0x458760
static void opRadiationIncrease(Program* program)
{
    int amount = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        scriptPredefinedError(program, "radiation_inc", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    critterAdjustRadiation(object, amount);
}

// radiation_dec
// 0x458800
static void opRadiationDecrease(Program* program)
{
    int amount = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        scriptPredefinedError(program, "radiation_dec", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    int radiation = critterGetRadiation(object);
    int adjustment = radiation >= 0 ? -amount : 0;

    critterAdjustRadiation(object, adjustment);
}

// critter_attempt_placement
// 0x4588B4
static void opCritterAttemptPlacement(Program* program)
{
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == NULL) {
        scriptPredefinedError(program, "critter_attempt_placement", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (elevation != critter->elevation && PID_TYPE(critter->pid) == OBJ_TYPE_CRITTER) {
        _combat_delete_critter(critter);
    }

    objectSetLocation(critter, 0, elevation, NULL);

    int rc = _obj_attempt_placement(critter, tile, elevation, 1);
    programStackPushInteger(program, rc);
}

// obj_pid
// 0x4589A0
static void opGetObjectPid(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int pid = -1;
    if (obj) {
        pid = obj->pid;
    } else {
        scriptPredefinedError(program, "obj_pid", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, pid);
}

// cur_map_index
// 0x458A30
static void opGetCurrentMap(Program* program)
{
    int mapIndex = mapGetCurrentMap();
    programStackPushInteger(program, mapIndex);
}

// critter_add_trait
// 0x458A54
static void opCritterAddTrait(Program* program)
{
    int value = programStackPopInteger(program);
    int param = programStackPopInteger(program);
    int kind = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            switch (kind) {
            case CRITTER_TRAIT_PERK:
                if (1) {
                    char* critterName = critterGetName(object);
                    char* perkName = perkGetName(param);
                    debugPrint("\nintextra::critter_add_trait: Adding Perk %s to %s", perkName, critterName);

                    if (value > 0) {
                        if (perkAddForce(object, param) != 0) {
                            scriptError("\nScript Error: %s: op_critter_add_trait: perk_add_force failed", program->name);
                            debugPrint("Perk: %d", param);
                        }
                    } else {
                        if (perkRemove(object, param) != 0) {
                            // FIXME: typo in debug message, should be perk_sub
                            scriptError("\nScript Error: %s: op_critter_add_trait: per_sub failed", program->name);
                            debugPrint("Perk: %d", param);
                        }
                    }

                    if (object == gDude) {
                        interfaceRenderHitPoints(true);
                    }
                }
                break;
            case CRITTER_TRAIT_OBJECT:
                switch (param) {
                case CRITTER_TRAIT_OBJECT_AI_PACKET:
                    critterSetAiPacket(object, value);
                    break;
                case CRITTER_TRAIT_OBJECT_TEAM:
                    if (objectIsPartyMember(object)) {
                        break;
                    }

                    if (object->data.critter.combat.team == value) {
                        break;
                    }

                    if (_isLoadingGame()) {
                        break;
                    }

                    critterSetTeam(object, value);
                    break;
                }
                break;
            default:
                scriptError("\nScript Error: %s: op_critter_add_trait: Trait out of range", program->name);
                break;
            }
        }
    } else {
        scriptPredefinedError(program, "critter_add_trait", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, -1);
}

// critter_rm_trait
// 0x458C2C
static void opCritterRemoveTrait(Program* program)
{
    int value = programStackPopInteger(program);
    int param = programStackPopInteger(program);
    int kind = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        scriptPredefinedError(program, "critter_rm_trait", SCRIPT_ERROR_OBJECT_IS_NULL);
        // FIXME: Ruins stack.
        return;
    }

    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        switch (kind) {
        case CRITTER_TRAIT_PERK:
            while (perkGetRank(object, param) > 0) {
                if (perkRemove(object, param) != 0) {
                    scriptError("\nScript Error: op_critter_rm_trait: perk_sub failed");
                }
            }
            break;
        default:
            scriptError("\nScript Error: %s: op_critter_rm_trait: Trait out of range", program->name);
            break;
        }
    }

    programStackPushInteger(program, -1);
}

// proto_data
// 0x458D38
static void opGetProtoData(Program* program)
{
    int member = programStackPopInteger(program);
    int pid = programStackPopInteger(program);

    ProtoDataMemberValue value;
    value.integerValue = 0;
    int valueType = protoGetDataMember(pid, member, &value);
    switch (valueType) {
    case PROTO_DATA_MEMBER_TYPE_INT:
        programStackPushInteger(program, value.integerValue);
        break;
    case PROTO_DATA_MEMBER_TYPE_STRING:
        programStackPushString(program, value.stringValue);
        break;
    default:
        programStackPushInteger(program, 0);
        break;
    }
}

// message_str
// 0x458E10
static void opGetMessageString(Program* program)
{
    // 0x518EFC
    static char errStr[] = "Error";

    int messageIndex = programStackPopInteger(program);
    int messageListIndex = programStackPopInteger(program);

    char* string;
    if (messageIndex >= 0) {
        string = _scr_get_msg_str_speech(messageListIndex, messageIndex, 1);
        if (string == NULL) {
            debugPrint("\nError: No message file EXISTS!: index %d, line %d", messageListIndex, messageIndex);
            string = errStr;
        }
    } else {
        string = errStr;
    }

    programStackPushString(program, string);
}

// critter_inven_obj
// 0x458F00
static void opCritterGetInventoryObject(Program* program)
{
    int type = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (PID_TYPE(critter->pid) == OBJ_TYPE_CRITTER) {
        switch (type) {
        case INVEN_TYPE_WORN:
            programStackPushPointer(program, critterGetArmor(critter));
            break;
        case INVEN_TYPE_RIGHT_HAND:
            if (critter == gDude) {
                if (interfaceGetCurrentHand() != HAND_LEFT) {
                    programStackPushPointer(program, critterGetItem2(critter));
                } else {
                    programStackPushPointer(program, NULL);
                }
            } else {
                programStackPushPointer(program, critterGetItem2(critter));
            }
            break;
        case INVEN_TYPE_LEFT_HAND:
            if (critter == gDude) {
                if (interfaceGetCurrentHand() == HAND_LEFT) {
                    programStackPushPointer(program, critterGetItem1(critter));
                } else {
                    programStackPushPointer(program, NULL);
                }
            } else {
                programStackPushPointer(program, critterGetItem1(critter));
            }
            break;
        case INVEN_TYPE_INV_COUNT:
            programStackPushInteger(program, critter->data.inventory.length);
            break;
        default:
            scriptError("script error: %s: Error in critter_inven_obj -- wrong type!", program->name);
            programStackPushInteger(program, 0);
            break;
        }
    } else {
        scriptPredefinedError(program, "critter_inven_obj", SCRIPT_ERROR_FOLLOWS);
        debugPrint("  Not a critter!");
        programStackPushInteger(program, 0);
    }
}

// obj_set_light_level
// 0x459088
static void opSetObjectLightLevel(Program* program)
{
    int lightDistance = programStackPopInteger(program);
    int lightIntensity = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        scriptPredefinedError(program, "obj_set_light_level", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    Rect rect;
    if (lightIntensity != 0) {
        if (objectSetLight(object, lightDistance, (lightIntensity * 65636) / 100, &rect) == -1) {
            return;
        }
    } else {
        if (objectSetLight(object, lightDistance, 0, &rect) == -1) {
            return;
        }
    }
    tileWindowRefreshRect(&rect, object->elevation);
}

// 0x459170
static void opWorldmap(Program* program)
{
    scriptsRequestWorldMap();
}

// inven_cmds
// 0x459178
static void _op_inven_cmds(Program* program)
{
    int index = programStackPopInteger(program);
    int cmd = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    Object* item = NULL;

    if (obj != NULL) {
        switch (cmd) {
        case 13:
            item = _inven_index_ptr(obj, index);
            break;
        }
    } else {
        scriptPredefinedError(program, "inven_cmds", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushPointer(program, item);
}

// float_msg
// 0x459280
static void opFloatMessage(Program* program)
{
    int floatingMessageType = programStackPopInteger(program);
    ProgramValue stringValue = programStackPopValue(program);
    char* string = NULL;
    if ((stringValue.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        string = programGetString(program, stringValue.opcode, stringValue.integerValue);
    }
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int color = _colorTable[32747];
    int a5 = _colorTable[0];
    int font = 101;

    if (obj == NULL) {
        scriptPredefinedError(program, "float_msg", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (string == NULL || *string == '\0') {
        textObjectsRemoveByOwner(obj);
        tileWindowRefresh();
        return;
    }

    if (obj->elevation != gElevation) {
        return;
    }

    if (floatingMessageType == FLOATING_MESSAGE_TYPE_COLOR_SEQUENCE) {
        floatingMessageType = _last_color + 1;
        if (floatingMessageType >= FLOATING_MESSAGE_TYPE_COUNT) {
            floatingMessageType = FLOATING_MESSAGE_TYPE_BLACK;
        }
        _last_color = floatingMessageType;
    }

    switch (floatingMessageType) {
    case FLOATING_MESSAGE_TYPE_WARNING:
        color = _colorTable[31744];
        a5 = _colorTable[0];
        font = 103;
        tileSetCenter(gDude->tile, TILE_SET_CENTER_REFRESH_WINDOW);
        break;
    case FLOATING_MESSAGE_TYPE_NORMAL:
    case FLOATING_MESSAGE_TYPE_YELLOW:
        color = _colorTable[32747];
        break;
    case FLOATING_MESSAGE_TYPE_BLACK:
    case FLOATING_MESSAGE_TYPE_PURPLE:
    case FLOATING_MESSAGE_TYPE_GREY:
        color = _colorTable[10570];
        break;
    case FLOATING_MESSAGE_TYPE_RED:
        color = _colorTable[31744];
        break;
    case FLOATING_MESSAGE_TYPE_GREEN:
        color = _colorTable[992];
        break;
    case FLOATING_MESSAGE_TYPE_BLUE:
        color = _colorTable[31];
        break;
    case FLOATING_MESSAGE_TYPE_NEAR_WHITE:
        color = _colorTable[21140];
        break;
    case FLOATING_MESSAGE_TYPE_LIGHT_RED:
        color = _colorTable[32074];
        break;
    case FLOATING_MESSAGE_TYPE_WHITE:
        color = _colorTable[32767];
        break;
    case FLOATING_MESSAGE_TYPE_DARK_GREY:
        color = _colorTable[8456];
        break;
    case FLOATING_MESSAGE_TYPE_LIGHT_GREY:
        color = _colorTable[15855];
        break;
    }

    Rect rect;
    if (textObjectAdd(obj, string, font, color, a5, &rect) != -1) {
        tileWindowRefreshRect(&rect, obj->elevation);
    }
}

// metarule
// 0x4594A0
static void opMetarule(Program* program)
{
    ProgramValue param = programStackPopValue(program);
    int rule = programStackPopInteger(program);

    int result = 0;

    switch (rule) {
    case METARULE_SIGNAL_END_GAME:
        result = 0;
        _game_user_wants_to_quit = 2;
        break;
    case METARULE_FIRST_RUN:
        result = (gMapHeader.flags & MAP_SAVED) == 0;
        break;
    case METARULE_ELEVATOR:
        scriptsRequestElevator(scriptGetSelf(program), param.integerValue);
        result = 0;
        break;
    case METARULE_PARTY_COUNT:
        result = _getPartyMemberCount();
        break;
    case METARULE_AREA_KNOWN:
        result = wmAreaVisitedState(param.integerValue);
        break;
    case METARULE_WHO_ON_DRUGS:
        result = queueHasEvent(static_cast<Object*>(param.pointerValue), EVENT_TYPE_DRUG);
        break;
    case METARULE_MAP_KNOWN:
        result = wmMapIsKnown(param.integerValue);
        break;
    case METARULE_IS_LOADGAME:
        result = _isLoadingGame();
        break;
    case METARULE_CAR_CURRENT_TOWN:
        result = wmCarCurrentArea();
        break;
    case METARULE_GIVE_CAR_TO_PARTY:
        result = wmCarGiveToParty();
        break;
    case METARULE_GIVE_CAR_GAS:
        result = wmCarFillGas(param.integerValue);
        break;
    case METARULE_SKILL_CHECK_TAG:
        result = skillIsTagged(param.integerValue);
        break;
    case METARULE_DROP_ALL_INVEN:
        if (1) {
            Object* object = static_cast<Object*>(param.pointerValue);
            result = itemDropAll(object, object->tile);
            if (gDude == object) {
                interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
                interfaceRenderArmorClass(false);
            }
        }
        break;
    case METARULE_INVEN_UNWIELD_WHO:
        if (1) {
            Object* object = static_cast<Object*>(param.pointerValue);

            int hand = HAND_RIGHT;
            if (object == gDude) {
                if (interfaceGetCurrentHand() == HAND_LEFT) {
                    hand = HAND_LEFT;
                }
            }

            result = _invenUnwieldFunc(object, hand, 0);

            if (object == gDude) {
                bool animated = !gameUiIsDisabled();
                interfaceUpdateItems(animated, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
            } else {
                Object* item = critterGetItem1(object);
                if (itemGetType(item) == ITEM_TYPE_WEAPON) {
                    item->flags &= ~OBJECT_IN_LEFT_HAND;
                }
            }
        }
        break;
    case METARULE_GET_WORLDMAP_XPOS:
        wmGetPartyWorldPos(&result, NULL);
        break;
    case METARULE_GET_WORLDMAP_YPOS:
        wmGetPartyWorldPos(NULL, &result);
        break;
    case METARULE_CURRENT_TOWN:
        if (wmGetPartyCurArea(&result) == -1) {
            debugPrint("\nIntextra: Error: metarule: current_town");
        }
        break;
    case METARULE_LANGUAGE_FILTER:
        result = static_cast<int>(settings.preferences.language_filter);
        break;
    case METARULE_VIOLENCE_FILTER:
        result = settings.preferences.violence_level;
        break;
    case METARULE_WEAPON_DAMAGE_TYPE:
        if (1) {
            Object* object = static_cast<Object*>(param.pointerValue);
            if (PID_TYPE(object->pid) == OBJ_TYPE_ITEM) {
                if (itemGetType(object) == ITEM_TYPE_WEAPON) {
                    result = weaponGetDamageType(NULL, object);
                    break;
                }
            } else {
                if (buildFid(OBJ_TYPE_MISC, 10, 0, 0, 0) == object->fid) {
                    result = DAMAGE_TYPE_EXPLOSION;
                    break;
                }
            }

            scriptPredefinedError(program, "metarule:w_damage_type", SCRIPT_ERROR_FOLLOWS);
            debugPrint("Not a weapon!");
        }
        break;
    case METARULE_CRITTER_BARTERS:
        if (1) {
            Object* object = static_cast<Object*>(param.pointerValue);
            if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                Proto* proto;
                protoGetProto(object->pid, &proto);
                if ((proto->critter.data.flags & CRITTER_BARTER) != 0) {
                    result = 1;
                }
            }
        }
        break;
    case METARULE_CRITTER_KILL_TYPE:
        result = critterGetKillType(static_cast<Object*>(param.pointerValue));
        break;
    case METARULE_SET_CAR_CARRY_AMOUNT:
        if (1) {
            Proto* proto;
            if (protoGetProto(PROTO_ID_CAR_TRUNK, &proto) != -1) {
                proto->item.data.container.maxSize = param.integerValue;
                result = 1;
            }
        }
        break;
    case METARULE_GET_CAR_CARRY_AMOUNT:
        if (1) {
            Proto* proto;
            if (protoGetProto(PROTO_ID_CAR_TRUNK, &proto) != -1) {
                result = proto->item.data.container.maxSize;
            }
        }
        break;
    }

    programStackPushInteger(program, result);
}

// anim
// 0x4598BC
static void opAnim(Program* program)
{
    ProgramValue frameValue = programStackPopValue(program);
    int anim = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    // CE: There is a bug in the `animate_rotation` macro in the user-space
    // sсripts - instead of passing direction, it passes object. The direction
    // argument is thrown away by preprocessor. Original code ignores this bug
    // since there is no distiction between integers and pointers. In addition
    // there is a guard in the code path below which simply ignores any value
    // greater than 6 (so rotation does not change when pointer is passed).
    int frame;
    if (frameValue.opcode == VALUE_TYPE_INT) {
        frame = frameValue.integerValue;
    } else if (anim == 1000 && frameValue.opcode == VALUE_TYPE_PTR) {
        // Force code path below to skip setting rotation.
        frame = ROTATION_COUNT;
    } else {
        programFatalError("script error: %s: invalid arg 2 to anim", program->name);
    }

    if (obj == NULL) {
        scriptPredefinedError(program, "anim", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (anim < ANIM_COUNT) {
        CritterCombatData* combatData = NULL;
        if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
            combatData = &(obj->data.critter.combat);
        }

        anim = _correctDeath(obj, anim, true);

        reg_anim_begin(ANIMATION_REQUEST_UNRESERVED);

        // TODO: Not sure about the purpose, why it handles knock down flag?
        if (frame == 0) {
            animationRegisterAnimate(obj, anim, 0);
            if (anim >= ANIM_FALL_BACK && anim <= ANIM_FALL_FRONT_BLOOD) {
                int fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, anim + 28, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 28);
                animationRegisterSetFid(obj, fid, -1);
            }

            if (combatData != NULL) {
                combatData->results &= DAM_KNOCKED_DOWN;
            }
        } else {
            int fid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 24);
            animationRegisterAnimateReversed(obj, anim, 0);

            if (anim == ANIM_PRONE_TO_STANDING) {
                fid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, ANIM_FALL_FRONT_SF, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 24);
            } else if (anim == ANIM_BACK_TO_STANDING) {
                fid = buildFid(FID_TYPE(obj->fid), obj->fid & 0xFFF, ANIM_FALL_BACK_SF, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 24);
            }

            if (combatData != NULL) {
                combatData->results |= DAM_KNOCKED_DOWN;
            }

            animationRegisterSetFid(obj, fid, -1);
        }

        reg_anim_end();
    } else if (anim == 1000) {
        if (frame < ROTATION_COUNT) {
            Rect rect;
            objectSetRotation(obj, frame, &rect);
            tileWindowRefreshRect(&rect, gElevation);
        }
    } else if (anim == 1010) {
        Rect rect;
        objectSetFrame(obj, frame, &rect);
        tileWindowRefreshRect(&rect, gElevation);
    } else {
        scriptError("\nScript Error: %s: op_anim: anim out of range", program->name);
    }
}

// obj_carrying_pid_obj
// 0x459B5C
static void opObjectCarryingObjectByPid(Program* program)
{
    int pid = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    Object* result = NULL;
    if (object != NULL) {
        result = objectGetCarriedObjectByPid(object, pid);
    } else {
        scriptPredefinedError(program, "obj_carrying_pid_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushPointer(program, result);
}

// reg_anim_func
// 0x459C20
static void opRegAnimFunc(Program* program)
{
    ProgramValue param = programStackPopValue(program);
    int cmd = programStackPopInteger(program);

    if (!isInCombat()) {
        switch (cmd) {
        case OP_REG_ANIM_FUNC_BEGIN:
            reg_anim_begin(param.integerValue);
            break;
        case OP_REG_ANIM_FUNC_CLEAR:
            reg_anim_clear(static_cast<Object*>(param.pointerValue));
            break;
        case OP_REG_ANIM_FUNC_END:
            reg_anim_end();
            break;
        }
    }
}

// reg_anim_animate
// 0x459CD4
static void opRegAnimAnimate(Program* program)
{
    int delay = programStackPopInteger(program);
    int anim = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (anim != 20 || object == NULL || object->pid != 0x100002F || (settings.preferences.violence_level >= 2)) {
            if (object != NULL) {
                animationRegisterAnimate(object, anim, delay);
            } else {
                scriptPredefinedError(program, "reg_anim_animate", SCRIPT_ERROR_OBJECT_IS_NULL);
            }
        }
    }
}

// reg_anim_animate_reverse
// 0x459DC4
static void opRegAnimAnimateReverse(Program* program)
{
    int delay = programStackPopInteger(program);
    int anim = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (object != NULL) {
            animationRegisterAnimateReversed(object, anim, delay);
        } else {
            scriptPredefinedError(program, "reg_anim_animate_reverse", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// reg_anim_obj_move_to_obj
// 0x459E74
static void opRegAnimObjectMoveToObject(Program* program)
{
    int delay = programStackPopInteger(program);
    Object* dest = static_cast<Object*>(programStackPopPointer(program));
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (object != NULL) {
            animationRegisterMoveToObject(object, dest, -1, delay);
        } else {
            scriptPredefinedError(program, "reg_anim_obj_move_to_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// reg_anim_obj_run_to_obj
// 0x459F28
static void opRegAnimObjectRunToObject(Program* program)
{
    int delay = programStackPopInteger(program);
    Object* dest = static_cast<Object*>(programStackPopPointer(program));
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (object != NULL) {
            animationRegisterRunToObject(object, dest, -1, delay);
        } else {
            scriptPredefinedError(program, "reg_anim_obj_run_to_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// reg_anim_obj_move_to_tile
// 0x459FDC
static void opRegAnimObjectMoveToTile(Program* program)
{
    int delay = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (object != NULL) {
            animationRegisterMoveToTile(object, tile, object->elevation, -1, delay);
        } else {
            scriptPredefinedError(program, "reg_anim_obj_move_to_tile", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// reg_anim_obj_run_to_tile
// 0x45A094
static void opRegAnimObjectRunToTile(Program* program)
{
    int delay = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (object != NULL) {
            animationRegisterRunToTile(object, tile, object->elevation, -1, delay);
        } else {
            scriptPredefinedError(program, "reg_anim_obj_run_to_tile", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// play_gmovie
// 0x45A14C
static void opPlayGameMovie(Program* program)
{
    // 0x453F9C
    static const unsigned short flags[MOVIE_COUNT] = {
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
    };

    program->flags |= PROGRAM_FLAG_0x20;

    int movie = programStackPopInteger(program);

    // CE: Disable map updates. Needed to stop animation of objects (dude in
    // particular) when playing movies (the problem can be seen as visual
    // artifacts when playing endgame oilrig explosion).
    bool isoWasDisabled = isoDisable();

    gameDialogDisable();

    if (gameMoviePlay(movie, flags[movie]) == -1) {
        debugPrint("\nError playing movie %d!", movie);
    }

    gameDialogEnable();

    if (isoWasDisabled) {
        isoEnable();
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// add_mult_objs_to_inven
// 0x45A200
static void opAddMultipleObjectsToInventory(Program* program)
{
    int quantity = programStackPopInteger(program);
    Object* item = static_cast<Object*>(programStackPopPointer(program));
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL || item == NULL) {
        return;
    }

    if (quantity < 0) {
        quantity = 1;
    } else if (quantity > 99999) {
        // SFALL
        quantity = 99999;
    }

    if (itemAdd(object, item, quantity) == 0) {
        Rect rect;
        _obj_disconnect(item, &rect);
        tileWindowRefreshRect(&rect, item->elevation);
    }
}

// rm_mult_objs_from_inven
// 0x45A2D4
static void opRemoveMultipleObjectsFromInventory(Program* program)
{
    int quantityToRemove = programStackPopInteger(program);
    Object* item = static_cast<Object*>(programStackPopPointer(program));
    Object* owner = static_cast<Object*>(programStackPopPointer(program));

    if (owner == NULL || item == NULL) {
        scriptPredefinedError(program, "rm_mult_objs_from_inven", SCRIPT_ERROR_OBJECT_IS_NULL);
        programStackPushInteger(program, 0);
        return;
    }

    bool itemWasEquipped = (item->flags & OBJECT_EQUIPPED) != 0;

    int quantity = itemGetQuantity(owner, item);
    if (quantity > quantityToRemove) {
        quantity = quantityToRemove;
    }

    if (quantity != 0) {
        if (itemRemove(owner, item, quantity) == 0) {
            Rect updatedRect;
            _obj_connect(item, 1, 0, &updatedRect);
            if (itemWasEquipped) {
                if (owner == gDude) {
                    bool animated = !gameUiIsDisabled();
                    interfaceUpdateItems(animated, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
                }
            }
        }
    }

    programStackPushInteger(program, quantity);
}

// get_month
// 0x45A40C
static void opGetMonth(Program* program)
{
    int month;
    gameTimeGetDate(&month, NULL, NULL);

    programStackPushInteger(program, month);
}

// get_day
// 0x45A43C
static void opGetDay(Program* program)
{
    int day;
    gameTimeGetDate(NULL, &day, NULL);

    programStackPushInteger(program, day);
}

// explosion
// 0x45A46C
static void opExplosion(Program* program)
{
    int maxDamage = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);

    if (tile == -1) {
        debugPrint("\nError: explosion: bad tile_num!");
        return;
    }

    int minDamage = 1;
    if (maxDamage == 0) {
        minDamage = 0;
    }

    scriptsRequestExplosion(tile, elevation, minDamage, maxDamage);
}

// days_since_visited
// 0x45A528
static void opGetDaysSinceLastVisit(Program* program)
{
    int days;

    if (gMapHeader.lastVisitTime != 0) {
        days = (gameTimeGetTime() - gMapHeader.lastVisitTime) / GAME_TIME_TICKS_PER_DAY;
    } else {
        days = -1;
    }

    programStackPushInteger(program, days);
}

// gsay_start
// 0x45A56C
static void _op_gsay_start(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    if (_gdialogStart() != 0) {
        program->flags &= ~PROGRAM_FLAG_0x20;
        programFatalError("Error starting dialog.");
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// gsay_end
// 0x45A5B0
static void _op_gsay_end(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;
    _gdialogGo();
    program->flags &= ~PROGRAM_FLAG_0x20;
}

// gsay_reply
// 0x45A5D4
static void _op_gsay_reply(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    ProgramValue msg = programStackPopValue(program);
    int messageListId = programStackPopInteger(program);

    if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        char* string = programGetString(program, msg.opcode, msg.integerValue);
        gameDialogSetTextReply(program, messageListId, string);
    } else if (msg.opcode == VALUE_TYPE_INT) {
        gameDialogSetMessageReply(program, messageListId, msg.integerValue);
    } else {
        programFatalError("script error: %s: invalid arg %d to gsay_reply", program->name, 0);
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// gsay_option
// 0x45A6C4
static void _op_gsay_option(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int reaction = programStackPopInteger(program);
    ProgramValue proc = programStackPopValue(program);
    ProgramValue msg = programStackPopValue(program);
    int messageListId = programStackPopInteger(program);

    if ((proc.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        char* procName = programGetString(program, proc.opcode, proc.integerValue);
        if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            const char* string = programGetString(program, msg.opcode, msg.integerValue);
            gameDialogAddTextOptionWithProcIdentifier(messageListId, string, procName, reaction);
        } else if (msg.opcode == VALUE_TYPE_INT) {
            gameDialogAddMessageOptionWithProcIdentifier(messageListId, msg.integerValue, procName, reaction);
        } else {
            programFatalError("script error: %s: invalid arg %d to gsay_option", program->name, 1);
        }
    } else if ((proc.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
        if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            const char* string = programGetString(program, msg.opcode, msg.integerValue);
            gameDialogAddTextOptionWithProc(messageListId, string, proc.integerValue, reaction);
        } else if (msg.opcode == VALUE_TYPE_INT) {
            gameDialogAddMessageOptionWithProc(messageListId, msg.integerValue, proc.integerValue, reaction);
        } else {
            programFatalError("script error: %s: invalid arg %d to gsay_option", program->name, 1);
        }
    } else {
        programFatalError("Invalid arg 3 to sayOption");
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// gsay_message
// 0x45A8AC
static void _op_gsay_message(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int reaction = programStackPopInteger(program);
    ProgramValue msg = programStackPopValue(program);
    int messageListId = programStackPopInteger(program);

    if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        char* string = programGetString(program, msg.opcode, msg.integerValue);
        gameDialogSetTextReply(program, messageListId, string);
    } else if (msg.opcode == VALUE_TYPE_INT) {
        gameDialogSetMessageReply(program, messageListId, msg.integerValue);
    } else {
        programFatalError("script error: %s: invalid arg %d to gsay_message", program->name, 1);
    }

    gameDialogAddMessageOptionWithProcIdentifier(-2, -2, NULL, 50);
    _gdialogSayMessage();

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// giq_option
// 0x45A9B4
static void _op_giq_option(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int reaction = programStackPopInteger(program);
    ProgramValue proc = programStackPopValue(program);
    ProgramValue msg = programStackPopValue(program);
    int messageListId = programStackPopInteger(program);
    int iq = programStackPopInteger(program);

    int intelligence = critterGetStat(gDude, STAT_INTELLIGENCE);
    intelligence += perkGetRank(gDude, PERK_SMOOTH_TALKER);

    if (iq < 0) {
        if (-intelligence < iq) {
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }
    } else {
        if (intelligence < iq) {
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }
    }

    if ((proc.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        char* procName = programGetString(program, proc.opcode, proc.integerValue);
        if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            char* string = programGetString(program, msg.opcode, msg.integerValue);
            gameDialogAddTextOptionWithProcIdentifier(messageListId, string, procName, reaction);
        } else if (msg.opcode == VALUE_TYPE_INT) {
            gameDialogAddMessageOptionWithProcIdentifier(messageListId, msg.integerValue, procName, reaction);
        } else {
            programFatalError("script error: %s: invalid arg %d to giq_option", program->name, 1);
        }
    } else if (proc.opcode == VALUE_TYPE_INT) {
        if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            char* string = programGetString(program, msg.opcode, msg.integerValue);
            gameDialogAddTextOptionWithProc(messageListId, string, proc.integerValue, reaction);
        } else if (msg.opcode == VALUE_TYPE_INT) {
            gameDialogAddMessageOptionWithProc(messageListId, msg.integerValue, proc.integerValue, reaction);
        } else {
            programFatalError("script error: %s: invalid arg %d to giq_option", program->name, 1);
        }
    } else {
        programFatalError("script error: %s: invalid arg %d to giq_option", program->name, 3);
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// poison
// 0x45AB90
static void opPoison(Program* program)
{
    int amount = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj == NULL) {
        scriptPredefinedError(program, "poison", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (critterAdjustPoison(obj, amount) != 0) {
        debugPrint("\nScript Error: poison: adjust failed!");
    }
}

// get_poison
// 0x45AC44
static void opGetPoison(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int poison = 0;
    if (obj != NULL) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
            poison = critterGetPoison(obj);
        } else {
            debugPrint("\nScript Error: get_poison: who is not a critter!");
        }
    } else {
        scriptPredefinedError(program, "get_poison", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, poison);
}

// party_add
// 0x45ACF4
static void opPartyAdd(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    if (object == NULL) {
        scriptPredefinedError(program, "party_add", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    partyMemberAdd(object);
}

// party_remove
// 0x45AD68
static void opPartyRemove(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    if (object == NULL) {
        scriptPredefinedError(program, "party_remove", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    partyMemberRemove(object);
}

// reg_anim_animate_forever
// 0x45ADDC
static void opRegAnimAnimateForever(Program* program)
{
    int anim = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (obj != NULL) {
            animationRegisterAnimateForever(obj, anim, -1);
        } else {
            scriptPredefinedError(program, "reg_anim_animate_forever", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// critter_injure
// 0x45AE8C
static void opCritterInjure(Program* program)
{
    int flags = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == NULL) {
        scriptPredefinedError(program, "critter_injure", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    bool reverse = (flags & DAM_PERFORM_REVERSE) != 0;

    flags &= DAM_CRIP;

    if (reverse) {
        critter->data.critter.combat.results &= ~flags;
    } else {
        critter->data.critter.combat.results |= flags;
    }

    if (critter == gDude) {
        if ((flags & DAM_CRIP_ARM_ANY) != 0) {
            int leftItemAction;
            int rightItemAction;
            interfaceGetItemActions(&leftItemAction, &rightItemAction);
            interfaceUpdateItems(true, leftItemAction, rightItemAction);
        }
    }
}

// combat_is_initialized
// 0x45AF7C
static void opCombatIsInitialized(Program* program)
{
    programStackPushInteger(program, isInCombat() ? 1 : 0);
}

// gdialog_barter
// 0x45AFA0
static void _op_gdialog_barter(Program* program)
{
    int data = programStackPopInteger(program);

    if (gameDialogBarter(data) == -1) {
        debugPrint("\nScript Error: gdialog_barter: failed");
    }
}

// difficulty_level
// 0x45B010
static void opGetGameDifficulty(Program* program)
{
    programStackPushInteger(program, settings.preferences.game_difficulty);
}

// running_burning_guy
// 0x45B05C
static void opGetRunningBurningGuy(Program* program)
{
    programStackPushInteger(program, static_cast<int>(settings.preferences.running_burning_guy));
}

// inven_unwield
static void _op_inven_unwield(Program* program)
{
    Object* obj;
    int v1;

    obj = scriptGetSelf(program);
    v1 = 1;

    if (obj == gDude && !interfaceGetCurrentHand()) {
        v1 = 0;
    }

    _inven_unwield(obj, v1);
}

// obj_is_locked
// 0x45B0D8
static void opObjectIsLocked(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    bool locked = false;
    if (object != NULL) {
        locked = objectIsLocked(object);
    } else {
        scriptPredefinedError(program, "obj_is_locked", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, locked ? 1 : 0);
}

// obj_lock
// 0x45B16C
static void opObjectLock(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        objectLock(object);
    } else {
        scriptPredefinedError(program, "obj_lock", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// obj_unlock
// 0x45B1E0
static void opObjectUnlock(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        objectUnlock(object);
    } else {
        scriptPredefinedError(program, "obj_unlock", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// obj_is_open
// 0x45B254
static void opObjectIsOpen(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    bool isOpen = false;
    if (object != NULL) {
        isOpen = objectIsOpen(object);
    } else {
        scriptPredefinedError(program, "obj_is_open", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, isOpen ? 1 : 0);
}

// obj_open
// 0x45B2E8
static void opObjectOpen(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        objectOpen(object);
    } else {
        scriptPredefinedError(program, "obj_open", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// obj_close
// 0x45B35C
static void opObjectClose(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        objectClose(object);
    } else {
        scriptPredefinedError(program, "obj_close", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// game_ui_disable
// 0x45B3D0
static void opGameUiDisable(Program* program)
{
    gameUiDisable(0);
}

// game_ui_enable
// 0x45B3D8
static void opGameUiEnable(Program* program)
{
    gameUiEnable();
}

// game_ui_is_disabled
// 0x45B3E0
static void opGameUiIsDisabled(Program* program)
{
    programStackPushInteger(program, gameUiIsDisabled() ? 1 : 0);
}

// gfade_out
// 0x45B404
static void opGameFadeOut(Program* program)
{
    int data = programStackPopInteger(program);

    if (data != 0) {
        paletteFadeTo(gPaletteBlack);
    } else {
        scriptPredefinedError(program, "gfade_out", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// gfade_in
// 0x45B47C
static void opGameFadeIn(Program* program)
{
    int data = programStackPopInteger(program);

    if (data != 0) {
        paletteFadeTo(_cmap);
    } else {
        scriptPredefinedError(program, "gfade_in", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// item_caps_total
// 0x45B4F4
static void opItemCapsTotal(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int amount = 0;
    if (object != NULL) {
        amount = itemGetTotalCaps(object);
    } else {
        scriptPredefinedError(program, "item_caps_total", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, amount);
}

// item_caps_adjust
// 0x45B588
static void opItemCapsAdjust(Program* program)
{
    int amount = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int rc = -1;

    if (object != NULL) {
        rc = itemCapsAdjust(object, amount);
    } else {
        scriptPredefinedError(program, "item_caps_adjust", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, rc);
}

// anim_action_frame
static void _op_anim_action_frame(Program* program)
{
    int anim = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int actionFrame = 0;

    if (object != NULL) {
        int fid = buildFid(FID_TYPE(object->fid), object->fid & 0xFFF, anim, 0, object->rotation);
        CacheEntry* frmHandle;
        Art* frm = artLock(fid, &frmHandle);
        if (frm != NULL) {
            actionFrame = artGetActionFrame(frm);
            artUnlock(frmHandle);
        }
    } else {
        scriptPredefinedError(program, "anim_action_frame", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, actionFrame);
}

// reg_anim_play_sfx
// 0x45B740
static void opRegAnimPlaySfx(Program* program)
{
    int delay = programStackPopInteger(program);
    char* soundEffectName = programStackPopString(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (soundEffectName == NULL) {
        scriptPredefinedError(program, "reg_anim_play_sfx", SCRIPT_ERROR_FOLLOWS);
        debugPrint(" Can't match string!");
    }

    if (obj != NULL) {
        animationRegisterPlaySoundEffect(obj, soundEffectName, delay);
    } else {
        scriptPredefinedError(program, "reg_anim_play_sfx", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// critter_mod_skill
// 0x45B840
static void opCritterModifySkill(Program* program)
{
    int points = programStackPopInteger(program);
    int skill = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter != NULL && points != 0) {
        if (PID_TYPE(critter->pid) == OBJ_TYPE_CRITTER) {
            if (critter == gDude) {
                int normalizedPoints = abs(points);
                if (skillIsTagged(skill)) {
                    // Halve number of skill points. Increment/decrement skill
                    // points routines handle that.
                    normalizedPoints /= 2;
                }

                if (points > 0) {
                    // Increment skill points one by one.
                    for (int it = 0; it < normalizedPoints; it++) {
                        skillAddForce(gDude, skill);
                    }
                } else {
                    // Decrement skill points one by one.
                    for (int it = 0; it < normalizedPoints; it++) {
                        skillSubForce(gDude, skill);
                    }
                }

                // TODO: Checking for critter is dude twice probably means this
                // is inlined function.
                if (critter == gDude) {
                    int leftItemAction;
                    int rightItemAction;
                    interfaceGetItemActions(&leftItemAction, &rightItemAction);
                    interfaceUpdateItems(false, leftItemAction, rightItemAction);
                }
            } else {
                scriptPredefinedError(program, "critter_mod_skill", SCRIPT_ERROR_FOLLOWS);
                debugPrint(" Can't modify anyone except obj_dude!");
            }
        }
    } else {
        scriptPredefinedError(program, "critter_mod_skill", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, 0);
}

// sfx_build_char_name
// 0x45B9C4
static void opSfxBuildCharName(Program* program)
{
    int extra = programStackPopInteger(program);
    int anim = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj != NULL) {
        char soundEffectName[16];
        strcpy(soundEffectName, sfxBuildCharName(obj, anim, extra));
        programStackPushString(program, soundEffectName);
    } else {
        scriptPredefinedError(program, "sfx_build_char_name", SCRIPT_ERROR_OBJECT_IS_NULL);
        programStackPushString(program, NULL);
    }
}

// sfx_build_ambient_name
// 0x45BAA8
static void opSfxBuildAmbientName(Program* program)
{
    char* baseName = programStackPopString(program);

    char soundEffectName[16];
    strcpy(soundEffectName, gameSoundBuildAmbientSoundEffectName(baseName));
    programStackPushString(program, soundEffectName);
}

// sfx_build_interface_name
// 0x45BB54
static void opSfxBuildInterfaceName(Program* program)
{
    char* baseName = programStackPopString(program);

    char soundEffectName[16];
    strcpy(soundEffectName, gameSoundBuildInterfaceName(baseName));
    programStackPushString(program, soundEffectName);
}

// sfx_build_item_name
// 0x45BC00
static void opSfxBuildItemName(Program* program)
{
    const char* baseName = programStackPopString(program);

    char soundEffectName[16];
    strcpy(soundEffectName, gameSoundBuildInterfaceName(baseName));
    programStackPushString(program, soundEffectName);
}

// sfx_build_weapon_name
// 0x45BCAC
static void opSfxBuildWeaponName(Program* program)
{
    Object* target = static_cast<Object*>(programStackPopPointer(program));
    int hitMode = programStackPopInteger(program);
    Object* weapon = static_cast<Object*>(programStackPopPointer(program));
    int weaponSfxType = programStackPopInteger(program);

    char soundEffectName[16];
    strcpy(soundEffectName, sfxBuildWeaponName(weaponSfxType, weapon, hitMode, target));
    programStackPushString(program, soundEffectName);
}

// sfx_build_scenery_name
// 0x45BD7C
static void opSfxBuildSceneryName(Program* program)
{
    int actionType = programStackPopInteger(program);
    int action = programStackPopInteger(program);
    char* baseName = programStackPopString(program);

    char soundEffectName[16];
    strcpy(soundEffectName, sfxBuildSceneryName(actionType, action, baseName));
    programStackPushString(program, soundEffectName);
}

// sfx_build_open_name
// 0x45BE58
static void opSfxBuildOpenName(Program* program)
{
    int action = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        char soundEffectName[16];
        strcpy(soundEffectName, sfxBuildOpenName(object, action));
        programStackPushString(program, soundEffectName);
    } else {
        scriptPredefinedError(program, "sfx_build_open_name", SCRIPT_ERROR_OBJECT_IS_NULL);
        programStackPushString(program, NULL);
    }
}

// attack_setup
// 0x45BF38
static void opAttackSetup(Program* program)
{
    Object* defender = static_cast<Object*>(programStackPopPointer(program));
    Object* attacker = static_cast<Object*>(programStackPopPointer(program));

    program->flags |= PROGRAM_FLAG_0x20;

    if (attacker != NULL) {
        if (!critterIsActive(attacker) || (attacker->flags & OBJECT_HIDDEN) != 0) {
            debugPrint("\n   But is already dead or invisible");
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }

        if (!critterIsActive(defender) || (defender->flags & OBJECT_HIDDEN) != 0) {
            debugPrint("\n   But target is already dead or invisible");
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }

        if ((defender->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0) {
            debugPrint("\n   But target is AFRAID");
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }

        if (isInCombat()) {
            if ((attacker->data.critter.combat.maneuver & CRITTER_MANEUVER_ENGAGING) == 0) {
                attacker->data.critter.combat.maneuver |= CRITTER_MANEUVER_ENGAGING;
                attacker->data.critter.combat.whoHitMe = defender;
            }
        } else {
            STRUCT_664980 attack;
            attack.attacker = attacker;
            attack.defender = defender;
            attack.actionPointsBonus = 0;
            attack.accuracyBonus = 0;
            attack.damageBonus = 0;
            attack.minDamage = 0;
            attack.maxDamage = INT_MAX;
            attack.field_1C = 0;

            scriptsRequestCombat(&attack);
        }
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// destroy_mult_objs
// 0x45C0E8
static void opDestroyMultipleObjects(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int quantity = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    Object* self = scriptGetSelf(program);
    bool isSelf = self == object;

    int result = 0;

    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        _combat_delete_critter(object);
    }

    Object* owner = objectGetOwner(object);
    if (owner != NULL) {
        int quantityToDestroy = itemGetQuantity(owner, object);
        if (quantityToDestroy > quantity) {
            quantityToDestroy = quantity;
        }

        itemRemove(owner, object, quantityToDestroy);

        if (owner == gDude) {
            bool animated = !gameUiIsDisabled();
            interfaceUpdateItems(animated, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
        }

        _obj_connect(object, 1, 0, NULL);

        if (isSelf) {
            object->sid = -1;
            object->flags |= (OBJECT_HIDDEN | OBJECT_NO_SAVE);
        } else {
            reg_anim_clear(object);
            objectDestroy(object, NULL);
        }

        result = quantityToDestroy;
    } else {
        reg_anim_clear(object);

        Rect rect;
        objectDestroy(object, &rect);
        tileWindowRefreshRect(&rect, gElevation);
    }

    programStackPushInteger(program, result);

    program->flags &= ~PROGRAM_FLAG_0x20;

    if (isSelf) {
        program->flags |= PROGRAM_FLAG_0x0100;
    }
}

// use_obj_on_obj
// 0x45C290
static void opUseObjectOnObject(Program* program)
{
    Object* target = static_cast<Object*>(programStackPopPointer(program));
    Object* item = static_cast<Object*>(programStackPopPointer(program));

    if (item == NULL) {
        scriptPredefinedError(program, "use_obj_on_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (target == NULL) {
        scriptPredefinedError(program, "use_obj_on_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    Script* script;
    int sid = scriptGetSid(program);
    if (scriptGetScript(sid, &script) == -1) {
        // FIXME: Should be SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID.
        scriptPredefinedError(program, "use_obj_on_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    Object* self = scriptGetSelf(program);

    // SFALL: Override `self` via `op_set_self`.
    // CE: Implementation is different. Sfall integrates via `scriptGetSid` by
    // returning fake script with overridden `self`.
    if (script->overriddenSelf != nullptr) {
        self = script->overriddenSelf;
        script->overriddenSelf = nullptr;
    }

    if (PID_TYPE(self->pid) == OBJ_TYPE_CRITTER) {
        _action_use_an_item_on_object(self, target, item);
    } else {
        _obj_use_item_on(self, target, item);
    }
}

// endgame_slideshow
// 0x45C3B0
static void opEndgameSlideshow(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;
    scriptsRequestEndgame();
    program->flags &= ~PROGRAM_FLAG_0x20;
}

// move_obj_inven_to_obj
// 0x45C3D0
static void opMoveObjectInventoryToObject(Program* program)
{
    Object* object2 = static_cast<Object*>(programStackPopPointer(program));
    Object* object1 = static_cast<Object*>(programStackPopPointer(program));

    if (object1 == NULL) {
        scriptPredefinedError(program, "move_obj_inven_to_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (object2 == NULL) {
        scriptPredefinedError(program, "move_obj_inven_to_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    Object* oldArmor = NULL;
    Object* item2 = NULL;
    if (object1 == gDude) {
        oldArmor = critterGetArmor(object1);
    } else {
        item2 = critterGetItem2(object1);
    }

    if (object1 != gDude && item2 != NULL) {
        int flags = 0;
        if ((item2->flags & 0x01000000) != 0) {
            flags |= 0x01000000;
        }

        if ((item2->flags & 0x02000000) != 0) {
            flags |= 0x02000000;
        }

        _correctFidForRemovedItem(object1, item2, flags);
    }

    itemMoveAll(object1, object2);

    if (object1 == gDude) {
        if (oldArmor != NULL) {
            _adjust_ac(gDude, oldArmor, NULL);
        }

        _proto_dude_update_gender();

        bool animated = !gameUiIsDisabled();
        interfaceUpdateItems(animated, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }
}

// endgame_movie
// 0x45C54C
static void opEndgameMovie(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;
    endgamePlayMovie();
    program->flags &= ~PROGRAM_FLAG_0x20;
}

// obj_art_fid
// 0x45C56C
static void opGetObjectFid(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int fid = 0;
    if (object != NULL) {
        fid = object->fid;
    } else {
        scriptPredefinedError(program, "obj_art_fid", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, fid);
}

// art_anim
// 0x45C5F8
static void opGetFidAnim(Program* program)
{
    int data = programStackPopInteger(program);
    programStackPushInteger(program, (data & 0xFF0000) >> 16);
}

// party_member_obj
// 0x45C66C
static void opGetPartyMember(Program* program)
{
    int data = programStackPopInteger(program);

    Object* object = partyMemberFindByPid(data);
    programStackPushPointer(program, object);
}

// rotation_to_tile
// 0x45C6DC
static void opGetRotationToTile(Program* program)
{
    // CE: There is a bug in Olympus (tgrdqest) - object is passed as one of the
    // arguments instead of tile. Original game (x86) does not distinguish
    // between integers and pointers, so one of the tiles is silently ignored
    // while calculating rotation. As a workaround this opcode now accepts
    // both integers and objects.
    ProgramValue value2 = programStackPopValue(program);
    ProgramValue value1 = programStackPopValue(program);

    int tile2;
    if (value2.isInt()) {
        tile2 = value2.integerValue;
    } else if (value2.isPointer()) {
        tile2 = static_cast<Object*>(value2.pointerValue)->tile;
    } else {
        programFatalError("script error: %s: invalid arg 2 to rotation_to_tile", program->name);
    }

    int tile1;
    if (value1.isInt()) {
        tile1 = value1.integerValue;
    } else if (value1.isPointer()) {
        tile1 = static_cast<Object*>(value1.pointerValue)->tile;
    } else {
        programFatalError("script error: %s: invalid arg 1 to rotation_to_tile", program->name);
    }

    int rotation = tileGetRotationTo(tile1, tile2);
    programStackPushInteger(program, rotation);
}

// jam_lock
// 0x45C778
static void opJamLock(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    objectJamLock(object);
}

// gdialog_set_barter_mod
// 0x45C7D4
static void opGameDialogSetBarterMod(Program* program)
{
    int data = programStackPopInteger(program);

    gameDialogSetBarterModifier(data);
}

// combat_difficulty
// 0x45C830
static void opGetCombatDifficulty(Program* program)
{
    programStackPushInteger(program, settings.preferences.combat_difficulty);
}

// obj_on_screen
// 0x45C878
static void opObjectOnScreen(Program* program)
{
    Rect rect;
    rectCopy(&rect, &stru_453FC0);

    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int result = 0;

    if (object != NULL) {
        if (gElevation == object->elevation) {
            Rect objectRect;
            objectGetRect(object, &objectRect);

            if (rectIntersection(&objectRect, &rect, &objectRect) == 0) {
                result = 1;
            }
        }
    } else {
        scriptPredefinedError(program, "obj_on_screen", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, result);
}

// critter_is_fleeing
// 0x45C93C
static void opCritterIsFleeing(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    bool fleeing = false;
    if (obj != NULL) {
        fleeing = (obj->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0;
    } else {
        scriptPredefinedError(program, "critter_is_fleeing", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, fleeing ? 1 : 0);
}

// critter_set_flee_state
// 0x45C9DC
static void opCritterSetFleeState(Program* program)
{
    int fleeing = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        if (fleeing != 0) {
            object->data.critter.combat.maneuver |= CRITTER_MANUEVER_FLEEING;
        } else {
            object->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;
        }
    } else {
        scriptPredefinedError(program, "critter_set_flee_state", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// terminate_combat
// 0x45CA84
static void opTerminateCombat(Program* program)
{
    if (isInCombat()) {
        _game_user_wants_to_quit = 1;
        Object* self = scriptGetSelf(program);
        if (self != NULL) {
            if (PID_TYPE(self->pid) == OBJ_TYPE_CRITTER) {
                self->data.critter.combat.maneuver |= CRITTER_MANEUVER_DISENGAGING;
                self->data.critter.combat.whoHitMe = NULL;
                aiInfoSetLastTarget(self, NULL);
            }
        }
    }
}

// debug_msg
// 0x45CAC8
static void opDebugMessage(Program* program)
{
    char* string = programStackPopString(program);

    if (string != NULL) {
        if (settings.debug.show_script_messages) {
            debugPrint("\n");
            debugPrint(string);
        }
    }
}

// critter_stop_attacking
// 0x45CB70
static void opCritterStopAttacking(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj != NULL) {
        obj->data.critter.combat.maneuver |= CRITTER_MANEUVER_DISENGAGING;
        obj->data.critter.combat.whoHitMe = NULL;
        aiInfoSetLastTarget(obj, NULL);
    } else {
        scriptPredefinedError(program, "critter_stop_attacking", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// tile_contains_pid_obj
// 0x45CBF8
static void opTileGetObjectWithPid(Program* program)
{
    int pid = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    Object* found = NULL;

    if (tile != -1) {
        Object* object = objectFindFirstAtLocation(elevation, tile);
        while (object != NULL) {
            if (object->pid == pid) {
                found = object;
                break;
            }
            object = objectFindNextAtLocation();
        }
    }

    programStackPushPointer(program, found);
}

// obj_name
// 0x45CCC8
static void opGetObjectName(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj != NULL) {
        _strName = objectGetName(obj);
    } else {
        scriptPredefinedError(program, "obj_name", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushString(program, _strName);
}

// get_pc_stat
// 0x45CD64
static void opGetPcStat(Program* program)
{
    int data = programStackPopInteger(program);

    int value = pcGetStat(data);
    programStackPushInteger(program, value);
}

// 0x45CDD4
void _intExtraClose_()
{
    sfallOpcodesExit();
}

// 0x45CDD8
void _initIntExtra()
{
    interpreterRegisterOpcode(0x80A1, opGiveExpPoints); // op_give_exp_points
    interpreterRegisterOpcode(0x80A2, opScrReturn); // op_scr_return
    interpreterRegisterOpcode(0x80A3, opPlaySfx); // op_play_sfx
    interpreterRegisterOpcode(0x80A4, opGetObjectName); // op_obj_name
    interpreterRegisterOpcode(0x80A5, opSfxBuildOpenName); // op_sfx_build_open_name
    interpreterRegisterOpcode(0x80A6, opGetPcStat); // op_get_pc_stat
    interpreterRegisterOpcode(0x80A7, opTileGetObjectWithPid); // op_tile_contains_pid_obj
    interpreterRegisterOpcode(0x80A8, opSetMapStart); // op_set_map_start
    interpreterRegisterOpcode(0x80A9, opOverrideMapStart); // op_override_map_start
    interpreterRegisterOpcode(0x80AA, opHasSkill); // op_has_skill
    interpreterRegisterOpcode(0x80AB, opUsingSkill); // op_using_skill
    interpreterRegisterOpcode(0x80AC, opRollVsSkill); // op_roll_vs_skill
    interpreterRegisterOpcode(0x80AD, opSkillContest); // op_skill_contest
    interpreterRegisterOpcode(0x80AE, opDoCheck); // op_do_check
    interpreterRegisterOpcode(0x80AF, opSuccess); // op_success
    interpreterRegisterOpcode(0x80B0, opCritical); // op_critical
    interpreterRegisterOpcode(0x80B1, opHowMuch); // op_how_much
    interpreterRegisterOpcode(0x80B2, opMarkAreaKnown); // op_mark_area_known
    interpreterRegisterOpcode(0x80B3, opReactionInfluence); // op_reaction_influence
    interpreterRegisterOpcode(0x80B4, opRandom); // op_random
    interpreterRegisterOpcode(0x80B5, opRollDice); // op_roll_dice
    interpreterRegisterOpcode(0x80B6, opMoveTo); // op_move_to
    interpreterRegisterOpcode(0x80B7, opCreateObject); // op_create_object
    interpreterRegisterOpcode(0x80B8, opDisplayMsg); // op_display_msg
    interpreterRegisterOpcode(0x80B9, opScriptOverrides); // op_script_overrides
    interpreterRegisterOpcode(0x80BA, opObjectIsCarryingObjectWithPid); // op_obj_is_carrying_obj
    interpreterRegisterOpcode(0x80BB, opTileContainsObjectWithPid); // op_tile_contains_obj_pid
    interpreterRegisterOpcode(0x80BC, opGetSelf); // op_self_obj
    interpreterRegisterOpcode(0x80BD, opGetSource); // op_source_obj
    interpreterRegisterOpcode(0x80BE, opGetTarget); // op_target_obj
    interpreterRegisterOpcode(0x80BF, opGetDude);
    interpreterRegisterOpcode(0x80C0, opGetObjectBeingUsed); // op_obj_being_used_with
    interpreterRegisterOpcode(0x80C1, opGetLocalVar); // op_get_local_var
    interpreterRegisterOpcode(0x80C2, opSetLocalVar); // op_set_local_var
    interpreterRegisterOpcode(0x80C3, opGetMapVar); // op_get_map_var
    interpreterRegisterOpcode(0x80C4, opSetMapVar); // op_set_map_var
    interpreterRegisterOpcode(0x80C5, opGetGlobalVar); // op_get_global_var
    interpreterRegisterOpcode(0x80C6, opSetGlobalVar); // op_set_global_var
    interpreterRegisterOpcode(0x80C7, opGetScriptAction); // op_script_action
    interpreterRegisterOpcode(0x80C8, opGetObjectType); // op_obj_type
    interpreterRegisterOpcode(0x80C9, opGetItemType); // op_item_subtype
    interpreterRegisterOpcode(0x80CA, opGetCritterStat); // op_get_critter_stat
    interpreterRegisterOpcode(0x80CB, opSetCritterStat); // op_set_critter_stat
    interpreterRegisterOpcode(0x80CC, opAnimateStand); // op_animate_stand_obj
    interpreterRegisterOpcode(0x80CD, opAnimateStandReverse); // animate_stand_reverse_obj
    interpreterRegisterOpcode(0x80CE, opAnimateMoveObjectToTile); // animate_move_obj_to_tile
    interpreterRegisterOpcode(0x80CF, opTileInTileRect); // tile_in_tile_rect
    interpreterRegisterOpcode(0x80D0, opAttackComplex); // op_attack
    interpreterRegisterOpcode(0x80D1, opMakeDayTime); // op_make_daytime
    interpreterRegisterOpcode(0x80D2, opTileDistanceBetween); // op_tile_distance
    interpreterRegisterOpcode(0x80D3, opTileDistanceBetweenObjects); // op_tile_distance_objs
    interpreterRegisterOpcode(0x80D4, opGetObjectTile); // op_tile_num
    interpreterRegisterOpcode(0x80D5, opGetTileInDirection); // op_tile_num_in_direction
    interpreterRegisterOpcode(0x80D6, opPickup); // op_pickup_obj
    interpreterRegisterOpcode(0x80D7, opDrop); // op_drop_obj
    interpreterRegisterOpcode(0x80D8, opAddObjectToInventory); // op_add_obj_to_inven
    interpreterRegisterOpcode(0x80D9, opRemoveObjectFromInventory); // op_rm_obj_from_inven
    interpreterRegisterOpcode(0x80DA, opWieldItem); // op_wield_obj_critter
    interpreterRegisterOpcode(0x80DB, opUseObject); // op_use_obj
    interpreterRegisterOpcode(0x80DC, opObjectCanSeeObject); // op_obj_can_see_obj
    interpreterRegisterOpcode(0x80DD, opAttackComplex); // op_attack
    interpreterRegisterOpcode(0x80DE, opStartGameDialog); // op_start_gdialog
    interpreterRegisterOpcode(0x80DF, opEndGameDialog); // op_end_gdialog
    interpreterRegisterOpcode(0x80E0, opGameDialogReaction); // op_dialogue_reaction
    interpreterRegisterOpcode(0x80E1, opMetarule3); // op_metarule3
    interpreterRegisterOpcode(0x80E2, opSetMapMusic); // op_set_map_music
    interpreterRegisterOpcode(0x80E3, opSetObjectVisibility); // op_set_obj_visibility
    interpreterRegisterOpcode(0x80E4, opLoadMap); // op_load_map
    interpreterRegisterOpcode(0x80E5, opWorldmapCitySetPos); // op_wm_area_set_pos
    interpreterRegisterOpcode(0x80E6, opSetExitGrids); // op_set_exit_grids
    interpreterRegisterOpcode(0x80E7, opAnimBusy); // op_anim_busy
    interpreterRegisterOpcode(0x80E8, opCritterHeal); // op_critter_heal
    interpreterRegisterOpcode(0x80E9, opSetLightLevel); // op_set_light_level
    interpreterRegisterOpcode(0x80EA, opGetGameTime); // op_game_time
    interpreterRegisterOpcode(0x80EB, opGetGameTimeInSeconds); // op_game_time / 10
    interpreterRegisterOpcode(0x80EC, opGetObjectElevation); // op_elevation
    interpreterRegisterOpcode(0x80ED, opKillCritter); // op_kill_critter
    interpreterRegisterOpcode(0x80EE, opKillCritterType); // op_kill_critter_type
    interpreterRegisterOpcode(0x80EF, opCritterDamage); // op_critter_damage
    interpreterRegisterOpcode(0x80F0, opAddTimerEvent); // op_add_timer_event
    interpreterRegisterOpcode(0x80F1, opRemoveTimerEvent); // op_rm_timer_event
    interpreterRegisterOpcode(0x80F2, opGameTicks); // op_game_ticks
    interpreterRegisterOpcode(0x80F3, opHasTrait); // op_has_trait
    interpreterRegisterOpcode(0x80F4, opDestroyObject); // op_destroy_object
    interpreterRegisterOpcode(0x80F5, opObjectCanHearObject); // op_obj_can_hear_obj
    interpreterRegisterOpcode(0x80F6, opGameTimeHour); // op_game_time_hour
    interpreterRegisterOpcode(0x80F7, opGetFixedParam); // op_fixed_param
    interpreterRegisterOpcode(0x80F8, opTileIsVisible); // op_tile_is_visible
    interpreterRegisterOpcode(0x80F9, opGameDialogSystemEnter); // op_dialogue_system_enter
    interpreterRegisterOpcode(0x80FA, opGetActionBeingUsed); // op_action_being_used
    interpreterRegisterOpcode(0x80FB, opGetCritterState); // critter_state
    interpreterRegisterOpcode(0x80FC, opGameTimeAdvance); // op_game_time_advance
    interpreterRegisterOpcode(0x80FD, opRadiationIncrease); // op_radiation_inc
    interpreterRegisterOpcode(0x80FE, opRadiationDecrease); // op_radiation_dec
    interpreterRegisterOpcode(0x80FF, opCritterAttemptPlacement); // critter_attempt_placement
    interpreterRegisterOpcode(0x8100, opGetObjectPid); // op_obj_pid
    interpreterRegisterOpcode(0x8101, opGetCurrentMap); // op_cur_map_index
    interpreterRegisterOpcode(0x8102, opCritterAddTrait); // op_critter_add_trait
    interpreterRegisterOpcode(0x8103, opCritterRemoveTrait); // critter_rm_trait
    interpreterRegisterOpcode(0x8104, opGetProtoData); // op_proto_data
    interpreterRegisterOpcode(0x8105, opGetMessageString); // op_message_str
    interpreterRegisterOpcode(0x8106, opCritterGetInventoryObject); // op_critter_inven_obj
    interpreterRegisterOpcode(0x8107, opSetObjectLightLevel); // op_obj_set_light_level
    interpreterRegisterOpcode(0x8108, opWorldmap); // op_scripts_request_world_map
    interpreterRegisterOpcode(0x8109, _op_inven_cmds); // op_inven_cmds
    interpreterRegisterOpcode(0x810A, opFloatMessage); // op_float_msg
    interpreterRegisterOpcode(0x810B, opMetarule); // op_metarule
    interpreterRegisterOpcode(0x810C, opAnim); // op_anim
    interpreterRegisterOpcode(0x810D, opObjectCarryingObjectByPid); // op_obj_carrying_pid_obj
    interpreterRegisterOpcode(0x810E, opRegAnimFunc); // op_reg_anim_func
    interpreterRegisterOpcode(0x810F, opRegAnimAnimate); // op_reg_anim_animate
    interpreterRegisterOpcode(0x8110, opRegAnimAnimateReverse); // op_reg_anim_animate_reverse
    interpreterRegisterOpcode(0x8111, opRegAnimObjectMoveToObject); // op_reg_anim_obj_move_to_obj
    interpreterRegisterOpcode(0x8112, opRegAnimObjectRunToObject); // op_reg_anim_obj_run_to_obj
    interpreterRegisterOpcode(0x8113, opRegAnimObjectMoveToTile); // op_reg_anim_obj_move_to_tile
    interpreterRegisterOpcode(0x8114, opRegAnimObjectRunToTile); // op_reg_anim_obj_run_to_tile
    interpreterRegisterOpcode(0x8115, opPlayGameMovie); // op_play_gmovie
    interpreterRegisterOpcode(0x8116, opAddMultipleObjectsToInventory); // op_add_mult_objs_to_inven
    interpreterRegisterOpcode(0x8117, opRemoveMultipleObjectsFromInventory); // rm_mult_objs_from_inven
    interpreterRegisterOpcode(0x8118, opGetMonth); // op_month
    interpreterRegisterOpcode(0x8119, opGetDay); // op_day
    interpreterRegisterOpcode(0x811A, opExplosion); // op_explosion
    interpreterRegisterOpcode(0x811B, opGetDaysSinceLastVisit); // op_days_since_visited
    interpreterRegisterOpcode(0x811C, _op_gsay_start);
    interpreterRegisterOpcode(0x811D, _op_gsay_end);
    interpreterRegisterOpcode(0x811E, _op_gsay_reply); // op_gsay_reply
    interpreterRegisterOpcode(0x811F, _op_gsay_option); // op_gsay_option
    interpreterRegisterOpcode(0x8120, _op_gsay_message); // op_gsay_message
    interpreterRegisterOpcode(0x8121, _op_giq_option); // op_giq_option
    interpreterRegisterOpcode(0x8122, opPoison); // op_poison
    interpreterRegisterOpcode(0x8123, opGetPoison); // op_get_poison
    interpreterRegisterOpcode(0x8124, opPartyAdd); // op_party_add
    interpreterRegisterOpcode(0x8125, opPartyRemove); // op_party_remove
    interpreterRegisterOpcode(0x8126, opRegAnimAnimateForever); // op_reg_anim_animate_forever
    interpreterRegisterOpcode(0x8127, opCritterInjure); // op_critter_injure
    interpreterRegisterOpcode(0x8128, opCombatIsInitialized); // op_is_in_combat
    interpreterRegisterOpcode(0x8129, _op_gdialog_barter); // op_gdialog_barter
    interpreterRegisterOpcode(0x812A, opGetGameDifficulty); // op_game_difficulty
    interpreterRegisterOpcode(0x812B, opGetRunningBurningGuy); // op_running_burning_guy
    interpreterRegisterOpcode(0x812C, _op_inven_unwield); // op_inven_unwield
    interpreterRegisterOpcode(0x812D, opObjectIsLocked); // op_obj_is_locked
    interpreterRegisterOpcode(0x812E, opObjectLock); // op_obj_lock
    interpreterRegisterOpcode(0x812F, opObjectUnlock); // op_obj_unlock
    interpreterRegisterOpcode(0x8131, opObjectOpen); // op_obj_open
    interpreterRegisterOpcode(0x8130, opObjectIsOpen); // op_obj_is_open
    interpreterRegisterOpcode(0x8132, opObjectClose); // op_obj_close
    interpreterRegisterOpcode(0x8133, opGameUiDisable); // op_game_ui_disable
    interpreterRegisterOpcode(0x8134, opGameUiEnable); // op_game_ui_enable
    interpreterRegisterOpcode(0x8135, opGameUiIsDisabled); // op_game_ui_is_disabled
    interpreterRegisterOpcode(0x8136, opGameFadeOut); // op_gfade_out
    interpreterRegisterOpcode(0x8137, opGameFadeIn); // op_gfade_in
    interpreterRegisterOpcode(0x8138, opItemCapsTotal); // op_item_caps_total
    interpreterRegisterOpcode(0x8139, opItemCapsAdjust); // op_item_caps_adjust
    interpreterRegisterOpcode(0x813A, _op_anim_action_frame); // op_anim_action_frame
    interpreterRegisterOpcode(0x813B, opRegAnimPlaySfx); // op_reg_anim_play_sfx
    interpreterRegisterOpcode(0x813C, opCritterModifySkill); // op_critter_mod_skill
    interpreterRegisterOpcode(0x813D, opSfxBuildCharName); // op_sfx_build_char_name
    interpreterRegisterOpcode(0x813E, opSfxBuildAmbientName); // op_sfx_build_ambient_name
    interpreterRegisterOpcode(0x813F, opSfxBuildInterfaceName); // op_sfx_build_interface_name
    interpreterRegisterOpcode(0x8140, opSfxBuildItemName); // op_sfx_build_item_name
    interpreterRegisterOpcode(0x8141, opSfxBuildWeaponName); // op_sfx_build_weapon_name
    interpreterRegisterOpcode(0x8142, opSfxBuildSceneryName); // op_sfx_build_scenery_name
    interpreterRegisterOpcode(0x8143, opAttackSetup); // op_attack_setup
    interpreterRegisterOpcode(0x8144, opDestroyMultipleObjects); // op_destroy_mult_objs
    interpreterRegisterOpcode(0x8145, opUseObjectOnObject); // op_use_obj_on_obj
    interpreterRegisterOpcode(0x8146, opEndgameSlideshow); // op_endgame_slideshow
    interpreterRegisterOpcode(0x8147, opMoveObjectInventoryToObject); // op_move_obj_inven_to_obj
    interpreterRegisterOpcode(0x8148, opEndgameMovie); // op_endgame_movie
    interpreterRegisterOpcode(0x8149, opGetObjectFid); // op_obj_art_fid
    interpreterRegisterOpcode(0x814A, opGetFidAnim); // op_art_anim
    interpreterRegisterOpcode(0x814B, opGetPartyMember); // op_party_member_obj
    interpreterRegisterOpcode(0x814C, opGetRotationToTile); // op_rotation_to_tile
    interpreterRegisterOpcode(0x814D, opJamLock); // op_jam_lock
    interpreterRegisterOpcode(0x814E, opGameDialogSetBarterMod); // op_gdialog_set_barter_mod
    interpreterRegisterOpcode(0x814F, opGetCombatDifficulty); // op_combat_difficulty
    interpreterRegisterOpcode(0x8150, opObjectOnScreen); // op_obj_on_screen
    interpreterRegisterOpcode(0x8151, opCritterIsFleeing); // op_critter_is_fleeing
    interpreterRegisterOpcode(0x8152, opCritterSetFleeState); // op_critter_set_flee_state
    interpreterRegisterOpcode(0x8153, opTerminateCombat); // op_terminate_combat
    interpreterRegisterOpcode(0x8154, opDebugMessage); // op_debug_msg
    interpreterRegisterOpcode(0x8155, opCritterStopAttacking); // op_critter_stop_attacking

    sfallOpcodesInit();
}

// NOTE: Uncollapsed 0x45D878.
//
// 0x45D878
void intExtraUpdate()
{
}

// NOTE: Uncollapsed 0x45D878.
//
// 0x45D878
void intExtraRemoveProgramReferences(Program* program)
{
}

} // namespace fallout
