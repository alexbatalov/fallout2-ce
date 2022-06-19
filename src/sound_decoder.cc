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

#define SOUND_DECODER_IN_BUFFER_SIZE (512)

typedef int (*DECODINGPROC)(SoundDecoder* soundDecoder, int offset, int bits);

static bool soundDecoderPrepare(SoundDecoder* a1, SoundDecoderReadProc* readProc, int fileHandle);
static unsigned char soundDecoderReadNextChunk(SoundDecoder* a1);
static void _init_pack_tables();
static int _ReadBand_Fail_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt0_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt3_16_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt17_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt18_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt19_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt20_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt21_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt22_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt23_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt24_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt26_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt27_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBand_Fmt29_(SoundDecoder* soundDecoder, int offset, int bits);
static int _ReadBands_(SoundDecoder* ptr);
static void _untransform_subband0(unsigned char* a1, unsigned char* a2, int a3, int a4);
static void _untransform_subband(unsigned char* a1, unsigned char* a2, int a3, int a4);
static void _untransform_all(SoundDecoder* a1);

static inline void soundDecoderRequireBits(SoundDecoder* soundDecoder, int bits);
static inline void soundDecoderDropBits(SoundDecoder* soundDecoder, int bits);

// 0x51E328
static int gSoundDecodersCount = 0;

// 0x51E32C
static bool _inited_ = false;

// 0x51E330
static DECODINGPROC _ReadBand_tbl[32] = {
    _ReadBand_Fmt0_,
    _ReadBand_Fail_,
    _ReadBand_Fail_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt3_16_,
    _ReadBand_Fmt17_,
    _ReadBand_Fmt18_,
    _ReadBand_Fmt19_,
    _ReadBand_Fmt20_,
    _ReadBand_Fmt21_,
    _ReadBand_Fmt22_,
    _ReadBand_Fmt23_,
    _ReadBand_Fmt24_,
    _ReadBand_Fail_,
    _ReadBand_Fmt26_,
    _ReadBand_Fmt27_,
    _ReadBand_Fail_,
    _ReadBand_Fmt29_,
    _ReadBand_Fail_,
    _ReadBand_Fail_,
};

// 0x6AD960
static unsigned char _pack11_2[128];

// 0x6AD9E0
static unsigned char _pack3_3[32];

// 0x6ADA00
static unsigned short word_6ADA00[128];

// 0x6ADB00
static unsigned char* _AudioDecoder_scale0;

// 0x6ADB04
static unsigned char* _AudioDecoder_scale_tbl;

// 0x4D3BB0
static bool soundDecoderPrepare(SoundDecoder* soundDecoder, SoundDecoderReadProc* readProc, int fileHandle)
{
    soundDecoder->readProc = readProc;
    soundDecoder->fd = fileHandle;

    soundDecoder->bufferIn = (unsigned char*)malloc(SOUND_DECODER_IN_BUFFER_SIZE);
    if (soundDecoder->bufferIn == NULL) {
        return false;
    }

    soundDecoder->bufferInSize = SOUND_DECODER_IN_BUFFER_SIZE;
    soundDecoder->remainingInSize = 0;

    return true;
}

// 0x4D3BE0
static unsigned char soundDecoderReadNextChunk(SoundDecoder* soundDecoder)
{
    soundDecoder->remainingInSize = soundDecoder->readProc(soundDecoder->fd, soundDecoder->bufferIn, soundDecoder->bufferInSize);
    if (soundDecoder->remainingInSize == 0) {
        memset(soundDecoder->bufferIn, 0, soundDecoder->bufferInSize);
        soundDecoder->remainingInSize = soundDecoder->bufferInSize;
    }

    soundDecoder->nextIn = soundDecoder->bufferIn;
    soundDecoder->remainingInSize -= 1;
    return *soundDecoder->nextIn++;
}

// 0x4D3C78
static void _init_pack_tables()
{
    int i;
    int j;
    int m;

    if (_inited_) {
        return;
    }

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            for (m = 0; m < 3; m++) {
                _pack3_3[i + j * 3 + m * 9] = i + j * 4 + m * 16;
            }
        }
    }

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            for (m = 0; m < 5; m++) {
                word_6ADA00[i + j * 5 + m * 25] = i + j * 8 + m * 64;
            }
        }
    }

    for (i = 0; i < 11; i++) {
        for (j = 0; j < 11; j++) {
            _pack11_2[i + j * 11] = i + j * 16;
        }
    }

    _inited_ = true;
}

