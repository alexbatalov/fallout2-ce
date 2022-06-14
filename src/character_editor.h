#ifndef CHARACTER_EDITOR_H
#define CHARACTER_EDITOR_H

#include "art.h"
#include "db.h"
#include "geometry.h"
#include "message.h"
#include "perk_defs.h"
#include "proto_types.h"
#include "skill_defs.h"
#include "stat_defs.h"
#include "trait_defs.h"

// TODO: Should be MAX(PERK_COUNT, TRAIT_COUNT).
#define DIALOG_PICKER_NUM_OPTIONS PERK_COUNT
#define TOWN_REPUTATION_COUNT 19
#define ADDICTION_REPUTATION_COUNT 8

typedef enum EditorFolder {
    EDITOR_FOLDER_PERKS,
    EDITOR_FOLDER_KARMA,
    EDITOR_FOLDER_KILLS,
} EditorFolder;

enum {
    EDITOR_DERIVED_STAT_ARMOR_CLASS,
    EDITOR_DERIVED_STAT_ACTION_POINTS,
    EDITOR_DERIVED_STAT_CARRY_WEIGHT,
    EDITOR_DERIVED_STAT_MELEE_DAMAGE,
    EDITOR_DERIVED_STAT_DAMAGE_RESISTANCE,
    EDITOR_DERIVED_STAT_POISON_RESISTANCE,
    EDITOR_DERIVED_STAT_RADIATION_RESISTANCE,
    EDITOR_DERIVED_STAT_SEQUENCE,
    EDITOR_DERIVED_STAT_HEALING_RATE,
    EDITOR_DERIVED_STAT_CRITICAL_CHANCE,
    EDITOR_DERIVED_STAT_COUNT,
};

enum {
    EDITOR_FIRST_PRIMARY_STAT,
    EDITOR_HIT_POINTS = 43,
    EDITOR_POISONED,
    EDITOR_RADIATED,
    EDITOR_EYE_DAMAGE,
    EDITOR_CRIPPLED_RIGHT_ARM,
    EDITOR_CRIPPLED_LEFT_ARM,
    EDITOR_CRIPPLED_RIGHT_LEG,
    EDITOR_CRIPPLED_LEFT_LEG,
    EDITOR_FIRST_DERIVED_STAT,
    EDITOR_FIRST_SKILL = EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_COUNT,
    EDITOR_TAG_SKILL = EDITOR_FIRST_SKILL + SKILL_COUNT,
    EDITOR_SKILLS,
    EDITOR_OPTIONAL_TRAITS,
    EDITOR_FIRST_TRAIT,
    EDITOR_BUTTONS_COUNT = EDITOR_FIRST_TRAIT + TRAIT_COUNT,
};

