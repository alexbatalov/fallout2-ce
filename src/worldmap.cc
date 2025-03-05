#include "worldmap.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "animation.h"
#include "art.h"
#include "automap.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "critter.h"
#include "cycle.h"
#include "db.h"
#include "dbox.h"
#include "debug.h"
#include "display_monitor.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "input.h"
#include "interface.h"
#include "item.h"
#include "kb.h"
#include "memory.h"
#include "mouse.h"
#include "object.h"
#include "palette.h"
#include "party_member.h"
#include "perk.h"
#include "proto_instance.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "sfall_config.h"
#include "sfall_global_scripts.h"
#include "skill.h"
#include "stat.h"
#include "string_parsers.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

#define CITY_NAME_SIZE (40)
#define TILE_WALK_MASK_NAME_SIZE (40)
#define ENTRANCE_LIST_CAPACITY (10)

// Up from 6 to handle `Tartar 3rd Floor 2` and `Livos Living Rooms` sfx
// configuration in Olympus.
#define MAP_AMBIENT_SOUND_EFFECTS_CAPACITY (7)
#define MAP_STARTING_POINTS_CAPACITY (15)

#define SUBTILE_GRID_WIDTH (7)
#define SUBTILE_GRID_HEIGHT (6)

#define ENCOUNTER_ENTRY_SPECIAL (0x01)

#define ENCOUNTER_SUBINFO_DEAD (0x01)

#define WM_WINDOW_DIAL_X (532)
#define WM_WINDOW_DIAL_Y (48)

#define WM_TOWN_LIST_SCROLL_UP_X (480)
#define WM_TOWN_LIST_SCROLL_UP_Y (137)

#define WM_TOWN_LIST_SCROLL_DOWN_X (WM_TOWN_LIST_SCROLL_UP_X)
#define WM_TOWN_LIST_SCROLL_DOWN_Y (152)

#define WM_WINDOW_GLOBE_OVERLAY_X (495)
#define WM_WINDOW_GLOBE_OVERLAY_Y (330)

#define WM_WINDOW_CAR_X (514)
#define WM_WINDOW_CAR_Y (336)

#define WM_WINDOW_CAR_OVERLAY_X (499)
#define WM_WINDOW_CAR_OVERLAY_Y (330)

#define WM_WINDOW_CAR_FUEL_BAR_X (500)
#define WM_WINDOW_CAR_FUEL_BAR_Y (339)
#define WM_WINDOW_CAR_FUEL_BAR_HEIGHT (70)

#define WM_TOWN_WORLD_SWITCH_X (519)
#define WM_TOWN_WORLD_SWITCH_Y (439)

#define WM_TILE_WIDTH (350)
#define WM_TILE_HEIGHT (300)

#define WM_SUBTILE_SIZE (50)

#define WM_WINDOW_WIDTH (640)
#define WM_WINDOW_HEIGHT (480)

#define WM_VIEW_X (22)
#define WM_VIEW_Y (21)
#define WM_VIEW_WIDTH (450)
#define WM_VIEW_HEIGHT (443)

typedef enum EncounterFormationType {
    ENCOUNTER_FORMATION_TYPE_SURROUNDING,
    ENCOUNTER_FORMATION_TYPE_STRAIGHT_LINE,
    ENCOUNTER_FORMATION_TYPE_DOUBLE_LINE,
    ENCOUNTER_FORMATION_TYPE_WEDGE,
    ENCOUNTER_FORMATION_TYPE_CONE,
    ENCOUNTER_FORMATION_TYPE_HUDDLE,
    ENCOUNTER_FORMATION_TYPE_COUNT,
} EncounterFormationType;

typedef enum EncounterFrequencyType {
    ENCOUNTER_FREQUENCY_TYPE_NONE,
    ENCOUNTER_FREQUENCY_TYPE_RARE,
    ENCOUNTER_FREQUENCY_TYPE_UNCOMMON,
    ENCOUNTER_FREQUENCY_TYPE_COMMON,
    ENCOUNTER_FREQUENCY_TYPE_FREQUENT,
    ENCOUNTER_FREQUENCY_TYPE_FORCED,
    ENCOUNTER_FREQUENCY_TYPE_COUNT,
} EncounterFrequencyType;

typedef enum EncounterSceneryType {
    ENCOUNTER_SCENERY_TYPE_NONE,
    ENCOUNTER_SCENERY_TYPE_LIGHT,
    ENCOUNTER_SCENERY_TYPE_NORMAL,
    ENCOUNTER_SCENERY_TYPE_HEAVY,
    ENCOUNTER_SCENERY_TYPE_COUNT,
} EncounterSceneryType;

typedef enum EncounterSituation {
    ENCOUNTER_SITUATION_NOTHING,
    ENCOUNTER_SITUATION_AMBUSH,
    ENCOUNTER_SITUATION_FIGHTING,
    ENCOUNTER_SITUATION_AND,
    ENCOUNTER_SITUATION_COUNT,
} EncounterSituation;

typedef enum EncounterLogicalOperator {
    ENCOUNTER_LOGICAL_OPERATOR_NONE,
    ENCOUNTER_LOGICAL_OPERATOR_AND,
    ENCOUNTER_LOGICAL_OPERATOR_OR,
} EncounterLogicalOperator;

typedef enum EncounterConditionType {
    ENCOUNTER_CONDITION_TYPE_NONE = 0,
    ENCOUNTER_CONDITION_TYPE_GLOBAL = 1,
    ENCOUNTER_CONDITION_TYPE_NUMBER_OF_CRITTERS = 2,
    ENCOUNTER_CONDITION_TYPE_RANDOM = 3,
    ENCOUNTER_CONDITION_TYPE_PLAYER = 4,
    ENCOUNTER_CONDITION_TYPE_DAYS_PLAYED = 5,
    ENCOUNTER_CONDITION_TYPE_TIME_OF_DAY = 6,
} EncounterConditionType;

typedef enum EncounterConditionalOperator {
    ENCOUNTER_CONDITIONAL_OPERATOR_NONE,
    ENCOUNTER_CONDITIONAL_OPERATOR_EQUAL,
    ENCOUNTER_CONDITIONAL_OPERATOR_NOT_EQUAL,
    ENCOUNTER_CONDITIONAL_OPERATOR_LESS_THAN,
    ENCOUNTER_CONDITIONAL_OPERATOR_GREATER_THAN,
    ENCOUNTER_CONDITIONAL_OPERATOR_COUNT,
} EncounterConditionalOperator;

typedef enum EncounterRatioMode {
    ENCOUNTER_RATIO_MODE_USE_RATIO,
    ENCOUNTER_RATIO_MODE_SINGLE,
} EncounterRatioMode;

typedef enum Daytime {
    DAY_PART_MORNING,
    DAY_PART_AFTERNOON,
    DAY_PART_NIGHT,
    DAY_PART_COUNT,
} Daytime;

typedef enum LockState {
    LOCK_STATE_UNLOCKED,
    LOCK_STATE_LOCKED,
} LockState;

typedef enum SubtileState {
    SUBTILE_STATE_UNKNOWN,
    SUBTILE_STATE_KNOWN,
    SUBTILE_STATE_VISITED,
} SubtileState;

typedef enum SubtileFill {
    SUBTILE_FILL_NONE,
    SUBTILE_FILL_N,
    SUBTILE_FILL_S,
    SUBTILE_FILL_E,
    SUBTILE_FILL_W,
    SUBTILE_FILL_NW,
    SUBTILE_FILL_NE,
    SUBTILE_FILL_SW,
    SUBTILE_FILL_SE,
    SUBTILE_FILL_COUNT,
} SubtileFill;

typedef enum WorldMapEncounterFrm {
    WORLD_MAP_ENCOUNTER_FRM_RANDOM_BRIGHT,
    WORLD_MAP_ENCOUNTER_FRM_RANDOM_DARK,
    WORLD_MAP_ENCOUNTER_FRM_SPECIAL_BRIGHT,
    WORLD_MAP_ENCOUNTER_FRM_SPECIAL_DARK,
    WORLD_MAP_ENCOUNTER_FRM_COUNT,
} WorldMapEncounterFrm;

typedef enum WorldmapArrowFrm {
    WORLDMAP_ARROW_FRM_NORMAL,
    WORLDMAP_ARROW_FRM_PRESSED,
    WORLDMAP_ARROW_FRM_COUNT,
} WorldmapArrowFrm;

typedef enum CitySize {
    CITY_SIZE_SMALL,
    CITY_SIZE_MEDIUM,
    CITY_SIZE_LARGE,
    CITY_SIZE_COUNT,
} CitySize;

typedef struct EntranceInfo {
    int state;
    int x;
    int y;
    int map;
    int elevation;
    int tile;
    int rotation;
} EntranceInfo;

typedef struct CityInfo {
    char name[CITY_NAME_SIZE];
    int areaId;
    int x;
    int y;
    int size;
    int state;
    int lockState;
    int visitedState;
    int mapFid;
    int labelFid;
    int entrancesLength;
    EntranceInfo entrances[ENTRANCE_LIST_CAPACITY];
} CityInfo;

typedef struct MapAmbientSoundEffectInfo {
    char name[40];
    int chance;
} MapAmbientSoundEffectInfo;

typedef struct MapStartPointInfo {
    int elevation;
    int tile;
    int rotation;
} MapStartPointInfo;

typedef struct MapInfo {
    char lookupName[40];
    int field_28;
    int field_2C;
    char mapFileName[40];
    char music[40];
    int flags;
    int ambientSoundEffectsLength;
    MapAmbientSoundEffectInfo ambientSoundEffects[MAP_AMBIENT_SOUND_EFFECTS_CAPACITY];
    int startPointsLength;
    MapStartPointInfo startPoints[MAP_STARTING_POINTS_CAPACITY];
} MapInfo;

typedef struct Terrain {
    char lookupName[40];
    int difficulty;
    int mapsLength;
    int maps[20];
} Terrain;

typedef struct EncounterConditionEntry {
    int type;
    int conditionalOperator;
    int param;
    int value;
} EncounterConditionEntry;

typedef struct EncounterCondition {
    int entriesLength;
    EncounterConditionEntry entries[3];
    int logicalOperators[2];
} EncounterCondition;

typedef struct EncounterTableSubEntry {
    int minimumCount;
    int maximumCount;
    int encounterIndex;
    int situation;
} EncounterTableSubEntry;

typedef struct EncounterTableEntry {
    int flags;
    int map;
    int scenery;
    int chance;
    int counter;
    EncounterCondition condition;
    int subEntiesLength;
    EncounterTableSubEntry subEntries[6];
} EncounterTableEntry;

typedef struct EncounterTable {
    char lookupName[40];
    int index;
    int mapsLength;
    int maps[6];
    int field_48;
    int entriesLength;
    EncounterTableEntry entries[41];
} EncounterTable;

typedef struct EncounterItem {
    int pid;
    int minimumQuantity;
    int maximumQuantity;
    bool isEquipped;
} EncounterItem;

typedef struct EncounterEntry {
    char field_0[40];
    int field_28;
    int ratioMode;
    int ratio;
    int pid;
    int flags;
    int distance;
    int tile;
    int itemsLength;
    EncounterItem items[10];
    int team;
    int scriptIdx;
    EncounterCondition condition;
} EncounterEntry;

typedef struct Encounter {
    char name[40];
    int position;
    int spacing;
    int distance;
    int entriesLength;
    EncounterEntry entries[10];
} Encounter;

typedef struct SubtileInfo {
    int terrain;
    int fill;
    int encounterChance[DAY_PART_COUNT];
    int encounterType;
    int state;
} SubtileInfo;

// A worldmap tile is 7x6 area, thus consisting of 42 individual subtiles.
typedef struct TileInfo {
    int fid;
    CacheEntry* handle;
    unsigned char* data;
    char walkMaskName[TILE_WALK_MASK_NAME_SIZE];
    unsigned char* walkMaskData;
    int encounterDifficultyModifier;
    SubtileInfo subtiles[SUBTILE_GRID_HEIGHT][SUBTILE_GRID_WIDTH];
} TileInfo;

typedef struct CitySizeDescription {
    int fid;
    FrmImage frmImage;
} CitySizeDescription;

typedef struct WmGenData {
    bool mousePressed;
    bool didMeetFrankHorrigan;

    int currentAreaId;
    int worldPosX;
    int worldPosY;
    SubtileInfo* currentSubtile;

    int dword_672E18;

    bool isWalking;
    int walkDestinationX;
    int walkDestinationY;
    int walkDistance;
    int walkLineDelta;
    int walkLineDeltaMainAxisStep;
    int walkLineDeltaCrossAxisStep;
    int walkWorldPosMainAxisStepX;
    int walkWorldPosCrossAxisStepX;
    int walkWorldPosMainAxisStepY;
    int walkWorldPosCrossAxisStepY;

    bool encounterIconIsVisible;
    int encounterMapId;
    int encounterTableId;
    int encounterEntryId;
    int encounterCursorId;

    int oldWorldPosX;
    int oldWorldPosY;

    bool isInCar;
    int currentCarAreaId;
    int carFuel;

    CacheEntry* carImageFrmHandle;
    Art* carImageFrm;
    int carImageFrmWidth;
    int carImageFrmHeight;
    int carImageCurrentFrameIndex;

    FrmImage hotspotNormalFrmImage;
    FrmImage hotspotPressedFrmImage;

    FrmImage destinationMarkerFrmImage;
    FrmImage locationMarkerFrmImage;

    FrmImage encounterCursorFrmImages[WORLD_MAP_ENCOUNTER_FRM_COUNT];

    int viewportMaxX;
    int viewportMaxY;

    FrmImage tabsBackgroundFrmImage;
    int tabsOffsetY;

    FrmImage tabsBorderFrmImage;

    CacheEntry* dialFrmHandle;
    int dialFrmWidth;
    int dialFrmHeight;
    int dialFrmCurrentFrameIndex;
    Art* dialFrm;

    FrmImage carOverlayFrmImage;
    FrmImage globeOverlayFrmImage;

    int oldTabsOffsetY;
    int tabsScrollingDelta;

    FrmImage redButtonNormalFrmImage;
    FrmImage redButtonPressedFrmImage;

    FrmImage scrollUpButtonFrmImages[WORLDMAP_ARROW_FRM_COUNT];
    FrmImage scrollDownButtonFrmImages[WORLDMAP_ARROW_FRM_COUNT];

    FrmImage monthsFrmImage;
    FrmImage numbersFrmImage;

    int oldFont;
} WmGenData;

static void wmSetFlags(int* flagsPtr, int flag, int value);
static int wmGenDataInit();
static int wmGenDataReset();
static int wmWorldMapSaveTempData();
static int wmWorldMapLoadTempData();
static int wmConfigInit();
static int wmReadEncounterType(Config* config, char* lookupName, char* sectionKey);
static int wmParseEncounterTableIndex(EncounterTableEntry* encounterTableEntry, char* string);
static int wmParseEncounterSubEncStr(EncounterTableEntry* encounterTableEntry, char** stringPtr);
static int wmParseFindSubEncTypeMatch(char* str, int* valuePtr);
static int wmFindEncBaseTypeMatch(char* str, int* valuePtr);
static int wmReadEncBaseType(char* name, int* valuePtr);
static int wmParseEncBaseSubTypeStr(EncounterEntry* encounterEntry, char** stringPtr);
static int wmEncBaseTypeSlotInit(Encounter* encounter);
static int wmEncBaseSubTypeSlotInit(EncounterEntry* encounterEntry);
static int wmEncounterSubEncSlotInit(EncounterTableSubEntry* encounterTableSubEntry);
static int wmEncounterTypeSlotInit(EncounterTableEntry* encounterTableEntry);
static int wmEncounterTableSlotInit(EncounterTable* encounterTable);
static int wmTileSlotInit(TileInfo* tile);
static int wmTerrainTypeSlotInit(Terrain* terrain);
static int wmConditionalDataInit(EncounterCondition* condition);
static int wmParseTerrainTypes(Config* config, char* string);
static int wmParseTerrainRndMaps(Config* config, Terrain* terrain);
static int wmParseSubTileInfo(TileInfo* tile, int row, int column, char* string);
static int wmParseFindEncounterTypeMatch(char* string, int* valuePtr);
static int wmParseFindTerrainTypeMatch(char* string, int* valuePtr);
static int wmParseEncounterItemType(char** stringPtr, EncounterItem* encounterItem, int* itemCountPtr, const char* delim);
static int wmParseItemType(char* string, EncounterItem* encounterItem);
static int wmParseConditional(char** stringPtr, const char* a2, EncounterCondition* condition);
static int wmParseSubConditional(char** stringPtr, const char* a2, int* typePtr, int* operatorPtr, int* paramPtr, int* valuePtr);
static int wmParseConditionalEval(char** stringPtr, int* conditionalOperatorPtr);
static int wmAreaSlotInit(CityInfo* area);
static int wmAreaInit();
static int wmParseFindMapIdxMatch(char* string, int* valuePtr);
static int wmEntranceSlotInit(EntranceInfo* entrance);
static int wmMapSlotInit(MapInfo* map);
static int wmMapInit();
static int wmRStartSlotInit(MapStartPointInfo* rsp);
static int wmMatchEntranceFromMap(int areaIdx, int mapIdx, int* entranceIdxPtr);
static int wmMatchEntranceElevFromMap(int areaIdx, int mapIdx, int elevation, int* entranceIdxPtr);
static int wmMatchAreaFromMap(int mapIdx, int* areaIdxPtr);
static int wmWorldMapFunc(int a1);
static int wmInterfaceCenterOnParty();
static void wmCheckGameEvents();
static int wmRndEncounterOccurred();
static int wmPartyFindCurSubTile();
static int wmFindCurSubTileFromPos(int x, int y, SubtileInfo** subtilePtr);
static int wmFindCurTileFromPos(int x, int y, TileInfo** tilePtr);
static int wmRndEncounterPick();
static int wmSetupCritterObjs(int encounterIndex, Object** critterPtr, int critterCount);
static int wmSetupRndNextTileNumInit(Encounter* encounter);
static int wmSetupRndNextTileNum(Encounter* encounter, EncounterEntry* encounterEntry, int* tilePtr);
static bool wmEvalConditional(EncounterCondition* encounterCondition, int* critterCountPtr);
static bool wmEvalSubConditional(int operand1, int condionalOperator, int operand2);
static bool wmGameTimeIncrement(int ticksToAdd);
static int wmGrabTileWalkMask(int tileIdx);
static bool wmWorldPosInvalid(int x, int y);
static void wmPartyInitWalking(int x, int y);
static void wmPartyWalkingStep();
static void wmInterfaceScrollTabsStart(int delta);
static void wmInterfaceScrollTabsStop();
static void wmInterfaceScrollTabsUpdate();
static int wmInterfaceInit();
static int wmInterfaceExit();
static int wmInterfaceScroll(int dx, int dy, bool* successPtr);
static int wmInterfaceScrollPixel(int stepX, int stepY, int dx, int dy, bool* success, bool shouldRefresh);
static void wmMouseBkProc();
static int wmMarkSubTileOffsetVisited(int tile, int subtileX, int subtileY, int offsetX, int offsetY);
static int wmMarkSubTileOffsetKnown(int tile, int subtileX, int subtileY, int offsetX, int offsetY);
static int wmMarkSubTileOffsetVisitedFunc(int tile, int subtileX, int subtileY, int offsetX, int offsetY, int subtileState);
static void wmMarkSubTileRadiusVisited(int x, int y);
static int wmTileGrabArt(int tileIdx);
static int wmInterfaceRefresh();
static void wmInterfaceRefreshDate(bool shouldRefreshWindow);
static int wmMatchWorldPosToArea(int x, int y, int* areaIdxPtr);
static int wmInterfaceDrawCircleOverlay(CityInfo* cityInfo, CitySizeDescription* citySizeInfo, unsigned char* buffer, int x, int y);
static void wmInterfaceDrawSubTileRectFogged(unsigned char* dest, int width, int height, int pitch);
static int wmInterfaceDrawSubTileList(TileInfo* tileInfo, int column, int row, int x, int y, int a6);
static int wmDrawCursorStopped();
static bool wmCursorIsVisible();
static int wmGetAreaName(CityInfo* city, char* name);
static void wmMarkAllSubTiles(int state);
static int wmTownMapFunc(int* mapIdxPtr);
static int wmTownMapInit();
static int wmTownMapRefresh();
static int wmTownMapExit();
static int wmRefreshInterfaceOverlay(bool shouldRefreshWindow);
static void wmInterfaceRefreshCarFuel();
static int wmRefreshTabs();
static int wmMakeTabsLabelList(int** quickDestinationsPtr, int* quickDestinationsLengthPtr);
static int wmTabsCompareNames(const void* a1, const void* a2);
static int wmFreeTabsLabelList(int** quickDestinationsListPtr, int* quickDestinationsLengthPtr);
static void wmRefreshInterfaceDial(bool shouldRefreshWindow);
static void wmInterfaceDialSyncTime(bool shouldRefreshWindow);
static int wmAreaFindFirstValidMap(int* mapIdxPtr);
static void wmFadeOut();
static void wmFadeIn();
static void wmFadeReset();
static void wmBlinkRndEncounterIcon(bool special);

// 0x4BC860
static const int _can_rest_here[ELEVATION_COUNT] = {
    MAP_CAN_REST_ELEVATION_0,
    MAP_CAN_REST_ELEVATION_1,
    MAP_CAN_REST_ELEVATION_2,
};

// 0x4BC86C
static const int gDayPartEncounterFrequencyModifiers[DAY_PART_COUNT] = {
    40,
    30,
    0,
};

// 0x4BC878
static const char* gWorldmapEncDefaultMsg[2] = {
    "You detect something up ahead.",
    "Do you wish to encounter it?",
};

// 0x4BC880
static MessageListItem gWorldmapMessageListItem;

// 0x50EE44
static char _aCricket[] = "cricket";

// 0x50EE4C
static char _aCricket1[] = "cricket1";

// 0x51DD88
static const char* wmStateStrs[2] = {
    "off",
    "on"
};

// 0x51DD90
static const char* wmYesNoStrs[2] = {
    "no",
    "yes",
};

// 0x51DD98
static const char* wmFreqStrs[ENCOUNTER_FREQUENCY_TYPE_COUNT] = {
    "none",
    "rare",
    "uncommon",
    "common",
    "frequent",
    "forced",
};

// 0x51DDB0
static const char* wmFillStrs[SUBTILE_FILL_COUNT] = {
    "no_fill",
    "fill_n",
    "fill_s",
    "fill_e",
    "fill_w",
    "fill_nw",
    "fill_ne",
    "fill_sw",
    "fill_se",
};

// 0x51DDD4
static const char* wmSceneryStrs[ENCOUNTER_SCENERY_TYPE_COUNT] = {
    "none",
    "light",
    "normal",
    "heavy",
};

// 0x51DDE4
static Terrain* wmTerrainTypeList = nullptr;

// 0x51DDE8
static int wmMaxTerrainTypes = 0;

// 0x51DDEC
static TileInfo* wmTileInfoList = nullptr;

// 0x51DDF0
static int wmMaxTileNum = 0;

// The width of worldmap grid in tiles.
//
// There is no separate variable for grid height, instead its calculated as
// [wmMaxTileNum] / [gWorldmapTilesGridWidth].
//
// num_horizontal_tiles
// 0x51DDF4
static int wmNumHorizontalTiles = 0;

// 0x51DDF8
static CityInfo* wmAreaInfoList = nullptr;

// 0x51DDFC
static int wmMaxAreaNum = 0;

// 0x51DE00
static const char* wmAreaSizeStrs[CITY_SIZE_COUNT] = {
    "small",
    "medium",
    "large",
};

// 0x51DE0C
static MapInfo* wmMapInfoList = nullptr;

// 0x51DE10
static int wmMaxMapNum = 0;

// 0x51DE14
static int wmBkWin = -1;

// 0x51DE24
static unsigned char* wmBkWinBuf = nullptr;

// 0x51DE2C
static int wmWorldOffsetX = 0;

// 0x51DE30
static int wmWorldOffsetY = 0;

// 0x51DE34
unsigned char* circleBlendTable = nullptr;

// 0x51DE38
static int wmInterfaceWasInitialized = 0;

// 0x51DE3C
static const char* wmEncOpStrs[ENCOUNTER_SITUATION_COUNT] = {
    "nothing",
    "ambush",
    "fighting",
    "and",
};

// 0x51DE4C
static const char* wmConditionalOpStrs[ENCOUNTER_CONDITIONAL_OPERATOR_COUNT] = {
    "_",
    "==",
    "!=",
    "<",
    ">",
};

// 0x51DE64
static const char* wmConditionalQualifierStrs[2] = {
    "and",
    "or",
};

// 0x51DE6C
static const char* wmFormationStrs[ENCOUNTER_FORMATION_TYPE_COUNT] = {
    "surrounding",
    "straight_line",
    "double_line",
    "wedge",
    "cone",
    "huddle",
};

// 0x51DE84
static const int wmRndCursorFids[WORLD_MAP_ENCOUNTER_FRM_COUNT] = {
    154,
    155,
    438,
    439,
};

// 0x51DE94
static int* wmLabelList = nullptr;

// 0x51DE98
static int wmLabelCount = 0;

// 0x51DE9C
static int wmTownMapCurArea = -1;

// 0x51DEA0
static unsigned int wmLastRndTime = 0;

// 0x51DEA4
static int wmRndIndex = 0;

// 0x51DEA8
static int wmRndCallCount = 0;

// 0x51DEAC
static int _terrainCounter = 1;

// 0x51DEC8
static char* wmRemapSfxList[2] = {
    _aCricket,
    _aCricket1,
};

// 0x672DB8
static int wmRndTileDirs[2];

// 0x672DC0
static int wmRndCenterTiles[2];

// 0x672DC8
static int wmRndCenterRotations[2];

// 0x672DD0
static int wmRndRotOffsets[2];

// Buttons for city entrances.
//
// 0x672DD8
static int wmTownMapButtonId[ENTRANCE_LIST_CAPACITY];

// NOTE: There are no symbols in |mapper2.exe| for the range between |wmGenData|
// and |wmMsgFile| implying everything in between are fields of the large
// struct.
//
// 0x672E00
static WmGenData wmGenData;

// worldmap.msg
//
// 0x672FB0
static MessageList wmMsgFile;

// 0x672FB8
static int wmFreqValues[6];

// 0x672FD0
static int wmRndOriginalCenterTile;

// worldmap.txt
//
// 0x672FD4
static Config* pConfigCfg;

// 0x672FD8
static int wmTownMapSubButtonIds[7];

// 0x672FF4
static Encounter* wmEncBaseTypeList;

// 0x672FF8
static CitySizeDescription wmSphereData[CITY_SIZE_COUNT];

// 0x673034
static EncounterTable* wmEncounterTableList;

