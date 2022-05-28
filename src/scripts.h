#ifndef SCRIPTS_H
#define SCRIPTS_H

#include "combat_defs.h"
#include "db.h"
#include "interpreter.h"
#include "message.h"
#include "obj_types.h"

#define SCRIPT_LIST_EXTENT_SIZE 16

#define SCRIPT_FLAG_0x01 (0x01)
#define SCRIPT_FLAG_0x02 (0x02)
#define SCRIPT_FLAG_0x04 (0x04)
#define SCRIPT_FLAG_0x08 (0x08)
#define SCRIPT_FLAG_0x10 (0x10)

// 24 * 60 * 60 * 10
#define GAME_TIME_TICKS_PER_DAY (864000)

// 365 * 24 * 60 * 60 * 10
#define GAME_TIME_TICKS_PER_YEAR (315360000)

typedef enum ScriptRequests {
    SCRIPT_REQUEST_COMBAT = 0x01,
    SCRIPT_REQUEST_0x02 = 0x02,
    SCRIPT_REQUEST_WORLD_MAP = 0x04,
    SCRIPT_REQUEST_ELEVATOR = 0x08,
    SCRIPT_REQUEST_EXPLOSION = 0x10,
    SCRIPT_REQUEST_DIALOG = 0x20,
    SCRIPT_REQUEST_0x40 = 0x40,
    SCRIPT_REQUEST_ENDGAME = 0x80,
    SCRIPT_REQUEST_LOOTING = 0x100,
    SCRIPT_REQUEST_STEALING = 0x200,
    SCRIPT_REQUEST_0x0400 = 0x400,
} ScriptRequests;

typedef enum ScriptType {
    SCRIPT_TYPE_SYSTEM, // s_system
    SCRIPT_TYPE_SPATIAL, // s_spatial
    SCRIPT_TYPE_TIMED, // s_time
    SCRIPT_TYPE_ITEM, // s_item
    SCRIPT_TYPE_CRITTER, // s_critter
    SCRIPT_TYPE_COUNT,
} ScriptType;

typedef enum ScriptProc {
    SCRIPT_PROC_NO_PROC = 0,
    SCRIPT_PROC_START = 1,
    SCRIPT_PROC_SPATIAL = 2,
    SCRIPT_PROC_DESCRIPTION = 3,
    SCRIPT_PROC_PICKUP = 4,
    SCRIPT_PROC_DROP = 5,
    SCRIPT_PROC_USE = 6,
    SCRIPT_PROC_USE_OBJ_ON = 7,
    SCRIPT_PROC_USE_SKILL_ON = 8,
    SCRIPT_PROC_9 = 9, // use_ad_on_proc
    SCRIPT_PROC_10 = 10, // use_disad_on_proc
    SCRIPT_PROC_TALK = 11,
    SCRIPT_PROC_CRITTER = 12,
    SCRIPT_PROC_COMBAT = 13,
    SCRIPT_PROC_DAMAGE = 14,
    SCRIPT_PROC_MAP_ENTER = 15,
    SCRIPT_PROC_MAP_EXIT = 16,
    SCRIPT_PROC_CREATE = 17,
    SCRIPT_PROC_DESTROY = 18,
    SCRIPT_PROC_19 = 19, // barter_init_proc
    SCRIPT_PROC_20 = 20, // barter_proc
    SCRIPT_PROC_LOOK_AT = 21,
    SCRIPT_PROC_TIMED = 22,
    SCRIPT_PROC_MAP_UPDATE = 23,
    SCRIPT_PROC_PUSH = 24,
    SCRIPT_PROC_IS_DROPPING = 25,
    SCRIPT_PROC_COMBAT_IS_STARTING = 26,
    SCRIPT_PROC_COMBAT_IS_OVER = 27,
    SCRIPT_PROC_COUNT,
} ScriptProc;

typedef struct ScriptsListEntry {
    char name[16];
    int local_vars_num;
} ScriptsListEntry;

