#include "item.h"

#include "animation.h"
#include "art.h"
#include "automap.h"
#include "combat.h"
#include "critter.h"
#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "interface.h"
#include "inventory.h"
#include "light.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "platform_compat.h"
#include "proto.h"
#include "proto_instance.h"
#include "queue.h"
#include "random.h"
#include "sfall_config.h"
#include "skill.h"
#include "stat.h"
#include "tile.h"
#include "trait.h"

#include <string.h>

#include <vector>

#define ADDICTION_COUNT (9)

// Max number of books that can be loaded from books.ini. This limit is imposed
// by Sfall.
#define BOOKS_MAX 50

static int _item_load_(File* stream);
static void _item_compact(int inventoryItemIndex, Inventory* inventory);
static int _item_move_func(Object* a1, Object* a2, Object* a3, int quantity, bool a5);
static bool _item_identical(Object* a1, Object* a2);
static int stealthBoyTurnOn(Object* object);
static int stealthBoyTurnOff(Object* critter, Object* item);
static int _insert_drug_effect(Object* critter_obj, Object* item_obj, int a3, int* stats, int* mods);
static void _perform_drug_effect(Object* critter_obj, int* stats, int* mods, bool is_immediate);
static bool _drug_effect_allowed(Object* critter, int pid);
static int _insert_withdrawal(Object* obj, int a2, int a3, int a4, int a5);
static int _item_wd_clear_all(Object* a1, void* data);
static void performWithdrawalStart(Object* obj, int perk, int a3);
static void performWithdrawalEnd(Object* obj, int a2);
static int drugGetAddictionGvarByPid(int drugPid);
static void dudeSetAddiction(int drugPid);
static void dudeClearAddiction(int drugPid);
static bool dudeIsAddicted(int drugPid);

static void booksInit();
static void booksInitVanilla();
static void booksInitCustom();
static void booksAdd(int bookPid, int messageId, int skill);
static void booksExit();

typedef struct DrugDescription {
    int drugPid;
    int gvar;
    int field_8;
} DrugDescription;

typedef struct BookDescription {
    int bookPid;
    int messageId;
    int skill;
} BookDescription;

// 0x509FFC
static char _aItem_1[] = "<item>";

// Maps weapon extended flags to skill.
//
// 0x519160
static const int _attack_skill[9] = {
    -1,
    SKILL_UNARMED,
    SKILL_UNARMED,
    SKILL_MELEE_WEAPONS,
    SKILL_MELEE_WEAPONS,
    SKILL_THROWING,
    SKILL_SMALL_GUNS,
    SKILL_SMALL_GUNS,
    SKILL_SMALL_GUNS,
};

// A map of item's extendedFlags to animation.
//
// 0x519184
static const int _attack_anim[9] = {
    ANIM_STAND,
    ANIM_THROW_PUNCH,
    ANIM_KICK_LEG,
    ANIM_SWING_ANIM,
    ANIM_THRUST_ANIM,
    ANIM_THROW_ANIM,
    ANIM_FIRE_SINGLE,
    ANIM_FIRE_BURST,
    ANIM_FIRE_CONTINUOUS,
};

// Maps weapon extended flags to weapon class
//
// 0x5191A8
static const int _attack_subtype[9] = {
    ATTACK_TYPE_NONE, // 0 // None
    ATTACK_TYPE_UNARMED, // 1 // Punch // Brass Knuckles, Power First
    ATTACK_TYPE_UNARMED, // 2 // Kick?
    ATTACK_TYPE_MELEE, // 3 // Swing //  Sledgehammer (prim), Club, Knife (prim), Spear (prim), Crowbar
    ATTACK_TYPE_MELEE, // 4 // Thrust // Sledgehammer (sec), Knife (sec), Spear (sec)
    ATTACK_TYPE_THROW, // 5 // Throw // Rock,
    ATTACK_TYPE_RANGED, // 6 // Single // 10mm SMG (prim), Rocket Launcher, Hunting Rifle, Plasma Rifle, Laser Pistol
    ATTACK_TYPE_RANGED, // 7 // Burst // 10mm SMG (sec), Minigun
    ATTACK_TYPE_RANGED, // 8 // Continous // Only: Flamer, Improved Flamer, Flame Breath
};

// 0x5191CC
static DrugDescription gDrugDescriptions[ADDICTION_COUNT] = {
    { PROTO_ID_NUKA_COLA, GVAR_NUKA_COLA_ADDICT, 0 },
    { PROTO_ID_BUFF_OUT, GVAR_BUFF_OUT_ADDICT, 4 },
    { PROTO_ID_MENTATS, GVAR_MENTATS_ADDICT, 4 },
    { PROTO_ID_PSYCHO, GVAR_PSYCHO_ADDICT, 4 },
    { PROTO_ID_RADAWAY, GVAR_RADAWAY_ADDICT, 0 },
    { PROTO_ID_BEER, GVAR_ALCOHOL_ADDICT, 0 },
    { PROTO_ID_BOOZE, GVAR_ALCOHOL_ADDICT, 0 },
    { PROTO_ID_JET, GVAR_ADDICT_JET, 4 },
    { PROTO_ID_DECK_OF_TRAGIC_CARDS, GVAR_ADDICT_TRAGIC, 0 },
};

// 0x519238
static char* _name_item = _aItem_1;

// item.msg
//
// 0x59E980
static MessageList gItemsMessageList;

// 0x59E988
static int _wd_onset;

// 0x59E98C
static Object* _wd_obj;

// 0x59E990
static int _wd_gvar;

static std::vector<BookDescription> gBooks;

