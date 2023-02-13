#ifndef SOUND_DECODER_H
#define SOUND_DECODER_H

#include <stddef.h>

namespace fallout {

typedef int(SoundDecoderReadProc)(void* data, void* buffer, unsigned int size);

typedef struct SoundDecoder {
    SoundDecoderReadProc* readProc;
    void* data;
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
    int levels;
    int subbands;
    int samples_per_subband;
    int total_samples;
    unsigned char* prev_samples;
    unsigned char* samples;
    int block_samples_per_subband;
    int block_total_samples;
    int channels;
    int rate;
    int file_cnt;
    unsigned char* samp_ptr;
    int samp_cnt;
} SoundDecoder;

size_t soundDecoderDecode(SoundDecoder* soundDecoder, void* buffer, size_t size);
void soundDecoderFree(SoundDecoder* soundDecoder);
SoundDecoder* soundDecoderInit(SoundDecoderReadProc* readProc, void* data, int* channelsPtr, int* sampleRatePtr, int* sampleCountPtr);

} // namespace fallout

#endif /* SOUND_DECODER_H */
