#ifndef WORLD_MAP_H
#define WORLD_MAP_H

#include "db.h"

namespace fallout {

#define CAR_FUEL_MAX (80000)

typedef enum MapFlags {
    MAP_SAVED = 0x01,
    MAP_DEAD_BODIES_AGE = 0x02,
    MAP_PIPBOY_ACTIVE = 0x04,
    MAP_CAN_REST_ELEVATION_0 = 0x08,
    MAP_CAN_REST_ELEVATION_1 = 0x10,
    MAP_CAN_REST_ELEVATION_2 = 0x20,
} MapFlags;

typedef enum CityState {
    CITY_STATE_UNKNOWN,
    CITY_STATE_KNOWN,
    CITY_STATE_VISITED,
    CITY_STATE_INVISIBLE = -66,
} CityState;

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

#define ENCOUNTER_FLAG_NO_CAR 0x1
#define ENCOUNTER_FLAG_LOCK 0x2
#define ENCOUNTER_FLAG_NO_ICON 0x4
#define ENCOUNTER_FLAG_ICON_SP 0x8
#define ENCOUNTER_FLAG_FADEOUT 0x10

extern unsigned char* circleBlendTable;

int wmWorldMap_init();
void wmWorldMap_exit();
int wmWorldMap_reset();
int wmWorldMap_save(File* stream);
int wmWorldMap_load(File* stream);
int wmMapMaxCount();
int wmMapIdxToName(int mapIdx, char* dest, size_t size);
int wmMapMatchNameToIdx(char* name);
bool wmMapIdxIsSaveable(int mapIdx);
bool wmMapIsSaveable();
bool wmMapDeadBodiesAge();
bool wmMapCanRestHere(int elevation);
bool wmMapPipboyActive();
int wmMapMarkVisited(int mapIdx);
int wmMapMarkMapEntranceState(int mapIdx, int elevation, int state);
void wmWorldMap();
int wmCheckGameAreaEvents();
int wmSetupRandomEncounter();
bool wmEvalTileNumForPlacement(int tile);
int wmSubTileMarkRadiusVisited(int x, int y, int radius);
int wmSubTileGetVisitedState(int x, int y, int* statePtr);
int wmGetAreaIdxName(int areaIdx, char* name);
bool wmAreaIsKnown(int areaIdx);
int wmAreaVisitedState(int areaIdx);
bool wmMapIsKnown(int mapIdx);
int wmAreaMarkVisited(int areaIdx);
bool wmAreaMarkVisitedState(int areaIdx, int state);
bool wmAreaSetVisibleState(int areaIdx, int state, bool force);
int wmAreaSetWorldPos(int areaIdx, int x, int y);
int wmGetPartyWorldPos(int* xPtr, int* yPtr);
int wmGetPartyCurArea(int* areaIdxPtr);
void wmTownMap();
int wmCarUseGas(int amount);
int wmCarFillGas(int amount);
int wmCarGasAmount();
bool wmCarIsOutOfGas();
int wmCarCurrentArea();
int wmCarGiveToParty();
int wmSfxMaxCount();
int wmSfxRollNextIdx();
int wmSfxIdxName(int sfxIdx, char** namePtr);
int wmMapMusicStart();
int wmSetMapMusic(int mapIdx, const char* name);
int wmMatchAreaContainingMapIdx(int mapIdx, int* areaIdxPtr);
int wmTeleportToArea(int areaIdx);

void wmSetPartyWorldPos(int x, int y);
void wmCarSetCurrentArea(int area);
void wmForceEncounter(int map, unsigned int flags);

} // namespace fallout

#endif /* WORLD_MAP_H */
