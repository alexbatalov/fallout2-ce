#ifndef INTERFACE_H
#define INTERFACE_H

#include "db.h"
#include "obj_types.h"

namespace fallout {

#define INDICATOR_BOX_WIDTH 130
#define INDICATOR_BOX_HEIGHT 21

#define INTERFACE_BAR_WIDTH 640
#define INTERFACE_BAR_HEIGHT 100

typedef enum InterfaceItemAction {
    INTERFACE_ITEM_ACTION_DEFAULT = -1,
    INTERFACE_ITEM_ACTION_USE,
    INTERFACE_ITEM_ACTION_PRIMARY,
    INTERFACE_ITEM_ACTION_PRIMARY_AIMING,
    INTERFACE_ITEM_ACTION_SECONDARY,
    INTERFACE_ITEM_ACTION_SECONDARY_AIMING,
    INTERFACE_ITEM_ACTION_RELOAD,
    INTERFACE_ITEM_ACTION_COUNT,
} InterfaceItemAction;

extern int gInterfaceBarWindow;
extern bool gInterfaceBarMode;
extern int gInterfaceBarWidth;
extern bool gInterfaceBarIsCustom;
extern int gInterfaceBarContentOffset;
extern int gInterfaceSidePanelsImageId;
extern bool gInterfaceSidePanelsExtendFromScreenEdge;

int interfaceInit();
void interfaceReset();
void interfaceFree();
int interfaceLoad(File* stream);
int interfaceSave(File* stream);
void interfaceBarHide();
void interfaceBarShow();
void interfaceBarEnable();
void interfaceBarDisable();
bool interfaceBarEnabled();
void interfaceBarRefresh();
void interfaceRenderHitPoints(bool animate);
void interfaceRenderArmorClass(bool animate);
void interfaceRenderActionPoints(int actionPointsLeft, int bonusActionPoints);
int interfaceGetCurrentHitMode(int* hitMode, bool* aiming);
int interfaceUpdateItems(bool animated, int leftItemAction, int rightItemAction);
int interfaceBarSwapHands(bool animated);
int interfaceGetItemActions(int* leftItemAction, int* rightItemAction);
int interfaceCycleItemAction();
void _intface_use_item();
int interfaceGetCurrentHand();
int interfaceGetActiveItem(Object** a1);
int _intface_update_ammo_lights();
void interfaceBarEndButtonsShow(bool animated);
void interfaceBarEndButtonsHide(bool animated);
void interfaceBarEndButtonsRenderGreenLights();
void interfaceBarEndButtonsRenderRedLights();
int indicatorBarRefresh();
bool indicatorBarShow();
bool indicatorBarHide();
bool interface_get_current_attack_mode(int* hit_mode);

unsigned char* customInterfaceBarGetBackgroundImageData();

} // namespace fallout

#endif /* INTERFACE_H */
