#ifndef LIPS_H
#define LIPS_H

#include <stddef.h>

#include "sound.h"

namespace fallout {

#define PHONEME_COUNT (42)

typedef enum LipsFlags {
    LIPS_FLAG_0x01 = 0x01,
    LIPS_FLAG_0x02 = 0x02,
} LipsFlags;

typedef struct SpeechMarker {
    int marker;
    int position;
} SpeechMarker;

typedef struct LipsData {
    int version;
    int field_4;
    int flags;
    Sound* sound;
    int field_10;
    void* field_14;
    unsigned char* phonemes;
    int field_1C;
    int field_20;
    int field_24;
    int field_28;
    int field_2C;
    SpeechMarker* markers;
    int field_34;
    int field_38;
    int field_3C;
    int field_40;
    int field_44;
    int field_48;
    int field_4C;
    char file_name[8];
    char field_58[4];
    char field_5C[4];
    char field_60[4];
    char field_64[260];
} LipsData;

extern unsigned char gLipsCurrentPhoneme;
extern bool gLipsPhonemeChanged;
extern LipsData gLipsData;

void lipsTicker();
int lipsStart();
int lipsLoad(const char* audioFileName, const char* headFileName);
int lipsFree();

} // namespace fallout

#endif /* LIPS_H */
