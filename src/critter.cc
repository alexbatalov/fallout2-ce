#include "critter.h"

#include "animation.h"
#include "character_editor.h"
#include "combat.h"
#include "debug.h"
#include "display_monitor.h"
#include "endgame.h"
#include "game.h"
#include "geometry.h"
#include "interface.h"
#include "item.h"
#include "map.h"
#include "memory.h"
#include "object.h"
#include "party_member.h"
#include "platform_compat.h"
#include "proto.h"
#include "queue.h"
#include "random.h"
#include "reaction.h"
#include "scripts.h"
#include "skill.h"
#include "stat.h"
#include "tile.h"
#include "trait.h"
#include "world_map.h"

#include <stdio.h>
#include <string.h>

// 0x50141C
char _aCorpse[] = "corpse";

// 0x501494
char byte_501494[] = "";

// 0x51833C
char* _name_critter = _aCorpse;

// Modifiers to endurance for performing radiation damage check.
//
// 0x518340
const int gRadiationEnduranceModifiers[RADIATION_LEVEL_COUNT] = {
    2,
    0,
    -2,
    -4,
    -6,
    -8,
};

// List of stats affected by radiation.
//
// The values of this list specify stats that can be affected by radiation.
// The amount of penalty to every stat (identified by index) is stored
// separately in [gRadiationEffectPenalties] per radiation level.
//
// The order of stats is important - primary stats must be at the top. See
// [RADIATION_EFFECT_PRIMARY_STAT_COUNT] for more info.
//
// 0x518358
const int gRadiationEffectStats[RADIATION_EFFECT_COUNT] = {
    STAT_STRENGTH,
    STAT_PERCEPTION,
    STAT_ENDURANCE,
    STAT_CHARISMA,
    STAT_INTELLIGENCE,
    STAT_AGILITY,
    STAT_CURRENT_HIT_POINTS,
    STAT_HEALING_RATE,
};

// Denotes how many primary stats at the top of [gRadiationEffectStats] array.
// These stats are used to determine if critter is alive after applying
// radiation effects.
#define RADIATION_EFFECT_PRIMARY_STAT_COUNT 6

// List of stat modifiers caused by radiation at different radiation levels.
//
// 0x518378
const int gRadiationEffectPenalties[RADIATION_LEVEL_COUNT][RADIATION_EFFECT_COUNT] = {
    // clang-format off
    {   0,   0,   0,   0,   0,   0,   0,   0 },
    {  -1,   0,   0,   0,   0,   0,   0,   0 },
    {  -1,   0,   0,   0,   0,  -1,   0,  -3 },
    {  -2,   0,  -1,   0,   0,  -2,  -5,  -5 },
    {  -4,  -3,  -3,  -3,  -1,  -5, -15, -10 },
    {  -6,  -5,  -5,  -5,  -3,  -6, -20, -10 },
    // clang-format on
};

// 0x518438
Object* _critterClearObj = NULL;

// scrname.msg
//
// 0x56D754
MessageList gCritterMessageList;

// 0x56D75C
char gDudeName[DUDE_NAME_MAX_LENGTH];

// 0x56D77C
int _sneak_working;

// 0x56D780
int gKillsByType[KILL_TYPE_COUNT];

// Something with radiation.
//
// 0x56D7CC
int _old_rad_level;

