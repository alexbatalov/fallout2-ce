#include "grayscale.h"

#include <algorithm>

#include "color.h"

namespace fallout {

// 0x596D90
static unsigned char _GreyTable[256];

// 0x44FA78
void grayscalePaletteUpdate(int a1, int a2)
{
    if (a1 >= 0 && a2 <= 255) {
        for (int index = a1; index <= a2; index++) {
            // NOTE: The only way to explain so much calls to [_Color2RGB_] with
            // the same repeated pattern is by the use of min/max macros.

            int v1 = std::max((_Color2RGB_(index) & 0x7C00) >> 10, std::max((_Color2RGB_(index) & 0x3E0) >> 5, _Color2RGB_(index) & 0x1F));
            int v2 = std::min((_Color2RGB_(index) & 0x7C00) >> 10, std::min((_Color2RGB_(index) & 0x3E0) >> 5, _Color2RGB_(index) & 0x1F));
            int v3 = v1 + v2;
            int v4 = (int)((double)v3 * 240.0 / 510.0);

            int paletteIndex = ((v4 & 0xFF) << 10) | ((v4 & 0xFF) << 5) | (v4 & 0xFF);
            _GreyTable[index] = _colorTable[paletteIndex];
        }
    }
}

// 0x44FC40
void grayscalePaletteApply(unsigned char* buffer, int width, int height, int pitch)
{
    unsigned char* ptr = buffer;
    int skip = pitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char c = *ptr;
            *ptr++ = _GreyTable[c];
        }
        ptr += skip;
    }
}

} // namespace fallout
