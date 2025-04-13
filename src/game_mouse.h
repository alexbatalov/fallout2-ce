#ifndef GAME_MOUSE_H
#define GAME_MOUSE_H

#include "obj_types.h"

namespace fallout {

typedef enum GameMouseMode {
    GAME_MOUSE_MODE_MOVE,
    GAME_MOUSE_MODE_ARROW,
    GAME_MOUSE_MODE_CROSSHAIR,
    GAME_MOUSE_MODE_USE_CROSSHAIR,
    GAME_MOUSE_MODE_USE_FIRST_AID,
    GAME_MOUSE_MODE_USE_DOCTOR,
    GAME_MOUSE_MODE_USE_LOCKPICK,
    GAME_MOUSE_MODE_USE_STEAL,
    GAME_MOUSE_MODE_USE_TRAPS,
    GAME_MOUSE_MODE_USE_SCIENCE,
    GAME_MOUSE_MODE_USE_REPAIR,
    GAME_MOUSE_MODE_COUNT,
    FIRST_GAME_MOUSE_MODE_SKILL = GAME_MOUSE_MODE_USE_FIRST_AID,
    GAME_MOUSE_MODE_SKILL_COUNT = GAME_MOUSE_MODE_COUNT - FIRST_GAME_MOUSE_MODE_SKILL,
} GameMouseMode;

typedef enum GameMouseActionMenuItem {
    GAME_MOUSE_ACTION_MENU_ITEM_CANCEL = 0,
    GAME_MOUSE_ACTION_MENU_ITEM_DROP = 1,
    GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY = 2,
    GAME_MOUSE_ACTION_MENU_ITEM_LOOK = 3,
    GAME_MOUSE_ACTION_MENU_ITEM_ROTATE = 4,
    GAME_MOUSE_ACTION_MENU_ITEM_TALK = 5,
    GAME_MOUSE_ACTION_MENU_ITEM_USE = 6,
    GAME_MOUSE_ACTION_MENU_ITEM_UNLOAD = 7,
    GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL = 8,
    GAME_MOUSE_ACTION_MENU_ITEM_PUSH = 9,
    GAME_MOUSE_ACTION_MENU_ITEM_COUNT,
} GameMouseActionMenuItem;

typedef enum MouseCursorType {
    MOUSE_CURSOR_NONE,
    MOUSE_CURSOR_ARROW,
    MOUSE_CURSOR_SMALL_ARROW_UP,
    MOUSE_CURSOR_SMALL_ARROW_DOWN,
    MOUSE_CURSOR_SCROLL_NW,
    MOUSE_CURSOR_SCROLL_N,
    MOUSE_CURSOR_SCROLL_NE,
    MOUSE_CURSOR_SCROLL_E,
    MOUSE_CURSOR_SCROLL_SE,
    MOUSE_CURSOR_SCROLL_S,
    MOUSE_CURSOR_SCROLL_SW,
    MOUSE_CURSOR_SCROLL_W,
    MOUSE_CURSOR_SCROLL_NW_INVALID,
    MOUSE_CURSOR_SCROLL_N_INVALID,
    MOUSE_CURSOR_SCROLL_NE_INVALID,
    MOUSE_CURSOR_SCROLL_E_INVALID,
    MOUSE_CURSOR_SCROLL_SE_INVALID,
    MOUSE_CURSOR_SCROLL_S_INVALID,
    MOUSE_CURSOR_SCROLL_SW_INVALID,
    MOUSE_CURSOR_SCROLL_W_INVALID,
    MOUSE_CURSOR_CROSSHAIR,
    MOUSE_CURSOR_PLUS,
    MOUSE_CURSOR_DESTROY,
    MOUSE_CURSOR_USE_CROSSHAIR,
    MOUSE_CURSOR_WATCH,
    MOUSE_CURSOR_WAIT_PLANET,
    MOUSE_CURSOR_WAIT_WATCH,
    MOUSE_CURSOR_TYPE_COUNT,
    FIRST_GAME_MOUSE_ANIMATED_CURSOR = MOUSE_CURSOR_WAIT_PLANET,
} MouseCursorType;

extern bool _gmouse_clicked_on_edge;

extern Object* gGameMouseBouncingCursor;
extern Object* gGameMouseHexCursor;

int gameMouseInit();
int gameMouseReset();
void gameMouseExit();
void _gmouse_enable();
void _gmouse_disable(int allowScrolling);
void _gmouse_enable_scrolling();
void _gmouse_disable_scrolling();
bool gmouse_scrolling_is_enabled();
int _gmouse_is_scrolling();
void gameMouseRefresh();
void _gmouse_handle_event(int mouseX, int mouseY, int mouseState);
int gameMouseSetCursor(int cursor);
int gameMouseGetCursor();
void gmouse_set_mapper_mode(int mode);
void gameMouseSetMode(int a1);
int gameMouseGetMode();
void gameMouseCycleMode();
void _gmouse_3d_refresh();
void gameMouseResetBouncingCursorFid();
void gameMouseObjectsShow();
void gameMouseObjectsHide();
bool gameMouseObjectsIsVisible();
int gameMouseRenderPrimaryAction(int x, int y, int menuItem, int width, int height);
int _gmouse_3d_pick_frame_hot(int* a1, int* a2);
int gameMouseRenderActionMenuItems(int x, int y, const int* menuItems, int menuItemsCount, int width, int height);
int gameMouseHighlightActionMenuItemAtIndex(int menuItemIndex);
void gameMouseLoadItemHighlight();
void _gmouse_remove_item_outline(Object* object);

void gameMouseRefreshImmediately();
Object* gmouse_get_outlined_object();

} // namespace fallout

#endif /* GAME_MOUSE_H */
