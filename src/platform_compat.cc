#include "platform_compat.h"

#include <SDL.h>
#include <string.h>

#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
#include <timeapi.h>
#else
#include <sys/time.h>
#endif

int compat_stricmp(const char* string1, const char* string2)
{
    return SDL_strcasecmp(string1, string2);
}

int compat_strnicmp(const char* string1, const char* string2, size_t size)
{
    return SDL_strncasecmp(string1, string2, size);
}

char* compat_strupr(char* string)
{
    return SDL_strupr(string);
}

char* compat_strlwr(char* string)
{
    return SDL_strlwr(string);
}

char* compat_itoa(int value, char* buffer, int radix)
{
    return SDL_itoa(value, buffer, radix);
}

void compat_splitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
#if defined(_WIN32)
    _splitpath(path, drive, dir, fname, ext);
#else
    std::filesystem::path p(path);
    
    if (drive != NULL) {
        strcpy(drive, p.root_name().string().substr(0, COMPAT_MAX_DRIVE - 1).c_str());
    }

    if (dir != NULL) {
        strcpy(dir, p.parent_path().string().substr(0, COMPAT_MAX_DIR - 1).c_str());
    }

    if (fname != NULL) {
        strcpy(fname, p.stem().string().substr(0, COMPAT_MAX_FNAME - 1).c_str());
    }

    if (ext != NULL) {
        strcpy(ext, p.extension().string().substr(0, COMPAT_MAX_EXT - 1).c_str());
    }
#endif
}

void compat_makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext)
{
#if defined(_WIN32)
    _makepath(path, drive, dir, fname, ext);
#else
    std::filesystem::path p;

    if (drive != NULL) {
        p.append(drive);
    }

    if (dir != NULL) {
        p.append(dir);
    }

    if (fname != NULL) {
        p.append(fname);
    }

    if (ext != NULL) {
        p.replace_extension(ext);
    }

    strcpy(path, p.string().substr(0, COMPAT_MAX_PATH - 1).c_str());
#endif
}

long compat_filelength(int fd)
{
    long originalOffset = lseek(fd, 0, SEEK_CUR);
    lseek(fd, 0, SEEK_SET);
    long filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, originalOffset, SEEK_SET);
    return filesize;
}

int compat_mkdir(const char* path)
{
    std::error_code ec;
    if (std::filesystem::create_directory(std::filesystem::path(path), ec)) {
        return 0;
    }

    return ec.value();
}

unsigned int compat_timeGetTime()
{
#ifdef _WIN32
    return timeGetTime();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec / 1000;
#endif
}
