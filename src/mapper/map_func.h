#ifndef FALLOUT_MAPPER_MAP_FUNC_H_
#define FALLOUT_MAPPER_MAP_FUNC_H_

#include "geometry.h"

namespace fallout {

void setup_map_dirs();
void copy_proto_lists();
void sort_rect(Rect* a, Rect* b);
void draw_rect(Rect* rect);
void erase_rect(Rect* rect);
int toolbar_proto(int type, int id);
bool map_toggle_block_obj_viewing_on();

} // namespace fallout

#endif /* FALLOUT_MAPPER_MAP_FUNC_H_ */
