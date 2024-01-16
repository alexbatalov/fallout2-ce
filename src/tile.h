#ifndef TILE_H
#define TILE_H

#include "geometry.h"
#include "map.h"

namespace fallout {

#define TILE_SET_CENTER_REFRESH_WINDOW 0x01
#define TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS 0x02

typedef void(TileWindowRefreshProc)(Rect* rect);
typedef void(TileWindowRefreshElevationProc)(Rect* rect, int elevation);

extern const int _off_tile[6];
extern const int dword_51D984[6];
extern int gHexGridSize;
extern int gCenterTile;

int tileInit(TileData** a1, int squareGridWidth, int squareGridHeight, int hexGridWidth, int hexGridHeight, unsigned char* buf, int windowWidth, int windowHeight, int windowPitch, TileWindowRefreshProc* windowRefreshProc);
void _tile_reset_();
void tileReset();
void tileExit();
void tileDisable();
void tileEnable();
void tileWindowRefreshRect(Rect* rect, int elevation);
void tileWindowRefresh();
int tileSetCenter(int tile, int flags);
void tile_toggle_roof(bool refresh);
int tileRoofIsVisible();
int tileToScreenXY(int tile, int* x, int* y, int elevation);
int tileFromScreenXY(int x, int y, int elevation, bool ignoreBounds = false);
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
void tile_fill_roof(int x, int y, int elevation, bool on);
void tileRenderFloorsInRect(Rect* rect, int elevation);
bool _square_roof_intersect(int x, int y, int elevation);
void _grid_render(Rect* rect, int elevation);
int _tile_scroll_to(int tile, int flags);

static bool tileIsValid(int tile)
{
    return tile >= 0 && tile < gHexGridSize;
}

} // namespace fallout

#endif /* TILE_H */
