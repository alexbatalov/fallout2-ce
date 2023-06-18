#include "game_mouse.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "critter.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "interface.h"
#include "item.h"
#include "kb.h"
#include "mouse.h"
#include "object.h"
#include "proto.h"
#include "proto_instance.h"
#include "settings.h"
#include "sfall_config.h"
#include "skill.h"
#include "skilldex.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

typedef enum ScrollableDirections {
    SCROLLABLE_W = 0x01,
    SCROLLABLE_E = 0x02,
    SCROLLABLE_N = 0x04,
    SCROLLABLE_S = 0x08,
} ScrollableDirections;

// 0x518BF8
static bool gGameMouseInitialized = false;

// 0x518BFC
static int _gmouse_enabled = 0;

// 0x518C00
static int _gmouse_mapper_mode = 0;

// 0x518C04
static int _gmouse_click_to_scroll = 0;

// 0x518C08
static int _gmouse_scrolling_enabled = 1;

// 0x518C0C
static int gGameMouseCursor = MOUSE_CURSOR_NONE;

// 0x518C10
static CacheEntry* gGameMouseCursorFrmHandle = INVALID_CACHE_ENTRY;

// 0x518C14
static const int gGameMouseCursorFrmIds[MOUSE_CURSOR_TYPE_COUNT] = {
    266,
    267,
    268,
    269,
    270,
    271,
    272,
    273,
    274,
    275,
    276,
    277,
    330,
    331,
    329,
    328,
    332,
    334,
    333,
    335,
    279,
    280,
    281,
    293,
    310,
    278,
    295,
};

// 0x518C80
static bool gGameMouseObjectsInitialized = false;

// 0x518C84
static bool _gmouse_3d_hover_test = false;

// 0x518C88
static unsigned int _gmouse_3d_last_move_time = 0;

// actmenu.frm
// 0x518C8C
static Art* gGameMouseActionMenuFrm = NULL;

// 0x518C90
static CacheEntry* gGameMouseActionMenuFrmHandle = INVALID_CACHE_ENTRY;

// 0x518C94
static int gGameMouseActionMenuFrmWidth = 0;

// 0x518C98
static int gGameMouseActionMenuFrmHeight = 0;

// 0x518C9C
static int gGameMouseActionMenuFrmDataSize = 0;

// 0x518CA0
static int _gmouse_3d_menu_frame_hot_x = 0;

// 0x518CA4
static int _gmouse_3d_menu_frame_hot_y = 0;

// 0x518CA8
static unsigned char* gGameMouseActionMenuFrmData = NULL;

// actpick.frm
// 0x518CAC
static Art* gGameMouseActionPickFrm = NULL;

// 0x518CB0
static CacheEntry* gGameMouseActionPickFrmHandle = INVALID_CACHE_ENTRY;

// 0x518CB4
static int gGameMouseActionPickFrmWidth = 0;

// 0x518CB8
static int gGameMouseActionPickFrmHeight = 0;

// 0x518CBC
static int gGameMouseActionPickFrmDataSize = 0;

// 0x518CC0
static int _gmouse_3d_pick_frame_hot_x = 0;

// 0x518CC4
static int _gmouse_3d_pick_frame_hot_y = 0;

// 0x518CC8
static unsigned char* gGameMouseActionPickFrmData = NULL;

// acttohit.frm
// 0x518CCC
static Art* gGameMouseActionHitFrm = NULL;

// 0x518CD0
static CacheEntry* gGameMouseActionHitFrmHandle = INVALID_CACHE_ENTRY;

// 0x518CD4
static int gGameMouseActionHitFrmWidth = 0;

// 0x518CD8
static int gGameMouseActionHitFrmHeight = 0;

// 0x518CDC
static int gGameMouseActionHitFrmDataSize = 0;

// 0x518CE0
static unsigned char* gGameMouseActionHitFrmData = NULL;

// blank.frm
// 0x518CE4
static Art* gGameMouseBouncingCursorFrm = NULL;

// 0x518CE8
static CacheEntry* gGameMouseBouncingCursorFrmHandle = INVALID_CACHE_ENTRY;

// 0x518CEC
static int gGameMouseBouncingCursorFrmWidth = 0;

// 0x518CF0
static int gGameMouseBouncingCursorFrmHeight = 0;

// 0x518CF4
static int gGameMouseBouncingCursorFrmDataSize = 0;

// 0x518CF8
static unsigned char* gGameMouseBouncingCursorFrmData = NULL;

// msef000.frm
// 0x518CFC
static Art* gGameMouseHexCursorFrm = NULL;

// 0x518D00
static CacheEntry* gGameMouseHexCursorFrmHandle = INVALID_CACHE_ENTRY;

// 0x518D04
static int gGameMouseHexCursorFrmWidth = 0;

// 0x518D08
static int gGameMouseHexCursorHeight = 0;

// 0x518D0C
static int gGameMouseHexCursorDataSize = 0;

// 0x518D10
static unsigned char* gGameMouseHexCursorFrmData = NULL;

// 0x518D14
static unsigned char gGameMouseActionMenuItemsLength = 0;

// 0x518D18
static unsigned char* _gmouse_3d_menu_actions_start = NULL;

// 0x518D1C
static unsigned char gGameMouseActionMenuHighlightedItemIndex = 0;

// 0x518D1E
static const short gGameMouseActionMenuItemFrmIds[GAME_MOUSE_ACTION_MENU_ITEM_COUNT] = {
    253, // Cancel
    255, // Drop
    257, // Inventory
    259, // Look
    261, // Rotate
    263, // Talk
    265, // Use/Get
    302, // Unload
    304, // Skill
    435, // Push
};

// 0x518D34
static int _gmouse_3d_modes_enabled = 1;

// 0x518D38
static int gGameMouseMode = GAME_MOUSE_MODE_MOVE;

// 0x518D3C
static int gGameMouseModeFrmIds[GAME_MOUSE_MODE_COUNT] = {
    249,
    250,
    251,
    293,
    293,
    293,
    293,
    293,
    293,
    293,
    293,
};

// 0x518D68
static const int gGameMouseModeSkills[GAME_MOUSE_MODE_SKILL_COUNT] = {
    SKILL_FIRST_AID,
    SKILL_DOCTOR,
    SKILL_LOCKPICK,
    SKILL_STEAL,
    SKILL_TRAPS,
    SKILL_SCIENCE,
    SKILL_REPAIR,
};

// 0x518D84
static int gGameMouseAnimatedCursorNextFrame = 0;

// 0x518D88
static unsigned int gGameMouseAnimatedCursorLastUpdateTimestamp = 0;

// 0x518D8C
static int _gmouse_bk_last_cursor = -1;

// 0x518D90
static bool gGameMouseItemHighlightEnabled = true;

// 0x518D94
static Object* gGameMouseHighlightedItem = NULL;

// 0x518D98
bool _gmouse_clicked_on_edge = false;

// 0x518D9C
static int dword_518D9C = -1;

// 0x596C3C
static int gGameMouseActionMenuItems[GAME_MOUSE_ACTION_MENU_ITEM_COUNT];

// 0x596C64
static int gGameMouseLastX;

// 0x596C68
static int gGameMouseLastY;

// blank.frm
// 0x596C6C
Object* gGameMouseBouncingCursor;

// msef000.frm
// 0x596C70
Object* gGameMouseHexCursor;

// 0x596C74
static Object* gGameMousePointedObject;

static int _gmouse_get_click_to_scroll();
static void _gmouse_3d_enable_modes();
static int gameMouseSetBouncingCursorFid(int fid);
static Object* gameMouseGetObjectUnderCursor(int objectType, bool a2, int elevation);
static int gameMouseRenderAccuracy(const char* string, int color);
static int gameMouseRenderActionPoints(const char* string, int color);
static int gameMouseObjectsInit();
static int gameMouseObjectsReset();
static void gameMouseObjectsFree();
static int gameMouseActionMenuInit();
static void gameMouseActionMenuFree();
static int gmouse_3d_set_flat_fid(int fid, Rect* rect);
static int gameMouseUpdateHexCursorFid(Rect* rect);
static int _gmouse_3d_move_to(int x, int y, int elevation, Rect* a4);
static int gameMouseHandleScrolling(int x, int y, int cursor);
static int objectIsDoor(Object* object);
static bool gameMouseClickOnInterfaceBar();

static void customMouseModeFrmsInit();

// 0x44B2B0
int gameMouseInit()
{
    if (gGameMouseInitialized) {
        return -1;
    }

    if (gameMouseObjectsInit() != 0) {
        return -1;
    }

    gGameMouseInitialized = true;
    _gmouse_enabled = 1;

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    // SFALL
    customMouseModeFrmsInit();

    return 0;
}

// 0x44B2E8
int gameMouseReset()
{
    if (!gGameMouseInitialized) {
        return -1;
    }

    // NOTE: Uninline.
    if (gameMouseObjectsReset() != 0) {
        return -1;
    }

    // NOTE: Uninline.
    _gmouse_enable();

    _gmouse_scrolling_enabled = 1;
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    gGameMouseAnimatedCursorNextFrame = 0;
    gGameMouseAnimatedCursorLastUpdateTimestamp = 0;
    _gmouse_clicked_on_edge = 0;

    return 0;
}

// 0x44B3B8
void gameMouseExit()
{
    if (!gGameMouseInitialized) {
        return;
    }

    mouseHideCursor();

    mouseSetFrame(NULL, 0, 0, 0, 0, 0, 0);

    // NOTE: Uninline.
    gameMouseObjectsFree();

    if (gGameMouseCursorFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseCursorFrmHandle);
    }
    gGameMouseCursorFrmHandle = INVALID_CACHE_ENTRY;

    _gmouse_enabled = 0;
    gGameMouseInitialized = false;
    gGameMouseCursor = -1;
}

// 0x44B454
void _gmouse_enable()
{
    if (!_gmouse_enabled) {
        gGameMouseCursor = -1;
        gameMouseSetCursor(MOUSE_CURSOR_NONE);
        _gmouse_scrolling_enabled = 1;
        _gmouse_enabled = 1;
        _gmouse_bk_last_cursor = -1;
    }
}

