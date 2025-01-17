#include "file_find.h"

#include <stddef.h>
#include <string.h>

#include <fpattern/fpattern.h>

namespace fallout {

// 0x4E6380
bool fileFindFirst(const char* path, DirectoryFileFindData* findData)
{
#if defined(_WIN32)
    findData->hFind = FindFirstFileA(path, &(findData->ffd));
    if (findData->hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
#else
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    char fname[COMPAT_MAX_FNAME];
    char ext[COMPAT_MAX_EXT];
    compat_splitpath(path, drive, dir, fname, ext);

    // Reassemble file name and extension to serve as a pattern. It has to be
    // lowercased because underlying `fpattern` implementation uses lowercased
    // char-by-char matching.
    compat_makepath(findData->pattern, nullptr, nullptr, fname, ext);
    compat_strlwr(findData->pattern);

    char basePath[COMPAT_MAX_PATH];
    compat_makepath(basePath, drive, dir, nullptr, nullptr);
    compat_resolve_path(basePath);

    findData->dir = opendir(basePath);
    if (findData->dir == nullptr) {
        return false;
    }

    findData->entry = readdir(findData->dir);
    while (findData->entry != nullptr) {
        char entryName[COMPAT_MAX_FNAME];
        strcpy(entryName, fileFindGetName(findData));
        compat_strlwr(entryName);
        if (fpattern_match(findData->pattern, entryName)) {
            break;
        }
        findData->entry = readdir(findData->dir);
    }

    if (findData->entry == nullptr) {
        closedir(findData->dir);
        findData->dir = nullptr;
        return false;
    }
#endif

    return true;
}

// 0x4E63A8
bool fileFindNext(DirectoryFileFindData* findData)
{
#if defined(_WIN32)
    if (!FindNextFileA(findData->hFind, &(findData->ffd))) {
        return false;
    }
#else
    findData->entry = readdir(findData->dir);
    while (findData->entry != nullptr) {
        char entryName[COMPAT_MAX_FNAME];
        strcpy(entryName, fileFindGetName(findData));
        compat_strlwr(entryName);
        if (fpattern_match(findData->pattern, entryName)) {
            break;
        }
        findData->entry = readdir(findData->dir);
    }

    if (findData->entry == nullptr) {
        closedir(findData->dir);
        findData->dir = nullptr;
        return false;
    }
#endif

    return true;
}

// 0x4E63CC
bool findFindClose(DirectoryFileFindData* findData)
{
#if defined(_MSC_VER)
    FindClose(findData->hFind);
#else
    if (findData->dir != nullptr) {
        if (closedir(findData->dir) != 0) {
            return false;
        }
    }
#endif

    return true;
}

} // namespace fallout
