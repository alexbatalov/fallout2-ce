#include "tile_hires_stencil.h"
#include "debug.h"
#include "draw.h"
#include "geometry.h"
#include "sfall_config.h"
#include "stdio.h"
#include "tile.h"
#include <string.h>
#include <vector>

// #define DO_DEBUG_CHECKS 1

#ifdef DO_DEBUG_CHECKS
#include <stdlib.h>
#endif

namespace fallout {

//
// This file is responsible for drawing a overlay over tiles which
//   are not visible in original screen resolution.
//

/** This holds hex tiles which can be center tile */
static bool visited_tiles[ELEVATION_COUNT][HEX_GRID_SIZE];

static constexpr int pixels_per_horizontal_move = 32;
static constexpr int pixels_per_vertical_move = 24;

// A overlay is actually a grid of small squares, each square is 16x12 pixels
//  which is half of possible screen move by keyboard or mouse
static constexpr int squares_per_horizontal_move = 2;
static constexpr int squares_per_vertical_move = 2;
static constexpr int square_width = pixels_per_horizontal_move / squares_per_horizontal_move;
static constexpr int square_height = pixels_per_vertical_move / squares_per_vertical_move;

static_assert(pixels_per_horizontal_move % square_width == 0);
static_assert(pixels_per_vertical_move % square_height == 0);

// Dimensions of the grid of squares
static constexpr int square_grid_width = 500;
static constexpr int square_grid_height = 300;

// This array holds information about which squares are visible
static bool visible_squares[ELEVATION_COUNT][square_grid_width][square_grid_height];

// Dimensions of the grid of squares were calculated by
// going through all hex tiles and finding the lowest and highest x and y.
// This way we can be sure that all hex tiles can be covered by squares.
static_assert(square_width * square_grid_width == 8000);
static_assert(square_height * square_grid_height == 3600);

// What can be seen on the screen in original resolution
static constexpr int screen_view_width = 640;
static constexpr int screen_view_height = 380;

// How many squares can be seen in horizontal and vertical directions
static constexpr int squares_screen_width_half = screen_view_width / 2 / square_width;
static constexpr int squares_screen_height_half = screen_view_height / 2 / square_height;
// Horizontal visibility fits hex grid perfectly
static_assert(screen_view_width % (2 * square_width) == 0);
// In the vertical direction we have 10+10 pixels left per each direction
// which is covered by squares but theoretically could be seen in the original game
static_assert(screen_view_height % (2 * square_height) == 20);

static bool gIsTileHiresStencilEnabled = true;

static void clean_cache()
{
    memset(visited_tiles, 0, sizeof(visited_tiles));
    memset(visible_squares, 0, sizeof(visible_squares));
}
static void clean_cache_for_elevation(int elevation)
{
    memset(visited_tiles[elevation], 0, sizeof(visited_tiles[elevation]));
    memset(visible_squares[elevation], 0, sizeof(visible_squares[elevation]));
}

static Point get_screen_diff()
{
    constexpr int hex_tile_with_lowest_x = 199;
    constexpr int hex_tile_with_lowest_y = 0;

#ifdef DO_DEBUG_CHECKS
    {
        // Just to ensure once again that we have calculated the lowest and highest hex tiles correctly

        int minX = 0x7FFFFFFF;
        int minY = 0x7FFFFFFF;
        int maxX = 0;
        int maxY = 0;
        int minTileX = -1;
        int minTileY = -1;
        for (int t = 0; t < HEX_GRID_SIZE; t++) {
            int x;
            int y;
            tileToScreenXY(t, &x, &y, gElevation);
            if (x < minX) {
                minX = x;
                minTileX = t;
            };
            if (y < minY) {
                minY = y;
                minTileY = t;
            };
            if (x > maxX) {
                maxX = x;
            }
            if (y > maxY) {
                maxY = y;
            }
        }
        if (minTileX != hex_tile_with_lowest_x || minTileY != hex_tile_with_lowest_y) {
            printf("min tileX=%i x=%i tileY=%i y=%i maxX=%i maxY=%i dx=%i dy=%i\n",
                minTileX, minX,
                minTileY, minY,
                maxX, maxY,
                maxX - minX, maxY - minY);
            exit(108);
        }
    };
#endif

    int offsetX;
    int tmp;
    int offsetY;
    tileToScreenXY(hex_tile_with_lowest_x, &offsetX, &tmp, gElevation);
    tileToScreenXY(hex_tile_with_lowest_y, &tmp, &offsetY, gElevation);

    Point out;
    out.x = offsetX;
    out.y = offsetY;
    return out;
};

// This enum is used to tell the marking function to not to re-mark
// squares which are already marked by previous calls from neighboring tiles
enum class MarkOnlyPart {
    FULL,
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

static void mark_screen_tiles_around_as_visible(int center_tile, const Point& screen_diff, MarkOnlyPart part)
{
    int centerTileScreenX;
    int centerTileScreenY;
    tileToScreenXY(center_tile, &centerTileScreenX, &centerTileScreenY, gElevation);

    int centerTileGlobalX = centerTileScreenX - screen_diff.x;
    int centerTileGlobalY = centerTileScreenY - screen_diff.y;

#ifdef DO_DEBUG_CHECKS
    {
        if (centerTileGlobalX % square_width != 0 || centerTileGlobalY % square_height != 0) {
            printf("centerTileGlobalX=%i, centerTileGlobalY=%i\n", centerTileGlobalX, centerTileGlobalY);
            exit(101);
        }

        if (
            centerTileGlobalX - squares_screen_width_half * square_width < 0 || centerTileGlobalY - squares_screen_height_half * square_height < 0) {
            printf("centerTileGlobalX=%i, centerTileGlobalY=%i\n", centerTileGlobalX, centerTileGlobalY);
            printf("kekekek border=%i\n", gTileBorderInitialized);
            exit(102);
        }
        if (
            centerTileGlobalX + squares_screen_width_half * square_width >= square_width * square_grid_width
            || centerTileGlobalY + squares_screen_height_half * square_height >= square_height * square_grid_height) {
            printf("centerTileGlobalX=%i, centerTileGlobalY=%i\n", centerTileGlobalX, centerTileGlobalY);
            exit(103);
        }
    };
#endif

    int squareX = centerTileGlobalX / square_width;
    int squareY = centerTileGlobalY / square_height;

    int horizonal_start_full = squareX - squares_screen_width_half + 1;
    int horizonal_end_full = squareX + squares_screen_width_half;

    int horizontal_start = part != MarkOnlyPart::RIGHT
        ? horizonal_start_full
        : horizonal_end_full - squares_per_horizontal_move;
    int horizontal_end = part != MarkOnlyPart::LEFT
        ? horizonal_end_full
        : horizonal_start_full + squares_per_horizontal_move;

    int vertical_start_full = squareY - squares_screen_height_half;
    int vertical_end_full = squareY + squares_screen_height_half;

    int vertical_start = part != MarkOnlyPart::DOWN
        ? vertical_start_full
        : vertical_end_full - squares_per_vertical_move;
    int vertical_end = part != MarkOnlyPart::UP
        ? vertical_end_full
        : vertical_start_full + squares_per_vertical_move;

    for (int x = horizontal_start; x <= horizontal_end; x++) {
        for (int y = vertical_start; y <= vertical_end; y++) {
            visible_squares[gElevation][x][y] = true;
        }
    }
}

void tile_hires_stencil_on_center_tile_or_elevation_change()
{
    if (!gIsTileHiresStencilEnabled) {
        return;
    }

    if (!gTileBorderInitialized) {
        return;
    };

    if (visited_tiles[gElevation][gCenterTile]) {
        debugPrint("tile_hires_stencil_on_center_tile_or_elevation_change tile was visited gElevation=%i gCenterTile=%i so doing nothing\n",
            gElevation, gCenterTile);

        return;
    };

    debugPrint("tile_hires_stencil_on_center_tile_or_elevation_change non-visited tile gElevation=%i gCenterTile=%i\n",
        gElevation, gCenterTile);

    clean_cache_for_elevation(gElevation);

    struct TileToVisit {
        int tile;
        MarkOnlyPart part;
    };

    std::vector<struct TileToVisit> tiles_to_visit {};
    tiles_to_visit.reserve(7000);

    tiles_to_visit.push_back({ gCenterTile, MarkOnlyPart::FULL });

    int visited_tiles_count = 0;

    auto screen_diff = get_screen_diff();

    while (!tiles_to_visit.empty()) {
        auto tileInfo = tiles_to_visit.back();
        tiles_to_visit.pop_back();

        if (visited_tiles[gElevation][tileInfo.tile]) {
            continue;
        }

        if (tileInfo.tile != gCenterTile) [[unlikely]] {
            if (tileInfo.tile < 0 || tileInfo.tile >= HEX_GRID_SIZE) {
                continue;
            }
            if (_obj_scroll_blocking_at(tileInfo.tile, gElevation) == 0) {
                continue;
            }

            // TODO: Maybe create new function in tile.cc and use it here
            int tile_x = HEX_GRID_WIDTH - 1 - tileInfo.tile % HEX_GRID_WIDTH;
            int tile_y = tileInfo.tile / HEX_GRID_WIDTH;
            if (
                tile_x <= gTileBorderMinX || tile_x >= gTileBorderMaxX || tile_y <= gTileBorderMinY || tile_y >= gTileBorderMaxY) {
                continue;
            }
        }

        visited_tiles_count++;

        visited_tiles[gElevation][tileInfo.tile] = true;
        mark_screen_tiles_around_as_visible(tileInfo.tile, screen_diff, tileInfo.part);

        int tileScreenX;
        int tileScreenY;
        tileToScreenXY(tileInfo.tile, &tileScreenX, &tileScreenY, gElevation);

        // tile size is 32 x 18
        //
        //  / \      ^
        // |   |     | 18
        //  \ /      v
        //
        // <-32->
        //
        // Scrolling left-right changes x by 32
        // But scrolling top-bottom changes y by 24
        //
        //
        //        / \
        //       |   |         <----- tiles on vertical change, 24 px
        //      / \ / \            |
        //     |   |   |   <------ | ----- tiles on horizontal change, 32 px
        //      \ / \ /            |
        //       |   |         <--/
        //        \ /
        //

        constexpr int tile_center_offset_x = 16;
        constexpr int tile_center_offset_y = 8;

        tiles_to_visit.push_back({ tileFromScreenXY(
                                       tileScreenX - pixels_per_horizontal_move + tile_center_offset_x,
                                       tileScreenY + tile_center_offset_y,
                                       gElevation, true),
            MarkOnlyPart::LEFT });
        tiles_to_visit.push_back({ tileFromScreenXY(
                                       tileScreenX + pixels_per_horizontal_move + tile_center_offset_x,
                                       tileScreenY + tile_center_offset_y,
                                       gElevation, true),
            MarkOnlyPart::RIGHT });
        tiles_to_visit.push_back({ tileFromScreenXY(
                                       tileScreenX + tile_center_offset_x,
                                       tileScreenY - pixels_per_vertical_move + tile_center_offset_y,
                                       gElevation, true),
            MarkOnlyPart::UP });
        tiles_to_visit.push_back({ tileFromScreenXY(
                                       tileScreenX + tile_center_offset_x,
                                       tileScreenY + pixels_per_vertical_move + tile_center_offset_y,
                                       gElevation, true),
            MarkOnlyPart::DOWN });
    }

    debugPrint("tile_hires_stencil_on_center_tile_or_elevation_change visited_tiles_count=%i\n", visited_tiles_count);
}

void tile_hires_stencil_draw(Rect* rect, unsigned char* buffer, int windowWidth, int windowHeight)
{
    if (!gIsTileHiresStencilEnabled) {
        return;
    }

    int minX = rect->left;
    int minY = rect->top;
    int maxX = rect->right;
    int maxY = rect->bottom;

    auto screen_diff = get_screen_diff();
    int minXglobal = minX - screen_diff.x;
    int minYglobal = minY - screen_diff.y;
    int maxXglobal = maxX - screen_diff.x;
    int maxYglobal = maxY - screen_diff.y;

    if (minXglobal < 0) {
        // This should never happen
        minXglobal = 0;
    };
    if (minYglobal < 0) {
        // This should never happen
        minYglobal = 0;
    };
    if (maxXglobal >= square_width * square_grid_width) {
        // This can happen if screen resulution is really huge
        maxXglobal = square_width * square_grid_width - 1;
    }
    if (maxYglobal >= square_height * square_grid_height) {
        // This can happen if screen resulution is really huge
        maxYglobal = square_height * square_grid_height - 1;
    }

    if (minXglobal % square_width != 0) {
        minXglobal = minXglobal - (minXglobal % square_width);
    };
    if (minYglobal % square_height != 0) {
        minYglobal = minYglobal - (minYglobal % square_height);
    };
    maxXglobal++;
    if ((maxXglobal) % square_width != 0) {
        maxXglobal = maxXglobal - (maxXglobal % square_width) + square_width;
    };
    maxYglobal++;
    if ((maxYglobal) % square_height != 0) {
        maxYglobal = maxYglobal - (maxYglobal % square_height) + square_height;
    };

    int minSquareX = minXglobal / square_width;
    int minSquareY = minYglobal / square_height;
    int maxSquareX = maxXglobal / square_width;
    int maxSquareY = maxYglobal / square_height;
    for (int x = minSquareX; x <= maxSquareX; x++) {
        for (int y = minSquareY; y <= maxSquareY; y++) {
            if (!visible_squares[gElevation][x][y]) {
                int screenX = x * square_width + screen_diff.x;
                int screenY = y * square_height + screen_diff.y;

                Rect squareRect;
                squareRect.left = screenX;
                squareRect.top = screenY;
                squareRect.right = screenX + square_width;
                squareRect.bottom = screenY + square_height;

                Rect intersection;
                if (rectIntersection(rect, &squareRect, &intersection) == -1) {
                    continue;
                };

                bufferFill(buffer + windowWidth * intersection.top + intersection.left,
                    intersection.right - intersection.left + 1,
                    intersection.bottom - intersection.top + 1,
                    windowWidth,
                    0x0);
            }
        }
    }
}

void tile_hires_stencil_init()
{
    configGetBool(&gSfallConfig, SFALL_CONFIG_MAIN_KEY, SFALL_CONFIG_ENABLE_HIRES_STENCIL, &gIsTileHiresStencilEnabled);
    if (!gIsTileHiresStencilEnabled) {
        return;
    }

    debugPrint("tile_hires_stencil_init\n");
    clean_cache();
    tile_hires_stencil_on_center_tile_or_elevation_change();
    tileWindowRefresh();
}

} // namespace fallout