// 0x44B48C
void _gmouse_disable(int a1)
{
    if (_gmouse_enabled) {
        gameMouseSetCursor(MOUSE_CURSOR_NONE);
        _gmouse_enabled = 0;

        if (a1 & 1) {
            _gmouse_scrolling_enabled = 1;
        } else {
            _gmouse_scrolling_enabled = 0;
        }
    }
}

// 0x44B4CC
void _gmouse_enable_scrolling()
{
    _gmouse_scrolling_enabled = 1;
}

// 0x44B4D8
void _gmouse_disable_scrolling()
{
    _gmouse_scrolling_enabled = 0;
}

// NOTE: Inlined.
//
// 0x44B4E4
bool gmouse_scrolling_is_enabled()
{
    return _gmouse_scrolling_enabled;
}

// 0x44B504
int _gmouse_get_click_to_scroll()
{
    return _gmouse_click_to_scroll;
}

// 0x44B54C
int _gmouse_is_scrolling()
{
    int v1 = 0;

    if (_gmouse_scrolling_enabled) {
        int x;
        int y;
        mouseGetPosition(&x, &y);
        if (x == _scr_size.left || x == _scr_size.right || y == _scr_size.top || y == _scr_size.bottom) {
            switch (gGameMouseCursor) {
            case MOUSE_CURSOR_SCROLL_NW:
            case MOUSE_CURSOR_SCROLL_N:
            case MOUSE_CURSOR_SCROLL_NE:
            case MOUSE_CURSOR_SCROLL_E:
            case MOUSE_CURSOR_SCROLL_SE:
            case MOUSE_CURSOR_SCROLL_S:
            case MOUSE_CURSOR_SCROLL_SW:
            case MOUSE_CURSOR_SCROLL_W:
            case MOUSE_CURSOR_SCROLL_NW_INVALID:
            case MOUSE_CURSOR_SCROLL_N_INVALID:
            case MOUSE_CURSOR_SCROLL_NE_INVALID:
            case MOUSE_CURSOR_SCROLL_E_INVALID:
            case MOUSE_CURSOR_SCROLL_SE_INVALID:
            case MOUSE_CURSOR_SCROLL_S_INVALID:
            case MOUSE_CURSOR_SCROLL_SW_INVALID:
            case MOUSE_CURSOR_SCROLL_W_INVALID:
                v1 = 1;
                break;
            default:
                return v1;
            }
        }
    }

    return v1;
}

// 0x44B684
void gameMouseRefresh()
{
    if (!gGameMouseInitialized) {
        return;
    }

    int mouseX;
    int mouseY;

    if (gGameMouseCursor >= FIRST_GAME_MOUSE_ANIMATED_CURSOR) {
        _mouse_info();

        // NOTE: Uninline.
        if (gmouse_scrolling_is_enabled()) {
            mouseGetPosition(&mouseX, &mouseY);
            int oldMouseCursor = gGameMouseCursor;

            if (gameMouseHandleScrolling(mouseX, mouseY, gGameMouseCursor) == 0) {
                switch (oldMouseCursor) {
                case MOUSE_CURSOR_SCROLL_NW:
                case MOUSE_CURSOR_SCROLL_N:
                case MOUSE_CURSOR_SCROLL_NE:
                case MOUSE_CURSOR_SCROLL_E:
                case MOUSE_CURSOR_SCROLL_SE:
                case MOUSE_CURSOR_SCROLL_S:
                case MOUSE_CURSOR_SCROLL_SW:
                case MOUSE_CURSOR_SCROLL_W:
                case MOUSE_CURSOR_SCROLL_NW_INVALID:
                case MOUSE_CURSOR_SCROLL_N_INVALID:
                case MOUSE_CURSOR_SCROLL_NE_INVALID:
                case MOUSE_CURSOR_SCROLL_E_INVALID:
                case MOUSE_CURSOR_SCROLL_SE_INVALID:
                case MOUSE_CURSOR_SCROLL_S_INVALID:
                case MOUSE_CURSOR_SCROLL_SW_INVALID:
                case MOUSE_CURSOR_SCROLL_W_INVALID:
                    break;
                default:
                    _gmouse_bk_last_cursor = oldMouseCursor;
                    break;
                }
                return;
            }

            if (_gmouse_bk_last_cursor != -1) {
                gameMouseSetCursor(_gmouse_bk_last_cursor);
                _gmouse_bk_last_cursor = -1;
                return;
            }
        }

        gameMouseSetCursor(gGameMouseCursor);
        return;
    }

    if (!_gmouse_enabled) {
        // NOTE: Uninline.
        if (gmouse_scrolling_is_enabled()) {
            mouseGetPosition(&mouseX, &mouseY);
            int oldMouseCursor = gGameMouseCursor;

            if (gameMouseHandleScrolling(mouseX, mouseY, gGameMouseCursor) == 0) {
                switch (oldMouseCursor) {
                case MOUSE_CURSOR_SCROLL_NW:
                case MOUSE_CURSOR_SCROLL_N:
                case MOUSE_CURSOR_SCROLL_NE:
                case MOUSE_CURSOR_SCROLL_E:
                case MOUSE_CURSOR_SCROLL_SE:
                case MOUSE_CURSOR_SCROLL_S:
                case MOUSE_CURSOR_SCROLL_SW:
                case MOUSE_CURSOR_SCROLL_W:
                case MOUSE_CURSOR_SCROLL_NW_INVALID:
                case MOUSE_CURSOR_SCROLL_N_INVALID:
                case MOUSE_CURSOR_SCROLL_NE_INVALID:
                case MOUSE_CURSOR_SCROLL_E_INVALID:
                case MOUSE_CURSOR_SCROLL_SE_INVALID:
                case MOUSE_CURSOR_SCROLL_S_INVALID:
                case MOUSE_CURSOR_SCROLL_SW_INVALID:
                case MOUSE_CURSOR_SCROLL_W_INVALID:
                    break;
                default:
                    _gmouse_bk_last_cursor = oldMouseCursor;
                    break;
                }

                return;
            }

            if (_gmouse_bk_last_cursor != -1) {
                gameMouseSetCursor(_gmouse_bk_last_cursor);
                _gmouse_bk_last_cursor = -1;
            }
        }

        return;
    }

    mouseGetPosition(&mouseX, &mouseY);

    int oldMouseCursor = gGameMouseCursor;
    if (gameMouseHandleScrolling(mouseX, mouseY, MOUSE_CURSOR_NONE) == 0) {
        switch (oldMouseCursor) {
        case MOUSE_CURSOR_SCROLL_NW:
        case MOUSE_CURSOR_SCROLL_N:
        case MOUSE_CURSOR_SCROLL_NE:
        case MOUSE_CURSOR_SCROLL_E:
        case MOUSE_CURSOR_SCROLL_SE:
        case MOUSE_CURSOR_SCROLL_S:
        case MOUSE_CURSOR_SCROLL_SW:
        case MOUSE_CURSOR_SCROLL_W:
        case MOUSE_CURSOR_SCROLL_NW_INVALID:
        case MOUSE_CURSOR_SCROLL_N_INVALID:
        case MOUSE_CURSOR_SCROLL_NE_INVALID:
        case MOUSE_CURSOR_SCROLL_E_INVALID:
        case MOUSE_CURSOR_SCROLL_SE_INVALID:
        case MOUSE_CURSOR_SCROLL_S_INVALID:
        case MOUSE_CURSOR_SCROLL_SW_INVALID:
        case MOUSE_CURSOR_SCROLL_W_INVALID:
            break;
        default:
            _gmouse_bk_last_cursor = oldMouseCursor;
            break;
        }
        return;
    }

    if (_gmouse_bk_last_cursor != -1) {
        gameMouseSetCursor(_gmouse_bk_last_cursor);
        _gmouse_bk_last_cursor = -1;
    }

    if (windowGetAtPoint(mouseX, mouseY) != gIsoWindow) {
        if (gGameMouseCursor == MOUSE_CURSOR_NONE) {
            gameMouseObjectsHide();
            gameMouseSetCursor(MOUSE_CURSOR_ARROW);

            if (gGameMouseMode >= 2 && !isInCombat()) {
                gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
            }
        }
        return;
    }

    // NOTE: Strange set of conditions and jumps. Not sure about this one.
    switch (gGameMouseCursor) {
    case MOUSE_CURSOR_NONE:
    case MOUSE_CURSOR_ARROW:
    case MOUSE_CURSOR_SMALL_ARROW_UP:
    case MOUSE_CURSOR_SMALL_ARROW_DOWN:
    case MOUSE_CURSOR_CROSSHAIR:
    case MOUSE_CURSOR_USE_CROSSHAIR:
        if (gGameMouseCursor != MOUSE_CURSOR_NONE) {
            gameMouseSetCursor(MOUSE_CURSOR_NONE);
        }

        if ((gGameMouseHexCursor->flags & OBJECT_HIDDEN) != 0) {
            gameMouseObjectsShow();
        }

        break;
    }

    Rect r1;
    if (_gmouse_3d_move_to(mouseX, mouseY, gElevation, &r1) == 0) {
        tileWindowRefreshRect(&r1, gElevation);
    }

    if ((gGameMouseHexCursor->flags & OBJECT_HIDDEN) != 0 || _gmouse_mapper_mode != 0) {
        return;
    }

    unsigned int v3 = _get_bk_time();
    if (mouseX == gGameMouseLastX && mouseY == gGameMouseLastY) {
        if (_gmouse_3d_hover_test || getTicksBetween(v3, _gmouse_3d_last_move_time) < 250) {
            return;
        }

        if (gGameMouseMode != GAME_MOUSE_MODE_MOVE) {
            if (gGameMouseMode == GAME_MOUSE_MODE_ARROW) {
                _gmouse_3d_last_move_time = v3;
                _gmouse_3d_hover_test = true;

                Object* pointedObject = gameMouseGetObjectUnderCursor(-1, true, gElevation);
                if (pointedObject != NULL) {
                    int primaryAction = -1;

                    switch (FID_TYPE(pointedObject->fid)) {
                    case OBJ_TYPE_ITEM:
                        primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                        if (gGameMouseItemHighlightEnabled) {
                            Rect tmp;
                            if (objectSetOutline(pointedObject, OUTLINE_TYPE_ITEM, &tmp) == 0) {
                                tileWindowRefreshRect(&tmp, gElevation);
                                gGameMouseHighlightedItem = pointedObject;
                            }
                        }
                        break;
                    case OBJ_TYPE_CRITTER:
                        if (pointedObject == gDude) {
                            primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_ROTATE;
                        } else {
                            if (_obj_action_can_talk_to(pointedObject)) {
                                if (isInCombat()) {
                                    primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                                } else {
                                    primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_TALK;
                                }
                            } else {
                                if (_critter_flag_check(pointedObject->pid, CRITTER_NO_STEAL)) {
                                    primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                                } else {
                                    primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                                }
                            }
                        }
                        break;
                    case OBJ_TYPE_SCENERY:
                        if (!_obj_action_can_use(pointedObject)) {
                            primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                        } else {
                            primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                        }
                        break;
                    case OBJ_TYPE_WALL:
                        primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                        break;
                    }

                    if (primaryAction != -1) {
                        if (gameMouseRenderPrimaryAction(mouseX, mouseY, primaryAction, _scr_size.right - _scr_size.left + 1, _scr_size.bottom - _scr_size.top - 99) == 0) {
                            Rect tmp;
                            int fid = buildFid(OBJ_TYPE_INTERFACE, 282, 0, 0, 0);
                            // NOTE: Uninline.
                            if (gmouse_3d_set_flat_fid(fid, &tmp) == 0) {
                                tileWindowRefreshRect(&tmp, gElevation);
                            }
                        }
                    }

                    if (pointedObject != gGameMousePointedObject) {
                        gGameMousePointedObject = pointedObject;
                        _obj_look_at(gDude, gGameMousePointedObject);
                    }
                }
            } else if (gGameMouseMode == GAME_MOUSE_MODE_CROSSHAIR) {
                Object* pointedObject = gameMouseGetObjectUnderCursor(OBJ_TYPE_CRITTER, false, gElevation);
                if (pointedObject == NULL) {
                    pointedObject = gameMouseGetObjectUnderCursor(-1, false, gElevation);
                    if (!objectIsDoor(pointedObject)) {
                        pointedObject = NULL;
                    }
                }

                if (pointedObject != NULL) {
                    bool pointedObjectIsCritter = FID_TYPE(pointedObject->fid) == OBJ_TYPE_CRITTER;

                    if (settings.preferences.combat_looks) {
                        if (_obj_examine(gDude, pointedObject) == -1) {
                            _obj_look_at(gDude, pointedObject);
                        }
                    }

                    int color;
                    int accuracy;
                    char formattedAccuracy[8];
                    if (_combat_to_hit(pointedObject, &accuracy)) {
                        snprintf(formattedAccuracy, sizeof(formattedAccuracy), "%d%%", accuracy);

                        if (pointedObjectIsCritter) {
                            if (pointedObject->data.critter.combat.team != 0) {
                                color = _colorTable[32767];
                            } else {
                                color = _colorTable[32495];
                            }
                        } else {
                            color = _colorTable[17969];
                        }
                    } else {
                        snprintf(formattedAccuracy, sizeof(formattedAccuracy), " %c ", 'X');

                        if (pointedObjectIsCritter) {
                            if (pointedObject->data.critter.combat.team != 0) {
                                color = _colorTable[31744];
                            } else {
                                color = _colorTable[18161];
                            }
                        } else {
                            color = _colorTable[32239];
                        }
                    }

                    if (gameMouseRenderAccuracy(formattedAccuracy, color) == 0) {
                        Rect tmp;
                        int fid = buildFid(OBJ_TYPE_INTERFACE, 284, 0, 0, 0);
                        // NOTE: Uninline.
                        if (gmouse_3d_set_flat_fid(fid, &tmp) == 0) {
                            tileWindowRefreshRect(&tmp, gElevation);
                        }
                    }

                    if (gGameMousePointedObject != pointedObject) {
                        gGameMousePointedObject = pointedObject;
                    }
                } else {
                    Rect tmp;
                    if (gameMouseUpdateHexCursorFid(&tmp) == 0) {
                        tileWindowRefreshRect(&tmp, gElevation);
                    }
                }

                _gmouse_3d_last_move_time = v3;
                _gmouse_3d_hover_test = true;
            }
            return;
        }

        char formattedActionPoints[8];
        int color;
        int distance = _make_path(gDude, gDude->tile, gGameMouseHexCursor->tile, NULL, 1);
        if (distance != 0) {
            if (!isInCombat()) {
                formattedActionPoints[0] = '\0';
                color = _colorTable[31744];
            } else {
                int actionPointsMax = critterGetMovementPointCostAdjustedForCrippledLegs(gDude, distance);
                int actionPointsRequired = std::max(0, actionPointsMax - _combat_free_move);

                if (actionPointsRequired <= gDude->data.critter.combat.ap) {
                    snprintf(formattedActionPoints, sizeof(formattedActionPoints), "%d", actionPointsRequired);
                    color = _colorTable[32767];
                } else {
                    snprintf(formattedActionPoints, sizeof(formattedActionPoints), "%c", 'X');
                    color = _colorTable[31744];
                }
            }
        } else {
            snprintf(formattedActionPoints, sizeof(formattedActionPoints), "%c", 'X');
            color = _colorTable[31744];
        }

        if (gameMouseRenderActionPoints(formattedActionPoints, color) == 0) {
            Rect tmp;
            objectGetRect(gGameMouseHexCursor, &tmp);
            tileWindowRefreshRect(&tmp, 0);
        }

        _gmouse_3d_last_move_time = v3;
        _gmouse_3d_hover_test = true;
        dword_518D9C = gGameMouseHexCursor->tile;
        return;
    }

    _gmouse_3d_last_move_time = v3;
    _gmouse_3d_hover_test = false;
    gGameMouseLastX = mouseX;
    gGameMouseLastY = mouseY;

    if (!_gmouse_mapper_mode) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, 0, 0, 0, 0);
        gameMouseSetBouncingCursorFid(fid);
    }

    int v34 = 0;

    Rect r2;
    Rect r26;
    if (gameMouseUpdateHexCursorFid(&r2) == 0) {
        v34 |= 1;
    }

    if (gGameMouseHighlightedItem != NULL) {
        if (objectClearOutline(gGameMouseHighlightedItem, &r26) == 0) {
            v34 |= 2;
        }
        gGameMouseHighlightedItem = NULL;
    }

    switch (v34) {
    case 3:
        rectUnion(&r2, &r26, &r2);
        // FALLTHROUGH
    case 1:
        tileWindowRefreshRect(&r2, gElevation);
        break;
    case 2:
        tileWindowRefreshRect(&r26, gElevation);
        break;
    }
}

