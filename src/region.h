#ifndef REGION_H
#define REGION_H

#include "geometry.h"
#include "interpreter.h"

namespace fallout {

#define REGION_NAME_LENGTH (32)

typedef struct Region Region;

typedef void RegionMouseEventCallback(Region* region, void* userData, int event);

typedef struct Region {
    char name[REGION_NAME_LENGTH];
    Point* points;
    int field_24;
    int field_28;
    int field_2C;
    int field_30;
    int field_34;
    int field_38;
    int pointsLength;
    int pointsCapacity;
    Program* program;
    int procs[4];
    int rightProcs[4];
    int field_68;
    int field_6C;
    int field_70;
    int field_74;
    RegionMouseEventCallback* mouseEventCallback;
    RegionMouseEventCallback* rightMouseEventCallback;
    void* mouseEventCallbackUserData;
    void* rightMouseEventCallbackUserData;
    void* userData;
} Region;

void _regionSetBound(Region* region);
Region* regionCreate(int initialCapacity);
void regionAddPoint(Region* region, int x, int y);
void regionDelete(Region* region);
void regionSetName(Region* region, const char* src);
char* regionGetName(Region* region);
void* regionGetUserData(Region* region);
void regionSetUserData(Region* region, void* data);
void regionAddFlag(Region* region, int value);

} // namespace fallout

#endif /* REGION_H */