enum {
    EDITOR_GRAPHIC_BIG_NUMBERS,
    EDITOR_GRAPHIC_AGE_MASK,
    EDITOR_GRAPHIC_AGE_OFF,
    EDITOR_GRAPHIC_DOWN_ARROW_OFF,
    EDITOR_GRAPHIC_DOWN_ARROW_ON,
    EDITOR_GRAPHIC_NAME_MASK,
    EDITOR_GRAPHIC_NAME_ON,
    EDITOR_GRAPHIC_NAME_OFF,
    EDITOR_GRAPHIC_FOLDER_MASK, // mask for all three folders
    EDITOR_GRAPHIC_SEX_MASK,
    EDITOR_GRAPHIC_SEX_OFF,
    EDITOR_GRAPHIC_SEX_ON,
    EDITOR_GRAPHIC_SLIDER, // image containing small plus/minus buttons appeared near selected skill
    EDITOR_GRAPHIC_SLIDER_MINUS_OFF,
    EDITOR_GRAPHIC_SLIDER_MINUS_ON,
    EDITOR_GRAPHIC_SLIDER_PLUS_OFF,
    EDITOR_GRAPHIC_SLIDER_PLUS_ON,
    EDITOR_GRAPHIC_SLIDER_TRANS_MINUS_OFF,
    EDITOR_GRAPHIC_SLIDER_TRANS_MINUS_ON,
    EDITOR_GRAPHIC_SLIDER_TRANS_PLUS_OFF,
    EDITOR_GRAPHIC_SLIDER_TRANS_PLUS_ON,
    EDITOR_GRAPHIC_UP_ARROW_OFF,
    EDITOR_GRAPHIC_UP_ARROW_ON,
    EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP,
    EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN,
    EDITOR_GRAPHIC_AGE_ON,
    EDITOR_GRAPHIC_AGE_BOX, // image containing right and left buttons with age stepper in the middle
    EDITOR_GRAPHIC_ATTRIBOX, // ??? black image with two little arrows (up and down) in the right-top corner
    EDITOR_GRAPHIC_ATTRIBWN, // ??? not sure where and when it's used
    EDITOR_GRAPHIC_CHARWIN, // ??? looks like metal plate
    EDITOR_GRAPHIC_DONE_BOX, // metal plate holding DONE button
    EDITOR_GRAPHIC_FEMALE_OFF,
    EDITOR_GRAPHIC_FEMALE_ON,
    EDITOR_GRAPHIC_MALE_OFF,
    EDITOR_GRAPHIC_MALE_ON,
    EDITOR_GRAPHIC_NAME_BOX, // placeholder for name
    EDITOR_GRAPHIC_LEFT_ARROW_UP,
    EDITOR_GRAPHIC_LEFT_ARROW_DOWN,
    EDITOR_GRAPHIC_RIGHT_ARROW_UP,
    EDITOR_GRAPHIC_RIGHT_ARROW_DOWN,
    EDITOR_GRAPHIC_BARARRWS, // ??? two arrows up/down with some strange knob at the top, probably for scrollbar
    EDITOR_GRAPHIC_OPTIONS_BASE, // options metal plate
    EDITOR_GRAPHIC_OPTIONS_BUTTON_OFF,
    EDITOR_GRAPHIC_OPTIONS_BUTTON_ON,
    EDITOR_GRAPHIC_KARMA_FOLDER_SELECTED, // all three folders with middle folder selected (karma)
    EDITOR_GRAPHIC_KILLS_FOLDER_SELECTED, // all theee folders with right folder selected (kills)
    EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED, // all three folders with left folder selected (perks)
    EDITOR_GRAPHIC_KARMAFDR_PLACEOLDER, // ??? placeholder for traits folder image <- this is comment from intrface.lst
    EDITOR_GRAPHIC_TAG_SKILL_BUTTON_OFF,
    EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON,
    EDITOR_GRAPHIC_COUNT,
};

typedef struct KarmaEntry {
    int gvar;
    int art_num;
    int name;
    int description;
} KarmaEntry;

typedef struct GenericReputationEntry {
    int threshold;
    int name;
} GenericReputationEntry;

typedef struct TownReputationEntry {
    int gvar;
    int city;
} TownReputationEntry;

typedef struct PerkDialogOption {
    // Depending on the current mode this value is the id of either
    // perk, trait (handling Mutate perk), or skill (handling Tag perk).
    int value;
    char* name;
} PerkDialogOption;

// TODO: Field order is probably wrong.
typedef struct KillInfo {
    const char* name;
    int killTypeId;
    int kills;
} KillInfo;

extern int gCharacterEditorFrmIds[50];
extern const unsigned char gCharacterEditorFrmShouldCopy[EDITOR_GRAPHIC_COUNT];
extern const int gCharacterEditorDerivedStatFrmIds[EDITOR_DERIVED_STAT_COUNT];
extern const int gCharacterEditorPrimaryStatY[7];
extern const int gCharacterEditorDerivedStatsMap[EDITOR_DERIVED_STAT_COUNT];
extern char byte_431D93[64];
extern const int dword_431DD4[7];

extern const double dbl_50170B;
extern const double dbl_501713;
extern const double dbl_5018F0;
extern const double dbl_5019BE;

