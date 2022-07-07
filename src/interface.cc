#include "interface.h"

#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "config.h"
#include "core.h"
#include "critter.h"
#include "cycle.h"
#include "debug.h"
#include "display_monitor.h"
#include "draw.h"
#include "endgame.h"
#include "game.h"
#include "game_config.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "geometry.h"
#include "item.h"
#include "memory.h"
#include "object.h"
#include "platform_compat.h"
#include "proto.h"
#include "proto_instance.h"
#include "proto_types.h"
#include "skill.h"
#include "stat.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"

#include <stdio.h>
#include <string.h>

// The width of connectors in the indicator box.
//
// There are male connectors on the left, and female connectors on the right.
// When displaying series of boxes they appear to be plugged into a chain.
#define INDICATOR_BOX_CONNECTOR_WIDTH 3

// Minimum radiation amount to display RADIATED indicator.
#define RADATION_INDICATOR_THRESHOLD 65

// Minimum poison amount to display POISONED indicator.
#define POISON_INDICATOR_THRESHOLD 0

// The maximum number of indicator boxes the indicator bar can display.
//
// For unknown reason this number is 6, even though there are only 5 different
// indicator types. In addition to that, default screen width 640px cannot hold
// 6 boxes 130px each.
#define INDICATOR_SLOTS_COUNT (6)

// The values of it's members are offsets to beginning of numbers in
// numbers.frm.
typedef enum InterfaceNumbersColor {
    INTERFACE_NUMBERS_COLOR_WHITE = 0,
    INTERFACE_NUMBERS_COLOR_YELLOW = 120,
    INTERFACE_NUMBERS_COLOR_RED = 240,
} InterfaceNumbersColor;

// Available indicators.
//
// Indicator boxes in the bar are displayed according to the order of this enum.
typedef enum Indicator {
    INDICATOR_ADDICT,
    INDICATOR_SNEAK,
    INDICATOR_LEVEL,
    INDICATOR_POISONED,
    INDICATOR_RADIATED,
    INDICATOR_COUNT,
} Indicator;

// Provides metadata about indicator boxes.
typedef struct IndicatorDescription {
    // An identifier of title in `intrface.msg`.
    int title;

    // A flag denoting this box represents something harmful to the player. It
    // affects color of the title.
    bool isBad;

    // Prerendered indicator data.
    //
    // This value is provided at runtime during indicator box initialization.
    // It includes indicator box background with it's title positioned in the
    // center and is green colored if indicator is good, or red otherwise, as
    // denoted by [isBad] property.
    unsigned char* data;
} IndicatorDescription;

typedef struct InterfaceItemState {
    Object* item;
    unsigned char isDisabled;
    unsigned char isWeapon;
    int primaryHitMode;
    int secondaryHitMode;
    int action;
    int itemFid;
} InterfaceItemState;

static int _intface_redraw_items_callback(Object* a1, Object* a2);
static int _intface_change_fid_callback(Object* a1, Object* a2);
static void interfaceBarSwapHandsAnimatePutAwayTakeOutSequence(int previousWeaponAnimationCode, int weaponAnimationCode);
static int interfaceBarRefreshMainAction();
static int endTurnButtonInit();
static int endTurnButtonFree();
static int endCombatButtonInit();
static int endCombatButtonFree();
static void interfaceUpdateAmmoBar(int x, int ratio);
static int _intface_item_reload();
static void interfaceRenderCounter(int x, int y, int previousValue, int value, int offset, int delay);
static int indicatorBarInit();
static void interfaceBarFree();
static void indicatorBarReset();
static int indicatorBoxCompareByPosition(const void* a, const void* b);
static void indicatorBarRender(int count);
static bool indicatorBarAdd(int indicator);

// 0x518F08
static bool gInterfaceBarInitialized = false;

// 0x518F0C
static bool gInterfaceBarSwapHandsInProgress = false;

// 0x518F10
static bool gInterfaceBarEnabled = false;

// 0x518F14
static bool _intfaceHidden = false;

// 0x518F18
static int gInventoryButton = -1;

// 0x518F1C
static CacheEntry* gInventoryButtonUpFrmHandle = NULL;

// 0x518F20
static CacheEntry* gInventoryButtonDownFrmHandle = NULL;

// 0x518F24
static int gOptionsButton = -1;

// 0x518F28
static CacheEntry* gOptionsButtonUpFrmHandle = NULL;

// 0x518F2C
static CacheEntry* gOptionsButtonDownFrmHandle = NULL;

// 0x518F30
static int gSkilldexButton = -1;

// 0x518F34
static CacheEntry* gSkilldexButtonUpFrmHandle = NULL;

// 0x518F38
static CacheEntry* gSkilldexButtonDownFrmHandle = NULL;

// 0x518F3C
static CacheEntry* gSkilldexButtonMaskFrmHandle = NULL;

// 0x518F40
static int gMapButton = -1;

// 0x518F44
static CacheEntry* gMapButtonUpFrmHandle = NULL;

// 0x518F48
static CacheEntry* gMapButtonDownFrmHandle = NULL;

// 0x518F4C
static CacheEntry* gMapButtonMaskFrmHandle = NULL;

// 0x518F50
static int gPipboyButton = -1;

// 0x518F54
static CacheEntry* gPipboyButtonUpFrmHandle = NULL;

// 0x518F58
static CacheEntry* gPipboyButtonDownFrmHandle = NULL;

// 0x518F5C
static int gCharacterButton = -1;
// 0x518F60
static CacheEntry* gCharacterButtonUpFrmHandle = NULL;
// 0x518F64
static CacheEntry* gCharacterButtonDownFrmHandle = NULL;

// 0x518F68
static int gSingleAttackButton = -1;

// 0x518F6C
static CacheEntry* gSingleAttackButtonUpHandle = NULL;

// 0x518F70
static CacheEntry* gSingleAttackButtonDownHandle = NULL;

//
CacheEntry* _itemButtonDisabledKey = NULL;

// 0x518F78
static int gInterfaceCurrentHand = HAND_LEFT;

// 0x518F7C
static const Rect gInterfaceBarMainActionRect = { 267, 26, 455, 93 };

// 0x518F8C
static int gChangeHandsButton = -1;

// 0x518F90
static CacheEntry* gChangeHandsButtonUpFrmHandle = NULL;

// 0x518F94
static CacheEntry* gChangeHandsButtonDownFrmHandle = NULL;

// 0x518F98
static CacheEntry* gChangeHandsButtonMaskFrmHandle = NULL;

// 0x518F9C
static bool gInterfaceBarEndButtonsIsVisible = false;

// Combat mode curtains rect.
//
// 0x518FA0
static const Rect gInterfaceBarEndButtonsRect = { 580, 38, 637, 96 };

// 0x518FB0
static int gEndTurnButton = -1;

// 0x518FB4
static CacheEntry* gEndTurnButtonUpFrmHandle = NULL;

// 0x518FB8
static CacheEntry* gEndTurnButtonDownFrmHandle = NULL;

// 0x518FBC
static int gEndCombatButton = -1;

// 0x518FC0
static CacheEntry* gEndCombatButtonUpFrmHandle = NULL;

// 0x518FC4
static CacheEntry* gEndCombatButtonDownFrmHandle = NULL;

// 0x518FC8
static unsigned char* gGreenLightFrmData = NULL;

// 0x518FCC
static unsigned char* gYellowLightFrmData = NULL;

// 0x518FD0
static unsigned char* gRedLightFrmData = NULL;

// 0x518FD4
static const Rect gInterfaceBarActionPointsBarRect = { 316, 14, 406, 19 };

// 0x518FE4
static unsigned char* gNumbersFrmData = NULL;

// 0x518FE8
static IndicatorDescription gIndicatorDescriptions[INDICATOR_COUNT] = {
    { 102, true, NULL }, // ADDICT
    { 100, false, NULL }, // SNEAK
    { 101, false, NULL }, // LEVEL
    { 103, true, NULL }, // POISONED
    { 104, true, NULL }, // RADIATED
};

// 0x519024
int gInterfaceBarWindow = -1;

// 0x519028
static int gIndicatorBarWindow = -1;

// Last hit points rendered in interface.
//
// Used to animate changes.
//
// 0x51902C
static int gInterfaceLastRenderedHitPoints = 0;

// Last color used to render hit points in interface.
//
// Used to animate changes.
//
// 0x519030
static int gInterfaceLastRenderedHitPointsColor = INTERFACE_NUMBERS_COLOR_RED;

// Last armor class rendered in interface.
//
// Used to animate changes.
//
// 0x519034
static int gInterfaceLastRenderedArmorClass = 0;

// Each slot contains one of indicators or -1 if slot is empty.
//
// 0x5970E0
static int gIndicatorSlots[INDICATOR_SLOTS_COUNT];

// 0x5970F8
static InterfaceItemState gInterfaceItemStates[HAND_COUNT];

// 0x597128
static CacheEntry* gYellowLightFrmHandle;

// 0x59712C
static CacheEntry* gRedLightFrmHandle;

// 0x597130
static CacheEntry* gNumbersFrmHandle;

// 0x597138
static bool gIndicatorBarIsVisible;

// 0x59713C
static unsigned char* gChangeHandsButtonUpFrmData;

// 0x597140
static CacheEntry* gGreenLightFrmHandle;

// 0x597144
static unsigned char* gEndCombatButtonUpFrmData;

// 0x597148
static unsigned char* gEndCombatButtonDownFrmData;

// 0x59714C
static unsigned char* gChangeHandsButtonDownFrmData;

// 0x597150
static unsigned char* gEndTurnButtonDownFrmData;

// 0x597154
static unsigned char _itemButtonDown[188 * 67];

// 0x59A288
static unsigned char* gEndTurnButtonUpFrmData;

// 0x59A28C
static unsigned char* gChangeHandsButtonMaskFrmData;

// 0x59A290
static unsigned char* gCharacterButtonUpFrmData;

// 0x59A294
static unsigned char* gSingleAttackButtonUpData;

// 0x59A298
static unsigned char* _itemButtonDisabled;

// 0x59A29C
static unsigned char* gMapButtonDownFrmData;

// 0x59A2A0
static unsigned char* gPipboyButtonUpFrmData;

// 0x59A2A4
static unsigned char* gCharacterButtonDownFrmData;

// 0x59A2A8
static unsigned char* gSingleAttackButtonDownData;

// 0x59A2AC
static unsigned char* gPipboyButtonDownFrmData;

// 0x59A2B0
static unsigned char* gMapButtonMaskFrmData;

// 0x59A2B4
static unsigned char _itemButtonUp[188 * 67];

// 0x59D3E8
static unsigned char* gMapButtonUpFrmData;

// 0x59D3EC
static unsigned char* gSkilldexButtonMaskFrmData;

// 0x59D3F0
static unsigned char* gSkilldexButtonDownFrmData;

// 0x59D3F4
static unsigned char* gInterfaceWindowBuffer;

// 0x59D3F8
static unsigned char* gInventoryButtonUpFrmData;

// 0x59D3FC
static unsigned char* gOptionsButtonUpFrmData;

// 0x59D400
static unsigned char* gOptionsButtonDownFrmData;

// 0x59D404
static unsigned char* gSkilldexButtonUpFrmData;

