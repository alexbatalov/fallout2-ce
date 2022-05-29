#include "map.h"

#include "animation.h"
#include "automap.h"
#include "character_editor.h"
#include "color.h"
#include "combat.h"
#include "core.h"
#include "critter.h"
#include "cycle.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_config.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "interface.h"
#include "item.h"
#include "light.h"
#include "loadsave.h"
#include "memory.h"
#include "object.h"
#include "palette.h"
#include "pipboy.h"
#include "proto.h"
#include "proto_instance.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "text_object.h"
#include "tile.h"
#include "window_manager.h"
#include "world_map.h"

#include <stdio.h>
#include <string.h>

// 0x50B058
char byte_50B058[] = "";

// 0x50B30C
char _aErrorF2[] = "ERROR! F2";

// 0x519540
IsoWindowRefreshProc* _map_scroll_refresh = isoWindowRefreshRectGame;

// 0x519544
const int _map_data_elev_flags[ELEVATION_COUNT] = {
    2,
    4,
    8,
};

// 0x519550
unsigned int gIsoWindowScrollTimestamp = 0;

// 0x519554
bool gIsoEnabled = false;

// 0x519558
int gEnteringElevation = 0;

// 0x51955C
int gEnteringTile = -1;

// 0x519560
int gEnteringRotation = ROTATION_NE;

// 0x519564
int gMapSid = -1;

// local_vars
// 0x519568
int* gMapLocalVars = NULL;

// map_vars
// 0x51956C
int* gMapGlobalVars = NULL;

// local_vars_num
// 0x519570
int gMapLocalVarsLength = 0;

// map_vars_num
// 0x519574
int gMapGlobalVarsLength = 0;

// Current elevation.
//
// 0x519578
int gElevation = 0;

// 0x51957C
char* _errMapName = byte_50B058;

// 0x519584
int _wmMapIdx = -1;

// 0x614868
TileData _square_data[ELEVATION_COUNT];

// 0x631D28
MapTransition gMapTransition;

// 0x631D38
Rect gIsoWindowRect;

// map.msg
//
// map_msg_file
// 0x631D48
MessageList gMapMessageList;

// 0x631D50
unsigned char* gIsoWindowBuffer;

// 0x631D54
MapHeader gMapHeader;

// 0x631E40
TileData* _square[ELEVATION_COUNT];

// 0x631E4C
int gIsoWindow;

// 0x631E50
char _scratchStr[40];

// Last map file name.
//
// 0x631E78
char _map_path[COMPAT_MAX_PATH];

// iso_init
// 0x481CA0
int isoInit()
{
    tileScrollLimitingDisable();
    tileScrollBlockingDisable();

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        _square[elevation] = &(_square_data[elevation]);
    }

    gIsoWindow = windowCreate(0, 0, screenGetWidth(), screenGetVisibleHeight(), 256, 10);
    if (gIsoWindow == -1) {
        debugPrint("win_add failed in iso_init\n");
        return -1;
    }

    gIsoWindowBuffer = windowGetBuffer(gIsoWindow);
    if (gIsoWindowBuffer == NULL) {
        debugPrint("win_get_buf failed in iso_init\n");
        return -1;
    }

    if (windowGetRect(gIsoWindow, &gIsoWindowRect) != 0) {
        debugPrint("win_get_rect failed in iso_init\n");
        return -1;
    }

    if (artInit() != 0) {
        debugPrint("art_init failed in iso_init\n");
        return -1;
    }

    debugPrint(">art_init\t\t");

    if (tileInit(_square, SQUARE_GRID_WIDTH, SQUARE_GRID_HEIGHT, HEX_GRID_WIDTH, HEX_GRID_HEIGHT, gIsoWindowBuffer, screenGetWidth(), screenGetVisibleHeight(), screenGetWidth(), isoWindowRefreshRect) != 0) {
        debugPrint("tile_init failed in iso_init\n");
        return -1;
    }

    debugPrint(">tile_init\t\t");

    if (objectsInit(gIsoWindowBuffer, screenGetWidth(), screenGetVisibleHeight(), screenGetWidth()) != 0) {
        debugPrint("obj_init failed in iso_init\n");
        return -1;
    }

    debugPrint(">obj_init\t\t");

    colorCycleInit();
    debugPrint(">cycle_init\t\t");

    tileScrollBlockingEnable();
    tileScrollLimitingEnable();

    if (interfaceInit() != 0) {
        debugPrint("intface_init failed in iso_init\n");
        return -1;
    }

    debugPrint(">intface_init\t\t");

    mapMakeMapsDirectory();

    gEnteringElevation = -1;
    gEnteringTile = -1;
    gEnteringRotation = -1;

    return 0;
}

// 0x481ED4
void isoReset()
{
    if (gMapGlobalVars != NULL) {
        internal_free(gMapGlobalVars);
        gMapGlobalVars = NULL;
        gMapGlobalVarsLength = 0;
    }

    if (gMapLocalVars != NULL) {
        internal_free(gMapLocalVars);
        gMapLocalVars = NULL;
        gMapLocalVarsLength = 0;
    }

    artReset();
    tileReset();
    objectsReset();
    colorCycleReset();
    interfaceReset();
    gEnteringElevation = -1;
    gEnteringTile = -1;
    gEnteringRotation = -1;
}

// 0x481F48
void isoExit()
{
    interfaceFree();
    colorCycleFree();
    objectsExit();
    tileExit();
    artExit();

    if (gMapGlobalVars != NULL) {
        internal_free(gMapGlobalVars);
        gMapGlobalVars = NULL;
        gMapGlobalVarsLength = 0;
    }

    if (gMapLocalVars != NULL) {
        internal_free(gMapLocalVars);
        gMapLocalVars = NULL;
        gMapLocalVarsLength = 0;
    }
}

