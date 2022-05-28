#ifndef INVENTORY_H
#define INVENTORY_H

#include "art.h"
#include "cache.h"
#include "message.h"
#include "obj_types.h"

#define INVENTORY_NORMAL_WINDOW_PC_ROTATION_DELAY (1000U / ROTATION_COUNT)
#define OFF_59E7BC_COUNT 12

typedef enum InventoryArrowFrm {
    INVENTORY_ARROW_FRM_LEFT_ARROW_UP,
    INVENTORY_ARROW_FRM_LEFT_ARROW_DOWN,
    INVENTORY_ARROW_FRM_RIGHT_ARROW_UP,
    INVENTORY_ARROW_FRM_RIGHT_ARROW_DOWN,
    INVENTORY_ARROW_FRM_COUNT,
} InventoryArrowFrm;

typedef enum InventoryWindowCursor {
    INVENTORY_WINDOW_CURSOR_HAND,
    INVENTORY_WINDOW_CURSOR_ARROW,
    INVENTORY_WINDOW_CURSOR_PICK,
    INVENTORY_WINDOW_CURSOR_MENU,
    INVENTORY_WINDOW_CURSOR_BLANK,
    INVENTORY_WINDOW_CURSOR_COUNT,
} InventoryWindowCursor;

typedef enum InventoryWindowType {
    // Normal inventory window with quick character sheet.
    INVENTORY_WINDOW_TYPE_NORMAL,

    // Narrow inventory window with just an item scroller that's shown when
    // a "Use item on" is selected from context menu.
    INVENTORY_WINDOW_TYPE_USE_ITEM_ON,

    // Looting/strealing interface.
    INVENTORY_WINDOW_TYPE_LOOT,

    // Barter interface.
    INVENTORY_WINDOW_TYPE_TRADE,

    // Supplementary "Move items" window. Used to set quantity of items when
    // moving items between inventories.
    INVENTORY_WINDOW_TYPE_MOVE_ITEMS,

    // Supplementary "Set timer" window. Internally it's implemented as "Move
    // items" window but with timer overlay and slightly different adjustment
    // mechanics.
    INVENTORY_WINDOW_TYPE_SET_TIMER,

    INVENTORY_WINDOW_TYPE_COUNT,
} InventoryWindowType;

typedef struct InventoryWindowConfiguration {
    int field_0; // artId
    int width;
    int height;
    int x;
    int y;
} InventoryWindowDescription;

typedef struct InventoryCursorData {
    Art* frm;
    unsigned char* frmData;
    int width;
    int height;
    int offsetX;
    int offsetY;
    CacheEntry* frmHandle;
} InventoryCursorData;

typedef void InventoryPrintItemDescriptionHandler(char* string);

extern const int dword_46E6D0[7];
extern const int dword_46E6EC[7];
extern const int gInventoryArrowFrmIds[INVENTORY_ARROW_FRM_COUNT];

extern int gInventorySlotsCount;
extern Object* _inven_dude;
extern int _inven_pid;
extern bool _inven_is_initialized;
extern int _inven_display_msg_line;
extern const InventoryWindowDescription gInventoryWindowDescriptions[INVENTORY_WINDOW_TYPE_COUNT];
extern bool _dropped_explosive;
extern int gInventoryScrollUpButton;
extern int gInventoryScrollDownButton;
extern int gSecondaryInventoryScrollUpButton;
extern int gSecondaryInventoryScrollDownButton;
extern unsigned int gInventoryWindowDudeRotationTimestamp;
extern int gInventoryWindowDudeRotation;
extern const int gInventoryWindowCursorFrmIds[INVENTORY_WINDOW_CURSOR_COUNT];
extern Object* _last_target;
extern const int _act_use[4];
extern const int _act_no_use[3];
extern const int _act_just_use[3];
extern const int _act_nothing[2];
extern const int _act_weap[4];
extern const int _act_weap2[3];

