#ifndef FALLOUT_MAPPER_MAPPER_H_
#define FALLOUT_MAPPER_MAPPER_H_

#include "map.h"
#include "obj_types.h"

namespace fallout {

extern MapTransition mapInfo;

extern int menu_val_0[8];
extern int menu_val_2[8];
extern int menu_val_1[21];
extern unsigned char* tool;
extern int tool_win;

int mapper_main(int argc, char** argv);
void print_toolbar_name(int object_type);
int mapper_inven_unwield(Object* obj, int right_hand);

} // namespace fallout

#endif /* FALLOUT_MAPPER_MAPPER_H_ */