// 0x481FB4
void _map_init()
{
    char* executable;
    configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, "executable", &executable);
    if (compat_stricmp(executable, "mapper") == 0) {
        _map_scroll_refresh = isoWindowRefreshRectMapper;
    }

    if (messageListInit(&gMapMessageList)) {
        char path[COMPAT_MAX_PATH];
        sprintf(path, "%smap.msg", asc_5186C8);

        if (!messageListLoad(&gMapMessageList, path)) {
            debugPrint("\nError loading map_msg_file!");
        }
    } else {
        debugPrint("\nError initing map_msg_file!");
    }

    _map_new_map();
    tickersAdd(gameMouseRefresh);
    _gmouse_disable(0);
    windowUnhide(gIsoWindow);
}

// 0x482084
void _map_exit()
{
    windowHide(gIsoWindow);
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    tickersRemove(gameMouseRefresh);
    if (!messageListFree(&gMapMessageList)) {
        debugPrint("\nError exiting map_msg_file!");
    }
}

// 0x4820C0
void isoEnable()
{
    if (!gIsoEnabled) {
        textObjectsEnable();
        if (!gameUiIsDisabled()) {
            _gmouse_enable();
        }
        tickersAdd(_object_animate);
        tickersAdd(_dude_fidget);
        _scr_enable_critters();
        gIsoEnabled = true;
    }
}

// 0x482104
bool isoDisable()
{
    if (!gIsoEnabled) {
        return false;
    }

    _scr_disable_critters();
    tickersRemove(_dude_fidget);
    tickersRemove(_object_animate);
    _gmouse_disable(0);
    textObjectsDisable();

    gIsoEnabled = false;

    return true;
}

// 0x482148
bool isoIsDisabled()
{
    return gIsoEnabled == false;
}

// map_set_elevation
// 0x482158
int mapSetElevation(int elevation)
{
    if (!elevationIsValid(elevation)) {
        return -1;
    }

    bool gameMouseWasVisible = false;
    if (gameMouseGetCursor() != MOUSE_CURSOR_WAIT_PLANET) {
        gameMouseWasVisible = gameMouseObjectsIsVisible();
        gameMouseObjectsHide();
        gameMouseSetCursor(MOUSE_CURSOR_NONE);
    }

    if (elevation != gElevation) {
        _wmMapMarkMapEntranceState(gMapHeader.field_34, elevation, 1);
    }

    gElevation = elevation;

    reg_anim_clear(gDude);
    _dude_stand(gDude, gDude->rotation, gDude->fid);
    _partyMemberSyncPosition();

    if (gMapSid != -1) {
        scriptsExecMapUpdateProc();
    }

    if (gameMouseWasVisible) {
        gameMouseObjectsShow();
    }

    return 0;
}

// 0x482220
int mapSetGlobalVar(int var, int value)
{
    if (var < 0 || var >= gMapGlobalVarsLength) {
        debugPrint("ERROR: attempt to reference map var out of range: %d", var);
        return -1;
    }

    gMapGlobalVars[var] = value;

    return 0;
}

// 0x482250
int mapGetGlobalVar(int var)
{
    if (var < 0 || var >= gMapGlobalVarsLength) {
        debugPrint("ERROR: attempt to reference map var out of range: %d", var);
        return 0;
    }

    return gMapGlobalVars[var];
}

// 0x482280
int mapSetLocalVar(int var, int value)
{
    if (var < 0 || var >= gMapLocalVarsLength) {
        debugPrint("ERROR: attempt to reference local var out of range: %d", var);
        return -1;
    }

    gMapLocalVars[var] = value;

    return 0;
}

// 0x4822B0
int mapGetLocalVar(int var)
{
    if (var < 0 || var >= gMapLocalVarsLength) {
        debugPrint("ERROR: attempt to reference local var out of range: %d", var);
        return 0;
    }

    return gMapLocalVars[var];
}

// Make a room to store more local variables.
//
// 0x4822E0
int _map_malloc_local_var(int a1)
{
    int oldMapLocalVarsLength = gMapLocalVarsLength;
    gMapLocalVarsLength += a1;

    int* vars = (int*)internal_realloc(gMapLocalVars, sizeof(*vars) * gMapLocalVarsLength);
    if (vars == NULL) {
        debugPrint("\nError: Ran out of memory!");
    }

    gMapLocalVars = vars;
    memset((unsigned char*)vars + sizeof(*vars) * oldMapLocalVarsLength, 0, sizeof(*vars) * a1);

    return oldMapLocalVarsLength;
}

// 0x48234C
void mapSetStart(int tile, int elevation, int rotation)
{
    gMapHeader.enteringTile = tile;
    gMapHeader.enteringElevation = elevation;
    gMapHeader.enteringRotation = rotation;
}

// 0x4824CC
char* mapGetName(int map, int elevation)
{
    if (map < 0 || map >= mapGetCount()) {
        return NULL;
    }

    if (!elevationIsValid(elevation)) {
        return NULL;
    }

    MessageListItem messageListItem;
    return getmsg(&gMapMessageList, &messageListItem, map * 3 + elevation + 200);
}

// TODO: Check, probably returns true if map1 and map2 represents the same city.
//
// 0x482528
bool _is_map_idx_same(int map1, int map2)
{
    if (map1 < 0 || map1 >= mapGetCount()) {
        return 0;
    }

    if (map2 < 0 || map2 >= mapGetCount()) {
        return 0;
    }

    if (!_wmMapIdxIsSaveable(map1)) {
        return 0;
    }

    if (!_wmMapIdxIsSaveable(map2)) {
        return 0;
    }

    int city1;
    if (_wmMatchAreaContainingMapIdx(map1, &city1) == -1) {
        return 0;
    }

    int city2;
    if (_wmMatchAreaContainingMapIdx(map2, &city2) == -1) {
        return 0;
    }

    return city1 == city2;
}