// 0x4770E0
int itemsInit()
{
    if (!messageListInit(&gItemsMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s%s", asc_5186C8, "item.msg");

    if (!messageListLoad(&gItemsMessageList, path)) {
        return -1;
    }

    // SFALL
    booksInit();

    return 0;
}

// 0x477144
void itemsReset()
{
    return;
}

// 0x477148
void itemsExit()
{
    messageListFree(&gItemsMessageList);

    // SFALL
    booksExit();
}

// NOTE: Collapsed.
//
// 0x477154
static int _item_load_(File* stream)
{
    return 0;
}

// NOTE: Uncollapsed 0x477154.
int itemsLoad(File* stream)
{
    return _item_load_(stream);
}

// NOTE: Uncollapsed 0x477154.
int itemsSave(File* stream)
{
    return _item_load_(stream);
}

// 0x477158
int itemAttemptAdd(Object* owner, Object* itemToAdd, int quantity)
{
    if (quantity < 1) {
        return -1;
    }

    int parentType = FID_TYPE(owner->fid);
    if (parentType == OBJ_TYPE_ITEM) {
        int itemType = itemGetType(owner);
        if (itemType == ITEM_TYPE_CONTAINER) {
            // NOTE: Uninline.
            int sizeToAdd = itemGetSize(itemToAdd);
            sizeToAdd *= quantity;

            int currentSize = containerGetTotalSize(owner);
            int maxSize = containerGetMaxSize(owner);
            if (currentSize + sizeToAdd >= maxSize) {
                return -6;
            }

            Object* containerOwner = objectGetOwner(owner);
            if (containerOwner != NULL) {
                if (FID_TYPE(containerOwner->fid) == OBJ_TYPE_CRITTER) {
                    int weightToAdd = itemGetWeight(itemToAdd);
                    weightToAdd *= quantity;

                    int currentWeight = objectGetInventoryWeight(containerOwner);
                    int maxWeight = critterGetStat(containerOwner, STAT_CARRY_WEIGHT);
                    if (currentWeight + weightToAdd > maxWeight) {
                        return -6;
                    }
                }
            }
        } else if (itemType == ITEM_TYPE_MISC) {
            // NOTE: Uninline.
            int powerTypePid = miscItemGetPowerTypePid(owner);
            if (powerTypePid != itemToAdd->pid) {
                return -1;
            }
        } else {
            return -1;
        }
    } else if (parentType == OBJ_TYPE_CRITTER) {
        if (critterGetBodyType(owner) != BODY_TYPE_BIPED) {
            // SFALL: Fix for being unable to plant items on non-biped critters
            // with the "Barter" flag set (e.g. Skynet and Goris).
            Proto* proto;
            if (protoGetProto(owner->pid, &proto) == -1) {
                return -5;
            }

            if ((proto->critter.flags & 0x02) == 0) {
                return -5;
            }
        }

        int weightToAdd = itemGetWeight(itemToAdd);
        weightToAdd *= quantity;

        int currentWeight = objectGetInventoryWeight(owner);
        int maxWeight = critterGetStat(owner, STAT_CARRY_WEIGHT);
        if (currentWeight + weightToAdd > maxWeight) {
            return -6;
        }
    }

    return itemAdd(owner, itemToAdd, quantity);
}

// item_add
// 0x4772B8
int itemAdd(Object* owner, Object* itemToAdd, int quantity)
{
    if (quantity < 1) {
        return -1;
    }

    Inventory* inventory = &(owner->data.inventory);

    int index;
    for (index = 0; index < inventory->length; index++) {
        if (_item_identical(inventory->items[index].item, itemToAdd) != 0) {
            break;
        }
    }

    if (index == inventory->length) {
        if (inventory->length == inventory->capacity || inventory->items == NULL) {
            InventoryItem* inventoryItems = (InventoryItem*)internal_realloc(inventory->items, sizeof(InventoryItem) * (inventory->capacity + 10));
            if (inventoryItems == NULL) {
                return -1;
            }

            inventory->items = inventoryItems;
            inventory->capacity += 10;
        }

        inventory->items[inventory->length].item = itemToAdd;
        inventory->items[inventory->length].quantity = quantity;

        if (itemToAdd->pid == PROTO_ID_STEALTH_BOY_II) {
            if ((itemToAdd->flags & OBJECT_IN_ANY_HAND) != 0) {
                // NOTE: Uninline.
                stealthBoyTurnOn(owner);
            }
        }

        inventory->length++;
        itemToAdd->owner = owner;

        return 0;
    }

    if (itemToAdd == inventory->items[index].item) {
        debugPrint("Warning! Attempt to add same item twice in item_add()\n");
        return 0;
    }

    if (itemGetType(itemToAdd) == ITEM_TYPE_AMMO) {
        // NOTE: Uninline.
        int ammoQuantityToAdd = ammoGetQuantity(itemToAdd);

        int ammoQuantity = ammoGetQuantity(inventory->items[index].item);

        // NOTE: Uninline.
        int capacity = ammoGetCapacity(itemToAdd);

        ammoQuantity += ammoQuantityToAdd;
        if (ammoQuantity > capacity) {
            ammoSetQuantity(itemToAdd, ammoQuantity - capacity);
            inventory->items[index].quantity++;
        } else {
            ammoSetQuantity(itemToAdd, ammoQuantity);
        }

        inventory->items[index].quantity += quantity - 1;
    } else {
        inventory->items[index].quantity += quantity;
    }

    objectDestroy(inventory->items[index].item, NULL);
    inventory->items[index].item = itemToAdd;
    itemToAdd->owner = owner;

    return 0;
}

// 0x477490
int itemRemove(Object* owner, Object* itemToRemove, int quantity)
{
    Inventory* inventory = &(owner->data.inventory);
    Object* item1 = critterGetItem1(owner);
    Object* item2 = critterGetItem2(owner);

    int index = 0;
    for (; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (inventoryItem->item == itemToRemove) {
            break;
        }

        if (itemGetType(inventoryItem->item) == ITEM_TYPE_CONTAINER) {
            if (itemRemove(inventoryItem->item, itemToRemove, quantity) == 0) {
                return 0;
            }
        }
    }

    if (index == inventory->length) {
        return -1;
    }

    InventoryItem* inventoryItem = &(inventory->items[index]);
    if (inventoryItem->quantity < quantity) {
        return -1;
    }

    if (inventoryItem->quantity == quantity) {
        // NOTE: Uninline.
        _item_compact(index, inventory);
    } else {
        // TODO: Not sure about this line.
        if (_obj_copy(&(inventoryItem->item), itemToRemove) == -1) {
            return -1;
        }

        _obj_disconnect(inventoryItem->item, NULL);

        inventoryItem->quantity -= quantity;

        if (itemGetType(itemToRemove) == ITEM_TYPE_AMMO) {
            int capacity = ammoGetCapacity(itemToRemove);
            ammoSetQuantity(inventoryItem->item, capacity);
        }
    }

    if (itemToRemove->pid == PROTO_ID_STEALTH_BOY_I || itemToRemove->pid == PROTO_ID_STEALTH_BOY_II) {
        if (itemToRemove == item1 || itemToRemove == item2) {
            Object* owner = objectGetOwner(itemToRemove);
            if (owner != NULL) {
                stealthBoyTurnOff(owner, itemToRemove);
            }
        }
    }

    itemToRemove->owner = NULL;
    itemToRemove->flags &= ~OBJECT_EQUIPPED;

    return 0;
}

// NOTE: Inlined.
//
// 0x4775D8
static void _item_compact(int inventoryItemIndex, Inventory* inventory)
{
    for (int index = inventoryItemIndex + 1; index < inventory->length; index++) {
        InventoryItem* prev = &(inventory->items[index - 1]);
        InventoryItem* curr = &(inventory->items[index]);
        memcpy(prev, curr, sizeof(*prev));
    }
    inventory->length--;
}

// 0x477608
static int _item_move_func(Object* a1, Object* a2, Object* a3, int quantity, bool a5)
{
    if (itemRemove(a1, a3, quantity) == -1) {
        return -1;
    }

    int rc;
    if (a5) {
        rc = itemAdd(a2, a3, quantity);
    } else {
        rc = itemAttemptAdd(a2, a3, quantity);
    }

    if (rc != 0) {
        if (itemAdd(a1, a3, quantity) != 0) {
            Object* owner = objectGetOwner(a1);
            if (owner == NULL) {
                owner = a1;
            }

            if (owner->tile != -1) {
                Rect updatedRect;
                _obj_connect(a3, owner->tile, owner->elevation, &updatedRect);
                tileWindowRefreshRect(&updatedRect, gElevation);
            }
        }
        return -1;
    }

    a3->owner = a2;

    return 0;
}

// 0x47769C
int _item_move(Object* a1, Object* a2, Object* a3, int quantity)
{
    return _item_move_func(a1, a2, a3, quantity, false);
}

// 0x4776A4
int _item_move_force(Object* a1, Object* a2, Object* a3, int quantity)
{
    return _item_move_func(a1, a2, a3, quantity, true);
}

// 0x4776AC
void _item_move_all(Object* a1, Object* a2)
{
    Inventory* inventory = &(a1->data.inventory);
    while (inventory->length > 0) {
        InventoryItem* inventoryItem = &(inventory->items[0]);
        _item_move_func(a1, a2, inventoryItem->item, inventoryItem->quantity, true);
    }
}

// 0x4776E0
int _item_move_all_hidden(Object* a1, Object* a2)
{
    Inventory* inventory = &(a1->data.inventory);
    // TODO: Not sure about two loops.
    for (int i = 0; i < inventory->length;) {
        for (int j = i; j < inventory->length;) {
            bool v5;
            InventoryItem* inventoryItem = &(inventory->items[j]);
            if (PID_TYPE(inventoryItem->item->pid) == OBJ_TYPE_ITEM) {
                Proto* proto;
                if (protoGetProto(inventoryItem->item->pid, &proto) != -1) {
                    v5 = (proto->item.extendedFlags & ItemProtoExtendedFlags_NaturalWeapon) == 0;
                } else {
                    v5 = true;
                }
            } else {
                v5 = true;
            }

            if (!v5) {
                _item_move_func(a1, a2, inventoryItem->item, inventoryItem->quantity, true);
            } else {
                i++;
                j++;
            }
        }
    }
    return 0;
}

// 0x477770
int _item_destroy_all_hidden(Object* a1)
{
    Inventory* inventory = &(a1->data.inventory);
    // TODO: Not sure about this one. Why two loops?
    for (int i = 0; i < inventory->length;) {
        // TODO: Probably wrong, something with two loops.
        for (int j = i; j < inventory->length;) {
            bool v5;
            InventoryItem* inventoryItem = &(inventory->items[j]);
            if (PID_TYPE(inventoryItem->item->pid) == OBJ_TYPE_ITEM) {
                Proto* proto;
                if (protoGetProto(inventoryItem->item->pid, &proto) != -1) {
                    v5 = (proto->item.extendedFlags & ItemProtoExtendedFlags_NaturalWeapon) == 0;
                } else {
                    v5 = true;
                }
            } else {
                v5 = true;
            }

            if (!v5) {
                itemRemove(a1, inventoryItem->item, 1);
                _obj_destroy(inventoryItem->item);
            } else {
                i++;
                j++;
            }
        }
    }
    return 0;
}

// 0x477804
int _item_drop_all(Object* critter, int tile)
{
    bool hasEquippedItems = false;

    int frmId = critter->fid & 0xFFF;

    Inventory* inventory = &(critter->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        Object* item = inventoryItem->item;
        if (item->pid == PROTO_ID_MONEY) {
            if (itemRemove(critter, item, inventoryItem->quantity) != 0) {
                return -1;
            }

            if (_obj_connect(item, tile, critter->elevation, NULL) != 0) {
                if (itemAdd(critter, item, 1) != 0) {
                    _obj_destroy(item);
                }
                return -1;
            }

            item->data.item.misc.charges = inventoryItem->quantity;
        } else {
            if ((item->flags & OBJECT_EQUIPPED) != 0) {
                hasEquippedItems = true;

                if ((item->flags & OBJECT_WORN) != 0) {
                    Proto* proto;
                    if (protoGetProto(critter->pid, &proto) == -1) {
                        return -1;
                    }

                    frmId = proto->fid & 0xFFF;
                    _adjust_ac(critter, item, NULL);
                }
            }

            for (int index = 0; index < inventoryItem->quantity; index++) {
                if (itemRemove(critter, item, 1) != 0) {
                    return -1;
                }

                if (_obj_connect(item, tile, critter->elevation, NULL) != 0) {
                    if (itemAdd(critter, item, 1) != 0) {
                        _obj_destroy(item);
                    }
                    return -1;
                }
            }
        }
    }

    if (hasEquippedItems) {
        Rect updatedRect;
        int fid = buildFid(OBJ_TYPE_CRITTER, frmId, FID_ANIM_TYPE(critter->fid), 0, (critter->fid & 0x70000000) >> 28);
        objectSetFid(critter, fid, &updatedRect);
        if (FID_ANIM_TYPE(critter->fid) == ANIM_STAND) {
            tileWindowRefreshRect(&updatedRect, gElevation);
        }
    }

    return -1;
}

// 0x4779F0
static bool _item_identical(Object* a1, Object* a2)
{
    if (a1->pid != a2->pid) {
        return false;
    }

    if (a1->sid != a2->sid) {
        return false;
    }

    if ((a1->flags & (OBJECT_EQUIPPED | OBJECT_USED)) != 0) {
        return false;
    }

    if ((a2->flags & (OBJECT_EQUIPPED | OBJECT_USED)) != 0) {
        return false;
    }

    Proto* proto;
    protoGetProto(a1->pid, &proto);
    if (proto->item.type == ITEM_TYPE_CONTAINER) {
        return false;
    }

    Inventory* inventory1 = &(a1->data.inventory);
    Inventory* inventory2 = &(a2->data.inventory);
    if (inventory1->length != 0 || inventory2->length != 0) {
        return false;
    }

    int v1;
    if (proto->item.type == ITEM_TYPE_AMMO || a1->pid == PROTO_ID_MONEY) {
        v1 = a2->data.item.ammo.quantity;
        a2->data.item.ammo.quantity = a1->data.item.ammo.quantity;
    }

    // NOTE: Probably inlined memcmp, but I'm not sure why it only checks 32
    // bytes.
    int i;
    for (i = 0; i < 8; i++) {
        if (a1->field_2C_array[i] != a2->field_2C_array[i]) {
            break;
        }
    }

    if (proto->item.type == ITEM_TYPE_AMMO || a1->pid == PROTO_ID_MONEY) {
        a2->data.item.ammo.quantity = v1;
    }

    return i == 8;
}

// 0x477AE4
char* itemGetName(Object* obj)
{
    _name_item = protoGetName(obj->pid);
    return _name_item;
}

// 0x477AF4
char* itemGetDescription(Object* obj)
{
    return protoGetDescription(obj->pid);
}

// 0x477AFC
int itemGetType(Object* item)
{
    if (item == NULL) {
        return ITEM_TYPE_MISC;
    }

    if (PID_TYPE(item->pid) != OBJ_TYPE_ITEM) {
        return ITEM_TYPE_MISC;
    }

    if (item->pid == PROTO_ID_SHIV) {
        return ITEM_TYPE_MISC;
    }

    Proto* proto;
    protoGetProto(item->pid, &proto);

    return proto->item.type;
}

// NOTE: Unused.
//
// 0x477B4C
int itemGetMaterial(Object* item)
{
    Proto* proto;
    protoGetProto(item->pid, &proto);

    return proto->item.material;
}

// 0x477B68
int itemGetSize(Object* item)
{
    if (item == NULL) {
        return 0;
    }

    Proto* proto;
    protoGetProto(item->pid, &proto);

    return proto->item.size;
}

// 0x477B88
int itemGetWeight(Object* item)
{
    if (item == NULL) {
        return 0;
    }

    Proto* proto;
    protoGetProto(item->pid, &proto);
    int weight = proto->item.weight;

    // NOTE: Uninline.
    if (weaponIsNatural(item)) {
        weight = 0;
    }

    int itemType = proto->item.type;
    if (itemType == ITEM_TYPE_ARMOR) {
        switch (proto->pid) {
        case PROTO_ID_POWER_ARMOR:
        case PROTO_ID_HARDENED_POWER_ARMOR:
        case PROTO_ID_ADVANCED_POWER_ARMOR:
        case PROTO_ID_ADVANCED_POWER_ARMOR_MK_II:
            weight /= 2;
            break;
        }
    } else if (itemType == ITEM_TYPE_CONTAINER) {
        weight += objectGetInventoryWeight(item);
    } else if (itemType == ITEM_TYPE_WEAPON) {
        // NOTE: Uninline.
        int ammoQuantity = ammoGetQuantity(item);
        if (ammoQuantity > 0) {
            // NOTE: Uninline.
            int ammoTypePid = weaponGetAmmoTypePid(item);
            if (ammoTypePid != -1) {
                Proto* ammoProto;
                if (protoGetProto(ammoTypePid, &ammoProto) != -1) {
                    weight += ammoProto->item.weight * ((ammoQuantity - 1) / ammoProto->item.data.ammo.quantity + 1);
                }
            }
        }
    }

    return weight;
}

// Returns cost of item.
//
// When [item] is container the returned cost includes cost of container
// itself plus cost of contained items.
//
// When [item] is a weapon the returned value includes cost of weapon
// itself plus cost of remaining ammo (see below).
//
// When [item] is an ammo it's cost is calculated from ratio of fullness.
//
// 0x477CAC
int itemGetCost(Object* obj)
{
    // TODO: This function needs review. A lot of functionality is inlined.
    // Find these functions and use them.
    if (obj == NULL) {
        return 0;
    }

    Proto* proto;
    protoGetProto(obj->pid, &proto);

    int cost = proto->item.cost;

    switch (proto->item.type) {
    case ITEM_TYPE_CONTAINER:
        cost += objectGetCost(obj);
        break;
    case ITEM_TYPE_WEAPON:
        if (1) {
            // NOTE: Uninline.
            int ammoQuantity = ammoGetQuantity(obj);
            if (ammoQuantity > 0) {
                // NOTE: Uninline.
                int ammoTypePid = weaponGetAmmoTypePid(obj);
                if (ammoTypePid != -1) {
                    Proto* ammoProto;
                    protoGetProto(ammoTypePid, &ammoProto);

                    cost += ammoQuantity * ammoProto->item.cost / ammoProto->item.data.ammo.quantity;
                }
            }
        }
        break;
    case ITEM_TYPE_AMMO:
        if (1) {
            // NOTE: Uninline.
            int ammoQuantity = ammoGetQuantity(obj);
            cost *= ammoQuantity;
            // NOTE: Uninline.
            int ammoCapacity = ammoGetCapacity(obj);
            cost /= ammoCapacity;
        }
        break;
    }

    return cost;
}

// Returns cost of object's items.
//
// 0x477DAC
int objectGetCost(Object* obj)
{
    if (obj == NULL) {
        return 0;
    }

    int cost = 0;

    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (itemGetType(inventoryItem->item) == ITEM_TYPE_AMMO) {
            Proto* proto;
            protoGetProto(inventoryItem->item->pid, &proto);

            // Ammo stack in inventory is a bit special. It is counted in clips,
            // `inventoryItem->quantity` is the number of clips. The ammo object
            // itself tracks remaining number of ammo in only one instance of
            // the clip implying all other clips in the stack are full.
            //
            // In order to correctly calculate cost of the ammo stack, add cost
            // of all full clips...
            cost += proto->item.cost * (inventoryItem->quantity - 1);

            // ...and add cost of the current clip, which is proportional to
            // it's capacity.
            cost += itemGetCost(inventoryItem->item);
        } else {
            cost += itemGetCost(inventoryItem->item) * inventoryItem->quantity;
        }
    }

    if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER) {
        Object* item2 = critterGetItem2(obj);
        if (item2 != NULL && (item2->flags & OBJECT_IN_RIGHT_HAND) == 0) {
            cost += itemGetCost(item2);
        }

        Object* item1 = critterGetItem1(obj);
        if (item1 != NULL && (item1->flags & OBJECT_IN_LEFT_HAND) == 0) {
            cost += itemGetCost(item1);
        }

        Object* armor = critterGetArmor(obj);
        if (armor != NULL && (armor->flags & OBJECT_WORN) == 0) {
            cost += itemGetCost(armor);
        }
    }

    return cost;
}

