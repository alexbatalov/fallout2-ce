#ifndef PALETTE_H
#define PALETTE_H

namespace fallout {

extern unsigned char gPaletteWhite[256 * 3];
extern unsigned char gPaletteBlack[256 * 3];

void paletteInit();
void paletteReset();
void paletteExit();
void paletteFadeTo(unsigned char* palette);
void paletteSetEntries(unsigned char* palette);
void paletteSetEntriesInRange(unsigned char* palette, int start, int end);

} // namespace fallout

#endif /* PALETTE_H */
