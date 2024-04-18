#include "color.h"

#include <math.h>
#include <string.h>

#include <algorithm>

#include "svga.h"

namespace fallout {

#define COLOR_PALETTE_STACK_CAPACITY 16

typedef struct ColorPaletteStackEntry {
    unsigned char mappedColors[256];
    unsigned char cmap[768];
    unsigned char colorTable[32768];
} ColorPaletteStackEntry;

static int colorPaletteFileOpen(const char* filePath, int flags);
static int colorPaletteFileRead(int fd, void* buffer, size_t size);
static int colorPaletteFileClose(int fd);
static void* colorPaletteMallocDefaultImpl(size_t size);
static void* colorPaletteReallocDefaultImpl(void* ptr, size_t size);
static void colorPaletteFreeDefaultImpl(void* ptr);
static void _setIntensityTableColor(int a1);
static void _setIntensityTables();
static void _setMixTableColor(int a1);
static void _buildBlendTable(unsigned char* ptr, unsigned char ch);
static void _rebuildColorBlendTables();

// 0x50F930
static char _aColor_cNoError[] = "color.c: No errors\n";

// 0x50F95C
static char _aColor_cColorTa[] = "color.c: color table not found\n";

// 0x50F984
static char _aColor_cColorpa[] = "color.c: colorpalettestack overflow";

// 0x50F9AC
static char aColor_cColor_0[] = "color.c: colorpalettestack underflow";

// 0x51DF10
static char* _errorStr = _aColor_cNoError;

// 0x51DF14
static bool _colorsInited = false;

// 0x51DF18
static double gBrightness = 1.0;

// 0x51DF20
static ColorTransitionCallback* gColorPaletteTransitionCallback = nullptr;

// 0x51DF24
static MallocProc* gColorPaletteMallocProc = colorPaletteMallocDefaultImpl;

// 0x51DF28
static ReallocProc* gColorPaletteReallocProc = colorPaletteReallocDefaultImpl;

// 0x51DF2C
static FreeProc* gColorPaletteFreeProc = colorPaletteFreeDefaultImpl;

// 0x51DF30
static ColorFileNameManger* gColorFileNameMangler = nullptr;

// 0x51DF34
unsigned char _cmap[768] = {
    0x3F, 0x3F, 0x3F
};

// 0x673050
static ColorPaletteStackEntry* gColorPaletteStack[COLOR_PALETTE_STACK_CAPACITY];

// 0x673090
unsigned char _systemCmap[256 * 3];

// 0x673390
unsigned char _currentGammaTable[64];

// 0x6733D0
unsigned char* _blendTable[256];

// 0x6737D0
unsigned char _mappedColor[256];

// 0x6738D0
Color colorMixAddTable[256][256];

// 0x6838D0
Color intensityColorTable[256][256];

// 0x6938D0
Color colorMixMulTable[256][256];

// 0x6A38D0
unsigned char _colorTable[32768];

// 0x6AB8D0
static int gColorPaletteStackSize;

// 0x6AB928
static ColorPaletteFileReadProc* gColorPaletteFileReadProc;

// 0x6AB92C
static ColorPaletteCloseProc* gColorPaletteFileCloseProc;

// 0x6AB930
static ColorPaletteFileOpenProc* gColorPaletteFileOpenProc;

// NOTE: Inlined.
//
// 0x4C7200
static int colorPaletteFileOpen(const char* filePath, int flags)
{
    if (gColorPaletteFileOpenProc != nullptr) {
        return gColorPaletteFileOpenProc(filePath, flags);
    }

    return -1;
}

// NOTE: Inlined.
//
// 0x4C7218
static int colorPaletteFileRead(int fd, void* buffer, size_t size)
{
    if (gColorPaletteFileReadProc != nullptr) {
        return gColorPaletteFileReadProc(fd, buffer, size);
    }

    return -1;
}

// NOTE: Inlined.
//
// 0x4C7230
static int colorPaletteFileClose(int fd)
{
    if (gColorPaletteFileCloseProc != nullptr) {
        return gColorPaletteFileCloseProc(fd);
    }

    return -1;
}

// 0x4C7248
void colorPaletteSetFileIO(ColorPaletteFileOpenProc* openProc, ColorPaletteFileReadProc* readProc, ColorPaletteCloseProc* closeProc)
{
    gColorPaletteFileOpenProc = openProc;
    gColorPaletteFileReadProc = readProc;
    gColorPaletteFileCloseProc = closeProc;
}

// 0x4C725C
static void* colorPaletteMallocDefaultImpl(size_t size)
{
    return malloc(size);
}

// 0x4C7264
static void* colorPaletteReallocDefaultImpl(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

// 0x4C726C
static void colorPaletteFreeDefaultImpl(void* ptr)
{
    free(ptr);
}

// 0x4C72B4
int _calculateColor(int intensity, Color color)
{
    return intensityColorTable[color][intensity / 512];
}

// 0x4C72E0
int Color2RGB(Color c)
{
    int r = _cmap[3 * c] >> 1;
    int g = _cmap[3 * c + 1] >> 1;
    int b = _cmap[3 * c + 2] >> 1;

    return (r << 10) | (g << 5) | b;
}

// Performs animated palette transition.
//
// 0x4C7320
void colorPaletteFadeBetween(unsigned char* oldPalette, unsigned char* newPalette, int steps)
{
    for (int step = 0; step < steps; step++) {
        sharedFpsLimiter.mark();

        unsigned char palette[768];

        for (int index = 0; index < 768; index++) {
            palette[index] = oldPalette[index] - (oldPalette[index] - newPalette[index]) * step / steps;
        }

        if (gColorPaletteTransitionCallback != nullptr) {
            if (step % 128 == 0) {
                gColorPaletteTransitionCallback();
            }
        }

        _setSystemPalette(palette);
        renderPresent();
        sharedFpsLimiter.throttle();
    }

    sharedFpsLimiter.mark();
    _setSystemPalette(newPalette);
    renderPresent();
    sharedFpsLimiter.throttle();
}

// 0x4C73D4
void colorPaletteSetTransitionCallback(ColorTransitionCallback* callback)
{
    gColorPaletteTransitionCallback = callback;
}

// 0x4C73E4
void _setSystemPalette(unsigned char* palette)
{
    unsigned char newPalette[768];

    for (int index = 0; index < 768; index++) {
        newPalette[index] = _currentGammaTable[palette[index]];
        _systemCmap[index] = palette[index];
    }

    directDrawSetPalette(newPalette);
}

// 0x4C7420
unsigned char* _getSystemPalette()
{
    return _systemCmap;
}

// 0x4C7428
void _setSystemPaletteEntries(unsigned char* palette, int start, int end)
{
    unsigned char newPalette[768];

    int length = end - start + 1;
    for (int index = 0; index < length; index++) {
        newPalette[index * 3] = _currentGammaTable[palette[index * 3]];
        newPalette[index * 3 + 1] = _currentGammaTable[palette[index * 3 + 1]];
        newPalette[index * 3 + 2] = _currentGammaTable[palette[index * 3 + 2]];

        _systemCmap[start * 3 + index * 3] = palette[index * 3];
        _systemCmap[start * 3 + index * 3 + 1] = palette[index * 3 + 1];
        _systemCmap[start * 3 + index * 3 + 2] = palette[index * 3 + 2];
    }

    directDrawSetPaletteInRange(newPalette, start, end - start + 1);
}

// 0x4C7550
static void _setIntensityTableColor(int cc)
{
    int shift = 0;

    for (int index = 0; index < 128; index++) {
        int r = (Color2RGB(cc) & 0x7C00) >> 10;
        int g = (Color2RGB(cc) & 0x3E0) >> 5;
        int b = (Color2RGB(cc) & 0x1F);

        int darkerR = ((r * shift) >> 16);
        int darkerG = ((g * shift) >> 16);
        int darkerB = ((b * shift) >> 16);
        int darkerColor = (darkerR << 10) | (darkerG << 5) | darkerB;
        intensityColorTable[cc][index] = _colorTable[darkerColor];

        int lighterR = r + (((0x1F - r) * shift) >> 16);
        int lighterG = g + (((0x1F - g) * shift) >> 16);
        int lighterB = b + (((0x1F - b) * shift) >> 16);
        int lighterColor = (lighterR << 10) | (lighterG << 5) | lighterB;
        intensityColorTable[cc][128 + index] = _colorTable[lighterColor];

        shift += 512;
    }
}

// 0x4C7658
static void _setIntensityTables()
{
    for (int index = 0; index < 256; index++) {
        if (_mappedColor[index] != 0) {
            _setIntensityTableColor(index);
        } else {
            memset(intensityColorTable[index], 0, 256);
        }
    }
}

// 0x4C769C
static void _setMixTableColor(int a1)
{
    int i;
    int v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19;
    int v20, v21, v22, v23, v24, v25, v26, v27, v28, v29;

    for (i = 0; i < 256; i++) {
        if (_mappedColor[a1] && _mappedColor[i]) {
            v2 = (Color2RGB(a1) & 0x7C00) >> 10;
            v3 = (Color2RGB(a1) & 0x3E0) >> 5;
            v4 = (Color2RGB(a1) & 0x1F);

            v5 = (Color2RGB(i) & 0x7C00) >> 10;
            v6 = (Color2RGB(i) & 0x3E0) >> 5;
            v7 = (Color2RGB(i) & 0x1F);

            v8 = v2 + v5;
            v9 = v3 + v6;
            v10 = v4 + v7;

            v11 = v8;

            if (v9 > v11) {
                v11 = v9;
            }

            if (v10 > v11) {
                v11 = v10;
            }

            if (v11 <= 0x1F) {
                int paletteIndex = (v8 << 10) | (v9 << 5) | v10;
                v12 = _colorTable[paletteIndex];
            } else {
                v13 = v11 - 0x1F;

                v14 = v8 - v13;
                v15 = v9 - v13;
                v16 = v10 - v13;

                if (v14 < 0) {
                    v14 = 0;
                }

                if (v15 < 0) {
                    v15 = 0;
                }

                if (v16 < 0) {
                    v16 = 0;
                }

                v17 = (v14 << 10) | (v15 << 5) | v16;
                v18 = _colorTable[v17];

                v19 = (int)((((double)v11 + (-31.0)) * 0.0078125 + 1.0) * 65536.0);
                v12 = _calculateColor(v19, v18);
            }

            colorMixAddTable[a1][i] = v12;

            v20 = (Color2RGB(a1) & 0x7C00) >> 10;
            v21 = (Color2RGB(a1) & 0x3E0) >> 5;
            v22 = (Color2RGB(a1) & 0x1F);

            v23 = (Color2RGB(i) & 0x7C00) >> 10;
            v24 = (Color2RGB(i) & 0x3E0) >> 5;
            v25 = (Color2RGB(i) & 0x1F);

            v26 = (v20 * v23) >> 5;
            v27 = (v21 * v24) >> 5;
            v28 = (v22 * v25) >> 5;

            v29 = (v26 << 10) | (v27 << 5) | v28;
            colorMixMulTable[a1][i] = _colorTable[v29];
        } else {
            if (_mappedColor[i]) {
                colorMixAddTable[a1][i] = i;
                colorMixMulTable[a1][i] = i;
            } else {
                colorMixAddTable[a1][i] = a1;
                colorMixMulTable[a1][i] = a1;
            }
        }
    }
}

// 0x4C78E4
bool colorPaletteLoad(const char* path)
{
    if (gColorFileNameMangler != nullptr) {
        path = gColorFileNameMangler(path);
    }

    // NOTE: Uninline.
    int fd = colorPaletteFileOpen(path, 0x200);
    if (fd == -1) {
        _errorStr = _aColor_cColorTa;
        return false;
    }

    for (int index = 0; index < 256; index++) {
        unsigned char r;
        unsigned char g;
        unsigned char b;

        // NOTE: Uninline.
        colorPaletteFileRead(fd, &r, sizeof(r));

        // NOTE: Uninline.
        colorPaletteFileRead(fd, &g, sizeof(g));

        // NOTE: Uninline.
        colorPaletteFileRead(fd, &b, sizeof(b));

        if (r <= 0x3F && g <= 0x3F && b <= 0x3F) {
            _mappedColor[index] = 1;
        } else {
            r = 0;
            g = 0;
            b = 0;
            _mappedColor[index] = 0;
        }

        _cmap[index * 3] = r;
        _cmap[index * 3 + 1] = g;
        _cmap[index * 3 + 2] = b;
    }

    // NOTE: Uninline.
    colorPaletteFileRead(fd, _colorTable, 0x8000);

    unsigned int type;
    // NOTE: Uninline.
    colorPaletteFileRead(fd, &type, sizeof(type));

    // NOTE: The value is "NEWC". Original code uses cmp opcode, not stricmp,
    // or comparing characters one-by-one.
    if (type == 'NEWC') {
        // NOTE: Uninline.
        colorPaletteFileRead(fd, intensityColorTable, sizeof(intensityColorTable));

        // NOTE: Uninline.
        colorPaletteFileRead(fd, colorMixAddTable, sizeof(colorMixAddTable));

        // NOTE: Uninline.
        colorPaletteFileRead(fd, colorMixMulTable, sizeof(colorMixMulTable));
    } else {
        _setIntensityTables();

        for (int index = 0; index < 256; index++) {
            _setMixTableColor(index);
        }
    }

    _rebuildColorBlendTables();

    // NOTE: Uninline.
    colorPaletteFileClose(fd);

    return true;
}

// 0x4C7AB4
char* _colorError()
{
    return _errorStr;
}

// 0x4C7B44
static void _buildBlendTable(unsigned char* ptr, unsigned char ch)
{
    int r, g, b;
    int i, j;
    int v12, v14, v16;
    unsigned char* beg;

    beg = ptr;

    r = (Color2RGB(ch) & 0x7C00) >> 10;
    g = (Color2RGB(ch) & 0x3E0) >> 5;
    b = (Color2RGB(ch) & 0x1F);

    for (i = 0; i < 256; i++) {
        ptr[i] = i;
    }

    ptr += 256;

    int b_1 = b;
    int v31 = 6;
    int g_1 = g;
    int r_1 = r;

    int b_2 = b_1;
    int g_2 = g_1;
    int r_2 = r_1;

    for (j = 0; j < 7; j++) {
        for (i = 0; i < 256; i++) {
            v12 = (Color2RGB(i) & 0x7C00) >> 10;
            v14 = (Color2RGB(i) & 0x3E0) >> 5;
            v16 = (Color2RGB(i) & 0x1F);
            int index = 0;
            index |= (r_2 + v12 * v31) / 7 << 10;
            index |= (g_2 + v14 * v31) / 7 << 5;
            index |= (b_2 + v16 * v31) / 7;
            ptr[i] = _colorTable[index];
        }
        v31--;
        ptr += 256;
        r_2 += r_1;
        g_2 += g_1;
        b_2 += b_1;
    }

    int v18 = 0;
    for (j = 0; j < 6; j++) {
        int v20 = v18 / 7 + 0xFFFF;

        for (i = 0; i < 256; i++) {
            ptr[i] = _calculateColor(v20, ch);
        }

        v18 += 0x10000;
        ptr += 256;
    }
}

// 0x4C7D90
static void _rebuildColorBlendTables()
{
    int i;

    for (i = 0; i < 256; i++) {
        if (_blendTable[i]) {
            _buildBlendTable(_blendTable[i], i);
        }
    }
}

// 0x4C7DC0
unsigned char* _getColorBlendTable(int ch)
{
    unsigned char* ptr;

    if (_blendTable[ch] == nullptr) {
        ptr = (unsigned char*)gColorPaletteMallocProc(4100);
        *(int*)ptr = 1;
        _blendTable[ch] = ptr + 4;
        _buildBlendTable(_blendTable[ch], ch);
    }

    ptr = _blendTable[ch];
    *(int*)((unsigned char*)ptr - 4) = *(int*)((unsigned char*)ptr - 4) + 1;

    return ptr;
}

// 0x4C7E20
void _freeColorBlendTable(int a1)
{
    unsigned char* v2 = _blendTable[a1];
    if (v2 != nullptr) {
        int* count = (int*)(v2 - sizeof(int));
        *count -= 1;
        if (*count == 0) {
            gColorPaletteFreeProc(count);
            _blendTable[a1] = nullptr;
        }
    }
}

// 0x4C7E58
void colorPaletteSetMemoryProcs(MallocProc* mallocProc, ReallocProc* reallocProc, FreeProc* freeProc)
{
    gColorPaletteMallocProc = mallocProc;
    gColorPaletteReallocProc = reallocProc;
    gColorPaletteFreeProc = freeProc;
}

// 0x4C7E6C
void colorSetBrightness(double value)
{
    gBrightness = value;

    for (int i = 0; i < 64; i++) {
        double value = pow(i, gBrightness);
        _currentGammaTable[i] = (unsigned char)std::clamp(value, 0.0, 63.0);
    }

    _setSystemPalette(_systemCmap);
}

// NOTE: Unused.
//
// 0x4C8828
bool colorPushColorPalette()
{
    if (gColorPaletteStackSize >= COLOR_PALETTE_STACK_CAPACITY) {
        _errorStr = _aColor_cColorpa;
        return false;
    }

    ColorPaletteStackEntry* entry = (ColorPaletteStackEntry*)malloc(sizeof(*entry));
    gColorPaletteStack[gColorPaletteStackSize] = entry;

    memcpy(entry->mappedColors, _mappedColor, sizeof(_mappedColor));
    memcpy(entry->cmap, _cmap, sizeof(_cmap));
    memcpy(entry->colorTable, _colorTable, sizeof(_colorTable));

    gColorPaletteStackSize++;

    return true;
}

// NOTE: Unused.
//
// 0x4C88E0
bool colorPopColorPalette()
{
    if (gColorPaletteStackSize == 0) {
        _errorStr = aColor_cColor_0;
        return false;
    }

    gColorPaletteStackSize--;

    ColorPaletteStackEntry* entry = gColorPaletteStack[gColorPaletteStackSize];

    memcpy(_mappedColor, entry->mappedColors, sizeof(_mappedColor));
    memcpy(_cmap, entry->cmap, sizeof(_cmap));
    memcpy(_colorTable, entry->colorTable, sizeof(_colorTable));

    free(entry);
    gColorPaletteStack[gColorPaletteStackSize] = nullptr;

    _setIntensityTables();

    for (int index = 0; index < 256; index++) {
        _setMixTableColor(index);
    }

    _rebuildColorBlendTables();

    return true;
}

// 0x4C89CC
bool _initColors()
{
    if (_colorsInited) {
        return true;
    }

    _colorsInited = true;

    colorSetBrightness(1.0);

    if (!colorPaletteLoad("color.pal")) {
        return false;
    }

    _setSystemPalette(_cmap);

    return true;
}

// 0x4C8A18
void _colorsClose()
{
    for (int index = 0; index < 256; index++) {
        _freeColorBlendTable(index);
    }

    for (int index = 0; index < gColorPaletteStackSize; index++) {
        free(gColorPaletteStack[index]);
    }

    gColorPaletteStackSize = 0;
}

} // namespace fallout