// scrname_init
// 0x42CF50
int critterInit()
{
    dudeResetName();

    memset(gKillsByType, 0, sizeof(gKillsByType));

    if (!messageListInit(&gCritterMessageList)) {
        debugPrint("\nError: Initing critter name message file!");
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    sprintf(path, "%sscrname.msg", asc_5186C8);

    if (!messageListLoad(&gCritterMessageList, path)) {
        debugPrint("\nError: Loading critter name message file!");
        return -1;
    }

    return 0;
}

// 0x42CFE4
void critterReset()
{
    dudeResetName();
    memset(gKillsByType, 0, sizeof(gKillsByType));
}

// 0x42D004
void critterExit()
{
    messageListFree(&gCritterMessageList);
}

// 0x42D01C
int critterLoad(File* stream)
{
    if (fileReadInt32(stream, &_sneak_working) == -1) {
        return -1;
    }

    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    return protoCritterDataRead(stream, &(proto->critter.data));
}

// 0x42D058
int critterSave(File* stream)
{
    if (fileWriteInt32(stream, _sneak_working) == -1) {
        return -1;
    }

    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    return protoCritterDataWrite(stream, &(proto->critter.data));
}

// 0x42D094
void critterProtoDataCopy(CritterProtoData* dest, CritterProtoData* src)
{
    memcpy(dest, src, sizeof(CritterProtoData));
}

// 0x42D0A8
char* critterGetName(Object* obj)
{
    if (obj == gDude) {
        return gDudeName;
    }

    if (obj->field_80 == -1) {
        if (obj->sid != -1) {
            Script* script;
            if (scriptGetScript(obj->sid, &script) != -1) {
                obj->field_80 = script->field_14;
            }
        }
    }

    char* name = NULL;
    if (obj->field_80 != -1) {
        MessageListItem messageListItem;
        messageListItem.num = 101 + obj->field_80;
        if (messageListGetItem(&gCritterMessageList, &messageListItem)) {
            name = messageListItem.text;
        }
    }

    if (name == NULL || *name == '\0') {
        name = protoGetName(obj->pid);
    }

    _name_critter = name;

    return name;
}

// 0x42D138
int dudeSetName(const char* name)
{
    if (strlen(name) <= DUDE_NAME_MAX_LENGTH) {
        strncpy(gDudeName, name, DUDE_NAME_MAX_LENGTH);
        return 0;
    }

    return -1;
}

// 0x42D170
void dudeResetName()
{
    strncpy(gDudeName, "None", DUDE_NAME_MAX_LENGTH);
}

// 0x42D18C
int critterGetHitPoints(Object* critter)
{
    return (critter->pid >> 24) == OBJ_TYPE_CRITTER ? critter->data.critter.hp : 0;
}

// 0x42D1A4
int critterAdjustHitPoints(Object* critter, int a2)
{
    if ((critter->pid >> 24) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    int maximumHp = critterGetStat(critter, STAT_MAXIMUM_HIT_POINTS);
    int newHp = critter->data.critter.hp + a2;

    critter->data.critter.hp = newHp;
    if (maximumHp >= newHp) {
        if (newHp <= 0 && (critter->data.critter.combat.results & DAM_DEAD) == 0) {
            critterKill(critter, -1, true);
        }
    } else {
        critter->data.critter.hp = maximumHp;
    }

    return 0;
}

// 0x42D1F8
int critterGetPoison(Object* critter)
{
    return (critter->pid >> 24) == OBJ_TYPE_CRITTER ? critter->data.critter.poison : 0;
}

// Adjust critter's current poison by specified amount.
//
// For unknown reason this function only works on dude.
//
// The [amount] can either be positive (adds poison) or negative (removes
// poison).
//
// 0x42D210
int critterAdjustPoison(Object* critter, int amount)
{
    MessageListItem messageListItem;

    if (critter != gDude) {
        return -1;
    }

    if (amount > 0) {
        // Take poison resistance into account.
        amount -= amount * critterGetStat(critter, STAT_POISON_RESISTANCE) / 100;
    } else {
        if (gDude->data.critter.poison <= 0) {
            // Critter is not poisoned and we're want to decrease it even
            // further, which makes no sense.
            return 0;
        }
    }

    int newPoison = critter->data.critter.poison + amount;
    if (newPoison > 0) {
        critter->data.critter.poison = newPoison;

        _queue_clear_type(EVENT_TYPE_POISON, NULL);
        queueAddEvent(10 * (505 - 5 * newPoison), gDude, NULL, EVENT_TYPE_POISON);

        // You have been poisoned!
        messageListItem.num = 3000;
        if (amount < 0) {
            // You feel a little better.
            messageListItem.num = 3002;
        }
    } else {
        critter->data.critter.poison = 0;

        // You feel better.
        messageListItem.num = 3003;
    }

    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        displayMonitorAddMessage(messageListItem.text);
    }

    if (critter == gDude) {
        indicatorBarRefresh();
    }

    return 0;
}

// 0x42D318
int poisonEventProcess(Object* obj, void* data)
{
    if (obj != gDude) {
        return 0;
    }

    critterAdjustPoison(obj, -2);
    critterAdjustHitPoints(obj, -1);

    interfaceRenderHitPoints(false);

    MessageListItem messageListItem;
    // You take damage from poison.
    messageListItem.num = 3001;
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        displayMonitorAddMessage(messageListItem.text);
    }

    // NOTE: Uninline.
    int hitPoints = critterGetHitPoints(obj);
    if (hitPoints > 5) {
        return 0;
    }

    return 1;
}

