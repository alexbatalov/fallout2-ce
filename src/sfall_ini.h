#ifndef FALLOUT_SFALL_INI_H_
#define FALLOUT_SFALL_INI_H_

#include "config.h"
#include "dictionary.h"
#include "interpreter.h"
#include <cstddef>

namespace fallout {

/// Sets base directory to lookup .ini files.
void sfall_ini_set_base_path(const char* path);

/// Reads integer key identified by "fileName|section|key" triplet into `value`.
bool sfall_ini_get_int(const char* triplet, int* value);

/// Reads string key identified by "fileName|section|key" triplet into `value`.
bool sfall_ini_get_string(const char* triplet, char* value, size_t size);

/// Writes integer key identified by "fileName|section|key" triplet.
bool sfall_ini_set_int(const char* triplet, int value);

/// Writes string key identified by "fileName|section|key" triplet.
bool sfall_ini_set_string(const char* triplet, const char* value);

// metarule and opcode implementations
void mf_set_ini_setting(Program* program, int args);
void mf_get_ini_section(Program* program, int args);
void mf_get_ini_sections(Program* program, int args);
void op_get_ini_setting(Program* program);
void op_get_ini_string(Program* program);

} // namespace fallout

#endif /* FALLOUT_SFALL_INI_H_ */
