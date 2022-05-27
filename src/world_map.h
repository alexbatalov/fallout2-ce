#ifndef WORLD_MAP_H
#define WORLD_MAP_H

#include "art.h"
#include "config.h"
#include "db.h"
#include "map_defs.h"
#include "message.h"
#include "obj_types.h"

#define CITY_NAME_SIZE (40)
#define TILE_WALK_MASK_NAME_SIZE (40)
#define ENTRANCE_LIST_CAPACITY (10)

#define MAP_AMBIENT_SOUND_EFFECTS_CAPACITY (6)
#define MAP_STARTING_POINTS_CAPACITY (15)

#define SUBTILE_GRID_WIDTH (7)
#define SUBTILE_GRID_HEIGHT (6)

#define ENCOUNTER_ENTRY_SPECIAL (0x01)

#define ENCOUNTER_SUBINFO_DEAD (0x01)

#define CAR_FUEL_MAX (80000)

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

typedef enum MapFlags {
    MAP_SAVED = 0x01,
    MAP_DEAD_BODIES_AGE = 0x02,
    MAP_PIPBOY_ACTIVE = 0x04,
    MAP_CAN_REST_ELEVATION_0 = 0x08,
    MAP_CAN_REST_ELEVATION_1 = 0x10,
    MAP_CAN_REST_ELEVATION_2 = 0x20,
} MapFlags;

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

typedef enum Daytime {
    DAY_PART_MORNING,
    DAY_PART_AFTERNOON,
    DAY_PART_NIGHT,
    DAY_PART_COUNT,
} Daytime;

typedef enum CityState {
    CITY_STATE_UNKNOWN,
    CITY_STATE_KNOWN,
    CITY_STATE_VISITED,
    CITY_STATE_INVISIBLE = -66,
} CityState;

typedef enum SubtileState {
    SUBTILE_STATE_UNKNOWN,
    SUBTILE_STATE_KNOWN,
    SUBTILE_STATE_VISITED,
} SubtileState;

typedef enum City {
    CITY_ARROYO,
    CITY_DEN,
    CITY_KLAMATH,
    CITY_MODOC,
    CITY_VAULT_CITY,
    CITY_GECKO,
    CITY_BROKEN_HILLS,
    CITY_NEW_RENO,
    CITY_SIERRA_ARMY_BASE,
    CITY_VAULT_15,
    CITY_NEW_CALIFORNIA_REPUBLIC,
    CITY_VAULT_13,
    CITY_MILITARY_BASE,
    CITY_REDDING,
    CITY_SAN_FRANCISCO,
    CITY_NAVARRO,
    CITY_ENCLAVE,
    CITY_ABBEY,
    CITY_PRIMITIVE_TRIBE,
    CITY_ENV_PROTECTION_AGENCY,
    CITY_MODOC_GHOST_TOWN,
    CITY_CAR_OUT_OF_GAS,
    CITY_DESTROYED_ARROYO,
    CITY_KLAMATH_TOXIC_CAVES,
    CITY_DEN_SLAVE_RUN,
    CITY_RAIDERS,
    CITY_RANDOM_ENCOUNTER_DESERT,
    CITY_RANDOM_ENCOUNTER_MOUNTAIN,
    CITY_RANDOM_ENCOUNTER_CITY,
    CITY_RANDOM_ENCOUNTER_COAST,
    CITY_GOLGOTHA,
    CITY_SPECIAL_ENCOUNTER_WHALE,
    CITY_SPECIAL_ENCOUNTER_TIN_WOODSMAN,
    CITY_SPECIAL_ENCOUNTER_BIG_HEAD,
    CITY_SPECIAL_ENCOUNTER_FEDERATION_SHUTTLE,
    CITY_SPECIAL_ENCOUNTER_UNWASHED_VILLAGERS,
    CITY_SPECIAL_ENCOUNTER_MONTY_PYTHON_BRIDGE,
    CITY_SPECIAL_ENCOUNTER_CAFE_OF_BROKEN_DREAMS,
    CITY_SPECIAL_ENCOUNTER_HOLY_HAND_GRANADE_I,
    CITY_SPECIAL_ENCOUNTER_HOLY_HAND_GRANADE_II,
    CITY_SPECIAL_ENCOUNTER_GUARDIAN_OF_FOREVER,
    CITY_SPECIAL_ENCOUNTER_TOXIC_WASTE_DUMP,
    CITY_SPECIAL_ENCOUNTER_PARIAHS,
    CITY_SPECIAL_ENCOUNTER_MAD_COWS,
    CITY_CARAVAN_ENCOUNTERS,
    CITY_FAKE_VAULT_13_A,
    CITY_FAKE_VAULT_13_B,
    CITY_SHADOW_WORLDS,
    CITY_RENO_STABLES,
    CITY_COUNT,
} City;

