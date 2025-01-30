#ifndef TILE_HIRES_STENCIL_H
#define TILE_HIRES_STENCIL_H

#include "geometry.h"

namespace fallout {

void tile_hires_stencil_init();

void tile_hires_stencil_on_center_tile_or_elevation_change();

void tile_hires_stencil_draw(Rect* rect, unsigned char* buffer, int windowWidth, int windowHeight);

} // namespace fallout

#endif