// 0x59D408
static unsigned char* gInventoryButtonDownFrmData;

// A slice of main interface background containing 10 shadowed action point
// dots. In combat mode individual colored dots are rendered on top of this
// background.
//
// This buffer is initialized once and does not change throughout the game.
//
// 0x59D40C
static unsigned char gInterfaceActionPointsBarBackground[90 * 5];

// Should the game window stretch all the way to the bottom or sit at the top of the interface bar (default)
bool gInterfaceBarMode = false;

// intface_init
// 0x45D880
int interfaceInit()
{
    int fid;
    CacheEntry* backgroundFrmHandle;
    unsigned char* backgroundFrmData;

    if (gInterfaceBarWindow != -1) {
        return -1;
    }

    gInterfaceBarInitialized = 1;

    int interfaceBarWindowX = (screenGetWidth() - INTERFACE_BAR_WIDTH) / 2;
    int interfaceBarWindowY = screenGetHeight() - INTERFACE_BAR_HEIGHT;

    gInterfaceBarWindow = windowCreate(interfaceBarWindowX, interfaceBarWindowY, INTERFACE_BAR_WIDTH, INTERFACE_BAR_HEIGHT, _colorTable[0], WINDOW_HIDDEN);
    if (gInterfaceBarWindow == -1) {
        goto err;
    }

    gInterfaceWindowBuffer = windowGetBuffer(gInterfaceBarWindow);
    if (gInterfaceWindowBuffer == NULL) {
        goto err;
    }

    fid = buildFid(6, 16, 0, 0, 0);
    backgroundFrmData = artLockFrameData(fid, 0, 0, &backgroundFrmHandle);
    if (backgroundFrmData == NULL) {
        goto err;
    }

    blitBufferToBuffer(backgroundFrmData, INTERFACE_BAR_WIDTH, INTERFACE_BAR_HEIGHT - 1, INTERFACE_BAR_WIDTH, gInterfaceWindowBuffer, 640);
    artUnlock(backgroundFrmHandle);

    fid = buildFid(6, 47, 0, 0, 0);
    gInventoryButtonUpFrmData = artLockFrameData(fid, 0, 0, &gInventoryButtonUpFrmHandle);
    if (gInventoryButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 46, 0, 0, 0);
    gInventoryButtonDownFrmData = artLockFrameData(fid, 0, 0, &gInventoryButtonDownFrmHandle);
    if (gInventoryButtonDownFrmData == NULL) {
        goto err;
    }

    gInventoryButton = buttonCreate(gInterfaceBarWindow, 211, 41, 32, 21, -1, -1, -1, KEY_LOWERCASE_I, gInventoryButtonUpFrmData, gInventoryButtonDownFrmData, NULL, 0);
    if (gInventoryButton == -1) {
        goto err;
    }

    buttonSetCallbacks(gInventoryButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(6, 18, 0, 0, 0);
    gOptionsButtonUpFrmData = artLockFrameData(fid, 0, 0, &gOptionsButtonUpFrmHandle);
    if (gOptionsButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 17, 0, 0, 0);
    gOptionsButtonDownFrmData = artLockFrameData(fid, 0, 0, &gOptionsButtonDownFrmHandle);
    if (gOptionsButtonDownFrmData == NULL) {
        goto err;
    }

    gOptionsButton = buttonCreate(gInterfaceBarWindow, 210, 62, 34, 34, -1, -1, -1, KEY_LOWERCASE_O, gOptionsButtonUpFrmData, gOptionsButtonDownFrmData, NULL, 0);
    if (gOptionsButton == -1) {
        goto err;
    }

    buttonSetCallbacks(gOptionsButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(6, 6, 0, 0, 0);
    gSkilldexButtonUpFrmData = artLockFrameData(fid, 0, 0, &gSkilldexButtonUpFrmHandle);
    if (gSkilldexButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 7, 0, 0, 0);
    gSkilldexButtonDownFrmData = artLockFrameData(fid, 0, 0, &gSkilldexButtonDownFrmHandle);
    if (gSkilldexButtonDownFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 6, 0, 0, 0);
    gSkilldexButtonMaskFrmData = artLockFrameData(fid, 0, 0, &gSkilldexButtonMaskFrmHandle);
    if (gSkilldexButtonMaskFrmData == NULL) {
        goto err;
    }

    gSkilldexButton = buttonCreate(gInterfaceBarWindow, 523, 6, 22, 21, -1, -1, -1, KEY_LOWERCASE_S, gSkilldexButtonUpFrmData, gSkilldexButtonDownFrmData, NULL, BUTTON_FLAG_TRANSPARENT);
    if (gSkilldexButton == -1) {
        goto err;
    }

    buttonSetMask(gSkilldexButton, gSkilldexButtonMaskFrmData);
    buttonSetCallbacks(gSkilldexButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(6, 13, 0, 0, 0);
    gMapButtonUpFrmData = artLockFrameData(fid, 0, 0, &gMapButtonUpFrmHandle);
    if (gMapButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 10, 0, 0, 0);
    gMapButtonDownFrmData = artLockFrameData(fid, 0, 0, &gMapButtonDownFrmHandle);
    if (gMapButtonDownFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 13, 0, 0, 0);
    gMapButtonMaskFrmData = artLockFrameData(fid, 0, 0, &gMapButtonMaskFrmHandle);
    if (gMapButtonMaskFrmData == NULL) {
        goto err;
    }

    gMapButton = buttonCreate(gInterfaceBarWindow, 526, 40, 41, 19, -1, -1, -1, KEY_TAB, gMapButtonUpFrmData, gMapButtonDownFrmData, NULL, BUTTON_FLAG_TRANSPARENT);
    if (gMapButton == -1) {
        goto err;
    }

    buttonSetMask(gMapButton, gMapButtonMaskFrmData);
    buttonSetCallbacks(gMapButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(6, 59, 0, 0, 0);
    gPipboyButtonUpFrmData = artLockFrameData(fid, 0, 0, &gPipboyButtonUpFrmHandle);
    if (gPipboyButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 58, 0, 0, 0);
    gPipboyButtonDownFrmData = artLockFrameData(fid, 0, 0, &gPipboyButtonDownFrmHandle);
    if (gPipboyButtonDownFrmData == NULL) {
        goto err;
    }

    gPipboyButton = buttonCreate(gInterfaceBarWindow, 526, 78, 41, 19, -1, -1, -1, KEY_LOWERCASE_P, gPipboyButtonUpFrmData, gPipboyButtonDownFrmData, NULL, 0);
    if (gPipboyButton == -1) {
        goto err;
    }

    buttonSetMask(gPipboyButton, gMapButtonMaskFrmData);
    buttonSetCallbacks(gPipboyButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(6, 57, 0, 0, 0);
    gCharacterButtonUpFrmData = artLockFrameData(fid, 0, 0, &gCharacterButtonUpFrmHandle);
    if (gCharacterButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 56, 0, 0, 0);
    gCharacterButtonDownFrmData = artLockFrameData(fid, 0, 0, &gCharacterButtonDownFrmHandle);
    if (gCharacterButtonDownFrmData == NULL) {
        goto err;
    }

    gCharacterButton = buttonCreate(gInterfaceBarWindow, 526, 59, 41, 19, -1, -1, -1, KEY_LOWERCASE_C, gCharacterButtonUpFrmData, gCharacterButtonDownFrmData, NULL, 0);
    if (gCharacterButton == -1) {
        goto err;
    }

    buttonSetMask(gCharacterButton, gMapButtonMaskFrmData);
    buttonSetCallbacks(gCharacterButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(6, 32, 0, 0, 0);
    gSingleAttackButtonUpData = artLockFrameData(fid, 0, 0, &gSingleAttackButtonUpHandle);
    if (gSingleAttackButtonUpData == NULL) {
        goto err;
    }

    fid = buildFid(6, 31, 0, 0, 0);
    gSingleAttackButtonDownData = artLockFrameData(fid, 0, 0, &gSingleAttackButtonDownHandle);
    if (gSingleAttackButtonDownData == NULL) {
        goto err;
    }

    fid = buildFid(6, 73, 0, 0, 0);
    _itemButtonDisabled = artLockFrameData(fid, 0, 0, &_itemButtonDisabledKey);
    if (_itemButtonDisabled == NULL) {
        goto err;
    }

    memcpy(_itemButtonUp, gSingleAttackButtonUpData, sizeof(_itemButtonUp));
    memcpy(_itemButtonDown, gSingleAttackButtonDownData, sizeof(_itemButtonDown));

    gSingleAttackButton = buttonCreate(gInterfaceBarWindow, 267, 26, 188, 67, -1, -1, -1, -20, _itemButtonUp, _itemButtonDown, NULL, BUTTON_FLAG_TRANSPARENT);
    if (gSingleAttackButton == -1) {
        goto err;
    }

    buttonSetRightMouseCallbacks(gSingleAttackButton, -1, KEY_LOWERCASE_N, NULL, NULL);
    buttonSetCallbacks(gSingleAttackButton, _gsound_lrg_butt_press, _gsound_lrg_butt_release);

    fid = buildFid(6, 6, 0, 0, 0);
    gChangeHandsButtonUpFrmData = artLockFrameData(fid, 0, 0, &gChangeHandsButtonUpFrmHandle);
    if (gChangeHandsButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 7, 0, 0, 0);
    gChangeHandsButtonDownFrmData = artLockFrameData(fid, 0, 0, &gChangeHandsButtonDownFrmHandle);
    if (gChangeHandsButtonDownFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 6, 0, 0, 0);
    gChangeHandsButtonMaskFrmData = artLockFrameData(fid, 0, 0, &gChangeHandsButtonMaskFrmHandle);
    if (gChangeHandsButtonMaskFrmData == NULL) {
        goto err;
    }

    // Swap hands button
    gChangeHandsButton = buttonCreate(gInterfaceBarWindow, 218, 6, 22, 21, -1, -1, -1, KEY_LOWERCASE_B, gChangeHandsButtonUpFrmData, gChangeHandsButtonDownFrmData, NULL, BUTTON_FLAG_TRANSPARENT);
    if (gChangeHandsButton == -1) {
        goto err;
    }

    buttonSetMask(gChangeHandsButton, gChangeHandsButtonMaskFrmData);
    buttonSetCallbacks(gChangeHandsButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(6, 82, 0, 0, 0);
    gNumbersFrmData = artLockFrameData(fid, 0, 0, &gNumbersFrmHandle);
    if (gNumbersFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 83, 0, 0, 0);
    gGreenLightFrmData = artLockFrameData(fid, 0, 0, &gGreenLightFrmHandle);
    if (gGreenLightFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 84, 0, 0, 0);
    gYellowLightFrmData = artLockFrameData(fid, 0, 0, &gYellowLightFrmHandle);
    if (gYellowLightFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 85, 0, 0, 0);
    gRedLightFrmData = artLockFrameData(fid, 0, 0, &gRedLightFrmHandle);
    if (gRedLightFrmData == NULL) {
        goto err;
    }

    blitBufferToBuffer(gInterfaceWindowBuffer + 640 * 14 + 316, 90, 5, 640, gInterfaceActionPointsBarBackground, 90);

    if (indicatorBarInit() == -1) {
        goto err;
    }

    gInterfaceCurrentHand = HAND_LEFT;

    // FIXME: For unknown reason these values initialized with -1. It's never
    // checked for -1, so I have no explanation for this.
    gInterfaceItemStates[HAND_LEFT].item = (Object*)-1;
    gInterfaceItemStates[HAND_RIGHT].item = (Object*)-1;

    displayMonitorInit();

    gInterfaceBarEnabled = true;
    gInterfaceBarInitialized = false;
    _intfaceHidden = 1;

    return 0;

err:

    interfaceFree();

    return -1;
}

// 0x45E3D0
void interfaceReset()
{
    interfaceBarEnable();

    if (gInterfaceBarWindow != -1 && !_intfaceHidden) {
        windowHide(gInterfaceBarWindow);
        _intfaceHidden = 1;
    }

    indicatorBarRefresh();
    displayMonitorReset();

    // NOTE: Uninline a seemingly inlined routine.
    indicatorBarReset();

    gInterfaceCurrentHand = 0;
}

// 0x45E440
void interfaceFree()
{
    if (gInterfaceBarWindow != -1) {
        displayMonitorExit();

        if (gRedLightFrmData != NULL) {
            artUnlock(gRedLightFrmHandle);
            gRedLightFrmData = NULL;
        }

        if (gYellowLightFrmData != NULL) {
            artUnlock(gYellowLightFrmHandle);
            gYellowLightFrmData = NULL;
        }

        if (gGreenLightFrmData != NULL) {
            artUnlock(gGreenLightFrmHandle);
            gGreenLightFrmData = NULL;
        }

        if (gNumbersFrmData != NULL) {
            artUnlock(gNumbersFrmHandle);
            gNumbersFrmData = NULL;
        }

        if (gChangeHandsButton != -1) {
            buttonDestroy(gChangeHandsButton);
            gChangeHandsButton = -1;
        }

        if (gChangeHandsButtonMaskFrmData != NULL) {
            artUnlock(gChangeHandsButtonMaskFrmHandle);
            gChangeHandsButtonMaskFrmHandle = NULL;
            gChangeHandsButtonMaskFrmData = NULL;
        }

        if (gChangeHandsButtonDownFrmData != NULL) {
            artUnlock(gChangeHandsButtonDownFrmHandle);
            gChangeHandsButtonDownFrmHandle = NULL;
            gChangeHandsButtonDownFrmData = NULL;
        }

        if (gChangeHandsButtonUpFrmData != NULL) {
            artUnlock(gChangeHandsButtonUpFrmHandle);
            gChangeHandsButtonUpFrmHandle = NULL;
            gChangeHandsButtonUpFrmData = NULL;
        }

        if (gSingleAttackButton != -1) {
            buttonDestroy(gSingleAttackButton);
            gSingleAttackButton = -1;
        }

        if (_itemButtonDisabled != NULL) {
            artUnlock(_itemButtonDisabledKey);
            _itemButtonDisabledKey = NULL;
            _itemButtonDisabled = NULL;
        }

        if (gSingleAttackButtonDownData != NULL) {
            artUnlock(gSingleAttackButtonDownHandle);
            gSingleAttackButtonDownHandle = NULL;
            gSingleAttackButtonDownData = NULL;
        }

        if (gSingleAttackButtonUpData != NULL) {
            artUnlock(gSingleAttackButtonUpHandle);
            gSingleAttackButtonUpHandle = NULL;
            gSingleAttackButtonUpData = NULL;
        }

        if (gCharacterButton != -1) {
            buttonDestroy(gCharacterButton);
            gCharacterButton = -1;
        }

        if (gCharacterButtonDownFrmData != NULL) {
            artUnlock(gCharacterButtonDownFrmHandle);
            gCharacterButtonDownFrmHandle = NULL;
            gCharacterButtonDownFrmData = NULL;
        }

        if (gCharacterButtonUpFrmData != NULL) {
            artUnlock(gCharacterButtonUpFrmHandle);
            gCharacterButtonUpFrmHandle = NULL;
            gCharacterButtonUpFrmData = NULL;
        }

        if (gPipboyButton != -1) {
            buttonDestroy(gPipboyButton);
            gPipboyButton = -1;
        }

        if (gPipboyButtonDownFrmData != NULL) {
            artUnlock(gPipboyButtonDownFrmHandle);
            gPipboyButtonDownFrmHandle = NULL;
            gPipboyButtonDownFrmData = NULL;
        }

        if (gPipboyButtonUpFrmData != NULL) {
            artUnlock(gPipboyButtonUpFrmHandle);
            gPipboyButtonUpFrmHandle = NULL;
            gPipboyButtonUpFrmData = NULL;
        }

        if (gMapButton != -1) {
            buttonDestroy(gMapButton);
            gMapButton = -1;
        }

        if (gMapButtonMaskFrmData != NULL) {
            artUnlock(gMapButtonMaskFrmHandle);
            gMapButtonMaskFrmHandle = NULL;
            gMapButtonMaskFrmData = NULL;
        }

        if (gMapButtonDownFrmData != NULL) {
            artUnlock(gMapButtonDownFrmHandle);
            gMapButtonDownFrmHandle = NULL;
            gMapButtonDownFrmData = NULL;
        }

        if (gMapButtonUpFrmData != NULL) {
            artUnlock(gMapButtonUpFrmHandle);
            gMapButtonUpFrmHandle = NULL;
            gMapButtonUpFrmData = NULL;
        }

        if (gSkilldexButton != -1) {
            buttonDestroy(gSkilldexButton);
            gSkilldexButton = -1;
        }

        if (gSkilldexButtonMaskFrmData != NULL) {
            artUnlock(gSkilldexButtonMaskFrmHandle);
            gSkilldexButtonMaskFrmHandle = NULL;
            gSkilldexButtonMaskFrmData = NULL;
        }

        if (gSkilldexButtonDownFrmData != NULL) {
            artUnlock(gSkilldexButtonDownFrmHandle);
            gSkilldexButtonDownFrmHandle = NULL;
            gSkilldexButtonDownFrmData = NULL;
        }

        if (gSkilldexButtonUpFrmData != NULL) {
            artUnlock(gSkilldexButtonUpFrmHandle);
            gSkilldexButtonUpFrmHandle = NULL;
            gSkilldexButtonUpFrmData = NULL;
        }

        if (gOptionsButton != -1) {
            buttonDestroy(gOptionsButton);
            gOptionsButton = -1;
        }

        if (gOptionsButtonDownFrmData != NULL) {
            artUnlock(gOptionsButtonDownFrmHandle);
            gOptionsButtonDownFrmHandle = NULL;
            gOptionsButtonDownFrmData = NULL;
        }

        if (gOptionsButtonUpFrmData != NULL) {
            artUnlock(gOptionsButtonUpFrmHandle);
            gOptionsButtonUpFrmHandle = NULL;
            gOptionsButtonUpFrmData = NULL;
        }

        if (gInventoryButton != -1) {
            buttonDestroy(gInventoryButton);
            gInventoryButton = -1;
        }

        if (gInventoryButtonDownFrmData != NULL) {
            artUnlock(gInventoryButtonDownFrmHandle);
            gInventoryButtonDownFrmHandle = NULL;
            gInventoryButtonDownFrmData = NULL;
        }

        if (gInventoryButtonUpFrmData != NULL) {
            artUnlock(gInventoryButtonUpFrmHandle);
            gInventoryButtonUpFrmHandle = NULL;
            gInventoryButtonUpFrmData = NULL;
        }

        if (gInterfaceBarWindow != -1) {
            windowDestroy(gInterfaceBarWindow);
            gInterfaceBarWindow = -1;
        }
    }

    interfaceBarFree();
}

// 0x45E860
int interfaceLoad(File* stream)
{
    if (gInterfaceBarWindow == -1) {
        if (interfaceInit() == -1) {
            return -1;
        }
    }

    int interfaceBarEnabled;
    if (fileReadInt32(stream, &interfaceBarEnabled) == -1) return -1;

    int v2;
    if (fileReadInt32(stream, &v2) == -1) return -1;

    int interfaceCurrentHand;
    if (fileReadInt32(stream, &interfaceCurrentHand) == -1) return -1;

    bool interfaceBarEndButtonsIsVisible;
    if (fileReadBool(stream, &interfaceBarEndButtonsIsVisible) == -1) return -1;

    if (!gInterfaceBarEnabled) {
        interfaceBarEnable();
    }

    if (v2) {
        if (gInterfaceBarWindow != -1 && !_intfaceHidden) {
            windowHide(gInterfaceBarWindow);
            _intfaceHidden = 1;
        }
        indicatorBarRefresh();
    } else {
        _intface_show();
    }

    interfaceRenderHitPoints(false);
    interfaceRenderArmorClass(false);

    gInterfaceCurrentHand = interfaceCurrentHand;

    interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);

    if (interfaceBarEndButtonsIsVisible != gInterfaceBarEndButtonsIsVisible) {
        if (interfaceBarEndButtonsIsVisible) {
            interfaceBarEndButtonsShow(false);
        } else {
            interfaceBarEndButtonsHide(false);
        }
    }

    if (!interfaceBarEnabled) {
        interfaceBarDisable();
    }

    indicatorBarRefresh();

    windowRefresh(gInterfaceBarWindow);

    return 0;
}

// 0x45E988
int interfaceSave(File* stream)
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    if (fileWriteInt32(stream, gInterfaceBarEnabled) == -1) return -1;
    if (fileWriteInt32(stream, _intfaceHidden) == -1) return -1;
    if (fileWriteInt32(stream, gInterfaceCurrentHand) == -1) return -1;
    if (fileWriteInt32(stream, gInterfaceBarEndButtonsIsVisible) == -1) return -1;

    return 0;
}

// 0x45EA10
void _intface_show()
{
    if (gInterfaceBarWindow != -1) {
        if (_intfaceHidden) {
            interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
            interfaceRenderHitPoints(false);
            interfaceRenderArmorClass(false);
            windowUnhide(gInterfaceBarWindow);
            _intfaceHidden = false;
        }
    }
    indicatorBarRefresh();
}

// 0x45EA64
void interfaceBarEnable()
{
    if (!gInterfaceBarEnabled) {
        buttonEnable(gInventoryButton);
        buttonEnable(gOptionsButton);
        buttonEnable(gSkilldexButton);
        buttonEnable(gMapButton);
        buttonEnable(gPipboyButton);
        buttonEnable(gCharacterButton);

        if (gInterfaceItemStates[gInterfaceCurrentHand].isDisabled == 0) {
            buttonEnable(gSingleAttackButton);
        }

        buttonEnable(gEndTurnButton);
        buttonEnable(gEndCombatButton);
        displayMonitorEnable();

        gInterfaceBarEnabled = true;
    }
}

// 0x45EAFC
void interfaceBarDisable()
{
    if (gInterfaceBarEnabled) {
        displayMonitorDisable();
        buttonDisable(gInventoryButton);
        buttonDisable(gOptionsButton);
        buttonDisable(gSkilldexButton);
        buttonDisable(gMapButton);
        buttonDisable(gPipboyButton);
        buttonDisable(gCharacterButton);
        if (gInterfaceItemStates[gInterfaceCurrentHand].isDisabled == 0) {
            buttonDisable(gSingleAttackButton);
        }
        buttonDisable(gEndTurnButton);
        buttonDisable(gEndCombatButton);
        gInterfaceBarEnabled = false;
    }
}

// 0x45EB90
bool interfaceBarEnabled()
{
    return gInterfaceBarEnabled;
}

// 0x45EB98
void interfaceBarRefresh()
{
    if (gInterfaceBarWindow != -1) {
        interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
        interfaceRenderHitPoints(false);
        interfaceRenderArmorClass(false);
        indicatorBarRefresh();
        windowRefresh(gInterfaceBarWindow);
    }
    indicatorBarRefresh();
}

// Render hit points.
//
// 0x45EBD8
void interfaceRenderHitPoints(bool animate)
{
    if (gInterfaceBarWindow == -1) {
        return;
    }

    int hp = critterGetHitPoints(gDude);
    int maxHp = critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS);

    int red = (int)((double)maxHp * 0.25);
    int yellow = (int)((double)maxHp * 0.5);

    int color;
    if (hp < red) {
        color = INTERFACE_NUMBERS_COLOR_RED;
    } else if (hp < yellow) {
        color = INTERFACE_NUMBERS_COLOR_YELLOW;
    } else {
        color = INTERFACE_NUMBERS_COLOR_WHITE;
    }

    int v1[4];
    int v2[3];
    int count = 1;

    v1[0] = gInterfaceLastRenderedHitPoints;
    v2[0] = gInterfaceLastRenderedHitPointsColor;

    if (gInterfaceLastRenderedHitPointsColor != color) {
        if (hp >= gInterfaceLastRenderedHitPoints) {
            if (gInterfaceLastRenderedHitPoints < red && hp >= red) {
                v1[count] = red;
                v2[count] = INTERFACE_NUMBERS_COLOR_YELLOW;
                count += 1;
            }

            if (gInterfaceLastRenderedHitPoints < yellow && hp >= yellow) {
                v1[count] = yellow;
                v2[count] = INTERFACE_NUMBERS_COLOR_WHITE;
                count += 1;
            }
        } else {
            if (gInterfaceLastRenderedHitPoints >= yellow && hp < yellow) {
                v1[count] = yellow;
                v2[count] = INTERFACE_NUMBERS_COLOR_YELLOW;
                count += 1;
            }

            if (gInterfaceLastRenderedHitPoints >= red && hp < red) {
                v1[count] = red;
                v2[count] = INTERFACE_NUMBERS_COLOR_RED;
                count += 1;
            }
        }
    }

    v1[count] = hp;

    if (animate) {
        int delay = 250 / (abs(gInterfaceLastRenderedHitPoints - hp) + 1);
        for (int index = 0; index < count; index++) {
            interfaceRenderCounter(473, 40, v1[index], v1[index + 1], v2[index], delay);
        }
    } else {
        interfaceRenderCounter(473, 40, gInterfaceLastRenderedHitPoints, hp, color, 0);
    }

    gInterfaceLastRenderedHitPoints = hp;
    gInterfaceLastRenderedHitPointsColor = color;
}

// Render armor class.
//
// 0x45EDA8
void interfaceRenderArmorClass(bool animate)
{
    int armorClass = critterGetStat(gDude, STAT_ARMOR_CLASS);

    int delay = 0;
    if (animate) {
        delay = 250 / (abs(gInterfaceLastRenderedArmorClass - armorClass) + 1);
    }

    interfaceRenderCounter(473, 75, gInterfaceLastRenderedArmorClass, armorClass, 0, delay);

    gInterfaceLastRenderedArmorClass = armorClass;
}

// 0x45EE0C
void interfaceRenderActionPoints(int actionPointsLeft, int bonusActionPoints)
{
    unsigned char* frmData;

    if (gInterfaceBarWindow == -1) {
        return;
    }

    blitBufferToBuffer(gInterfaceActionPointsBarBackground, 90, 5, 90, gInterfaceWindowBuffer + 14 * 640 + 316, 640);

    if (actionPointsLeft == -1) {
        frmData = gRedLightFrmData;
        actionPointsLeft = 10;
        bonusActionPoints = 0;
    } else {
        frmData = gGreenLightFrmData;

        if (actionPointsLeft < 0) {
            actionPointsLeft = 0;
        }

        if (actionPointsLeft > 10) {
            actionPointsLeft = 10;
        }

        if (bonusActionPoints >= 0) {
            if (actionPointsLeft + bonusActionPoints > 10) {
                bonusActionPoints = 10 - actionPointsLeft;
            }
        } else {
            bonusActionPoints = 0;
        }
    }

    int index;
    for (index = 0; index < actionPointsLeft; index++) {
        blitBufferToBuffer(frmData, 5, 5, 5, gInterfaceWindowBuffer + 14 * 640 + 316 + index * 9, 640);
    }

    for (; index < (actionPointsLeft + bonusActionPoints); index++) {
        blitBufferToBuffer(gYellowLightFrmData, 5, 5, 5, gInterfaceWindowBuffer + 14 * 640 + 316 + index * 9, 640);
    }

    if (!gInterfaceBarInitialized) {
        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarActionPointsBarRect);
    }
}

// 0x45EF6C
int interfaceGetCurrentHitMode(int* hitMode, bool* aiming)
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    *aiming = false;

    switch (gInterfaceItemStates[gInterfaceCurrentHand].action) {
    case INTERFACE_ITEM_ACTION_PRIMARY_AIMING:
        *aiming = true;
        // FALLTHROUGH
    case INTERFACE_ITEM_ACTION_PRIMARY:
        *hitMode = gInterfaceItemStates[gInterfaceCurrentHand].primaryHitMode;
        return 0;
    case INTERFACE_ITEM_ACTION_SECONDARY_AIMING:
        *aiming = true;
        // FALLTHROUGH
    case INTERFACE_ITEM_ACTION_SECONDARY:
        *hitMode = gInterfaceItemStates[gInterfaceCurrentHand].secondaryHitMode;
        return 0;
    }

    return -1;
}

// 0x45EFEC
int interfaceUpdateItems(bool animated, int leftItemAction, int rightItemAction)
{
    if (isoIsDisabled()) {
        animated = false;
    }

    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    Object* oldCurrentItem = gInterfaceItemStates[gInterfaceCurrentHand].item;

    InterfaceItemState* leftItemState = &(gInterfaceItemStates[HAND_LEFT]);
    Object* item1 = critterGetItem1(gDude);
    if (item1 == leftItemState->item && leftItemState->item != NULL) {
        if (leftItemState->item != NULL) {
            leftItemState->isDisabled = _can_use_weapon(item1);
            leftItemState->itemFid = itemGetInventoryFid(item1);
        }
    } else {
        leftItemState->item = item1;
        if (item1 != NULL) {
            leftItemState->isDisabled = _can_use_weapon(item1);
            leftItemState->primaryHitMode = HIT_MODE_LEFT_WEAPON_PRIMARY;
            leftItemState->secondaryHitMode = HIT_MODE_LEFT_WEAPON_SECONDARY;
            leftItemState->isWeapon = itemGetType(item1) == ITEM_TYPE_WEAPON;

            if (leftItemAction == INTERFACE_ITEM_ACTION_DEFAULT) {
                if (leftItemState->isWeapon != 0) {
                    leftItemState->action = INTERFACE_ITEM_ACTION_PRIMARY;
                } else {
                    leftItemState->action = INTERFACE_ITEM_ACTION_USE;
                }
            } else {
                leftItemState->action = leftItemAction;
            }

            leftItemState->itemFid = itemGetInventoryFid(item1);
        } else {
            leftItemState->isDisabled = 0;
            leftItemState->isWeapon = 1;
            leftItemState->action = INTERFACE_ITEM_ACTION_PRIMARY;
            leftItemState->itemFid = -1;

            int unarmed = skillGetValue(gDude, SKILL_UNARMED);
            int agility = critterGetStat(gDude, STAT_AGILITY);
            int strength = critterGetStat(gDude, STAT_STRENGTH);
            int level = pcGetStat(PC_STAT_LEVEL);

            if (unarmed > 99 && agility > 6 && strength > 4 && level > 8) {
                leftItemState->primaryHitMode = HIT_MODE_HAYMAKER;
            } else if (unarmed > 74 && agility > 5 && strength > 4 && level > 5) {
                leftItemState->primaryHitMode = HIT_MODE_HAMMER_PUNCH;
            } else if (unarmed > 54 && agility > 5) {
                leftItemState->primaryHitMode = HIT_MODE_STRONG_PUNCH;
            } else {
                leftItemState->primaryHitMode = HIT_MODE_PUNCH;
            }

            if (unarmed > 129 && agility > 6 && strength > 4 && level > 15) {
                leftItemState->secondaryHitMode = HIT_MODE_PIERCING_STRIKE;
            } else if (unarmed > 114 && agility > 6 && strength > 4 && level > 11) {
                leftItemState->secondaryHitMode = HIT_MODE_PALM_STRIKE;
            } else if (unarmed > 74 && agility > 6 && strength > 4 && level > 4) {
                leftItemState->secondaryHitMode = HIT_MODE_JAB;
            } else {
                leftItemState->secondaryHitMode = HIT_MODE_PUNCH;
            }
        }
    }

    InterfaceItemState* rightItemState = &(gInterfaceItemStates[HAND_RIGHT]);

    Object* item2 = critterGetItem2(gDude);
    if (item2 == rightItemState->item && rightItemState->item != NULL) {
        if (rightItemState->item != NULL) {
            rightItemState->isDisabled = _can_use_weapon(rightItemState->item);
            rightItemState->itemFid = itemGetInventoryFid(rightItemState->item);
        }
    } else {
        rightItemState->item = item2;

        if (item2 != NULL) {
            rightItemState->isDisabled = _can_use_weapon(item2);
            rightItemState->primaryHitMode = HIT_MODE_RIGHT_WEAPON_PRIMARY;
            rightItemState->secondaryHitMode = HIT_MODE_RIGHT_WEAPON_SECONDARY;
            rightItemState->isWeapon = itemGetType(item2) == ITEM_TYPE_WEAPON;

            if (rightItemAction == INTERFACE_ITEM_ACTION_DEFAULT) {
                if (rightItemState->isWeapon != 0) {
                    rightItemState->action = INTERFACE_ITEM_ACTION_PRIMARY;
                } else {
                    rightItemState->action = INTERFACE_ITEM_ACTION_USE;
                }
            } else {
                rightItemState->action = rightItemAction;
            }
            rightItemState->itemFid = itemGetInventoryFid(item2);
        } else {
            rightItemState->isDisabled = 0;
            rightItemState->isWeapon = 1;
            rightItemState->action = INTERFACE_ITEM_ACTION_PRIMARY;
            rightItemState->itemFid = -1;

            int unarmed = skillGetValue(gDude, SKILL_UNARMED);
            int agility = critterGetStat(gDude, STAT_AGILITY);
            int strength = critterGetStat(gDude, STAT_STRENGTH);
            int level = pcGetStat(PC_STAT_LEVEL);

            if (unarmed > 79 && agility > 5 && strength > 5 && level > 8) {
                rightItemState->primaryHitMode = HIT_MODE_POWER_KICK;
            } else if (unarmed > 59 && agility > 5 && level > 5) {
                rightItemState->primaryHitMode = HIT_MODE_SNAP_KICK;
            } else if (unarmed > 39 && agility > 5) {
                rightItemState->primaryHitMode = HIT_MODE_STRONG_KICK;
            } else {
                rightItemState->primaryHitMode = HIT_MODE_KICK;
            }

            if (unarmed > 124 && agility > 7 && strength > 5 && level > 14) {
                rightItemState->secondaryHitMode = HIT_MODE_PIERCING_KICK;
            } else if (unarmed > 99 && agility > 6 && strength > 5 && level > 11) {
                rightItemState->secondaryHitMode = HIT_MODE_HOOK_KICK;
            } else if (unarmed > 59 && agility > 6 && strength > 5 && level > 5) {
                rightItemState->secondaryHitMode = HIT_MODE_HIP_KICK;
            } else {
                rightItemState->secondaryHitMode = HIT_MODE_KICK;
            }
        }
    }

    if (animated) {
        Object* newCurrentItem = gInterfaceItemStates[gInterfaceCurrentHand].item;
        if (newCurrentItem != oldCurrentItem) {
            int animationCode = 0;
            if (newCurrentItem != NULL) {
                if (itemGetType(newCurrentItem) == ITEM_TYPE_WEAPON) {
                    animationCode = weaponGetAnimationCode(newCurrentItem);
                }
            }

            interfaceBarSwapHandsAnimatePutAwayTakeOutSequence((gDude->fid & 0xF000) >> 12, animationCode);

            return 0;
        }
    }

    interfaceBarRefreshMainAction();

    return 0;
}

// 0x45F404
int interfaceBarSwapHands(bool animated)
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    gInterfaceCurrentHand = 1 - gInterfaceCurrentHand;

    if (animated) {
        Object* item = gInterfaceItemStates[gInterfaceCurrentHand].item;
        int animationCode = 0;
        if (item != NULL) {
            if (itemGetType(item) == ITEM_TYPE_WEAPON) {
                animationCode = weaponGetAnimationCode(item);
            }
        }

        interfaceBarSwapHandsAnimatePutAwayTakeOutSequence((gDude->fid & 0xF000) >> 12, animationCode);
    } else {
        interfaceBarRefreshMainAction();
    }

    int mode = gameMouseGetMode();
    if (mode == GAME_MOUSE_MODE_CROSSHAIR || mode == GAME_MOUSE_MODE_USE_CROSSHAIR) {
        gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
    }

    return 0;
}

// 0x45F4B4
int interfaceGetItemActions(int* leftItemAction, int* rightItemAction)
{
    *leftItemAction = gInterfaceItemStates[HAND_LEFT].action;
    *rightItemAction = gInterfaceItemStates[HAND_RIGHT].action;
    return 0;
}

// 0x45F4E0
int interfaceCycleItemAction()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    InterfaceItemState* itemState = &(gInterfaceItemStates[gInterfaceCurrentHand]);

    int oldAction = itemState->action;
    if (itemState->isWeapon != 0) {
        bool done = false;
        while (!done) {
            itemState->action++;
            switch (itemState->action) {
            case INTERFACE_ITEM_ACTION_PRIMARY:
                done = true;
                break;
            case INTERFACE_ITEM_ACTION_PRIMARY_AIMING:
                if (_item_w_called_shot(gDude, itemState->primaryHitMode)) {
                    done = true;
                }
                break;
            case INTERFACE_ITEM_ACTION_SECONDARY:
                if (itemState->secondaryHitMode != HIT_MODE_PUNCH
                    && itemState->secondaryHitMode != HIT_MODE_KICK
                    && weaponGetAttackTypeForHitMode(itemState->item, itemState->secondaryHitMode) != ATTACK_TYPE_NONE) {
                    done = true;
                }
                break;
            case INTERFACE_ITEM_ACTION_SECONDARY_AIMING:
                if (itemState->secondaryHitMode != HIT_MODE_PUNCH
                    && itemState->secondaryHitMode != HIT_MODE_KICK
                    && weaponGetAttackTypeForHitMode(itemState->item, itemState->secondaryHitMode) != ATTACK_TYPE_NONE
                    && _item_w_called_shot(gDude, itemState->secondaryHitMode)) {
                    done = true;
                }
                break;
            case INTERFACE_ITEM_ACTION_RELOAD:
                if (ammoGetCapacity(itemState->item) != ammoGetQuantity(itemState->item)) {
                    done = true;
                }
                break;
            case INTERFACE_ITEM_ACTION_COUNT:
                itemState->action = INTERFACE_ITEM_ACTION_USE;
                break;
            }
        }
    }

    if (oldAction != itemState->action) {
        interfaceBarRefreshMainAction();
    }

    return 0;
}