// Calculates total weight of the items in inventory.
//
// 0x477E98
int objectGetInventoryWeight(Object* obj)
{
    if (obj == NULL) {
        return 0;
    }

    int weight = 0;

    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        Object* item = inventoryItem->item;
        weight += itemGetWeight(item) * inventoryItem->quantity;
    }

    if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER) {
        Object* item2 = critterGetItem2(obj);
        if (item2 != NULL) {
            if ((item2->flags & OBJECT_IN_RIGHT_HAND) == 0) {
                weight += itemGetWeight(item2);
            }
        }

        Object* item1 = critterGetItem1(obj);
        if (item1 != NULL) {
            if ((item1->flags & OBJECT_IN_LEFT_HAND) == 0) {
                weight += itemGetWeight(item1);
            }
        }

        Object* armor = critterGetArmor(obj);
        if (armor != NULL) {
            if ((armor->flags & OBJECT_WORN) == 0) {
                weight += itemGetWeight(armor);
            }
        }
    }

    return weight;
}

// 0x477F3C
bool _can_use_weapon(Object* weapon)
{
    if (weapon == NULL) {
        return false;
    }

    if (itemGetType(weapon) != ITEM_TYPE_WEAPON) {
        return false;
    }

    int flags = gDude->data.critter.combat.results;
    if ((flags & DAM_CRIP_ARM_LEFT) != 0 && (flags & DAM_CRIP_ARM_RIGHT) != 0) {
        return true;
    }

    // NOTE: Uninline.
    bool isTwoHanded = weaponIsTwoHanded(weapon);
    if (isTwoHanded) {
        if ((flags & DAM_CRIP_ARM_LEFT) != 0 || (flags & DAM_CRIP_ARM_RIGHT) != 0) {
            return true;
        }
    }

    return false;
}

// 0x477FB0
int itemGetInventoryFid(Object* item)
{
    Proto* proto;

    if (item == NULL) {
        return -1;
    }

    protoGetProto(item->pid, &proto);

    return proto->item.inventoryFid;
}

// 0x477FF8
Object* critterGetWeaponForHitMode(Object* critter, int hitMode)
{
    switch (hitMode) {
    case HIT_MODE_LEFT_WEAPON_PRIMARY:
    case HIT_MODE_LEFT_WEAPON_SECONDARY:
    case HIT_MODE_LEFT_WEAPON_RELOAD:
        return critterGetItem1(critter);
    case HIT_MODE_RIGHT_WEAPON_PRIMARY:
    case HIT_MODE_RIGHT_WEAPON_SECONDARY:
    case HIT_MODE_RIGHT_WEAPON_RELOAD:
        return critterGetItem2(critter);
    }

    return NULL;
}

// 0x478040
int _item_mp_cost(Object* obj, int hitMode, bool aiming)
{
    if (obj == NULL) {
        return 0;
    }

    Object* item_obj = critterGetWeaponForHitMode(obj, hitMode);

    if (item_obj != NULL && itemGetType(item_obj) != ITEM_TYPE_WEAPON) {
        return 2;
    }

    return _item_w_mp_cost(obj, hitMode, aiming);
}

// Returns quantity of [a2] in [obj]s inventory.
//
// 0x47808C
int _item_count(Object* obj, Object* a2)
{
    int quantity = 0;

    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        Object* item = inventoryItem->item;
        if (item == a2) {
            quantity = inventoryItem->quantity;
        } else {
            if (itemGetType(item) == ITEM_TYPE_CONTAINER) {
                quantity = _item_count(item, a2);
                if (quantity > 0) {
                    return quantity;
                }
            }
        }
    }

    return quantity;
}

// Returns true if [a1] posesses an item with 0x2000 flag.
//
// 0x4780E4
int _item_queued(Object* obj)
{
    if (obj == NULL) {
        return false;
    }

    if ((obj->flags & OBJECT_USED) != 0) {
        return true;
    }

    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if ((inventoryItem->item->flags & OBJECT_USED) != 0) {
            return true;
        }

        if (itemGetType(inventoryItem->item) == ITEM_TYPE_CONTAINER) {
            if (_item_queued(inventoryItem->item)) {
                return true;
            }
        }
    }

    return false;
}

// 0x478154
Object* _item_replace(Object* a1, Object* a2, int a3)
{
    if (a1 == NULL) {
        return NULL;
    }

    if (a2 == NULL) {
        return NULL;
    }

    Inventory* inventory = &(a1->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (_item_identical(inventoryItem->item, a2)) {
            Object* item = inventoryItem->item;
            if (itemRemove(a1, item, 1) == 0) {
                item->flags |= a3;
                if (itemAdd(a1, item, 1) == 0) {
                    return item;
                }

                item->flags &= ~a3;
                if (itemAdd(a1, item, 1) != 0) {
                    _obj_destroy(item);
                }
            }
        }

        if (itemGetType(inventoryItem->item) == ITEM_TYPE_CONTAINER) {
            Object* obj = _item_replace(inventoryItem->item, a2, a3);
            if (obj != NULL) {
                return obj;
            }
        }
    }

    return NULL;
}

// Returns true if [item] is an natural weapon of it's owner.
//
// See [ItemProtoExtendedFlags_NaturalWeapon] for more details on natural weapons.
//
// 0x478244
int weaponIsNatural(Object* obj)
{
    Proto* proto;

    if (PID_TYPE(obj->pid) != OBJ_TYPE_ITEM) {
        return 0;
    }

    if (protoGetProto(obj->pid, &proto) == -1) {
        return 0;
    }

    return proto->item.extendedFlags & ItemProtoExtendedFlags_NaturalWeapon;
}

