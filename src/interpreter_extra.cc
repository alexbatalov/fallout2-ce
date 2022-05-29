#include "interpreter_extra.h"

#include "actions.h"
#include "animation.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "core.h"
#include "critter.h"
#include "debug.h"
#include "dialog.h"
#include "display_monitor.h"
#include "endgame.h"
#include "game.h"
#include "game_config.h"
#include "game_dialog.h"
#include "game_movie.h"
#include "game_sound.h"
#include "interface.h"
#include "item.h"
#include "light.h"
#include "loadsave.h"
#include "map.h"
#include "object.h"
#include "palette.h"
#include "perk.h"
#include "proto.h"
#include "proto_instance.h"
#include "queue.h"
#include "random.h"
#include "reaction.h"
#include "scripts.h"
#include "skill.h"
#include "stat.h"
#include "text_object.h"
#include "tile.h"
#include "trait.h"
#include "world_map.h"

#include <stdio.h>
#include <string.h>

// 0x504B04
char _Error_0[] = "Error";

// 0x504B0C
char _aCritter[] = "<Critter>";

// Maps light level to light intensity.
//
// Middle value is mapped one-to-one which corresponds to 50% light level
// (cavern lighting). Light levels above (51-100%) and below (0-49) is
// calculated as percentage from two adjacent light values.
//
// See [opSetLightLevel] for math.
//
// 0x453F90
const int dword_453F90[3] = {
    0x4000,
    0xA000,
    0x10000,
};

// 0x453F9C
const unsigned short word_453F9C[MOVIE_COUNT] = {
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

// 0x453FC0
Rect stru_453FC0 = { 0, 0, 640, 480 };

// 0x518EC0
const char* _dbg_error_strs[SCRIPT_ERROR_COUNT] = {
    "unimped",
    "obj is NULL",
    "can't match program to sid",
    "follows",
};

// 0x518ED0
const int _ftList[11] = {
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

// 0x518EFC
char* _errStr = _Error_0;

// Last message type during op_float_msg sequential.
//
// 0x518F00
int _last_color = 1;

// 0x518F04
char* _strName = _aCritter;

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
int gGameDialogReactionOrFidget;

// 0x453FD0
void scriptPredefinedError(Program* program, const char* name, int error)
{
    char string[260];

    sprintf(string, "Script Error: %s: op_%s: %s", program->name, name, _dbg_error_strs[error]);

    debugPrint(string);
}

// 0x45400C
void scriptError(const char* format, ...)
{
    char string[260];

    va_list argptr;
    va_start(argptr, format);
    vsprintf(string, format, argptr);
    va_end(argptr);

    debugPrint(string);
}

// 0x45404C
int tileIsVisible(int tile)
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
int _correctFidForRemovedItem(Object* a1, Object* a2, int flags)
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
            newFid = buildFid((fid & 0xF000000) >> 24, fid & 0xFFF, (fid & 0xFF0000) >> 16, 0, (fid & 0x70000000) >> 28);
        }
    } else {
        if (a1 == gDude) {
            newFid = buildFid((fid & 0xF000000) >> 24, _art_vault_guy_num, (fid & 0xFF0000) >> 16, v8, (fid & 0x70000000) >> 28);
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
void opGiveExpPoints(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to give_exp_points", program->name);
    }

    if (pcAddExperience(data) != 0) {
        scriptError("\nScript Error: %s: op_give_exp_points: stat_pc_set failed");
    }
}

// scr_return
// 0x454238
void opScrReturn(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to scr_return", program->name);
    }

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        script->field_28 = data;
    }
}

// play_sfx
// 0x4542AC
void opPlaySfx(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("script error: %s: invalid arg to play_sfx", program->name);
    }

    char* name = programGetString(program, opcode, data);
    soundPlayFile(name);
}

// set_map_start
// 0x454314
void opSetMapStart(Program* program)
{
    opcode_t opcode[4];
    int data[4];

    for (int arg = 0; arg < 4; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to set_map_start", program->name, arg);
        }
    }

    int x = data[3];
    int y = data[2];
    int elevation = data[1];
    int rotation = data[0];

    if (mapSetElevation(elevation) != 0) {
        scriptError("\nScript Error: %s: op_set_map_start: map_set_elevation failed", program->name);
        return;
    }

    int tile = 200 * y + x;
    if (tileSetCenter(tile, TILE_SET_CENTER_FLAG_0x01 | TILE_SET_CENTER_FLAG_0x02) != 0) {
        scriptError("\nScript Error: %s: op_set_map_start: tile_set_center failed", program->name);
        return;
    }

    mapSetStart(tile, elevation, rotation);
}

// override_map_start
// 0x4543F4
void opOverrideMapStart(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    opcode_t opcode[4];
    int data[4];

    for (int arg = 0; arg < 4; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to override_map_start", program->name, arg);
        }
    }

    int x = data[3];
    int y = data[2];
    int elevation = data[1];
    int rotation = data[0];

    char text[60];
    sprintf(text, "OVERRIDE_MAP_START: x: %d, y: %d", x, y);
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

        tileSetCenter(tile, TILE_SET_CENTER_FLAG_0x01);
        tileWindowRefresh();
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// has_skill
// 0x454568
void opHasSkill(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to has_skill", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int skill = data[0];

    int result = 0;
    if (object != NULL) {
        if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
            result = skillGetValue(object, skill);
        }
    } else {
        scriptPredefinedError(program, "has_skill", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// using_skill
// 0x454634
void opUsingSkill(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to using_skill", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int skill = data[0];

    // NOTE: In the original source code this value is left uninitialized, that
    // explains why garbage is returned when using something else than dude and
    // SKILL_SNEAK as arguments.
    int result = 0;

    if (skill == SKILL_SNEAK && object == gDude) {
        result = dudeHasState(DUDE_STATE_SNEAKING);
    }

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// roll_vs_skill
// 0x4546E8
void opRollVsSkill(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to roll_vs_skill", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int skill = data[1];
    int modifier = data[0];

    int roll = ROLL_CRITICAL_FAILURE;
    if (object != NULL) {
        if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
            int sid = scriptGetSid(program);

            Script* script;
            if (scriptGetScript(sid, &script) != -1) {
                roll = skillRoll(object, skill, modifier, &(script->howMuch));
            }
        }
    } else {
        scriptPredefinedError(program, "roll_vs_skill", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, roll);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// skill_contest
// 0x4547D4
void opSkillContest(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to skill_contest", program->name, arg);
        }
    }

    scriptPredefinedError(program, "skill_contest", SCRIPT_ERROR_NOT_IMPLEMENTED);
    programStackPushInt32(program, 0);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// do_check
// 0x454890
void opDoCheck(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to do_check", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int stat = data[1];
    int mod = data[0];

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

    programStackPushInt32(program, roll);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// success
// 0x4549A8
void opSuccess(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to success", program->name);
    }

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

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// critical
// 0x454A44
void opCritical(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to critical", program->name);
    }

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

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// how_much
// 0x454AD0
void opHowMuch(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to how_much", program->name);
    }

    int result = 0;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        result = script->howMuch;
    } else {
        scriptPredefinedError(program, "how_much", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// mark_area_known
// 0x454B6C
void opMarkAreaKnown(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to mark_area_known", program->name, arg);
        }
    }

    // TODO: Provide meaningful names.
    if (data[2] == 0) {
        if (data[0] == CITY_STATE_INVISIBLE) {
            _wmAreaSetVisibleState(data[1], 0, 1);
        } else {
            _wmAreaSetVisibleState(data[1], 1, 1);
            _wmAreaMarkVisitedState(data[1], data[0]);
        }
    } else if (data[2] == 1) {
        _wmMapMarkVisited(data[1]);
    }
}

// reaction_influence
// 0x454C34
void opReactionInfluence(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to reaction_influence", program->name, arg);
        }
    }

    int result = _reaction_influence_();
    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// random
// 0x454CD4
void opRandom(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to random", program->name, arg);
        }
    }

    int result;
    if (_vcr_status() == 2) {
        result = randomBetween(data[1], data[0]);
    } else {
        result = (data[0] - data[1]) / 2;
    }

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// roll_dice
// 0x454D88
void opRollDice(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to roll_dice", program->name, arg);
        }
    }

    scriptPredefinedError(program, "roll_dice", SCRIPT_ERROR_NOT_IMPLEMENTED);

    programStackPushInt32(program, 0);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// move_to