// 0x45F5EC
void _intface_use_item()
{
    if (gInterfaceBarWindow == -1) {
        return;
    }

    InterfaceItemState* ptr = &(gInterfaceItemStates[gInterfaceCurrentHand]);

    if (ptr->isWeapon != 0) {
        if (ptr->action == INTERFACE_ITEM_ACTION_RELOAD) {
            if (isInCombat()) {
                int hitMode = gInterfaceCurrentHand == HAND_LEFT
                    ? HIT_MODE_LEFT_WEAPON_RELOAD
                    : HIT_MODE_RIGHT_WEAPON_RELOAD;

                int actionPointsRequired = _item_mp_cost(gDude, hitMode, false);
                if (actionPointsRequired <= gDude->data.critter.combat.ap) {
                    if (_intface_item_reload() == 0) {
                        if (actionPointsRequired > gDude->data.critter.combat.ap) {
                            gDude->data.critter.combat.ap = 0;
                        } else {
                            gDude->data.critter.combat.ap -= actionPointsRequired;
                        }
                        interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
                    }
                }
            } else {
                _intface_item_reload();
            }
        } else {
            gameMouseSetCursor(MOUSE_CURSOR_CROSSHAIR);
            gameMouseSetMode(GAME_MOUSE_MODE_CROSSHAIR);
            if (!isInCombat()) {
                _combat(NULL);
            }
        }
    } else if (_proto_action_can_use_on(ptr->item->pid)) {
        gameMouseSetCursor(MOUSE_CURSOR_USE_CROSSHAIR);
        gameMouseSetMode(GAME_MOUSE_MODE_USE_CROSSHAIR);
    } else if (_obj_action_can_use(ptr->item)) {
        if (isInCombat()) {
            int actionPointsRequired = _item_mp_cost(gDude, ptr->secondaryHitMode, false);
            if (actionPointsRequired <= gDude->data.critter.combat.ap) {
                _obj_use_item(gDude, ptr->item);
                interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
                if (actionPointsRequired > gDude->data.critter.combat.ap) {
                    gDude->data.critter.combat.ap = 0;
                } else {
                    gDude->data.critter.combat.ap -= actionPointsRequired;
                }

                interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
            }
        } else {
            _obj_use_item(gDude, ptr->item);
            interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
        }
    }
}

