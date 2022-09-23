#ifndef SOUND_EFFECTS_LIST_H
#define SOUND_EFFECTS_LIST_H

namespace fallout {

#define SFXL_OK (0)
#define SFXL_ERR (1)
#define SFXL_ERR_TAG_INVALID (2)

bool soundEffectsListIsValidTag(int tag);
int soundEffectsListInit(const char* soundEffectsPath, int a2, int debugLevel);
void soundEffectsListExit();
int soundEffectsListGetTag(char* name, int* tagPtr);
int soundEffectsListGetFilePath(int tag, char** pathPtr);
int soundEffectsListGetDataSize(int tag, int* sizePtr);
int soundEffectsListGetFileSize(int tag, int* sizePtr);

} // namespace fallout

#endif /* SOUND_EFFECTS_LIST_H */
