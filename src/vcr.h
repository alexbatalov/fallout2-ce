#ifndef FALLOUT_VCR_H_
#define FALLOUT_VCR_H_

#include "db.h"

namespace fallout {

#define VCR_BUFFER_CAPACITY 4096

typedef enum VcrState {
    VCR_STATE_RECORDING,
    VCR_STATE_PLAYING,
    VCR_STATE_TURNED_OFF,
} VcrState;

#define VCR_STATE_STOP_REQUESTED 0x80000000

typedef enum VcrTerminationFlags {
    // Specifies that VCR playback should stop if any key is pressed.
    VCR_TERMINATE_ON_KEY_PRESS = 0x01,

    // Specifies that VCR playback should stop if mouse is mouved.
    VCR_TERMINATE_ON_MOUSE_MOVE = 0x02,

    // Specifies that VCR playback should stop if any mouse button is pressed.
    VCR_TERMINATE_ON_MOUSE_PRESS = 0x04,
} VcrTerminationFlags;

typedef enum VcrPlaybackCompletionReason {
    VCR_PLAYBACK_COMPLETION_REASON_NONE = 0,

    // Indicates that VCR playback completed normally.
    VCR_PLAYBACK_COMPLETION_REASON_COMPLETED = 1,

    // Indicates that VCR playback terminated according to termination flags.
    VCR_PLAYBACK_COMPLETION_REASON_TERMINATED = 2,
} VcrPlaybackCompletionReason;

typedef enum VcrEntryType {
    VCR_ENTRY_TYPE_NONE = 0,
    VCR_ENTRY_TYPE_INITIAL_STATE = 1,
    VCR_ENTRY_TYPE_KEYBOARD_EVENT = 2,
    VCR_ENTRY_TYPE_MOUSE_EVENT = 3,
} VcrEntryType;

typedef void(VcrPlaybackCompletionCallback)(int reason);

typedef struct VcrEntry {
    unsigned int type;
    unsigned int time;
    unsigned int counter;
    union {
        struct {
            int mouseX;
            int mouseY;
            int keyboardLayout;
        } initial;
        struct {
            short key;
        } keyboardEvent;
        struct {
            int dx;
            int dy;
            int buttons;
        } mouseEvent;
    };
} VcrEntry;

extern VcrEntry* _vcr_buffer;
extern int _vcr_buffer_index;
extern unsigned int gVcrState;
extern unsigned int _vcr_time;
extern unsigned int _vcr_counter;
extern unsigned int gVcrTerminateFlags;
extern int gVcrPlaybackCompletionReason;

bool vcrRecord(const char* fileName);
bool vcrPlay(const char* fileName, unsigned int terminationFlags, VcrPlaybackCompletionCallback* callback);
void vcrStop();
int vcrGetState();
int vcrUpdate();
bool vcrDump();
bool vcrWriteEntry(VcrEntry* ptr, File* stream);
bool vcrReadEntry(VcrEntry* ptr, File* stream);

} // namespace fallout

#endif /* FALLOUT_VCR_H_ */
