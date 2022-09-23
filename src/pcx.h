#ifndef PCX_H
#define PCX_H

#include "db.h"

namespace fallout {

typedef struct PcxHeader {
    unsigned char identifier;
    unsigned char version;
    unsigned char encoding;
    unsigned char bitsPerPixel;
    short minX;
    short minY;
    short maxX;
    short maxY;
    short horizontalResolution;
    short verticalResolution;
    unsigned char palette[48];
    unsigned char reserved1;
    unsigned char planeCount;
    short bytesPerLine;
    short paletteType;
    short horizontalScreenSize;
    short verticalScreenSize;
    unsigned char reserved2[54];
} PcxHeader;

extern unsigned char gPcxLastRunLength;
extern unsigned char gPcxLastValue;

void pcxReadHeader(PcxHeader* pcxHeader, File* stream);
int pcxReadLine(unsigned char* data, int size, File* stream);
int pcxReadPalette(PcxHeader* pcxHeader, unsigned char* palette, File* stream);
unsigned char* pcxRead(const char* path, int* widthPtr, int* heightPtr, unsigned char* palette);

} // namespace fallout

#endif /* PCX_H */
