#ifndef AUDIO_FILE_H
#define AUDIO_FILE_H

#include "sound_decoder.h"

typedef enum AudioFileFlags {
    AUDIO_FILE_IN_USE = 0x01,
    AUDIO_FILE_COMPRESSED = 0x02,
} AudioFileFlags;

typedef struct AudioFile {
    int flags;
    int fileHandle;
    SoundDecoder* soundDecoder;
    int fileSize;
    int field_10;
    int field_14;
    int position;
} AudioFile;

typedef bool(AudioFileIsCompressedProc)(char* filePath);

int audioFileOpen(const char* fname, int flags, ...);
int audioFileClose(int a1);
int audioFileRead(int a1, void* buf, unsigned int size);
long audioFileSeek(int handle, long offset, int origin);
long audioFileGetSize(int a1);
long audioFileTell(int a1);
int audioFileWrite(int handle, const void* buf, unsigned int size);
int audioFileInit(AudioFileIsCompressedProc* isCompressedProc);
void audioFileExit();

#endif /* AUDIO_FILE_H */
