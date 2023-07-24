#ifndef COMBAT_AI_H
#define COMBAT_AI_H

#include "combat_ai_defs.h"
#include "combat_defs.h"
#include "db.h"
#include "obj_types.h"

namespace fallout {

typedef enum AiMessageType {
    AI_MESSAGE_TYPE_RUN,
    AI_MESSAGE_TYPE_MOVE,
    AI_MESSAGE_TYPE_ATTACK,
    AI_MESSAGE_TYPE_MISS,
    AI_MESSAGE_TYPE_HIT,
} AiMessageType;

extern const char* gAreaAttackModeKeys[AREA_ATTACK_MODE_COUNT];
extern const char* gAttackWhoKeys[ATTACK_WHO_COUNT];
extern const char* gBestWeaponKeys[BEST_WEAPON_COUNT];
extern const char* gChemUseKeys[CHEM_USE_COUNT];
extern const char* gDistanceModeKeys[DISTANCE_COUNT];
extern const char* gRunAwayModeKeys[RUN_AWAY_MODE_COUNT];
extern const char* gDispositionKeys[DISPOSITION_COUNT];
extern const char* gHurtTooMuchKeys[HURT_COUNT];

int aiInit();
void aiReset();
int aiExit();
int aiLoad(File* stream);
int aiSave(File* stream);
int combat_ai_num();
char* combat_ai_name(int packet_num);
int aiGetAreaAttackMode(Object* obj);
int aiGetRunAwayMode(Object* obj);
int aiGetBestWeapon(Object* obj);
int aiGetDistance(Object* obj);
int aiGetAttackWho(Object* obj);
int aiGetChemUse(Object* obj);
int aiSetAreaAttackMode(Object* critter, int areaAttackMode);
int aiSetRunAwayMode(Object* obj, int run_away_mode);
int aiSetBestWeapon(Object* critter, int bestWeapon);
int aiSetDistance(Object* critter, int distance);
int aiSetAttackWho(Object* critter, int attackWho);
int aiSetChemUse(Object* critter, int chemUse);
int aiGetDisposition(Object* obj);
int aiSetDisposition(Object* obj, int a2);
int _caiSetupTeamCombat(Object* attackerTeam, Object* defenderTeam);
int _caiTeamCombatInit(Object** crittersList, int crittersListLength);
void _caiTeamCombatExit();
Object* _ai_search_inven_weap(Object* critter, bool checkRequiredActionPoints, Object* defender);
Object* _ai_search_inven_armor(Object* critter);
int _cAIPrepWeaponItem(Object* critter, Object* item);
void aiAttemptWeaponReload(Object* critter, int animate);
void _combat_ai_begin(int a1, void* a2);
void _combat_ai_over();
int _cai_perform_distance_prefs(Object* a1, Object* a2);
void _combat_ai(Object* a1, Object* a2);
bool _combatai_want_to_join(Object* a1);
bool _combatai_want_to_stop(Object* a1);
int critterSetTeam(Object* obj, int team);
int critterSetAiPacket(Object* object, int aiPacket);
int _combatai_msg(Object* critter, Attack* attack, int type, int delay);
Object* _combat_ai_random_target(Attack* attack);
void _combatai_check_retaliation(Object* a1, Object* a2);
bool isWithinPerception(Object* a1, Object* a2);
void aiMessageListReloadIfNeeded();
void _combatai_notify_onlookers(Object* a1);
void _combatai_notify_friends(Object* a1);
void _combatai_delete_critter(Object* obj);

} // namespace fallout

#endif /* COMBAT_AI_H */
