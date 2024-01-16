#ifndef PARTY_MEMBER_H
#define PARTY_MEMBER_H

#include <vector>

#include "db.h"
#include "obj_types.h"
#include "scripts.h"

namespace fallout {

extern int gPartyMemberDescriptionsLength;
extern int* gPartyMemberPids;

int partyMembersInit();
void partyMembersReset();
void partyMembersExit();
int partyMemberAdd(Object* object);
int partyMemberRemove(Object* object);
int _partyMemberPrepSave();
int _partyMemberUnPrepSave();
int partyMembersSave(File* stream);
int _partyMemberPrepLoad();
int _partyMemberRecoverLoad();
int partyMembersLoad(File* stream);
void _partyMemberClear();
int _partyMemberSyncPosition();
int _partyMemberRestingHeal(int a1);
Object* partyMemberFindByPid(int a1);
bool _isPotentialPartyMember(Object* object);
bool objectIsPartyMember(Object* object);
int _getPartyMemberCount();
int _partyMemberPrepItemSaveAll();
int partyMemberGetBestSkill(Object* object);
Object* partyMemberGetBestInSkill(int skill);
int partyGetBestSkillValue(int skill);
void _partyMemberSaveProtos();
bool partyMemberSupportsDisposition(Object* object, int disposition);
bool partyMemberSupportsAreaAttackMode(Object* object, int areaAttackMode);
bool partyMemberSupportsRunAwayMode(Object* object, int runAwayMode);
bool partyMemberSupportsBestWeapon(Object* object, int bestWeapon);
bool partyMemberSupportsDistance(Object* object, int distanceMode);
bool partyMemberSupportsAttackWho(Object* object, int attackWho);
bool partyMemberSupportsChemUse(Object* object, int chemUse);
int _partyMemberIncLevels();
bool partyIsAnyoneCanBeHealedByRest();
int partyGetMaxWoundToHealByRest();
std::vector<Object*> get_all_party_members_objects(bool include_hidden);

} // namespace fallout

#endif /* PARTY_MEMBER_H */