typedef struct Script {
    // scr_id
    int sid;

    // scr_next
    int field_4;

    union {
        struct {
            // scr_udata.sp.built_tile
            int built_tile;
            // scr_udata.sp.radius
            int radius;
        } sp;
        struct {
            // scr_udata.tm.time
            int time;
        } tm;
    };

    // scr_flags
    int flags;

    // scr_script_idx
    int field_14;

    Program* program;

    // scr_oid
    int field_1C;

    // scr_local_var_offset
    int localVarsOffset;

    // scr_num_local_vars
    int localVarsCount;

    // return value?
    int field_28;

    // Currently executed action.
    //
    // See [opGetScriptAction].
    int action;
    int fixedParam;
    Object* owner;

    // source_obj
    Object* source;

    // target_obj
    Object* target;
    int actionBeingUsed;
    int scriptOverrides;
    int field_48;
    int howMuch;
    int field_50;
    int procs[SCRIPT_PROC_COUNT];
    int field_C4;
    int field_C8;
    int field_CC;
    int field_D0;
    int field_D4;
    int field_D8;
    int field_DC;
} Script;

typedef struct ScriptListExtent {
    Script scripts[SCRIPT_LIST_EXTENT_SIZE];
    // Number of scripts in the extent
    int length;
    struct ScriptListExtent* next;
} ScriptListExtent;

typedef struct ScriptList {
    ScriptListExtent* head;
    ScriptListExtent* tail;
    // Number of extents in the script list.
    int length;
    int nextScriptId;
} ScriptList;

extern char _Error_2[];
extern char byte_50D6C0[];

extern int _num_script_indexes;
extern int gScriptsEnumerationScriptIndex;
extern ScriptListExtent* gScriptsEnumerationScriptListExtent;
extern int gScriptsEnumerationElevation;
extern bool _scr_SpatialsEnabled;
extern ScriptList gScriptLists[SCRIPT_TYPE_COUNT];
extern const char* gScriptsBasePath;
extern bool gScriptsEnabled;
extern int _script_engine_run_critters;
extern int _script_engine_game_mode;
extern int gGameTime;
extern const int gGameTimeDaysPerMonth[12];
extern const char* gScriptProcNames[SCRIPT_PROC_COUNT];
extern ScriptsListEntry* gScriptsListEntries;
extern int gScriptsListEntriesLength;
extern int _cur_id;
extern int _count_;
extern int _last_time__;
extern int _last_light_time;
extern Object* _scrQueueTestObj;
extern int _scrQueueTestValue;
extern char* _err_str;
extern char* _blank_str;

extern unsigned int gScriptsRequests;
extern STRUCT_664980 stru_664958;
extern STRUCT_664980 stru_664980;
extern int gScriptsRequestedElevatorType;
extern int gScriptsRequestedElevatorLevel;
extern int gScriptsRequestedExplosionTile;
extern int gScriptsRequestedExplosionElevation;
extern int gScriptsRequestedExplosionMinDamage;
extern int gScriptsRequestedExplosionMaxDamage;
extern Object* gScriptsRequestedDialogWith;
extern Object* gScriptsRequestedLootingBy;
extern Object* gScriptsRequestedLootingFrom;
extern Object* gScriptsRequestedStealingBy;
extern Object* gScriptsRequestedStealingFrom;
extern MessageList _script_dialog_msgs[1450];
extern MessageList gScrMessageList;
extern char _hour_str[7];
extern int _lasttime;
extern bool _set;
extern char _tempStr1[20];