extern bool gCharacterEditorIsoWasEnabled;
extern int gCharacterEditorCurrentSkill;
extern int gCharacterEditorSkillValueAdjustmentSliderY;
extern int gCharacterEditorRemainingCharacterPoints;
extern KarmaEntry* gKarmaEntries;
extern int gKarmaEntriesLength;
extern GenericReputationEntry* gGenericReputationEntries;
extern int gGenericReputationEntriesLength;
extern const TownReputationEntry gTownReputationEntries[TOWN_REPUTATION_COUNT];
extern const int gAddictionReputationVars[ADDICTION_REPUTATION_COUNT];
extern const int gAddictionReputationFrmIds[ADDICTION_REPUTATION_COUNT];
extern int gCharacterEditorFolderViewScrollUpBtn;
extern int gCharacterEditorFolderViewScrollDownBtn;

extern char gCharacterEditorFolderCardString[256];
extern int gCharacterEditorSkillsBackup[SKILL_COUNT];
extern MessageList gCharacterEditorMessageList;
extern PerkDialogOption gPerkDialogOptionList[DIALOG_PICKER_NUM_OPTIONS];
extern int gCharacterEditorOptionalTraitBtns[TRAIT_COUNT];
extern MessageListItem gCharacterEditorMessageListItem;
extern char gCharacterEditorCardTitle[48];
extern char gPerkDialogCardTitle[48];
extern int gCharacterEditorTagSkillBtns[SKILL_COUNT];
extern char gCharacterEditorNameBackup[32];
extern Size gCharacterEditorFrmSize[EDITOR_GRAPHIC_COUNT];
extern CacheEntry* gCharacterEditorFrmHandle[EDITOR_GRAPHIC_COUNT];
extern unsigned char* gCharacterEditorFrmCopy[EDITOR_GRAPHIC_COUNT];
extern unsigned char* gCharacterEditorFrmData[EDITOR_GRAPHIC_COUNT];
extern int gCharacterEditorFolderViewMaxLines;
extern int gCharacterEditorFolderViewCurrentLine;
extern int gCharacterEditorFolderCardFrmId;
extern int gCharacterEditorFolderViewTopLine;
extern char* gCharacterEditorFolderCardTitle;
extern char* gCharacterEditorFolderCardSubtitle;
extern int gCharacterEditorFolderViewOffsetY;
extern int gCharacterEditorKarmaFolderTopLine;
extern int gCharacterEditorFolderViewHighlightedLine;
extern char* gCharacterEditorFolderCardDescription;
extern int gCharacterEditorFolderViewNextY;
extern int gCharacterEditorKillsFolderTopLine;
extern int gCharacterEditorPerkFolderTopLine;
extern unsigned char* gPerkDialogBackgroundBuffer;
extern int gPerkDialogWindow;
extern int gCharacterEditorSliderPlusBtn;
extern int gCharacterEditorSliderMinusBtn;
extern int gCharacterEditorPrimaryStatMinusBtns[7];
extern unsigned char* gCharacterEditorWindowBuffer;
extern int gCharacterEditorWindow;
extern int gCharacterEditorPrimaryStatPlusBtns[7];
extern unsigned char* gPerkDialogWindowBuffer;
extern CritterProtoData gCharacterEditorDudeDataBackup;
extern unsigned char* gCharacterEditorWindowBackgroundBuffer;
extern int gPerkDialogCurrentLine;
extern int gPerkDialogPreviousCurrentLine;
extern int gCharacterEditorUnspentSkillPointsBackup;
extern int gCharacterEditorLastLevel;
extern int gCharacterEditorOldFont;
extern int gCharacterEditorKillsCount;
extern CacheEntry* gCharacterEditorWindowBackgroundHandle;
extern int gCharacterEditorHitPointsBackup;
extern int gCharacterEditorMouseY;
extern int gCharacterEditorMouseX;
extern int characterEditorSelectedItem;
extern int characterEditorWindowSelectedFolder;
extern bool gCharacterEditorCardDrawn;
extern int gPerkDialogTopLine;
extern bool gPerkDialogCardDrawn;
extern int gCharacterEditorPerksBackup[PERK_COUNT];
extern unsigned int _repFtime;
extern unsigned int _frame_time;
extern int gCharacterEditorOldTaggedSkillCount;
extern int gCharacterEditorLastLevelBackup;
extern bool gCharacterEditorIsCreationMode;
extern int gCharacterEditorTaggedSkillsBackup[NUM_TAGGED_SKILLS];
extern int gPerkDialogCardFrmId;
extern int gCharacterEditorCardFrmId;
extern int gCharacterEditorOptionalTraitsBackup[3];
extern int gCharacterEditorTempTraitCount;
extern int gPerkDialogOptionCount;
extern int gCharacterEditorTempTraits[3];
extern int gCharacterEditorTaggedSkillCount;
extern int gCharacterEditorTempTaggedSkills[NUM_TAGGED_SKILLS];
extern char gCharacterEditorHasFreePerkBackup;
extern unsigned char gCharacterEditorHasFreePerk;
extern unsigned char gCharacterEditorIsSkillsFirstDraw;