// 0x45F7FC
int interfaceGetCurrentHand()
{
    return gInterfaceCurrentHand;
}

// 0x45F804
int interfaceGetActiveItem(Object** itemPtr)
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    *itemPtr = gInterfaceItemStates[gInterfaceCurrentHand].item;

    return 0;
}

// 0x45F838
int _intface_update_ammo_lights()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    InterfaceItemState* p = &(gInterfaceItemStates[gInterfaceCurrentHand]);

    int ratio = 0;

    if (p->isWeapon != 0) {
        // calls sub_478674 twice, probably because if min/max kind macro
        int maximum = ammoGetCapacity(p->item);
        if (maximum > 0) {
            int current = ammoGetQuantity(p->item);
            ratio = (int)((double)current / (double)maximum * 70.0);
        }
    } else {
        if (itemGetType(p->item) == ITEM_TYPE_MISC) {
            // calls sub_4793D0 twice, probably because if min/max kind macro
            int maximum = miscItemGetMaxCharges(p->item);
            if (maximum > 0) {
                int current = miscItemGetCharges(p->item);
                ratio = (int)((double)current / (double)maximum * 70.0);
            }
        }
    }

    interfaceUpdateAmmoBar(463, ratio);

    return 0;
}

// 0x45F96C
void interfaceBarEndButtonsShow(bool animated)
{
    if (gInterfaceBarWindow == -1) {
        return;
    }

    if (gInterfaceBarEndButtonsIsVisible) {
        return;
    }

    int fid = buildFid(6, 104, 0, 0, 0);
    CacheEntry* handle;
    Art* art = artLock(fid, &handle);
    if (art == NULL) {
        return;
    }

    int frameCount = artGetFrameCount(art);
    soundPlayFile("iciboxx1");

    if (animated) {
        unsigned int delay = 1000 / artGetFramesPerSecond(art);
        int time = 0;
        int frame = 0;
        while (frame < frameCount) {
            if (getTicksSince(time) >= delay) {
                unsigned char* src = artGetFrameData(art, frame, 0);
                if (src != NULL) {
                    blitBufferToBuffer(src, 57, 58, 57, gInterfaceWindowBuffer + 640 * 38 + 580, 640);
                    windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
                }

                time = _get_time();
                frame++;
            }
            gameMouseRefresh();
        }
    } else {
        unsigned char* src = artGetFrameData(art, frameCount - 1, 0);
        blitBufferToBuffer(src, 57, 58, 57, gInterfaceWindowBuffer + 640 * 38 + 580, 640);
        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
    }

    artUnlock(handle);

    gInterfaceBarEndButtonsIsVisible = true;
    endTurnButtonInit();
    endCombatButtonInit();
    interfaceBarEndButtonsRenderRedLights();
}