// 0x42D38C
int critterGetRadiation(Object* obj)
{
    return (obj->pid >> 24) == OBJ_TYPE_CRITTER ? obj->data.critter.radiation : 0;
}

// 0x42D3A4
int critterAdjustRadiation(Object* obj, int amount)
{
    MessageListItem messageListItem;

    if (obj != gDude) {
        return -1;
    }

    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    if (amount > 0) {
        amount -= critterGetStat(obj, STAT_RADIATION_RESISTANCE) * amount / 100;
    }

    if (amount > 0) {
        proto->critter.data.flags |= 0x02;
    }

    if (amount > 0) {
        Object* geigerCounter = NULL;

        Object* item1 = critterGetItem1(gDude);
        if (item1 != NULL) {
            if (item1->pid == PROTO_ID_GEIGER_COUNTER_I || item1->pid == PROTO_ID_GEIGER_COUNTER_II) {
                geigerCounter = item1;
            }
        }

        Object* item2 = critterGetItem2(gDude);
        if (item2 != NULL) {
            if (item2->pid == PROTO_ID_GEIGER_COUNTER_I || item2->pid == PROTO_ID_GEIGER_COUNTER_II) {
                geigerCounter = item2;
            }
        }

        if (geigerCounter != NULL) {
            if (miscItemIsOn(geigerCounter)) {
                if (amount > 5) {
                    // The geiger counter is clicking wildly.
                    messageListItem.num = 1009;
                } else {
                    // The geiger counter is clicking.
                    messageListItem.num = 1008;
                }

                if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
                    displayMonitorAddMessage(messageListItem.text);
                }
            }
        }
    }

    if (amount >= 10) {
        // You have received a large dose of radiation.
        messageListItem.num = 1007;

        if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
    }

    obj->data.critter.radiation += amount;
    if (obj->data.critter.radiation < 0) {
        obj->data.critter.radiation = 0;
    }

    if (obj == gDude) {
        indicatorBarRefresh();
    }

    return 0;
}

// 0x42D4F4
int _critter_check_rads(Object* obj)
{
    if (obj != gDude) {
        return 0;
    }

    Proto* proto;
    protoGetProto(obj->pid, &proto);
    if ((proto->critter.data.flags & 0x02) == 0) {
        return 0;
    }

    _old_rad_level = 0;

    _queue_clear_type(EVENT_TYPE_RADIATION, _get_rad_damage_level);

    // NOTE: Uninline
    int radiation = critterGetRadiation(obj);

    int radiationLevel;
    if (radiation > 999)
        radiationLevel = RADIATION_LEVEL_FATAL;
    else if (radiation > 599)
        radiationLevel = RADIATION_LEVEL_DEADLY;
    else if (radiation > 399)
        radiationLevel = RADIATION_LEVEL_CRITICAL;
    else if (radiation > 199)
        radiationLevel = RADIATION_LEVEL_ADVANCED;
    else if (radiation > 99)
        radiationLevel = RADIATION_LEVEL_MINOR;
    else
        radiationLevel = RADIATION_LEVEL_NONE;

    if (statRoll(obj, STAT_ENDURANCE, gRadiationEnduranceModifiers[radiationLevel], NULL) <= ROLL_FAILURE) {
        radiationLevel++;
    }

    if (radiationLevel > _old_rad_level) {
        // Create timer event for applying radiation damage.
        RadiationEvent* radiationEvent = (RadiationEvent*)internal_malloc(sizeof(*radiationEvent));
        if (radiationEvent == NULL) {
            return 0;
        }

        radiationEvent->radiationLevel = radiationLevel;
        radiationEvent->isHealing = 0;
        queueAddEvent(36000 * randomBetween(4, 18), obj, radiationEvent, EVENT_TYPE_RADIATION);
    }

    proto->critter.data.flags &= ~(0x02);

    return 0;
}

// 0x42D618
int _get_rad_damage_level(Object* obj, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;

    _old_rad_level = radiationEvent->radiationLevel;

    return 0;
}

// 0x42D624
int _clear_rad_damage(Object* obj, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;

    if (radiationEvent->isHealing) {
        _process_rads(obj, radiationEvent->radiationLevel, true);
    }

    return 1;
}

