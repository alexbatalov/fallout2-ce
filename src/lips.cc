#include "lips.h"

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "db.h"
#include "debug.h"
#include "game_sound.h"
#include "input.h"
#include "memory.h"
#include "platform_compat.h"
#include "sound.h"
#include "svga.h"

namespace fallout {

static char* _lips_fix_string(const char* fileName, size_t length);
static int lipsReadV1(LipsData* a1, File* stream);
static int _lips_make_speech();

// 0x519240
unsigned char gLipsCurrentPhoneme = 0;

// 0x519241
static unsigned char gLipsPreviousPhoneme = 0;

// 0x519244
static int _head_marker_current = 0;

// 0x519248
bool gLipsPhonemeChanged = true;

// 0x51924C
LipsData gLipsData = {
    2,
    22528,
    0,
    nullptr,
    -1,
    nullptr,
    nullptr,
    0,
    0,
    0,
    0,
    0,
    nullptr,
    0,
    0,
    50,
    100,
    0,
    0,
    0,
    "TEST",
    "VOC",
    "TXT",
    "LIP",
};

// 0x5193B4
static int _speechStartTime = 0;

// 0x613CA0
static char _lips_subdir_name[14];

// 0x613CAE
static char _tmp_str[50];

// 0x47AAC0
static char* _lips_fix_string(const char* fileName, size_t length)
{
    strncpy(_tmp_str, fileName, length);
    return _tmp_str;
}

// 0x47AAD8
void lipsTicker()
{
    int v0;
    SpeechMarker* speech_marker;
    int v5;

    v0 = _head_marker_current;

    if ((gLipsData.flags & LIPS_FLAG_0x02) != 0) {
        int v1 = _soundGetPosition(gLipsData.sound);

        speech_marker = &(gLipsData.markers[v0]);
        while (v1 > speech_marker->position) {
            gLipsCurrentPhoneme = gLipsData.phonemes[v0];
            v0++;

            if (v0 >= gLipsData.field_2C) {
                v0 = 0;
                gLipsCurrentPhoneme = gLipsData.phonemes[0];

                if ((gLipsData.flags & LIPS_FLAG_0x01) == 0) {
                    _head_marker_current = 0;
                    soundStop(gLipsData.sound);
                    v0 = _head_marker_current;
                    gLipsData.flags &= ~(LIPS_FLAG_0x01 | LIPS_FLAG_0x02);
                }

                break;
            }

            speech_marker = &(gLipsData.markers[v0]);
        }

        if (v0 >= gLipsData.field_2C - 1) {
            _head_marker_current = v0;

            v5 = 0;
            if (gLipsData.field_2C <= 5) {
                debugPrint("Error: Too few markers to stop speech!");
            } else {
                v5 = 3;
            }

            speech_marker = &(gLipsData.markers[v5]);
            if (v1 < speech_marker->position) {
                v0 = 0;
                gLipsCurrentPhoneme = gLipsData.phonemes[0];

                if ((gLipsData.flags & LIPS_FLAG_0x01) == 0) {
                    _head_marker_current = 0;
                    soundStop(gLipsData.sound);
                    v0 = _head_marker_current;
                    gLipsData.flags &= ~(LIPS_FLAG_0x01 | LIPS_FLAG_0x02);
                }
            }
        }
    }

    if (gLipsPreviousPhoneme != gLipsCurrentPhoneme) {
        gLipsPreviousPhoneme = gLipsCurrentPhoneme;
        gLipsPhonemeChanged = true;
    }

    _head_marker_current = v0;

    soundContinueAll();
}

// 0x47AC2C
int lipsStart()
{
    gLipsData.flags |= LIPS_FLAG_0x02;
    _head_marker_current = 0;

    if (_soundSetPosition(gLipsData.sound, gLipsData.field_20) != 0) {
        debugPrint("Failed set of start_offset!\n");
    }

    int v2 = _head_marker_current;
    while (1) {
        _head_marker_current = v2;

        SpeechMarker* speechEntry = &(gLipsData.markers[v2]);
        if (gLipsData.field_20 <= speechEntry->position) {
            break;
        }

        v2++;

        gLipsCurrentPhoneme = gLipsData.phonemes[v2];
    }

    int speechVolume = speechGetVolume();
    soundSetVolume(gLipsData.sound, (int)(speechVolume * 0.69));

    _speechStartTime = getTicks();

    if (soundPlay(gLipsData.sound) != 0) {
        debugPrint("Failed play!\n");
        _head_marker_current = 0;

        soundStop(gLipsData.sound);
        gLipsData.flags |= ~(LIPS_FLAG_0x01 | LIPS_FLAG_0x02);
    }

    return 0;
}

// 0x47AD98
static int lipsReadV1(LipsData* lipsData, File* stream)
{
    int sound;
    int field_14;
    int phonemes;
    int markers;

    if (fileReadInt32(stream, &(lipsData->version)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_4)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->flags)) == -1) return -1;
    if (fileReadInt32(stream, &(sound)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_10)) == -1) return -1;
    if (fileReadInt32(stream, &(field_14)) == -1) return -1;
    if (fileReadInt32(stream, &(phonemes)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_1C)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_20)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_24)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_28)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_2C)) == -1) return -1;
    if (fileReadInt32(stream, &(markers)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_34)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_38)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_3C)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_40)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_44)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_48)) == -1) return -1;
    if (fileReadInt32(stream, &(lipsData->field_4C)) == -1) return -1;
    if (fileReadFixedLengthString(stream, lipsData->field_50, 8) == -1) return -1;
    if (fileReadFixedLengthString(stream, lipsData->field_58, 4) == -1) return -1;
    if (fileReadFixedLengthString(stream, lipsData->field_5C, 4) == -1) return -1;
    if (fileReadFixedLengthString(stream, lipsData->field_60, 4) == -1) return -1;
    if (fileReadFixedLengthString(stream, lipsData->field_64, 260) == -1) return -1;

    // NOTE: Original code is different. For unknown reason it assigns values
    // from file (integers) and treat them as pointers, which is obviously wrong
    // is in this case.
    lipsData->sound = nullptr;
    lipsData->field_14 = nullptr;
    lipsData->phonemes = nullptr;
    lipsData->markers = nullptr;

    return 0;
}

