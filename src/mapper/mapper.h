#ifndef FALLOUT_MAPPER_MAPPER_H_
#define FALLOUT_MAPPER_MAPPER_H_

#include "map.h"
#include "obj_types.h"

namespace fallout {

extern MapTransition mapInfo;

extern int tool_win;

int mapper_inven_unwield(Object* obj, int right_hand);

} // namespace fallout

#endif /* FALLOUT_MAPPER_MAPPER_H_ */