// 0x4825CC
int _get_map_idx_same(int map1, int map2)
{
    int city1 = -1;
    if (_wmMatchAreaContainingMapIdx(map1, &city1) == -1) {
        return -1;
    }

    int city2 = -2;
    if (_wmMatchAreaContainingMapIdx(map2, &city2) == -1) {
        return -1;
    }

    if (city1 != city2) {
        return -1;
    }

    return city1;
}

// 0x48261C
char* mapGetCityName(int map)
{
    int city;
    if (_wmMatchAreaContainingMapIdx(map, &city) == -1) {
        return _aErrorF2;
    }

    MessageListItem messageListItem;
    char* name = getmsg(&gMapMessageList, &messageListItem, 1500 + city);
    return name;
}

// 0x48268C
char* _map_get_description_idx_(int map)
{
    int city;
    if (_wmMatchAreaContainingMapIdx(map, &city) == 0) {
        _wmGetAreaIdxName(city, _scratchStr);
    } else {
        strcpy(_scratchStr, _errMapName);
    }

    return _scratchStr;
}

// 0x4826B8
int mapGetCurrentMap()
{
    return gMapHeader.field_34;
}

// 0x4826C0
int mapScroll(int dx, int dy)
{
    if (getTicksSince(gIsoWindowScrollTimestamp) < 33) {
        return -2;
    }

    gIsoWindowScrollTimestamp = _get_time();

    int screenDx = dx * 32;
    int screenDy = dy * 24;

    if (screenDx == 0 && screenDy == 0) {
        return -1;
    }

    gameMouseObjectsHide();

    int centerScreenX;
    int centerScreenY;
    tileToScreenXY(gCenterTile, &centerScreenX, &centerScreenY, gElevation);
    centerScreenX += screenDx + 16;
    centerScreenY += screenDy + 8;

    int newCenterTile = tileFromScreenXY(centerScreenX, centerScreenY, gElevation);
    if (newCenterTile == -1) {
        return -1;
    }

    if (tileSetCenter(newCenterTile, 0) == -1) {
        return -1;
    }

    Rect r1;
    rectCopy(&r1, &gIsoWindowRect);

    Rect r2;
    rectCopy(&r2, &r1);

    int width = screenGetWidth();
    int pitch = width;
    int height = screenGetVisibleHeight();

    if (screenDx != 0) {
        width -= 32;
    }

    if (screenDy != 0) {
        height -= 24;
    }

    if (screenDx < 0) {
        r2.right = r2.left - screenDx;
    } else {
        r2.left = r2.right - screenDx;
    }

    unsigned char* src;
    unsigned char* dest;
    int step;
    if (screenDy < 0) {
        r1.bottom = r1.top - screenDy;
        src = gIsoWindowBuffer + pitch * (height - 1);
        dest = gIsoWindowBuffer + pitch * (screenGetVisibleHeight() - 1);
        if (screenDx < 0) {
            dest -= screenDx;
        } else {
            src += screenDx;
        }
        step = -pitch;
    } else {
        r1.top = r1.bottom - screenDy;
        dest = gIsoWindowBuffer;
        src = gIsoWindowBuffer + pitch * screenDy;

        if (screenDx < 0) {
            dest -= screenDx;
        } else {
            src += screenDx;
        }
        step = pitch;
    }

    for (int y = 0; y < height; y++) {
        memmove(dest, src, width);
        dest += step;
        src += step;
    }

    if (screenDx != 0) {
        _map_scroll_refresh(&r2);
    }

    if (screenDy != 0) {
        _map_scroll_refresh(&r1);
    }

    windowRefresh(gIsoWindow);

    return 0;
}

// 0x482900
char* mapBuildPath(char* name)
{
    if (*name != '\\') {
        sprintf(_map_path, "maps\\%s", name);
        return _map_path;
    }
    return name;
}

// 0x482924
int mapSetEnteringLocation(int elevation, int tile_num, int orientation)
{
    gEnteringElevation = elevation;
    gEnteringTile = tile_num;
    gEnteringRotation = orientation;
    return 0;
}

// 0x482938
void _map_new_map()
{
    mapSetElevation(0);
    tileSetCenter(20100, TILE_SET_CENTER_FLAG_0x02);
    memset(&gMapTransition, 0, sizeof(gMapTransition));
    gMapHeader.enteringElevation = 0;
    gMapHeader.enteringRotation = 0;
    gMapHeader.localVariablesCount = 0;
    gMapHeader.version = 20;
    gMapHeader.name[0] = '\0';
    gMapHeader.enteringTile = 20100;
    _obj_remove_all();
    _anim_stop();

    if (gMapGlobalVars != NULL) {
        internal_free(gMapGlobalVars);
        gMapGlobalVars = NULL;
        gMapGlobalVarsLength = 0;
    }

    if (gMapLocalVars != NULL) {
        internal_free(gMapLocalVars);
        gMapLocalVars = NULL;
        gMapLocalVarsLength = 0;
    }

    _square_reset();
    _map_place_dude_and_mouse();
    tileWindowRefresh();
}

// 0x482A68
int mapLoadByName(char* fileName)
{
    int rc;

    compat_strupr(fileName);

    rc = -1;

    char* extension = strstr(fileName, ".MAP");
    if (extension != NULL) {
        strcpy(extension, ".SAV");

        const char* filePath = mapBuildPath(fileName);

        File* stream = fileOpen(filePath, "rb");

        strcpy(extension, ".MAP");

        if (stream != NULL) {
            fileClose(stream);
            rc = mapLoadSaved(fileName);
            worldmapStartMapMusic();
        }
    }

    if (rc == -1) {
        const char* filePath = mapBuildPath(fileName);
        File* stream = fileOpen(filePath, "rb");
        if (stream != NULL) {
            rc = mapLoad(stream);
            fileClose(stream);
        }

        if (rc == 0) {
            strcpy(gMapHeader.name, fileName);
            gDude->data.critter.combat.whoHitMe = NULL;
        }
    }

    return rc;
}