// 0x4D3D9C
static int _ReadBand_Fail_(SoundDecoder* soundDecoder, int offset, int bits)
{
    return 0;
}

// 0x4D3DA0
static int _ReadBand_Fmt0_(SoundDecoder* soundDecoder, int offset, int bits)
{
    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        *p = 0;
        p += soundDecoder->field_24;
        i--;
    }

    return 1;
}

// 0x4D3DC8
static int _ReadBand_Fmt3_16_(SoundDecoder* soundDecoder, int offset, int bits)
{
    int value;
    int v14;

    short* base = (short*)_AudioDecoder_scale0;
    base += UINT_MAX << (bits - 1);

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    v14 = (1 << bits) - 1;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, bits);
        value = soundDecoder->hold;
        soundDecoderDropBits(soundDecoder, bits);

        *p = base[v14 & value];
        p += soundDecoder->field_24;

        i--;
    }

    return 1;
}

// 0x4D3E90
static int _ReadBand_Fmt17_(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 3);

        int value = soundDecoder->hold & 0xFF;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->field_24;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += soundDecoder->field_24;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            soundDecoderDropBits(soundDecoder, 2);

            *p = 0;
            p += soundDecoder->field_24;

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

            p += soundDecoder->field_24;
            i--;
        }
    }
    return 1;
}

// 0x4D3F98
static int _ReadBand_Fmt18_(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 2);

        int value = soundDecoder->hold;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->field_24;

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

            p += soundDecoder->field_24;
            i--;
        }
    }
    return 1;
}

// 0x4D4068
static int _ReadBand_Fmt19_(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;
    base -= 1;

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 5);
        int value = soundDecoder->hold & 0x1F;
        soundDecoderDropBits(soundDecoder, 5);

        value = _pack3_3[value];

        *p = base[value & 0x03];
        p += soundDecoder->field_24;
        if (--i == 0) {
            break;
        }

        *p = base[(value >> 2) & 0x03];
        p += soundDecoder->field_24;
        if (--i == 0) {
            break;
        }

        *p = base[value >> 4];
        p += soundDecoder->field_24;

        i--;
    }

    return 1;
}

// 0x4D4158
static int _ReadBand_Fmt20_(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 4);

        int value = soundDecoder->hold & 0xFF;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->field_24;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += soundDecoder->field_24;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            soundDecoderDropBits(soundDecoder, 2);

            *p = 0;
            p += soundDecoder->field_24;

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

            p += soundDecoder->field_24;
            i--;
        }
    }

    return 1;
}

// 0x4D4254
static int _ReadBand_Fmt21_(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 3);

        int value = soundDecoder->hold & 0xFF;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->field_24;

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

            p += soundDecoder->field_24;
            i--;
        }
    }

    return 1;
}

// 0x4D4338
static int _ReadBand_Fmt22_(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;
    base -= 2;

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 7);
        int value = soundDecoder->hold & 0x7F;
        soundDecoderDropBits(soundDecoder, 7);

        value = word_6ADA00[value];

        *p = base[value & 7];
        p += soundDecoder->field_24;

        if (--i == 0) {
            break;
        }

        *p = base[((value >> 3) & 7)];
        p += soundDecoder->field_24;

        if (--i == 0) {
            break;
        }

        *p = base[value >> 6];
        p += soundDecoder->field_24;

        if (--i == 0) {
            break;
        }
    }

    return 1;
}

// 0x4D4434
static int _ReadBand_Fmt23_(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 5);

        int value = soundDecoder->hold;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->field_24;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += soundDecoder->field_24;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            soundDecoderDropBits(soundDecoder, 2);

            *p = 0;
            p += soundDecoder->field_24;

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

            p += soundDecoder->field_24;
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
            p += soundDecoder->field_24;
            i--;
        }
    }

    return 1;
}

// 0x4D4584
static int _ReadBand_Fmt24_(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 4);

        int value = soundDecoder->hold & 0xFF;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->field_24;

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

            p += soundDecoder->field_24;

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
            p += soundDecoder->field_24;
            i--;
        }
    }

    return 1;
}

// 0x4D4698
static int _ReadBand_Fmt26_(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 5);

        int value = soundDecoder->hold;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->field_24;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += soundDecoder->field_24;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            soundDecoderDropBits(soundDecoder, 2);

            *p = 0;
            p += soundDecoder->field_24;

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
            p += soundDecoder->field_24;
            i--;
        }
    }

    return 1;
}

