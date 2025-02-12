#include "tile.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#include <algorithm>
#include <stack>

#include "art.h"
#include "color.h"
#include "config.h"
#include "debug.h"
#include "draw.h"
#include "game_mouse.h"
#include "light.h"
#include "map.h"
#include "object.h"
#include "platform_compat.h"
#include "settings.h"
#include "svga.h"
#include "tile_hires_stencil.h"

namespace fallout {

typedef struct RightsideUpTableEntry {
    int field_0;
    int field_4;
} RightsideUpTableEntry;

typedef struct UpsideDownTableEntry {
    int field_0;
    int field_4;
} UpsideDownTableEntry;

typedef struct STRUCT_51DA6C {
    int field_0;
    int offsets[2];
    int intensity;
} STRUCT_51DA6C;

typedef struct RightsideUpTriangle {
    int field_0;
    int field_4;
    int field_8;
} RightsideUpTriangle;

typedef struct UpsideDownTriangle {
    int field_0;
    int field_4;
    int field_8;
} UpsideDownTriangle;

struct roof_fill_task {
    int x;
    int y;
};

static void tileSetBorder(int windowWidth, int windowHeight, int hexGridWidth, int hexGridHeight);
static void tileRefreshMapper(Rect* rect, int elevation);
static void tileRefreshGame(Rect* rect, int elevation);
static void roof_fill_push_task_if_in_bounds(std::stack<roof_fill_task>& tasks_stack, int x, int y);
static void roof_fill_off_process_task(std::stack<roof_fill_task>& tasks_stack, int elevation, bool on);
static void tileRenderRoof(int fid, int x, int y, Rect* rect, int light);
static void _draw_grid(int tile, int elevation, Rect* rect);
static void tileRenderFloor(int fid, int x, int y, Rect* rect);
static int _tile_make_line(int currentCenterTile, int newCenterTile, int* tiles, int tilesCapacity);

// 0x50E7C7
static double const dbl_50E7C7 = -4.0;

// 0x51D950
bool gTileBorderInitialized = false;

// 0x51D954
static bool gTileScrollBlockingEnabled = true;

// 0x51D958
static bool gTileScrollLimitingEnabled = true;

// 0x51D95C
static bool gTileRoofIsVisible = true;

// 0x51D960
static bool gTileGridIsVisible = false;

// 0x51D964
static TileWindowRefreshElevationProc* gTileWindowRefreshElevationProc = tileRefreshGame;

// 0x51D968
static bool gTileEnabled = true;

// 0x51D96C
const int _off_tile[6] = {
    16,
    32,
    16,
    -16,
    -32,
    -16,
};

// 0x51D984
const int dword_51D984[6] = {
    -12,
    0,
    12,
    12,
    0,
    -12,
};

// 0x51D99C
static RightsideUpTableEntry _rightside_up_table[13] = {
    { -1, 2 },
    { 78, 2 },
    { 76, 6 },
    { 73, 8 },
    { 71, 10 },
    { 68, 14 },
    { 65, 16 },
    { 63, 18 },
    { 61, 20 },
    { 58, 24 },
    { 55, 26 },
    { 53, 28 },
    { 50, 32 },
};

// 0x51DA04
static UpsideDownTableEntry _upside_down_table[13] = {
    { 0, 32 },
    { 48, 32 },
    { 49, 30 },
    { 52, 26 },
    { 55, 24 },
    { 57, 22 },
    { 60, 18 },
    { 63, 16 },
    { 65, 14 },
    { 67, 12 },
    { 70, 8 },
    { 73, 6 },
    { 75, 4 },
};

// 0x51DA6C
static STRUCT_51DA6C _verticies[10] = {
    { 16, -1, -201, 0 },
    { 48, -2, -2, 0 },
    { 960, 0, 0, 0 },
    { 992, 199, -1, 0 },
    { 1024, 198, 198, 0 },
    { 1936, 200, 200, 0 },
    { 1968, 399, 199, 0 },
    { 2000, 398, 398, 0 },
    { 2912, 400, 400, 0 },
    { 2944, 599, 399, 0 },
};

// 0x51DB0C
static RightsideUpTriangle _rightside_up_triangles[5] = {
    { 2, 3, 0 },
    { 3, 4, 1 },
    { 5, 6, 3 },
    { 6, 7, 4 },
    { 8, 9, 6 },
};

// 0x51DB48
static UpsideDownTriangle _upside_down_triangles[5] = {
    { 0, 3, 1 },
    { 2, 5, 3 },
    { 3, 6, 4 },
    { 5, 8, 6 },
    { 6, 9, 7 },
};

// 0x668224
static int _intensity_map[3280];

// 0x66B564
static int _dir_tile2[2][6];

// Deltas to perform tile calculations in given direction.
//
// 0x66B594
static int _dir_tile[2][6];

// 0x66B5C4
static unsigned char _tile_grid_blocked[512];

// 0x66B7C4
static unsigned char _tile_grid_occupied[512];

// 0x66B9C4
static unsigned char _tile_mask[512];

// 0x66BBC4
int gTileBorderMinX = 0;

// 0x66BBC8
int gTileBorderMinY = 0;

// 0x66BBCC
int gTileBorderMaxX = 0;

// 0x66BBD0
int gTileBorderMaxY = 0;

// 0x66BBD4
static Rect gTileWindowRect;

// 0x66BBE4
static unsigned char _tile_grid[32 * 16];

// 0x66BDE4
static int _square_y;

// 0x66BDE8
static int _square_x;

// 0x66BDEC
static int _square_offx;

// 0x66BDF0
static int _square_offy;

// 0x66BDF4
static TileWindowRefreshProc* gTileWindowRefreshProc;

// 0x66BDF8
static int _tile_offy;

// 0x66BDFC
static int _tile_offx;

// 0x66BE00
static int gSquareGridSize;

// Number of tiles horizontally.
//
// Currently this value is always 200.
//
// 0x66BE04
static int gHexGridWidth;

// 0x66BE08
static TileData** gTileSquares;

// 0x66BE0C
static unsigned char* gTileWindowBuffer;

// Number of tiles vertically.
//
// Currently this value is always 200.
//
// 0x66BE10
static int gHexGridHeight;

// 0x66BE14
static int gTileWindowHeight;

// 0x66BE18
static int _tile_x;

// 0x66BE1C
static int _tile_y;

// The number of tiles in the hex grid.
//
// 0x66BE20
int gHexGridSize;

// 0x66BE24
static int gSquareGridHeight;

// 0x66BE28
static int gTileWindowPitch;

// 0x66BE2C
static int gSquareGridWidth;

// 0x66BE30
static int gTileWindowWidth;

// 0x66BE34
int gCenterTile;

// 0x4B0C40
int tileInit(TileData** a1, int squareGridWidth, int squareGridHeight, int hexGridWidth, int hexGridHeight, unsigned char* buf, int windowWidth, int windowHeight, int windowPitch, TileWindowRefreshProc* windowRefreshProc)
{
    int v11;
    int v12;
    int v13;

    int v20;
    int v21;
    int v22;
    int v23;
    int v24;
    int v25;

    gSquareGridWidth = squareGridWidth;
    gTileSquares = a1;
    gHexGridHeight = hexGridHeight;
    gSquareGridHeight = squareGridHeight;
    gHexGridWidth = hexGridWidth;
    _dir_tile[0][0] = -1;
    _dir_tile[0][4] = 1;
    _dir_tile[1][1] = -1;
    gHexGridSize = hexGridWidth * hexGridHeight;
    _dir_tile[1][3] = 1;
    gTileWindowBuffer = buf;
    _dir_tile2[0][0] = -1;
    gTileWindowWidth = windowWidth;
    _dir_tile2[0][3] = -1;
    gTileWindowHeight = windowHeight;
    _dir_tile2[1][1] = 1;
    gTileWindowPitch = windowPitch;
    _dir_tile2[1][2] = 1;
    gTileWindowRect.right = windowWidth - 1;
    gSquareGridSize = squareGridHeight * squareGridWidth;
    gTileWindowRect.bottom = windowHeight - 1;
    gTileWindowRect.left = 0;
    gTileWindowRefreshProc = windowRefreshProc;
    gTileWindowRect.top = 0;
    _dir_tile[0][1] = hexGridWidth - 1;
    _dir_tile[0][2] = hexGridWidth;
    gTileGridIsVisible = 0;
    _dir_tile[0][3] = hexGridWidth + 1;
    _dir_tile[1][2] = hexGridWidth;
    _dir_tile2[0][4] = hexGridWidth;
    _dir_tile2[0][5] = hexGridWidth;
    _dir_tile[0][5] = -hexGridWidth;
    _dir_tile[1][0] = -hexGridWidth - 1;
    _dir_tile[1][4] = 1 - hexGridWidth;
    _dir_tile[1][5] = -hexGridWidth;
    _dir_tile2[0][1] = -hexGridWidth - 1;
    _dir_tile2[1][4] = -hexGridWidth;
    _dir_tile2[0][2] = hexGridWidth - 1;
    _dir_tile2[1][5] = -hexGridWidth;
    _dir_tile2[1][0] = hexGridWidth + 1;
    _dir_tile2[1][3] = 1 - hexGridWidth;

    v11 = 0;
    v12 = 0;
    do {
        v13 = 64;
        do {
            _tile_mask[v12++] = v13 > v11;
            v13 -= 4;
        } while (v13);

        do {
            _tile_mask[v12++] = v13 > v11 ? 2 : 0;
            v13 += 4;
        } while (v13 != 64);

        v11 += 16;
    } while (v11 != 64);

    v11 = 0;
    do {
        v13 = 0;
        do {
            _tile_mask[v12++] = 0;
            v13++;
        } while (v13 < 32);
        v11++;
    } while (v11 < 8);

    v11 = 0;
    do {
        v13 = 0;
        do {
            _tile_mask[v12++] = v13 > v11 ? 0 : 3;
            v13 += 4;
        } while (v13 != 64);

        v13 = 64;
        do {
            _tile_mask[v12++] = v13 > v11 ? 0 : 4;
            v13 -= 4;
        } while (v13);

        v11 += 16;
    } while (v11 != 64);

    bufferFill(_tile_grid, 32, 16, 32, 0);
    bufferDrawLine(_tile_grid, 32, 16, 0, 31, 4, _colorTable[4228]);
    bufferDrawLine(_tile_grid, 32, 31, 4, 31, 12, _colorTable[4228]);
    bufferDrawLine(_tile_grid, 32, 31, 12, 16, 15, _colorTable[4228]);
    bufferDrawLine(_tile_grid, 32, 0, 12, 16, 15, _colorTable[4228]);
    bufferDrawLine(_tile_grid, 32, 0, 4, 0, 12, _colorTable[4228]);
    bufferDrawLine(_tile_grid, 32, 16, 0, 0, 4, _colorTable[4228]);

    bufferFill(_tile_grid_occupied, 32, 16, 32, 0);
    bufferDrawLine(_tile_grid_occupied, 32, 16, 0, 31, 4, _colorTable[31]);
    bufferDrawLine(_tile_grid_occupied, 32, 31, 4, 31, 12, _colorTable[31]);
    bufferDrawLine(_tile_grid_occupied, 32, 31, 12, 16, 15, _colorTable[31]);
    bufferDrawLine(_tile_grid_occupied, 32, 0, 12, 16, 15, _colorTable[31]);
    bufferDrawLine(_tile_grid_occupied, 32, 0, 4, 0, 12, _colorTable[31]);
    bufferDrawLine(_tile_grid_occupied, 32, 16, 0, 0, 4, _colorTable[31]);

    bufferFill(_tile_grid_blocked, 32, 16, 32, 0);
    bufferDrawLine(_tile_grid_blocked, 32, 16, 0, 31, 4, _colorTable[31744]);
    bufferDrawLine(_tile_grid_blocked, 32, 31, 4, 31, 12, _colorTable[31744]);
    bufferDrawLine(_tile_grid_blocked, 32, 31, 12, 16, 15, _colorTable[31744]);
    bufferDrawLine(_tile_grid_blocked, 32, 0, 12, 16, 15, _colorTable[31744]);
    bufferDrawLine(_tile_grid_blocked, 32, 0, 4, 0, 12, _colorTable[31744]);
    bufferDrawLine(_tile_grid_blocked, 32, 16, 0, 0, 4, _colorTable[31744]);

    for (v20 = 0; v20 < 16; v20++) {
        v21 = v20 * 32;
        v22 = 31;
        v23 = v21 + 31;

        if (_tile_grid_blocked[v23] == 0) {
            do {
                --v22;
                --v23;
            } while (v22 > 0 && _tile_grid_blocked[v23] == 0);
        }

        v24 = v21;
        v25 = 0;
        if (_tile_grid_blocked[v21] == 0) {
            do {
                ++v25;
                ++v24;
            } while (v25 < 32 && _tile_grid_blocked[v24] == 0);
        }

        bufferDrawLine(_tile_grid_blocked, 32, v25, v20, v22, v20, _colorTable[31744]);
    }

    // In order to calculate scroll borders correctly we need to pretend we're
    // at original resolution. Since border is calculated only once at start,
    // there is not need to change it all the time.
    gTileWindowWidth = ORIGINAL_ISO_WINDOW_WIDTH;
    gTileWindowHeight = ORIGINAL_ISO_WINDOW_HEIGHT;

    tileSetCenter(hexGridWidth * (hexGridHeight / 2) + hexGridWidth / 2, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    tileSetBorder(windowWidth, windowHeight, hexGridWidth, hexGridHeight);

    // Restore actual window size and set center one more time to calculate
    // correct screen offsets, which are required for subsequent object update
    // area calculations.
    gTileWindowWidth = windowWidth;
    gTileWindowHeight = windowHeight;

    tileSetCenter(hexGridWidth * (hexGridHeight / 2) + hexGridWidth / 2, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);

    if (compat_stricmp(settings.system.executable.c_str(), "mapper") == 0) {
        gTileWindowRefreshElevationProc = tileRefreshMapper;
    }

    return 0;
}

// 0x4B11E4
static void tileSetBorder(int windowWidth, int windowHeight, int hexGridWidth, int hexGridHeight)
{
    // TODO: Borders, scroll blockers and tile system overall were designed
    // with 640x480 in mind, so using windowWidth and windowHeight is
    // meaningless for calculating borders. For now keep borders for original
    // resolution.
    int v1 = tileFromScreenXY(-320, -240, 0);
    int v2 = tileFromScreenXY(-320, ORIGINAL_ISO_WINDOW_HEIGHT + 240, 0);

    gTileBorderMinX = abs(hexGridWidth - 1 - v2 % hexGridWidth - _tile_x) + 6;
    gTileBorderMinY = abs(_tile_y - v1 / hexGridWidth) + 7;
    gTileBorderMaxX = hexGridWidth - gTileBorderMinX - 1;
    gTileBorderMaxY = hexGridHeight - gTileBorderMinY - 1;

    if ((gTileBorderMinX & 1) == 0) {
        gTileBorderMinX++;
    }

    if ((gTileBorderMaxX & 1) == 0) {
        gTileBorderMinX--;
    }

    gTileBorderInitialized = true;
}

// NOTE: Collapsed.
//
// 0x4B129C
void _tile_reset_()
{
}

// NOTE: Uncollapsed 0x4B129C.
void tileReset()
{
    _tile_reset_();
}

// NOTE: Uncollapsed 0x4B129C.
void tileExit()
{
    _tile_reset_();
}

// 0x4B12A8
void tileDisable()
{
    gTileEnabled = false;
}

// 0x4B12B4
void tileEnable()
{
    gTileEnabled = true;
}

// 0x4B12C0
void tileWindowRefreshRect(Rect* rect, int elevation)
{
    if (gTileEnabled) {
        if (elevation == gElevation) {
            gTileWindowRefreshElevationProc(rect, elevation);
        }
    }
}

// 0x4B12D8
void tileWindowRefresh()
{
    if (gTileEnabled) {
        gTileWindowRefreshElevationProc(&gTileWindowRect, gElevation);
    }
}

// 0x4B12F8
int tileSetCenter(int tile, int flags)
{
    if (!tileIsValid(tile)) {
        return -1;
    }

    if ((flags & TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS) == 0) {
        if (gTileScrollLimitingEnabled) {
            int tileScreenX;
            int tileScreenY;
            tileToScreenXY(tile, &tileScreenX, &tileScreenY, gElevation);

            int dudeScreenX;
            int dudeScreenY;
            tileToScreenXY(gDude->tile, &dudeScreenX, &dudeScreenY, gElevation);

            int dx = abs(dudeScreenX - tileScreenX);
            int dy = abs(dudeScreenY - tileScreenY);

            if (dx > abs(dudeScreenX - _tile_offx)
                || dy > abs(dudeScreenY - _tile_offy)) {
                if (dx >= 480 || dy >= 400) {
                    return -1;
                }
            }
        }

        if (gTileScrollBlockingEnabled) {
            if (_obj_scroll_blocking_at(tile, gElevation) == 0) {
                return -1;
            }
        }
    }

    int tile_x = gHexGridWidth - 1 - tile % gHexGridWidth;
    int tile_y = tile / gHexGridWidth;

    if (gTileBorderInitialized) {
        if (tile_x <= gTileBorderMinX || tile_x >= gTileBorderMaxX || tile_y <= gTileBorderMinY || tile_y >= gTileBorderMaxY) {
            return -1;
        }
    }

    _tile_y = tile_y;
    _tile_offx = (gTileWindowWidth - 32) / 2;
    _tile_x = tile_x;
    _tile_offy = (gTileWindowHeight - 16) / 2;

    if (tile_x & 1) {
        _tile_x -= 1;
        _tile_offx -= 32;
    }

    _square_x = _tile_x / 2;
    _square_y = _tile_y / 2;
    _square_offx = _tile_offx - 16;
    _square_offy = _tile_offy - 2;

    if (_tile_y & 1) {
        _square_offy -= 12;
        _square_offx -= 16;
    }

    gCenterTile = tile;

    tile_hires_stencil_on_center_tile_or_elevation_change();

    if ((flags & TILE_SET_CENTER_REFRESH_WINDOW) != 0) {
        // NOTE: Uninline.
        tileWindowRefresh();
    }

    return 0;
}

// 0x4B1554
static void tileRefreshMapper(Rect* rect, int elevation)
{
    Rect rectToUpdate;

    if (rectIntersection(rect, &gTileWindowRect, &rectToUpdate) == -1) {
        return;
    }

    bufferFill(gTileWindowBuffer + gTileWindowPitch * rectToUpdate.top + rectToUpdate.left,
        rectToUpdate.right - rectToUpdate.left + 1,
        rectToUpdate.bottom - rectToUpdate.top + 1,
        gTileWindowPitch,
        0);

    tileRenderFloorsInRect(&rectToUpdate, elevation);
    _grid_render(&rectToUpdate, elevation);
    _obj_render_pre_roof(&rectToUpdate, elevation);
    tileRenderRoofsInRect(&rectToUpdate, elevation);
    _obj_render_post_roof(&rectToUpdate, elevation);

    tile_hires_stencil_draw(&rectToUpdate, gTileWindowBuffer, gTileWindowWidth, gTileWindowHeight);

    gTileWindowRefreshProc(&rectToUpdate);
}

// 0x4B15E8
static void tileRefreshGame(Rect* rect, int elevation)
{
    Rect rectToUpdate;

    if (rectIntersection(rect, &gTileWindowRect, &rectToUpdate) == -1) {
        return;
    }

    // CE: Clear dirty rect to prevent most of the visual artifacts near map
    // edges.
    bufferFill(gTileWindowBuffer + rectToUpdate.top * gTileWindowPitch + rectToUpdate.left,
        rectGetWidth(&rectToUpdate),
        rectGetHeight(&rectToUpdate),
        gTileWindowPitch,
        0);

    tileRenderFloorsInRect(&rectToUpdate, elevation);
    _obj_render_pre_roof(&rectToUpdate, elevation);
    tileRenderRoofsInRect(&rectToUpdate, elevation);
    _obj_render_post_roof(&rectToUpdate, elevation);

    tile_hires_stencil_draw(&rectToUpdate, gTileWindowBuffer, gTileWindowWidth, gTileWindowHeight);

    gTileWindowRefreshProc(&rectToUpdate);
}

// 0x4B1634
void tile_toggle_roof(bool refresh)
{
    gTileRoofIsVisible = !gTileRoofIsVisible;

    if (refresh) {
        // NOTE: Uninline.
        tileWindowRefresh();
    }
}

// 0x4B166C
int tileRoofIsVisible()
{
    return gTileRoofIsVisible;
}

// 0x4B1674
int tileToScreenXY(int tile, int* screenX, int* screenY, int elevation)
{
    int v3;
    int v4;
    int v5;
    int v6;

    if (!tileIsValid(tile)) {
        return -1;
    }

    v3 = gHexGridWidth - 1 - tile % gHexGridWidth;
    v4 = tile / gHexGridWidth;

    *screenX = _tile_offx;
    *screenY = _tile_offy;

    v5 = (v3 - _tile_x) / -2;
    *screenX += 48 * ((v3 - _tile_x) / 2);
    *screenY += 12 * v5;

    if (v3 & 1) {
        if (v3 <= _tile_x) {
            *screenX -= 16;
            *screenY += 12;
        } else {
            *screenX += 32;
        }
    }

    v6 = v4 - _tile_y;
    *screenX += 16 * v6;
    *screenY += 12 * v6;

    return 0;
}

// CE: Added optional `ignoreBounds` param to return tile number without
// validating hex grid bounds. The resulting invalid tile number serves as an
// origin for calculations using prepared offsets table during objects
// rendering.
//
// 0x4B1754
int tileFromScreenXY(int screenX, int screenY, int elevation, bool ignoreBounds)
{
    int v2;
    int v3;
    int v4;
    int v5;
    int v6;
    int v7;
    int v8;
    int v9;
    int v10;
    int v11;
    int v12;

    v2 = screenY - _tile_offy;
    if (v2 >= 0) {
        v3 = v2 / 12;
    } else {
        v3 = (v2 + 1) / 12 - 1;
    }

    v4 = screenX - _tile_offx - 16 * v3;
    v5 = v2 - 12 * v3;

    if (v4 >= 0) {
        v6 = v4 / 64;
    } else {
        v6 = (v4 + 1) / 64 - 1;
    }

    v7 = v6 + v3;
    v8 = v4 - (v6 * 64);
    v9 = 2 * v6;

    if (v8 >= 32) {
        v8 -= 32;
        v9++;
    }

    v10 = _tile_y + v7;
    v11 = _tile_x + v9;

    switch (_tile_mask[32 * v5 + v8]) {
    case 2:
        v11++;
        if (v11 & 1) {
            v10--;
        }
        break;
    case 1:
        v10--;
        break;
    case 3:
        v11--;
        if (!(v11 & 1)) {
            v10++;
        }
        break;
    case 4:
        v10++;
        break;
    default:
        break;
    }

    v12 = gHexGridWidth - 1 - v11;
    if (v12 >= 0 && v12 < gHexGridWidth && v10 >= 0 && v10 < gHexGridHeight) {
        return gHexGridWidth * v10 + v12;
    }

    if (ignoreBounds) {
        return gHexGridWidth * v10 + v12;
    }

    return -1;
}

// tile_distance
// 0x4B185C
int tileDistanceBetween(int tile1, int tile2)
{
    int i;
    int v9;
    int v8;
    int v2;

    if (tile1 == -1) {
        return 9999;
    }

    if (tile2 == -1) {
        return 9999;
    }

    int x1;
    int y1;
    tileToScreenXY(tile2, &x1, &y1, 0);

    v2 = tile1;
    for (i = 0; v2 != tile2; i++) {
        // TODO: Looks like inlined rotation_to_tile.
        int x2;
        int y2;
        tileToScreenXY(v2, &x2, &y2, 0);

        int dx = x1 - x2;
        int dy = y1 - y2;

        if (x1 == x2) {
            if (dy < 0) {
                v9 = 0;
            } else {
                v9 = 2;
            }
        } else {
            v8 = (int)trunc(atan2((double)-dy, (double)dx) * 180.0 * 0.3183098862851122);

            v9 = 360 - (v8 + 180) - 90;
            if (v9 < 0) {
                v9 += 360;
            }

            v9 /= 60;

            if (v9 >= 6) {
                v9 = 5;
            }
        }

        v2 += _dir_tile[v2 % gHexGridWidth & 1][v9];
    }

    return i;
}

// 0x4B1994
bool tileIsInFrontOf(int tile1, int tile2)
{
    int x1;
    int y1;
    tileToScreenXY(tile1, &x1, &y1, 0);

    int x2;
    int y2;
    tileToScreenXY(tile2, &x2, &y2, 0);

    int dx = x2 - x1;
    int dy = y2 - y1;

    return (double)dx <= (double)dy * dbl_50E7C7;
}

// 0x4B1A00
bool tileIsToRightOf(int tile1, int tile2)
{
    int x1;
    int y1;
    tileToScreenXY(tile1, &x1, &y1, 0);

    int x2;
    int y2;
    tileToScreenXY(tile2, &x2, &y2, 0);

    int dx = x2 - x1;
    int dy = y2 - y1;

    // NOTE: the value below looks like 4/3, which is 0x3FF55555555555, but it's
    // binary value is slightly different: 0x3FF55555555556. This difference plays
    // important role as seen right in the beginning of the game, comparing tiles
    // 17488 (0x4450) and 15288 (0x3BB8).
    return (double)dx <= (double)dy * 1.3333333333333335;
}

// tile_num_in_direction
// 0x4B1A6C
int tileGetTileInDirection(int tile, int rotation, int distance)
{
    int newTile = tile;
    for (int index = 0; index < distance; index++) {
        if (tileIsEdge(newTile)) {
            break;
        }

        int parity = (newTile % gHexGridWidth) & 1;
        newTile += _dir_tile[parity][rotation];
    }

    return newTile;
}

// rotation_to_tile
// 0x4B1ABC
int tileGetRotationTo(int tile1, int tile2)
{
    int x1;
    int y1;
    tileToScreenXY(tile1, &x1, &y1, 0);

    int x2;
    int y2;
    tileToScreenXY(tile2, &x2, &y2, 0);

    int dy = y2 - y1;
    x2 -= x1;
    y2 -= y1;

    if (x2 != 0) {
        // TODO: Check.
        int v6 = (int)trunc(atan2((double)-dy, (double)x2) * 180.0 * 0.3183098862851122);
        int v7 = 360 - (v6 + 180) - 90;
        if (v7 < 0) {
            v7 += 360;
        }

        v7 /= 60;

        if (v7 >= ROTATION_COUNT) {
            v7 = ROTATION_NW;
        }
        return v7;
    }

    return dy < 0 ? ROTATION_NE : ROTATION_SE;
}

// 0x4B1B84
int _tile_num_beyond(int from, int to, int distance)
{
    if (distance <= 0 || from == to) {
        return from;
    }

    int fromX;
    int fromY;
    tileToScreenXY(from, &fromX, &fromY, 0);
    fromX += 16;
    fromY += 8;

    int toX;
    int toY;
    tileToScreenXY(to, &toX, &toY, 0);
    toX += 16;
    toY += 8;

    int deltaX = toX - fromX;
    int deltaY = toY - fromY;

    int v27 = 2 * abs(deltaX);

    int stepX = 0;
    if (deltaX > 0)
        stepX = 1;
    else if (deltaX < 0)
        stepX = -1;

    int v26 = 2 * abs(deltaY);

    int stepY = 0;
    if (deltaY > 0)
        stepY = 1;
    else if (deltaY < 0)
        stepY = -1;

    int v28 = from;
    int tileX = fromX;
    int tileY = fromY;

    int v6 = 0;

    if (v27 > v26) {
        int middle = v26 - v27 / 2;
        while (true) {
            int tile = tileFromScreenXY(tileX, tileY, 0);
            if (tile != v28) {
                v6 += 1;
                if (v6 == distance || tileIsEdge(tile)) {
                    return tile;
                }

                v28 = tile;
            }

            if (middle >= 0) {
                middle -= v27;
                tileY += stepY;
            }

            middle += v26;
            tileX += stepX;
        }
    } else {
        int middle = v27 - v26 / 2;
        while (true) {
            int tile = tileFromScreenXY(tileX, tileY, 0);
            if (tile != v28) {
                v6 += 1;
                if (v6 == distance || tileIsEdge(tile)) {
                    return tile;
                }

                v28 = tile;
            }

            if (middle >= 0) {
                middle -= v26;
                tileX += stepX;
            }

            middle += v27;
            tileY += stepY;
        }
    }

    assert(false && "Should be unreachable");
}

// 0x4B1D20
bool tileIsEdge(int tile)
{
    if (!tileIsValid(tile)) {
        return false;
    }

    if (tile < gHexGridWidth) {
        return true;
    }

    if (tile >= gHexGridSize - gHexGridWidth) {
        return true;
    }

    if (tile % gHexGridWidth == 0) {
        return true;
    }

    if (tile % gHexGridWidth == gHexGridWidth - 1) {
        return true;
    }

    return false;
}

// 0x4B1D80
void tileScrollBlockingEnable()
{
    gTileScrollBlockingEnabled = true;
}

// 0x4B1D8C
void tileScrollBlockingDisable()
{
    gTileScrollBlockingEnabled = false;
}

// 0x4B1D98
bool tileScrollBlockingIsEnabled()
{
    return gTileScrollBlockingEnabled;
}

// 0x4B1DA0
void tileScrollLimitingEnable()
{
    gTileScrollLimitingEnabled = true;
}

// 0x4B1DAC
void tileScrollLimitingDisable()
{
    gTileScrollLimitingEnabled = false;
}

// 0x4B1DB8
bool tileScrollLimitingIsEnabled()
{
    return gTileScrollLimitingEnabled;
}

// 0x4B1DC0
int squareTileToScreenXY(int squareTile, int* coordX, int* coordY, int elevation)
{
    int v5;
    int v6;
    int v7;
    int v8;
    int v9;

    if (squareTile < 0 || squareTile >= gSquareGridSize) {
        return -1;
    }

    v5 = gSquareGridWidth - 1 - squareTile % gSquareGridWidth;
    v6 = squareTile / gSquareGridWidth;
    v7 = _square_x;

    *coordX = _square_offx;
    *coordY = _square_offy;

    v8 = v5 - v7;
    *coordX += 48 * v8;
    *coordY -= 12 * v8;

    v9 = v6 - _square_y;
    *coordX += 32 * v9;
    *coordY += 24 * v9;

    return 0;
}

// 0x4B1E60
int squareTileToRoofScreenXY(int squareTile, int* screenX, int* screenY, int elevation)
{
    int v5;
    int v6;
    int v7;
    int v8;
    int v9;
    int v10;

    if (squareTile < 0 || squareTile >= gSquareGridSize) {
        return -1;
    }

    v5 = gSquareGridWidth - 1 - squareTile % gSquareGridWidth;
    v6 = squareTile / gSquareGridWidth;
    v7 = _square_x;
    *screenX = _square_offx;
    *screenY = _square_offy;

    v8 = v5 - v7;
    *screenX += 48 * v8;
    *screenY -= 12 * v8;

    v9 = v6 - _square_y;
    *screenX += 32 * v9;
    v10 = 24 * v9 + *screenY;
    *screenY = v10;
    *screenY = v10 - 96;

    return 0;
}

// 0x4B1F04
int squareTileFromScreenXY(int screenX, int screenY, int elevation)
{
    int coordY;
    int coordX;

    squareTileScreenToCoord(screenX, screenY, elevation, &coordX, &coordY);

    if (coordX >= 0 && coordX < gSquareGridWidth && coordY >= 0 && coordY < gSquareGridHeight) {
        return coordX + gSquareGridWidth * coordY;
    }

    return -1;
}

// 0x4B1F94
void squareTileScreenToCoord(int screenX, int screenY, int elevation, int* coordX, int* coordY)
{
    int v4;
    int v5;
    int v6;
    int v8;

    v4 = screenX - _square_offx;
    v5 = screenY - _square_offy - 12;
    v6 = 3 * v4 - 4 * v5;
    *coordX = v6 >= 0 ? (v6 / 192) : ((v6 + 1) / 192 - 1);

    v8 = 4 * v5 + v4;
    *coordY = v8 >= 0 ? (v8 / 128) : ((v8 + 1) / 128 - 1);

    *coordX += _square_x;
    *coordY += _square_y;

    *coordX = gSquareGridWidth - 1 - *coordX;
}

// 0x4B203C
void squareTileScreenToCoordRoof(int screenX, int screenY, int elevation, int* coordX, int* coordY)
{
    int v4;
    int v5;
    int v6;
    int v8;

    v4 = screenX - _square_offx;
    v5 = screenY + 96 - _square_offy - 12;
    v6 = 3 * v4 - 4 * v5;

    *coordX = (v6 >= 0) ? (v6 / 192) : ((v6 + 1) / 192 - 1);

    v8 = 4 * v5 + v4;
    *coordY = v8 >= 0 ? (v8 / 128) : ((v8 + 1) / 128 - 1);

    *coordX += _square_x;
    *coordY += _square_y;

    *coordX = gSquareGridWidth - 1 - *coordX;
}

// 0x4B20E8
void tileRenderRoofsInRect(Rect* rect, int elevation)
{
    if (!gTileRoofIsVisible) {
        return;
    }

    int temp;
    int minY;
    int minX;
    int maxX;
    int maxY;

    squareTileScreenToCoordRoof(rect->left, rect->top, elevation, &temp, &minY);
    squareTileScreenToCoordRoof(rect->right, rect->top, elevation, &minX, &temp);
    squareTileScreenToCoordRoof(rect->left, rect->bottom, elevation, &maxX, &temp);
    squareTileScreenToCoordRoof(rect->right, rect->bottom, elevation, &temp, &maxY);

    if (minX < 0) {
        minX = 0;
    }

    if (minX >= gSquareGridWidth) {
        minX = gSquareGridWidth - 1;
    }

    if (minY < 0) {
        minY = 0;
    }

    // FIXME: Probably a bug - testing X, then changing Y.
    if (minX >= gSquareGridHeight) {
        minY = gSquareGridHeight - 1;
    }

    int light = lightGetAmbientIntensity();

    int baseSquareTile = gSquareGridWidth * minY;

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            int squareTile = baseSquareTile + x;
            int frmId = gTileSquares[elevation]->field_0[squareTile];
            frmId >>= 16;
            if ((((frmId & 0xF000) >> 12) & 0x01) == 0) {
                int fid = buildFid(OBJ_TYPE_TILE, frmId & 0xFFF, 0, 0, 0);
                if (fid != buildFid(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
                    int screenX;
                    int screenY;
                    squareTileToRoofScreenXY(squareTile, &screenX, &screenY, elevation);
                    tileRenderRoof(fid, screenX, screenY, rect, light);
                }
            }
        }
        baseSquareTile += gSquareGridWidth;
    }
}

static void roof_fill_push_task_if_in_bounds(std::stack<roof_fill_task>& tasks_stack, int x, int y)
{
    if (x >= 0 && x < gSquareGridWidth && y >= 0 && y < gSquareGridHeight) {
        tasks_stack.push(roof_fill_task { x, y });
    };
};

static void roof_fill_off_process_task(std::stack<roof_fill_task>& tasks_stack, int elevation, bool on)
{
    auto [x, y] = tasks_stack.top();
    tasks_stack.pop();

    int squareTileIndex = gSquareGridWidth * y + x;
    int squareTile = gTileSquares[elevation]->field_0[squareTileIndex];
    int roof = (squareTile >> 16) & 0xFFFF;

    int id = roof & 0xFFF;
    if (buildFid(OBJ_TYPE_TILE, id, 0, 0, 0) != buildFid(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
        int flag = (roof & 0xF000) >> 12;

        if (on ? ((flag & 0x01) != 0) : ((flag & 0x03) == 0)) {
            if (on) {
                flag &= ~0x01;
            } else {
                flag |= 0x01;
            }

            gTileSquares[elevation]->field_0[squareTileIndex] = (squareTile & 0xFFFF) | (((flag << 12) | id) << 16);

            roof_fill_push_task_if_in_bounds(tasks_stack, x - 1, y);
            roof_fill_push_task_if_in_bounds(tasks_stack, x + 1, y);
            roof_fill_push_task_if_in_bounds(tasks_stack, x, y - 1);
            roof_fill_push_task_if_in_bounds(tasks_stack, x, y + 1);
        }
    }
}

// 0x4B23D4
void tile_fill_roof(int x, int y, int elevation, bool on)
{
    std::stack<roof_fill_task> tasks_stack;

    roof_fill_push_task_if_in_bounds(tasks_stack, x, y);

    while (!tasks_stack.empty()) {
        roof_fill_off_process_task(tasks_stack, elevation, on);
    }
}

// 0x4B24E0
static void tileRenderRoof(int fid, int x, int y, Rect* rect, int light)
{
    CacheEntry* tileFrmHandle;
    Art* tileFrm = artLock(fid, &tileFrmHandle);
    if (tileFrm == nullptr) {
        return;
    }

    int tileWidth = artGetWidth(tileFrm, 0, 0);
    int tileHeight = artGetHeight(tileFrm, 0, 0);

    Rect tileRect;
    tileRect.left = x;
    tileRect.top = y;
    tileRect.right = x + tileWidth - 1;
    tileRect.bottom = y + tileHeight - 1;

    if (rectIntersection(&tileRect, rect, &tileRect) == 0) {
        unsigned char* tileFrmBuffer = artGetFrameData(tileFrm, 0, 0);
        tileFrmBuffer += tileWidth * (tileRect.top - y) + (tileRect.left - x);

        CacheEntry* eggFrmHandle;
        Art* eggFrm = artLock(gEgg->fid, &eggFrmHandle);
        if (eggFrm != nullptr) {
            int eggWidth = artGetWidth(eggFrm, 0, 0);
            int eggHeight = artGetHeight(eggFrm, 0, 0);

            int eggScreenX;
            int eggScreenY;
            tileToScreenXY(gEgg->tile, &eggScreenX, &eggScreenY, gEgg->elevation);

            eggScreenX += 16;
            eggScreenY += 8;

            eggScreenX += eggFrm->xOffsets[0];
            eggScreenY += eggFrm->yOffsets[0];

            eggScreenX += gEgg->x;
            eggScreenY += gEgg->y;

            Rect eggRect;
            eggRect.left = eggScreenX - eggWidth / 2;
            eggRect.top = eggScreenY - eggHeight + 1;
            eggRect.right = eggRect.left + eggWidth - 1;
            eggRect.bottom = eggScreenY;

            gEgg->sx = eggRect.left;
            gEgg->sy = eggRect.top;

            Rect intersectedRect;
            if (rectIntersection(&eggRect, &tileRect, &intersectedRect) == 0) {
                Rect rects[4];

                rects[0].left = tileRect.left;
                rects[0].top = tileRect.top;
                rects[0].right = tileRect.right;
                rects[0].bottom = intersectedRect.top - 1;

                rects[1].left = tileRect.left;
                rects[1].top = intersectedRect.top;
                rects[1].right = intersectedRect.left - 1;
                rects[1].bottom = intersectedRect.bottom;

                rects[2].left = intersectedRect.right + 1;
                rects[2].top = intersectedRect.top;
                rects[2].right = tileRect.right;
                rects[2].bottom = intersectedRect.bottom;

                rects[3].left = tileRect.left;
                rects[3].top = intersectedRect.bottom + 1;
                rects[3].right = tileRect.right;
                rects[3].bottom = tileRect.bottom;

                for (int i = 0; i < 4; i++) {
                    Rect* cr = &(rects[i]);
                    if (cr->left <= cr->right && cr->top <= cr->bottom) {
                        _dark_trans_buf_to_buf(tileFrmBuffer + tileWidth * (cr->top - tileRect.top) + (cr->left - tileRect.left),
                            cr->right - cr->left + 1,
                            cr->bottom - cr->top + 1,
                            tileWidth,
                            gTileWindowBuffer,
                            cr->left,
                            cr->top,
                            gTileWindowPitch,
                            light);
                    }
                }

                unsigned char* eggBuf = artGetFrameData(eggFrm, 0, 0);
                _intensity_mask_buf_to_buf(tileFrmBuffer + tileWidth * (intersectedRect.top - tileRect.top) + (intersectedRect.left - tileRect.left),
                    intersectedRect.right - intersectedRect.left + 1,
                    intersectedRect.bottom - intersectedRect.top + 1,
                    tileWidth,
                    gTileWindowBuffer + gTileWindowPitch * intersectedRect.top + intersectedRect.left,
                    gTileWindowPitch,
                    eggBuf + eggWidth * (intersectedRect.top - eggRect.top) + (intersectedRect.left - eggRect.left),
                    eggWidth,
                    light);
            } else {
                _dark_trans_buf_to_buf(tileFrmBuffer, tileRect.right - tileRect.left + 1, tileRect.bottom - tileRect.top + 1, tileWidth, gTileWindowBuffer, tileRect.left, tileRect.top, gTileWindowPitch, light);
            }

            artUnlock(eggFrmHandle);
        }
    }

    artUnlock(tileFrmHandle);
}

// 0x4B2944
void tileRenderFloorsInRect(Rect* rect, int elevation)
{
    int minY;
    int maxX;
    int maxY;
    int minX;
    int temp;

    squareTileScreenToCoord(rect->left, rect->top, elevation, &temp, &minY);
    squareTileScreenToCoord(rect->right, rect->top, elevation, &minX, &temp);
    squareTileScreenToCoord(rect->left, rect->bottom, elevation, &maxX, &temp);
    squareTileScreenToCoord(rect->right, rect->bottom, elevation, &temp, &maxY);

    if (minX < 0) {
        minX = 0;
    }

    if (minX >= gSquareGridWidth) {
        minX = gSquareGridWidth - 1;
    }

    if (minY < 0) {
        minY = 0;
    }

    if (minX >= gSquareGridHeight) {
        minY = gSquareGridHeight - 1;
    }

    lightGetAmbientIntensity();

    int baseSquareTile = gSquareGridWidth * minY;

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            int squareTile = baseSquareTile + x;
            int frmId = gTileSquares[elevation]->field_0[squareTile];
            if ((((frmId & 0xF000) >> 12) & 0x01) == 0) {
                int tileScreenX;
                int tileScreenY;
                squareTileToScreenXY(squareTile, &tileScreenX, &tileScreenY, elevation);
                int fid = buildFid(OBJ_TYPE_TILE, frmId & 0xFFF, 0, 0, 0);
                tileRenderFloor(fid, tileScreenX, tileScreenY, rect);
            }
        }
        baseSquareTile += gSquareGridWidth;
    }
}

