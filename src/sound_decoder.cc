// NOTE: Functions in these module are somewhat different from what can be seen
// in IDA because of two new helper functions that deal with incoming bits. I
// bet something like these were implemented via function-like macro in the
// same manner zlib deals with bits. The pattern is so common in this module so
// I made an exception and extracted it into separate functions to increase
// readability.

#include "sound_decoder.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

namespace fallout {

#define SOUND_DECODER_IN_BUFFER_SIZE (512)

typedef int (*ReadBandFunc)(SoundDecoder* soundDecoder, int offset, int bits);

static bool soundDecoderPrepare(SoundDecoder* soundDecoder, SoundDecoderReadProc* readProc, void* data);
static unsigned char soundDecoderReadNextChunk(SoundDecoder* soundDecoder);
static void init_pack_tables();
static int ReadBand_Fail(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt0(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt3_16(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt17(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt18(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt19(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt20(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt21(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt22(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt23(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt24(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt26(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt27(SoundDecoder* soundDecoder, int offset, int bits);
static int ReadBand_Fmt29(SoundDecoder* soundDecoder, int offset, int bits);
static bool ReadBands(SoundDecoder* soundDecoder);
static void untransform_subband0(unsigned char* a1, unsigned char* a2, int a3, int a4);
static void untransform_subband(unsigned char* a1, unsigned char* a2, int a3, int a4);
static void untransform_all(SoundDecoder* soundDecoder);
static bool soundDecoderFill(SoundDecoder* soundDecoder);

static inline void soundDecoderRequireBits(SoundDecoder* soundDecoder, int bits);
static inline void soundDecoderDropBits(SoundDecoder* soundDecoder, int bits);
static int ReadBand_Fmt31(SoundDecoder* soundDecoder, int offset, int bits);

// 0x51E328
static int gSoundDecodersCount = 0;

// 0x51E330
static ReadBandFunc _ReadBand_tbl[32] = {
    ReadBand_Fmt0,
    ReadBand_Fail,
    ReadBand_Fail,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt17,
    ReadBand_Fmt18,
    ReadBand_Fmt19,
    ReadBand_Fmt20,
    ReadBand_Fmt21,
    ReadBand_Fmt22,
    ReadBand_Fmt23,
    ReadBand_Fmt24,
    ReadBand_Fail,
    ReadBand_Fmt26,
    ReadBand_Fmt27,
    ReadBand_Fail,
    ReadBand_Fmt29,
    ReadBand_Fail,
    ReadBand_Fmt31,
};

// 0x6AD960
static unsigned char pack11_2[128];

// 0x6AD9E0
static unsigned char pack3_3[32];

// 0x6ADA00
static unsigned short pack5_3[128];

// 0x6ADB00
static unsigned char* _AudioDecoder_scale0;

// 0x6ADB04
static unsigned char* _AudioDecoder_scale_tbl;

// 0x4D3BB0
static bool soundDecoderPrepare(SoundDecoder* soundDecoder, SoundDecoderReadProc* readProc, void* data)
{
    soundDecoder->readProc = readProc;
    soundDecoder->data = data;

    soundDecoder->bufferIn = (unsigned char*)malloc(SOUND_DECODER_IN_BUFFER_SIZE);
    if (soundDecoder->bufferIn == nullptr) {
        return false;
    }

    soundDecoder->bufferInSize = SOUND_DECODER_IN_BUFFER_SIZE;
    soundDecoder->remainingInSize = 0;

    return true;
}

// 0x4D3BE0
static unsigned char soundDecoderReadNextChunk(SoundDecoder* soundDecoder)
{
    soundDecoder->remainingInSize = soundDecoder->readProc(soundDecoder->data, soundDecoder->bufferIn, soundDecoder->bufferInSize);
    if (soundDecoder->remainingInSize == 0) {
        memset(soundDecoder->bufferIn, 0, soundDecoder->bufferInSize);
        soundDecoder->remainingInSize = soundDecoder->bufferInSize;
    }

    soundDecoder->nextIn = soundDecoder->bufferIn;
    soundDecoder->remainingInSize -= 1;
    return *soundDecoder->nextIn++;
}

// 0x4D3C78
static void init_pack_tables()
{
    // 0x51E32C
    static bool inited = false;

    int i;
    int j;
    int m;

    if (inited) {
        return;
    }

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            for (m = 0; m < 3; m++) {
                pack3_3[i + j * 3 + m * 9] = i + j * 4 + m * 16;
            }
        }
    }

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            for (m = 0; m < 5; m++) {
                pack5_3[i + j * 5 + m * 25] = i + j * 8 + m * 64;
            }
        }
    }

    for (i = 0; i < 11; i++) {
        for (j = 0; j < 11; j++) {
            pack11_2[i + j * 11] = i + j * 16;
        }
    }

    inited = true;
}

// 0x4D3D9C
static int ReadBand_Fail(SoundDecoder* soundDecoder, int offset, int bits)
{
    return 0;
}

// 0x4D3DA0
static int ReadBand_Fmt0(SoundDecoder* soundDecoder, int offset, int bits)
{
    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        *p = 0;
        p += soundDecoder->subbands;
        i--;
    }

    return 1;
}

// 0x4D3DC8
static int ReadBand_Fmt3_16(SoundDecoder* soundDecoder, int offset, int bits)
{
    int value;
    int v14;

    short* base = (short*)_AudioDecoder_scale0;
    base += (int)(UINT_MAX << (bits - 1));

    int* p = (int*)soundDecoder->samples;
    p += offset;

    v14 = (1 << bits) - 1;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, bits);
        value = soundDecoder->hold;
        soundDecoderDropBits(soundDecoder, bits);

        *p = base[v14 & value];
        p += soundDecoder->subbands;

        i--;
    }

    return 1;
}

// 0x4D3E90
static int ReadBand_Fmt17(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 3);

        int value = soundDecoder->hold & 0xFF;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            soundDecoderDropBits(soundDecoder, 2);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            soundDecoderDropBits(soundDecoder, 3);

            if (value & 0x04) {
                *p = base[1];
            } else {
                *p = base[-1];
            }

            p += soundDecoder->subbands;
            i--;
        }
    }
    return 1;
}

// 0x4D3F98
static int ReadBand_Fmt18(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 2);

        int value = soundDecoder->hold;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                return 1;
            }
        } else {
            soundDecoderDropBits(soundDecoder, 2);

            if (value & 0x02) {
                *p = base[1];
            } else {
                *p = base[-1];
            }

            p += soundDecoder->subbands;
            i--;
        }
    }
    return 1;
}