// lips_load_file
// 0x47AFAC
int lipsLoad(const char* audioFileName, const char* headFileName)
{
    char* sep;
    int i;
    char v60[16];

    SpeechMarker* speech_marker;
    SpeechMarker* prev_speech_marker;

    char path[260];
    strcpy(path, "SOUND\\SPEECH\\");

    strcpy(_lips_subdir_name, headFileName);

    strcat(path, _lips_subdir_name);

    strcat(path, "\\");

    sep = strchr(path, '.');
    if (sep != nullptr) {
        *sep = '\0';
    }

    strcpy(v60, audioFileName);

    sep = strchr(v60, '.');
    if (sep != nullptr) {
        *sep = '\0';
    }

    strncpy(gLipsData.field_50, v60, 8);

    strcat(path, _lips_fix_string(gLipsData.field_50, sizeof(gLipsData.field_50)));
    strcat(path, ".");
    strcat(path, gLipsData.field_60);

    lipsFree();

    // FIXME: stream is not closed if any error is encountered during reading.
    File* stream = fileOpen(path, "rb");
    if (stream != nullptr) {
        if (fileReadInt32(stream, &(gLipsData.version)) == -1) {
            return -1;
        }

        if (gLipsData.version == 1) {
            debugPrint("\nLoading old save-file version (1)");

            if (fileSeek(stream, 0, SEEK_SET) != 0) {
                return -1;
            }

            if (lipsReadV1(&gLipsData, stream) != 0) {
                return -1;
            }
        } else if (gLipsData.version == 2) {
            debugPrint("\nLoading current save-file version (2)");

            if (fileReadInt32(stream, &(gLipsData.field_4)) == -1) return -1;
            if (fileReadInt32(stream, &(gLipsData.flags)) == -1) return -1;
            if (fileReadInt32(stream, &(gLipsData.field_10)) == -1) return -1;
            if (fileReadInt32(stream, &(gLipsData.field_1C)) == -1) return -1;
            if (fileReadInt32(stream, &(gLipsData.field_24)) == -1) return -1;
            if (fileReadInt32(stream, &(gLipsData.field_28)) == -1) return -1;
            if (fileReadInt32(stream, &(gLipsData.field_2C)) == -1) return -1;
            if (fileReadFixedLengthString(stream, gLipsData.field_50, 8) == -1) return -1;
            if (fileReadFixedLengthString(stream, gLipsData.field_58, 4) == -1) return -1;
        } else {
            debugPrint("\nError: Lips file WRONG version: %s!", path);
        }
    }

    gLipsData.phonemes = (unsigned char*)internal_malloc(gLipsData.field_24);
    if (gLipsData.phonemes == nullptr) {
        debugPrint("Out of memory in lips_load_file.'\n");
        return -1;
    }

    if (stream != nullptr) {
        for (i = 0; i < gLipsData.field_24; i++) {
            if (fileReadUInt8(stream, &(gLipsData.phonemes[i])) == -1) {
                debugPrint("lips_load_file: Error reading phoneme type.\n");
                return -1;
            }
        }

        for (i = 0; i < gLipsData.field_24; i++) {
            unsigned char phoneme = gLipsData.phonemes[i];
            if (phoneme >= PHONEME_COUNT) {
                debugPrint("\nLoad error: Speech phoneme %d is invalid (%d)!", i, phoneme);
            }
        }
    }

    gLipsData.markers = (SpeechMarker*)internal_malloc(sizeof(*speech_marker) * gLipsData.field_2C);
    if (gLipsData.markers == nullptr) {
        debugPrint("Out of memory in lips_load_file.'\n");
        return -1;
    }

    if (stream != nullptr) {
        for (i = 0; i < gLipsData.field_2C; i++) {
            speech_marker = &(gLipsData.markers[i]);

            if (fileReadInt32(stream, &(speech_marker->marker)) == -1) break;
            if (fileReadInt32(stream, &(speech_marker->position)) == -1) break;
        }

        if (i != gLipsData.field_2C) {
            debugPrint("lips_load_file: Error reading marker type.");
            return -1;
        }

        speech_marker = &(gLipsData.markers[0]);

        if (speech_marker->marker != 1 && speech_marker->marker != 0) {
            debugPrint("\nLoad error: Speech marker 0 is invalid (%d)!", speech_marker->marker);
        }

        if (speech_marker->position != 0) {
            debugPrint("Load error: Speech marker 0 has invalid position(%d)!", speech_marker->position);
        }

        for (i = 1; i < gLipsData.field_2C; i++) {
            speech_marker = &(gLipsData.markers[i]);
            prev_speech_marker = &(gLipsData.markers[i - 1]);

            if (speech_marker->marker != 1 && speech_marker->marker != 0) {
                debugPrint("\nLoad error: Speech marker %d is invalid (%d)!", i, speech_marker->marker);
            }

            if (speech_marker->position < prev_speech_marker->position) {
                debugPrint("Load error: Speech marker %d has invalid position(%d)!", i, speech_marker->position);
            }
        }
    }

    if (stream != nullptr) {
        fileClose(stream);
    }

    gLipsData.field_38 = 0;
    gLipsData.field_34 = 0;
    gLipsData.field_48 = 0;
    gLipsData.field_20 = 0;
    gLipsData.field_3C = 50;
    gLipsData.field_40 = 100;

    if (gLipsData.version == 1) {
        gLipsData.field_4 = 22528;
    }

    strcpy(gLipsData.field_58, "ACM");
    strcpy(gLipsData.field_5C, "TXT");
    strcpy(gLipsData.field_60, "LIP");

    _lips_make_speech();

    _head_marker_current = 0;
    gLipsCurrentPhoneme = gLipsData.phonemes[0];

    return 0;
}