// 0x45FAC0
void interfaceBarEndButtonsHide(bool animated)
{
    if (gInterfaceBarWindow == -1) {
        return;
    }

    if (!gInterfaceBarEndButtonsIsVisible) {
        return;
    }

    int fid = buildFid(6, 104, 0, 0, 0);
    CacheEntry* handle;
    Art* art = artLock(fid, &handle);
    if (art == NULL) {
        return;
    }

    endTurnButtonFree();
    endCombatButtonFree();
    soundPlayFile("icibcxx1");

    if (animated) {
        unsigned int delay = 1000 / artGetFramesPerSecond(art);
        unsigned int time = 0;
        int frame = artGetFrameCount(art);

        while (frame != 0) {
            if (getTicksSince(time) >= delay) {
                unsigned char* src = artGetFrameData(art, frame - 1, 0);
                unsigned char* dest = gInterfaceWindowBuffer + 640 * 38 + 580;
                if (src != NULL) {
                    blitBufferToBuffer(src, 57, 58, 57, dest, 640);
                    windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
                }

                time = _get_time();
                frame--;
            }
            gameMouseRefresh();
        }
    } else {
        unsigned char* dest = gInterfaceWindowBuffer + 640 * 38 + 580;
        unsigned char* src = artGetFrameData(art, 0, 0);
        blitBufferToBuffer(src, 57, 58, 57, dest, 640);
        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
    }

    artUnlock(handle);
    gInterfaceBarEndButtonsIsVisible = false;
}

