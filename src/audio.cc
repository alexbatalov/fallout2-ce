#include "audio.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "db.h"
#include "debug.h"
#include "memory_manager.h"
#include "sound.h"
#include "sound_decoder.h"

namespace fallout {

typedef enum AudioFlags {
    AUDIO_IN_USE = 0x01,
    AUDIO_COMPRESSED = 0x02,
} AudioFileFlags;

typedef struct Audio {
    int flags;
    File* stream;
    SoundDecoder* soundDecoder;
    int fileSize;
    int sampleRate;
    int channels;
    int position;
} Audio;

static bool defaultCompressionFunc(char* filePath);
static int audioSoundDecoderReadHandler(void* data, void* buf, unsigned int size);

// 0x5108BC
static AudioQueryCompressedFunc* queryCompressedFunc = defaultCompressionFunc;

// 0x56CB00
static int gAudioListLength;

// 0x56CB04
static Audio* gAudioList;

// 0x41A2B0
static bool defaultCompressionFunc(char* filePath)
{
    char* pch = strrchr(filePath, '.');
    if (pch != nullptr) {
        strcpy(pch + 1, "raw");
    }

    return false;
}

// 0x41A2D0
static int audioSoundDecoderReadHandler(void* data, void* buffer, unsigned int size)
{
    return fileRead(buffer, 1, size, reinterpret_cast<File*>(data));
}

// AudioOpen
// 0x41A2EC
int audioOpen(const char* fname, int* sampleRate)
{
    char path[80];
    snprintf(path, sizeof(path), "%s", fname);

    int compression;
    if (queryCompressedFunc(path)) {
        compression = 2;
    } else {
        compression = 0;
    }

    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        debugPrint("AudioOpen: Couldn't open %s for read\n", path);
        return -1;
    }

    int index;
    for (index = 0; index < gAudioListLength; index++) {
        if ((gAudioList[index].flags & AUDIO_IN_USE) == 0) {
            break;
        }
    }

    if (index == gAudioListLength) {
        if (gAudioList != nullptr) {
            gAudioList = (Audio*)internal_realloc_safe(gAudioList, sizeof(*gAudioList) * (gAudioListLength + 1), __FILE__, __LINE__); // "..\int\audio.c", 216
        } else {
            gAudioList = (Audio*)internal_malloc_safe(sizeof(*gAudioList), __FILE__, __LINE__); // "..\int\audio.c", 218
        }
        gAudioListLength++;
    }

    Audio* audioFile = &(gAudioList[index]);
    audioFile->flags = AUDIO_IN_USE;
    audioFile->stream = stream;

    if (compression == 2) {
        audioFile->flags |= AUDIO_COMPRESSED;
        audioFile->soundDecoder = soundDecoderInit(audioSoundDecoderReadHandler, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));
        audioFile->fileSize *= 2;

        *sampleRate = audioFile->sampleRate;
    } else {
        audioFile->fileSize = fileGetSize(stream);
    }

    audioFile->position = 0;

    return index + 1;
}

// 0x41A50C
int audioClose(int handle)
{
    Audio* audioFile = &(gAudioList[handle - 1]);
    fileClose(audioFile->stream);

    if ((audioFile->flags & AUDIO_COMPRESSED) != 0) {
        soundDecoderFree(audioFile->soundDecoder);
    }

    memset(audioFile, 0, sizeof(Audio));

    return 0;
}

// 0x41A574
int audioRead(int handle, void* buffer, unsigned int size)
{
    Audio* audioFile = &(gAudioList[handle - 1]);

    int bytesRead;
    if ((audioFile->flags & AUDIO_COMPRESSED) != 0) {
        bytesRead = soundDecoderDecode(audioFile->soundDecoder, buffer, size);
    } else {
        bytesRead = fileRead(buffer, 1, size, audioFile->stream);
    }

    audioFile->position += bytesRead;

    return bytesRead;
}

// 0x41A5E0
long audioSeek(int handle, long offset, int origin)
{
    int pos;
    unsigned char* buf;
    int v10;

    Audio* audioFile = &(gAudioList[handle - 1]);

    switch (origin) {
    case SEEK_SET:
        pos = offset;
        break;
    case SEEK_CUR:
        pos = offset + audioFile->position;
        break;
    case SEEK_END:
        pos = offset + audioFile->fileSize;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    if ((audioFile->flags & AUDIO_COMPRESSED) != 0) {
        if (pos < audioFile->position) {
            soundDecoderFree(audioFile->soundDecoder);
            fileSeek(audioFile->stream, 0, SEEK_SET);
            audioFile->soundDecoder = soundDecoderInit(audioSoundDecoderReadHandler, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));
            audioFile->position = 0;
            audioFile->fileSize *= 2;

            if (pos != 0) {
                buf = (unsigned char*)internal_malloc_safe(4096, __FILE__, __LINE__); // "..\int\audio.c", 361
                while (pos > 4096) {
                    pos -= 4096;
                    audioRead(handle, buf, 4096);
                }

                if (pos != 0) {
                    audioRead(handle, buf, pos);
                }

                internal_free_safe(buf, __FILE__, __LINE__); // // "..\int\audio.c", 367
            }
        } else {
            buf = (unsigned char*)internal_malloc_safe(1024, __FILE__, __LINE__); // "..\int\audio.c", 321
            v10 = audioFile->position - pos;
            while (v10 > 1024) {
                v10 -= 1024;
                audioRead(handle, buf, 1024);
            }

            if (v10 != 0) {
                audioRead(handle, buf, v10);
            }

            // TODO: Probably leaks memory.
        }

        return audioFile->position;
    } else {
        return fileSeek(audioFile->stream, offset, origin);
    }
}

// 0x41A78C
long audioGetSize(int handle)
{
    Audio* audioFile = &(gAudioList[handle - 1]);
    return audioFile->fileSize;
}

// 0x41A7A8
long audioTell(int handle)
{
    Audio* audioFile = &(gAudioList[handle - 1]);
    return audioFile->position;
}

// AudioWrite
// 0x41A7C4
int audioWrite(int handle, const void* buf, unsigned int size)
{
    debugPrint("AudioWrite shouldn't be ever called\n");
    return 0;
}

// 0x41A7D4
int audioInit(AudioQueryCompressedFunc* func)
{
    queryCompressedFunc = func;
    gAudioList = nullptr;
    gAudioListLength = 0;

    return soundSetDefaultFileIO(audioOpen, audioClose, audioRead, audioWrite, audioSeek, audioTell, audioGetSize);
}

// 0x41A818
void audioExit()
{
    if (gAudioList != nullptr) {
        internal_free_safe(gAudioList, __FILE__, __LINE__); // "..\int\audio.c", 406
    }

    gAudioListLength = 0;
    gAudioList = nullptr;
}

} // namespace fallout