// 0x482B34
int mapLoadById(int map)
{
    scriptSetFixedParam(gMapSid, map);

    char name[16];
    if (mapGetFileName(map, name) == -1) {
        return -1;
    }

    _wmMapIdx = map;

    int rc = mapLoadByName(name);

    worldmapStartMapMusic();

    return rc;
}

// 0x482B74
int mapLoad(File* stream)
{
    _map_save_in_game(true);
    backgroundSoundLoad("wind2", 12, 13, 16);
    isoDisable();
    _partyMemberPrepLoad();
    _gmouse_disable_scrolling();

    int savedMouseCursorId = gameMouseGetCursor();
    gameMouseSetCursor(MOUSE_CURSOR_WAIT_PLANET);
    fileSetReadProgressHandler(gameMouseRefresh, 32768);
    tileDisable();

    int rc = 0;

    windowFill(gIsoWindow, 0, 0, _scr_size.right - _scr_size.left + 1, _scr_size.bottom - _scr_size.top - 99, _colorTable[0]);
    windowRefresh(gIsoWindow);
    _anim_stop();
    scriptsDisable();

    gMapSid = -1;

    const char* error = NULL;

    error = "Invalid file handle";
    if (stream == NULL) {
        goto err;
    }

    error = "Error reading header";
    if (mapHeaderRead(&gMapHeader, stream) != 0) {
        goto err;
    }

    error = "Invalid map version";
    if (gMapHeader.version != 19 && gMapHeader.version != 20) {
        goto err;
    }

    if (gEnteringElevation == -1) {
        gEnteringElevation = gMapHeader.enteringElevation;
        gEnteringTile = gMapHeader.enteringTile;
        gEnteringRotation = gMapHeader.enteringRotation;
    }

    _obj_remove_all();

    if (gMapHeader.globalVariablesCount < 0) {
        gMapHeader.globalVariablesCount = 0;
    }

    if (gMapHeader.localVariablesCount < 0) {
        gMapHeader.localVariablesCount = 0;
    }

    error = "Error loading global vars";
    mapGlobalVariablesFree();

    if (gMapHeader.globalVariablesCount != 0) {
        gMapGlobalVars = (int*)internal_malloc(sizeof(*gMapGlobalVars) * gMapHeader.globalVariablesCount);
        if (gMapGlobalVars == NULL) {
            goto err;
        }

        gMapGlobalVarsLength = gMapHeader.globalVariablesCount;
    }

    if (fileReadInt32List(stream, gMapGlobalVars, gMapGlobalVarsLength) != 0) {
        goto err;
    }

    error = "Error loading local vars";
    mapLocalVariablesFree();

    if (gMapHeader.localVariablesCount != 0) {
        gMapLocalVars = (int*)internal_malloc(sizeof(*gMapLocalVars) * gMapHeader.localVariablesCount);
        if (gMapLocalVars == NULL) {
            goto err;
        }

        gMapLocalVarsLength = gMapHeader.localVariablesCount;
    }

    if (fileReadInt32List(stream, gMapLocalVars, gMapLocalVarsLength) != 0) {
        goto err;
    }

    if (_square_load(stream, gMapHeader.flags) != 0) {
        goto err;
    }

    error = "Error reading scripts";
    if (scriptLoadAll(stream) != 0) {
        goto err;
    }

    error = "Error reading objects";
    if (objectLoadAll(stream) != 0) {
        goto err;
    }

    if ((gMapHeader.flags & 1) == 0) {
        _map_fix_critter_combat_data();
    }

    error = "Error setting map elevation";
    if (mapSetElevation(gEnteringElevation) != 0) {
        goto err;
    }

    error = "Error setting tile center";
    if (tileSetCenter(gEnteringTile, TILE_SET_CENTER_FLAG_0x02) != 0) {
        goto err;
    }

    lightSetLightLevel(LIGHT_LEVEL_MAX, false);
    objectSetLocation(gDude, gCenterTile, gElevation, NULL);
    objectSetRotation(gDude, gEnteringRotation, NULL);
    gMapHeader.field_34 = mapGetIndexByFileName(gMapHeader.name);

    if ((gMapHeader.flags & 1) == 0) {
        char path[COMPAT_MAX_PATH];
        sprintf(path, "maps\\%s", gMapHeader.name);

        char* extension = strstr(path, ".MAP");
        if (extension == NULL) {
            extension = strstr(path, ".map");
        }

        if (extension != NULL) {
            *extension = '\0';
        }

        strcat(path, ".GAM");
        globalVarsRead(path, "MAP_GLOBAL_VARS:", &gMapGlobalVarsLength, &gMapGlobalVars);
        gMapHeader.globalVariablesCount = gMapGlobalVarsLength;
    }

    scriptsEnable();

    if (gMapHeader.scriptIndex > 0) {
        error = "Error creating new map script";
        if (scriptAdd(&gMapSid, SCRIPT_TYPE_SYSTEM) == -1) {
            goto err;
        }

        Object* object;
        int fid = buildFid(5, 12, 0, 0, 0);
        objectCreateWithFidPid(&object, fid, -1);
        object->flags |= (OBJECT_FLAG_0x20000000 | OBJECT_TEMPORARY | OBJECT_HIDDEN);
        objectSetLocation(object, 1, 0, NULL);
        object->sid = gMapSid;
        scriptSetFixedParam(gMapSid, (gMapHeader.flags & 1) == 0);

        Script* script;
        scriptGetScript(gMapSid, &script);
        script->field_14 = gMapHeader.scriptIndex - 1;
        script->flags |= SCRIPT_FLAG_0x08;
        object->id = scriptsNewObjectId();
        script->field_1C = object->id;
        script->owner = object;
        _scr_spatials_disable();
        scriptExecProc(gMapSid, SCRIPT_PROC_MAP_ENTER);
        _scr_spatials_enable();

        error = "Error Setting up random encounter";
        if (worldmapSetupRandomEncounter() == -1) {
            goto err;
        }
    }

    error = NULL;

err:

    if (error != NULL) {
        char message[100]; // TODO: Size is probably wrong.
        sprintf(message, "%s while loading map.", error);
        debugPrint(message);
        _map_new_map();
        rc = -1;
    } else {
        _obj_preload_art_cache(gMapHeader.flags);
    }

    _partyMemberRecoverLoad();
    _intface_show();
    _proto_dude_update_gender();
    _map_place_dude_and_mouse();
    fileSetReadProgressHandler(NULL, 0);
    isoEnable();
    _gmouse_disable_scrolling();
    gameMouseSetCursor(MOUSE_CURSOR_WAIT_PLANET);

    if (scriptsExecStartProc() == -1) {
        debugPrint("\n   Error: scr_load_all_scripts failed!");
    }

    scriptsExecMapEnterProc();
    scriptsExecMapUpdateProc();
    tileEnable();

    if (gMapTransition.map > 0) {
        if (gMapTransition.rotation >= 0) {
            objectSetRotation(gDude, gMapTransition.rotation, NULL);
        }
    } else {
        tileWindowRefresh();
    }

    gameTimeScheduleUpdateEvent();

    if (_gsound_sfx_q_start() == -1) {
        rc = -1;
    }

    _wmMapMarkVisited(gMapHeader.field_34);
    _wmMapMarkMapEntranceState(gMapHeader.field_34, gElevation, 1);

    if (_wmCheckGameAreaEvents() != 0) {
        rc = -1;
    }

    fileSetReadProgressHandler(NULL, 0);

    if (gameUiIsDisabled() == 0) {
        _gmouse_enable_scrolling();
    }

    gameMouseSetCursor(savedMouseCursorId);

    gEnteringElevation = -1;
    gEnteringTile = -1;
    gEnteringRotation = -1;

    gameMovieFadeOut();

    gMapHeader.version = 20;

    return rc;
}