// 0x4D47A4
static int _ReadBand_Fmt27_(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 4);

        int value = soundDecoder->hold;
        if (!(value & 0x01)) {
            soundDecoderDropBits(soundDecoder, 1);

            *p = 0;
            p += soundDecoder->field_24;

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
            p += soundDecoder->field_24;
            i--;
        }
    }

    return 1;
}

// 0x4D4870
static int _ReadBand_Fmt29_(SoundDecoder* soundDecoder, int offset, int bits)
{
    short* base = (short*)_AudioDecoder_scale0;

    int* p = (int*)soundDecoder->field_34;
    p += offset;

    int i = soundDecoder->field_28;
    while (i != 0) {
        soundDecoderRequireBits(soundDecoder, 7);
        int value = soundDecoder->hold & 0x7F;
        soundDecoderDropBits(soundDecoder, 7);

        value = _pack11_2[value];

        *p = base[(value & 0x0F) - 5];
        p += soundDecoder->field_24;
        if (--i == 0) {
            break;
        }

        *p = base[(value >> 4) - 5];
        p += soundDecoder->field_24;
        if (--i == 0) {
            break;
        }
    }

    return 1;
}

// 0x4D493C
static int _ReadBands_(SoundDecoder* soundDecoder)
{
    int v9;
    int v15;
    int v17;
    int v19;
    unsigned short* v18;
    int v21;
    DECODINGPROC fn;

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

    _init_pack_tables();

    for (int index = 0; index < soundDecoder->field_24; index++) {
        soundDecoderRequireBits(soundDecoder, 5);
        int bits = soundDecoder->hold & 0x1F;
        soundDecoderDropBits(soundDecoder, 5);

        fn = _ReadBand_tbl[bits];
        if (!fn(soundDecoder, index, bits)) {
            return 0;
        }
    }
    return 1;
}

// 0x4D4ADC
static void _untransform_subband0(unsigned char* a1, unsigned char* a2, int a3, int a4)
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
static void _untransform_subband(unsigned char* a1, unsigned char* a2, int a3, int a4)
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
static void _untransform_all(SoundDecoder* soundDecoder)
{
    int v8;
    unsigned char* ptr;
    int v3;
    int v4;
    unsigned char* j;
    int v6;
    int* v5;

    if (!soundDecoder->field_20) {
        return;
    }

    ptr = soundDecoder->field_34;

    v8 = soundDecoder->field_28;
    while (v8 > 0) {
        v3 = soundDecoder->field_24 >> 1;
        v4 = soundDecoder->field_38;
        if (v4 > v8) {
            v4 = v8;
        }

        v4 *= 2;

        _untransform_subband0(soundDecoder->field_30, ptr, v3, v4);

        v5 = (int*)ptr;
        for (v6 = 0; v6 < v4; v6++) {
            *v5 += 1;
            v5 += v3;
        }

        j = 4 * v3 + soundDecoder->field_30;
        while (1) {
            v3 >>= 1;
            v4 *= 2;
            if (v3 == 0) {
                break;
            }
            _untransform_subband(j, ptr, v3, v4);
            j += 8 * v3;
        }

        ptr += soundDecoder->field_3C * 4;
        v8 -= soundDecoder->field_38;
    }
}

// 0x4D4FA0
size_t soundDecoderDecode(SoundDecoder* soundDecoder, void* buffer, size_t size)
{
    unsigned char* dest;
    unsigned char* v5;
    int v6;
    int v4;

    dest = (unsigned char*)buffer;
    v4 = 0;
    v5 = soundDecoder->field_4C;
    v6 = soundDecoder->field_50;

    size_t bytesRead;
    for (bytesRead = 0; bytesRead < size; bytesRead += 2) {
        if (!v6) {
            if (!soundDecoder->field_48) {
                break;
            }

            if (!_ReadBands_(soundDecoder)) {
                break;
            }

            _untransform_all(soundDecoder);

            soundDecoder->field_48 -= soundDecoder->field_2C;
            soundDecoder->field_4C = soundDecoder->field_34;
            soundDecoder->field_50 = soundDecoder->field_2C;

            if (soundDecoder->field_48 < 0) {
                soundDecoder->field_50 += soundDecoder->field_48;
                soundDecoder->field_48 = 0;
            }

            v5 = soundDecoder->field_4C;
            v6 = soundDecoder->field_50;
        }

        int v13 = *(int*)v5;
        v5 += 4;
        *(unsigned short*)(dest + bytesRead) = (v13 >> soundDecoder->field_20) & 0xFFFF;
        v6--;
    }

    soundDecoder->field_4C = v5;
    soundDecoder->field_50 = v6;

    return bytesRead;
}

