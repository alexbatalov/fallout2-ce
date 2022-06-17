#include "file_find.h"
#ifndef _WIN32
#include <regex>
#endif

#include <stddef.h>

#ifndef _WIN32
bool findFileRegex(DirectoryFileFindData* findData) {
    bool findingNextFile = (findData->filename[0] != '\0');

    std::string filenameRegex = findData->searchPath.filename().string();

    auto wildcardPosition = filenameRegex.find("*");
    if (wildcardPosition != std::string::npos) {
        filenameRegex = filenameRegex.insert(wildcardPosition, ".");
    }

    try {
        const std::filesystem::directory_iterator end;
        for (std::filesystem::directory_iterator iter{findData->searchPath.parent_path()}; iter != end; iter++) {

            std::string currentFilename = iter->path().filename().string();

            if (findingNextFile) {
                if (currentFilename == findData->filename) {
                    findingNextFile = false;
                }
                continue;
            }

            if (std::regex_match(currentFilename, std::regex(filenameRegex))) {

                memset(findData->filename, 0, sizeof findData->filename);
                currentFilename.copy(findData->filename, currentFilename.length());
                findData->filename[currentFilename.length()] = '\0';
                return true;
            }
        }
    }
    catch (std::exception&) {}

    memset(findData->filename, 0, sizeof findData->filename);
    return false;
}
#endif

// 0x4E6380
bool fileFindFirst(const char* path, DirectoryFileFindData* findData)
{
#if defined(_WIN32)
    findData->hFind = FindFirstFileA(path, &(findData->ffd));
    if (findData->hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    return true;
#else
    memset(findData->filename, 0, sizeof findData->filename);
    findData->searchPath = compat_convertPathSeparators(path);
    return findFileRegex(findData);
#endif
}

// 0x4E63A8
bool fileFindNext(DirectoryFileFindData* findData)
{
#if defined(_WIN32)
    if (!FindNextFileA(findData->hFind, &(findData->ffd))) {
        return false;
    }
    return true;
#else
    return findFileRegex(findData);
#endif
}

// 0x4E63CC
bool findFindClose(DirectoryFileFindData* findData)
{
#if defined(_MSC_VER)
    FindClose(findData->hFind);
#endif

    return true;
}
