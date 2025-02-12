#include "pcx.h"

#include "memory_manager.h"

namespace fallout {

// 0x519DC8
unsigned char gPcxLastRunLength = 0;

// 0x519DC9
unsigned char gPcxLastValue = 0;

// NOTE: The reading method in this function is a little bit odd. It does not
// use high level reading functions, which can read right into struct. Instead
// they read everything into temporary variables. There are no error checks.
//
// 0x4961D4
void pcxReadHeader(PcxHeader* pcxHeader, File* stream)
{
    pcxHeader->identifier = fileReadChar(stream);
    pcxHeader->version = fileReadChar(stream);
    pcxHeader->encoding = fileReadChar(stream);
    pcxHeader->bitsPerPixel = fileReadChar(stream);

    short minX;
    fileRead(&minX, 2, 1, stream);
    pcxHeader->minX = minX;

    short minY;
    fileRead(&minY, 2, 1, stream);
    pcxHeader->minY = minY;

    short maxX;
    fileRead(&maxX, 2, 1, stream);
    pcxHeader->maxX = maxX;

    short maxY;
    fileRead(&maxY, 2, 1, stream);
    pcxHeader->maxY = maxY;

    short horizontalResolution;
    fileRead(&horizontalResolution, 2, 1, stream);
    pcxHeader->horizontalResolution = horizontalResolution;

    short verticalResolution;
    fileRead(&verticalResolution, 2, 1, stream);
    pcxHeader->verticalResolution = verticalResolution;

    for (int index = 0; index < 48; index++) {
        pcxHeader->palette[index] = fileReadChar(stream);
    }

    pcxHeader->reserved1 = fileReadChar(stream);
    pcxHeader->planeCount = fileReadChar(stream);

    short bytesPerLine;
    fileRead(&bytesPerLine, 2, 1, stream);
    pcxHeader->bytesPerLine = bytesPerLine;

    short paletteType;
    fileRead(&paletteType, 2, 1, stream);
    pcxHeader->paletteType = paletteType;

    short horizontalScreenSize;
    fileRead(&horizontalScreenSize, 2, 1, stream);
    pcxHeader->horizontalScreenSize = horizontalScreenSize;

    short verticalScreenSize;
    fileRead(&verticalScreenSize, 2, 1, stream);
    pcxHeader->verticalScreenSize = verticalScreenSize;

    for (int index = 0; index < 54; index++) {
        pcxHeader->reserved2[index] = fileReadChar(stream);
    }
}

// 0x49636C
int pcxReadLine(unsigned char* data, int size, File* stream)
{
    unsigned char runLength = gPcxLastRunLength;
    unsigned char value = gPcxLastValue;

    int uncompressedSize = 0;
    int index = 0;
    do {
        uncompressedSize += runLength;
        while (runLength > 0 && index < size) {
            data[index] = value;
            runLength--;
            index++;
        }

        gPcxLastRunLength = runLength;
        gPcxLastValue = value;

        if (runLength != 0) {
            uncompressedSize -= runLength;
            break;
        }

        value = fileReadChar(stream);
        if ((value & 0xC0) == 0xC0) {
            gPcxLastRunLength = value & 0x3F;
            value = fileReadChar(stream);
            runLength = gPcxLastRunLength;
        } else {
            runLength = 1;
        }
    } while (index < size);

    gPcxLastRunLength = runLength;
    gPcxLastValue = value;

    return uncompressedSize;
}

// 0x49641C
int pcxReadPalette(PcxHeader* pcxHeader, unsigned char* palette, File* stream)
{
    if (pcxHeader->version != 5) {
        return 0;
    }

    long pos = fileTell(stream);
    long size = fileGetSize(stream);
    fileSeek(stream, size - 769, SEEK_SET);
    if (fileReadChar(stream) != 12) {
        fileSeek(stream, pos, SEEK_SET);
        return 0;
    }

    for (int index = 0; index < 768; index++) {
        palette[index] = fileReadChar(stream);
    }

    fileSeek(stream, pos, SEEK_SET);

    return 1;
}

// 0x496494
unsigned char* pcxRead(const char* path, int* widthPtr, int* heightPtr, unsigned char* palette)
{
    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        return nullptr;
    }

    PcxHeader pcxHeader;
    pcxReadHeader(&pcxHeader, stream);

    int width = pcxHeader.maxX - pcxHeader.minX + 1;
    int height = pcxHeader.maxY - pcxHeader.minY + 1;

    *widthPtr = width;
    *heightPtr = height;

    int bytesPerLine = pcxHeader.planeCount * pcxHeader.bytesPerLine;
    unsigned char* data = (unsigned char*)internal_malloc_safe(bytesPerLine * height, __FILE__, __LINE__); // "..\\int\\PCX.C", 195
    if (data == nullptr) {
        // NOTE: This code is unreachable, internal_malloc_safe never fails.
        fileClose(stream);
        return nullptr;
    }

    gPcxLastRunLength = 0;
    gPcxLastValue = 0;

    unsigned char* ptr = data;
    for (int y = 0; y < height; y++) {
        pcxReadLine(ptr, bytesPerLine, stream);
        ptr += width;
    }

    pcxReadPalette(&pcxHeader, palette, stream);

    fileClose(stream);

    return data;
}

} // namespace fallout
