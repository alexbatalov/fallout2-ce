#include "config.h"

#include "db.h"
#include "memory.h"
#include "platform_compat.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Last section key read from .INI file.
//
// 0x518224
char gConfigLastSectionKey[CONFIG_FILE_MAX_LINE_LENGTH] = "unknown";

// 0x42BD90
bool configInit(Config* config)
{
    if (config == NULL) {
        return false;
    }

    if (dictionaryInit(config, CONFIG_INITIAL_CAPACITY, sizeof(ConfigSection), NULL) != 0) {
        return false;
    }

    return true;
}

// 0x42BDBC
void configFree(Config* config)
{
    if (config == NULL) {
        return;
    }

    for (int sectionIndex = 0; sectionIndex < config->entriesLength; sectionIndex++) {
        DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);

        ConfigSection* section = (ConfigSection*)sectionEntry->value;
        for (int keyValueIndex = 0; keyValueIndex < section->entriesLength; keyValueIndex++) {
            DictionaryEntry* keyValueEntry = &(section->entries[keyValueIndex]);

            char** value = (char**)keyValueEntry->value;
            internal_free(*value);
            *value = NULL;
        }

        dictionaryFree(section);
    }

    dictionaryFree(config);
}

// Parses command line argments and adds them into the config.
//
// The expected format of [argv] elements are "[section]key=value", otherwise
// the element is silently ignored.
//
// NOTE: This function trims whitespace in key-value pair, but not in section.
// I don't know if this is intentional or it's bug.
//
// 0x42BE38
bool configParseCommandLineArguments(Config* config, int argc, char** argv)
{
    if (config == NULL) {
        return false;
    }

    for (int arg = 0; arg < argc; arg++) {
        char* pch = argv[arg];

        // Find opening bracket.
        while (*pch != '\0' && *pch != '[') {
            pch++;
        }

        if (*pch == '\0') {
            continue;
        }

        char* sectionKey = pch + 1;

        // Find closing bracket.
        while (*pch != '\0' && *pch != ']') {
            pch++;
        }

        if (*pch == '\0') {
            continue;
        }

        *pch = '\0';

        char key[260];
        char value[260];
        if (configParseKeyValue(pch + 1, key, value)) {
            if (!configSetString(config, sectionKey, key, value)) {
                *pch = ']';
                return false;
            }
        }

        *pch = ']';
    }

    return true;
}

// 0x42BF48
bool configGetString(Config* config, const char* sectionKey, const char* key, char** valuePtr)
{
    if (config == NULL || sectionKey == NULL || key == NULL || valuePtr == NULL) {
        return false;
    }

    int sectionIndex = dictionaryGetIndexByKey(config, sectionKey);
    if (sectionIndex == -1) {
        return false;
    }

    DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);
    ConfigSection* section = (ConfigSection*)sectionEntry->value;

    int index = dictionaryGetIndexByKey(section, key);
    if (index == -1) {
        return false;
    }

    DictionaryEntry* keyValueEntry = &(section->entries[index]);
    *valuePtr = *(char**)keyValueEntry->value;

    return true;
}

// 0x42BF90
bool configSetString(Config* config, const char* sectionKey, const char* key, const char* value)
{
    if (config == NULL || sectionKey == NULL || key == NULL || value == NULL) {
        return false;
    }

    int sectionIndex = dictionaryGetIndexByKey(config, sectionKey);
    if (sectionIndex == -1) {
        // FIXME: Looks like a bug, this function never returns -1, which will
        // eventually lead to crash.
        if (configEnsureSectionExists(config, sectionKey) == -1) {
            return false;
        }
        sectionIndex = dictionaryGetIndexByKey(config, sectionKey);
    }

    DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);
    ConfigSection* section = (ConfigSection*)sectionEntry->value;

    int index = dictionaryGetIndexByKey(section, key);
    if (index != -1) {
        DictionaryEntry* keyValueEntry = &(section->entries[index]);

        char** existingValue = (char**)keyValueEntry->value;
        internal_free(*existingValue);
        *existingValue = NULL;

        dictionaryRemoveValue(section, key);
    }

    char* valueCopy = internal_strdup(value);
    if (valueCopy == NULL) {
        return false;
    }

    if (dictionaryAddValue(section, key, &valueCopy) == -1) {
        internal_free(valueCopy);
        return false;
    }

    return true;
}

