#ifndef CONFIG_H
#define CONFIG_H

#include "dictionary.h"

#define CONFIG_FILE_MAX_LINE_LENGTH (256)

// The initial number of sections (or key-value) pairs in the config.
#define CONFIG_INITIAL_CAPACITY (10)

// A representation of .INI file.
//
// It's implemented as a [Dictionary] whos keys are section names of .INI file,
// and it's values are [ConfigSection] structs.
typedef Dictionary Config;

// Representation of .INI section.
//
// It's implemented as a [Dictionary] whos keys are names of .INI file
// key-pair values, and it's values are pointers to strings (char**).
typedef Dictionary ConfigSection;

extern char gConfigLastSectionKey[CONFIG_FILE_MAX_LINE_LENGTH];

bool configInit(Config* config);
void configFree(Config* config);
bool configParseCommandLineArguments(Config* config, int argc, char** argv);
bool configGetString(Config* config, const char* sectionKey, const char* key, char** valuePtr);
bool configSetString(Config* config, const char* sectionKey, const char* key, const char* value);
bool configGetInt(Config* config, const char* sectionKey, const char* key, int* valuePtr);
bool configGetIntList(Config* config, const char* section, const char* key, int* arr, int count);
bool configSetInt(Config* config, const char* sectionKey, const char* key, int value);
bool configRead(Config* config, const char* filePath, bool isDb);
bool configWrite(Config* config, const char* filePath, bool isDb);
bool configParseLine(Config* config, char* string);
bool configParseKeyValue(char* string, char* key, char* value);
bool configEnsureSectionExists(Config* config, const char* sectionKey);
bool configTrimString(char* string);
bool configGetDouble(Config* config, const char* sectionKey, const char* key, double* valuePtr);
bool configSetDouble(Config* config, const char* sectionKey, const char* key, double value);

bool configGetBool(Config* config, const char* sectionKey, const char* key, bool* valuePtr);
bool configSetBool(Config* config, const char* sectionKey, const char* key, bool value);

#endif /* CONFIG_H */
