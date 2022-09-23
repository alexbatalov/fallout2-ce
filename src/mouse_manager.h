#ifndef MOUSE_MANAGER_H
#define MOUSE_MANAGER_H

namespace fallout {

#define MOUSE_MGR_CACHE_CAPACITY 32

typedef char*(MouseManagerNameMangler)(char* fileName);
typedef int(MouseManagerRateProvider)();
typedef int(MouseManagerTimeProvider)();

typedef enum MouseManagerMouseType {
    MOUSE_MANAGER_MOUSE_TYPE_NONE,
    MOUSE_MANAGER_MOUSE_TYPE_STATIC,
    MOUSE_MANAGER_MOUSE_TYPE_ANIMATED,
} MouseManagerMouseType;

typedef struct MouseManagerStaticData {
    unsigned char* data;
    int field_4;
    int field_8;
    int width;
    int height;
} MouseManagerStaticData;

typedef struct MouseManagerAnimatedData {
    unsigned char** field_0;
    unsigned char** field_4;
    int* field_8;
    int* field_C;
    int width;
    int height;
    float field_18;
    int field_1C;
    int field_20;
    signed char field_24;
    signed char frameCount;
    signed char field_26;
} MouseManagerAnimatedData;

typedef struct MouseManagerCacheEntry {
    union {
        void* data;
        MouseManagerStaticData* staticData;
        MouseManagerAnimatedData* animatedData;
    };
    int type;
    unsigned char palette[256 * 3];
    int ref;
    char fileName[32];
    char field_32C[32];
} MouseManagerCacheEntry;

extern MouseManagerNameMangler* gMouseManagerNameMangler;
extern MouseManagerRateProvider* gMouseManagerRateProvider;
extern MouseManagerTimeProvider* gMouseManagerTimeProvider;

extern MouseManagerCacheEntry gMouseManagerCache[MOUSE_MGR_CACHE_CAPACITY];
extern bool gMouseManagerIsAnimating;
extern unsigned char* gMouseManagerCurrentPalette;
extern MouseManagerAnimatedData* gMouseManagerCurrentAnimatedData;
extern unsigned char* gMouseManagerCurrentStaticData;
extern int gMouseManagerCurrentCacheEntryIndex;

char* mouseManagerNameManglerDefaultImpl(char* a1);
int mouseManagerRateProviderDefaultImpl();
int mouseManagerTimeProviderDefaultImpl();
void mouseManagerSetNameMangler(MouseManagerNameMangler* func);
void mouseManagerFreeCacheEntry(MouseManagerCacheEntry* entry);
int mouseManagerInsertCacheEntry(void** data, int type, unsigned char* palette, const char* fileName);
void mouseManagerFlushCache();
MouseManagerCacheEntry* mouseManagerFindCacheEntry(const char* fileName, unsigned char** palettePtr, int* a3, int* a4, int* widthPtr, int* heightPtr, int* typePtr);
void mouseManagerInit();
void mouseManagerExit();
void mouseManagerUpdate();
int mouseManagerSetFrame(char* fileName, int a2);
bool mouseManagerSetMouseShape(char* fileName, int a2, int a3);
bool mouseManagerSetMousePointer(char* fileName);
void mouseManagerResetMouse();
void mouseManagerHideMouse();
void mouseManagerShowMouse();

} // namespace fallout

#endif /* MOUSE_MANAGER_H */
