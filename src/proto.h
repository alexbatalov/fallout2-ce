#ifndef PROTO_H
#define PROTO_H

#include "db.h"
#include "message.h"
#include "obj_types.h"
#include "perk_defs.h"
#include "platform_compat.h"
#include "proto_types.h"
#include "skill_defs.h"
#include "stat_defs.h"

typedef enum ItemDataMember {
    ITEM_DATA_MEMBER_PID = 0,
    ITEM_DATA_MEMBER_NAME = 1,
    ITEM_DATA_MEMBER_DESCRIPTION = 2,
    ITEM_DATA_MEMBER_FID = 3,
    ITEM_DATA_MEMBER_LIGHT_DISTANCE = 4,
    ITEM_DATA_MEMBER_LIGHT_INTENSITY = 5,
    ITEM_DATA_MEMBER_FLAGS = 6,
    ITEM_DATA_MEMBER_EXTENDED_FLAGS = 7,
    ITEM_DATA_MEMBER_SID = 8,
    ITEM_DATA_MEMBER_TYPE = 9,
    ITEM_DATA_MEMBER_MATERIAL = 11,
    ITEM_DATA_MEMBER_SIZE = 12,
    ITEM_DATA_MEMBER_WEIGHT = 13,
    ITEM_DATA_MEMBER_COST = 14,
    ITEM_DATA_MEMBER_INVENTORY_FID = 15,
    ITEM_DATA_MEMBER_WEAPON_RANGE = 555,
} ItemDataMember;

typedef enum CritterDataMember {
    CRITTER_DATA_MEMBER_PID = 0,
    CRITTER_DATA_MEMBER_NAME = 1,
    CRITTER_DATA_MEMBER_DESCRIPTION = 2,
    CRITTER_DATA_MEMBER_FID = 3,
    CRITTER_DATA_MEMBER_LIGHT_DISTANCE = 4,
    CRITTER_DATA_MEMBER_LIGHT_INTENSITY = 5,
    CRITTER_DATA_MEMBER_FLAGS = 6,
    CRITTER_DATA_MEMBER_EXTENDED_FLAGS = 7,
    CRITTER_DATA_MEMBER_SID = 8,
    CRITTER_DATA_MEMBER_DATA = 9,
    CRITTER_DATA_MEMBER_HEAD_FID = 10,
    CRITTER_DATA_MEMBER_BODY_TYPE = 11,
} CritterDataMember;

typedef enum SceneryDataMember {
    SCENERY_DATA_MEMBER_PID = 0,
    SCENERY_DATA_MEMBER_NAME = 1,
    SCENERY_DATA_MEMBER_DESCRIPTION = 2,
    SCENERY_DATA_MEMBER_FID = 3,
    SCENERY_DATA_MEMBER_LIGHT_DISTANCE = 4,
    SCENERY_DATA_MEMBER_LIGHT_INTENSITY = 5,
    SCENERY_DATA_MEMBER_FLAGS = 6,
    SCENERY_DATA_MEMBER_EXTENDED_FLAGS = 7,
    SCENERY_DATA_MEMBER_SID = 8,
    SCENERY_DATA_MEMBER_TYPE = 9,
    SCENERY_DATA_MEMBER_DATA = 10,
    SCENERY_DATA_MEMBER_MATERIAL = 11,
} SceneryDataMember;

typedef enum WallDataMember {
    WALL_DATA_MEMBER_PID = 0,
    WALL_DATA_MEMBER_NAME = 1,
    WALL_DATA_MEMBER_DESCRIPTION = 2,
    WALL_DATA_MEMBER_FID = 3,
    WALL_DATA_MEMBER_LIGHT_DISTANCE = 4,
    WALL_DATA_MEMBER_LIGHT_INTENSITY = 5,
    WALL_DATA_MEMBER_FLAGS = 6,
    WALL_DATA_MEMBER_EXTENDED_FLAGS = 7,
    WALL_DATA_MEMBER_SID = 8,
    WALL_DATA_MEMBER_MATERIAL = 9,
} WallDataMember;

