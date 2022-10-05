#include "mmx.h"

#include <string.h>

#include "svga.h"

namespace fallout {

// Return `true` if CPU supports MMX.
//
// 0x4E08A0
bool mmxIsSupported()
{
    return SDL_HasMMX() == SDL_TRUE;
}

// 0x4E0DB0
void mmxBlit(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height)
{
    if (gMmxEnabled) {
        // TODO: Blit with MMX.
        gMmxEnabled = false;
        mmxBlit(dest, destPitch, src, srcPitch, width, height);
        gMmxEnabled = true;
    } else {
        for (int y = 0; y < height; y++) {
            memcpy(dest, src, width);
            dest += destPitch;
            src += srcPitch;
        }
    }
}

// 0x4E0ED5
void mmxBlitTrans(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height)
{
    if (gMmxEnabled) {
        // TODO: Blit with MMX.
        gMmxEnabled = false;
        mmxBlitTrans(dest, destPitch, src, srcPitch, width, height);
        gMmxEnabled = true;
    } else {
        int destSkip = destPitch - width;
        int srcSkip = srcPitch - width;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                unsigned char c = *src++;
                if (c != 0) {
                    *dest = c;
                }
                dest++;
            }
            src += srcSkip;
            dest += destSkip;
        }
    }
}

} // namespace fallout
