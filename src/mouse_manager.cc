#include "mouse_manager.h"

#include <string.h>

#include "datafile.h"
#include "db.h"
#include "debug.h"
#include "input.h"
#include "memory_manager.h"
#include "mouse.h"
#include "platform_compat.h"
#include "svga.h"

namespace fallout {

// 0x5195A8
MouseManagerNameMangler* gMouseManagerNameMangler = mouseManagerNameManglerDefaultImpl;

// 0x5195AC
MouseManagerRateProvider* gMouseManagerRateProvider = mouseManagerRateProviderDefaultImpl;

// 0x5195B0
MouseManagerTimeProvider* gMouseManagerTimeProvider = mouseManagerTimeProviderDefaultImpl;

// 0x5195B4
int gMouseManagerCurrentRef = 1;

// 0x63247C
MouseManagerCacheEntry gMouseManagerCache[MOUSE_MGR_CACHE_CAPACITY];

// 0x638DFC
bool gMouseManagerIsAnimating;

// 0x638E00
unsigned char* gMouseManagerCurrentPalette;

// 0x638E04
MouseManagerAnimatedData* gMouseManagerCurrentAnimatedData;

// 0x638E08
unsigned char* gMouseManagerCurrentStaticData;

// 0x638E0C
int gMouseManagerCurrentCacheEntryIndex;

// 0x485250
char* mouseManagerNameManglerDefaultImpl(char* a1)
{
    return a1;
}

// 0x485254
int mouseManagerRateProviderDefaultImpl()
{
    return 1000;
}

// 0x48525C
int mouseManagerTimeProviderDefaultImpl()
{
    return getTicks();
}

// 0x485288
void mouseManagerSetNameMangler(MouseManagerNameMangler* func)
{
    gMouseManagerNameMangler = func;
}

// 0x4852B8
void mouseManagerFreeCacheEntry(MouseManagerCacheEntry* entry)
{
    switch (entry->type) {
    case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
        if (entry->staticData != nullptr) {
            if (entry->staticData->data != nullptr) {
                internal_free_safe(entry->staticData->data, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 120
                entry->staticData->data = nullptr;
            }
            internal_free_safe(entry->staticData, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 123
            entry->staticData = nullptr;
        }
        break;
    case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
        if (entry->animatedData != nullptr) {
            if (entry->animatedData->field_0 != nullptr) {
                for (int index = 0; index < entry->animatedData->frameCount; index++) {
                    internal_free_safe(entry->animatedData->field_0[index], __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 134
                    internal_free_safe(entry->animatedData->field_4[index], __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 135
                }
                internal_free_safe(entry->animatedData->field_0, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 137
                internal_free_safe(entry->animatedData->field_4, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 138
                internal_free_safe(entry->animatedData->field_8, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 139
                internal_free_safe(entry->animatedData->field_C, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 140
            }
            internal_free_safe(entry->animatedData, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 143
            entry->animatedData = nullptr;
        }
        break;
    }

    entry->type = 0;
    entry->fileName[0] = '\0';
}

// 0x4853F8
int mouseManagerInsertCacheEntry(void** data, int type, unsigned char* palette, const char* fileName)
{
    int foundIndex = -1;
    int index;
    for (index = 0; index < MOUSE_MGR_CACHE_CAPACITY; index++) {
        MouseManagerCacheEntry* cacheEntry = &(gMouseManagerCache[index]);
        if (cacheEntry->type == MOUSE_MANAGER_MOUSE_TYPE_NONE && foundIndex == -1) {
            foundIndex = index;
        }

        if (compat_stricmp(fileName, cacheEntry->fileName) == 0) {
            mouseManagerFreeCacheEntry(cacheEntry);
            foundIndex = index;
            break;
        }
    }

    if (foundIndex != -1) {
        index = foundIndex;
    }

    if (index == MOUSE_MGR_CACHE_CAPACITY) {
        int v2 = -1;
        int v1 = gMouseManagerCurrentRef;
        for (int index = 0; index < MOUSE_MGR_CACHE_CAPACITY; index++) {
            MouseManagerCacheEntry* cacheEntry = &(gMouseManagerCache[index]);
            if (v1 > cacheEntry->ref) {
                v1 = cacheEntry->ref;
                v2 = index;
            }
        }

        if (v2 == -1) {
            debugPrint("Mouse cache overflow!!!!\n");
            exit(1);
        }

        index = v2;
        mouseManagerFreeCacheEntry(&(gMouseManagerCache[index]));
    }

    MouseManagerCacheEntry* cacheEntry = &(gMouseManagerCache[index]);
    cacheEntry->type = type;
    memcpy(cacheEntry->palette, palette, sizeof(cacheEntry->palette));
    cacheEntry->ref = gMouseManagerCurrentRef++;
    strncpy(cacheEntry->fileName, fileName, sizeof(cacheEntry->fileName) - 1);
    cacheEntry->field_32C[0] = '\0';
    cacheEntry->data = *data;

    return index;
}

// NOTE: Inlined.
//
// 0x4853D4
void mouseManagerFlushCache()
{
    for (int index = 0; index < MOUSE_MGR_CACHE_CAPACITY; index++) {
        mouseManagerFreeCacheEntry(&(gMouseManagerCache[index]));
    }
}

// 0x48554C
MouseManagerCacheEntry* mouseManagerFindCacheEntry(const char* fileName, unsigned char** palettePtr, int* a3, int* a4, int* widthPtr, int* heightPtr, int* typePtr)
{
    for (int index = 0; index < MOUSE_MGR_CACHE_CAPACITY; index++) {
        MouseManagerCacheEntry* cacheEntry = &(gMouseManagerCache[index]);
        if (compat_strnicmp(cacheEntry->fileName, fileName, 31) == 0 || compat_strnicmp(cacheEntry->field_32C, fileName, 31) == 0) {
            *palettePtr = cacheEntry->palette;
            *typePtr = cacheEntry->type;

            gMouseManagerCurrentCacheEntryIndex = index;

            switch (cacheEntry->type) {
            case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
                *a3 = cacheEntry->staticData->field_4;
                *a4 = cacheEntry->staticData->field_8;
                *widthPtr = cacheEntry->staticData->width;
                *heightPtr = cacheEntry->staticData->height;
                break;
            case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
                *widthPtr = cacheEntry->animatedData->width;
                *heightPtr = cacheEntry->animatedData->height;
                *a3 = cacheEntry->animatedData->field_8[cacheEntry->animatedData->field_26];
                *a4 = cacheEntry->animatedData->field_C[cacheEntry->animatedData->field_26];
                break;
            }

            return cacheEntry;
        }
    }

    return nullptr;
}

// 0x48568C
void mouseManagerInit()
{
    mouseSetSensitivity(1.0);
}

// 0x48569C
void mouseManagerExit()
{
    mouseSetFrame(nullptr, 0, 0, 0, 0, 0, 0);

    if (gMouseManagerCurrentStaticData != nullptr) {
        internal_free_safe(gMouseManagerCurrentStaticData, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 243
        gMouseManagerCurrentStaticData = nullptr;
    }

    // NOTE: Uninline.
    mouseManagerFlushCache();

    gMouseManagerCurrentPalette = nullptr;
    gMouseManagerCurrentAnimatedData = nullptr;
}

// 0x485704
void mouseManagerUpdate()
{
    if (!gMouseManagerIsAnimating) {
        return;
    }

    if (gMouseManagerCurrentAnimatedData == nullptr) {
        debugPrint("Animating == 1 but curAnim == 0\n");
    }

    if (gMouseManagerTimeProvider() >= gMouseManagerCurrentAnimatedData->field_1C) {
        gMouseManagerCurrentAnimatedData->field_1C = (int)(gMouseManagerCurrentAnimatedData->field_18 / gMouseManagerCurrentAnimatedData->frameCount * gMouseManagerRateProvider() + gMouseManagerTimeProvider());
        if (gMouseManagerCurrentAnimatedData->field_24 != gMouseManagerCurrentAnimatedData->field_26) {
            int v1 = gMouseManagerCurrentAnimatedData->field_26 + gMouseManagerCurrentAnimatedData->field_20;
            if (v1 < 0) {
                v1 = gMouseManagerCurrentAnimatedData->frameCount - 1;
            } else if (v1 >= gMouseManagerCurrentAnimatedData->frameCount) {
                v1 = 0;
            }

            gMouseManagerCurrentAnimatedData->field_26 = v1;
            memcpy(gMouseManagerCurrentAnimatedData->field_0[gMouseManagerCurrentAnimatedData->field_26],
                gMouseManagerCurrentAnimatedData->field_4[gMouseManagerCurrentAnimatedData->field_26],
                gMouseManagerCurrentAnimatedData->width * gMouseManagerCurrentAnimatedData->height);

            sub_42EE84(gMouseManagerCurrentAnimatedData->field_0[gMouseManagerCurrentAnimatedData->field_26],
                gMouseManagerCurrentPalette,
                gMouseManagerCurrentAnimatedData->width,
                gMouseManagerCurrentAnimatedData->height);

            mouseSetFrame(gMouseManagerCurrentAnimatedData->field_0[v1],
                gMouseManagerCurrentAnimatedData->width,
                gMouseManagerCurrentAnimatedData->height,
                gMouseManagerCurrentAnimatedData->width,
                gMouseManagerCurrentAnimatedData->field_8[v1],
                gMouseManagerCurrentAnimatedData->field_C[v1],
                0);
        }
    }
}

// 0x485868
int mouseManagerSetFrame(char* fileName, int a2)
{
    char* mangledFileName = gMouseManagerNameMangler(fileName);

    unsigned char* palette;
    int temp;
    int type;
    MouseManagerCacheEntry* cacheEntry = mouseManagerFindCacheEntry(fileName, &palette, &temp, &temp, &temp, &temp, &type);
    if (cacheEntry != nullptr) {
        if (type == MOUSE_MANAGER_MOUSE_TYPE_ANIMATED) {
            cacheEntry->animatedData->field_24 = a2;
            if (cacheEntry->animatedData->field_24 >= cacheEntry->animatedData->field_26) {
                int v1 = cacheEntry->animatedData->field_24 - cacheEntry->animatedData->field_26;
                int v2 = cacheEntry->animatedData->frameCount + cacheEntry->animatedData->field_26 - cacheEntry->animatedData->field_24;
                if (v1 >= v2) {
                    cacheEntry->animatedData->field_20 = -1;
                } else {
                    cacheEntry->animatedData->field_20 = 1;
                }
            } else {
                int v1 = cacheEntry->animatedData->field_26 - cacheEntry->animatedData->field_24;
                int v2 = cacheEntry->animatedData->frameCount + cacheEntry->animatedData->field_24 - cacheEntry->animatedData->field_26;
                if (v1 < v2) {
                    cacheEntry->animatedData->field_20 = -1;
                } else {
                    cacheEntry->animatedData->field_20 = 1;
                }
            }

            if (!gMouseManagerIsAnimating || gMouseManagerCurrentAnimatedData != cacheEntry->animatedData) {
                memcpy(cacheEntry->animatedData->field_0[cacheEntry->animatedData->field_26],
                    cacheEntry->animatedData->field_4[cacheEntry->animatedData->field_26],
                    cacheEntry->animatedData->width * cacheEntry->animatedData->height);

                mouseSetFrame(cacheEntry->animatedData->field_0[cacheEntry->animatedData->field_26],
                    cacheEntry->animatedData->width,
                    cacheEntry->animatedData->height,
                    cacheEntry->animatedData->width,
                    cacheEntry->animatedData->field_8[cacheEntry->animatedData->field_26],
                    cacheEntry->animatedData->field_C[cacheEntry->animatedData->field_26],
                    0);

                gMouseManagerIsAnimating = true;
            }

            gMouseManagerCurrentAnimatedData = cacheEntry->animatedData;
            gMouseManagerCurrentPalette = palette;
            gMouseManagerCurrentAnimatedData->field_1C = gMouseManagerTimeProvider();
            return true;
        }

        mouseManagerSetMousePointer(fileName);
        return true;
    }

    if (gMouseManagerIsAnimating) {
        gMouseManagerCurrentPalette = nullptr;
        gMouseManagerIsAnimating = 0;
        gMouseManagerCurrentAnimatedData = nullptr;
    } else {
        if (gMouseManagerCurrentStaticData != nullptr) {
            internal_free_safe(gMouseManagerCurrentStaticData, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 337
            gMouseManagerCurrentStaticData = nullptr;
        }
    }

    File* stream = fileOpen(mangledFileName, "r");
    if (stream == nullptr) {
        debugPrint("mouseSetFrame: couldn't find %s\n", mangledFileName);
        return false;
    }

    char string[80];
    fileReadString(string, sizeof(string), stream);
    if (compat_strnicmp(string, "anim", 4) != 0) {
        fileClose(stream);
        mouseManagerSetMousePointer(fileName);
        return true;
    }

    // NOTE: Uninline.
    char* sep = strchr(string, ' ');
    if (sep == nullptr) {
        // FIXME: Leaks stream.
        return false;
    }

    int v3;
    float v4;
    sscanf(sep + 1, "%d %f", &v3, &v4);

    MouseManagerAnimatedData* animatedData = (MouseManagerAnimatedData*)internal_malloc_safe(sizeof(*animatedData), __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 359
    animatedData->field_0 = (unsigned char**)internal_malloc_safe(sizeof(*animatedData->field_0) * v3, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 360
    animatedData->field_4 = (unsigned char**)internal_malloc_safe(sizeof(*animatedData->field_4) * v3, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 361
    animatedData->field_8 = (int*)internal_malloc_safe(sizeof(*animatedData->field_8) * v3, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 362
    animatedData->field_C = (int*)internal_malloc_safe(sizeof(*animatedData->field_8) * v3, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 363
    animatedData->field_18 = v4;
    animatedData->field_1C = gMouseManagerTimeProvider();
    animatedData->field_26 = 0;
    animatedData->field_24 = a2;
    animatedData->frameCount = v3;
    if (animatedData->frameCount / 2 <= a2) {
        animatedData->field_20 = -1;
    } else {
        animatedData->field_20 = 1;
    }

    int width;
    int height;
    for (int index = 0; index < v3; index++) {
        string[0] = '\0';
        fileReadString(string, sizeof(string), stream);
        if (string[0] == '\0') {
            debugPrint("Not enough frames in %s, got %d, needed %d", mangledFileName, index, v3);
            break;
        }

        // NOTE: Uninline.
        char* sep = strchr(string, ' ');
        if (sep == nullptr) {
            debugPrint("Bad line %s in %s\n", string, fileName);
            // FIXME: Leaking stream.
            return false;
        }

        *sep = '\0';

        int v5;
        int v6;
        sscanf(sep + 1, "%d %d", &v5, &v6);

        animatedData->field_4[index] = datafileReadRaw(gMouseManagerNameMangler(string), &width, &height);
        animatedData->field_0[index] = (unsigned char*)internal_malloc_safe(width * height, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 390
        memcpy(animatedData->field_0[index], animatedData->field_4[index], width * height);
        sub_42EE84(animatedData->field_0[index], datafileGetPalette(), width, height);
        animatedData->field_8[index] = v5;
        animatedData->field_C[index] = v6;
    }

    fileClose(stream);

    animatedData->width = width;
    animatedData->height = height;

    gMouseManagerCurrentCacheEntryIndex = mouseManagerInsertCacheEntry(reinterpret_cast<void**>(&animatedData), MOUSE_MANAGER_MOUSE_TYPE_ANIMATED, datafileGetPalette(), fileName);
    strncpy(gMouseManagerCache[gMouseManagerCurrentCacheEntryIndex].field_32C, fileName, 31);

    gMouseManagerCurrentAnimatedData = animatedData;
    gMouseManagerCurrentPalette = gMouseManagerCache[gMouseManagerCurrentCacheEntryIndex].palette;
    gMouseManagerIsAnimating = true;

    mouseSetFrame(animatedData->field_0[0],
        animatedData->width,
        animatedData->height,
        animatedData->width,
        animatedData->field_8[0],
        animatedData->field_C[0],
        0);

    return true;
}

// 0x485E58
bool mouseManagerSetMouseShape(char* fileName, int a2, int a3)
{
    unsigned char* palette;
    int temp;
    int width;
    int height;
    int type;
    MouseManagerCacheEntry* cacheEntry = mouseManagerFindCacheEntry(fileName, &palette, &temp, &temp, &width, &height, &type);
    char* mangledFileName = gMouseManagerNameMangler(fileName);

    if (cacheEntry == nullptr) {
        MouseManagerStaticData* staticData;
        staticData = (MouseManagerStaticData*)internal_malloc_safe(sizeof(*staticData), __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 430
        staticData->data = datafileReadRaw(mangledFileName, &width, &height);
        staticData->field_4 = a2;
        staticData->field_8 = a3;
        staticData->width = width;
        staticData->height = height;
        gMouseManagerCurrentCacheEntryIndex = mouseManagerInsertCacheEntry(reinterpret_cast<void**>(&staticData), MOUSE_MANAGER_MOUSE_TYPE_STATIC, datafileGetPalette(), fileName);

        // NOTE: Original code is slightly different. It obtains address of
        // `staticData` and sets it's it into `cacheEntry`, which is a bit
        // awkward. Maybe there is more level on indirection was used. Any way
        // in order to make code path below unaltered take entire cache entry.
        cacheEntry = &(gMouseManagerCache[gMouseManagerCurrentCacheEntryIndex]);

        type = MOUSE_MANAGER_MOUSE_TYPE_STATIC;
        palette = gMouseManagerCache[gMouseManagerCurrentCacheEntryIndex].palette;
    }

    switch (type) {
    case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
        if (gMouseManagerCurrentStaticData != nullptr) {
            internal_free_safe(gMouseManagerCurrentStaticData, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 446
        }

        gMouseManagerCurrentStaticData = (unsigned char*)internal_malloc_safe(width * height, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 448
        memcpy(gMouseManagerCurrentStaticData, cacheEntry->staticData->data, width * height);
        sub_42EE84(gMouseManagerCurrentStaticData, palette, width, height);
        mouseSetFrame(gMouseManagerCurrentStaticData, width, height, width, a2, a3, 0);
        gMouseManagerIsAnimating = false;
        break;
    case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
        gMouseManagerCurrentAnimatedData = cacheEntry->animatedData;
        gMouseManagerIsAnimating = true;
        gMouseManagerCurrentPalette = palette;
        break;
    }

    return true;
}

// 0x486010
bool mouseManagerSetMousePointer(char* fileName)
{
    unsigned char* palette;
    int v1;
    int v2;
    int width;
    int height;
    int type;
    MouseManagerCacheEntry* cacheEntry = mouseManagerFindCacheEntry(fileName, &palette, &v1, &v2, &width, &height, &type);
    if (cacheEntry != nullptr) {
        if (gMouseManagerCurrentStaticData != nullptr) {
            internal_free_safe(gMouseManagerCurrentStaticData, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 482
            gMouseManagerCurrentStaticData = nullptr;
        }

        gMouseManagerCurrentPalette = nullptr;
        gMouseManagerIsAnimating = false;
        gMouseManagerCurrentAnimatedData = nullptr;

        switch (type) {
        case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
            gMouseManagerCurrentStaticData = (unsigned char*)internal_malloc_safe(width * height, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 492
            memcpy(gMouseManagerCurrentStaticData, cacheEntry->staticData->data, width * height);
            sub_42EE84(gMouseManagerCurrentStaticData, palette, width, height);
            mouseSetFrame(gMouseManagerCurrentStaticData, width, height, width, v1, v2, 0);
            gMouseManagerIsAnimating = false;
            break;
        case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
            gMouseManagerCurrentAnimatedData = cacheEntry->animatedData;
            gMouseManagerCurrentPalette = palette;
            gMouseManagerCurrentAnimatedData->field_26 = 0;
            gMouseManagerCurrentAnimatedData->field_24 = 0;
            mouseSetFrame(gMouseManagerCurrentAnimatedData->field_0[0],
                gMouseManagerCurrentAnimatedData->width,
                gMouseManagerCurrentAnimatedData->height,
                gMouseManagerCurrentAnimatedData->width,
                gMouseManagerCurrentAnimatedData->field_8[0],
                gMouseManagerCurrentAnimatedData->field_C[0],
                0);
            gMouseManagerIsAnimating = true;
            break;
        }
        return true;
    }

    char* dot = strrchr(fileName, '.');
    if (dot != nullptr && compat_stricmp(dot + 1, "mou") == 0) {
        return mouseManagerSetMouseShape(fileName, 0, 0);
    }

    char* mangledFileName = gMouseManagerNameMangler(fileName);
    File* stream = fileOpen(mangledFileName, "r");
    if (stream == nullptr) {
        debugPrint("Can't find %s\n", mangledFileName);
        return false;
    }

    char string[80];
    string[0] = '\0';
    fileReadString(string, sizeof(string) - 1, stream);
    if (string[0] == '\0') {
        return false;
    }

    bool rc;
    if (compat_strnicmp(string, "anim", 4) == 0) {
        fileClose(stream);
        rc = mouseManagerSetFrame(fileName, 0);
    } else {
        // NOTE: Uninline.
        char* sep = strchr(string, ' ');
        if (sep != nullptr) {
            return 0;
        }

        *sep = '\0';

        int v3;
        int v4;
        sscanf(sep + 1, "%d %d", &v3, &v4);

        fileClose(stream);

        rc = mouseManagerSetMouseShape(string, v3, v4);
    }

    strncpy(gMouseManagerCache[gMouseManagerCurrentCacheEntryIndex].field_32C, fileName, 31);

    return rc;
}

// 0x4862AC
void mouseManagerResetMouse()
{
    MouseManagerCacheEntry* entry = &(gMouseManagerCache[gMouseManagerCurrentCacheEntryIndex]);

    int imageWidth;
    int imageHeight;
    switch (entry->type) {
    case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
        imageWidth = entry->staticData->width;
        imageHeight = entry->staticData->height;
        break;
    case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
        imageWidth = entry->animatedData->width;
        imageHeight = entry->animatedData->height;
        break;
    }

    switch (entry->type) {
    case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
        if (gMouseManagerCurrentStaticData != nullptr) {
            if (gMouseManagerCurrentStaticData != nullptr) {
                internal_free_safe(gMouseManagerCurrentStaticData, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 572
            }

            gMouseManagerCurrentStaticData = (unsigned char*)internal_malloc_safe(imageWidth * imageHeight, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 574
            memcpy(gMouseManagerCurrentStaticData, entry->staticData->data, imageWidth * imageHeight);
            sub_42EE84(gMouseManagerCurrentStaticData, entry->palette, imageWidth, imageHeight);

            mouseSetFrame(gMouseManagerCurrentStaticData,
                imageWidth,
                imageHeight,
                imageWidth,
                entry->staticData->field_4,
                entry->staticData->field_8,
                0);
        } else {
            debugPrint("Hm, current mouse type is M_STATIC, but no current mouse pointer\n");
        }
        break;
    case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
        if (gMouseManagerCurrentAnimatedData != nullptr) {
            for (int index = 0; index < gMouseManagerCurrentAnimatedData->frameCount; index++) {
                memcpy(gMouseManagerCurrentAnimatedData->field_0[index], gMouseManagerCurrentAnimatedData->field_4[index], imageWidth * imageHeight);
                sub_42EE84(gMouseManagerCurrentAnimatedData->field_0[index], entry->palette, imageWidth, imageHeight);
            }

            mouseSetFrame(gMouseManagerCurrentAnimatedData->field_0[gMouseManagerCurrentAnimatedData->field_26],
                imageWidth,
                imageHeight,
                imageWidth,
                gMouseManagerCurrentAnimatedData->field_8[gMouseManagerCurrentAnimatedData->field_26],
                gMouseManagerCurrentAnimatedData->field_C[gMouseManagerCurrentAnimatedData->field_26],
                0);
        } else {
            debugPrint("Hm, current mouse type is M_ANIMATED, but no current mouse pointer\n");
        }
    }
}

// 0x4865C4
void mouseManagerHideMouse()
{
    mouseHideCursor();
}

// 0x4865CC
void mouseManagerShowMouse()
{
    mouseShowCursor();
}

} // namespace fallout
