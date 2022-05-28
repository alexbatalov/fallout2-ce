#include "xfile.h"

#include "file_find.h"

#include <assert.h>
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 0x6B24D0
XBase* gXbaseHead;

// 0x6B24D4
bool gXbaseExitHandlerRegistered;

// 0x4DED6C
int xfileClose(XFile* stream)
{
    assert(stream); // "stream", "xfile.c", 112

    int rc;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        rc = dfileClose(stream->dfile);
        break;
    case XFILE_TYPE_GZFILE:
        rc = gzclose(stream->gzfile);
        break;
    default:
        rc = fclose(stream->file);
        break;
    }

    memset(stream, 0, sizeof(*stream));

    free(stream);

    return rc;
}

// 0x4DEE2C
XFile* xfileOpen(const char* filePath, const char* mode)
{
    assert(filePath); // "filename", "xfile.c", 162
    assert(mode); // "mode", "xfile.c", 163

    XFile* stream = (XFile*)malloc(sizeof(*stream));
    if (stream == NULL) {
        return NULL;
    }

    memset(stream, 0, sizeof(*stream));

    // NOTE: Compiled code uses different lengths.
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(filePath, drive, dir, NULL, NULL);

    char path[COMPAT_MAX_PATH];
    if (drive[0] != '\0' || dir[0] == '\\' || dir[0] == '/' || dir[0] == '.') {
        // [filePath] is an absolute path. Attempt to open as plain stream.
        stream->file = fopen(filePath, mode);
        if (stream->file == NULL) {
            free(stream);
            return NULL;
        }

        stream->type = XFILE_TYPE_FILE;
        sprintf(path, filePath);
    } else {
        // [filePath] is a relative path. Loop thru open xbases and attempt to
        // open [filePath] from appropriate xbase.
        XBase* curr = gXbaseHead;
        while (curr != NULL) {
            if (curr->isDbase) {
                // Attempt to open dfile stream from dbase.
                stream->dfile = dfileOpen(curr->dbase, filePath, mode);
                if (stream->dfile != NULL) {
                    stream->type = XFILE_TYPE_DFILE;
                    sprintf(path, filePath);
                    break;
                }
            } else {
                // Build path relative to directory-based xbase.
                sprintf(path, "%s\\%s", curr->path, filePath);

                // Attempt to open plain stream.
                stream->file = fopen(path, mode);
                if (stream->file != NULL) {
                    stream->type = XFILE_TYPE_FILE;
                    break;
                }
            }
            curr = curr->next;
        }

        if (stream->file == NULL) {
            // File was not opened during the loop above. Attempt to open file
            // relative to the current working directory.
            stream->file = fopen(filePath, mode);
            if (stream->file == NULL) {
                free(stream);
                return NULL;
            }

            stream->type = XFILE_TYPE_FILE;
            sprintf(path, filePath);
        }
    }

    if (stream->type == XFILE_TYPE_FILE) {
        // Opened file is a plain stream, which might be gzipped. In this case
        // first two bytes will contain magic numbers.
        int ch1 = fgetc(stream->file);
        int ch2 = fgetc(stream->file);
        if (ch1 == 0x1F && ch2 == 0x8B) {
            // File is gzipped. Close plain stream and reopen this file as
            // gzipped stream.
            fclose(stream->file);

            stream->type = XFILE_TYPE_GZFILE;
            stream->gzfile = gzopen(path, mode);
        } else {
            // File is not gzipped.
            rewind(stream->file);
        }
    }

    return stream;
}

// 0x4DF11C
int xfilePrintFormatted(XFile* stream, const char* format, ...)
{
    assert(format); // "format", "xfile.c", 305

    va_list args;
    va_start(args, format);

    int rc = xfilePrintFormattedArgs(stream, format, args);

    va_end(args);

    return rc;
}