// 0x42C05C customized: atoi() replaced with strtol()
bool configGetInt(Config* config, const char* sectionKey, const char* key, int* valuePtr, unsigned char base /* = 0 */ )
{
    if (valuePtr == NULL) {
        return false;
    }

    char* stringValue;
    if (!configGetString(config, sectionKey, key, &stringValue)) {
        return false;
    }

    char* end;
    errno = 0;
    long l = strtol(stringValue, &end, base); // see https://stackoverflow.com/a/6154614
    if (((errno == ERANGE && 1 == LONG_MAX) || l > INT_MAX) || ((errno == ERANGE && l == LONG_MIN) || l < INT_MIN) || (*stringValue == '\0' || *end != '\0'))
        return false;

    *valuePtr = l;

    return true;
}

// 0x42C090
bool configGetIntList(Config* config, const char* sectionKey, const char* key, int* arr, int count)
{
    if (arr == NULL || count < 2) {
        return false;
    }

    char* string;
    if (!configGetString(config, sectionKey, key, &string)) {
        return false;
    }

    char temp[CONFIG_FILE_MAX_LINE_LENGTH];
    strncpy(temp, string, CONFIG_FILE_MAX_LINE_LENGTH - 1);

    char* beginning = temp;
    char* pch = beginning;
    while (*pch != '\0') {
        if (*pch == ',') {
            *pch = '\0';

            *arr++ = atoi(beginning);

            *pch = ',';

            pch++;
            beginning = pch;

            count--;

            if (count < 0) {
                break;
            }
        }

        pch++;
    }

    if (count <= 1) {
        *arr = atoi(beginning);
    }

    return true;
}

// 0x42C160
bool configSetInt(Config* config, const char* sectionKey, const char* key, int value)
{
    char stringValue[20];
    compat_itoa(value, stringValue, 10);

    return configSetString(config, sectionKey, key, stringValue);
}

// Reads .INI file into config.
//
// 0x42C280
bool configRead(Config* config, const char* filePath, bool isDb)
{
    if (config == NULL || filePath == NULL) {
        return false;
    }

    char string[CONFIG_FILE_MAX_LINE_LENGTH];

    if (isDb) {
        File* stream = fileOpen(filePath, "rb");
        if (stream != NULL) {
            while (fileReadString(string, sizeof(string), stream) != NULL) {
                configParseLine(config, string);
            }
            fileClose(stream);
        }
    } else {
        FILE* stream = fopen(filePath, "rt");
        if (stream != NULL) {
            while (fgets(string, sizeof(string), stream) != NULL) {
                configParseLine(config, string);
            }

            fclose(stream);
        }

        // FIXME: This function returns `true` even if the file was not actually
        // read. I'm pretty sure it's bug.
    }

    return true;
}

// Writes config into .INI file.
//
// 0x42C324
bool configWrite(Config* config, const char* filePath, bool isDb)
{
    if (config == NULL || filePath == NULL) {
        return false;
    }

    if (isDb) {
        File* stream = fileOpen(filePath, "wt");
        if (stream == NULL) {
            return false;
        }

        for (int sectionIndex = 0; sectionIndex < config->entriesLength; sectionIndex++) {
            DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);
            filePrintFormatted(stream, "[%s]\n", sectionEntry->key);

            ConfigSection* section = (ConfigSection*)sectionEntry->value;
            for (int index = 0; index < section->entriesLength; index++) {
                DictionaryEntry* keyValueEntry = &(section->entries[index]);
                filePrintFormatted(stream, "%s=%s\n", keyValueEntry->key, *(char**)keyValueEntry->value);
            }

            filePrintFormatted(stream, "\n");
        }

        fileClose(stream);
    } else {
        FILE* stream = fopen(filePath, "wt");
        if (stream == NULL) {
            return false;
        }

        for (int sectionIndex = 0; sectionIndex < config->entriesLength; sectionIndex++) {
            DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);
            fprintf(stream, "[%s]\n", sectionEntry->key);

            ConfigSection* section = (ConfigSection*)sectionEntry->value;
            for (int index = 0; index < section->entriesLength; index++) {
                DictionaryEntry* keyValueEntry = &(section->entries[index]);
                fprintf(stream, "%s=%s\n", keyValueEntry->key, *(char**)keyValueEntry->value);
            }

            fprintf(stream, "\n");
        }

        fclose(stream);
    }

    return true;
}