// 0x4D4068
static int ReadBand_Fmt19(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;
    base -= 1;

    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 5);
        int value = soundDecoder->hold & 0x1F;
        soundDecoderDropBits(soundDecoder, 5);

        value = pack3_3[value];

        *p = base[value & 0x03];
        p += soundDecoder->subbands;
        if (--i == 0) {
            break;
        }

        *p = base[(value >> 2) & 0x03];
        p += soundDecoder->subbands;
        if (--i == 0) {
            break;
        }

        *p = base[value >> 4];
        p += soundDecoder->subbands;

        i--;
    }

    return 1;
}

// 0x4D4158
static int ReadBand_Fmt20(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 4);

        int value = soundDecoder->hold & 0xFF;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            soundDecoderDropBits(soundDecoder, 2);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            soundDecoderDropBits(soundDecoder, 4);

            if (value & 0x08) {
                if (value & 0x04) {
                    *p = base[2];
                } else {
                    *p = base[1];
                }
            } else {
                if (value & 0x04) {
                    *p = base[-1];
                } else {
                    *p = base[-2];
                }
            }

            p += soundDecoder->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4D4254
static int ReadBand_Fmt21(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 3);

        int value = soundDecoder->hold & 0xFF;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            soundDecoderDropBits(soundDecoder, 3);

            if (value & 0x04) {
                if (value & 0x02) {
                    *p = base[2];
                } else {
                    *p = base[1];
                }
            } else {
                if (value & 0x02) {
                    *p = base[-1];
                } else {
                    *p = base[-2];
                }
            }

            p += soundDecoder->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4D4338
static int ReadBand_Fmt22(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;
    base -= 2;

    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 7);
        int value = soundDecoder->hold & 0x7F;
        soundDecoderDropBits(soundDecoder, 7);

        value = pack5_3[value];

        *p = base[value & 7];
        p += soundDecoder->subbands;

        if (--i == 0) {
            break;
        }

        *p = base[((value >> 3) & 7)];
        p += soundDecoder->subbands;

        if (--i == 0) {
            break;
        }

        *p = base[value >> 6];
        p += soundDecoder->subbands;

        if (--i == 0) {
            break;
        }
    }

    return 1;
}

