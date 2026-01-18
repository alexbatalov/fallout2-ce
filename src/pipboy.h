#ifndef PIPBOY_H
#define PIPBOY_H

#include "db.h"
#include "message.h"

namespace fallout {

typedef enum PipboyOpenIntent {
    PIPBOY_OPEN_INTENT_UNSPECIFIED = 0,
    PIPBOY_OPEN_INTENT_REST = 1,
} PipboyOpenIntent;

int pipboyOpen(int intent);
void pipboyInit();
void pipboyReset();
int pipboySave(File* stream);
int pipboyLoad(File* stream);

extern MessageList gPipboyMessageList;
int pipboyMessageListInit();
void pipboyMessageListFree();

} // namespace fallout

#endif /* PIPBOY_H */
