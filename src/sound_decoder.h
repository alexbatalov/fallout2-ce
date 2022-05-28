#ifndef SOUND_DECODER_H
#define SOUND_DECODER_H

#include <stddef.h>

#define SOUND_DECODER_IN_BUFFER_SIZE (512)

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

typedef int (*DECODINGPROC)(SoundDecoder* soundDecoder, int offset, int bits);

extern int gSoundDecodersCount;
extern bool _inited_;
extern DECODINGPROC _ReadBand_tbl[32];
extern unsigned char _pack11_2[128];
extern unsigned char _pack3_3[32];
extern unsigned short word_6ADA00[128];
extern unsigned char* _AudioDecoder_scale0;
extern unsigned char* _AudioDecoder_scale_tbl;

bool soundDecoderPrepare(SoundDecoder* a1, SoundDecoderReadProc* readProc, int fileHandle);
unsigned char soundDecoderReadNextChunk(SoundDecoder* a1);
void _init_pack_tables();

int _ReadBand_Fail_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt0_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt3_16_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt17_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt18_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt19_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt20_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt21_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt22_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt23_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt24_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt26_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt27_(SoundDecoder* soundDecoder, int offset, int bits);
int _ReadBand_Fmt29_(SoundDecoder* soundDecoder, int offset, int bits);

int _ReadBands_(SoundDecoder* ptr);
void _untransform_subband0(unsigned char* a1, unsigned char* a2, int a3, int a4);
void _untransform_subband(unsigned char* a1, unsigned char* a2, int a3, int a4);
void _untransform_all(SoundDecoder* a1);
size_t soundDecoderDecode(SoundDecoder* soundDecoder, void* buffer, size_t size);
void soundDecoderFree(SoundDecoder* soundDecoder);
SoundDecoder* soundDecoderInit(SoundDecoderReadProc* readProc, int fileHandle, int* out_a3, int* out_a4, int* out_a5);

#endif /* SOUND_DECODER_H */
