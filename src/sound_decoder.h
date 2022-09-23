#ifndef SOUND_DECODER_H
#define SOUND_DECODER_H

#include <stddef.h>

namespace fallout {

typedef int(SoundDecoderReadProc)(int fileHandle, void* buffer, unsigned int size);

typedef struct SoundDecoder {
    SoundDecoderReadProc* readProc;
    int fd;
    unsigned char* bufferIn;
    size_t bufferInSize;

    // Next input byte.
    unsigned char* nextIn;

    // Number of bytes remaining in the input buffer.
    int remainingInSize;

    // Bit accumulator.
    int hold;

    // Number of bits in bit accumulator.
    int bits;
    int field_20;
    int field_24;
    int field_28;
    int field_2C;
    unsigned char* field_30;
    unsigned char* field_34;
    int field_38;
    int field_3C;
    int field_40;
    int field_44;
    int field_48;
    unsigned char* field_4C;
    int field_50;
} SoundDecoder;

size_t soundDecoderDecode(SoundDecoder* soundDecoder, void* buffer, size_t size);
void soundDecoderFree(SoundDecoder* soundDecoder);
SoundDecoder* soundDecoderInit(SoundDecoderReadProc* readProc, int fileHandle, int* out_a3, int* out_a4, int* out_a5);

} // namespace fallout

#endif /* SOUND_DECODER_H */
