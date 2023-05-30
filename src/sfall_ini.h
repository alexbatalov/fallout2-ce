#ifndef FALLOUT_SFALL_INI_H_
#define FALLOUT_SFALL_INI_H_

#include <cstddef>

namespace fallout {

/// Sets base directory to lookup .ini files.
void sfall_ini_set_base_path(const char* path);

/// Reads integer key identified by "fileName|section|key" triplet into `value`.
bool sfall_ini_get_int(const char* triplet, int* value);

/// Reads string key identified by "fileName|section|key" triplet into `value`.
bool sfall_ini_get_string(const char* triplet, char* value, size_t size);

} // namespace fallout

#endif /* FALLOUT_SFALL_INI_H_ */