// 0x478280
int weaponGetAttackTypeForHitMode(Object* weapon, int hitMode)
{
    if (weapon == NULL) {
        return ATTACK_TYPE_UNARMED;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    int index;
    if (hitMode == HIT_MODE_LEFT_WEAPON_PRIMARY || hitMode == HIT_MODE_RIGHT_WEAPON_PRIMARY) {
        index = proto->item.extendedFlags & 0xF;
    } else {
        index = (proto->item.extendedFlags & 0xF0) >> 4;
    }

    return _attack_subtype[index];
}

// 0x4782CC
int weaponGetSkillForHitMode(Object* weapon, int hitMode)
{
    if (weapon == NULL) {
        return SKILL_UNARMED;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    int index;
    if (hitMode == HIT_MODE_LEFT_WEAPON_PRIMARY || hitMode == HIT_MODE_RIGHT_WEAPON_PRIMARY) {
        index = proto->item.extendedFlags & 0xF;
    } else {
        index = (proto->item.extendedFlags & 0xF0) >> 4;
    }

    int skill = _attack_skill[index];

    if (skill == SKILL_SMALL_GUNS) {
        int damageType = weaponGetDamageType(NULL, weapon);
        if (damageType == DAMAGE_TYPE_LASER || damageType == DAMAGE_TYPE_PLASMA || damageType == DAMAGE_TYPE_ELECTRICAL) {
            skill = SKILL_ENERGY_WEAPONS;
        } else {
            if ((proto->item.extendedFlags & ItemProtoExtendedFlags_BigGun) != 0) {
                skill = SKILL_BIG_GUNS;
            }
        }
    }

    return skill;
}

// Returns skill value when critter is about to perform hitMode.
//
// 0x478370
int _item_w_skill_level(Object* critter, int hitMode)
{
    if (critter == NULL) {
        return 0;
    }

    int skill;

    // NOTE: Uninline.
    Object* weapon = critterGetWeaponForHitMode(critter, hitMode);
    if (weapon != NULL) {
        skill = weaponGetSkillForHitMode(weapon, hitMode);
    } else {
        skill = SKILL_UNARMED;
    }

    return skillGetValue(critter, skill);
}

// 0x4783B8
int weaponGetDamageMinMax(Object* weapon, int* minDamagePtr, int* maxDamagePtr)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    if (minDamagePtr != NULL) {
        *minDamagePtr = proto->item.data.weapon.minDamage;
    }

    if (maxDamagePtr != NULL) {
        *maxDamagePtr = proto->item.data.weapon.maxDamage;
    }

    return 0;
}

// 0x478448
int weaponGetMeleeDamage(Object* critter, int hitMode)
{
    if (critter == NULL) {
        return 0;
    }

    int minDamage = 0;
    int maxDamage = 0;
    int meleeDamage = 0;
    int unarmedDamage = 0;

    // NOTE: Uninline.
    Object* weapon = critterGetWeaponForHitMode(critter, hitMode);

    if (weapon != NULL) {
        Proto* proto;
        protoGetProto(weapon->pid, &proto);

        minDamage = proto->item.data.weapon.minDamage;
        maxDamage = proto->item.data.weapon.maxDamage;

        int attackType = weaponGetAttackTypeForHitMode(weapon, hitMode);
        if (attackType == ATTACK_TYPE_MELEE || attackType == ATTACK_TYPE_UNARMED) {
            meleeDamage = critterGetStat(critter, STAT_MELEE_DAMAGE);
        }
    } else {
        minDamage = 1;
        maxDamage = critterGetStat(critter, STAT_MELEE_DAMAGE) + 2;

        switch (hitMode) {
        case HIT_MODE_STRONG_PUNCH:
        case HIT_MODE_JAB:
            unarmedDamage = 3;
            break;
        case HIT_MODE_HAMMER_PUNCH:
        case HIT_MODE_STRONG_KICK:
            unarmedDamage = 4;
            break;
        case HIT_MODE_HAYMAKER:
        case HIT_MODE_PALM_STRIKE:
        case HIT_MODE_SNAP_KICK:
        case HIT_MODE_HIP_KICK:
            unarmedDamage = 7;
            break;
        case HIT_MODE_POWER_KICK:
        case HIT_MODE_HOOK_KICK:
            unarmedDamage = 9;
            break;
        case HIT_MODE_PIERCING_STRIKE:
            unarmedDamage = 10;
            break;
        case HIT_MODE_PIERCING_KICK:
            unarmedDamage = 12;
            break;
        }
    }

    return randomBetween(unarmedDamage + minDamage, unarmedDamage + meleeDamage + maxDamage);
}

// 0x478570
int weaponGetDamageType(Object* critter, Object* weapon)
{
    Proto* proto;

    if (weapon != NULL) {
        protoGetProto(weapon->pid, &proto);

        return proto->item.data.weapon.damageType;
    }

    if (critter != NULL) {
        return critterGetDamageType(critter);
    }

    return 0;
}

// 0x478598
int weaponIsTwoHanded(Object* weapon)
{
    Proto* proto;

    if (weapon == NULL) {
        return 0;
    }

    protoGetProto(weapon->pid, &proto);

    return (proto->item.extendedFlags & WEAPON_TWO_HAND) != 0;
}

// 0x4785DC
int critterGetAnimationForHitMode(Object* critter, int hitMode)
{
    // NOTE: Uninline.
    Object* weapon = critterGetWeaponForHitMode(critter, hitMode);
    return weaponGetAnimationForHitMode(weapon, hitMode);
}

// 0x47860C
int weaponGetAnimationForHitMode(Object* weapon, int hitMode)
{
    if (hitMode == HIT_MODE_KICK || (hitMode >= FIRST_ADVANCED_KICK_HIT_MODE && hitMode <= LAST_ADVANCED_KICK_HIT_MODE)) {
        return ANIM_KICK_LEG;
    }

    if (weapon == NULL) {
        return ANIM_THROW_PUNCH;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    int index;
    if (hitMode == HIT_MODE_LEFT_WEAPON_PRIMARY || hitMode == HIT_MODE_RIGHT_WEAPON_PRIMARY) {
        index = proto->item.extendedFlags & 0xF;
    } else {
        index = (proto->item.extendedFlags & 0xF0) >> 4;
    }

    return _attack_anim[index];
}

// 0x478674
int ammoGetCapacity(Object* ammoOrWeapon)
{
    if (ammoOrWeapon == NULL) {
        return 0;
    }

    Proto* proto;
    protoGetProto(ammoOrWeapon->pid, &proto);

    if (proto->item.type == ITEM_TYPE_AMMO) {
        return proto->item.data.ammo.quantity;
    } else {
        return proto->item.data.weapon.ammoCapacity;
    }
}

// 0x4786A0
int ammoGetQuantity(Object* ammoOrWeapon)
{
    if (ammoOrWeapon == NULL) {
        return 0;
    }

    Proto* proto;
    protoGetProto(ammoOrWeapon->pid, &proto);

    // NOTE: Looks like the condition jumps were erased during compilation only
    // because ammo's quantity and weapon's ammo quantity coincidently stored
    // in the same offset relative to [Object].
    if (proto->item.type == ITEM_TYPE_AMMO) {
        return ammoOrWeapon->data.item.ammo.quantity;
    } else {
        return ammoOrWeapon->data.item.weapon.ammoQuantity;
    }
}

// 0x4786C8
int ammoGetCaliber(Object* ammoOrWeapon)
{
    Proto* proto;

    if (ammoOrWeapon == NULL) {
        return 0;
    }

    protoGetProto(ammoOrWeapon->pid, &proto);

    if (proto->item.type != ITEM_TYPE_AMMO) {
        if (protoGetProto(ammoOrWeapon->data.item.weapon.ammoTypePid, &proto) == -1) {
            return 0;
        }
    }

    return proto->item.data.ammo.caliber;
}

// 0x478714
void ammoSetQuantity(Object* ammoOrWeapon, int quantity)
{
    if (ammoOrWeapon == NULL) {
        return;
    }

    // NOTE: Uninline.
    int capacity = ammoGetCapacity(ammoOrWeapon);
    if (quantity > capacity) {
        quantity = capacity;
    }

    Proto* proto;
    protoGetProto(ammoOrWeapon->pid, &proto);

    if (proto->item.type == ITEM_TYPE_AMMO) {
        ammoOrWeapon->data.item.ammo.quantity = quantity;
    } else {
        ammoOrWeapon->data.item.weapon.ammoQuantity = quantity;
    }
}

// 0x478768
int _item_w_try_reload(Object* critter, Object* weapon)
{
    // NOTE: Uninline.
    int quantity = ammoGetQuantity(weapon);
    int capacity = ammoGetCapacity(weapon);
    if (quantity == capacity) {
        return -1;
    }

    if (weapon->pid != PROTO_ID_SOLAR_SCORCHER) {
        int inventoryItemIndex = -1;
        for (;;) {
            Object* ammo = _inven_find_type(critter, ITEM_TYPE_AMMO, &inventoryItemIndex);
            if (ammo == NULL) {
                break;
            }

            if (weapon->data.item.weapon.ammoTypePid == ammo->pid) {
                if (weaponCanBeReloadedWith(weapon, ammo) != 0) {
                    int rc = _item_w_reload(weapon, ammo);
                    if (rc == 0) {
                        _obj_destroy(ammo);
                    }

                    if (rc == -1) {
                        return -1;
                    }

                    return 0;
                }
            }
        }

        inventoryItemIndex = -1;
        for (;;) {
            Object* ammo = _inven_find_type(critter, ITEM_TYPE_AMMO, &inventoryItemIndex);
            if (ammo == NULL) {
                break;
            }

            if (weaponCanBeReloadedWith(weapon, ammo) != 0) {
                int rc = _item_w_reload(weapon, ammo);
                if (rc == 0) {
                    _obj_destroy(ammo);
                }

                if (rc == -1) {
                    return -1;
                }

                return 0;
            }
        }
    }

    if (_item_w_reload(weapon, NULL) != 0) {
        return -1;
    }

    return 0;
}

// Checks if weapon can be reloaded with the specified ammo.
//
// 0x478874
bool weaponCanBeReloadedWith(Object* weapon, Object* ammo)
{
    if (weapon->pid == PROTO_ID_SOLAR_SCORCHER) {
        // Check light level to recharge solar scorcher.
        if (lightGetLightLevel() > 62259) {
            return true;
        }

        // There is not enough light to recharge this item.
        MessageListItem messageListItem;
        char* msg = getmsg(&gItemsMessageList, &messageListItem, 500);
        displayMonitorAddMessage(msg);

        return false;
    }

    if (ammo == NULL) {
        return false;
    }

    Proto* weaponProto;
    protoGetProto(weapon->pid, &weaponProto);

    Proto* ammoProto;
    protoGetProto(ammo->pid, &ammoProto);

    if (weaponProto->item.type != ITEM_TYPE_WEAPON) {
        return false;
    }

    if (ammoProto->item.type != ITEM_TYPE_AMMO) {
        return false;
    }

    // Check ammo matches weapon caliber.
    if (weaponProto->item.data.weapon.caliber != ammoProto->item.data.ammo.caliber) {
        return false;
    }

    // If weapon is not empty, we should only reload it with the same ammo.
    if (ammoGetQuantity(weapon) != 0) {
        if (weapon->data.item.weapon.ammoTypePid != ammo->pid) {
            return false;
        }
    }

    return true;
}

// 0x478918
int _item_w_reload(Object* weapon, Object* ammo)
{
    if (!weaponCanBeReloadedWith(weapon, ammo)) {
        return -1;
    }

    // NOTE: Uninline.
    int ammoQuantity = ammoGetQuantity(weapon);

    // NOTE: Uninline.
    int ammoCapacity = ammoGetCapacity(weapon);

    if (weapon->pid == PROTO_ID_SOLAR_SCORCHER) {
        ammoSetQuantity(weapon, ammoCapacity);
        return 0;
    }

    // NOTE: Uninline.
    int v10 = ammoGetQuantity(ammo);

    int v11 = v10;
    if (ammoQuantity < ammoCapacity) {
        int v12;
        if (ammoQuantity + v10 > ammoCapacity) {
            v11 = v10 - (ammoCapacity - ammoQuantity);
            v12 = ammoCapacity;
        } else {
            v11 = 0;
            v12 = ammoQuantity + v10;
        }

        weapon->data.item.weapon.ammoTypePid = ammo->pid;

        ammoSetQuantity(ammo, v11);
        ammoSetQuantity(weapon, v12);
    }

    return v11;
}

// 0x478A1C
int _item_w_range(Object* critter, int hitMode)
{
    int range;
    int v12;

    // NOTE: Uninline.
    Object* weapon = critterGetWeaponForHitMode(critter, hitMode);

    if (weapon != NULL && hitMode != 4 && hitMode != 5 && (hitMode < 8 || hitMode > 19)) {
        Proto* proto;
        protoGetProto(weapon->pid, &proto);
        if (hitMode == HIT_MODE_LEFT_WEAPON_PRIMARY || hitMode == HIT_MODE_RIGHT_WEAPON_PRIMARY) {
            range = proto->item.data.weapon.maxRange1;
        } else {
            range = proto->item.data.weapon.maxRange2;
        }

        if (weaponGetAttackTypeForHitMode(weapon, hitMode) == ATTACK_TYPE_THROW) {
            if (critter == gDude) {
                v12 = critterGetStat(critter, STAT_STRENGTH) + 2 * perkGetRank(critter, PERK_HEAVE_HO);
            } else {
                v12 = critterGetStat(critter, STAT_STRENGTH);
            }

            int maxRange = 3 * v12;
            if (range >= maxRange) {
                range = maxRange;
            }
        }

        return range;
    }

    if (_critter_flag_check(critter->pid, CRITTER_FLAG_0x2000)) {
        return 2;
    }

    return 1;
}

// Returns action points required for hit mode.
//
// 0x478B24
int _item_w_mp_cost(Object* critter, int hitMode, bool aiming)
{
    int actionPoints;

    // NOTE: Uninline.
    Object* weapon = critterGetWeaponForHitMode(critter, hitMode);

    if (hitMode == HIT_MODE_LEFT_WEAPON_RELOAD || hitMode == HIT_MODE_RIGHT_WEAPON_RELOAD) {
        if (weapon != NULL) {
            Proto* proto;
            protoGetProto(weapon->pid, &proto);
            if (proto->item.data.weapon.perk == PERK_WEAPON_FAST_RELOAD) {
                return 1;
            }

            if (weapon->pid == PROTO_ID_SOLAR_SCORCHER) {
                return 0;
            }
        }
        return 2;
    }

    switch (hitMode) {
    case HIT_MODE_PALM_STRIKE:
        actionPoints = 6;
        break;
    case HIT_MODE_PIERCING_STRIKE:
        actionPoints = 8;
        break;
    case HIT_MODE_STRONG_KICK:
    case HIT_MODE_SNAP_KICK:
    case HIT_MODE_POWER_KICK:
        actionPoints = 4;
        break;
    case HIT_MODE_HIP_KICK:
    case HIT_MODE_HOOK_KICK:
        actionPoints = 7;
        break;
    case HIT_MODE_PIERCING_KICK:
        actionPoints = 9;
        break;
    default:
        // TODO: Inverse conditions.
        if (weapon != NULL && hitMode != HIT_MODE_PUNCH && hitMode != HIT_MODE_KICK && hitMode != HIT_MODE_STRONG_PUNCH && hitMode != HIT_MODE_HAMMER_PUNCH && hitMode != HIT_MODE_HAYMAKER) {
            if (hitMode == HIT_MODE_LEFT_WEAPON_PRIMARY || hitMode == HIT_MODE_RIGHT_WEAPON_PRIMARY) {
                // NOTE: Uninline.
                actionPoints = weaponGetActionPointCost1(weapon);
            } else {
                // NOTE: Uninline.
                actionPoints = weaponGetActionPointCost2(weapon);
            }

            if (critter == gDude) {
                if (traitIsSelected(TRAIT_FAST_SHOT)) {
                    if (_item_w_range(critter, hitMode) > 2) {
                        actionPoints--;
                    }
                }
            }
        } else {
            actionPoints = 3;
        }
        break;
    }

    if (critter == gDude) {
        int attackType = weaponGetAttackTypeForHitMode(weapon, hitMode);

        if (perkHasRank(gDude, PERK_BONUS_HTH_ATTACKS)) {
            if (attackType == ATTACK_TYPE_MELEE || attackType == ATTACK_TYPE_UNARMED) {
                actionPoints -= 1;
            }
        }

        if (perkHasRank(gDude, PERK_BONUS_RATE_OF_FIRE)) {
            if (attackType == ATTACK_TYPE_RANGED) {
                actionPoints -= 1;
            }
        }
    }

    if (aiming) {
        actionPoints += 1;
    }

    if (actionPoints < 1) {
        actionPoints = 1;
    }

    return actionPoints;
}

// 0x478D08
int weaponGetMinStrengthRequired(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    return proto->item.data.weapon.minStrength;
}

// 0x478D30
int weaponGetCriticalFailureType(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    return proto->item.data.weapon.criticalFailureType;
}

// 0x478D58
int weaponGetPerk(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    return proto->item.data.weapon.perk;
}

// 0x478D80
int weaponGetBurstRounds(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    return proto->item.data.weapon.rounds;
}

// 0x478DA8
int weaponGetAnimationCode(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    return proto->item.data.weapon.animationCode;
}

// 0x478DD0
int weaponGetProjectilePid(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    return proto->item.data.weapon.projectilePid;
}

// 0x478DF8
int weaponGetAmmoTypePid(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    if (itemGetType(weapon) != ITEM_TYPE_WEAPON) {
        return -1;
    }

    return weapon->data.item.weapon.ammoTypePid;
}

// 0x478E18
char weaponGetSoundId(Object* weapon)
{
    if (weapon == NULL) {
        return '\0';
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    return proto->item.data.weapon.soundCode & 0xFF;
}

// 0x478E5C
int _item_w_called_shot(Object* critter, int hitMode)
{
    if (critter == gDude && traitIsSelected(TRAIT_FAST_SHOT)) {
        return 0;
    }

    // NOTE: Uninline.
    int anim = critterGetAnimationForHitMode(critter, hitMode);
    if (anim == ANIM_FIRE_BURST || anim == ANIM_FIRE_CONTINUOUS) {
        return 0;
    }

    // NOTE: Uninline.
    Object* weapon = critterGetWeaponForHitMode(critter, hitMode);
    int damageType = weaponGetDamageType(critter, weapon);

    return damageType != DAMAGE_TYPE_EXPLOSION
        && damageType != DAMAGE_TYPE_FIRE
        && damageType != DAMAGE_TYPE_EMP
        && (damageType != DAMAGE_TYPE_PLASMA || anim != ANIM_THROW_ANIM);
}

// 0x478EF4
int _item_w_can_unload(Object* weapon)
{
    if (weapon == NULL) {
        return false;
    }

    if (itemGetType(weapon) != ITEM_TYPE_WEAPON) {
        return false;
    }

    // NOTE: Uninline.
    int ammoCapacity = ammoGetCapacity(weapon);
    if (ammoCapacity <= 0) {
        return false;
    }

    // NOTE: Uninline.
    int ammoQuantity = ammoGetQuantity(weapon);
    if (ammoQuantity <= 0) {
        return false;
    }

    if (weapon->pid == PROTO_ID_SOLAR_SCORCHER) {
        return false;
    }

    if (weaponGetAmmoTypePid(weapon) == -1) {
        return false;
    }

    return true;
}

// 0x478F80
Object* _item_w_unload(Object* weapon)
{
    if (!_item_w_can_unload(weapon)) {
        return NULL;
    }

    // NOTE: Uninline.
    int ammoTypePid = weaponGetAmmoTypePid(weapon);
    if (ammoTypePid == -1) {
        return NULL;
    }

    Object* ammo;
    if (objectCreateWithPid(&ammo, ammoTypePid) != 0) {
        return NULL;
    }

    _obj_disconnect(ammo, NULL);

    // NOTE: Uninline.
    int ammoQuantity = ammoGetQuantity(weapon);

    // NOTE: Uninline.
    int ammoCapacity = ammoGetCapacity(ammo);

    int remainingQuantity;
    if (ammoQuantity <= ammoCapacity) {
        ammoSetQuantity(ammo, ammoQuantity);
        remainingQuantity = 0;
    } else {
        ammoSetQuantity(ammo, ammoCapacity);
        remainingQuantity = ammoQuantity - ammoCapacity;
    }
    ammoSetQuantity(weapon, remainingQuantity);

    return ammo;
}

// 0x47905C
int weaponGetActionPointCost1(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    return proto->item.data.weapon.actionPointCost1;
}

// NOTE: Inlined.
//
// 0x479084
int weaponGetActionPointCost2(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(weapon->pid, &proto);

    return proto->item.data.weapon.actionPointCost2;
}

// 0x4790AC
int _item_w_compute_ammo_cost(Object* obj, int* inout_a2)
{
    int pid;

    if (inout_a2 == NULL) {
        return -1;
    }

    if (obj == NULL) {
        return 0;
    }

    pid = obj->pid;
    if (pid == PROTO_ID_SUPER_CATTLE_PROD || pid == PROTO_ID_MEGA_POWER_FIST) {
        *inout_a2 *= 2;
    }

    return 0;
}

// 0x4790E8
bool weaponIsGrenade(Object* weapon)
{
    int damageType = weaponGetDamageType(NULL, weapon);
    return damageType == DAMAGE_TYPE_EXPLOSION || damageType == DAMAGE_TYPE_PLASMA || damageType == DAMAGE_TYPE_EMP;
}

// 0x47910C
int _item_w_area_damage_radius(Object* weapon, int hitMode)
{
    int attackType = weaponGetAttackTypeForHitMode(weapon, hitMode);
    int anim = weaponGetAnimationForHitMode(weapon, hitMode);
    int damageType = weaponGetDamageType(NULL, weapon);

    int v1 = 0;
    if (attackType == ATTACK_TYPE_RANGED) {
        if (anim == ANIM_FIRE_SINGLE && damageType == DAMAGE_TYPE_EXPLOSION) {
            // NOTE: Uninline.
            v1 = _item_w_rocket_dmg_radius(weapon);
        }
    } else if (attackType == ATTACK_TYPE_THROW) {
        // NOTE: Uninline.
        if (weaponIsGrenade(weapon)) {
            // NOTE: Uninline.
            v1 = _item_w_grenade_dmg_radius(weapon);
        }
    }
    return v1;
}

// 0x479180
int _item_w_grenade_dmg_radius(Object* weapon)
{
    return 2;
}

// 0x479188
int _item_w_rocket_dmg_radius(Object* weapon)
{
    return 3;
}

// 0x479190
int weaponGetAmmoArmorClassModifier(Object* weapon)
{
    // NOTE: Uninline.
    int ammoTypePid = weaponGetAmmoTypePid(weapon);
    if (ammoTypePid == -1) {
        return 0;
    }

    Proto* proto;
    if (protoGetProto(ammoTypePid, &proto) == -1) {
        return 0;
    }

    return proto->item.data.ammo.armorClassModifier;
}

// 0x4791E0
int weaponGetAmmoDamageResistanceModifier(Object* weapon)
{
    // NOTE: Uninline.
    int ammoTypePid = weaponGetAmmoTypePid(weapon);
    if (ammoTypePid == -1) {
        return 0;
    }

    Proto* proto;
    if (protoGetProto(ammoTypePid, &proto) == -1) {
        return 0;
    }

    return proto->item.data.ammo.damageResistanceModifier;
}

// 0x479230
int weaponGetAmmoDamageMultiplier(Object* weapon)
{
    // NOTE: Uninline.
    int ammoTypePid = weaponGetAmmoTypePid(weapon);
    if (ammoTypePid == -1) {
        return 1;
    }

    Proto* proto;
    if (protoGetProto(ammoTypePid, &proto) == -1) {
        return 1;
    }

    return proto->item.data.ammo.damageMultiplier;
}

// 0x479294
int weaponGetAmmoDamageDivisor(Object* weapon)
{
    // NOTE: Uninline.
    int ammoTypePid = weaponGetAmmoTypePid(weapon);
    if (ammoTypePid == -1) {
        return 1;
    }

    Proto* proto;
    if (protoGetProto(ammoTypePid, &proto) == -1) {
        return 1;
    }

    return proto->item.data.ammo.damageDivisor;
}

// 0x4792F8
int armorGetArmorClass(Object* armor)
{
    if (armor == NULL) {
        return 0;
    }

    Proto* proto;
    protoGetProto(armor->pid, &proto);

    return proto->item.data.armor.armorClass;
}

// 0x479318
int armorGetDamageResistance(Object* armor, int damageType)
{
    if (armor == NULL) {
        return 0;
    }

    Proto* proto;
    protoGetProto(armor->pid, &proto);

    return proto->item.data.armor.damageResistance[damageType];
}

// 0x479338
int armorGetDamageThreshold(Object* armor, int damageType)
{
    if (armor == NULL) {
        return 0;
    }

    Proto* proto;
    protoGetProto(armor->pid, &proto);

    return proto->item.data.armor.damageThreshold[damageType];
}

// 0x479358
int armorGetPerk(Object* armor)
{
    if (armor == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(armor->pid, &proto);

    return proto->item.data.armor.perk;
}

// 0x479380
int armorGetMaleFid(Object* armor)
{
    if (armor == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(armor->pid, &proto);

    return proto->item.data.armor.maleFid;
}

// 0x4793A8
int armorGetFemaleFid(Object* armor)
{
    if (armor == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(armor->pid, &proto);

    return proto->item.data.armor.femaleFid;
}

// 0x4793D0
int miscItemGetMaxCharges(Object* miscItem)
{
    if (miscItem == NULL) {
        return 0;
    }

    Proto* proto;
    protoGetProto(miscItem->pid, &proto);

    return proto->item.data.misc.charges;
}

// 0x4793F0
int miscItemGetCharges(Object* miscItem)
{
    if (miscItem == NULL) {
        return 0;
    }

    return miscItem->data.item.misc.charges;
}

// 0x4793F8
int miscItemSetCharges(Object* miscItem, int charges)
{
    // NOTE: Uninline.
    int maxCharges = miscItemGetMaxCharges(miscItem);

    if (charges > maxCharges) {
        charges = maxCharges;
    }

    miscItem->data.item.misc.charges = charges;

    return 0;
}

// NOTE: Unused.
//
// 0x479434
int miscItemGetPowerType(Object* miscItem)
{
    if (miscItem == NULL) {
        return 0;
    }

    Proto* proto;
    protoGetProto(miscItem->pid, &proto);

    return proto->item.data.misc.powerType;
}

// NOTE: Inlined.
//
// 0x479454
int miscItemGetPowerTypePid(Object* miscItem)
{
    if (miscItem == NULL) {
        return -1;
    }

    Proto* proto;
    protoGetProto(miscItem->pid, &proto);

    return proto->item.data.misc.powerTypePid;
}

// 0x47947C
bool miscItemIsConsumable(Object* miscItem)
{
    if (miscItem == NULL) {
        return false;
    }

    Proto* proto;
    protoGetProto(miscItem->pid, &proto);

    return proto->item.data.misc.charges != 0;
}

// 0x4794A4
int _item_m_use_charged_item(Object* critter, Object* miscItem)
{
    int pid = miscItem->pid;
    if (pid == PROTO_ID_STEALTH_BOY_I
        || pid == PROTO_ID_GEIGER_COUNTER_I
        || pid == PROTO_ID_STEALTH_BOY_II
        || pid == PROTO_ID_GEIGER_COUNTER_II) {
        // NOTE: Uninline.
        bool isOn = miscItemIsOn(miscItem);

        if (isOn) {
            miscItemTurnOff(miscItem);
        } else {
            miscItemTurnOn(miscItem);
        }
    } else if (pid == PROTO_ID_MOTION_SENSOR) {
        // NOTE: Uninline.
        if (miscItemConsumeCharge(miscItem) == 0) {
            automapShow(true, true);
        } else {
            MessageListItem messageListItem;
            // %s has no charges left.
            messageListItem.num = 5;
            if (messageListGetItem(&gItemsMessageList, &messageListItem)) {
                char text[80];
                const char* itemName = objectGetName(miscItem);
                sprintf(text, messageListItem.text, itemName);
                displayMonitorAddMessage(text);
            }
        }
    }

    return 0;
}

// 0x4795A4
int miscItemConsumeCharge(Object* item)
{
    // NOTE: Uninline.
    int charges = miscItemGetCharges(item);
    if (charges <= 0) {
        return -1;
    }

    // NOTE: Uninline.
    miscItemSetCharges(item, charges - 1);

    return 0;
}

// 0x4795F0
int miscItemTrickleEventProcess(Object* item, void* data)
{
    // NOTE: Uninline.
    if (miscItemConsumeCharge(item) == 0) {
        int delay;
        if (item->pid == PROTO_ID_STEALTH_BOY_I || item->pid == PROTO_ID_STEALTH_BOY_II) {
            delay = 600;
        } else {
            delay = 3000;
        }

        queueAddEvent(delay, item, NULL, EVENT_TYPE_ITEM_TRICKLE);
    } else {
        Object* critter = objectGetOwner(item);
        if (critter == gDude) {
            MessageListItem messageListItem;
            // %s has no charges left.
            messageListItem.num = 5;
            if (messageListGetItem(&gItemsMessageList, &messageListItem)) {
                char text[80];
                const char* itemName = objectGetName(item);
                sprintf(text, messageListItem.text, itemName);
                displayMonitorAddMessage(text);
            }
        }
        miscItemTurnOff(item);
    }

    return 0;
}

// 0x4796A8
bool miscItemIsOn(Object* obj)
{
    if (obj == NULL) {
        return false;
    }

    if (!miscItemIsConsumable(obj)) {
        return false;
    }

    return queueHasEvent(obj, EVENT_TYPE_ITEM_TRICKLE);
}

// Turns on geiger counter or stealth boy.
//
// 0x4796D0
int miscItemTurnOn(Object* item)
{
    MessageListItem messageListItem;
    char text[80];

    Object* critter = objectGetOwner(item);
    if (critter == NULL) {
        // This item can only be used from the interface bar.
        messageListItem.num = 9;
        if (messageListGetItem(&gItemsMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }

        return -1;
    }

    // NOTE: Uninline.
    if (miscItemConsumeCharge(item) != 0) {
        if (critter == gDude) {
            messageListItem.num = 5;
            if (messageListGetItem(&gItemsMessageList, &messageListItem)) {
                char* name = objectGetName(item);
                sprintf(text, messageListItem.text, name);
                displayMonitorAddMessage(text);
            }
        }

        return -1;
    }

    if (item->pid == PROTO_ID_STEALTH_BOY_I || item->pid == PROTO_ID_STEALTH_BOY_II) {
        queueAddEvent(600, item, 0, EVENT_TYPE_ITEM_TRICKLE);
        item->pid = PROTO_ID_STEALTH_BOY_II;

        if (critter != NULL) {
            // NOTE: Uninline.
            stealthBoyTurnOn(critter);
        }
    } else {
        queueAddEvent(3000, item, 0, EVENT_TYPE_ITEM_TRICKLE);
        item->pid = PROTO_ID_GEIGER_COUNTER_II;
    }

    if (critter == gDude) {
        // %s is on.
        messageListItem.num = 6;
        if (messageListGetItem(&gItemsMessageList, &messageListItem)) {
            char* name = objectGetName(item);
            sprintf(text, messageListItem.text, name);
            displayMonitorAddMessage(text);
        }

        if (item->pid == PROTO_ID_GEIGER_COUNTER_II) {
            // You pass the Geiger counter over you body. The rem counter reads: %d
            messageListItem.num = 8;
            if (messageListGetItem(&gItemsMessageList, &messageListItem)) {
                int radiation = critterGetRadiation(critter);
                sprintf(text, messageListItem.text, radiation);
                displayMonitorAddMessage(text);
            }
        }
    }

    return 0;
}

// Turns off geiger counter or stealth boy.
//
// 0x479898
int miscItemTurnOff(Object* item)
{
    Object* owner = objectGetOwner(item);

    queueRemoveEventsByType(item, EVENT_TYPE_ITEM_TRICKLE);

    if (owner != NULL && item->pid == PROTO_ID_STEALTH_BOY_II) {
        stealthBoyTurnOff(owner, item);
    }

    if (item->pid == PROTO_ID_STEALTH_BOY_I || item->pid == PROTO_ID_STEALTH_BOY_II) {
        item->pid = PROTO_ID_STEALTH_BOY_I;
    } else {
        item->pid = PROTO_ID_GEIGER_COUNTER_I;
    }

    if (owner == gDude) {
        interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }

    if (owner == gDude) {
        // %s is off.
        MessageListItem messageListItem;
        messageListItem.num = 7;
        if (messageListGetItem(&gItemsMessageList, &messageListItem)) {
            const char* name = objectGetName(item);
            char text[80];
            sprintf(text, messageListItem.text, name);
            displayMonitorAddMessage(text);
        }
    }

    return 0;
}

// 0x479954
int _item_m_turn_off_from_queue(Object* obj, void* data)
{
    miscItemTurnOff(obj);
    return 1;
}

// NOTE: Inlined.
//
// 0x479960
static int stealthBoyTurnOn(Object* object)
{
    if ((object->flags & OBJECT_TRANS_GLASS) != 0) {
        return -1;
    }

    object->flags |= OBJECT_TRANS_GLASS;

    Rect rect;
    objectGetRect(object, &rect);
    tileWindowRefreshRect(&rect, object->elevation);

    return 0;
}

// 0x479998
static int stealthBoyTurnOff(Object* critter, Object* item)
{
    Object* item1 = critterGetItem1(critter);
    if (item1 != NULL && item1 != item && item1->pid == PROTO_ID_STEALTH_BOY_II) {
        return -1;
    }

    Object* item2 = critterGetItem2(critter);
    if (item2 != NULL && item2 != item && item2->pid == PROTO_ID_STEALTH_BOY_II) {
        return -1;
    }

    if ((critter->flags & OBJECT_TRANS_GLASS) == 0) {
        return -1;
    }

    critter->flags &= ~OBJECT_TRANS_GLASS;

    Rect rect;
    objectGetRect(critter, &rect);
    tileWindowRefreshRect(&rect, critter->elevation);

    return 0;
}

// 0x479A00
int containerGetMaxSize(Object* container)
{
    if (container == NULL) {
        return 0;
    }

    Proto* proto;
    protoGetProto(container->pid, &proto);

    return proto->item.data.container.maxSize;
}

// 0x479A20
int containerGetTotalSize(Object* container)
{
    if (container == NULL) {
        return 0;
    }

    int totalSize = 0;

    Inventory* inventory = &(container->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);

        int size = itemGetSize(inventoryItem->item);
        totalSize += inventory->items[index].quantity * size;
    }

    return totalSize;
}

// 0x479A74
int ammoGetArmorClassModifier(Object* armor)
{
    if (armor == NULL) {
        return 0;
    }

    Proto* proto;
    if (protoGetProto(armor->pid, &proto) == -1) {
        return 0;
    }

    return proto->item.data.ammo.armorClassModifier;
}

// 0x479AA4
int ammoGetDamageResistanceModifier(Object* armor)
{
    if (armor == NULL) {
        return 0;
    }

    Proto* proto;
    if (protoGetProto(armor->pid, &proto) == -1) {
        return 0;
    }

    return proto->item.data.ammo.damageResistanceModifier;
}

// 0x479AD4
int ammoGetDamageMultiplier(Object* armor)
{
    if (armor == NULL) {
        return 0;
    }

    Proto* proto;
    if (protoGetProto(armor->pid, &proto) == -1) {
        return 0;
    }

    return proto->item.data.ammo.damageMultiplier;
}

// 0x479B04
int ammoGetDamageDivisor(Object* armor)
{
    if (armor == NULL) {
        return 0;
    }

    Proto* proto;
    if (protoGetProto(armor->pid, &proto) == -1) {
        return 0;
    }

    return proto->item.data.ammo.damageDivisor;
}

// 0x479B44
static int _insert_drug_effect(Object* critter, Object* item, int a3, int* stats, int* mods)
{
    int index;
    for (index = 0; index < 3; index++) {
        if (mods[index] != 0) {
            break;
        }
    }

    if (index == 3) {
        return -1;
    }

    DrugEffectEvent* drugEffectEvent = (DrugEffectEvent*)internal_malloc(sizeof(*drugEffectEvent));
    if (drugEffectEvent == NULL) {
        return -1;
    }

    drugEffectEvent->drugPid = item->pid;

    for (index = 0; index < 3; index++) {
        drugEffectEvent->stats[index] = stats[index];
        drugEffectEvent->modifiers[index] = mods[index];
    }

    int delay = 600 * a3;
    if (critter == gDude) {
        if (traitIsSelected(TRAIT_CHEM_RESISTANT)) {
            delay /= 2;
        }
    }

    if (queueAddEvent(delay, critter, drugEffectEvent, EVENT_TYPE_DRUG) == -1) {
        internal_free(drugEffectEvent);
        return -1;
    }

    return 0;
}

// 0x479C20
static void _perform_drug_effect(Object* critter, int* stats, int* mods, bool isImmediate)
{
    int v10;
    int v11;
    int v12;
    MessageListItem messageListItem;
    const char* name;
    const char* text;
    char v24[92]; // TODO: Size is probably wrong.
    char str[92]; // TODO: Size is probably wrong.

    bool statsChanged = false;

    int v5 = 0;
    bool v32 = false;
    if (stats[0] == -2) {
        v5 = 1;
        v32 = true;
    }

    for (int index = v5; index < 3; index++) {
        int stat = stats[index];
        if (stat == -1) {
            continue;
        }

        if (stat == STAT_CURRENT_HIT_POINTS) {
            critter->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;
        }

        v10 = critterGetBonusStat(critter, stat);

        int before;
        if (critter == gDude) {
            before = critterGetStat(gDude, stat);
        }

        if (v32) {
            v11 = randomBetween(mods[index - 1], mods[index]) + v10;
            v32 = false;
        } else {
            v11 = mods[index] + v10;
        }

        if (stat == STAT_CURRENT_HIT_POINTS) {
            v12 = critterGetBaseStatWithTraitModifier(critter, STAT_CURRENT_HIT_POINTS);
            if (v11 + v12 <= 0 && critter != gDude) {
                name = critterGetName(critter);
                // %s succumbs to the adverse effects of chems.
                text = getmsg(&gItemsMessageList, &messageListItem, 600);
                sprintf(v24, text, name);
                _combatKillCritterOutsideCombat(critter, v24);
            }
        }

        critterSetBonusStat(critter, stat, v11);

        if (critter == gDude) {
            if (stat == STAT_CURRENT_HIT_POINTS) {
                interfaceRenderHitPoints(true);
            }

            int after = critterGetStat(critter, stat);
            if (after != before) {
                // 1 - You gained %d %s.
                // 2 - You lost %d %s.
                messageListItem.num = after < before ? 2 : 1;
                if (messageListGetItem(&gItemsMessageList, &messageListItem)) {
                    char* statName = statGetName(stat);
                    sprintf(str, messageListItem.text, after < before ? before - after : after - before, statName);
                    displayMonitorAddMessage(str);
                    statsChanged = true;
                }
            }
        }
    }

    if (critterGetStat(critter, STAT_CURRENT_HIT_POINTS) > 0) {
        if (critter == gDude && !statsChanged && isImmediate) {
            // Nothing happens.
            messageListItem.num = 10;
            if (messageListGetItem(&gItemsMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
        }
    } else {
        if (critter == gDude) {
            // You suffer a fatal heart attack from chem overdose.
            messageListItem.num = 4;
            if (messageListGetItem(&gItemsMessageList, &messageListItem)) {
                strcpy(v24, messageListItem.text);
                // TODO: Why message is ignored?
            }
        } else {
            name = critterGetName(critter);
            // %s succumbs to the adverse effects of chems.
            text = getmsg(&gItemsMessageList, &messageListItem, 600);
            sprintf(v24, text, name);
            // TODO: Why message is ignored?
        }
    }
}

// 0x479EE4
static bool _drug_effect_allowed(Object* critter, int pid)
{
    int index;
    DrugDescription* drugDescription;
    for (index = 0; index < ADDICTION_COUNT; index++) {
        drugDescription = &(gDrugDescriptions[index]);
        if (drugDescription->drugPid == pid) {
            break;
        }
    }

    if (index == ADDICTION_COUNT) {
        return true;
    }

    if (drugDescription->field_8 == 0) {
        return true;
    }

    // TODO: Probably right, but let's check it once.
    int count = 0;
    DrugEffectEvent* drugEffectEvent = (DrugEffectEvent*)queueFindFirstEvent(critter, EVENT_TYPE_DRUG);
    while (drugEffectEvent != NULL) {
        if (drugEffectEvent->drugPid == pid) {
            count++;
            if (count >= drugDescription->field_8) {
                return false;
            }
        }
        drugEffectEvent = (DrugEffectEvent*)queueFindNextEvent(critter, EVENT_TYPE_DRUG);
    }

    return true;
}

// 0x479F60
int _item_d_take_drug(Object* critter, Object* item)
{
    if (critterIsDead(critter)) {
        return -1;
    }

    if (critterGetBodyType(critter) == BODY_TYPE_ROBOTIC) {
        return -1;
    }

    Proto* proto;
    protoGetProto(item->pid, &proto);

    if (item->pid == PROTO_ID_JET_ANTIDOTE) {
        if (dudeIsAddicted(PROTO_ID_JET)) {
            performWithdrawalEnd(critter, PERK_JET_ADDICTION);

            if (critter == gDude) {
                // NOTE: Uninline.
                dudeClearAddiction(PROTO_ID_JET);
            }

            return 0;
        }
    }

    _wd_obj = critter;
    _wd_gvar = drugGetAddictionGvarByPid(item->pid);
    _wd_onset = proto->item.data.drug.withdrawalOnset;

    _queue_clear_type(EVENT_TYPE_WITHDRAWAL, _item_wd_clear_all);

    if (_drug_effect_allowed(critter, item->pid)) {
        _perform_drug_effect(critter, proto->item.data.drug.stat, proto->item.data.drug.amount, true);
        _insert_drug_effect(critter, item, proto->item.data.drug.duration1, proto->item.data.drug.stat, proto->item.data.drug.amount1);
        _insert_drug_effect(critter, item, proto->item.data.drug.duration2, proto->item.data.drug.stat, proto->item.data.drug.amount2);
    } else {
        if (critter == gDude) {
            MessageListItem messageListItem;
            // That didn't seem to do that much.
            char* msg = getmsg(&gItemsMessageList, &messageListItem, 50);
            displayMonitorAddMessage(msg);
        }
    }

    if (!dudeIsAddicted(item->pid)) {
        int addictionChance = proto->item.data.drug.addictionChance;
        if (critter == gDude) {
            if (traitIsSelected(TRAIT_CHEM_RELIANT)) {
                addictionChance *= 2;
            }

            if (traitIsSelected(TRAIT_CHEM_RESISTANT)) {
                addictionChance /= 2;
            }

            if (perkGetRank(gDude, PERK_FLOWER_CHILD)) {
                addictionChance /= 2;
            }
        }

        if (randomBetween(1, 100) <= addictionChance) {
            _insert_withdrawal(critter, 1, proto->item.data.drug.withdrawalOnset, proto->item.data.drug.withdrawalEffect, item->pid);

            if (critter == gDude) {
                // NOTE: Uninline.
                dudeSetAddiction(item->pid);
            }
        }
    }

    return 1;
}

// 0x47A178
int _item_d_clear(Object* obj, void* data)
{
    if (objectIsPartyMember(obj)) {
        return 0;
    }

    drugEffectEventProcess(obj, data);

    return 1;
}

// 0x47A198
int drugEffectEventProcess(Object* obj, void* data)
{
    DrugEffectEvent* drugEffectEvent = (DrugEffectEvent*)data;

    if (obj == NULL) {
        return 0;
    }

    if (PID_TYPE(obj->pid) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    _perform_drug_effect(obj, drugEffectEvent->stats, drugEffectEvent->modifiers, false);

    if (!(obj->data.critter.combat.results & DAM_DEAD)) {
        return 0;
    }

    return 1;
}

// 0x47A1D0
int drugEffectEventRead(File* stream, void** dataPtr)
{
    DrugEffectEvent* drugEffectEvent = (DrugEffectEvent*)internal_malloc(sizeof(*drugEffectEvent));
    if (drugEffectEvent == NULL) {
        return -1;
    }

    if (fileReadInt32List(stream, drugEffectEvent->stats, 3) == -1) goto err;
    if (fileReadInt32List(stream, drugEffectEvent->modifiers, 3) == -1) goto err;

    *dataPtr = drugEffectEvent;
    return 0;

err:

    internal_free(drugEffectEvent);
    return -1;
}

// 0x47A254
int drugEffectEventWrite(File* stream, void* data)
{
    DrugEffectEvent* drugEffectEvent = (DrugEffectEvent*)data;

    if (fileWriteInt32List(stream, drugEffectEvent->stats, 3) == -1) return -1;
    if (fileWriteInt32List(stream, drugEffectEvent->modifiers, 3) == -1) return -1;

    return 0;
}

// 0x47A290
static int _insert_withdrawal(Object* obj, int a2, int duration, int perk, int pid)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)internal_malloc(sizeof(*withdrawalEvent));
    if (withdrawalEvent == NULL) {
        return -1;
    }

    withdrawalEvent->field_0 = a2;
    withdrawalEvent->pid = pid;
    withdrawalEvent->perk = perk;

    if (queueAddEvent(600 * duration, obj, withdrawalEvent, EVENT_TYPE_WITHDRAWAL) == -1) {
        internal_free(withdrawalEvent);
        return -1;
    }

    return 0;
}

// 0x47A2FC
int _item_wd_clear(Object* obj, void* data)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)data;

    if (objectIsPartyMember(obj)) {
        return 0;
    }

    if (!withdrawalEvent->field_0) {
        performWithdrawalEnd(obj, withdrawalEvent->perk);
    }

    return 1;
}

// 0x47A324
static int _item_wd_clear_all(Object* a1, void* data)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)data;

    if (a1 != _wd_obj) {
        return 0;
    }

    if (drugGetAddictionGvarByPid(withdrawalEvent->pid) != _wd_gvar) {
        return 0;
    }

    if (!withdrawalEvent->field_0) {
        performWithdrawalEnd(_wd_obj, withdrawalEvent->perk);
    }

    _insert_withdrawal(a1, 1, _wd_onset, withdrawalEvent->perk, withdrawalEvent->pid);

    _wd_obj = NULL;

    return 1;
}

// 0x47A384
int withdrawalEventProcess(Object* obj, void* data)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)data;

    if (withdrawalEvent->field_0) {
        performWithdrawalStart(obj, withdrawalEvent->perk, withdrawalEvent->pid);
    } else {
        if (withdrawalEvent->perk == PERK_JET_ADDICTION) {
            return 0;
        }

        performWithdrawalEnd(obj, withdrawalEvent->perk);

        if (obj == gDude) {
            // NOTE: Uninline.
            dudeClearAddiction(withdrawalEvent->pid);
        }
    }

    if (obj == gDude) {
        return 1;
    }

    return 0;
}

// read withdrawal event
// 0x47A404
int withdrawalEventRead(File* stream, void** dataPtr)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)internal_malloc(sizeof(*withdrawalEvent));
    if (withdrawalEvent == NULL) {
        return -1;
    }

    if (fileReadInt32(stream, &(withdrawalEvent->field_0)) == -1) goto err;
    if (fileReadInt32(stream, &(withdrawalEvent->pid)) == -1) goto err;
    if (fileReadInt32(stream, &(withdrawalEvent->perk)) == -1) goto err;

    *dataPtr = withdrawalEvent;
    return 0;