// 0x45FC04
void interfaceBarEndButtonsRenderGreenLights()
{
    if (gInterfaceBarEndButtonsIsVisible) {
        buttonEnable(gEndTurnButton);
        buttonEnable(gEndCombatButton);

        // endltgrn.frm - green lights around end turn/combat window
        int lightsFid = buildFid(6, 109, 0, 0, 0);
        CacheEntry* lightsFrmHandle;
        unsigned char* lightsFrmData = artLockFrameData(lightsFid, 0, 0, &lightsFrmHandle);
        if (lightsFrmData == NULL) {
            return;
        }

        soundPlayFile("icombat2");
        blitBufferToBufferTrans(lightsFrmData, 57, 58, 57, gInterfaceWindowBuffer + 38 * 640 + 580, 640);
        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);

        artUnlock(lightsFrmHandle);
    }
}

// 0x45FC98
void interfaceBarEndButtonsRenderRedLights()
{
    if (gInterfaceBarEndButtonsIsVisible) {
        buttonDisable(gEndTurnButton);
        buttonDisable(gEndCombatButton);

        CacheEntry* lightsFrmHandle;
        // endltred.frm - red lights around end turn/combat window
        int lightsFid = buildFid(6, 110, 0, 0, 0);
        unsigned char* lightsFrmData = artLockFrameData(lightsFid, 0, 0, &lightsFrmHandle);
        if (lightsFrmData == NULL) {
            return;
        }

        soundPlayFile("icombat1");
        blitBufferToBufferTrans(lightsFrmData, 57, 58, 57, gInterfaceWindowBuffer + 38 * 640 + 580, 640);
        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);

        artUnlock(lightsFrmHandle);
    }
}

// 0x45FD88
static int interfaceBarRefreshMainAction()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    buttonEnable(gSingleAttackButton);

    InterfaceItemState* itemState = &(gInterfaceItemStates[gInterfaceCurrentHand]);
    int actionPoints = -1;

    if (itemState->isDisabled == 0) {
        memcpy(_itemButtonUp, gSingleAttackButtonUpData, sizeof(_itemButtonUp));
        memcpy(_itemButtonDown, gSingleAttackButtonDownData, sizeof(_itemButtonDown));

        if (itemState->isWeapon == 0) {
            int fid;
            if (_proto_action_can_use_on(itemState->item->pid)) {
                // USE ON
                fid = buildFid(6, 294, 0, 0, 0);
            } else if (_obj_action_can_use(itemState->item)) {
                // USE
                fid = buildFid(6, 292, 0, 0, 0);
            } else {
                fid = -1;
            }

            if (fid != -1) {
                CacheEntry* useTextFrmHandle;
                Art* useTextFrm = artLock(fid, &useTextFrmHandle);
                if (useTextFrm != NULL) {
                    int width = artGetWidth(useTextFrm, 0, 0);
                    int height = artGetHeight(useTextFrm, 0, 0);
                    unsigned char* data = artGetFrameData(useTextFrm, 0, 0);
                    blitBufferToBufferTrans(data, width, height, width, _itemButtonUp + 188 * 7 + 181 - width, 188);
                    _dark_trans_buf_to_buf(data, width, height, width, _itemButtonDown, 181 - width + 1, 5, 188, 59641);
                    artUnlock(useTextFrmHandle);
                }

                actionPoints = _item_mp_cost(gDude, itemState->primaryHitMode, false);
            }
        } else {
            int primaryFid = -1;
            int bullseyeFid = -1;
            int hitMode = -1;

            // NOTE: This value is decremented at 0x45FEAC, probably to build
            // jump table.
            switch (itemState->action) {
            case INTERFACE_ITEM_ACTION_PRIMARY_AIMING:
                bullseyeFid = buildFid(6, 288, 0, 0, 0);
                // FALLTHROUGH
            case INTERFACE_ITEM_ACTION_PRIMARY:
                hitMode = itemState->primaryHitMode;
                break;
            case INTERFACE_ITEM_ACTION_SECONDARY_AIMING:
                bullseyeFid = buildFid(6, 288, 0, 0, 0);
                // FALLTHROUGH
            case INTERFACE_ITEM_ACTION_SECONDARY:
                hitMode = itemState->secondaryHitMode;
                break;
            case INTERFACE_ITEM_ACTION_RELOAD:
                actionPoints = _item_mp_cost(gDude, gInterfaceCurrentHand == HAND_LEFT ? HIT_MODE_LEFT_WEAPON_RELOAD : HIT_MODE_RIGHT_WEAPON_RELOAD, false);
                primaryFid = buildFid(6, 291, 0, 0, 0);
                break;
            }

            if (bullseyeFid != -1) {
                CacheEntry* bullseyeFrmHandle;
                Art* bullseyeFrm = artLock(bullseyeFid, &bullseyeFrmHandle);
                if (bullseyeFrm != NULL) {
                    int width = artGetWidth(bullseyeFrm, 0, 0);
                    int height = artGetHeight(bullseyeFrm, 0, 0);
                    unsigned char* data = artGetFrameData(bullseyeFrm, 0, 0);
                    blitBufferToBufferTrans(data, width, height, width, _itemButtonUp + 188 * (60 - height) + (181 - width), 188);

                    int v9 = 60 - height - 2;
                    if (v9 < 0) {
                        v9 = 0;
                        height -= 2;
                    }

                    _dark_trans_buf_to_buf(data, width, height, width, _itemButtonDown, 181 - width + 1, v9, 188, 59641);
                    artUnlock(bullseyeFrmHandle);
                }
            }

            if (hitMode != -1) {
                actionPoints = _item_w_mp_cost(gDude, hitMode, bullseyeFid != -1);

                int id;
                int anim = critterGetAnimationForHitMode(gDude, hitMode);
                switch (anim) {
                case ANIM_THROW_PUNCH:
                    switch (hitMode) {
                    case HIT_MODE_STRONG_PUNCH:
                        id = 432; // strong punch
                        break;
                    case HIT_MODE_HAMMER_PUNCH:
                        id = 425; // hammer punch
                        break;
                    case HIT_MODE_HAYMAKER:
                        id = 428; // lightning punch
                        break;
                    case HIT_MODE_JAB:
                        id = 421; // chop punch
                        break;
                    case HIT_MODE_PALM_STRIKE:
                        id = 423; // dragon punch
                        break;
                    case HIT_MODE_PIERCING_STRIKE:
                        id = 424; // force punch
                        break;
                    default:
                        id = 42; // punch
                        break;
                    }
                    break;
                case ANIM_KICK_LEG:
                    switch (hitMode) {
                    case HIT_MODE_STRONG_KICK:
                        id = 430; // skick.frm - strong kick text
                        break;
                    case HIT_MODE_SNAP_KICK:
                        id = 431; // snapkick.frm - snap kick text
                        break;
                    case HIT_MODE_POWER_KICK:
                        id = 429; // cm_pwkck.frm - roundhouse kick text
                        break;
                    case HIT_MODE_HIP_KICK:
                        id = 426; // hipk.frm - kip kick text
                        break;
                    case HIT_MODE_HOOK_KICK:
                        id = 427; // cm_hookk.frm - jump kick text
                        break;
                    case HIT_MODE_PIERCING_KICK: // cm_prckk.frm - death blossom kick text
                        id = 422;
                        break;
                    default:
                        id = 41; // kick.frm - kick text
                        break;
                    }
                    break;
                case ANIM_THROW_ANIM:
                    id = 117; // throw
                    break;
                case ANIM_THRUST_ANIM:
                    id = 45; // thrust
                    break;
                case ANIM_SWING_ANIM:
                    id = 44; // swing
                    break;
                case ANIM_FIRE_SINGLE:
                    id = 43; // single
                    break;
                case ANIM_FIRE_BURST:
                case ANIM_FIRE_CONTINUOUS:
                    id = 40; // burst
                    break;
                }

                primaryFid = buildFid(6, id, 0, 0, 0);
            }

            if (primaryFid != -1) {
                CacheEntry* primaryFrmHandle;
                Art* primaryFrm = artLock(primaryFid, &primaryFrmHandle);
                if (primaryFrm != NULL) {
                    int width = artGetWidth(primaryFrm, 0, 0);
                    int height = artGetHeight(primaryFrm, 0, 0);
                    unsigned char* data = artGetFrameData(primaryFrm, 0, 0);
                    blitBufferToBufferTrans(data, width, height, width, _itemButtonUp + 188 * 7 + 181 - width, 188);
                    _dark_trans_buf_to_buf(data, width, height, width, _itemButtonDown, 181 - width + 1, 5, 188, 59641);
                    artUnlock(primaryFrmHandle);
                }
            }
        }
    }

    if (actionPoints >= 0 && actionPoints < 10) {
        // movement point text
        int fid = buildFid(6, 289, 0, 0, 0);

        CacheEntry* handle;
        Art* art = artLock(fid, &handle);
        if (art != NULL) {
            int width = artGetWidth(art, 0, 0);
            int height = artGetHeight(art, 0, 0);
            unsigned char* data = artGetFrameData(art, 0, 0);

            blitBufferToBufferTrans(data, width, height, width, _itemButtonUp + 188 * (60 - height) + 7, 188);

            int v29 = 60 - height - 2;
            if (v29 < 0) {
                v29 = 0;
                height -= 2;
            }

            _dark_trans_buf_to_buf(data, width, height, width, _itemButtonDown, 7 + 1, v29, 188, 59641);
            artUnlock(handle);

            int offset = width + 7;

            // movement point numbers - ten numbers 0 to 9, each 10 pixels wide.
            fid = buildFid(6, 290, 0, 0, 0);
            art = artLock(fid, &handle);
            if (art != NULL) {
                width = artGetWidth(art, 0, 0);
                height = artGetHeight(art, 0, 0);
                data = artGetFrameData(art, 0, 0);

                blitBufferToBufferTrans(data + actionPoints * 10, 10, height, width, _itemButtonUp + 188 * (60 - height) + 7 + offset, 188);

                int v40 = 60 - height - 2;
                if (v40 < 0) {
                    v40 = 0;
                    height -= 2;
                }
                _dark_trans_buf_to_buf(data + actionPoints * 10, 10, height, width, _itemButtonDown, offset + 7 + 1, v40, 188, 59641);

                artUnlock(handle);
            }
        }
    } else {
        memcpy(_itemButtonUp, _itemButtonDisabled, sizeof(_itemButtonUp));
        memcpy(_itemButtonDown, _itemButtonDisabled, sizeof(_itemButtonDown));
    }

    if (itemState->itemFid != -1) {
        CacheEntry* itemFrmHandle;
        Art* itemFrm = artLock(itemState->itemFid, &itemFrmHandle);
        if (itemFrm != NULL) {
            int width = artGetWidth(itemFrm, 0, 0);
            int height = artGetHeight(itemFrm, 0, 0);
            unsigned char* data = artGetFrameData(itemFrm, 0, 0);

            int v46 = (188 - width) / 2;
            int v47 = (67 - height) / 2 - 2;

            blitBufferToBufferTrans(data, width, height, width, _itemButtonUp + 188 * ((67 - height) / 2) + v46, 188);

            if (v47 < 0) {
                v47 = 0;
                height -= 2;
            }

            _dark_trans_buf_to_buf(data, width, height, width, _itemButtonDown, v46 + 1, v47, 188, 63571);
            artUnlock(itemFrmHandle);
        }
    }

    if (!gInterfaceBarInitialized) {
        _intface_update_ammo_lights();

        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarMainActionRect);

        if (itemState->isDisabled != 0) {
            buttonDisable(gSingleAttackButton);
        } else {
            buttonEnable(gSingleAttackButton);
        }
    }

    return 0;
}