// 0x4D4434
static int ReadBand_Fmt23(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 5);

        int value = soundDecoder->hold;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            soundDecoderDropBits(soundDecoder, 2);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x04)) {
            soundDecoderDropBits(soundDecoder, 4);

            if (value & 0x08) {
                *p = base[1];
            } else {
                *p = base[-1];
            }

            p += soundDecoder->subbands;
            if (--i == 0) {
                break;
            }
        } else {
            soundDecoderDropBits(soundDecoder, 5);

            value >>= 3;
            value &= 0x03;
            if (value >= 2) {
                value += 3;
            }

            *p = base[value - 3];
            p += soundDecoder->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4D4584
static int ReadBand_Fmt24(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 4);

        int value = soundDecoder->hold & 0xFF;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            soundDecoderDropBits(soundDecoder, 3);

            if (value & 0x04) {
                *p = base[1];
            } else {
                *p = base[-1];
            }

            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            soundDecoderDropBits(soundDecoder, 4);

            value >>= 2;
            value &= 0x03;
            if (value >= 2) {
                value += 3;
            }

            *p = base[value - 3];
            p += soundDecoder->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4D4698
static int ReadBand_Fmt26(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 5);

        int value = soundDecoder->hold;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            soundDecoderDropBits(soundDecoder, 2);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            soundDecoderDropBits(soundDecoder, 5);

            value >>= 2;
            value &= 0x07;
            if (value >= 4) {
                value += 1;
            }

            *p = base[value - 4];
            p += soundDecoder->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4D47A4
static int ReadBand_Fmt27(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 4);

        int value = soundDecoder->hold;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            soundDecoderDropBits(soundDecoder, 4);

            value >>= 1;
            value &= 0x07;
            if (value >= 4) {
                value += 1;
            }

            *p = base[value - 4];
            p += soundDecoder->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4D4870
static int ReadBand_Fmt29(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->samples;
    p += offset;

    int i = soundDecoder->samples_per_subband;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 7);
        int value = soundDecoder->hold & 0x7F;
        soundDecoderDropBits(soundDecoder, 7);

        value = pack11_2[value];

        *p = base[(value & 0x0F) - 5];
        p += soundDecoder->subbands;
        if (--i == 0) {
            break;
        }

        *p = base[(value >> 4) - 5];
        p += soundDecoder->subbands;
        if (--i == 0) {
            break;
        }
    }

    return 1;
}

// 0x4D493C
static bool ReadBands(SoundDecoder* soundDecoder)
{
    int v9;
    int v15;
    int v17;
    int v19;
    unsigned short* v18;
    int v21;
    ReadBandFunc fn;

    soundDecoderRequireBits(soundDecoder, 4);
    v9 = soundDecoder->hold & 0xF;
    soundDecoderDropBits(soundDecoder, 4);

    soundDecoderRequireBits(soundDecoder, 16);
    v15 = soundDecoder->hold & 0xFFFF;
    soundDecoderDropBits(soundDecoder, 16);

    v17 = 1 << v9;

    v18 = (unsigned short*)_AudioDecoder_scale0;
    v19 = v17;
    v21 = 0;
    while (v19--) {
        *v18++ = v21;
        v21 += v15;
    }

    v18 = (unsigned short*)_AudioDecoder_scale0;
    v19 = v17;
    v21 = -v15;
    while (v19--) {
        v18--;
        *v18 = v21;
        v21 -= v15;
    }

    init_pack_tables();

    for (int index = 0; index < soundDecoder->subbands; index++) {
        soundDecoderRequireBits(soundDecoder, 5);
        int bits = soundDecoder->hold & 0x1F;
        soundDecoderDropBits(soundDecoder, 5);

        fn = _ReadBand_tbl[bits];
        if (!fn(soundDecoder, index, bits)) {
            return false;
        }
    }
    return true;
}