err:

    internal_free(withdrawalEvent);
    return -1;
}

// 0x47A484
int withdrawalEventWrite(File* stream, void* data)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)data;

    if (fileWriteInt32(stream, withdrawalEvent->field_0) == -1) return -1;
    if (fileWriteInt32(stream, withdrawalEvent->pid) == -1) return -1;
    if (fileWriteInt32(stream, withdrawalEvent->perk) == -1) return -1;

    return 0;
}

// perform_withdrawal_start
// 0x47A4C4
static void performWithdrawalStart(Object* obj, int perk, int pid)
{
    if (PID_TYPE(obj->pid) != OBJ_TYPE_CRITTER) {
        debugPrint("\nERROR: perform_withdrawal_start: Was called on non-critter!");
        return;
    }

    perkAddEffect(obj, perk);

    if (obj == gDude) {
        char* description = perkGetDescription(perk);
        displayMonitorAddMessage(description);
    }

    int duration = 10080;
    if (obj == gDude) {
        if (traitIsSelected(TRAIT_CHEM_RELIANT)) {
            duration /= 2;
        }

        if (perkGetRank(obj, PERK_FLOWER_CHILD)) {
            duration /= 2;
        }
    }

    _insert_withdrawal(obj, 0, duration, perk, pid);
}

// perform_withdrawal_end
// 0x47A558
static void performWithdrawalEnd(Object* obj, int perk)
{
    if (PID_TYPE(obj->pid) != OBJ_TYPE_CRITTER) {
        debugPrint("\nERROR: perform_withdrawal_end: Was called on non-critter!");
        return;
    }

    perkRemoveEffect(obj, perk);

    if (obj == gDude) {
        MessageListItem messageListItem;
        messageListItem.num = 3;
        if (messageListGetItem(&gItemsMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
    }
}

// 0x47A5B4
static int drugGetAddictionGvarByPid(int drugPid)
{
    for (int index = 0; index < ADDICTION_COUNT; index++) {
        DrugDescription* drugDescription = &(gDrugDescriptions[index]);
        if (drugDescription->drugPid == drugPid) {
            return drugDescription->gvar;
        }
    }

    return -1;
}

// NOTE: Inlined.
//
// 0x47A5E8
static void dudeSetAddiction(int drugPid)
{
    int gvar = drugGetAddictionGvarByPid(drugPid);
    if (gvar != -1) {
        gGameGlobalVars[gvar] = 1;
    }

    dudeEnableState(DUDE_STATE_ADDICTED);
}

// NOTE: Inlined.
//
// 0x47A60C
static void dudeClearAddiction(int drugPid)
{
    int gvar = drugGetAddictionGvarByPid(drugPid);
    if (gvar != -1) {
        gGameGlobalVars[gvar] = 0;
    }

    if (!dudeIsAddicted(-1)) {
        dudeDisableState(DUDE_STATE_ADDICTED);
    }
}

// Returns `true` if dude has addiction to item with given pid or any addition
// if [pid] is -1.
//
// 0x47A640
static bool dudeIsAddicted(int drugPid)
{
    for (int index = 0; index < ADDICTION_COUNT; index++) {
        DrugDescription* drugDescription = &(gDrugDescriptions[index]);
        if (drugPid == -1 || drugPid == drugDescription->drugPid) {
            if (gGameGlobalVars[drugDescription->gvar] != 0) {
                return true;
            } else {
                return false;
            }
        }
    }

    return false;
}

// item_caps_total
// 0x47A6A8
int itemGetTotalCaps(Object* obj)
{
    int amount = 0;

    Inventory* inventory = &(obj->data.inventory);
    for (int i = 0; i < inventory->length; i++) {
        InventoryItem* inventoryItem = &(inventory->items[i]);
        Object* item = inventoryItem->item;

        if (item->pid == PROTO_ID_MONEY) {
            amount += inventoryItem->quantity;
        } else {
            if (itemGetType(item) == ITEM_TYPE_CONTAINER) {
                // recursively collect amount of caps in container
                amount += itemGetTotalCaps(item);
            }
        }
    }

    return amount;
}

// item_caps_adjust
// 0x47A6F8
int itemCapsAdjust(Object* obj, int amount)
{
    int caps = itemGetTotalCaps(obj);
    if (amount < 0 && caps < -amount) {
        return -1;
    }

    if (amount <= 0 || caps != 0) {
        Inventory* inventory = &(obj->data.inventory);

        for (int index = 0; index < inventory->length && amount != 0; index++) {
            InventoryItem* inventoryItem = &(inventory->items[index]);
            Object* item = inventoryItem->item;
            if (item->pid == PROTO_ID_MONEY) {
                if (amount <= 0 && -amount >= inventoryItem->quantity) {
                    objectDestroy(item, NULL);

                    amount += inventoryItem->quantity;

                    // NOTE: Uninline.
                    _item_compact(index, inventory);

                    index = -1;
                } else {
                    inventoryItem->quantity += amount;
                    amount = 0;
                }
            }
        }

        for (int index = 0; index < inventory->length && amount != 0; index++) {
            InventoryItem* inventoryItem = &(inventory->items[index]);
            Object* item = inventoryItem->item;
            if (itemGetType(item) == ITEM_TYPE_CONTAINER) {
                int capsInContainer = itemGetTotalCaps(item);
                if (amount <= 0 || capsInContainer <= 0) {
                    if (amount < 0) {
                        if (capsInContainer < -amount) {
                            if (itemCapsAdjust(item, capsInContainer) == 0) {
                                amount += capsInContainer;
                            }
                        } else {
                            if (itemCapsAdjust(item, amount) == 0) {
                                amount = 0;
                            }
                        }
                    }
                } else {
                    if (itemCapsAdjust(item, amount) == 0) {
                        amount = 0;
                    }
                }
            }
        }

        return 0;
    }

    Object* item;
    if (objectCreateWithPid(&item, PROTO_ID_MONEY) == 0) {
        _obj_disconnect(item, NULL);
        if (itemAdd(obj, item, amount) != 0) {
            objectDestroy(item, NULL);
            return -1;
        }
    }

    return 0;
}

// 0x47A8C8
int itemGetMoney(Object* item)
{
    if (item->pid != PROTO_ID_MONEY) {
        return -1;
    }

    return item->data.item.misc.charges;
}

// 0x47A8D8
int itemSetMoney(Object* item, int amount)
{
    if (item->pid != PROTO_ID_MONEY) {
        return -1;
    }

    item->data.item.misc.charges = amount;

    return 0;
}

static void booksInit()
{
    booksInitVanilla();
    booksInitCustom();
}

static void booksExit()
{
    gBooks.clear();
}

static void booksInitVanilla()
{
    // 802: You learn new science information.
    booksAdd(PROTO_ID_BIG_BOOK_OF_SCIENCE, 802, SKILL_SCIENCE);

    // 803: You learn a lot about repairing broken electronics.
    booksAdd(PROTO_ID_DEANS_ELECTRONICS, 803, SKILL_REPAIR);

    // 804: You learn new ways to heal injury.
    booksAdd(PROTO_ID_FIRST_AID_BOOK, 804, SKILL_FIRST_AID);

    // 805: You learn how to handle your guns better.
    booksAdd(PROTO_ID_GUNS_AND_BULLETS, 805, SKILL_SMALL_GUNS);

    // 806: You learn a lot about wilderness survival.
    booksAdd(PROTO_ID_SCOUT_HANDBOOK, 806, SKILL_OUTDOORSMAN);
}

static void booksInitCustom()
{
    char* booksFilePath;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BOOKS_FILE_KEY, &booksFilePath);
    if (booksFilePath != NULL && *booksFilePath == '\0') {
        booksFilePath = NULL;
    }

    if (booksFilePath != NULL) {
        Config booksConfig;
        if (configInit(&booksConfig)) {
            if (configRead(&booksConfig, booksFilePath, false)) {
                bool overrideVanilla = false;
                configGetBool(&booksConfig, "main", "overrideVanilla", &overrideVanilla);
                if (overrideVanilla) {
                    gBooks.clear();
                }

                int bookCount = 0;
                configGetInt(&booksConfig, "main", "count", &bookCount);
                if (bookCount > BOOKS_MAX) {
                    bookCount = BOOKS_MAX;
                }

                char sectionKey[4];
                for (int index = 0; index < bookCount; index++) {
                    // Books numbering starts with 1.
                    sprintf(sectionKey, "%d", index + 1);

                    int bookPid;
                    if (!configGetInt(&booksConfig, sectionKey, "PID", &bookPid)) continue;

                    int messageId;
                    if (!configGetInt(&booksConfig, sectionKey, "TextID", &messageId)) continue;

                    int skill;
                    if (!configGetInt(&booksConfig, sectionKey, "Skill", &skill)) continue;

                    booksAdd(bookPid, messageId, skill);
                }
            }

            configFree(&booksConfig);
        }
    }
}

static void booksAdd(int bookPid, int messageId, int skill)
{
    BookDescription bookDescription;
    bookDescription.bookPid = bookPid;
    bookDescription.messageId = messageId;
    bookDescription.skill = skill;
    gBooks.emplace_back(std::move(bookDescription));
}

bool booksGetInfo(int bookPid, int* messageIdPtr, int* skillPtr)
{
    for (auto& bookDescription : gBooks) {
        if (bookDescription.bookPid == bookPid) {
            *messageIdPtr = bookDescription.messageId;
            *skillPtr = bookDescription.skill;
            return true;
        }
    }
    return false;
}