// 0x483188
int mapLoadSaved(char* fileName)
{
    debugPrint("\nMAP: Loading SAVED map.");

    char mapName[16]; // TODO: Size is probably wrong.
    _strmfe(mapName, fileName, "SAV");

    int rc = mapLoadByName(mapName);

    if (gameTimeGetTime() >= gMapHeader.field_38) {
        if (((gameTimeGetTime() - gMapHeader.field_38) / 36000) >= 24) {
            objectUnjamAll();
        }

        if (_map_age_dead_critters() == -1) {
            debugPrint("\nError: Critter aging failed on map load!");
            return -1;
        }
    }

    if (!_wmMapIsSaveable()) {
        debugPrint("\nDestroying RANDOM encounter map.");

        char v15[16];
        strcpy(v15, gMapHeader.name);

        _strmfe(gMapHeader.name, v15, "SAV");

        _MapDirEraseFile_("MAPS\\", gMapHeader.name);

        strcpy(gMapHeader.name, v15);
    }

    return rc;
}

// 0x48328C
int _map_age_dead_critters()
{
    if (!_wmMapDeadBodiesAge()) {
        return 0;
    }

    int v4 = (gameTimeGetTime() - gMapHeader.field_38) / 36000;
    if (v4 == 0) {
        return 0;
    }

    Object* obj = objectFindFirst();
    while (obj != NULL) {
        if (obj->pid >> 24 == OBJ_TYPE_CRITTER
            && obj != gDude
            && !objectIsPartyMember(obj)
            && !critterIsDead(obj)) {
            obj->data.critter.combat.maneuver &= 0x04;
            if (critterGetKillType(obj) != KILL_TYPE_ROBOT && _critter_flag_check(obj->pid, 512) == 0) {
                _critter_heal_hours(obj, v4);
            }
        }
        obj = objectFindNext();
    }

    int v20;
    if (v4 <= 336) {
        if (v4 > 144) {
            v20 = 1;
        } else {
            v20 = 0;
        }
    } else {
        v20 = 2;
    }

    if (v20 == 0) {
        return 0;
    }

    int capacity = 100;
    int count = 0;
    Object** objects = (Object**)internal_malloc(sizeof(*objects) * capacity);

    obj = objectFindFirst();
    while (obj != NULL) {
        int type = obj->pid >> 24;
        if (type == OBJ_TYPE_CRITTER) {
            if (obj != gDude && critterIsDead(obj)) {
                if (critterGetKillType(obj) != KILL_TYPE_ROBOT && _critter_flag_check(obj->pid, 512) == 0) {
                    objects[count++] = obj;

                    if (count >= capacity) {
                        capacity *= 2;
                        objects = (Object**)internal_realloc(objects, sizeof(*objects) * capacity);
                        if (objects == NULL) {
                            debugPrint("\nError: Out of Memory!");
                            return -1;
                        }
                    }
                }
            }
        } else if (v20 == 2 && type == OBJ_TYPE_MISC && obj->pid == 0x500000B) {
            objects[count++] = obj;
            if (count >= capacity) {
                capacity *= 2;
                objects = (Object**)internal_realloc(objects, sizeof(*objects) * capacity);
                if (objects == NULL) {
                    debugPrint("\nError: Out of Memory!");
                    return -1;
                }
            }
        }
        obj = objectFindNext();
    }

    int rc = 0;
    for (int index = 0; index < count; index++) {
        Object* obj = objects[index];
        if (obj->pid >> 24 == OBJ_TYPE_CRITTER) {
            if (_critter_flag_check(obj->pid, 64) == 0) {
                _item_drop_all(obj, obj->tile);
            }

            Object* a1;
            if (objectCreateWithPid(&a1, 0x5000004) == -1) {
                rc = -1;
                break;
            }

            objectSetLocation(a1, obj->tile, obj->elevation, NULL);

            Proto* proto;
            protoGetProto(obj->pid, &proto);

            int frame = randomBetween(0, 3);
            if ((proto->critter.flags & 0x800)) {
                frame += 6;
            } else {
                if (critterGetKillType(obj) != KILL_TYPE_RAT
                    && critterGetKillType(obj) != KILL_TYPE_MANTIS) {
                    frame += 3;
                }
            }

            objectSetFrame(a1, frame, NULL);
        }

        reg_anim_clear(obj);
        objectDestroy(obj, NULL);
    }

    internal_free(objects);

    return rc;
}

