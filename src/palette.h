#ifndef PALETTE_H
#define PALETTE_H

extern unsigned char gPalette[256 * 3];
extern unsigned char gPaletteWhite[256 * 3];
extern unsigned char gPaletteBlack[256 * 3];
extern int gPaletteFadeSteps;

void paletteInit();
void _palette_reset_();
void paletteReset();
void paletteExit();
void paletteFadeTo(unsigned char* palette);
void paletteSetEntries(unsigned char* palette);
void paletteSetEntriesInRange(unsigned char* palette, int start, int end);

#endif /* PALETTE_H */
