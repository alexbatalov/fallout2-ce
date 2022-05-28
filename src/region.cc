#include "region.h"

#include "debug.h"
#include "memory_manager.h"

#include <limits.h>
#include <string.h>

char _aNull[] = "<null>";

// Probably recalculates bounding box of the region.
//
// 0x4A2B50
void _regionSetBound(Region* region)
{
    int v1 = INT_MAX;
    int v2 = INT_MIN;
    int v3 = INT_MAX;
    int v4 = INT_MIN;
    int v5 = 0;
    int v6 = 0;
    int v7 = 0;

    for (int index = 0; index < region->pointsLength; index++) {
        Point* point = &(region->points[index]);
        if (v1 >= point->x) v1 = point->x;
        if (v3 >= point->y) v3 = point->y;
        if (v2 <= point->x) v2 = point->x;
        if (v4 <= point->y) v4 = point->y;
        v6 += point->x;
        v7 += point->y;
        v5++;
    }

    region->field_28 = v3;
    region->field_2C = v2;
    region->field_30 = v4;
    region->field_24 = v1;

    if (v5 != 0) {
        region->field_34 = v6 / v5;
        region->field_38 = v7 / v5;
    }
}

// 0x4A2D78
Region* regionCreate(int initialCapacity)
{
    Region* region = (Region*)internal_malloc_safe(sizeof(*region), __FILE__, __LINE__); // "..\int\REGION.C", 142
    memset(region, 0, sizeof(*region));

    if (initialCapacity != 0) {
        region->points = (Point*)internal_malloc_safe(sizeof(*region->points) * (initialCapacity + 1), __FILE__, __LINE__); // "..\int\REGION.C", 147
        region->pointsCapacity = initialCapacity + 1;
    } else {
        region->points = NULL;
        region->pointsCapacity = 0;
    }

    region->name[0] = '\0';
    region->field_74 = 0;
    region->field_28 = INT_MIN;
    region->field_30 = INT_MAX;
    region->field_54 = 0;
    region->field_5C = 0;
    region->field_64 = 0;
    region->field_68 = 0;
    region->field_6C = 0;
    region->field_70 = 0;
    region->field_60 = 0;
    region->field_78 = 0;
    region->field_7C = 0;
    region->field_80 = 0;
    region->field_84 = 0;
    region->pointsLength = 0;
    region->field_24 = 0;
    region->field_2C = 0;
    region->field_50 = 0;
    region->field_4C = 0;
    region->field_48 = 0;
    region->field_58 = 0;

    return region;
}

// regionAddPoint
// 0x4A2E68
void regionAddPoint(Region* region, int x, int y)
{
    if (region == NULL) {
        debugPrint("regionAddPoint(): null region ptr\n");
        return;
    }

    if (region->points != NULL) {
        if (region->pointsCapacity - 1 == region->pointsLength) {
            region->points = (Point*)internal_realloc_safe(region->points, sizeof(*region->points) * (region->pointsCapacity + 1), __FILE__, __LINE__); // "..\int\REGION.C", 190
            region->pointsCapacity++;
        }
    } else {
        region->pointsCapacity = 2;
        region->pointsLength = 0;
        region->points = (Point*)internal_malloc_safe(sizeof(*region->points) * 2, __FILE__, __LINE__); // "..\int\REGION.C", 185
    }

    int pointIndex = region->pointsLength;
    region->pointsLength++;

    Point* point = &(region->points[pointIndex]);
    point->x = x;
    point->y = y;

    Point* end = &(region->points[pointIndex + 1]);
    end->x = region->points->x;
    end->y = region->points->y;
}

// regionDelete
// 0x4A2F0C
void regionDelete(Region* region)
{
    if (region == NULL) {
        debugPrint("regionDelete(): null region ptr\n");
        return;
    }

    if (region->points != NULL) {
        internal_free_safe(region->points, __FILE__, __LINE__); // "..\int\REGION.C", 206
    }

    internal_free_safe(region, __FILE__, __LINE__); // "..\int\REGION.C", 207
}

// regionAddName
// 0x4A2F54
void regionSetName(Region* region, const char* name)
{
    if (region == NULL) {
        debugPrint("regionAddName(): null region ptr\n");
        return;
    }

    if (name == NULL) {
        region->name[0] = '\0';
        return;
    }

    strncpy(region->name, name, REGION_NAME_LENGTH - 1);
}

// regionGetName
// 0x4A2F80
char* regionGetName(Region* region)
{
    if (region == NULL) {
        debugPrint("regionGetName(): null region ptr\n");
        return _aNull;
    }

    return region->name;
}

// regionGetUserData
// 0x4A2F98
void* regionGetUserData(Region* region)
{
    if (region == NULL) {
        debugPrint("regionGetUserData(): null region ptr\n");
        return NULL;
    }

    return region->userData;
}

// regionSetUserData
// 0x4A2FB4
void regionSetUserData(Region* region, void* data)
{
    if (region == NULL) {
        debugPrint("regionSetUserData(): null region ptr\n");
        return;
    }

    region->userData = data;
}

// 0x4A2FD0
void regionAddFlag(Region* region, int value)
{
    region->field_74 |= value;
}
