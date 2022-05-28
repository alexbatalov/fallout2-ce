#ifndef SKILLDEX_H
#define SKILLDEX_H

#include "art.h"
#include "geometry.h"
#include "message.h"

#define SKILLDEX_SKILL_BUTTON_BUFFER_COUNT (SKILLDEX_SKILL_COUNT * 2)

typedef enum SkilldexFrm {
    SKILLDEX_FRM_BACKGROUND,
    SKILLDEX_FRM_BUTTON_ON,
    SKILLDEX_FRM_BUTTON_OFF,
    SKILLDEX_FRM_LITTLE_RED_BUTTON_UP,
    SKILLDEX_FRM_LITTLE_RED_BUTTON_DOWN,
    SKILLDEX_FRM_BIG_NUMBERS,
    SKILLDEX_FRM_COUNT,
} SkilldexFrm;

typedef enum SkilldexSkill {
    SKILLDEX_SKILL_SNEAK,
    SKILLDEX_SKILL_LOCKPICK,
    SKILLDEX_SKILL_STEAL,
    SKILLDEX_SKILL_TRAPS,
    SKILLDEX_SKILL_FIRST_AID,
    SKILLDEX_SKILL_DOCTOR,
    SKILLDEX_SKILL_SCIENCE,
    SKILLDEX_SKILL_REPAIR,
    SKILLDEX_SKILL_COUNT,
} SkilldexSkill;

typedef enum SkilldexRC {
    SKILLDEX_RC_ERROR = -1,
    SKILLDEX_RC_CANCELED,
    SKILLDEX_RC_SNEAK,
    SKILLDEX_RC_LOCKPICK,
    SKILLDEX_RC_STEAL,
    SKILLDEX_RC_TRAPS,
    SKILLDEX_RC_FIRST_AID,
    SKILLDEX_RC_DOCTOR,
    SKILLDEX_RC_SCIENCE,
    SKILLDEX_RC_REPAIR,
} SkilldexRC;

extern bool gSkilldexWindowIsoWasEnabled;
extern const int gSkilldexFrmIds[SKILLDEX_FRM_COUNT];
extern const int gSkilldexSkills[SKILLDEX_SKILL_COUNT];

extern Size gSkilldexFrmSizes[SKILLDEX_FRM_COUNT];
extern unsigned char* gSkilldexButtonsData[SKILLDEX_SKILL_BUTTON_BUFFER_COUNT];
extern MessageList gSkilldexMessageList;
extern MessageListItem gSkilldexMessageListItem;
extern unsigned char* gSkilldexFrmData[SKILLDEX_FRM_COUNT];
extern CacheEntry* gSkilldexFrmHandles[SKILLDEX_FRM_COUNT];
extern int gSkilldexWindow;
extern unsigned char* gSkilldexWindowBuffer;
extern int gSkilldexWindowOldFont;

int skilldexOpen();
int skilldexWindowInit();
void skilldexWindowFree();

#endif /* SKILLDEX_H */
