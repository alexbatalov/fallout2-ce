#include "file_find.h"

// 0x4E6380
bool fileFindFirst(const char* path, DirectoryFileFindData* findData)
{
#if defined(_MSC_VER)
    findData->hFind = FindFirstFileA(path, &(findData->ffd));
    if (findData->hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
#elif defined(__WATCOMC__)
    findData->dir = opendir(path);
    if (findData->dir == NULL) {
        return false;
    }

    findData->entry = readdir(findData->dir);
    if (findData->entry == NULL) {
        closedir(findData->dir);
        return false;
    }
#else
#error Not implemented
#endif

    return true;
}

// 0x4E63A8
bool fileFindNext(DirectoryFileFindData* findData)
{
#if defined(_MSC_VER)
    if (!FindNextFileA(findData->hFind, &(findData->ffd))) {
        return false;
    }
#elif defined(__WATCOMC__)
    findData->entry = readdir(findData->dir);
    if (findData->entry == NULL) {
        closedir(findData->dir);
        return false;
    }
#else
#error Not implemented
#endif

    return true;
}

// 0x4E63CC
bool findFindClose(DirectoryFileFindData* findData)
{
#if defined(_MSC_VER)
    FindClose(findData->hFind);
#elif defined(__WATCOMC__)
    if (closedir(findData->dir) != 0) {
        return false;
    }
#else
#error Not implemented
#endif

    return true;
}
