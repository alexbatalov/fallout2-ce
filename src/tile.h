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

extern bool gTileBorderInitialized;
extern bool gTileScrollBlockingEnabled;
extern bool gTileScrollLimitingEnabled;
extern bool gTileRoofIsVisible;
extern bool gTileGridIsVisible;
extern TileWindowRefreshElevationProc* gTileWindowRefreshElevationProc;
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
extern int gTileBorderMinX;
extern int gTileBorderMinY;
extern int gTileBorderMaxX;
extern int gTileBorderMaxY;
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
extern TileData** gTileSquares;
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
void tileSetBorder(int windowWidth, int windowHeight, int hexGridWidth, int hexGridHeight);
void _tile_reset_();
void tileReset();
void tileExit();
void tileDisable();
void tileEnable();
void tileWindowRefreshRect(Rect* rect, int elevation);
void tileWindowRefresh();
int tileSetCenter(int tile, int flags);
void tileRefreshMapper(Rect* rect, int elevation);
void tileRefreshGame(Rect* rect, int elevation);
int tileRoofIsVisible();
int tileToScreenXY(int tile, int* x, int* y, int elevation);
int tileFromScreenXY(int x, int y, int elevation);
int tileDistanceBetween(int a1, int a2);
bool tileIsInFrontOf(int tile1, int tile2);
bool tileIsToRightOf(int tile1, int tile2);
int tileGetTileInDirection(int tile, int rotation, int distance);
int tileGetRotationTo(int a1, int a2);
int _tile_num_beyond(int from, int to, int distance);
bool tileIsEdge(int tile);
void tileScrollBlockingEnable();
void tileScrollBlockingDisable();
bool tileScrollBlockingIsEnabled();
void tileScrollLimitingEnable();
void tileScrollLimitingDisable();
bool tileScrollLimitingIsEnabled();
int squareTileToScreenXY(int squareTile, int* coordX, int* coordY, int elevation);
int squareTileToRoofScreenXY(int squareTile, int* screenX, int* screenY, int elevation);
int squareTileFromScreenXY(int screenX, int screenY, int elevation);
void squareTileScreenToCoord(int screenX, int screenY, int elevation, int* coordX, int* coordY);
void squareTileScreenToCoordRoof(int screenX, int screenY, int elevation, int* coordX, int* coordY);
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

static bool tileIsValid(int tile)
{
    return tile >= 0 && tile < gHexGridSize;
}

#endif /* TILE_H */
