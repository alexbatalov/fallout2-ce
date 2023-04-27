#ifndef AUDIO_FILE_H
#define AUDIO_FILE_H

namespace fallout {

typedef bool(AudioFileQueryCompressedFunc)(char* filePath);

int audioFileOpen(const char* fname, int* sampleRate);
int audioFileClose(int handle);
int audioFileRead(int handle, void* buf, unsigned int size);
long audioFileSeek(int handle, long offset, int origin);
long audioFileGetSize(int handle);
long audioFileTell(int handle);
int audioFileWrite(int handle, const void* buf, unsigned int size);
int audioFileInit(AudioFileQueryCompressedFunc* func);
void audioFileExit();

} // namespace fallout

#endif /* AUDIO_FILE_H */
