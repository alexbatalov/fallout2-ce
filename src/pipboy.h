#ifndef PIPBOY_H
#define PIPBOY_H

#include "db.h"

int pipboyOpen(bool forceRest);
void pipboyInit();
void pipboyReset();
int pipboySave(File* stream);
int pipboyLoad(File* stream);

#endif /* PIPBOY_H */
