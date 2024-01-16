#include "db.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "platform_compat.h"
#include "xfile.h"

namespace fallout {

typedef struct FileList {
    XList xlist;
    struct FileList* next;
} FileList;

static int _db_list_compare(const void* p1, const void* p2);

// Generic file progress report handler.
//
// 0x51DEEC
static FileReadProgressHandler* gFileReadProgressHandler = nullptr;

// Bytes read so far while tracking progress.
//
// Once this value reaches [gFileReadProgressChunkSize] the handler is called
// and this value resets to zero.
//
// 0x51DEF0
static int gFileReadProgressBytesRead = 0;

// The number of bytes to read between calls to progress handler.
//
// 0x673040
static int gFileReadProgressChunkSize;

// 0x673044
static FileList* gFileListHead;

// Opens file database.
//
// Returns -1 if [filePath1] was specified, but could not be opened by the
// underlying xbase implementation. Result of opening [filePath2] is ignored.
// Returns 0 on success.
//
// NOTE: There are two unknown parameters passed via edx and ecx. The [a2] is
// always 0 at the calling sites, and [a4] is always 1. Both parameters are not
// used, so it's impossible to figure out their meaning.
//
// 0x4C5D30
int dbOpen(const char* filePath1, int a2, const char* filePath2, int a4)
{
    if (filePath1 != nullptr) {
        if (!xbaseOpen(filePath1)) {
            return -1;
        }
    }

    if (filePath2 != nullptr) {
        xbaseOpen(filePath2);
    }

    return 0;
}

// 0x4C5D58
int _db_total()
{
    return 0;
}

// 0x4C5D60
void dbExit()
{
    xbaseReopenAll(nullptr);
}

// TODO: sizePtr should be long*.
//
// 0x4C5D68
int dbGetFileSize(const char* filePath, int* sizePtr)
{
    assert(filePath); // "filename", "db.c", 108
    assert(sizePtr); // "de", "db.c", 109

    File* stream = xfileOpen(filePath, "rb");
    if (stream == nullptr) {
        return -1;
    }

    *sizePtr = xfileGetSize(stream);

    xfileClose(stream);

    return 0;
}

// 0x4C5DD4
int dbGetFileContents(const char* filePath, void* ptr)
{
    assert(filePath); // "filename", "db.c", 141
    assert(ptr); // "buf", "db.c", 142

    File* stream = xfileOpen(filePath, "rb");
    if (stream == nullptr) {
        return -1;
    }

    long size = xfileGetSize(stream);
    if (gFileReadProgressHandler != nullptr) {
        unsigned char* byteBuffer = (unsigned char*)ptr;

        long remainingSize = size;
        long chunkSize = gFileReadProgressChunkSize - gFileReadProgressBytesRead;

        while (remainingSize >= chunkSize) {
            size_t bytesRead = xfileRead(byteBuffer, sizeof(*byteBuffer), chunkSize, stream);
            byteBuffer += bytesRead;
            remainingSize -= bytesRead;

            gFileReadProgressBytesRead = 0;
            gFileReadProgressHandler();

            chunkSize = gFileReadProgressChunkSize;
        }

        if (remainingSize != 0) {
            gFileReadProgressBytesRead += xfileRead(byteBuffer, sizeof(*byteBuffer), remainingSize, stream);
        }
    } else {
        xfileRead(ptr, 1, size, stream);
    }

    xfileClose(stream);

    return 0;
}

// 0x4C5EB4
int fileClose(File* stream)
{
    return xfileClose(stream);
}

// 0x4C5EC8
File* fileOpen(const char* filename, const char* mode)
{
    return xfileOpen(filename, mode);
}

// 0x4C5ED0
int filePrintFormatted(File* stream, const char* format, ...)
{
    assert(format); // "format", "db.c", 224

    va_list args;
    va_start(args, format);

    int rc = xfilePrintFormattedArgs(stream, format, args);

    va_end(args);

    return rc;
}

// 0x4C5F24
int fileReadChar(File* stream)
{
    if (gFileReadProgressHandler != nullptr) {
        int ch = xfileReadChar(stream);

        gFileReadProgressBytesRead++;
        if (gFileReadProgressBytesRead >= gFileReadProgressChunkSize) {
            gFileReadProgressHandler();
            gFileReadProgressBytesRead = 0;
        }

        return ch;
    }

    return xfileReadChar(stream);
}

// 0x4C5F70
char* fileReadString(char* string, size_t size, File* stream)
{
    if (gFileReadProgressHandler != nullptr) {
        if (xfileReadString(string, size, stream) == nullptr) {
            return nullptr;
        }

        gFileReadProgressBytesRead += strlen(string);
        while (gFileReadProgressBytesRead >= gFileReadProgressChunkSize) {
            gFileReadProgressHandler();
            gFileReadProgressBytesRead -= gFileReadProgressChunkSize;
        }

        return string;
    }

    return xfileReadString(string, size, stream);
}

// 0x4C5FEC
int fileWriteString(const char* string, File* stream)
{
    return xfileWriteString(string, stream);
}

// 0x4C5FFC
size_t fileRead(void* ptr, size_t size, size_t count, File* stream)
{
    if (gFileReadProgressHandler != nullptr) {
        unsigned char* byteBuffer = (unsigned char*)ptr;

        size_t totalBytesRead = 0;
        long remainingSize = size * count;
        long chunkSize = gFileReadProgressChunkSize - gFileReadProgressBytesRead;

        while (remainingSize >= chunkSize) {
            size_t bytesRead = xfileRead(byteBuffer, sizeof(*byteBuffer), chunkSize, stream);
            byteBuffer += bytesRead;
            totalBytesRead += bytesRead;
            remainingSize -= bytesRead;

            gFileReadProgressBytesRead = 0;
            gFileReadProgressHandler();

            chunkSize = gFileReadProgressChunkSize;
        }

        if (remainingSize != 0) {
            size_t bytesRead = xfileRead(byteBuffer, sizeof(*byteBuffer), remainingSize, stream);
            gFileReadProgressBytesRead += bytesRead;
            totalBytesRead += bytesRead;
        }

        return totalBytesRead / size;
    }

    return xfileRead(ptr, size, count, stream);
}

// 0x4C60B8
size_t fileWrite(const void* buf, size_t size, size_t count, File* stream)
{
    return xfileWrite(buf, size, count, stream);
}

// 0x4C60C0
int fileSeek(File* stream, long offset, int origin)
{
    return xfileSeek(stream, offset, origin);
}

// 0x4C60C8
long fileTell(File* stream)
{
    return xfileTell(stream);
}

// 0x4C60D0
void fileRewind(File* stream)
{
    xfileRewind(stream);
}

// 0x4C60D8
int fileEof(File* stream)
{
    return xfileEof(stream);
}

// NOTE: Not sure about signness.
//
// 0x4C60E0
int fileReadUInt8(File* stream, unsigned char* valuePtr)
{
    int value = fileReadChar(stream);
    if (value == -1) {
        return -1;
    }

    *valuePtr = value & 0xFF;

    return 0;
}

// NOTE: Not sure about signness.
//
// 0x4C60F4
int fileReadInt16(File* stream, short* valuePtr)
{
    unsigned char high;
    // NOTE: Uninline.
    if (fileReadUInt8(stream, &high) == -1) {
        return -1;
    }

    unsigned char low;
    // NOTE: Uninline.
    if (fileReadUInt8(stream, &low) == -1) {
        return -1;
    }

    *valuePtr = (high << 8) | low;

    return 0;
}

// NOTE: Probably uncollapsed 0x4C60F4. There are only couple of places where
// the game reads/writes 16-bit integers. I'm not sure there are unsigned
// shorts used, but there are definitely signed (art offsets can be both
// positive and negative). Provided just in case.
int fileReadUInt16(File* stream, unsigned short* valuePtr)
{
    return fileReadInt16(stream, (short*)valuePtr);
}

// 0x4C614C
int fileReadInt32(File* stream, int* valuePtr)
{
    int value;

    if (xfileRead(&value, 4, 1, stream) == -1) {
        return -1;
    }

    *valuePtr = ((value & 0xFF000000) >> 24) | ((value & 0xFF0000) >> 8) | ((value & 0xFF00) << 8) | ((value & 0xFF) << 24);

    return 0;
}

// NOTE: Uncollapsed 0x4C614C. The opposite of [_db_fwriteLong]. It can be either
// signed vs. unsigned variant, as well as int vs. long. It's provided here to
// identify places where data was written with [_db_fwriteLong].
int _db_freadInt(File* stream, int* valuePtr)
{
    return fileReadInt32(stream, valuePtr);
}

// NOTE: Probably uncollapsed 0x4C614C.
int fileReadUInt32(File* stream, unsigned int* valuePtr)
{
    return _db_freadInt(stream, (int*)valuePtr);
}

// NOTE: Uncollapsed 0x4C614C. The opposite of [fileWriteFloat].
int fileReadFloat(File* stream, float* valuePtr)
{
    return fileReadInt32(stream, (int*)valuePtr);
}

int fileReadBool(File* stream, bool* valuePtr)
{
    int value;
    if (fileReadInt32(stream, &value) == -1) {
        return -1;
    }

    *valuePtr = (value != 0);

    return 0;
}

// NOTE: Not sure about signness.
//
// 0x4C61AC
int fileWriteUInt8(File* stream, unsigned char value)
{
    return xfileWriteChar(value, stream);
};

// 0x4C61C8
int fileWriteInt16(File* stream, short value)
{
    // NOTE: Uninline.
    if (fileWriteUInt8(stream, (value >> 8) & 0xFF) == -1) {
        return -1;
    }

    // NOTE: Uninline.
    if (fileWriteUInt8(stream, value & 0xFF) == -1) {
        return -1;
    }

    return 0;
}

// NOTE: Probably uncollapsed 0x4C61C8.
int fileWriteUInt16(File* stream, unsigned short value)
{
    return fileWriteInt16(stream, (short)value);
}

// NOTE: Not sure about signness and int vs. long.
//
// 0x4C6214
int fileWriteInt32(File* stream, int value)
{
    // NOTE: Uninline.
    return _db_fwriteLong(stream, value);
}

// NOTE: Can either be signed vs. unsigned variant of [fileWriteInt32],
// or int vs. long.
//
// 0x4C6244
int _db_fwriteLong(File* stream, int value)
{
    if (fileWriteInt16(stream, (value >> 16) & 0xFFFF) == -1) {
        return -1;
    }

    if (fileWriteInt16(stream, value & 0xFFFF) == -1) {
        return -1;
    }

    return 0;
}

// NOTE: Probably uncollapsed 0x4C6214 or 0x4C6244.
int fileWriteUInt32(File* stream, unsigned int value)
{
    return _db_fwriteLong(stream, (int)value);
}

// 0x4C62C4
int fileWriteFloat(File* stream, float value)
{
    // NOTE: Uninline.
    return _db_fwriteLong(stream, *(int*)&value);
}

int fileWriteBool(File* stream, bool value)
{
    return _db_fwriteLong(stream, value ? 1 : 0);
}

// 0x4C62FC
int fileReadUInt8List(File* stream, unsigned char* arr, int count)
{
    for (int index = 0; index < count; index++) {
        unsigned char ch;
        // NOTE: Uninline.
        if (fileReadUInt8(stream, &ch) == -1) {
            return -1;
        }

        arr[index] = ch;
    }

    return 0;
}

// NOTE: Probably uncollapsed 0x4C62FC. There are couple of places where
// [fileReadUInt8List] is used to read strings of fixed length. I'm not
// pretty sure this function existed in the original code, but at least
// it increases visibility of these places.
int fileReadFixedLengthString(File* stream, char* string, int length)
{
    return fileReadUInt8List(stream, (unsigned char*)string, length);
}

// 0x4C6330
int fileReadInt16List(File* stream, short* arr, int count)
{
    for (int index = 0; index < count; index++) {
        short value;
        // NOTE: Uninline.
        if (fileReadInt16(stream, &value) == -1) {
            return -1;
        }

        arr[index] = value;
    }

    return 0;
}

// NOTE: Probably uncollapsed 0x4C6330.
int fileReadUInt16List(File* stream, unsigned short* arr, int count)
{
    return fileReadInt16List(stream, (short*)arr, count);
}

// NOTE: Not sure about signed/unsigned int/long.
//
// 0x4C63BC
int fileReadInt32List(File* stream, int* arr, int count)
{
    if (count == 0) {
        return 0;
    }

    if (fileRead(arr, sizeof(*arr) * count, 1, stream) < 1) {
        return -1;
    }

    for (int index = 0; index < count; index++) {
        int value = arr[index];
        arr[index] = ((value & 0xFF000000) >> 24) | ((value & 0xFF0000) >> 8) | ((value & 0xFF00) << 8) | ((value & 0xFF) << 24);
    }

    return 0;
}

// NOTE: Uncollapsed 0x4C63BC. The opposite of [_db_fwriteLongCount].
int _db_freadIntCount(File* stream, int* arr, int count)
{
    return fileReadInt32List(stream, arr, count);
}

// NOTE: Probably uncollapsed 0x4C63BC.
int fileReadUInt32List(File* stream, unsigned int* arr, int count)
{
    return fileReadInt32List(stream, (int*)arr, count);
}

// 0x4C6464
int fileWriteUInt8List(File* stream, unsigned char* arr, int count)
{
    for (int index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (fileWriteUInt8(stream, arr[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

// NOTE: Probably uncollapsed 0x4C6464. See [fileReadFixedLengthString].
int fileWriteFixedLengthString(File* stream, char* string, int length)
{
    return fileWriteUInt8List(stream, (unsigned char*)string, length);
}

// 0x4C6490
int fileWriteInt16List(File* stream, short* arr, int count)
{
    for (int index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (fileWriteInt16(stream, arr[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

// NOTE: Probably uncollapsed 0x4C6490.
int fileWriteUInt16List(File* stream, unsigned short* arr, int count)
{
    return fileWriteInt16List(stream, (short*)arr, count);
}

// NOTE: Can be either signed/unsigned + int/long variant.
//
// 0x4C64F8
int fileWriteInt32List(File* stream, int* arr, int count)
{
    for (int index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (_db_fwriteLong(stream, arr[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

// NOTE: Not sure about signed/unsigned int/long.
//
// 0x4C6550
int _db_fwriteLongCount(File* stream, int* arr, int count)
{
    for (int index = 0; index < count; index++) {
        int value = arr[index];

        // NOTE: Uninline.
        if (fileWriteInt16(stream, (value >> 16) & 0xFFFF) == -1) {
            return -1;
        }

        // NOTE: Uninline.
        if (fileWriteInt16(stream, value & 0xFFFF) == -1) {
            return -1;
        }
    }

    return 0;
}

// NOTE: Probably uncollapsed 0x4C64F8 or 0x4C6550.
int fileWriteUInt32List(File* stream, unsigned int* arr, int count)
{
    return fileWriteInt32List(stream, (int*)arr, count);
}

// 0x4C6628
int fileNameListInit(const char* pattern, char*** fileNameListPtr, int a3, int a4)
{
    FileList* fileList = (FileList*)malloc(sizeof(*fileList));
    if (fileList == nullptr) {
        return 0;
    }

    memset(fileList, 0, sizeof(*fileList));

    XList* xlist = &(fileList->xlist);
    if (!xlistInit(pattern, xlist)) {
        free(fileList);
        return 0;
    }

    int length = 0;
    if (xlist->fileNamesLength != 0) {
        qsort(xlist->fileNames, xlist->fileNamesLength, sizeof(*xlist->fileNames), _db_list_compare);

        int fileNamesLength = xlist->fileNamesLength;
        for (int index = 0; index < fileNamesLength - 1; index++) {
            if (compat_stricmp(xlist->fileNames[index], xlist->fileNames[index + 1]) == 0) {
                char* temp = xlist->fileNames[index + 1];
                memmove(&(xlist->fileNames[index + 1]), &(xlist->fileNames[index + 2]), sizeof(*xlist->fileNames) * (xlist->fileNamesLength - index - 1));
                xlist->fileNames[xlist->fileNamesLength - 1] = temp;

                fileNamesLength--;
                index--;
            }
        }

        bool isWildcard = *pattern == '*';

        for (int index = 0; index < fileNamesLength; index += 1) {
            char* name = xlist->fileNames[index];
            char dir[COMPAT_MAX_DIR];
            char fileName[COMPAT_MAX_FNAME];
            char extension[COMPAT_MAX_EXT];
            compat_windows_path_to_native(name);
            compat_splitpath(name, nullptr, dir, fileName, extension);

            if (!isWildcard || *dir == '\0' || (strchr(dir, '\\') == nullptr && strchr(dir, '/') == nullptr)) {
                // NOTE: Quick and dirty fix to buffer overflow. See RE to
                // understand the problem.
                char path[COMPAT_MAX_PATH];
                snprintf(path, sizeof(path), "%s%s", fileName, extension);
                free(xlist->fileNames[length]);
                xlist->fileNames[length] = compat_strdup(path);
                length++;
            }
        }
    }

    fileList->next = gFileListHead;
    gFileListHead = fileList;

    *fileNameListPtr = xlist->fileNames;

    return length;
}

// 0x4C6868
void fileNameListFree(char*** fileNameListPtr, int a2)
{
    if (gFileListHead == nullptr) {
        return;
    }

    FileList* currentFileList = gFileListHead;
    FileList* previousFileList = gFileListHead;
    while (*fileNameListPtr != currentFileList->xlist.fileNames) {
        previousFileList = currentFileList;
        currentFileList = currentFileList->next;
        if (currentFileList == nullptr) {
            return;
        }
    }

    if (previousFileList == gFileListHead) {
        gFileListHead = currentFileList->next;
    } else {
        previousFileList->next = currentFileList->next;
    }

    xlistFree(&(currentFileList->xlist));

    free(currentFileList);
}

// TODO: Return type should be long.
//
// 0x4C68BC
int fileGetSize(File* stream)
{
    return xfileGetSize(stream);
}

// 0x4C68C4
void fileSetReadProgressHandler(FileReadProgressHandler* handler, int size)
{
    if (handler != nullptr && size != 0) {
        gFileReadProgressHandler = handler;
        gFileReadProgressChunkSize = size;
    } else {
        gFileReadProgressHandler = nullptr;
        gFileReadProgressChunkSize = 0;
    }
}

// 0x4C68E8
int _db_list_compare(const void* p1, const void* p2)
{
    return compat_stricmp(*(const char**)p1, *(const char**)p2);
}

} // namespace fallout
