#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include "platform_compat.h"

// I'll just use this for random Switch utility functions.

namespace fallout {

bool fileExists(const char* path, const char* filename) {
    char fullPath[COMPAT_MAX_PATH];
    snprintf(fullPath, sizeof(fullPath), "%s%s", path, filename);

    FILE* file = fopen(fullPath, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}
} // namespace fallout

#endif // UTILS_H
