#include "world_map.h"

#include "animation.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "core.h"
#include "critter.h"
#include "cycle.h"
#include "dbox.h"
#include "debug.h"
#include "display_monitor.h"
#include "draw.h"
#include "game.h"
#include "game_config.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "interface.h"
#include "item.h"
#include "memory.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "proto_instance.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "skill.h"
#include "stat.h"
#include "string_parsers.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define WM_TILE_WIDTH (350)
#define WM_TILE_HEIGHT (300)

#define WM_SUBTILE_SIZE (50)

#define WM_WINDOW_WIDTH (640)
#define WM_WINDOW_HEIGHT (480)

#define WM_VIEW_X (22)
#define WM_VIEW_Y (21)
#define WM_VIEW_WIDTH (450)
#define WM_VIEW_HEIGHT (443)

// 0x4BC860
const int _can_rest_here[ELEVATION_COUNT] = {
    MAP_CAN_REST_ELEVATION_0,
    MAP_CAN_REST_ELEVATION_1,
    MAP_CAN_REST_ELEVATION_2,
};

// 0x4BC86C
const int gDayPartEncounterFrequencyModifiers[DAY_PART_COUNT] = {
    40,
    30,
    0,
};

// 0x4BC878
const char* off_4BC878[2] = {
    "You detect something up ahead.",
    "Do you wish to encounter it?",
};

// 0x4BC880
MessageListItem stru_4BC880;

// 0x50EE44
char _aCricket[] = "cricket";

// 0x50EE4C
char _aCricket1[] = "cricket1";

// 0x51DD88
const char* _wmStateStrs[2] = {
    "off",
    "on"
};

// 0x51DD90
const char* _wmYesNoStrs[2] = {
    "no",
    "yes",
};

// 0x51DD98
const char* gEncounterFrequencyTypeKeys[ENCOUNTER_FREQUENCY_TYPE_COUNT] = {
    "none",
    "rare",
    "uncommon",
    "common",
    "frequent",
    "forced",
};