bool gameMouseClickOnInterfaceBar()
{
    Rect interfaceBarWindowRect;
    windowGetRect(gInterfaceBarWindow, &interfaceBarWindowRect);

    int interfaceBarWindowRectLeft = 0;
    int interfaceBarWindowRectRight = _scr_size.right;

    if (gInterfaceBarMode) {
        interfaceBarWindowRectLeft = interfaceBarWindowRect.left;
        interfaceBarWindowRectRight = interfaceBarWindowRect.right;
    }

    return _mouse_click_in(interfaceBarWindowRectLeft, interfaceBarWindowRect.top, interfaceBarWindowRectRight, interfaceBarWindowRect.bottom);
}

// 0x44BFA8
void _gmouse_handle_event(int mouseX, int mouseY, int mouseState)
{
    if (!gGameMouseInitialized) {
        return;
    }

    if (gGameMouseCursor >= MOUSE_CURSOR_WAIT_PLANET) {
        return;
    }

    if (!_gmouse_enabled) {
        return;
    }

    if (_gmouse_clicked_on_edge) {
        if (_gmouse_get_click_to_scroll()) {
            return;
        }
    }

    if (gameMouseClickOnInterfaceBar()) {
        return;
    }

    if ((mouseState & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
        if ((mouseState & MOUSE_EVENT_RIGHT_BUTTON_REPEAT) == 0 && (gGameMouseHexCursor->flags & OBJECT_HIDDEN) == 0) {
            gameMouseCycleMode();
        }
        return;
    }

    if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
        if (gGameMouseMode == GAME_MOUSE_MODE_MOVE) {
            int actionPoints;
            if (isInCombat()) {
                actionPoints = _combat_free_move + gDude->data.critter.combat.ap;
            } else {
                actionPoints = -1;
            }

            if (gPressedPhysicalKeys[SDL_SCANCODE_LSHIFT] || gPressedPhysicalKeys[SDL_SCANCODE_RSHIFT]) {
                if (settings.preferences.running) {
                    _dude_move(actionPoints);
                    return;
                }
            } else {
                if (!settings.preferences.running) {
                    _dude_move(actionPoints);
                    return;
                }
            }

            _dude_run(actionPoints);
            return;
        }

        if (gGameMouseMode == GAME_MOUSE_MODE_ARROW) {
            Object* v5 = gameMouseGetObjectUnderCursor(-1, true, gElevation);
            if (v5 != NULL) {
                switch (FID_TYPE(v5->fid)) {
                case OBJ_TYPE_ITEM:
                    actionPickUp(gDude, v5);
                    break;
                case OBJ_TYPE_CRITTER:
                    if (v5 == gDude) {
                        if (FID_ANIM_TYPE(gDude->fid) == ANIM_STAND) {
                            Rect a1;
                            if (objectRotateClockwise(v5, &a1) == 0) {
                                tileWindowRefreshRect(&a1, v5->elevation);
                            }
                        }
                    } else {
                        if (_obj_action_can_talk_to(v5)) {
                            if (isInCombat()) {
                                if (_obj_examine(gDude, v5) == -1) {
                                    _obj_look_at(gDude, v5);
                                }
                            } else {
                                actionTalk(gDude, v5);
                            }
                        } else {
                            _action_loot_container(gDude, v5);
                        }
                    }
                    break;
                case OBJ_TYPE_SCENERY:
                    if (_obj_action_can_use(v5)) {
                        _action_use_an_object(gDude, v5);
                    } else {
                        if (_obj_examine(gDude, v5) == -1) {
                            _obj_look_at(gDude, v5);
                        }
                    }
                    break;
                case OBJ_TYPE_WALL:
                    if (_obj_examine(gDude, v5) == -1) {
                        _obj_look_at(gDude, v5);
                    }
                    break;
                }
            }
            return;
        }

        if (gGameMouseMode == GAME_MOUSE_MODE_CROSSHAIR) {
            Object* v7 = gameMouseGetObjectUnderCursor(OBJ_TYPE_CRITTER, false, gElevation);
            if (v7 == NULL) {
                v7 = gameMouseGetObjectUnderCursor(-1, false, gElevation);
                if (!objectIsDoor(v7)) {
                    v7 = NULL;
                }
            }

            if (v7 != NULL) {
                _combat_attack_this(v7);
                _gmouse_3d_hover_test = true;
                gGameMouseLastY = mouseY;
                gGameMouseLastX = mouseX;
                _gmouse_3d_last_move_time = getTicks() - 250;
            }
            return;
        }

        if (gGameMouseMode == GAME_MOUSE_MODE_USE_CROSSHAIR) {
            Object* object = gameMouseGetObjectUnderCursor(-1, true, gElevation);
            if (object != NULL) {
                Object* weapon;
                if (interfaceGetActiveItem(&weapon) != -1) {
                    if (isInCombat()) {
                        int hitMode = interfaceGetCurrentHand()
                            ? HIT_MODE_RIGHT_WEAPON_PRIMARY
                            : HIT_MODE_LEFT_WEAPON_PRIMARY;

                        int actionPointsRequired = itemGetActionPointCost(gDude, hitMode, false);
                        if (actionPointsRequired <= gDude->data.critter.combat.ap) {
                            if (_action_use_an_item_on_object(gDude, object, weapon) != -1) {
                                int actionPoints = gDude->data.critter.combat.ap;
                                if (actionPointsRequired > actionPoints) {
                                    gDude->data.critter.combat.ap = 0;
                                } else {
                                    gDude->data.critter.combat.ap -= actionPointsRequired;
                                }
                                interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
                            }
                        }
                    } else {
                        _action_use_an_item_on_object(gDude, object, weapon);
                    }
                }
            }
            gameMouseSetCursor(MOUSE_CURSOR_NONE);
            gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
            return;
        }

        if (gGameMouseMode == GAME_MOUSE_MODE_USE_FIRST_AID
            || gGameMouseMode == GAME_MOUSE_MODE_USE_DOCTOR
            || gGameMouseMode == GAME_MOUSE_MODE_USE_LOCKPICK
            || gGameMouseMode == GAME_MOUSE_MODE_USE_STEAL
            || gGameMouseMode == GAME_MOUSE_MODE_USE_TRAPS
            || gGameMouseMode == GAME_MOUSE_MODE_USE_SCIENCE
            || gGameMouseMode == GAME_MOUSE_MODE_USE_REPAIR) {
            Object* object = gameMouseGetObjectUnderCursor(-1, 1, gElevation);
            if (object == NULL || actionUseSkill(gDude, object, gGameMouseModeSkills[gGameMouseMode - FIRST_GAME_MOUSE_MODE_SKILL]) != -1) {
                gameMouseSetCursor(MOUSE_CURSOR_NONE);
                gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
            }
            return;
        }
    }

    if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT) == MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT && gGameMouseMode == GAME_MOUSE_MODE_ARROW) {
        Object* v16 = gameMouseGetObjectUnderCursor(-1, true, gElevation);
        if (v16 != NULL) {
            int actionMenuItemsCount = 0;
            int actionMenuItems[6];
            switch (FID_TYPE(v16->fid)) {
            case OBJ_TYPE_ITEM:
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                if (itemGetType(v16) == ITEM_TYPE_CONTAINER) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL;
                }
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            case OBJ_TYPE_CRITTER:
                if (v16 == gDude) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_ROTATE;
                } else {
                    if (_obj_action_can_talk_to(v16)) {
                        if (!isInCombat()) {
                            actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_TALK;
                        }
                    } else {
                        if (!_critter_flag_check(v16->pid, CRITTER_NO_STEAL)) {
                            actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                        }
                    }

                    if (actionCheckPush(gDude, v16)) {
                        actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_PUSH;
                    }
                }

                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            case OBJ_TYPE_SCENERY:
                if (_obj_action_can_use(v16)) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                }

                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            case OBJ_TYPE_WALL:
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                if (_obj_action_can_use(v16)) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                }
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            }

            if (gameMouseRenderActionMenuItems(mouseX, mouseY, actionMenuItems, actionMenuItemsCount, _scr_size.right - _scr_size.left + 1, _scr_size.bottom - _scr_size.top - 99) == 0) {
                Rect v43;
                int fid = buildFid(OBJ_TYPE_INTERFACE, 283, 0, 0, 0);
                // NOTE: Uninline.
                if (gmouse_3d_set_flat_fid(fid, &v43) == 0 && _gmouse_3d_move_to(mouseX, mouseY, gElevation, &v43) == 0) {
                    tileWindowRefreshRect(&v43, gElevation);
                    isoDisable();

                    int v33 = mouseY;
                    int actionIndex = 0;
                    while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_UP) == 0) {
                        sharedFpsLimiter.mark();

                        inputGetInput();

                        if (_game_user_wants_to_quit != 0) {
                            actionMenuItems[actionIndex] = 0;
                        }

                        int v48;
                        int v47;
                        mouseGetPosition(&v48, &v47);

                        if (abs(v47 - v33) > 10) {
                            if (v33 >= v47) {
                                actionIndex -= 1;
                            } else {
                                actionIndex += 1;
                            }

                            if (gameMouseHighlightActionMenuItemAtIndex(actionIndex) == 0) {
                                tileWindowRefreshRect(&v43, gElevation);
                            }
                            v33 = v47;
                        }

                        renderPresent();
                        sharedFpsLimiter.throttle();
                    }

                    isoEnable();

                    _gmouse_3d_hover_test = false;
                    gGameMouseLastX = mouseX;
                    gGameMouseLastY = mouseY;
                    _gmouse_3d_last_move_time = getTicks();

                    _mouse_set_position(mouseX, v33);

                    if (gameMouseUpdateHexCursorFid(&v43) == 0) {
                        tileWindowRefreshRect(&v43, gElevation);
                    }

                    switch (actionMenuItems[actionIndex]) {
                    case GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY:
                        inventoryOpenUseItemOn(v16);
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_LOOK:
                        if (_obj_examine(gDude, v16) == -1) {
                            _obj_look_at(gDude, v16);
                        }
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_ROTATE:
                        if (objectRotateClockwise(v16, &v43) == 0) {
                            tileWindowRefreshRect(&v43, v16->elevation);
                        }
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_TALK:
                        actionTalk(gDude, v16);
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_USE:
                        switch (FID_TYPE(v16->fid)) {
                        case OBJ_TYPE_SCENERY:
                            _action_use_an_object(gDude, v16);
                            break;
                        case OBJ_TYPE_CRITTER:
                            _action_loot_container(gDude, v16);
                            break;
                        default:
                            actionPickUp(gDude, v16);
                            break;
                        }
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL:
                        if (1) {
                            int skill = -1;

                            int rc = skilldexOpen();
                            switch (rc) {
                            case SKILLDEX_RC_SNEAK:
                                _action_skill_use(SKILL_SNEAK);
                                break;
                            case SKILLDEX_RC_LOCKPICK:
                                skill = SKILL_LOCKPICK;
                                break;
                            case SKILLDEX_RC_STEAL:
                                skill = SKILL_STEAL;
                                break;
                            case SKILLDEX_RC_TRAPS:
                                skill = SKILL_TRAPS;
                                break;
                            case SKILLDEX_RC_FIRST_AID:
                                skill = SKILL_FIRST_AID;
                                break;
                            case SKILLDEX_RC_DOCTOR:
                                skill = SKILL_DOCTOR;
                                break;
                            case SKILLDEX_RC_SCIENCE:
                                skill = SKILL_SCIENCE;
                                break;
                            case SKILLDEX_RC_REPAIR:
                                skill = SKILL_REPAIR;
                                break;
                            }

                            if (skill != -1) {
                                actionUseSkill(gDude, v16, skill);
                            }
                        }
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_PUSH:
                        actionPush(gDude, v16);
                        break;
                    }
                }
            }
        }
    }
}

