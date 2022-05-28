#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

// TODO: This is compatibility cross-platform layer. Designed to have minimal
// impact on the codebase. Remove once it's no longer needed.

int compat_stricmp(const char* string1, const char* string2);
int compat_strnicmp(const char* string1, const char* string2, size_t size);
char* compat_strupr(char* string);
char* compat_strlwr(char* string);
char* compat_itoa(int value, char* buffer, int radix);

#endif /* PLATFORM_COMPAT_H */
