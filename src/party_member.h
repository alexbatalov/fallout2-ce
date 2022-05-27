#ifndef PARTY_MEMBER_H
#define PARTY_MEMBER_H

#include "combat_ai_defs.h"
#include "db.h"
#include "obj_types.h"
#include "scripts.h"

typedef struct PartyMemberDescription {
    bool areaAttackMode[AREA_ATTACK_MODE_COUNT];
    bool runAwayMode[RUN_AWAY_MODE_COUNT];
    bool bestWeapon[BEST_WEAPON_COUNT];
    bool distanceMode[DISTANCE_COUNT];
    bool attackWho[ATTACK_WHO_COUNT];
    bool chemUse[CHEM_USE_COUNT];
    bool disposition[DISPOSITION_COUNT];
    int level_minimum;
    int level_up_every;
    int level_pids_num;
    int level_pids[5];
} PartyMemberDescription;

typedef struct STRU_519DBC {
    int field_0;
    int field_4; // party member level
    int field_8; // early what?
} STRU_519DBC;

typedef struct STRUCT_519DA8 {
    Object* object;
    Script* script;
    int* vars;
    struct STRUCT_519DA8* next;
} STRUCT_519DA8;

extern int gPartyMemberDescriptionsLength;
extern int* gPartyMemberPids;
extern STRUCT_519DA8* _itemSaveListHead;
extern STRUCT_519DA8* gPartyMembers;
extern int gPartyMembersLength;
extern int _partyMemberItemCount;
extern int _partyStatePrepped;
extern PartyMemberDescription* gPartyMemberDescriptions;
extern STRU_519DBC* _partyMemberLevelUpInfoList;
extern int _curID;

int partyMembersInit();
void partyMembersReset();
void partyMembersExit();
int partyMemberGetDescription(Object* object, PartyMemberDescription** partyMemberDescriptionPtr);
void partyMemberDescriptionInit(PartyMemberDescription* partyMemberDescription);
int partyMemberAdd(Object* object);
int partyMemberRemove(Object* object);
int _partyMemberPrepSave();
int _partyMemberUnPrepSave();
int partyMembersSave(File* stream);
int _partyMemberPrepLoad();
int _partyMemberPrepLoadInstance(STRUCT_519DA8* a1);
int _partyMemberRecoverLoad();
int _partyMemberRecoverLoadInstance(STRUCT_519DA8* a1);
int partyMembersLoad(File* stream);
void _partyMemberClear();
int _partyMemberSyncPosition();
int _partyMemberRestingHeal(int a1);
Object* partyMemberFindByPid(int a1);
bool _isPotentialPartyMember(Object* object);
bool objectIsPartyMember(Object* object);
int _getPartyMemberCount();
int _partyMemberNewObjID();
int _partyMemberNewObjIDRecurseFind(Object* object, int objectId);
int _partyMemberPrepItemSaveAll();
int _partyMemberPrepItemSave(Object* object);
int _partyMemberItemSave(Object* object);
int _partyMemberItemRecover(STRUCT_519DA8* a1);
int _partyMemberClearItemList();
int partyMemberGetBestSkill(Object* object);
Object* partyMemberGetBestInSkill(int skill);
int partyGetBestSkillValue(int skill);
int _partyFixMultipleMembers();
void _partyMemberSaveProtos();
bool partyMemberSupportsDisposition(Object* object, int disposition);
bool partyMemberSupportsAreaAttackMode(Object* object, int areaAttackMode);
bool partyMemberSupportsRunAwayMode(Object* object, int runAwayMode);
bool partyMemberSupportsBestWeapon(Object* object, int bestWeapon);
bool partyMemberSupportsDistance(Object* object, int distanceMode);
bool partyMemberSupportsAttackWho(Object* object, int attackWho);
bool partyMemberSupportsChemUse(Object* object, int chemUse);
int _partyMemberIncLevels();
int _partyMemberCopyLevelInfo(Object* object, int a2);
bool partyIsAnyoneCanBeHealedByRest();
int partyGetMaxWoundToHealByRest();

#endif /* PARTY_MEMBER_H */