typedef enum MiscDataMember {
    MISC_DATA_MEMBER_PID = 0,
    MISC_DATA_MEMBER_NAME = 1,
    MISC_DATA_MEMBER_DESCRIPTION = 2,
    MISC_DATA_MEMBER_FID = 3,
    MISC_DATA_MEMBER_LIGHT_DISTANCE = 4,
    MISC_DATA_MEMBER_LIGHT_INTENSITY = 5,
    MISC_DATA_MEMBER_FLAGS = 6,
    MISC_DATA_MEMBER_EXTENDED_FLAGS = 7,
} MiscDataMember;

typedef enum ProtoDataMemberType {
    PROTO_DATA_MEMBER_TYPE_INT = 1,
    PROTO_DATA_MEMBER_TYPE_STRING = 2,
} ProtoDataMemberType;

typedef union ProtoDataMemberValue {
    int integerValue;
    char* stringValue;
} ProtoDataMemberValue;

typedef enum PrototypeMessage {
    PROTOTYPE_MESSAGE_NAME,
    PROTOTYPE_MESSAGE_DESCRIPTION,
} PrototypeMesage;

extern char _aProto_0[];

extern char _cd_path_base[COMPAT_MAX_PATH];
extern ProtoList _protoLists[11];
extern const size_t _proto_sizes[11];
extern int _protos_been_initialized;
extern CritterProto gDudeProto;
extern char* _proto_path_base;
extern int _init_true;
extern int _retval;

extern char* _mp_perk_code_None;
extern char* _mp_perk_code_strs[PERK_COUNT];
extern char* _mp_critter_stats_list;
extern char* _critter_stats_list_None;
extern char* _critter_stats_list_strs[STAT_COUNT];
extern MessageList _proto_msg_files[6];
extern char* gRaceTypeNames[2];
extern char* gSceneryTypeNames[6];
extern MessageList gProtoMessageList;
extern char* gMaterialTypeNames[8];
extern char* _proto_none_str;
extern char* gBodyTypeNames[3];
extern char* gItemTypeNames[7];
extern char* gDamageTypeNames[7];
extern char* gCaliberTypeNames[19];

extern char** _perk_code_strs;
extern char** _critter_stats_list;

int _proto_list_str(int pid, char* proto_path);
bool _proto_action_can_use(int pid);
bool _proto_action_can_use_on(int pid);
bool _proto_action_can_talk_to(int pid);
int _proto_action_can_pickup(int pid);
char* protoGetMessage(int pid, int message);
char* protoGetName(int pid);
char* protoGetDescription(int pid);
int _proto_critter_init(Proto* a1, int a2);
void objectDataReset(Object* obj);
int objectCritterCombatDataRead(CritterCombatData* data, File* stream);
int objectCritterCombatDataWrite(CritterCombatData* data, File* stream);
int objectDataRead(Object* obj, File* stream);
int objectDataWrite(Object* obj, File* stream);
int _proto_update_gen(Object* obj);
int _proto_update_init(Object* obj);
int _proto_dude_update_gender();
int _proto_dude_init(const char* path);
int protoGetDataMember(int pid, int member, ProtoDataMemberValue* value);
int protoInit();
void protoReset();
void protoExit();
int _proto_header_load();
int protoItemDataRead(ItemProtoData* item_data, int type, File* stream);
int protoSceneryDataRead(SceneryProtoData* scenery_data, int type, File* stream);
int protoRead(Proto* buf, File* stream);
int protoItemDataWrite(ItemProtoData* item_data, int type, File* stream);
int protoSceneryDataWrite(SceneryProtoData* scenery_data, int type, File* stream);
int protoWrite(Proto* buf, File* stream);
int _proto_save_pid(int pid);
int _proto_load_pid(int pid, Proto** out_proto);
int _proto_find_free_subnode(int type, Proto** out_ptr);
void _proto_remove_some_list(int type);
void _proto_remove_list(int type);
void _proto_remove_all();
int protoGetProto(int pid, Proto** out_proto);
int _proto_new_id(int a1);
int _proto_max_id(int a1);
int _ResetPlayer();

#endif /* PROTO_H */
