#ifndef XFILE_H
#define XFILE_H

#include "dfile.h"

#include <stdio.h>
#include <zlib.h>

typedef enum XFileType {
    XFILE_TYPE_FILE,
    XFILE_TYPE_DFILE,
    XFILE_TYPE_GZFILE,
} XFileType;

// A universal database of files.
typedef struct XBase {
    // The path to directory or .DAT file that this xbase represents.
    char* path;

    // The [DBase] instance that this xbase represents.
    DBase* dbase;

    // A flag used to denote that this xbase represents .DAT file (true), or
    // a directory (false).
    //
    // NOTE: Original type is 1 byte, likely unsigned char.
    bool isDbase;

    // Next [XBase] in linked list.
    struct XBase* next;
} XBase;

typedef struct XFile {
    XFileType type;
    union {
        FILE* file;
        DFile* dfile;
        gzFile gzfile;
    };
} XFile;

typedef struct XList {
    int fileNamesLength;
    char** fileNames;
} XList;

typedef enum XFileEnumerationEntryType {
    XFILE_ENUMERATION_ENTRY_TYPE_FILE,
    XFILE_ENUMERATION_ENTRY_TYPE_DIRECTORY,
    XFILE_ENUMERATION_ENTRY_TYPE_DFILE,
} XFileEnumerationEntryType;

typedef struct XListEnumerationContext {
    char name[FILENAME_MAX];
    unsigned char type;
    XList* xlist;
} XListEnumerationContext;

typedef bool(XListEnumerationHandler)(XListEnumerationContext* context);

extern XBase* gXbaseHead;
extern bool gXbaseExitHandlerRegistered;

int xfileClose(XFile* stream);
XFile* xfileOpen(const char* filename, const char* mode);
int xfilePrintFormatted(XFile* xfile, const char* format, ...);
int xfilePrintFormattedArgs(XFile* stream, const char* format, va_list args);
int xfileReadChar(XFile* stream);
char* xfileReadString(char* string, int size, XFile* stream);
int xfileWriteChar(int ch, XFile* stream);
int xfileWriteString(const char* s, XFile* stream);
size_t xfileRead(void* ptr, size_t size, size_t count, XFile* stream);
size_t xfileWrite(const void* buf, size_t size, size_t count, XFile* stream);
int xfileSeek(XFile* stream, long offset, int origin);
long xfileTell(XFile* stream);
void xfileRewind(XFile* stream);
int xfileEof(XFile* stream);
long xfileGetSize(XFile* stream);
bool xbaseReopenAll(char* paths);
bool xbaseOpen(const char* path);
bool xlistEnumerate(const char* pattern, XListEnumerationHandler* handler, XList* xlist);
bool xlistInit(const char* pattern, XList* xlist);
void xlistFree(XList* xlist);
int xbaseMakeDirectory(const char* path);
void xbaseCloseAll();
void xbaseExitHandler(void);
bool xlistEnumerateHandler(XListEnumerationContext* context);

#endif /* XFILE_H */
