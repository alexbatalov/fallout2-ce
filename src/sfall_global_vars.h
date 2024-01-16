#ifndef FALLOUT_SFALL_GLOBAL_VARS_H_
#define FALLOUT_SFALL_GLOBAL_VARS_H_

#include "db.h"

namespace fallout {

bool sfall_gl_vars_init();
void sfall_gl_vars_reset();
void sfall_gl_vars_exit();
bool sfall_gl_vars_save(File* stream);
bool sfall_gl_vars_load(File* stream);
bool sfall_gl_vars_store(const char* key, int value);
bool sfall_gl_vars_store(int key, int value);
bool sfall_gl_vars_fetch(const char* key, int& value);
bool sfall_gl_vars_fetch(int key, int& value);

} // namespace fallout

#endif /* FALLOUT_SFALL_GLOBAL_VARS_H_ */