// 0x44C840
int gameMouseSetCursor(int cursor)
{
    if (!gGameMouseInitialized) {
        return -1;
    }

    if (cursor != MOUSE_CURSOR_ARROW && cursor == gGameMouseCursor && (gGameMouseCursor < 25 || gGameMouseCursor >= 27)) {
        return -1;
    }

    CacheEntry* mouseCursorFrmHandle;
    int fid = buildFid(OBJ_TYPE_INTERFACE, gGameMouseCursorFrmIds[cursor], 0, 0, 0);
    Art* mouseCursorFrm = artLock(fid, &mouseCursorFrmHandle);
    if (mouseCursorFrm == NULL) {
        return -1;
    }

    bool shouldUpdate = true;
    int frame = 0;
    if (cursor >= FIRST_GAME_MOUSE_ANIMATED_CURSOR) {
        unsigned int tick = getTicks();

        if ((gGameMouseHexCursor->flags & OBJECT_HIDDEN) == 0) {
            gameMouseObjectsHide();
        }

        unsigned int delay = 1000 / artGetFramesPerSecond(mouseCursorFrm);
        if (getTicksBetween(tick, gGameMouseAnimatedCursorLastUpdateTimestamp) < delay && cursor == gGameMouseCursor) {
            shouldUpdate = false;
        } else {
            if (artGetFrameCount(mouseCursorFrm) <= gGameMouseAnimatedCursorNextFrame) {
                gGameMouseAnimatedCursorNextFrame = 0;
            }

            frame = gGameMouseAnimatedCursorNextFrame;
            gGameMouseAnimatedCursorLastUpdateTimestamp = tick;
            gGameMouseAnimatedCursorNextFrame++;
        }
    }

    if (!shouldUpdate) {
        return -1;
    }

    int width = artGetWidth(mouseCursorFrm, frame, 0);
    int height = artGetHeight(mouseCursorFrm, frame, 0);

    int offsetX;
    int offsetY;
    artGetRotationOffsets(mouseCursorFrm, 0, &offsetX, &offsetY);

    offsetX = width / 2 - offsetX;
    offsetY = height - 1 - offsetY;

    unsigned char* mouseCursorFrmData = artGetFrameData(mouseCursorFrm, frame, 0);
    if (mouseSetFrame(mouseCursorFrmData, width, height, width, offsetX, offsetY, 0) != 0) {
        return -1;
    }

    if (gGameMouseCursorFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseCursorFrmHandle);
    }

    gGameMouseCursor = cursor;
    gGameMouseCursorFrmHandle = mouseCursorFrmHandle;

    return 0;
}

