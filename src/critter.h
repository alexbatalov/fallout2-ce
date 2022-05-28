#ifndef CRITTER_H
#define CRITTER_H

#include "db.h"
#include "message.h"
#include "obj_types.h"
#include "proto_types.h"

// Maximum length of dude's name length.
#define DUDE_NAME_MAX_LENGTH (32)

// The number of effects caused by radiation.
//
// A radiation effect is an identifier and does not have it's own name. It's
// stat is specified in [gRadiationEffectStats], and it's amount is specified
// in [gRadiationEffectPenalties] for every [RadiationLevel].
#define RADIATION_EFFECT_COUNT 8

// Radiation levels.
//
// The names of levels are taken from Fallout 3, comments from Fallout 2.
typedef enum RadiationLevel {
    // Very nauseous.
    RADIATION_LEVEL_NONE,

    // Slightly fatigued.
    RADIATION_LEVEL_MINOR,

    // Vomiting does not stop.
    RADIATION_LEVEL_ADVANCED,

    // Hair is falling out.
    RADIATION_LEVEL_CRITICAL,

    // Skin is falling off.
    RADIATION_LEVEL_DEADLY,

    // Intense agony.
    RADIATION_LEVEL_FATAL,

    // The number of radiation levels.
    RADIATION_LEVEL_COUNT,
} RadiationLevel;

typedef enum DudeState {
    DUDE_STATE_SNEAKING = 0,
    DUDE_STATE_LEVEL_UP_AVAILABLE = 3,
    DUDE_STATE_ADDICTED = 4,
} DudeState;

extern char _aCorpse[];
extern char byte_501494[];

extern char* _name_critter;
extern const int gRadiationEnduranceModifiers[RADIATION_LEVEL_COUNT];
extern const int gRadiationEffectStats[RADIATION_EFFECT_COUNT];
extern const int gRadiationEffectPenalties[RADIATION_LEVEL_COUNT][RADIATION_EFFECT_COUNT];
extern Object* _critterClearObj;

extern MessageList gCritterMessageList;
extern char gDudeName[DUDE_NAME_MAX_LENGTH];
extern int _sneak_working;
extern int gKillsByType[KILL_TYPE_COUNT];
extern int _old_rad_level;

int critterInit();
void critterReset();
void critterExit();
int critterLoad(File* stream);
int critterSave(File* stream);
char* critterGetName(Object* obj);
void critterProtoDataCopy(CritterProtoData* dest, CritterProtoData* src);
int dudeSetName(const char* name);
void dudeResetName();
int critterGetHitPoints(Object* critter);
int critterAdjustHitPoints(Object* critter, int amount);
int critterGetPoison(Object* critter);
int critterAdjustPoison(Object* obj, int amount);
int poisonEventProcess(Object* obj, void* data);
int critterGetRadiation(Object* critter);
int critterAdjustRadiation(Object* obj, int amount);
int _critter_check_rads(Object* critter);
int _get_rad_damage_level(Object* obj, void* data);
int _clear_rad_damage(Object* obj, void* data);
void _process_rads(Object* obj, int radiationLevel, bool direction);
int radiationEventProcess(Object* obj, void* data);
int radiationEventRead(File* stream, void** dataPtr);
int radiationEventWrite(File* stream, void* data);
int critterGetDamageType(Object* critter);
int killsIncByType(int killType);
int killsGetByType(int killType);
int killsLoad(File* stream);
int killsSave(File* stream);
int critterGetKillType(Object* critter);
char* killTypeGetName(int killType);
char* killTypeGetDescription(int killType);
int _critter_heal_hours(Object* obj, int a2);
int _critterClearObjDrugs(Object* obj, void* data);
void critterKill(Object* critter, int anim, bool a3);
int critterGetExp(Object* critter);
bool critterIsActive(Object* critter);
bool critterIsDead(Object* critter);
bool critterIsCrippled(Object* critter);
bool _critter_is_prone(Object* critter);
int critterGetBodyType(Object* critter);
int gcdLoad(const char* path);
int protoCritterDataRead(File* stream, CritterProtoData* critterData);
int gcdSave(const char* path);
int protoCritterDataWrite(File* stream, CritterProtoData* critterData);
void dudeDisableState(int state);
void dudeEnableState(int state);
void dudeToggleState(int state);
bool dudeHasState(int state);
int sneakEventProcess(Object* obj, void* data);
int _critter_sneak_clear(Object* obj, void* data);
bool dudeIsSneaking();
int knockoutEventProcess(Object* obj, void* data);
int _critter_wake_clear(Object* obj, void* data);
int _critter_set_who_hit_me(Object* a1, Object* a2);
bool _critter_can_obj_dude_rest();
int critterGetMovementPointCostAdjustedForCrippledLegs(Object* critter, int a2);
bool critterIsEncumbered(Object* critter);
bool critterIsFleeing(Object* a1);
bool _critter_flag_check(int pid, int flag);

#endif /* CRITTER_H */
