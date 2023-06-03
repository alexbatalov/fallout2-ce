#include "dfile.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include <fpattern.h>

#include "platform_compat.h"

namespace fallout {

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

static int dbaseFindEntryByFilePath(const void* a1, const void* a2);
static DFile* dfileOpenInternal(DBase* dbase, const char* filename, const char* mode, DFile* a4);
static int dfileReadCharInternal(DFile* stream);
static bool dfileReadCompressed(DFile* stream, void* ptr, size_t size);
static void dfileUngetCompressed(DFile* stream, int ch);

static void dfile_normalize_path(char* path);

// Reads .DAT file contents.
//
// 0x4E4F58
DBase* dbaseOpen(const char* filePath)
{
    assert(filePath); // "filename", "dfile.c", 74

    FILE* stream = compat_fopen(filePath, "rb");
    if (stream == NULL) {
        return NULL;
    }

    DBase* dbase = (DBase*)malloc(sizeof(*dbase));
    if (dbase == NULL) {
        fclose(stream);
        return NULL;
    }

    memset(dbase, 0, sizeof(*dbase));

    // Get file size, and reposition stream to read footer, which contains two
    // 32-bits ints.
    int fileSize = getFileSize(stream);
    if (fseek(stream, fileSize - sizeof(int) * 2, SEEK_SET) != 0) {
        goto err;
    }

    // Read the size of entries table.
    int entriesDataSize;
    if (fread(&entriesDataSize, sizeof(entriesDataSize), 1, stream) != 1) {
        goto err;
    }

    // Read the size of entire dbase content.
    //
    // NOTE: It appears that this approach allows existence of arbitrary data in
    // the beginning of the .DAT file.
    int dbaseDataSize;
    if (fread(&dbaseDataSize, sizeof(dbaseDataSize), 1, stream) != 1) {
        goto err;
    }

    // Reposition stream to the beginning of the entries table.
    if (fseek(stream, fileSize - entriesDataSize - sizeof(int) * 2, SEEK_SET) != 0) {
        goto err;
    }

    if (fread(&(dbase->entriesLength), sizeof(dbase->entriesLength), 1, stream) != 1) {
        goto err;
    }

    dbase->entries = (DBaseEntry*)malloc(sizeof(*dbase->entries) * dbase->entriesLength);
    if (dbase->entries == NULL) {
        goto err;
    }

    memset(dbase->entries, 0, sizeof(*dbase->entries) * dbase->entriesLength);

    // Read entries one by one, stopping on any error.
    int entryIndex;
    for (entryIndex = 0; entryIndex < dbase->entriesLength; entryIndex++) {
        DBaseEntry* entry = &(dbase->entries[entryIndex]);

        int pathLength;
        if (fread(&pathLength, sizeof(pathLength), 1, stream) != 1) {
            break;
        }

        entry->path = (char*)malloc(pathLength + 1);
        if (entry->path == NULL) {
            break;
        }

        if (fread(entry->path, pathLength, 1, stream) != 1) {
            break;
        }

        entry->path[pathLength] = '\0';

        // CE: Normalize entry path.
        dfile_normalize_path(entry->path);

        if (fread(&(entry->compressed), sizeof(entry->compressed), 1, stream) != 1) {
            break;
        }

        if (fread(&(entry->uncompressedSize), sizeof(entry->uncompressedSize), 1, stream) != 1) {
            break;
        }

        if (fread(&(entry->dataSize), sizeof(entry->dataSize), 1, stream) != 1) {
            break;
        }

        if (fread(&(entry->dataOffset), sizeof(entry->dataOffset), 1, stream) != 1) {
            break;
        }
    }

    if (entryIndex < dbase->entriesLength) {
        // We haven't reached the end, which means there was an error while
        // reading entries.
        goto err;
    }

    dbase->path = compat_strdup(filePath);
    dbase->dataOffset = fileSize - dbaseDataSize;

    fclose(stream);

    return dbase;

err:

    dbaseClose(dbase);

    fclose(stream);

    return NULL;
}

// Closes [dbase], all open file handles, frees all associated resources,
// including the [dbase] itself.
//
// 0x4E5270
bool dbaseClose(DBase* dbase)
{
    assert(dbase); // "dbase", "dfile.c", 173

    DFile* curr = dbase->dfileHead;
    while (curr != NULL) {
        DFile* next = curr->next;
        dfileClose(curr);
        curr = next;
    }

    if (dbase->entries != NULL) {
        for (int index = 0; index < dbase->entriesLength; index++) {
            DBaseEntry* entry = &(dbase->entries[index]);
            char* entryName = entry->path;
            if (entryName != NULL) {
                free(entryName);
            }
        }
        free(dbase->entries);
    }

    if (dbase->path != NULL) {
        free(dbase->path);
    }

    memset(dbase, 0, sizeof(*dbase));

    free(dbase);

    return true;
}

// 0x4E5308
bool dbaseFindFirstEntry(DBase* dbase, DFileFindData* findFileData, const char* pattern)
{
    // CE: Normalize pattern to match entries. Underlying `fpattern`
    // implementation is case-sensitive on non-Windows platforms, so both
    // pattern and entries should match in case and have native path separators.
    char normalizedPattern[COMPAT_MAX_PATH];
    strcpy(normalizedPattern, pattern);
    dfile_normalize_path(normalizedPattern);

    for (int index = 0; index < dbase->entriesLength; index++) {
        DBaseEntry* entry = &(dbase->entries[index]);
        if (fpattern_match(normalizedPattern, entry->path)) {
            strcpy(findFileData->fileName, entry->path);
            strcpy(findFileData->pattern, normalizedPattern);
            findFileData->index = index;
            return true;
        }
    }

    return false;
}

// 0x4E53A0
bool dbaseFindNextEntry(DBase* dbase, DFileFindData* findFileData)
{
    for (int index = findFileData->index + 1; index < dbase->entriesLength; index++) {
        DBaseEntry* entry = &(dbase->entries[index]);
        if (fpattern_match(findFileData->pattern, entry->path)) {
            strcpy(findFileData->fileName, entry->path);
            findFileData->index = index;
            return true;
        }
    }

    return false;
}

// 0x4E541C
bool dbaseFindClose(DBase* dbase, DFileFindData* findFileData)
{
    return true;
}

// [filelength].
//
// 0x4E5424
long dfileGetSize(DFile* stream)
{
    return stream->entry->uncompressedSize;
}

// [fclose].
//
// 0x4E542C
int dfileClose(DFile* stream)
{
    assert(stream); // "stream", "dfile.c", 253

    int rc = 0;

    if (stream->entry->compressed == 1) {
        if (inflateEnd(stream->decompressionStream) != Z_OK) {
            rc = -1;
        }
    }

    if (stream->decompressionStream != NULL) {
        free(stream->decompressionStream);
    }

    if (stream->decompressionBuffer != NULL) {
        free(stream->decompressionBuffer);
    }

    if (stream->stream != NULL) {
        fclose(stream->stream);
    }

    // Loop thru open file handles and find previous to remove current handle
    // from linked list.
    //
    // NOTE: Compiled code is slightly different.
    DFile* curr = stream->dbase->dfileHead;
    DFile* prev = NULL;
    while (curr != NULL) {
        if (curr == stream) {
            break;
        }

        prev = curr;
        curr = curr->next;
    }

    if (curr != NULL) {
        if (prev == NULL) {
            stream->dbase->dfileHead = stream->next;
        } else {
            prev->next = stream->next;
        }
    }

    memset(stream, 0, sizeof(*stream));

    free(stream);

    return rc;
}

// [fopen].
//
// 0x4E5504
DFile* dfileOpen(DBase* dbase, const char* filePath, const char* mode)
{
    assert(dbase); // dfile.c, 295
    assert(filePath); // dfile.c, 296
    assert(mode); // dfile.c, 297

    return dfileOpenInternal(dbase, filePath, mode, 0);
}

// [vfprintf].
//
// 0x4E56C0
int dfilePrintFormattedArgs(DFile* stream, const char* format, va_list args)
{
    assert(stream); // "stream", "dfile.c", 368
    assert(format); // "format", "dfile.c", 369

    return -1;
}

// [fgetc].
//
// This function reports \r\n sequence as one character \n, even though it
// consumes two characters from the underlying stream.
//
// 0x4E5700
int dfileReadChar(DFile* stream)
{
    assert(stream); // "stream", "dfile.c", 384

    if ((stream->flags & DFILE_EOF) != 0 || (stream->flags & DFILE_ERROR) != 0) {
        return -1;
    }

    if ((stream->flags & DFILE_HAS_UNGETC) != 0) {
        stream->flags &= ~DFILE_HAS_UNGETC;
        return stream->ungotten;
    }

    int ch = dfileReadCharInternal(stream);
    if (ch == -1) {
        stream->flags |= DFILE_EOF;
    }

    return ch;
}

// [fgets].
//
// Both Windows (\r\n) and Unix (\n) line endings are recognized. Windows
// line ending is reported as \n.
//
// 0x4E5764
char* dfileReadString(char* string, int size, DFile* stream)
{
    assert(string); // "s", "dfile.c", 407
    assert(size); // "n", "dfile.c", 408
    assert(stream); // "stream", "dfile.c", 409

    if ((stream->flags & DFILE_EOF) != 0 || (stream->flags & DFILE_ERROR) != 0) {
        return NULL;
    }

    char* pch = string;

    if ((stream->flags & DFILE_HAS_UNGETC) != 0) {
        *pch++ = stream->ungotten & 0xFF;
        size--;
        stream->flags &= ~DFILE_HAS_UNGETC;
    }

    // Read up to size - 1 characters one by one saving space for the null
    // terminator.
    for (int index = 0; index < size - 1; index++) {
        int ch = dfileReadCharInternal(stream);
        if (ch == -1) {
            break;
        }

        *pch++ = ch & 0xFF;

        if (ch == '\n') {
            break;
        }
    }

    if (pch == string) {
        // No character was set into the buffer.
        return NULL;
    }

    *pch = '\0';

    return string;
}

// [fputc].
//
// 0x4E5830
int dfileWriteChar(int ch, DFile* stream)
{
    assert(stream); // "stream", "dfile.c", 437

    return -1;
}

// [fputs].
//
// 0x4E5854
int dfileWriteString(const char* string, DFile* stream)
{
    assert(string); // "s", "dfile.c", 448
    assert(stream); // "stream", "dfile.c", 449

    return -1;
}

// [fread].
//
// 0x4E58FC
size_t dfileRead(void* ptr, size_t size, size_t count, DFile* stream)
{
    assert(ptr); // "ptr", "dfile.c", 499
    assert(stream); // "stream", dfile.c, 500

    if ((stream->flags & DFILE_EOF) != 0 || (stream->flags & DFILE_ERROR) != 0) {
        return 0;
    }

    size_t remainingSize = stream->entry->uncompressedSize - stream->position;
    if ((stream->flags & DFILE_HAS_UNGETC) != 0) {
        remainingSize++;
    }

    size_t bytesToRead = size * count;
    if (remainingSize < bytesToRead) {
        bytesToRead = remainingSize;
        stream->flags |= DFILE_EOF;
    }

    size_t extraBytesRead = 0;
    if ((stream->flags & DFILE_HAS_UNGETC) != 0) {
        unsigned char* byteBuffer = (unsigned char*)ptr;
        *byteBuffer++ = stream->ungotten & 0xFF;
        ptr = byteBuffer;

        bytesToRead--;

        stream->flags &= ~DFILE_HAS_UNGETC;
        extraBytesRead = 1;
    }

    size_t bytesRead;
    if (stream->entry->compressed == 1) {
        if (!dfileReadCompressed(stream, ptr, bytesToRead)) {
            stream->flags |= DFILE_ERROR;
            return false;
        }

        bytesRead = bytesToRead;
    } else {
        bytesRead = fread(ptr, 1, bytesToRead, stream->stream) + extraBytesRead;
        stream->position += bytesRead;
    }

    return bytesRead / size;
}

// [fwrite].
//
// 0x4E59F8
size_t dfileWrite(const void* ptr, size_t size, size_t count, DFile* stream)
{
    assert(ptr); // "ptr", "dfile.c", 538
    assert(stream); // "stream", "dfile.c", 539

    return count - 1;
}

// [fseek].
//
// 0x4E5A74
int dfileSeek(DFile* stream, long offset, int origin)
{
    assert(stream); // "stream", "dfile.c", 569

    if ((stream->flags & DFILE_ERROR) != 0) {
        return 1;
    }

    if ((stream->flags & DFILE_TEXT) != 0) {
        if (offset != 0 && origin != SEEK_SET) {
            // NOTE: For unknown reason this function does not allow arbitrary
            // seeks in text streams, whether compressed or not. It only
            // supports rewinding. Probably because of reading functions which
            // handle \r\n sequence as \n.
            return 1;
        }
    }

    long offsetFromBeginning;
    switch (origin) {
    case SEEK_SET:
        offsetFromBeginning = offset;
        break;
    case SEEK_CUR:
        offsetFromBeginning = stream->position + offset;
        break;
    case SEEK_END:
        offsetFromBeginning = stream->entry->uncompressedSize + offset;
        break;
    default:
        return 1;
    }

    if (offsetFromBeginning >= stream->entry->uncompressedSize) {
        return 1;
    }

    long pos = stream->position;
    if (offsetFromBeginning == pos) {
        stream->flags &= ~(DFILE_HAS_UNGETC | DFILE_EOF);
        return 0;
    }

    if (offsetFromBeginning != 0) {
        if (stream->entry->compressed == 1) {
            if (offsetFromBeginning < pos) {
                // We cannot go backwards in compressed stream, so the only way
                // is to start from the beginning.
                dfileRewind(stream);
            }

            // Consume characters one by one until we reach specified offset.
            while (offsetFromBeginning > stream->position) {
                if (dfileReadCharInternal(stream) == -1) {
                    return 1;
                }
            }
        } else {
            if (fseek(stream->stream, offsetFromBeginning - pos, SEEK_CUR) != 0) {
                stream->flags |= DFILE_ERROR;
                return 1;
            }

            // FIXME: I'm not sure what this assignment means. This field is
            // only meaningful when reading compressed streams.
            stream->compressedBytesRead = offsetFromBeginning;
        }

        stream->flags &= ~(DFILE_HAS_UNGETC | DFILE_EOF);
        return 0;
    }

    if (fseek(stream->stream, stream->dbase->dataOffset + stream->entry->dataOffset, SEEK_SET) != 0) {
        stream->flags |= DFILE_ERROR;
        return 1;
    }

    if (inflateEnd(stream->decompressionStream) != Z_OK) {
        stream->flags |= DFILE_ERROR;
        return 1;
    }

    stream->decompressionStream->zalloc = Z_NULL;
    stream->decompressionStream->zfree = Z_NULL;
    stream->decompressionStream->opaque = Z_NULL;
    stream->decompressionStream->next_in = stream->decompressionBuffer;
    stream->decompressionStream->avail_in = 0;

    if (inflateInit(stream->decompressionStream) != Z_OK) {
        stream->flags |= DFILE_ERROR;
        return 1;
    }

    stream->position = 0;
    stream->compressedBytesRead = 0;
    stream->flags &= ~(DFILE_HAS_UNGETC | DFILE_EOF);

    return 0;
}

// [ftell].
//
// 0x4E5C88
long dfileTell(DFile* stream)
{
    assert(stream); // "stream", "dfile.c", 654

    return stream->position;
}

// [rewind].
//
// 0x4E5CB0
void dfileRewind(DFile* stream)
{
    assert(stream); // "stream", "dfile.c", 664

    dfileSeek(stream, 0, SEEK_SET);

    stream->flags &= ~DFILE_ERROR;
}

// [feof].
//
// 0x4E5D10
int dfileEof(DFile* stream)
{
    assert(stream); // "stream", "dfile.c", 685

    return stream->flags & DFILE_EOF;
}

// The [bsearch] comparison callback, which is used to find [DBaseEntry] for
// specified [filePath].
//
// 0x4E5D70
static int dbaseFindEntryByFilePath(const void* a1, const void* a2)
{
    const char* filePath = (const char*)a1;
    DBaseEntry* entry = (DBaseEntry*)a2;

    return compat_stricmp(filePath, entry->path);
}

// 0x4E5D9C
static DFile* dfileOpenInternal(DBase* dbase, const char* filePath, const char* mode, DFile* dfile)
{
    // CE: Normalize path to match entries. Even though
    // `dbaseFindEntryByFilePath` uses case-insensitive compare, it still needs
    // native path separators.
    char normalizedFilePath[COMPAT_MAX_PATH];
    strcpy(normalizedFilePath, filePath);
    dfile_normalize_path(normalizedFilePath);

    DBaseEntry* entry = (DBaseEntry*)bsearch(normalizedFilePath, dbase->entries, dbase->entriesLength, sizeof(*dbase->entries), dbaseFindEntryByFilePath);
    if (entry == NULL) {
        goto err;
    }

    if (mode[0] != 'r') {
        goto err;
    }

    if (dfile == NULL) {
        dfile = (DFile*)malloc(sizeof(*dfile));
        if (dfile == NULL) {
            return NULL;
        }

        memset(dfile, 0, sizeof(*dfile));
        dfile->dbase = dbase;
        dfile->next = dbase->dfileHead;
        dbase->dfileHead = dfile;
    } else {
        if (dbase != dfile->dbase) {
            goto err;
        }

        if (dfile->stream != NULL) {
            fclose(dfile->stream);
            dfile->stream = NULL;
        }

        dfile->compressedBytesRead = 0;
        dfile->position = 0;
        dfile->flags = 0;
    }

    dfile->entry = entry;

    // Open stream to .DAT file.
    dfile->stream = compat_fopen(dbase->path, "rb");
    if (dfile->stream == NULL) {
        goto err;
    }

    // Relocate stream to the beginning of data for specified entry.
    if (fseek(dfile->stream, dbase->dataOffset + entry->dataOffset, SEEK_SET) != 0) {
        goto err;
    }

    if (entry->compressed == 1) {
        // Entry is compressed, setup decompression stream and decompression
        // buffer. This step is not needed when previous instance of dfile is
        // passed via parameter, which might already have stream and
        // buffer allocated.
        if (dfile->decompressionStream == NULL) {
            dfile->decompressionStream = (z_streamp)malloc(sizeof(*dfile->decompressionStream));
            if (dfile->decompressionStream == NULL) {
                goto err;
            }

            dfile->decompressionBuffer = (unsigned char*)malloc(DFILE_DECOMPRESSION_BUFFER_SIZE);
            if (dfile->decompressionBuffer == NULL) {
                goto err;
            }
        }

        dfile->decompressionStream->zalloc = Z_NULL;
        dfile->decompressionStream->zfree = Z_NULL;
        dfile->decompressionStream->opaque = Z_NULL;
        dfile->decompressionStream->next_in = dfile->decompressionBuffer;
        dfile->decompressionStream->avail_in = 0;

        if (inflateInit(dfile->decompressionStream) != Z_OK) {
            goto err;
        }
    } else {
        // Entry is not compressed, there is no need to keep decompression
        // stream and decompression buffer (in case [dfile] was passed via
        // parameter).
        if (dfile->decompressionStream != NULL) {
            free(dfile->decompressionStream);
            dfile->decompressionStream = NULL;
        }

        if (dfile->decompressionBuffer != NULL) {
            free(dfile->decompressionBuffer);
            dfile->decompressionBuffer = NULL;
        }
    }

    if (mode[1] == 't') {
        dfile->flags |= DFILE_TEXT;
    }

    return dfile;

err:

    if (dfile != NULL) {
        dfileClose(dfile);
    }

    return NULL;
}

// 0x4E5F9C
static int dfileReadCharInternal(DFile* stream)
{
    if (stream->entry->compressed == 1) {
        char ch;
        if (!dfileReadCompressed(stream, &ch, sizeof(ch))) {
            return -1;
        }

        if ((stream->flags & DFILE_TEXT) != 0) {
            // NOTE: I'm not sure if they are comparing as chars or ints. Since
            // character literals are ints, let's cast read characters to int as
            // well.
            if (ch == '\r') {
                char nextCh;
                if (dfileReadCompressed(stream, &nextCh, sizeof(nextCh))) {
                    if (nextCh == '\n') {
                        ch = nextCh;
                    } else {
                        // NOTE: Uninline.
                        dfileUngetCompressed(stream, nextCh & 0xFF);
                    }
                }
            }
        }

        return ch & 0xFF;
    }

    if (stream->position >= stream->entry->uncompressedSize) {
        return -1;
    }

    int ch = fgetc(stream->stream);
    if (ch != -1) {
        if ((stream->flags & DFILE_TEXT) != 0) {
            // This is a text stream, attempt to detect \r\n sequence.
            if (ch == '\r') {
                if (stream->position + 1 < stream->entry->uncompressedSize) {
                    int nextCh = fgetc(stream->stream);
                    if (nextCh == '\n') {
                        ch = nextCh;
                        stream->position++;
                    } else {
                        ungetc(nextCh, stream->stream);
                    }
                }
            }
        }

        stream->position++;
    }

    return ch;
}

// 0x4E6078
static bool dfileReadCompressed(DFile* stream, void* ptr, size_t size)
{
    if ((stream->flags & DFILE_HAS_COMPRESSED_UNGETC) != 0) {
        unsigned char* byteBuffer = (unsigned char*)ptr;
        *byteBuffer++ = stream->compressedUngotten & 0xFF;
        ptr = byteBuffer;

        size--;

        stream->flags &= ~DFILE_HAS_COMPRESSED_UNGETC;
        stream->position++;

        if (size == 0) {
            return true;
        }
    }

    stream->decompressionStream->next_out = (Bytef*)ptr;
    stream->decompressionStream->avail_out = size;

    do {
        if (stream->decompressionStream->avail_out == 0) {
            // Everything was decompressed.
            break;
        }

        if (stream->decompressionStream->avail_in == 0) {
            // No more unprocessed data, request next chunk.
            size_t bytesToRead = std::min(DFILE_DECOMPRESSION_BUFFER_SIZE, stream->entry->dataSize - stream->compressedBytesRead);

            if (fread(stream->decompressionBuffer, bytesToRead, 1, stream->stream) != 1) {
                break;
            }

            stream->decompressionStream->avail_in = bytesToRead;
            stream->decompressionStream->next_in = stream->decompressionBuffer;

            stream->compressedBytesRead += bytesToRead;
        }
    } while (inflate(stream->decompressionStream, Z_NO_FLUSH) == Z_OK);

    if (stream->decompressionStream->avail_out != 0) {
        // There are some data still waiting, which means there was in error
        // during decompression loop above.
        return false;
    }

    stream->position += size;

    return true;
}

// NOTE: Inlined.
//
// 0x4E613C
static void dfileUngetCompressed(DFile* stream, int ch)
{
    stream->compressedUngotten = ch;
    stream->flags |= DFILE_HAS_COMPRESSED_UNGETC;
    stream->position--;
}

static void dfile_normalize_path(char* path)
{
    compat_windows_path_to_native(path);
    compat_strlwr(path);
}

} // namespace fallout
