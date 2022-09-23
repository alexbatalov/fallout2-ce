#include "version.h"

#include <stdio.h>

namespace fallout {

// 0x4B4580
void versionGetVersion(char* dest)
{
    sprintf(dest, "FALLOUT II %d.%02d", VERSION_MAJOR, VERSION_MINOR);
}

} // namespace fallout
