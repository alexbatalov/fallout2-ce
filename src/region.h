#ifndef REGION_H
#define REGION_H

#include "geometry.h"
#include "interpreter.h"

#define REGION_NAME_LENGTH (32)

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
    int field_48;
    int field_4C;
    int field_50;
    int field_54;
    int field_58;
    int field_5C;
    int field_60;
    int field_64;
    int field_68;
    int field_6C;
    int field_70;
    int field_74;
    int field_78;
    int field_7C;
    int field_80;
    int field_84;
    void* userData;
} Region;

void _regionSetBound(Region* region);
Region* regionCreate(int a1);
void regionAddPoint(Region* region, int x, int y);
void regionDelete(Region* region);
void regionSetName(Region* region, const char* src);
char* regionGetName(Region* region);
void* regionGetUserData(Region* region);
void regionSetUserData(Region* region, void* data);
void regionAddFlag(Region* region, int value);

#endif /* REGION_H */