typedef enum Map {
    MAP_RND_DESERT_1 = 0,
    MAP_RND_DESERT_2 = 1,
    MAP_RND_DESERT_3 = 2,
    MAP_ARROYO_CAVES = 3,
    MAP_ARROYO_VILLAGE = 4,
    MAP_ARROYO_BRIDGE = 5,
    MAP_DEN_ENTRANCE = 6,
    MAP_DEN_BUSINESS = 7,
    MAP_DEN_RESIDENTIAL = 8,
    MAP_KLAMATH_1 = 9,
    MAP_KLAMATH_MALL = 10,
    MAP_KLAMATH_RATCAVES = 11,
    MAP_KLAMATH_TOXICCAVES = 12,
    MAP_KLAMATH_TRAPCAVES = 13,
    MAP_KLAMATH_GRAZE = 14,
    MAP_VAULTCITY_COURTYARD = 15,
    MAP_VAULTCITY_DOWNTOWN = 16,
    MAP_VAULTCITY_COUNCIL = 17,
    MAP_MODOC_MAINSTREET = 18,
    MAP_MODOC_BEDNBREAKFAST = 19,
    MAP_MODOC_BRAHMINPASTURES = 20,
    MAP_MODOC_GARDEN = 21,
    MAP_MODOC_DOWNTHESHITTER = 22,
    MAP_MODOC_WELL = 23,
    MAP_GHOST_FARM = 24,
    MAP_GHOST_CAVERN = 25,
    MAP_GHOST_LAKE = 26,
    MAP_SIERRA_BATTLE = 27,
    MAP_SIERRA_123 = 28,
    MAP_SIERRA_4 = 29,
    MAP_VAULT_CITY_VAULT = 30,
    MAP_GECKO_SETTLEMENT = 31,
    MAP_GECKO_POWER_PLANT = 32,
    MAP_GECKO_JUNKYARD = 33,
    MAP_GECKO_ACCESS_TUNNELS = 34,
    MAP_ARROYO_WILDERNESS = 35,
    MAP_VAULT_15 = 36,
    MAP_THE_SQUAT_A = 37,
    MAP_THE_SQUAT_B = 38,
    MAP_VAULT_15_EAST_ENTRANCE = 39,
    MAP_VAULT_13 = 40,
    MAP_VAULT_13_ENTRANCE = 41,
    MAP_NCR_DOWNTOWN = 42,
    MAP_NCR_COUNCIL_1 = 43,
    MAP_NCR_WESTIN_RANCH = 44,
    MAP_NCR_GRAZING_LANDS = 45,
    MAP_NCR_BAZAAR = 46,
    MAP_NCR_COUNCIL_2 = 47,
    MAP_KLAMATH_CANYON = 48,
    MAP_MILITARY_BASE_12 = 49,
    MAP_MILITARY_BASE_34 = 50,
    MAP_MILITARY_BASE_ENTRANCE = 51,
    MAP_DEN_SLAVE_RUN = 52,
    MAP_CAR_DESERT = 53,
    MAP_NEW_RENO_1 = 54,
    MAP_NEW_RENO_2 = 55,
    MAP_NEW_RENO_3 = 56,
    MAP_NEW_RENO_4 = 57,
    MAP_NEW_RENO_CHOP_SHOP = 58,
    MAP_NEW_RENO_GOLGATHA = 59,
    MAP_NEW_RENO_STABLES = 60,
    MAP_NEW_RENO_BOXING = 61,
    MAP_REDDING_WANAMINGO_ENT = 62,
    MAP_REDDING_WANAMINGO_12 = 63,
    MAP_REDDING_DOWNTOWN = 64,
    MAP_REDDING_MINE_ENT = 65,
    MAP_REDDING_DTOWN_TUNNEL = 66,
    MAP_REDDING_MINE_TUNNEL = 67,
    MAP_RND_CITY1 = 68,
    MAP_RND_CAVERN0 = 69,
    MAP_RND_CAVERN1 = 70,
    MAP_RND_CAVERN2 = 71,
    MAP_RND_CAVERN3 = 72,
    MAP_RND_CAVERN4 = 73,
    MAP_RND_MOUNTAIN1 = 74,
    MAP_RND_MOUNTAIN2 = 75,
    MAP_RND_COAST1 = 76,
    MAP_RND_COAST2 = 77,
    MAP_BROKEN_HILLS1 = 78,
    MAP_BROKEN_HILLS2 = 79,
    MAP_RND_CAVERN5 = 80,
    MAP_RND_DESERT4 = 81,
    MAP_RND_DESERT5 = 82,
    MAP_RND_DESERT6 = 83,
    MAP_RND_DESERT7 = 84,
    MAP_RND_COAST3 = 85,
    MAP_RND_COAST4 = 86,
    MAP_RND_COAST5 = 87,
    MAP_RND_COAST6 = 88,
    MAP_RND_COAST7 = 89,
    MAP_RND_COAST8 = 90,
    MAP_RND_COAST9 = 91,
    MAP_RAIDERS_CAMP1 = 92,
    MAP_RAIDERS_CAMP2 = 93,
    MAP_BH_RND_DESERT = 94,
    MAP_BH_RND_MOUNTAIN = 95,
    MAP_SPECIAL_RND_WHALE = 96,
    MAP_SPECIAL_RND_WOODSMAN = 97,
    MAP_SPECIAL_RND_HEAD = 98,
    MAP_SPECIAL_RND_SHUTTLE = 99,
    MAP_SPECIAL_RND_UNWASHED = 100,
    MAP_SPECIAL_RND_BRIDGE = 101,
    MAP_SPECIAL_RND_CAFE = 102,
    MAP_SPECIAL_RND_HOLY1 = 103,
    MAP_SPECIAL_RND_HOLY2 = 104,
    MAP_SPECIAL_RND_GUARDIAN = 105,
    MAP_SPECIAL_RND_TOXIC = 106,
    MAP_SPECIAL_RND_PARIAH = 107,
    MAP_SPECIAL_RND_MAD_COW = 108,
    MAP_NAVARRO_ENTRANCE = 109,
    MAP_RND_COAST_10 = 110,
    MAP_RND_COAST_11 = 111,
    MAP_RND_COAST_12 = 112,
    MAP_RND_DESERT_8 = 113,
    MAP_RND_DESERT_9 = 114,
    MAP_RND_DESERT_10 = 115,
    MAP_RND_DESERT_11 = 116,
    MAP_RND_DESERT_12 = 117,
    MAP_RND_CAVERN_5 = 118,
    MAP_RND_CAVERN_6 = 119,
    MAP_RND_CAVERN_7 = 120,
    MAP_RND_MOUNTAIN_3 = 121,
    MAP_RND_MOUNTAIN_4 = 122,
    MAP_RND_MOUNTAIN_5 = 123,
    MAP_RND_MOUNTAIN_6 = 124,
    MAP_RND_CITY_2 = 125,
    MAP_ARROYO_TEMPLE = 126,
    MAP_DESTROYED_ARROYO_BRIDGE = 127,
    MAP_ENCLAVE_DETENTION = 128,
    MAP_ENCLAVE_DOCK = 129,
    MAP_ENCLAVE_END_FIGHT = 130,
    MAP_ENCLAVE_BARRACKS = 131,
    MAP_ENCLAVE_PRESIDENT = 132,
    MAP_ENCLAVE_REACTOR = 133,
    MAP_ENCLAVE_TRAP_ROOM = 134,
    MAP_SAN_FRAN_TANKER = 135,
    MAP_SAN_FRAN_DOCK = 136,
    MAP_SAN_FRAN_CHINATOWN = 137,
    MAP_SHUTTLE_EXTERIOR = 138,
    MAP_SHUTTLE_INTERIOR = 139,
    MAP_ELRONOLOGIST_BASE = 140,
    MAP_RND_CITY_3 = 141,
    MAP_RND_CITY_4 = 142,
    MAP_RND_CITY_5 = 143,
    MAP_RND_CITY_6 = 144,
    MAP_RND_CITY_7 = 145,
    MAP_RND_CITY_8 = 146,
    MAP_NEW_RENO_VB = 147,
    MAP_SHI_TEMPLE = 148,
    MAP_IN_GAME_MOVIE1 = 149,
} Map;

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
    int field_28;
    int x;
    int y;
    int size;
    int state;
    // lock state
    int field_3C;
    int field_40;
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
    int field_8;
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
    char field_0[40];
    int field_28;
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

