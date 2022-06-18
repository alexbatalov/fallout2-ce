#include "audio_file.h"

#include "debug.h"
#include "memory_manager.h"
#include "platform_compat.h"
#include "sound.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool _defaultCompressionFunc__(char* filePath);
static int audioFileSoundDecoderReadHandler(int fileHandle, void* buffer, unsigned int size);

// 0x5108C0
static AudioFileIsCompressedProc* _queryCompressedFunc_2 = _defaultCompressionFunc__;

// 0x56CB10
static AudioFile* gAudioFileList;

// 0x56CB14
static int gAudioFileListLength;

// 0x41A850
static bool _defaultCompressionFunc__(char* filePath)
{
    char* pch = strrchr(filePath, '.');
    if (pch != NULL) {
        strcpy(pch + 1, "war");
    }

    return false;
}

// 0x41A870
static int audioFileSoundDecoderReadHandler(int fileHandle, void* buffer, unsigned int size)
{
    return fread(buffer, 1, size, (FILE*)fileHandle);
}

// 0x41A88C
int audioFileOpen(const char* fname, int flags, ...)
{
    char path[COMPAT_MAX_PATH];
    strcpy(path, fname);

    int compression;
    if (_queryCompressedFunc_2(path)) {
        compression = 2;
    } else {
        compression = 0;
    }

    char mode[4];
    memset(mode, '\0', 4);

    // NOTE: Original implementation is slightly different, it uses separate
    // variable to track index where to set 't' and 'b'.
    char* pm = mode;
    if (flags & 0x01) {
        *pm++ = 'w';
    } else if (flags & 0x02) {
        *pm++ = 'w';
        *pm++ = '+';
    } else {
        *pm++ = 'r';
    }

    if (flags & 0x0100) {
        *pm++ = 't';
    } else if (flags & 0x0200) {
        *pm++ = 'b';
    }

    FILE* stream = compat_fopen(path, mode);
    if (stream == NULL) {
        return -1;
    }

    int index;
    for (index = 0; index < gAudioFileListLength; index++) {
        if ((gAudioFileList[index].flags & AUDIO_FILE_IN_USE) == 0) {
            break;
        }
    }

    if (index == gAudioFileListLength) {
        if (gAudioFileList != NULL) {
            gAudioFileList = (AudioFile*)internal_realloc_safe(gAudioFileList, sizeof(*gAudioFileList) * (gAudioFileListLength + 1), __FILE__, __LINE__); // "..\int\audiof.c", 207
        } else {
            gAudioFileList = (AudioFile*)internal_malloc_safe(sizeof(*gAudioFileList), __FILE__, __LINE__); // "..\int\audiof.c", 209
        }
        gAudioFileListLength++;
    }

    AudioFile* audioFile = &(gAudioFileList[index]);
    audioFile->flags = AUDIO_FILE_IN_USE;
    audioFile->fileHandle = (int)stream;

    if (compression == 2) {
        audioFile->flags |= AUDIO_FILE_COMPRESSED;
        audioFile->soundDecoder = soundDecoderInit(audioFileSoundDecoderReadHandler, audioFile->fileHandle, &(audioFile->field_14), &(audioFile->field_10), &(audioFile->fileSize));
        audioFile->fileSize *= 2;
    } else {
        audioFile->fileSize = compat_filelength(fileno(stream));
    }

    audioFile->position = 0;

    return index + 1;
}

// 0x41AAA0
int audioFileClose(int fileHandle)
{
    AudioFile* audioFile = &(gAudioFileList[fileHandle - 1]);
    fclose((FILE*)audioFile->fileHandle);

    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        soundDecoderFree(audioFile->soundDecoder);
    }

    // Reset audio file (which also resets it's use flag).
    memset(audioFile, 0, sizeof(*audioFile));

    return 0;
}

// 0x41AB08
int audioFileRead(int fileHandle, void* buffer, unsigned int size)
{

    AudioFile* ptr = &(gAudioFileList[fileHandle - 1]);

    int bytesRead;
    if ((ptr->flags & AUDIO_FILE_COMPRESSED) != 0) {
        bytesRead = soundDecoderDecode(ptr->soundDecoder, buffer, size);
    } else {
        bytesRead = fread(buffer, 1, size, (FILE*)ptr->fileHandle);
    }

    ptr->position += bytesRead;

    return bytesRead;
}

// 0x41AB74
long audioFileSeek(int fileHandle, long offset, int origin)
{
    void* buf;
    int remaining;
    int a4;

    AudioFile* audioFile = &(gAudioFileList[fileHandle - 1]);

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

            fseek((FILE*)audioFile->fileHandle, 0, 0);

            audioFile->soundDecoder = soundDecoderInit(audioFileSoundDecoderReadHandler, audioFile->fileHandle, &(audioFile->field_14), &(audioFile->field_10), &(audioFile->fileSize));
            audioFile->fileSize *= 2;
            audioFile->position = 0;

            if (a4) {
                buf = internal_malloc_safe(4096, __FILE__, __LINE__); // "..\int\audiof.c", 364
                while (a4 > 4096) {
                    audioFileRead(fileHandle, buf, 4096);
                    a4 -= 4096;
                }
                if (a4 != 0) {
                    audioFileRead(fileHandle, buf, a4);
                }
                internal_free_safe(buf, __FILE__, __LINE__); // "..\int\audiof.c", 370
            }
        } else {
            buf = internal_malloc_safe(0x400, __FILE__, __LINE__); // "..\int\audiof.c", 316
            remaining = audioFile->position - a4;
            while (remaining > 1024) {
                audioFileRead(fileHandle, buf, 1024);
                remaining -= 1024;
            }
            if (remaining != 0) {
                audioFileRead(fileHandle, buf, remaining);
            }
            // TODO: Obiously leaks memory.
        }
        return audioFile->position;
    }

    return fseek((FILE*)audioFile->fileHandle, offset, origin);
}

// 0x41AD20
long audioFileGetSize(int fileHandle)
{
    AudioFile* audioFile = &(gAudioFileList[fileHandle - 1]);
    return audioFile->fileSize;
}

// 0x41AD3C
long audioFileTell(int fileHandle)
{
    AudioFile* audioFile = &(gAudioFileList[fileHandle - 1]);
    return audioFile->position;
}

// AudiofWrite
// 0x41AD58
int audioFileWrite(int fileHandle, const void* buffer, unsigned int size)
{
    debugPrint("AudiofWrite shouldn't be ever called\n");
    return 0;
}

// 0x41AD68
int audioFileInit(AudioFileIsCompressedProc* isCompressedProc)
{
    _queryCompressedFunc_2 = isCompressedProc;
    gAudioFileList = NULL;
    gAudioFileListLength = 0;

    return soundSetDefaultFileIO(audioFileOpen, audioFileClose, audioFileRead, audioFileWrite, audioFileSeek, audioFileTell, audioFileGetSize);
}

// 0x41ADAC
void audioFileExit()
{
    if (gAudioFileList != NULL) {
        internal_free_safe(gAudioFileList, __FILE__, __LINE__); // "..\int\audiof.c", 405
    }

    gAudioFileListLength = 0;
    gAudioFileList = NULL;
}