// [vfprintf].
//
// 0x4DF1AC
int xfilePrintFormattedArgs(XFile* stream, const char* format, va_list args)
{
    assert(stream); // "stream", "xfile.c", 332
    assert(format); // "format", "xfile.c", 333

    int rc;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        rc = dfilePrintFormattedArgs(stream->dfile, format, args);
        break;
    case XFILE_TYPE_GZFILE:
        rc = gzvprintf(stream->gzfile, format, args);
        break;
    default:
        rc = vfprintf(stream->file, format, args);
        break;
    }

    return rc;
}

// 0x4DF22C
int xfileReadChar(XFile* stream)
{
    assert(stream); // "stream", "xfile.c", 354

    int ch;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        ch = dfileReadChar(stream->dfile);
        break;
    case XFILE_TYPE_GZFILE:
        ch = gzgetc(stream->gzfile);
        break;
    default:
        ch = fgetc(stream->file);
        break;
    }

    return ch;
}

// 0x4DF280
char* xfileReadString(char* string, int size, XFile* stream)
{
    assert(string); // "s", "xfile.c", 375
    assert(size); // "n", "xfile.c", 376
    assert(stream); // "stream", "xfile.c", 377

    char* result;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        result = dfileReadString(string, size, stream->dfile);
        break;
    case XFILE_TYPE_GZFILE:
        result = gzgets(stream->gzfile, string, size);
        break;
    default:
        result = fgets(string, size, stream->file);
        break;
    }

    return result;
}

// 0x4DF320
int xfileWriteChar(int ch, XFile* stream)
{
    assert(stream); // "stream", "xfile.c", 399

    int rc;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        rc = dfileWriteChar(ch, stream->dfile);
        break;
    case XFILE_TYPE_GZFILE:
        rc = gzputc(stream->gzfile, ch);
        break;
    default:
        rc = fputc(ch, stream->file);
        break;
    }

    return rc;
}

// 0x4DF380
int xfileWriteString(const char* string, XFile* stream)
{
    assert(string); // "s", "xfile.c", 421
    assert(stream); // "stream", "xfile.c", 422

    int rc;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        rc = dfileWriteString(string, stream->dfile);
        break;
    case XFILE_TYPE_GZFILE:
        rc = gzputs(stream->gzfile, string);
        break;
    default:
        rc = fputs(string, stream->file);
        break;
    }

    return rc;
}

// 0x4DF44C
size_t xfileRead(void* ptr, size_t size, size_t count, XFile* stream)
{
    assert(ptr); // "ptr", "xfile.c", 421
    assert(stream); // "stream", "xfile.c", 422

    size_t elementsRead;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        elementsRead = dfileRead(ptr, size, count, stream->dfile);
        break;
    case XFILE_TYPE_GZFILE:
        // FIXME: There is a bug in the return value. Both [dfileRead] and
        // [fread] returns number of elements read, but [gzwrite] have no such
        // concept, it works with bytes, and returns number of bytes read.
        // Depending on the [size] and [count] parameters this function can
        // return wrong result.
        elementsRead = gzread(stream->gzfile, ptr, size * count);
        break;
    default:
        elementsRead = fread(ptr, size, count, stream->file);
        break;
    }

    return elementsRead;
}

// 0x4DF4E8
size_t xfileWrite(const void* ptr, size_t size, size_t count, XFile* stream)
{
    assert(ptr); // "ptr", "xfile.c", 504
    assert(stream); // "stream", "xfile.c", 505

    size_t elementsWritten;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        elementsWritten = dfileWrite(ptr, size, count, stream->dfile);
        break;
    case XFILE_TYPE_GZFILE:
        // FIXME: There is a bug in the return value. [fwrite] returns number
        // of elements written (while [dfileWrite] does not support writing at
        // all), but [gzwrite] have no such concept, it works with bytes, and
        // returns number of bytes written. Depending on the [size] and [count]
        // parameters this function can return wrong result.
        elementsWritten = gzwrite(stream->gzfile, ptr, size * count);
        break;
    default:
        elementsWritten = fwrite(ptr, size, count, stream->file);
        break;
    }

    return elementsWritten;
}