// lips_make_speech
// 0x47B5D0
static int _lips_make_speech()
{
    if (gLipsData.field_14 != nullptr) {
        internal_free(gLipsData.field_14);
        gLipsData.field_14 = nullptr;
    }

    char path[COMPAT_MAX_PATH];
    char* v1 = _lips_fix_string(gLipsData.field_50, sizeof(gLipsData.field_50));
    snprintf(path, sizeof(path), "%s%s\\%s.%s", "SOUND\\SPEECH\\", _lips_subdir_name, v1, "ACM");

    if (gLipsData.sound != nullptr) {
        soundDelete(gLipsData.sound);
        gLipsData.sound = nullptr;
    }

    gLipsData.sound = soundAllocate(SOUND_TYPE_MEMORY, SOUND_16BIT);
    if (gLipsData.sound == nullptr) {
        debugPrint("\nsoundAllocate falied in lips_make_speech!");
        return -1;
    }

    if (soundSetFileIO(gLipsData.sound, audioOpen, audioClose, audioRead, nullptr, audioSeek, nullptr, audioGetSize)) {
        debugPrint("Ack!");
        debugPrint("Error!");
    }

    if (soundLoad(gLipsData.sound, path)) {
        soundDelete(gLipsData.sound);
        gLipsData.sound = nullptr;

        debugPrint("lips_make_speech: soundLoad failed with path ");
        debugPrint("%s -- file probably doesn't exist.\n", path);
        return -1;
    }

    gLipsData.field_34 = 8 * (gLipsData.field_1C / gLipsData.field_2C);

    return 0;
}

// 0x47B730
int lipsFree()
{
    if (gLipsData.field_14 != nullptr) {
        internal_free(gLipsData.field_14);
        gLipsData.field_14 = nullptr;
    }

    if (gLipsData.sound != nullptr) {
        _head_marker_current = 0;

        soundStop(gLipsData.sound);

        gLipsData.flags &= ~0x03;

        soundDelete(gLipsData.sound);

        gLipsData.sound = nullptr;
    }

    if (gLipsData.phonemes != nullptr) {
        internal_free(gLipsData.phonemes);
        gLipsData.phonemes = nullptr;
    }

    if (gLipsData.markers != nullptr) {
        internal_free(gLipsData.markers);
        gLipsData.markers = nullptr;
    }

    return 0;
}

} // namespace fallout
