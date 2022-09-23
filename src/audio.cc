#include "audio.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "db.h"
#include "debug.h"
#include "memory_manager.h"
#include "pointer_registry.h"
#include "sound.h"

namespace fallout {

static bool _defaultCompressionFunc(char* filePath);
static int audioSoundDecoderReadHandler(int fileHandle, void* buf, unsigned int size);

// 0x5108BC
static AudioFileIsCompressedProc* _queryCompressedFunc = _defaultCompressionFunc;

// 0x56CB00
static int gAudioListLength;

// 0x56CB04
static AudioFile* gAudioList;

// 0x41A2B0
static bool _defaultCompressionFunc(char* filePath)
{
    char* pch = strrchr(filePath, '.');
    if (pch != NULL) {
        strcpy(pch + 1, "war");
    }

    return false;
}

// 0x41A2D0
static int audioSoundDecoderReadHandler(int fileHandle, void* buffer, unsigned int size)
{
    return fileRead(buffer, 1, size, (File*)intToPtr(fileHandle));
}

// AudioOpen
// 0x41A2EC
int audioOpen(const char* fname, int flags, ...)
{
    char path[80];
    sprintf(path, "%s", fname);

    int compression;
    if (_queryCompressedFunc(path)) {
        compression = 2;
    } else {
        compression = 0;
    }

    char mode[4];
    memset(mode, 0, 4);

    // NOTE: Original implementation is slightly different, it uses separate
    // variable to track index where to set 't' and 'b'.
    char* pm = mode;
    if (flags & 1) {
        *pm++ = 'w';
    } else if (flags & 2) {
        *pm++ = 'w';
        *pm++ = '+';
    } else {
        *pm++ = 'r';
    }

    if (flags & 0x100) {
        *pm++ = 't';
    } else if (flags & 0x200) {
        *pm++ = 'b';
    }

    File* stream = fileOpen(path, mode);
    if (stream == NULL) {
        debugPrint("AudioOpen: Couldn't open %s for read\n", path);
        return -1;
    }

    int index;
    for (index = 0; index < gAudioListLength; index++) {
        if ((gAudioList[index].flags & AUDIO_FILE_IN_USE) == 0) {
            break;
        }
    }

    if (index == gAudioListLength) {
        if (gAudioList != NULL) {
            gAudioList = (AudioFile*)internal_realloc_safe(gAudioList, sizeof(*gAudioList) * (gAudioListLength + 1), __FILE__, __LINE__); // "..\int\audio.c", 216
        } else {
            gAudioList = (AudioFile*)internal_malloc_safe(sizeof(*gAudioList), __FILE__, __LINE__); // "..\int\audio.c", 218
        }
        gAudioListLength++;
    }

    AudioFile* audioFile = &(gAudioList[index]);
    audioFile->flags = AUDIO_FILE_IN_USE;
    audioFile->fileHandle = ptrToInt(stream);

    if (compression == 2) {
        audioFile->flags |= AUDIO_FILE_COMPRESSED;
        audioFile->soundDecoder = soundDecoderInit(audioSoundDecoderReadHandler, audioFile->fileHandle, &(audioFile->field_14), &(audioFile->field_10), &(audioFile->fileSize));
        audioFile->fileSize *= 2;
    } else {
        audioFile->fileSize = fileGetSize(stream);
    }

    audioFile->position = 0;

    return index + 1;
}

// 0x41A50C
int audioClose(int fileHandle)
{
    AudioFile* audioFile = &(gAudioList[fileHandle - 1]);
    fileClose((File*)intToPtr(audioFile->fileHandle, true));

    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        soundDecoderFree(audioFile->soundDecoder);
    }

    memset(audioFile, 0, sizeof(AudioFile));

    return 0;
}

// 0x41A574
int audioRead(int fileHandle, void* buffer, unsigned int size)
{
    AudioFile* audioFile = &(gAudioList[fileHandle - 1]);

    int bytesRead;
    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        bytesRead = soundDecoderDecode(audioFile->soundDecoder, buffer, size);
    } else {
        bytesRead = fileRead(buffer, 1, size, (File*)intToPtr(audioFile->fileHandle));
    }

    audioFile->position += bytesRead;

    return bytesRead;
}

// 0x41A5E0
long audioSeek(int fileHandle, long offset, int origin)
{
    int pos;
    unsigned char* buf;
    int v10;

    AudioFile* audioFile = &(gAudioList[fileHandle - 1]);

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

    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        if (pos < audioFile->position) {
            soundDecoderFree(audioFile->soundDecoder);
            fileSeek((File*)intToPtr(audioFile->fileHandle), 0, SEEK_SET);
            audioFile->soundDecoder = soundDecoderInit(audioSoundDecoderReadHandler, audioFile->fileHandle, &(audioFile->field_14), &(audioFile->field_10), &(audioFile->fileSize));
            audioFile->position = 0;
            audioFile->fileSize *= 2;

            if (pos != 0) {
                buf = (unsigned char*)internal_malloc_safe(4096, __FILE__, __LINE__); // "..\int\audio.c", 361
                while (pos > 4096) {
                    pos -= 4096;
                    audioRead(fileHandle, buf, 4096);
                }

                if (pos != 0) {
                    audioRead(fileHandle, buf, pos);
                }

                internal_free_safe(buf, __FILE__, __LINE__); // // "..\int\audio.c", 367
            }
        } else {
            buf = (unsigned char*)internal_malloc_safe(1024, __FILE__, __LINE__); // "..\int\audio.c", 321
            v10 = audioFile->position - pos;
            while (v10 > 1024) {
                v10 -= 1024;
                audioRead(fileHandle, buf, 1024);
            }

            if (v10 != 0) {
                audioRead(fileHandle, buf, v10);
            }

            // TODO: Probably leaks memory.
        }

        return audioFile->position;
    } else {
        return fileSeek((File*)intToPtr(audioFile->fileHandle), offset, origin);
    }
}

// 0x41A78C
long audioGetSize(int fileHandle)
{
    AudioFile* audioFile = &(gAudioList[fileHandle - 1]);
    return audioFile->fileSize;
}

// 0x41A7A8
long audioTell(int fileHandle)
{
    AudioFile* audioFile = &(gAudioList[fileHandle - 1]);
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
int audioInit(AudioFileIsCompressedProc* isCompressedProc)
{
    _queryCompressedFunc = isCompressedProc;
    gAudioList = NULL;
    gAudioListLength = 0;

    return soundSetDefaultFileIO(audioOpen, audioClose, audioRead, audioWrite, audioSeek, audioTell, audioGetSize);
}

// 0x41A818
void audioExit()
{
    if (gAudioList != NULL) {
        internal_free_safe(gAudioList, __FILE__, __LINE__); // "..\int\audio.c", 406
    }

    gAudioListLength = 0;
    gAudioList = NULL;
}

} // namespace fallout
