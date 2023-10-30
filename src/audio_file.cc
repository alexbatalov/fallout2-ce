#include "audio_file.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "memory_manager.h"
#include "platform_compat.h"
#include "sound.h"
#include "sound_decoder.h"

namespace fallout {

typedef enum AudioFileFlags {
    AUDIO_FILE_IN_USE = 0x01,
    AUDIO_FILE_COMPRESSED = 0x02,
} AudioFileFlags;

typedef struct AudioFile {
    int flags;
    FILE* stream;
    SoundDecoder* soundDecoder;
    int fileSize;
    int sampleRate;
    int channels;
    int position;
} AudioFile;

static bool defaultCompressionFunc(char* filePath);
static int audioFileSoundDecoderReadHandler(void* data, void* buffer, unsigned int size);

// 0x5108C0
static AudioFileQueryCompressedFunc* queryCompressedFunc = defaultCompressionFunc;

// 0x56CB10
static AudioFile* gAudioFileList;

// 0x56CB14
static int gAudioFileListLength;

// 0x41A850
static bool defaultCompressionFunc(char* filePath)
{
    char* pch = strrchr(filePath, '.');
    if (pch != nullptr) {
        strcpy(pch + 1, "raw");
    }

    return false;
}

// 0x41A870
static int audioFileSoundDecoderReadHandler(void* data, void* buffer, unsigned int size)
{
    return fread(buffer, 1, size, reinterpret_cast<FILE*>(data));
}

// 0x41A88C
int audioFileOpen(const char* fname, int* sampleRate)
{
    char path[COMPAT_MAX_PATH];
    strcpy(path, fname);

    int compression;
    if (queryCompressedFunc(path)) {
        compression = 2;
    } else {
        compression = 0;
    }

    FILE* stream = compat_fopen(path, "rb");
    if (stream == nullptr) {
        return -1;
    }

    int index;
    for (index = 0; index < gAudioFileListLength; index++) {
        if ((gAudioFileList[index].flags & AUDIO_FILE_IN_USE) == 0) {
            break;
        }
    }

    if (index == gAudioFileListLength) {
        if (gAudioFileList != nullptr) {
            gAudioFileList = (AudioFile*)internal_realloc_safe(gAudioFileList, sizeof(*gAudioFileList) * (gAudioFileListLength + 1), __FILE__, __LINE__); // "..\int\audiof.c", 207
        } else {
            gAudioFileList = (AudioFile*)internal_malloc_safe(sizeof(*gAudioFileList), __FILE__, __LINE__); // "..\int\audiof.c", 209
        }
        gAudioFileListLength++;
    }

    AudioFile* audioFile = &(gAudioFileList[index]);
    audioFile->flags = AUDIO_FILE_IN_USE;
    audioFile->stream = stream;

    if (compression == 2) {
        audioFile->flags |= AUDIO_FILE_COMPRESSED;
        audioFile->soundDecoder = soundDecoderInit(audioFileSoundDecoderReadHandler, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));
        audioFile->fileSize *= 2;

        *sampleRate = audioFile->sampleRate;
    } else {
        audioFile->fileSize = getFileSize(stream);
    }

    audioFile->position = 0;

    return index + 1;
}

// 0x41AAA0
int audioFileClose(int handle)
{
    AudioFile* audioFile = &(gAudioFileList[handle - 1]);
    fclose(audioFile->stream);

    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        soundDecoderFree(audioFile->soundDecoder);
    }

    // Reset audio file (which also resets it's use flag).
    memset(audioFile, 0, sizeof(*audioFile));

    return 0;
}

// 0x41AB08
int audioFileRead(int handle, void* buffer, unsigned int size)
{

    AudioFile* ptr = &(gAudioFileList[handle - 1]);

    int bytesRead;
    if ((ptr->flags & AUDIO_FILE_COMPRESSED) != 0) {
        bytesRead = soundDecoderDecode(ptr->soundDecoder, buffer, size);
    } else {
        bytesRead = fread(buffer, 1, size, ptr->stream);
    }

    ptr->position += bytesRead;

    return bytesRead;
}

// 0x41AB74
long audioFileSeek(int handle, long offset, int origin)
{
    void* buf;
    int remaining;
    int a4;

    AudioFile* audioFile = &(gAudioFileList[handle - 1]);

    switch (origin) {
    case SEEK_SET:
        a4 = offset;
        break;
    case SEEK_CUR:
        a4 = audioFile->fileSize + offset;
        break;
    case SEEK_END:
        a4 = audioFile->position + offset;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        if (a4 <= audioFile->position) {
            soundDecoderFree(audioFile->soundDecoder);

            fseek(audioFile->stream, 0, 0);

            audioFile->soundDecoder = soundDecoderInit(audioFileSoundDecoderReadHandler, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));
            audioFile->fileSize *= 2;
            audioFile->position = 0;

            if (a4) {
                buf = internal_malloc_safe(4096, __FILE__, __LINE__); // "..\int\audiof.c", 364
                while (a4 > 4096) {
                    audioFileRead(handle, buf, 4096);
                    a4 -= 4096;
                }
                if (a4 != 0) {
                    audioFileRead(handle, buf, a4);
                }
                internal_free_safe(buf, __FILE__, __LINE__); // "..\int\audiof.c", 370
            }
        } else {
            buf = internal_malloc_safe(0x400, __FILE__, __LINE__); // "..\int\audiof.c", 316
            remaining = audioFile->position - a4;
            while (remaining > 1024) {
                audioFileRead(handle, buf, 1024);
                remaining -= 1024;
            }
            if (remaining != 0) {
                audioFileRead(handle, buf, remaining);
            }
            // TODO: Obiously leaks memory.
        }
        return audioFile->position;
    }

    return fseek(audioFile->stream, offset, origin);
}

// 0x41AD20
long audioFileGetSize(int handle)
{
    AudioFile* audioFile = &(gAudioFileList[handle - 1]);
    return audioFile->fileSize;
}

// 0x41AD3C
long audioFileTell(int handle)
{
    AudioFile* audioFile = &(gAudioFileList[handle - 1]);
    return audioFile->position;
}

// AudiofWrite
// 0x41AD58
int audioFileWrite(int handle, const void* buffer, unsigned int size)
{
    debugPrint("AudiofWrite shouldn't be ever called\n");
    return 0;
}

// 0x41AD68
int audioFileInit(AudioFileQueryCompressedFunc* func)
{
    queryCompressedFunc = func;
    gAudioFileList = nullptr;
    gAudioFileListLength = 0;

    return soundSetDefaultFileIO(audioFileOpen, audioFileClose, audioFileRead, audioFileWrite, audioFileSeek, audioFileTell, audioFileGetSize);
}

// 0x41ADAC
void audioFileExit()
{
    if (gAudioFileList != nullptr) {
        internal_free_safe(gAudioFileList, __FILE__, __LINE__); // "..\int\audiof.c", 405
    }

    gAudioFileListLength = 0;
    gAudioFileList = nullptr;
}

} // namespace fallout
