#include "file_find.h"

#include <stddef.h>
#include <string.h>

#include <fpattern.h>

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
    strcpy(findData->path, path);

    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(path, drive, dir, nullptr, nullptr);

    char basePath[COMPAT_MAX_PATH];
    compat_makepath(basePath, drive, dir, nullptr, nullptr);

    findData->dir = opendir(basePath);
    if (findData->dir == nullptr) {
        return false;
    }

    findData->entry = readdir(findData->dir);
    while (findData->entry != nullptr) {
        char entryPath[COMPAT_MAX_PATH];
        compat_makepath(entryPath, drive, dir, fileFindGetName(findData), nullptr);
        if (fpattern_match(findData->path, entryPath)) {
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
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(findData->path, drive, dir, nullptr, nullptr);

    findData->entry = readdir(findData->dir);
    while (findData->entry != nullptr) {
        char entryPath[COMPAT_MAX_PATH];
        compat_makepath(entryPath, drive, dir, fileFindGetName(findData), nullptr);
        if (fpattern_match(findData->path, entryPath)) {
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