// 0x48358C
int _map_target_load_area()
{
    int city = -1;
    if (_wmMatchAreaContainingMapIdx(gMapHeader.field_34, &city) == -1) {
        city = -1;
    }
    return city;
}

// 0x4835B4
int mapSetTransition(MapTransition* transition)
{
    if (transition == NULL) {
        return -1;
    }

    memcpy(&gMapTransition, transition, sizeof(gMapTransition));

    if (gMapTransition.map == 0) {
        gMapTransition.map = -2;
    }

    if (isInCombat()) {
        _game_user_wants_to_quit = 1;
    }

    return 0;
}

// 0x4835F8
int mapHandleTransition()
{
    if (gMapTransition.map == 0) {
        return 0;
    }

    gameMouseObjectsHide();

    gameMouseSetCursor(MOUSE_CURSOR_NONE);

    if (gMapTransition.map == -1) {
        if (!isInCombat()) {
            _anim_stop();
            _wmTownMap();
            memset(&gMapTransition, 0, sizeof(gMapTransition));
        }
    } else if (gMapTransition.map == -2) {
        if (!isInCombat()) {
            _anim_stop();
            _wmWorldMap();
            memset(&gMapTransition, 0, sizeof(gMapTransition));
        }
    } else {
        if (!isInCombat()) {
            if (gMapTransition.map != gMapHeader.field_34 || gElevation == gMapTransition.elevation) {
                mapLoadById(gMapTransition.map);
            }

            if (gMapTransition.tile != -1 && gMapTransition.tile != 0
                && gMapHeader.field_34 != MAP_MODOC_BEDNBREAKFAST && gMapHeader.field_34 != MAP_THE_SQUAT_A
                && elevationIsValid(gMapTransition.elevation)) {
                objectSetLocation(gDude, gMapTransition.tile, gMapTransition.elevation, NULL);
                mapSetElevation(gMapTransition.elevation);
                objectSetRotation(gDude, gMapTransition.rotation, NULL);
            }

            if (tileSetCenter(gDude->tile, TILE_SET_CENTER_FLAG_0x01) == -1) {
                debugPrint("\nError: map: attempt to center out-of-bounds!");
            }

            memset(&gMapTransition, 0, sizeof(gMapTransition));

            int city;
            _wmMatchAreaContainingMapIdx(gMapHeader.field_34, &city);
            if (_wmTeleportToArea(city) == -1) {
                debugPrint("\nError: couldn't make jump on worldmap for map jump!");
            }
        }
    }

    return 0;
}

// 0x483784
void _map_fix_critter_combat_data()
{
    for (Object* object = objectFindFirst(); object != NULL; object = objectFindNext()) {
        if (object->pid == -1) {
            continue;
        }

        if ((object->pid >> 24) != OBJ_TYPE_CRITTER) {
            continue;
        }

        if (object->data.critter.combat.whoHitMeCid == -1) {
            object->data.critter.combat.whoHitMe = NULL;
        }
    }
}

// map_save
// 0x483850
int _map_save()
{
    char temp[80];
    temp[0] = '\0';

    char* masterPatchesPath;
    if (configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &masterPatchesPath)) {
        strcat(temp, masterPatchesPath);
        compat_mkdir(temp);

        strcat(temp, "\\MAPS");
        compat_mkdir(temp);
    }

    int rc = -1;
    if (gMapHeader.name[0] != '\0') {
        char* mapFileName = mapBuildPath(gMapHeader.name);
        File* stream = fileOpen(mapFileName, "wb");
        if (stream != NULL) {
            rc = _map_save_file(stream);
            fileClose(stream);
        } else {
            sprintf(temp, "Unable to open %s to write!", gMapHeader.name);
            debugPrint(temp);
        }

        if (rc == 0) {
            sprintf(temp, "%s saved.", gMapHeader.name);
            debugPrint(temp);
        }
    } else {
        debugPrint("\nError: map_save: map header corrupt!");
    }

    return rc;
}

