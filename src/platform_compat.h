#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#include <stddef.h>
#include <stdio.h>

#include <zlib.h>

// Provides DbgPrint on NXDK; no-op on all other platforms.
#include "xbox_debug.h"

namespace fallout {
#ifdef NXDK
#ifndef _O_RDONLY
#define _O_RDONLY 0
#endif

#ifndef _O_BINARY
#define _O_BINARY 0
#endif
#endif
// TODO: This is compatibility cross-platform layer. Designed to have minimal
// impact on the codebase. Remove once it's no longer needed.

// A naive cross-platform MAX_PATH/PATH_MAX/MAX_PATH drop-in replacement.
//
// TODO: Remove when we migrate to use std::filesystem::path or std::string to
// represent paths across the codebase.
#define COMPAT_MAX_PATH 260

#define COMPAT_MAX_DRIVE 3
#define COMPAT_MAX_DIR 256
#define COMPAT_MAX_FNAME 256
#define COMPAT_MAX_EXT 256

int compat_stricmp(const char* string1, const char* string2);
int compat_strnicmp(const char* string1, const char* string2, size_t size);
char* compat_strupr(char* string);
char* compat_strlwr(char* string);
char* compat_itoa(int value, char* buffer, int radix);
void compat_splitpath(const char* path, char* drive, char* dir, char* fname, char* ext);
void compat_makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext);
int compat_open(const char* filePath, int flags);
int compat_close(int fileHandle);
int compat_read(int fileHandle, void* buf, unsigned int size);
int compat_write(int fileHandle, const void* buf, unsigned int size);
long compat_lseek(int fileHandle, long offset, int origin);
long compat_tell(int fileHandle);
long compat_filelength(int fd);
int compat_mkdir(const char* path);
unsigned int compat_timeGetTime();
FILE* compat_fopen(const char* path, const char* mode);
#ifndef Z_SOLO
gzFile compat_gzopen(const char* path, const char* mode);
#endif
char* compat_fgets(char* buffer, int maxCount, FILE* stream);
#ifndef Z_SOLO
char* compat_gzgets(gzFile stream, char* buffer, int maxCount);
#endif
int compat_remove(const char* path);
int compat_rename(const char* oldFileName, const char* newFileName);
void compat_windows_path_to_native(char* path);
void compat_resolve_path(char* path);
int compat_access(const char* path, int mode);
char* compat_strdup(const char* string);
long getFileSize(FILE* stream);

} // namespace fallout

#endif /* PLATFORM_COMPAT_H */
