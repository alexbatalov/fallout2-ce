#ifndef TILE_H
#define TILE_H

#include "geometry.h"
#include "map.h"
#include "obj_types.h"

#include <stdbool.h>

#define TILE_SET_CENTER_FLAG_0x01 0x01
#define TILE_SET_CENTER_FLAG_0x02 0x02

typedef struct STRUCT_51D99C {
    int field_0;
    int field_4;
} STRUCT_51D99C;

typedef struct STRUCT_51DA04 {
    int field_0;
    int field_4;
} STRUCT_51DA04;

typedef struct STRUCT_51DA6C {
    int field_0;
    int field_4;
    int field_8;
    int field_C; // something with light level?
} STRUCT_51DA6C;

typedef struct STRUCT_51DB0C {
    int field_0;
    int field_4;
    int field_8;
} STRUCT_51DB0C;

typedef struct STRUCT_51DB48 {
    int field_0;
    int field_4;
    int field_8;
} STRUCT_51DB48;

typedef void(TileWindowRefreshProc)(Rect* rect);
typedef void(TileWindowRefreshElevationProc)(Rect* rect, int elevation);

extern double const dbl_50E7C7;

extern bool _borderInitialized;
extern bool _scroll_blocking_on;
extern bool _scroll_limiting_on;
extern int _show_roof;
extern int _show_grid;
extern TileWindowRefreshElevationProc* _tile_refresh;
extern bool gTileEnabled;
extern const int _off_tile[6];
extern const int dword_51D984[6];
extern STRUCT_51D99C _rightside_up_table[13];
extern STRUCT_51DA04 _upside_down_table[13];
extern STRUCT_51DA6C _verticies[10];
extern STRUCT_51DB0C _rightside_up_triangles[5];
extern STRUCT_51DB48 _upside_down_triangles[5];

extern int _intensity_map[3280];
extern int _dir_tile2[2][6];
extern int _dir_tile[2][6];
extern unsigned char _tile_grid_blocked[512];
extern unsigned char _tile_grid_occupied[512];
extern unsigned char _tile_mask[512];
extern int _tile_border;
extern int dword_66BBC8;
extern int dword_66BBCC;
extern int dword_66BBD0;
extern Rect gTileWindowRect;
extern unsigned char _tile_grid[512];
extern int _square_rect;
extern int _square_x;
extern int _square_offx;
extern int _square_offy;
extern TileWindowRefreshProc* gTileWindowRefreshProc;
extern int _tile_offy;
extern int _tile_offx;
extern int gSquareGridSize;
extern int gHexGridWidth;
extern TileData** _squares;
extern unsigned char* gTileWindowBuffer;
extern int gHexGridHeight;
extern int gTileWindowHeight;
extern int _tile_x;
extern int _tile_y;
extern int gHexGridSize;
extern int gSquareGridHeight;
extern int gTileWindowPitch;
extern int gSquareGridWidth;
extern int gTileWindowWidth;
extern int gCenterTile;

int tileInit(TileData** a1, int squareGridWidth, int squareGridHeight, int hexGridWidth, int hexGridHeight, unsigned char* buf, int windowWidth, int windowHeight, int windowPitch, TileWindowRefreshProc* windowRefreshProc);
void _tile_set_border(int a1, int a2, int a3, int a4);
void _tile_reset_();
void tileReset();
void tileExit();
void tileDisable();
void tileEnable();
void tileWindowRefreshRect(Rect* rect, int elevation);
void tileWindowRefresh();
int tileSetCenter(int a1, int a2);
void _refresh_mapper(Rect* rect, int elevation);
void _refresh_game(Rect* rect, int elevation);
int _tile_roof_visible();
int tileToScreenXY(int tile, int* x, int* y, int elevation);
int tileFromScreenXY(int x, int y, int elevation);
int tileDistanceBetween(int a1, int a2);
bool _tile_in_front_of(int tile1, int tile2);
bool _tile_to_right_of(int tile1, int tile2);
int tileGetTileInDirection(int tile, int rotation, int distance);
int tileGetRotationTo(int a1, int a2);
int _tile_num_beyond(int from, int to, int distance);
int _tile_on_edge(int a1);
void _tile_enable_scroll_blocking();
void _tile_disable_scroll_blocking();
bool _tile_get_scroll_blocking();
void _tile_enable_scroll_limiting();
void _tile_disable_scroll_limiting();
bool _tile_get_scroll_limiting();
int _square_coord(int a1, int* a2, int* a3, int elevation);
int squareTileToScreenXY(int a1, int* a2, int* a3, int elevation);
int _square_num(int x, int y, int elevation);
void _square_xy(int a1, int a2, int elevation, int* a3, int* a4);
void _square_xy_roof(int a1, int a2, int elevation, int* a3, int* a4);
void tileRenderRoofsInRect(Rect* rect, int elevation);
void _roof_fill_on(int x, int y, int elevation);
void _tile_fill_roof(int x, int y, int elevation, int a4);
void sub_4B23DC(int x, int y, int elevation);
void tileRenderRoof(int fid, int x, int y, Rect* rect, int light);
void tileRenderFloorsInRect(Rect* rect, int elevation);
bool _square_roof_intersect(int x, int y, int elevation);
void _grid_render(Rect* rect, int elevation);
void _draw_grid(int tile, int elevation, Rect* rect);
void tileRenderFloor(int fid, int x, int y, Rect* rect);
int _tile_make_line(int currentCenterTile, int newCenterTile, int* tiles, int tilesCapacity);
int _tile_scroll_to(int tile, int flags);

#endif /* TILE_H */