// 0x4DF5D8
int xfileSeek(XFile* stream, long offset, int origin)
{
    assert(stream); // "stream", "xfile.c", 547

    int result;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        result = dfileSeek(stream->dfile, offset, origin);
        break;
    case XFILE_TYPE_GZFILE:
        result = gzseek(stream->gzfile, offset, origin);
        break;
    default:
        result = fseek(stream->file, offset, origin);
        break;
    }

    return result;
}

// 0x4DF690
long xfileTell(XFile* stream)
{
    assert(stream); // "stream", "xfile.c", 588

    long pos;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        pos = dfileTell(stream->dfile);
        break;
    case XFILE_TYPE_GZFILE:
        pos = gztell(stream->gzfile);
        break;
    default:
        pos = ftell(stream->file);
        break;
    }

    return pos;
}

// 0x4DF6E4
void xfileRewind(XFile* stream)
{
    assert(stream); // "stream", "xfile.c", 608

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        dfileRewind(stream->dfile);
        break;
    case XFILE_TYPE_GZFILE:
        gzrewind(stream->gzfile);
        break;
    default:
        rewind(stream->file);
        break;
    }
}

// 0x4DF780
int xfileEof(XFile* stream)
{
    assert(stream); // "stream", "xfile.c", 648

    int rc;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        rc = dfileEof(stream->dfile);
        break;
    case XFILE_TYPE_GZFILE:
        rc = gzeof(stream->gzfile);
        break;
    default:
        rc = feof(stream->file);
        break;
    }

    return rc;
}

// 0x4DF828
long xfileGetSize(XFile* stream)
{
    assert(stream); // "stream", "xfile.c", 690

    long fileSize;

    switch (stream->type) {
    case XFILE_TYPE_DFILE:
        fileSize = dfileGetSize(stream->dfile);
        break;
    case XFILE_TYPE_GZFILE:
        fileSize = 0;
        break;
    default:
        fileSize = compat_filelength(fileno(stream->file));
        break;
    }

    return fileSize;
}

// Closes all open xbases and opens a set of xbases specified by [paths].
//
// [paths] is a set of paths separated by semicolon. Can be NULL, in this case
// all open xbases are simply closed.
//
// 0x4DF878
bool xbaseReopenAll(char* paths)
{
    // NOTE: Uninline.
    xbaseCloseAll();

    if (paths != NULL) {
        char* tok = strtok(paths, ";");
        while (tok != NULL) {
            if (!xbaseOpen(tok)) {
                return false;
            }
            tok = strtok(NULL, ";");
        }
    }

    return true;
}

// 0x4DF938
bool xbaseOpen(const char* path)
{
    assert(path); // "path", "xfile.c", 747

    // Register atexit handler so that underlying dbase (if any) can be
    // gracefully closed.
    if (!gXbaseExitHandlerRegistered) {
        atexit(xbaseExitHandler);
        gXbaseExitHandlerRegistered = true;
    }

    XBase* curr = gXbaseHead;
    XBase* prev = NULL;
    while (curr != NULL) {
        if (compat_stricmp(path, curr->path) == 0) {
            break;
        }

        prev = curr;
        curr = curr->next;
    }

    if (curr != NULL) {
        if (prev != NULL) {
            // Move found xbase to the top.
            prev->next = curr->next;
            curr->next = gXbaseHead;
            gXbaseHead = curr;
        }
        return true;
    }

    XBase* xbase = (XBase*)malloc(sizeof(*xbase));
    if (xbase == NULL) {
        return false;
    }

    memset(xbase, 0, sizeof(*xbase));

    xbase->path = strdup(path);
    if (xbase->path == NULL) {
        free(xbase);
        return false;
    }

    DBase* dbase = dbaseOpen(path);
    if (dbase != NULL) {
        xbase->isDbase = true;
        xbase->dbase = dbase;
        xbase->next = gXbaseHead;
        gXbaseHead = xbase;
        return true;
    }

    char workingDirectory[COMPAT_MAX_PATH];
    if (getcwd(workingDirectory, COMPAT_MAX_PATH) == NULL) {
        // FIXME: Leaking xbase and path.
        return false;
    }

    if (chdir(path) == 0) {
        chdir(workingDirectory);
        xbase->next = gXbaseHead;
        gXbaseHead = xbase;
        return true;
    }

    if (xbaseMakeDirectory(path) != 0) {
        // FIXME: Leaking xbase and path.
        return false;
    }

    chdir(workingDirectory);

    xbase->next = gXbaseHead;
    gXbaseHead = xbase;

    return true;
}