// 0x460658
static int _intface_redraw_items_callback(Object* a1, Object* a2)
{
    interfaceBarRefreshMainAction();
    return 0;
}

// 0x460660
static int _intface_change_fid_callback(Object* a1, Object* a2)
{
    gInterfaceBarSwapHandsInProgress = false;
    return 0;
}

// 0x46066C
static void interfaceBarSwapHandsAnimatePutAwayTakeOutSequence(int previousWeaponAnimationCode, int weaponAnimationCode)
{
    gInterfaceBarSwapHandsInProgress = true;

    reg_anim_clear(gDude);
    reg_anim_begin(2);
    reg_anim_update_light(gDude, 4, 0);

    if (previousWeaponAnimationCode != 0) {
        const char* sfx = sfxBuildCharName(gDude, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
        reg_anim_play_sfx(gDude, sfx, 0);
        reg_anim_animate(gDude, ANIM_PUT_AWAY, 0);
    }

    reg_anim_11_1(NULL, NULL, _intface_redraw_items_callback, -1);

    Object* item = gInterfaceItemStates[gInterfaceCurrentHand].item;
    if (item != NULL && item->lightDistance > 4) {
        reg_anim_update_light(gDude, item->lightDistance, 0);
    }

    if (weaponAnimationCode != 0) {
        reg_anim_18(gDude, weaponAnimationCode, -1);
    } else {
        int fid = buildFid(1, gDude->fid & 0xFFF, ANIM_STAND, 0, gDude->rotation + 1);
        reg_anim_17(gDude, fid, -1);
    }

    reg_anim_11_1(NULL, NULL, _intface_change_fid_callback, -1);

    if (reg_anim_end() == -1) {
        return;
    }

    bool interfaceBarWasEnabled = gInterfaceBarEnabled;

    interfaceBarDisable();
    _gmouse_disable(0);

    gameMouseSetCursor(MOUSE_CURSOR_WAIT_WATCH);

    while (gInterfaceBarSwapHandsInProgress) {
        if (_game_user_wants_to_quit) {
            break;
        }

        _get_input();
    }

    gameMouseSetCursor(MOUSE_CURSOR_NONE);

    _gmouse_enable();

    if (interfaceBarWasEnabled) {
        interfaceBarEnable();
    }
}

// 0x4607E0
static int endTurnButtonInit()
{
    int fid;

    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    if (!gInterfaceBarEndButtonsIsVisible) {
        return -1;
    }

    fid = buildFid(6, 105, 0, 0, 0);
    gEndTurnButtonUpFrmData = artLockFrameData(fid, 0, 0, &gEndTurnButtonUpFrmHandle);
    if (gEndTurnButtonUpFrmData == NULL) {
        return -1;
    }

    fid = buildFid(6, 106, 0, 0, 0);
    gEndTurnButtonDownFrmData = artLockFrameData(fid, 0, 0, &gEndTurnButtonDownFrmHandle);
    if (gEndTurnButtonDownFrmData == NULL) {
        return -1;
    }

    gEndTurnButton = buttonCreate(gInterfaceBarWindow, 590, 43, 38, 22, -1, -1, -1, 32, gEndTurnButtonUpFrmData, gEndTurnButtonDownFrmData, NULL, 0);
    if (gEndTurnButton == -1) {
        return -1;
    }

    _win_register_button_disable(gEndTurnButton, gEndTurnButtonUpFrmData, gEndTurnButtonUpFrmData, gEndTurnButtonUpFrmData);
    buttonSetCallbacks(gEndTurnButton, _gsound_med_butt_press, _gsound_med_butt_release);

    return 0;
}

// 0x4608C4
static int endTurnButtonFree()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    if (gEndTurnButton != -1) {
        buttonDestroy(gEndTurnButton);
        gEndTurnButton = -1;
    }

    if (gEndTurnButtonDownFrmData) {
        artUnlock(gEndTurnButtonDownFrmHandle);
        gEndTurnButtonDownFrmHandle = NULL;
        gEndTurnButtonDownFrmData = NULL;
    }

    if (gEndTurnButtonUpFrmData) {
        artUnlock(gEndTurnButtonUpFrmHandle);
        gEndTurnButtonUpFrmHandle = NULL;
        gEndTurnButtonUpFrmData = NULL;
    }

    return 0;
}

// 0x460940
static int endCombatButtonInit()
{
    int fid;

    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    if (!gInterfaceBarEndButtonsIsVisible) {
        return -1;
    }

    fid = buildFid(6, 107, 0, 0, 0);
    gEndCombatButtonUpFrmData = artLockFrameData(fid, 0, 0, &gEndCombatButtonUpFrmHandle);
    if (gEndCombatButtonUpFrmData == NULL) {
        return -1;
    }

    fid = buildFid(6, 108, 0, 0, 0);
    gEndCombatButtonDownFrmData = artLockFrameData(fid, 0, 0, &gEndCombatButtonDownFrmHandle);
    if (gEndCombatButtonDownFrmData == NULL) {
        return -1;
    }

    gEndCombatButton = buttonCreate(gInterfaceBarWindow, 590, 65, 38, 22, -1, -1, -1, 13, gEndCombatButtonUpFrmData, gEndCombatButtonDownFrmData, NULL, 0);
    if (gEndCombatButton == -1) {
        return -1;
    }

    _win_register_button_disable(gEndCombatButton, gEndCombatButtonUpFrmData, gEndCombatButtonUpFrmData, gEndCombatButtonUpFrmData);
    buttonSetCallbacks(gEndCombatButton, _gsound_med_butt_press, _gsound_med_butt_release);

    return 0;
}

// 0x460A24
static int endCombatButtonFree()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    if (gEndCombatButton != -1) {
        buttonDestroy(gEndCombatButton);
        gEndCombatButton = -1;
    }

    if (gEndCombatButtonDownFrmData != NULL) {
        artUnlock(gEndCombatButtonDownFrmHandle);
        gEndCombatButtonDownFrmHandle = NULL;
        gEndCombatButtonDownFrmData = NULL;
    }

    if (gEndCombatButtonUpFrmData != NULL) {
        artUnlock(gEndCombatButtonUpFrmHandle);
        gEndCombatButtonUpFrmHandle = NULL;
        gEndCombatButtonUpFrmData = NULL;
    }

    return 0;
}

// 0x460AA0
static void interfaceUpdateAmmoBar(int x, int ratio)
{
    if ((ratio & 1) != 0) {
        ratio -= 1;
    }

    unsigned char* dest = gInterfaceWindowBuffer + 640 * 26 + x;

    for (int index = 70; index > ratio; index--) {
        *dest = 14;
        dest += 640;
    }

    while (ratio > 0) {
        *dest = 196;
        dest += 640;

        *dest = 14;
        dest += 640;

        ratio -= 2;
    }

    if (!gInterfaceBarInitialized) {
        Rect rect;
        rect.left = x;
        rect.top = 26;
        rect.right = x + 1;
        rect.bottom = 26 + 70;
        windowRefreshRect(gInterfaceBarWindow, &rect);
    }
}

// 0x460B20
static int _intface_item_reload()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    bool v0 = false;
    while (_item_w_try_reload(gDude, gInterfaceItemStates[gInterfaceCurrentHand].item) != -1) {
        v0 = true;
    }

    interfaceCycleItemAction();
    interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);

    if (!v0) {
        return -1;
    }

    const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_READY, gInterfaceItemStates[gInterfaceCurrentHand].item, HIT_MODE_RIGHT_WEAPON_PRIMARY, NULL);
    soundPlayFile(sfx);

    return 0;
}

// Renders hit points.
//
// [delay] is an animation delay.
// [previousValue] is only meaningful for animation.
// [offset] = 0 - grey, 120 - yellow, 240 - red.
//
// 0x460BA0
static void interfaceRenderCounter(int x, int y, int previousValue, int value, int offset, int delay)
{
    if (value > 999) {
        value = 999;
    } else if (value < -999) {
        value = -999;
    }

    unsigned char* numbers = gNumbersFrmData + offset;
    unsigned char* dest = gInterfaceWindowBuffer + 640 * y;

    unsigned char* downSrc = numbers + 90;
    unsigned char* upSrc = numbers + 99;
    unsigned char* minusSrc = numbers + 108;
    unsigned char* plusSrc = numbers + 114;

    unsigned char* signDest = dest + x;
    unsigned char* hundredsDest = dest + x + 6;
    unsigned char* tensDest = dest + x + 6 + 9;
    unsigned char* onesDest = dest + x + 6 + 9 * 2;

    int normalizedSign;
    int normalizedValue;
    if (gInterfaceBarInitialized || delay == 0) {
        normalizedSign = value >= 0 ? 1 : -1;
        normalizedValue = abs(value);
    } else {
        normalizedSign = previousValue >= 0 ? 1 : -1;
        normalizedValue = previousValue;
    }

    int ones = normalizedValue % 10;
    int tens = (normalizedValue / 10) % 10;
    int hundreds = normalizedValue / 100;

    blitBufferToBuffer(numbers + 9 * hundreds, 9, 17, 360, hundredsDest, 640);
    blitBufferToBuffer(numbers + 9 * tens, 9, 17, 360, tensDest, 640);
    blitBufferToBuffer(numbers + 9 * ones, 9, 17, 360, onesDest, 640);
    blitBufferToBuffer(normalizedSign >= 0 ? plusSrc : minusSrc, 6, 17, 360, signDest, 640);

    if (!gInterfaceBarInitialized) {
        Rect numbersRect = { x, y, x + 33, y + 17 };
        windowRefreshRect(gInterfaceBarWindow, &numbersRect);
        if (delay != 0) {
            int change = value - previousValue >= 0 ? 1 : -1;
            int v14 = previousValue >= 0 ? 1 : -1;
            int v49 = change * v14;
            while (previousValue != value) {
                if ((hundreds | tens | ones) == 0) {
                    v49 = 1;
                }

                blitBufferToBuffer(upSrc, 9, 17, 360, onesDest, 640);
                _mouse_info();
                gameMouseRefresh();
                coreDelay(delay);
                windowRefreshRect(gInterfaceBarWindow, &numbersRect);

                ones += v49;

                if (ones > 9 || ones < 0) {
                    blitBufferToBuffer(upSrc, 9, 17, 360, tensDest, 640);
                    _mouse_info();
                    gameMouseRefresh();
                    coreDelay(delay);
                    windowRefreshRect(gInterfaceBarWindow, &numbersRect);

                    tens += v49;
                    ones -= 10 * v49;
                    if (tens == 10 || tens == -1) {
                        blitBufferToBuffer(upSrc, 9, 17, 360, hundredsDest, 640);
                        _mouse_info();
                        gameMouseRefresh();
                        coreDelay(delay);
                        windowRefreshRect(gInterfaceBarWindow, &numbersRect);

                        hundreds += v49;
                        tens -= 10 * v49;
                        if (hundreds == 10 || hundreds == -1) {
                            hundreds -= 10 * v49;
                        }

                        blitBufferToBuffer(downSrc, 9, 17, 360, hundredsDest, 640);
                        _mouse_info();
                        gameMouseRefresh();
                        coreDelay(delay);
                        windowRefreshRect(gInterfaceBarWindow, &numbersRect);
                    }

                    blitBufferToBuffer(downSrc, 9, 17, 360, tensDest, 640);
                    coreDelay(delay);
                    windowRefreshRect(gInterfaceBarWindow, &numbersRect);
                }

                blitBufferToBuffer(downSrc, 9, 17, 360, onesDest, 640);
                _mouse_info();
                gameMouseRefresh();
                coreDelay(delay);
                windowRefreshRect(gInterfaceBarWindow, &numbersRect);

                previousValue += change;

                blitBufferToBuffer(numbers + 9 * hundreds, 9, 17, 360, hundredsDest, 640);
                blitBufferToBuffer(numbers + 9 * tens, 9, 17, 360, tensDest, 640);
                blitBufferToBuffer(numbers + 9 * ones, 9, 17, 360, onesDest, 640);

                blitBufferToBuffer(previousValue >= 0 ? plusSrc : minusSrc, 6, 17, 360, signDest, 640);
                _mouse_info();
                gameMouseRefresh();
                coreDelay(delay);
                windowRefreshRect(gInterfaceBarWindow, &numbersRect);
            }
        }
    }
}

