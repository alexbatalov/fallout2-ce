#ifndef ITEM_H
#define ITEM_H

#include "db.h"
#include "message.h"
#include "obj_types.h"

#define ADDICTION_COUNT (9)

typedef enum _WeaponClass {
    ATTACK_TYPE_NONE,
    ATTACK_TYPE_UNARMED, // unarmed
    ATTACK_TYPE_MELEE, // melee
    ATTACK_TYPE_THROW,
    ATTACK_TYPE_RANGED,
    ATTACK_TYPE_COUNT,
} WeaponClass;

typedef struct DrugDescription {
    int drugPid;
    int gvar;
    int field_8;
} DrugDescription;

extern char _aItem_1[];

extern const int _attack_skill[9];
extern const int _attack_anim[9];
extern const int _attack_subtype[9];
extern DrugDescription gDrugDescriptions[ADDICTION_COUNT];
extern char* _name_item;

extern MessageList gItemsMessageList;
extern int _wd_onset;
extern Object* _wd_obj;
extern int _wd_gvar;

int itemsInit();
void itemsReset();
void itemsExit();
int _item_load_(File* stream);
int itemsLoad(File* stream);
int itemsSave(File* stream);
int itemAttemptAdd(Object* owner, Object* itemToAdd, int quantity);
int itemAdd(Object* owner, Object* itemToAdd, int quantity);
int itemRemove(Object* a1, Object* a2, int quantity);
void _item_compact(int inventoryItemIndex, Inventory* inventory);
int _item_move_func(Object* a1, Object* a2, Object* a3, int quantity, bool a5);
int _item_move(Object* a1, Object* a2, Object* a3, int quantity);
int _item_move_force(Object* a1, Object* a2, Object* a3, int quantity);
void _item_move_all(Object* a1, Object* a2);
int _item_move_all_hidden(Object* a1, Object* a2);
int _item_destroy_all_hidden(Object* a1);
int _item_drop_all(Object* critter, int tile);
bool _item_identical(Object* a1, Object* a2);
char* itemGetName(Object* obj);
char* itemGetDescription(Object* obj);
int itemGetType(Object* item);
int itemGetMaterial(Object* item);
int itemGetSize(Object* obj);
int itemGetWeight(Object* item);
int itemGetCost(Object* obj);
int objectGetCost(Object* obj);
int objectGetInventoryWeight(Object* obj);
bool _can_use_weapon(Object* item_obj);
int itemGetInventoryFid(Object* obj);
Object* critterGetWeaponForHitMode(Object* critter, int hitMode);
int _item_mp_cost(Object* obj, int hitMode, bool aiming);
int _item_count(Object* obj, Object* a2);
int _item_queued(Object* obj);
Object* _item_replace(Object* a1, Object* a2, int a3);
int weaponIsNatural(Object* obj);
int weaponGetAttackTypeForHitMode(Object* a1, int a2);
int weaponGetSkillForHitMode(Object* a1, int a2);
int _item_w_skill_level(Object* a1, int a2);
int weaponGetDamageMinMax(Object* weapon, int* minDamagePtr, int* maxDamagePtr);
int weaponGetMeleeDamage(Object* critter, int hitMode);
int weaponGetDamageType(Object* critter, Object* weapon);
int weaponIsTwoHanded(Object* weapon);
int critterGetAnimationForHitMode(Object* critter, int hitMode);
int weaponGetAnimationForHitMode(Object* weapon, int hitMode);
int ammoGetCapacity(Object* ammoOrWeapon);
int ammoGetQuantity(Object* ammoOrWeapon);
int ammoGetCaliber(Object* ammoOrWeapon);
void ammoSetQuantity(Object* ammoOrWeapon, int quantity);
int _item_w_try_reload(Object* critter, Object* weapon);
bool weaponCanBeReloadedWith(Object* weapon, Object* ammo);
int _item_w_reload(Object* weapon, Object* ammo);
int _item_w_range(Object* critter, int hitMode);
int _item_w_mp_cost(Object* critter, int hitMode, bool aiming);
int weaponGetMinStrengthRequired(Object* weapon);
int weaponGetCriticalFailureType(Object* weapon);
int weaponGetPerk(Object* weapon);
int weaponGetBurstRounds(Object* weapon);
int weaponGetAnimationCode(Object* weapon);
int weaponGetProjectilePid(Object* weapon);
int weaponGetAmmoTypePid(Object* weapon);
char weaponGetSoundId(Object* weapon);
int _item_w_called_shot(Object* critter, int hitMode);
int _item_w_can_unload(Object* weapon);
Object* _item_w_unload(Object* weapon);
int weaponGetActionPointCost1(Object* weapon);
int weaponGetActionPointCost2(Object* weapon);
int _item_w_compute_ammo_cost(Object* obj, int* inout_a2);
bool _item_w_is_grenade(Object* weapon);
int _item_w_area_damage_radius(Object* weapon, int hitMode);
int _item_w_grenade_dmg_radius(Object* weapon);
int _item_w_rocket_dmg_radius(Object* weapon);
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
int stealthBoyTurnOn(Object* object);
int stealthBoyTurnOff(Object* critter, Object* item);
int containerGetMaxSize(Object* container);
int containerGetTotalSize(Object* container);
int ammoGetArmorClassModifier(Object* armor);
int ammoGetDamageResistanceModifier(Object* armor);
int ammoGetDamageMultiplier(Object* armor);
int ammoGetDamageDivisor(Object* armor);
int _insert_drug_effect(Object* critter_obj, Object* item_obj, int a3, int* stats, int* mods);
void _perform_drug_effect(Object* critter_obj, int* stats, int* mods, bool is_immediate);
bool _drug_effect_allowed(Object* critter, int pid);
int _item_d_take_drug(Object* critter_obj, Object* item_obj);
int _item_d_clear(Object* obj, void* data);
int drugEffectEventProcess(Object* obj, void* data);
int drugEffectEventRead(File* stream, void** dataPtr);
int drugEffectEventWrite(File* stream, void* data);
int _insert_withdrawal(Object* obj, int a2, int a3, int a4, int a5);
int _item_wd_clear(Object* obj, void* a2);
int _item_wd_clear_all(Object* a1, void* data);
int withdrawalEventProcess(Object* obj, void* data);
int withdrawalEventRead(File* stream, void** dataPtr);
int withdrawalEventWrite(File* stream, void* data);
void performWithdrawalStart(Object* obj, int perk, int a3);
void performWithdrawalEnd(Object* obj, int a2);
int drugGetAddictionGvarByPid(int drugPid);
void dudeSetAddiction(int drugPid);
void dudeClearAddiction(int drugPid);
bool dudeIsAddicted(int drugPid);
int itemGetTotalCaps(Object* obj);
int itemCapsAdjust(Object* obj, int amount);
int itemGetMoney(Object* obj);
int itemSetMoney(Object* obj, int a2);

#endif /* ITEM_H */