// 0x4D4ADC
static void untransform_subband0(unsigned char* a1, unsigned char* a2, int a3, int a4)
{
    short* p;

    p = (short*)a2;
    p += a3;

    if (a4 == 2) {
        int i = a3;
        while (i != 0) {
            i--;
        }
    } else if (a4 == 4) {
        int v31 = a3;
        int* v9 = (int*)a2;
        v9 += a3;

        int* v10 = (int*)a2;
        v10 += a3 * 3;

        int* v11 = (int*)a2;
        v11 += a3 * 2;

        while (v31 != 0) {
            int* v33 = (int*)a2;
            int* v34 = (int*)a1;

            int v12 = *v34 >> 16;

            int v13 = *v33;
            *v33 = (int)(*(short*)v34) + 2 * v12 + v13;

            int v14 = *v9;
            *v9 = 2 * v13 - v12 - v14;

            int v15 = *v11;
            *v11 = 2 * v14 + v15 + v13;

            int v16 = *v10;
            *v10 = 2 * v15 - v14 - v16;

            v10++;
            v11++;
            v9++;

            *(short*)a1 = v15 & 0xFFFF;
            *(short*)(a1 + 2) = v16 & 0xFFFF;

            a1 += 4;
            a2 += 4;

            v31--;
        }
    } else {
        int v30 = a4 >> 1;
        int v32 = a3;
        while (v32 != 0) {
            int* v19 = (int*)a2;

            int v20;
            int v22;
            if (v30 & 0x01) {

            } else {
                v20 = (int)*(short*)a1;
                v22 = *(int*)a1 >> 16;
            }

            int v23 = v30 >> 1;
            while (--v23 != -1) {
                int v24 = *v19;
                *v19 += 2 * v22 + v20;
                v19 += a3;

                int v26 = *v19;
                *v19 = 2 * v24 - v22 - v26;
                v19 += a3;

                v20 = *v19;
                *v19 += 2 * v26 + v24;
                v19 += a3;

                v22 = *v19;
                *v19 = 2 * v20 - v26 - v22;
                v19 += a3;
            }

            *(short*)a1 = v20 & 0xFFFF;
            *(short*)(a1 + 2) = v22 & 0xFFFF;

            a1 += 4;
            a2 += 4;
            v32--;
        }
    }
}

// 0x4D4D1C
static void untransform_subband(unsigned char* a1, unsigned char* a2, int a3, int a4)
{
    int v13;
    int* v14;
    int* v25;
    int* v26;
    int v15;
    int v16;
    int v17;

    int* v18;
    int v19;
    int* v20;
    int* v21;

    v26 = (int*)a1;
    v25 = (int*)a2;

    if (a4 == 4) {
        unsigned char* v4 = a2 + 4 * a3;
        unsigned char* v5 = a2 + 3 * a3;
        unsigned char* v6 = a2 + 2 * a3;
        int v7;
        int v8;
        int v9;
        int v10;
        int v11;
        while (a3--) {
            v7 = *(unsigned int*)(v26 + 4);
            v8 = *(unsigned int*)v25;
            *(unsigned int*)v25 = *(unsigned int*)v26 + 2 * v7;

            v9 = *(unsigned int*)v4;
            *(unsigned int*)v4 = 2 * v8 - v7 - v9;

            v10 = *(unsigned int*)v6;
            v5 += 4;
            *v6 += 2 * v9 + v8;

            v11 = *(unsigned int*)(v5 - 4);
            v6 += 4;

            *(unsigned int*)(v5 - 4) = 2 * v10 - v9 - v11;
            v4 += 4;

            *(unsigned int*)v26 = v10;
            *(unsigned int*)(v26 + 4) = v11;

            v26 += 2;
            v25 += 1;
        }
    } else {
        int v24 = a3;

        while (v24 != 0) {
            v13 = a4 >> 2;
            v14 = v25;
            v15 = v26[0];
            v16 = v26[1];

            while (--v13 != -1) {
                v17 = *v14;
                *v14 += 2 * v16 + v15;

                v18 = v14 + a3;
                v19 = *v18;
                *v18 = 2 * v17 - v16 - v19;

                v20 = v18 + a3;
                v15 = *v20;
                *v20 += 2 * v19 + v17;

                v21 = v20 + a3;
                v16 = *v21;
                *v21 = 2 * v15 - v19 - v16;

                v14 = v21 + a3;
            }

            v26[0] = v15;
            v26[1] = v16;

            v26 += 2;
            v25 += 1;

            v24--;
        }
    }
}

// 0x4D4E80
static void untransform_all(SoundDecoder* soundDecoder)
{
    int v8;
    unsigned char* ptr;
    int v3;
    int v4;
    unsigned char* j;
    int v6;
    int* v5;

    if (!soundDecoder->levels) {
        return;
    }

    ptr = soundDecoder->samples;

    v8 = soundDecoder->samples_per_subband;
    while (v8 > 0) {
        v3 = soundDecoder->subbands >> 1;
        v4 = soundDecoder->block_samples_per_subband;
        if (v4 > v8) {
            v4 = v8;
        }

        v4 *= 2;

        untransform_subband0(soundDecoder->prev_samples, ptr, v3, v4);

        v5 = (int*)ptr;
        for (v6 = 0; v6 < v4; v6++) {
            *v5 += 1;
            v5 += v3;
        }

        j = 4 * v3 + soundDecoder->prev_samples;
        while (1) {
            v3 >>= 1;
            v4 *= 2;
            if (v3 == 0) {
                break;
            }
            untransform_subband(j, ptr, v3, v4);
            j += 8 * v3;
        }

        ptr += soundDecoder->block_total_samples * 4;
        v8 -= soundDecoder->block_samples_per_subband;
    }
}

