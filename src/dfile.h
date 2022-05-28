#ifndef DFILE_H
#define DFILE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <zlib.h>

// The size of decompression buffer for reading compressed [DFile]s.
#define DFILE_DECOMPRESSION_BUFFER_SIZE (0x400)

// Specifies that [DFile] has unget character.
//
// NOTE: There is an unused function at 0x4E5894 which ungets one character and
// stores it in [ungotten]. Since that function is not used, this flag will
// never be set.
#define DFILE_HAS_UNGETC (0x01)

// Specifies that [DFile] has reached end of stream.
#define DFILE_EOF (0x02)

// Specifies that [DFile] is in error state.
//
// [dfileRewind] can be used to clear this flag.
#define DFILE_ERROR (0x04)

// Specifies that [DFile] was opened in text mode.
#define DFILE_TEXT (0x08)

// Specifies that [DFile] has unget compressed character.
#define DFILE_HAS_COMPRESSED_UNGETC (0x10)

typedef struct DBase DBase;
typedef struct DBaseEntry DBaseEntry;
typedef struct DFile DFile;

// A representation of .DAT file.
typedef struct DBase {
    // The path of .DAT file that this structure represents.
    char* path;

    // The offset to the beginning of data section of .DAT file.
    int dataOffset;

    // The number of entries.
    int entriesLength;

    // The array of entries.
    DBaseEntry* entries;

    // The head of linked list of open file handles.
    DFile* dfileHead;
} DBase;

typedef struct DBaseEntry {
    char* path;
    unsigned char compressed;
    int uncompressedSize;
    int dataSize;
    int dataOffset;
} DBaseEntry;

// A handle to open entry in .DAT file.
typedef struct DFile {
    DBase* dbase;
    DBaseEntry* entry;
    int flags;

    // The stream of .DAT file opened for reading in binary mode.
    //
    // This stream is not shared across open handles. Instead every [DFile]
    // opens it's own stream via [fopen], which is then closed via [fclose] in
    // [dfileClose].
    FILE* stream;

    // The inflate stream used to decompress data.
    //
    // This value is NULL if entry is not compressed.
    z_streamp decompressionStream;

    // The decompression buffer of size [DFILE_DECOMPRESSION_BUFFER_SIZE].
    //
    // This value is NULL if entry is not compressed.
    unsigned char* decompressionBuffer;

    // The last ungot character.
    //
    // See [DFILE_HAS_UNGETC] notes.
    int ungotten;

    // The last ungot compressed character.
    //
    // This value is used when reading compressed text streams to detect
    // Windows end of line sequence \r\n.
    int compressedUngotten;

    // The number of bytes read so far from compressed stream.
    //
    // This value is only used when reading compressed streams. The range is
    // 0..entry->dataSize.
    int compressedBytesRead;

    // The position in read stream.
    //
    // This value is tracked in terms of uncompressed data (even in compressed
    // streams). The range is 0..entry->uncompressedSize.
    long position;

    // Next [DFile] in linked list.
    //
    // [DFile]s are stored in [DBase] in reverse order, so it's actually a
    // previous opened file, not next.
    DFile* next;
} DFile;

typedef struct DFileFindData {
    // The name of file that was found during previous search.
    char fileName[MAX_PATH];

    // The pattern to search.
    //
    // This value is set automatically when [dbaseFindFirstEntry] succeeds so
    // that subsequent calls to [dbaseFindNextEntry] know what to look for.
    char pattern[MAX_PATH];

    // The index of entry that was found during previous search.
    //
    // This value is set automatically when [dbaseFindFirstEntry] and
    // [dbaseFindNextEntry] succeed so that subsequent calls to [dbaseFindNextEntry]
    // knows where to start search from.
    int index;
} DFileFindData;

DBase* dbaseOpen(const char* filename);
bool dbaseClose(DBase* dbase);
bool dbaseFindFirstEntry(DBase* dbase, DFileFindData* findFileData, const char* pattern);
bool dbaseFindNextEntry(DBase* dbase, DFileFindData* findFileData);
bool dbaseFindClose(DBase* dbase, DFileFindData* findFileData);
long dfileGetSize(DFile* stream);
int dfileClose(DFile* stream);
DFile* dfileOpen(DBase* dbase, const char* filename, const char* mode);
int dfilePrintFormattedArgs(DFile* stream, const char* format, va_list args);
int dfileReadChar(DFile* stream);
char* dfileReadString(char* str, int size, DFile* stream);
int dfileWriteChar(int ch, DFile* stream);
int dfileWriteString(const char* s, DFile* stream);
size_t dfileRead(void* ptr, size_t size, size_t count, DFile* stream);
size_t dfileWrite(const void* ptr, size_t size, size_t count, DFile* stream);
int dfileSeek(DFile* stream, long offset, int origin);
long dfileTell(DFile* stream);
void dfileRewind(DFile* stream);
int dfileEof(DFile* stream);
int dbaseFindEntryByFilePath(const void* a1, const void* a2);
DFile* dfileOpenInternal(DBase* dbase, const char* filename, const char* mode, DFile* a4);
int dfileReadCharInternal(DFile* stream);
bool dfileReadCompressed(DFile* stream, void* ptr, size_t size);
void dfileUngetCompressed(DFile* stream, int ch);

#endif /* DFILE_H */