// 0x4D5048
void soundDecoderFree(SoundDecoder* soundDecoder)
{
    if (soundDecoder->bufferIn != NULL) {
        free(soundDecoder->bufferIn);
    }

    if (soundDecoder->field_30 != NULL) {
        free(soundDecoder->field_30);
    }

    if (soundDecoder->field_34 != NULL) {
        free(soundDecoder->field_34);
    }

    free(soundDecoder);

    gSoundDecodersCount--;

    if (gSoundDecodersCount == 0) {
        if (_AudioDecoder_scale_tbl != NULL) {
            free(_AudioDecoder_scale_tbl);
            _AudioDecoder_scale_tbl = NULL;
        }
    }
}

// 0x4D50A8
SoundDecoder* soundDecoderInit(SoundDecoderReadProc* readProc, int fileHandle, int* out_a3, int* out_a4, int* out_a5)
{
    int v14;
    int v20;
    int v73;

    SoundDecoder* soundDecoder = (SoundDecoder*)malloc(sizeof(*soundDecoder));
    if (soundDecoder == NULL) {
        return NULL;
    }

    memset(soundDecoder, 0, sizeof(*soundDecoder));

    gSoundDecodersCount++;

    if (!soundDecoderPrepare(soundDecoder, readProc, fileHandle)) {
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
    soundDecoder->field_48 = soundDecoder->hold & 0xFFFF;
    soundDecoderDropBits(soundDecoder, 16);

    soundDecoderRequireBits(soundDecoder, 16);
    soundDecoder->field_48 |= (soundDecoder->hold & 0xFFFF) << 16;
    soundDecoderDropBits(soundDecoder, 16);

    soundDecoderRequireBits(soundDecoder, 16);
    soundDecoder->field_40 = soundDecoder->hold & 0xFFFF;
    soundDecoderDropBits(soundDecoder, 16);

    soundDecoderRequireBits(soundDecoder, 16);
    soundDecoder->field_44 = soundDecoder->hold & 0xFFFF;
    soundDecoderDropBits(soundDecoder, 16);

    soundDecoderRequireBits(soundDecoder, 4);
    soundDecoder->field_20 = soundDecoder->hold & 0x0F;
    soundDecoderDropBits(soundDecoder, 4);

    soundDecoderRequireBits(soundDecoder, 12);
    soundDecoder->field_24 = 1 << soundDecoder->field_20;
    soundDecoder->field_28 = soundDecoder->hold & 0x0FFF;
    soundDecoder->field_2C = soundDecoder->field_28 * soundDecoder->field_24;
    soundDecoderDropBits(soundDecoder, 12);

    if (soundDecoder->field_20 != 0) {
        v73 = 3 * soundDecoder->field_24 / 2 - 2;
    } else {
        v73 = 0;
    }

    soundDecoder->field_38 = 2048 / soundDecoder->field_24 - 2;
    if (soundDecoder->field_38 < 1) {
        soundDecoder->field_38 = 1;
    }

    soundDecoder->field_3C = soundDecoder->field_38 * soundDecoder->field_24;

    if (v73 != 0) {
        soundDecoder->field_30 = (unsigned char*)malloc(sizeof(unsigned char*) * v73);
        if (soundDecoder->field_30 == NULL) {
            goto L66;
        }

        memset(soundDecoder->field_30, 0, sizeof(unsigned char*) * v73);
    }

    soundDecoder->field_34 = (unsigned char*)malloc(sizeof(unsigned char*) * soundDecoder->field_2C);
    if (soundDecoder->field_34 == NULL) {
        goto L66;
    }

    soundDecoder->field_50 = 0;

    if (gSoundDecodersCount == 1) {
        _AudioDecoder_scale_tbl = (unsigned char*)malloc(0x20000);
        _AudioDecoder_scale0 = _AudioDecoder_scale_tbl + 0x10000;
    }

    *out_a3 = soundDecoder->field_40;
    *out_a4 = soundDecoder->field_44;
    *out_a5 = soundDecoder->field_48;

    return soundDecoder;

L66:

    soundDecoderFree(soundDecoder);

    *out_a3 = 0;
    *out_a4 = 0;
    *out_a5 = 0;

    return 0;
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