// Number of enc_base_types.
//
// 0x673038
static int wmMaxEncBaseTypes;

// 0x67303C
static int wmMaxEncounterInfoTables;

static bool gTownMapHotkeysFix;
static double gGameTimeIncRemainder = 0.0;
static FrmImage _backgroundFrmImage;
static FrmImage _townFrmImage;
static bool wmFaded = false;
static int wmForceEncounterMapId = -1;
static unsigned int wmForceEncounterFlags = 0;

static inline bool cityIsValid(int city)
{
    return city >= 0 && city < wmMaxAreaNum;
}

// 0x4BC890
static void wmSetFlags(int* flagsPtr, int flag, int value)
{
    if (value) {
        *flagsPtr |= flag;
    } else {
        *flagsPtr &= ~flag;
    }
}

// 0x4BC89C
int wmWorldMap_init()
{
    char path[COMPAT_MAX_PATH];

    if (wmGenDataInit() == -1) {
        return -1;
    }

    if (!messageListInit(&wmMsgFile)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "worldmap.msg");

    if (!messageListLoad(&wmMsgFile, path)) {
        return -1;
    }

    if (wmConfigInit() == -1) {
        return -1;
    }

    wmGenData.viewportMaxX = WM_TILE_WIDTH * wmNumHorizontalTiles - WM_VIEW_WIDTH;
    wmGenData.viewportMaxY = WM_TILE_HEIGHT * (wmMaxTileNum / wmNumHorizontalTiles) - WM_VIEW_HEIGHT;
    circleBlendTable = _getColorBlendTable(_colorTable[992]);

    wmMarkSubTileRadiusVisited(wmGenData.worldPosX, wmGenData.worldPosY);
    wmWorldMapSaveTempData();

    // SFALL
    gTownMapHotkeysFix = true;
    configGetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_TOWN_MAP_HOTKEYS_FIX_KEY, &gTownMapHotkeysFix);

    // CE: City size fids should be initialized during startup. They are used
    // during |wmTeleportToArea| to calculate worldmap position when jumping
    // from Temple to Arroyo - before giving a chance to |wmInterfaceInit| to
    // initialize it.
    for (int citySize = 0; citySize < CITY_SIZE_COUNT; citySize++) {
        CitySizeDescription* citySizeDescription = &(wmSphereData[citySize]);
        citySizeDescription->fid = buildFid(OBJ_TYPE_INTERFACE, 336 + citySize, 0, 0, 0);
    }

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_WORLDMAP, &wmMsgFile);

    return 0;
}

// 0x4BC984
static int wmGenDataInit()
{
    wmGenData.didMeetFrankHorrigan = false;
    wmGenData.currentAreaId = -1;
    wmGenData.worldPosX = 173;
    wmGenData.worldPosY = 122;
    wmGenData.currentSubtile = nullptr;
    wmGenData.dword_672E18 = 0;
    wmGenData.isWalking = false;
    wmGenData.walkDestinationX = -1;
    wmGenData.walkDestinationY = -1;
    wmGenData.walkDistance = 0;
    wmGenData.walkLineDelta = 0;
    wmGenData.walkLineDeltaMainAxisStep = 0;
    wmGenData.walkLineDeltaCrossAxisStep = 0;
    wmGenData.walkWorldPosMainAxisStepX = 0;
    wmGenData.walkWorldPosMainAxisStepY = 0;
    wmGenData.walkWorldPosCrossAxisStepY = 0;
    wmGenData.encounterIconIsVisible = false;
    wmGenData.encounterMapId = -1;
    wmGenData.encounterTableId = -1;
    wmGenData.encounterEntryId = -1;
    wmGenData.encounterCursorId = -1;
    wmGenData.oldWorldPosX = 0;
    wmGenData.oldWorldPosY = 0;
    wmGenData.isInCar = false;
    wmGenData.currentCarAreaId = -1;
    wmGenData.carFuel = CAR_FUEL_MAX;
    wmGenData.carImageFrmHandle = INVALID_CACHE_ENTRY;
    wmGenData.carImageFrmWidth = 0;
    wmGenData.carImageFrmHeight = 0;
    wmGenData.carImageCurrentFrameIndex = 0;
    wmGenData.mousePressed = false;
    wmGenData.walkWorldPosCrossAxisStepX = 0;
    wmGenData.carImageFrm = nullptr;

    wmGenData.viewportMaxY = 0;
    wmGenData.tabsOffsetY = 0;
    wmGenData.dialFrmHandle = INVALID_CACHE_ENTRY;
    wmGenData.dialFrm = nullptr;
    wmGenData.dialFrmWidth = 0;
    wmGenData.dialFrmHeight = 0;
    wmGenData.dialFrmCurrentFrameIndex = 0;
    wmGenData.oldTabsOffsetY = 0;
    wmGenData.tabsScrollingDelta = 0;
    wmGenData.viewportMaxX = 0;

    wmForceEncounterMapId = -1;
    wmForceEncounterFlags = 0;

    return 0;
}

// 0x4BCBFC
static int wmGenDataReset()
{
    wmGenData.didMeetFrankHorrigan = false;
    wmGenData.currentSubtile = nullptr;
    wmGenData.dword_672E18 = 0;
    wmGenData.isWalking = false;
    wmGenData.walkDistance = 0;
    wmGenData.walkLineDelta = 0;
    wmGenData.walkLineDeltaMainAxisStep = 0;
    wmGenData.walkLineDeltaCrossAxisStep = 0;
    wmGenData.walkWorldPosMainAxisStepX = 0;
    wmGenData.walkWorldPosMainAxisStepY = 0;
    wmGenData.walkWorldPosCrossAxisStepY = 0;
    wmGenData.encounterIconIsVisible = false;
    wmGenData.mousePressed = false;
    wmGenData.currentAreaId = -1;
    wmGenData.worldPosX = 173;
    wmGenData.worldPosY = 122;
    wmGenData.walkDestinationX = -1;
    wmGenData.walkDestinationY = -1;
    wmGenData.encounterMapId = -1;
    wmGenData.encounterTableId = -1;
    wmGenData.encounterEntryId = -1;
    wmGenData.encounterCursorId = -1;
    wmGenData.currentCarAreaId = -1;
    wmGenData.carFuel = CAR_FUEL_MAX;
    wmGenData.carImageFrmHandle = INVALID_CACHE_ENTRY;
    wmGenData.dialFrmHandle = INVALID_CACHE_ENTRY;
    wmGenData.walkWorldPosCrossAxisStepX = 0;
    wmGenData.oldWorldPosX = 0;
    wmGenData.oldWorldPosY = 0;
    wmGenData.isInCar = false;
    wmGenData.carImageFrmWidth = 0;
    wmGenData.carImageFrmHeight = 0;
    wmGenData.carImageCurrentFrameIndex = 0;
    wmGenData.tabsOffsetY = 0;
    wmGenData.dialFrm = nullptr;
    wmGenData.dialFrmWidth = 0;
    wmGenData.dialFrmHeight = 0;
    wmGenData.dialFrmCurrentFrameIndex = 0;
    wmGenData.oldTabsOffsetY = 0;
    wmGenData.tabsScrollingDelta = 0;
    wmGenData.carImageFrm = nullptr;

    wmMarkSubTileRadiusVisited(wmGenData.worldPosX, wmGenData.worldPosY);

    wmForceEncounterMapId = -1;
    wmForceEncounterFlags = 0;

    return 0;
}

// 0x4BCE00
void wmWorldMap_exit()
{
    if (wmTerrainTypeList != nullptr) {
        internal_free(wmTerrainTypeList);
        wmTerrainTypeList = nullptr;
    }

    if (wmTileInfoList) {
        internal_free(wmTileInfoList);
        wmTileInfoList = nullptr;
    }

    wmNumHorizontalTiles = 0;
    wmMaxTileNum = 0;

    if (wmEncounterTableList != nullptr) {
        internal_free(wmEncounterTableList);
        wmEncounterTableList = nullptr;
    }

    wmMaxEncounterInfoTables = 0;

    if (wmEncBaseTypeList != nullptr) {
        internal_free(wmEncBaseTypeList);
        wmEncBaseTypeList = nullptr;
    }

    wmMaxEncBaseTypes = 0;

    if (wmAreaInfoList != nullptr) {
        internal_free(wmAreaInfoList);
        wmAreaInfoList = nullptr;
    }

    wmMaxAreaNum = 0;

    if (wmMapInfoList != nullptr) {
        internal_free(wmMapInfoList);
    }

    wmMaxMapNum = 0;

    if (circleBlendTable != nullptr) {
        _freeColorBlendTable(_colorTable[992]);
        circleBlendTable = nullptr;
    }

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_WORLDMAP, nullptr);
    messageListFree(&wmMsgFile);
}

// 0x4BCEF8
int wmWorldMap_reset()
{
    wmWorldOffsetX = 0;
    wmWorldOffsetY = 0;

    // CE: Fix Pathfinder perk.
    gGameTimeIncRemainder = 0.0;

    wmWorldMapLoadTempData();
    wmMarkAllSubTiles(0);

    return wmGenDataReset();
}

// 0x4BCF28
int wmWorldMap_save(File* stream)
{
    int i;
    int j;
    int k;
    EncounterTable* encounter_table;
    EncounterTableEntry* encounter_entry;

    if (fileWriteBool(stream, wmGenData.didMeetFrankHorrigan) == -1) return -1;
    if (fileWriteInt32(stream, wmGenData.currentAreaId) == -1) return -1;
    if (fileWriteInt32(stream, wmGenData.worldPosX) == -1) return -1;
    if (fileWriteInt32(stream, wmGenData.worldPosY) == -1) return -1;
    if (fileWriteBool(stream, wmGenData.encounterIconIsVisible) == -1) return -1;
    if (fileWriteInt32(stream, wmGenData.encounterMapId) == -1) return -1;
    if (fileWriteInt32(stream, wmGenData.encounterTableId) == -1) return -1;
    if (fileWriteInt32(stream, wmGenData.encounterEntryId) == -1) return -1;
    if (fileWriteBool(stream, wmGenData.isInCar) == -1) return -1;
    if (fileWriteInt32(stream, wmGenData.currentCarAreaId) == -1) return -1;
    if (fileWriteInt32(stream, wmGenData.carFuel) == -1) return -1;
    if (fileWriteInt32(stream, wmMaxAreaNum) == -1) return -1;

    for (int areaIdx = 0; areaIdx < wmMaxAreaNum; areaIdx++) {
        CityInfo* cityInfo = &(wmAreaInfoList[areaIdx]);
        if (fileWriteInt32(stream, cityInfo->x) == -1) return -1;
        if (fileWriteInt32(stream, cityInfo->y) == -1) return -1;
        if (fileWriteInt32(stream, cityInfo->state) == -1) return -1;
        if (fileWriteInt32(stream, cityInfo->visitedState) == -1) return -1;
        if (fileWriteInt32(stream, cityInfo->entrancesLength) == -1) return -1;

        for (int entranceIdx = 0; entranceIdx < cityInfo->entrancesLength; entranceIdx++) {
            EntranceInfo* entrance = &(cityInfo->entrances[entranceIdx]);
            if (fileWriteInt32(stream, entrance->state) == -1) return -1;
        }
    }

    if (fileWriteInt32(stream, wmMaxTileNum) == -1) return -1;
    if (fileWriteInt32(stream, wmNumHorizontalTiles) == -1) return -1;

    for (int tileIndex = 0; tileIndex < wmMaxTileNum; tileIndex++) {
        TileInfo* tileInfo = &(wmTileInfoList[tileIndex]);

        for (int column = 0; column < SUBTILE_GRID_HEIGHT; column++) {
            for (int row = 0; row < SUBTILE_GRID_WIDTH; row++) {
                SubtileInfo* subtile = &(tileInfo->subtiles[column][row]);

                if (fileWriteInt32(stream, subtile->state) == -1) return -1;
            }
        }
    }

    k = 0;
    for (i = 0; i < wmMaxEncounterInfoTables; i++) {
        encounter_table = &(wmEncounterTableList[i]);

        for (j = 0; j < encounter_table->entriesLength; j++) {
            encounter_entry = &(encounter_table->entries[j]);

            if (encounter_entry->counter != -1) {
                k++;
            }
        }
    }

    if (fileWriteInt32(stream, k) == -1) return -1;

    for (i = 0; i < wmMaxEncounterInfoTables; i++) {
        encounter_table = &(wmEncounterTableList[i]);

        for (j = 0; j < encounter_table->entriesLength; j++) {
            encounter_entry = &(encounter_table->entries[j]);

            if (encounter_entry->counter != -1) {
                if (fileWriteInt32(stream, i) == -1) return -1;
                if (fileWriteInt32(stream, j) == -1) return -1;
                if (fileWriteInt32(stream, encounter_entry->counter) == -1) return -1;
            }
        }
    }

    return 0;
}

// 0x4BD28C
int wmWorldMap_load(File* stream)
{
    if (fileReadBool(stream, &(wmGenData.didMeetFrankHorrigan)) == -1) return -1;
    if (fileReadInt32(stream, &(wmGenData.currentAreaId)) == -1) return -1;
    if (fileReadInt32(stream, &(wmGenData.worldPosX)) == -1) return -1;
    if (fileReadInt32(stream, &(wmGenData.worldPosY)) == -1) return -1;
    if (fileReadBool(stream, &(wmGenData.encounterIconIsVisible)) == -1) return -1;
    if (fileReadInt32(stream, &(wmGenData.encounterMapId)) == -1) return -1;
    if (fileReadInt32(stream, &(wmGenData.encounterTableId)) == -1) return -1;
    if (fileReadInt32(stream, &(wmGenData.encounterEntryId)) == -1) return -1;
    if (fileReadBool(stream, &(wmGenData.isInCar)) == -1) return -1;
    if (fileReadInt32(stream, &(wmGenData.currentCarAreaId)) == -1) return -1;
    if (fileReadInt32(stream, &(wmGenData.carFuel)) == -1) return -1;

    int numCities;
    if (fileReadInt32(stream, &numCities) == -1) return -1;

    for (int areaIdx = 0; areaIdx < numCities; areaIdx++) {
        CityInfo* city = &(wmAreaInfoList[areaIdx]);

        if (fileReadInt32(stream, &(city->x)) == -1) return -1;
        if (fileReadInt32(stream, &(city->y)) == -1) return -1;
        if (fileReadInt32(stream, &(city->state)) == -1) return -1;
        if (fileReadInt32(stream, &(city->visitedState)) == -1) return -1;

        int entranceCount;
        if (fileReadInt32(stream, &(entranceCount)) == -1) {
            return -1;
        }

        for (int entranceIdx = 0; entranceIdx < entranceCount; entranceIdx++) {
            EntranceInfo* entrance = &(city->entrances[entranceIdx]);

            if (fileReadInt32(stream, &(entrance->state)) == -1) {
                return -1;
            }
        }
    }

    int numTiles;
    if (fileReadInt32(stream, &numTiles) == -1) return -1;

    int numHorizontalTiles;
    if (fileReadInt32(stream, &numHorizontalTiles) == -1) return -1;

    for (int tileIndex = 0; tileIndex < numTiles; tileIndex++) {
        TileInfo* tile = &(wmTileInfoList[tileIndex]);

        for (int column = 0; column < SUBTILE_GRID_HEIGHT; column++) {
            for (int row = 0; row < SUBTILE_GRID_WIDTH; row++) {
                SubtileInfo* subtile = &(tile->subtiles[column][row]);

                if (fileReadInt32(stream, &(subtile->state)) == -1) return -1;
            }
        }
    }

    int numCounters;
    if (fileReadInt32(stream, &numCounters) == -1) return -1;

    for (int counterIdx = 0; counterIdx < numCounters; counterIdx++) {
        int encounterTableIdx;
        int encounterTableEntryIdx;

        if (fileReadInt32(stream, &encounterTableIdx) == -1) return -1;
        EncounterTable* encounterTable = &(wmEncounterTableList[encounterTableIdx]);

        if (fileReadInt32(stream, &encounterTableEntryIdx) == -1) return -1;
        EncounterTableEntry* encounterTableEntry = &(encounterTable->entries[encounterTableEntryIdx]);

        if (fileReadInt32(stream, &(encounterTableEntry->counter)) == -1) return -1;
    }

    wmInterfaceCenterOnParty();

    return 0;
}

// 0x4BD678
static int wmWorldMapSaveTempData()
{
    File* stream = fileOpen("worldmap.dat", "wb");
    if (stream == nullptr) {
        return -1;
    }

    int rc = 0;
    if (wmWorldMap_save(stream) == -1) {
        rc = -1;
    }

    fileClose(stream);

    return rc;
}

// 0x4BD6B4
static int wmWorldMapLoadTempData()
{
    File* stream = fileOpen("worldmap.dat", "rb");
    if (stream == nullptr) {
        return -1;
    }

    int rc = 0;
    if (wmWorldMap_load(stream) == -1) {
        rc = -1;
    }

    fileClose(stream);

    return rc;
}

// 0x4BD6F0
static int wmConfigInit()
{
    if (wmAreaInit() == -1) {
        return -1;
    }

    Config config;
    if (!configInit(&config)) {
        return -1;
    }

    if (configRead(&config, "data\\worldmap.txt", true)) {
        for (int index = 0; index < ENCOUNTER_FREQUENCY_TYPE_COUNT; index++) {
            if (!configGetInt(&config, "data", wmFreqStrs[index], &(wmFreqValues[index]))) {
                break;
            }
        }

        char* terrainTypes;
        configGetString(&config, "data", "terrain_types", &terrainTypes);
        wmParseTerrainTypes(&config, terrainTypes);

        for (int index = 0;; index++) {
            char section[40];
            snprintf(section, sizeof(section), "Encounter Table %d", index);

            char* lookupName;
            if (!configGetString(&config, section, "lookup_name", &lookupName)) {
                break;
            }

            if (wmReadEncounterType(&config, lookupName, section) == -1) {
                return -1;
            }
        }

        if (!configGetInt(&config, "Tile Data", "num_horizontal_tiles", &wmNumHorizontalTiles)) {
            showMesageBox("\nwmConfigInit::Error loading tile data!");
            return -1;
        }

        for (int tileIndex = 0; tileIndex < 9999; tileIndex++) {
            char section[40];
            snprintf(section, sizeof(section), "Tile %d", tileIndex);

            int artIndex;
            if (!configGetInt(&config, section, "art_idx", &artIndex)) {
                break;
            }

            wmMaxTileNum++;

            TileInfo* worldmapTiles = (TileInfo*)internal_realloc(wmTileInfoList, sizeof(*wmTileInfoList) * wmMaxTileNum);
            if (worldmapTiles == nullptr) {
                showMesageBox("\nwmConfigInit::Error loading tiles!");
                exit(1);
            }

            wmTileInfoList = worldmapTiles;

            TileInfo* tile = &(worldmapTiles[wmMaxTileNum - 1]);

            // NOTE: Uninline.
            wmTileSlotInit(tile);

            tile->fid = buildFid(OBJ_TYPE_INTERFACE, artIndex, 0, 0, 0);

            int encounterDifficulty;
            if (configGetInt(&config, section, "encounter_difficulty", &encounterDifficulty)) {
                tile->encounterDifficultyModifier = encounterDifficulty;
            }

            char* walkMaskName;
            if (configGetString(&config, section, "walk_mask_name", &walkMaskName)) {
                strncpy(tile->walkMaskName, walkMaskName, TILE_WALK_MASK_NAME_SIZE);
            }

            for (int column = 0; column < SUBTILE_GRID_HEIGHT; column++) {
                for (int row = 0; row < SUBTILE_GRID_WIDTH; row++) {
                    char key[40];
                    snprintf(key, sizeof(key), "%d_%d", row, column);

                    char* subtileProps;
                    if (!configGetString(&config, section, key, &subtileProps)) {
                        showMesageBox("\nwmConfigInit::Error loading tiles!");
                        exit(1);
                    }

                    if (wmParseSubTileInfo(tile, row, column, subtileProps) == -1) {
                        showMesageBox("\nwmConfigInit::Error loading tiles!");
                        exit(1);
                    }
                }
            }
        }
    }

    configFree(&config);

    return 0;
}

// 0x4BD9F0
static int wmReadEncounterType(Config* config, char* lookupName, char* sectionKey)
{
    wmMaxEncounterInfoTables++;

    EncounterTable* encounterTables = (EncounterTable*)internal_realloc(wmEncounterTableList, sizeof(EncounterTable) * wmMaxEncounterInfoTables);
    if (encounterTables == nullptr) {
        showMesageBox("\nwmConfigInit::Error loading Encounter Table!");
        exit(1);
    }

    wmEncounterTableList = encounterTables;

    EncounterTable* encounterTable = &(encounterTables[wmMaxEncounterInfoTables - 1]);

    // NOTE: Uninline.
    wmEncounterTableSlotInit(encounterTable);

    encounterTable->index = wmMaxEncounterInfoTables - 1;
    strncpy(encounterTable->lookupName, lookupName, 40);

    char* str;
    if (configGetString(config, sectionKey, "maps", &str)) {
        while (*str != '\0') {
            if (encounterTable->mapsLength >= 6) {
                break;
            }

            if (strParseStrFromFunc(&str, &(encounterTable->maps[encounterTable->mapsLength]), wmParseFindMapIdxMatch) == -1) {
                break;
            }

            encounterTable->mapsLength++;
        }
    }

    for (;;) {
        char key[40];
        snprintf(key, sizeof(key), "enc_%02d", encounterTable->entriesLength);

        char* str;
        if (!configGetString(config, sectionKey, key, &str)) {
            break;
        }

        if (encounterTable->entriesLength >= 40) {
            showMesageBox("\nwmConfigInit::Error: Encounter Table: Too many table indexes!!");
            exit(1);
        }

        pConfigCfg = config;

        if (wmParseEncounterTableIndex(&(encounterTable->entries[encounterTable->entriesLength]), str) == -1) {
            return -1;
        }

        encounterTable->entriesLength++;
    }

    return 0;
}

// 0x4BDB64
static int wmParseEncounterTableIndex(EncounterTableEntry* encounterTableEntry, char* string)
{
    // NOTE: Uninline.
    if (wmEncounterTypeSlotInit(encounterTableEntry) == -1) {
        return -1;
    }

    while (string != nullptr && *string != '\0') {
        strParseIntWithKey(&string, "chance", &(encounterTableEntry->chance), ":");
        strParseIntWithKey(&string, "counter", &(encounterTableEntry->counter), ":");

        if (strstr(string, "special")) {
            encounterTableEntry->flags |= ENCOUNTER_ENTRY_SPECIAL;

            // CE: Original code unconditionally consumes 8 characters, which is
            // right when "special" is followed by conditions (separated with
            // comma). However when "special" is the last keyword (which I guess
            // is wrong, but present in worldmap.txt), consuming 8 characters
            // sets pointer past NULL terminator, which can lead to many bad
            // things (UB).
            string += 7;
            if (*string != '\0') {
                string++;
            }
        }

        if (string != nullptr) {
            char* pch = strstr(string, "map:");
            if (pch != nullptr) {
                string = pch + 4;
                strParseStrFromFunc(&string, &(encounterTableEntry->map), wmParseFindMapIdxMatch);
            }
        }

        if (wmParseEncounterSubEncStr(encounterTableEntry, &string) == -1) {
            break;
        }

        if (string != nullptr) {
            char* pch = strstr(string, "scenery:");
            if (pch != nullptr) {
                string = pch + 8;
                strParseStrFromList(&string, &(encounterTableEntry->scenery), wmSceneryStrs, ENCOUNTER_SCENERY_TYPE_COUNT);
            }
        }

        wmParseConditional(&string, "if", &(encounterTableEntry->condition));
    }

    return 0;
}

// 0x4BDCA8
static int wmParseEncounterSubEncStr(EncounterTableEntry* encounterTableEntry, char** stringPtr)
{
    char* string = *stringPtr;
    if (compat_strnicmp(string, "enc:", 4) != 0) {
        return -1;
    }

    // Consume "enc:".
    string += 4;

    char* comma = strstr(string, ",");
    if (comma != nullptr) {
        // Comma is present, position string pointer to the next chunk.
        *stringPtr = comma + 1;
        *comma = '\0';
    } else {
        // No comma, this chunk is the last one.
        *stringPtr = nullptr;
    }

    while (string != nullptr) {
        EncounterTableSubEntry* encounterTableSubEntry = &(encounterTableEntry->subEntries[encounterTableEntry->subEntiesLength]);

        // NOTE: Uninline.
        wmEncounterSubEncSlotInit(encounterTableSubEntry);

        if (*string == '(') {
            string++;
            encounterTableSubEntry->minimumCount = atoi(string);

            while (*string != '\0' && *string != '-') {
                string++;
            }

            if (*string == '-') {
                string++;
            }

            encounterTableSubEntry->maximumCount = atoi(string);

            while (*string != '\0' && *string != ')') {
                string++;
            }

            if (*string == ')') {
                string++;
            }
        }

        while (*string == ' ') {
            string++;
        }

        char* end = string;
        while (*end != '\0' && *end != ' ') {
            end++;
        }

        char ch = *end;
        *end = '\0';

        if (strParseStrFromFunc(&string, &(encounterTableSubEntry->encounterIndex), wmParseFindSubEncTypeMatch) == -1) {
            return -1;
        }

        *end = ch;

        if (ch == ' ') {
            string++;
        }

        end = string;
        while (*end != '\0' && *end != ' ') {
            end++;
        }

        ch = *end;
        *end = '\0';

        if (*string != '\0') {
            strParseStrFromList(&string, &(encounterTableSubEntry->situation), wmEncOpStrs, ENCOUNTER_SITUATION_COUNT);
        }

        *end = ch;

        encounterTableEntry->subEntiesLength++;

        while (*string == ' ') {
            string++;
        }

        if (*string == '\0') {
            string = nullptr;
        }
    }

    if (comma != nullptr) {
        *comma = ',';
    }

    return 0;
}

// 0x4BDE94
static int wmParseFindSubEncTypeMatch(char* str, int* valuePtr)
{
    *valuePtr = 0;

    if (compat_stricmp(str, "player") == 0) {
        *valuePtr = -1;
        return 0;
    }

    if (wmFindEncBaseTypeMatch(str, valuePtr) == 0) {
        return 0;
    }

    if (wmReadEncBaseType(str, valuePtr) == 0) {
        return 0;
    }

    return -1;
}

// 0x4BDED8
static int wmFindEncBaseTypeMatch(char* str, int* valuePtr)
{
    for (int index = 0; index < wmMaxEncBaseTypes; index++) {
        if (compat_stricmp(wmEncBaseTypeList[index].name, str) == 0) {
            *valuePtr = index;
            return 0;
        }
    }

    *valuePtr = -1;
    return -1;
}