// Applies radiation.
//
// 0x42D63C
void _process_rads(Object* obj, int radiationLevel, bool isHealing)
{
    MessageListItem messageListItem;

    if (radiationLevel == RADIATION_LEVEL_NONE) {
        return;
    }

    int radiationLevelIndex = radiationLevel - 1;
    int modifier = isHealing ? -1 : 1;

    if (obj == gDude) {
        // Radiation level message, higher is worse.
        messageListItem.num = 1000 + radiationLevelIndex;
        if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
    }

    for (int effect = 0; effect < RADIATION_EFFECT_COUNT; effect++) {
        int value = critterGetBonusStat(obj, gRadiationEffectStats[effect]);
        value += modifier * gRadiationEffectPenalties[radiationLevelIndex][effect];
        critterSetBonusStat(obj, gRadiationEffectStats[effect], value);
    }

    if ((obj->data.critter.combat.results & DAM_DEAD) == 0) {
        // Loop thru effects affecting primary stats. If any of the primary stat
        // dropped below minimal value, kill it.
        for (int effect = 0; effect < RADIATION_EFFECT_PRIMARY_STAT_COUNT; effect++) {
            int base = critterGetBaseStatWithTraitModifier(obj, gRadiationEffectStats[effect]);
            int bonus = critterGetBonusStat(obj, gRadiationEffectStats[effect]);
            if (base + bonus < PRIMARY_STAT_MIN) {
                critterKill(obj, -1, 1);
                break;
            }
        }
    }

    if ((obj->data.critter.combat.results & DAM_DEAD) != 0) {
        if (obj == gDude) {
            // You have died from radiation sickness.
            messageListItem.num = 1006;
            if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }
    }
}

// 0x42D740
int radiationEventProcess(Object* obj, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;
    if (!radiationEvent->isHealing) {
        // Schedule healing stats event in 7 days.
        RadiationEvent* newRadiationEvent = (RadiationEvent*)internal_malloc(sizeof(*newRadiationEvent));
        if (newRadiationEvent != NULL) {
            _queue_clear_type(EVENT_TYPE_RADIATION, _clear_rad_damage);
            newRadiationEvent->radiationLevel = radiationEvent->radiationLevel;
            newRadiationEvent->isHealing = 1;
            queueAddEvent(GAME_TIME_TICKS_PER_DAY * 7, obj, newRadiationEvent, EVENT_TYPE_RADIATION);
        }
    }

    _process_rads(obj, radiationEvent->radiationLevel, radiationEvent->isHealing);

    return 1;
}

// 0x42D7A0
int radiationEventRead(File* stream, void** dataPtr)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)internal_malloc(sizeof(*radiationEvent));
    if (radiationEvent == NULL) {
        return -1;
    }

    if (fileReadInt32(stream, &(radiationEvent->radiationLevel)) == -1) goto err;
    if (fileReadInt32(stream, &(radiationEvent->isHealing)) == -1) goto err;

    *dataPtr = radiationEvent;
    return 0;

err:

    internal_free(radiationEvent);
    return -1;
}

// 0x42D7FC
int radiationEventWrite(File* stream, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;

    if (fileWriteInt32(stream, radiationEvent->radiationLevel) == -1) return -1;
    if (fileWriteInt32(stream, radiationEvent->isHealing) == -1) return -1;

    return 0;
}

