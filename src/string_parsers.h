#ifndef STRING_PARSERS_H
#define STRING_PARSERS_H

namespace fallout {

typedef int(StringParserCallback)(char* string, int* valuePtr);

int strParseInt(char** stringPtr, int* valuePtr);
int strParseStrFromList(char** stringPtr, int* valuePtr, const char** list, int count);
int strParseStrFromFunc(char** stringPtr, int* valuePtr, StringParserCallback* callback);
int strParseIntWithKey(char** stringPtr, const char* key, int* valuePtr, const char* delimeter);
int strParseKeyValue(char** stringPtr, char* key, int* valuePtr, const char* delimeter);

} // namespace fallout

#endif /* STRING_PARSERS_H */