// 0x454E28
void opMoveTo(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to move_to", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int tile = data[1];
    int elevation = data[0];

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
                tileSetCenter(object->tile, TILE_SET_CENTER_FLAG_0x01);
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

            if (object->elevation != elevation && (object->pid >> 24) == OBJ_TYPE_CRITTER) {
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

    programStackPushInt32(program, newTile);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// create_object_sid
// 0x454FA8
void opCreateObject(Program* program)
{
    opcode_t opcode[4];
    int data[4];

    for (int arg = 0; arg < 4; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to create_object", program->name, arg);
        }
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
        switch (object->pid >> 24) {
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
            script->sp.built_tile = ((object->elevation << 29) & 0xE0000000) | object->tile;
            script->sp.radius = 3;
        }

        object->id = scriptsNewObjectId();
        script->field_1C = object->id;
        script->owner = object;
        _scr_find_str_run_info(sid - 1, &(script->field_50), object->sid);
    };

out:

    programStackPushInt32(program, (int)object);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// destroy_object
// 0x4551E4
void opDestroyObject(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to destroy_object", program->name);
    }

    Object* object = (Object*)data;

    if (object == NULL) {
        scriptPredefinedError(program, "destroy_object", SCRIPT_ERROR_OBJECT_IS_NULL);
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
        if (_isLoadingGame()) {
            debugPrint("\nError: attempt to destroy critter in load/save-game: %s!", program->name);
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }
    }

    bool isSelf = object == scriptGetSelf(program);

    if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
        _combat_delete_critter(object);
    }

    Object* owner = objectGetOwner(object);
    if (owner != NULL) {
        int quantity = _item_count(owner, object);
        itemRemove(owner, object, quantity);

        if (owner == gDude) {
            bool animated = !gameUiIsDisabled();
            interfaceUpdateItems(animated, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
        }

        _obj_connect(object, 1, 0, NULL);

        if (isSelf) {
            object->sid = -1;
            object->flags |= (OBJECT_HIDDEN | OBJECT_TEMPORARY);
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
void opDisplayMsg(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING) {
        programFatalError("script error: %s: invalid arg to display_msg", program->name);
    }

    char* string = programGetString(program, opcode, data);
    displayMonitorAddMessage(string);

    bool showScriptMessages = false;
    configGetBool(&gGameConfig, GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_SCRIPT_MESSAGES_KEY, &showScriptMessages);

    if (showScriptMessages) {
        debugPrint("\n");
        debugPrint(string);
    }
}

// script_overrides
// 0x455430
void opScriptOverrides(Program* program)
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
void opObjectIsCarryingObjectWithPid(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to obj_is_carrying_obj", program->name, arg);
        }
    }

    Object* obj = (Object*)data[1];
    int pid = data[0];

    int result = 0;
    if (obj != NULL) {
        result = objectGetCarriedQuantityByPid(obj, pid);
    } else {
        scriptPredefinedError(program, "obj_is_carrying_obj_pid", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// tile_contains_obj_pid
// 0x455534
void opTileContainsObjectWithPid(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to tile_contains_obj_pid", program->name, arg);
        }
    }

    int tile = data[2];
    int elevation = data[1];
    int pid = data[0];

    int result = 0;

    Object* object = objectFindFirstAtLocation(elevation, tile);
    while (object) {
        if (object->pid == pid) {
            result = 1;
            break;
        }
        object = objectFindNextAtLocation();
    }

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// self_obj
// 0x455600
void opGetSelf(Program* program)
{
    Object* object = scriptGetSelf(program);
    programStackPushInt32(program, (int)object);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// source_obj
// 0x455624
void opGetSource(Program* program)
{
    Object* object = NULL;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        object = script->source;
    } else {
        scriptPredefinedError(program, "source_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInt32(program, (int)object);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// target_obj
// 0x455678
void opGetTarget(Program* program)
{
    Object* object = NULL;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        object = script->target;
    } else {
        scriptPredefinedError(program, "target_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInt32(program, (int)object);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// dude_obj
// 0x4556CC
void opGetDude(Program* program)
{
    programStackPushInt32(program, (int)gDude);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// NOTE: The implementation is the same as in [opGetTarget].
//
// obj_being_used_with
// 0x4556EC
void opGetObjectBeingUsed(Program* program)
{
    Object* object = NULL;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        object = script->target;
    } else {
        scriptPredefinedError(program, "obj_being_used_with", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInt32(program, (int)object);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// local_var
// 0x455740
void opGetLocalVar(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        // FIXME: The error message is wrong.
        programFatalError("script error: %s: invalid arg to op_global_var", program->name);
    }

    int value = -1;

    int sid = scriptGetSid(program);
    scriptGetLocalVar(sid, data, &value);

    programStackPushInt32(program, value);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// set_local_var
// 0x4557C8
void opSetLocalVar(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to set_local_var", program->name, arg);
        }
    }

    int variable = data[1];
    int value = data[0];

    int sid = scriptGetSid(program);
    scriptSetLocalVar(sid, variable, value);
}

// map_var
// 0x455858
void opGetMapVar(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to op_map_var", program->name);
    }

    int value = mapGetGlobalVar(data);

    programStackPushInt32(program, value);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// set_map_var
// 0x4558C8
void opSetMapVar(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to set_map_var", program->name, arg);
        }
    }

    int variable = data[1];
    int value = data[0];

    mapSetGlobalVar(variable, value);
}

// global_var
// 0x455950
void opGetGlobalVar(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to op_global_var", program->name);
    }

    int value = -1;
    if (gGameGlobalVarsLength != 0) {
        value = gameGetGlobalVar(data);
    } else {
        scriptError("\nScript Error: %s: op_global_var: no global vars found!", program->name);
    }

    programStackPushInt32(program, value);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// set_global_var
// 0x4559EC
// 0x80C6
void opSetGlobalVar(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to set_global_var", program->name, arg);
        }
    }

    int variable = data[1];
    int value = data[0];

    if (gGameGlobalVarsLength != 0) {
        gameSetGlobalVar(variable, value);
    } else {
        scriptError("\nScript Error: %s: op_set_global_var: no global vars found!", program->name);
    }
}

// script_action
// 0x455A90
void opGetScriptAction(Program* program)
{
    int action = 0;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        action = script->action;
    } else {
        scriptPredefinedError(program, "script_action", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInt32(program, action);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// obj_type
// 0x455AE4
void opGetObjectType(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to op_obj_type", program->name);
    }

    Object* object = (Object*)data;

    int objectType = -1;
    if (object != NULL) {
        objectType = (object->fid & 0xF000000) >> 24;
    }

    programStackPushInt32(program, objectType);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// obj_item_subtype
// 0x455B6C
void opGetItemType(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to op_item_subtype", program->name);
    }

    Object* obj = (Object*)data;

    int itemType = -1;
    if (obj != NULL) {
        if ((obj->pid >> 24) == OBJ_TYPE_ITEM) {
            Proto* proto;
            if (protoGetProto(obj->pid, &proto) != -1) {
                itemType = itemGetType(obj);
            }
        }
    }

    programStackPushInt32(program, itemType);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// get_critter_stat
// 0x455C10
void opGetCritterStat(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to get_critter_stat", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int stat = data[0];

    int value = -1;
    if (object != NULL) {
        value = critterGetStat(object, stat);
    } else {
        scriptPredefinedError(program, "get_critter_stat", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, value);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// NOTE: Despite it's name it does not actually "set" stat, but "adjust". So
// it's last argument is amount of adjustment, not it's final value.
//
// set_critter_stat
// 0x455CCC
void opSetCritterStat(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to set_critter_stat", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int stat = data[1];
    int value = data[0];

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

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// animate_stand_obj
// 0x455DC8
void opAnimateStand(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to animate_stand_obj", program->name);
    }

    Object* object = (Object*)data;
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
        reg_anim_begin(1);
        reg_anim_animate(object, ANIM_STAND, 0);
        reg_anim_end();
    }
}

// animate_stand_reverse_obj
// 0x455E7C
void opAnimateStandReverse(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        // FIXME: typo in message, should be animate_stand_reverse_obj.
        programFatalError("script error: %s: invalid arg to animate_stand_obj", program->name);
    }

    Object* object = (Object*)data;
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
        reg_anim_begin(0x01);
        reg_anim_animate_reverse(object, ANIM_STAND, 0);
        reg_anim_end();
    }
}

// animate_move_obj_to_tile
// 0x455F30
void opAnimateMoveObjectToTile(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to animate_move_obj_to_tile", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int tile = data[1];
    int flags = data[0];

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

    reg_anim_begin(1);

    if (flags == 0) {
        reg_anim_obj_move_to_tile(object, tile, object->elevation, -1, 0);
    } else {
        reg_anim_obj_run_to_tile(object, tile, object->elevation, -1, 0);
    }

    reg_anim_end();
}

// tile_in_tile_rect
// 0x45607C
void opTileInTileRect(Program* program)
{
    opcode_t opcode[5];
    int data[5];
    Point points[5];

    for (int arg = 0; arg < 5; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to tile_in_tile_rect", program->name, arg);
        }

        points[arg].x = data[arg] % 200;
        points[arg].y = data[arg] / 200;
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

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// make_daytime
// 0x456170
void opMakeDayTime(Program* program)
{
}

// tile_distance
// 0x456174
void opTileDistanceBetween(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to tile_distance", program->name, arg);
        }
    }

    int tile1 = data[1];
    int tile2 = data[0];

    int distance;

    if (tile1 != -1 && tile2 != -1) {
        distance = tileDistanceBetween(tile1, tile2);
    } else {
        distance = 9999;
    }

    programStackPushInt32(program, distance);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// tile_distance_objs
// 0x456228
void opTileDistanceBetweenObjects(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to tile_distance_objs", program->name, arg);
        }
    }

    Object* object1 = (Object*)data[1];
    Object* object2 = (Object*)data[0];

    int distance = 9999;
    if (object1 != NULL && object2 != NULL) {
        if (data[1] >= HEX_GRID_SIZE && data[0] >= HEX_GRID_SIZE) {
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

    programStackPushInt32(program, distance);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// tile_num
// 0x456324
void opGetObjectTile(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to tile_num", program->name);
    }

    Object* obj = (Object*)data;

    int tile = -1;
    if (obj != NULL) {
        tile = obj->tile;
    } else {
        scriptPredefinedError(program, "tile_num", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, tile);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// tile_num_in_direction
// 0x4563B4
void opGetTileInDirection(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to tile_num_in_direction", program->name, arg);
        }
    }

    int origin = data[2];
    int rotation = data[1];
    int distance = data[0];

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

    programStackPushInt32(program, tile);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// pickup_obj
// 0x4564D4
void opPickup(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to pickup_obj", program->name);
    }

    Object* object = (Object*)data;

    if (object == NULL) {
        return;
    }

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) == 1) {
        scriptPredefinedError(program, "pickup_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
        return;
    }

    if (script->target == NULL) {
        scriptPredefinedError(program, "pickup_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    actionPickUp(script->target, object);
}

// drop_obj
// 0x456580
void opDrop(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to drop_obj", program->name);
    }

    Object* object = (Object*)data;

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
void opAddObjectToInventory(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to add_obj_to_inven", program->name, arg);
        }
    }

    Object* owner = (Object*)data[1];
    Object* item = (Object*)data[0];

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
void opRemoveObjectFromInventory(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to rm_obj_from_inven", program->name, arg);
        }
    }

    Object* owner = (Object*)data[1];
    Object* item = (Object*)data[0];

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
void opWieldItem(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to wield_obj_critter", program->name, arg);
        }
    }

    Object* critter = (Object*)data[1];
    Object* item = (Object*)data[0];

    if (critter == NULL) {
        scriptPredefinedError(program, "wield_obj_critter", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (item == NULL) {
        scriptPredefinedError(program, "wield_obj_critter", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if ((critter->pid >> 24) != OBJ_TYPE_CRITTER) {
        scriptPredefinedError(program, "wield_obj_critter", SCRIPT_ERROR_FOLLOWS);
        debugPrint(" Only works for critters!  ERROR ERROR ERROR!");
        return;
    }

    int hand = HAND_RIGHT;

    bool v1 = false;
    Object* oldArmor = NULL;
    Object* newArmor = NULL;
    if (critter == gDude) {
        if (interfaceGetCurrentHand() == HAND_LEFT) {
            hand = HAND_LEFT;
        }

        if (itemGetType(item) == ITEM_TYPE_ARMOR) {
            oldArmor = critterGetArmor(gDude);
        }

        v1 = true;
        newArmor = item;
    }

    if (_inven_wield(critter, item, hand) == -1) {
        scriptPredefinedError(program, "wield_obj_critter", SCRIPT_ERROR_FOLLOWS);
        debugPrint(" inven_wield failed!  ERROR ERROR ERROR!");
        return;
    }

    if (critter == gDude) {
        if (v1) {
            _adjust_ac(critter, oldArmor, newArmor);
        }

        bool animated = !gameUiIsDisabled();
        interfaceUpdateItems(animated, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }
}

// use_obj
// 0x4569D0
void opUseObject(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to use_obj", program->name);
    }

    Object* object = (Object*)data;

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
    if ((self->pid >> 24) == OBJ_TYPE_CRITTER) {
        _action_use_an_object(script->target, object);
    } else {
        _obj_use(self, object);
    }
}

// obj_can_see_obj
// 0x456AC4
void opObjectCanSeeObject(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to obj_can_see_obj", program->name, arg);
        }
    }

    Object* object1 = (Object*)data[1];
    Object* object2 = (Object*)data[0];

    int result = 0;

    if (object1 != NULL && object2 != NULL) {
        if (object2->tile != -1) {
            // NOTE: Looks like dead code, I guess these checks were incorporated
            // into higher level functions, but this code left intact.
            if (object2 == gDude) {
                dudeHasState(0);
            }

            critterGetStat(object1, STAT_PERCEPTION);

            if (objectCanHearObject(object1, object2)) {
                Object* a5;
                _make_straight_path(object1, object1->tile, object2->tile, NULL, &a5, 16);
                if (a5 == object2) {
                    result = 1;
                }
            }
        }
    } else {
        scriptPredefinedError(program, "obj_can_see_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// attack_complex
// 0x456C00
void opAttackComplex(Program* program)
{
    opcode_t opcode[8];
    int data[8];

    for (int arg = 0; arg < 8; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to attack", program->name, arg);
        }
    }

    Object* target = (Object*)data[7];
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
        if ((combatData->maneuver & CRITTER_MANEUVER_0x01) == 0) {
            combatData->maneuver |= CRITTER_MANEUVER_0x01;
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
void opStartGameDialog(Program* program)
{
    opcode_t opcode[5];
    int data[5];

    for (int arg = 0; arg < 5; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to start_gdialog", program->name, arg);
        }
    }

    Object* obj = (Object*)data[3];
    int reactionLevel = data[2];
    int headId = data[1];
    int backgroundId = data[0];

    if (isInCombat()) {
        return;
    }

    if (obj == NULL) {
        scriptPredefinedError(program, "start_gdialog", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    gGameDialogHeadFid = -1;
    if ((obj->pid >> 24) == OBJ_TYPE_CRITTER) {
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
void opEndGameDialog(Program* program)
{
    if (_gdialogExitFromScript() != -1) {
        gGameDialogSpeaker = NULL;
        gGameDialogSid = -1;
    }
}

// dialogue_reaction
// 0x456FA4
void opGameDialogReaction(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int value = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, value);
    }

    if ((opcode & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to dialogue_reaction", program->name);
    }

    gGameDialogReactionOrFidget = value;
    _talk_to_critter_reacts(value);
}

// metarule3
// 0x457110
void opMetarule3(Program* program)
{
    opcode_t opcode[4];
    int data[4];

    for (int arg = 0; arg < 4; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to metarule3", program->name, arg);
        }
    }

    int rule = data[3];
    int result = 0;

    switch (rule) {
    case METARULE3_CLR_FIXED_TIMED_EVENTS:
        if (1) {
            _scrSetQueueTestVals((Object*)data[2], data[1]);
            _queue_clear_type(EVENT_TYPE_SCRIPT, _scrQueueRemoveFixed);
        }
        break;
    case METARULE3_MARK_SUBTILE:
        result = _wmSubTileMarkRadiusVisited(data[2], data[1], data[0]);
        break;
    case METARULE3_GET_KILL_COUNT:
        result = killsGetByType(data[2]);
        break;
    case METARULE3_MARK_MAP_ENTRANCE:
        result = _wmMapMarkMapEntranceState(data[2], data[1], data[0]);
        break;
    case METARULE3_WM_SUBTILE_STATE:
        if (1) {
            int state;
            if (_wmSubTileGetVisitedState(data[2], data[1], &state) == 0) {
                result = state;
            }
        }
        break;
    case METARULE3_TILE_GET_NEXT_CRITTER:
        if (1) {
            int tile = data[2];
            int elevation = data[1];
            Object* previousCritter = (Object*)data[0];

            bool critterFound = previousCritter == NULL;

            Object* object = objectFindFirstAtLocation(elevation, tile);
            while (object != NULL) {
                if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
                    if (critterFound) {
                        result = (int)object;
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
            Object* obj = (Object*)data[2];
            int frmId = data[1];

            int fid = buildFid((obj->fid & 0xF000000) >> 24,
                frmId,
                (obj->fid & 0xFF0000) >> 16,
                (obj->fid & 0xF000) >> 12,
                (obj->fid & 0x70000000) >> 28);

            Rect updatedRect;
            objectSetFid(obj, fid, &updatedRect);
            tileWindowRefreshRect(&updatedRect, gElevation);
        }
        break;
    case METARULE3_TILE_SET_CENTER:
        result = tileSetCenter(data[2], TILE_SET_CENTER_FLAG_0x01);
        break;
    case METARULE3_109:
        result = aiGetChemUse((Object*)data[2]);
        break;
    case METARULE3_110:
        result = carIsEmpty() ? 1 : 0;
        break;
    case METARULE3_111:
        result = _map_target_load_area();
        break;
    }

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// set_map_music
// 0x45734C
void opSetMapMusic(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }
    }

    if ((opcode[1] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        // FIXME: argument is wrong, should be 1.
        programFatalError("script error: %s: invalid arg %d to set_map_music", program->name, 2);
    }

    int mapIndex = data[1];

    char* string = NULL;
    if ((opcode[0] & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        string = programGetString(program, opcode[0], data[0]);
    } else {
        // FIXME: argument is wrong, should be 0.
        programFatalError("script error: %s: invalid arg %d to set_map_music", program->name, 2);
    }

    debugPrint("\nset_map_music: %d, %s", mapIndex, string);
    worldmapSetMapMusic(mapIndex, string);
}

// NOTE: Function name is a bit misleading. Last parameter is a boolean value
// where 1 or true makes object invisible, and value 0 (false) makes it visible
// again. So a better name for this function is opSetObjectInvisible.
//
//
// set_obj_visibility
// 0x45741C
void opSetObjectVisibility(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to set_obj_visibility", program->name, arg);
        }
    }

    Object* obj = (Object*)data[1];
    int invisible = data[0];

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
                if ((obj->pid >> 24) == OBJ_TYPE_CRITTER) {
                    obj->flags |= OBJECT_NO_BLOCK;
                }

                tileWindowRefreshRect(&rect, obj->elevation);
            }
        }
    } else {
        if ((obj->flags & OBJECT_HIDDEN) != 0) {
            if ((obj->pid >> 24) == OBJ_TYPE_CRITTER) {
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
void opLoadMap(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    opcode[0] = programStackPopInt16(program);
    data[0] = programStackPopInt32(program);

    if (opcode[0] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode[0], data[0]);
    }

    if ((opcode[0] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg 0 to load_map", program->name);
    }

    opcode[1] = programStackPopInt16(program);
    data[1] = programStackPopInt32(program);

    if (opcode[1] == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode[1], data[1]);
    }

    int param = data[0];
    int mapIndexOrName = data[1];

    char* mapName = NULL;

    if ((opcode[1] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        if ((opcode[1] & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            mapName = programGetString(program, opcode[1], mapIndexOrName);
        } else {
            programFatalError("script error: %s: invalid arg 1 to load_map", program->name);
        }
    }

    int mapIndex = -1;

    if (mapName != NULL) {
        gGameGlobalVars[GVAR_LOAD_MAP_INDEX] = param;
        mapIndex = mapGetIndexByFileName(mapName);
    } else {
        if (mapIndexOrName >= 0) {
            gGameGlobalVars[GVAR_LOAD_MAP_INDEX] = param;
            mapIndex = mapIndexOrName;
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
void opWorldmapCitySetPos(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to wm_area_set_pos", program->name, arg);
        }
    }

    int city = data[2];
    int x = data[1];
    int y = data[0];

    if (worldmapCitySetPos(city, x, y) == -1) {
        scriptPredefinedError(program, "wm_area_set_pos", SCRIPT_ERROR_FOLLOWS);
        debugPrint("Invalid Parameter!");
    }
}

// set_exit_grids
// 0x457730
void opSetExitGrids(Program* program)
{
    opcode_t opcode[5];
    int data[5];

    for (int arg = 0; arg < 5; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to set_exit_grids", program->name, arg);
        }
    }

    int elevation = data[4];
    int destinationMap = data[3];
    int destinationElevation = data[2];
    int destinationTile = data[1];
    int destinationRotation = data[0];

    Object* object = objectFindFirstAtElevation(elevation);
    while (object != NULL) {
        if (object->pid >= PROTO_ID_0x5000010 && object->pid <= PROTO_ID_0x5000017) {
            object->data.misc.map = destinationMap;
            object->data.misc.tile = destinationTile;
            object->data.misc.elevation = destinationElevation;
        }
        object = objectFindNextAtElevation();
    }
}

// anim_busy
// 0x4577EC
void opAnimBusy(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to anim_busy", program->name);
    }

    Object* object = (Object*)data;

    int rc = 0;
    if (object != NULL) {
        rc = animationIsBusy(object);
    } else {
        scriptPredefinedError(program, "anim_busy", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, rc);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// critter_heal
// 0x457880
void opCritterHeal(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to critter_heal", program->name, arg);
        }
    }

    Object* critter = (Object*)data[1];
    int amount = data[0];

    int rc = critterAdjustHitPoints(critter, amount);

    if (critter == gDude) {
        interfaceRenderHitPoints(true);
    }

    programStackPushInt32(program, rc);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// set_light_level
// 0x457934
void opSetLightLevel(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to set_light_level", program->name);
    }

    int lightLevel = data;

    if (data == 50) {
        lightSetLightLevel(dword_453F90[1], true);
        return;
    }

    int lightIntensity;
    if (data > 50) {
        lightIntensity = dword_453F90[1] + data * (dword_453F90[2] - dword_453F90[1]) / 100;
    } else {
        lightIntensity = dword_453F90[0] + data * (dword_453F90[1] - dword_453F90[0]) / 100;
    }

    lightSetLightLevel(lightIntensity, true);
}

// game_time
// 0x4579F4
void opGetGameTime(Program* program)
{
    int time = gameTimeGetTime();
    programStackPushInt32(program, time);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// game_time_in_seconds
// 0x457A18
void opGetGameTimeInSeconds(Program* program)
{
    int time = gameTimeGetTime();
    programStackPushInt32(program, time / 10);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// elevation
// 0x457A44
void opGetObjectElevation(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to elevation", program->name);
    }

    Object* object = (Object*)data;

    int elevation = 0;
    if (object != NULL) {
        elevation = object->elevation;
    } else {
        scriptPredefinedError(program, "elevation", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, elevation);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// kill_critter
// 0x457AD4
void opKillCritter(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to kill_critter", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int deathFrame = data[0];

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
int _correctDeath(Object* critter, int anim, bool forceBack)
{
    if (anim >= ANIM_BIG_HOLE_SF && anim <= ANIM_FALL_FRONT_BLOOD_SF) {
        int violenceLevel = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
        configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violenceLevel);

        bool useStandardDeath = false;
        if (violenceLevel < VIOLENCE_LEVEL_MAXIMUM_BLOOD) {
            useStandardDeath = true;
        } else {
            int fid = buildFid(1, critter->fid & 0xFFF, anim, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
            if (!artExists(fid)) {
                useStandardDeath = true;
            }
        }

        if (useStandardDeath) {
            if (forceBack) {
                anim = ANIM_FALL_BACK;
            } else {
                int fid = buildFid(1, critter->fid & 0xFFF, ANIM_FALL_FRONT, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
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
void opKillCritterType(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to kill_critter", program->name, arg);
        }
    }

    int pid = data[1];
    int deathFrame = data[0];

    if (_isLoadingGame()) {
        debugPrint("\nError: attempt to destroy critter in load/save-game: %s!", program->name);
        return;
    }

    program->flags |= PROGRAM_FLAG_0x20;

    Object* previousObj = NULL;
    int count = 0;
    int v3 = 0;

    Object* obj = objectFindFirst();
    while (obj != NULL) {
        if (((obj->fid & 0xFF0000) >> 16) >= ANIM_FALL_BACK_SF) {
            obj = objectFindNext();
            continue;
        }

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
                    int anim = _correctDeath(obj, _ftList[v3], 1);
                    critterKill(obj, anim, 1);
                    v3 += 1;
                    if (v3 >= 11) {
                        v3 = 0;
                    }
                } else {
                    critterKill(obj, ANIM_FALL_BACK_SF, 1);
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

            gMapHeader.field_38 = gameTimeGetTime();
        }

        obj = objectFindNext();
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// critter_dmg
// 0x457EB4
void opCritterDamage(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to critter_damage", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int amount = data[1];
    int damageTypeWithFlags = data[0];

    if (object == NULL) {
        scriptPredefinedError(program, "critter_damage", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if ((object->pid >> 24) != OBJ_TYPE_CRITTER) {
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
    _action_dmg(object->tile, object->elevation, amount, amount, damageType, animate, bypassArmor);

    program->flags &= ~PROGRAM_FLAG_0x20;

    if (self == object) {
        program->flags |= PROGRAM_FLAG_0x0100;
    }
}

// add_timer_event
// 0x457FF0
void opAddTimerEvent(Program* s)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(s);
        data[arg] = programStackPopInt32(s);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(s, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to add_timer_event", s->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int delay = data[1];
    int param = data[0];

    if (object == NULL) {
        scriptError("\nScript Error: %s: op_add_timer_event: pobj is NULL!", s->name);
        return;
    }

    scriptAddTimerEvent(object->sid, delay, param);
}

// rm_timer_event
// 0x458094
void opRemoveTimerEvent(Program* program)
{
    int elevation;

    elevation = 0;

    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to rm_timer_event", program->name);
    }

    Object* object = (Object*)data;

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
void opGameTicks(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to game_ticks", program->name);
    }

    int ticks = data;

    if (ticks < 0) {
        ticks = 0;
    }

    programStackPushInt32(program, ticks * 10);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// NOTE: The name of this function is misleading. It has (almost) nothing to do
// with player's "Traits" as a feature. Instead it's used to query many
// information of the critters using passed parameters. It's like "metarule" but
// for critters.
//
// 0x458180
// has_trait
void opHasTrait(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to has_trait", program->name, arg);
        }
    }

    int type = data[2];
    Object* object = (Object*)data[1];
    int param = data[0];

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
                if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
                    result = object->data.critter.combat.aiPacket;
                }
                break;
            case CRITTER_TRAIT_OBJECT_TEAM:
                if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
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

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// obj_can_hear_obj
// 0x45835C
void opObjectCanHearObject(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d, to obj_can_hear_obj", program->name, arg);
        }
    }

    Object* object1 = (Object*)data[1];
    Object* object2 = (Object*)data[0];

    bool canHear = false;

    // FIXME: This is clearly an error. If any of the object is NULL
    // dereferencing will crash the game.
    if (object2 == NULL || object1 == NULL) {
        if (object2->elevation == object1->elevation) {
            if (object2->tile != -1 && object1->tile != -1) {
                if (objectCanHearObject(object2, object1)) {
                    canHear = true;
                }
            }
        }
    }

    programStackPushInt32(program, canHear);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// game_time_hour
// 0x458438
void opGameTimeHour(Program* program)
{
    int value = gameTimeGetHour();
    programStackPushInt32(program, value);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// fixed_param
// 0x45845C
void opGetFixedParam(Program* program)
{
    int fixedParam = 0;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        fixedParam = script->fixedParam;
    } else {
        scriptPredefinedError(program, "fixed_param", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInt32(program, fixedParam);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// tile_is_visible
// 0x4584B0
void opTileIsVisible(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to tile_is_visible", program->name);
    }

    int isVisible = 0;
    if (tileIsVisible(data)) {
        isVisible = 1;
    }

    programStackPushInt32(program, isVisible);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// dialogue_system_enter
// 0x458534
void opGameDialogSystemEnter(Program* program)
{
    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        return;
    }

    Object* self = scriptGetSelf(program);
    if ((self->pid >> 24) == OBJ_TYPE_CRITTER) {
        if (!critterIsActive(self)) {
            return;
        }
    }

    if (isInCombat()) {
        return;
    }

    if (_game_state_request(4) == -1) {
        return;
    }

    gGameDialogSpeaker = scriptGetSelf(program);
}

// action_being_used
// 0x458594
void opGetActionBeingUsed(Program* program)
{
    int action = -1;

    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        action = script->actionBeingUsed;
    } else {
        scriptPredefinedError(program, "action_being_used", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInt32(program, action);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// critter_state
// 0x4585E8
void opGetCritterState(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to critter_state", program->name);
    }

    Object* critter = (Object*)data;

    int state = CRITTER_STATE_DEAD;
    if (critter != NULL && (critter->pid >> 24) == OBJ_TYPE_CRITTER) {
        if (critterIsActive(critter)) {
            state = CRITTER_STATE_NORMAL;

            int anim = (critter->fid & 0xFF0000) >> 16;
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

    programStackPushInt32(program, state);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// game_time_advance
// 0x4586C8
void opGameTimeAdvance(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to game_time_advance", program->name);
    }

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
void opRadiationIncrease(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to radiation_inc", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int amount = data[0];

    if (object == NULL) {
        scriptPredefinedError(program, "radiation_inc", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    critterAdjustRadiation(object, amount);
}

// radiation_dec
// 0x458800
void opRadiationDecrease(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to radiation_dec", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int amount = data[0];

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
void opCritterAttemptPlacement(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to critter_attempt_placement", program->name, arg);
        }
    }

    Object* critter = (Object*)data[2];
    int tile = data[1];
    int elevation = data[0];

    if (critter == NULL) {
        scriptPredefinedError(program, "critter_attempt_placement", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (elevation != critter->elevation && critter->pid >> 24 == OBJ_TYPE_CRITTER) {
        _combat_delete_critter(critter);
    }

    objectSetLocation(critter, 0, elevation, NULL);

    int rc = _obj_attempt_placement(critter, tile, elevation, 1);
    programStackPushInt32(program, rc);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// obj_pid
// 0x4589A0
void opGetObjectPid(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to obj_pid", program->name);
    }

    Object* obj = (Object*)data;

    int pid = -1;
    if (obj) {
        pid = obj->pid;
    } else {
        scriptPredefinedError(program, "obj_pid", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, pid);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// cur_map_index
// 0x458A30
void opGetCurrentMap(Program* program)
{
    int mapIndex = mapGetCurrentMap();
    programStackPushInt32(program, mapIndex);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// critter_add_trait
// 0x458A54
void opCritterAddTrait(Program* program)
{
    opcode_t opcode[4];
    int data[4];

    for (int arg = 0; arg < 4; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to critter_add_trait", program->name, arg);
        }
    }

    Object* object = (Object*)data[3];
    int kind = data[2];
    int param = data[1];
    int value = data[0];

    if (object != NULL) {
        if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
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

    programStackPushInt32(program, -1);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// critter_rm_trait
// 0x458C2C
void opCritterRemoveTrait(Program* program)
{
    opcode_t opcode[4];
    int data[4];

    for (int arg = 0; arg < 4; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to critter_rm_trait", program->name, arg);
        }
    }

    Object* object = (Object*)data[3];
    int kind = data[2];
    int param = data[1];
    int value = data[0];

    if (object == NULL) {
        scriptPredefinedError(program, "critter_rm_trait", SCRIPT_ERROR_OBJECT_IS_NULL);
        // FIXME: Ruins stack.
        return;
    }

    if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
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

    programStackPushInt32(program, -1);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// proto_data
// 0x458D38
void opGetProtoData(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to proto_data", program->name, arg);
        }
    }

    int pid = data[1];
    int member = data[0];

    ProtoDataMemberValue value;
    value.integerValue = 0;
    int valueType = protoGetDataMember(pid, member, &value);
    switch (valueType) {
    case PROTO_DATA_MEMBER_TYPE_INT:
        programStackPushInt32(program, value.integerValue);
        programStackPushInt16(program, VALUE_TYPE_INT);
        break;
    case PROTO_DATA_MEMBER_TYPE_STRING:
        programStackPushInt32(program, programPushString(program, value.stringValue));
        programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);
        break;
    default:
        programStackPushInt32(program, 0);
        programStackPushInt16(program, VALUE_TYPE_INT);
        break;
    }
}

// message_str
// 0x458E10
void opGetMessageString(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to message_str", program->name, arg);
        }
    }

    int messageListIndex = data[1];
    int messageIndex = data[0];

    char* string;
    if (messageIndex >= 1) {
        string = _scr_get_msg_str_speech(messageListIndex, messageIndex, 1);
        if (string == NULL) {
            debugPrint("\nError: No message file EXISTS!: index %d, line %d", messageListIndex, messageIndex);
            string = _errStr;
        }
    } else {
        string = _errStr;
    }

    programStackPushInt32(program, programPushString(program, string));
    programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);
}

// critter_inven_obj
// 0x458F00
void opCritterGetInventoryObject(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to critter_inven_obj", program->name, arg);
        }
    }

    Object* critter = (Object*)data[1];
    int type = data[0];

    int result = 0;

    if ((critter->pid >> 24) == OBJ_TYPE_CRITTER) {
        switch (type) {
        case INVEN_TYPE_WORN:
            result = (int)critterGetArmor(critter);
            break;
        case INVEN_TYPE_RIGHT_HAND:
            if (critter == gDude) {
                if (interfaceGetCurrentHand() != HAND_LEFT) {
                    result = (int)critterGetItem2(critter);
                }
            } else {
                result = (int)critterGetItem2(critter);
            }
            break;
        case INVEN_TYPE_LEFT_HAND:
            if (critter == gDude) {
                if (interfaceGetCurrentHand() == HAND_LEFT) {
                    result = (int)critterGetItem1(critter);
                }
            } else {
                result = (int)critterGetItem1(critter);
            }
            break;
        case INVEN_TYPE_INV_COUNT:
            result = critter->data.inventory.length;
            break;
        default:
            scriptError("script error: %s: Error in critter_inven_obj -- wrong type!", program->name);
            break;
        }
    } else {
        scriptPredefinedError(program, "critter_inven_obj", SCRIPT_ERROR_FOLLOWS);
        debugPrint("  Not a critter!");
    }

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// obj_set_light_level
// 0x459088
void opSetObjectLightLevel(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to obj_set_light_level", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int lightIntensity = data[1];
    int lightDistance = data[0];

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
void opWorldmap(Program* program)
{
    scriptsRequestWorldMap();
}

// inven_cmds
// 0x459178
void _op_inven_cmds(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to inven_cmds", program->name, arg);
        }
    }

    Object* obj = (Object*)data[2];
    int cmd = data[1];
    int index = data[0];

    Object* item = NULL;

    if (obj != NULL) {
        switch (cmd) {
        case 13:
            item = _inven_index_ptr(obj, index);
            break;
        }
    } else {
        // FIXME: Should be inven_cmds.
        scriptPredefinedError(program, "anim", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, (int)item);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// float_msg
// 0x459280
void opFloatMessage(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    char* string = NULL;
    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if (arg == 1) {
            if ((opcode[arg] & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
                string = programGetString(program, opcode[arg], data[arg]);
            }
        } else {
            if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
                programFatalError("script error: %s: invalid arg %d to float_msg", program->name, arg);
            }
        }
    }

    Object* obj = (Object*)data[2];
    int floatingMessageType = data[0];

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
        tileSetCenter(gDude->tile, TILE_SET_CENTER_FLAG_0x01);
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
void opMetarule(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to metarule", program->name, arg);
        }
    }

    int rule = data[1];
    int param = data[0];

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
        scriptsRequestElevator(scriptGetSelf(program), param);
        result = 0;
        break;
    case METARULE_PARTY_COUNT:
        result = _getPartyMemberCount();
        break;
    case METARULE_AREA_KNOWN:
        result = _wmAreaVisitedState(param);
        break;
    case METARULE_WHO_ON_DRUGS:
        result = queueHasEvent((Object*)param, EVENT_TYPE_DRUG);
        break;
    case METARULE_MAP_KNOWN:
        result = _wmMapIsKnown(param);
        break;
    case METARULE_IS_LOADGAME:
        result = _isLoadingGame();
        break;
    case METARULE_CAR_CURRENT_TOWN:
        result = carGetCity();
        break;
    case METARULE_GIVE_CAR_TO_PARTY:
        result = _wmCarGiveToParty();
        break;
    case METARULE_GIVE_CAR_GAS:
        result = carAddFuel(param);
        break;
    case METARULE_SKILL_CHECK_TAG:
        result = skillIsTagged(param);
        break;
    case METARULE_DROP_ALL_INVEN:
        if (1) {
            Object* object = (Object*)param;
            result = _item_drop_all(object, object->tile);
            if (gDude == object) {
                interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
                interfaceRenderArmorClass(false);
            }
        }
        break;
    case METARULE_INVEN_UNWIELD_WHO:
        if (1) {
            Object* object = (Object*)param;

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
        _wmGetPartyWorldPos(&result, NULL);
        break;
    case METARULE_GET_WORLDMAP_YPOS:
        _wmGetPartyWorldPos(NULL, &result);
        break;
    case METARULE_CURRENT_TOWN:
        if (_wmGetPartyCurArea(&result) == -1) {
            debugPrint("\nIntextra: Error: metarule: current_town");
        }
        break;
    case METARULE_LANGUAGE_FILTER:
        configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, &result);
        break;
    case METARULE_VIOLENCE_FILTER:
        configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &result);
        break;
    case METARULE_WEAPON_DAMAGE_TYPE:
        if (1) {
            Object* object = (Object*)param;
            if ((object->pid >> 24) == OBJ_TYPE_ITEM) {
                if (itemGetType(object) == ITEM_TYPE_WEAPON) {
                    result = weaponGetDamageType(NULL, object);
                    break;
                }
            } else {
                if (buildFid(5, 10, 0, 0, 0) == object->fid) {
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
            Object* object = (Object*)param;
            if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
                Proto* proto;
                protoGetProto(object->pid, &proto);
                if ((proto->critter.data.flags & 0x02) != 0) {
                    result = 1;
                }
            }
        }
        break;
    case METARULE_CRITTER_KILL_TYPE:
        result = critterGetKillType((Object*)param);
        break;
    case METARULE_SET_CAR_CARRY_AMOUNT:
        if (1) {
            Proto* proto;
            if (protoGetProto(PROTO_ID_CAR_TRUNK, &proto) != -1) {
                proto->item.data.container.maxSize = param;
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

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// anim
// 0x4598BC
void opAnim(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to anim", program->name, arg);
        }
    }

    Object* obj = (Object*)data[2];
    int anim = data[1];
    int frame = data[0];

    if (obj == NULL) {
        scriptPredefinedError(program, "anim", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (anim < ANIM_COUNT) {
        CritterCombatData* combatData = NULL;
        if ((obj->pid >> 24) == OBJ_TYPE_CRITTER) {
            combatData = &(obj->data.critter.combat);
        }

        anim = _correctDeath(obj, anim, true);

        reg_anim_begin(1);

        // TODO: Not sure about the purpose, why it handles knock down flag?
        if (frame == 0) {
            reg_anim_animate(obj, anim, 0);
            if (anim >= ANIM_FALL_BACK && anim <= ANIM_FALL_FRONT_BLOOD) {
                int fid = buildFid(1, obj->fid & 0xFFF, anim + 28, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 28);
                reg_anim_17(obj, fid, -1);
            }

            if (combatData != NULL) {
                combatData->results &= DAM_KNOCKED_DOWN;
            }
        } else {
            int fid = buildFid((obj->fid & 0xF000000) >> 24, obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 24);
            reg_anim_animate_reverse(obj, anim, 0);

            if (anim == ANIM_PRONE_TO_STANDING) {
                fid = buildFid((obj->fid & 0xF000000) >> 24, obj->fid & 0xFFF, ANIM_FALL_FRONT_SF, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 24);
            } else if (anim == ANIM_BACK_TO_STANDING) {
                fid = buildFid((obj->fid & 0xF000000) >> 24, obj->fid & 0xFFF, ANIM_FALL_BACK_SF, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 24);
            }

            if (combatData != NULL) {
                combatData->results |= DAM_KNOCKED_DOWN;
            }

            reg_anim_17(obj, fid, -1);
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
void opObjectCarryingObjectByPid(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to obj_carrying_pid_obj", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int pid = data[0];

    Object* result = NULL;
    if (object != NULL) {
        result = objectGetCarriedObjectByPid(object, pid);
    } else {
        scriptPredefinedError(program, "obj_carrying_pid_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, (int)result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// reg_anim_func
// 0x459C20
void opRegAnimFunc(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to reg_anim_func", program->name, arg);
        }
    }

    int cmd = data[1];
    int param = data[0];

    if (!isInCombat()) {
        switch (cmd) {
        case OP_REG_ANIM_FUNC_BEGIN:
            reg_anim_begin(param);
            break;
        case OP_REG_ANIM_FUNC_CLEAR:
            reg_anim_clear((Object*)param);
            break;
        case OP_REG_ANIM_FUNC_END:
            reg_anim_end();
            break;
        }
    }
}

// reg_anim_animate
// 0x459CD4
void opRegAnimAnimate(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to reg_anim_animate", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int anim = data[1];
    int delay = data[0];

    if (!isInCombat()) {
        int violenceLevel = VIOLENCE_LEVEL_NONE;
        if (anim != 20 || object == NULL || object->pid != 0x100002F || (configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violenceLevel) && violenceLevel >= 2)) {
            if (object != NULL) {
                reg_anim_animate(object, anim, delay);
            } else {
                scriptPredefinedError(program, "reg_anim_animate", SCRIPT_ERROR_OBJECT_IS_NULL);
            }
        }
    }
}

// reg_anim_animate_reverse
// 0x459DC4
void opRegAnimAnimateReverse(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to reg_anim_animate_reverse", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int anim = data[1];
    int delay = data[0];

    if (!isInCombat()) {
        if (object != NULL) {
            reg_anim_animate_reverse(object, anim, delay);
        } else {
            scriptPredefinedError(program, "reg_anim_animate_reverse", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// reg_anim_obj_move_to_obj
// 0x459E74
void opRegAnimObjectMoveToObject(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to reg_anim_obj_move_to_obj", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    Object* dest = (Object*)data[1];
    int delay = data[0];

    if (!isInCombat()) {
        if (object != NULL) {
            reg_anim_obj_move_to_obj(object, dest, -1, delay);
        } else {
            scriptPredefinedError(program, "reg_anim_obj_move_to_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// reg_anim_obj_run_to_obj
// 0x459F28
void opRegAnimObjectRunToObject(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to reg_anim_obj_run_to_obj", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    Object* dest = (Object*)data[1];
    int delay = data[0];

    if (!isInCombat()) {
        if (object != NULL) {
            reg_anim_obj_run_to_obj(object, dest, -1, delay);
        } else {
            scriptPredefinedError(program, "reg_anim_obj_run_to_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// reg_anim_obj_move_to_tile
// 0x459FDC
void opRegAnimObjectMoveToTile(Program* prg)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(prg);
        data[arg] = programStackPopInt32(prg);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(prg, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to reg_anim_obj_move_to_tile", prg->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int tile = data[1];
    int delay = data[0];

    if (!isInCombat()) {
        if (object != NULL) {
            reg_anim_obj_move_to_tile(object, tile, object->elevation, -1, delay);
        } else {
            scriptPredefinedError(prg, "reg_anim_obj_move_to_tile", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// reg_anim_obj_run_to_tile
// 0x45A094
void opRegAnimObjectRunToTile(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to reg_anim_obj_run_to_tile", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    int tile = data[1];
    int delay = data[0];

    if (!isInCombat()) {
        if (object != NULL) {
            reg_anim_obj_run_to_tile(object, tile, object->elevation, -1, delay);
        } else {
            scriptPredefinedError(program, "reg_anim_obj_run_to_tile", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// play_gmovie
// 0x45A14C
void opPlayGameMovie(Program* program)
{
    unsigned short flags[MOVIE_COUNT];
    memcpy(flags, word_453F9C, sizeof(word_453F9C));

    program->flags |= PROGRAM_FLAG_0x20;

    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to play_gmovie", program->name);
    }

    gameDialogDisable();

    if (gameMoviePlay(data, word_453F9C[data]) == -1) {
        debugPrint("\nError playing movie %d!", data);
    }

    gameDialogEnable();

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// add_mult_objs_to_inven
// 0x45A200
void opAddMultipleObjectsToInventory(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to add_mult_objs_to_inven", program->name, arg);
        }
    }

    Object* object = (Object*)data[2];
    Object* item = (Object*)data[1];
    int quantity = data[0];

    if (object == NULL || item == NULL) {
        return;
    }

    if (quantity < 0) {
        quantity = 1;
    } else if (quantity > 99999) {
        quantity = 500;
    }

    if (itemAdd(object, item, quantity) == 0) {
        Rect rect;
        _obj_disconnect(item, &rect);
        tileWindowRefreshRect(&rect, item->elevation);
    }
}

// rm_mult_objs_from_inven
// 0x45A2D4
void opRemoveMultipleObjectsFromInventory(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to rm_mult_objs_from_inven", program->name, arg);
        }
    }

    Object* owner = (Object*)data[2];
    Object* item = (Object*)data[1];
    int quantityToRemove = data[0];

    if (owner == NULL || item == NULL) {
        // FIXME: Ruined stack.
        return;
    }

    bool itemWasEquipped = (item->flags & OBJECT_EQUIPPED) != 0;

    int quantity = _item_count(owner, item);
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

    programStackPushInt32(program, quantity);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// get_month
// 0x45A40C
void opGetMonth(Program* program)
{
    int month;
    gameTimeGetDate(&month, NULL, NULL);

    programStackPushInt32(program, month);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// get_day
// 0x45A43C
void opGetDay(Program* program)
{
    int day;
    gameTimeGetDate(NULL, &day, NULL);

    programStackPushInt32(program, day);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// explosion
// 0x45A46C
void opExplosion(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to explosion", program->name, arg);
        }
    }

    int tile = data[2];
    int elevation = data[1];
    int maxDamage = data[0];

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
void opGetDaysSinceLastVisit(Program* program)
{
    int days;

    if (gMapHeader.field_38 != 0) {
        days = (gameTimeGetTime() - gMapHeader.field_38) / GAME_TIME_TICKS_PER_DAY;
    } else {
        days = -1;
    }

    programStackPushInt32(program, days);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// gsay_start
// 0x45A56C
void _op_gsay_start(Program* program)
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
void _op_gsay_end(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;
    _gdialogGo();
    program->flags &= ~PROGRAM_FLAG_0x20;
}

// gsay_reply
// 0x45A5D4
void _op_gsay_reply(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    opcode_t opcode[2];
    int data[2];

    char* string = NULL;
    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            if (arg == 0) {
                if ((opcode[arg] & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
                    string = programGetString(program, opcode[arg], data[arg]);
                } else {
                    programFatalError("script error: %s: invalid arg %d to gsay_reply", program->name, arg);
                }
            } else {
                programFatalError("script error: %s: invalid arg %d to gsay_reply", program->name, arg);
            }
        }
    }

    int messageListId = data[1];
    int messageId = data[0];

    if (string != NULL) {
        gameDialogSetTextReply(program, messageListId, string);
    } else {
        gameDialogSetMessageReply(program, messageListId, messageId);
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// gsay_option
// 0x45A6C4
void _op_gsay_option(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    opcode_t opcode[4];
    int data[4];

    // TODO: Original code is slightly different, does not use loop for first
    // two args, but uses loop for two last args.
    char* string = NULL;
    for (int arg = 0; arg < 4; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            if (arg == 2) {
                if ((opcode[arg] & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
                    string = programGetString(program, opcode[arg], data[arg]);
                } else {
                    programFatalError("script error: %s: invalid arg %d to gsay_option", program->name, arg);
                }
            } else {
                programFatalError("script error: %s: invalid arg %d to gsay_option", program->name, arg);
            }
        }
    }

    int messageListId = data[3];
    int messageId = data[2];
    int proc = data[1];
    int reaction = data[0];

    // TODO: Not sure about this, needs testing.
    if ((opcode[1] & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        char* procName = programGetString(program, opcode[1], data[1]);
        if (string != NULL) {
            gameDialogAddTextOptionWithProcIdentifier(data[3], string, procName, reaction);
        } else {
            gameDialogAddMessageOptionWithProcIdentifier(data[3], data[2], procName, reaction);
        }
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if ((opcode[1] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 3 to sayOption");
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if (string != NULL) {
        gameDialogAddTextOptionWithProc(data[3], string, proc, reaction);
        program->flags &= ~PROGRAM_FLAG_0x20;
    } else {
        gameDialogAddMessageOptionWithProc(data[3], data[2], proc, reaction);
        program->flags &= ~PROGRAM_FLAG_0x20;
    }
}

// gsay_message
// 0x45A8AC
void _op_gsay_message(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    opcode_t opcode[3];
    int data[3];

    char* string = NULL;

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            if (arg == 1) {
                if ((opcode[arg] & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
                    string = programGetString(program, opcode[arg], data[arg]);
                } else {
                    programFatalError("script error: %s: invalid arg %d to gsay_message", program->name, arg);
                }
            } else {
                programFatalError("script error: %s: invalid arg %d to gsay_message", program->name, arg);
            }
        }
    }

    int messageListId = data[2];
    int messageId = data[1];
    int reaction = data[0];

    if (string != NULL) {
        gameDialogSetTextReply(program, messageListId, string);
    } else {
        gameDialogSetMessageReply(program, messageListId, messageId);
    }

    gameDialogAddMessageOptionWithProcIdentifier(-2, -2, NULL, 50);
    _gdialogSayMessage();

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// giq_option
// 0x45A9B4
void _op_giq_option(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    opcode_t opcode[5];
    int data[5];

    char* string = NULL;

    for (int arg = 0; arg < 5; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            if (arg == 2) {
                if ((opcode[arg] & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
                    string = programGetString(program, opcode[arg], data[arg]);
                } else {
                    programFatalError("script error: %s: invalid arg %d to giq_option", program->name, arg);
                }
            } else {
                programFatalError("script error: %s: invalid arg %d to giq_option", program->name, arg);
            }
        }
    }

    int iq = data[4];
    int messageListId = data[3];
    int messageId = data[2];
    int proc = data[1];
    int reaction = data[0];

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

    if ((opcode[1] & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        char* procName = programGetString(program, opcode[1], data[1]);
        if (string != NULL) {
            gameDialogAddTextOptionWithProcIdentifier(messageListId, string, procName, reaction);
        } else {
            gameDialogAddMessageOptionWithProcIdentifier(messageListId, messageId, procName, reaction);
        }
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if ((opcode[1] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        programFatalError("Invalid arg 4 to sayOption");
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if (string != NULL) {
        gameDialogAddTextOptionWithProc(messageListId, string, proc, reaction);
        program->flags &= ~PROGRAM_FLAG_0x20;
    } else {
        gameDialogAddMessageOptionWithProc(messageListId, messageId, proc, reaction);
        program->flags &= ~PROGRAM_FLAG_0x20;
    }
}

// poison
// 0x45AB90
void opPoison(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to poison", program->name, arg);
        }
    }

    Object* obj = (Object*)data[1];
    int amount = data[0];

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
void opGetPoison(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to get_poison", program->name);
    }

    Object* obj = (Object*)data;

    int poison = 0;
    if (obj != NULL) {
        if (obj->pid >> 24 == 1) {
            poison = critterGetPoison(obj);
        } else {
            debugPrint("\nScript Error: get_poison: who is not a critter!");
        }
    } else {
        scriptPredefinedError(program, "get_poison", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, poison);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// party_add
// 0x45ACF4
void opPartyAdd(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to party_add", program->name);
    }

    Object* object = (Object*)data;
    if (object == NULL) {
        scriptPredefinedError(program, "party_add", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    partyMemberAdd(object);
}

// party_remove
// 0x45AD68
void opPartyRemove(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to party_remove", program->name);
    }

    Object* object = (Object*)data;
    if (object == NULL) {
        scriptPredefinedError(program, "party_remove", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    partyMemberRemove(object);
}

// reg_anim_animate_forever
// 0x45ADDC
void opRegAnimAnimateForever(Program* prg)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(prg);
        data[arg] = programStackPopInt32(prg);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(prg, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to reg_anim_animate_forever", prg->name, arg);
        }
    }

    Object* obj = (Object*)data[1];
    int anim = data[0];

    if (!isInCombat()) {
        if (obj != NULL) {
            reg_anim_animate_forever(obj, anim, -1);
        } else {
            scriptPredefinedError(prg, "reg_anim_animate_forever", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// critter_injure
// 0x45AE8C
void opCritterInjure(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to critter_injure", program->name, arg);
        }
    }

    Object* critter = (Object*)data[1];
    int flags = data[0];

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
        if ((flags & (DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT)) != 0) {
            int leftItemAction;
            int rightItemAction;
            interfaceGetItemActions(&leftItemAction, &rightItemAction);
            interfaceUpdateItems(true, leftItemAction, rightItemAction);
        }
    }
}

// combat_is_initialized
// 0x45AF7C
void opCombatIsInitialized(Program* program)
{
    programStackPushInt32(program, isInCombat());
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// gdialog_barter
// 0x45AFA0
void _op_gdialog_barter(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to gdialog_barter", program->name);
    }

    if (gameDialogBarter(data) == -1) {
        debugPrint("\nScript Error: gdialog_barter: failed");
    }
}

// difficulty_level
// 0x45B010
void opGetGameDifficulty(Program* program)
{
    int gameDifficulty;
    if (!configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, &gameDifficulty)) {
        gameDifficulty = GAME_DIFFICULTY_NORMAL;
    }

    programStackPushInt32(program, gameDifficulty);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// running_burning_guy
// 0x45B05C
void opGetRunningBurningGuy(Program* program)
{
    int runningBurningGuy;
    if (!configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_BURNING_GUY_KEY, &runningBurningGuy)) {
        runningBurningGuy = 1;
    }

    programStackPushInt32(program, runningBurningGuy);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// inven_unwield
void _op_inven_unwield(Program* program)
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
void opObjectIsLocked(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to obj_is_locked", program->name);
    }

    Object* object = (Object*)data;

    bool locked = false;
    if (object != NULL) {
        locked = objectIsLocked(object);
    } else {
        scriptPredefinedError(program, "obj_is_locked", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, locked);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// obj_lock
// 0x45B16C
void opObjectLock(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to obj_lock", program->name);
    }

    Object* object = (Object*)data;

    if (object != NULL) {
        objectLock(object);
    } else {
        scriptPredefinedError(program, "obj_lock", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// obj_unlock
// 0x45B1E0
void opObjectUnlock(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to obj_unlock", program->name);
    }

    Object* object = (Object*)data;

    if (object != NULL) {
        objectUnlock(object);
    } else {
        scriptPredefinedError(program, "obj_unlock", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// obj_is_open
// 0x45B254
void opObjectIsOpen(Program* s)
{
    opcode_t opcode = programStackPopInt16(s);
    int data = programStackPopInt32(s);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(s, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to obj_is_open", s->name);
    }

    Object* object = (Object*)data;

    bool isOpen = false;
    if (object != NULL) {
        isOpen = objectIsOpen(object);
    } else {
        scriptPredefinedError(s, "obj_is_open", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(s, isOpen);
    programStackPushInt16(s, VALUE_TYPE_INT);
}

// obj_open
// 0x45B2E8
void opObjectOpen(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to obj_open", program->name);
    }

    Object* object = (Object*)data;

    if (object != NULL) {
        objectOpen(object);
    } else {
        scriptPredefinedError(program, "obj_open", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// obj_close
// 0x45B35C
void opObjectClose(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to obj_close", program->name);
    }

    Object* object = (Object*)data;

    if (object != NULL) {
        objectClose(object);
    } else {
        scriptPredefinedError(program, "obj_close", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// game_ui_disable
// 0x45B3D0
void opGameUiDisable(Program* program)
{
    gameUiDisable(0);
}

// game_ui_enable
// 0x45B3D8
void opGameUiEnable(Program* program)
{
    gameUiEnable();
}

// game_ui_is_disabled
// 0x45B3E0
void opGameUiIsDisabled(Program* program)
{
    programStackPushInt32(program, gameUiIsDisabled());
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// gfade_out
// 0x45B404
void opFadeOut(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to gfade_out", program->name);
    }

    if (data != 0) {
        paletteFadeTo(gPaletteBlack);
    } else {
        scriptPredefinedError(program, "gfade_out", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// gfade_in
// 0x45B47C
void opFadeIn(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to gfade_in", program->name);
    }

    if (data != 0) {
        paletteFadeTo(_cmap);
    } else {
        scriptPredefinedError(program, "gfade_in", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// item_caps_total
// 0x45B4F4
void opItemCapsTotal(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to item_caps_total", program->name);
    }

    Object* object = (Object*)data;

    int amount = 0;
    if (object != NULL) {
        amount = itemGetTotalCaps(object);
    } else {
        scriptPredefinedError(program, "item_caps_total", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, amount);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// item_caps_adjust
// 0x45B588
void opItemCapsAdjust(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to item_caps_adjust", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int amount = data[0];

    int rc = -1;

    if (object != NULL) {
        rc = itemCapsAdjust(object, amount);
    } else {
        scriptPredefinedError(program, "item_caps_adjust", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, rc);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// anim_action_frame
void _op_anim_action_frame(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to anim_action_frame", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int anim = data[0];

    int actionFrame = 0;

    if (object != NULL) {
        int fid = buildFid((object->fid & 0xF000000) >> 24, object->fid & 0xFFF, anim, 0, object->rotation);
        CacheEntry* frmHandle;
        Art* frm = artLock(fid, &frmHandle);
        if (frm != NULL) {
            actionFrame = artGetActionFrame(frm);
            artUnlock(frmHandle);
        }
    } else {
        scriptPredefinedError(program, "anim_action_frame", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, actionFrame);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// reg_anim_play_sfx
// 0x45B740
void opRegAnimPlaySfx(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if (arg == 1) {
            if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_STRING) {
                programFatalError("script error: %s: invalid arg %d to reg_anim_play_sfx", program->name, arg);
            }
        } else {
            if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
                programFatalError("script error: %s: invalid arg %d to reg_anim_play_sfx", program->name, arg);
            }
        }
    }

    Object* obj = (Object*)data[2];
    int name = data[1];
    int delay = data[0];

    char* soundEffectName = programGetString(program, opcode[1], name);
    if (soundEffectName == NULL) {
        scriptPredefinedError(program, "reg_anim_play_sfx", SCRIPT_ERROR_FOLLOWS);
        debugPrint(" Can't match string!");
    }

    if (obj != NULL) {
        reg_anim_play_sfx(obj, soundEffectName, delay);
    } else {
        scriptPredefinedError(program, "reg_anim_play_sfx", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// critter_mod_skill
// 0x45B840
void opCritterModifySkill(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to critter_mod_skill", program->name, arg);
        }
    }

    Object* critter = (Object*)data[2];
    int skill = data[1];
    int points = data[0];

    if (critter != NULL && points != 0) {
        if (critter->pid >> 24 == OBJ_TYPE_CRITTER) {
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

    programStackPushInt32(program, 0);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// sfx_build_char_name
// 0x45B9C4
void opSfxBuildCharName(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to sfx_build_char_name", program->name, arg);
        }
    }

    Object* obj = (Object*)data[2];
    int anim = data[1];
    int extra = data[0];

    int stringOffset = 0;

    if (obj != NULL) {
        char soundEffectName[16];
        strcpy(soundEffectName, sfxBuildCharName(obj, anim, extra));
        stringOffset = programPushString(program, soundEffectName);
    } else {
        scriptPredefinedError(program, "sfx_build_char_name", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, stringOffset);
    programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);
}

// sfx_build_ambient_name
// 0x45BAA8
void opSfxBuildAmbientName(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to sfx_build_ambient_name", program->name);
    }

    char* baseName = programGetString(program, opcode, data);

    char soundEffectName[16];
    strcpy(soundEffectName, gameSoundBuildAmbientSoundEffectName(baseName));

    int stringOffset = programPushString(program, soundEffectName);

    programStackPushInt32(program, stringOffset);
    programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);
}

// sfx_build_interface_name
// 0x45BB54
void opSfxBuildInterfaceName(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to sfx_build_interface_name", program->name);
    }

    char* baseName = programGetString(program, opcode, data);

    char soundEffectName[16];
    strcpy(soundEffectName, gameSoundBuildInterfaceName(baseName));

    int stringOffset = programPushString(program, soundEffectName);

    programStackPushInt32(program, stringOffset);
    programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);
}

// sfx_build_item_name
// 0x45BC00
void opSfxBuildItemName(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to sfx_build_item_name", program->name);
    }

    const char* baseName = programGetString(program, opcode, data);

    char soundEffectName[16];
    strcpy(soundEffectName, gameSoundBuildInterfaceName(baseName));

    int stringOffset = programPushString(program, soundEffectName);

    programStackPushInt32(program, stringOffset);
    programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);
}

// sfx_build_weapon_name
// 0x45BCAC
void opSfxBuildWeaponName(Program* program)
{
    opcode_t opcode[4];
    int data[4];

    for (int arg = 0; arg < 4; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to sfx_build_weapon_name", program->name, arg);
        }
    }

    int weaponSfxType = data[3];
    Object* weapon = (Object*)data[2];
    int hitMode = data[1];
    Object* target = (Object*)data[0];

    char soundEffectName[16];
    strcpy(soundEffectName, sfxBuildWeaponName(weaponSfxType, weapon, hitMode, target));

    int stringOffset = programPushString(program, soundEffectName);

    programStackPushInt32(program, stringOffset);
    programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);
}

// sfx_build_scenery_name
// 0x45BD7C
void opSfxBuildSceneryName(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to sfx_build_scenery_name", program->name, arg);
        }
    }

    int action = data[1];
    int actionType = data[0];

    char* baseName = programGetString(program, opcode[2], data[2]);

    char soundEffectName[16];
    strcpy(soundEffectName, sfxBuildSceneryName(actionType, action, baseName));

    int stringOffset = programPushString(program, soundEffectName);

    programStackPushInt32(program, stringOffset);
    programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);
}

// sfx_build_open_name
// 0x45BE58
void opSfxBuildOpenName(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to sfx_build_open_name", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int action = data[0];

    int stringOffset = 0;

    if (object != NULL) {
        char soundEffectName[16];
        strcpy(soundEffectName, sfxBuildOpenName(object, action));

        stringOffset = programPushString(program, soundEffectName);
    } else {
        scriptPredefinedError(program, "sfx_build_open_name", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, stringOffset);
    programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);
}

// attack_setup
// 0x45BF38
void opAttackSetup(Program* program)
{
    opcode_t opcodes[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcodes[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcodes[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcodes[arg], data[arg]);
        }

        if ((opcodes[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to attack_setup", program->name, arg);
        }
    }

    Object* attacker = (Object*)data[1];
    Object* defender = (Object*)data[0];

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
            if ((attacker->data.critter.combat.maneuver & CRITTER_MANEUVER_0x01) == 0) {
                attacker->data.critter.combat.maneuver |= CRITTER_MANEUVER_0x01;
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

            // FIXME: Something bad here, when attacker and defender are
            // the same object, these objects are used as flags, which
            // are later used in 0x422F3C as flags of defender.
            if (data[1] == data[0]) {
                attack.field_1C = 1;
                attack.field_20 = data[1];
                attack.field_24 = data[0];
            } else {
                attack.field_1C = 0;
            }

            scriptsRequestCombat(&attack);
        }
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// destroy_mult_objs
// 0x45C0E8
void opDestroyMultipleObjects(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to destroy_mult_objs", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int quantity = data[0];

    Object* self = scriptGetSelf(program);
    bool isSelf = self == object;

    int result = 0;

    if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
        _combat_delete_critter(object);
    }

    Object* owner = objectGetOwner(object);
    if (owner != NULL) {
        int quantityToDestroy = _item_count(owner, object);
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
            object->flags |= (OBJECT_HIDDEN | OBJECT_TEMPORARY);
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

    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);

    program->flags &= ~PROGRAM_FLAG_0x20;

    if (isSelf) {
        program->flags |= PROGRAM_FLAG_0x0100;
    }
}

// use_obj_on_obj
// 0x45C290
void opUseObjectOnObject(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to use_obj_on_obj", program->name, arg);
        }
    }

    Object* item = (Object*)data[1];
    Object* target = (Object*)data[0];

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
    if ((self->pid >> 24) == OBJ_TYPE_CRITTER) {
        _action_use_an_item_on_object(self, target, item);
    } else {
        _obj_use_item_on(self, target, item);
    }
}

// endgame_slideshow
// 0x45C3B0
void opEndgameSlideshow(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;
    scriptsRequestEndgame();
    program->flags &= ~PROGRAM_FLAG_0x20;
}

// move_obj_inven_to_obj
// 0x45C3D0
void opMoveObjectInventoryToObject(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to move_obj_inven_to_obj", program->name, arg);
        }
    }

    Object* object1 = (Object*)data[1];
    Object* object2 = (Object*)data[0];

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

    _item_move_all(object1, object2);

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
void opEndgameMovie(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;
    endgamePlayMovie();
    program->flags &= ~PROGRAM_FLAG_0x20;
}

// obj_art_fid
// 0x45C56C
void opGetObjectFid(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to obj_art_fid", program->name);
    }

    Object* object = (Object*)data;

    int fid = 0;
    if (object != NULL) {
        fid = object->fid;
    } else {
        scriptPredefinedError(program, "obj_art_fid", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, fid);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// art_anim
// 0x45C5F8
void opGetFidAnim(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to art_anim", program->name);
    }

    programStackPushInt32(program, (data & 0xFF0000) >> 16);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// party_member_obj
// 0x45C66C
void opGetPartyMember(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to party_member_obj", program->name);
    }

    Object* object = partyMemberFindByPid(data);
    programStackPushInt32(program, (int)object);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// rotation_to_tile
// 0x45C6DC
void opGetRotationToTile(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to rotation_to_tile", program->name, arg);
        }
    }

    int tile1 = data[1];
    int tile2 = data[0];

    int rotation = tileGetRotationTo(tile1, tile2);
    programStackPushInt32(program, rotation);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// jam_lock
// 0x45C778
void opJamLock(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to jam_lock", program->name);
    }

    Object* object = (Object*)data;

    objectJamLock(object);
}

// gdialog_set_barter_mod
// 0x45C7D4
void opGameDialogSetBarterMod(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to gdialog_set_barter_mod", program->name);
    }

    gameDialogSetBarterModifier(data);
}

// combat_difficulty
// 0x45C830
void opGetCombatDifficulty(Program* program)
{
    int combatDifficulty;
    if (!configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, &combatDifficulty)) {
        combatDifficulty = 0;
    }

    programStackPushInt32(program, combatDifficulty);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// obj_on_screen
// 0x45C878
void opObjectOnScreen(Program* program)
{
    Rect rect;
    rectCopy(&rect, &stru_453FC0);

    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to obj_on_screen", program->name);
    }

    Object* object = (Object*)data;

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

    //debugPrint("ObjOnScreen: %d\n", result);
    programStackPushInt32(program, result);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// critter_is_fleeing
// 0x45C93C
void opCritterIsFleeing(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to critter_is_fleeing", program->name);
    }

    Object* obj = (Object*)data;

    bool fleeing = false;
    if (obj != NULL) {
        fleeing = (obj->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0;
    } else {
        scriptPredefinedError(program, "critter_is_fleeing", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInt32(program, fleeing);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// critter_set_flee_state
// 0x45C9DC
void opCritterSetFleeState(Program* program)
{
    opcode_t opcode[2];
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & 0xF7FF) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to critter_set_flee_state", program->name, arg);
        }
    }

    Object* object = (Object*)data[1];
    int fleeing = data[0];

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
void opTerminateCombat(Program* program)
{
    if (isInCombat()) {
        _game_user_wants_to_quit = 1;
        Object* self = scriptGetSelf(program);
        if (self != NULL) {
            if ((self->pid >> 24) == 1) {
                self->data.critter.combat.maneuver |= CRITTER_MANEUVER_STOP_ATTACKING;
                self->data.critter.combat.whoHitMe = NULL;
                _combatAIInfoSetLastTarget(self, NULL);
            }
        }
    }
}

// debug_msg
// 0x45CAC8
void opDebugMessage(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_STRING) {
        programFatalError("script error: %s: invalid arg to debug_msg", program->name);
    }

    char* string = programGetString(program, opcode, data);

    if (string != NULL) {
        bool showScriptMessages = false;
        configGetBool(&gGameConfig, GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_SCRIPT_MESSAGES_KEY, &showScriptMessages);
        if (showScriptMessages) {
            debugPrint("\n");
            debugPrint(string);
        }
    }
}

// critter_stop_attacking
// 0x45CB70
void opCritterStopAttacking(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to critter_stop_attacking", program->name);
    }

    Object* obj = (Object*)data;

    if (obj != NULL) {
        obj->data.critter.combat.maneuver |= CRITTER_MANEUVER_STOP_ATTACKING;
        obj->data.critter.combat.whoHitMe = NULL;
        _combatAIInfoSetLastTarget(obj, NULL);
    } else {
        scriptPredefinedError(program, "critter_stop_attacking", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// tile_contains_pid_obj
// 0x45CBF8
void opTileGetObjectWithPid(Program* program)
{
    opcode_t opcode[3];
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        opcode[arg] = programStackPopInt16(program);
        data[arg] = programStackPopInt32(program);

        if (opcode[arg] == VALUE_TYPE_DYNAMIC_STRING) {
            programPopString(program, opcode[arg], data[arg]);
        }

        if ((opcode[arg] & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
            programFatalError("script error: %s: invalid arg %d to tile_contains_pid_obj", program->name, arg);
        }
    }

    int tile = data[2];
    int elevation = data[1];
    int pid = data[0];
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

    programStackPushInt32(program, (int)found);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// obj_name
// 0x45CCC8
void opGetObjectName(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to obj_name", program->name);
    }

    Object* obj = (Object*)data;
    if (obj != NULL) {
        _strName = objectGetName(obj);
    } else {
        scriptPredefinedError(program, "obj_name", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    int stringOffset = programPushString(program, _strName);

    programStackPushInt32(program, stringOffset);
    programStackPushInt16(program, VALUE_TYPE_DYNAMIC_STRING);
}

// get_pc_stat
// 0x45CD64
void opGetPcStat(Program* program)
{
    opcode_t opcode = programStackPopInt16(program);
    int data = programStackPopInt32(program);

    if (opcode == VALUE_TYPE_DYNAMIC_STRING) {
        programPopString(program, opcode, data);
    }

    if ((opcode & 0xF7FF) != VALUE_TYPE_INT) {
        programFatalError("script error: %s: invalid arg to get_pc_stat", program->name);
    }

    int value = pcGetStat(data);
    programStackPushInt32(program, value);
    programStackPushInt16(program, VALUE_TYPE_INT);
}

// 0x45CDD4
void _intExtraClose_()
{
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
    interpreterRegisterOpcode(0x8136, opFadeOut); // op_gfade_out
    interpreterRegisterOpcode(0x8137, opFadeIn); // op_gfade_in
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
}

// 0x45D878
void _intExtraRemoveProgramReferences_()
{
}