// 0x42D82C
int critterGetDamageType(Object* obj)
{
    if ((obj->pid >> 24) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    Proto* proto;
    if (protoGetProto(obj->pid, &proto) == -1) {
        return 0;
    }

    return proto->critter.data.damageType;
}

// 0x42D878
int killsIncByType(int killType)
{
    if (killType != -1 && killType < KILL_TYPE_COUNT) {
        gKillsByType[killType]++;
        return 0;
    }

    return -1;
}

// 0x42D8A8
int killsGetByType(int killType)
{
    if (killType != -1 && killType < KILL_TYPE_COUNT) {
        return gKillsByType[killType];
    }

    return 0;
}

// 0x42D8C0
int killsLoad(File* stream)
{
    if (fileReadInt32List(stream, gKillsByType, KILL_TYPE_COUNT) == -1) {
        fileClose(stream);
        return -1;
    }

    return 0;
}

// 0x42D8F0
int killsSave(File* stream)
{
    if (fileWriteInt32List(stream, gKillsByType, KILL_TYPE_COUNT) == -1) {
        fileClose(stream);
        return -1;
    }

    return 0;
}

// 0x42D920
int critterGetKillType(Object* obj)
{
    if (obj == gDude) {
        int gender = critterGetStat(obj, STAT_GENDER);
        if (gender == GENDER_FEMALE) {
            return KILL_TYPE_WOMAN;
        }
        return KILL_TYPE_MAN;
    }

    if ((obj->pid >> 24) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    Proto* proto;
    protoGetProto(obj->pid, &proto);

    return proto->critter.data.killType;
}

// 0x42D974
char* killTypeGetName(int killType)
{
    if (killType != -1 && killType < KILL_TYPE_COUNT) {
        if (killType >= 0 && killType < KILL_TYPE_COUNT) {
            MessageListItem messageListItem;
            return getmsg(&gProtoMessageList, &messageListItem, 1450 + killType);
        } else {
            return NULL;
        }
    } else {
        return byte_501494;
    }
}

// 0x42D9B4
char* killTypeGetDescription(int killType)
{
    if (killType != -1 && killType < KILL_TYPE_COUNT) {
        if (killType >= 0 && killType < KILL_TYPE_COUNT) {
            MessageListItem messageListItem;
            return getmsg(&gProtoMessageList, &messageListItem, 1469 + killType);
        } else {
            return NULL;
        }
    } else {
        return byte_501494;
    }
}

// 0x42D9F4
int _critter_heal_hours(Object* critter, int a2)
{
    if ((critter->pid >> 24) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    if (critter->data.critter.hp < critterGetStat(critter, STAT_MAXIMUM_HIT_POINTS)) {
        critterAdjustHitPoints(critter, 14 * (a2 / 3));
    }

    critter->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;

    return 0;
}

// 0x42DA54
int _critterClearObjDrugs(Object* obj, void* data)
{
    return obj == _critterClearObj;
}

// 0x42DA64
void critterKill(Object* critter, int anim, bool a3)
{
    if ((critter->pid >> 24) != OBJ_TYPE_CRITTER) {
        return;
    }

    int elevation = critter->elevation;

    partyMemberRemove(critter);

    // NOTE: Original code uses goto to jump out from nested conditions below.
    bool shouldChangeFid = false;
    int fid;
    if (_critter_is_prone(critter)) {
        int current = (critter->fid & 0xFF0000) >> 16;
        if (current == ANIM_FALL_BACK || current == ANIM_FALL_FRONT) {
            bool back = false;
            if (current == ANIM_FALL_BACK) {
                back = true;
            } else {
                fid = buildFid(1, critter->fid & 0xFFF, ANIM_FALL_FRONT_SF, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
                if (!artExists(fid)) {
                    back = true;
                }
            }

            if (back) {
                fid = buildFid(1, critter->fid & 0xFFF, ANIM_FALL_BACK_SF, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
            }

            shouldChangeFid = true;
        }
    } else {
        if (anim < 0) {
            anim = LAST_SF_DEATH_ANIM;
        }

        if (anim > LAST_SF_DEATH_ANIM) {
            debugPrint("\nError: Critter Kill: death_frame out of range!");
            anim = LAST_SF_DEATH_ANIM;
        }

        fid = buildFid(1, critter->fid & 0xFFF, anim, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
        _obj_fix_violence_settings(&fid);
        if (!artExists(fid)) {
            debugPrint("\nError: Critter Kill: Can't match fid!");

            fid = buildFid(1, critter->fid & 0xFFF, ANIM_FALL_BACK_BLOOD_SF, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
            _obj_fix_violence_settings(&fid);
        }

        shouldChangeFid = true;
    }

    Rect updatedRect;
    Rect tempRect;

    if (shouldChangeFid) {
        objectSetFrame(critter, 0, &updatedRect);

        objectSetFid(critter, fid, &tempRect);
        rectUnion(&updatedRect, &tempRect, &updatedRect);
    }

    if (!_critter_flag_check(critter->pid, 2048)) {
        critter->flags |= OBJECT_NO_BLOCK;
        _obj_toggle_flat(critter, &tempRect);
    }

    // NOTE: using uninitialized updatedRect/tempRect if fid was not set.

    rectUnion(&updatedRect, &tempRect, &updatedRect);

    _obj_turn_off_light(critter, &tempRect);
    rectUnion(&updatedRect, &tempRect, &updatedRect);

    critter->data.critter.hp = 0;
    critter->data.critter.combat.results |= DAM_DEAD;

    if (critter->sid != -1) {
        scriptRemove(critter->sid);
        critter->sid = -1;
    }

    _critterClearObj = critter;
    _queue_clear_type(EVENT_TYPE_DRUG, _critterClearObjDrugs);

    _item_destroy_all_hidden(critter);

    if (a3) {
        tileWindowRefreshRect(&updatedRect, elevation);
    }

    if (critter == gDude) {
        endgameSetupDeathEnding(ENDGAME_DEATH_ENDING_REASON_DEATH);
        _game_user_wants_to_quit = 2;
    }
}

// Returns experience for killing [critter].
//
// 0x42DCB8
int critterGetExp(Object* critter)
{
    Proto* proto;
    protoGetProto(critter->pid, &proto);
    return proto->critter.data.experience;
}

// 0x42DCDC
bool critterIsActive(Object* critter)
{
    if (critter == NULL) {
        return false;
    }

    if ((critter->pid >> 24) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if ((critter->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD)) != 0) {
        return false;
    }

    if ((critter->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD | DAM_LOSE_TURN)) != 0) {
        return false;
    }

    return (critter->data.critter.combat.results & DAM_DEAD) == 0;
}

// 0x42DD18
bool critterIsDead(Object* critter)
{
    if (critter == NULL) {
        return false;
    }

    if ((critter->pid >> 24) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (critterGetStat(critter, STAT_CURRENT_HIT_POINTS) <= 0) {
        return true;
    }

    if ((critter->data.critter.combat.results & DAM_DEAD) != 0) {
        return true;
    }

    return false;
}

// 0x42DD58
bool critterIsCrippled(Object* critter)
{
    if (critter == NULL) {
        return false;
    }

    if ((critter->pid >> 24) != OBJ_TYPE_CRITTER) {
        return false;
    }

    return (critter->data.critter.combat.results & DAM_CRIP) != 0;
}

// 0x42DD80
bool _critter_is_prone(Object* critter)
{
    if (critter == NULL) {
        return false;
    }

    if ((critter->pid >> 24) != OBJ_TYPE_CRITTER) {
        return false;
    }

    int anim = (critter->fid & 0xFF0000) >> 16;

    return (critter->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0
        || anim >= FIRST_KNOCKDOWN_AND_DEATH_ANIM && anim <= LAST_KNOCKDOWN_AND_DEATH_ANIM
        || anim >= FIRST_SF_DEATH_ANIM && anim <= LAST_SF_DEATH_ANIM;
}

// critter_body_type
// 0x42DDC4
int critterGetBodyType(Object* critter)
{
    if (critter == NULL) {
        debugPrint("\nError: critter_body_type: pobj was NULL!");
        return 0;
    }

    if ((critter->pid >> 24) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    Proto* proto;
    protoGetProto(critter->pid, &proto);
    return proto->critter.data.bodyType;
}

// 0x42DE58
int gcdLoad(const char* path)
{
    File* stream = fileOpen(path, "rb");
    if (stream == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    if (protoCritterDataRead(stream, &(proto->critter.data)) == -1) {
        fileClose(stream);
        return -1;
    }

    fileRead(gDudeName, DUDE_NAME_MAX_LENGTH, 1, stream);

    if (skillsLoad(stream) == -1) {
        fileClose(stream);
        return -1;
    }

    if (traitsLoad(stream) == -1) {
        fileClose(stream);
        return -1;
    }

    if (fileReadInt32(stream, &characterEditorRemainingCharacterPoints) == -1) {
        fileClose(stream);
        return -1;
    }

    proto->critter.data.baseStats[STAT_DAMAGE_RESISTANCE_EMP] = 100;
    proto->critter.data.bodyType = 0;
    proto->critter.data.experience = 0;
    proto->critter.data.killType = 0;

    fileClose(stream);
    return 0;
}

// 0x42DF70
int protoCritterDataRead(File* stream, CritterProtoData* critterData)
{
    if (fileReadInt32(stream, &(critterData->flags)) == -1) return -1;
    if (fileReadInt32List(stream, critterData->baseStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (fileReadInt32List(stream, critterData->bonusStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (fileReadInt32List(stream, critterData->skills, SKILL_COUNT) == -1) return -1;
    if (fileReadInt32(stream, &(critterData->bodyType)) == -1) return -1;
    if (fileReadInt32(stream, &(critterData->experience)) == -1) return -1;
    if (fileReadInt32(stream, &(critterData->killType)) == -1) return -1;

    // NOTE: For unknown reason damage type is not present in two protos: Sentry
    // Bot and Weak Brahmin. These two protos are 412 bytes, not 416.
    //
    // Given that only Floating Eye Bot, Floater, and Nasty Floater have
    // natural damage type other than normal, I think addition of natural
    // damage type as a feature was a last minute design decision. Most protos
    // were updated, but not all. Another suggestion is that some team member
    // used outdated toolset to build those two protos (mapper or whatever
    // they used to create protos in the first place).
    //
    // Regardless of the reason, damage type is considered optional by original
    // code as seen at 0x42E01B.
    if (fileReadInt32(stream, &(critterData->damageType)) == -1) {
        critterData->damageType = DAMAGE_TYPE_NORMAL;
    }

    return 0;
}

// 0x42E08C
int gcdSave(const char* path)
{
    File* stream = fileOpen(path, "wb");
    if (stream == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    if (protoCritterDataWrite(stream, &(proto->critter.data)) == -1) {
        fileClose(stream);
        return -1;
    }

    fileWrite(gDudeName, DUDE_NAME_MAX_LENGTH, 1, stream);

    if (skillsSave(stream) == -1) {
        fileClose(stream);
        return -1;
    }

    if (traitsSave(stream) == -1) {
        fileClose(stream);
        return -1;
    }

    if (fileWriteInt32(stream, characterEditorRemainingCharacterPoints) == -1) {
        fileClose(stream);
        return -1;
    }

    fileClose(stream);
    return 0;
}

// 0x42E174
int protoCritterDataWrite(File* stream, CritterProtoData* critterData)
{
    if (fileWriteInt32(stream, critterData->flags) == -1) return -1;
    if (fileWriteInt32List(stream, critterData->baseStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (fileWriteInt32List(stream, critterData->bonusStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (fileWriteInt32List(stream, critterData->skills, SKILL_COUNT) == -1) return -1;
    if (fileWriteInt32(stream, critterData->bodyType) == -1) return -1;
    if (fileWriteInt32(stream, critterData->experience) == -1) return -1;
    if (fileWriteInt32(stream, critterData->killType) == -1) return -1;
    if (fileWriteInt32(stream, critterData->damageType) == -1) return -1;

    return 0;
}

// 0x42E220
void dudeDisableState(int state)
{
    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    proto->critter.data.flags &= ~(1 << state);

    if (state == DUDE_STATE_SNEAKING) {
        queueRemoveEventsByType(gDude, EVENT_TYPE_SNEAK);
    }

    indicatorBarRefresh();
}

// 0x42E26C
void dudeEnableState(int state)
{
    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    proto->critter.data.flags |= (1 << state);

    if (state == DUDE_STATE_SNEAKING) {
        sneakEventProcess(NULL, NULL);
    }

    indicatorBarRefresh();
}

// 0x42E2B0
void dudeToggleState(int state)
{
    // NOTE: Uninline.
    if (dudeHasState(state)) {
        dudeDisableState(state);
    } else {
        dudeEnableState(state);
    }
}

// 0x42E2F8
bool dudeHasState(int state)
{
    Proto* proto;
    protoGetProto(gDude->pid, &proto);
    return (proto->critter.data.flags & (1 << state)) != 0;
}

// 0x42E32C
int sneakEventProcess(Object* obj, void* data)
{
    int time;

    int sneak = skillGetValue(gDude, SKILL_SNEAK);
    if (skillRoll(gDude, SKILL_SNEAK, 0, NULL) < ROLL_SUCCESS) {
        time = 600;
        _sneak_working = false;

        if (sneak > 250)
            time = 100;
        else if (sneak > 200)
            time = 120;
        else if (sneak > 170)
            time = 150;
        else if (sneak > 135)
            time = 200;
        else if (sneak > 100)
            time = 300;
        else if (sneak > 80)
            time = 400;
    } else {
        time = 600;
        _sneak_working = true;
    }

    queueAddEvent(time, gDude, NULL, EVENT_TYPE_SNEAK);

    return 0;
}

// 0x42E3E4
int _critter_sneak_clear(Object* obj, void* data)
{
    dudeDisableState(DUDE_STATE_SNEAKING);
    return 1;
}

// Returns true if dude is really sneaking.
//
// 0x42E3F4
bool dudeIsSneaking()
{
    // NOTE: Uninline.
    if (dudeHasState(DUDE_STATE_SNEAKING)) {
        return _sneak_working;
    }

    return false;
}

// 0x42E424
int knockoutEventProcess(Object* obj, void* data)
{
    if ((obj->data.critter.combat.results & DAM_DEAD) != 0) {
        return 0;
    }

    obj->data.critter.combat.results &= ~(DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN);
    obj->data.critter.combat.results |= DAM_KNOCKED_DOWN;

    if (isInCombat()) {
        obj->data.critter.combat.maneuver |= CRITTER_MANEUVER_0x01;
    } else {
        _dude_standup(obj);
    }

    return 0;
}

// 0x42E460
int _critter_wake_clear(Object* obj, void* data)
{
    if ((obj->pid >> 24) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    if ((obj->data.critter.combat.results & DAM_DEAD) != 0) {
        return 0;
    }

    obj->data.critter.combat.results &= ~(DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN);

    int fid = buildFid((obj->fid & 0xF000000) >> 24, obj->fid & 0xFFF, ANIM_STAND, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    objectSetFid(obj, fid, 0);

    return 0;
}

// 0x42E4C0
int _critter_set_who_hit_me(Object* a1, Object* a2)
{
    if (a1 == NULL) {
        return -1;
    }

    if (a2 != NULL && ((a2->fid & 0xF000000) >> 24) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    if ((a1->pid >> 24) == OBJ_TYPE_CRITTER) {
        if (a2 == NULL || a1->data.critter.combat.team != a2->data.critter.combat.team || statRoll(a1, STAT_INTELLIGENCE, -1, NULL) < 2 && (!objectIsPartyMember(a1) || !objectIsPartyMember(a2))) {
            a1->data.critter.combat.whoHitMe = a2;
            if (a2 == gDude) {
                reactionSetValue(a1, -3);
            }
        }
    }

    return 0;
}

// 0x42E564
bool _critter_can_obj_dude_rest()
{
    bool v1 = false;
    if (!_wmMapCanRestHere(gElevation)) {
        v1 = true;
    }

    bool result = true;

    Object** critterList;
    int critterListLength = objectListCreate(-1, gElevation, OBJ_TYPE_CRITTER, &critterList);

    // TODO: Check conditions in this loop.
    for (int index = 0; index < critterListLength; index++) {
        Object* critter = critterList[index];
        if ((critter->data.critter.combat.results & DAM_DEAD) != 0) {
            continue;
        }

        if (critter == gDude) {
            continue;
        }

        if (critter->data.critter.combat.whoHitMe != gDude) {
            if (!v1 || critter->data.critter.combat.team == gDude->data.critter.combat.team) {
                continue;
            }
        }

        result = false;
        break;
    }

    if (critterListLength != 0) {
        objectListFree(critterList);
    }

    return result;
}

// 0x42E62C
int critterGetMovementPointCostAdjustedForCrippledLegs(Object* critter, int actionPoints)
{
    if ((critter->pid >> 24) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    int flags = critter->data.critter.combat.results;
    if ((flags & DAM_CRIP_LEG_LEFT) != 0 && (flags & DAM_CRIP_LEG_RIGHT) != 0) {
        return 8 * actionPoints;
    } else if ((flags & (DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT)) != 0) {
        return 4 * actionPoints;
    } else {
        return actionPoints;
    }
}

// 0x42E66C
bool critterIsEncumbered(Object* critter)
{
    int maxWeight = critterGetStat(critter, STAT_CARRY_WEIGHT);
    int currentWeight = objectGetInventoryWeight(critter);
    return maxWeight < currentWeight;
}

// 0x42E690
bool critterIsFleeing(Object* critter)
{
    return critter != NULL
        ? (critter->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0
        : false;
}

// Checks proto critter flag.
//
// 0x42E6AC
bool _critter_flag_check(int pid, int flag)
{
    if (pid == -1) {
        return false;
    }

    if ((pid >> 24) != OBJ_TYPE_CRITTER) {
        return false;
    }

    Proto* proto;
    protoGetProto(pid, &proto);
    return (proto->critter.data.flags & flag) != 0;
}
