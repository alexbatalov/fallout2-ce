#ifndef CHARACTER_SELECTOR_H
#define CHARACTER_SELECTOR_H

#include "art.h"

#define CS_WINDOW_WIDTH (640)
#define CS_WINDOW_HEIGHT (480)

#define CS_WINDOW_BACKGROUND_X (40)
#define CS_WINDOW_BACKGROUND_Y (30)
#define CS_WINDOW_BACKGROUND_WIDTH (560)
#define CS_WINDOW_BACKGROUND_HEIGHT (300)

#define CS_WINDOW_PREVIOUS_BUTTON_X (292)
#define CS_WINDOW_PREVIOUS_BUTTON_Y (320)

#define CS_WINDOW_NEXT_BUTTON_X (318)
#define CS_WINDOW_NEXT_BUTTON_Y (320)

#define CS_WINDOW_TAKE_BUTTON_X (81)
#define CS_WINDOW_TAKE_BUTTON_Y (323)

#define CS_WINDOW_MODIFY_BUTTON_X (435)
#define CS_WINDOW_MODIFY_BUTTON_Y (320)

#define CS_WINDOW_CREATE_BUTTON_X (80)
#define CS_WINDOW_CREATE_BUTTON_Y (425)

#define CS_WINDOW_BACK_BUTTON_X (461)
#define CS_WINDOW_BACK_BUTTON_Y (425)

#define CS_WINDOW_NAME_MID_X (318)
#define CS_WINDOW_PRIMARY_STAT_MID_X (362)
#define CS_WINDOW_SECONDARY_STAT_MID_X (379)
#define CS_WINDOW_BIO_X (438)

typedef enum PremadeCharacter {
    PREMADE_CHARACTER_NARG,
    PREMADE_CHARACTER_CHITSA,
    PREMADE_CHARACTER_MINGUN,
    PREMADE_CHARACTER_COUNT,
} PremadeCharacter;

typedef struct PremadeCharacterDescription {
    char fileName[20];
    int face;
    char field_18[20];
} PremadeCharacterDescription;

extern int gCurrentPremadeCharacter;
extern PremadeCharacterDescription gPremadeCharacterDescriptions[PREMADE_CHARACTER_COUNT];
extern const int gPremadeCharacterCount;

extern int gCharacterSelectorWindow;
extern unsigned char* gCharacterSelectorWindowBuffer;
extern unsigned char* gCharacterSelectorBackground;
extern int gCharacterSelectorWindowPreviousButton;
extern CacheEntry* gCharacterSelectorWindowPreviousButtonDownFrmHandle;
extern CacheEntry* gCharacterSelectorWindowPreviousButtonUpFrmHandle;
extern int gCharacterSelectorWindowNextButton;
extern CacheEntry* gCharacterSelectorWindowNextButtonUpFrmHandle;
extern CacheEntry* gCharacterSelectorWindowNextButtonDownFrmHandle;
extern int gCharacterSelectorWindowTakeButton;
extern CacheEntry* gCharacterSelectorWindowTakeButtonUpFrmHandle;
extern CacheEntry* gCharacterSelectorWindowTakeButtonDownFrmHandle;
extern int gCharacterSelectorWindowModifyButton;
extern CacheEntry* gCharacterSelectorWindowModifyButtonUpFrmHandle;
extern CacheEntry* gCharacterSelectorWindowModifyButtonDownFrmHandle;
extern int gCharacterSelectorWindowCreateButton;
extern CacheEntry* gCharacterSelectorWindowCreateButtonUpFrmHandle;
extern CacheEntry* gCharacterSelectorWindowCreateButtonDownFrmHandle;
extern int gCharacterSelectorWindowBackButton;
extern CacheEntry* gCharacterSelectorWindowBackButtonUpFrmHandle;
extern CacheEntry* gCharacterSelectorWindowBackButtonDownFrmHandle;

extern unsigned char* gCharacterSelectorWindowTakeButtonUpFrmData;
extern unsigned char* gCharacterSelectorWindowModifyButtonDownFrmData;
extern unsigned char* gCharacterSelectorWindowBackButtonUpFrmData;
extern unsigned char* gCharacterSelectorWindowCreateButtonUpFrmData;
extern unsigned char* gCharacterSelectorWindowModifyButtonUpFrmData;
extern unsigned char* gCharacterSelectorWindowBackButtonDownFrmData;
extern unsigned char* gCharacterSelectorWindowCreateButtonDownFrmData;
extern unsigned char* gCharacterSelectorWindowTakeButtonDownFrmData;
extern unsigned char* gCharacterSelectorWindowNextButtonDownFrmData;
extern unsigned char* gCharacterSelectorWindowNextButtonUpFrmData;
extern unsigned char* gCharacterSelectorWindowPreviousButtonUpFrmData;
extern unsigned char* gCharacterSelectorWindowPreviousButtonDownFrmData;

int characterSelectorOpen();
bool characterSelectorWindowInit();
void characterSelectorWindowFree();
bool characterSelectorWindowRefresh();
bool characterSelectorWindowRenderFace();
bool characterSelectorWindowRenderStats();
bool characterSelectorWindowRenderBio();

#endif /* CHARACTER_SELECTOR_H */
