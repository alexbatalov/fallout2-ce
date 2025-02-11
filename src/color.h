#ifndef COLOR_H
#define COLOR_H

namespace fallout {

typedef unsigned char Color;
typedef const char*(ColorFileNameManger)(const char*);
typedef void(ColorTransitionCallback)();

extern unsigned char _cmap[768];

extern unsigned char _systemCmap[256 * 3];
extern unsigned char _currentGammaTable[64];
extern unsigned char* _blendTable[256];
extern unsigned char _mappedColor[256];
extern Color colorMixAddTable[256][256];
extern Color intensityColorTable[256][256];
extern Color colorMixMulTable[256][256];
extern unsigned char _colorTable[32768];

int _calculateColor(int intensity, Color color);
int Color2RGB(Color c);
void colorPaletteFadeBetween(unsigned char* oldPalette, unsigned char* newPalette, int steps);
void colorPaletteSetTransitionCallback(ColorTransitionCallback* callback);
void _setSystemPalette(unsigned char* palette);
unsigned char* _getSystemPalette();
void _setSystemPaletteEntries(unsigned char* a1, int a2, int a3);
bool colorPaletteLoad(const char* path);
char* _colorError();
unsigned char* _getColorBlendTable(int ch);
void _freeColorBlendTable(int a1);
void colorSetBrightness(double value);
bool _initColors();
void _colorsClose();

} // namespace fallout

#endif /* COLOR_H */
