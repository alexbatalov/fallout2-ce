#ifndef FILE_FIND_H
#define FILE_FIND_H

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

#include "platform_compat.h"

namespace fallout {

// NOTE: This structure is significantly different from what was in the
// original code. Watcom provides opendir/readdir/closedir implementations,
// that use Win32 FindFirstFile/FindNextFile under the hood, which in turn
// is designed to deal with patterns.
//
// The first attempt was to use `dirent` implementation by Toni Ronkko
// (https://github.com/tronkko/dirent), however it appears to be incompatible
// with what is provided by Watcom. Toni's implementation adds `*` wildcard
// unconditionally implying `opendir` accepts directory name only, which I
// guess is fine when your goal is compliance with POSIX implementation.
// However in Watcom `opendir` can handle file patterns gracefully. The problem
// can be seen during game startup when cleaning MAPS directory using
// MAPS\*.SAV pattern. Toni's implementation tries to convert that to pattern
// for Win32 API, thus making it MAPS\*.SAV\*, which is obviously incorrect
// path/pattern for any implementation.
//
// Eventually I've decided to go with compiler-specific implementation, keeping
// original implementation for Watcom (not tested). I'm not sure it will work
// in other compilers, so for now just stick with the error.
typedef struct DirectoryFileFindData {
#if defined(_WIN32)
    HANDLE hFind;
    WIN32_FIND_DATAA ffd;
#else
    DIR* dir;
    struct dirent* entry;
    char pattern[COMPAT_MAX_FNAME];
#endif
} DirectoryFileFindData;

bool fileFindFirst(const char* path, DirectoryFileFindData* findData);
bool fileFindNext(DirectoryFileFindData* findData);
bool findFindClose(DirectoryFileFindData* findData);

static inline bool fileFindIsDirectory(DirectoryFileFindData* findData)
{
#if defined(_WIN32)
    return (findData->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#elif defined(__WATCOMC__)
    return (findData->entry->d_attr & _A_SUBDIR) != 0;
#else
    return findData->entry->d_type == DT_DIR;
#endif
}

static inline char* fileFindGetName(DirectoryFileFindData* findData)
{
#if defined(_WIN32)
    return findData->ffd.cFileName;
#else
    return findData->entry->d_name;
#endif
}

} // namespace fallout

#endif /* FILE_FIND_H */
