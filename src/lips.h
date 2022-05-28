#ifndef LIPS_H
#define LIPS_H

#include "db.h"
#include "sound.h"

#include <stddef.h>

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
    char field_50[8];
    char field_58[4];
    char field_5C[4];
    char field_60[4];
    char field_64[260];
} LipsData;

extern unsigned char gLipsCurrentPhoneme;
extern unsigned char gLipsPreviousPhoneme;
extern int _head_marker_current;
extern bool gLipsPhonemeChanged;
extern LipsData gLipsData;
extern int _speechStartTime;

extern char _lips_subdir_name[14];
extern char _tmp_str[50];

char* _lips_fix_string(const char* fileName, size_t length);
void lipsTicker();
int lipsStart();
int lipsReadV1(LipsData* a1, File* stream);
int lipsLoad(const char* audioFileName, const char* headFileName);
int _lips_make_speech();
int lipsFree();

#endif /* LIPS_H */