// 0x461134
static int indicatorBarInit()
{
    int oldFont = fontGetCurrent();

    if (gIndicatorBarWindow != -1) {
        return 0;
    }

    MessageList messageList;
    MessageListItem messageListItem;
    int rc = 0;
    if (!messageListInit(&messageList)) {
        rc = -1;
    }

    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s%s", asc_5186C8, "intrface.msg");

    if (rc != -1) {
        if (!messageListLoad(&messageList, path)) {
            rc = -1;
        }
    }

    if (rc == -1) {
        debugPrint("\nINTRFACE: Error indicator box messages! **\n");
        return -1;
    }

    CacheEntry* indicatorBoxFrmHandle;
    int width;
    int height;
    int indicatorBoxFid = buildFid(6, 126, 0, 0, 0);
    unsigned char* indicatorBoxFrmData = artLockFrameDataReturningSize(indicatorBoxFid, &indicatorBoxFrmHandle, &width, &height);
    if (indicatorBoxFrmData == NULL) {
        debugPrint("\nINTRFACE: Error initializing indicator box graphics! **\n");
        messageListFree(&messageList);
        return -1;
    }

    for (int index = 0; index < INDICATOR_COUNT; index++) {
        IndicatorDescription* indicatorDescription = &(gIndicatorDescriptions[index]);

        indicatorDescription->data = (unsigned char*)internal_malloc(INDICATOR_BOX_WIDTH * INDICATOR_BOX_HEIGHT);
        if (indicatorDescription->data == NULL) {
            debugPrint("\nINTRFACE: Error initializing indicator box graphics! **");

            while (--index >= 0) {
                internal_free(gIndicatorDescriptions[index].data);
            }

            messageListFree(&messageList);
            artUnlock(indicatorBoxFrmHandle);

            return -1;
        }
    }

    fontSetCurrent(101);

    for (int index = 0; index < INDICATOR_COUNT; index++) {
        IndicatorDescription* indicator = &(gIndicatorDescriptions[index]);

        char text[1024];
        strcpy(text, getmsg(&messageList, &messageListItem, indicator->title));

        int color = indicator->isBad ? _colorTable[31744] : _colorTable[992];

        memcpy(indicator->data, indicatorBoxFrmData, INDICATOR_BOX_WIDTH * INDICATOR_BOX_HEIGHT);

        // NOTE: For unknown reason it uses 24 as a height of the box to center
        // the title. One explanation is that these boxes were redesigned, but
        // this value was not changed. On the other hand 24 is
        // [INDICATOR_BOX_HEIGHT] + [INDICATOR_BOX_CONNECTOR_WIDTH]. Maybe just
        // a coincidence. I guess we'll never find out.
        int y = (24 - fontGetLineHeight()) / 2;
        int x = (INDICATOR_BOX_WIDTH - fontGetStringWidth(text)) / 2;
        fontDrawText(indicator->data + INDICATOR_BOX_WIDTH * y + x, text, INDICATOR_BOX_WIDTH, INDICATOR_BOX_WIDTH, color);
    }

    gIndicatorBarIsVisible = true;
    indicatorBarRefresh();

    messageListFree(&messageList);
    artUnlock(indicatorBoxFrmHandle);
    fontSetCurrent(oldFont);

    return 0;
}

// 0x461454
static void interfaceBarFree()
{
    if (gIndicatorBarWindow != -1) {
        windowDestroy(gIndicatorBarWindow);
        gIndicatorBarWindow = -1;
    }

    for (int index = 0; index < INDICATOR_COUNT; index++) {
        IndicatorDescription* indicatorBoxDescription = &(gIndicatorDescriptions[index]);
        if (indicatorBoxDescription->data != NULL) {
            internal_free(indicatorBoxDescription->data);
            indicatorBoxDescription->data = NULL;
        }
    }
}

// NOTE: This function is not referenced in the original code.
//
// 0x4614A0
static void indicatorBarReset()
{
    if (gIndicatorBarWindow != -1) {
        windowDestroy(gIndicatorBarWindow);
        gIndicatorBarWindow = -1;
    }

    gIndicatorBarIsVisible = true;
}

// Updates indicator bar.
//
// 0x4614CC
int indicatorBarRefresh()
{
    if (gInterfaceBarWindow != -1 && gIndicatorBarIsVisible && !_intfaceHidden) {
        for (int index = 0; index < INDICATOR_SLOTS_COUNT; index++) {
            gIndicatorSlots[index] = -1;
        }

        int count = 0;

        if (dudeHasState(DUDE_STATE_SNEAKING)) {
            if (indicatorBarAdd(INDICATOR_SNEAK)) {
                ++count;
            }
        }

        if (dudeHasState(DUDE_STATE_LEVEL_UP_AVAILABLE)) {
            if (indicatorBarAdd(INDICATOR_LEVEL)) {
                ++count;
            }
        }

        if (dudeHasState(DUDE_STATE_ADDICTED)) {
            if (indicatorBarAdd(INDICATOR_ADDICT)) {
                ++count;
            }
        }

        if (critterGetPoison(gDude) > POISON_INDICATOR_THRESHOLD) {
            if (indicatorBarAdd(INDICATOR_POISONED)) {
                ++count;
            }
        }

        if (critterGetRadiation(gDude) > RADATION_INDICATOR_THRESHOLD) {
            if (indicatorBarAdd(INDICATOR_RADIATED)) {
                ++count;
            }
        }

        if (count > 1) {
            qsort(gIndicatorSlots, count, sizeof(*gIndicatorSlots), indicatorBoxCompareByPosition);
        }

        if (gIndicatorBarWindow != -1) {
            windowDestroy(gIndicatorBarWindow);
            gIndicatorBarWindow = -1;
        }

        if (count != 0) {
            Rect interfaceBarWindowRect;
            windowGetRect(gInterfaceBarWindow, &interfaceBarWindowRect);

            gIndicatorBarWindow = windowCreate(interfaceBarWindowRect.left,
                screenGetHeight() - INTERFACE_BAR_HEIGHT - INDICATOR_BOX_HEIGHT,
                (INDICATOR_BOX_WIDTH - INDICATOR_BOX_CONNECTOR_WIDTH) * count,
                INDICATOR_BOX_HEIGHT,
                _colorTable[0],
                0);
            indicatorBarRender(count);
            windowRefresh(gIndicatorBarWindow);
        }

        return count;
    }

    if (gIndicatorBarWindow != -1) {
        windowDestroy(gIndicatorBarWindow);
        gIndicatorBarWindow = -1;
    }

    return 0;
}

// 0x461624
static int indicatorBoxCompareByPosition(const void* a, const void* b)
{
    int indicatorBox1 = *(int*)a;
    int indicatorBox2 = *(int*)b;

    if (indicatorBox1 == indicatorBox2) {
        return 0;
    } else if (indicatorBox1 < indicatorBox2) {
        return -1;
    } else {
        return 1;
    }
}

// Renders indicator boxes into the indicator bar window.
//
// 0x461648
static void indicatorBarRender(int count)
{
    if (gIndicatorBarWindow == -1) {
        return;
    }

    if (count == 0) {
        return;
    }

    int windowWidth = windowGetWidth(gIndicatorBarWindow);
    unsigned char* windowBuffer = windowGetBuffer(gIndicatorBarWindow);

    // The initial number of connections is 2 - one is first box to the screen
    // boundary, the other is female socket (initially empty). Every displayed
    // box adds one more connection (it is "plugged" into previous box and
    // exposes it's own empty female socket).
    int connections = 2;

    // The width of displayed indicator boxes as if there were no connections.
    int unconnectedIndicatorsWidth = 0;

    // The X offset to display next box.
    int x = 0;

    // The first box is connected to the screen boundary, so we have to clamp
    // male connectors on the left.
    int connectorWidthCompensation = INDICATOR_BOX_CONNECTOR_WIDTH;

    for (int index = 0; index < count; index++) {
        int indicator = gIndicatorSlots[index];
        IndicatorDescription* indicatorDescription = &(gIndicatorDescriptions[indicator]);

        blitBufferToBufferTrans(indicatorDescription->data + connectorWidthCompensation,
            INDICATOR_BOX_WIDTH - connectorWidthCompensation,
            INDICATOR_BOX_HEIGHT,
            INDICATOR_BOX_WIDTH,
            windowBuffer + x, windowWidth);

        connectorWidthCompensation = 0;

        unconnectedIndicatorsWidth += INDICATOR_BOX_WIDTH;
        x = unconnectedIndicatorsWidth - INDICATOR_BOX_CONNECTOR_WIDTH * connections;
        connections++;
    }
}

// Adds indicator to the indicator bar.
//
// Returns `true` if indicator was added, or `false` if there is no available
// space in the indicator bar.
//
// 0x4616F0
static bool indicatorBarAdd(int indicator)
{
    for (int index = 0; index < INDICATOR_SLOTS_COUNT; index++) {
        if (gIndicatorSlots[index] == -1) {
            gIndicatorSlots[index] = indicator;
            return true;
        }
    }

    debugPrint("\nINTRFACE: no free bar box slots!\n");

    return false;
}

// 0x461740
bool indicatorBarShow()
{
    bool oldIsVisible = gIndicatorBarIsVisible;
    gIndicatorBarIsVisible = true;

    indicatorBarRefresh();

    return oldIsVisible;
}

// 0x461760
bool indicatorBarHide()
{
    bool oldIsVisible = gIndicatorBarIsVisible;
    gIndicatorBarIsVisible = false;

    indicatorBarRefresh();

    return oldIsVisible;
}
