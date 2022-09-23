#ifndef AUDIO_H
#define AUDIO_H

#include "audio_file.h"

namespace fallout {

int audioOpen(const char* fname, int mode, ...);
int audioClose(int fileHandle);
int audioRead(int fileHandle, void* buffer, unsigned int size);
long audioSeek(int fileHandle, long offset, int origin);
long audioGetSize(int fileHandle);
long audioTell(int fileHandle);
int audioWrite(int handle, const void* buf, unsigned int size);
int audioInit(AudioFileIsCompressedProc* isCompressedProc);
void audioExit();

} // namespace fallout

#endif /* AUDIO_H */
