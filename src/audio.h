#ifndef AUDIO_H
#define AUDIO_H

#include "audio_file.h"

extern AudioFileIsCompressedProc* _queryCompressedFunc;

extern int gAudioListLength;
extern AudioFile* gAudioList;

bool _defaultCompressionFunc(char* filePath);
int audioSoundDecoderReadHandler(int fileHandle, void* buf, unsigned int size);
int audioOpen(const char* fname, int mode, ...);
int audioClose(int fileHandle);
int audioRead(int fileHandle, void* buffer, unsigned int size);
long audioSeek(int fileHandle, long offset, int origin);
long audioGetSize(int fileHandle);
long audioTell(int fileHandle);
int audioWrite(int handle, const void* buf, unsigned int size);
int audioInit(AudioFileIsCompressedProc* isCompressedProc);
void audioExit();

#endif /* AUDIO_H */
