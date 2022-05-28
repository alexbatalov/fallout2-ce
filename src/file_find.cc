#include "file_find.h"

#include <stddef.h>

// 0x4E6380
bool fileFindFirst(const char* path, DirectoryFileFindData* findData)
{
#if defined(_WIN32)
    findData->hFind = FindFirstFileA(path, &(findData->ffd));
    if (findData->hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
#else
    findData->dir = opendir(path);
    if (findData->dir == NULL) {
        return false;
    }

    findData->entry = readdir(findData->dir);
    if (findData->entry == NULL) {
        closedir(findData->dir);
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
    if (findData->entry == NULL) {
        closedir(findData->dir);
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
    if (closedir(findData->dir) != 0) {
        return false;
    }
#endif

    return true;
}