typedef struct ENCOUNTER_ENTRY_ENC {
    int minQuantity; // min
    int maxQuantity; // max
    int field_8;
    int situation;
} ENCOUNTER_ENTRY_ENC;

typedef struct EncounterEntry {
    int flags;
    int map;
    int scenery;
    int chance;
    int counter;
    EncounterCondition condition;
    int field_50;
    ENCOUNTER_ENTRY_ENC field_54[6];
} EncounterEntry;

typedef struct EncounterTable {
    char lookupName[40];
    int field_28;
    int mapsLength;
    int maps[6];
    int field_48;
    int entriesLength;
    EncounterEntry entries[41];
} EncounterTable;

typedef struct ENC_BASE_TYPE_38_48 {
    int pid;
    int minimumQuantity;
    int maximumQuantity;
    bool isEquipped;
} ENC_BASE_TYPE_38_48;

typedef struct ENC_BASE_TYPE_38 {
    char field_0[40];
    int field_28;
    int field_2C;
    int ratio;
    int pid;
    int flags;
    int distance;
    int tile;
    int itemsLength;
    ENC_BASE_TYPE_38_48 items[10];
    int team;
    int script;
    EncounterCondition condition;
} ENC_BASE_TYPE_38;

typedef struct ENC_BASE_TYPE {
    char name[40];
    int position;
    int spacing;
    int distance;
    int field_34;
    ENC_BASE_TYPE_38 field_38[10];
} ENC_BASE_TYPE;