// 0x4DFB3C
bool xlistEnumerate(const char* pattern, XListEnumerationHandler* handler, XList* xlist)
{
    assert(pattern); // "filespec", "xfile.c", 845
    assert(handler); // "enumfunc", "xfile.c", 846

    DirectoryFileFindData directoryFileFindData;
    XListEnumerationContext context;

    context.xlist = xlist;

    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    char fileName[COMPAT_MAX_FNAME];
    char extension[COMPAT_MAX_EXT];
    compat_splitpath(pattern, drive, dir, fileName, extension);
    if (drive[0] != '\0' || dir[0] == '\\' || dir[0] == '/' || dir[0] == '.') {
        if (fileFindFirst(pattern, &directoryFileFindData)) {
            do {
                bool isDirectory = fileFindIsDirectory(&directoryFileFindData);
                char* entryName = fileFindGetName(&directoryFileFindData);

                if (isDirectory) {
                    if (strcmp(entryName, "..") == 0 || strcmp(entryName, ".") == 0) {
                        continue;
                    }

                    context.type = XFILE_ENUMERATION_ENTRY_TYPE_DIRECTORY;
                } else {
                    context.type = XFILE_ENUMERATION_ENTRY_TYPE_FILE;
                }

                compat_makepath(context.name, drive, dir, entryName, NULL);

                if (!handler(&context)) {
                    break;
                }
            } while (fileFindNext(&directoryFileFindData));
        }
        return findFindClose(&directoryFileFindData);
    }

    XBase* xbase = gXbaseHead;
    while (xbase != NULL) {
        if (xbase->isDbase) {
            DFileFindData dbaseFindData;
            if (dbaseFindFirstEntry(xbase->dbase, &dbaseFindData, pattern)) {
                context.type = XFILE_ENUMERATION_ENTRY_TYPE_DFILE;

                do {
                    strcpy(context.name, dbaseFindData.fileName);
                    if (!handler(&context)) {
                        return dbaseFindClose(xbase->dbase, &dbaseFindData);
                    }
                } while (dbaseFindNextEntry(xbase->dbase, &dbaseFindData));

                dbaseFindClose(xbase->dbase, &dbaseFindData);
            }
        } else {
            char path[COMPAT_MAX_PATH];
            sprintf(path, "%s\\%s", xbase->path, pattern);

            if (fileFindFirst(path, &directoryFileFindData)) {
                do {
                    bool isDirectory = fileFindIsDirectory(&directoryFileFindData);
                    char* entryName = fileFindGetName(&directoryFileFindData);

                    if (isDirectory) {
                        if (strcmp(entryName, "..") == 0 || strcmp(entryName, ".") == 0) {
                            continue;
                        }

                        context.type = XFILE_ENUMERATION_ENTRY_TYPE_DIRECTORY;
                    } else {
                        context.type = XFILE_ENUMERATION_ENTRY_TYPE_FILE;
                    }

                    compat_makepath(context.name, drive, dir, entryName, NULL);

                    if (!handler(&context)) {
                        break;
                    }
                } while (fileFindNext(&directoryFileFindData));
            }
            return findFindClose(&directoryFileFindData);
        }
        xbase = xbase->next;
    }

    compat_splitpath(pattern, drive, dir, fileName, extension);
    if (fileFindFirst(pattern, &directoryFileFindData)) {
        do {
            bool isDirectory = fileFindIsDirectory(&directoryFileFindData);
            char* entryName = fileFindGetName(&directoryFileFindData);

            if (isDirectory) {
                if (strcmp(entryName, "..") == 0 || strcmp(entryName, ".") == 0) {
                    continue;
                }

                context.type = XFILE_ENUMERATION_ENTRY_TYPE_DIRECTORY;
            } else {
                context.type = XFILE_ENUMERATION_ENTRY_TYPE_FILE;
            }

            compat_makepath(context.name, drive, dir, entryName, NULL);

            if (!handler(&context)) {
                break;
            }
        } while (fileFindNext(&directoryFileFindData));
    }
    return findFindClose(&directoryFileFindData);
}