// NOTE: Inlined.
//
// 0x4D4F58
static bool soundDecoderFill(SoundDecoder* soundDecoder)
{
    // CE: Implementation is slightly different. `ReadBands` now handles new
    // Fmt31 used in some Russian localizations. The appropriate handler acts as
    // both decoder and transformer, so there is no need to untransform bands
    // once again. This approach assumes band 31 is never used by standard acms
    // and mods.
    if (ReadBands(soundDecoder)) {
        untransform_all(soundDecoder);
    }

    soundDecoder->file_cnt -= soundDecoder->total_samples;
    soundDecoder->samp_ptr = soundDecoder->samples;
    soundDecoder->samp_cnt = soundDecoder->total_samples;

    if (soundDecoder->file_cnt < 0) {
        soundDecoder->samp_cnt += soundDecoder->file_cnt;
        soundDecoder->file_cnt = 0;
    }

    return true;
}

// 0x4D4FA0
size_t soundDecoderDecode(SoundDecoder* soundDecoder, void* buffer, size_t size)
{
    unsigned char* dest;
    unsigned char* samp_ptr;
    int samp_cnt;

    dest = (unsigned char*)buffer;
    samp_ptr = soundDecoder->samp_ptr;
    samp_cnt = soundDecoder->samp_cnt;

    size_t bytesRead;
    for (bytesRead = 0; bytesRead < size; bytesRead += 2) {
        if (samp_cnt == 0) {
            if (soundDecoder->file_cnt == 0) {
                break;
            }

            // NOTE: Uninline.
            if (!soundDecoderFill(soundDecoder)) {
                break;
            }

            samp_ptr = soundDecoder->samp_ptr;
            samp_cnt = soundDecoder->samp_cnt;
        }

        int sample = *(int*)samp_ptr;
        samp_ptr += 4;
        *(unsigned short*)(dest + bytesRead) = (sample >> soundDecoder->levels) & 0xFFFF;
        samp_cnt--;
    }

    soundDecoder->samp_ptr = samp_ptr;
    soundDecoder->samp_cnt = samp_cnt;

    return bytesRead;
}

// 0x4D5048
void soundDecoderFree(SoundDecoder* soundDecoder)
{
    if (soundDecoder->bufferIn != nullptr) {
        free(soundDecoder->bufferIn);
    }

    if (soundDecoder->prev_samples != nullptr) {
        free(soundDecoder->prev_samples);
    }

    if (soundDecoder->samples != nullptr) {
        free(soundDecoder->samples);
    }

    free(soundDecoder);

    gSoundDecodersCount--;

    if (gSoundDecodersCount == 0) {
        if (_AudioDecoder_scale_tbl != nullptr) {
            free(_AudioDecoder_scale_tbl);
            _AudioDecoder_scale_tbl = nullptr;
        }
    }
}