// 0x483980
int _map_save_file(File* stream)
{
    if (stream == NULL) {
        return -1;
    }

    scriptsDisable();

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int tile;
        for (tile = 0; tile < SQUARE_GRID_SIZE; tile++) {
            int fid;

            fid = buildFid(4, _square[elevation]->field_0[tile] & 0xFFF, 0, 0, 0);
            if (fid != buildFid(4, 1, 0, 0, 0)) {
                break;
            }

            fid = buildFid(4, (_square[elevation]->field_0[tile] >> 16) & 0xFFF, 0, 0, 0);
            if (fid != buildFid(4, 1, 0, 0, 0)) {
                break;
            }
        }

        if (tile == SQUARE_GRID_SIZE) {
            Object* object = objectFindFirstAtElevation(elevation);
            if (object != NULL) {
                // TODO: Implementation is slightly different, check in debugger.
                while (object != NULL && (object->flags & OBJECT_TEMPORARY)) {
                    object = objectFindNextAtElevation();
                }

                if (object != NULL) {
                    gMapHeader.flags &= ~_map_data_elev_flags[elevation];
                } else {
                    gMapHeader.flags |= _map_data_elev_flags[elevation];
                }
            } else {
                gMapHeader.flags |= _map_data_elev_flags[elevation];
            }
        } else {
            gMapHeader.flags &= ~_map_data_elev_flags[elevation];
        }
    }

    gMapHeader.localVariablesCount = gMapLocalVarsLength;
    gMapHeader.globalVariablesCount = gMapGlobalVarsLength;
    gMapHeader.darkness = 1;

    mapHeaderWrite(&gMapHeader, stream);

    if (gMapHeader.globalVariablesCount != 0) {
        fileWriteInt32List(stream, gMapGlobalVars, gMapHeader.globalVariablesCount);
    }

    if (gMapHeader.localVariablesCount != 0) {
        fileWriteInt32List(stream, gMapLocalVars, gMapHeader.localVariablesCount);
    }

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        if ((gMapHeader.flags & _map_data_elev_flags[elevation]) == 0) {
            _db_fwriteLongCount(stream, _square[elevation]->field_0, SQUARE_GRID_SIZE);
        }
    }

    char err[80];

    if (scriptSaveAll(stream) == -1) {
        sprintf(err, "Error saving scripts in %s", gMapHeader.name);
        // TODO: Incomplete.
        // _win_msg(err, 80, 80, _colorTable[31744]);
    }

    if (objectSaveAll(stream) == -1) {
        sprintf(err, "Error saving objects in %s", gMapHeader.name);
        // TODO: Incomplete.
        // _win_msg(err, 80, 80, _colorTable[31744]);
    }

    scriptsEnable();

    return 0;
}

// 0x483C98
int _map_save_in_game(bool a1)
{
    if (gMapHeader.name[0] == '\0') {
        return 0;
    }

    _anim_stop();
    _partyMemberSaveProtos();

    if (a1) {
        _queue_leaving_map();
        _partyMemberPrepLoad();
        _partyMemberPrepItemSaveAll();
        scriptsExecMapExitProc();

        if (gMapSid != -1) {
            Script* script;
            scriptGetScript(gMapSid, &script);
        }

        gameTimeScheduleUpdateEvent();
        _obj_reset_roof();
    }

    gMapHeader.flags |= 0x01;
    gMapHeader.field_38 = gameTimeGetTime();

    char name[16];

    if (a1 && !_wmMapIsSaveable()) {
        debugPrint("\nNot saving RANDOM encounter map.");

        strcpy(name, gMapHeader.name);
        _strmfe(gMapHeader.name, name, "SAV");
        _MapDirEraseFile_("MAPS\\", gMapHeader.name);
        strcpy(gMapHeader.name, name);
    } else {
        debugPrint("\n Saving \".SAV\" map.");

        strcpy(name, gMapHeader.name);
        _strmfe(gMapHeader.name, name, "SAV");
        if (_map_save() == -1) {
            return -1;
        }

        strcpy(gMapHeader.name, name);

        automapSaveCurrent();

        if (a1) {
            gMapHeader.name[0] = '\0';
            _obj_remove_all();
            _proto_remove_all();
            _square_reset();
            gameTimeScheduleUpdateEvent();
        }
    }

    return 0;
}

// 0x483E28
void mapMakeMapsDirectory()
{
    char path[COMPAT_MAX_PATH];

    char* masterPatchesPath;
    if (configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &masterPatchesPath)) {
        strcpy(path, masterPatchesPath);
    } else {
        strcpy(path, "DATA");
    }

    compat_mkdir(path);

    strcat(path, "\\MAPS");
    compat_mkdir(path);
}

// 0x483ED0
void isoWindowRefreshRect(Rect* rect)
{
    windowRefreshRect(gIsoWindow, rect);
}

// 0x483EE4
void isoWindowRefreshRectGame(Rect* rect)
{
    Rect clampedDirtyRect;
    if (rectIntersection(rect, &gIsoWindowRect, &clampedDirtyRect) == -1) {
        return;
    }

    tileRenderFloorsInRect(&clampedDirtyRect, gElevation);
    _grid_render(&clampedDirtyRect, gElevation);
    _obj_render_pre_roof(&clampedDirtyRect, gElevation);
    tileRenderRoofsInRect(&clampedDirtyRect, gElevation);
    _obj_render_post_roof(&clampedDirtyRect, gElevation);
}

// 0x483F44
void isoWindowRefreshRectMapper(Rect* rect)
{
    Rect clampedDirtyRect;
    if (rectIntersection(rect, &gIsoWindowRect, &clampedDirtyRect) == -1) {
        return;
    }

    bufferFill(gIsoWindowBuffer + clampedDirtyRect.top * (_scr_size.right - _scr_size.left + 1) + clampedDirtyRect.left,
        clampedDirtyRect.right - clampedDirtyRect.left + 1,
        clampedDirtyRect.bottom - clampedDirtyRect.top + 1,
        _scr_size.right - _scr_size.left + 1,
        0);
    tileRenderFloorsInRect(&clampedDirtyRect, gElevation);
    _grid_render(&clampedDirtyRect, gElevation);
    _obj_render_pre_roof(&clampedDirtyRect, gElevation);
    tileRenderRoofsInRect(&clampedDirtyRect, gElevation);
    _obj_render_post_roof(&clampedDirtyRect, gElevation);
}

