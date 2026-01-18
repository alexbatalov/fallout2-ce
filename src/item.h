#ifndef ITEM_H
#define ITEM_H

#include "db.h"
#include "obj_types.h"

namespace fallout {

typedef enum AttackType {
    ATTACK_TYPE_NONE,
    ATTACK_TYPE_UNARMED,
    ATTACK_TYPE_MELEE,
    ATTACK_TYPE_THROW,
    ATTACK_TYPE_RANGED,
    ATTACK_TYPE_COUNT,
} AttackType;

typedef enum HealingItem {
    HEALING_ITEM_STIMPACK,
    HEALING_ITEM_SUPER_STIMPACK,
    HEALING_ITEM_HEALING_POWDER,
    HEALING_ITEM_COUNT,
} HealingItem;

int itemsInit();
void itemsReset();
void itemsExit();
int itemsLoad(File* stream);
int itemsSave(File* stream);
int itemAttemptAdd(Object* owner, Object* itemToAdd, int quantity);
int itemAdd(Object* owner, Object* itemToAdd, int quantity);
int itemRemove(Object* owner, Object* itemToRemove, int quantity);
int itemMove(Object* from, Object* to, Object* item, int quantity);
int itemMoveForce(Object* from, Object* to, Object* item, int quantity);
void itemMoveAll(Object* from, Object* to);
int itemMoveAllHidden(Object* from, Object* to);
int itemDestroyAllHidden(Object* owner);
int itemDropAll(Object* critter, int tile);
char* itemGetName(Object* obj);
char* itemGetDescription(Object* obj);
int itemGetType(Object* item);
int itemGetMaterial(Object* item);
int itemGetSize(Object* obj);
int itemGetWeight(Object* item);
int itemGetCost(Object* obj);
int objectGetCost(Object* obj);
int objectGetInventoryWeight(Object* obj);
bool dudeIsWeaponDisabled(Object* weapon);
int itemGetInventoryFid(Object* obj);
Object* critterGetWeaponForHitMode(Object* critter, int hitMode);
int itemGetActionPointCost(Object* obj, int hitMode, bool aiming);
int itemGetQuantity(Object* obj, Object* item);
int itemIsQueued(Object* obj);
Object* itemReplace(Object* owner, Object* itemToReplace, int flags);
bool itemIsHidden(Object* obj);
int weaponGetAttackTypeForHitMode(Object* weapon, int hitMode);
int weaponGetSkillForHitMode(Object* weapon, int hitMode);
int weaponGetSkillValue(Object* critter, int hitMode);
int weaponGetDamageMinMax(Object* weapon, int* minDamagePtr, int* maxDamagePtr);
int weaponGetDamage(Object* critter, int hitMode);
int weaponGetDamageType(Object* critter, Object* weapon);
int weaponIsTwoHanded(Object* weapon);
int critterGetAnimationForHitMode(Object* critter, int hitMode);
int weaponGetAnimationForHitMode(Object* weapon, int hitMode);
int ammoGetCapacity(Object* ammoOrWeapon);
int ammoGetQuantity(Object* ammoOrWeapon);
int ammoGetCaliber(Object* ammoOrWeapon);
void ammoSetQuantity(Object* ammoOrWeapon, int quantity);
int weaponAttemptReload(Object* critter, Object* weapon);
bool weaponCanBeReloadedWith(Object* weapon, Object* ammo);
int weaponReload(Object* weapon, Object* ammo);
int weaponGetRange(Object* critter, int hitMode);
int weaponGetActionPointCost(Object* critter, int hitMode, bool aiming);
int weaponGetMinStrengthRequired(Object* weapon);
int weaponGetCriticalFailureType(Object* weapon);
int weaponGetPerk(Object* weapon);
int weaponGetBurstRounds(Object* weapon);
int weaponGetAnimationCode(Object* weapon);
int weaponGetProjectilePid(Object* weapon);
int weaponGetAmmoTypePid(Object* weapon);
char weaponGetSoundId(Object* weapon);
bool critterCanAim(Object* critter, int hitMode);
int weaponCanBeUnloaded(Object* weapon);
Object* weaponUnload(Object* weapon);
int weaponGetPrimaryActionPointCost(Object* weapon);
int weaponGetSecondaryActionPointCost(Object* weapon);
int _item_w_compute_ammo_cost(Object* obj, int* inout_a2);
bool weaponIsGrenade(Object* weapon);
int weaponGetDamageRadius(Object* weapon, int hitMode);
int weaponGetGrenadeExplosionRadius(Object* weapon);
int weaponGetRocketExplosionRadius(Object* weapon);
int weaponGetAmmoArmorClassModifier(Object* weapon);
int weaponGetAmmoDamageResistanceModifier(Object* weapon);
int weaponGetAmmoDamageMultiplier(Object* weapon);
int weaponGetAmmoDamageDivisor(Object* weapon);
int armorGetArmorClass(Object* armor);
int armorGetDamageResistance(Object* armor, int damageType);
int armorGetDamageThreshold(Object* armor, int damageType);
int armorGetPerk(Object* armor);
int armorGetMaleFid(Object* armor);
int armorGetFemaleFid(Object* armor);
int miscItemGetMaxCharges(Object* miscItem);
int miscItemGetCharges(Object* miscItem);
int miscItemSetCharges(Object* miscItem, int charges);
int miscItemGetPowerType(Object* miscItem);
int miscItemGetPowerTypePid(Object* miscItem);
bool miscItemIsConsumable(Object* obj);
int _item_m_use_charged_item(Object* critter, Object* item);
int miscItemConsumeCharge(Object* miscItem);
int miscItemTrickleEventProcess(Object* item_obj, void* data);
bool miscItemIsOn(Object* obj);
int miscItemTurnOn(Object* item_obj);
int miscItemTurnOff(Object* item_obj);
int _item_m_turn_off_from_queue(Object* obj, void* data);
int containerGetMaxSize(Object* container);
int containerGetTotalSize(Object* container);
int ammoGetArmorClassModifier(Object* armor);
int ammoGetDamageResistanceModifier(Object* armor);
int ammoGetDamageMultiplier(Object* armor);
int ammoGetDamageDivisor(Object* armor);
int _item_d_take_drug(Object* critter_obj, Object* item_obj);
int _item_d_clear(Object* obj, void* data);
int drugEffectEventProcess(Object* obj, void* data);
int drugEffectEventRead(File* stream, void** dataPtr);
int drugEffectEventWrite(File* stream, void* data);
int _item_wd_clear(Object* obj, void* data);
int withdrawalEventProcess(Object* obj, void* data);
int withdrawalEventRead(File* stream, void** dataPtr);
int withdrawalEventWrite(File* stream, void* data);
int itemGetTotalCaps(Object* obj);
int itemCapsAdjust(Object* obj, int amount);
int itemGetMoney(Object* obj);
int itemSetMoney(Object* obj, int amount);

bool booksGetInfo(int bookPid, int* messageIdPtr, int* skillPtr);
bool explosionEmitsLight();
void weaponSetGrenadeExplosionRadius(int value);
void weaponSetRocketExplosionRadius(int value);
void explosiveAdd(int pid, int activePid, int minDamage, int maxDamage);
bool explosiveIsExplosive(int pid);
bool explosiveIsActiveExplosive(int pid);
bool explosiveActivate(int* pidPtr);
bool explosiveSetDamage(int pid, int minDamage, int maxDamage);
bool explosiveGetDamage(int pid, int* minDamagePtr, int* maxDamagePtr);
void explosionSettingsReset();
void explosionGetPattern(int* startRotationPtr, int* endRotationPtr);
void explosionSetPattern(int startRotation, int endRotation);
int explosionGetFrm();
void explosionSetFrm(int frm);
void explosionSetRadius(int radius);
int explosionGetDamageType();
void explosionSetDamageType(int damageType);
int explosionGetMaxTargets();
void explosionSetMaxTargets(int maxTargets);
bool itemIsHealing(int pid);

} // namespace fallout

#endif /* ITEM_H */