// 0x44C9E8
int gameMouseGetCursor()
{
    return gGameMouseCursor;
}

// 0x44C9F0
void gmouse_set_mapper_mode(int mode)
{
    _gmouse_mapper_mode = mode;
}

// 0x44C9F8
void _gmouse_3d_enable_modes()
{
    _gmouse_3d_modes_enabled = 1;
}

// 0x44CA18
void gameMouseSetMode(int mode)
{
    if (!gGameMouseInitialized) {
        return;
    }

    if (!_gmouse_3d_modes_enabled) {
        return;
    }

    if (mode == gGameMouseMode) {
        return;
    }

    int fid = buildFid(OBJ_TYPE_INTERFACE, 0, 0, 0, 0);
    gameMouseSetBouncingCursorFid(fid);

    fid = buildFid(OBJ_TYPE_INTERFACE, gGameMouseModeFrmIds[mode], 0, 0, 0);

    Rect rect;
    // NOTE: Uninline.
    if (gmouse_3d_set_flat_fid(fid, &rect) == -1) {
        return;
    }

    int mouseX;
    int mouseY;
    mouseGetPosition(&mouseX, &mouseY);

    Rect r2;
    if (_gmouse_3d_move_to(mouseX, mouseY, gElevation, &r2) == 0) {
        rectUnion(&rect, &r2, &rect);
    }

    int v5 = 0;
    if (gGameMouseMode == GAME_MOUSE_MODE_CROSSHAIR) {
        v5 = -1;
    }

    if (mode != 0) {
        if (mode == GAME_MOUSE_MODE_CROSSHAIR) {
            v5 = 1;
        }

        if (gGameMouseMode == 0) {
            if (objectDisableOutline(gGameMouseHexCursor, &r2) == 0) {
                rectUnion(&rect, &r2, &rect);
            }
        }
    } else {
        if (objectEnableOutline(gGameMouseHexCursor, &r2) == 0) {
            rectUnion(&rect, &r2, &rect);
        }
    }

    gGameMouseMode = mode;
    _gmouse_3d_hover_test = false;
    _gmouse_3d_last_move_time = getTicks();

    tileWindowRefreshRect(&rect, gElevation);

    switch (v5) {
    case 1:
        _combat_outline_on();
        break;
    case -1:
        _combat_outline_off();
        break;
    }
}

// 0x44CB6C
int gameMouseGetMode()
{
    return gGameMouseMode;
}

// 0x44CB74
void gameMouseCycleMode()
{
    int mode = (gGameMouseMode + 1) % 3;

    if (isInCombat()) {
        Object* item;
        if (interfaceGetActiveItem(&item) == 0) {
            if (item != NULL && itemGetType(item) != ITEM_TYPE_WEAPON && mode == GAME_MOUSE_MODE_CROSSHAIR) {
                mode = GAME_MOUSE_MODE_MOVE;
            }
        }
    } else {
        if (mode == GAME_MOUSE_MODE_CROSSHAIR) {
            mode = GAME_MOUSE_MODE_MOVE;
        }
    }

    gameMouseSetMode(mode);
}

// 0x44CBD0
void _gmouse_3d_refresh()
{
    gGameMouseLastX = -1;
    gGameMouseLastY = -1;
    _gmouse_3d_hover_test = false;
    _gmouse_3d_last_move_time = 0;
    gameMouseRefresh();
}

// 0x44CBFC
int gameMouseSetBouncingCursorFid(int fid)
{
    if (!gGameMouseInitialized) {
        return -1;
    }

    if (!artExists(fid)) {
        return -1;
    }

    if (gGameMouseBouncingCursor->fid == fid) {
        return -1;
    }

    if (!_gmouse_mapper_mode) {
        return objectSetFid(gGameMouseBouncingCursor, fid, NULL);
    }

    int v1 = 0;

    Rect oldRect;
    if (gGameMouseBouncingCursor->fid != -1) {
        objectGetRect(gGameMouseBouncingCursor, &oldRect);
        v1 |= 1;
    }

    int rc = -1;

    Rect rect;
    if (objectSetFid(gGameMouseBouncingCursor, fid, &rect) == 0) {
        rc = 0;
        v1 |= 2;
    }

    if ((gGameMouseHexCursor->flags & OBJECT_HIDDEN) == 0) {
        if (v1 == 1) {
            tileWindowRefreshRect(&oldRect, gElevation);
        } else if (v1 == 2) {
            tileWindowRefreshRect(&rect, gElevation);
        } else if (v1 == 3) {
            rectUnion(&oldRect, &rect, &oldRect);
            tileWindowRefreshRect(&oldRect, gElevation);
        }
    }

    return rc;
}

// 0x44CD0C
void gameMouseResetBouncingCursorFid()
{
    int fid = buildFid(OBJ_TYPE_INTERFACE, 0, 0, 0, 0);
    gameMouseSetBouncingCursorFid(fid);
}

// 0x44CD2C
void gameMouseObjectsShow()
{
    if (!gGameMouseInitialized) {
        return;
    }

    int v2 = 0;

    Rect rect1;
    if (objectShow(gGameMouseBouncingCursor, &rect1) == 0) {
        v2 |= 1;
    }

    Rect rect2;
    if (objectShow(gGameMouseHexCursor, &rect2) == 0) {
        v2 |= 2;
    }

    Rect tmp;
    if (gGameMouseMode != GAME_MOUSE_MODE_MOVE) {
        if (objectDisableOutline(gGameMouseHexCursor, &tmp) == 0) {
            if ((v2 & 2) != 0) {
                rectUnion(&rect2, &tmp, &rect2);
            } else {
                memcpy(&rect2, &tmp, sizeof(rect2));
                v2 |= 2;
            }
        }
    }

    if (gameMouseUpdateHexCursorFid(&tmp) == 0) {
        if ((v2 & 2) != 0) {
            rectUnion(&rect2, &tmp, &rect2);
        } else {
            memcpy(&rect2, &tmp, sizeof(rect2));
            v2 |= 2;
        }
    }

    if (v2 != 0) {
        Rect* rect;
        switch (v2) {
        case 1:
            rect = &rect1;
            break;
        case 2:
            rect = &rect2;
            break;
        case 3:
            rectUnion(&rect1, &rect2, &rect1);
            rect = &rect1;
            break;
        default:
            assert(false && "Should be unreachable");
        }

        tileWindowRefreshRect(rect, gElevation);
    }

    _gmouse_3d_hover_test = false;
    _gmouse_3d_last_move_time = getTicks() - 250;
}

// 0x44CE34
void gameMouseObjectsHide()
{
    if (!gGameMouseInitialized) {
        return;
    }

    int v1 = 0;

    Rect rect1;
    if (objectHide(gGameMouseBouncingCursor, &rect1) == 0) {
        v1 |= 1;
    }

    Rect rect2;
    if (objectHide(gGameMouseHexCursor, &rect2) == 0) {
        v1 |= 2;
    }

    if (v1 == 1) {
        tileWindowRefreshRect(&rect1, gElevation);
    } else if (v1 == 2) {
        tileWindowRefreshRect(&rect2, gElevation);
    } else if (v1 == 3) {
        rectUnion(&rect1, &rect2, &rect1);
        tileWindowRefreshRect(&rect1, gElevation);
    }
}

// 0x44CEB0
bool gameMouseObjectsIsVisible()
{
    return (gGameMouseHexCursor->flags & OBJECT_HIDDEN) == 0;
}

// 0x44CEC4
Object* gameMouseGetObjectUnderCursor(int objectType, bool a2, int elevation)
{
    int mouseX;
    int mouseY;
    mouseGetPosition(&mouseX, &mouseY);

    bool v13 = false;
    if (objectType == -1) {
        if (_square_roof_intersect(mouseX, mouseY, elevation)) {
            if (_obj_intersects_with(gEgg, mouseX, mouseY) == 0) {
                v13 = true;
            }
        }
    }

    Object* v4 = NULL;
    if (!v13) {
        ObjectWithFlags* entries;
        int count = _obj_create_intersect_list(mouseX, mouseY, elevation, objectType, &entries);
        for (int index = count - 1; index >= 0; index--) {
            ObjectWithFlags* ptr = &(entries[index]);
            if (a2 || gDude != ptr->object) {
                v4 = ptr->object;
                if ((ptr->flags & 0x01) != 0) {
                    if ((ptr->flags & 0x04) == 0) {
                        if (FID_TYPE(ptr->object->fid) != OBJ_TYPE_CRITTER || (ptr->object->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD)) == 0) {
                            break;
                        }
                    }
                }
            }
        }

        if (count != 0) {
            _obj_delete_intersect_list(&entries);
        }
    }
    return v4;
}

