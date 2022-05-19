#ifndef ELEVATOR_H
#define ELEVATOR_H

#include "art.h"
#include "geometry.h"

// The maximum number of elevator levels.
#define ELEVATOR_LEVEL_MAX (4)

// NOTE: There are two variables which hold background data used in the
// elevator window - [gElevatorBackgroundFrmData] and [gElevatorPanelFrmData].
// For unknown reason they are using -1 to denote that they are not set
// (instead of using NULL).
#define ELEVATOR_BACKGROUND_NULL ((unsigned char*)(-1))

typedef enum Elevator {
    ELEVATOR_BROTHERHOOD_OF_STEEL_MAIN,
    ELEVATOR_BROTHERHOOD_OF_STEEL_SURFACE,
    ELEVATOR_MASTER_UPPER,
    ELEVATOR_MASTER_LOWER,
    ELEVATOR_MILITARY_BASE_UPPER,
    ELEVATOR_MILITARY_BASE_LOWER,
    ELEVATOR_GLOW_UPPER,
    ELEVATOR_GLOW_LOWER,
    ELEVATOR_VAULT_13,
    ELEVATOR_NECROPOLIS,
    ELEVATOR_SIERRA_1,
    ELEVATOR_SIERRA_2,
    ELEVATOR_SIERRA_SERVICE,
    ELEVATOR_COUNT = 24,
} Elevator;

typedef enum ElevatorFrm {
    ELEVATOR_FRM_BUTTON_DOWN,
    ELEVATOR_FRM_BUTTON_UP,
    ELEVATOR_FRM_GAUGE,
    ELEVATOR_FRM_COUNT,
} ElevatorFrm;

typedef struct ElevatorBackground {
    int backgroundFrmId;
    int panelFrmId;
} ElevatorBackground;

typedef struct ElevatorDescription {
    int map;
    int elevation;
    int tile;
} ElevatorDescription;

extern const int gElevatorFrmIds[ELEVATOR_FRM_COUNT];
extern const ElevatorBackground gElevatorBackgrounds[ELEVATOR_COUNT];
extern const int gElevatorLevels[ELEVATOR_COUNT];
extern const ElevatorDescription gElevatorDescriptions[ELEVATOR_COUNT][ELEVATOR_LEVEL_MAX];
extern const char gElevatorLevelLabels[ELEVATOR_COUNT][ELEVATOR_LEVEL_MAX];

extern const char* gElevatorSoundEffects[ELEVATOR_LEVEL_MAX - 1][ELEVATOR_LEVEL_MAX];

extern Size gElevatorFrmSizes[ELEVATOR_FRM_COUNT];
extern int gElevatorBackgroundFrmWidth;
extern int gElevatorBackgroundFrmHeight;
extern int gElevatorPanelFrmWidth;
extern int gElevatorPanelFrmHeight;
extern int gElevatorWindow;
extern CacheEntry* gElevatorFrmHandles[ELEVATOR_FRM_COUNT];
extern CacheEntry* gElevatorBackgroundFrmHandle;
extern CacheEntry* gElevatorPanelFrmHandle;
extern unsigned char* gElevatorWindowBuffer;
extern bool gElevatorWindowIsoWasEnabled;
extern unsigned char* gElevatorFrmData[ELEVATOR_FRM_COUNT];
extern unsigned char* gElevatorBackgroundFrmData;
extern unsigned char* gElevatorPanelFrmData;

int elevatorSelectLevel(int elevator, int* mapPtr, int* elevationPtr, int* tilePtr);
int elevatorWindowInit(int elevator);
void elevatorWindowFree();
int elevatorGetLevelFromKeyCode(int elevator, int keyCode);

#endif /* ELEVATOR_H */