int gameTimeGetTime();
void gameTimeGetDate(int* monthPtr, int* dayPtr, int* yearPtr);
int gameTimeGetHour();
char* gameTimeGetTimeString();
void gameTimeAddTicks(int a1);
void gameTimeAddSeconds(int a1);
void gameTimeSetTime(int time);
int gameTimeScheduleUpdateEvent();
int gameTimeEventProcess(Object* obj, void* data);
int _scriptsCheckGameEvents(int* moviePtr, int window);
int mapUpdateEventProcess(Object* obj, void* data);
int scriptsNewObjectId();
int scriptGetSid(Program* a1);
Object* scriptGetSelf(Program* s);
int scriptSetObjects(int sid, Object* source, Object* target);
void scriptSetFixedParam(int a1, int a2);
int scriptSetActionBeingUsed(int sid, int a2);
Program* scriptsCreateProgramByName(const char* name);
void _doBkProcesses();
void _script_chk_critters();
void _script_chk_timed_events();
void _scrSetQueueTestVals(Object* a1, int a2);
int _scrQueueRemoveFixed(Object* obj, void* data);
int scriptAddTimerEvent(int sid, int delay, int param);
int scriptEventWrite(File* stream, void* data);
int scriptEventRead(File* stream, void** dataPtr);
int scriptEventProcess(Object* obj, void* data);
int scriptsClearPendingRequests();
int _scripts_clear_combat_requests(Script* script);
int scriptsHandleRequests();
int _scripts_check_state_in_combat();
int scriptsRequestCombat(STRUCT_664980* a1);
void _scripts_request_combat_locked(STRUCT_664980* ptr);
void scriptsRequestWorldMap();
int scriptsRequestElevator(Object* a1, int a2);
int scriptsRequestExplosion(int tile, int elevation, int minDamage, int maxDamage);
void scriptsRequestDialog(Object* a1);
void scriptsRequestEndgame();
int scriptsRequestLooting(Object* a1, Object* a2);
int scriptsRequestStealing(Object* a1, Object* a2);
int scriptExecProc(int sid, int proc);
int scriptLocateProcs(Script* scr);
bool scriptHasProc(int sid, int proc);
int scriptsLoadScriptsList();
int scriptsFreeScriptsList();
int _scr_find_str_run_info(int a1, int* a2, int sid);
int scriptsGetFileName(int scriptIndex, char* name);
int scriptsSetDudeScript();
int scriptsClearDudeScript();
int scriptsInit();
int _scr_reset();
int _scr_game_init();
int scriptsReset();
int scriptsExit();
int _scr_message_free();
int _scr_game_exit();
int scriptsEnable();
int scriptsDisable();
void _scr_enable_critters();
void _scr_disable_critters();
int scriptsSaveGameGlobalVars(File* stream);
int scriptsLoadGameGlobalVars(File* stream);
int scriptsSkipGameGlobalVars(File* stream);
int _scr_header_load();
int scriptWrite(Script* scr, File* stream);
int scriptListExtentWrite(ScriptListExtent* a1, File* stream);
int scriptSaveAll(File* stream);
int scriptRead(Script* scr, File* stream);
int scriptListExtentRead(ScriptListExtent* a1, File* stream);
int scriptLoadAll(File* stream);
int scriptGetScript(int sid, Script** script);
int scriptGetNewId(int scriptType);
int scriptAdd(int* sidPtr, int scriptType);
int scriptsRemoveLocalVars(Script* script);
int scriptRemove(int index);
int _scr_remove_all();
int _scr_remove_all_force();
Script* scriptGetFirstSpatialScript(int a1);
Script* scriptGetNextSpatialScript();
void _scr_spatials_enable();
void _scr_spatials_disable();
bool scriptsExecSpatialProc(Object* obj, int tile, int elevation);
int scriptsExecStartProc();
void scriptsExecMapEnterProc();
void scriptsExecMapUpdateProc();
void scriptsExecMapUpdateScripts(int a1);
void scriptsExecMapExitProc();
int scriptsGetMessageList(int a1, MessageList** out_message_list);
char* _scr_get_msg_str(int messageListId, int messageId);
char* _scr_get_msg_str_speech(int messageListId, int messageId, int a3);
int scriptGetLocalVar(int a1, int a2, int* a3);
int scriptSetLocalVar(int a1, int a2, int a3);
bool _scr_end_combat();
int _scr_explode_scenery(Object* a1, int tile, int radius, int elevation);

#endif /* SCRIPTS_H */
