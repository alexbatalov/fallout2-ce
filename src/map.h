#ifndef MAP_H
#define MAP_H

#include "combat_defs.h"
#include "db.h"
#include "geometry.h"
#include "map_defs.h"
#include "message.h"
#include "platform_compat.h"

#define ORIGINAL_ISO_WINDOW_WIDTH 640
#define ORIGINAL_ISO_WINDOW_HEIGHT 380

// TODO: Probably not needed -> replace with array?
typedef struct TileData {
    int field_0[SQUARE_GRID_SIZE];
} TileData;

typedef struct MapHeader {
    // map_ver
    int version;

    // map_name
    char name[16];

    // map_ent_tile
    int enteringTile;

    // map_ent_elev
    int enteringElevation;

    // map_ent_rot
    int enteringRotation;

    // map_num_loc_vars
    int localVariablesCount;

    // 0map_script_idx
    int scriptIndex;

    // map_flags
    int flags;

    // map_darkness
    int darkness;

    // map_num_glob_vars
    int globalVariablesCount;

    // map_number
    int field_34;
    int field_38;
    int field_3C[44];
} MapHeader;

typedef struct MapTransition {
    int map;
    int elevation;
    int tile;
    int rotation;
} MapTransition;

typedef void IsoWindowRefreshProc(Rect* rect);

extern char byte_50B058[];
extern char _aErrorF2[];
extern IsoWindowRefreshProc* _map_scroll_refresh;
extern const int _map_data_elev_flags[ELEVATION_COUNT];
extern unsigned int gIsoWindowScrollTimestamp;
extern bool gIsoEnabled;
extern int gEnteringElevation;
extern int gEnteringTile;
extern int gEnteringRotation;
extern int gMapSid;
extern int* gMapLocalVars;
extern int* gMapGlobalVars;
extern int gMapLocalVarsLength;
extern int gMapGlobalVarsLength;
extern int gElevation;
extern char* _errMapName;
extern int _wmMapIdx;

extern TileData _square_data[ELEVATION_COUNT];
extern MapTransition gMapTransition;
extern Rect gIsoWindowRect;
extern MessageList gMapMessageList;
extern unsigned char* gIsoWindowBuffer;
extern MapHeader gMapHeader;
extern TileData* _square[ELEVATION_COUNT];
extern int gIsoWindow;
extern char _scratchStr[40];
extern char _map_path[COMPAT_MAX_PATH];

int isoInit();
void isoReset();
void isoExit();
void _map_init();
void _map_exit();
void isoEnable();
bool isoDisable();
bool isoIsDisabled();
int mapSetElevation(int elevation);
int mapSetGlobalVar(int var, int value);
int mapGetGlobalVar(int var);
int mapSetLocalVar(int var, int value);
int mapGetLocalVar(int var);
int _map_malloc_local_var(int a1);
void mapSetStart(int a1, int a2, int a3);
char* mapGetName(int map_num, int elev);
bool _is_map_idx_same(int map_num1, int map_num2);
int _get_map_idx_same(int map_num1, int map_num2);
char* mapGetCityName(int map_num);
char* _map_get_description_idx_(int map_index);
int mapGetCurrentMap();
int mapScroll(int dx, int dy);
char* mapBuildPath(char* name);
int mapSetEnteringLocation(int a1, int a2, int a3);
void _map_new_map();
int mapLoadByName(char* fileName);
int mapLoadById(int map_index);
int mapLoad(File* stream);
int mapLoadSaved(char* fileName);
int _map_age_dead_critters();
int _map_target_load_area();
int mapSetTransition(MapTransition* transition);
int mapHandleTransition();
void _map_fix_critter_combat_data();
int _map_save();
int _map_save_file(File* stream);
int _map_save_in_game(bool a1);
void mapMakeMapsDirectory();
void isoWindowRefreshRect(Rect* rect);
void isoWindowRefreshRectGame(Rect* rect);
void isoWindowRefreshRectMapper(Rect* rect);
void mapGlobalVariablesFree();
void mapLocalVariablesFree();
void _map_place_dude_and_mouse();
void _square_reset();
int _square_load(File* stream, int a2);
int mapHeaderWrite(MapHeader* ptr, File* stream);
int mapHeaderRead(MapHeader* ptr, File* stream);

#endif /* MAP_H */
