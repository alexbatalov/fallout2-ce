#ifndef VERSION_H
#define VERSION_H

#include <stddef.h>

namespace fallout {

// The size of buffer for version string.
#define VERSION_MAX (32)

#define VERSION_MAJOR (1)
#define VERSION_MINOR (2)
#define VERSION_RELEASE ('R')

void versionGetVersion(char* dest, size_t size);

} // namespace fallout

#endif /* VERSION_H */