// 0x51DDB0
const char* _wmFillStrs[9] = {
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
const char* _wmSceneryStrs[ENCOUNTER_SCENERY_TYPE_COUNT] = {
    "none",
    "light",
    "normal",
    "heavy",
};

// 0x51DDE4
Terrain* gTerrains = NULL;

// 0x51DDE8
int gTerrainsLength = 0;

// 0x51DDEC
TileInfo* gWorldmapTiles = NULL;

// 0x51DDF0
int gWorldmapTilesLength = 0;

// The width of worldmap grid in tiles.
//
// There is no separate variable for grid height, instead its calculated as
// [gWorldmapTilesLength] / [gWorldmapTilesGridWidth].
//
// num_horizontal_tiles
// 0x51DDF4
int gWorldmapGridWidth = 0;

// 0x51DDF8
CityInfo* gCities = NULL;

// 0x51DDFC
int gCitiesLength = 0;

// 0x51DE00
const char* gCitySizeKeys[CITY_SIZE_COUNT] = {
    "small",
    "medium",
    "large",
};

// 0x51DE0C
MapInfo* gMaps = NULL;

// 0x51DE10
int gMapsLength = 0;

// 0x51DE14
int gWorldmapWindow = -1;

// 0x51DE18
CacheEntry* gWorldmapBoxFrmHandle = INVALID_CACHE_ENTRY;

// 0x51DE1C
int gWorldmapBoxFrmWidth = 0;

// 0x51DE20
int gWorldmapBoxFrmHeight = 0;

// 0x51DE24
unsigned char* gWorldmapWindowBuffer = NULL;

// 0x51DE28
unsigned char* gWorldmapBoxFrmData = NULL;

// 0x51DE2C
int gWorldmapOffsetX = 0;

// 0x51DE30
int gWorldmapOffsetY = 0;

//
unsigned char* _circleBlendTable = NULL;

//
int _wmInterfaceWasInitialized = 0;

// encounter types
const char* _wmEncOpStrs[ENCOUNTER_SITUATION_COUNT] = {
    "nothing",
    "ambush",
    "fighting",
    "and",
};

// operators
const char* _wmConditionalOpStrs[ENCOUNTER_CONDITIONAL_OPERATOR_COUNT] = {
    "_",
    "==",
    "!=",
    "<",
    ">",
};

// 0x51DE6C
const char* gEncounterFormationTypeKeys[ENCOUNTER_FORMATION_TYPE_COUNT] = {
    "surrounding",
    "straight_line",
    "double_line",
    "wedge",
    "cone",
    "huddle",
};

// 0x51DE84
int gWorldmapEncounterFrmIds[WORLD_MAP_ENCOUNTER_FRM_COUNT] = {
    154,
    155,
    438,
    439,
};

// 0x51DE94
int* gQuickDestinations = NULL;

// 0x51DE98
int gQuickDestinationsLength = 0;

// 0x51DE9C
int _wmTownMapCurArea = -1;

// 0x51DEA0
unsigned int _wmLastRndTime = 0;

// 0x51DEA4
int _wmRndIndex = 0;

// 0x51DEA8
int _wmRndCallCount = 0;

// 0x51DEAC
int _terrainCounter = 1;

// 0x51DEB0
unsigned int _lastTime_2 = 0;

// 0x51DEB4
bool _couldScroll = true;

// 0x51DEB8
unsigned char* gWorldmapCityMapFrmData = NULL;

// 0x51DEBC
CacheEntry* gWorldmapCityMapFrmHandle = INVALID_CACHE_ENTRY;

// 0x51DEC0
int gWorldmapCityMapFrmWidth = 0;

// 0x51DEC4
int gWorldmapCityMapFrmHeight = 0;

// 0x51DEC8
char* _wmRemapSfxList[2] = {
    _aCricket,
    _aCricket1,
};

// 0x672DB8
int _wmRndTileDirs[2];

// 0x672DC0
int _wmRndCenterTiles[2];

// 0x672DC8
int _wmRndCenterRotations[2];

// 0x672DD0
int _wmRndRotOffsets[2];

// Buttons for city entrances.
//
// 0x672DD8
int _wmTownMapButtonId[ENTRANCE_LIST_CAPACITY];

// 0x672E00
int _wmGenData;

// 0x672E04
int _Meet_Frank_Horrigan;

// Current_town.
//
// 0x672E08
int _WorldMapCurrArea;

// Current town x.
//
// 0x672E0C
int _world_xpos;

// Current town y.
//
// 0x672E10
int _world_ypos;

// 0x672E14
SubtileInfo* _world_subtile;

// 0x672E18
int dword_672E18;

// 0x672E1C
bool gWorldmapIsTravelling;

// 0x672E20
int gWorldmapTravelDestX;

// 0x672E24
int gWorldmapTravelDestY;

// 0x672E28
int dword_672E28;

// 0x672E2C
int dword_672E2C;

// 0x672E30
int dword_672E30;

// 0x672E34
int dword_672E34;

// 0x672E38
int _x_line_inc;

// 0x672E3C
int dword_672E3C;

// 0x672E40
int _y_line_inc;

// 0x672E44
int dword_672E44;

// 0x672E48
int _wmEncounterIconShow;

// 0x672E4C
int _EncounterMapID;

// 0x672E50
int dword_672E50;

// 0x672E54
int dword_672E54;

// 0x672E58
int _wmRndCursorFid;

// 0x672E5C
int _old_world_xpos;

// 0x672E60
int _old_world_ypos;

// 0x672E64
bool gWorldmapIsInCar;

// 0x672E68
int _carCurrentArea;

// 0x672E6C
int gWorldmapCarFuel;

// 0x672E70
CacheEntry* gWorldmapCarFrmHandle;

// 0x672E74
Art* gWorldmapCarFrm;

// 0x672E78
int gWorldmapCarFrmWidth;

// 0x672E7C
int gWorldmapCarFrmHeight;

// 0x672E80
int gWorldmapCarFrmCurrentFrame;

// 0x672E84
CacheEntry* gWorldmapHotspotUpFrmHandle;

// 0x672E88
unsigned char* gWorldmapHotspotUpFrmData;

// 0x672E8C
CacheEntry* gWorldmapHotspotDownFrmHandle;

// 0x672E90
unsigned char* gWorldmapHotspotDownFrmData;

// 0x672E94
int gWorldmapHotspotUpFrmWidth;

// 0x672E98
int gWorldmapHotspotUpFrmHeight;

// 0x672E9C
CacheEntry* gWorldmapDestinationMarkerFrmHandle;

// 0x672EA0
unsigned char* gWorldmapDestinationMarkerFrmData;

// 0x672EA4
int gWorldmapDestinationMarkerFrmWidth;

// 0x672EA8
int gWorldmapDestinationMarkerFrmHeight;

// 0x672EAC
CacheEntry* gWorldmapLocationMarkerFrmHandle;

// 0x672EB0
unsigned char* gWorldmapLocationMarkerFrmData;

// 0x672EB4
int gWorldmapLocationMarkerFrmWidth;

// 0x672EB8
int gWorldmapLocationMarkerFrmHeight;

// 0x672EBC
CacheEntry* gWorldmapEncounterFrmHandles[WORLD_MAP_ENCOUNTER_FRM_COUNT];

// 0x672ECC
unsigned char* gWorldmapEncounterFrmData[WORLD_MAP_ENCOUNTER_FRM_COUNT];

// 0x672EDC
int gWorldmapEncounterFrmWidths[WORLD_MAP_ENCOUNTER_FRM_COUNT];

// 0x672EEC
int gWorldmapEncounterFrmHeights[WORLD_MAP_ENCOUNTER_FRM_COUNT];

// 0x672EFC
int _wmViewportRightScrlLimit;

// 0x672F00
int _wmViewportBottomtScrlLimit;

// 0x672F04
CacheEntry* gWorldmapTownTabsUnderlayFrmHandle;

// 0x672F08
int gWorldmapTownTabsUnderlayFrmWidth;

// 0x672F0C
int gWorldmapTownTabsUnderlayFrmHeight;

// 0x672F10
int _LastTabsYOffset;

// 0x672F14
unsigned char* gWorldmapTownTabsUnderlayFrmData;

// 0x672F18
CacheEntry* gWorldmapTownTabsEdgeFrmHandle;

// 0x672F1C
unsigned char* gWorldmapTownTabsEdgeFrmData;

// 0x672F20
CacheEntry* gWorldmapDialFrmHandle;

// 0x672F24
int gWorldmapDialFrmWidth;

// 0x672F28
int gWorldmapDialFrmHeight;

// 0x672F2C
int gWorldmapDialFrmCurrentFrame;

// 0x672F30
Art* gWorldmapDialFrm;

// 0x672F34
CacheEntry* gWorldmapCarOverlayFrmHandle;

// 0x672F38
int gWorldmapCarOverlayFrmWidth;

// 0x672F3C
int gWorldmapCarOverlayFrmHeight;

// 0x672F40
unsigned char* gWorldmapCarOverlayFrmData;

// 0x672F44
CacheEntry* gWorldmapGlobeOverlayFrmHandle;

// 0x672F48
int gWorldmapGlobeOverlayFrmWidth;

// 0x672F4C
int gWorldmapGloveOverlayFrmHeight;

// 0x672F50
unsigned char* gWorldmapGlobeOverlayFrmData;

// 0x672F54
int dword_672F54;

// 0x672F58
int _tabsOffset;

// 0x672F5C
CacheEntry* gWorldmapLittleRedButtonUpFrmHandle;

// 0x672F60
CacheEntry* gWorldmapLittleRedButtonDownFrmHandle;

// 0x672F64
unsigned char* gWorldmapLittleRedButtonUpFrmData;

// 0x672F68
unsigned char* gWorldmapLittleRedButtonDownFrmData;

// 0x672F6C
CacheEntry* gWorldmapTownListScrollUpFrmHandle[WORLDMAP_ARROW_FRM_COUNT];

// 0x672F74
int gWorldmapTownListScrollUpFrmWidth;

// 0x672F78
int gWorldmapTownListScrollUpFrmHeight;

// 0x672F7C
unsigned char* gWorldmapTownListScrollUpFrmData[WORLDMAP_ARROW_FRM_COUNT];

// 0x672F84
CacheEntry* gWorldmapTownListScrollDownFrmHandle[WORLDMAP_ARROW_FRM_COUNT];

// 0x672F8C
int gWorldmapTownListScrollDownFrmWidth;

// 0x672F90
int gWorldmapTownListScrollDownFrmHeight;

// 0x672F94
unsigned char* gWorldmapTownListScrollDownFrmData[WORLDMAP_ARROW_FRM_COUNT];

// 0x672F9C
CacheEntry* gWorldmapMonthsFrmHandle;

// 0x672FA0
Art* gWorldmapMonthsFrm;

// 0x672FA4
CacheEntry* gWorldmapNumbersFrmHandle;

// 0x672FA8
Art* gWorldmapNumbersFrm;

// 0x672FAC
int _fontnum;

// worldmap.msg
//
// 0x672FB0
MessageList gWorldmapMessageList;

// 0x672FB8
int _wmFreqValues[6];

// 0x672FD0
int _wmRndOriginalCenterTile;

// worldmap.txt
//
// 0x672FD4
Config* gWorldmapConfig;

// 0x672FD8
int _wmTownMapSubButtonIds[7];

// 0x672FF4
ENC_BASE_TYPE* _wmEncBaseTypeList;

// 0x672FF8
CitySizeDescription gCitySizeDescriptions[CITY_SIZE_COUNT];

// 0x673034
EncounterTable* gEncounterTables;

// Number of enc_base_types.
//
// 0x673038
int _wmMaxEncBaseTypes;

// 0x67303C
int gEncounterTablesLength;

// wmWorldMap_init
// 0x4BC89C
int worldmapInit()
{
    char path[MAX_PATH];

    if (_wmGenDataInit() == -1) {
        return -1;
    }

    if (!messageListInit(&gWorldmapMessageList)) {
        return -1;
    }

    sprintf(path, "%s%s", asc_5186C8, "worldmap.msg");

    if (!messageListLoad(&gWorldmapMessageList, path)) {
        return -1;
    }

    if (worldmapConfigInit() == -1) {
        return -1;
    }

    _wmViewportRightScrlLimit = WM_TILE_WIDTH * gWorldmapGridWidth - WM_VIEW_WIDTH;
    _wmViewportBottomtScrlLimit = WM_TILE_HEIGHT * (gWorldmapTilesLength / gWorldmapGridWidth) - WM_VIEW_HEIGHT;
    _circleBlendTable = _getColorBlendTable(_colorTable[992]);

    _wmMarkSubTileRadiusVisited(_world_xpos, _world_ypos);
    _wmWorldMapSaveTempData();

    return 0;
}

// 0x4BC984
int _wmGenDataInit()
{
    _Meet_Frank_Horrigan = 0;
    _WorldMapCurrArea = -1;
    _world_xpos = 173;
    _world_ypos = 122;
    _world_subtile = 0;
    dword_672E18 = 0;
    gWorldmapIsTravelling = false;
    gWorldmapTravelDestX = -1;
    gWorldmapTravelDestY = -1;
    dword_672E28 = 0;
    dword_672E2C = 0;
    dword_672E30 = 0;
    dword_672E34 = 0;
    _x_line_inc = 0;
    _y_line_inc = 0;
    dword_672E44 = 0;
    _wmEncounterIconShow = 0;
    _EncounterMapID = -1;
    dword_672E50 = -1;
    dword_672E54 = -1;
    _wmRndCursorFid = -1;
    _old_world_xpos = 0;
    _old_world_ypos = 0;
    gWorldmapIsInCar = false;
    _carCurrentArea = -1;
    gWorldmapCarFuel = CAR_FUEL_MAX;
    gWorldmapCarFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapCarFrmWidth = 0;
    gWorldmapCarFrmHeight = 0;
    gWorldmapCarFrmCurrentFrame = 0;
    gWorldmapHotspotUpFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapHotspotUpFrmData = NULL;
    gWorldmapHotspotDownFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapHotspotDownFrmData = NULL;
    gWorldmapHotspotUpFrmWidth = 0;
    gWorldmapHotspotUpFrmHeight = 0;
    gWorldmapDestinationMarkerFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapDestinationMarkerFrmData = NULL;
    gWorldmapDestinationMarkerFrmWidth = 0;
    gWorldmapDestinationMarkerFrmHeight = 0;
    gWorldmapLocationMarkerFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapLocationMarkerFrmData = NULL;
    gWorldmapLocationMarkerFrmWidth = 0;
    _wmGenData = 0;
    dword_672E3C = 0;
    gWorldmapLocationMarkerFrmHeight = 0;
    gWorldmapCarFrm = NULL;

    for (int index = 0; index < WORLD_MAP_ENCOUNTER_FRM_COUNT; index++) {
        gWorldmapEncounterFrmHandles[index] = INVALID_CACHE_ENTRY;
        gWorldmapEncounterFrmData[index] = NULL;
        gWorldmapEncounterFrmWidths[index] = 0;
        gWorldmapEncounterFrmHeights[index] = 0;
    }

    _wmViewportBottomtScrlLimit = 0;
    gWorldmapTownTabsUnderlayFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapTownTabsUnderlayFrmData = NULL;
    gWorldmapTownTabsUnderlayFrmWidth = 0;
    gWorldmapTownTabsUnderlayFrmHeight = 0;
    _LastTabsYOffset = 0;
    gWorldmapTownTabsEdgeFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapTownTabsEdgeFrmData = 0;
    gWorldmapDialFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapDialFrm = NULL;
    gWorldmapDialFrmWidth = 0;
    gWorldmapDialFrmHeight = 0;
    gWorldmapDialFrmCurrentFrame = 0;
    gWorldmapCarOverlayFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapCarOverlayFrmData = NULL;
    gWorldmapCarOverlayFrmWidth = 0;
    gWorldmapCarOverlayFrmHeight = 0;
    gWorldmapGlobeOverlayFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapGlobeOverlayFrmData = NULL;
    gWorldmapGlobeOverlayFrmWidth = 0;
    gWorldmapGloveOverlayFrmHeight = 0;
    dword_672F54 = 0;
    _tabsOffset = 0;
    gWorldmapLittleRedButtonUpFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapLittleRedButtonDownFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapLittleRedButtonUpFrmData = NULL;
    gWorldmapLittleRedButtonDownFrmData = NULL;
    _wmViewportRightScrlLimit = 0;

    for (int index = 0; index < WORLDMAP_ARROW_FRM_COUNT; index++) {
        gWorldmapTownListScrollDownFrmHandle[index] = INVALID_CACHE_ENTRY;
        gWorldmapTownListScrollUpFrmHandle[index] = INVALID_CACHE_ENTRY;
        gWorldmapTownListScrollUpFrmData[index] = NULL;
        gWorldmapTownListScrollDownFrmData[index] = NULL;
    }

    gWorldmapTownListScrollUpFrmHeight = 0;
    gWorldmapTownListScrollDownFrmWidth = 0;
    gWorldmapTownListScrollDownFrmHeight = 0;
    gWorldmapMonthsFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapMonthsFrm = NULL;
    gWorldmapNumbersFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapNumbersFrm = NULL;
    gWorldmapTownListScrollUpFrmWidth = 0;

    return 0;
}

// 0x4BCBFC
int _wmGenDataReset()
{
    _Meet_Frank_Horrigan = 0;
    _world_subtile = 0;
    dword_672E18 = 0;
    gWorldmapIsTravelling = false;
    dword_672E28 = 0;
    dword_672E2C = 0;
    dword_672E30 = 0;
    dword_672E34 = 0;
    _x_line_inc = 0;
    _y_line_inc = 0;
    dword_672E44 = 0;
    _wmEncounterIconShow = 0;
    _wmGenData = 0;
    _WorldMapCurrArea = -1;
    _world_xpos = 173;
    _world_ypos = 122;
    gWorldmapTravelDestX = -1;
    gWorldmapTravelDestY = -1;
    _EncounterMapID = -1;
    dword_672E50 = -1;
    dword_672E54 = -1;
    _wmRndCursorFid = -1;
    _carCurrentArea = -1;
    gWorldmapCarFuel = CAR_FUEL_MAX;
    gWorldmapCarFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapTownTabsUnderlayFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapTownTabsEdgeFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapDialFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapCarOverlayFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapGlobeOverlayFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapLittleRedButtonUpFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapLittleRedButtonDownFrmHandle = INVALID_CACHE_ENTRY;
    dword_672E3C = 0;
    _old_world_xpos = 0;
    _old_world_ypos = 0;
    gWorldmapIsInCar = false;
    gWorldmapCarFrmWidth = 0;
    gWorldmapCarFrmHeight = 0;
    gWorldmapCarFrmCurrentFrame = 0;
    gWorldmapTownTabsUnderlayFrmData = NULL;
    gWorldmapTownTabsUnderlayFrmHeight = 0;
    _LastTabsYOffset = 0;
    gWorldmapTownTabsEdgeFrmData = 0;
    gWorldmapDialFrm = NULL;
    gWorldmapDialFrmWidth = 0;
    gWorldmapDialFrmHeight = 0;
    gWorldmapDialFrmCurrentFrame = 0;
    gWorldmapCarOverlayFrmData = NULL;
    gWorldmapCarOverlayFrmWidth = 0;
    gWorldmapCarOverlayFrmHeight = 0;
    gWorldmapGlobeOverlayFrmData = NULL;
    gWorldmapGlobeOverlayFrmWidth = 0;
    gWorldmapGloveOverlayFrmHeight = 0;
    dword_672F54 = 0;
    _tabsOffset = 0;
    gWorldmapLittleRedButtonUpFrmData = NULL;
    gWorldmapLittleRedButtonDownFrmData = NULL;
    gWorldmapTownTabsUnderlayFrmWidth = 0;
    gWorldmapCarFrm = NULL;

    for (int index = 0; index < WORLDMAP_ARROW_FRM_COUNT; index++) {
        gWorldmapTownListScrollUpFrmData[index] = NULL;
        gWorldmapTownListScrollDownFrmHandle[index] = INVALID_CACHE_ENTRY;

        gWorldmapTownListScrollDownFrmData[index] = NULL;
        gWorldmapTownListScrollUpFrmHandle[index] = INVALID_CACHE_ENTRY;
    }

    gWorldmapMonthsFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapNumbersFrmHandle = INVALID_CACHE_ENTRY;
    gWorldmapTownListScrollUpFrmWidth = 0;
    gWorldmapTownListScrollUpFrmHeight = 0;
    gWorldmapTownListScrollDownFrmWidth = 0;
    gWorldmapTownListScrollDownFrmHeight = 0;
    gWorldmapMonthsFrm = NULL;
    gWorldmapNumbersFrm = NULL;

    _wmMarkSubTileRadiusVisited(_world_xpos, _world_ypos);

    return 0;
}

// 0x4BCE00
void worldmapExit()
{
    if (gTerrains != NULL) {
        internal_free(gTerrains);
        gTerrains = NULL;
    }

    if (gWorldmapTiles) {
        internal_free(gWorldmapTiles);
        gWorldmapTiles = NULL;
    }

    gWorldmapGridWidth = 0;
    gWorldmapTilesLength = 0;

    if (gEncounterTables != NULL) {
        internal_free(gEncounterTables);
        gEncounterTables = NULL;
    }

    gEncounterTablesLength = 0;

    if (_wmEncBaseTypeList != NULL) {
        internal_free(_wmEncBaseTypeList);
        _wmEncBaseTypeList = NULL;
    }

    _wmMaxEncBaseTypes = 0;

    if (gCities != NULL) {
        internal_free(gCities);
        gCities = NULL;
    }

    gCitiesLength = 0;

    if (gMaps != NULL) {
        internal_free(gMaps);
    }

    gMapsLength = 0;

    if (_circleBlendTable != NULL) {
        _freeColorBlendTable(_colorTable[992]);
        _circleBlendTable = NULL;
    }

    messageListFree(&gWorldmapMessageList);
}

// 0x4BCEF8
int worldmapReset()
{
    gWorldmapOffsetX = 0;
    gWorldmapOffsetY = 0;

    _wmWorldMapLoadTempData();
    _wmMarkAllSubTiles(0);

    return _wmGenDataReset();
}

// 0x4BCF28
int worldmapSave(File* stream)
{
    int i;
    int j;
    int k;
    EncounterTable* encounter_table;
    EncounterEntry* encounter_entry;

    if (fileWriteInt32(stream, _Meet_Frank_Horrigan) == -1) return -1;
    if (fileWriteInt32(stream, _WorldMapCurrArea) == -1) return -1;
    if (fileWriteInt32(stream, _world_xpos) == -1) return -1;
    if (fileWriteInt32(stream, _world_ypos) == -1) return -1;
    if (fileWriteInt32(stream, _wmEncounterIconShow) == -1) return -1;
    if (fileWriteInt32(stream, _EncounterMapID) == -1) return -1;
    if (fileWriteInt32(stream, dword_672E50) == -1) return -1;
    if (fileWriteInt32(stream, dword_672E54) == -1) return -1;
    if (fileWriteBool(stream, gWorldmapIsInCar) == -1) return -1;
    if (fileWriteInt32(stream, _carCurrentArea) == -1) return -1;
    if (fileWriteInt32(stream, gWorldmapCarFuel) == -1) return -1;
    if (fileWriteInt32(stream, gCitiesLength) == -1) return -1;

    for (int cityIndex = 0; cityIndex < gCitiesLength; cityIndex++) {
        CityInfo* cityInfo = &(gCities[cityIndex]);
        if (fileWriteInt32(stream, cityInfo->x) == -1) return -1;
        if (fileWriteInt32(stream, cityInfo->y) == -1) return -1;
        if (fileWriteInt32(stream, cityInfo->state) == -1) return -1;
        if (fileWriteInt32(stream, cityInfo->field_40) == -1) return -1;
        if (fileWriteInt32(stream, cityInfo->entrancesLength) == -1) return -1;

        for (int entranceIndex = 0; entranceIndex < cityInfo->entrancesLength; entranceIndex++) {
            EntranceInfo* entrance = &(cityInfo->entrances[entranceIndex]);
            if (fileWriteInt32(stream, entrance->state) == -1) return -1;
        }
    }

    if (fileWriteInt32(stream, gWorldmapTilesLength) == -1) return -1;
    if (fileWriteInt32(stream, gWorldmapGridWidth) == -1) return -1;

    for (int tileIndex = 0; tileIndex < gWorldmapTilesLength; tileIndex++) {
        TileInfo* tileInfo = &(gWorldmapTiles[tileIndex]);

        for (int column = 0; column < SUBTILE_GRID_HEIGHT; column++) {
            for (int row = 0; row < SUBTILE_GRID_WIDTH; row++) {
                SubtileInfo* subtile = &(tileInfo->subtiles[column][row]);

                if (fileWriteInt32(stream, subtile->state) == -1) return -1;
            }
        }
    }

    k = 0;
    for (i = 0; i < gEncounterTablesLength; i++) {
        encounter_table = &(gEncounterTables[i]);

        for (j = 0; j < encounter_table->entriesLength; j++) {
            encounter_entry = &(encounter_table->entries[j]);

            if (encounter_entry->counter != -1) {
                k++;
            }
        }
    }

    if (fileWriteInt32(stream, k) == -1) return -1;

    for (i = 0; i < gEncounterTablesLength; i++) {
        encounter_table = &(gEncounterTables[i]);

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
int worldmapLoad(File* stream)
{
    int i;
    int j;
    int k;
    int cities_count;
    int v38;
    int v39;
    int v35;
    EncounterTable* encounter_table;
    EncounterEntry* encounter_entry;

    if (fileReadInt32(stream, &(_Meet_Frank_Horrigan)) == -1) return -1;
    if (fileReadInt32(stream, &(_WorldMapCurrArea)) == -1) return -1;
    if (fileReadInt32(stream, &(_world_xpos)) == -1) return -1;
    if (fileReadInt32(stream, &(_world_ypos)) == -1) return -1;
    if (fileReadInt32(stream, &(_wmEncounterIconShow)) == -1) return -1;
    if (fileReadInt32(stream, &(_EncounterMapID)) == -1) return -1;
    if (fileReadInt32(stream, &(dword_672E50)) == -1) return -1;
    if (fileReadInt32(stream, &(dword_672E54)) == -1) return -1;
    if (fileReadBool(stream, &(gWorldmapIsInCar)) == -1) return -1;
    if (fileReadInt32(stream, &(_carCurrentArea)) == -1) return -1;
    if (fileReadInt32(stream, &(gWorldmapCarFuel)) == -1) return -1;
    if (fileReadInt32(stream, &(cities_count)) == -1) return -1;

    for (int cityIndex = 0; cityIndex < cities_count; cityIndex++) {
        CityInfo* city = &(gCities[cityIndex]);

        if (fileReadInt32(stream, &(city->x)) == -1) return -1;
        if (fileReadInt32(stream, &(city->y)) == -1) return -1;
        if (fileReadInt32(stream, &(city->state)) == -1) return -1;
        if (fileReadInt32(stream, &(city->field_40)) == -1) return -1;

        int entranceCount;
        if (fileReadInt32(stream, &(entranceCount)) == -1) {
            return -1;
        }

        for (int entranceIndex = 0; entranceIndex < entranceCount; entranceIndex++) {
            EntranceInfo* entrance = &(city->entrances[entranceIndex]);

            if (fileReadInt32(stream, &(entrance->state)) == -1) {
                return -1;
            }
        }
    }

    if (fileReadInt32(stream, &(v39)) == -1) return -1;
    if (fileReadInt32(stream, &(v38)) == -1) return -1;

    for (int tileIndex = 0; tileIndex < v39; tileIndex++) {
        TileInfo* tile = &(gWorldmapTiles[tileIndex]);

        for (int column = 0; column < SUBTILE_GRID_HEIGHT; column++) {
            for (int row = 0; row < SUBTILE_GRID_WIDTH; row++) {
                SubtileInfo* subtile = &(tile->subtiles[column][row]);

                if (fileReadInt32(stream, &(subtile->state)) == -1) return -1;
            }
        }
    }

    if (fileReadInt32(stream, &(v35)) == -1) return -1;

    for (i = 0; i < v35; i++) {
        if (fileReadInt32(stream, &(j)) == -1) return -1;
        encounter_table = &(gEncounterTables[j]);

        if (fileReadInt32(stream, &(k)) == -1) return -1;
        encounter_entry = &(encounter_table->entries[k]);

        if (fileReadInt32(stream, &(encounter_entry->counter)) == -1) return -1;
    }

    _wmInterfaceCenterOnParty();

    return 0;
}

// 0x4BD678
int _wmWorldMapSaveTempData()
{
    File* stream = fileOpen("worldmap.dat", "wb");
    if (stream == NULL) {
        return -1;
    }

    int rc = 0;
    if (worldmapSave(stream) == -1) {
        rc = -1;
    }

    fileClose(stream);

    return rc;
}

// 0x4BD6B4
int _wmWorldMapLoadTempData()
{
    File* stream = fileOpen("worldmap.dat", "rb");
    if (stream == NULL) {
        return -1;
    }

    int rc = 0;
    if (worldmapLoad(stream) == -1) {
        rc = -1;
    }

    fileClose(stream);

    return rc;
}

// 0x4BD6F0
int worldmapConfigInit()
{
    if (cityInit() == -1) {
        return -1;
    }

    Config config;
    if (!configInit(&config)) {
        return -1;
    }

    if (configRead(&config, "data\\worldmap.txt", true)) {
        for (int index = 0; index < ENCOUNTER_FREQUENCY_TYPE_COUNT; index++) {
            if (!configGetInt(&config, "data", gEncounterFrequencyTypeKeys[index], &(_wmFreqValues[index]))) {
                break;
            }
        }

        char* terrainTypes;
        configGetString(&config, "data", "terrain_types", &terrainTypes);
        _wmParseTerrainTypes(&config, terrainTypes);

        for (int index = 0;; index++) {
            char section[40];
            sprintf(section, "Encounter Table %d", index);

            char* lookupName;
            if (!configGetString(&config, section, "lookup_name", &lookupName)) {
                break;
            }

            if (worldmapConfigLoadEncounterTable(&config, lookupName, section) == -1) {
                return -1;
            }
        }

        if (!configGetInt(&config, "Tile Data", "num_horizontal_tiles", &gWorldmapGridWidth)) {
            showMesageBox("\nwmConfigInit::Error loading tile data!");
            return -1;
        }

        for (int tileIndex = 0; tileIndex < 9999; tileIndex++) {
            char section[40];
            sprintf(section, "Tile %d", tileIndex);

            int artIndex;
            if (!configGetInt(&config, section, "art_idx", &artIndex)) {
                break;
            }

            gWorldmapTilesLength++;

            TileInfo* worldmapTiles = (TileInfo*)internal_realloc(gWorldmapTiles, sizeof(*gWorldmapTiles) * gWorldmapTilesLength);
            if (worldmapTiles == NULL) {
                showMesageBox("\nwmConfigInit::Error loading tiles!");
                exit(1);
            }

            gWorldmapTiles = worldmapTiles;

            TileInfo* tile = &(worldmapTiles[gWorldmapTilesLength - 1]);

            // NOTE: Uninline.
            worldmapTileInfoInit(tile);

            tile->fid = buildFid(6, artIndex, 0, 0, 0);

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
                    sprintf(key, "%d_%d", row, column);

                    char* subtileProps;
                    if (!configGetString(&config, section, key, &subtileProps)) {
                        showMesageBox("\nwmConfigInit::Error loading tiles!");
                        exit(1);
                    }

                    if (worldmapConfigLoadSubtile(tile, row, column, subtileProps) == -1) {
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
int worldmapConfigLoadEncounterTable(Config* config, char* lookupName, char* sectionKey)
{
    gEncounterTablesLength++;

    EncounterTable* encounterTables = (EncounterTable*)internal_realloc(gEncounterTables, sizeof(EncounterTable) * gEncounterTablesLength);
    if (encounterTables == NULL) {
        showMesageBox("\nwmConfigInit::Error loading Encounter Table!");
        exit(1);
    }

    gEncounterTables = encounterTables;

    EncounterTable* encounterTable = &(encounterTables[gEncounterTablesLength - 1]);

    // NOTE: Uninline.
    worldmapEncounterTableInit(encounterTable);

    encounterTable->field_28 = gEncounterTablesLength - 1;
    strncpy(encounterTable->lookupName, lookupName, 40);

    char* str;
    if (configGetString(config, sectionKey, "maps", &str)) {
        while (*str != '\0') {
            if (encounterTable->mapsLength >= 6) {
                break;
            }

            if (strParseStrFromFunc(&str, &(encounterTable->maps[encounterTable->mapsLength]), worldmapFindMapByLookupName) == -1) {
                break;
            }

            encounterTable->mapsLength++;
        }
    }

    for (;;) {
        char key[40];
        sprintf(key, "enc_%02d", encounterTable->entriesLength);

        char* str;
        if (!configGetString(config, sectionKey, key, &str)) {
            break;
        }

        if (encounterTable->entriesLength >= 40) {
            showMesageBox("\nwmConfigInit::Error: Encounter Table: Too many table indexes!!");
            exit(1);
        }

        gWorldmapConfig = config;

        if (worldmapConfigLoadEncounterEntry(&(encounterTable->entries[encounterTable->entriesLength]), str) == -1) {
            return -1;
        }

        encounterTable->entriesLength++;
    }

    return 0;
}

// 0x4BDB64
int worldmapConfigLoadEncounterEntry(EncounterEntry* entry, char* string)
{
    // NOTE: Uninline.
    if (worldmapEncounterTableEntryInit(entry) == -1) {
        return -1;
    }

    while (string != NULL && *string != '\0') {
        strParseIntWithKey(&string, "chance", &(entry->chance), ":");
        strParseIntWithKey(&string, "counter", &(entry->counter), ":");

        if (strstr(string, "special")) {
            entry->flags |= ENCOUNTER_ENTRY_SPECIAL;
            string += 8;
        }

        if (string != NULL) {
            char* pch = strstr(string, "map:");
            if (pch != NULL) {
                string = pch + 4;
                strParseStrFromFunc(&string, &(entry->map), worldmapFindMapByLookupName);
            }
        }

        if (_wmParseEncounterSubEncStr(entry, &string) == -1) {
            break;
        }

        if (string != NULL) {
            char* pch = strstr(string, "scenery:");
            if (pch != NULL) {
                string = pch + 8;
                strParseStrFromList(&string, &(entry->scenery), _wmSceneryStrs, ENCOUNTER_SCENERY_TYPE_COUNT);
            }
        }

        worldmapConfigParseCondition(&string, "if", &(entry->condition));
    }

    return 0;
}

// 0x4BDCA8
int _wmParseEncounterSubEncStr(EncounterEntry* encounterEntry, char** stringPtr)
{
    char* string = *stringPtr;
    if (strnicmp(string, "enc:", 4) != 0) {
        return -1;
    }

    // Consume "enc:".
    string += 4;

    char* comma = strstr(string, ",");
    if (comma != NULL) {
        // Comma is present, position string pointer to the next chunk.
        *stringPtr = comma + 1;
        *comma = '\0';
    } else {
        // No comma, this chunk is the last one.
        *stringPtr = NULL;
    }

    while (string != NULL) {
        ENCOUNTER_ENTRY_ENC* entry = &(encounterEntry->field_54[encounterEntry->field_50]);

        // NOTE: Uninline.
        _wmEncounterSubEncSlotInit(entry);

        if (*string == '(') {
            string++;
            entry->minQuantity = atoi(string);

            while (*string != '\0' && *string != '-') {
                string++;
            }

            if (*string == '-') {
                string++;
            }

            entry->maxQuantity = atoi(string);

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

        if (strParseStrFromFunc(&string, &(entry->field_8), _wmParseFindSubEncTypeMatch) == -1) {
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
            strParseStrFromList(&string, &(entry->situation), _wmEncOpStrs, ENCOUNTER_SITUATION_COUNT);
        }

        *end = ch;

        encounterEntry->field_50++;

        while (*string == ' ') {
            string++;
        }

        if (*string == '\0') {
            string = NULL;
        }
    }

    if (comma != NULL) {
        *comma = ',';
    }

    return 0;
}

// 0x4BDE94
int _wmParseFindSubEncTypeMatch(char* str, int* valuePtr)
{
    *valuePtr = 0;

    if (stricmp(str, "player") == 0) {
        *valuePtr = -1;
        return 0;
    }

    if (_wmFindEncBaseTypeMatch(str, valuePtr) == 0) {
        return 0;
    }

    if (_wmReadEncBaseType(str, valuePtr) == 0) {
        return 0;
    }

    return -1;
}

// 0x4BDED8
int _wmFindEncBaseTypeMatch(char* str, int* valuePtr)
{
    for (int index = 0; index < _wmMaxEncBaseTypes; index++) {
        if (stricmp(_wmEncBaseTypeList[index].name, str) == 0) {
            *valuePtr = index;
            return 0;
        }
    }

    *valuePtr = -1;
    return -1;
}

// 0x4BDF34
int _wmReadEncBaseType(char* name, int* valuePtr)
{
    char section[40];
    sprintf(section, "Encounter: %s", name);

    char key[40];
    sprintf(key, "type_00");

    char* string;
    if (!configGetString(gWorldmapConfig, section, key, &string)) {
        return -1;
    }

    _wmMaxEncBaseTypes++;

    ENC_BASE_TYPE* arr = (ENC_BASE_TYPE*)internal_realloc(_wmEncBaseTypeList, sizeof(*_wmEncBaseTypeList) * _wmMaxEncBaseTypes);
    if (arr == NULL) {
        showMesageBox("\nwmConfigInit::Error Reading EncBaseType!");
        exit(1);
    }

    _wmEncBaseTypeList = arr;

    ENC_BASE_TYPE* entry = &(arr[_wmMaxEncBaseTypes - 1]);

    // NOTE: Uninline.
    _wmEncBaseTypeSlotInit(entry);

    strncpy(entry->name, name, 40);

    while (1) {
        if (_wmParseEncBaseSubTypeStr(&(entry->field_38[entry->field_34]), &string) == -1) {
            return -1;
        }

        entry->field_34++;

        sprintf(key, "type_%02d", entry->field_34);

        if (!configGetString(gWorldmapConfig, section, key, &string)) {
            int team;
            configGetInt(gWorldmapConfig, section, "team_num", &team);

            for (int index = 0; index < entry->field_34; index++) {
                ENC_BASE_TYPE_38* ptr = &(entry->field_38[index]);
                if ((ptr->pid >> 24) == OBJ_TYPE_CRITTER) {
                    ptr->team = team;
                }
            }

            if (configGetString(gWorldmapConfig, section, "position", &string)) {
                strParseStrFromList(&string, &(entry->position), gEncounterFormationTypeKeys, ENCOUNTER_FORMATION_TYPE_COUNT);
                strParseIntWithKey(&string, "spacing", &(entry->spacing), ":");
                strParseIntWithKey(&string, "distance", &(entry->distance), ":");
            }

            *valuePtr = _wmMaxEncBaseTypes - 1;

            return 0;
        }
    }

    return -1;
}

// 0x4BE140
int _wmParseEncBaseSubTypeStr(ENC_BASE_TYPE_38* ptr, char** stringPtr)
{
    char* string = *stringPtr;

    // NOTE: Uninline.
    if (_wmEncBaseSubTypeSlotInit(ptr) == -1) {
        return -1;
    }

    if (strParseIntWithKey(&string, "ratio", &(ptr->ratio), ":") == 0) {
        ptr->field_2C = 0;
    }

    if (strstr(string, "dead,") == string) {
        ptr->flags |= ENCOUNTER_SUBINFO_DEAD;
        string += 5;
    }

    strParseIntWithKey(&string, "pid", &(ptr->pid), ":");
    if (ptr->pid == 0) {
        ptr->pid = -1;
    }

    strParseIntWithKey(&string, "distance", &(ptr->distance), ":");
    strParseIntWithKey(&string, "tilenum", &(ptr->tile), ":");

    for (int index = 0; index < 10; index++) {
        if (strstr(string, "item:") == NULL) {
            break;
        }

        _wmParseEncounterItemType(&string, &(ptr->items[ptr->itemsLength]), &(ptr->itemsLength), ":");
    }

    strParseIntWithKey(&string, "script", &(ptr->script), ":");
    worldmapConfigParseCondition(&string, "if", &(ptr->condition));

    return 0;
}

// NOTE: Inlined.
//
// 0x4BE2A0
int _wmEncBaseTypeSlotInit(ENC_BASE_TYPE* entry)
{
    entry->name[0] = '\0';
    entry->position = ENCOUNTER_FORMATION_TYPE_SURROUNDING;
    entry->spacing = 1;
    entry->distance = -1;
    entry->field_34 = 0;

    return 0;
}

// NOTE: Inlined.
//
// 0x4BE2C4
int _wmEncBaseSubTypeSlotInit(ENC_BASE_TYPE_38* entry)
{
    entry->field_28 = -1;
    entry->field_2C = 1;
    entry->ratio = 100;
    entry->pid = -1;
    entry->flags = 0;
    entry->distance = 0;
    entry->tile = -1;
    entry->itemsLength = 0;
    entry->script = -1;
    entry->team = -1;

    return worldmapConfigInitEncounterCondition(&(entry->condition));
}

// NOTE: Inlined.
//
// 0x4BE32C
int _wmEncounterSubEncSlotInit(ENCOUNTER_ENTRY_ENC* entry)
{
    entry->minQuantity = 1;
    entry->maxQuantity = 1;
    entry->field_8 = -1;
    entry->situation = ENCOUNTER_SITUATION_NOTHING;

    return 0;
}

// NOTE: Inlined.
//
// 0x4BE34C
int worldmapEncounterTableEntryInit(EncounterEntry* entry)
{
    entry->flags = 0;
    entry->map = -1;
    entry->scenery = ENCOUNTER_SCENERY_TYPE_NORMAL;
    entry->chance = 0;
    entry->counter = -1;
    entry->field_50 = 0;

    return worldmapConfigInitEncounterCondition(&(entry->condition));
}

// NOTE: Inlined.
//
// 0x4BE3B8
int worldmapEncounterTableInit(EncounterTable* encounterTable)
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
int worldmapTileInfoInit(TileInfo* tile)
{
    tile->fid = -1;
    tile->handle = INVALID_CACHE_ENTRY;
    tile->data = NULL;
    tile->walkMaskName[0] = '\0';
    tile->walkMaskData = NULL;
    tile->encounterDifficultyModifier = 0;

    return 0;
}

// NOTE: Inlined.
//
// 0x4BE400
int worldmapTerrainInfoInit(Terrain* terrain)
{
    terrain->field_0[0] = '\0';
    terrain->field_28 = 0;
    terrain->mapsLength = 0;

    return 0;
}

// 0x4BE378
int worldmapConfigInitEncounterCondition(EncounterCondition* condition)
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
int _wmParseTerrainTypes(Config* config, char* string)
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

    gTerrainsLength = terrainCount;

    gTerrains = (Terrain*)internal_malloc(sizeof(*gTerrains) * terrainCount);
    if (gTerrains == NULL) {
        return -1;
    }

    for (int index = 0; index < gTerrainsLength; index++) {
        Terrain* terrain = &(gTerrains[index]);

        // NOTE: Uninline.
        worldmapTerrainInfoInit(terrain);
    }

    strlwr(string);

    pch = string;
    for (int index = 0; index < gTerrainsLength; index++) {
        Terrain* terrain = &(gTerrains[index]);

        pch += strspn(pch, " ");

        int endPos = strcspn(pch, ",");
        char end = pch[endPos];
        pch[endPos] = '\0';

        int delimeterPos = strcspn(pch, ":");
        char delimeter = pch[delimeterPos];
        pch[delimeterPos] = '\0';

        strncpy(terrain->field_0, pch, 40);
        terrain->field_28 = atoi(pch + delimeterPos + 1);

        pch[delimeterPos] = delimeter;
        pch[endPos] = end;

        if (end == ',') {
            pch += endPos + 1;
        }
    }

    for (int index = 0; index < gTerrainsLength; index++) {
        _wmParseTerrainRndMaps(config, &(gTerrains[index]));
    }

    return 0;
}

// 0x4BE598
int _wmParseTerrainRndMaps(Config* config, Terrain* terrain)
{
    char section[40];
    sprintf(section, "Random Maps: %s", terrain->field_0);

    for (;;) {
        char key[40];
        sprintf(key, "map_%02d", terrain->mapsLength);

        char* string;
        if (!configGetString(config, section, key, &string)) {
            break;
        }

        if (strParseStrFromFunc(&string, &(terrain->maps[terrain->mapsLength]), worldmapFindMapByLookupName) == -1) {
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
int worldmapConfigLoadSubtile(TileInfo* tile, int row, int column, char* string)
{
    SubtileInfo* subtile = &(tile->subtiles[column][row]);
    subtile->state = SUBTILE_STATE_UNKNOWN;

    if (strParseStrFromFunc(&string, &(subtile->terrain), worldmapFindTerrainByLookupName) == -1) {
        return -1;
    }

    if (strParseStrFromList(&string, &(subtile->field_4), _wmFillStrs, 9) == -1) {
        return -1;
    }

    for (int index = 0; index < DAY_PART_COUNT; index++) {
        if (strParseStrFromList(&string, &(subtile->encounterChance[index]), gEncounterFrequencyTypeKeys, ENCOUNTER_FREQUENCY_TYPE_COUNT) == -1) {
            return -1;
        }
    }

    if (strParseStrFromFunc(&string, &(subtile->encounterType), worldmapFindEncounterTableByLookupName) == -1) {
        return -1;
    }

    return 0;
}

// 0x4BE6D4
int worldmapFindEncounterTableByLookupName(char* string, int* valuePtr)
{
    for (int index = 0; index < gEncounterTablesLength; index++) {
        if (stricmp(string, gEncounterTables[index].lookupName) == 0) {
            *valuePtr = index;
            return 0;
        }
    }

    debugPrint("WorldMap Error: Couldn't find match for Encounter Type!");

    *valuePtr = -1;

    return -1;
}

// 0x4BE73C
int worldmapFindTerrainByLookupName(char* string, int* valuePtr)
{
    for (int index = 0; index < gTerrainsLength; index++) {
        Terrain* terrain = &(gTerrains[index]);
        if (stricmp(string, terrain->field_0) == 0) {
            *valuePtr = index;
            return 0;
        }
    }

    debugPrint("WorldMap Error: Couldn't find match for Terrain Type!");

    *valuePtr = -1;

    return -1;
}

// 0x4BE7A4
int _wmParseEncounterItemType(char** stringPtr, ENC_BASE_TYPE_38_48* a2, int* a3, const char* delim)
{
    char* string;
    int v2, v3;
    char tmp, tmp2;
    int v20;

    string = *stringPtr;
    v20 = 0;

    if (*string == '\0') {
        return -1;
    }

    strlwr(string);

    if (*string == ',') {
        string++;
        *stringPtr += 1;
    }

    string += strspn(string, " ");

    v2 = strcspn(string, ",");

    tmp = string[v2];
    string[v2] = '\0';

    v3 = strcspn(string, delim);
    tmp2 = string[v3];
    string[v3] = '\0';

    if (strcmp(string, "item") == 0) {
        *stringPtr += v2 + 1;
        v20 = 1;
        _wmParseItemType(string + v3 + 1, a2);
        *a3 = *a3 + 1;
    }

    string[v3] = tmp2;
    string[v2] = tmp;

    return v20 ? 0 : -1;
}

// 0x4BE888
int _wmParseItemType(char* string, ENC_BASE_TYPE_38_48* ptr)
{
    while (*string == ' ') {
        string++;
    }

    ptr->minimumQuantity = 1;
    ptr->maximumQuantity = 1;
    ptr->isEquipped = false;

    if (*string == '(') {
        string++;

        ptr->minimumQuantity = atoi(string);

        while (isdigit(*string)) {
            string++;
        }

        if (*string == '-') {
            string++;

            ptr->maximumQuantity = atoi(string);

            while (isdigit(*string)) {
                string++;
            }
        } else {
            ptr->maximumQuantity = ptr->minimumQuantity;
        }

        if (*string == ')') {
            string++;
        }
    }

    while (*string == ' ') {
        string++;
    }

    ptr->pid = atoi(string);

    while (isdigit(*string)) {
        string++;
    }

    while (*string == ' ') {
        string++;
    }

    if (strstr(string, "{wielded}") != NULL
        || strstr(string, "(wielded)") != NULL
        || strstr(string, "{worn}") != NULL
        || strstr(string, "(worn)") != NULL) {
        ptr->isEquipped = true;
    }

    return 0;
}

// 0x4BE988
int worldmapConfigParseCondition(char** stringPtr, const char* a2, EncounterCondition* condition)
{
    while (condition->entriesLength < 3) {
        EncounterConditionEntry* conditionEntry = &(condition->entries[condition->entriesLength]);
        if (worldmapConfigParseConditionEntry(stringPtr, a2, &(conditionEntry->type), &(conditionEntry->conditionalOperator), &(conditionEntry->param), &(conditionEntry->value)) == -1) {
            return -1;
        }

        condition->entriesLength++;

        char* andStatement = strstr(*stringPtr, "and");
        if (andStatement != NULL) {
            *stringPtr = andStatement + 3;
            condition->logicalOperators[condition->entriesLength - 1] = ENCOUNTER_LOGICAL_OPERATOR_AND;
            continue;
        }

        char* orStatement = strstr(*stringPtr, "or");
        if (orStatement != NULL) {
            *stringPtr = orStatement + 2;
            condition->logicalOperators[condition->entriesLength - 1] = ENCOUNTER_LOGICAL_OPERATOR_OR;
            continue;
        }

        break;
    }

    return 0;
}

// 0x4BEA24
int worldmapConfigParseConditionEntry(char** stringPtr, const char* a2, int* typePtr, int* operatorPtr, int* paramPtr, int* valuePtr)
{
    char* pch;
    int v2;
    int v3;
    char tmp;
    char tmp2;
    int v57;

    char* string = *stringPtr;

    if (string == NULL) {
        return -1;
    }

    if (*string == '\0') {
        return -1;
    }

    strlwr(string);

    if (*string == ',') {
        string++;
        *stringPtr = string;
    }

    string += strspn(string, " ");

    v2 = strcspn(string, ",");

    tmp = *(string + v2);
    *(string + v2) = '\0';

    v3 = strcspn(string, "(");
    tmp2 = *(string + v3);
    *(string + v3) = '\0';

    v57 = 0;
    if (strstr(string, a2) == string) {
        v57 = 1;
    }

    *(string + v3) = tmp2;
    *(string + v2) = tmp;

    if (v57 == 0) {
        return -1;
    }

    string += v3 + 1;

    if (strstr(string, "rand(") == string) {
        string += 5;
        *typePtr = ENCOUNTER_CONDITION_TYPE_RANDOM;
        *operatorPtr = ENCOUNTER_CONDITIONAL_OPERATOR_NONE;
        *paramPtr = atoi(string);

        pch = strstr(string, ")");
        if (pch != NULL) {
            string = pch + 1;
        }

        pch = strstr(string, ")");
        if (pch != NULL) {
            string = pch + 1;
        }

        pch = strstr(string, ",");
        if (pch != NULL) {
            string = pch + 1;
        }

        *stringPtr = string;
        return 0;
    } else if (strstr(string, "global(") == string) {
        string += 7;
        *typePtr = ENCOUNTER_CONDITION_TYPE_GLOBAL;
        *paramPtr = atoi(string);

        pch = strstr(string, ")");
        if (pch != NULL) {
            string = pch + 1;
        }

        while (*string == ' ') {
            string++;
        }

        if (worldmapConfigParseEncounterConditionalOperator(&string, operatorPtr) != -1) {
            *valuePtr = atoi(string);

            pch = strstr(string, ")");
            if (pch != NULL) {
                string = pch + 1;
            }

            pch = strstr(string, ",");
            if (pch != NULL) {
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

        if (worldmapConfigParseEncounterConditionalOperator(&string, operatorPtr) != -1) {
            *valuePtr = atoi(string);

            pch = strstr(string, ")");
            if (pch != NULL) {
                string = pch + 1;
            }

            pch = strstr(string, ",");
            if (pch != NULL) {
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

        if (worldmapConfigParseEncounterConditionalOperator(&string, operatorPtr) != -1) {
            *valuePtr = atoi(string);

            pch = strstr(string, ")");
            if (pch != NULL) {
                string = pch + 1;
            }

            pch = strstr(string, ",");
            if (pch != NULL) {
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

        if (worldmapConfigParseEncounterConditionalOperator(&string, operatorPtr) != -1) {
            *valuePtr = atoi(string);

            pch = strstr(string, ")");
            if (pch != NULL) {
                string = pch + 1;
            }

            pch = strstr(string, ",");
            if (pch != NULL) {
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

        if (worldmapConfigParseEncounterConditionalOperator(&string, operatorPtr) != -1) {
            *valuePtr = atoi(string);

            pch = strstr(string, ")");
            if (pch != NULL) {
                string = pch + 1;
            }

            pch = strstr(string, ",");
            if (pch != NULL) {
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
int worldmapConfigParseEncounterConditionalOperator(char** stringPtr, int* conditionalOperatorPtr)
{
    char* string = *stringPtr;

    *conditionalOperatorPtr = ENCOUNTER_CONDITIONAL_OPERATOR_NONE;

    int index;
    for (index = 0; index < ENCOUNTER_CONDITIONAL_OPERATOR_COUNT; index++) {
        if (strstr(string, _wmConditionalOpStrs[index]) == string) {
            break;
        }
    }

    if (index == ENCOUNTER_CONDITIONAL_OPERATOR_COUNT) {
        return -1;
    }

    *conditionalOperatorPtr = index;

    string += strlen(_wmConditionalOpStrs[index]);
    while (*string == ' ') {
        string++;
    }

    *stringPtr = string;

    return 0;
}

// NOTE: Inlined.
//
// 0x4BEF1C
int worldmapCityInfoInit(CityInfo* area)
{
    area->name[0] = '\0';
    area->field_28 = -1;
    area->x = 0;
    area->y = 0;
    area->size = CITY_SIZE_LARGE;
    area->state = 0;
    area->field_3C = 0;
    area->field_40 = 0;
    area->mapFid = -1;
    area->labelFid = -1;
    area->entrancesLength = 0;

    return 0;
}

// 0x4BEF68
int cityInit()
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

    if (_wmMapInit() == -1) {
        return -1;
    }

    if (configInit(&cfg) == -1) {
        return -1;
    }

    if (configRead(&cfg, "data\\city.txt", true)) {
        area_idx = 0;
        do {
            sprintf(section, "Area %02d", area_idx);
            if (!configGetInt(&cfg, section, "townmap_art_idx", &num)) {
                break;
            }

            gCitiesLength++;

            cities = (CityInfo*)internal_realloc(gCities, sizeof(CityInfo) * gCitiesLength);
            if (cities == NULL) {
                showMesageBox("\nwmConfigInit::Error loading areas!");
                exit(1);
            }

            gCities = cities;

            city = &(cities[gCitiesLength - 1]);

            // NOTE: Uninline.
            worldmapCityInfoInit(city);

            city->field_28 = area_idx;

            if (num != -1) {
                num = buildFid(6, num, 0, 0, 0);
            }

            city->mapFid = num;

            if (configGetInt(&cfg, section, "townmap_label_art_idx", &num)) {
                if (num != -1) {
                    num = buildFid(6, num, 0, 0, 0);
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

            if (strParseStrFromList(&str, &(city->state), _wmStateStrs, 2) == -1) {
                return -1;
            }

            if (configGetString(&cfg, section, "lock_state", &str)) {
                if (strParseStrFromList(&str, &(city->field_3C), _wmStateStrs, 2) == -1) {
                    return -1;
                }
            }

            if (!configGetString(&cfg, section, "size", &str)) {
                showMesageBox("\nwmConfigInit::Error loading areas!");
                exit(1);
            }

            if (strParseStrFromList(&str, &(city->size), gCitySizeKeys, 3) == -1) {
                return -1;
            }

            while (city->entrancesLength < ENTRANCE_LIST_CAPACITY) {
                sprintf(key, "entrance_%d", city->entrancesLength);

                if (!configGetString(&cfg, section, key, &str)) {
                    break;
                }

                entrance = &(city->entrances[city->entrancesLength]);

                // NOTE: Uninline.
                worldmapCityEntranceInfoInit(entrance);

                if (strParseStrFromList(&str, &(entrance->state), _wmStateStrs, 2) == -1) {
                    return -1;
                }

                if (strParseInt(&str, &(entrance->x)) == -1) {
                    return -1;
                }

                if (strParseInt(&str, &(entrance->y)) == -1) {
                    return -1;
                }

                if (strParseStrFromFunc(&str, &(entrance->map), &worldmapFindMapByLookupName) == -1) {
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

    if (gCitiesLength != CITY_COUNT) {
        showMesageBox("\nwmAreaInit::Error loading Cities!");
        exit(1);
    }

    return 0;
}

// 0x4BF3E0
int worldmapFindMapByLookupName(char* string, int* valuePtr)
{
    for (int index = 0; index < gMapsLength; index++) {
        MapInfo* map = &(gMaps[index]);
        if (stricmp(string, map->lookupName) == 0) {
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
int worldmapCityEntranceInfoInit(EntranceInfo* entrance)
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
int worldmapMapInfoInit(MapInfo* map)
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
int _wmMapInit()
{
    char* str;
    int num;
    MapInfo* maps;
    MapInfo* map;
    int j;
    MapAmbientSoundEffectInfo* sfx;
    MapStartPointInfo* rsp;

    Config config;
    if (!configInit(&config)) {
        return -1;
    }

    if (configRead(&config, "data\\maps.txt", true)) {
        for (int mapIndex = 0;; mapIndex++) {
            char section[40];
            sprintf(section, "Map %03d", mapIndex);

            if (!configGetString(&config, section, "lookup_name", &str)) {
                break;
            }

            gMapsLength++;

            maps = (MapInfo*)internal_realloc(gMaps, sizeof(*gMaps) * gMapsLength);
            if (maps == NULL) {
                showMesageBox("\nwmConfigInit::Error loading maps!");
                exit(1);
            }

            gMaps = maps;

            map = &(maps[gMapsLength - 1]);
            worldmapMapInfoInit(map);

            strncpy(map->lookupName, str, 40);

            if (!configGetString(&config, section, "map_name", &str)) {
                showMesageBox("\nwmConfigInit::Error loading maps!");
                exit(1);
            }

            strlwr(str);
            strncpy(map->mapFileName, str, 40);

            if (configGetString(&config, section, "music", &str)) {
                strncpy(map->music, str, 40);
            }

            if (configGetString(&config, section, "ambient_sfx", &str)) {
                while (str) {
                    sfx = &(map->ambientSoundEffects[map->ambientSoundEffectsLength]);
                    if (strParseKeyValue(&str, sfx->name, &(sfx->chance), ":") == -1) {
                        return -1;
                    }

                    map->ambientSoundEffectsLength++;

                    if (*str == '\0') {
                        str = NULL;
                    }

                    if (map->ambientSoundEffectsLength >= MAP_AMBIENT_SOUND_EFFECTS_CAPACITY) {
                        if (str != NULL) {
                            debugPrint("\nwmMapInit::Error reading ambient sfx.  Too many!  Str: %s, MapIdx: %d", map->lookupName, mapIndex);
                            str = NULL;
                        }
                    }
                }
            }

            if (configGetString(&config, section, "saved", &str)) {
                if (strParseStrFromList(&str, &num, _wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                if (num) {
                    map->flags |= MAP_SAVED;
                } else {
                    map->flags &= ~MAP_SAVED;
                }
            }

            if (configGetString(&config, section, "dead_bodies_age", &str)) {
                if (strParseStrFromList(&str, &num, _wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                if (num) {
                    map->flags |= MAP_DEAD_BODIES_AGE;
                } else {
                    map->flags &= ~MAP_DEAD_BODIES_AGE;
                }
            }

            if (configGetString(&config, section, "can_rest_here", &str)) {
                if (strParseStrFromList(&str, &num, _wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                if (num) {
                    map->flags |= MAP_CAN_REST_ELEVATION_0;
                } else {
                    map->flags &= ~MAP_CAN_REST_ELEVATION_0;
                }

                if (strParseStrFromList(&str, &num, _wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                if (num) {
                    map->flags |= MAP_CAN_REST_ELEVATION_1;
                } else {
                    map->flags &= ~MAP_CAN_REST_ELEVATION_1;
                }

                if (strParseStrFromList(&str, &num, _wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                if (num) {
                    map->flags |= MAP_CAN_REST_ELEVATION_2;
                } else {
                    map->flags &= ~MAP_CAN_REST_ELEVATION_2;
                }
            }

            if (configGetString(&config, section, "pipbody_active", &str)) {
                if (strParseStrFromList(&str, &num, _wmYesNoStrs, 2) == -1) {
                    return -1;
                }

                if (num) {
                    map->flags |= MAP_PIPBOY_ACTIVE;
                } else {
                    map->flags &= ~MAP_PIPBOY_ACTIVE;
                }
            }

            if (configGetString(&config, section, "random_start_point_0", &str)) {
                j = 0;
                while (str != NULL) {
                    while (*str != '\0') {
                        if (map->startPointsLength >= MAP_STARTING_POINTS_CAPACITY) {
                            break;
                        }

                        rsp = &(map->startPoints[map->startPointsLength]);

                        // NOTE: Uninline.
                        worldmapRandomStartingPointInit(rsp);

                        strParseIntWithKey(&str, "elev", &(rsp->elevation), ":");
                        strParseIntWithKey(&str, "tile_num", &(rsp->tile), ":");

                        map->startPointsLength++;
                    }

                    char key[40];
                    sprintf(key, "random_start_point_%1d", ++j);

                    if (!configGetString(&config, section, key, &str)) {
                        str = NULL;
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
int worldmapRandomStartingPointInit(MapStartPointInfo* rsp)
{
    rsp->elevation = 0;
    rsp->tile = -1;
    rsp->field_8 = -1;

    return 0;
}

// 0x4BF96C
int mapGetCount()
{
    return gMapsLength;
}

// 0x4BF974
int mapGetFileName(int map, char* dest)
{
    if (map == -1 || map > gMapsLength) {
        dest[0] = '\0';
        return -1;
    }

    sprintf(dest, "%s.MAP", gMaps[map].mapFileName);
    return 0;
}

// 0x4BF9BC
int mapGetIndexByFileName(char* name)
{
    strlwr(name);

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

    for (int index = 0; index < gMapsLength; index++) {
        if (strcmp(gMaps[index].mapFileName, name) == 0) {
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
bool _wmMapIdxIsSaveable(int map_index)
{
    return (gMaps[map_index].flags & MAP_SAVED) != 0;
}

// 0x4BFA64
bool _wmMapIsSaveable()
{
    return (gMaps[gMapHeader.field_34].flags & MAP_SAVED) != 0;
}

// 0x4BFA90
bool _wmMapDeadBodiesAge()
{
    return (gMaps[gMapHeader.field_34].flags & MAP_DEAD_BODIES_AGE) != 0;
}

// 0x4BFABC
bool _wmMapCanRestHere(int elevation)
{
    int flags[3];

    // NOTE: I'm not sure why they're copied.
    memcpy(flags, _can_rest_here, sizeof(flags));

    MapInfo* map = &(gMaps[gMapHeader.field_34]);

    return (map->flags & flags[elevation]) != 0;
}

// 0x4BFAFC
bool _wmMapPipboyActive()
{
    return gameMovieIsSeen(MOVIE_VSUIT);
}

// 0x4BFB08
int _wmMapMarkVisited(int mapIndex)
{
    if (mapIndex < 0 || mapIndex >= gMapsLength) {
        return -1;
    }

    MapInfo* map = &(gMaps[mapIndex]);
    if ((map->flags & MAP_SAVED) == 0) {
        return 0;
    }

    int cityIndex;
    if (_wmMatchAreaContainingMapIdx(mapIndex, &cityIndex) == -1) {
        return -1;
    }

    _wmAreaMarkVisitedState(cityIndex, 2);

    return 0;
}

// 0x4BFB64
int _wmMatchEntranceFromMap(int cityIndex, int mapIndex, int* entranceIndexPtr)
{
    CityInfo* city = &(gCities[cityIndex]);

    for (int entranceIndex = 0; entranceIndex < city->entrancesLength; entranceIndex++) {
        EntranceInfo* entrance = &(city->entrances[entranceIndex]);

        if (mapIndex == entrance->map) {
            *entranceIndexPtr = entranceIndex;
            return 0;
        }
    }

    *entranceIndexPtr = -1;
    return -1;
}

// 0x4BFBE8
int _wmMatchEntranceElevFromMap(int cityIndex, int map, int elevation, int* entranceIndexPtr)
{
    CityInfo* city = &(gCities[cityIndex]);

    for (int entranceIndex = 0; entranceIndex < city->entrancesLength; entranceIndex++) {
        EntranceInfo* entrance = &(city->entrances[entranceIndex]);
        if (entrance->map == map) {
            if (elevation == -1 || entrance->elevation == -1 || elevation == entrance->elevation) {
                *entranceIndexPtr = entranceIndex;
                return 0;
            }
        }
    }

    *entranceIndexPtr = -1;
    return -1;
}

// 0x4BFC7C
int _wmMatchAreaFromMap(int mapIndex, int* cityIndexPtr)
{
    for (int cityIndex = 0; cityIndex < gCitiesLength; cityIndex++) {
        CityInfo* city = &(gCities[cityIndex]);

        for (int entranceIndex = 0; entranceIndex < city->entrancesLength; entranceIndex++) {
            EntranceInfo* entrance = &(city->entrances[entranceIndex]);
            if (mapIndex == entrance->map) {
                *cityIndexPtr = cityIndex;
                return 0;
            }
        }
    }

    *cityIndexPtr = -1;
    return -1;
}

// Mark map entrance.
//
// 0x4BFD50
int _wmMapMarkMapEntranceState(int mapIndex, int elevation, int state)
{
    if (mapIndex < 0 || mapIndex >= gMapsLength) {
        return -1;
    }

    MapInfo* map = &(gMaps[mapIndex]);
    if ((map->flags & MAP_SAVED) == 0) {
        return -1;
    }

    int cityIndex;
    if (_wmMatchAreaContainingMapIdx(mapIndex, &cityIndex) == -1) {
        return -1;
    }

    int entranceIndex;
    if (_wmMatchEntranceElevFromMap(cityIndex, mapIndex, elevation, &entranceIndex) == -1) {
        return -1;
    }

    CityInfo* city = &(gCities[cityIndex]);
    EntranceInfo* entrance = &(city->entrances[entranceIndex]);
    entrance->state = state;

    return 0;
}

// 0x4BFE0C
void _wmWorldMap()
{
    _wmWorldMapFunc(0);
}

// 0x4BFE10
int _wmWorldMapFunc(int a1)
{
    if (worldmapWindowInit() == -1) {
        worldmapWindowFree();
        return -1;
    }

    _wmMatchWorldPosToArea(_world_xpos, _world_ypos, &_WorldMapCurrArea);

    unsigned int v24 = 0;
    int map = -1;
    int v25 = 0;

    int rc = 0;
    for (;;) {
        int keyCode = _get_input();
        unsigned int tick = _get_time();

        int mouseX;
        int mouseY;
        mouseGetPositionInWindow(gWorldmapWindow, &mouseX, &mouseY);

        int v4 = gWorldmapOffsetX + mouseX - WM_VIEW_X;
        int v5 = gWorldmapOffsetY + mouseY - WM_VIEW_Y;

        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            showQuitConfirmationDialog();
        }

        _scriptsCheckGameEvents(NULL, gWorldmapWindow);

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        int mouseEvent = mouseGetEvent();

        if (gWorldmapIsTravelling) {
            worldmapPerformTravel();

            if (gWorldmapIsInCar) {
                worldmapPerformTravel();
                worldmapPerformTravel();
                worldmapPerformTravel();

                if (gameGetGlobalVar(GVAR_CAR_BLOWER)) {
                    worldmapPerformTravel();
                }

                if (gameGetGlobalVar(GVAR_NEW_RENO_CAR_UPGRADE)) {
                    worldmapPerformTravel();
                }

                if (gameGetGlobalVar(GVAR_NEW_RENO_SUPER_CAR)) {
                    worldmapPerformTravel();
                    worldmapPerformTravel();
                    worldmapPerformTravel();
                }

                gWorldmapCarFrmCurrentFrame++;
                if (gWorldmapCarFrmCurrentFrame >= artGetFrameCount(gWorldmapCarFrm)) {
                    gWorldmapCarFrmCurrentFrame = 0;
                }

                carConsumeFuel(100);

                if (gWorldmapCarFuel <= 0) {
                    gWorldmapTravelDestX = 0;
                    gWorldmapTravelDestY = 0;
                    gWorldmapIsTravelling = false;

                    _wmMatchWorldPosToArea(v4, v5, &_WorldMapCurrArea);

                    gWorldmapIsInCar = false;

                    if (_WorldMapCurrArea == -1) {
                        _carCurrentArea = CITY_CAR_OUT_OF_GAS;

                        CityInfo* city = &(gCities[CITY_CAR_OUT_OF_GAS]);

                        CitySizeDescription* citySizeDescription = &(gCitySizeDescriptions[city->size]);
                        int worldmapX = _world_xpos + gWorldmapHotspotUpFrmWidth / 2 + citySizeDescription->width / 2;
                        int worldmapY = _world_ypos + gWorldmapHotspotUpFrmHeight / 2 + citySizeDescription->height / 2;
                        worldmapCitySetPos(CITY_CAR_OUT_OF_GAS, worldmapX, worldmapY);

                        city->state = 1;
                        city->field_40 = 1;

                        _WorldMapCurrArea = CITY_CAR_OUT_OF_GAS;
                    } else {
                        _carCurrentArea = _WorldMapCurrArea;
                    }

                    debugPrint("\nRan outta gas!");
                }
            }

            worldmapWindowRefresh();

            if (getTicksBetween(tick, v24) > 1000) {
                if (_partyMemberRestingHeal(3)) {
                    interfaceRenderHitPoints(false);
                    v24 = tick;
                }
            }

            _wmMarkSubTileRadiusVisited(_world_xpos, _world_ypos);

            if (dword_672E28 <= 0) {
                gWorldmapIsTravelling = false;
                _wmMatchWorldPosToArea(_world_xpos, _world_ypos, &_WorldMapCurrArea);
            }

            worldmapWindowRefresh();

            if (_wmGameTimeIncrement(18000)) {
                if (_game_user_wants_to_quit != 0) {
                    break;
                }
            }

            if (gWorldmapIsTravelling) {
                if (_wmRndEncounterOccurred()) {
                    if (_EncounterMapID != -1) {
                        if (gWorldmapIsInCar) {
                            _wmMatchAreaContainingMapIdx(_EncounterMapID, &_carCurrentArea);
                        }
                        mapLoadById(_EncounterMapID);
                    }
                    break;
                }
            }
        }

        if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0 && (mouseEvent & MOUSE_EVENT_LEFT_BUTTON_REPEAT) == 0) {
            if (mouseHitTestInWindow(gWorldmapWindow, WM_VIEW_X, WM_VIEW_Y, 472, 465)) {
                if (!gWorldmapIsTravelling && !_wmGenData && abs(_world_xpos - v4) < 5 && abs(_world_ypos - v5) < 5) {
                    _wmGenData = true;
                    worldmapWindowRefresh();
                }
            } else {
                continue;
            }
        }

        if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
            if (_wmGenData) {
                _wmGenData = false;
                worldmapWindowRefresh();

                if (abs(_world_xpos - v4) < 5 && abs(_world_ypos - v5) < 5) {
                    if (_WorldMapCurrArea != -1) {
                        CityInfo* city = &(gCities[_WorldMapCurrArea]);
                        if (city->field_40 == 2 && city->mapFid != -1) {
                            if (worldmapCityMapViewSelect(&map) == -1) {
                                v25 = -1;
                                break;
                            }
                        } else {
                            if (_wmAreaFindFirstValidMap(&map) == -1) {
                                v25 = -1;
                                break;
                            }

                            city->field_40 = 2;
                        }
                    } else {
                        map = 0;
                    }

                    if (map != -1) {
                        if (gWorldmapIsInCar) {
                            gWorldmapIsInCar = false;
                            if (_WorldMapCurrArea == -1) {
                                _wmMatchAreaContainingMapIdx(map, &_carCurrentArea);
                            } else {
                                _carCurrentArea = _WorldMapCurrArea;
                            }
                        }
                        mapLoadById(map);
                        break;
                    }
                }
            } else {
                if (mouseHitTestInWindow(gWorldmapWindow, WM_VIEW_X, WM_VIEW_Y, 472, 465)) {
                    _wmPartyInitWalking(v4, v5);
                }

                _wmGenData = 0;
            }
        }

        if (_tabsOffset) {
            _LastTabsYOffset += _tabsOffset;
            worldmapWindowRenderChrome(true);

            if (_tabsOffset > -1) {
                if (dword_672F54 <= _LastTabsYOffset) {
                    _wmInterfaceScrollTabsStop();
                }
            } else {
                if (dword_672F54 >= _LastTabsYOffset) {
                    _wmInterfaceScrollTabsStop();
                }
            }
        }

        if (keyCode == KEY_UPPERCASE_T || keyCode == KEY_LOWERCASE_T) {
            if (!gWorldmapIsTravelling && _WorldMapCurrArea != -1) {
                CityInfo* city = &(gCities[_WorldMapCurrArea]);
                if (city->field_40 == 2 && city->mapFid != -1) {
                    if (worldmapCityMapViewSelect(&map) == -1) {
                        rc = -1;
                    }

                    if (map != -1) {
                        if (gWorldmapIsInCar) {
                            _wmMatchAreaContainingMapIdx(map, &_carCurrentArea);
                        }

                        mapLoadById(map);
                    }
                }
            }
        } else if (keyCode == KEY_HOME) {
            _wmInterfaceCenterOnParty();
        } else if (keyCode == KEY_ARROW_UP) {
            worldmapWindowScroll(20, 20, 0, -1, 0, 1);
        } else if (keyCode == KEY_ARROW_LEFT) {
            worldmapWindowScroll(20, 20, -1, 0, 0, 1);
        } else if (keyCode == KEY_ARROW_DOWN) {
            worldmapWindowScroll(20, 20, 0, 1, 0, 1);
        } else if (keyCode == KEY_ARROW_RIGHT) {
            worldmapWindowScroll(20, 20, 1, 0, 0, 1);
        } else if (keyCode == KEY_CTRL_ARROW_UP) {
            _wmInterfaceScrollTabsStart(-27);
        } else if (keyCode == KEY_CTRL_ARROW_DOWN) {
            _wmInterfaceScrollTabsStart(27);
        } else if (keyCode >= KEY_CTRL_F1 && keyCode <= KEY_CTRL_F7) {
            int quickDestinationIndex = _LastTabsYOffset / 27 + (keyCode - KEY_CTRL_F1);
            if (quickDestinationIndex < gQuickDestinationsLength) {
                int cityIndex = gQuickDestinations[quickDestinationIndex];
                CityInfo* city = &(gCities[cityIndex]);
                if (_wmAreaIsKnown(city->field_28)) {
                    if (_WorldMapCurrArea != cityIndex) {
                        _wmPartyInitWalking(city->x, city->y);
                        _wmGenData = 0;
                    }
                }
            }
        }

        if (map != -1 || v25 == -1) {
            break;
        }
    }

    if (worldmapWindowFree() == -1) {
        return -1;
    }

    return rc;
}

// 0x4C056C
int _wmCheckGameAreaEvents()
{
    if (_WorldMapCurrArea == CITY_FAKE_VAULT_13_A) {
        if (_WorldMapCurrArea < gCitiesLength) {
            gCities[CITY_FAKE_VAULT_13_A].state = 0;
        }

        if (gCitiesLength > CITY_FAKE_VAULT_13_B) {
            gCities[CITY_FAKE_VAULT_13_B].state = 1;
        }

        _wmAreaMarkVisitedState(CITY_FAKE_VAULT_13_B, 2);
    }

    return 0;
}

// 0x4C05C4
int _wmInterfaceCenterOnParty()
{
    int v0;
    int v1;

    v0 = _world_xpos - 203;
    if ((v0 & 0x80000000) == 0) {
        if (v0 > _wmViewportRightScrlLimit) {
            v0 = _wmViewportRightScrlLimit;
        }
    } else {
        v0 = 0;
    }

    v1 = _world_ypos - 200;
    if ((v1 & 0x80000000) == 0) {
        if (v1 > _wmViewportBottomtScrlLimit) {
            v1 = _wmViewportBottomtScrlLimit;
        }
    } else {
        v1 = 0;
    }

    gWorldmapOffsetX = v0;
    gWorldmapOffsetY = v1;

    worldmapWindowRefresh();

    return 0;
}

// 0x4C0634
int _wmRndEncounterOccurred()
{
    unsigned int v0 = _get_time();
    if (getTicksBetween(v0, _wmLastRndTime) < 1500) {
        return 0;
    }

    _wmLastRndTime = v0;

    if (abs(_old_world_xpos - _world_xpos) < 3) {
        return 0;
    }

    if (abs(_old_world_ypos - _world_ypos) < 3) {
        return 0;
    }

    int v26;
    _wmMatchWorldPosToArea(_world_xpos, _world_ypos, &v26);
    if (v26 != -1) {
        return 0;
    }

    if (!_Meet_Frank_Horrigan) {
        unsigned int gameTime = gameTimeGetTime();
        if (gameTime / GAME_TIME_TICKS_PER_DAY > 35) {
            _EncounterMapID = v26;
            _Meet_Frank_Horrigan = true;
            if (gWorldmapIsInCar) {
                _wmMatchAreaContainingMapIdx(MAP_IN_GAME_MOVIE1, &_carCurrentArea);
            }
            mapLoadById(MAP_IN_GAME_MOVIE1);
            return 1;
        }
    }

    // NOTE: Uninline.
    _wmPartyFindCurSubTile();

    int dayPart;
    int gameTimeHour = gameTimeGetHour();
    if (gameTimeHour >= 1800 || gameTimeHour < 600) {
        dayPart = DAY_PART_NIGHT;
    } else if (gameTimeHour >= 1200) {
        dayPart = DAY_PART_AFTERNOON;
    } else {
        dayPart = DAY_PART_MORNING;
    }

    int frequency = _wmFreqValues[_world_subtile->encounterChance[dayPart]];
    if (frequency > 0 && frequency < 100) {
        int gameDifficulty = GAME_DIFFICULTY_NORMAL;
        if (configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, &gameDifficulty)) {
            int modifier = frequency / 15;
            switch (gameDifficulty) {
            case GAME_DIFFICULTY_EASY:
                frequency -= modifier;
                break;
            case GAME_DIFFICULTY_HARD:
                frequency += modifier;
                break;
            }
        }
    }

    int chance = randomBetween(0, 100);
    if (chance >= frequency) {
        return 0;
    }

    _wmRndEncounterPick();

    int v8 = 1;
    _wmEncounterIconShow = 1;
    _wmRndCursorFid = 0;

    EncounterTable* encounterTable = &(gEncounterTables[dword_672E50]);
    EncounterEntry* encounter = &(encounterTable->entries[dword_672E54]);
    if ((encounter->flags & ENCOUNTER_ENTRY_SPECIAL) != 0) {
        _wmRndCursorFid = 2;
        _wmMatchAreaContainingMapIdx(_EncounterMapID, &v26);

        CityInfo* city = &(gCities[v26]);
        CitySizeDescription* citySizeDescription = &(gCitySizeDescriptions[city->size]);
        int worldmapX = _world_xpos + gWorldmapHotspotUpFrmWidth / 2 + citySizeDescription->width / 2;
        int worldmapY = _world_ypos + gWorldmapHotspotUpFrmHeight / 2 + citySizeDescription->height / 2;
        worldmapCitySetPos(v26, worldmapX, worldmapY);
        v8 = 3;

        if (v26 >= 0 && v26 < gCitiesLength) {
            CityInfo* city = &(gCities[v26]);
            if (city->field_3C != 1) {
                city->state = 1;
            }
        }
    }

    // Blinking.
    for (int index = 0; index < 7; index++) {
        _wmRndCursorFid = v8 - _wmRndCursorFid;

        if (worldmapWindowRefresh() == -1) {
            return -1;
        }

        coreDelay(200);
    }

    if (gWorldmapIsInCar) {
        int modifiers[DAY_PART_COUNT];

        // NOTE: I'm not sure why they're copied.
        memcpy(modifiers, gDayPartEncounterFrequencyModifiers, sizeof(gDayPartEncounterFrequencyModifiers));

        frequency -= modifiers[dayPart];
    }

    bool randomEncounterIsDetected = false;
    if (frequency > chance) {
        int outdoorsman = partyGetBestSkillValue(SKILL_OUTDOORSMAN);
        Object* scanner = objectGetCarriedObjectByPid(gDude, PROTO_ID_MOTION_SENSOR);
        if (scanner != NULL) {
            if (gDude == scanner->owner) {
                outdoorsman += 20;
            }
        }

        if (outdoorsman > 95) {
            outdoorsman = 95;
        }

        TileInfo* tile;
        // NOTE: Uninline.
        _wmFindCurTileFromPos(_world_xpos, _world_ypos, &tile);
        debugPrint("\nEncounter Difficulty Mod: %d", tile->encounterDifficultyModifier);

        outdoorsman += tile->encounterDifficultyModifier;

        if (randomBetween(1, 100) < outdoorsman) {
            randomEncounterIsDetected = true;

            int xp = 100 - outdoorsman;
            if (xp > 0) {
                MessageListItem messageListItem;
                char* text = getmsg(&gMiscMessageList, &messageListItem, 8500);
                if (strlen(text) < 110) {
                    char formattedText[120];
                    sprintf(formattedText, text, xp);
                    displayMonitorAddMessage(formattedText);
                } else {
                    debugPrint("WorldMap: Error: Rnd Encounter string too long!");
                }

                debugPrint("WorldMap: Giving Player [%d] Experience For Catching Rnd Encounter!", xp);

                if (xp < 100) {
                    pcAddExperience(xp);
                }
            }
        }
    } else {
        randomEncounterIsDetected = true;
    }

    _old_world_xpos = _world_xpos;
    _old_world_ypos = _world_ypos;

    if (randomEncounterIsDetected) {
        MessageListItem messageListItem;

        const char* title = off_4BC878[0];
        const char* body = off_4BC878[1];

        title = getmsg(&gWorldmapMessageList, &messageListItem, 2999);
        body = getmsg(&gWorldmapMessageList, &messageListItem, 3000 + 50 * dword_672E50 + dword_672E54);
        if (showDialogBox(title, &body, 1, 169, 116, _colorTable[32328], NULL, _colorTable[32328], DIALOG_BOX_LARGE | DIALOG_BOX_YES_NO) == 0) {
            _wmEncounterIconShow = 0;
            _EncounterMapID = -1;
            dword_672E50 = -1;
            dword_672E54 = -1;
            return 0;
        }
    }

    return 1;
}

// NOTE: Inlined.
//
// 0x4C0BE4
int _wmPartyFindCurSubTile()
{
    return _wmFindCurSubTileFromPos(_world_xpos, _world_ypos, &_world_subtile);
}

// 0x4C0C00
int _wmFindCurSubTileFromPos(int x, int y, SubtileInfo** subtile)
{
    int tileIndex = y / WM_TILE_HEIGHT * gWorldmapGridWidth + x / WM_TILE_WIDTH % gWorldmapGridWidth;
    TileInfo* tile = &(gWorldmapTiles[tileIndex]);

    int column = y % WM_TILE_HEIGHT / WM_SUBTILE_SIZE;
    int row = x % WM_TILE_WIDTH / WM_SUBTILE_SIZE;
    *subtile = &(tile->subtiles[column][row]);

    return 0;
}

// NOTE: Inlined.
//
// 0x4C0CA8
int _wmFindCurTileFromPos(int x, int y, TileInfo** tile)
{
    int tileIndex = y / WM_TILE_HEIGHT * gWorldmapGridWidth + x / WM_TILE_WIDTH % gWorldmapGridWidth;
    *tile = &(gWorldmapTiles[tileIndex]);

    return 0;
}

// 0x4C0CF4
int _wmRndEncounterPick()
{
    if (_world_subtile == NULL) {
        // NOTE: Uninline.
        _wmPartyFindCurSubTile();
    }

    dword_672E50 = _world_subtile->encounterType;

    EncounterTable* encounterTable = &(gEncounterTables[dword_672E50]);

    int candidates[41];
    int candidatesLength = 0;
    int totalChance = 0;
    for (int index = 0; index < encounterTable->entriesLength; index++) {
        EncounterEntry* encounterTableEntry = &(encounterTable->entries[index]);

        bool selected = true;
        if (_wmEvalConditional(&(encounterTableEntry->condition), NULL) == 0) {
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

    int v1 = critterGetStat(gDude, STAT_LUCK) - 5;
    int v2 = randomBetween(0, totalChance) + v1;

    if (perkHasRank(gDude, PERK_EXPLORER)) {
        v2 += 2;
    }

    if (perkHasRank(gDude, PERK_RANGER)) {
        v2++;
    }

    if (perkHasRank(gDude, PERK_SCOUT)) {
        v2++;
    }

    int gameDifficulty;
    if (configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, &gameDifficulty)) {
        switch (gameDifficulty) {
        case GAME_DIFFICULTY_EASY:
            v2 += 5;
            if (v2 > totalChance) {
                v2 = totalChance;
            }
            break;
        case GAME_DIFFICULTY_HARD:
            v2 -= 5;
            if (v2 < 0) {
                v2 = 0;
            }
            break;
        }
    }

    int index;
    for (index = 0; index < candidatesLength; index++) {
        EncounterEntry* encounterTableEntry = &(encounterTable->entries[candidates[index]]);
        if (v2 < encounterTableEntry->chance) {
            break;
        }

        v2 -= encounterTableEntry->chance;
    }

    if (index == candidatesLength) {
        index = candidatesLength - 1;
    }

    dword_672E54 = candidates[index];

    EncounterEntry* encounterTableEntry = &(encounterTable->entries[dword_672E54]);
    if (encounterTableEntry->counter > 0) {
        encounterTableEntry->counter--;
    }

    if (encounterTableEntry->map == -1) {
        if (encounterTable->mapsLength <= 0) {
            Terrain* terrain = &(gTerrains[_world_subtile->terrain]);
            int randomMapIndex = randomBetween(0, terrain->mapsLength - 1);
            _EncounterMapID = terrain->maps[randomMapIndex];
        } else {
            int randomMapIndex = randomBetween(0, encounterTable->mapsLength - 1);
            _EncounterMapID = encounterTable->maps[randomMapIndex];
        }
    } else {
        _EncounterMapID = encounterTableEntry->map;
    }

    return 0;
}

// wmSetupRandomEncounter
// 0x4C0FA4
int worldmapSetupRandomEncounter()
{
    MessageListItem messageListItem;
    char* msg;

    if (_EncounterMapID == -1) {
        return 0;
    }

    EncounterTable* encounterTable = &(gEncounterTables[dword_672E50]);
    EncounterEntry* encounterTableEntry = &(encounterTable->entries[dword_672E54]);

    // You encounter:
    msg = getmsg(&gWorldmapMessageList, &messageListItem, 2998);
    displayMonitorAddMessage(msg);

    msg = getmsg(&gWorldmapMessageList, &messageListItem, 3000 + 50 * dword_672E50 + dword_672E54);
    displayMonitorAddMessage(msg);

    int gameDifficulty;
    switch (encounterTableEntry->scenery) {
    case ENCOUNTER_SCENERY_TYPE_NONE:
    case ENCOUNTER_SCENERY_TYPE_LIGHT:
    case ENCOUNTER_SCENERY_TYPE_NORMAL:
    case ENCOUNTER_SCENERY_TYPE_HEAVY:
        debugPrint("\nwmSetupRandomEncounter: Scenery Type: %s", _wmSceneryStrs[encounterTableEntry->scenery]);
        configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, &gameDifficulty);
        break;
    default:
        debugPrint("\nERROR: wmSetupRandomEncounter: invalid Scenery Type!");
        return -1;
    }

    Object* v0 = NULL;
    for (int i = 0; i < encounterTableEntry->field_50; i++) {
        ENCOUNTER_ENTRY_ENC* v3 = &(encounterTableEntry->field_54[i]);

        int v9 = randomBetween(v3->minQuantity, v3->maxQuantity);

        switch (gameDifficulty) {
        case GAME_DIFFICULTY_EASY:
            v9 -= 2;
            if (v9 < v3->minQuantity) {
                v9 = v3->minQuantity;
            }
            break;
        case GAME_DIFFICULTY_HARD:
            v9 += 2;
            break;
        }

        int partyMemberCount = _getPartyMemberCount();
        if (partyMemberCount > 2) {
            v9 += 2;
        }

        if (v9 != 0) {
            Object* v35;
            if (worldmapSetupCritters(v3->field_8, &v35, v9) == -1) {
                scriptsRequestWorldMap();
                return -1;
            }

            if (i > 0) {
                if (v0 != NULL) {
                    if (v0 != v35) {
                        if (encounterTableEntry->field_50 != 1) {
                            if (encounterTableEntry->field_50 == 2 && !isInCombat()) {
                                v0->data.critter.combat.whoHitMe = v35;
                                v35->data.critter.combat.whoHitMe = v0;

                                STRUCT_664980 combat;
                                combat.attacker = v0;
                                combat.defender = v35;
                                combat.actionPointsBonus = 0;
                                combat.accuracyBonus = 0;
                                combat.damageBonus = 0;
                                combat.minDamage = 0;
                                combat.maxDamage = 500;
                                combat.field_1C = 0;

                                _caiSetupTeamCombat(v35, v0);
                                _scripts_request_combat_locked(&combat);
                            }
                        } else {
                            if (!isInCombat()) {
                                v0->data.critter.combat.whoHitMe = gDude;

                                STRUCT_664980 combat;
                                combat.attacker = v0;
                                combat.defender = gDude;
                                combat.actionPointsBonus = 0;
                                combat.accuracyBonus = 0;
                                combat.damageBonus = 0;
                                combat.minDamage = 0;
                                combat.maxDamage = 500;
                                combat.field_1C = 0;

                                _caiSetupTeamCombat(gDude, v0);
                                _scripts_request_combat_locked(&combat);
                            }
                        }
                    }
                }
            }

            v0 = v35;
        }
    }

    return 0;
}

// wmSetupCritterObjs
// 0x4C11FC
int worldmapSetupCritters(int type_idx, Object** critterPtr, int critterCount)
{
    if (type_idx == -1) {
        return 0;
    }

    *critterPtr = 0;

    ENC_BASE_TYPE* v25 = &(_wmEncBaseTypeList[type_idx]);

    debugPrint("\nwmSetupCritterObjs: typeIdx: %d, Formation: %s", type_idx, gEncounterFormationTypeKeys[v25->position]);

    if (_wmSetupRndNextTileNumInit(v25) == -1) {
        return -1;
    }

    for (int i = 0; i < v25->field_34; i++) {
        ENC_BASE_TYPE_38* v5 = &(v25->field_38[i]);

        if (v5->pid == -1) {
            continue;
        }

        if (!_wmEvalConditional(&(v5->condition), &critterCount)) {
            continue;
        }

        int v23;
        switch (v5->field_2C) {
        case 0:
            v23 = v5->ratio * critterCount / 100;
            break;
        case 1:
            v23 = 1;
            break;
        default:
            assert(false && "Should be unreachable");
        }

        if (v23 < 1) {
            v23 = 1;
        }

        for (int j = 0; j < v23; j++) {
            int tile;
            if (_wmSetupRndNextTileNum(v25, v5, &tile) == -1) {
                debugPrint("\nERROR: wmSetupCritterObjs: wmSetupRndNextTileNum:");
                continue;
            }

            if (v5->pid == -1) {
                continue;
            }

            Object* object;
            if (objectCreateWithPid(&object, v5->pid) == -1) {
                return -1;
            }

            if (*critterPtr == NULL) {
                if ((v5->pid >> 24) == OBJ_TYPE_CRITTER) {
                    *critterPtr = object;
                }
            }

            if (v5->team != -1) {
                if ((object->pid >> 24) == OBJ_TYPE_CRITTER) {
                    object->data.critter.combat.team = v5->team;
                }
            }

            if (v5->script != -1) {
                if (object->sid != -1) {
                    scriptRemove(object->sid);
                    object->sid = -1;
                }

                _obj_new_sid_inst(object, SCRIPT_TYPE_CRITTER, v5->script - 1);
            }

            if (v25->position != ENCOUNTER_FORMATION_TYPE_SURROUNDING) {
                objectSetLocation(object, tile, gElevation, NULL);
            } else {
                _obj_attempt_placement(object, tile, 0, 0);
            }

            int direction = tileGetRotationTo(tile, gDude->tile);
            objectSetRotation(object, direction, NULL);

            for (int itemIndex = 0; itemIndex < v5->itemsLength; itemIndex++) {
                ENC_BASE_TYPE_38_48* v10 = &(v5->items[itemIndex]);

                int quantity;
                if (v10->maximumQuantity == v10->minimumQuantity) {
                    quantity = v10->maximumQuantity;
                } else {
                    quantity = randomBetween(v10->minimumQuantity, v10->maximumQuantity);
                }

                if (quantity == 0) {
                    continue;
                }

                Object* item;
                if (objectCreateWithPid(&item, v10->pid) == -1) {
                    return -1;
                }

                if (v10->pid == PROTO_ID_MONEY) {
                    if (perkHasRank(gDude, PERK_FORTUNE_FINDER)) {
                        quantity *= 2;
                    }
                }

                if (itemAdd(object, item, quantity) == -1) {
                    return -1;
                }

                _obj_disconnect(item, NULL);

                if (v10->isEquipped) {
                    if (_inven_wield(object, item, 1) == -1) {
                        debugPrint("\nERROR: wmSetupCritterObjs: Inven Wield Failed: %d on %s: Critter Fid: %d", item->pid, critterGetName(object), object->fid);
                    }
                }
            }
        }
    }

    return 0;
}

// 0x4C155C
int _wmSetupRndNextTileNumInit(ENC_BASE_TYPE* a1)
{
    for (int index = 0; index < 2; index++) {
        _wmRndCenterRotations[index] = 0;
        _wmRndTileDirs[index] = 0;
        _wmRndCenterTiles[index] = -1;

        if (index & 1) {
            _wmRndRotOffsets[index] = 5;
        } else {
            _wmRndRotOffsets[index] = 1;
        }
    }

    _wmRndCallCount = 0;

    switch (a1->position) {
    case ENCOUNTER_FORMATION_TYPE_SURROUNDING:
        _wmRndCenterTiles[0] = gDude->tile;
        _wmRndTileDirs[0] = randomBetween(0, ROTATION_COUNT - 1);

        _wmRndOriginalCenterTile = _wmRndCenterTiles[0];

        return 0;
    case ENCOUNTER_FORMATION_TYPE_STRAIGHT_LINE:
    case ENCOUNTER_FORMATION_TYPE_DOUBLE_LINE:
    case ENCOUNTER_FORMATION_TYPE_WEDGE:
    case ENCOUNTER_FORMATION_TYPE_CONE:
    case ENCOUNTER_FORMATION_TYPE_HUDDLE:
        if (1) {
            MapInfo* map = &(gMaps[gMapHeader.field_34]);
            if (map->startPointsLength != 0) {
                int rspIndex = randomBetween(0, map->startPointsLength - 1);
                MapStartPointInfo* rsp = &(map->startPoints[rspIndex]);

                _wmRndCenterTiles[0] = rsp->tile;
                _wmRndCenterTiles[1] = _wmRndCenterTiles[0];

                _wmRndCenterRotations[0] = rsp->field_8;
                _wmRndCenterRotations[1] = _wmRndCenterRotations[0];
            } else {
                _wmRndCenterRotations[0] = 0;
                _wmRndCenterRotations[1] = 0;

                _wmRndCenterTiles[0] = gDude->tile;
                _wmRndCenterTiles[1] = gDude->tile;
            }

            _wmRndTileDirs[0] = tileGetRotationTo(_wmRndCenterTiles[0], gDude->tile);
            _wmRndTileDirs[1] = tileGetRotationTo(_wmRndCenterTiles[1], gDude->tile);

            _wmRndOriginalCenterTile = _wmRndCenterTiles[0];

            return 0;
        }
    default:
        debugPrint("\nERROR: wmSetupCritterObjs: invalid Formation Type!");

        return -1;
    }
}

// wmSetupRndNextTileNum
// 0x4C16F0
int _wmSetupRndNextTileNum(ENC_BASE_TYPE* a1, ENC_BASE_TYPE_38* a2, int* out_tile_num)
{
    int tile_num;

    int attempt = 0;
    while (1) {
        switch (a1->position) {
        case ENCOUNTER_FORMATION_TYPE_SURROUNDING:
            if (1) {
                int distance;
                if (a2->distance != 0) {
                    distance = a2->distance;
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

                int origin = a2->tile;
                if (origin == -1) {
                    origin = tileGetTileInDirection(gDude->tile, _wmRndTileDirs[0], distance);
                }

                if (++_wmRndTileDirs[0] >= ROTATION_COUNT) {
                    _wmRndTileDirs[0] = 0;
                }

                int randomizedDistance = randomBetween(0, distance / 2);
                int randomizedRotation = randomBetween(0, ROTATION_COUNT - 1);
                tile_num = tileGetTileInDirection(origin, (randomizedRotation + _wmRndTileDirs[0]) % ROTATION_COUNT, randomizedDistance);
            }
            break;
        case ENCOUNTER_FORMATION_TYPE_STRAIGHT_LINE:
            tile_num = _wmRndCenterTiles[_wmRndIndex];
            if (_wmRndCallCount != 0) {
                int rotation = (_wmRndRotOffsets[_wmRndIndex] + _wmRndTileDirs[_wmRndIndex]) % ROTATION_COUNT;
                int origin = tileGetTileInDirection(_wmRndCenterTiles[_wmRndIndex], rotation, a1->spacing);
                int v13 = tileGetTileInDirection(origin, (rotation + _wmRndRotOffsets[_wmRndIndex]) % ROTATION_COUNT, a1->spacing);
                _wmRndCenterTiles[_wmRndIndex] = v13;
                _wmRndIndex = 1 - _wmRndIndex;
                tile_num = v13;
            }
            break;
        case ENCOUNTER_FORMATION_TYPE_DOUBLE_LINE:
            tile_num = _wmRndCenterTiles[_wmRndIndex];
            if (_wmRndCallCount != 0) {
                int rotation = (_wmRndRotOffsets[_wmRndIndex] + _wmRndTileDirs[_wmRndIndex]) % ROTATION_COUNT;
                int origin = tileGetTileInDirection(_wmRndCenterTiles[_wmRndIndex], rotation, a1->spacing);
                int v17 = tileGetTileInDirection(origin, (rotation + _wmRndRotOffsets[_wmRndIndex]) % ROTATION_COUNT, a1->spacing);
                _wmRndCenterTiles[_wmRndIndex] = v17;
                _wmRndIndex = 1 - _wmRndIndex;
                tile_num = v17;
            }
            break;
        case ENCOUNTER_FORMATION_TYPE_WEDGE:
            tile_num = _wmRndCenterTiles[_wmRndIndex];
            if (_wmRndCallCount != 0) {
                tile_num = tileGetTileInDirection(_wmRndCenterTiles[_wmRndIndex], (_wmRndRotOffsets[_wmRndIndex] + _wmRndTileDirs[_wmRndIndex]) % ROTATION_COUNT, a1->spacing);
                _wmRndCenterTiles[_wmRndIndex] = tile_num;
                _wmRndIndex = 1 - _wmRndIndex;
            }
            break;
        case ENCOUNTER_FORMATION_TYPE_CONE:
            tile_num = _wmRndCenterTiles[_wmRndIndex];
            if (_wmRndCallCount != 0) {
                tile_num = tileGetTileInDirection(_wmRndCenterTiles[_wmRndIndex], (_wmRndTileDirs[_wmRndIndex] + 3 + _wmRndRotOffsets[_wmRndIndex]) % ROTATION_COUNT, a1->spacing);
                _wmRndCenterTiles[_wmRndIndex] = tile_num;
                _wmRndIndex = 1 - _wmRndIndex;
            }
            break;
        case ENCOUNTER_FORMATION_TYPE_HUDDLE:
            tile_num = _wmRndCenterTiles[0];
            if (_wmRndCallCount != 0) {
                _wmRndTileDirs[0] = (_wmRndTileDirs[0] + 1) % ROTATION_COUNT;
                tile_num = tileGetTileInDirection(_wmRndCenterTiles[0], _wmRndTileDirs[0], a1->spacing);
                _wmRndCenterTiles[0] = tile_num;
            }
            break;
        default:
            assert(false && "Should be unreachable");
        }

        ++attempt;
        ++_wmRndCallCount;

        if (_wmEvalTileNumForPlacement(tile_num)) {
            break;
        }

        debugPrint("\nWARNING: EVAL-TILE-NUM FAILED!");

        if (tileDistanceBetween(_wmRndOriginalCenterTile, _wmRndCenterTiles[_wmRndIndex]) > 25) {
            return -1;
        }

        if (attempt > 25) {
            return -1;
        }
    }

    debugPrint("\nwmSetupRndNextTileNum:TileNum: %d", tile_num);

    *out_tile_num = tile_num;

    return 0;
}

// 0x4C1A64
bool _wmEvalTileNumForPlacement(int tile)
{
    if (_obj_blocking_at(gDude, tile, gElevation) != NULL) {
        return false;
    }

    if (pathfinderFindPath(gDude, gDude->tile, tile, NULL, 0, _obj_shoot_blocking_at) == 0) {
        return false;
    }

    return true;
}

// 0x4C1AC8
bool _wmEvalConditional(EncounterCondition* a1, int* a2)
{
    int value;

    bool matches = true;
    for (int index = 0; index < a1->entriesLength; index++) {
        EncounterConditionEntry* ptr = &(a1->entries[index]);

        matches = true;
        switch (ptr->type) {
        case ENCOUNTER_CONDITION_TYPE_GLOBAL:
            value = gameGetGlobalVar(ptr->param);
            if (!_wmEvalSubConditional(value, ptr->conditionalOperator, ptr->value)) {
                matches = false;
            }
            break;
        case ENCOUNTER_CONDITION_TYPE_NUMBER_OF_CRITTERS:
            if (!_wmEvalSubConditional(*a2, ptr->conditionalOperator, ptr->value)) {
                matches = false;
            }
            break;
        case ENCOUNTER_CONDITION_TYPE_RANDOM:
            value = randomBetween(0, 100);
            if (value > ptr->param) {
                matches = false;
            }
            break;
        case ENCOUNTER_CONDITION_TYPE_PLAYER:
            value = pcGetStat(PC_STAT_LEVEL);
            if (!_wmEvalSubConditional(value, ptr->conditionalOperator, ptr->value)) {
                matches = false;
            }
            break;
        case ENCOUNTER_CONDITION_TYPE_DAYS_PLAYED:
            value = gameTimeGetTime();
            if (!_wmEvalSubConditional(value / GAME_TIME_TICKS_PER_DAY, ptr->conditionalOperator, ptr->value)) {
                matches = false;
            }
            break;
        case ENCOUNTER_CONDITION_TYPE_TIME_OF_DAY:
            value = gameTimeGetHour();
            if (!_wmEvalSubConditional(value / 100, ptr->conditionalOperator, ptr->value)) {
                matches = false;
            }
            break;
        }

        if (!matches) {
            // FIXME: Can overflow with all 3 conditions specified.
            if (a1->logicalOperators[index] == ENCOUNTER_LOGICAL_OPERATOR_AND) {
                break;
            }
        }
    }

    return matches;
}

// 0x4C1C0C
bool _wmEvalSubConditional(int operand1, int condionalOperator, int operand2)
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
bool _wmGameTimeIncrement(int a1)
{
    if (a1 == 0) {
        return false;
    }

    while (a1 != 0) {
        unsigned int gameTime = gameTimeGetTime();
        unsigned int nextEventTime = queueGetNextEventTime();
        int v1 = nextEventTime >= gameTime ? a1 : nextEventTime - gameTime;
        a1 -= v1;

        gameTimeAddTicks(v1);

        int hour = gameTimeGetHour() / 100;

        int frameCount = artGetFrameCount(gWorldmapDialFrm);
        int frame = (hour + 12) % frameCount;
        if (gWorldmapDialFrmCurrentFrame != frame) {
            gWorldmapDialFrmCurrentFrame = frame;
            worldmapWindowRenderDial(true);
        }

        worldmapWindowRenderDate(true);

        if (queueProcessEvents()) {
            break;
        }
    }

    return true;
}

// Reads .msk file if needed.
//
// 0x4C1CE8
int _wmGrabTileWalkMask(int tile)
{
    TileInfo* tileInfo = &(gWorldmapTiles[tile]);
    if (tileInfo->walkMaskData != NULL) {
        return 0;
    }

    if (*tileInfo->walkMaskName == '\0') {
        return 0;
    }

    tileInfo->walkMaskData = (unsigned char*)internal_malloc(13200);
    if (tileInfo->walkMaskData == NULL) {
        return -1;
    }

    char path[MAX_PATH];
    sprintf(path, "data\\%s.msk", tileInfo->walkMaskName);

    File* stream = fileOpen(path, "rb");
    if (stream == NULL) {
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
bool _wmWorldPosInvalid(int a1, int a2)
{
    int v3 = a2 / WM_TILE_HEIGHT * gWorldmapGridWidth + a1 / WM_TILE_WIDTH % gWorldmapGridWidth;
    if (_wmGrabTileWalkMask(v3) == -1) {
        return false;
    }

    TileInfo* tileDescription = &(gWorldmapTiles[v3]);
    unsigned char* mask = tileDescription->walkMaskData;
    if (mask == NULL) {
        return false;
    }

    // Mask length is 13200, which is 300 * 44
    // 44 * 8 is 352, which is probably left 2 bytes intact
    // TODO: Check math.
    int pos = (a2 % WM_TILE_HEIGHT) * 44 + (a1 % WM_TILE_WIDTH) / 8;
    int bit = 1 << (((a1 % WM_TILE_WIDTH) / 8) & 3);
    return (mask[pos] & bit) != 0;
}

// 0x4C1E54
void _wmPartyInitWalking(int x, int y)
{
    gWorldmapTravelDestX = x;
    gWorldmapTravelDestY = y;
    _WorldMapCurrArea = -1;
    gWorldmapIsTravelling = true;

    int dx = abs(x - _world_xpos);
    int dy = abs(y - _world_ypos);

    if (dx < dy) {
        dword_672E28 = dy;
        dword_672E30 = 2 * dx;
        _x_line_inc = 0;
        dword_672E2C = 2 * dx - dy;
        dword_672E34 = 2 * (dx - dy);
        dword_672E3C = 1;
        _y_line_inc = 1;
        dword_672E44 = 1;
    } else {
        dword_672E28 = dx;
        dword_672E30 = 2 * dy;
        _y_line_inc = 0;
        dword_672E2C = 2 * dy - dx;
        dword_672E34 = 2 * (dy - dx);
        _x_line_inc = 1;
        dword_672E3C = 1;
        dword_672E44 = 1;
    }

    if (gWorldmapTravelDestX < _world_xpos) {
        dword_672E3C = -dword_672E3C;
        _x_line_inc = -_x_line_inc;
    }

    if (gWorldmapTravelDestY < _world_ypos) {
        dword_672E44 = -dword_672E44;
        _y_line_inc = -_y_line_inc;
    }

    if (!_wmCursorIsVisible()) {
        _wmInterfaceCenterOnParty();
    }
}

// 0x4C1F90
void worldmapPerformTravel()
{
    if (dword_672E28 <= 0) {
        return;
    }

    _terrainCounter++;
    if (_terrainCounter > 4) {
        _terrainCounter = 1;
    }

    // NOTE: Uninline.
    _wmPartyFindCurSubTile();

    Terrain* terrain = &(gTerrains[_world_subtile->terrain]);
    int v1 = terrain->field_28 - perkGetRank(gDude, PERK_PATHFINDER);
    if (v1 < 1) {
        v1 = 1;
    }

    if (_terrainCounter / v1 >= 1) {
        int v3;
        int v4;
        if (dword_672E2C >= 0) {
            if (_wmWorldPosInvalid(dword_672E3C + _world_xpos, dword_672E44 + _world_ypos)) {
                gWorldmapTravelDestX = 0;
                gWorldmapTravelDestY = 0;
                gWorldmapIsTravelling = false;
                _wmMatchWorldPosToArea(_world_xpos, _world_xpos, &_WorldMapCurrArea);
                dword_672E28 = 0;
                return;
            }

            v3 = dword_672E3C;
            dword_672E2C += dword_672E34;
            _world_xpos += dword_672E3C;
            v4 = dword_672E44;
            _world_ypos += dword_672E44;
        } else {
            if (_wmWorldPosInvalid(_x_line_inc + _world_xpos, _y_line_inc + _world_ypos) == 1) {
                gWorldmapTravelDestX = 0;
                gWorldmapTravelDestY = 0;
                gWorldmapIsTravelling = false;
                _wmMatchWorldPosToArea(_world_xpos, _world_xpos, &_WorldMapCurrArea);
                dword_672E28 = 0;
                return;
            }

            v3 = _x_line_inc;
            dword_672E2C += dword_672E30;
            _world_ypos += _y_line_inc;
            v4 = _y_line_inc;
            _world_xpos += _x_line_inc;
        }

        worldmapWindowScroll(1, 1, v3, v4, NULL, false);

        dword_672E28 -= 1;
        if (dword_672E28 == 0) {
            gWorldmapTravelDestY = 0;
            gWorldmapIsTravelling = false;
            gWorldmapTravelDestX = 0;
        }
    }
}

// 0x4C219C
void _wmInterfaceScrollTabsStart(int a1)
{
    int i;
    int v3;

    for (i = 0; i < 7; i++) {
        buttonDisable(_wmTownMapSubButtonIds[i]);
    }

    dword_672F54 = _LastTabsYOffset;

    v3 = _LastTabsYOffset + 7 * a1;

    if (a1 >= 0) {
        if (gWorldmapTownTabsUnderlayFrmHeight - 230 <= dword_672F54) {
            goto L11;
        } else {
            dword_672F54 = _LastTabsYOffset + 7 * a1;
            if (v3 > gWorldmapTownTabsUnderlayFrmHeight - 230) {
            }
        }
    } else {
        if (_LastTabsYOffset <= 0) {
            goto L11;
        } else {
            dword_672F54 = _LastTabsYOffset + 7 * a1;
            if (v3 < 0) {
                dword_672F54 = 0;
            }
        }
    }

    _tabsOffset = a1;

L11:

    if (!_tabsOffset) {
        return;
    }

    _LastTabsYOffset += _tabsOffset;

    worldmapWindowRenderChrome(true);

    if (_tabsOffset > -1) {
        if (dword_672F54 > _LastTabsYOffset) {
            return;
        }
    } else if (dword_672F54 < _LastTabsYOffset) {
        return;
    }

    _wmInterfaceScrollTabsStop();
}

// 0x4C2270
void _wmInterfaceScrollTabsStop()
{
    int i;

    _tabsOffset = 0;

    for (i = 0; i < 7; i++) {
        buttonEnable(_wmTownMapSubButtonIds[i]);
    }
}

// 0x4C2324
int worldmapWindowInit()
{
    int fid;
    Art* frm;
    CacheEntry* frmHandle;
    
    _wmLastRndTime = _get_time();
    _fontnum = fontGetCurrent();
    fontSetCurrent(0);

    _map_save_in_game(true);

    const char* backgroundSoundFileName = gWorldmapIsInCar ? "20car" : "23world";
    _gsound_background_play_level_music(backgroundSoundFileName, 12);

    indicatorBarHide();
    isoDisable();
    colorCycleDisable();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    int worldmapWindowX = (screenGetWidth() - WM_WINDOW_WIDTH) / 2;
    int worldmapWindowY = (screenGetHeight() - WM_WINDOW_HEIGHT) / 2;
    gWorldmapWindow = windowCreate(worldmapWindowX, worldmapWindowY, WM_WINDOW_WIDTH, WM_WINDOW_HEIGHT, _colorTable[0], WINDOW_FLAG_0x04);
    if (gWorldmapWindow == -1) {
        return -1;
    }

    fid = buildFid(6, 136, 0, 0, 0);
    frm = artLock(fid, &gWorldmapBoxFrmHandle);
    if (frm == NULL) {
        return -1;
    }

    gWorldmapBoxFrmWidth = artGetWidth(frm, 0, 0);
    gWorldmapBoxFrmHeight = artGetHeight(frm, 0, 0);

    artUnlock(gWorldmapBoxFrmHandle);
    gWorldmapBoxFrmHandle = INVALID_CACHE_ENTRY;

    fid = buildFid(6, 136, 0, 0, 0);
    gWorldmapBoxFrmData = artLockFrameData(fid, 0, 0, &gWorldmapBoxFrmHandle);
    if (gWorldmapBoxFrmData == NULL) {
        return -1;
    }

    gWorldmapWindowBuffer = windowGetBuffer(gWorldmapWindow);
    if (gWorldmapWindowBuffer == NULL) {
        return -1;
    }

    blitBufferToBuffer(gWorldmapBoxFrmData, gWorldmapBoxFrmWidth, gWorldmapBoxFrmHeight, gWorldmapBoxFrmWidth, gWorldmapWindowBuffer, WM_WINDOW_WIDTH);

    for (int citySize = 0; citySize < CITY_SIZE_COUNT; citySize++) {
        CitySizeDescription* citySizeDescription = &(gCitySizeDescriptions[citySize]);

        fid = buildFid(6, 336 + citySize, 0, 0, 0);
        citySizeDescription->fid = fid;

        frm = artLock(fid, &(citySizeDescription->handle));
        if (frm == NULL) {
            return -1;
        }

        citySizeDescription->width = artGetWidth(frm, 0, 0);
        citySizeDescription->height = artGetHeight(frm, 0, 0);

        artUnlock(citySizeDescription->handle);
        citySizeDescription->handle = INVALID_CACHE_ENTRY;

        citySizeDescription->data = artLockFrameData(fid, 0, 0, &(citySizeDescription->handle));
        // FIXME: check is obviously wrong, should be citySizeDescription->data.
        if (frm == NULL) {
            return -1;
        }
    }

    fid = buildFid(6, 168, 0, 0, 0);
    frm = artLock(fid, &gWorldmapHotspotUpFrmHandle);
    if (frm == NULL) {
        return -1;
    }

    gWorldmapHotspotUpFrmWidth = artGetWidth(frm, 0, 0);
    gWorldmapHotspotUpFrmHeight = artGetHeight(frm, 0, 0);

    artUnlock(gWorldmapHotspotUpFrmHandle);
    gWorldmapHotspotUpFrmHandle = INVALID_CACHE_ENTRY;

    // hotspot1.frm - town map selector shape #1
    fid = buildFid(6, 168, 0, 0, 0);
    gWorldmapHotspotUpFrmData = artLockFrameData(fid, 0, 0, &gWorldmapHotspotUpFrmHandle);

    // hotspot2.frm - town map selector shape #2
    fid = buildFid(6, 223, 0, 0, 0);
    gWorldmapHotspotDownFrmData = artLockFrameData(fid, 0, 0, &gWorldmapHotspotDownFrmHandle);
    if (gWorldmapHotspotDownFrmData == NULL) {
        return -1;
    }

    // wmaptarg.frm - world map move target maker #1
    fid = buildFid(6, 139, 0, 0, 0);
    frm = artLock(fid, &gWorldmapDestinationMarkerFrmHandle);
    if (frm == NULL) {
        return -1;
    }

    gWorldmapDestinationMarkerFrmWidth = artGetWidth(frm, 0, 0);
    gWorldmapDestinationMarkerFrmHeight = artGetHeight(frm, 0, 0);

    artUnlock(gWorldmapDestinationMarkerFrmHandle);
    gWorldmapDestinationMarkerFrmHandle = INVALID_CACHE_ENTRY;

    // wmaploc.frm - world map location marker
    fid = buildFid(6, 138, 0, 0, 0);
    frm = artLock(fid, &gWorldmapLocationMarkerFrmHandle);
    if (frm == NULL) {
        return -1;
    }

    gWorldmapLocationMarkerFrmWidth = artGetWidth(frm, 0, 0);
    gWorldmapLocationMarkerFrmHeight = artGetHeight(frm, 0, 0);

    artUnlock(gWorldmapLocationMarkerFrmHandle);
    gWorldmapLocationMarkerFrmHandle = INVALID_CACHE_ENTRY;

    // wmaptarg.frm - world map move target maker #1
    fid = buildFid(6, 139, 0, 0, 0);
    gWorldmapDestinationMarkerFrmData = artLockFrameData(fid, 0, 0, &gWorldmapDestinationMarkerFrmHandle);
    if (gWorldmapDestinationMarkerFrmData == NULL) {
        return -1;
    }

    // wmaploc.frm - world map location marker
    fid = buildFid(6, 138, 0, 0, 0);
    gWorldmapLocationMarkerFrmData = artLockFrameData(fid, 0, 0, &gWorldmapLocationMarkerFrmHandle);
    if (gWorldmapLocationMarkerFrmData == NULL) {
        return -1;
    }

    for (int index = 0; index < WORLD_MAP_ENCOUNTER_FRM_COUNT; index++) {
        fid = buildFid(6, gWorldmapEncounterFrmIds[index], 0, 0, 0);
        frm = artLock(fid, &(gWorldmapEncounterFrmHandles[index]));
        if (frm == NULL) {
            return -1;
        }

        gWorldmapEncounterFrmWidths[index] = artGetWidth(frm, 0, 0);
        gWorldmapEncounterFrmHeights[index] = artGetHeight(frm, 0, 0);

        artUnlock(gWorldmapEncounterFrmHandles[index]);

        gWorldmapEncounterFrmHandles[index] = INVALID_CACHE_ENTRY;
        gWorldmapEncounterFrmData[index] = artLockFrameData(fid, 0, 0, &(gWorldmapEncounterFrmHandles[index]));
    }

    for (int index = 0; index < gWorldmapTilesLength; index++) {
        gWorldmapTiles[index].handle = INVALID_CACHE_ENTRY;
    }

    // wmtabs.frm - worldmap town tabs underlay
    fid = buildFid(6, 364, 0, 0, 0);
    frm = artLock(fid, &frmHandle);
    if (frm == NULL) {
        return -1;
    }

    gWorldmapTownTabsUnderlayFrmWidth = artGetWidth(frm, 0, 0);
    gWorldmapTownTabsUnderlayFrmHeight = artGetHeight(frm, 0, 0);

    artUnlock(frmHandle);

    gWorldmapTownTabsUnderlayFrmData = artLockFrameData(fid, 0, 0, &gWorldmapTownTabsUnderlayFrmHandle) + gWorldmapTownTabsUnderlayFrmWidth * 27;
    if (gWorldmapTownTabsUnderlayFrmData == NULL) {
        return -1;
    }

    // wmtbedge.frm - worldmap town tabs edging overlay
    fid = buildFid(6, 367, 0, 0, 0);
    gWorldmapTownTabsEdgeFrmData = artLockFrameData(fid, 0, 0, &gWorldmapTownTabsEdgeFrmHandle);
    if (gWorldmapTownTabsEdgeFrmData == NULL) {
        return -1;
    }

    // wmdial.frm - worldmap night/day dial
    fid = buildFid(6, 365, 0, 0, 0);
    gWorldmapDialFrm = artLock(fid, &gWorldmapDialFrmHandle);
    if (gWorldmapDialFrm == NULL) {
        return -1;
    }

    gWorldmapDialFrmWidth = artGetWidth(gWorldmapDialFrm, 0, 0);
    gWorldmapDialFrmHeight = artGetHeight(gWorldmapDialFrm, 0, 0);

    // wmscreen - worldmap overlay screen
    fid = buildFid(6, 363, 0, 0, 0);
    frm = artLock(fid, &frmHandle);
    if (frm == NULL) {
        return -1;
    }

    gWorldmapCarOverlayFrmWidth = artGetWidth(frm, 0, 0);
    gWorldmapCarOverlayFrmHeight = artGetHeight(frm, 0, 0);

    artUnlock(frmHandle);

    gWorldmapCarOverlayFrmData = artLockFrameData(fid, 0, 0, &gWorldmapCarOverlayFrmHandle);
    if (gWorldmapCarOverlayFrmData == NULL) {
        return -1;
    }

    // wmglobe.frm - worldmap globe stamp overlay
    fid = buildFid(6, 366, 0, 0, 0);
    frm = artLock(fid, &frmHandle);
    if (frm == NULL) {
        return -1;
    }

    gWorldmapGlobeOverlayFrmWidth = artGetWidth(frm, 0, 0);
    gWorldmapGloveOverlayFrmHeight = artGetHeight(frm, 0, 0);

    artUnlock(frmHandle);

    gWorldmapGlobeOverlayFrmData = artLockFrameData(fid, 0, 0, &gWorldmapGlobeOverlayFrmHandle);
    if (gWorldmapGlobeOverlayFrmData == NULL) {
        return -1;
    }

    // lilredup.frm - little red button up
    fid = buildFid(6, 8, 0, 0, 0);
    frm = artLock(fid, &frmHandle);
    if (frm == NULL) {
        return -1;
    }

    int littleRedButtonUpWidth = artGetWidth(frm, 0, 0);
    int littleRedButtonUpHeight = artGetHeight(frm, 0, 0);

    artUnlock(frmHandle);

    gWorldmapLittleRedButtonUpFrmData = artLockFrameData(fid, 0, 0, &gWorldmapLittleRedButtonUpFrmHandle);

    // lilreddn.frm - little red button down
    fid = buildFid(6, 9, 0, 0, 0);
    gWorldmapLittleRedButtonDownFrmData = artLockFrameData(fid, 0, 0, &gWorldmapLittleRedButtonDownFrmHandle);

    // months.frm - month strings for pip boy
    fid = buildFid(6, 129, 0, 0, 0);
    gWorldmapMonthsFrm = artLock(fid, &gWorldmapMonthsFrmHandle);
    if (gWorldmapMonthsFrm == NULL) {
        return -1;
    }

    // numbers.frm - numbers for the hit points and fatigue counters
    fid = buildFid(6, 82, 0, 0, 0);
    gWorldmapNumbersFrm = artLock(fid, &gWorldmapNumbersFrmHandle);
    if (gWorldmapNumbersFrm == NULL) {
        return -1;
    }

    // create town/world switch button
    buttonCreate(gWorldmapWindow,
        WM_TOWN_WORLD_SWITCH_X,
        WM_TOWN_WORLD_SWITCH_Y,
        littleRedButtonUpWidth, 
        littleRedButtonUpHeight, 
        -1, 
        -1, 
        -1,
        KEY_UPPERCASE_T,
        gWorldmapLittleRedButtonUpFrmData,
        gWorldmapLittleRedButtonDownFrmData,
        NULL, 
        BUTTON_FLAG_TRANSPARENT);

    for (int index = 0; index < 7; index++) {
        _wmTownMapSubButtonIds[index] = buttonCreate(gWorldmapWindow,
            508, 
            138 + 27 * index,
            littleRedButtonUpWidth, 
            littleRedButtonUpHeight, 
            -1, 
            -1,
            -1, 
            KEY_CTRL_F1 + index, 
            gWorldmapLittleRedButtonUpFrmData, 
            gWorldmapLittleRedButtonDownFrmData, 
            NULL, 
            BUTTON_FLAG_TRANSPARENT);
    }

    for (int index = 0; index < WORLDMAP_ARROW_FRM_COUNT; index++) {
        // 200 - uparwon.frm - character editor
        // 199 - uparwoff.frm - character editor
        fid = buildFid(6, 200 - index, 0, 0, 0);
        frm = artLock(fid, &(gWorldmapTownListScrollUpFrmHandle[index]));
        if (frm == NULL) {
            return -1;
        }

        gWorldmapTownListScrollUpFrmWidth = artGetWidth(frm, 0, 0);
        gWorldmapTownListScrollUpFrmHeight = artGetHeight(frm, 0, 0);
        gWorldmapTownListScrollUpFrmData[index] = artGetFrameData(frm, 0, 0);
    }

    for (int index = 0; index < WORLDMAP_ARROW_FRM_COUNT; index++) {
        // 182 - dnarwon.frm - character editor
        // 181 - dnarwoff.frm - character editor
        fid = buildFid(6, 182 - index, 0, 0, 0);
        frm = artLock(fid, &(gWorldmapTownListScrollDownFrmHandle[index]));
        if (frm == NULL) {
            return -1;
        }

        gWorldmapTownListScrollDownFrmWidth = artGetWidth(frm, 0, 0);
        gWorldmapTownListScrollDownFrmHeight = artGetHeight(frm, 0, 0);
        gWorldmapTownListScrollDownFrmData[index] = artGetFrameData(frm, 0, 0);
    }

    // Scroll up button.
    buttonCreate(gWorldmapWindow,
        WM_TOWN_LIST_SCROLL_UP_X,
        WM_TOWN_LIST_SCROLL_UP_Y,
        gWorldmapTownListScrollUpFrmWidth, 
        gWorldmapTownListScrollUpFrmHeight, 
        -1, 
        -1, 
        -1,
        KEY_CTRL_ARROW_UP,
        gWorldmapTownListScrollUpFrmData[WORLDMAP_ARROW_FRM_NORMAL], 
        gWorldmapTownListScrollUpFrmData[WORLDMAP_ARROW_FRM_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);

    // Scroll down button.
    buttonCreate(gWorldmapWindow, 
        WM_TOWN_LIST_SCROLL_DOWN_X, 
        WM_TOWN_LIST_SCROLL_DOWN_Y, 
        gWorldmapTownListScrollDownFrmWidth, 
        gWorldmapTownListScrollDownFrmHeight,
        -1,
        -1,
        -1, 
        KEY_CTRL_ARROW_DOWN,
        gWorldmapTownListScrollDownFrmData[WORLDMAP_ARROW_FRM_NORMAL], 
        gWorldmapTownListScrollDownFrmData[WORLDMAP_ARROW_FRM_PRESSED],
        NULL, 
        BUTTON_FLAG_TRANSPARENT);

    if (gWorldmapIsInCar) {
        // wmcarmve.frm - worldmap car movie
        fid = buildFid(6, 433, 0, 0, 0);
        gWorldmapCarFrm = artLock(fid, &gWorldmapCarFrmHandle);
        if (gWorldmapCarFrm == NULL) {
            return -1;
        }

        gWorldmapCarFrmWidth = artGetWidth(gWorldmapCarFrm, 0, 0);
        gWorldmapCarFrmHeight = artGetHeight(gWorldmapCarFrm, 0, 0);
    }

    tickersAdd(worldmapWindowHandleMouseScrolling);

    if (_wmMakeTabsLabelList(&gQuickDestinations, &gQuickDestinationsLength) == -1) {
        return -1;
    }

    _wmInterfaceWasInitialized = 1;

    if (worldmapWindowRefresh() == -1) {
        return -1;
    }

    windowRefresh(gWorldmapWindow);
    scriptsDisable();
    _scr_remove_all();

    return 0;
}

// 0x4C2E44
int worldmapWindowFree()
{
    int i;
    TileInfo* tile;

    tickersRemove(worldmapWindowHandleMouseScrolling);

    if (gWorldmapBoxFrmData != NULL) {
        artUnlock(gWorldmapBoxFrmHandle);
        gWorldmapBoxFrmData = NULL;
    }
    gWorldmapBoxFrmHandle = INVALID_CACHE_ENTRY;

    if (gWorldmapWindow != -1) {
        windowDestroy(gWorldmapWindow);
        gWorldmapWindow = -1;
    }

    if (gWorldmapHotspotUpFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gWorldmapHotspotUpFrmHandle);
    }
    gWorldmapHotspotUpFrmData = NULL;

    if (gWorldmapHotspotDownFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gWorldmapHotspotDownFrmHandle);
    }
    gWorldmapHotspotDownFrmData = NULL;

    if (gWorldmapDestinationMarkerFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gWorldmapDestinationMarkerFrmHandle);
    }
    gWorldmapDestinationMarkerFrmData = NULL;

    if (gWorldmapLocationMarkerFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gWorldmapLocationMarkerFrmHandle);
    }
    gWorldmapLocationMarkerFrmData = NULL;

    for (i = 0; i < 4; i++) {
        if (gWorldmapEncounterFrmHandles[i] != INVALID_CACHE_ENTRY) {
            artUnlock(gWorldmapEncounterFrmHandles[i]);
        }
        gWorldmapEncounterFrmData[i] = NULL;
    }

    for (i = 0; i < CITY_SIZE_COUNT; i++) {
        CitySizeDescription* citySizeDescription = &(gCitySizeDescriptions[i]);
        // FIXME: probably unsafe code, no check for -1
        artUnlock(citySizeDescription->handle);
        citySizeDescription->handle = INVALID_CACHE_ENTRY;
        citySizeDescription->data = NULL;
    }

    for (i = 0; i < gWorldmapTilesLength; i++) {
        tile = &(gWorldmapTiles[i]);
        if (tile->handle != INVALID_CACHE_ENTRY) {
            artUnlock(tile->handle);
            tile->handle = INVALID_CACHE_ENTRY;
            tile->data = NULL;

            if (tile->walkMaskData != NULL) {
                internal_free(tile->walkMaskData);
                tile->walkMaskData = NULL;
            }
        }
    }

    if (gWorldmapTownTabsUnderlayFrmData != NULL) {
        artUnlock(gWorldmapTownTabsUnderlayFrmHandle);
        gWorldmapTownTabsUnderlayFrmHandle = INVALID_CACHE_ENTRY;
        gWorldmapTownTabsUnderlayFrmData = NULL;
    }

    if (gWorldmapTownTabsEdgeFrmData != NULL) {
        artUnlock(gWorldmapTownTabsEdgeFrmHandle);
        gWorldmapTownTabsEdgeFrmHandle = INVALID_CACHE_ENTRY;
        gWorldmapTownTabsEdgeFrmData = NULL;
    }

    if (gWorldmapDialFrm != NULL) {
        artUnlock(gWorldmapDialFrmHandle);
        gWorldmapDialFrmHandle = INVALID_CACHE_ENTRY;
        gWorldmapDialFrm = NULL;
    }

    if (gWorldmapCarOverlayFrmData != NULL) {
        artUnlock(gWorldmapCarOverlayFrmHandle);
        gWorldmapCarOverlayFrmHandle = INVALID_CACHE_ENTRY;
        gWorldmapCarOverlayFrmData = NULL;
    }
    if (gWorldmapGlobeOverlayFrmData != NULL) {
        artUnlock(gWorldmapGlobeOverlayFrmHandle);
        gWorldmapGlobeOverlayFrmHandle = INVALID_CACHE_ENTRY;
        gWorldmapGlobeOverlayFrmData = NULL;
    }

    if (gWorldmapLittleRedButtonUpFrmData != NULL) {
        artUnlock(gWorldmapLittleRedButtonUpFrmHandle);
        gWorldmapLittleRedButtonUpFrmHandle = INVALID_CACHE_ENTRY;
        gWorldmapLittleRedButtonUpFrmData = NULL;
    }

    if (gWorldmapLittleRedButtonDownFrmData != NULL) {
        artUnlock(gWorldmapLittleRedButtonDownFrmHandle);
        gWorldmapLittleRedButtonDownFrmHandle = INVALID_CACHE_ENTRY;
        gWorldmapLittleRedButtonDownFrmData = NULL;
    }

    for (i = 0; i < 2; i++) {
        artUnlock(gWorldmapTownListScrollUpFrmHandle[i]);
        gWorldmapTownListScrollUpFrmHandle[i] = INVALID_CACHE_ENTRY;
        gWorldmapTownListScrollUpFrmData[i] = NULL;

        artUnlock(gWorldmapTownListScrollDownFrmHandle[i]);
        gWorldmapTownListScrollDownFrmHandle[i] = INVALID_CACHE_ENTRY;
        gWorldmapTownListScrollDownFrmData[i] = NULL;
    }

    gWorldmapTownListScrollUpFrmHeight = 0;
    gWorldmapTownListScrollDownFrmWidth = 0;
    gWorldmapTownListScrollDownFrmHeight = 0;
    gWorldmapTownListScrollUpFrmWidth = 0;

    if (gWorldmapMonthsFrm != NULL) {
        artUnlock(gWorldmapMonthsFrmHandle);
        gWorldmapMonthsFrmHandle = INVALID_CACHE_ENTRY;
        gWorldmapMonthsFrm = NULL;
    }

    if (gWorldmapNumbersFrm != NULL) {
        artUnlock(gWorldmapNumbersFrmHandle);
        gWorldmapNumbersFrmHandle = INVALID_CACHE_ENTRY;
        gWorldmapNumbersFrm = NULL;
    }

    if (gWorldmapCarFrm != NULL) {
        artUnlock(gWorldmapCarFrmHandle);
        gWorldmapCarFrmHandle = INVALID_CACHE_ENTRY;
        gWorldmapCarFrm = NULL;

        gWorldmapCarFrmWidth = 0;
        gWorldmapCarFrmHeight = 0;
    }

    _wmEncounterIconShow = 0;
    _EncounterMapID = -1;
    dword_672E50 = -1;
    dword_672E54 = -1;

    indicatorBarShow();
    isoEnable();
    colorCycleEnable();

    fontSetCurrent(_fontnum);

    if (gQuickDestinations != NULL) {
        internal_free(gQuickDestinations);
        gQuickDestinations = NULL;
    }

    gQuickDestinationsLength = 0;

    _wmInterfaceWasInitialized = 0;

    scriptsEnable();

    return 0;
}

// FIXME: There is small bug in this function. There is [success] flag returned
// by reference so that calling code can update scrolling mouse cursor to invalid
// range. It works OK on straight directions. But in diagonals when scrolling in
// one direction is possible (and in fact occured), it will still be reported as
// error.
//
// 0x4C3200
int worldmapWindowScroll(int stepX, int stepY, int dx, int dy, bool* success, bool shouldRefresh)
{
    int v6 = gWorldmapOffsetY;
    int v7 = gWorldmapOffsetX;

    if (success != NULL) {
        *success = true;
    }

    if (dy < 0) {
        if (v6 > 0) {
            v6 -= stepY;
            if (v6 < 0) {
                v6 = 0;
            }
        } else {
            if (success != NULL) {
                *success = false;
            }
        }
    } else if (dy > 0) {
        if (v6 < _wmViewportBottomtScrlLimit) {
            v6 += stepY;
            if (v6 > _wmViewportBottomtScrlLimit) {
                v6 = _wmViewportBottomtScrlLimit;
            }
        } else {
            if (success != NULL) {
                *success = false;
            }
        }
    }

    if (dx < 0) {
        if (v7 > 0) {
            v7 -= stepX;
            if (v7 < 0) {
                v7 = 0;
            }
        } else {
            if (success != NULL) {
                *success = false;
            }
        }
    } else if (dx > 0) {
        if (v7 < _wmViewportRightScrlLimit) {
            v7 += stepX;
            if (v7 > _wmViewportRightScrlLimit) {
                v7 = _wmViewportRightScrlLimit;
            }
        } else {
            if (success != NULL) {
                *success = false;
            }
        }
    }

    gWorldmapOffsetY = v6;
    gWorldmapOffsetX = v7;

    if (shouldRefresh) {
        if (worldmapWindowRefresh() == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4C32EC
void worldmapWindowHandleMouseScrolling()
{
    int x;
    int y;
    mouseGetPositionInWindow(gWorldmapWindow, &x, &y);

    int dx = 0;
    if (x == 639) {
        dx = 1;
    } else if (x == 0) {
        dx = -1;
    }

    int dy = 0;
    if (y == 479) {
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
        if (getTicksBetween(tick, _lastTime_2) > 50) {
            _lastTime_2 = _get_bk_time();
            worldmapWindowScroll(20, 20, dx, dy, &_couldScroll, true);
        }

        if (!_couldScroll) {
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

// 0x4C3434
int _wmMarkSubTileOffsetVisitedFunc(int a1, int a2, int a3, int a4, int a5, int a6)
{
    int v7;
    int v8;
    int v9;
    int* v;

    v7 = a2 + a4;
    v8 = a1;
    v9 = a3 + a5;

    if (v7 >= 0) {
        if (v7 >= 7) {
            if (a1 % gWorldmapGridWidth == gWorldmapGridWidth - 1) {
                return -1;
            }

            v8 = a1 + 1;
            v7 %= 7;
        }
    } else {
        if (!(a1 % gWorldmapGridWidth)) {
            return -1;
        }

        v7 += 7;
        v8 = a1 - 1;
    }

    if (v9 >= 0) {
        if (v9 >= 6) {
            if (v8 > gWorldmapTilesLength - gWorldmapGridWidth - 1) {
                return -1;
            }

            v8 += gWorldmapGridWidth;
            v9 %= 6;
        }
    } else {
        if (v8 < gWorldmapGridWidth) {
            return -1;
        }

        v9 += 6;
        v8 -= gWorldmapGridWidth;
    }

    TileInfo* tile = &(gWorldmapTiles[v8]);
    SubtileInfo* subtile = &(tile->subtiles[v9][v7]);
    v = &(subtile->state);
    if (a6 != 1 || *v == 0) {
        *v = a6;
    }

    return 0;
}

// 0x4C3550
void _wmMarkSubTileRadiusVisited(int x, int y)
{
    int radius = 1;

    if (perkHasRank(gDude, PERK_SCOUT)) {
        radius = 2;
    }

    _wmSubTileMarkRadiusVisited(x, y, radius);
}

// Mark worldmap tile as visible?/visited?
//
// 0x4C35A8
int _wmSubTileMarkRadiusVisited(int x, int y, int radius)
{
    int v4, v5;

    int tile = x / WM_TILE_WIDTH % gWorldmapGridWidth + y / WM_TILE_HEIGHT * gWorldmapGridWidth;
    v4 = x % WM_TILE_WIDTH / WM_SUBTILE_SIZE;
    v5 = y % WM_TILE_HEIGHT / WM_SUBTILE_SIZE;

    for (int i = -radius; i <= radius; i++) {
        for (int v6 = -radius; v6 <= radius; v6++) {
            _wmMarkSubTileOffsetVisitedFunc(tile, v4, v5, v6, i, SUBTILE_STATE_KNOWN);
        }
    }

    SubtileInfo* subtile = &(gWorldmapTiles[tile].subtiles[v5][v4]);
    subtile->state = SUBTILE_STATE_VISITED;

    switch (subtile->field_4) {
    case 2:
        while (v5-- > 0) {
            _wmMarkSubTileOffsetVisitedFunc(tile, v4, 0, v5, 0, SUBTILE_STATE_VISITED);
        }
        break;
    case 4:
        while (v4-- > -1) {
            _wmMarkSubTileOffsetVisitedFunc(tile, v4, 0, v5, 0, SUBTILE_STATE_VISITED);
        }

        if (tile % gWorldmapGridWidth > 0) {
            for (int i = 0; i < 7; i++) {
                _wmMarkSubTileOffsetVisitedFunc(tile - 1, i + 1, v5, 0, 0, SUBTILE_STATE_VISITED);
            }
        }
        break;
    }

    return 0;
}

// 0x4C3740
int _wmSubTileGetVisitedState(int x, int y, int* a3)
{
    TileInfo* tile;
    SubtileInfo* ptr;

    tile = &(gWorldmapTiles[y / WM_TILE_HEIGHT * gWorldmapGridWidth + x / WM_TILE_WIDTH % gWorldmapGridWidth]);
    ptr = &(tile->subtiles[y % WM_TILE_HEIGHT / WM_SUBTILE_SIZE][x % WM_TILE_WIDTH / WM_SUBTILE_SIZE]);
    *a3 = ptr->state;

    return 0;
}

// Load tile art if needed.
//
// 0x4C37EC
int _wmTileGrabArt(int tile_index)
{
    TileInfo* tile = &(gWorldmapTiles[tile_index]);
    if (tile->data != NULL) {
        return 0;
    }

    tile->data = artLockFrameData(tile->fid, 0, 0, &(tile->handle));
    if (tile->data != NULL) {
        return 0;
    }

    worldmapWindowFree();

    return -1;
}

// 0x4C3830
int worldmapWindowRefresh()
{
    if (_wmInterfaceWasInitialized != 1) {
        return 0;
    }

    int v17 = gWorldmapOffsetX % WM_TILE_WIDTH;
    int v18 = gWorldmapOffsetY % WM_TILE_HEIGHT;
    int v20 = WM_TILE_HEIGHT - v18;
    int v21 = WM_TILE_WIDTH * v18;
    int v19 = WM_TILE_WIDTH - v17;

    // Render tiles.
    int y = 0;
    int x = 0;
    int v0 = gWorldmapOffsetY / WM_TILE_HEIGHT * gWorldmapGridWidth + gWorldmapOffsetX / WM_TILE_WIDTH % gWorldmapGridWidth;
    while (y < WM_VIEW_HEIGHT) {
        x = 0;
        int v23 = 0;
        int height;
        while (x < WM_VIEW_WIDTH) {
            if (_wmTileGrabArt(v0) == -1) {
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

            TileInfo* tileInfo = &(gWorldmapTiles[v0]);
            blitBufferToBuffer(tileInfo->data + srcX,
                width,
                height,
                WM_TILE_WIDTH,
                gWorldmapWindowBuffer + WM_WINDOW_WIDTH * (y + WM_VIEW_Y) + WM_VIEW_X + x,
                WM_WINDOW_WIDTH);
            v0++;

            x += width;
            v23++;
        }

        v0 += gWorldmapGridWidth - v23;
        y += height;
    }

    // Render cities.
    for (int index = 0; index < gCitiesLength; index++) {
        CityInfo* cityInfo = &(gCities[index]);
        if (cityInfo->state != CITY_STATE_UNKNOWN) {
            CitySizeDescription* citySizeDescription = &(gCitySizeDescriptions[cityInfo->size]);
            int cityX = cityInfo->x - gWorldmapOffsetX;
            int cityY = cityInfo->y - gWorldmapOffsetY;
            if (cityX >= 0 && cityX <= 472 - citySizeDescription->width
                && cityY >= 0 && cityY <= 465 - citySizeDescription->height) {
                worldmapWindowRenderCity(cityInfo, citySizeDescription, gWorldmapWindowBuffer, cityX, cityY);
            }
        }
    }

    // Hide unknown subtiles, dim unvisited.
    int v25 = gWorldmapOffsetX / WM_TILE_WIDTH % gWorldmapGridWidth + gWorldmapOffsetY / WM_TILE_HEIGHT * gWorldmapGridWidth;
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
                    TileInfo* tileInfo = &(gWorldmapTiles[v25]);
                    worldmapWindowDimSubtile(tileInfo, column, row, v15, v34, 1);

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

        v25 += gWorldmapGridWidth - v24;
        v30 += v29;
    }

    _wmDrawCursorStopped();

    worldmapWindowRenderChrome(true);

    return 0;
}

// 0x4C3C9C
void worldmapWindowRenderDate(bool shouldRefreshWindow)
{
    int month;
    int day;
    int year;
    gameTimeGetDate(&month, &day, &year);

    month--;

    unsigned char* dest = gWorldmapWindowBuffer;

    int numbersFrmWidth = artGetWidth(gWorldmapNumbersFrm, 0, 0);
    int numbersFrmHeight = artGetHeight(gWorldmapNumbersFrm, 0, 0);
    unsigned char* numbersFrmData = artGetFrameData(gWorldmapNumbersFrm, 0, 0);

    dest += WM_WINDOW_WIDTH * 12 + 487;
    blitBufferToBuffer(numbersFrmData + 9 * (day / 10), 9, numbersFrmHeight, numbersFrmWidth, dest, WM_WINDOW_WIDTH);
    blitBufferToBuffer(numbersFrmData + 9 * (day % 10), 9, numbersFrmHeight, numbersFrmWidth, dest + 9, WM_WINDOW_WIDTH);

    int monthsFrmWidth = artGetWidth(gWorldmapMonthsFrm, 0, 0);
    unsigned char* monthsFrmData = artGetFrameData(gWorldmapMonthsFrm, 0, 0);
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
        windowRefreshRect(gWorldmapWindow, &rect);
    }
}

// 0x4C3F00
int _wmMatchWorldPosToArea(int a1, int a2, int* a3)
{
    int v3 = a2 + WM_VIEW_Y;
    int v4 = a1 + WM_VIEW_X;

    int index;
    for (index = 0; index < gCitiesLength; index++) {
        CityInfo* city = &(gCities[index]);
        if (city->state) {
            if (v4 >= city->x && v3 >= city->y) {
                CitySizeDescription* citySizeDescription = &(gCitySizeDescriptions[city->size]);
                if (v4 <= city->x + citySizeDescription->width && v3 <= city->y + citySizeDescription->height) {
                    break;
                }
            }
        }
    }

    if (index == gCitiesLength) {
        *a3 = -1;
    } else {
        *a3 = index;
    }

    return 0;
}

// FIXME: This function does not set current font, which is a bit unusual for a
// function which draw text. I doubt it was done on purpose, likely simply
// forgotten. Because of this, city names are rendered with current font, which
// can be any, but in this case it uses default text font, not interface font.
//
// 0x4C3FA8
int worldmapWindowRenderCity(CityInfo* city, CitySizeDescription* citySizeDescription, unsigned char* dest, int x, int y)
{
    _dark_translucent_trans_buf_to_buf(citySizeDescription->data,
        citySizeDescription->width,
        citySizeDescription->height,
        citySizeDescription->width,
        dest,
        x,
        y,
        WM_WINDOW_WIDTH,
        0x10000,
        _circleBlendTable,
        _commonGrayTable);

    int nameY = y + citySizeDescription->height + 1;
    int maxY = 464 - fontGetLineHeight();
    if (nameY < maxY) {
        MessageListItem messageListItem;
        const char* name;
        if (_wmAreaIsKnown(city->field_28)) {
            name = getmsg(&gMapMessageList, &messageListItem, 1500 + city->field_28);
        } else {
            name = getmsg(&gWorldmapMessageList, &messageListItem, 1004);
        }

        char text[40];
        strncpy(text, name, 40);

        int width = fontGetStringWidth(text);
        fontDrawText(dest + WM_WINDOW_WIDTH * nameY + x + citySizeDescription->width / 2 - width / 2, text, width, WM_WINDOW_WIDTH, _colorTable[992]);
    }

    return 0;
}

// Helper function that dims specified rectangle in given buffer. It's used to
// slightly darken subtile which is known, but not visited.
//
// 0x4C40A8
void worldmapWindowDimRect(unsigned char* dest, int width, int height, int pitch)
{
    int skipY = pitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char byte = *dest;
            unsigned int index = (byte << 8) + 75;
            *dest++ = _intensityColorTable[index];
        }
        dest += skipY;
    }
}

// 0x4C40E4
int worldmapWindowDimSubtile(TileInfo* tileInfo, int column, int row, int x, int y, int a6)
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
        unsigned char* dest = gWorldmapWindowBuffer + WM_WINDOW_WIDTH * destY + destX;
        switch (subtileInfo->state) {
        case SUBTILE_STATE_UNKNOWN:
            bufferFill(dest, width, height, WM_WINDOW_WIDTH, _colorTable[0]);
            break;
        case SUBTILE_STATE_KNOWN:
            worldmapWindowDimRect(dest, width, height, WM_WINDOW_WIDTH);
            break;
        }
    }

    return 0;
}

// 0x4C41EC
int _wmDrawCursorStopped()
{
    unsigned char* src;
    int width;
    int height;

    if (gWorldmapTravelDestX >= 1 || gWorldmapTravelDestY >= 1) {

        if (_wmEncounterIconShow == 1) {
            src = gWorldmapEncounterFrmData[_wmRndCursorFid];
            width = gWorldmapEncounterFrmWidths[_wmRndCursorFid];
            height = gWorldmapEncounterFrmHeights[_wmRndCursorFid];
        } else {
            src = gWorldmapLocationMarkerFrmData;
            width = gWorldmapLocationMarkerFrmWidth;
            height = gWorldmapLocationMarkerFrmHeight;
        }

        if (_world_xpos >= gWorldmapOffsetX && _world_xpos < gWorldmapOffsetX + WM_VIEW_WIDTH
            && _world_ypos >= gWorldmapOffsetY && _world_ypos < gWorldmapOffsetY + WM_VIEW_HEIGHT) {
            blitBufferToBufferTrans(src, width, height, width, gWorldmapWindowBuffer + WM_WINDOW_WIDTH * (WM_VIEW_Y - gWorldmapOffsetY + _world_ypos - height / 2) + WM_VIEW_X - gWorldmapOffsetX + _world_xpos - width / 2, WM_WINDOW_WIDTH);
        }

        if (gWorldmapTravelDestX >= gWorldmapOffsetX && gWorldmapTravelDestX < gWorldmapOffsetX + WM_VIEW_WIDTH
            && gWorldmapTravelDestY >= gWorldmapOffsetY && gWorldmapTravelDestY < gWorldmapOffsetY + WM_VIEW_HEIGHT) {
            blitBufferToBufferTrans(gWorldmapDestinationMarkerFrmData, gWorldmapDestinationMarkerFrmWidth, gWorldmapDestinationMarkerFrmHeight, gWorldmapDestinationMarkerFrmWidth, gWorldmapWindowBuffer + WM_WINDOW_WIDTH * (WM_VIEW_Y - gWorldmapOffsetY + gWorldmapTravelDestY - gWorldmapDestinationMarkerFrmHeight / 2) + WM_VIEW_X - gWorldmapOffsetX + gWorldmapTravelDestX - gWorldmapDestinationMarkerFrmWidth / 2, WM_WINDOW_WIDTH);
        }
    } else {
        if (_wmEncounterIconShow == 1) {
            src = gWorldmapEncounterFrmData[_wmRndCursorFid];
            width = gWorldmapEncounterFrmWidths[_wmRndCursorFid];
            height = gWorldmapEncounterFrmHeights[_wmRndCursorFid];
        } else {
            src = _wmGenData ? gWorldmapHotspotDownFrmData : gWorldmapHotspotUpFrmData;
            width = gWorldmapHotspotUpFrmWidth;
            height = gWorldmapHotspotUpFrmHeight;
        }

        if (_world_xpos >= gWorldmapOffsetX && _world_xpos < gWorldmapOffsetX + WM_VIEW_WIDTH
            && _world_ypos >= gWorldmapOffsetY && _world_ypos < gWorldmapOffsetY + WM_VIEW_HEIGHT) {
            blitBufferToBufferTrans(src, width, height, width, gWorldmapWindowBuffer + WM_WINDOW_WIDTH * (WM_VIEW_Y - gWorldmapOffsetY + _world_ypos - height / 2) + WM_VIEW_X - gWorldmapOffsetX + _world_xpos - width / 2, WM_WINDOW_WIDTH);
        }
    }

    return 0;
}

// 0x4C4490
bool _wmCursorIsVisible()
{
    return _world_xpos >= gWorldmapOffsetX
        && _world_ypos >= gWorldmapOffsetY
        && _world_xpos < gWorldmapOffsetX + WM_VIEW_WIDTH
        && _world_ypos < gWorldmapOffsetY + WM_VIEW_HEIGHT;
}

// Copy city short name.
//
// 0x4C450C
int _wmGetAreaIdxName(int index, char* name)
{
    MessageListItem messageListItem;

    getmsg(&gMapMessageList, &messageListItem, 1500 + index);
    strncpy(name, messageListItem.text, 40);

    return 0;
}

// Returns true if world area is known.
//
// 0x4C453C
bool _wmAreaIsKnown(int cityIndex)
{
    if (!cityIsValid(cityIndex)) {
        return false;
    }

    CityInfo* city = &(gCities[cityIndex]);
    if (city->field_40) {
        if (city->state == CITY_STATE_KNOWN) {
            return true;
        }
    }

    return false;
}

// 0x4C457C
int _wmAreaVisitedState(int area)
{
    if (!cityIsValid(area)) {
        return 0;
    }

    CityInfo* city = &(gCities[area]);
    if (city->field_40 && city->state == 1) {
        return city->field_40;
    }

    return 0;
}

// 0x4C45BC
bool _wmMapIsKnown(int mapIndex)
{
    int cityIndex;
    if (_wmMatchAreaFromMap(mapIndex, &cityIndex) != 0) {
        return false;
    }

    int entranceIndex;
    if (_wmMatchEntranceFromMap(cityIndex, mapIndex, &entranceIndex) != 0) {
        return false;
    }

    CityInfo* city = &(gCities[cityIndex]);
    EntranceInfo* entrance = &(city->entrances[entranceIndex]);

    if (entrance->state != 1) {
        return false;
    }

    return true;
}

// 0x4C4634
bool _wmAreaMarkVisitedState(int cityIndex, int a2)
{
    if (!cityIsValid(cityIndex)) {
        return false;
    }

    CityInfo* city = &(gCities[cityIndex]);
    int v5 = city->field_40;
    if (city->state == 1 && a2 != 0) {
        _wmMarkSubTileRadiusVisited(city->x, city->y);
    }

    city->field_40 = a2;

    SubtileInfo* subtile;
    if (_wmFindCurSubTileFromPos(city->x, city->y, &subtile) == -1) {
        return false;
    }

    if (a2 == 1) {
        subtile->state = SUBTILE_STATE_KNOWN;
    } else if (a2 == 2 && v5 == 0) {
        city->field_40 = 1;
    }

    return true;
}

// 0x4C46CC
bool _wmAreaSetVisibleState(int cityIndex, int a2, int a3)
{
    if (!cityIsValid(cityIndex)) {
        return false;
    }

    CityInfo* city = &(gCities[cityIndex]);
    if (city->field_3C != 1 || a3) {
        city->state = a2;
        return true;
    }

    return false;
}

// wm_area_set_pos
// 0x4C4710
int worldmapCitySetPos(int cityIndex, int x, int y)
{
    if (!cityIsValid(cityIndex)) {
        return -1;
    }

    if (x < 0 || x >= WM_TILE_WIDTH * gWorldmapGridWidth) {
        return -1;
    }

    if (y < 0 || y >= WM_TILE_HEIGHT * (gWorldmapTilesLength / gWorldmapGridWidth)) {
        return -1;
    }

    CityInfo* city = &(gCities[cityIndex]);
    city->x = x;
    city->y = y;

    return 0;
}

// Returns current town x/y.
//
// 0x4C47A4
int _wmGetPartyWorldPos(int* out_x, int* out_y)
{
    if (out_x != NULL) {
        *out_x = _world_xpos;
    }

    if (out_y != NULL) {
        *out_y = _world_ypos;
    }

    return 0;
}

// Returns current town.
//
// 0x4C47C0
int _wmGetPartyCurArea(int* a1)
{
    if (a1) {
        *a1 = _WorldMapCurrArea;
        return 0;
    }

    return -1;
}

// 0x4C47D8
void _wmMarkAllSubTiles(int a1)
{
    for (int tileIndex = 0; tileIndex < gWorldmapTilesLength; tileIndex++) {
        TileInfo* tile = &(gWorldmapTiles[tileIndex]);
        for (int column = 0; column < SUBTILE_GRID_HEIGHT; column++) {
            for (int row = 0; row < SUBTILE_GRID_WIDTH; row++) {
                SubtileInfo* subtile = &(tile->subtiles[column][row]);
                subtile->state = a1;
            }
        }
    }
}

// 0x4C4850
void _wmTownMap()
{
    _wmWorldMapFunc(1);
}

// 0x4C485C
int worldmapCityMapViewSelect(int* mapIndexPtr)
{
    *mapIndexPtr = -1;

    if (worldmapCityMapViewInit() == -1) {
        worldmapCityMapViewFree();
        return -1;
    }

    if (_WorldMapCurrArea == -1) {
        return -1;
    }

    CityInfo* city = &(gCities[_WorldMapCurrArea]);

    for (;;) {
        int keyCode = _get_input();
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

                *mapIndexPtr = entrance->map;

                mapSetEnteringLocation(entrance->elevation, entrance->tile, entrance->rotation);

                break;
            }

            if (keyCode >= KEY_CTRL_F1 && keyCode <= KEY_CTRL_F7) {
                int v10 = _LastTabsYOffset / 27 + keyCode - KEY_CTRL_F1;
                if (v10 < gQuickDestinationsLength) {
                    int v11 = gQuickDestinations[v10];
                    CityInfo* v12 = &(gCities[v11]);
                    if (!_wmAreaIsKnown(v12->field_28)) {
                        break;
                    }

                    if (v11 != _WorldMapCurrArea) {
                        _wmPartyInitWalking(v12->x, v12->y);

                        _wmGenData = 0;

                        break;
                    }
                }
            } else {
                if (keyCode == KEY_CTRL_ARROW_UP) {
                    _wmInterfaceScrollTabsStart(-27);
                } else if (keyCode == KEY_CTRL_ARROW_DOWN) {
                    _wmInterfaceScrollTabsStart(27);
                } else if (keyCode == 2069) {
                    if (worldmapCityMapViewRefresh() == -1) {
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
    }

    if (worldmapCityMapViewFree() == -1) {
        return -1;
    }

    return 0;
}

// 0x4C4A6C
int worldmapCityMapViewInit()
{
    _wmTownMapCurArea = _WorldMapCurrArea;

    CityInfo* city = &(gCities[_WorldMapCurrArea]);

    Art* mapFrm = artLock(city->mapFid, &gWorldmapCityMapFrmHandle);
    if (mapFrm == NULL) {
        return -1;
    }

    gWorldmapCityMapFrmWidth = artGetWidth(mapFrm, 0, 0);
    gWorldmapCityMapFrmHeight = artGetHeight(mapFrm, 0, 0);

    artUnlock(gWorldmapCityMapFrmHandle);
    gWorldmapCityMapFrmHandle = INVALID_CACHE_ENTRY;

    gWorldmapCityMapFrmData = artLockFrameData(city->mapFid, 0, 0, &gWorldmapCityMapFrmHandle);
    if (gWorldmapCityMapFrmData == NULL) {
        return -1;
    }

    for (int index = 0; index < city->entrancesLength; index++) {
        _wmTownMapButtonId[index] = -1;
    }

    for (int index = 0; index < city->entrancesLength; index++) {
        EntranceInfo* entrance = &(city->entrances[index]);
        if (entrance->state == 0) {
            continue;
        }

        if (entrance->x == -1 || entrance->y == -1) {
            continue;
        }

        _wmTownMapButtonId[index] = buttonCreate(gWorldmapWindow,
            entrance->x,
            entrance->y,
            gWorldmapHotspotUpFrmWidth,
            gWorldmapHotspotUpFrmHeight,
            -1,
            2069,
            -1,
            KEY_1 + index,
            gWorldmapHotspotUpFrmData,
            gWorldmapHotspotDownFrmData,
            NULL,
            BUTTON_FLAG_TRANSPARENT);

        if (_wmTownMapButtonId[index] == -1) {
            return -1;
        }
    }

    tickersRemove(worldmapWindowHandleMouseScrolling);

    if (worldmapCityMapViewRefresh() == -1) {
        return -1;
    }

    return 0;
}

// 0x4C4BD0
int worldmapCityMapViewRefresh()
{
    blitBufferToBuffer(gWorldmapCityMapFrmData,
        gWorldmapCityMapFrmWidth,
        gWorldmapCityMapFrmHeight,
        gWorldmapCityMapFrmWidth,
        gWorldmapWindowBuffer + WM_WINDOW_WIDTH * WM_VIEW_Y + WM_VIEW_X,
        WM_WINDOW_WIDTH);

    worldmapWindowRenderChrome(false);

    CityInfo* city = &(gCities[_WorldMapCurrArea]);

    for (int index = 0; index < city->entrancesLength; index++) {
        EntranceInfo* entrance = &(city->entrances[index]);
        if (entrance->state == 0) {
            continue;
        }

        if (entrance->x == -1 || entrance->y == -1) {
            continue;
        }

        MessageListItem messageListItem;
        messageListItem.num = 200 + 10 * _wmTownMapCurArea + index;
        if (messageListGetItem(&gWorldmapMessageList, &messageListItem)) {
            if (messageListItem.text != NULL) {
                int width = fontGetStringWidth(messageListItem.text);
                windowDrawText(gWorldmapWindow, messageListItem.text, width, gWorldmapHotspotUpFrmWidth / 2 + entrance->x - width / 2, gWorldmapHotspotUpFrmHeight + entrance->y + 2, _colorTable[992] | 0x2010000);
            }
        }
    }

    windowRefresh(gWorldmapWindow);

    return 0;
}

// 0x4C4D00
int worldmapCityMapViewFree()
{
    if (gWorldmapCityMapFrmHandle != INVALID_CACHE_ENTRY) {
        artUnlock(gWorldmapCityMapFrmHandle);
        gWorldmapCityMapFrmHandle = INVALID_CACHE_ENTRY;
        gWorldmapCityMapFrmData = NULL;
        gWorldmapCityMapFrmWidth = 0;
        gWorldmapCityMapFrmHeight = 0;
    }

    if (_wmTownMapCurArea != -1) {
        CityInfo* city = &(gCities[_wmTownMapCurArea]);
        for (int index = 0; index < city->entrancesLength; index++) {
            if (_wmTownMapButtonId[index] != -1) {
                buttonDestroy(_wmTownMapButtonId[index]);
                _wmTownMapButtonId[index] = -1;
            }
        }
    }

    if (worldmapWindowRefresh() == -1) {
        return -1;
    }

    tickersAdd(worldmapWindowHandleMouseScrolling);

    return 0;
}

// 0x4C4DA4
int carConsumeFuel(int amount)
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

    gWorldmapCarFuel -= amount;

    if (gWorldmapCarFuel < 0) {
        gWorldmapCarFuel = 0;
    }

    return 0;
}

// Returns amount of fuel that does not fit into tank.
//
// 0x4C4E34
int carAddFuel(int amount)
{
    if ((amount + gWorldmapCarFuel) <= CAR_FUEL_MAX) {
        gWorldmapCarFuel += amount;
        return 0;
    }

    int remaining = CAR_FUEL_MAX - gWorldmapCarFuel;

    gWorldmapCarFuel = CAR_FUEL_MAX;

    return remaining;
}

// 0x4C4E74
int carGetFuel()
{
    return gWorldmapCarFuel;
}

// 0x4C4E7C
bool carIsEmpty()
{
    return gWorldmapCarFuel <= 0;
}

// 0x4C4E8C
int carGetCity()
{
    return _carCurrentArea;
}

// 0x4C4E94
int _wmCarGiveToParty()
{
    MessageListItem messageListItem;
    memcpy(&messageListItem, &stru_4BC880, sizeof(MessageListItem));

    if (gWorldmapCarFuel <= 0) {
        // The car is out of power.
        char* msg = getmsg(&gWorldmapMessageList, &messageListItem, 1502);
        displayMonitorAddMessage(msg);
        return -1;
    }

    gWorldmapIsInCar = true;

    MapTransition transition;
    memset(&transition, 0, sizeof(transition));

    transition.map = -2;
    mapSetTransition(&transition);

    CityInfo* city = &(gCities[CITY_CAR_OUT_OF_GAS]);
    city->state = 0;
    city->field_40 = 0;

    return 0;
}

// 0x4C4F28
int ambientSoundEffectGetLength()
{
    int mapIndex = mapGetCurrentMap();
    if (mapIndex < 0 || mapIndex >= gMapsLength) {
        return -1;
    }

    MapInfo* map = &(gMaps[mapIndex]);
    return map->ambientSoundEffectsLength;
}

// 0x4C4F5C
int ambientSoundEffectGetRandom()
{
    int mapIndex = mapGetCurrentMap();
    if (mapIndex < 0 || mapIndex >= gMapsLength) {
        return -1;
    }

    MapInfo* map = &(gMaps[mapIndex]);

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
int ambientSoundEffectGetName(int ambientSoundEffectIndex, char** namePtr)
{
    if (namePtr == NULL) {
        return -1;
    }

    *namePtr = NULL;

    int mapIndex = mapGetCurrentMap();
    if (mapIndex < 0 || mapIndex >= gMapsLength) {
        return -1;
    }

    MapInfo* map = &(gMaps[mapIndex]);
    if (ambientSoundEffectIndex < 0 || ambientSoundEffectIndex >= map->ambientSoundEffectsLength) {
        return -1;
    }

    MapAmbientSoundEffectInfo* ambientSoundEffectInfo = &(map->ambientSoundEffects[ambientSoundEffectIndex]);
    *namePtr = ambientSoundEffectInfo->name;

    int v1 = 0;
    if (strcmp(ambientSoundEffectInfo->name, "brdchir1") == 0) {
        v1 = 1;
    } else if (strcmp(ambientSoundEffectInfo->name, "brdchirp") == 0) {
        v1 = 2;
    }

    if (v1 != 0) {
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
            *namePtr = _wmRemapSfxList[v1 - 1];
        }
    }

    return 0;
}

// 0x4C50F4
int worldmapWindowRenderChrome(bool shouldRefreshWindow)
{
    blitBufferToBufferTrans(gWorldmapBoxFrmData,
        gWorldmapBoxFrmWidth,
        gWorldmapBoxFrmHeight,
        gWorldmapBoxFrmWidth,
        gWorldmapWindowBuffer,
        WM_WINDOW_WIDTH);

    worldmapRenderQuickDestinations();

    int v1 = gameTimeGetHour();
    v1 /= 100;

    int frameCount = artGetFrameCount(gWorldmapDialFrm);
    int newFrame = (v1 + 12) % frameCount;
    if (gWorldmapDialFrmCurrentFrame != newFrame) {
        gWorldmapDialFrmCurrentFrame = newFrame;
        worldmapWindowRenderDial(false);
    }

    worldmapWindowRenderDial(false);

    if (gWorldmapIsInCar) {
        unsigned char* data = artGetFrameData(gWorldmapCarFrm, gWorldmapCarFrmCurrentFrame, 0);
        if (data == NULL) {
            return -1;
        }

        blitBufferToBuffer(data,
            gWorldmapCarFrmWidth,
            gWorldmapCarFrmHeight,
            gWorldmapCarFrmWidth,
            gWorldmapWindowBuffer + WM_WINDOW_WIDTH * WM_WINDOW_CAR_Y + WM_WINDOW_CAR_X,
            WM_WINDOW_WIDTH);

        blitBufferToBufferTrans(gWorldmapCarOverlayFrmData,
            gWorldmapCarOverlayFrmWidth,
            gWorldmapCarOverlayFrmHeight,
            gWorldmapCarOverlayFrmWidth,
            gWorldmapWindowBuffer + WM_WINDOW_WIDTH * WM_WINDOW_CAR_OVERLAY_Y + WM_WINDOW_CAR_OVERLAY_X,
            WM_WINDOW_WIDTH);

        worldmapWindowRenderCarFuelBar();
    } else {
        blitBufferToBufferTrans(gWorldmapGlobeOverlayFrmData,
            gWorldmapGlobeOverlayFrmWidth,
            gWorldmapGloveOverlayFrmHeight,
            gWorldmapGlobeOverlayFrmWidth,
            gWorldmapWindowBuffer + WM_WINDOW_WIDTH * WM_WINDOW_GLOBE_OVERLAY_Y + WM_WINDOW_GLOBE_OVERLAY_X,
            WM_WINDOW_WIDTH);
    }

    worldmapWindowRenderDate(false);

    if (shouldRefreshWindow) {
        windowRefresh(gWorldmapWindow);
    }

    return 0;
}

// 0x4C5244
void worldmapWindowRenderCarFuelBar()
{
    int ratio = (WM_WINDOW_CAR_FUEL_BAR_HEIGHT * gWorldmapCarFuel) / CAR_FUEL_MAX;
    if ((ratio & 1) != 0) {
        ratio -= 1;
    }

    unsigned char* dest = gWorldmapWindowBuffer + WM_WINDOW_WIDTH * WM_WINDOW_CAR_FUEL_BAR_Y + WM_WINDOW_CAR_FUEL_BAR_X;

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
int worldmapRenderQuickDestinations()
{
    unsigned char* v30;
    unsigned char* v0;
    int v31;
    CityInfo* city;
    Art* art;
    CacheEntry* cache_entry;
    int width;
    int height;
    unsigned char* buf;
    int v10;
    unsigned char* v11;
    unsigned char* v12;
    int v32;
    unsigned char* v13;

    blitBufferToBufferTrans(gWorldmapTownTabsUnderlayFrmData + gWorldmapTownTabsUnderlayFrmWidth * _LastTabsYOffset + 9, 119, 178, gWorldmapTownTabsUnderlayFrmWidth, gWorldmapWindowBuffer + WM_WINDOW_WIDTH * 135 + 501, WM_WINDOW_WIDTH);

    v30 = gWorldmapWindowBuffer + WM_WINDOW_WIDTH * 138 + 530;
    v0 = gWorldmapWindowBuffer + WM_WINDOW_WIDTH * 138 + 530 - WM_WINDOW_WIDTH * (_LastTabsYOffset % 27);
    v31 = _LastTabsYOffset / 27;

    if (v31 < gQuickDestinationsLength) {
        city = &(gCities[gQuickDestinations[v31]]);
        if (city->labelFid != -1) {
            art = artLock(city->labelFid, &cache_entry);
            if (art == NULL) {
                return -1;
            }

            width = artGetWidth(art, 0, 0);
            height = artGetHeight(art, 0, 0);
            buf = artGetFrameData(art, 0, 0);
            if (buf == NULL) {
                return -1;
            }

            v10 = height - _LastTabsYOffset % 27;
            v11 = buf + width * (_LastTabsYOffset % 27);

            v12 = v0;
            if (v0 < v30 - WM_WINDOW_WIDTH) {
                v12 = v30 - WM_WINDOW_WIDTH;
            }

            blitBufferToBuffer(v11, width, v10, width, v12, WM_WINDOW_WIDTH);
            artUnlock(cache_entry);
            cache_entry = INVALID_CACHE_ENTRY;
        }
    }

    v13 = v0 + WM_WINDOW_WIDTH * 27;
    v32 = v31 + 6;

    for (int v14 = v31 + 1; v14 < v32; v14++) {
        if (v14 < gQuickDestinationsLength) {
            city = &(gCities[gQuickDestinations[v14]]);
            if (city->labelFid != -1) {
                art = artLock(city->labelFid, &cache_entry);
                if (art == NULL) {
                    return -1;
                }

                width = artGetWidth(art, 0, 0);
                height = artGetHeight(art, 0, 0);
                buf = artGetFrameData(art, 0, 0);
                if (buf == NULL) {
                    return -1;
                }

                blitBufferToBuffer(buf, width, height, width, v13, WM_WINDOW_WIDTH);
                artUnlock(cache_entry);

                cache_entry = INVALID_CACHE_ENTRY;
            }
        }
        v13 += WM_WINDOW_WIDTH * 27;
    }

    if (v31 + 6 < gQuickDestinationsLength) {
        city = &(gCities[gQuickDestinations[v31 + 6]]);
        if (city->labelFid != -1) {
            art = artLock(city->labelFid, &cache_entry);
            if (art == NULL) {
                return -1;
            }

            width = artGetWidth(art, 0, 0);
            height = artGetHeight(art, 0, 0);
            buf = artGetFrameData(art, 0, 0);
            if (buf == NULL) {
                return -1;
            }

            blitBufferToBuffer(buf, width, height, width, v13, WM_WINDOW_WIDTH);
            artUnlock(cache_entry);

            cache_entry = INVALID_CACHE_ENTRY;
        }
    }

    blitBufferToBufferTrans(gWorldmapTownTabsEdgeFrmData, 119, 178, 119, gWorldmapWindowBuffer + WM_WINDOW_WIDTH * 135 + 501, WM_WINDOW_WIDTH);

    return 0;
}

// Creates array of cities available as quick destinations.
//
// 0x4C55D4
int _wmMakeTabsLabelList(int** quickDestinationsPtr, int* quickDestinationsLengthPtr)
{
    int* quickDestinations = *quickDestinationsPtr;

    if (quickDestinations != NULL) {
        internal_free(quickDestinations);
        quickDestinations = NULL;
    }

    *quickDestinationsPtr = NULL;
    *quickDestinationsLengthPtr = 0;

    int capacity = 10;

    quickDestinations = (int*)internal_malloc(sizeof(*quickDestinations) * capacity);
    *quickDestinationsPtr = quickDestinations;

    if (quickDestinations == NULL) {
        return -1;
    }

    int quickDestinationsLength = *quickDestinationsLengthPtr;
    for (int index = 0; index < gCitiesLength; index++) {
        if (_wmAreaIsKnown(index) && gCities[index].labelFid != -1) {
            quickDestinationsLength++;
            *quickDestinationsLengthPtr = quickDestinationsLength;

            if (capacity <= quickDestinationsLength) {
                capacity += 10;

                quickDestinations = (int*)internal_realloc(quickDestinations, sizeof(*quickDestinations) * capacity);
                if (quickDestinations == NULL) {
                    return -1;
                }

                *quickDestinationsPtr = quickDestinations;
            }

            quickDestinations[quickDestinationsLength - 1] = index;
        }
    }

    qsort(quickDestinations, quickDestinationsLength, sizeof(*quickDestinations), worldmapCompareCitiesByName);

    return 0;
}

// 0x4C56C8
int worldmapCompareCitiesByName(const void* a1, const void* a2)
{
    int v1 = *(int*)a1;
    int v2 = *(int*)a2;

    CityInfo* city1 = &(gCities[v1]);
    CityInfo* city2 = &(gCities[v2]);

    return stricmp(city1->name, city2->name);
}

// 0x4C5734
void worldmapWindowRenderDial(bool shouldRefreshWindow)
{
    unsigned char* data = artGetFrameData(gWorldmapDialFrm, gWorldmapDialFrmCurrentFrame, 0);
    blitBufferToBufferTrans(data,
        gWorldmapDialFrmWidth,
        gWorldmapDialFrmHeight,
        gWorldmapDialFrmWidth,
        gWorldmapWindowBuffer + WM_WINDOW_WIDTH * WM_WINDOW_DIAL_Y + WM_WINDOW_DIAL_X,
        WM_WINDOW_WIDTH);

    if (shouldRefreshWindow) {
        Rect rect;
        rect.left = WM_WINDOW_DIAL_X;
        rect.top = WM_WINDOW_DIAL_Y - 1;
        rect.right = rect.left + gWorldmapDialFrmWidth;
        rect.bottom = rect.top + gWorldmapDialFrmHeight;
        windowRefreshRect(gWorldmapWindow, &rect);
    }
}

// 0x4C5804
int _wmAreaFindFirstValidMap(int* out_a1)
{
    *out_a1 = -1;

    if (_WorldMapCurrArea == -1) {
        return -1;
    }

    CityInfo* city = &(gCities[_WorldMapCurrArea]);
    if (city->entrancesLength == 0) {
        return -1;
    }

    for (int index = 0; index < city->entrancesLength; index++) {
        EntranceInfo* entrance = &(city->entrances[index]);
        if (entrance->state != 0) {
            *out_a1 = entrance->map;
            return 0;
        }
    }

    EntranceInfo* entrance = &(city->entrances[0]);
    entrance->state = 1;

    *out_a1 = entrance->map;
    return 0;
}

// 0x4C58C0
int worldmapStartMapMusic()
{
    do {
        int mapIndex = mapGetCurrentMap();
        if (mapIndex == -1 || mapIndex >= gMapsLength) {
            break;
        }

        MapInfo* map = &(gMaps[mapIndex]);
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

// wmSetMapMusic
// 0x4C5928
int worldmapSetMapMusic(int mapIndex, const char* name)
{
    if (mapIndex == -1 || mapIndex >= gMapsLength) {
        return -1;
    }

    if (name == NULL) {
        return -1;
    }

    debugPrint("\nwmSetMapMusic: %d, %s", mapIndex, name);

    MapInfo* map = &(gMaps[mapIndex]);

    strncpy(map->music, name, 40);
    map->music[39] = '\0';

    if (mapGetCurrentMap() == mapIndex) {
        backgroundSoundDelete();
        worldmapStartMapMusic();
    }

    return 0;
}

// 0x4C59A4
int _wmMatchAreaContainingMapIdx(int mapIndex, int* cityIndexPtr)
{
    *cityIndexPtr = 0;

    for (int cityIndex = 0; cityIndex < gCitiesLength; cityIndex++) {
        CityInfo* cityInfo = &(gCities[cityIndex]);
        for (int entranceIndex = 0; entranceIndex < cityInfo->entrancesLength; entranceIndex++) {
            EntranceInfo* entranceInfo = &(cityInfo->entrances[entranceIndex]);
            if (entranceInfo->map == mapIndex) {
                *cityIndexPtr = cityIndex;
                return 0;
            }
        }
    }

    return -1;
}

// 0x4C5A1C
int _wmTeleportToArea(int cityIndex)
{
    if (!cityIsValid(cityIndex)) {
        return -1;
    }

    _WorldMapCurrArea = cityIndex;
    gWorldmapTravelDestX = 0;
    gWorldmapTravelDestY = 0;
    gWorldmapIsTravelling = false;

    CityInfo* city = &(gCities[cityIndex]);
    _world_xpos = city->x;
    _world_ypos = city->y;

    return 0;
}