typedef struct SubtileInfo {
    int terrain;
    int field_4;
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

//
typedef struct CitySizeDescription {
    int fid;
    int width;
    int height;
    CacheEntry* handle;
    unsigned char* data;
} CitySizeDescription;

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

extern const int _can_rest_here[ELEVATION_COUNT];
extern const int gDayPartEncounterFrequencyModifiers[DAY_PART_COUNT];
extern const char* off_4BC878[2];
extern MessageListItem stru_4BC880;

extern char _aCricket[];
extern char _aCricket1[];

extern const char* _wmStateStrs[2];
extern const char* _wmYesNoStrs[2];
extern const char* gEncounterFrequencyTypeKeys[ENCOUNTER_FREQUENCY_TYPE_COUNT];
extern const char* _wmFillStrs[9];
extern const char* _wmSceneryStrs[ENCOUNTER_SCENERY_TYPE_COUNT];
extern Terrain* gTerrains;
extern int gTerrainsLength;
extern TileInfo* gWorldmapTiles;
extern int gWorldmapTilesLength;
extern int gWorldmapGridWidth;
extern CityInfo* gCities;
extern int gCitiesLength;
extern const char* gCitySizeKeys[CITY_SIZE_COUNT];
extern MapInfo* gMaps;
extern int gMapsLength;
extern int gWorldmapWindow;
extern CacheEntry* gWorldmapBoxFrmHandle;
extern int gWorldmapBoxFrmWidth;
extern int gWorldmapBoxFrmHeight;
extern unsigned char* gWorldmapWindowBuffer;
extern unsigned char* gWorldmapBoxFrmData;
extern int gWorldmapOffsetX;
extern int gWorldmapOffsetY;
extern unsigned char* _circleBlendTable;
extern int _wmInterfaceWasInitialized;
extern const char* _wmEncOpStrs[ENCOUNTER_SITUATION_COUNT];
extern const char* _wmConditionalOpStrs[ENCOUNTER_CONDITIONAL_OPERATOR_COUNT];
extern const char* gEncounterFormationTypeKeys[ENCOUNTER_FORMATION_TYPE_COUNT];
extern int gWorldmapEncounterFrmIds[WORLD_MAP_ENCOUNTER_FRM_COUNT];
extern int* gQuickDestinations;
extern int gQuickDestinationsLength;
extern int _wmTownMapCurArea;
extern unsigned int _wmLastRndTime;
extern int _wmRndIndex;
extern int _wmRndCallCount;
extern int _terrainCounter;
extern unsigned int _lastTime_2;
extern bool _couldScroll;
extern unsigned char* gWorldmapCurrentCityMapFrmData;
extern CacheEntry* gWorldmapCityMapFrmHandle;
extern int gWorldmapCityMapFrmWidth;
extern int gWorldmapCityMapFrmHeight;
extern char* _wmRemapSfxList[2];

extern int _wmRndTileDirs[2];
extern int _wmRndCenterTiles[2];
extern int _wmRndCenterRotations[2];
extern int _wmRndRotOffsets[2];
extern int _wmTownMapButtonId[ENTRANCE_LIST_CAPACITY];
extern int _wmGenData;
extern int _Meet_Frank_Horrigan;
extern int _WorldMapCurrArea;
extern int _world_xpos;
extern int _world_ypos;
extern SubtileInfo* _world_subtile;
extern int dword_672E18;
extern bool gWorldmapIsTravelling;
extern int gWorldmapTravelDestX;
extern int gWorldmapTravelDestY;
extern int dword_672E28;
extern int dword_672E2C;
extern int dword_672E30;
extern int dword_672E34;
extern int _x_line_inc;
extern int dword_672E3C;
extern int _y_line_inc;
extern int dword_672E44;
extern int _wmEncounterIconShow;
extern int _EncounterMapID;
extern int dword_672E50;
extern int dword_672E54;
extern int _wmRndCursorFid;
extern int _old_world_xpos;
extern int _old_world_ypos;
extern bool gWorldmapIsInCar;
extern int _carCurrentArea;
extern int gWorldmapCarFuel;
extern CacheEntry* gWorldmapCarFrmHandle;
extern Art* gWorldmapCarFrm;
extern int gWorldmapCarFrmWidth;
extern int gWorldmapCarFrmHeight;
extern int gWorldmapCarFrmCurrentFrame;
extern CacheEntry* gWorldmapHotspotUpFrmHandle;
extern unsigned char* gWorldmapHotspotUpFrmData;
extern CacheEntry* gWorldmapHotspotDownFrmHandle;
extern unsigned char* gWorldmapHotspotDownFrmData;
extern int gWorldmapHotspotUpFrmWidth;
extern int gWorldmapHotspotUpFrmHeight;
extern CacheEntry* gWorldmapDestinationMarkerFrmHandle;
extern unsigned char* gWorldmapDestinationMarkerFrmData;
extern int gWorldmapDestinationMarkerFrmWidth;
extern int gWorldmapDestinationMarkerFrmHeight;
extern CacheEntry* gWorldmapLocationMarkerFrmHandle;
extern unsigned char* gWorldmapLocationMarkerFrmData;
extern int gWorldmapLocationMarkerFrmWidth;
extern int gWorldmapLocationMarkerFrmHeight;
extern CacheEntry* gWorldmapEncounterFrmHandles[WORLD_MAP_ENCOUNTER_FRM_COUNT];
extern unsigned char* gWorldmapEncounterFrmData[WORLD_MAP_ENCOUNTER_FRM_COUNT];
extern int gWorldmapEncounterFrmWidths[WORLD_MAP_ENCOUNTER_FRM_COUNT];
extern int gWorldmapEncounterFrmHeights[WORLD_MAP_ENCOUNTER_FRM_COUNT];
extern int _wmViewportRightScrlLimit;
extern int _wmViewportBottomtScrlLimit;
extern CacheEntry* gWorldmapTownTabsUnderlayFrmHandle;
extern int gWorldmapTownTabsUnderlayFrmWidth;
extern int gWorldmapTownTabsUnderlayFrmHeight;
extern int _LastTabsYOffset;
extern unsigned char* gWorldmapTownTabsUnderlayFrmData;
extern CacheEntry* gWorldmapTownTabsEdgeFrmHandle;
extern unsigned char* gWorldmapTownTabsEdgeFrmData;
extern CacheEntry* gWorldmapDialFrmHandle;
extern int gWorldmapDialWidth;
extern int gWorldmapDialHeight;
extern int gWorldmapDialFrmCurrentFrame;
extern Art* gWorldmapDialFrm;
extern CacheEntry* gWorldmapCarOverlayFrmHandle;
extern int gWorldmapCarOverlayFrmWidth;
extern int gWorldmapCarOverlayFrmHeight;
extern unsigned char* gWorldmapOverlayFrmData;
extern CacheEntry* gWorldmapGlobeOverlayFrmHandle;
extern int gWorldmapGlobeOverlayFrmWidth;
extern int gWorldmapGloveOverlayFrmHeight;
extern unsigned char* gWorldmapGlobeOverlayFrmData;
extern int dword_672F54;
extern int _tabsOffset;
extern CacheEntry* gWorldmapLittleRedButtonUpFrmHandle;
extern CacheEntry* gWorldmapLittleRedButtonDownFrmHandle;
extern unsigned char* gWorldmapLittleRedButtonUpFrmData;
extern unsigned char* gWorldmapLittleRedButtonDownFrmData;
extern CacheEntry* gWorldmapTownListScrollUpFrmHandle[2];
extern int gWorldmapTownListScrollUpFrmWidth;
extern int gWorldmapTownListScrollUpFrmHeight;
extern unsigned char* gWorldmapTownListScrollUpFrmData[2];
extern CacheEntry* gWorldmapTownListScrollDownFrmHandle[2];
extern int gWorldmapTownListScrollDownFrmWidth;
extern int gWorldmapTownListScrollDownFrmHeight;
extern unsigned char* gWorldmapTownListScrollDownFrmData[2];
extern CacheEntry* gWorldmapMonthsFrmHandle;
extern Art* gWorldmapMonthsFrm;
extern CacheEntry* gWorldmapNumbersFrmHandle;
extern Art* gWorldmapNumbersFrm;
extern int _fontnum;
extern MessageList gWorldmapMessageList;
extern int _wmFreqValues[6];
extern int _wmRndOriginalCenterTile;
extern Config* gWorldmapConfig;
extern int _wmTownMapSubButtonIds[7];
extern ENC_BASE_TYPE* _wmEncBaseTypeList;
extern CitySizeDescription gCitySizeDescriptions[CITY_SIZE_COUNT];
extern EncounterTable* gEncounterTables;
extern int _wmMaxEncBaseTypes;
extern int gEncounterTablesLength;

int worldmapInit();
int _wmGenDataInit();
int _wmGenDataReset();
void worldmapExit();
int worldmapReset();
int worldmapSave(File* stream);
int worldmapLoad(File* stream);
int _wmWorldMapSaveTempData();
int _wmWorldMapLoadTempData();
int worldmapConfigInit();
int worldmapConfigLoadEncounterTable(Config* config, char* lookup_name, char* section);
int worldmapConfigLoadEncounterEntry(EncounterEntry* entry, char* str);
int _wmParseEncounterSubEncStr(EncounterEntry* entry, char** str_ptr);
int _wmParseFindSubEncTypeMatch(char* str, int* out_value);
int _wmFindEncBaseTypeMatch(char* str, int* out_value);
int _wmReadEncBaseType(char* str, int* out_value);
int _wmParseEncBaseSubTypeStr(ENC_BASE_TYPE_38* ptr, char** str_ptr);
int _wmEncBaseTypeSlotInit(ENC_BASE_TYPE* entry);
int _wmEncBaseSubTypeSlotInit(ENC_BASE_TYPE_38* entry);
int _wmEncounterSubEncSlotInit(ENCOUNTER_ENTRY_ENC* entry);
int worldmapEncounterTableEntryInit(EncounterEntry* entry);
int worldmapEncounterTableInit(EncounterTable* encounterTable);
int worldmapTileInfoInit(TileInfo* tile);
int worldmapTerrainInfoInit(Terrain* terrain);
int worldmapConfigInitEncounterCondition(EncounterCondition* condition);
int _wmParseTerrainTypes(Config* config, char* str);
int _wmParseTerrainRndMaps(Config* config, Terrain* terrain);
int worldmapConfigLoadSubtile(TileInfo* tile, int x, int y, char* str);
int worldmapFindEncounterTableByLookupName(char* str, int* out_value);
int worldmapFindTerrainByLookupName(char* str, int* out_value);
int _wmParseEncounterItemType(char** str_ptr, ENC_BASE_TYPE_38_48* a2, int* a3, const char* delim);
int _wmParseItemType(char* str, ENC_BASE_TYPE_38_48* ptr);
int worldmapConfigParseCondition(char** stringPtr, const char* a2, EncounterCondition* condition);
int worldmapConfigParseConditionEntry(char** stringPtr, const char* a2, int* typePtr, int* operatorPtr, int* paramPtr, int* valuePtr);
int worldmapConfigParseEncounterConditionalOperator(char** str_ptr, int* out_op);
int worldmapCityInfoInit(CityInfo* area);
int cityInit();
int worldmapFindMapByLookupName(char* str, int* out_value);
int worldmapCityEntranceInfoInit(EntranceInfo* entrance);
int worldmapMapInfoInit(MapInfo* map);
int _wmMapInit();
int worldmapRandomStartingPointInit(MapStartPointInfo* rsp);
int mapGetCount();
int mapGetFileName(int map_index, char* dest);
int mapGetIndexByFileName(char* name);
bool _wmMapIdxIsSaveable(int map_index);
bool _wmMapIsSaveable();
bool _wmMapDeadBodiesAge();
bool _wmMapCanRestHere(int elevation);
bool _wmMapPipboyActive();
int _wmMapMarkVisited(int map_index);
int _wmMatchEntranceFromMap(int cityIndex, int mapIndex, int* entranceIndexPtr);
int _wmMatchEntranceElevFromMap(int cityIndex, int map, int elevation, int* entranceIndexPtr);
int _wmMatchAreaFromMap(int a1, int* out_a2);
int _wmMapMarkMapEntranceState(int a1, int a2, int a3);
void _wmWorldMap();
int _wmWorldMapFunc(int a1);
int _wmCheckGameAreaEvents();
int _wmInterfaceCenterOnParty();
int _wmRndEncounterOccurred();
int _wmPartyFindCurSubTile();
int _wmFindCurSubTileFromPos(int x, int y, SubtileInfo** subtile);
int _wmFindCurTileFromPos(int x, int y, TileInfo** tile);
int _wmRndEncounterPick();
int worldmapSetupRandomEncounter();
int worldmapSetupRandomEncounter();
int worldmapSetupCritters(int type_idx, Object** out_critter, int a3);
int _wmSetupRndNextTileNumInit(ENC_BASE_TYPE* a1);
int _wmSetupRndNextTileNum(ENC_BASE_TYPE* a1, ENC_BASE_TYPE_38* a2, int* out_tile_num);
bool _wmEvalTileNumForPlacement(int tile);
bool _wmEvalConditional(EncounterCondition* a1, int* a2);
bool _wmEvalSubConditional(int a1, int a2, int a3);
bool _wmGameTimeIncrement(int a1);
int _wmGrabTileWalkMask(int tile_index);
bool _wmWorldPosInvalid(int a1, int a2);
void _wmPartyInitWalking(int x, int y);
void worldmapPerformTravel();
void _wmInterfaceScrollTabsStart(int a1);
void _wmInterfaceScrollTabsStop();
int worldmapWindowInit();
int worldmapWindowFree();
int worldmapWindowScroll(int a1, int a2, int a3, int a4, bool* a5, bool a6);
void worldmapWindowHandleMouseScrolling();
int _wmMarkSubTileOffsetVisitedFunc(int a1, int a2, int a3, int a4, int a5, int a6);
void _wmMarkSubTileRadiusVisited(int x, int y);
int _wmSubTileMarkRadiusVisited(int x, int y, int radius);
int _wmSubTileGetVisitedState(int a1, int a2, int* a3);
int _wmTileGrabArt(int tile_index);
int worldmapWindowRefresh();
void worldmapWindowRenderDate(bool shouldRefreshWindow);
int _wmMatchWorldPosToArea(int a1, int a2, int* a3);
int worldmapWindowRenderCity(CityInfo* cityInfo, CitySizeDescription* citySizeInfo, unsigned char* buffer, int x, int y);
void worldmapWindowDimRect(unsigned char* dest, int width, int height, int pitch);
int worldmapWindowDimSubtile(TileInfo* tileInfo, int a2, int a3, int a4, int a5, int a6);
int _wmDrawCursorStopped();
bool _wmCursorIsVisible();
int _wmGetAreaIdxName(int index, char* name);
bool _wmAreaIsKnown(int city_index);
int _wmAreaVisitedState(int a1);
bool _wmMapIsKnown(int map_index);
bool _wmAreaMarkVisitedState(int a1, int a2);
bool _wmAreaSetVisibleState(int a1, int a2, int a3);
int worldmapCitySetPos(int index, int x, int y);
int _wmGetPartyWorldPos(int* out_x, int* out_y);
int _wmGetPartyCurArea(int* a1);
void _wmMarkAllSubTiles(int a1);
void _wmTownMap();
int worldmapCityMapViewSelect(int* mapIndexPtr);
int worldmapCityMapViewInit();
int worldmapCityMapViewRefresh();
int worldmapCityMapViewFree();
int carConsumeFuel(int a1);
int carAddFuel(int a1);
int carGetFuel();
bool carIsEmpty();
int carGetCity();
int _wmCarGiveToParty();
int ambientSoundEffectGetLength();
int ambientSoundEffectGetRandom();
int ambientSoundEffectGetName(int ambientSoundEffectIndex, char** namePtr);
int worldmapWindowRenderChrome(bool shouldRefreshWindow);
void worldmapWindowRenderCarFuelBar();
int worldmapRenderQuickDestinations();
int _wmMakeTabsLabelList(int** out_cities, int* out_len);
int worldmapCompareCitiesByName(const void* a1, const void* a2);
void worldmapWindowRenderDial(bool shouldRefreshWindow);
int _wmAreaFindFirstValidMap(int* out_a1);
int worldmapStartMapMusic();
int worldmapSetMapMusic(int a1, const char* name);
int _wmMatchAreaContainingMapIdx(int map_index, int* out_city_index);
int _wmTeleportToArea(int a1);

static inline bool cityIsValid(int city)
{
    return city >= 0 && city < gCitiesLength;
}

#endif /* WORLD_MAP_H */