// 0x4BDF34
static int wmReadEncBaseType(char* name, int* valuePtr)
{
    char section[40];
    snprintf(section, sizeof(section), "Encounter: %s", name);

    char key[40];
    snprintf(key, sizeof(key), "type_00");

    char* string;
    if (!configGetString(pConfigCfg, section, key, &string)) {
        return -1;
    }

    wmMaxEncBaseTypes++;

    Encounter* encounters = (Encounter*)internal_realloc(wmEncBaseTypeList, sizeof(*wmEncBaseTypeList) * wmMaxEncBaseTypes);
    if (encounters == nullptr) {
        showMesageBox("\nwmConfigInit::Error Reading EncBaseType!");
        exit(1);
    }

    wmEncBaseTypeList = encounters;

    Encounter* encounter = &(encounters[wmMaxEncBaseTypes - 1]);

    // NOTE: Uninline.
    wmEncBaseTypeSlotInit(encounter);

    strncpy(encounter->name, name, 40);

    while (1) {
        if (wmParseEncBaseSubTypeStr(&(encounter->entries[encounter->entriesLength]), &string) == -1) {
            return -1;
        }

        encounter->entriesLength++;

        snprintf(key, sizeof(key), "type_%02d", encounter->entriesLength);

        if (!configGetString(pConfigCfg, section, key, &string)) {
            int team;
            configGetInt(pConfigCfg, section, "team_num", &team);

            for (int index = 0; index < encounter->entriesLength; index++) {
                EncounterEntry* encounterEntry = &(encounter->entries[index]);
                if (PID_TYPE(encounterEntry->pid) == OBJ_TYPE_CRITTER) {
                    encounterEntry->team = team;
                }
            }

            if (configGetString(pConfigCfg, section, "position", &string)) {
                strParseStrFromList(&string, &(encounter->position), wmFormationStrs, ENCOUNTER_FORMATION_TYPE_COUNT);
                strParseIntWithKey(&string, "spacing", &(encounter->spacing), ":");
                strParseIntWithKey(&string, "distance", &(encounter->distance), ":");
            }

            *valuePtr = wmMaxEncBaseTypes - 1;

            return 0;
        }
    }

    return -1;
}

// 0x4BE140
static int wmParseEncBaseSubTypeStr(EncounterEntry* encounterEntry, char** stringPtr)
{
    char* string = *stringPtr;

    // NOTE: Uninline.
    if (wmEncBaseSubTypeSlotInit(encounterEntry) == -1) {
        return -1;
    }

    if (strParseIntWithKey(&string, "ratio", &(encounterEntry->ratio), ":") == 0) {
        encounterEntry->ratioMode = ENCOUNTER_RATIO_MODE_USE_RATIO;
    }

    if (strstr(string, "dead,") == string) {
        encounterEntry->flags |= ENCOUNTER_SUBINFO_DEAD;
        string += 5;
    }

    strParseIntWithKey(&string, "pid", &(encounterEntry->pid), ":");
    if (encounterEntry->pid == 0) {
        encounterEntry->pid = -1;
    }

    strParseIntWithKey(&string, "distance", &(encounterEntry->distance), ":");
    strParseIntWithKey(&string, "tilenum", &(encounterEntry->tile), ":");

    for (int index = 0; index < 10; index++) {
        if (strstr(string, "item:") == nullptr) {
            break;
        }

        wmParseEncounterItemType(&string, &(encounterEntry->items[encounterEntry->itemsLength]), &(encounterEntry->itemsLength), ":");
    }

    strParseIntWithKey(&string, "script", &(encounterEntry->scriptIdx), ":");
    wmParseConditional(&string, "if", &(encounterEntry->condition));

    return 0;
}

// NOTE: Inlined.
//
// 0x4BE2A0
static int wmEncBaseTypeSlotInit(Encounter* encounter)
{
    encounter->name[0] = '\0';
    encounter->position = ENCOUNTER_FORMATION_TYPE_SURROUNDING;
    encounter->spacing = 1;
    encounter->distance = -1;
    encounter->entriesLength = 0;

    return 0;
}

// NOTE: Inlined.
//
// 0x4BE2C4
static int wmEncBaseSubTypeSlotInit(EncounterEntry* encounterEntry)
{
    encounterEntry->field_28 = -1;
    encounterEntry->ratioMode = ENCOUNTER_RATIO_MODE_SINGLE;
    encounterEntry->ratio = 100;
    encounterEntry->pid = -1;
    encounterEntry->flags = 0;
    encounterEntry->distance = 0;
    encounterEntry->tile = -1;
    encounterEntry->itemsLength = 0;
    encounterEntry->scriptIdx = -1;
    encounterEntry->team = -1;

    return wmConditionalDataInit(&(encounterEntry->condition));
}

// NOTE: Inlined.
//
// 0x4BE32C
static int wmEncounterSubEncSlotInit(EncounterTableSubEntry* encounterTableSubEntry)
{
    encounterTableSubEntry->minimumCount = 1;
    encounterTableSubEntry->maximumCount = 1;
    encounterTableSubEntry->encounterIndex = -1;
    encounterTableSubEntry->situation = ENCOUNTER_SITUATION_NOTHING;

    return 0;
}

// NOTE: Inlined.
//
// 0x4BE34C
static int wmEncounterTypeSlotInit(EncounterTableEntry* encounterTableEntry)
{
    encounterTableEntry->flags = 0;
    encounterTableEntry->map = -1;
    encounterTableEntry->scenery = ENCOUNTER_SCENERY_TYPE_NORMAL;
    encounterTableEntry->chance = 0;
    encounterTableEntry->counter = -1;
    encounterTableEntry->subEntiesLength = 0;

    return wmConditionalDataInit(&(encounterTableEntry->condition));
}

// NOTE: Inlined.
//
// 0x4BE3B8
static int wmEncounterTableSlotInit(EncounterTable* encounterTable)
{
    encounterTable->lookupName[0] = '\0';
    encounterTable->mapsLength = 0;
    encounterTable->field_48 = 0;
    encounterTable->entriesLength = 0;

    return 0;
}

// NOTE: Inlined.
//
// 0x4BE3D4
static int wmTileSlotInit(TileInfo* tile)
{
    tile->fid = -1;
    tile->handle = INVALID_CACHE_ENTRY;
    tile->data = nullptr;
    tile->walkMaskName[0] = '\0';
    tile->walkMaskData = nullptr;
    tile->encounterDifficultyModifier = 0;

    return 0;
}

// NOTE: Inlined.
//
// 0x4BE400
static int wmTerrainTypeSlotInit(Terrain* terrain)
{
    terrain->lookupName[0] = '\0';
    terrain->difficulty = 0;
    terrain->mapsLength = 0;

    return 0;
}

// 0x4BE378
static int wmConditionalDataInit(EncounterCondition* condition)
{
    condition->entriesLength = 0;

    for (int index = 0; index < 3; index++) {
        EncounterConditionEntry* conditionEntry = &(condition->entries[index]);
        conditionEntry->type = ENCOUNTER_CONDITION_TYPE_NONE;
        conditionEntry->conditionalOperator = ENCOUNTER_CONDITIONAL_OPERATOR_NONE;
        conditionEntry->param = 0;
        conditionEntry->value = 0;
    }

    for (int index = 0; index < 2; index++) {
        condition->logicalOperators[index] = ENCOUNTER_LOGICAL_OPERATOR_NONE;
    }

    return 0;
}

// 0x4BE414
static int wmParseTerrainTypes(Config* config, char* string)
{
    if (*string == '\0') {
        return -1;
    }

    int terrainCount = 1;

    char* pch = string;
    while (*pch != '\0') {
        if (*pch == ',') {
            terrainCount++;
        }
        pch++;
    }

    wmMaxTerrainTypes = terrainCount;

    wmTerrainTypeList = (Terrain*)internal_malloc(sizeof(*wmTerrainTypeList) * terrainCount);
    if (wmTerrainTypeList == nullptr) {
        return -1;
    }

    for (int index = 0; index < wmMaxTerrainTypes; index++) {
        Terrain* terrain = &(wmTerrainTypeList[index]);

        // NOTE: Uninline.
        wmTerrainTypeSlotInit(terrain);
    }

    compat_strlwr(string);

    pch = string;
    for (int index = 0; index < wmMaxTerrainTypes; index++) {
        Terrain* terrain = &(wmTerrainTypeList[index]);

        pch += strspn(pch, " ");

        size_t endPos = strcspn(pch, ",");
        char end = pch[endPos];
        pch[endPos] = '\0';

        size_t delimeterPos = strcspn(pch, ":");
        char delimeter = pch[delimeterPos];
        pch[delimeterPos] = '\0';

        strncpy(terrain->lookupName, pch, 40);
        terrain->difficulty = atoi(pch + delimeterPos + 1);

        pch[delimeterPos] = delimeter;
        pch[endPos] = end;

        if (end == ',') {
            pch += endPos + 1;
        }
    }

    for (int index = 0; index < wmMaxTerrainTypes; index++) {
        wmParseTerrainRndMaps(config, &(wmTerrainTypeList[index]));
    }

    return 0;
}

// 0x4BE598
static int wmParseTerrainRndMaps(Config* config, Terrain* terrain)
{
    char section[40];
    snprintf(section, sizeof(section), "Random Maps: %s", terrain->lookupName);

    for (;;) {
        char key[40];
        snprintf(key, sizeof(key), "map_%02d", terrain->mapsLength);

        char* string;
        if (!configGetString(config, section, key, &string)) {
            break;
        }

        if (strParseStrFromFunc(&string, &(terrain->maps[terrain->mapsLength]), wmParseFindMapIdxMatch) == -1) {
            return -1;
        }

        terrain->mapsLength++;

        if (terrain->mapsLength >= 20) {
            return -1;
        }
    }

    return 0;
}

// 0x4BE61C
static int wmParseSubTileInfo(TileInfo* tile, int row, int column, char* string)
{
    SubtileInfo* subtile = &(tile->subtiles[column][row]);
    subtile->state = SUBTILE_STATE_UNKNOWN;

    if (strParseStrFromFunc(&string, &(subtile->terrain), wmParseFindTerrainTypeMatch) == -1) {
        return -1;
    }

    if (strParseStrFromList(&string, &(subtile->fill), wmFillStrs, SUBTILE_FILL_COUNT) == -1) {
        return -1;
    }

    for (int index = 0; index < DAY_PART_COUNT; index++) {
        if (strParseStrFromList(&string, &(subtile->encounterChance[index]), wmFreqStrs, ENCOUNTER_FREQUENCY_TYPE_COUNT) == -1) {
            return -1;
        }
    }

    if (strParseStrFromFunc(&string, &(subtile->encounterType), wmParseFindEncounterTypeMatch) == -1) {
        return -1;
    }

    return 0;
}

// 0x4BE6D4
static int wmParseFindEncounterTypeMatch(char* string, int* valuePtr)
{
    for (int index = 0; index < wmMaxEncounterInfoTables; index++) {
        if (compat_stricmp(string, wmEncounterTableList[index].lookupName) == 0) {
            *valuePtr = index;
            return 0;
        }
    }

    debugPrint("WorldMap Error: Couldn't find match for Encounter Type!");

    *valuePtr = -1;

    return -1;
}

// 0x4BE73C
static int wmParseFindTerrainTypeMatch(char* string, int* valuePtr)
{
    for (int index = 0; index < wmMaxTerrainTypes; index++) {
        Terrain* terrain = &(wmTerrainTypeList[index]);
        if (compat_stricmp(string, terrain->lookupName) == 0) {
            *valuePtr = index;
            return 0;
        }
    }

    debugPrint("WorldMap Error: Couldn't find match for Terrain Type!");

    *valuePtr = -1;

    return -1;
}

// 0x4BE7A4
static int wmParseEncounterItemType(char** stringPtr, EncounterItem* encounterItem, int* itemCountPtr, const char* delimeters)
{
    char* string = *stringPtr;

    if (*string == '\0') {
        return -1;
    }

    compat_strlwr(string);

    if (*string == ',') {
        string++;
        *stringPtr += 1;
    }

    string += strspn(string, " ");

    size_t commaPos = strcspn(string, ",");

    char comma = string[commaPos];
    string[commaPos] = '\0';

    size_t delimPos = strcspn(string, delimeters);
    char delim = string[delimPos];
    string[delimPos] = '\0';

    bool found = false;
    if (strcmp(string, "item") == 0) {
        *stringPtr += commaPos + 1;
        found = true;
        wmParseItemType(string + delimPos + 1, encounterItem);
        *itemCountPtr += 1;
    }

    string[delimPos] = delim;
    string[commaPos] = comma;

    return found ? 0 : -1;
}

// 0x4BE888
static int wmParseItemType(char* string, EncounterItem* encounterItem)
{
    while (*string == ' ') {
        string++;
    }

    encounterItem->minimumQuantity = 1;
    encounterItem->maximumQuantity = 1;
    encounterItem->isEquipped = false;

    if (*string == '(') {
        string++;

        encounterItem->minimumQuantity = atoi(string);

        while (isdigit(*string)) {
            string++;
        }

        if (*string == '-') {
            string++;

            encounterItem->maximumQuantity = atoi(string);

            while (isdigit(*string)) {
                string++;
            }
        } else {
            encounterItem->maximumQuantity = encounterItem->minimumQuantity;
        }

        if (*string == ')') {
            string++;
        }
    }

    while (*string == ' ') {
        string++;
    }

    encounterItem->pid = atoi(string);

    while (isdigit(*string)) {
        string++;
    }

    while (*string == ' ') {
        string++;
    }

    if (strstr(string, "{wielded}") != nullptr
        || strstr(string, "(wielded)") != nullptr
        || strstr(string, "{worn}") != nullptr
        || strstr(string, "(worn)") != nullptr) {
        encounterItem->isEquipped = true;
    }

    return 0;
}

// 0x4BE988
static int wmParseConditional(char** stringPtr, const char* a2, EncounterCondition* condition)
{
    while (condition->entriesLength < 3) {
        EncounterConditionEntry* conditionEntry = &(condition->entries[condition->entriesLength]);
        if (wmParseSubConditional(stringPtr, a2, &(conditionEntry->type), &(conditionEntry->conditionalOperator), &(conditionEntry->param), &(conditionEntry->value)) == -1) {
            return -1;
        }

        condition->entriesLength++;

        char* andStatement = strstr(*stringPtr, "and");
        if (andStatement != nullptr) {
            *stringPtr = andStatement + 3;
            condition->logicalOperators[condition->entriesLength - 1] = ENCOUNTER_LOGICAL_OPERATOR_AND;
            continue;
        }

        char* orStatement = strstr(*stringPtr, "or");
        if (orStatement != nullptr) {
            *stringPtr = orStatement + 2;
            condition->logicalOperators[condition->entriesLength - 1] = ENCOUNTER_LOGICAL_OPERATOR_OR;
            continue;
        }

        break;
    }

    return 0;
}

// 0x4BEA24
static int wmParseSubConditional(char** stringPtr, const char* a2, int* typePtr, int* operatorPtr, int* paramPtr, int* valuePtr)
{
    char* string = *stringPtr;

    if (string == nullptr) {
        return -1;
    }

    if (*string == '\0') {
        return -1;
    }

    compat_strlwr(string);

    if (*string == ',') {
        string++;
        *stringPtr = string;
    }

    string += strspn(string, " ");

    size_t commaPos = strcspn(string, ",");

    char comma = string[commaPos];
    string[commaPos] = '\0';

    size_t parenPos = strcspn(string, "(");
    char paren = string[parenPos];
    string[parenPos] = '\0';

    bool found = false;
    if (strstr(string, a2) == string) {
        found = true;
    }

    string[parenPos] = paren;
    string[commaPos] = comma;

    if (!found) {
        return -1;
    }

    string += parenPos + 1;

    char* pch;
    if (strstr(string, "rand(") == string) {
        string += 5;
        *typePtr = ENCOUNTER_CONDITION_TYPE_RANDOM;
        *operatorPtr = ENCOUNTER_CONDITIONAL_OPERATOR_NONE;
        *paramPtr = atoi(string);

        pch = strstr(string, ")");
        if (pch != nullptr) {
            string = pch + 1;
        }

        pch = strstr(string, ")");
        if (pch != nullptr) {
            string = pch + 1;
        }

        pch = strstr(string, ",");
        if (pch != nullptr) {
            string = pch + 1;
        }

        *stringPtr = string;
        return 0;
    } else if (strstr(string, "global(") == string) {
        string += 7;
        *typePtr = ENCOUNTER_CONDITION_TYPE_GLOBAL;
        *paramPtr = atoi(string);

        pch = strstr(string, ")");
        if (pch != nullptr) {
            string = pch + 1;
        }

        while (*string == ' ') {
            string++;
        }

        if (wmParseConditionalEval(&string, operatorPtr) != -1) {
            *valuePtr = atoi(string);

            pch = strstr(string, ")");
            if (pch != nullptr) {
                string = pch + 1;
            }

            pch = strstr(string, ",");
            if (pch != nullptr) {
                string = pch + 1;
            }
            *stringPtr = string;
            return 0;
        }
    } else if (strstr(string, "player(level)") == string) {
        string += 13;
        *typePtr = ENCOUNTER_CONDITION_TYPE_PLAYER;

        while (*string == ' ') {
            string++;
        }

        if (wmParseConditionalEval(&string, operatorPtr) != -1) {
            *valuePtr = atoi(string);

            pch = strstr(string, ")");
            if (pch != nullptr) {
                string = pch + 1;
            }

            pch = strstr(string, ",");
            if (pch != nullptr) {
                string = pch + 1;
            }
            *stringPtr = string;
            return 0;
        }
    } else if (strstr(string, "days_played") == string) {
        string += 11;
        *typePtr = ENCOUNTER_CONDITION_TYPE_DAYS_PLAYED;

        while (*string == ' ') {
            string++;
        }

        if (wmParseConditionalEval(&string, operatorPtr) != -1) {
            *valuePtr = atoi(string);

            pch = strstr(string, ")");
            if (pch != nullptr) {
                string = pch + 1;
            }

            pch = strstr(string, ",");
            if (pch != nullptr) {
                string = pch + 1;
            }
            *stringPtr = string;
            return 0;
        }
    } else if (strstr(string, "time_of_day") == string) {
        string += 11;
        *typePtr = ENCOUNTER_CONDITION_TYPE_TIME_OF_DAY;

        while (*string == ' ') {
            string++;
        }

        if (wmParseConditionalEval(&string, operatorPtr) != -1) {
            *valuePtr = atoi(string);

            pch = strstr(string, ")");
            if (pch != nullptr) {
                string = pch + 1;
            }

            pch = strstr(string, ",");
            if (pch != nullptr) {
                string = pch + 1;
            }
            *stringPtr = string;
            return 0;
        }
    } else if (strstr(string, "enctr(num_critters)") == string) {
        string += 19;
        *typePtr = ENCOUNTER_CONDITION_TYPE_NUMBER_OF_CRITTERS;

        while (*string == ' ') {
            string++;
        }

        if (wmParseConditionalEval(&string, operatorPtr) != -1) {
            *valuePtr = atoi(string);

            pch = strstr(string, ")");
            if (pch != nullptr) {
                string = pch + 1;
            }

            pch = strstr(string, ",");
            if (pch != nullptr) {
                string = pch + 1;
            }
            *stringPtr = string;
            return 0;
        }
    } else {
        *stringPtr = string;
        return 0;
    }

    return -1;
}

// 0x4BEEBC
static int wmParseConditionalEval(char** stringPtr, int* conditionalOperatorPtr)
{
    char* string = *stringPtr;

    *conditionalOperatorPtr = ENCOUNTER_CONDITIONAL_OPERATOR_NONE;

    int index;
    for (index = 0; index < ENCOUNTER_CONDITIONAL_OPERATOR_COUNT; index++) {
        if (strstr(string, wmConditionalOpStrs[index]) == string) {
            break;
        }
    }

    if (index == ENCOUNTER_CONDITIONAL_OPERATOR_COUNT) {
        return -1;
    }

    *conditionalOperatorPtr = index;

    string += strlen(wmConditionalOpStrs[index]);
    while (*string == ' ') {
        string++;
    }

    *stringPtr = string;

    return 0;
}

// NOTE: Inlined.
//
// 0x4BEF1C
static int wmAreaSlotInit(CityInfo* area)
{
    area->name[0] = '\0';
    area->areaId = -1;
    area->x = 0;
    area->y = 0;
    area->size = CITY_SIZE_LARGE;
    area->state = CITY_STATE_UNKNOWN;
    area->lockState = LOCK_STATE_UNLOCKED;
    area->visitedState = 0;
    area->mapFid = -1;
    area->labelFid = -1;
    area->entrancesLength = 0;

    return 0;
}

// 0x4BEF68
static int wmAreaInit()
{
    Config cfg;
    char section[40];
    char key[40];
    int area_idx;
    int num;
    char* str;
    CityInfo* cities;
    CityInfo* city;
    EntranceInfo* entrance;

    if (wmMapInit() == -1) {
        return -1;
    }

    if (!configInit(&cfg)) {
        return -1;
    }

    if (configRead(&cfg, "data\\city.txt", true)) {
        area_idx = 0;
        do {
            snprintf(section, sizeof(section), "Area %02d", area_idx);
            if (!configGetInt(&cfg, section, "townmap_art_idx", &num)) {
                break;
            }

            wmMaxAreaNum++;

            cities = (CityInfo*)internal_realloc(wmAreaInfoList, sizeof(CityInfo) * wmMaxAreaNum);
            if (cities == nullptr) {
                showMesageBox("\nwmConfigInit::Error loading areas!");
                exit(1);
            }

            wmAreaInfoList = cities;

            city = &(cities[wmMaxAreaNum - 1]);

            // NOTE: Uninline.
            wmAreaSlotInit(city);

            city->areaId = area_idx;

            if (num != -1) {
                num = buildFid(OBJ_TYPE_INTERFACE, num, 0, 0, 0);
            }

            city->mapFid = num;

            if (configGetInt(&cfg, section, "townmap_label_art_idx", &num)) {
                if (num != -1) {
                    num = buildFid(OBJ_TYPE_INTERFACE, num, 0, 0, 0);
                }

                city->labelFid = num;
            }

            if (!configGetString(&cfg, section, "area_name", &str)) {
                showMesageBox("\nwmConfigInit::Error loading areas!");
                exit(1);
            }

            strncpy(city->name, str, 40);

            if (!configGetString(&cfg, section, "world_pos", &str)) {
                showMesageBox("\nwmConfigInit::Error loading areas!");
                exit(1);
            }

            if (strParseInt(&str, &(city->x)) == -1) {
                return -1;
            }

            if (strParseInt(&str, &(city->y)) == -1) {
                return -1;
            }

            if (!configGetString(&cfg, section, "start_state", &str)) {
                showMesageBox("\nwmConfigInit::Error loading areas!");
                exit(1);
            }

            if (strParseStrFromList(&str, &(city->state), wmStateStrs, 2) == -1) {
                return -1;
            }

            if (configGetString(&cfg, section, "lock_state", &str)) {
                if (strParseStrFromList(&str, &(city->lockState), wmStateStrs, 2) == -1) {
                    return -1;
                }
            }

            if (!configGetString(&cfg, section, "size", &str)) {
                showMesageBox("\nwmConfigInit::Error loading areas!");
                exit(1);
            }

            if (strParseStrFromList(&str, &(city->size), wmAreaSizeStrs, 3) == -1) {
                return -1;
            }

            while (city->entrancesLength < ENTRANCE_LIST_CAPACITY) {
                snprintf(key, sizeof(key), "entrance_%d", city->entrancesLength);

                if (!configGetString(&cfg, section, key, &str)) {
                    break;
                }

                entrance = &(city->entrances[city->entrancesLength]);

                // NOTE: Uninline.
                wmEntranceSlotInit(entrance);

                if (strParseStrFromList(&str, &(entrance->state), wmStateStrs, 2) == -1) {
                    return -1;
                }

                if (strParseInt(&str, &(entrance->x)) == -1) {
                    return -1;
                }

                if (strParseInt(&str, &(entrance->y)) == -1) {
                    return -1;
                }

                if (strParseStrFromFunc(&str, &(entrance->map), &wmParseFindMapIdxMatch) == -1) {
                    return -1;
                }

                if (strParseInt(&str, &(entrance->elevation)) == -1) {
                    return -1;
                }

                if (strParseInt(&str, &(entrance->tile)) == -1) {
                    return -1;
                }

                if (strParseInt(&str, &(entrance->rotation)) == -1) {
                    return -1;
                }

                city->entrancesLength++;
            }

            area_idx++;
        } while (area_idx < 5000);
    }

    configFree(&cfg);

    if (wmMaxAreaNum != CITY_COUNT) {
        showMesageBox("\nwmAreaInit::Error loading Cities!");
        exit(1);
    }

    return 0;
}

// 0x4BF3E0
static int wmParseFindMapIdxMatch(char* string, int* valuePtr)
{
    for (int index = 0; index < wmMaxMapNum; index++) {
        MapInfo* map = &(wmMapInfoList[index]);
        if (compat_stricmp(string, map->lookupName) == 0) {
            *valuePtr = index;
            return 0;
        }
    }

    debugPrint("\nWorldMap Error: Couldn't find match for Map Index!");

    *valuePtr = -1;
    return -1;
}

// NOTE: Inlined.
//
// 0x4BF448
static int wmEntranceSlotInit(EntranceInfo* entrance)
{
    entrance->state = 0;
    entrance->x = 0;
    entrance->y = 0;
    entrance->map = -1;
    entrance->elevation = 0;
    entrance->tile = 0;
    entrance->rotation = 0;

    return 0;
}

// 0x4BF47C
static int wmMapSlotInit(MapInfo* map)
{
    map->lookupName[0] = '\0';
    map->field_28 = -1;
    map->field_2C = -1;
    map->mapFileName[0] = '\0';
    map->music[0] = '\0';
    map->flags = 0x3F;
    map->ambientSoundEffectsLength = 0;
    map->startPointsLength = 0;

    return 0;
}