// 0x4D50A8
SoundDecoder* soundDecoderInit(SoundDecoderReadProc* readProc, void* data, int* channelsPtr, int* sampleRatePtr, int* sampleCountPtr)
{
    int v14;
    int v20;
    int v73;

    SoundDecoder* soundDecoder = (SoundDecoder*)malloc(sizeof(*soundDecoder));
    if (soundDecoder == nullptr) {
        return nullptr;
    }

    memset(soundDecoder, 0, sizeof(*soundDecoder));

    gSoundDecodersCount++;

    if (!soundDecoderPrepare(soundDecoder, readProc, data)) {
        goto L66;
    }

    soundDecoder->hold = 0;
    soundDecoder->bits = 0;

    soundDecoderRequireBits(soundDecoder, 24);
    v14 = soundDecoder->hold;
    soundDecoderDropBits(soundDecoder, 24);

    if ((v14 & 0xFFFFFF) != 0x32897) {
        goto L66;
    }

    soundDecoderRequireBits(soundDecoder, 8);
    v20 = soundDecoder->hold;
    soundDecoderDropBits(soundDecoder, 8);

    if (v20 != 1) {
        goto L66;
    }

    soundDecoderRequireBits(soundDecoder, 16);
    soundDecoder->file_cnt = soundDecoder->hold & 0xFFFF;
    soundDecoderDropBits(soundDecoder, 16);

    soundDecoderRequireBits(soundDecoder, 16);
    soundDecoder->file_cnt |= (soundDecoder->hold & 0xFFFF) << 16;
    soundDecoderDropBits(soundDecoder, 16);

    soundDecoderRequireBits(soundDecoder, 16);
    soundDecoder->channels = soundDecoder->hold & 0xFFFF;
    soundDecoderDropBits(soundDecoder, 16);

    soundDecoderRequireBits(soundDecoder, 16);
    soundDecoder->rate = soundDecoder->hold & 0xFFFF;
    soundDecoderDropBits(soundDecoder, 16);

    soundDecoderRequireBits(soundDecoder, 4);
    soundDecoder->levels = soundDecoder->hold & 0x0F;
    soundDecoderDropBits(soundDecoder, 4);

    soundDecoderRequireBits(soundDecoder, 12);
    soundDecoder->subbands = 1 << soundDecoder->levels;
    soundDecoder->samples_per_subband = soundDecoder->hold & 0x0FFF;
    soundDecoder->total_samples = soundDecoder->samples_per_subband * soundDecoder->subbands;
    soundDecoderDropBits(soundDecoder, 12);

    if (soundDecoder->levels != 0) {
        v73 = 3 * soundDecoder->subbands / 2 - 2;
    } else {
        v73 = 0;
    }

    soundDecoder->block_samples_per_subband = 2048 / soundDecoder->subbands - 2;
    if (soundDecoder->block_samples_per_subband < 1) {
        soundDecoder->block_samples_per_subband = 1;
    }

    soundDecoder->block_total_samples = soundDecoder->block_samples_per_subband * soundDecoder->subbands;

    if (v73 != 0) {
        soundDecoder->prev_samples = (unsigned char*)malloc(sizeof(unsigned char*) * v73);
        if (soundDecoder->prev_samples == nullptr) {
            goto L66;
        }

        memset(soundDecoder->prev_samples, 0, sizeof(unsigned char*) * v73);
    }

    soundDecoder->samples = (unsigned char*)malloc(sizeof(unsigned char*) * soundDecoder->total_samples);
    if (soundDecoder->samples == nullptr) {
        goto L66;
    }

    soundDecoder->samp_cnt = 0;

    if (gSoundDecodersCount == 1) {
        _AudioDecoder_scale_tbl = (unsigned char*)malloc(0x20000);
        _AudioDecoder_scale0 = _AudioDecoder_scale_tbl + 0x10000;
    }

    *channelsPtr = soundDecoder->channels;
    *sampleRatePtr = soundDecoder->rate;
    *sampleCountPtr = soundDecoder->file_cnt;

    return soundDecoder;

L66:

    soundDecoderFree(soundDecoder);

    *channelsPtr = 0;
    *sampleRatePtr = 0;
    *sampleCountPtr = 0;

    return nullptr;
}

static inline void soundDecoderRequireBits(SoundDecoder* soundDecoder, int bits)
{
    while (soundDecoder->bits < bits) {
        soundDecoder->remainingInSize--;

        unsigned char ch;
        if (soundDecoder->remainingInSize < 0) {
            ch = soundDecoderReadNextChunk(soundDecoder);
        } else {
            ch = *soundDecoder->nextIn++;
        }
        soundDecoder->hold |= ch << soundDecoder->bits;
        soundDecoder->bits += 8;
    }
}

static inline void soundDecoderDropBits(SoundDecoder* soundDecoder, int bits)
{
    soundDecoder->hold >>= bits;
    soundDecoder->bits -= bits;
}

static int ReadBand_Fmt31(SoundDecoder* soundDecoder, int offset, int bits)
{
    int* samples = (int*)soundDecoder->samples;

    int remaining_samples = soundDecoder->total_samples;
    while (remaining_samples != 0) {
        soundDecoderRequireBits(soundDecoder, 16);
        int value = soundDecoder->hold & 0xFFFF;
        soundDecoderDropBits(soundDecoder, 16);

        *samples++ = (value << 16) >> (16 - soundDecoder->levels);

        remaining_samples--;
    }

    return 0;
}

} // namespace fallout