// 0x44CFA0
int gameMouseRenderPrimaryAction(int x, int y, int menuItem, int width, int height)
{
    CacheEntry* menuItemFrmHandle;
    int menuItemFid = buildFid(OBJ_TYPE_INTERFACE, gGameMouseActionMenuItemFrmIds[menuItem], 0, 0, 0);
    Art* menuItemFrm = artLock(menuItemFid, &menuItemFrmHandle);
    if (menuItemFrm == NULL) {
        return -1;
    }

    CacheEntry* arrowFrmHandle;
    int arrowFid = buildFid(OBJ_TYPE_INTERFACE, gGameMouseModeFrmIds[GAME_MOUSE_MODE_ARROW], 0, 0, 0);
    Art* arrowFrm = artLock(arrowFid, &arrowFrmHandle);
    if (arrowFrm == NULL) {
        artUnlock(menuItemFrmHandle);
        // FIXME: Why this is success?
        return 0;
    }

    unsigned char* arrowFrmData = artGetFrameData(arrowFrm, 0, 0);
    int arrowFrmWidth = artGetWidth(arrowFrm, 0, 0);
    int arrowFrmHeight = artGetHeight(arrowFrm, 0, 0);

    unsigned char* menuItemFrmData = artGetFrameData(menuItemFrm, 0, 0);
    int menuItemFrmWidth = artGetWidth(menuItemFrm, 0, 0);
    int menuItemFrmHeight = artGetHeight(menuItemFrm, 0, 0);

    unsigned char* arrowFrmDest = gGameMouseActionPickFrmData;
    unsigned char* menuItemFrmDest = gGameMouseActionPickFrmData;

    _gmouse_3d_pick_frame_hot_x = 0;
    _gmouse_3d_pick_frame_hot_y = 0;

    gGameMouseActionPickFrm->xOffsets[0] = gGameMouseActionPickFrmWidth / 2;
    gGameMouseActionPickFrm->yOffsets[0] = gGameMouseActionPickFrmHeight - 1;

    int maxX = x + menuItemFrmWidth + arrowFrmWidth - 1;
    int maxY = y + menuItemFrmHeight - 1;
    int shiftY = maxY - height + 2;

    if (maxX < width) {
        menuItemFrmDest += arrowFrmWidth;
        if (maxY >= height) {
            _gmouse_3d_pick_frame_hot_y = shiftY;
            gGameMouseActionPickFrm->yOffsets[0] -= shiftY;
            arrowFrmDest += gGameMouseActionPickFrmWidth * shiftY;
        }
    } else {
        artUnlock(arrowFrmHandle);

        arrowFid = buildFid(OBJ_TYPE_INTERFACE, 285, 0, 0, 0);
        arrowFrm = artLock(arrowFid, &arrowFrmHandle);
        arrowFrmData = artGetFrameData(arrowFrm, 0, 0);
        arrowFrmDest += menuItemFrmWidth;

        gGameMouseActionPickFrm->xOffsets[0] = -gGameMouseActionPickFrm->xOffsets[0];
        _gmouse_3d_pick_frame_hot_x += menuItemFrmWidth + arrowFrmWidth;

        if (maxY >= height) {
            _gmouse_3d_pick_frame_hot_y += shiftY;
            gGameMouseActionPickFrm->yOffsets[0] -= shiftY;

            arrowFrmDest += gGameMouseActionPickFrmWidth * shiftY;
        }
    }

    memset(gGameMouseActionPickFrmData, 0, gGameMouseActionPickFrmDataSize);

    blitBufferToBuffer(arrowFrmData, arrowFrmWidth, arrowFrmHeight, arrowFrmWidth, arrowFrmDest, gGameMouseActionPickFrmWidth);
    blitBufferToBuffer(menuItemFrmData, menuItemFrmWidth, menuItemFrmHeight, menuItemFrmWidth, menuItemFrmDest, gGameMouseActionPickFrmWidth);

    artUnlock(arrowFrmHandle);
    artUnlock(menuItemFrmHandle);

    return 0;
}

// 0x44D200
int _gmouse_3d_pick_frame_hot(int* a1, int* a2)
{
    *a1 = _gmouse_3d_pick_frame_hot_x;
    *a2 = _gmouse_3d_pick_frame_hot_y;
    return 0;
}

// 0x44D214
int gameMouseRenderActionMenuItems(int x, int y, const int* menuItems, int menuItemsLength, int width, int height)
{
    _gmouse_3d_menu_actions_start = NULL;
    gGameMouseActionMenuHighlightedItemIndex = 0;
    gGameMouseActionMenuItemsLength = 0;

    if (menuItems == NULL) {
        return -1;
    }

    if (menuItemsLength == 0 || menuItemsLength >= GAME_MOUSE_ACTION_MENU_ITEM_COUNT) {
        return -1;
    }

    CacheEntry* menuItemFrmHandles[GAME_MOUSE_ACTION_MENU_ITEM_COUNT];
    Art* menuItemFrms[GAME_MOUSE_ACTION_MENU_ITEM_COUNT];

    for (int index = 0; index < menuItemsLength; index++) {
        int frmId = gGameMouseActionMenuItemFrmIds[menuItems[index]] & 0xFFFF;
        if (index == 0) {
            frmId -= 1;
        }

        int fid = buildFid(OBJ_TYPE_INTERFACE, frmId, 0, 0, 0);

        menuItemFrms[index] = artLock(fid, &(menuItemFrmHandles[index]));
        if (menuItemFrms[index] == NULL) {
            while (--index >= 0) {
                artUnlock(menuItemFrmHandles[index]);
            }
            return -1;
        }
    }

    int fid = buildFid(OBJ_TYPE_INTERFACE, gGameMouseModeFrmIds[GAME_MOUSE_MODE_ARROW], 0, 0, 0);
    CacheEntry* arrowFrmHandle;
    Art* arrowFrm = artLock(fid, &arrowFrmHandle);
    if (arrowFrm == NULL) {
        // FIXME: Unlock arts.
        return -1;
    }

    int arrowWidth = artGetWidth(arrowFrm, 0, 0);
    int arrowHeight = artGetHeight(arrowFrm, 0, 0);

    int menuItemWidth = artGetWidth(menuItemFrms[0], 0, 0);
    int menuItemHeight = artGetHeight(menuItemFrms[0], 0, 0);

    _gmouse_3d_menu_frame_hot_x = 0;
    _gmouse_3d_menu_frame_hot_y = 0;

    gGameMouseActionMenuFrm->xOffsets[0] = gGameMouseActionMenuFrmWidth / 2;
    gGameMouseActionMenuFrm->yOffsets[0] = gGameMouseActionMenuFrmHeight - 1;

    int v60 = y + menuItemsLength * menuItemHeight - 1;
    int v24 = v60 - height + 2;
    unsigned char* v22 = gGameMouseActionMenuFrmData;
    unsigned char* v58 = v22;

    unsigned char* arrowData;
    if (x + arrowWidth + menuItemWidth - 1 < width) {
        arrowData = artGetFrameData(arrowFrm, 0, 0);
        v58 = v22 + arrowWidth;
        if (height <= v60) {
            _gmouse_3d_menu_frame_hot_y += v24;
            v22 += gGameMouseActionMenuFrmWidth * v24;
            gGameMouseActionMenuFrm->yOffsets[0] -= v24;
        }
    } else {
        // Mirrored arrow (from left to right).
        fid = buildFid(OBJ_TYPE_INTERFACE, 285, 0, 0, 0);
        arrowFrm = artLock(fid, &arrowFrmHandle);
        arrowData = artGetFrameData(arrowFrm, 0, 0);
        gGameMouseActionMenuFrm->xOffsets[0] = -gGameMouseActionMenuFrm->xOffsets[0];
        _gmouse_3d_menu_frame_hot_x += menuItemWidth + arrowWidth;
        if (v60 >= height) {
            _gmouse_3d_menu_frame_hot_y += v24;
            gGameMouseActionMenuFrm->yOffsets[0] -= v24;
            v22 += gGameMouseActionMenuFrmWidth * v24;
        }
    }

    memset(gGameMouseActionMenuFrmData, 0, gGameMouseActionMenuFrmDataSize);
    blitBufferToBuffer(arrowData, arrowWidth, arrowHeight, arrowWidth, v22, gGameMouseActionPickFrmWidth);

    unsigned char* v38 = v58;
    for (int index = 0; index < menuItemsLength; index++) {
        unsigned char* data = artGetFrameData(menuItemFrms[index], 0, 0);
        blitBufferToBuffer(data, menuItemWidth, menuItemHeight, menuItemWidth, v38, gGameMouseActionPickFrmWidth);
        v38 += gGameMouseActionMenuFrmWidth * menuItemHeight;
    }

    artUnlock(arrowFrmHandle);

    for (int index = 0; index < menuItemsLength; index++) {
        artUnlock(menuItemFrmHandles[index]);
    }

    memcpy(gGameMouseActionMenuItems, menuItems, sizeof(*gGameMouseActionMenuItems) * menuItemsLength);
    gGameMouseActionMenuItemsLength = menuItemsLength;
    _gmouse_3d_menu_actions_start = v58;

    Sound* sound = soundEffectLoad("iaccuxx1", NULL);
    if (sound != NULL) {
        soundEffectPlay(sound);
    }

    return 0;
}

// 0x44D630
int gameMouseHighlightActionMenuItemAtIndex(int menuItemIndex)
{
    if (menuItemIndex < 0 || menuItemIndex >= gGameMouseActionMenuItemsLength) {
        return -1;
    }

    CacheEntry* handle;
    int fid = buildFid(OBJ_TYPE_INTERFACE, gGameMouseActionMenuItemFrmIds[gGameMouseActionMenuItems[gGameMouseActionMenuHighlightedItemIndex]], 0, 0, 0);
    Art* art = artLock(fid, &handle);
    if (art == NULL) {
        return -1;
    }

    int width = artGetWidth(art, 0, 0);
    int height = artGetHeight(art, 0, 0);
    unsigned char* data = artGetFrameData(art, 0, 0);
    blitBufferToBuffer(data, width, height, width, _gmouse_3d_menu_actions_start + gGameMouseActionMenuFrmWidth * height * gGameMouseActionMenuHighlightedItemIndex, gGameMouseActionMenuFrmWidth);
    artUnlock(handle);

    fid = buildFid(OBJ_TYPE_INTERFACE, gGameMouseActionMenuItemFrmIds[gGameMouseActionMenuItems[menuItemIndex]] - 1, 0, 0, 0);
    art = artLock(fid, &handle);
    if (art == NULL) {
        return -1;
    }

    data = artGetFrameData(art, 0, 0);
    blitBufferToBuffer(data, width, height, width, _gmouse_3d_menu_actions_start + gGameMouseActionMenuFrmWidth * height * menuItemIndex, gGameMouseActionMenuFrmWidth);
    artUnlock(handle);

    gGameMouseActionMenuHighlightedItemIndex = menuItemIndex;

    return 0;
}

