#include "string_parsers.h"

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "platform_compat.h"

namespace fallout {

// strParseInt
// 0x4AFD10
int strParseInt(char** stringPtr, int* valuePtr)
{
    char *str, *remaining_str;
    size_t v1, v2, v3;
    char tmp;

    if (*stringPtr == nullptr) {
        return 0;
    }

    str = *stringPtr;

    compat_strlwr(str);

    v1 = strspn(str, " ");
    str += v1;

    v2 = strcspn(str, ",");
    v3 = v1 + v2;

    remaining_str = *stringPtr + v3;
    *stringPtr = remaining_str;

    if (*remaining_str != '\0') {
        *stringPtr = remaining_str + 1;
    }

    if (v2 != 0) {
        tmp = *(str + v2);
        *(str + v2) = '\0';
    }

    *valuePtr = atoi(str);

    if (v2 != 0) {
        *(str + v2) = tmp;
    }

    return 0;
}

// strParseStrFromList
// 0x4AFE08
int strParseStrFromList(char** stringPtr, int* valuePtr, const char** stringList, int stringListLength)
{
    int i;
    char *str, *remaining_str;
    size_t v1, v2, v3;
    char tmp;

    if (*stringPtr == nullptr) {
        return 0;
    }

    str = *stringPtr;

    compat_strlwr(str);

    v1 = strspn(str, " ");
    str += v1;

    v2 = strcspn(str, ",");
    v3 = v1 + v2;

    remaining_str = *stringPtr + v3;
    *stringPtr = remaining_str;

    if (*remaining_str != '\0') {
        *stringPtr = remaining_str + 1;
    }

    if (v2 != 0) {
        tmp = *(str + v2);
        *(str + v2) = '\0';
    }

    for (i = 0; i < stringListLength; i++) {
        if (compat_stricmp(str, stringList[i]) == 0) {
            break;
        }
    }

    if (v2 != 0) {
        *(str + v2) = tmp;
    }

    if (i == stringListLength) {
        debugPrint("\nstrParseStrFromList Error: Couldn't find match for string: %s!", str);
        *valuePtr = -1;
        return -1;
    }

    *valuePtr = i;

    return 0;
}

// strParseStrFromFunc
// 0x4AFEDC
int strParseStrFromFunc(char** stringPtr, int* valuePtr, StringParserCallback* callback)
{
    char *str, *remaining_str;
    size_t v1, v2, v3;
    char tmp;
    int result;

    if (*stringPtr == nullptr) {
        return 0;
    }

    str = *stringPtr;

    compat_strlwr(str);

    v1 = strspn(str, " ");
    str += v1;

    v2 = strcspn(str, ",");
    v3 = v1 + v2;

    remaining_str = *stringPtr + v3;
    *stringPtr = remaining_str;

    if (*remaining_str != '\0') {
        *stringPtr = remaining_str + 1;
    }

    if (v2 != 0) {
        tmp = *(str + v2);
        *(str + v2) = '\0';
    }

    result = callback(str, valuePtr);

    if (v2 != 0) {
        *(str + v2) = tmp;
    }

    if (result != 0) {
        debugPrint("\nstrParseStrFromFunc Error: Couldn't find match for string: %s!", str);
        *valuePtr = -1;
        return -1;
    }

    return 0;
}

// 0x4AFF7C
int strParseIntWithKey(char** stringPtr, const char* key, int* valuePtr, const char* delimeter)
{
    char* str;
    size_t v1, v2, v3, v4, v5;
    char tmp1, tmp2;
    int result;

    result = -1;

    if (*stringPtr == nullptr) {
        return 0;
    }

    str = *stringPtr;

    if (*str == '\0') {
        return -1;
    }

    compat_strlwr(str);

    if (*str == ',') {
        str++;
        *stringPtr = *stringPtr + 1;
    }

    v1 = strspn(str, " ");
    str += v1;

    v2 = strcspn(str, ",");
    v3 = v1 + v2;

    tmp1 = *(str + v2);
    *(str + v2) = '\0';

    v4 = strcspn(str, delimeter);
    v5 = v1 + v4;

    tmp2 = *(str + v4);
    *(str + v4) = '\0';

    if (strcmp(str, key) == 0) {
        *stringPtr = *stringPtr + v3;
        *valuePtr = atoi(str + v4 + 1);
        result = 0;
    }

    *(str + v4) = tmp2;
    *(str + v2) = tmp1;

    if (**stringPtr == ',') {
        *stringPtr = *stringPtr + 1;
    }

    return result;
}

// 0x4B005C
int strParseKeyValue(char** stringPtr, char* key, int* valuePtr, const char* delimiter)
{
    char* str;
    size_t v1, v2, v3, v4, v5;
    char tmp1, tmp2;

    if (*stringPtr == nullptr) {
        return 0;
    }

    str = *stringPtr;

    if (*str == '\0') {
        return -1;
    }

    compat_strlwr(str);

    if (*str == ',') {
        str++;
        *stringPtr = *stringPtr + 1;
    }

    v1 = strspn(str, " ");
    str += v1;

    v2 = strcspn(str, ",");
    v3 = v1 + v2;

    tmp1 = *(str + v2);
    *(str + v2) = '\0';

    v4 = strcspn(str, delimiter);
    v5 = v1 + v4;

    tmp2 = *(str + v4);
    *(str + v4) = '\0';

    strcpy(key, str);

    *stringPtr = *stringPtr + v3;
    *valuePtr = atoi(str + v4 + 1);

    *(str + v4) = tmp2;
    *(str + v2) = tmp1;

    return 0;
}

} // namespace fallout
