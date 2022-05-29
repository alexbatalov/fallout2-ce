#include "mmx.h"

#include "core.h"

#include <string.h>

// Return `true` if CPU supports MMX.
//
// 0x4E08A0
bool mmxIsSupported()
{
    int v1;

    // TODO: There are other ways to determine MMX using FLAGS register.

#ifdef _WIN32
    __asm
    {
        mov eax, 1
        cpuid
        and edx, 0x800000
        mov v1, edx
    }
#else
    v1 = 0;
#endif

    return v1 != 0;
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