// 0x484038
void mapGlobalVariablesFree()
{
    if (gMapGlobalVars != NULL) {
        internal_free(gMapGlobalVars);
        gMapGlobalVars = NULL;
        gMapGlobalVarsLength = 0;
    }
}

// 0x4840D4
void mapLocalVariablesFree()
{
    if (gMapLocalVars != NULL) {
        internal_free(gMapLocalVars);
        gMapLocalVars = NULL;
        gMapLocalVarsLength = 0;
    }
}

// 0x48411C
void _map_place_dude_and_mouse()
{
    _obj_clear_seen();

    if (gDude != NULL) {
        if (((gDude->fid & 0xFF0000) >> 16) != 0) {
            objectSetFrame(gDude, 0, 0);
            gDude->fid = buildFid(1, gDude->fid & 0xFFF, ANIM_STAND, (gDude->fid & 0xF000) >> 12, gDude->rotation + 1);
        }

        if (gDude->tile == -1) {
            objectSetLocation(gDude, gCenterTile, gElevation, NULL);
            objectSetRotation(gDude, gMapHeader.enteringRotation, 0);
        }

        objectSetLight(gDude, 4, 0x10000, 0);
        gDude->flags |= OBJECT_TEMPORARY;

        _dude_stand(gDude, gDude->rotation, gDude->fid);
        _partyMemberSyncPosition();
    }

    gameMouseResetBouncingCursorFid();
    gameMouseObjectsShow();
}

// 0x484210
void _square_reset()
{
    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int* p = _square[elevation]->field_0;
        for (int y = 0; y < SQUARE_GRID_HEIGHT; y++) {
            for (int x = 0; x < SQUARE_GRID_WIDTH; x++) {
                // TODO: Strange math, initially right, but need to figure it out and
                // check subsequent calls.
                int fid = *p;
                fid &= ~0xFFFF;
                *p = ((buildFid(4, 1, 0, 0, 0) & 0xFFF | (((fid >> 16) & 0xF000) >> 12)) << 16) | (fid & 0xFFFF);

                fid = *p;
                int v3 = (fid & 0xF000) >> 12;
                int v4 = (buildFid(4, 1, 0, 0, 0) & 0xFFF) | v3;

                fid &= ~0xFFFF;

                *p = v4 | ((fid >> 16) << 16);

                p++;
            }
        }
    }
}

// 0x48431C
int _square_load(File* stream, int flags)
{
    int v6;
    int v7;
    int v8;
    int v9;

    _square_reset();

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        if ((flags & _map_data_elev_flags[elevation]) == 0) {
            int* arr = _square[elevation]->field_0;
            if (_db_freadIntCount(stream, arr, SQUARE_GRID_SIZE) != 0) {
                return -1;
            }

            for (int tile = 0; tile < SQUARE_GRID_SIZE; tile++) {
                v6 = arr[tile];
                v6 &= ~(0xFFFF);
                v6 >>= 16;

                v7 = (v6 & 0xF000) >> 12;
                v7 &= ~(0x01);

                v8 = v6 & 0xFFF;
                v9 = arr[tile] & 0xFFFF;
                arr[tile] = ((v8 | (v7 << 12)) << 16) | v9;
            }
        }
    }

    return 0;
}

// 0x4843B8
int mapHeaderWrite(MapHeader* ptr, File* stream)
{
    if (fileWriteInt32(stream, ptr->version) == -1) return -1;
    if (fileWriteFixedLengthString(stream, ptr->name, 16) == -1) return -1;
    if (fileWriteInt32(stream, ptr->enteringTile) == -1) return -1;
    if (fileWriteInt32(stream, ptr->enteringElevation) == -1) return -1;
    if (fileWriteInt32(stream, ptr->enteringRotation) == -1) return -1;
    if (fileWriteInt32(stream, ptr->localVariablesCount) == -1) return -1;
    if (fileWriteInt32(stream, ptr->scriptIndex) == -1) return -1;
    if (fileWriteInt32(stream, ptr->flags) == -1) return -1;
    if (fileWriteInt32(stream, ptr->darkness) == -1) return -1;
    if (fileWriteInt32(stream, ptr->globalVariablesCount) == -1) return -1;
    if (fileWriteInt32(stream, ptr->field_34) == -1) return -1;
    if (fileWriteInt32(stream, ptr->field_38) == -1) return -1;
    if (fileWriteInt32List(stream, ptr->field_3C, 44) == -1) return -1;

    return 0;
}

// 0x4844B4
int mapHeaderRead(MapHeader* ptr, File* stream)
{
    if (fileReadInt32(stream, &(ptr->version)) == -1) return -1;
    if (fileReadFixedLengthString(stream, ptr->name, 16) == -1) return -1;
    if (fileReadInt32(stream, &(ptr->enteringTile)) == -1) return -1;
    if (fileReadInt32(stream, &(ptr->enteringElevation)) == -1) return -1;
    if (fileReadInt32(stream, &(ptr->enteringRotation)) == -1) return -1;
    if (fileReadInt32(stream, &(ptr->localVariablesCount)) == -1) return -1;
    if (fileReadInt32(stream, &(ptr->scriptIndex)) == -1) return -1;
    if (fileReadInt32(stream, &(ptr->flags)) == -1) return -1;
    if (fileReadInt32(stream, &(ptr->darkness)) == -1) return -1;
    if (fileReadInt32(stream, &(ptr->globalVariablesCount)) == -1) return -1;
    if (fileReadInt32(stream, &(ptr->field_34)) == -1) return -1;
    if (fileReadInt32(stream, &(ptr->field_38)) == -1) return -1;
    if (fileReadInt32List(stream, ptr->field_3C, 44) == -1) return -1;

    return 0;
}
