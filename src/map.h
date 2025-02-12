#ifndef MAP_H
#define MAP_H

#include "combat_defs.h"
#include "db.h"
#include "geometry.h"
#include "interpreter.h"
#include "map_defs.h"
#include "message.h"
#include "platform_compat.h"

namespace fallout {

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
    int index;

    // Time in game ticks when PC last visited this map.
    unsigned int lastVisitTime;
    int field_3C[44];
} MapHeader;

typedef struct MapTransition {
    int map;
    int elevation;
    int tile;
    int rotation;
} MapTransition;

typedef void IsoWindowRefreshProc(Rect* rect);

extern int gMapSid;
extern int* gMapLocalVars;
extern int* gMapGlobalVars;
extern int gMapLocalVarsLength;
extern int gMapGlobalVarsLength;
extern int gElevation;

extern MessageList gMapMessageList;
extern MapHeader gMapHeader;
extern TileData* _square[ELEVATION_COUNT];
extern int gIsoWindow;

int isoInit();
void isoReset();
void isoExit();
void _map_init();
void _map_exit();
void isoEnable();
bool isoDisable();
bool isoIsDisabled();
int mapSetElevation(int elevation);
int mapSetGlobalVar(int var, ProgramValue& value);
int mapGetGlobalVar(int var, ProgramValue& value);
int mapSetLocalVar(int var, ProgramValue& value);
int mapGetLocalVar(int var, ProgramValue& value);
int _map_malloc_local_var(int a1);
void mapSetStart(int a1, int a2, int a3);
char* mapGetName(int map_num, int elev);
bool _is_map_idx_same(int map_num1, int map_num2);
int _get_map_idx_same(int map_num1, int map_num2);
char* mapGetCityName(int map_num);
char* _map_get_description_idx_(int map_index);
int mapGetCurrentMap();
int mapScroll(int dx, int dy);
int mapSetEnteringLocation(int a1, int a2, int a3);
void mapNewMap();
int mapLoadByName(char* fileName);
int mapLoadById(int map_index);
int mapLoadSaved(char* fileName);
int _map_target_load_area();
int mapSetTransition(MapTransition* transition);
int mapHandleTransition();
int _map_save_in_game(bool a1);

} // namespace fallout

#endif /* MAP_H */
