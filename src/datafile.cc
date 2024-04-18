#include "datafile.h"

#include <string.h>

#include "color.h"
#include "db.h"
#include "memory_manager.h"
#include "pcx.h"
#include "platform_compat.h"

namespace fallout {

// 0x5184AC
DatafileLoader* gDatafileLoader = nullptr;

// 0x5184B0
DatafileNameMangler* gDatafileNameMangler = datafileDefaultNameManglerImpl;

// 0x56D7E0
unsigned char gDatafilePalette[768];

// 0x42EE70
char* datafileDefaultNameManglerImpl(char* path)
{
    return path;
}

// NOTE: Unused.
//
// 0x42EE74
void datafileSetNameMangler(DatafileNameMangler* mangler)
{
    gDatafileNameMangler = mangler;
}

// NOTE: Unused.
//
// 0x42EE7C
void datafileSetLoader(DatafileLoader* loader)
{
    gDatafileLoader = loader;
}

// 0x42EE84
void sub_42EE84(unsigned char* data, unsigned char* palette, int width, int height)
{
    unsigned char indexedPalette[256];

    indexedPalette[0] = 0;
    for (int index = 1; index < 256; index++) {
        // TODO: Check.
        int r = palette[index * 3 + 2] >> 3;
        int g = palette[index * 3 + 1] >> 3;
        int b = palette[index * 3] >> 3;
        int colorTableIndex = (r << 10) | (g << 5) | b;
        indexedPalette[index] = _colorTable[colorTableIndex];
    }

    int size = width * height;
    for (int index = 0; index < size; index++) {
        data[index] = indexedPalette[data[index]];
    }
}

// NOTE: Unused.
//
// 0x42EEF8
void sub_42EEF8(unsigned char* data, unsigned char* palette, int width, int height)
{
    unsigned char indexedPalette[256];

    indexedPalette[0] = 0;
    for (int index = 1; index < 256; index++) {
        // TODO: Check.
        int r = palette[index * 3 + 2] >> 1;
        int g = palette[index * 3 + 1] >> 1;
        int b = palette[index * 3] >> 1;
        int colorTableIndex = (r << 10) | (g << 5) | b;
        indexedPalette[index] = _colorTable[colorTableIndex];
    }

    int size = width * height;
    for (int index = 0; index < size; index++) {
        data[index] = indexedPalette[data[index]];
    }
}

// 0x42EF60
unsigned char* datafileReadRaw(char* path, int* widthPtr, int* heightPtr)
{
    char* mangledPath = gDatafileNameMangler(path);
    char* dot = strrchr(mangledPath, '.');
    if (dot != nullptr) {
        if (compat_stricmp(dot + 1, "pcx") == 0) {
            return pcxRead(mangledPath, widthPtr, heightPtr, gDatafilePalette);
        }
    }

    if (gDatafileLoader != nullptr) {
        return gDatafileLoader(mangledPath, gDatafilePalette, widthPtr, heightPtr);
    }

    return nullptr;
}

// 0x42EFCC
unsigned char* datafileRead(char* path, int* widthPtr, int* heightPtr)
{
    unsigned char* v1 = datafileReadRaw(path, widthPtr, heightPtr);
    if (v1 != nullptr) {
        sub_42EE84(v1, gDatafilePalette, *widthPtr, *heightPtr);
    }
    return v1;
}

// NOTE: Unused
//
// 0x42EFF4
unsigned char* sub_42EFF4(char* path)
{
    int width;
    int height;
    unsigned char* v3 = datafileReadRaw(path, &width, &height);
    if (v3 != nullptr) {
        internal_free_safe(v3, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 148
        return gDatafilePalette;
    }

    return nullptr;
}

// NOTE: Unused.
//
// 0x42F024
void sub_42F024(unsigned char* data, int* widthPtr, int* heightPtr)
{
    int width = *widthPtr;
    int height = *heightPtr;
    unsigned char* temp = (unsigned char*)internal_malloc_safe(width * height, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 157

    // NOTE: Original code does not initialize `x`.
    int y = 0;
    int x = 0;
    unsigned char* src1 = data;

    for (y = 0; y < height; y++) {
        if (*src1 == 0) {
            break;
        }

        unsigned char* src2 = src1;
        for (x = 0; x < width; x++) {
            if (*src2 == 0) {
                break;
            }

            *temp++ = *src2++;
        }

        src1 += width;
    }

    memcpy(data, temp, x * y);
    internal_free_safe(temp, __FILE__, __LINE__); // // "..\\int\\DATAFILE.C", 171
}

// 0x42F0E4
unsigned char* datafileGetPalette()
{
    return gDatafilePalette;
}

// NOTE: Unused.
//
// 0x42F0EC
unsigned char* datafileLoad(char* path, int* sizePtr)
{
    const char* mangledPath = gDatafileNameMangler(path);
    File* stream = fileOpen(mangledPath, "rb");
    if (stream == nullptr) {
        return nullptr;
    }

    int size = fileGetSize(stream);
    unsigned char* data = (unsigned char*)internal_malloc_safe(size, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 185
    if (data == nullptr) {
        // NOTE: This code is unreachable, internal_malloc_safe never fails.
        // Otherwise it leaks stream.
        *sizePtr = 0;
        return nullptr;
    }

    fileRead(data, 1, size, stream);
    fileClose(stream);
    *sizePtr = size;
    return data;
}

} // namespace fallout
