#ifndef SELFRUN_H
#define SELFRUN_H

#define SELFRUN_RECORDING_FILE_NAME_LENGTH 13
#define SELFRUN_MAP_FILE_NAME_LENGTH 13

typedef enum SelfrunState {
    SELFRUN_STATE_TURNED_OFF,
    SELFRUN_STATE_PLAYING,
    SELFRUN_STATE_RECORDING,
} SelfrunState;

typedef struct SelfrunData {
    char recordingFileName[SELFRUN_RECORDING_FILE_NAME_LENGTH];
    char mapFileName[SELFRUN_MAP_FILE_NAME_LENGTH];
    int stopKeyCode;
} SelfrunData;

static_assert(sizeof(SelfrunData) == 32, "wrong size");

extern int gSelfrunState;

int selfrunInitFileList(char*** fileListPtr, int* fileListLengthPtr);
int selfrunFreeFileList(char*** fileListPtr);
int selfrunPreparePlayback(const char* fileName, SelfrunData* selfrunData);
void selfrunPlaybackLoop(SelfrunData* selfrunData);
int selfrunPrepareRecording(const char* recordingName, const char* mapFileName, SelfrunData* selfrunData);
void selfrunRecordingLoop(SelfrunData* selfrunData);
void selfrunPlaybackCompleted(int reason);
int selfrunReadData(const char* path, SelfrunData* selfrunData);
int selfrunWriteData(const char* path, SelfrunData* selfrunData);

#endif /* SELFRUN_H */