// 0x4BF4BC
static int wmMapInit()
{
    char* str;
    int num;
    MapInfo* maps;
    MapInfo* map;

    Config config;
    if (!configInit(&config)) {
        return -1;
    }

    if (configRead(&config, "data\\maps.txt", true)) {
        for (int mapIdx = 0;; mapIdx++) {
            char section[40];
            snprintf(section, sizeof(section), "Map %03d", mapIdx);

            if (!configGetString(&config, section, "lookup_name", &str)) {
                break;
            }

            wmMaxMapNum++;

            maps = (MapInfo*)internal_realloc(wmMapInfoList, sizeof(*wmMapInfoList) * wmMaxMapNum);
            if (maps == nullptr) {
                showMesageBox("\nwmConfigInit::Error loading maps!");
                exit(1);
            }

            wmMapInfoList = maps;

            map = &(maps[wmMaxMapNum - 1]);
            wmMapSlotInit(map);

            strncpy(map->lookupName, str, 40);

            if (!configGetString(&config, section, "map_name", &str)) {
                showMesageBox("\nwmConfigInit::Error loading maps!");
                exit(1);
            }

            compat_strlwr(str);
            strncpy(map->mapFileName, str, 40);

            if (configGetString(&config, section, "music", &str)) {
                strncpy(map->music, str, 40);
            }

            if (configGetString(&config, section, "ambient_sfx", &str)) {
                while (str) {
                    MapAmbientSoundEffectInfo* sfx = &(map->ambientSoundEffects[map->ambientSoundEffectsLength]);
                    if (strParseKeyValue(&str, sfx->name, &(sfx->chance), ":") == -1) {
                        return -1;
                    }

                    map->ambientSoundEffectsLength++;

                    if (*str == '\0') {
                        str = nullptr;
                    }

                    if (map->ambientSoundEffectsLength >= MAP_AMBIENT_SOUND_EFFECTS_CAPACITY) {
                        if (str != nullptr) {
                            debugPrint("\nwmMapInit::Error reading ambient sfx.  Too many!  Str: %s, MapIdx: %d", map->lookupName, mapIdx);
                            str = nullptr;
                        }
                    }
                }
            }

            if (configGetString(&config, section, "saved", &str)) {
                if (strParseStrFromList(&str, &num, wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                // NOTE: Uninline.
                wmSetFlags(&(map->flags), MAP_SAVED, num);
            }

            if (configGetString(&config, section, "dead_bodies_age", &str)) {
                if (strParseStrFromList(&str, &num, wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                // NOTE: Uninline.
                wmSetFlags(&(map->flags), MAP_DEAD_BODIES_AGE, num);
            }

            if (configGetString(&config, section, "can_rest_here", &str)) {
                if (strParseStrFromList(&str, &num, wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                // NOTE: Uninline.
                wmSetFlags(&(map->flags), MAP_CAN_REST_ELEVATION_0, num);

                if (strParseStrFromList(&str, &num, wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                // NOTE: Uninline.
                wmSetFlags(&(map->flags), MAP_CAN_REST_ELEVATION_1, num);

                if (strParseStrFromList(&str, &num, wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                // NOTE: Uninline.
                wmSetFlags(&(map->flags), MAP_CAN_REST_ELEVATION_2, num);
            }

            if (configGetString(&config, section, "pipboy_active", &str)) {
                if (strParseStrFromList(&str, &num, wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                // NOTE: Uninline.
                wmSetFlags(&(map->flags), MAP_PIPBOY_ACTIVE, num);
            }

            // SFALL: Pip-boy automaps patch.
            if (configGetString(&config, section, "automap", &str)) {
                if (strParseStrFromList(&str, &num, wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                automapSetDisplayMap(mapIdx, num);
            }

            if (configGetString(&config, section, "random_start_point_0", &str)) {
                int rspIndex = 0;
                while (str != nullptr) {
                    while (*str != '\0') {
                        if (map->startPointsLength >= MAP_STARTING_POINTS_CAPACITY) {
                            break;
                        }

                        MapStartPointInfo* rsp = &(map->startPoints[map->startPointsLength]);

                        // NOTE: Uninline.
                        wmRStartSlotInit(rsp);

                        strParseIntWithKey(&str, "elev", &(rsp->elevation), ":");
                        strParseIntWithKey(&str, "tile_num", &(rsp->tile), ":");

                        map->startPointsLength++;
                    }

                    char key[40];
                    snprintf(key, sizeof(key), "random_start_point_%1d", ++rspIndex);

                    if (!configGetString(&config, section, key, &str)) {
                        str = nullptr;
                    }
                }
            }
        }
    }

    configFree(&config);

    return 0;
}

// NOTE: Inlined.
//
// 0x4BF954
static int wmRStartSlotInit(MapStartPointInfo* rsp)
{
    rsp->elevation = 0;
    rsp->tile = -1;
    rsp->rotation = -1;

    return 0;
}

// 0x4BF96C
int wmMapMaxCount()
{
    return wmMaxMapNum;
}

// 0x4BF974
int wmMapIdxToName(int mapIdx, char* dest, size_t size)
{
    if (mapIdx == -1 || mapIdx > wmMaxMapNum) {
        dest[0] = '\0';
        return -1;
    }

    snprintf(dest, size, "%s.MAP", wmMapInfoList[mapIdx].mapFileName);
    return 0;
}

// 0x4BF9BC
int wmMapMatchNameToIdx(char* name)
{
    compat_strlwr(name);

    char* pch = name;
    while (*pch != '\0' && *pch != '.') {
        pch++;
    }

    bool truncated = false;
    if (*pch != '\0') {
        *pch = '\0';
        truncated = true;
    }

    int map = -1;

    for (int index = 0; index < wmMaxMapNum; index++) {
        if (strcmp(wmMapInfoList[index].mapFileName, name) == 0) {
            map = index;
            break;
        }
    }

    if (truncated) {
        *pch = '.';
    }

    return map;
}

// 0x4BFA44
bool wmMapIdxIsSaveable(int mapIdx)
{
    return (wmMapInfoList[mapIdx].flags & MAP_SAVED) != 0;
}

// 0x4BFA64
bool wmMapIsSaveable()
{
    return (wmMapInfoList[gMapHeader.index].flags & MAP_SAVED) != 0;
}

// 0x4BFA90
bool wmMapDeadBodiesAge()
{
    return (wmMapInfoList[gMapHeader.index].flags & MAP_DEAD_BODIES_AGE) != 0;
}

// 0x4BFABC
bool wmMapCanRestHere(int elevation)
{
    int flags[3];

    // NOTE: I'm not sure why they're copied.
    memcpy(flags, _can_rest_here, sizeof(flags));

    MapInfo* map = &(wmMapInfoList[gMapHeader.index]);

    return (map->flags & flags[elevation]) != 0;
}

// 0x4BFAFC
bool wmMapPipboyActive()
{
    return gameMovieIsSeen(MOVIE_VSUIT);
}

// 0x4BFB08
int wmMapMarkVisited(int mapIdx)
{
    if (mapIdx < 0 || mapIdx >= wmMaxMapNum) {
        return -1;
    }

    MapInfo* map = &(wmMapInfoList[mapIdx]);
    if ((map->flags & MAP_SAVED) == 0) {
        return 0;
    }

    int areaIdx;
    if (wmMatchAreaContainingMapIdx(mapIdx, &areaIdx) == -1) {
        return -1;
    }

    // NOTE: Uninline.
    wmAreaMarkVisited(areaIdx);

    return 0;
}

// 0x4BFB64
static int wmMatchEntranceFromMap(int areaIdx, int mapIdx, int* entranceIdxPtr)
{
    CityInfo* city = &(wmAreaInfoList[areaIdx]);

    for (int entranceIdx = 0; entranceIdx < city->entrancesLength; entranceIdx++) {
        EntranceInfo* entrance = &(city->entrances[entranceIdx]);

        if (mapIdx == entrance->map) {
            *entranceIdxPtr = entranceIdx;
            return 0;
        }
    }

    *entranceIdxPtr = -1;
    return -1;
}

// 0x4BFBE8
static int wmMatchEntranceElevFromMap(int areaIdx, int mapIdx, int elevation, int* entranceIdxPtr)
{
    CityInfo* city = &(wmAreaInfoList[areaIdx]);

    for (int entranceIdx = 0; entranceIdx < city->entrancesLength; entranceIdx++) {
        EntranceInfo* entrance = &(city->entrances[entranceIdx]);
        if (entrance->map == mapIdx) {
            if (elevation == -1 || entrance->elevation == -1 || elevation == entrance->elevation) {
                *entranceIdxPtr = entranceIdx;
                return 0;
            }
        }
    }

    *entranceIdxPtr = -1;
    return -1;
}

// 0x4BFC7C
static int wmMatchAreaFromMap(int mapIdx, int* areaIdxPtr)
{
    for (int areaIdx = 0; areaIdx < wmMaxAreaNum; areaIdx++) {
        CityInfo* city = &(wmAreaInfoList[areaIdx]);

        for (int entranceIdx = 0; entranceIdx < city->entrancesLength; entranceIdx++) {
            EntranceInfo* entrance = &(city->entrances[entranceIdx]);
            if (mapIdx == entrance->map) {
                *areaIdxPtr = areaIdx;
                return 0;
            }
        }
    }

    *areaIdxPtr = -1;
    return -1;
}

// Mark map entrance.
//
// 0x4BFD50
int wmMapMarkMapEntranceState(int mapIdx, int elevation, int state)
{
    if (mapIdx < 0 || mapIdx >= wmMaxMapNum) {
        return -1;
    }

    MapInfo* map = &(wmMapInfoList[mapIdx]);
    if ((map->flags & MAP_SAVED) == 0) {
        return -1;
    }

    int areaIdx;
    if (wmMatchAreaContainingMapIdx(mapIdx, &areaIdx) == -1) {
        return -1;
    }

    int entranceIdx;
    if (wmMatchEntranceElevFromMap(areaIdx, mapIdx, elevation, &entranceIdx) == -1) {
        return -1;
    }

    CityInfo* city = &(wmAreaInfoList[areaIdx]);
    EntranceInfo* entrance = &(city->entrances[entranceIdx]);
    entrance->state = state;

    return 0;
}

// 0x4BFE0C
void wmWorldMap()
{
    wmWorldMapFunc(0);
}

// 0x4BFE10
static int wmWorldMapFunc(int a1)
{
    ScopedGameMode gm(GameMode::kWorldmap);

    wmFadeOut();

    if (wmInterfaceInit() == -1) {
        wmInterfaceExit();
        wmFadeReset();
        return -1;
    }

    wmFadeIn();

    wmMatchWorldPosToArea(wmGenData.worldPosX, wmGenData.worldPosY, &(wmGenData.currentAreaId));

    unsigned int partyHealTime = 0;
    int map = -1;
    int rc = 0;

    while (true) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        // SFALL: WorldmapLoopHook.
        sfall_gl_scr_process_worldmap();

        unsigned int now = getTicks();

        int mouseX;
        int mouseY;
        mouseGetPositionInWindow(wmBkWin, &mouseX, &mouseY);

        int worldX = wmWorldOffsetX + mouseX - WM_VIEW_X;
        int worldY = wmWorldOffsetY + mouseY - WM_VIEW_Y;

        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            showQuitConfirmationDialog();
        }

        // NOTE: Uninline.
        wmCheckGameEvents();

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        int mouseEvent = mouseGetEvent();

        if (wmGenData.isWalking) {
            wmPartyWalkingStep();

            if (wmGenData.isInCar) {
                wmPartyWalkingStep();
                wmPartyWalkingStep();
                wmPartyWalkingStep();

                if (gameGetGlobalVar(GVAR_CAR_BLOWER)) {
                    wmPartyWalkingStep();
                }

                if (gameGetGlobalVar(GVAR_NEW_RENO_CAR_UPGRADE)) {
                    wmPartyWalkingStep();
                }

                if (gameGetGlobalVar(GVAR_NEW_RENO_SUPER_CAR)) {
                    wmPartyWalkingStep();
                    wmPartyWalkingStep();
                    wmPartyWalkingStep();
                }

                wmGenData.carImageCurrentFrameIndex++;
                if (wmGenData.carImageCurrentFrameIndex >= artGetFrameCount(wmGenData.carImageFrm)) {
                    wmGenData.carImageCurrentFrameIndex = 0;
                }

                wmCarUseGas(100);

                if (wmGenData.carFuel <= 0) {
                    wmGenData.walkDestinationX = 0;
                    wmGenData.walkDestinationY = 0;
                    wmGenData.isWalking = false;

                    wmMatchWorldPosToArea(worldX, worldY, &(wmGenData.currentAreaId));

                    wmGenData.isInCar = false;

                    if (wmGenData.currentAreaId == -1) {
                        wmGenData.currentCarAreaId = CITY_CAR_OUT_OF_GAS;

                        CityInfo* city = &(wmAreaInfoList[CITY_CAR_OUT_OF_GAS]);

                        CitySizeDescription* citySizeDescription = &(wmSphereData[city->size]);
                        int worldmapX = wmGenData.worldPosX + wmGenData.hotspotNormalFrmImage.getWidth() / 2 + citySizeDescription->frmImage.getWidth() / 2;
                        int worldmapY = wmGenData.worldPosY + wmGenData.hotspotNormalFrmImage.getHeight() / 2 + citySizeDescription->frmImage.getHeight() / 2;
                        wmAreaSetWorldPos(CITY_CAR_OUT_OF_GAS, worldmapX, worldmapY);

                        city->state = CITY_STATE_KNOWN;
                        city->visitedState = 1;

                        wmGenData.currentAreaId = CITY_CAR_OUT_OF_GAS;
                    } else {
                        wmGenData.currentCarAreaId = wmGenData.currentAreaId;
                    }

                    debugPrint("\nRan outta gas!");
                }
            }

            wmInterfaceRefresh();

            if (getTicksBetween(now, partyHealTime) > 1000) {
                if (_partyMemberRestingHeal(3)) {
                    interfaceRenderHitPoints(false);
                    partyHealTime = now;
                }
            }

            wmMarkSubTileRadiusVisited(wmGenData.worldPosX, wmGenData.worldPosY);

            if (wmGenData.walkDistance <= 0) {
                wmGenData.isWalking = false;
                wmMatchWorldPosToArea(wmGenData.worldPosX, wmGenData.worldPosY, &(wmGenData.currentAreaId));
            }

            wmInterfaceRefresh();

            if (wmGameTimeIncrement(18000)) {
                if (_game_user_wants_to_quit != 0) {
                    break;
                }
            }

            if (wmGenData.isWalking) {
                if (wmRndEncounterOccurred()) {
                    if (wmGenData.encounterMapId != -1) {
                        if (wmGenData.isInCar) {
                            wmMatchAreaContainingMapIdx(wmGenData.encounterMapId, &(wmGenData.currentCarAreaId));
                        }

                        wmFadeOut();
                        mapLoadById(wmGenData.encounterMapId);
                    }
                    break;
                }
            }
        }

        if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0 && (mouseEvent & MOUSE_EVENT_LEFT_BUTTON_REPEAT) == 0) {
            if (mouseHitTestInWindow(wmBkWin, WM_VIEW_X, WM_VIEW_Y, 472, 465)) {
                if (!wmGenData.isWalking && !wmGenData.mousePressed && abs(wmGenData.worldPosX - worldX) < 5 && abs(wmGenData.worldPosY - worldY) < 5) {
                    wmGenData.mousePressed = true;
                    wmInterfaceRefresh();
                    renderPresent();
                }
            } else {
                continue;
            }
        }

        if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
            if (wmGenData.mousePressed) {
                wmGenData.mousePressed = false;
                wmInterfaceRefresh();

                if (abs(wmGenData.worldPosX - worldX) < 5 && abs(wmGenData.worldPosY - worldY) < 5) {
                    if (wmGenData.currentAreaId != -1) {
                        CityInfo* city = &(wmAreaInfoList[wmGenData.currentAreaId]);
                        if (city->visitedState == 2 && city->mapFid != -1) {
                            if (wmTownMapFunc(&map) == -1) {
                                rc = -1;
                                break;
                            }
                        } else {
                            if (wmAreaFindFirstValidMap(&map) == -1) {
                                rc = -1;
                                break;
                            }

                            city->visitedState = 2;
                        }
                    } else {
                        map = 0;
                    }

                    if (map != -1) {
                        if (wmGenData.isInCar) {
                            wmGenData.isInCar = false;
                            if (wmGenData.currentAreaId == -1) {
                                wmMatchAreaContainingMapIdx(map, &(wmGenData.currentCarAreaId));
                            } else {
                                wmGenData.currentCarAreaId = wmGenData.currentAreaId;
                            }
                        }

                        wmFadeOut();
                        mapLoadById(map);
                        break;
                    }
                }
            } else {
                if (mouseHitTestInWindow(wmBkWin, WM_VIEW_X, WM_VIEW_Y, 472, 465)) {
                    wmPartyInitWalking(worldX, worldY);
                }

                wmGenData.mousePressed = false;
            }
        }

        // NOTE: Uninline.
        wmInterfaceScrollTabsUpdate();

        if (keyCode == KEY_UPPERCASE_T || keyCode == KEY_LOWERCASE_T) {
            if (!wmGenData.isWalking && wmGenData.currentAreaId != -1) {
                CityInfo* city = &(wmAreaInfoList[wmGenData.currentAreaId]);
                if (city->visitedState == 2 && city->mapFid != -1) {
                    if (wmTownMapFunc(&map) == -1) {
                        rc = -1;
                    }

                    if (map != -1) {
                        if (wmGenData.isInCar) {
                            // SFALL: Fix for the car being lost when entering a
                            // location via the Town/World button and then
                            // leaving on foot.
                            //
                            // CE: Fix is very different, but looks right -
                            // matches the code above (processing mouse events).
                            wmGenData.isInCar = false;

                            wmMatchAreaContainingMapIdx(map, &(wmGenData.currentCarAreaId));
                        }

                        wmFadeOut();
                        mapLoadById(map);
                    }
                }
            }
        } else if (keyCode == KEY_HOME) {
            wmInterfaceCenterOnParty();
        } else if (keyCode == KEY_ARROW_UP) {
            // NOTE: Uninline.
            wmInterfaceScroll(0, -1, nullptr);
        } else if (keyCode == KEY_ARROW_LEFT) {
            // NOTE: Uninline.
            wmInterfaceScroll(-1, 0, nullptr);
        } else if (keyCode == KEY_ARROW_DOWN) {
            // NOTE: Uninline.
            wmInterfaceScroll(0, 1, nullptr);
        } else if (keyCode == KEY_ARROW_RIGHT) {
            // NOTE: Uninline.
            wmInterfaceScroll(1, 0, nullptr);
        } else if (keyCode == KEY_CTRL_ARROW_UP) {
            wmInterfaceScrollTabsStart(-27);
        } else if (keyCode == KEY_CTRL_ARROW_DOWN) {
            wmInterfaceScrollTabsStart(27);
        } else if (keyCode >= KEY_CTRL_F1 && keyCode <= KEY_CTRL_F7) {
            int quickDestinationIndex = wmGenData.tabsOffsetY / 27 + (keyCode - KEY_CTRL_F1);
            if (quickDestinationIndex < wmLabelCount) {
                int areaIdx = wmLabelList[quickDestinationIndex];
                CityInfo* city = &(wmAreaInfoList[areaIdx]);
                if (wmAreaIsKnown(city->areaId)) {
                    if (wmGenData.currentAreaId != areaIdx) {
                        // SFALL: Fix the position of the destination marker for
                        // small/medium location circles.
                        // CE: Fix is slightly different. `wmPartyInitWalking`
                        // assumes x/y are compensated for worldmap viewport
                        // offset (as can be seen earlier in this function).
                        CitySizeDescription* citySizeDescription = &(wmSphereData[city->size]);
                        int destX = city->x + citySizeDescription->frmImage.getWidth() / 2 - WM_VIEW_X;
                        int destY = city->y + citySizeDescription->frmImage.getHeight() / 2 - WM_VIEW_Y;
                        wmPartyInitWalking(destX, destY);
                        wmGenData.mousePressed = 0;
                    }
                }
            }
        }

        if ((mouseEvent & MOUSE_EVENT_WHEEL) != 0) {
            int wheelX;
            int wheelY;
            mouseGetWheel(&wheelX, &wheelY);

            if (mouseHitTestInWindow(wmBkWin, WM_VIEW_X, WM_VIEW_Y, 472, 465)) {
                wmInterfaceScrollPixel(20, 20, wheelX, -wheelY, nullptr, true);
            } else if (mouseHitTestInWindow(wmBkWin, 501, 135, 501 + 119, 135 + 178)) {
                if (wheelY != 0) {
                    wmInterfaceScrollTabsStart(wheelY > 0 ? 27 : -27);
                }
            }
        }

        if (map != -1 || rc == -1) {
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (wmInterfaceExit() == -1) {
        wmFadeReset();
        return -1;
    }

    wmFadeIn();

    return rc;
}

// 0x4C056C
int wmCheckGameAreaEvents()
{
    if (wmGenData.currentAreaId == CITY_FAKE_VAULT_13_A) {
        // NOTE: Uninline.
        wmAreaSetVisibleState(CITY_FAKE_VAULT_13_A, CITY_STATE_UNKNOWN, true);

        // NOTE: Uninline.
        wmAreaSetVisibleState(CITY_FAKE_VAULT_13_B, CITY_STATE_KNOWN, true);

        wmAreaMarkVisitedState(CITY_FAKE_VAULT_13_B, 2);
    }

    return 0;
}

// 0x4C05C4
static int wmInterfaceCenterOnParty()
{
    wmWorldOffsetX = std::clamp(wmGenData.worldPosX - 203, 0, wmGenData.viewportMaxX);
    wmWorldOffsetY = std::clamp(wmGenData.worldPosY - 200, 0, wmGenData.viewportMaxY);

    wmInterfaceRefresh();

    return 0;
}

// NOTE: Inlined.
//
// 0x4C0624
static void wmCheckGameEvents()
{
    _scriptsCheckGameEvents(nullptr, wmBkWin);
}

// 0x4C0634
static int wmRndEncounterOccurred()
{
    unsigned int now = getTicks();
    if (getTicksBetween(now, wmLastRndTime) < 1500) {
        return 0;
    }

    wmLastRndTime = now;

    if (abs(wmGenData.oldWorldPosX - wmGenData.worldPosX) < 3) {
        return 0;
    }

    if (abs(wmGenData.oldWorldPosY - wmGenData.worldPosY) < 3) {
        return 0;
    }

    int areaIdx;
    wmMatchWorldPosToArea(wmGenData.worldPosX, wmGenData.worldPosY, &areaIdx);
    if (areaIdx != -1) {
        return 0;
    }

    if (!wmGenData.didMeetFrankHorrigan) {
        unsigned int gameTime = gameTimeGetTime();
        if (gameTime / GAME_TIME_TICKS_PER_DAY > 35) {
            // SFALL: Add a flashing icon to the Horrigan encounter.
            wmBlinkRndEncounterIcon(true);

            wmGenData.encounterMapId = -1;
            wmGenData.didMeetFrankHorrigan = true;
            if (wmGenData.isInCar) {
                wmMatchAreaContainingMapIdx(MAP_IN_GAME_MOVIE1, &(wmGenData.currentCarAreaId));
            }

            wmFadeOut();
            mapLoadById(MAP_IN_GAME_MOVIE1);
            return 1;
        }
    }

    // SFALL: Handle forced encounter.
    // CE: In Sfall a check for forced encounter is inserted instead of check
    // for Horrigan encounter (above). This implemenation gives Horrigan
    // encounter a priority.
    if (wmForceEncounterMapId != -1) {
        if ((wmForceEncounterFlags & ENCOUNTER_FLAG_NO_CAR) != 0) {
            if (wmGenData.isInCar) {
                wmMatchAreaContainingMapIdx(wmForceEncounterMapId, &(wmGenData.currentCarAreaId));
            }
        }

        // For unknown reason fadeout and blinking icon are mutually exclusive.
        if ((wmForceEncounterFlags & ENCOUNTER_FLAG_FADEOUT) != 0) {
            wmFadeOut();
        } else if ((wmForceEncounterFlags & ENCOUNTER_FLAG_NO_ICON) == 0) {
            bool special = (wmForceEncounterFlags & ENCOUNTER_FLAG_ICON_SP) != 0;
            wmBlinkRndEncounterIcon(special);
        }

        mapLoadById(wmForceEncounterMapId);

        wmForceEncounterMapId = -1;
        wmForceEncounterFlags = 0;

        return 1;
    }

    // NOTE: Uninline.
    wmPartyFindCurSubTile();

    int dayPart;
    int gameTimeHour = gameTimeGetHour();
    if (gameTimeHour >= 1800 || gameTimeHour < 600) {
        dayPart = DAY_PART_NIGHT;
    } else if (gameTimeHour >= 1200) {
        dayPart = DAY_PART_AFTERNOON;
    } else {
        dayPart = DAY_PART_MORNING;
    }

    int frequency = wmFreqValues[wmGenData.currentSubtile->encounterChance[dayPart]];
    if (frequency > 0 && frequency < 100) {
        int modifier = frequency / 15;
        switch (settings.preferences.game_difficulty) {
        case GAME_DIFFICULTY_EASY:
            frequency -= modifier;
            break;
        case GAME_DIFFICULTY_HARD:
            frequency += modifier;
            break;
        }
    }

    int chance = randomBetween(0, 100);
    if (chance >= frequency) {
        return 0;
    }

    wmRndEncounterPick();

    EncounterTable* encounterTable = &(wmEncounterTableList[wmGenData.encounterTableId]);
    EncounterTableEntry* encounterTableEntry = &(encounterTable->entries[wmGenData.encounterEntryId]);
    if ((encounterTableEntry->flags & ENCOUNTER_ENTRY_SPECIAL) != 0) {
        wmMatchAreaContainingMapIdx(wmGenData.encounterMapId, &areaIdx);

        CityInfo* city = &(wmAreaInfoList[areaIdx]);
        CitySizeDescription* citySizeDescription = &(wmSphereData[city->size]);
        int worldmapX = wmGenData.worldPosX + wmGenData.hotspotNormalFrmImage.getWidth() / 2 + citySizeDescription->frmImage.getWidth() / 2;
        int worldmapY = wmGenData.worldPosY + wmGenData.hotspotNormalFrmImage.getHeight() / 2 + citySizeDescription->frmImage.getHeight() / 2;
        wmAreaSetWorldPos(areaIdx, worldmapX, worldmapY);

        if (areaIdx >= 0 && areaIdx < wmMaxAreaNum) {
            CityInfo* city = &(wmAreaInfoList[areaIdx]);
            if (city->lockState != LOCK_STATE_LOCKED) {
                city->state = CITY_STATE_KNOWN;
            }
        }
    }

    // Blinking.
    wmBlinkRndEncounterIcon((encounterTableEntry->flags & ENCOUNTER_ENTRY_SPECIAL) != 0);

    if (wmGenData.isInCar) {
        int modifiers[DAY_PART_COUNT];

        // NOTE: I'm not sure why they're copied.
        memcpy(modifiers, gDayPartEncounterFrequencyModifiers, sizeof(gDayPartEncounterFrequencyModifiers));

        frequency -= modifiers[dayPart];
    }

    bool randomEncounterIsDetected = false;
    if (frequency > chance) {
        int outdoorsman = partyGetBestSkillValue(SKILL_OUTDOORSMAN);
        Object* scanner = objectGetCarriedObjectByPid(gDude, PROTO_ID_MOTION_SENSOR);
        if (scanner != nullptr) {
            if (gDude == scanner->owner) {
                outdoorsman += 20;
            }
        }

        if (outdoorsman > 95) {
            outdoorsman = 95;
        }

        TileInfo* tile;
        // NOTE: Uninline.
        wmFindCurTileFromPos(wmGenData.worldPosX, wmGenData.worldPosY, &tile);
        debugPrint("\nEncounter Difficulty Mod: %d", tile->encounterDifficultyModifier);

        outdoorsman += tile->encounterDifficultyModifier;

        if (randomBetween(1, 100) < outdoorsman) {
            randomEncounterIsDetected = true;

            int xp = 100 - outdoorsman;
            if (xp > 0) {
                // SFALL: Display actual xp received.
                debugPrint("WorldMap: Giving Player [%d] Experience For Catching Rnd Encounter!", xp);

                int xpGained;
                pcAddExperience(xp, &xpGained);

                MessageListItem messageListItem;
                char* text = getmsg(&gMiscMessageList, &messageListItem, 8500);
                if (strlen(text) < 110) {
                    char formattedText[120];
                    snprintf(formattedText, sizeof(formattedText), text, xpGained);
                    displayMonitorAddMessage(formattedText);
                } else {
                    debugPrint("WorldMap: Error: Rnd Encounter string too long!");
                }
            }
        }
    } else {
        randomEncounterIsDetected = true;
    }

    wmGenData.oldWorldPosX = wmGenData.worldPosX;
    wmGenData.oldWorldPosY = wmGenData.worldPosY;

    if (randomEncounterIsDetected) {
        MessageListItem messageListItem;

        const char* title = gWorldmapEncDefaultMsg[0];
        const char* body = gWorldmapEncDefaultMsg[1];

        title = getmsg(&wmMsgFile, &messageListItem, 2999);
        body = getmsg(&wmMsgFile, &messageListItem, 3000 + 50 * wmGenData.encounterTableId + wmGenData.encounterEntryId);
        if (showDialogBox(title, &body, 1, 169, 116, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE | DIALOG_BOX_YES_NO) == 0) {
            wmGenData.encounterIconIsVisible = false;
            wmGenData.encounterMapId = -1;
            wmGenData.encounterTableId = -1;
            wmGenData.encounterEntryId = -1;
            return 0;
        }
    }

    return 1;
}

// NOTE: Inlined.
//
// 0x4C0BE4
static int wmPartyFindCurSubTile()
{
    return wmFindCurSubTileFromPos(wmGenData.worldPosX, wmGenData.worldPosY, &(wmGenData.currentSubtile));
}

// 0x4C0C00
static int wmFindCurSubTileFromPos(int x, int y, SubtileInfo** subtilePtr)
{
    int tileIndex = y / WM_TILE_HEIGHT * wmNumHorizontalTiles + x / WM_TILE_WIDTH % wmNumHorizontalTiles;
    TileInfo* tile = &(wmTileInfoList[tileIndex]);

    int column = y % WM_TILE_HEIGHT / WM_SUBTILE_SIZE;
    int row = x % WM_TILE_WIDTH / WM_SUBTILE_SIZE;
    *subtilePtr = &(tile->subtiles[column][row]);

    return 0;
}

// NOTE: Inlined.
//
// 0x4C0CA8
static int wmFindCurTileFromPos(int x, int y, TileInfo** tilePtr)
{
    int tileIndex = y / WM_TILE_HEIGHT * wmNumHorizontalTiles + x / WM_TILE_WIDTH % wmNumHorizontalTiles;
    *tilePtr = &(wmTileInfoList[tileIndex]);

    return 0;
}

// 0x4C0CF4
static int wmRndEncounterPick()
{
    if (wmGenData.currentSubtile == nullptr) {
        // NOTE: Uninline.
        wmPartyFindCurSubTile();
    }

    wmGenData.encounterTableId = wmGenData.currentSubtile->encounterType;

    EncounterTable* encounterTable = &(wmEncounterTableList[wmGenData.encounterTableId]);

    int candidates[41];
    int candidatesLength = 0;
    int totalChance = 0;
    for (int index = 0; index < encounterTable->entriesLength; index++) {
        EncounterTableEntry* encounterTableEntry = &(encounterTable->entries[index]);

        bool selected = true;
        if (wmEvalConditional(&(encounterTableEntry->condition), nullptr) == 0) {
            selected = false;
        }

        if (encounterTableEntry->counter == 0) {
            selected = false;
        }

        if (selected) {
            candidates[candidatesLength++] = index;
            totalChance += encounterTableEntry->chance;
        }
    }

    int effectiveLuck = critterGetStat(gDude, STAT_LUCK) - 5;
    int chance = randomBetween(0, totalChance) + effectiveLuck;

    if (perkHasRank(gDude, PERK_EXPLORER)) {
        chance += 2;
    }

    if (perkHasRank(gDude, PERK_RANGER)) {
        chance += 1;
    }

    if (perkHasRank(gDude, PERK_SCOUT)) {
        chance += 1;
    }

    switch (settings.preferences.game_difficulty) {
    case GAME_DIFFICULTY_EASY:
        chance += 5;
        if (chance > totalChance) {
            chance = totalChance;
        }
        break;
    case GAME_DIFFICULTY_HARD:
        chance -= 5;
        if (chance < 0) {
            chance = 0;
        }
        break;
    }

    int index;
    for (index = 0; index < candidatesLength; index++) {
        EncounterTableEntry* encounterTableEntry = &(encounterTable->entries[candidates[index]]);
        if (chance < encounterTableEntry->chance) {
            break;
        }

        chance -= encounterTableEntry->chance;
    }

    if (index == candidatesLength) {
        index = candidatesLength - 1;
    }

    wmGenData.encounterEntryId = candidates[index];

    EncounterTableEntry* encounterTableEntry = &(encounterTable->entries[wmGenData.encounterEntryId]);
    if (encounterTableEntry->counter > 0) {
        encounterTableEntry->counter--;
    }

    if (encounterTableEntry->map == -1) {
        if (encounterTable->mapsLength <= 0) {
            Terrain* terrain = &(wmTerrainTypeList[wmGenData.currentSubtile->terrain]);
            int randommapIdx = randomBetween(0, terrain->mapsLength - 1);
            wmGenData.encounterMapId = terrain->maps[randommapIdx];
        } else {
            int randommapIdx = randomBetween(0, encounterTable->mapsLength - 1);
            wmGenData.encounterMapId = encounterTable->maps[randommapIdx];
        }
    } else {
        wmGenData.encounterMapId = encounterTableEntry->map;
    }

    return 0;
}

// 0x4C0FA4
int wmSetupRandomEncounter()
{
    MessageListItem messageListItem;

    if (wmGenData.encounterMapId == -1) {
        return 0;
    }

    EncounterTable* encounterTable = &(wmEncounterTableList[wmGenData.encounterTableId]);
    EncounterTableEntry* encounterTableEntry = &(encounterTable->entries[wmGenData.encounterEntryId]);

    // SFALL: Display encounter description in one line.
    char formattedText[512];
    snprintf(formattedText, sizeof(formattedText),
        "%s %s",
        getmsg(&wmMsgFile, &messageListItem, 2998),
        getmsg(&wmMsgFile, &messageListItem, 3000 + 50 * wmGenData.encounterTableId + wmGenData.encounterEntryId));
    displayMonitorAddMessage(formattedText);

    int gameDifficulty = settings.preferences.game_difficulty;
    switch (encounterTableEntry->scenery) {
    case ENCOUNTER_SCENERY_TYPE_NONE:
    case ENCOUNTER_SCENERY_TYPE_LIGHT:
    case ENCOUNTER_SCENERY_TYPE_NORMAL:
    case ENCOUNTER_SCENERY_TYPE_HEAVY:
        debugPrint("\nwmSetupRandomEncounter: Scenery Type: %s", wmSceneryStrs[encounterTableEntry->scenery]);
        break;
    default:
        debugPrint("\nERROR: wmSetupRandomEncounter: invalid Scenery Type!");
        return -1;
    }

    Object* prevCritter = nullptr;
    for (int index = 0; index < encounterTableEntry->subEntiesLength; index++) {
        EncounterTableSubEntry* encounterTableSubEntry = &(encounterTableEntry->subEntries[index]);

        int critterCount = randomBetween(encounterTableSubEntry->minimumCount, encounterTableSubEntry->maximumCount);

        switch (gameDifficulty) {
        case GAME_DIFFICULTY_EASY:
            critterCount -= 2;
            if (critterCount < encounterTableSubEntry->minimumCount) {
                critterCount = encounterTableSubEntry->minimumCount;
            }
            break;
        case GAME_DIFFICULTY_HARD:
            critterCount += 2;
            break;
        }

        int partyMemberCount = _getPartyMemberCount();
        if (partyMemberCount > 2) {
            critterCount += 2;
        }

        if (critterCount != 0) {
            Object* critter;
            if (wmSetupCritterObjs(encounterTableSubEntry->encounterIndex, &critter, critterCount) == -1) {
                scriptsRequestWorldMap();
                return -1;
            }

            if (index > 0) {
                if (prevCritter != nullptr) {
                    if (prevCritter != critter) {
                        if (encounterTableEntry->subEntiesLength != 1) {
                            if (encounterTableEntry->subEntiesLength == 2 && !isInCombat()) {
                                prevCritter->data.critter.combat.whoHitMe = critter;
                                critter->data.critter.combat.whoHitMe = prevCritter;

                                CombatStartData combat;
                                combat.attacker = prevCritter;
                                combat.defender = critter;
                                combat.actionPointsBonus = 0;
                                combat.accuracyBonus = 0;
                                combat.damageBonus = 0;
                                combat.minDamage = 0;
                                combat.maxDamage = 500;
                                combat.overrideAttackResults = 0;

                                _caiSetupTeamCombat(critter, prevCritter);
                                _scripts_request_combat_locked(&combat);
                            }
                        } else {
                            if (!isInCombat()) {
                                prevCritter->data.critter.combat.whoHitMe = gDude;

                                CombatStartData combat;
                                combat.attacker = prevCritter;
                                combat.defender = gDude;
                                combat.actionPointsBonus = 0;
                                combat.accuracyBonus = 0;
                                combat.damageBonus = 0;
                                combat.minDamage = 0;
                                combat.maxDamage = 500;
                                combat.overrideAttackResults = 0;

                                _caiSetupTeamCombat(gDude, prevCritter);
                                _scripts_request_combat_locked(&combat);
                            }
                        }
                    }
                }
            }

            prevCritter = critter;
        }
    }

    return 0;
}

// wmSetupCritterObjs
// 0x4C11FC
static int wmSetupCritterObjs(int encounterIndex, Object** critterPtr, int critterCount)
{
    if (encounterIndex == -1) {
        return 0;
    }

    *critterPtr = nullptr;

    Encounter* encounter = &(wmEncBaseTypeList[encounterIndex]);

    debugPrint("\nwmSetupCritterObjs: typeIdx: %d, Formation: %s", encounterIndex, wmFormationStrs[encounter->position]);

    if (wmSetupRndNextTileNumInit(encounter) == -1) {
        return -1;
    }

    for (int index = 0; index < encounter->entriesLength; index++) {
        EncounterEntry* encounterEntry = &(encounter->entries[index]);

        if (encounterEntry->pid == -1) {
            continue;
        }

        if (!wmEvalConditional(&(encounterEntry->condition), &critterCount)) {
            continue;
        }

        int encounterEntryCritterCount;
        switch (encounterEntry->ratioMode) {
        case ENCOUNTER_RATIO_MODE_USE_RATIO:
            encounterEntryCritterCount = encounterEntry->ratio * critterCount / 100;
            break;
        case ENCOUNTER_RATIO_MODE_SINGLE:
            encounterEntryCritterCount = 1;
            break;
        default:
            assert(false && "Should be unreachable");
        }

        if (encounterEntryCritterCount < 1) {
            encounterEntryCritterCount = 1;
        }

        for (int critterIndex = 0; critterIndex < encounterEntryCritterCount; critterIndex++) {
            int tile;
            if (wmSetupRndNextTileNum(encounter, encounterEntry, &tile) == -1) {
                debugPrint("\nERROR: wmSetupCritterObjs: wmSetupRndNextTileNum:");
                continue;
            }

            if (encounterEntry->pid == -1) {
                continue;
            }

            Object* object;
            if (objectCreateWithPid(&object, encounterEntry->pid) == -1) {
                return -1;
            }

            if (*critterPtr == nullptr) {
                if (PID_TYPE(encounterEntry->pid) == OBJ_TYPE_CRITTER) {
                    *critterPtr = object;
                }
            }

            if (encounterEntry->team != -1) {
                if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                    object->data.critter.combat.team = encounterEntry->team;
                }
            }

            if (encounterEntry->scriptIdx != -1) {
                if (object->sid != -1) {
                    scriptRemove(object->sid);
                    object->sid = -1;
                }

                _obj_new_sid_inst(object, SCRIPT_TYPE_CRITTER, encounterEntry->scriptIdx - 1);
            }

            if (encounter->position != ENCOUNTER_FORMATION_TYPE_SURROUNDING) {
                objectSetLocation(object, tile, gElevation, nullptr);
            } else {
                _obj_attempt_placement(object, tile, 0, 0);
            }

            int direction = tileGetRotationTo(tile, gDude->tile);
            objectSetRotation(object, direction, nullptr);

            for (int itemIndex = 0; itemIndex < encounterEntry->itemsLength; itemIndex++) {
                EncounterItem* encounterItem = &(encounterEntry->items[itemIndex]);

                int quantity;
                if (encounterItem->maximumQuantity == encounterItem->minimumQuantity) {
                    quantity = encounterItem->maximumQuantity;
                } else {
                    quantity = randomBetween(encounterItem->minimumQuantity, encounterItem->maximumQuantity);
                }

                if (quantity == 0) {
                    continue;
                }

                Object* item;
                if (objectCreateWithPid(&item, encounterItem->pid) == -1) {
                    return -1;
                }

                if (encounterItem->pid == PROTO_ID_MONEY) {
                    if (perkHasRank(gDude, PERK_FORTUNE_FINDER)) {
                        quantity *= 2;
                    }
                }

                if (itemAdd(object, item, quantity) == -1) {
                    return -1;
                }

                _obj_disconnect(item, nullptr);

                if (encounterItem->isEquipped) {
                    if (_inven_wield(object, item, HAND_RIGHT) == -1) {
                        debugPrint("\nERROR: wmSetupCritterObjs: Inven Wield Failed: %d on %s: Critter Fid: %d", item->pid, critterGetName(object), object->fid);
                    }
                }
            }
        }
    }

    return 0;
}

// 0x4C155C
static int wmSetupRndNextTileNumInit(Encounter* encounter)
{
    for (int index = 0; index < 2; index++) {
        wmRndCenterRotations[index] = 0;
        wmRndTileDirs[index] = 0;
        wmRndCenterTiles[index] = -1;

        if (index & 1) {
            wmRndRotOffsets[index] = 5;
        } else {
            wmRndRotOffsets[index] = 1;
        }
    }

    wmRndCallCount = 0;

    switch (encounter->position) {
    case ENCOUNTER_FORMATION_TYPE_SURROUNDING:
        wmRndCenterTiles[0] = gDude->tile;
        wmRndTileDirs[0] = randomBetween(0, ROTATION_COUNT - 1);

        wmRndOriginalCenterTile = wmRndCenterTiles[0];

        return 0;
    case ENCOUNTER_FORMATION_TYPE_STRAIGHT_LINE:
    case ENCOUNTER_FORMATION_TYPE_DOUBLE_LINE:
    case ENCOUNTER_FORMATION_TYPE_WEDGE:
    case ENCOUNTER_FORMATION_TYPE_CONE:
    case ENCOUNTER_FORMATION_TYPE_HUDDLE: {
        MapInfo* map = &(wmMapInfoList[gMapHeader.index]);
        if (map->startPointsLength != 0) {
            int rspIndex = randomBetween(0, map->startPointsLength - 1);
            MapStartPointInfo* rsp = &(map->startPoints[rspIndex]);

            wmRndCenterTiles[0] = rsp->tile;
            wmRndCenterTiles[1] = wmRndCenterTiles[0];

            wmRndCenterRotations[0] = rsp->rotation;
            wmRndCenterRotations[1] = wmRndCenterRotations[0];
        } else {
            wmRndCenterRotations[0] = 0;
            wmRndCenterRotations[1] = 0;

            wmRndCenterTiles[0] = gDude->tile;
            wmRndCenterTiles[1] = gDude->tile;
        }

        wmRndTileDirs[0] = tileGetRotationTo(wmRndCenterTiles[0], gDude->tile);
        wmRndTileDirs[1] = tileGetRotationTo(wmRndCenterTiles[1], gDude->tile);

        wmRndOriginalCenterTile = wmRndCenterTiles[0];

        return 0;
    }
    default:
        debugPrint("\nERROR: wmSetupCritterObjs: invalid Formation Type!");

        return -1;
    }
}

// Determines tile to place the next object in the EncounterEntry at.
//
// wmSetupRndNextTileNum
// 0x4C16F0
static int wmSetupRndNextTileNum(Encounter* encounter, EncounterEntry* encounterEntry, int* tilePtr)
{
    int tile;

    int attempt = 0;
    while (true) {
        switch (encounter->position) {
        case ENCOUNTER_FORMATION_TYPE_SURROUNDING: {
            int distance;
            if (encounterEntry->distance != 0) {
                distance = encounterEntry->distance;
            } else {
                distance = randomBetween(-2, 2);

                distance += critterGetStat(gDude, STAT_PERCEPTION);

                if (perkHasRank(gDude, PERK_CAUTIOUS_NATURE)) {
                    distance += 3;
                }
            }

            if (distance < 0) {
                distance = 0;
            }

            int origin = encounterEntry->tile;
            if (origin == -1) {
                origin = tileGetTileInDirection(gDude->tile, wmRndTileDirs[0], distance);
            }

            if (++wmRndTileDirs[0] >= ROTATION_COUNT) {
                wmRndTileDirs[0] = 0;
            }

            int randomizedDistance = randomBetween(0, distance / 2);
            int randomizedRotation = randomBetween(0, ROTATION_COUNT - 1);
            tile = tileGetTileInDirection(origin, (randomizedRotation + wmRndTileDirs[0]) % ROTATION_COUNT, randomizedDistance);
            break;
        }
        case ENCOUNTER_FORMATION_TYPE_STRAIGHT_LINE:
            tile = wmRndCenterTiles[wmRndIndex];
            if (wmRndCallCount != 0) {
                int rotation = (wmRndRotOffsets[wmRndIndex] + wmRndTileDirs[wmRndIndex]) % ROTATION_COUNT;
                int origin = tileGetTileInDirection(wmRndCenterTiles[wmRndIndex], rotation, encounter->spacing);
                tile = tileGetTileInDirection(origin, (rotation + wmRndRotOffsets[wmRndIndex]) % ROTATION_COUNT, encounter->spacing);
                wmRndCenterTiles[wmRndIndex] = tile;
                wmRndIndex = 1 - wmRndIndex;
            }
            break;
        case ENCOUNTER_FORMATION_TYPE_DOUBLE_LINE:
            tile = wmRndCenterTiles[wmRndIndex];
            if (wmRndCallCount != 0) {
                int rotation = (wmRndRotOffsets[wmRndIndex] + wmRndTileDirs[wmRndIndex]) % ROTATION_COUNT;
                int origin = tileGetTileInDirection(wmRndCenterTiles[wmRndIndex], rotation, encounter->spacing);
                tile = tileGetTileInDirection(origin, (rotation + wmRndRotOffsets[wmRndIndex]) % ROTATION_COUNT, encounter->spacing);
                wmRndCenterTiles[wmRndIndex] = tile;
                wmRndIndex = 1 - wmRndIndex;
            }
            break;
        case ENCOUNTER_FORMATION_TYPE_WEDGE:
            tile = wmRndCenterTiles[wmRndIndex];
            if (wmRndCallCount != 0) {
                tile = tileGetTileInDirection(wmRndCenterTiles[wmRndIndex], (wmRndRotOffsets[wmRndIndex] + wmRndTileDirs[wmRndIndex]) % ROTATION_COUNT, encounter->spacing);
                wmRndCenterTiles[wmRndIndex] = tile;
                wmRndIndex = 1 - wmRndIndex;
            }
            break;
        case ENCOUNTER_FORMATION_TYPE_CONE:
            tile = wmRndCenterTiles[wmRndIndex];
            if (wmRndCallCount != 0) {
                tile = tileGetTileInDirection(wmRndCenterTiles[wmRndIndex], (wmRndTileDirs[wmRndIndex] + 3 + wmRndRotOffsets[wmRndIndex]) % ROTATION_COUNT, encounter->spacing);
                wmRndCenterTiles[wmRndIndex] = tile;
                wmRndIndex = 1 - wmRndIndex;
            }
            break;
        case ENCOUNTER_FORMATION_TYPE_HUDDLE:
            tile = wmRndCenterTiles[0];
            if (wmRndCallCount != 0) {
                wmRndTileDirs[0] = (wmRndTileDirs[0] + 1) % ROTATION_COUNT;
                tile = tileGetTileInDirection(wmRndCenterTiles[0], wmRndTileDirs[0], encounter->spacing);
                wmRndCenterTiles[0] = tile;
            }
            break;
        default:
            assert(false && "Should be unreachable");
        }

        ++attempt;
        ++wmRndCallCount;

        if (wmEvalTileNumForPlacement(tile)) {
            break;
        }

        debugPrint("\nWARNING: EVAL-TILE-NUM FAILED!");

        if (tileDistanceBetween(wmRndOriginalCenterTile, wmRndCenterTiles[wmRndIndex]) > 25) {
            return -1;
        }

        if (attempt > 25) {
            return -1;
        }
    }

    debugPrint("\nwmSetupRndNextTileNum:TileNum: %d", tile);

    *tilePtr = tile;

    return 0;
}

// 0x4C1A64
bool wmEvalTileNumForPlacement(int tile)
{
    if (_obj_blocking_at(gDude, tile, gElevation) != nullptr) {
        return false;
    }

    if (pathfinderFindPath(gDude, gDude->tile, tile, nullptr, 0, _obj_shoot_blocking_at) == 0) {
        return false;
    }

    return true;
}

// 0x4C1AC8
static bool wmEvalConditional(EncounterCondition* condition, int* critterCountPtr)
{
    int value;

    bool matches = true;
    for (int index = 0; index < condition->entriesLength; index++) {
        EncounterConditionEntry* conditionEntry = &(condition->entries[index]);

        matches = true;
        switch (conditionEntry->type) {
        case ENCOUNTER_CONDITION_TYPE_GLOBAL:
            value = gameGetGlobalVar(conditionEntry->param);
            if (!wmEvalSubConditional(value, conditionEntry->conditionalOperator, conditionEntry->value)) {
                matches = false;
            }
            break;
        case ENCOUNTER_CONDITION_TYPE_NUMBER_OF_CRITTERS:
            if (!wmEvalSubConditional(*critterCountPtr, conditionEntry->conditionalOperator, conditionEntry->value)) {
                matches = false;
            }
            break;
        case ENCOUNTER_CONDITION_TYPE_RANDOM:
            value = randomBetween(0, 100);
            if (value > conditionEntry->param) {
                matches = false;
            }
            break;
        case ENCOUNTER_CONDITION_TYPE_PLAYER:
            value = pcGetStat(PC_STAT_LEVEL);
            if (!wmEvalSubConditional(value, conditionEntry->conditionalOperator, conditionEntry->value)) {
                matches = false;
            }
            break;
        case ENCOUNTER_CONDITION_TYPE_DAYS_PLAYED:
            value = gameTimeGetTime();
            if (!wmEvalSubConditional(value / GAME_TIME_TICKS_PER_DAY, conditionEntry->conditionalOperator, conditionEntry->value)) {
                matches = false;
            }
            break;
        case ENCOUNTER_CONDITION_TYPE_TIME_OF_DAY:
            value = gameTimeGetHour();
            if (!wmEvalSubConditional(value / 100, conditionEntry->conditionalOperator, conditionEntry->value)) {
                matches = false;
            }
            break;
        }

        if (!matches) {
            // FIXME: Can overflow with all 3 conditions specified.
            if (condition->logicalOperators[index] == ENCOUNTER_LOGICAL_OPERATOR_AND) {
                break;
            }
        }
    }

    return matches;
}

// 0x4C1C0C
static bool wmEvalSubConditional(int operand1, int condionalOperator, int operand2)
{
    switch (condionalOperator) {
    case ENCOUNTER_CONDITIONAL_OPERATOR_EQUAL:
        return operand1 == operand2;
    case ENCOUNTER_CONDITIONAL_OPERATOR_NOT_EQUAL:
        return operand1 != operand2;
    case ENCOUNTER_CONDITIONAL_OPERATOR_LESS_THAN:
        return operand1 < operand2;
    case ENCOUNTER_CONDITIONAL_OPERATOR_GREATER_THAN:
        return operand1 > operand2;
    }

    return false;
}

// 0x4C1C50
static bool wmGameTimeIncrement(int ticksToAdd)
{
    if (ticksToAdd == 0) {
        return false;
    }

    // SFALL: Fix Pathfinder perk.
    int pathfinderRank = perkGetRank(gDude, PERK_PATHFINDER);
    double bonus = static_cast<double>(ticksToAdd) * static_cast<double>(pathfinderRank) * 0.25 + gGameTimeIncRemainder;
    gGameTimeIncRemainder = modf(bonus, &bonus);
    ticksToAdd -= static_cast<int>(bonus);

    while (ticksToAdd != 0) {
        unsigned int gameTime = gameTimeGetTime();
        unsigned int nextEventTime = queueGetNextEventTime();
        int ticksToNextEvent = nextEventTime >= gameTime ? ticksToAdd : nextEventTime - gameTime;
        ticksToAdd -= ticksToNextEvent;

        gameTimeAddTicks(ticksToNextEvent);

        // NOTE: Uninline.
        wmInterfaceDialSyncTime(true);

        wmInterfaceRefreshDate(true);

        if (queueProcessEvents()) {
            break;
        }
    }

    return true;
}

// Reads .msk file if needed.
//
// 0x4C1CE8
static int wmGrabTileWalkMask(int tileIdx)
{
    TileInfo* tileInfo = &(wmTileInfoList[tileIdx]);
    if (tileInfo->walkMaskData != nullptr) {
        return 0;
    }

    if (*tileInfo->walkMaskName == '\0') {
        return 0;
    }

    tileInfo->walkMaskData = (unsigned char*)internal_malloc(13200);
    if (tileInfo->walkMaskData == nullptr) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "data\\%s.msk", tileInfo->walkMaskName);

    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        return -1;
    }

    int rc = 0;

    if (fileReadUInt8List(stream, tileInfo->walkMaskData, 13200) == -1) {
        rc = -1;
    }

    fileClose(stream);

    return rc;
}

// 0x4C1D9C
static bool wmWorldPosInvalid(int x, int y)
{
    int tileIdx = y / WM_TILE_HEIGHT * wmNumHorizontalTiles + x / WM_TILE_WIDTH % wmNumHorizontalTiles;
    if (wmGrabTileWalkMask(tileIdx) == -1) {
        return false;
    }

    TileInfo* tileDescription = &(wmTileInfoList[tileIdx]);
    unsigned char* mask = tileDescription->walkMaskData;
    if (mask == nullptr) {
        return false;
    }

    // Mask length is 13200, which is 300 * 44
    // 44 * 8 is 352, which is probably left 2 bytes intact
    // TODO: Check math.
    int pos = (y % WM_TILE_HEIGHT) * 44 + (x % WM_TILE_WIDTH) / 8;
    int bit = 1 << (((x % WM_TILE_WIDTH) / 8) & 3);
    return (mask[pos] & bit) != 0;
}

// 0x4C1E54
static void wmPartyInitWalking(int x, int y)
{
    wmGenData.walkDestinationX = x;
    wmGenData.walkDestinationY = y;
    wmGenData.currentAreaId = -1;
    wmGenData.isWalking = true;

    int dx = abs(x - wmGenData.worldPosX);
    int dy = abs(y - wmGenData.worldPosY);

    if (dx < dy) {
        wmGenData.walkDistance = dy;
        wmGenData.walkLineDeltaMainAxisStep = 2 * dx;
        wmGenData.walkWorldPosMainAxisStepX = 0;
        wmGenData.walkLineDelta = 2 * dx - dy;
        wmGenData.walkLineDeltaCrossAxisStep = 2 * (dx - dy);
        wmGenData.walkWorldPosCrossAxisStepX = 1;
        wmGenData.walkWorldPosMainAxisStepY = 1;
        wmGenData.walkWorldPosCrossAxisStepY = 1;
    } else {
        wmGenData.walkDistance = dx;
        wmGenData.walkLineDeltaMainAxisStep = 2 * dy;
        wmGenData.walkWorldPosMainAxisStepY = 0;
        wmGenData.walkLineDelta = 2 * dy - dx;
        wmGenData.walkLineDeltaCrossAxisStep = 2 * (dy - dx);
        wmGenData.walkWorldPosMainAxisStepX = 1;
        wmGenData.walkWorldPosCrossAxisStepX = 1;
        wmGenData.walkWorldPosCrossAxisStepY = 1;
    }

    if (wmGenData.walkDestinationX < wmGenData.worldPosX) {
        wmGenData.walkWorldPosCrossAxisStepX = -wmGenData.walkWorldPosCrossAxisStepX;
        wmGenData.walkWorldPosMainAxisStepX = -wmGenData.walkWorldPosMainAxisStepX;
    }

    if (wmGenData.walkDestinationY < wmGenData.worldPosY) {
        wmGenData.walkWorldPosCrossAxisStepY = -wmGenData.walkWorldPosCrossAxisStepY;
        wmGenData.walkWorldPosMainAxisStepY = -wmGenData.walkWorldPosMainAxisStepY;
    }

    if (!wmCursorIsVisible()) {
        wmInterfaceCenterOnParty();
    }
}

// 0x4C1F90
static void wmPartyWalkingStep()
{
    if (wmGenData.walkDistance <= 0) {
        return;
    }

    _terrainCounter++;
    if (_terrainCounter > 4) {
        _terrainCounter = 1;
    }

    // NOTE: Uninline.
    wmPartyFindCurSubTile();

    Terrain* terrain = &(wmTerrainTypeList[wmGenData.currentSubtile->terrain]);
    // SFALL: Fix Pathfinder perk.
    int terrainDifficulty = terrain->difficulty;
    if (terrainDifficulty < 1) {
        terrainDifficulty = 1;
    }

    if (_terrainCounter / terrainDifficulty >= 1) {
        if (wmGenData.walkLineDelta >= 0) {
            if (wmWorldPosInvalid(wmGenData.walkWorldPosCrossAxisStepX + wmGenData.worldPosX, wmGenData.walkWorldPosCrossAxisStepY + wmGenData.worldPosY)) {
                wmGenData.walkDestinationX = 0;
                wmGenData.walkDestinationY = 0;
                wmGenData.isWalking = false;
                wmMatchWorldPosToArea(wmGenData.worldPosX, wmGenData.worldPosX, &(wmGenData.currentAreaId));
                wmGenData.walkDistance = 0;
                return;
            }

            wmGenData.walkLineDelta += wmGenData.walkLineDeltaCrossAxisStep;
            wmGenData.worldPosX += wmGenData.walkWorldPosCrossAxisStepX;
            wmGenData.worldPosY += wmGenData.walkWorldPosCrossAxisStepY;

            wmInterfaceScrollPixel(1,
                1,
                wmGenData.walkWorldPosCrossAxisStepX,
                wmGenData.walkWorldPosCrossAxisStepY,
                nullptr,
                false);
        } else {
            if (wmWorldPosInvalid(wmGenData.walkWorldPosMainAxisStepX + wmGenData.worldPosX, wmGenData.walkWorldPosMainAxisStepY + wmGenData.worldPosY) == 1) {
                wmGenData.walkDestinationX = 0;
                wmGenData.walkDestinationY = 0;
                wmGenData.isWalking = false;
                wmMatchWorldPosToArea(wmGenData.worldPosX, wmGenData.worldPosX, &(wmGenData.currentAreaId));
                wmGenData.walkDistance = 0;
                return;
            }

            wmGenData.walkLineDelta += wmGenData.walkLineDeltaMainAxisStep;
            wmGenData.worldPosY += wmGenData.walkWorldPosMainAxisStepY;
            wmGenData.worldPosX += wmGenData.walkWorldPosMainAxisStepX;

            wmInterfaceScrollPixel(1,
                1,
                wmGenData.walkWorldPosMainAxisStepX,
                wmGenData.walkWorldPosMainAxisStepY,
                nullptr,
                false);
        }

        wmGenData.walkDistance -= 1;
        if (wmGenData.walkDistance == 0) {
            wmGenData.walkDestinationY = 0;
            wmGenData.isWalking = false;
            wmGenData.walkDestinationX = 0;
        }
    }
}

// 0x4C219C
static void wmInterfaceScrollTabsStart(int delta)
{
    // SFALL: Fix world map cities list scrolling bug that might leave buttons
    // in the disabled state.
    if (delta >= 0) {
        if (wmGenData.tabsOffsetY < wmGenData.tabsBackgroundFrmImage.getHeight() - 230) {
            wmGenData.oldTabsOffsetY = std::min(wmGenData.tabsOffsetY + 7 * delta, wmGenData.tabsBackgroundFrmImage.getHeight() - 230);
            wmGenData.tabsScrollingDelta = delta;
        }
    } else {
        if (wmGenData.tabsOffsetY > 0) {
            wmGenData.oldTabsOffsetY = std::max(wmGenData.tabsOffsetY + 7 * delta, 0);
            wmGenData.tabsScrollingDelta = delta;
        }
    }

    if (wmGenData.tabsScrollingDelta == 0) {
        return;
    }

    for (int index = 0; index < 7; index++) {
        buttonDisable(wmTownMapSubButtonIds[index]);
    }

    wmInterfaceScrollTabsUpdate();
}

// 0x4C2270
static void wmInterfaceScrollTabsStop()
{
    wmGenData.tabsScrollingDelta = 0;

    for (int index = 0; index < 7; index++) {
        buttonEnable(wmTownMapSubButtonIds[index]);
    }
}

// NOTE: Inlined.
//
// 0x4C2290
static void wmInterfaceScrollTabsUpdate()
{
    if (wmGenData.tabsScrollingDelta != 0) {
        wmGenData.tabsOffsetY += wmGenData.tabsScrollingDelta;
        wmRefreshInterfaceOverlay(true);

        if (wmGenData.tabsScrollingDelta >= 0) {
            if (wmGenData.oldTabsOffsetY <= wmGenData.tabsOffsetY) {
                // NOTE: Uninline.
                wmInterfaceScrollTabsStop();
            }
        } else {
            if (wmGenData.oldTabsOffsetY >= wmGenData.tabsOffsetY) {
                // NOTE: Uninline.
                wmInterfaceScrollTabsStop();
            }
        }
    }
}

// 0x4C2324
static int wmInterfaceInit()
{
    int fid;

    wmLastRndTime = getTicks();

    // SFALL: Fix default worldmap font.
    // CE: This setting affects only city names. In Sfall it's configurable via
    // WorldMapFontPatch and is turned off by default.
    wmGenData.oldFont = fontGetCurrent();
    fontSetCurrent(101);

    _map_save_in_game(true);

    const char* backgroundSoundFileName = wmGenData.isInCar ? "20car" : "23world";
    _gsound_background_play_level_music(backgroundSoundFileName, 12);

    // CE: Hide entire interface, not just indicator bar, and disable tile
    // engine.
    interfaceBarHide();
    tileDisable();
    isoDisable();
    colorCycleDisable();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    // CE: Clear map window.
    windowFill(gIsoWindow,
        0,
        0,
        windowGetWidth(gIsoWindow),
        windowGetHeight(gIsoWindow),
        _colorTable[0]);
    windowRefresh(gIsoWindow);

    // CE: Stop all animations.
    animationStop();

    int worldmapWindowX = (screenGetWidth() - WM_WINDOW_WIDTH) / 2;
    int worldmapWindowY = (screenGetHeight() - WM_WINDOW_HEIGHT) / 2;
    wmBkWin = windowCreate(worldmapWindowX, worldmapWindowY, WM_WINDOW_WIDTH, WM_WINDOW_HEIGHT, _colorTable[0], WINDOW_MOVE_ON_TOP);
    if (wmBkWin == -1) {
        return -1;
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 136, 0, 0, 0);
    if (!_backgroundFrmImage.lock(fid)) {
        return -1;
    }

    wmBkWinBuf = windowGetBuffer(wmBkWin);
    if (wmBkWinBuf == nullptr) {
        return -1;
    }

    blitBufferToBuffer(_backgroundFrmImage.getData(),
        _backgroundFrmImage.getWidth(),
        _backgroundFrmImage.getHeight(),
        _backgroundFrmImage.getWidth(),
        wmBkWinBuf,
        WM_WINDOW_WIDTH);

    for (int citySize = 0; citySize < CITY_SIZE_COUNT; citySize++) {
        CitySizeDescription* citySizeDescription = &(wmSphereData[citySize]);
        if (!citySizeDescription->frmImage.lock(citySizeDescription->fid)) {
            return -1;
        }
    }

    // hotspot1.frm - town map selector shape #1
    fid = buildFid(OBJ_TYPE_INTERFACE, 168, 0, 0, 0);
    if (!wmGenData.hotspotNormalFrmImage.lock(fid)) {
        return -1;
    }

    // hotspot2.frm - town map selector shape #2
    fid = buildFid(OBJ_TYPE_INTERFACE, 223, 0, 0, 0);
    if (!wmGenData.hotspotPressedFrmImage.lock(fid)) {
        return -1;
    }

    // wmaptarg.frm - world map move target maker #1
    fid = buildFid(OBJ_TYPE_INTERFACE, 139, 0, 0, 0);
    if (!wmGenData.destinationMarkerFrmImage.lock(fid)) {
        return -1;
    }

    // wmaploc.frm - world map location marker
    fid = buildFid(OBJ_TYPE_INTERFACE, 138, 0, 0, 0);
    if (!wmGenData.locationMarkerFrmImage.lock(fid)) {
        return -1;
    }

    for (int index = 0; index < WORLD_MAP_ENCOUNTER_FRM_COUNT; index++) {
        fid = buildFid(OBJ_TYPE_INTERFACE, wmRndCursorFids[index], 0, 0, 0);
        if (!wmGenData.encounterCursorFrmImages[index].lock(fid)) {
            return -1;
        }
    }

    for (int index = 0; index < wmMaxTileNum; index++) {
        wmTileInfoList[index].handle = INVALID_CACHE_ENTRY;
    }

    // wmtabs.frm - worldmap town tabs underlay
    fid = buildFid(OBJ_TYPE_INTERFACE, 364, 0, 0, 0);
    if (!wmGenData.tabsBackgroundFrmImage.lock(fid)) {
        return -1;
    }

    // wmtbedge.frm - worldmap town tabs edging overlay
    fid = buildFid(OBJ_TYPE_INTERFACE, 367, 0, 0, 0);
    if (!wmGenData.tabsBorderFrmImage.lock(fid)) {
        return -1;
    }

    // wmdial.frm - worldmap night/day dial
    fid = buildFid(OBJ_TYPE_INTERFACE, 365, 0, 0, 0);
    wmGenData.dialFrm = artLock(fid, &(wmGenData.dialFrmHandle));
    if (wmGenData.dialFrm == nullptr) {
        return -1;
    }

    wmGenData.dialFrmWidth = artGetWidth(wmGenData.dialFrm, 0, 0);
    wmGenData.dialFrmHeight = artGetHeight(wmGenData.dialFrm, 0, 0);

    // wmscreen - worldmap overlay screen
    fid = buildFid(OBJ_TYPE_INTERFACE, 363, 0, 0, 0);
    if (!wmGenData.carOverlayFrmImage.lock(fid)) {
        return -1;
    }

    // wmglobe.frm - worldmap globe stamp overlay
    fid = buildFid(OBJ_TYPE_INTERFACE, 366, 0, 0, 0);
    if (!wmGenData.globeOverlayFrmImage.lock(fid)) {
        return -1;
    }

    // lilredup.frm - little red button up
    fid = buildFid(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    wmGenData.redButtonNormalFrmImage.lock(fid);

    // lilreddn.frm - little red button down
    fid = buildFid(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    wmGenData.redButtonPressedFrmImage.lock(fid);

    // months.frm - month strings for pip boy
    fid = buildFid(OBJ_TYPE_INTERFACE, 129, 0, 0, 0);
    if (!wmGenData.monthsFrmImage.lock(fid)) {
        return -1;
    }

    // numbers.frm - numbers for the hit points and fatigue counters
    fid = buildFid(OBJ_TYPE_INTERFACE, 82, 0, 0, 0);
    if (!wmGenData.numbersFrmImage.lock(fid)) {
        return -1;
    }

    // create town/world switch button
    int switchBtn = buttonCreate(wmBkWin,
        WM_TOWN_WORLD_SWITCH_X,
        WM_TOWN_WORLD_SWITCH_Y,
        wmGenData.redButtonNormalFrmImage.getWidth(),
        wmGenData.redButtonNormalFrmImage.getHeight(),
        -1,
        -1,
        -1,
        KEY_UPPERCASE_T,
        wmGenData.redButtonNormalFrmImage.getData(),
        wmGenData.redButtonPressedFrmImage.getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);

    // SFALL: Add missing button sounds.
    if (switchBtn != -1) {
        buttonSetCallbacks(switchBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    for (int index = 0; index < 7; index++) {
        wmTownMapSubButtonIds[index] = buttonCreate(wmBkWin,
            508,
            138 + 27 * index,
            wmGenData.redButtonNormalFrmImage.getWidth(),
            wmGenData.redButtonNormalFrmImage.getHeight(),
            -1,
            -1,
            -1,
            KEY_CTRL_F1 + index,
            wmGenData.redButtonNormalFrmImage.getData(),
            wmGenData.redButtonPressedFrmImage.getData(),
            nullptr,
            BUTTON_FLAG_TRANSPARENT);

        // SFALL: Add missing button sounds.
        if (wmTownMapSubButtonIds[index] != -1) {
            buttonSetCallbacks(wmTownMapSubButtonIds[index], _gsound_red_butt_press, _gsound_red_butt_release);
        }
    }

    for (int index = 0; index < WORLDMAP_ARROW_FRM_COUNT; index++) {
        // 200 - uparwon.frm - character editor
        // 199 - uparwoff.frm - character editor
        // SFALL: Fix images for scroll buttons.
        fid = buildFid(OBJ_TYPE_INTERFACE, 199 + index, 0, 0, 0);
        if (!wmGenData.scrollUpButtonFrmImages[index].lock(fid)) {
            return -1;
        }
    }

    for (int index = 0; index < WORLDMAP_ARROW_FRM_COUNT; index++) {
        // 182 - dnarwon.frm - character editor
        // 181 - dnarwoff.frm - character editor
        // SFALL: Fix images for scroll buttons.
        fid = buildFid(OBJ_TYPE_INTERFACE, 181 + index, 0, 0, 0);
        if (!wmGenData.scrollDownButtonFrmImages[index].lock(fid)) {
            return -1;
        }
    }

    // Scroll up button.
    int scrollUpBtn = buttonCreate(wmBkWin,
        WM_TOWN_LIST_SCROLL_UP_X,
        WM_TOWN_LIST_SCROLL_UP_Y,
        wmGenData.scrollUpButtonFrmImages[WORLDMAP_ARROW_FRM_NORMAL].getWidth(),
        wmGenData.scrollUpButtonFrmImages[WORLDMAP_ARROW_FRM_NORMAL].getHeight(),
        -1,
        -1,
        -1,
        KEY_CTRL_ARROW_UP,
        wmGenData.scrollUpButtonFrmImages[WORLDMAP_ARROW_FRM_NORMAL].getData(),
        wmGenData.scrollUpButtonFrmImages[WORLDMAP_ARROW_FRM_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);

    // SFALL: Add missing button sounds.
    if (scrollUpBtn != -1) {
        buttonSetCallbacks(scrollUpBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    // Scroll down button.
    int scrollDownBtn = buttonCreate(wmBkWin,
        WM_TOWN_LIST_SCROLL_DOWN_X,
        WM_TOWN_LIST_SCROLL_DOWN_Y,
        wmGenData.scrollDownButtonFrmImages[WORLDMAP_ARROW_FRM_NORMAL].getWidth(),
        wmGenData.scrollDownButtonFrmImages[WORLDMAP_ARROW_FRM_NORMAL].getHeight(),
        -1,
        -1,
        -1,
        KEY_CTRL_ARROW_DOWN,
        wmGenData.scrollDownButtonFrmImages[WORLDMAP_ARROW_FRM_NORMAL].getData(),
        wmGenData.scrollDownButtonFrmImages[WORLDMAP_ARROW_FRM_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);

    // SFALL: Add missing button sounds.
    if (scrollDownBtn != -1) {
        buttonSetCallbacks(scrollDownBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    if (wmGenData.isInCar) {
        // wmcarmve.frm - worldmap car movie
        fid = buildFid(OBJ_TYPE_INTERFACE, 433, 0, 0, 0);
        wmGenData.carImageFrm = artLock(fid, &(wmGenData.carImageFrmHandle));
        if (wmGenData.carImageFrm == nullptr) {
            return -1;
        }

        wmGenData.carImageFrmWidth = artGetWidth(wmGenData.carImageFrm, 0, 0);
        wmGenData.carImageFrmHeight = artGetHeight(wmGenData.carImageFrm, 0, 0);
    }

    tickersAdd(wmMouseBkProc);

    if (wmMakeTabsLabelList(&wmLabelList, &wmLabelCount) == -1) {
        return -1;
    }

    wmInterfaceWasInitialized = 1;

    if (wmInterfaceRefresh() == -1) {
        return -1;
    }

    windowRefresh(wmBkWin);
    scriptsDisable();
    _scr_remove_all();

    return 0;
}

// 0x4C2E44
static int wmInterfaceExit()
{
    int i;
    TileInfo* tile;

    tickersRemove(wmMouseBkProc);

    _backgroundFrmImage.unlock();

    if (wmBkWin != -1) {
        windowDestroy(wmBkWin);
        wmBkWin = -1;
    }

    wmGenData.hotspotNormalFrmImage.unlock();
    wmGenData.hotspotPressedFrmImage.unlock();

    wmGenData.destinationMarkerFrmImage.unlock();
    wmGenData.locationMarkerFrmImage.unlock();

    for (i = 0; i < 4; i++) {
        wmGenData.encounterCursorFrmImages[i].unlock();
    }

    for (i = 0; i < CITY_SIZE_COUNT; i++) {
        CitySizeDescription* citySizeDescription = &(wmSphereData[i]);
        citySizeDescription->frmImage.unlock();
    }

    for (i = 0; i < wmMaxTileNum; i++) {
        tile = &(wmTileInfoList[i]);
        if (tile->handle != INVALID_CACHE_ENTRY) {
            artUnlock(tile->handle);
            tile->handle = INVALID_CACHE_ENTRY;
            tile->data = nullptr;

            if (tile->walkMaskData != nullptr) {
                internal_free(tile->walkMaskData);
                tile->walkMaskData = nullptr;
            }
        }
    }

    wmGenData.tabsBackgroundFrmImage.unlock();
    wmGenData.tabsBorderFrmImage.unlock();

    if (wmGenData.dialFrm != nullptr) {
        artUnlock(wmGenData.dialFrmHandle);
        wmGenData.dialFrmHandle = INVALID_CACHE_ENTRY;
        wmGenData.dialFrm = nullptr;
    }

    wmGenData.carOverlayFrmImage.unlock();
    wmGenData.globeOverlayFrmImage.unlock();

    wmGenData.redButtonNormalFrmImage.unlock();
    wmGenData.redButtonPressedFrmImage.unlock();

    for (i = 0; i < 2; i++) {
        wmGenData.scrollUpButtonFrmImages[i].unlock();
        wmGenData.scrollDownButtonFrmImages[i].unlock();
    }

    wmGenData.monthsFrmImage.unlock();
    wmGenData.numbersFrmImage.unlock();

    if (wmGenData.carImageFrm != nullptr) {
        artUnlock(wmGenData.carImageFrmHandle);
        wmGenData.carImageFrmHandle = INVALID_CACHE_ENTRY;
        wmGenData.carImageFrm = nullptr;

        wmGenData.carImageFrmWidth = 0;
        wmGenData.carImageFrmHeight = 0;
    }

    wmGenData.encounterIconIsVisible = false;
    wmGenData.encounterMapId = -1;
    wmGenData.encounterTableId = -1;
    wmGenData.encounterEntryId = -1;

    // CE: Enable tile engine and interface.
    interfaceBarShow();
    tileEnable();
    isoEnable();
    colorCycleEnable();

    fontSetCurrent(wmGenData.oldFont);

    // NOTE: Uninline.
    wmFreeTabsLabelList(&wmLabelList, &wmLabelCount);

    wmInterfaceWasInitialized = 0;

    scriptsEnable();

    return 0;
}

// NOTE: Inlined.
//
// 0x4C31E8
static int wmInterfaceScroll(int dx, int dy, bool* successPtr)
{
    return wmInterfaceScrollPixel(20, 20, dx, dy, successPtr, 1);
}

// FIXME: There is small bug in this function. There is [success] flag returned
// by reference so that calling code can update scrolling mouse cursor to invalid
// range. It works OK on straight directions. But in diagonals when scrolling in
// one direction is possible (and in fact occured), it will still be reported as
// error.
//
// 0x4C3200
static int wmInterfaceScrollPixel(int stepX, int stepY, int dx, int dy, bool* success, bool shouldRefresh)
{
    if (success != nullptr) {
        *success = true;
    }

    if (dy < 0) {
        if (wmWorldOffsetY > 0) {
            wmWorldOffsetY -= stepY;
            if (wmWorldOffsetY < 0) {
                wmWorldOffsetY = 0;
            }
        } else {
            if (success != nullptr) {
                *success = false;
            }
        }
    } else if (dy > 0) {
        if (wmWorldOffsetY < wmGenData.viewportMaxY) {
            wmWorldOffsetY += stepY;
            if (wmWorldOffsetY > wmGenData.viewportMaxY) {
                wmWorldOffsetY = wmGenData.viewportMaxY;
            }
        } else {
            if (success != nullptr) {
                *success = false;
            }
        }
    }

    if (dx < 0) {
        if (wmWorldOffsetX > 0) {
            wmWorldOffsetX -= stepX;
            if (wmWorldOffsetX < 0) {
                wmWorldOffsetX = 0;
            }
        } else {
            if (success != nullptr) {
                *success = false;
            }
        }
    } else if (dx > 0) {
        if (wmWorldOffsetX < wmGenData.viewportMaxX) {
            wmWorldOffsetX += stepX;
            if (wmWorldOffsetX > wmGenData.viewportMaxX) {
                wmWorldOffsetX = wmGenData.viewportMaxX;
            }
        } else {
            if (success != nullptr) {
                *success = false;
            }
        }
    }

    if (shouldRefresh) {
        if (wmInterfaceRefresh() == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4C32EC
static void wmMouseBkProc()
{
    // 0x51DEB0
    static unsigned int lastTime = 0;

    // 0x51DEB4
    static bool couldScroll = true;

    int x;
    int y;
    mouseGetPosition(&x, &y);

    int dx = 0;
    if (x == screenGetWidth() - 1) {
        dx = 1;
    } else if (x == 0) {
        dx = -1;
    }

    int dy = 0;
    if (y == screenGetHeight() - 1) {
        dy = 1;
    } else if (y == 0) {
        dy = -1;
    }

    int oldMouseCursor = gameMouseGetCursor();
    int newMouseCursor = oldMouseCursor;

    if (dx != 0 || dy != 0) {
        if (dx > 0) {
            if (dy > 0) {
                newMouseCursor = MOUSE_CURSOR_SCROLL_SE;
            } else if (dy < 0) {
                newMouseCursor = MOUSE_CURSOR_SCROLL_NE;
            } else {
                newMouseCursor = MOUSE_CURSOR_SCROLL_E;
            }
        } else if (dx < 0) {
            if (dy > 0) {
                newMouseCursor = MOUSE_CURSOR_SCROLL_SW;
            } else if (dy < 0) {
                newMouseCursor = MOUSE_CURSOR_SCROLL_NW;
            } else {
                newMouseCursor = MOUSE_CURSOR_SCROLL_W;
            }
        } else {
            if (dy < 0) {
                newMouseCursor = MOUSE_CURSOR_SCROLL_N;
            } else if (dy > 0) {
                newMouseCursor = MOUSE_CURSOR_SCROLL_S;
            }
        }

        unsigned int tick = _get_bk_time();
        if (getTicksBetween(tick, lastTime) > 50) {
            lastTime = _get_bk_time();
            // NOTE: Uninline.
            wmInterfaceScroll(dx, dy, &couldScroll);
        }

        if (!couldScroll) {
            newMouseCursor += 8;
        }
    } else {
        if (oldMouseCursor != MOUSE_CURSOR_ARROW) {
            newMouseCursor = MOUSE_CURSOR_ARROW;
        }
    }

    if (oldMouseCursor != newMouseCursor) {
        gameMouseSetCursor(newMouseCursor);
    }
}

// NOTE: Inlined.
//
// 0x4C340C
static int wmMarkSubTileOffsetVisited(int tile, int subtileX, int subtileY, int offsetX, int offsetY)
{
    return wmMarkSubTileOffsetVisitedFunc(tile, subtileX, subtileY, offsetX, offsetY, SUBTILE_STATE_VISITED);
}

// NOTE: Inlined.
//
// 0x4C3420
static int wmMarkSubTileOffsetKnown(int tile, int subtileX, int subtileY, int offsetX, int offsetY)
{
    return wmMarkSubTileOffsetVisitedFunc(tile, subtileX, subtileY, offsetX, offsetY, SUBTILE_STATE_KNOWN);
}

// 0x4C3434
static int wmMarkSubTileOffsetVisitedFunc(int tile, int subtileX, int subtileY, int offsetX, int offsetY, int subtileState)
{
    int actualTile;
    int actualSubtileX;
    int actualSubtileY;
    TileInfo* tileInfo;
    SubtileInfo* subtileInfo;

    actualSubtileX = subtileX + offsetX;
    actualTile = tile;
    actualSubtileY = subtileY + offsetY;

    if (actualSubtileX >= 0) {
        if (actualSubtileX >= SUBTILE_GRID_WIDTH) {
            if (tile % wmNumHorizontalTiles == wmNumHorizontalTiles - 1) {
                return -1;
            }

            actualTile = tile + 1;
            actualSubtileX %= SUBTILE_GRID_WIDTH;
        }
    } else {
        if (!(tile % wmNumHorizontalTiles)) {
            return -1;
        }

        actualSubtileX += SUBTILE_GRID_WIDTH;
        actualTile = tile - 1;
    }

    if (actualSubtileY >= 0) {
        if (actualSubtileY >= SUBTILE_GRID_HEIGHT) {
            if (actualTile > wmMaxTileNum - wmNumHorizontalTiles - 1) {
                return -1;
            }

            actualTile += wmNumHorizontalTiles;
            actualSubtileY %= SUBTILE_GRID_HEIGHT;
        }
    } else {
        if (actualTile < wmNumHorizontalTiles) {
            return -1;
        }

        actualSubtileY += SUBTILE_GRID_HEIGHT;
        actualTile -= wmNumHorizontalTiles;
    }

    tileInfo = &(wmTileInfoList[actualTile]);
    subtileInfo = &(tileInfo->subtiles[actualSubtileY][actualSubtileX]);
    if (subtileState != SUBTILE_STATE_KNOWN || subtileInfo->state == SUBTILE_STATE_UNKNOWN) {
        subtileInfo->state = subtileState;
    }

    return 0;
}

// 0x4C3550
static void wmMarkSubTileRadiusVisited(int x, int y)
{
    int radius = 1;

    if (perkHasRank(gDude, PERK_SCOUT)) {
        radius = 2;
    }

    wmSubTileMarkRadiusVisited(x, y, radius);
}

// 0x4C35A8
int wmSubTileMarkRadiusVisited(int x, int y, int radius)
{
    int tile;
    int subtileX;
    int subtileY;
    int offsetX;
    int offsetY;
    SubtileInfo* subtile;

    tile = x / WM_TILE_WIDTH % wmNumHorizontalTiles + y / WM_TILE_HEIGHT * wmNumHorizontalTiles;
    subtileX = x % WM_TILE_WIDTH / WM_SUBTILE_SIZE;
    subtileY = y % WM_TILE_HEIGHT / WM_SUBTILE_SIZE;

    for (offsetY = -radius; offsetY <= radius; offsetY++) {
        for (offsetX = -radius; offsetX <= radius; offsetX++) {
            // NOTE: Uninline.
            wmMarkSubTileOffsetKnown(tile, subtileX, subtileY, offsetX, offsetY);
        }
    }

    subtile = &(wmTileInfoList[tile].subtiles[subtileY][subtileX]);
    subtile->state = SUBTILE_STATE_VISITED;

    switch (subtile->fill) {
    case SUBTILE_FILL_S:
        while (subtileY-- > 0) {
            // NOTE: Uninline.
            wmMarkSubTileOffsetVisited(tile, subtileX, subtileY, 0, 0);
        }
        break;
    case SUBTILE_FILL_W:
        while (subtileX-- >= 0) {
            // NOTE: Uninline.
            wmMarkSubTileOffsetVisited(tile, subtileX, subtileY, 0, 0);
        }

        if (tile % wmNumHorizontalTiles > 0) {
            for (subtileX = 0; subtileX < SUBTILE_GRID_WIDTH; subtileX++) {
                // NOTE: Uninline.
                wmMarkSubTileOffsetVisited(tile - 1, subtileX, subtileY, 0, 0);
            }
        }
        break;
    }

    return 0;
}

// 0x4C3740
int wmSubTileGetVisitedState(int x, int y, int* statePtr)
{
    TileInfo* tile;
    SubtileInfo* subtile;

    tile = &(wmTileInfoList[y / WM_TILE_HEIGHT * wmNumHorizontalTiles + x / WM_TILE_WIDTH % wmNumHorizontalTiles]);
    subtile = &(tile->subtiles[y % WM_TILE_HEIGHT / WM_SUBTILE_SIZE][x % WM_TILE_WIDTH / WM_SUBTILE_SIZE]);
    *statePtr = subtile->state;

    return 0;
}

// Load tile art if needed.
//
// 0x4C37EC
static int wmTileGrabArt(int tileIdx)
{
    TileInfo* tile = &(wmTileInfoList[tileIdx]);
    if (tile->data != nullptr) {
        return 0;
    }

    tile->data = artLockFrameData(tile->fid, 0, 0, &(tile->handle));
    if (tile->data != nullptr) {
        return 0;
    }

    wmInterfaceExit();

    return -1;
}

// 0x4C3830
static int wmInterfaceRefresh()
{
    if (wmInterfaceWasInitialized != 1) {
        return 0;
    }

    int v17 = wmWorldOffsetX % WM_TILE_WIDTH;
    int v18 = wmWorldOffsetY % WM_TILE_HEIGHT;
    int v20 = WM_TILE_HEIGHT - v18;
    int v21 = WM_TILE_WIDTH * v18;
    int v19 = WM_TILE_WIDTH - v17;

    // Render tiles.
    int y = 0;
    int x = 0;
    int v0 = wmWorldOffsetY / WM_TILE_HEIGHT * wmNumHorizontalTiles + wmWorldOffsetX / WM_TILE_WIDTH % wmNumHorizontalTiles;
    while (y < WM_VIEW_HEIGHT) {
        x = 0;
        int v23 = 0;
        int height;
        while (x < WM_VIEW_WIDTH) {
            if (wmTileGrabArt(v0) == -1) {
                return -1;
            }

            int width = WM_TILE_WIDTH;

            int srcX = 0;
            if (x == 0) {
                srcX = v17;
                width = v19;
            }

            if (width + x > WM_VIEW_WIDTH) {
                width = WM_VIEW_WIDTH - x;
            }

            height = WM_TILE_HEIGHT;
            if (y == 0) {
                height = v20;
                srcX += v21;
            }

            if (height + y > WM_VIEW_HEIGHT) {
                height = WM_VIEW_HEIGHT - y;
            }

            TileInfo* tileInfo = &(wmTileInfoList[v0]);
            blitBufferToBuffer(tileInfo->data + srcX,
                width,
                height,
                WM_TILE_WIDTH,
                wmBkWinBuf + WM_WINDOW_WIDTH * (y + WM_VIEW_Y) + WM_VIEW_X + x,
                WM_WINDOW_WIDTH);
            v0++;

            x += width;
            v23++;
        }

        v0 += wmNumHorizontalTiles - v23;
        y += height;
    }

    // Render cities.
    for (int index = 0; index < wmMaxAreaNum; index++) {
        CityInfo* cityInfo = &(wmAreaInfoList[index]);
        if (cityInfo->state != CITY_STATE_UNKNOWN) {
            CitySizeDescription* citySizeDescription = &(wmSphereData[cityInfo->size]);
            int cityX = cityInfo->x - wmWorldOffsetX;
            int cityY = cityInfo->y - wmWorldOffsetY;
            if (cityX >= 0 && cityX <= 472 - citySizeDescription->frmImage.getWidth()
                && cityY >= 0 && cityY <= 465 - citySizeDescription->frmImage.getHeight()) {
                wmInterfaceDrawCircleOverlay(cityInfo, citySizeDescription, wmBkWinBuf, cityX, cityY);
            }
        }
    }

    // Hide unknown subtiles, dim unvisited.
    int v25 = wmWorldOffsetX / WM_TILE_WIDTH % wmNumHorizontalTiles + wmWorldOffsetY / WM_TILE_HEIGHT * wmNumHorizontalTiles;
    int v30 = 0;
    while (v30 < WM_VIEW_HEIGHT) {
        int v24 = 0;
        int v33 = 0;
        int v29;
        while (v33 < WM_VIEW_WIDTH) {
            int v31 = WM_TILE_WIDTH;
            if (v33 == 0) {
                v31 = WM_TILE_WIDTH - v17;
            }

            if (v33 + v31 > WM_VIEW_WIDTH) {
                v31 = WM_VIEW_WIDTH - v33;
            }

            v29 = WM_TILE_HEIGHT;
            if (v30 == 0) {
                v29 -= v18;
            }

            if (v30 + v29 > WM_VIEW_HEIGHT) {
                v29 = WM_VIEW_HEIGHT - v30;
            }

            int v32;
            if (v30 != 0) {
                v32 = WM_VIEW_Y;
            } else {
                v32 = WM_VIEW_Y - v18;
            }

            int v13 = 0;
            int v34 = v30 + v32;

            for (int row = 0; row < SUBTILE_GRID_HEIGHT; row++) {
                int v35;
                if (v33 != 0) {
                    v35 = WM_VIEW_X;
                } else {
                    v35 = WM_VIEW_X - v17;
                }

                int v15 = v33 + v35;
                for (int column = 0; column < SUBTILE_GRID_WIDTH; column++) {
                    TileInfo* tileInfo = &(wmTileInfoList[v25]);
                    wmInterfaceDrawSubTileList(tileInfo, column, row, v15, v34, 1);

                    v15 += WM_SUBTILE_SIZE;
                    v35 += WM_SUBTILE_SIZE;
                }

                v32 += WM_SUBTILE_SIZE;
                v34 += WM_SUBTILE_SIZE;
            }

            v25++;
            v24++;
            v33 += v31;
        }

        v25 += wmNumHorizontalTiles - v24;
        v30 += v29;
    }

    wmDrawCursorStopped();

    wmRefreshInterfaceOverlay(true);

    return 0;
}

// 0x4C3C9C
static void wmInterfaceRefreshDate(bool shouldRefreshWindow)
{
    int month;
    int day;
    int year;
    gameTimeGetDate(&month, &day, &year);

    month--;

    unsigned char* dest = wmBkWinBuf;

    int numbersFrmWidth = wmGenData.numbersFrmImage.getWidth();
    int numbersFrmHeight = wmGenData.numbersFrmImage.getHeight();
    unsigned char* numbersFrmData = wmGenData.numbersFrmImage.getData();

    dest += WM_WINDOW_WIDTH * 12 + 487;
    blitBufferToBuffer(numbersFrmData + 9 * (day / 10), 9, numbersFrmHeight, numbersFrmWidth, dest, WM_WINDOW_WIDTH);
    blitBufferToBuffer(numbersFrmData + 9 * (day % 10), 9, numbersFrmHeight, numbersFrmWidth, dest + 9, WM_WINDOW_WIDTH);

    int monthsFrmWidth = wmGenData.monthsFrmImage.getWidth();
    unsigned char* monthsFrmData = wmGenData.monthsFrmImage.getData();
    blitBufferToBuffer(monthsFrmData + monthsFrmWidth * 15 * month, 29, 14, 29, dest + WM_WINDOW_WIDTH + 26, WM_WINDOW_WIDTH);

    dest += 98;
    for (int index = 0; index < 4; index++) {
        dest -= 9;
        blitBufferToBuffer(numbersFrmData + 9 * (year % 10), 9, numbersFrmHeight, numbersFrmWidth, dest, WM_WINDOW_WIDTH);
        year /= 10;
    }

    int gameTimeHour = gameTimeGetHour();
    dest += 72;
    for (int index = 0; index < 4; index++) {
        blitBufferToBuffer(numbersFrmData + 9 * (gameTimeHour % 10), 9, numbersFrmHeight, numbersFrmWidth, dest, WM_WINDOW_WIDTH);
        dest -= 9;
        gameTimeHour /= 10;
    }

    if (shouldRefreshWindow) {
        Rect rect;
        rect.left = 487;
        rect.top = 12;
        rect.bottom = numbersFrmHeight + 12;
        rect.right = 630;
        windowRefreshRect(wmBkWin, &rect);
    }
}

// 0x4C3F00
static int wmMatchWorldPosToArea(int x, int y, int* areaIdxPtr)
{
    int v3 = y + WM_VIEW_Y;
    int v4 = x + WM_VIEW_X;

    int index;
    for (index = 0; index < wmMaxAreaNum; index++) {
        CityInfo* city = &(wmAreaInfoList[index]);
        if (city->state) {
            if (v4 >= city->x && v3 >= city->y) {
                CitySizeDescription* citySizeDescription = &(wmSphereData[city->size]);
                if (v4 <= city->x + citySizeDescription->frmImage.getWidth() && v3 <= city->y + citySizeDescription->frmImage.getHeight()) {
                    break;
                }
            }
        }
    }

    if (index == wmMaxAreaNum) {
        *areaIdxPtr = -1;
    } else {
        *areaIdxPtr = index;
    }

    return 0;
}

// 0x4C3FA8
static int wmInterfaceDrawCircleOverlay(CityInfo* city, CitySizeDescription* citySizeDescription, unsigned char* dest, int x, int y)
{
    _dark_translucent_trans_buf_to_buf(citySizeDescription->frmImage.getData(),
        citySizeDescription->frmImage.getWidth(),
        citySizeDescription->frmImage.getHeight(),
        citySizeDescription->frmImage.getWidth(),
        dest,
        x,
        y,
        WM_WINDOW_WIDTH,
        0x10000,
        circleBlendTable,
        _commonGrayTable);

    // CE: Slightly increase whitespace between cirle and city name.
    int nameY = y + citySizeDescription->frmImage.getHeight() + 3;
    int maxY = 464 - fontGetLineHeight();
    if (nameY < maxY) {
        MessageListItem messageListItem;
        char name[40];
        if (wmAreaIsKnown(city->areaId)) {
            // NOTE: Uninline.
            wmGetAreaName(city, name);
        } else {
            strncpy(name, getmsg(&wmMsgFile, &messageListItem, 1004), 40);
        }

        int width = fontGetStringWidth(name);
        fontDrawText(dest + WM_WINDOW_WIDTH * nameY + x + citySizeDescription->frmImage.getWidth() / 2 - width / 2,
            name,
            width,
            WM_WINDOW_WIDTH,
            _colorTable[992] | FONT_SHADOW);
    }

    return 0;
}

// Helper function that dims specified rectangle in given buffer. It's used to
// slightly darken subtile which is known, but not visited.
//
// 0x4C40A8
static void wmInterfaceDrawSubTileRectFogged(unsigned char* dest, int width, int height, int pitch)
{
    int skipY = pitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char color = *dest;
            *dest++ = intensityColorTable[color][75];
        }
        dest += skipY;
    }
}

// 0x4C40E4
static int wmInterfaceDrawSubTileList(TileInfo* tileInfo, int column, int row, int x, int y, int a6)
{
    SubtileInfo* subtileInfo = &(tileInfo->subtiles[row][column]);

    int destY = y;
    int destX = x;

    int height = WM_SUBTILE_SIZE;
    if (y < WM_VIEW_Y) {
        if (y < 0) {
            height = y + 29;
        } else {
            height = WM_SUBTILE_SIZE - (WM_VIEW_Y - y);
        }
        destY = WM_VIEW_Y;
    }

    if (height + y > 464) {
        height -= height + y - 464;
    }

    int width = WM_SUBTILE_SIZE * a6;
    if (x < WM_VIEW_X) {
        destX = WM_VIEW_X;
        width -= WM_VIEW_X - x;
    }

    if (width + x > 472) {
        width -= width + x - 472;
    }

    if (width > 0 && height > 0) {
        unsigned char* dest = wmBkWinBuf + WM_WINDOW_WIDTH * destY + destX;
        switch (subtileInfo->state) {
        case SUBTILE_STATE_UNKNOWN:
            bufferFill(dest, width, height, WM_WINDOW_WIDTH, _colorTable[0]);
            break;
        case SUBTILE_STATE_KNOWN:
            wmInterfaceDrawSubTileRectFogged(dest, width, height, WM_WINDOW_WIDTH);
            break;
        }
    }

    return 0;
}

// 0x4C41EC
static int wmDrawCursorStopped()
{
    unsigned char* src;
    int width;
    int height;

    if (wmGenData.walkDestinationX >= 1 || wmGenData.walkDestinationY >= 1) {

        if (wmGenData.encounterIconIsVisible) {
            src = wmGenData.encounterCursorFrmImages[wmGenData.encounterCursorId].getData();
            width = wmGenData.encounterCursorFrmImages[wmGenData.encounterCursorId].getWidth();
            height = wmGenData.encounterCursorFrmImages[wmGenData.encounterCursorId].getHeight();
        } else {
            src = wmGenData.locationMarkerFrmImage.getData();
            width = wmGenData.locationMarkerFrmImage.getWidth();
            height = wmGenData.locationMarkerFrmImage.getHeight();
        }

        if (wmGenData.worldPosX >= wmWorldOffsetX && wmGenData.worldPosX < wmWorldOffsetX + WM_VIEW_WIDTH
            && wmGenData.worldPosY >= wmWorldOffsetY && wmGenData.worldPosY < wmWorldOffsetY + WM_VIEW_HEIGHT) {
            blitBufferToBufferTrans(src, width, height, width, wmBkWinBuf + WM_WINDOW_WIDTH * (WM_VIEW_Y - wmWorldOffsetY + wmGenData.worldPosY - height / 2) + WM_VIEW_X - wmWorldOffsetX + wmGenData.worldPosX - width / 2, WM_WINDOW_WIDTH);
        }

        if (wmGenData.walkDestinationX >= wmWorldOffsetX && wmGenData.walkDestinationX < wmWorldOffsetX + WM_VIEW_WIDTH
            && wmGenData.walkDestinationY >= wmWorldOffsetY && wmGenData.walkDestinationY < wmWorldOffsetY + WM_VIEW_HEIGHT) {
            blitBufferToBufferTrans(wmGenData.destinationMarkerFrmImage.getData(),
                wmGenData.destinationMarkerFrmImage.getWidth(),
                wmGenData.destinationMarkerFrmImage.getHeight(),
                wmGenData.destinationMarkerFrmImage.getWidth(),
                wmBkWinBuf + WM_WINDOW_WIDTH * (WM_VIEW_Y - wmWorldOffsetY + wmGenData.walkDestinationY - wmGenData.destinationMarkerFrmImage.getHeight() / 2) + WM_VIEW_X - wmWorldOffsetX + wmGenData.walkDestinationX - wmGenData.destinationMarkerFrmImage.getWidth() / 2,
                WM_WINDOW_WIDTH);
        }
    } else {
        if (wmGenData.encounterIconIsVisible) {
            src = wmGenData.encounterCursorFrmImages[wmGenData.encounterCursorId].getData();
            width = wmGenData.encounterCursorFrmImages[wmGenData.encounterCursorId].getWidth();
            height = wmGenData.encounterCursorFrmImages[wmGenData.encounterCursorId].getHeight();
        } else {
            src = wmGenData.mousePressed ? wmGenData.hotspotPressedFrmImage.getData() : wmGenData.hotspotNormalFrmImage.getData();
            width = wmGenData.hotspotNormalFrmImage.getWidth();
            height = wmGenData.hotspotNormalFrmImage.getHeight();
        }

        if (wmGenData.worldPosX >= wmWorldOffsetX && wmGenData.worldPosX < wmWorldOffsetX + WM_VIEW_WIDTH
            && wmGenData.worldPosY >= wmWorldOffsetY && wmGenData.worldPosY < wmWorldOffsetY + WM_VIEW_HEIGHT) {
            blitBufferToBufferTrans(src, width, height, width, wmBkWinBuf + WM_WINDOW_WIDTH * (WM_VIEW_Y - wmWorldOffsetY + wmGenData.worldPosY - height / 2) + WM_VIEW_X - wmWorldOffsetX + wmGenData.worldPosX - width / 2, WM_WINDOW_WIDTH);
        }
    }

    return 0;
}

// 0x4C4490
static bool wmCursorIsVisible()
{
    return wmGenData.worldPosX >= wmWorldOffsetX
        && wmGenData.worldPosY >= wmWorldOffsetY
        && wmGenData.worldPosX < wmWorldOffsetX + WM_VIEW_WIDTH
        && wmGenData.worldPosY < wmWorldOffsetY + WM_VIEW_HEIGHT;
}

// NOTE: Inlined.
//
// 0x4C44D8
static int wmGetAreaName(CityInfo* city, char* name)
{
    MessageListItem messageListItem;

    getmsg(&gMapMessageList, &messageListItem, city->areaId + 1500);
    strncpy(name, messageListItem.text, 40);

    return 0;
}

// Copy city short name.
//
// 0x4C450C
int wmGetAreaIdxName(int areaIdx, char* name)
{
    MessageListItem messageListItem;

    getmsg(&gMapMessageList, &messageListItem, 1500 + areaIdx);
    strncpy(name, messageListItem.text, 40);

    return 0;
}

// Returns true if world area is known.
//
// 0x4C453C
bool wmAreaIsKnown(int areaIdx)
{
    if (!cityIsValid(areaIdx)) {
        return false;
    }

    CityInfo* city = &(wmAreaInfoList[areaIdx]);
    if (city->visitedState) {
        if (city->state == CITY_STATE_KNOWN) {
            return true;
        }
    }

    return false;
}

// 0x4C457C
int wmAreaVisitedState(int areaIdx)
{
    if (!cityIsValid(areaIdx)) {
        return 0;
    }

    CityInfo* city = &(wmAreaInfoList[areaIdx]);
    if (city->visitedState && city->state == CITY_STATE_KNOWN) {
        return city->visitedState;
    }

    return 0;
}

// 0x4C45BC
bool wmMapIsKnown(int mapIdx)
{
    int areaIdx;
    if (wmMatchAreaFromMap(mapIdx, &areaIdx) != 0) {
        return false;
    }

    int entranceIdx;
    if (wmMatchEntranceFromMap(areaIdx, mapIdx, &entranceIdx) != 0) {
        return false;
    }

    CityInfo* city = &(wmAreaInfoList[areaIdx]);
    EntranceInfo* entrance = &(city->entrances[entranceIdx]);

    if (entrance->state != 1) {
        return false;
    }

    return true;
}

// 0x4C4624
int wmAreaMarkVisited(int areaIdx)
{
    return wmAreaMarkVisitedState(areaIdx, CITY_STATE_VISITED);
}

// 0x4C4634
bool wmAreaMarkVisitedState(int areaIdx, int state)
{
    if (!cityIsValid(areaIdx)) {
        return false;
    }

    CityInfo* city = &(wmAreaInfoList[areaIdx]);
    int oldVisitedState = city->visitedState;
    if (city->state == CITY_STATE_KNOWN && state != 0) {
        wmMarkSubTileRadiusVisited(city->x, city->y);
    }

    city->visitedState = state;

    SubtileInfo* subtile;
    if (wmFindCurSubTileFromPos(city->x, city->y, &subtile) == -1) {
        return false;
    }

    if (state == 1) {
        subtile->state = SUBTILE_STATE_KNOWN;
    } else if (state == 2 && oldVisitedState == 0) {
        city->visitedState = 1;
    }

    return true;
}

// 0x4C46CC
bool wmAreaSetVisibleState(int areaIdx, int state, bool force)
{
    if (!cityIsValid(areaIdx)) {
        return false;
    }

    CityInfo* city = &(wmAreaInfoList[areaIdx]);
    if (city->lockState != LOCK_STATE_LOCKED || force) {
        city->state = state;
        return true;
    }

    return false;
}

// 0x4C4710
int wmAreaSetWorldPos(int areaIdx, int x, int y)
{
    if (!cityIsValid(areaIdx)) {
        return -1;
    }

    if (x < 0 || x >= WM_TILE_WIDTH * wmNumHorizontalTiles) {
        return -1;
    }

    if (y < 0 || y >= WM_TILE_HEIGHT * (wmMaxTileNum / wmNumHorizontalTiles)) {
        return -1;
    }

    CityInfo* city = &(wmAreaInfoList[areaIdx]);
    city->x = x;
    city->y = y;

    return 0;
}

// Returns current town x/y.
//
// 0x4C47A4
int wmGetPartyWorldPos(int* xPtr, int* yPtr)
{
    if (xPtr != nullptr) {
        *xPtr = wmGenData.worldPosX;
    }

    if (yPtr != nullptr) {
        *yPtr = wmGenData.worldPosY;
    }

    return 0;
}

// Returns current town.
//
// 0x4C47C0
int wmGetPartyCurArea(int* areaIdxPtr)
{
    if (areaIdxPtr != nullptr) {
        *areaIdxPtr = wmGenData.currentAreaId;
        return 0;
    }

    return -1;
}

// 0x4C47D8
static void wmMarkAllSubTiles(int state)
{
    for (int tileIndex = 0; tileIndex < wmMaxTileNum; tileIndex++) {
        TileInfo* tile = &(wmTileInfoList[tileIndex]);
        for (int column = 0; column < SUBTILE_GRID_HEIGHT; column++) {
            for (int row = 0; row < SUBTILE_GRID_WIDTH; row++) {
                SubtileInfo* subtile = &(tile->subtiles[column][row]);
                subtile->state = state;
            }
        }
    }
}

// 0x4C4850
void wmTownMap()
{
    wmWorldMapFunc(1);
}

// 0x4C485C
static int wmTownMapFunc(int* mapIdxPtr)
{
    *mapIdxPtr = -1;

    if (wmTownMapInit() == -1) {
        wmTownMapExit();
        return -1;
    }

    if (wmGenData.currentAreaId == -1) {
        return -1;
    }

    CityInfo* city = &(wmAreaInfoList[wmGenData.currentAreaId]);

    for (;;) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            showQuitConfirmationDialog();
        }

        if (_game_user_wants_to_quit) {
            break;
        }

        if (keyCode != -1) {
            if (keyCode == KEY_ESCAPE) {
                break;
            }

            if (keyCode >= KEY_1 && keyCode < KEY_1 + city->entrancesLength) {
                EntranceInfo* entrance = &(city->entrances[keyCode - KEY_1]);

                // SFALL: Prevent using number keys to enter unvisited areas on
                // a town map.
                if (gTownMapHotkeysFix) {
                    if (entrance->state == 0 || entrance->x == -1 || entrance->y == -1) {
                        continue;
                    }
                }

                *mapIdxPtr = entrance->map;

                mapSetEnteringLocation(entrance->elevation, entrance->tile, entrance->rotation);

                break;
            }

            if (keyCode >= KEY_CTRL_F1 && keyCode <= KEY_CTRL_F7) {
                int quickDestinationIndex = wmGenData.tabsOffsetY / 27 + keyCode - KEY_CTRL_F1;
                if (quickDestinationIndex < wmLabelCount) {
                    int areaIdx = wmLabelList[quickDestinationIndex];
                    CityInfo* city = &(wmAreaInfoList[areaIdx]);
                    if (!wmAreaIsKnown(city->areaId)) {
                        break;
                    }

                    if (areaIdx != wmGenData.currentAreaId) {
                        // CE: Fix incorrect destination positioning. See
                        // `wmWorldMapFunc` for explanation.
                        CitySizeDescription* citySizeDescription = &(wmSphereData[city->size]);
                        int destX = city->x + citySizeDescription->frmImage.getWidth() / 2 - WM_VIEW_X;
                        int destY = city->y + citySizeDescription->frmImage.getHeight() / 2 - WM_VIEW_Y;
                        wmPartyInitWalking(destX, destY);

                        wmGenData.mousePressed = false;

                        break;
                    }
                }
            } else {
                if (keyCode == KEY_CTRL_ARROW_UP) {
                    wmInterfaceScrollTabsStart(-27);
                } else if (keyCode == KEY_CTRL_ARROW_DOWN) {
                    wmInterfaceScrollTabsStart(27);
                } else if (keyCode == 2069) {
                    if (wmTownMapRefresh() == -1) {
                        return -1;
                    }
                }

                if (keyCode == KEY_UPPERCASE_T || keyCode == KEY_LOWERCASE_T || keyCode == KEY_UPPERCASE_W || keyCode == KEY_LOWERCASE_W) {
                    keyCode = KEY_ESCAPE;
                }

                if (keyCode == KEY_ESCAPE) {
                    break;
                }
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (wmTownMapExit() == -1) {
        return -1;
    }

    return 0;
}

// 0x4C4A6C
static int wmTownMapInit()
{
    wmTownMapCurArea = wmGenData.currentAreaId;

    CityInfo* city = &(wmAreaInfoList[wmGenData.currentAreaId]);

    if (!_townFrmImage.lock(city->mapFid)) {
        return -1;
    }

    for (int index = 0; index < city->entrancesLength; index++) {
        wmTownMapButtonId[index] = -1;
    }

    for (int index = 0; index < city->entrancesLength; index++) {
        EntranceInfo* entrance = &(city->entrances[index]);
        if (entrance->state == 0) {
            continue;
        }

        if (entrance->x == -1 || entrance->y == -1) {
            continue;
        }

        wmTownMapButtonId[index] = buttonCreate(wmBkWin,
            entrance->x,
            entrance->y,
            wmGenData.hotspotNormalFrmImage.getWidth(),
            wmGenData.hotspotNormalFrmImage.getHeight(),
            -1,
            2069,
            -1,
            KEY_1 + index,
            wmGenData.hotspotNormalFrmImage.getData(),
            wmGenData.hotspotPressedFrmImage.getData(),
            nullptr,
            BUTTON_FLAG_TRANSPARENT);

        if (wmTownMapButtonId[index] == -1) {
            return -1;
        }
    }

    tickersRemove(wmMouseBkProc);

    if (wmTownMapRefresh() == -1) {
        return -1;
    }

    return 0;
}

// 0x4C4BD0
static int wmTownMapRefresh()
{
    blitBufferToBuffer(_townFrmImage.getData(),
        _townFrmImage.getWidth(),
        _townFrmImage.getHeight(),
        _townFrmImage.getWidth(),
        wmBkWinBuf + WM_WINDOW_WIDTH * WM_VIEW_Y + WM_VIEW_X,
        WM_WINDOW_WIDTH);

    wmRefreshInterfaceOverlay(false);

    CityInfo* city = &(wmAreaInfoList[wmGenData.currentAreaId]);

    for (int index = 0; index < city->entrancesLength; index++) {
        EntranceInfo* entrance = &(city->entrances[index]);
        if (entrance->state == 0) {
            continue;
        }

        if (entrance->x == -1 || entrance->y == -1) {
            continue;
        }

        MessageListItem messageListItem;
        messageListItem.num = 200 + 10 * wmTownMapCurArea + index;
        if (messageListGetItem(&wmMsgFile, &messageListItem)) {
            if (messageListItem.text != nullptr) {
                int width = fontGetStringWidth(messageListItem.text);
                // CE: Slightly increase whitespace between marker and entrance name.
                windowDrawText(wmBkWin,
                    messageListItem.text,
                    width,
                    wmGenData.hotspotNormalFrmImage.getWidth() / 2 + entrance->x - width / 2,
                    wmGenData.hotspotNormalFrmImage.getHeight() + entrance->y + 4,
                    _colorTable[992] | 0x2000000 | FONT_SHADOW);
            }
        }
    }

    windowRefresh(wmBkWin);

    return 0;
}

// 0x4C4D00
static int wmTownMapExit()
{
    _townFrmImage.unlock();

    if (wmTownMapCurArea != -1) {
        CityInfo* city = &(wmAreaInfoList[wmTownMapCurArea]);
        for (int index = 0; index < city->entrancesLength; index++) {
            if (wmTownMapButtonId[index] != -1) {
                buttonDestroy(wmTownMapButtonId[index]);
                wmTownMapButtonId[index] = -1;
            }
        }
    }

    if (wmInterfaceRefresh() == -1) {
        return -1;
    }

    tickersAdd(wmMouseBkProc);

    return 0;
}

// 0x4C4DA4
int wmCarUseGas(int amount)
{
    if (gameGetGlobalVar(GVAR_NEW_RENO_SUPER_CAR) != 0) {
        amount -= amount * 90 / 100;
    }

    if (gameGetGlobalVar(GVAR_NEW_RENO_CAR_UPGRADE) != 0) {
        amount -= amount * 10 / 100;
    }

    if (gameGetGlobalVar(GVAR_CAR_UPGRADE_FUEL_CELL_REGULATOR) != 0) {
        amount /= 2;
    }

    wmGenData.carFuel -= amount;

    if (wmGenData.carFuel < 0) {
        wmGenData.carFuel = 0;
    }

    return 0;
}

// Returns amount of fuel that does not fit into tank.
//
// 0x4C4E34
int wmCarFillGas(int amount)
{
    if ((amount + wmGenData.carFuel) <= CAR_FUEL_MAX) {
        wmGenData.carFuel += amount;
        return 0;
    }

    int remaining = CAR_FUEL_MAX - wmGenData.carFuel;

    wmGenData.carFuel = CAR_FUEL_MAX;

    return remaining;
}

// 0x4C4E74
int wmCarGasAmount()
{
    return wmGenData.carFuel;
}

// 0x4C4E7C
bool wmCarIsOutOfGas()
{
    return wmGenData.carFuel <= 0;
}

// 0x4C4E8C
int wmCarCurrentArea()
{
    return wmGenData.currentCarAreaId;
}

// 0x4C4E94
int wmCarGiveToParty()
{
    MessageListItem messageListItem;
    memcpy(&messageListItem, &gWorldmapMessageListItem, sizeof(MessageListItem));

    if (wmGenData.carFuel <= 0) {
        // The car is out of power.
        char* msg = getmsg(&wmMsgFile, &messageListItem, 1502);
        displayMonitorAddMessage(msg);
        return -1;
    }

    wmGenData.isInCar = true;

    MapTransition transition;
    memset(&transition, 0, sizeof(transition));

    transition.map = -2;
    mapSetTransition(&transition);

    CityInfo* city = &(wmAreaInfoList[CITY_CAR_OUT_OF_GAS]);
    city->state = CITY_STATE_UNKNOWN;
    city->visitedState = 0;

    return 0;
}

// 0x4C4F28
int wmSfxMaxCount()
{
    int mapIdx = mapGetCurrentMap();
    if (mapIdx < 0 || mapIdx >= wmMaxMapNum) {
        return -1;
    }

    MapInfo* map = &(wmMapInfoList[mapIdx]);
    return map->ambientSoundEffectsLength;
}

// 0x4C4F5C
int wmSfxRollNextIdx()
{
    int mapIdx = mapGetCurrentMap();
    if (mapIdx < 0 || mapIdx >= wmMaxMapNum) {
        return -1;
    }

    MapInfo* map = &(wmMapInfoList[mapIdx]);

    int totalChances = 0;
    for (int index = 0; index < map->ambientSoundEffectsLength; index++) {
        MapAmbientSoundEffectInfo* sfx = &(map->ambientSoundEffects[index]);
        totalChances += sfx->chance;
    }

    int chance = randomBetween(0, totalChances);
    for (int index = 0; index < map->ambientSoundEffectsLength; index++) {
        MapAmbientSoundEffectInfo* sfx = &(map->ambientSoundEffects[index]);
        if (chance >= sfx->chance) {
            chance -= sfx->chance;
            continue;
        }

        return index;
    }

    return -1;
}

// 0x4C5004
int wmSfxIdxName(int sfxIdx, char** namePtr)
{
    if (namePtr == nullptr) {
        return -1;
    }

    *namePtr = nullptr;

    int mapIdx = mapGetCurrentMap();
    if (mapIdx < 0 || mapIdx >= wmMaxMapNum) {
        return -1;
    }

    MapInfo* map = &(wmMapInfoList[mapIdx]);
    if (sfxIdx < 0 || sfxIdx >= map->ambientSoundEffectsLength) {
        return -1;
    }

    MapAmbientSoundEffectInfo* ambientSoundEffectInfo = &(map->ambientSoundEffects[sfxIdx]);
    *namePtr = ambientSoundEffectInfo->name;

    // Remap bird sounds for night.
    int remapped = 0;
    if (strcmp(ambientSoundEffectInfo->name, "brdchir1") == 0) {
        remapped = 1;
    } else if (strcmp(ambientSoundEffectInfo->name, "brdchirp") == 0) {
        remapped = 2;
    }

    if (remapped != 0) {
        int dayPart;

        int gameTimeHour = gameTimeGetHour();
        if (gameTimeHour <= 600 || gameTimeHour >= 1800) {
            dayPart = DAY_PART_NIGHT;
        } else if (gameTimeHour >= 1200) {
            dayPart = DAY_PART_AFTERNOON;
        } else {
            dayPart = DAY_PART_MORNING;
        }

        if (dayPart == DAY_PART_NIGHT) {
            *namePtr = wmRemapSfxList[remapped - 1];
        }
    }

    return 0;
}

// 0x4C50F4
static int wmRefreshInterfaceOverlay(bool shouldRefreshWindow)
{
    blitBufferToBufferTrans(_backgroundFrmImage.getData(),
        _backgroundFrmImage.getWidth(),
        _backgroundFrmImage.getHeight(),
        _backgroundFrmImage.getWidth(),
        wmBkWinBuf,
        WM_WINDOW_WIDTH);

    wmRefreshTabs();

    // NOTE: Uninline.
    wmInterfaceDialSyncTime(false);

    wmRefreshInterfaceDial(false);

    if (wmGenData.isInCar) {
        unsigned char* data = artGetFrameData(wmGenData.carImageFrm, wmGenData.carImageCurrentFrameIndex, 0);
        if (data == nullptr) {
            return -1;
        }

        blitBufferToBuffer(data,
            wmGenData.carImageFrmWidth,
            wmGenData.carImageFrmHeight,
            wmGenData.carImageFrmWidth,
            wmBkWinBuf + WM_WINDOW_WIDTH * WM_WINDOW_CAR_Y + WM_WINDOW_CAR_X,
            WM_WINDOW_WIDTH);

        blitBufferToBufferTrans(wmGenData.carOverlayFrmImage.getData(),
            wmGenData.carOverlayFrmImage.getWidth(),
            wmGenData.carOverlayFrmImage.getHeight(),
            wmGenData.carOverlayFrmImage.getWidth(),
            wmBkWinBuf + WM_WINDOW_WIDTH * WM_WINDOW_CAR_OVERLAY_Y + WM_WINDOW_CAR_OVERLAY_X,
            WM_WINDOW_WIDTH);

        wmInterfaceRefreshCarFuel();
    } else {
        blitBufferToBufferTrans(wmGenData.globeOverlayFrmImage.getData(),
            wmGenData.globeOverlayFrmImage.getWidth(),
            wmGenData.globeOverlayFrmImage.getHeight(),
            wmGenData.globeOverlayFrmImage.getWidth(),
            wmBkWinBuf + WM_WINDOW_WIDTH * WM_WINDOW_GLOBE_OVERLAY_Y + WM_WINDOW_GLOBE_OVERLAY_X,
            WM_WINDOW_WIDTH);
    }

    wmInterfaceRefreshDate(false);

    if (shouldRefreshWindow) {
        windowRefresh(wmBkWin);
    }

    return 0;
}

// 0x4C5244
static void wmInterfaceRefreshCarFuel()
{
    int ratio = (WM_WINDOW_CAR_FUEL_BAR_HEIGHT * wmGenData.carFuel) / CAR_FUEL_MAX;
    if ((ratio & 1) != 0) {
        ratio -= 1;
    }

    unsigned char* dest = wmBkWinBuf + WM_WINDOW_WIDTH * WM_WINDOW_CAR_FUEL_BAR_Y + WM_WINDOW_CAR_FUEL_BAR_X;

    for (int index = WM_WINDOW_CAR_FUEL_BAR_HEIGHT; index > ratio; index--) {
        *dest = 14;
        dest += 640;
    }

    while (ratio > 0) {
        *dest = 196;
        dest += WM_WINDOW_WIDTH;

        *dest = 14;
        dest += WM_WINDOW_WIDTH;

        ratio -= 2;
    }
}

// 0x4C52B0
static int wmRefreshTabs()
{
    unsigned char* v30;
    unsigned char* v0;
    int v31;
    CityInfo* city;
    int v10;
    unsigned char* v11;
    unsigned char* v12;
    int v32;
    unsigned char* v13;
    FrmImage labelFrm;

    // CE: Skip first empty tab (original code does this in the
    // `wmInterfaceInit`).
    unsigned char* src = wmGenData.tabsBackgroundFrmImage.getData() + wmGenData.tabsBackgroundFrmImage.getWidth() * 27;
    blitBufferToBufferTrans(src + wmGenData.tabsBackgroundFrmImage.getWidth() * wmGenData.tabsOffsetY + 9,
        119,
        178,
        wmGenData.tabsBackgroundFrmImage.getWidth(),
        wmBkWinBuf + WM_WINDOW_WIDTH * 135 + 501,
        WM_WINDOW_WIDTH);

    v30 = wmBkWinBuf + WM_WINDOW_WIDTH * 138 + 530;
    v0 = wmBkWinBuf + WM_WINDOW_WIDTH * 138 + 530 - WM_WINDOW_WIDTH * (wmGenData.tabsOffsetY % 27);
    v31 = wmGenData.tabsOffsetY / 27;

    if (v31 < wmLabelCount) {
        city = &(wmAreaInfoList[wmLabelList[v31]]);
        if (city->labelFid != -1) {
            if (!labelFrm.lock(city->labelFid)) {
                return -1;
            }

            v10 = labelFrm.getHeight() - wmGenData.tabsOffsetY % 27;
            v11 = labelFrm.getData() + labelFrm.getWidth() * (wmGenData.tabsOffsetY % 27);

            v12 = v0;
            if (v0 < v30 - WM_WINDOW_WIDTH) {
                v12 = v30 - WM_WINDOW_WIDTH;
            }

            blitBufferToBuffer(v11,
                labelFrm.getWidth(),
                v10,
                labelFrm.getWidth(),
                v12,
                WM_WINDOW_WIDTH);

            labelFrm.unlock();
        }
    }

    v13 = v0 + WM_WINDOW_WIDTH * 27;
    v32 = v31 + 6;

    for (int v14 = v31 + 1; v14 < v32; v14++) {
        if (v14 < wmLabelCount) {
            city = &(wmAreaInfoList[wmLabelList[v14]]);
            if (city->labelFid != -1) {
                if (!labelFrm.lock(city->labelFid)) {
                    return -1;
                }

                blitBufferToBuffer(labelFrm.getData(),
                    labelFrm.getWidth(),
                    labelFrm.getHeight(),
                    labelFrm.getWidth(),
                    v13,
                    WM_WINDOW_WIDTH);

                labelFrm.unlock();
            }
        }
        v13 += WM_WINDOW_WIDTH * 27;
    }

    if (v31 + 6 < wmLabelCount) {
        city = &(wmAreaInfoList[wmLabelList[v31 + 6]]);
        if (city->labelFid != -1) {
            if (!labelFrm.lock(city->labelFid)) {
                return -1;
            }

            blitBufferToBuffer(labelFrm.getData(),
                labelFrm.getWidth(),
                labelFrm.getHeight() - 5,
                labelFrm.getWidth(),
                v13,
                WM_WINDOW_WIDTH);

            labelFrm.unlock();
        }
    }

    blitBufferToBufferTrans(wmGenData.tabsBorderFrmImage.getData(),
        119,
        178,
        119,
        wmBkWinBuf + WM_WINDOW_WIDTH * 135 + 501,
        WM_WINDOW_WIDTH);

    return 0;
}

// Creates array of cities available as quick destinations.
//
// 0x4C55D4
static int wmMakeTabsLabelList(int** quickDestinationsPtr, int* quickDestinationsLengthPtr)
{
    int* quickDestinations = *quickDestinationsPtr;

    // NOTE: Uninline.
    wmFreeTabsLabelList(quickDestinationsPtr, quickDestinationsLengthPtr);

    int capacity = 10;

    quickDestinations = (int*)internal_malloc(sizeof(*quickDestinations) * capacity);
    *quickDestinationsPtr = quickDestinations;

    if (quickDestinations == nullptr) {
        return -1;
    }

    int quickDestinationsLength = *quickDestinationsLengthPtr;
    for (int index = 0; index < wmMaxAreaNum; index++) {
        if (wmAreaIsKnown(index) && wmAreaInfoList[index].labelFid != -1) {
            quickDestinationsLength++;
            *quickDestinationsLengthPtr = quickDestinationsLength;

            if (capacity <= quickDestinationsLength) {
                capacity += 10;

                quickDestinations = (int*)internal_realloc(quickDestinations, sizeof(*quickDestinations) * capacity);
                if (quickDestinations == nullptr) {
                    return -1;
                }

                *quickDestinationsPtr = quickDestinations;
            }

            quickDestinations[quickDestinationsLength - 1] = index;
        }
    }

    qsort(quickDestinations, quickDestinationsLength, sizeof(*quickDestinations), wmTabsCompareNames);

    return 0;
}

// 0x4C56C8
static int wmTabsCompareNames(const void* a1, const void* a2)
{
    int index1 = *(int*)a1;
    int index2 = *(int*)a2;

    CityInfo* city1 = &(wmAreaInfoList[index1]);
    CityInfo* city2 = &(wmAreaInfoList[index2]);

    return compat_stricmp(city1->name, city2->name);
}

// NOTE: Inlined.
//
// 0x4C5710
static int wmFreeTabsLabelList(int** quickDestinationsListPtr, int* quickDestinationsLengthPtr)
{
    if (*quickDestinationsListPtr != nullptr) {
        internal_free(*quickDestinationsListPtr);
        *quickDestinationsListPtr = nullptr;
    }

    *quickDestinationsLengthPtr = 0;

    return 0;
}

// 0x4C5734
static void wmRefreshInterfaceDial(bool shouldRefreshWindow)
{
    unsigned char* data = artGetFrameData(wmGenData.dialFrm, wmGenData.dialFrmCurrentFrameIndex, 0);
    blitBufferToBufferTrans(data,
        wmGenData.dialFrmWidth,
        wmGenData.dialFrmHeight,
        wmGenData.dialFrmWidth,
        wmBkWinBuf + WM_WINDOW_WIDTH * WM_WINDOW_DIAL_Y + WM_WINDOW_DIAL_X,
        WM_WINDOW_WIDTH);

    if (shouldRefreshWindow) {
        Rect rect;
        rect.left = WM_WINDOW_DIAL_X;
        rect.top = WM_WINDOW_DIAL_Y - 1;
        rect.right = rect.left + wmGenData.dialFrmWidth;
        rect.bottom = rect.top + wmGenData.dialFrmHeight;
        windowRefreshRect(wmBkWin, &rect);
    }
}

// NOTE: Inlined.
//
// 0x4C57BC
static void wmInterfaceDialSyncTime(bool shouldRefreshWindow)
{
    int gameHour;
    int frame;

    gameHour = gameTimeGetHour();
    frame = (gameHour / 100 + 12) % artGetFrameCount(wmGenData.dialFrm);
    if (frame != wmGenData.dialFrmCurrentFrameIndex) {
        wmGenData.dialFrmCurrentFrameIndex = frame;
        wmRefreshInterfaceDial(shouldRefreshWindow);
    }
}

// 0x4C5804
static int wmAreaFindFirstValidMap(int* mapIdxPtr)
{
    *mapIdxPtr = -1;

    if (wmGenData.currentAreaId == -1) {
        return -1;
    }

    CityInfo* city = &(wmAreaInfoList[wmGenData.currentAreaId]);
    if (city->entrancesLength == 0) {
        return -1;
    }

    for (int index = 0; index < city->entrancesLength; index++) {
        EntranceInfo* entrance = &(city->entrances[index]);
        if (entrance->state != 0) {
            *mapIdxPtr = entrance->map;
            return 0;
        }
    }

    EntranceInfo* entrance = &(city->entrances[0]);
    entrance->state = 1;

    *mapIdxPtr = entrance->map;
    return 0;
}

// 0x4C58C0
int wmMapMusicStart()
{
    do {
        int mapIdx = mapGetCurrentMap();
        if (mapIdx == -1 || mapIdx >= wmMaxMapNum) {
            break;
        }

        MapInfo* map = &(wmMapInfoList[mapIdx]);
        if (strlen(map->music) == 0) {
            break;
        }

        if (_gsound_background_play_level_music(map->music, 12) == -1) {
            break;
        }

        return 0;
    } while (0);

    debugPrint("\nWorldMap Error: Couldn't start map Music!");

    return -1;
}

// 0x4C5928
int wmSetMapMusic(int mapIdx, const char* name)
{
    if (mapIdx == -1 || mapIdx >= wmMaxMapNum) {
        return -1;
    }

    if (name == nullptr) {
        return -1;
    }

    debugPrint("\nwmSetMapMusic: %d, %s", mapIdx, name);

    MapInfo* map = &(wmMapInfoList[mapIdx]);

    strncpy(map->music, name, 40);
    map->music[39] = '\0';

    if (mapGetCurrentMap() == mapIdx) {
        backgroundSoundDelete();
        wmMapMusicStart();
    }

    return 0;
}

// 0x4C59A4
int wmMatchAreaContainingMapIdx(int mapIdx, int* areaIdxPtr)
{
    *areaIdxPtr = 0;

    for (int areaIdx = 0; areaIdx < wmMaxAreaNum; areaIdx++) {
        CityInfo* cityInfo = &(wmAreaInfoList[areaIdx]);
        for (int entranceIdx = 0; entranceIdx < cityInfo->entrancesLength; entranceIdx++) {
            EntranceInfo* entranceInfo = &(cityInfo->entrances[entranceIdx]);
            if (entranceInfo->map == mapIdx) {
                *areaIdxPtr = areaIdx;
                return 0;
            }
        }
    }

    return -1;
}

// 0x4C5A1C
int wmTeleportToArea(int areaIdx)
{
    if (!cityIsValid(areaIdx)) {
        return -1;
    }

    wmGenData.currentAreaId = areaIdx;
    wmGenData.walkDestinationX = 0;
    wmGenData.walkDestinationY = 0;
    wmGenData.isWalking = false;

    CityInfo* city = &(wmAreaInfoList[areaIdx]);

    // SFALL: Fix for incorrect positioning after exiting small/medium
    // locations.
    // CE: See `wmWorldMapFunc` for explanation.
    CitySizeDescription* citySizeDescription = &(wmSphereData[city->size]);

    // CE: This function might be called outside |wmWorldmapFunc|, so it's
    // image might not be locked.
    bool wasLocked = citySizeDescription->frmImage.isLocked();
    if (!wasLocked) {
        citySizeDescription->frmImage.lock(citySizeDescription->fid);
    }

    wmGenData.worldPosX = city->x + citySizeDescription->frmImage.getWidth() / 2 - WM_VIEW_X;
    wmGenData.worldPosY = city->y + citySizeDescription->frmImage.getHeight() / 2 - WM_VIEW_Y;

    if (!wasLocked) {
        citySizeDescription->frmImage.unlock();
    }

    return 0;
}

void wmFadeOut()
{
    if (!wmFaded) {
        paletteFadeTo(gPaletteBlack);
        wmFaded = true;
    }
}

void wmFadeIn()
{
    if (wmFaded) {
        paletteFadeTo(_cmap);
        wmFaded = false;
    }
}

void wmFadeReset()
{
    wmFaded = false;
    paletteSetEntries(_cmap);
}

void wmBlinkRndEncounterIcon(bool special)
{
    wmGenData.encounterIconIsVisible = true;

    // CE: Original code cycles circled bright and non-circled dark icons.
    int dark;
    int bright;
    if (special) {
        dark = WORLD_MAP_ENCOUNTER_FRM_SPECIAL_DARK;
        bright = WORLD_MAP_ENCOUNTER_FRM_SPECIAL_BRIGHT;
    } else {
        dark = WORLD_MAP_ENCOUNTER_FRM_RANDOM_DARK;
        bright = WORLD_MAP_ENCOUNTER_FRM_RANDOM_BRIGHT;
    }

    for (int index = 0; index < 7; index++) {
        wmGenData.encounterCursorId = index % 2 == 0 ? dark : bright;

        if (wmInterfaceRefresh() == -1) {
            return;
        }

        renderPresent();
        inputBlockForTocks(200);
    }

    wmGenData.encounterIconIsVisible = false;
}

void wmSetPartyWorldPos(int x, int y)
{
    wmGenData.worldPosX = x;
    wmGenData.worldPosY = y;
}

void wmCarSetCurrentArea(int area)
{
    wmGenData.currentCarAreaId = area;
}

void wmForceEncounter(int map, unsigned int flags)
{
    if ((wmForceEncounterFlags & (1 << 31)) != 0) {
        return;
    }

    wmForceEncounterMapId = map;
    wmForceEncounterFlags = flags;

    // I don't quite understand the reason why locking needs one more flag.
    if ((wmForceEncounterFlags & ENCOUNTER_FLAG_LOCK) != 0) {
        wmForceEncounterFlags |= (1 << 31);
    } else {
        wmForceEncounterFlags &= ~(1 << 31);
    }
}

} // namespace fallout
