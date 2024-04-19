#ifndef DB_H
#define DB_H

#include <stddef.h>

#include "xfile.h"

namespace fallout {

typedef XFile File;
typedef void FileReadProgressHandler();
typedef char* StrdupProc(const char* string);

int dbOpen(const char* filePath1, int a2, const char* filePath2, int a4);
int db_total();
void dbExit();
int dbGetFileSize(const char* filePath, int* sizePtr);
int dbGetFileContents(const char* filePath, void* ptr);
int fileClose(File* stream);
File* fileOpen(const char* filename, const char* mode);
int filePrintFormatted(File* stream, const char* format, ...);
int fileReadChar(File* stream);
char* fileReadString(char* str, size_t size, File* stream);
int fileWriteString(const char* s, File* stream);
size_t fileRead(void* buf, size_t size, size_t count, File* stream);
size_t fileWrite(const void* buf, size_t size, size_t count, File* stream);
int fileSeek(File* stream, long offset, int origin);
long fileTell(File* stream);
void fileRewind(File* stream);
int fileEof(File* stream);
int fileReadUInt8(File* stream, unsigned char* valuePtr);
int fileReadInt16(File* stream, short* valuePtr);
int fileReadUInt16(File* stream, unsigned short* valuePtr);
int fileReadInt32(File* stream, int* valuePtr);
int fileReadUInt32(File* stream, unsigned int* valuePtr);
int _db_freadInt(File* stream, int* valuePtr);
int fileReadFloat(File* stream, float* valuePtr);
int fileReadBool(File* stream, bool* valuePtr);
int fileWriteUInt8(File* stream, unsigned char value);
int fileWriteInt16(File* stream, short value);
int fileWriteUInt16(File* stream, unsigned short value);
int fileWriteInt32(File* stream, int value);
int _db_fwriteLong(File* stream, int value);
int fileWriteUInt32(File* stream, unsigned int value);
int fileWriteFloat(File* stream, float value);
int fileWriteBool(File* stream, bool value);
int fileReadUInt8List(File* stream, unsigned char* arr, int count);
int fileReadFixedLengthString(File* stream, char* string, int length);
int fileReadInt16List(File* stream, short* arr, int count);
int fileReadUInt16List(File* stream, unsigned short* arr, int count);
int fileReadInt32List(File* stream, int* arr, int count);
int _db_freadIntCount(File* stream, int* arr, int count);
int fileReadUInt32List(File* stream, unsigned int* arr, int count);
int fileWriteUInt8List(File* stream, unsigned char* arr, int count);
int fileWriteFixedLengthString(File* stream, char* string, int length);
int fileWriteInt16List(File* stream, short* arr, int count);
int fileWriteUInt16List(File* stream, unsigned short* arr, int count);
int fileWriteInt32List(File* stream, int* arr, int count);
int _db_fwriteLongCount(File* stream, int* arr, int count);
int fileWriteUInt32List(File* stream, unsigned int* arr, int count);
int fileNameListInit(const char* pattern, char*** fileNames, int a3, int a4);
void fileNameListFree(char*** fileNames, int a2);
int fileGetSize(File* stream);
void fileSetReadProgressHandler(FileReadProgressHandler* handler, int size);

} // namespace fallout

#endif /* DB_H */