// 0x44D774
int gameMouseRenderAccuracy(const char* string, int color)
{
    CacheEntry* crosshairFrmHandle;
    int fid = buildFid(OBJ_TYPE_INTERFACE, gGameMouseModeFrmIds[GAME_MOUSE_MODE_CROSSHAIR], 0, 0, 0);
    Art* crosshairFrm = artLock(fid, &crosshairFrmHandle);
    if (crosshairFrm == NULL) {
        return -1;
    }

    memset(gGameMouseActionHitFrmData, 0, gGameMouseActionHitFrmDataSize);

    int crosshairFrmWidth = artGetWidth(crosshairFrm, 0, 0);
    int crosshairFrmHeight = artGetHeight(crosshairFrm, 0, 0);
    unsigned char* crosshairFrmData = artGetFrameData(crosshairFrm, 0, 0);
    blitBufferToBuffer(crosshairFrmData,
        crosshairFrmWidth,
        crosshairFrmHeight,
        crosshairFrmWidth,
        gGameMouseActionHitFrmData,
        gGameMouseActionHitFrmWidth);

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    fontDrawText(gGameMouseActionHitFrmData + gGameMouseActionHitFrmWidth + crosshairFrmWidth + 1,
        string,
        gGameMouseActionHitFrmWidth - crosshairFrmWidth,
        gGameMouseActionHitFrmWidth,
        color);

    bufferOutline(gGameMouseActionHitFrmData + crosshairFrmWidth,
        gGameMouseActionHitFrmWidth - crosshairFrmWidth,
        gGameMouseActionHitFrmHeight,
        gGameMouseActionHitFrmWidth,
        _colorTable[0]);

    fontSetCurrent(oldFont);

    artUnlock(crosshairFrmHandle);

    return 0;
}

// 0x44D878
int gameMouseRenderActionPoints(const char* string, int color)
{
    memset(gGameMouseHexCursorFrmData, 0, gGameMouseHexCursorFrmWidth * gGameMouseHexCursorHeight);

    if (*string == '\0') {
        return 0;
    }

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    int length = fontGetStringWidth(string);
    fontDrawText(gGameMouseHexCursorFrmData + gGameMouseHexCursorFrmWidth * (gGameMouseHexCursorHeight - fontGetLineHeight()) / 2 + (gGameMouseHexCursorFrmWidth - length) / 2, string, gGameMouseHexCursorFrmWidth, gGameMouseHexCursorFrmWidth, color);

    bufferOutline(gGameMouseHexCursorFrmData, gGameMouseHexCursorFrmWidth, gGameMouseHexCursorHeight, gGameMouseHexCursorFrmWidth, _colorTable[0]);

    fontSetCurrent(oldFont);

    int fid = buildFid(OBJ_TYPE_INTERFACE, 1, 0, 0, 0);
    gameMouseSetBouncingCursorFid(fid);

    return 0;
}

// 0x44D954
void gameMouseLoadItemHighlight()
{
    gGameMouseItemHighlightEnabled = settings.preferences.item_highlight;
}

// 0x44D984
int gameMouseObjectsInit()
{
    int fid;

    if (gGameMouseObjectsInitialized) {
        return -1;
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 0, 0, 0, 0);
    if (objectCreateWithFidPid(&gGameMouseBouncingCursor, fid, -1) != 0) {
        return -1;
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 1, 0, 0, 0);
    if (objectCreateWithFidPid(&gGameMouseHexCursor, fid, -1) != 0) {
        return -1;
    }

    if (objectSetOutline(gGameMouseHexCursor, OUTLINE_PALETTED | OUTLINE_TYPE_2, NULL) != 0) {
        return -1;
    }

    if (gameMouseActionMenuInit() != 0) {
        return -1;
    }

    gGameMouseBouncingCursor->flags |= OBJECT_LIGHT_THRU;
    gGameMouseBouncingCursor->flags |= OBJECT_NO_SAVE;
    gGameMouseBouncingCursor->flags |= OBJECT_NO_REMOVE;
    gGameMouseBouncingCursor->flags |= OBJECT_SHOOT_THRU;
    gGameMouseBouncingCursor->flags |= OBJECT_NO_BLOCK;

    gGameMouseHexCursor->flags |= OBJECT_NO_REMOVE;
    gGameMouseHexCursor->flags |= OBJECT_NO_SAVE;
    gGameMouseHexCursor->flags |= OBJECT_LIGHT_THRU;
    gGameMouseHexCursor->flags |= OBJECT_SHOOT_THRU;
    gGameMouseHexCursor->flags |= OBJECT_NO_BLOCK;

    _obj_toggle_flat(gGameMouseHexCursor, NULL);

    int x;
    int y;
    mouseGetPosition(&x, &y);

    Rect v9;
    _gmouse_3d_move_to(x, y, gElevation, &v9);

    gGameMouseObjectsInitialized = true;

    gameMouseLoadItemHighlight();

    return 0;
}

// NOTE: Inlined.
//
// 0x44DAC0
int gameMouseObjectsReset()
{
    if (!gGameMouseObjectsInitialized) {
        return -1;
    }

    // NOTE: Uninline.
    _gmouse_3d_enable_modes();

    // NOTE: Uninline.
    gameMouseResetBouncingCursorFid();

    gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
    gameMouseObjectsShow();

    gGameMouseLastX = -1;
    gGameMouseLastY = -1;
    _gmouse_3d_hover_test = false;
    _gmouse_3d_last_move_time = getTicks();
    gameMouseLoadItemHighlight();

    return 0;
}

// NOTE: Inlined.
//
// 0x44DB34
void gameMouseObjectsFree()
{
    if (gGameMouseObjectsInitialized) {
        gameMouseActionMenuFree();

        gGameMouseBouncingCursor->flags &= ~OBJECT_NO_SAVE;
        gGameMouseHexCursor->flags &= ~OBJECT_NO_SAVE;

        objectDestroy(gGameMouseBouncingCursor, NULL);
        objectDestroy(gGameMouseHexCursor, NULL);

        gGameMouseObjectsInitialized = false;
    }
}

// 0x44DB78
int gameMouseActionMenuInit()
{
    int fid;

    // actmenu.frm - action menu
    fid = buildFid(OBJ_TYPE_INTERFACE, 283, 0, 0, 0);
    gGameMouseActionMenuFrm = artLock(fid, &gGameMouseActionMenuFrmHandle);
    if (gGameMouseActionMenuFrm == NULL) {
        goto err;
    }

    // actpick.frm - action pick
    fid = buildFid(OBJ_TYPE_INTERFACE, 282, 0, 0, 0);
    gGameMouseActionPickFrm = artLock(fid, &gGameMouseActionPickFrmHandle);
    if (gGameMouseActionPickFrm == NULL) {
        goto err;
    }

    // acttohit.frm - action to hit
    fid = buildFid(OBJ_TYPE_INTERFACE, 284, 0, 0, 0);
    gGameMouseActionHitFrm = artLock(fid, &gGameMouseActionHitFrmHandle);
    if (gGameMouseActionHitFrm == NULL) {
        goto err;
    }

    // blank.frm - used be mset000.frm for top of bouncing mouse cursor
    fid = buildFid(OBJ_TYPE_INTERFACE, 0, 0, 0, 0);
    gGameMouseBouncingCursorFrm = artLock(fid, &gGameMouseBouncingCursorFrmHandle);
    if (gGameMouseBouncingCursorFrm == NULL) {
        goto err;
    }

    // msef000.frm - hex mouse cursor
    fid = buildFid(OBJ_TYPE_INTERFACE, 1, 0, 0, 0);
    gGameMouseHexCursorFrm = artLock(fid, &gGameMouseHexCursorFrmHandle);
    if (gGameMouseHexCursorFrm == NULL) {
        goto err;
    }

    gGameMouseActionMenuFrmWidth = artGetWidth(gGameMouseActionMenuFrm, 0, 0);
    gGameMouseActionMenuFrmHeight = artGetHeight(gGameMouseActionMenuFrm, 0, 0);
    gGameMouseActionMenuFrmDataSize = gGameMouseActionMenuFrmWidth * gGameMouseActionMenuFrmHeight;
    gGameMouseActionMenuFrmData = artGetFrameData(gGameMouseActionMenuFrm, 0, 0);

    gGameMouseActionPickFrmWidth = artGetWidth(gGameMouseActionPickFrm, 0, 0);
    gGameMouseActionPickFrmHeight = artGetHeight(gGameMouseActionPickFrm, 0, 0);
    gGameMouseActionPickFrmDataSize = gGameMouseActionPickFrmWidth * gGameMouseActionPickFrmHeight;
    gGameMouseActionPickFrmData = artGetFrameData(gGameMouseActionPickFrm, 0, 0);

    gGameMouseActionHitFrmWidth = artGetWidth(gGameMouseActionHitFrm, 0, 0);
    gGameMouseActionHitFrmHeight = artGetHeight(gGameMouseActionHitFrm, 0, 0);
    gGameMouseActionHitFrmDataSize = gGameMouseActionHitFrmWidth * gGameMouseActionHitFrmHeight;
    gGameMouseActionHitFrmData = artGetFrameData(gGameMouseActionHitFrm, 0, 0);

    gGameMouseBouncingCursorFrmWidth = artGetWidth(gGameMouseBouncingCursorFrm, 0, 0);
    gGameMouseBouncingCursorFrmHeight = artGetHeight(gGameMouseBouncingCursorFrm, 0, 0);
    gGameMouseBouncingCursorFrmDataSize = gGameMouseBouncingCursorFrmWidth * gGameMouseBouncingCursorFrmHeight;
    gGameMouseBouncingCursorFrmData = artGetFrameData(gGameMouseBouncingCursorFrm, 0, 0);

    gGameMouseHexCursorFrmWidth = artGetWidth(gGameMouseHexCursorFrm, 0, 0);
    gGameMouseHexCursorHeight = artGetHeight(gGameMouseHexCursorFrm, 0, 0);
    gGameMouseHexCursorDataSize = gGameMouseHexCursorFrmWidth * gGameMouseHexCursorHeight;
    gGameMouseHexCursorFrmData = artGetFrameData(gGameMouseHexCursorFrm, 0, 0);

    return 0;

err:

    // NOTE: Original code is different. There is no call to this function.
    // Instead it either use deep nesting or bunch of goto's to unwind
    // locked frms from the point of failure.
    gameMouseActionMenuFree();

    return -1;
}