int characterEditorShow(bool isCreationMode);
int characterEditorWindowInit();
void characterEditorWindowFree();
void characterEditorInit();
int _get_input_str(int win, int cancelKeyCode, char* text, int maxLength, int x, int y, int textColor, int backgroundColor, int flags);
bool _isdoschar(int ch);
char* _strmfe(char* dest, const char* name, const char* ext);
void characterEditorDrawFolders();
void characterEditorDrawPerksFolder();
int characterEditorKillsCompare(const void* a1, const void* a2);
int characterEditorDrawKillsFolder();
void characterEditorDrawBigNumber(int x, int y, int flags, int value, int previousValue, int windowHandle);
void characterEditorDrawPcStats();
void characterEditorDrawPrimaryStat(int stat, bool animate, int previousValue);
void characterEditorDrawGender();
void characterEditorDrawAge();
void characterEditorDrawName();
void characterEditorDrawDerivedStats();
void characterEditorDrawSkills(int a1);
void characterEditorDrawCard();
int characterEditorEditName();
void _PrintName(unsigned char* buf, int pitch);
int characterEditorEditAge();
void characterEditorEditGender();
void characterEditorAdjustPrimaryStat(int eventCode);
int characterEditorShowOptions();
bool characterFileExists(const char* fname);
int characterPrintToFile(const char* fileName);
char* _AddSpaces(char* string, int length);
char* _AddDots(char* string, int length);
void characterEditorResetScreen();
void characterEditorRegisterInfoAreas();
void characterEditorSavePlayer();
void characterEditorRestorePlayer();
char* _itostndn(int value, char* dest);
int characterEditorDrawCardWithOptions(int graphicId, const char* name, const char* attributes, char* description);
void characterEditorHandleFolderButtonPressed();
void characterEditorHandleInfoButtonPressed(int eventCode);
void characterEditorHandleAdjustSkillButtonPressed(int a1);
void characterEditorToggleTaggedSkill(int skill);
void characterEditorDrawOptionalTraits();
void characterEditorToggleOptionalTrait(int trait);
void characterEditorDrawKarmaFolder();
int characterEditorSave(File* stream);
int characterEditorLoad(File* stream);
void characterEditorReset();
int characterEditorUpdateLevel();
void perkDialogRefreshPerks();
int perkDialogShow();
int perkDialogHandleInput(int count, void (*refreshProc)());
int perkDialogDrawPerks();
void perkDialogRefreshTraits();
bool perkDialogHandleMutatePerk();
void perkDialogRefreshSkills();
bool perkDialogHandleTagPerk();
void perkDialogDrawSkills();
int perkDialogDrawTraits(int a1);
int perkDialogOptionCompare(const void* a1, const void* a2);
int perkDialogDrawCard(int frmId, const char* name, const char* rank, char* description);
void _pop_perks();
int _is_supper_bonus();
int characterEditorFolderViewInit();
void characterEditorFolderViewScroll(int direction);
void characterEditorFolderViewClear();
int characterEditorFolderViewDrawHeading(const char* string);
bool characterEditorFolderViewDrawString(const char* string);
bool characterEditorFolderViewDrawKillsEntry(const char* name, int kills);
int karmaInit();
void karmaFree();
int karmaEntryCompare(const void* a1, const void* a2);
int genericReputationInit();
void genericReputationFree();
int genericReputationCompare(const void* a1, const void* a2);

void customKarmaFolderInit();
void customKarmaFolderFree();
int customKarmaFolderGetFrmId();

#endif /* CHARACTER_EDITOR_H */