// 0x4DFF28
bool xlistInit(const char* pattern, XList* xlist)
{
    xlistEnumerate(pattern, xlistEnumerateHandler, xlist);
    return xlist->fileNamesLength != -1;
}

// 0x4DFF48
void xlistFree(XList* xlist)
{
    assert(xlist); // "list", "xfile.c", 949

    for (int index = 0; index < xlist->fileNamesLength; index++) {
        if (xlist->fileNames[index] != NULL) {
            free(xlist->fileNames[index]);
        }
    }

    free(xlist->fileNames);

    memset(xlist, 0, sizeof(*xlist));
}

// Recursively creates specified file path.
//
// 0x4DFFAC
int xbaseMakeDirectory(const char* filePath)
{
    char workingDirectory[COMPAT_MAX_PATH];
    if (getcwd(workingDirectory, COMPAT_MAX_PATH) == NULL) {
        return -1;
    }

    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(filePath, drive, dir, NULL, NULL);

    char path[COMPAT_MAX_PATH];
    if (drive[0] != '\0' || dir[0] == '\\' || dir[0] == '/' || dir[0] == '.') {
        // [filePath] is an absolute path.
        strcpy(path, filePath);
    } else {
        // Find first directory-based xbase.
        XBase* curr = gXbaseHead;
        while (curr != NULL) {
            if (!curr->isDbase) {
                sprintf(path, "%s\\%s", curr->path, filePath);
                break;
            }
            curr = curr->next;
        }

        if (curr == NULL) {
            // Either there are no directory-based xbase, or there are no open
            // xbases at all - resolve path against current working directory.
            sprintf(path, "%s\\%s", workingDirectory, filePath);
        }
    }

    char* pch = path;

    if (*pch == '\\' || *pch == '/') {
        pch++;
    }

    while (*pch != '\0') {
        if (*pch == '\\' || *pch == '/') {
            char temp = *pch;
            *pch = '\0';

            if (chdir(path) != 0) {
                if (mkdir(path) != 0) {
                    chdir(workingDirectory);
                    return -1;
                }
            } else {
                chdir(workingDirectory);
            }

            *pch = temp;
        }
        pch++;
    }

    // Last path component.
    mkdir(path);

    chdir(workingDirectory);

    return 0;
}

// Closes all xbases.
//
// NOTE: Inlined.
//
// 0x4E01F8
void xbaseCloseAll()
{
    XBase* curr = gXbaseHead;
    gXbaseHead = NULL;

    while (curr != NULL) {
        XBase* next = curr->next;

        if (curr->isDbase) {
            dbaseClose(curr->dbase);
        }

        free(curr->path);
        free(curr);

        curr = next;
    }
}

// xbase atexit
void xbaseExitHandler(void)
{
    // NOTE: Uninline.
    xbaseCloseAll();
}

// 0x4E0278
bool xlistEnumerateHandler(XListEnumerationContext* context)
{
    if (context->type == XFILE_ENUMERATION_ENTRY_TYPE_DIRECTORY) {
        return true;
    }

    XList* xlist = context->xlist;

    char** fileNames = (char**)realloc(xlist->fileNames, sizeof(*fileNames) * (xlist->fileNamesLength + 1));
    if (fileNames == NULL) {
        xlistFree(xlist);
        xlist->fileNamesLength = -1;
        return false;
    }

    xlist->fileNames = fileNames;

    fileNames[xlist->fileNamesLength] = strdup(context->name);
    if (fileNames[xlist->fileNamesLength] == NULL) {
        xlistFree(xlist);
        xlist->fileNamesLength = -1;
        return false;
    }

    xlist->fileNamesLength++;

    return true;
}
