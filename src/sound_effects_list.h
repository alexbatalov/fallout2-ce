#ifndef SOUND_EFFECTS_LIST_H
#define SOUND_EFFECTS_LIST_H

#define SFXL_OK (0)
#define SFXL_ERR (1)
#define SFXL_ERR_TAG_INVALID (2)

typedef struct SoundEffectsListEntry {
    char* name;
    int dataSize;
    int fileSize;
    int tag;
} SoundEffectsListEntry;

extern bool gSoundEffectsListInitialized;
extern int gSoundEffectsListDebugLevel;
extern char* gSoundEffectsListPath;
extern int gSoundEffectsListPathLength;
extern SoundEffectsListEntry* gSoundEffectsListEntries;
extern int gSoundEffectsListEntriesLength;
extern int _sfxl_compression;

bool soundEffectsListIsValidTag(int tag);
int soundEffectsListInit(const char* soundEffectsPath, int a2, int debugLevel);
void soundEffectsListExit();
int soundEffectsListGetTag(char* name, int* tagPtr);
int soundEffectsListGetFilePath(int tag, char** pathPtr);
int soundEffectsListGetDataSize(int tag, int* sizePtr);
int soundEffectsListGetFileSize(int tag, int* sizePtr);
int soundEffectsListTagToIndex(int tag, int* indexPtr);
void soundEffectsListClear();
int soundEffectsListPopulateFileNames();
int soundEffectsListCopyFileNames(char** fileNameList);
int soundEffectsListPopulateFileSizes();
int soundEffectsListSort();
int soundEffectsListCompareByName(const void* a1, const void* a2);
int _sfxl_ad_reader(int fileHandle, void* buf, unsigned int size);

#endif /* SOUND_EFFECTS_LIST_H */