extern CacheEntry* _mt_key[8];
extern CacheEntry* _ikey[OFF_59E7BC_COUNT];
extern int _target_stack_offset[10];
extern MessageList gInventoryMessageList;
extern Object* _target_stack[10];
extern int _stack_offset[10];
extern Object* _stack[10];
extern int _mt_wid;
extern int _barter_mod;
extern int _btable_offset;
extern int _ptable_offset;
extern Inventory* _ptable_pud;
extern InventoryCursorData gInventoryCursorData[INVENTORY_WINDOW_CURSOR_COUNT];
extern Object* _ptable;
extern InventoryPrintItemDescriptionHandler* gInventoryPrintItemDescriptionHandler;
extern int _im_value;
extern int gInventoryCursor;
extern Object* _btable;
extern int _target_curr_stack;
extern Inventory* _btable_pud;
extern bool _inven_ui_was_disabled;
extern Object* gInventoryArmor;
extern Object* gInventoryLeftHandItem;
extern int gInventoryWindowDudeFid;
extern Inventory* _pud;
extern int gInventoryWindow;
extern Object* gInventoryRightHandItem;
extern int _curr_stack;
extern int gInventoryWindowMaxY;
extern int gInventoryWindowMaxX;
extern Inventory* _target_pud;
extern int _barter_back_win;

void _inven_reset_dude();
int inventoryMessageListInit();
int inventoryMessageListFree();
void inventoryOpen();
bool _setup_inventory(int inventoryWindowType);
void _exit_inventory(bool a1);
void _display_inventory(int a1, int a2, int inventoryWindowType);
void _display_target_inventory(int a1, int a2, Inventory* a3, int a4);
void _display_inventory_info(Object* item, int quantity, unsigned char* dest, int pitch, bool a5);
void _display_body(int fid, int inventoryWindowType);
int inventoryCommonInit();
void inventoryCommonFree();
void inventorySetCursor(int cursor);
void inventoryItemSlotOnMouseEnter(int btn, int keyCode);
void inventoryItemSlotOnMouseExit(int btn, int keyCode);
void _inven_update_lighting(Object* a1);
void _inven_pickup(int keyCode, int a2);
void _switch_hand(Object* a1, Object** a2, Object** a3, int a4);
void _adjust_ac(Object* critter, Object* oldArmor, Object* newArmor);
void _adjust_fid();
void inventoryOpenUseItemOn(Object* a1);
Object* critterGetItem2(Object* obj);
Object* critterGetItem1(Object* obj);
Object* critterGetArmor(Object* obj);
Object* objectGetCarriedObjectByPid(Object* obj, int pid);
int objectGetCarriedQuantityByPid(Object* obj, int pid);
void inventoryRenderSummary();
Object* _inven_find_type(Object* obj, int a2, int* inout_a3);
Object* _inven_find_id(Object* obj, int a2);
Object* _inven_index_ptr(Object* obj, int a2);
int _inven_wield(Object* a1, Object* a2, int a3);
int _invenWieldFunc(Object* a1, Object* a2, int a3, bool a4);
int _inven_unwield(Object* critter_obj, int a2);
int _invenUnwieldFunc(Object* obj, int a2, int a3);
int _inven_from_button(int a1, Object** a2, Object*** a3, Object** a4);
void inventoryRenderItemDescription(char* string);
void inventoryExamineItem(Object* critter, Object* item);
void inventoryWindowOpenContextMenu(int eventCode, int inventoryWindowType);
int inventoryOpenLooting(Object* a1, Object* a2);
int inventoryOpenStealing(Object* a1, Object* a2);
int _move_inventory(Object* a1, int a2, Object* a3, bool a4);
int _barter_compute_value(Object* a1, Object* a2);
int _barter_attempt_transaction(Object* a1, Object* a2, Object* a3, Object* a4);
void _barter_move_inventory(Object* a1, int quantity, int a3, int a4, Object* a5, Object* a6, bool a7);
void _barter_move_from_table_inventory(Object* a1, int quantity, int a3, Object* a4, Object* a5, bool a6);
void inventoryWindowRenderInnerInventories(int win, Object* a2, Object* a3, int a4);
void inventoryOpenTrade(int win, Object* a2, Object* a3, Object* a4, int a5);
void _container_enter(int a1, int a2);
void _container_exit(int keyCode, int inventoryWindowType);
int _drop_into_container(Object* a1, Object* a2, int a3, Object** a4, int quantity);
int _drop_ammo_into_weapon(Object* weapon, Object* ammo, Object** a3, int quantity, int keyCode);
void _draw_amount(int value, int inventoryWindowType);
int inventoryQuantitySelect(int inventoryWindowType, Object* item, int a3);
int inventoryQuantityWindowInit(int inventoryWindowType, Object* item);
int inventoryQuantityWindowFree(int inventoryWindowType);
int _inven_set_timer(Object* a1);

#endif /* INVENTORY_H */