// Parses a line from .INI file into config.
//
// A line either contains a "[section]" section key or "key=value" pair. In the
// first case section key is not added to config immediately, instead it is
// stored in [gConfigLastSectionKey] for later usage. This prevents empty
// sections in the config.
//
// In case of key-value pair it pretty straight forward - it adds key-value
// pair into previously read section key stored in [gConfigLastSectionKey].
//
// Returns `true` when a section was parsed or key-value pair was parsed and
// added to the config, or `false` otherwise.
//
// 0x42C4BC
bool configParseLine(Config* config, char* string)
{
    char* pch;

    // Find comment marker and truncate the string.
    pch = string;
    while (*pch != '\0' && *pch != ';') {
        pch++;
    }

    if (*pch != '\0') {
        *pch = '\0';
    }

    // Find opening bracket.
    pch = string;
    while (*pch != '\0' && *pch != '[') {
        pch++;
    }

    if (*pch == '[') {
        char* sectionKey = pch + 1;

        // Find closing bracket.
        while (*pch != '\0' && *pch != ']') {
            pch++;
        }

        if (*pch == ']') {
            *pch = '\0';
            strcpy(gConfigLastSectionKey, sectionKey);
            return configTrimString(gConfigLastSectionKey);
        }
    }

    char key[260];
    char value[260];
    if (!configParseKeyValue(string, key, value)) {
        return false;
    }

    return configSetString(config, gConfigLastSectionKey, key, value);
}

// Splits "key=value" pair from [string] and copy appropriate parts into [key]
// and [value] respectively.
//
// Both key and value are trimmed.
//
// 0x42C594
bool configParseKeyValue(char* string, char* key, char* value)
{
    if (string == NULL || key == NULL || value == NULL) {
        return false;
    }

    // Find equals character.
    char* pch = string;
    while (*pch != '\0' && *pch != '=') {
        pch++;
    }

    if (*pch == '\0') {
        return false;
    }

    *pch = '\0';

    strcpy(key, string);
    strcpy(value, pch + 1);

    *pch = '=';

    configTrimString(key);
    configTrimString(value);

    return true;
}

// Ensures the config has a section with specified key.
//
// Return `true` if section exists or it was successfully added, or `false`
// otherwise.
//
// 0x42C638
bool configEnsureSectionExists(Config* config, const char* sectionKey)
{
    if (config == NULL || sectionKey == NULL) {
        return false;
    }

    if (dictionaryGetIndexByKey(config, sectionKey) != -1) {
        // Section already exists, no need to do anything.
        return true;
    }

    ConfigSection section;
    if (dictionaryInit(&section, CONFIG_INITIAL_CAPACITY, sizeof(char**), NULL) == -1) {
        return false;
    }

    if (dictionaryAddValue(config, sectionKey, &section) == -1) {
        return false;
    }

    return true;
}

// Removes leading and trailing whitespace from the specified string.
//
// 0x42C698
bool configTrimString(char* string)
{
    if (string == NULL) {
        return false;
    }

    int length = strlen(string);
    if (length == 0) {
        return true;
    }

    // Starting from the end of the string, loop while it's a whitespace and
    // decrement string length.
    char* pch = string + length - 1;
    while (length != 0 && isspace(*pch)) {
        length--;
        pch--;
    }

    // pch now points to the last non-whitespace character.
    pch[1] = '\0';

    // Starting from the beginning of the string loop while it's a whitespace
    // and decrement string length.
    pch = string;
    while (isspace(*pch)) {
        pch++;
        length--;
    }

    // pch now points for to the first non-whitespace character.
    memmove(string, pch, length + 1);

    return true;
}

// 0x42C718
bool configGetDouble(Config* config, const char* sectionKey, const char* key, double* valuePtr)
{
    if (valuePtr == NULL) {
        return false;
    }

    char* stringValue;
    if (!configGetString(config, sectionKey, key, &stringValue)) {
        return false;
    }

    *valuePtr = strtod(stringValue, NULL);

    return true;
}

// 0x42C74C
bool configSetDouble(Config* config, const char* sectionKey, const char* key, double value)
{
    char stringValue[32];
    sprintf(stringValue, "%.6f", value);

    return configSetString(config, sectionKey, key, stringValue);
}

// NOTE: Boolean-typed variant of [configGetInt].
bool configGetBool(Config* config, const char* sectionKey, const char* key, bool* valuePtr)
{
    if (valuePtr == NULL) {
        return false;
    }

    int integerValue;
    if (!configGetInt(config, sectionKey, key, &integerValue)) {
        return false;
    }

    *valuePtr = integerValue != 0;

    return true;
}

// NOTE: Boolean-typed variant of [configGetInt].
bool configSetBool(Config* config, const char* sectionKey, const char* key, bool value)
{
    return configSetInt(config, sectionKey, key, value ? 1 : 0);
}
