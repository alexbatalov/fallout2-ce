#ifndef FALLOUT_SFALL_GLOBAL_SCRIPTS_H_
#define FALLOUT_SFALL_GLOBAL_SCRIPTS_H_

#include "interpreter.h"

namespace fallout {

bool sfall_gl_scr_init();
void sfall_gl_scr_reset();
void sfall_gl_scr_exit();
void sfall_gl_scr_exec_start_proc();
void sfall_gl_scr_remove_all();
void sfall_gl_scr_exec_map_update_scripts(int action);
void sfall_gl_scr_process_main();
void sfall_gl_scr_process_input();
void sfall_gl_scr_process_worldmap();
void sfall_gl_scr_set_repeat(Program* program, int frames);
void sfall_gl_scr_set_type(Program* program, int type);
bool sfall_gl_scr_is_loaded(Program* program);
void sfall_gl_scr_update(int burstSize);

} // namespace fallout

#endif /* FALLOUT_SFALL_GLOBAL_SCRIPTS_H_ */