// 0x44DE44
void gameMouseActionMenuFree()
{
    if (gGameMouseBouncingCursorFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseBouncingCursorFrmHandle);
    }
    gGameMouseBouncingCursorFrm = NULL;
    gGameMouseBouncingCursorFrmHandle = INVALID_CACHE_ENTRY;

    if (gGameMouseHexCursorFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseHexCursorFrmHandle);
    }
    gGameMouseHexCursorFrm = NULL;
    gGameMouseHexCursorFrmHandle = INVALID_CACHE_ENTRY;

    if (gGameMouseActionHitFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseActionHitFrmHandle);
    }
    gGameMouseActionHitFrm = NULL;
    gGameMouseActionHitFrmHandle = INVALID_CACHE_ENTRY;

    if (gGameMouseActionMenuFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseActionMenuFrmHandle);
    }
    gGameMouseActionMenuFrm = NULL;
    gGameMouseActionMenuFrmHandle = INVALID_CACHE_ENTRY;

    if (gGameMouseActionPickFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gGameMouseActionPickFrmHandle);
    }

    gGameMouseActionPickFrm = NULL;
    gGameMouseActionPickFrmHandle = INVALID_CACHE_ENTRY;

    gGameMouseActionPickFrmData = NULL;
    gGameMouseActionPickFrmWidth = 0;
    gGameMouseActionPickFrmHeight = 0;
    gGameMouseActionPickFrmDataSize = 0;
}

// NOTE: Inlined.
//
// 0x44DF1C
static int gmouse_3d_set_flat_fid(int fid, Rect* rect)
{
    if (objectSetFid(gGameMouseHexCursor, fid, rect) == 0) {
        return 0;
    }

    return -1;
}

// 0x44DF40
int gameMouseUpdateHexCursorFid(Rect* rect)
{
    int fid = buildFid(OBJ_TYPE_INTERFACE, gGameMouseModeFrmIds[gGameMouseMode], 0, 0, 0);
    if (gGameMouseHexCursor->fid == fid) {
        return -1;
    }

    // NOTE: Uninline.
    return gmouse_3d_set_flat_fid(fid, rect);
}

// 0x44DF94
int _gmouse_3d_move_to(int x, int y, int elevation, Rect* a4)
{
    if (_gmouse_mapper_mode == 0) {
        if (gGameMouseMode != GAME_MOUSE_MODE_MOVE) {
            int offsetX = 0;
            int offsetY = 0;
            CacheEntry* hexCursorFrmHandle;
            Art* hexCursorFrm = artLock(gGameMouseHexCursor->fid, &hexCursorFrmHandle);
            if (hexCursorFrm != NULL) {
                artGetRotationOffsets(hexCursorFrm, 0, &offsetX, &offsetY);

                int frameOffsetX;
                int frameOffsetY;
                artGetFrameOffsets(hexCursorFrm, 0, 0, &frameOffsetX, &frameOffsetY);

                offsetX += frameOffsetX;
                offsetY += frameOffsetY;

                artUnlock(hexCursorFrmHandle);
            }

            _obj_move(gGameMouseHexCursor, x + offsetX, y + offsetY, elevation, a4);
        } else {
            int tile = tileFromScreenXY(x, y, 0);
            if (tile != -1) {
                int screenX;
                int screenY;

                bool v1 = false;
                Rect rect1;
                if (tileToScreenXY(tile, &screenX, &screenY, 0) == 0) {
                    if (_obj_move(gGameMouseBouncingCursor, screenX + 16, screenY + 15, 0, &rect1) == 0) {
                        v1 = true;
                    }
                }

                Rect rect2;
                if (objectSetLocation(gGameMouseHexCursor, tile, elevation, &rect2) == 0) {
                    if (v1) {
                        rectUnion(&rect1, &rect2, &rect1);
                    } else {
                        rectCopy(&rect1, &rect2);
                    }

                    rectCopy(a4, &rect1);
                }
            }
        }
        return 0;
    }

    int tile;
    int x1 = 0;
    int y1 = 0;

    int fid = gGameMouseBouncingCursor->fid;
    if (FID_TYPE(fid) == OBJ_TYPE_TILE) {
        int squareTile = squareTileFromScreenXY(x, y, elevation);
        if (squareTile == -1) {
            tile = HEX_GRID_WIDTH * (2 * (squareTile / SQUARE_GRID_WIDTH) + 1) + 2 * (squareTile % SQUARE_GRID_WIDTH) + 1;
            x1 = -8;
            y1 = 13;

            if (compat_stricmp(settings.system.executable.c_str(), "mapper") == 0) {
                if (tileRoofIsVisible()) {
                    if ((gDude->flags & OBJECT_HIDDEN) == 0) {
                        y1 = -83;
                    }
                }
            }
        } else {
            tile = -1;
        }
    } else {
        tile = tileFromScreenXY(x, y, elevation);
    }

    if (tile != -1) {
        bool v1 = false;

        Rect rect1;
        Rect rect2;

        if (objectSetLocation(gGameMouseBouncingCursor, tile, elevation, &rect1) == 0) {
            if (x1 != 0 || y1 != 0) {
                if (_obj_offset(gGameMouseBouncingCursor, x1, y1, &rect2) == 0) {
                    rectUnion(&rect1, &rect2, &rect1);
                }
            }
            v1 = true;
        }

        if (gGameMouseMode != GAME_MOUSE_MODE_MOVE) {
            int offsetX = 0;
            int offsetY = 0;
            CacheEntry* hexCursorFrmHandle;
            Art* hexCursorFrm = artLock(gGameMouseHexCursor->fid, &hexCursorFrmHandle);
            if (hexCursorFrm != NULL) {
                artGetRotationOffsets(hexCursorFrm, 0, &offsetX, &offsetY);

                int frameOffsetX;
                int frameOffsetY;
                artGetFrameOffsets(hexCursorFrm, 0, 0, &frameOffsetX, &frameOffsetY);

                offsetX += frameOffsetX;
                offsetY += frameOffsetY;

                artUnlock(hexCursorFrmHandle);
            }

            if (_obj_move(gGameMouseHexCursor, x + offsetX, y + offsetY, elevation, &rect2) == 0) {
                if (v1) {
                    rectUnion(&rect1, &rect2, &rect1);
                } else {
                    rectCopy(&rect1, &rect2);
                    v1 = true;
                }
            }
        } else {
            if (objectSetLocation(gGameMouseHexCursor, tile, elevation, &rect2) == 0) {
                if (v1) {
                    rectUnion(&rect1, &rect2, &rect1);
                } else {
                    rectCopy(&rect1, &rect2);
                    v1 = true;
                }
            }
        }

        if (v1) {
            rectCopy(a4, &rect1);
        }
    }

    return 0;
}

// 0x44E42C
int gameMouseHandleScrolling(int x, int y, int cursor)
{
    if (!_gmouse_scrolling_enabled) {
        return -1;
    }

    int flags = 0;

    if (x <= _scr_size.left) {
        flags |= SCROLLABLE_W;
    }

    if (x >= _scr_size.right) {
        flags |= SCROLLABLE_E;
    }

    if (y <= _scr_size.top) {
        flags |= SCROLLABLE_N;
    }

    if (y >= _scr_size.bottom) {
        flags |= SCROLLABLE_S;
    }

    int dx = 0;
    int dy = 0;

    switch (flags) {
    case SCROLLABLE_W:
        dx = -1;
        cursor = MOUSE_CURSOR_SCROLL_W;
        break;
    case SCROLLABLE_E:
        dx = 1;
        cursor = MOUSE_CURSOR_SCROLL_E;
        break;
    case SCROLLABLE_N:
        dy = -1;
        cursor = MOUSE_CURSOR_SCROLL_N;
        break;
    case SCROLLABLE_N | SCROLLABLE_W:
        dx = -1;
        dy = -1;
        cursor = MOUSE_CURSOR_SCROLL_NW;
        break;
    case SCROLLABLE_N | SCROLLABLE_E:
        dx = 1;
        dy = -1;
        cursor = MOUSE_CURSOR_SCROLL_NE;
        break;
    case SCROLLABLE_S:
        dy = 1;
        cursor = MOUSE_CURSOR_SCROLL_S;
        break;
    case SCROLLABLE_S | SCROLLABLE_W:
        dx = -1;
        dy = 1;
        cursor = MOUSE_CURSOR_SCROLL_SW;
        break;
    case SCROLLABLE_S | SCROLLABLE_E:
        dx = 1;
        dy = 1;
        cursor = MOUSE_CURSOR_SCROLL_SE;
        break;
    }

    if (dx == 0 && dy == 0) {
        return -1;
    }

    int rc = mapScroll(dx, dy);
    switch (rc) {
    case -1:
        // Scrolling is blocked for whatever reason, upgrade cursor to
        // appropriate blocked version.
        cursor += 8;
        // FALLTHROUGH
    case 0:
        gameMouseSetCursor(cursor);
        break;
    }

    return 0;
}

// 0x44E544
void _gmouse_remove_item_outline(Object* object)
{
    if (gGameMouseHighlightedItem != NULL && gGameMouseHighlightedItem == object) {
        Rect rect;
        if (objectClearOutline(object, &rect) == 0) {
            tileWindowRefreshRect(&rect, gElevation);
        }
        gGameMouseHighlightedItem = NULL;
    }
}

// 0x44E580
int objectIsDoor(Object* object)
{
    if (object == NULL) {
        return false;
    }

    if (PID_TYPE(object->pid) != OBJ_TYPE_SCENERY) {
        return false;
    }

    Proto* proto;
    if (protoGetProto(object->pid, &proto) == -1) {
        return false;
    }

    return proto->scenery.type == SCENERY_TYPE_DOOR;
}

static void customMouseModeFrmsInit()
{
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_USE_FIRST_AID_FRM_KEY, &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_FIRST_AID]));
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_USE_DOCTOR_FRM_KEY, &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_DOCTOR]));
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_USE_LOCKPICK_FRM_KEY, &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_LOCKPICK]));
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_USE_STEAL_FRM_KEY, &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_STEAL]));
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_USE_TRAPS_FRM_KEY, &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_TRAPS]));
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_USE_SCIENCE_FRM_KEY, &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_SCIENCE]));
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_USE_REPAIR_FRM_KEY, &(gGameMouseModeFrmIds[GAME_MOUSE_MODE_USE_REPAIR]));
}

void gameMouseRefreshImmediately()
{
    gameMouseRefresh();
    renderPresent();
}

Object* gmouse_get_outlined_object()
{
    return gGameMouseHighlightedItem;
}

} // namespace fallout