// 0x4B2B10
bool _square_roof_intersect(int x, int y, int elevation)
{
    if (!gTileRoofIsVisible) {
        return false;
    }

    bool result = false;

    int tileX;
    int tileY;
    squareTileScreenToCoordRoof(x, y, elevation, &tileX, &tileY);

    TileData* ptr = gTileSquares[elevation];
    int idx = gSquareGridWidth * tileY + tileX;
    int upper = ptr->field_0[gSquareGridWidth * tileY + tileX] >> 16;
    int fid = buildFid(OBJ_TYPE_TILE, upper & 0xFFF, 0, 0, 0);
    if (fid != buildFid(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
        if ((((upper & 0xF000) >> 12) & 1) == 0) {
            int fid = buildFid(OBJ_TYPE_TILE, upper & 0xFFF, 0, 0, 0);
            CacheEntry* handle;
            Art* art = artLock(fid, &handle);
            if (art != nullptr) {
                unsigned char* data = artGetFrameData(art, 0, 0);
                if (data != nullptr) {
                    int v18;
                    int v17;
                    squareTileToRoofScreenXY(idx, &v18, &v17, elevation);

                    int width = artGetWidth(art, 0, 0);
                    if (data[width * (y - v17) + x - v18] != 0) {
                        result = true;
                    }
                }
                artUnlock(handle);
            }
        }
    }

    return result;
}

// 0x4B2E98
void _grid_render(Rect* rect, int elevation)
{
    if (!gTileGridIsVisible) {
        return;
    }

    for (int y = rect->top - 12; y < rect->bottom + 12; y += 6) {
        for (int x = rect->left - 32; x < rect->right + 32; x += 16) {
            int tile = tileFromScreenXY(x, y, elevation);
            _draw_grid(tile, elevation, rect);
        }
    }
}

// 0x4B2F4C
static void _draw_grid(int tile, int elevation, Rect* rect)
{
    if (tile == -1) {
        return;
    }

    int x;
    int y;
    tileToScreenXY(tile, &x, &y, elevation);

    Rect r;
    r.left = x;
    r.top = y;
    r.right = x + 32 - 1;
    r.bottom = y + 16 - 1;

    if (rectIntersection(&r, rect, &r) == -1) {
        return;
    }

    if (_obj_blocking_at(nullptr, tile, elevation) != nullptr) {
        blitBufferToBufferTrans(_tile_grid_blocked + 32 * (r.top - y) + (r.left - x),
            r.right - r.left + 1,
            r.bottom - r.top + 1,
            32,
            gTileWindowBuffer + gTileWindowPitch * r.top + r.left,
            gTileWindowPitch);
        return;
    }

    if (_obj_occupied(tile, elevation)) {
        blitBufferToBufferTrans(_tile_grid_occupied + 32 * (r.top - y) + (r.left - x),
            r.right - r.left + 1,
            r.bottom - r.top + 1,
            32,
            gTileWindowBuffer + gTileWindowPitch * r.top + r.left,
            gTileWindowPitch);
        return;
    }

    _translucent_trans_buf_to_buf(_tile_grid_occupied + 32 * (r.top - y) + (r.left - x),
        r.right - r.left + 1,
        r.bottom - r.top + 1,
        32,
        gTileWindowBuffer + gTileWindowPitch * r.top + r.left,
        0,
        0,
        gTileWindowPitch,
        _wallBlendTable,
        _commonGrayTable);
}

// 0x4B30C4
static void tileRenderFloor(int fid, int x, int y, Rect* rect)
{
    if (artIsObjectTypeHidden(FID_TYPE(fid)) != 0) {
        return;
    }

    CacheEntry* cacheEntry;
    Art* art = artLock(fid, &cacheEntry);
    if (art == nullptr) {
        return;
    }

    int elev = gElevation;
    int left = rect->left;
    int top = rect->top;
    int width = rect->right - rect->left + 1;
    int height = rect->bottom - rect->top + 1;
    int frameWidth;
    int frameHeight;
    int tile;
    int v76;
    int v77;
    int v78;
    int v79;

    int savedX = x;
    int savedY = y;

    if (left < 0) {
        left = 0;
    }

    if (top < 0) {
        top = 0;
    }

    if (left + width > gTileWindowWidth) {
        width = gTileWindowWidth - left;
    }

    if (top + height > gTileWindowHeight) {
        height = gTileWindowHeight - top;
    }

    if (x >= gTileWindowWidth || x > rect->right || y >= gTileWindowHeight || y > rect->bottom) goto out;

    frameWidth = artGetWidth(art, 0, 0);
    frameHeight = artGetHeight(art, 0, 0);

    if (left < x) {
        v79 = 0;
        int v12 = left + width;
        v77 = frameWidth + x <= v12 ? frameWidth : v12 - x;
    } else {
        v79 = left - x;
        x = left;
        v77 = frameWidth - v79;
        if (v77 > width) {
            v77 = width;
        }
    }

    if (top < y) {
        int v14 = height + top;
        v78 = 0;
        v76 = frameHeight + y <= v14 ? frameHeight : v14 - y;
    } else {
        v78 = top - y;
        y = top;
        v76 = frameHeight - v78;
        if (v76 > height) {
            v76 = height;
        }
    }

    if (v77 <= 0 || v76 <= 0) goto out;

    tile = tileFromScreenXY(savedX, savedY + 13, gElevation);
    if (tile != -1) {
        int parity = tile & 1;
        int ambientIntensity = lightGetAmbientIntensity();
        for (int i = 0; i < 10; i++) {
            // NOTE: Calls `lightGetTileIntensity` twice.
            _verticies[i].intensity = std::max(lightGetTileIntensity(elev, tile + _verticies[i].offsets[parity]), ambientIntensity);
        }

        int v23 = 0;
        for (int i = 0; i < 9; i++) {
            if (_verticies[i + 1].intensity != _verticies[i].intensity) {
                break;
            }

            v23++;
        }

        if (v23 == 9) {
            unsigned char* buf = artGetFrameData(art, 0, 0);
            _dark_trans_buf_to_buf(buf + frameWidth * v78 + v79, v77, v76, frameWidth, gTileWindowBuffer, x, y, gTileWindowPitch, _verticies[0].intensity);
            goto out;
        }

        for (int i = 0; i < 5; i++) {
            RightsideUpTriangle* triangle = &(_rightside_up_triangles[i]);
            int v32 = _verticies[triangle->field_8].intensity;
            int v33 = _verticies[triangle->field_8].field_0;
            int v34 = _verticies[triangle->field_4].intensity - _verticies[triangle->field_0].intensity;
            // TODO: Probably wrong.
            int v35 = v34 / 32;
            int v36 = (_verticies[triangle->field_0].intensity - v32) / 13;
            int* v37 = &(_intensity_map[v33]);
            if (v35 != 0) {
                if (v36 != 0) {
                    for (int i = 0; i < 13; i++) {
                        int v41 = v32;
                        int v42 = _rightside_up_table[i].field_4;
                        v37 += _rightside_up_table[i].field_0;
                        for (int j = 0; j < v42; j++) {
                            *v37++ = v41;
                            v41 += v35;
                        }
                        v32 += v36;
                    }
                } else {
                    for (int i = 0; i < 13; i++) {
                        int v38 = v32;
                        int v39 = _rightside_up_table[i].field_4;
                        v37 += _rightside_up_table[i].field_0;
                        for (int j = 0; j < v39; j++) {
                            *v37++ = v38;
                            v38 += v35;
                        }
                    }
                }
            } else {
                if (v36 != 0) {
                    for (int i = 0; i < 13; i++) {
                        int v46 = _rightside_up_table[i].field_4;
                        v37 += _rightside_up_table[i].field_0;
                        for (int j = 0; j < v46; j++) {
                            *v37++ = v32;
                        }
                        v32 += v36;
                    }
                } else {
                    for (int i = 0; i < 13; i++) {
                        int v44 = _rightside_up_table[i].field_4;
                        v37 += _rightside_up_table[i].field_0;
                        for (int j = 0; j < v44; j++) {
                            *v37++ = v32;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < 5; i++) {
            UpsideDownTriangle* triangle = &(_upside_down_triangles[i]);
            int v50 = _verticies[triangle->field_0].intensity;
            int v51 = _verticies[triangle->field_0].field_0;
            int v52 = _verticies[triangle->field_8].intensity - v50;
            // TODO: Probably wrong.
            int v53 = v52 / 32;
            int v54 = (_verticies[triangle->field_4].intensity - v50) / 13;
            int* v55 = &(_intensity_map[v51]);
            if (v53 != 0) {
                if (v54 != 0) {
                    for (int i = 0; i < 13; i++) {
                        int v59 = v50;
                        int v60 = _upside_down_table[i].field_4;
                        v55 += _upside_down_table[i].field_0;
                        for (int j = 0; j < v60; j++) {
                            *v55++ = v59;
                            v59 += v53;
                        }
                        v50 += v54;
                    }
                } else {
                    for (int i = 0; i < 13; i++) {
                        int v56 = v50;
                        int v57 = _upside_down_table[i].field_4;
                        v55 += _upside_down_table[i].field_0;
                        for (int j = 0; j < v57; j++) {
                            *v55++ = v56;
                            v56 += v53;
                        }
                    }
                }
            } else {
                if (v54 != 0) {
                    for (int i = 0; i < 13; i++) {
                        int v64 = _upside_down_table[i].field_4;
                        v55 += _upside_down_table[i].field_0;
                        for (int j = 0; j < v64; j++) {
                            *v55++ = v50;
                        }
                        v50 += v54;
                    }
                } else {
                    for (int i = 0; i < 13; i++) {
                        int v62 = _upside_down_table[i].field_4;
                        v55 += _upside_down_table[i].field_0;
                        for (int j = 0; j < v62; j++) {
                            *v55++ = v50;
                        }
                    }
                }
            }
        }

        unsigned char* v66 = gTileWindowBuffer + gTileWindowPitch * y + x;
        unsigned char* v67 = artGetFrameData(art, 0, 0) + frameWidth * v78 + v79;
        int* v68 = &(_intensity_map[160 + 80 * v78]) + v79;
        int v86 = frameWidth - v77;
        int v85 = gTileWindowPitch - v77;
        int v87 = 80 - v77;

        while (--v76 != -1) {
            for (int kk = 0; kk < v77; kk++) {
                if (*v67 != 0) {
                    *v66 = intensityColorTable[*v67][*v68 >> 9];
                }
                v67++;
                v68++;
                v66++;
            }
            v66 += v85;
            v68 += v87;
            v67 += v86;
        }
    }

out:

    artUnlock(cacheEntry);
}

// 0x4B372C
static int _tile_make_line(int from, int to, int* tiles, int tilesCapacity)
{
    if (tilesCapacity <= 1) {
        return 0;
    }

    int count = 0;

    int fromX;
    int fromY;
    tileToScreenXY(from, &fromX, &fromY, gElevation);
    fromX += 16;
    fromY += 8;

    int toX;
    int toY;
    tileToScreenXY(to, &toX, &toY, gElevation);
    toX += 16;
    toY += 8;

    tiles[count++] = from;

    int stepX;
    int deltaX = toX - fromX;
    if (deltaX > 0)
        stepX = 1;
    else if (deltaX < 0)
        stepX = -1;
    else
        stepX = 0;

    int stepY;
    int deltaY = toY - fromY;
    if (deltaY > 0)
        stepY = 1;
    else if (deltaY < 0)
        stepY = -1;
    else
        stepY = 0;

    int v28 = 2 * abs(toX - fromX);
    int v27 = 2 * abs(toY - fromY);

    int tileX = fromX;
    int tileY = fromY;

    if (v28 <= v27) {
        int middleX = v28 - v27 / 2;
        while (true) {
            int tile = tileFromScreenXY(tileX, tileY, gElevation);
            tiles[count] = tile;

            if (tile == to) {
                count++;
                break;
            }

            if (tile != tiles[count - 1] && (count == 1 || tile != tiles[count - 2])) {
                count++;
                if (count == tilesCapacity) {
                    break;
                }
            }

            if (tileY == toY) {
                break;
            }

            if (middleX >= 0) {
                tileX += stepX;
                middleX -= v27;
            }

            middleX += v28;
            tileY += stepY;
        }
    } else {
        int middleY = v27 - v28 / 2;
        while (true) {
            int tile = tileFromScreenXY(tileX, tileY, gElevation);
            tiles[count] = tile;

            if (tile == to) {
                count++;
                break;
            }

            if (tile != tiles[count - 1] && (count == 1 || tile != tiles[count - 2])) {
                count++;
                if (count == tilesCapacity) {
                    break;
                }
            }

            if (tileX == toX) {
                return count;
            }

            if (middleY >= 0) {
                tileY += stepY;
                middleY -= v28;
            }

            middleY += v27;
            tileX += stepX;
        }
    }

    return count;
}

// 0x4B3924
int _tile_scroll_to(int tile, int flags)
{
    if (tile == gCenterTile) {
        return -1;
    }

    int oldCenterTile = gCenterTile;

    int v9[200];
    int count = _tile_make_line(gCenterTile, tile, v9, 200);
    if (count == 0) {
        return -1;
    }

    int index = 1;
    for (; index < count; index++) {
        if (tileSetCenter(v9[index], 0) == -1) {
            break;
        }
    }

    int rc = 0;
    if ((flags & 0x01) != 0) {
        if (index != count) {
            tileSetCenter(oldCenterTile, 0);
            rc = -1;
        }
    }

    if ((flags & 0x02) != 0) {
        // NOTE: Uninline.
        tileWindowRefresh();
    }

    return rc;
}

} // namespace fallout
