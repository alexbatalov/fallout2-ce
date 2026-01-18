#include "character_editor.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "art.h"
#include "color.h"
#include "combat.h"
#include "critter.h"
#include "cycle.h"
#include "db.h"
#include "dbox.h"
#include "debug.h"
#include "delay.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "geometry.h"
#include "graph_lib.h"
#include "input.h"
#include "interface.h"
#include "item.h"
#include "kb.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "object.h"
#include "palette.h"
#include "perk.h"
#include "platform_compat.h"
#include "proto.h"
#include "scripts.h"
#include "sfall_config.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "trait.h"
#include "window_manager.h"
#include "word_wrap.h"
#include "worldmap.h"

namespace fallout {

#define RENDER_ALL_STATS 7

#define EDITOR_WINDOW_WIDTH 640
#define EDITOR_WINDOW_HEIGHT 480

#define NAME_BUTTON_X 9
#define NAME_BUTTON_Y 0

#define TAG_SKILLS_BUTTON_X 347
#define TAG_SKILLS_BUTTON_Y 26
#define TAG_SKILLS_BUTTON_CODE 536

#define PRINT_BTN_X 363
#define PRINT_BTN_Y 454

#define DONE_BTN_X 475
#define DONE_BTN_Y 454

#define CANCEL_BTN_X 571
#define CANCEL_BTN_Y 454

#define NAME_BTN_CODE 517
#define AGE_BTN_CODE 519
#define SEX_BTN_CODE 520

#define OPTIONAL_TRAITS_LEFT_BTN_X 23
#define OPTIONAL_TRAITS_RIGHT_BTN_X 298
#define OPTIONAL_TRAITS_BTN_Y 352

#define OPTIONAL_TRAITS_BTN_CODE 555

#define OPTIONAL_TRAITS_BTN_SPACE 2

#define SPECIAL_STATS_BTN_X 149

#define PERK_WINDOW_X 33
#define PERK_WINDOW_Y 91
#define PERK_WINDOW_WIDTH 573
#define PERK_WINDOW_HEIGHT 230

#define PERK_WINDOW_LIST_X 45
#define PERK_WINDOW_LIST_Y 43
#define PERK_WINDOW_LIST_WIDTH 192
#define PERK_WINDOW_LIST_HEIGHT 129

#define ANIMATE 0x01
#define RED_NUMBERS 0x02
#define BIG_NUM_WIDTH 14
#define BIG_NUM_HEIGHT 24
#define BIG_NUM_ANIMATION_DELAY 123

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

static int characterEditorWindowInit();
static void characterEditorWindowFree();
static int _get_input_str(int win, int cancelKeyCode, char* text, int maxLength, int x, int y, int textColor, int backgroundColor, int flags);
static void characterEditorDrawFolders();
static void characterEditorDrawPerksFolder();
static int characterEditorKillsCompare(const void* a1, const void* a2);
static int characterEditorDrawKillsFolder();
static void characterEditorDrawBigNumber(int x, int y, int flags, int value, int previousValue, int windowHandle);
static void characterEditorDrawPcStats();
static void characterEditorDrawPrimaryStat(int stat, bool animate, int previousValue);
static void characterEditorDrawGender();
static void characterEditorDrawAge();
static void characterEditorDrawName();
static void characterEditorDrawDerivedStats();
static void characterEditorDrawSkills(int a1);
static void characterEditorDrawCard();
static int characterEditorEditName();
static void _PrintName(unsigned char* buf, int pitch);
static int characterEditorEditAge();
static void characterEditorEditGender();
static void characterEditorAdjustPrimaryStat(int eventCode);
static int characterEditorShowOptions();
static bool characterFileExists(const char* fname);
static int characterPrintToFile(const char* fileName);
static char* _AddSpaces(char* string, int length);
static char* _AddDots(char* string, int length);
static void characterEditorResetScreen();
static void characterEditorRegisterInfoAreas();
static void characterEditorSavePlayer();
static void characterEditorRestorePlayer();
static char* _itostndn(int value, char* dest);
static int characterEditorDrawCardWithOptions(int graphicId, const char* name, const char* attributes, char* description);
static void characterEditorHandleFolderButtonPressed();
static void characterEditorHandleInfoButtonPressed(int eventCode);
static void characterEditorHandleAdjustSkillButtonPressed(int a1);
static void characterEditorToggleTaggedSkill(int skill);
static void characterEditorDrawOptionalTraits();
static void characterEditorToggleOptionalTrait(int trait);
static void characterEditorDrawKarmaFolder();
static int characterEditorUpdateLevel();
static void perkDialogRefreshPerks();
static int perkDialogShow();
static int perkDialogHandleInput(int count, void (*refreshProc)());
static int perkDialogDrawPerks();
static void perkDialogRefreshTraits();
static bool perkDialogHandleMutatePerk();
static void perkDialogRefreshSkills();
static bool perkDialogHandleTagPerk();
static void perkDialogDrawSkills();
static int perkDialogDrawTraits(int a1);
static int perkDialogOptionCompare(const void* a1, const void* a2);
static int perkDialogDrawCard(int frmId, const char* name, const char* rank, char* description);
static void _pop_perks();
static int _is_supper_bonus();
static int characterEditorFolderViewInit();
static void characterEditorFolderViewScroll(int direction);
static void characterEditorFolderViewClear();
static int characterEditorFolderViewDrawHeading(const char* string);
static bool characterEditorFolderViewDrawString(const char* string);
static bool characterEditorFolderViewDrawKillsEntry(const char* name, int kills);
static int karmaInit();
static void karmaFree();
static int karmaEntryCompare(const void* a1, const void* a2);
static int genericReputationInit();
static void genericReputationFree();
static int genericReputationCompare(const void* a1, const void* a2);

static void customKarmaFolderInit();
static void customKarmaFolderFree();
static int customKarmaFolderGetFrmId();

static void customTownReputationInit();
static void customTownReputationFree();

// 0x431C40
static int gCharacterEditorFrmIds[EDITOR_GRAPHIC_COUNT] = {
    170,
    175,
    176,
    181,
    182,
    183,
    184,
    185,
    186,
    187,
    188,
    189,
    190,
    191,
    192,
    193,
    194,
    195,
    196,
    197,
    198,
    199,
    200,
    8,
    9,
    204,
    205,
    206,
    207,
    208,
    209,
    210,
    211,
    212,
    213,
    214,
    122,
    123,
    124,
    125,
    219,
    220,
    221,
    222,
    178,
    179,
    180,
    38,
    215,
    216,
};

// flags to preload fid
//
// 0x431D08
static const unsigned char gCharacterEditorFrmShouldCopy[EDITOR_GRAPHIC_COUNT] = {
    0,
    0,
    1,
    0,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    0,
    0,
};

// graphic ids for derived stats panel
// NOTE: the type originally short
//
// 0x431D3A
static const int gCharacterEditorDerivedStatFrmIds[EDITOR_DERIVED_STAT_COUNT] = {
    18,
    19,
    20,
    21,
    22,
    23,
    83,
    24,
    25,
    26,
};

// y offsets for stats +/- buttons
//
// 0x431D50
static const int gCharacterEditorPrimaryStatY[7] = {
    37,
    70,
    103,
    136,
    169,
    202,
    235,
};

// stat ids for derived stats panel
// NOTE: the type is originally short
//
// 0x431D6
static const int gCharacterEditorDerivedStatsMap[EDITOR_DERIVED_STAT_COUNT] = {
    STAT_ARMOR_CLASS,
    STAT_MAXIMUM_ACTION_POINTS,
    STAT_CARRY_WEIGHT,
    STAT_MELEE_DAMAGE,
    STAT_DAMAGE_RESISTANCE,
    STAT_POISON_RESISTANCE,
    STAT_RADIATION_RESISTANCE,
    STAT_SEQUENCE,
    STAT_HEALING_RATE,
    STAT_CRITICAL_CHANCE,
};

// 0x431D93
static char byte_431D93[64];

// 0x431DD4
static const int dword_431DD4[7] = {
    1000000,
    100000,
    10000,
    1000,
    100,
    10,
    1,
};

// 0x5016E4
static char byte_5016E4[] = "------";

// 0x50170B
static const double dbl_50170B = 14.4;

// 0x501713
static const double dbl_501713 = 19.2;

// 0x5018F0
static const double dbl_5018F0 = 19.2;

// 0x5019BE
static const double dbl_5019BE = 14.4;

// 0x518528
static bool gCharacterEditorIsoWasEnabled = false;

// 0x51852C
static int gCharacterEditorCurrentSkill = 0;

// 0x518534
static int gCharacterEditorSkillValueAdjustmentSliderY = 27;

// 0x518538
int gCharacterEditorRemainingCharacterPoints = 0;

// 0x51853C
static KarmaEntry* gKarmaEntries = nullptr;

// 0x518540
static int gKarmaEntriesLength = 0;

// 0x518544
static GenericReputationEntry* gGenericReputationEntries = nullptr;

// 0x518548
static int gGenericReputationEntriesLength = 0;

// 0x51854C
static const TownReputationEntry gTownReputationEntries[TOWN_REPUTATION_COUNT] = {
    { GVAR_TOWN_REP_ARROYO, CITY_ARROYO },
    { GVAR_TOWN_REP_KLAMATH, CITY_KLAMATH },
    { GVAR_TOWN_REP_THE_DEN, CITY_DEN },
    { GVAR_TOWN_REP_VAULT_CITY, CITY_VAULT_CITY },
    { GVAR_TOWN_REP_GECKO, CITY_GECKO },
    { GVAR_TOWN_REP_MODOC, CITY_MODOC },
    { GVAR_TOWN_REP_SIERRA_BASE, CITY_SIERRA_ARMY_BASE },
    { GVAR_TOWN_REP_BROKEN_HILLS, CITY_BROKEN_HILLS },
    { GVAR_TOWN_REP_NEW_RENO, CITY_NEW_RENO },
    { GVAR_TOWN_REP_REDDING, CITY_REDDING },
    { GVAR_TOWN_REP_NCR, CITY_NEW_CALIFORNIA_REPUBLIC },
    { GVAR_TOWN_REP_VAULT_13, CITY_VAULT_13 },
    { GVAR_TOWN_REP_SAN_FRANCISCO, CITY_SAN_FRANCISCO },
    { GVAR_TOWN_REP_ABBEY, CITY_ABBEY },
    { GVAR_TOWN_REP_EPA, CITY_ENV_PROTECTION_AGENCY },
    { GVAR_TOWN_REP_PRIMITIVE_TRIBE, CITY_PRIMITIVE_TRIBE },
    { GVAR_TOWN_REP_RAIDERS, CITY_RAIDERS },
    { GVAR_TOWN_REP_VAULT_15, CITY_VAULT_15 },
    { GVAR_TOWN_REP_GHOST_FARM, CITY_MODOC_GHOST_TOWN },
};

// 0x5185E4
static const int gAddictionReputationVars[ADDICTION_REPUTATION_COUNT] = {
    GVAR_NUKA_COLA_ADDICT,
    GVAR_BUFF_OUT_ADDICT,
    GVAR_MENTATS_ADDICT,
    GVAR_PSYCHO_ADDICT,
    GVAR_RADAWAY_ADDICT,
    GVAR_ALCOHOL_ADDICT,
    GVAR_ADDICT_JET,
    GVAR_ADDICT_TRAGIC,
};

// 0x518604
static const int gAddictionReputationFrmIds[ADDICTION_REPUTATION_COUNT] = {
    142,
    126,
    140,
    144,
    145,
    52,
    136,
    149,
};

// 0x518624
static int gCharacterEditorFolderViewScrollUpBtn = -1;

// 0x518628
static int gCharacterEditorFolderViewScrollDownBtn = -1;

// 0x56FB60
static char gCharacterEditorFolderCardString[256];

// 0x56FC60
static int gCharacterEditorSkillsBackup[SKILL_COUNT];

// 0x56FCA8
static MessageList gCharacterEditorMessageList;

// 0x56FCB0
static PerkDialogOption gPerkDialogOptionList[DIALOG_PICKER_NUM_OPTIONS];

// buttons for selecting traits
//
// 0x5700A8
static int gCharacterEditorOptionalTraitBtns[TRAIT_COUNT];

// 0x5700E8
static MessageListItem gCharacterEditorMessageListItem;

// 0x5700F8
static char gCharacterEditorCardTitle[48];

// 0x570128
static char gPerkDialogCardTitle[48];

// buttons for tagging skills
//
// 0x570158
static int gCharacterEditorTagSkillBtns[SKILL_COUNT];

// pc name
//
// 0x5701A0
static char gCharacterEditorNameBackup[32];

// 0x570418
static unsigned char* gCharacterEditorFrmCopy[EDITOR_GRAPHIC_COUNT];

// 0x5705A8
static int gCharacterEditorFolderViewMaxLines;

// 0x5705AC
static int gCharacterEditorFolderViewCurrentLine;

// 0x5705B0
static int gCharacterEditorFolderCardFrmId;

// 0x5705B4
static int gCharacterEditorFolderViewTopLine;

// 0x5705B8
static char* gCharacterEditorFolderCardTitle;

// 0x5705BC
static char* gCharacterEditorFolderCardSubtitle;

// 0x5705C0
static int gCharacterEditorFolderViewOffsetY;

// 0x5705C4
static int gCharacterEditorKarmaFolderTopLine;

// 0x5705C8
static int gCharacterEditorFolderViewHighlightedLine;

// 0x5705CC
static char* gCharacterEditorFolderCardDescription;

// 0x5705D0
static int gCharacterEditorFolderViewNextY;

// 0x5705D4
static int gCharacterEditorKillsFolderTopLine;

// 0x5705D8
static int gCharacterEditorPerkFolderTopLine;

// 0x5705E0
static int gPerkDialogWindow;

// 0x5705E4
static int gCharacterEditorSliderPlusBtn;

// 0x5705E8
static int gCharacterEditorSliderMinusBtn;

// - stats buttons
//
// 0x5705EC
static int gCharacterEditorPrimaryStatMinusBtns[7];

// 0x570608
static unsigned char* gCharacterEditorWindowBuffer;

// 0x57060C
static int gCharacterEditorWindow;

// + stats buttons
//
// 0x570610
static int gCharacterEditorPrimaryStatPlusBtns[7];

// 0x57062C
static unsigned char* gPerkDialogWindowBuffer;

// 0x570630
static CritterProtoData gCharacterEditorDudeDataBackup;

// 0x5707A8
static int gPerkDialogCurrentLine;

// 0x5707AC
static int gPerkDialogPreviousCurrentLine;

// unspent skill points
//
// 0x5707B0
static int gCharacterEditorUnspentSkillPointsBackup;

// 0x5707B4
static int gCharacterEditorLastLevel;

// 0x5707B8
static int gCharacterEditorOldFont;

// 0x5707BC
static int gCharacterEditorKillsCount;

// current hit points
//
// 0x5707C4
static int gCharacterEditorHitPointsBackup;

// 0x5707C8
static int gCharacterEditorMouseY; // mouse y

// 0x5707CC
static int gCharacterEditorMouseX; // mouse x

// 0x5707D0
static int characterEditorSelectedItem;

// 0x5707D4
static int characterEditorWindowSelectedFolder;

// 0x5707D8
static bool gCharacterEditorCardDrawn;

// 0x5707DC
static int gPerkDialogTopLine;

// 0x5707E0
static bool gPerkDialogCardDrawn;

// 0x5707E4
static int gCharacterEditorPerksBackup[PERK_COUNT];

// 0x5709C0
static unsigned int _repFtime;

// 0x5709C4
static unsigned int _frame_time;

// 0x5709C8
static int gCharacterEditorOldTaggedSkillCount;

// 0x5709CC
static int gCharacterEditorLastLevelBackup;

// 0x5709E8
static int gPerkDialogCardFrmId;

// 0x5709EC
static int gCharacterEditorCardFrmId;

// 0x5709D0
static bool gCharacterEditorIsCreationMode;

// 0x5709D4
static int gCharacterEditorTaggedSkillsBackup[NUM_TAGGED_SKILLS];

// 0x5709F0
static int gCharacterEditorOptionalTraitsBackup[3];

// current index for selecting new trait
//
// 0x5709FC
static int gCharacterEditorTempTraitCount;

// 0x570A00
static int gPerkDialogOptionCount;

// 0x570A04
static int gCharacterEditorTempTraits[3];

// 0x570A10
static int gCharacterEditorTaggedSkillCount;

// 0x570A14
static int gCharacterEditorTempTaggedSkills[NUM_TAGGED_SKILLS];

// 0x570A28
static char gCharacterEditorHasFreePerkBackup;

// 0x570A29
static unsigned char gCharacterEditorHasFreePerk;

// 0x570A2A
static unsigned char gCharacterEditorIsSkillsFirstDraw;

static FrmImage _editorBackgroundFrmImage;
static FrmImage _editorFrmImages[EDITOR_GRAPHIC_COUNT];
static FrmImage _perkDialogBackgroundFrmImage;

struct CustomKarmaFolderDescription {
    int frmId;
    int threshold;
};

static std::vector<CustomKarmaFolderDescription> gCustomKarmaFolderDescriptions;
static std::vector<TownReputationEntry> gCustomTownReputationEntries;

// 0x431DF8
int characterEditorShow(bool isCreationMode)
{
    ScopedGameMode gm(!isCreationMode ? GameMode::kEditor : 0);

    char* messageListItemText;
    char line1[128];
    char line2[128];
    const char* lines[] = { line2 };

    gCharacterEditorIsCreationMode = isCreationMode;

    characterEditorSavePlayer();

    if (characterEditorWindowInit() == -1) {
        debugPrint("\n ** Error loading character editor data! **\n");
        return -1;
    }

    if (!gCharacterEditorIsCreationMode) {
        if (characterEditorUpdateLevel()) {
            critterUpdateDerivedStats(gDude);
            characterEditorDrawOptionalTraits();
            characterEditorDrawSkills(0);
            characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
            characterEditorDrawDerivedStats();
            characterEditorDrawCard();
        }
    }

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        _frame_time = getTicks();
        int keyCode = inputGetInput();

        convertMouseWheelToArrowKey(&keyCode);

        bool done = false;
        if (keyCode == 500) {
            done = true;
        }

        if (keyCode == KEY_RETURN || keyCode == KEY_UPPERCASE_D || keyCode == KEY_LOWERCASE_D) {
            done = true;
            soundPlayFile("ib1p1xx1");
        }

        if (done) {
            if (gCharacterEditorIsCreationMode) {
                if (gCharacterEditorRemainingCharacterPoints != 0) {
                    soundPlayFile("iisxxxx1");

                    // You must use all character points
                    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 118);
                    strcpy(line1, messageListItemText);

                    // before starting the game!
                    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 119);
                    strcpy(line2, messageListItemText);

                    showDialogBox(line1, lines, 1, 192, 126, _colorTable[32328], nullptr, _colorTable[32328], 0);
                    windowRefresh(gCharacterEditorWindow);

                    rc = -1;
                    continue;
                }

                if (gCharacterEditorTaggedSkillCount > 0) {
                    soundPlayFile("iisxxxx1");

                    // You must select all tag skills
                    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 142);
                    strcpy(line1, messageListItemText);

                    // before starting the game!
                    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 143);
                    strcpy(line2, messageListItemText);

                    showDialogBox(line1, lines, 1, 192, 126, _colorTable[32328], nullptr, _colorTable[32328], 0);
                    windowRefresh(gCharacterEditorWindow);

                    rc = -1;
                    continue;
                }

                if (_is_supper_bonus()) {
                    soundPlayFile("iisxxxx1");

                    // All stats must be between 1 and 10
                    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 157);
                    strcpy(line1, messageListItemText);

                    // before starting the game!
                    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 158);
                    strcpy(line2, messageListItemText);

                    showDialogBox(line1, lines, 1, 192, 126, _colorTable[32328], nullptr, _colorTable[32328], 0);
                    windowRefresh(gCharacterEditorWindow);

                    rc = -1;
                    continue;
                }

                if (compat_stricmp(critterGetName(gDude), "None") == 0) {
                    soundPlayFile("iisxxxx1");

                    // Warning: You haven't changed your player
                    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 160);
                    strcpy(line1, messageListItemText);

                    // name. Use this character any way?
                    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 161);
                    strcpy(line2, messageListItemText);

                    if (showDialogBox(line1, lines, 1, 192, 126, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_YES_NO) == 0) {
                        windowRefresh(gCharacterEditorWindow);

                        rc = -1;
                        continue;
                    }
                }
            }

            windowRefresh(gCharacterEditorWindow);
            rc = 0;
        } else if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            showQuitConfirmationDialog();
            windowRefresh(gCharacterEditorWindow);
        } else if (keyCode == 502 || keyCode == KEY_ESCAPE || keyCode == KEY_UPPERCASE_C || keyCode == KEY_LOWERCASE_C || _game_user_wants_to_quit != 0) {
            windowRefresh(gCharacterEditorWindow);
            rc = 1;
        } else if (gCharacterEditorIsCreationMode && (keyCode == 517 || keyCode == KEY_UPPERCASE_N || keyCode == KEY_LOWERCASE_N)) {
            characterEditorEditName();
            windowRefresh(gCharacterEditorWindow);
        } else if (gCharacterEditorIsCreationMode && (keyCode == 519 || keyCode == KEY_UPPERCASE_A || keyCode == KEY_LOWERCASE_A)) {
            characterEditorEditAge();
            windowRefresh(gCharacterEditorWindow);
        } else if (gCharacterEditorIsCreationMode && (keyCode == 520 || keyCode == KEY_UPPERCASE_S || keyCode == KEY_LOWERCASE_S)) {
            characterEditorEditGender();
            windowRefresh(gCharacterEditorWindow);
        } else if (gCharacterEditorIsCreationMode && (keyCode >= 503 && keyCode < 517)) {
            characterEditorAdjustPrimaryStat(keyCode);
            windowRefresh(gCharacterEditorWindow);
        } else if ((gCharacterEditorIsCreationMode && (keyCode == 501 || keyCode == KEY_UPPERCASE_O || keyCode == KEY_LOWERCASE_O))
            || (!gCharacterEditorIsCreationMode && (keyCode == 501 || keyCode == KEY_UPPERCASE_P || keyCode == KEY_LOWERCASE_P))) {
            characterEditorShowOptions();
            windowRefresh(gCharacterEditorWindow);
        } else if (keyCode >= 525 && keyCode < 535) {
            characterEditorHandleInfoButtonPressed(keyCode);
            windowRefresh(gCharacterEditorWindow);
        } else {
            switch (keyCode) {
            case KEY_TAB:
                if (characterEditorSelectedItem >= 0 && characterEditorSelectedItem < 7) {
                    characterEditorSelectedItem = gCharacterEditorIsCreationMode ? 82 : 7;
                } else if (characterEditorSelectedItem >= 7 && characterEditorSelectedItem < 9) {
                    if (gCharacterEditorIsCreationMode) {
                        characterEditorSelectedItem = 82;
                    } else {
                        characterEditorSelectedItem = 10;
                        characterEditorWindowSelectedFolder = 0;
                    }
                } else if (characterEditorSelectedItem >= 10 && characterEditorSelectedItem < 43) {
                    switch (characterEditorWindowSelectedFolder) {
                    case EDITOR_FOLDER_PERKS:
                        characterEditorSelectedItem = 10;
                        characterEditorWindowSelectedFolder = EDITOR_FOLDER_KARMA;
                        break;
                    case EDITOR_FOLDER_KARMA:
                        characterEditorSelectedItem = 10;
                        characterEditorWindowSelectedFolder = EDITOR_FOLDER_KILLS;
                        break;
                    case EDITOR_FOLDER_KILLS:
                        characterEditorSelectedItem = 43;
                        break;
                    }
                } else if (characterEditorSelectedItem >= 43 && characterEditorSelectedItem < 51) {
                    characterEditorSelectedItem = 51;
                } else if (characterEditorSelectedItem >= 51 && characterEditorSelectedItem < 61) {
                    characterEditorSelectedItem = 61;
                } else if (characterEditorSelectedItem >= 61 && characterEditorSelectedItem < 82) {
                    characterEditorSelectedItem = 0;
                } else if (characterEditorSelectedItem >= 82 && characterEditorSelectedItem < 98) {
                    characterEditorSelectedItem = 43;
                }
                characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
                characterEditorDrawOptionalTraits();
                characterEditorDrawSkills(0);
                characterEditorDrawPcStats();
                characterEditorDrawFolders();
                characterEditorDrawDerivedStats();
                characterEditorDrawCard();
                windowRefresh(gCharacterEditorWindow);
                break;
            case KEY_ARROW_LEFT:
            case KEY_MINUS:
            case KEY_UPPERCASE_J:
                if (characterEditorSelectedItem >= 0 && characterEditorSelectedItem < 7) {
                    if (gCharacterEditorIsCreationMode) {
                        _win_button_press_and_release(gCharacterEditorPrimaryStatMinusBtns[characterEditorSelectedItem]);
                        windowRefresh(gCharacterEditorWindow);
                    }
                } else if (characterEditorSelectedItem >= 61 && characterEditorSelectedItem < 79) {
                    if (gCharacterEditorIsCreationMode) {
                        _win_button_press_and_release(gCharacterEditorTagSkillBtns[characterEditorSelectedItem - 61]);
                        windowRefresh(gCharacterEditorWindow);
                    } else {
                        characterEditorHandleAdjustSkillButtonPressed(keyCode);
                        windowRefresh(gCharacterEditorWindow);
                    }
                } else if (characterEditorSelectedItem >= 82 && characterEditorSelectedItem < 98) {
                    if (gCharacterEditorIsCreationMode) {
                        _win_button_press_and_release(gCharacterEditorOptionalTraitBtns[characterEditorSelectedItem - 82]);
                        windowRefresh(gCharacterEditorWindow);
                    }
                }
                break;
            case KEY_ARROW_RIGHT:
            case KEY_PLUS:
            case KEY_UPPERCASE_N:
                if (characterEditorSelectedItem >= 0 && characterEditorSelectedItem < 7) {
                    if (gCharacterEditorIsCreationMode) {
                        _win_button_press_and_release(gCharacterEditorPrimaryStatPlusBtns[characterEditorSelectedItem]);
                        windowRefresh(gCharacterEditorWindow);
                    }
                } else if (characterEditorSelectedItem >= 61 && characterEditorSelectedItem < 79) {
                    if (gCharacterEditorIsCreationMode) {
                        _win_button_press_and_release(gCharacterEditorTagSkillBtns[characterEditorSelectedItem - 61]);
                        windowRefresh(gCharacterEditorWindow);
                    } else {
                        characterEditorHandleAdjustSkillButtonPressed(keyCode);
                        windowRefresh(gCharacterEditorWindow);
                    }
                } else if (characterEditorSelectedItem >= 82 && characterEditorSelectedItem < 98) {
                    if (gCharacterEditorIsCreationMode) {
                        _win_button_press_and_release(gCharacterEditorOptionalTraitBtns[characterEditorSelectedItem - 82]);
                        windowRefresh(gCharacterEditorWindow);
                    }
                }
                break;
            case KEY_ARROW_UP:
                if (characterEditorSelectedItem >= 10 && characterEditorSelectedItem < 43) {
                    if (characterEditorSelectedItem == 10) {
                        if (gCharacterEditorFolderViewTopLine > 0) {
                            characterEditorFolderViewScroll(-1);
                            characterEditorSelectedItem--;
                            characterEditorDrawFolders();
                            characterEditorDrawCard();
                        }
                    } else {
                        characterEditorSelectedItem--;
                        characterEditorDrawFolders();
                        characterEditorDrawCard();
                    }

                    windowRefresh(gCharacterEditorWindow);
                } else {
                    switch (characterEditorSelectedItem) {
                    case 0:
                        characterEditorSelectedItem = 6;
                        break;
                    case 7:
                        characterEditorSelectedItem = 9;
                        break;
                    case 43:
                        characterEditorSelectedItem = 50;
                        break;
                    case 51:
                        characterEditorSelectedItem = 60;
                        break;
                    case 61:
                        characterEditorSelectedItem = 78;
                        break;
                    case 82:
                        characterEditorSelectedItem = 97;
                        break;
                    default:
                        characterEditorSelectedItem -= 1;
                        break;
                    }

                    if (characterEditorSelectedItem >= 61 && characterEditorSelectedItem < 79) {
                        gCharacterEditorCurrentSkill = characterEditorSelectedItem - 61;
                    }

                    characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
                    characterEditorDrawOptionalTraits();
                    characterEditorDrawSkills(0);
                    characterEditorDrawPcStats();
                    characterEditorDrawFolders();
                    characterEditorDrawDerivedStats();
                    characterEditorDrawCard();
                    windowRefresh(gCharacterEditorWindow);
                }
                break;
            case KEY_ARROW_DOWN:
                if (characterEditorSelectedItem >= 10 && characterEditorSelectedItem < 43) {
                    if (characterEditorSelectedItem - 10 < gCharacterEditorFolderViewCurrentLine - gCharacterEditorFolderViewTopLine) {
                        if (characterEditorSelectedItem - 10 == gCharacterEditorFolderViewMaxLines - 1) {
                            characterEditorFolderViewScroll(1);
                        }

                        characterEditorSelectedItem++;

                        characterEditorDrawFolders();
                        characterEditorDrawCard();
                    }

                    windowRefresh(gCharacterEditorWindow);
                } else {
                    switch (characterEditorSelectedItem) {
                    case 6:
                        characterEditorSelectedItem = 0;
                        break;
                    case 9:
                        characterEditorSelectedItem = 7;
                        break;
                    case 50:
                        characterEditorSelectedItem = 43;
                        break;
                    case 60:
                        characterEditorSelectedItem = 51;
                        break;
                    case 78:
                        characterEditorSelectedItem = 61;
                        break;
                    case 97:
                        characterEditorSelectedItem = 82;
                        break;
                    default:
                        characterEditorSelectedItem += 1;
                        break;
                    }

                    if (characterEditorSelectedItem >= 61 && characterEditorSelectedItem < 79) {
                        gCharacterEditorCurrentSkill = characterEditorSelectedItem - 61;
                    }

                    characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
                    characterEditorDrawOptionalTraits();
                    characterEditorDrawSkills(0);
                    characterEditorDrawPcStats();
                    characterEditorDrawFolders();
                    characterEditorDrawDerivedStats();
                    characterEditorDrawCard();
                    windowRefresh(gCharacterEditorWindow);
                }
                break;
            case 521:
            case 523:
                characterEditorHandleAdjustSkillButtonPressed(keyCode);
                windowRefresh(gCharacterEditorWindow);
                break;
            case 535:
                characterEditorHandleFolderButtonPressed();
                windowRefresh(gCharacterEditorWindow);
                break;
            case 17000:
                characterEditorFolderViewScroll(-1);
                windowRefresh(gCharacterEditorWindow);
                break;
            case 17001:
                characterEditorFolderViewScroll(1);
                windowRefresh(gCharacterEditorWindow);
                break;
            default:
                if (gCharacterEditorIsCreationMode && (keyCode >= 536 && keyCode < 554)) {
                    characterEditorToggleTaggedSkill(keyCode - 536);
                    windowRefresh(gCharacterEditorWindow);
                } else if (gCharacterEditorIsCreationMode && (keyCode >= 555 && keyCode < 571)) {
                    characterEditorToggleOptionalTrait(keyCode - 555);
                    windowRefresh(gCharacterEditorWindow);
                } else {
                    if (keyCode == 390) {
                        takeScreenshot();
                    }

                    windowRefresh(gCharacterEditorWindow);
                }
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (rc == 0) {
        if (isCreationMode) {
            _proto_dude_update_gender();
            paletteFadeTo(gPaletteBlack);
        }
    }

    characterEditorWindowFree();

    if (rc == 1) {
        characterEditorRestorePlayer();
    }

    if (dudeHasState(DUDE_STATE_LEVEL_UP_AVAILABLE)) {
        dudeDisableState(DUDE_STATE_LEVEL_UP_AVAILABLE);
    }

    interfaceRenderHitPoints(false);

    return rc;
}

// 0x4329EC
static int characterEditorWindowInit()
{
    int i;
    int v1;
    int v3;
    char path[COMPAT_MAX_PATH];
    int fid;
    char* str;
    int len;
    int btn;
    int x;
    int y;
    char perks[32];
    char karma[32];
    char kills[32];

    gCharacterEditorOldFont = fontGetCurrent();
    gCharacterEditorOldTaggedSkillCount = 0;
    gCharacterEditorIsoWasEnabled = 0;
    gPerkDialogCardFrmId = -1;
    gCharacterEditorCardFrmId = -1;
    gPerkDialogCardDrawn = false;
    gCharacterEditorCardDrawn = false;
    gCharacterEditorIsSkillsFirstDraw = 1;
    gPerkDialogCardTitle[0] = '\0';
    gCharacterEditorCardTitle[0] = '\0';

    fontSetCurrent(101);

    gCharacterEditorSkillValueAdjustmentSliderY = gCharacterEditorCurrentSkill * (fontGetLineHeight() + 1) + 27;

    // skills
    skillsGetTagged(gCharacterEditorTempTaggedSkills, NUM_TAGGED_SKILLS);

    v1 = 0;
    for (i = 3; i >= 0; i--) {
        if (gCharacterEditorTempTaggedSkills[i] != -1) {
            break;
        }

        v1++;
    }

    if (gCharacterEditorIsCreationMode) {
        v1--;
    }

    gCharacterEditorTaggedSkillCount = v1;

    // traits
    traitsGetSelected(&(gCharacterEditorTempTraits[0]), &(gCharacterEditorTempTraits[1]));

    v3 = 0;
    for (i = 1; i >= 0; i--) {
        if (gCharacterEditorTempTraits[i] != -1) {
            break;
        }

        v3++;
    }

    gCharacterEditorTempTraitCount = v3;

    if (!gCharacterEditorIsCreationMode) {
        gCharacterEditorIsoWasEnabled = isoDisable();
    }

    colorCycleDisable();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    if (!messageListInit(&gCharacterEditorMessageList)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "editor.msg");

    if (!messageListLoad(&gCharacterEditorMessageList, path)) {
        return -1;
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, (gCharacterEditorIsCreationMode ? 169 : 177), 0, 0, 0);
    if (!_editorBackgroundFrmImage.lock(fid)) {
        messageListFree(&gCharacterEditorMessageList);
        return -1;
    }

    if (karmaInit() == -1) {
        return -1;
    }

    if (genericReputationInit() == -1) {
        return -1;
    }

    // SFALL: Custom karma folder.
    customKarmaFolderInit();

    // SFALL: Custom town reputation.
    customTownReputationInit();

    soundContinueAll();

    for (i = 0; i < EDITOR_GRAPHIC_COUNT; i++) {
        fid = buildFid(OBJ_TYPE_INTERFACE, gCharacterEditorFrmIds[i], 0, 0, 0);
        if (!_editorFrmImages[i].lock(fid)) {
            break;
        }
    }

    if (i != EDITOR_GRAPHIC_COUNT) {
        while (--i >= 0) {
            _editorFrmImages[i].unlock();
        }
        return -1;

        _editorBackgroundFrmImage.unlock();

        messageListFree(&gCharacterEditorMessageList);

        if (gCharacterEditorIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    soundContinueAll();

    for (i = 0; i < EDITOR_GRAPHIC_COUNT; i++) {
        if (gCharacterEditorFrmShouldCopy[i]) {
            gCharacterEditorFrmCopy[i] = (unsigned char*)internal_malloc(_editorFrmImages[i].getWidth() * _editorFrmImages[i].getHeight());
            if (gCharacterEditorFrmCopy[i] == nullptr) {
                break;
            }
            memcpy(gCharacterEditorFrmCopy[i], _editorFrmImages[i].getData(), _editorFrmImages[i].getWidth() * _editorFrmImages[i].getHeight());
        } else {
            gCharacterEditorFrmCopy[i] = (unsigned char*)-1;
        }
    }

    if (i != EDITOR_GRAPHIC_COUNT) {
        while (--i >= 0) {
            if (gCharacterEditorFrmShouldCopy[i]) {
                internal_free(gCharacterEditorFrmCopy[i]);
            }
        }

        for (i = 0; i < EDITOR_GRAPHIC_COUNT; i++) {
            _editorFrmImages[i].unlock();
        }

        _editorBackgroundFrmImage.unlock();

        messageListFree(&gCharacterEditorMessageList);
        if (gCharacterEditorIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);

        return -1;
    }

    int editorWindowX = (screenGetWidth() - EDITOR_WINDOW_WIDTH) / 2;
    int editorWindowY = (screenGetHeight() - EDITOR_WINDOW_HEIGHT) / 2;
    gCharacterEditorWindow = windowCreate(editorWindowX,
        editorWindowY,
        EDITOR_WINDOW_WIDTH,
        EDITOR_WINDOW_HEIGHT,
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (gCharacterEditorWindow == -1) {
        for (i = 0; i < EDITOR_GRAPHIC_COUNT; i++) {
            if (gCharacterEditorFrmShouldCopy[i]) {
                internal_free(gCharacterEditorFrmCopy[i]);
            }
            _editorFrmImages[i].unlock();
        }

        _editorBackgroundFrmImage.unlock();

        messageListFree(&gCharacterEditorMessageList);
        if (gCharacterEditorIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);

        return -1;
    }

    gCharacterEditorWindowBuffer = windowGetBuffer(gCharacterEditorWindow);
    memcpy(gCharacterEditorWindowBuffer, _editorBackgroundFrmImage.getData(), 640 * 480);

    if (gCharacterEditorIsCreationMode) {
        fontSetCurrent(103);

        // CHAR POINTS
        str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 116);
        fontDrawText(gCharacterEditorWindowBuffer + (286 * 640) + 14, str, 640, 640, _colorTable[18979]);
        characterEditorDrawBigNumber(126, 282, 0, gCharacterEditorRemainingCharacterPoints, 0, gCharacterEditorWindow);

        // OPTIONS
        str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 101);
        fontDrawText(gCharacterEditorWindowBuffer + (454 * 640) + 363, str, 640, 640, _colorTable[18979]);

        // OPTIONAL TRAITS
        str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 139);
        fontDrawText(gCharacterEditorWindowBuffer + (326 * 640) + 52, str, 640, 640, _colorTable[18979]);
        characterEditorDrawBigNumber(522, 228, 0, gPerkDialogOptionCount, 0, gCharacterEditorWindow);

        // TAG SKILLS
        str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 138);
        fontDrawText(gCharacterEditorWindowBuffer + (233 * 640) + 422, str, 640, 640, _colorTable[18979]);
        characterEditorDrawBigNumber(522, 228, 0, gCharacterEditorTaggedSkillCount, 0, gCharacterEditorWindow);
    } else {
        fontSetCurrent(103);

        str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 109);
        strcpy(perks, str);

        str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 110);
        strcpy(karma, str);

        str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 111);
        strcpy(kills, str);

        // perks selected
        len = fontGetStringWidth(perks);
        fontDrawText(
            gCharacterEditorFrmCopy[46] + 5 * _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth() + 61 - len / 2,
            perks,
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _colorTable[18979]);

        len = fontGetStringWidth(karma);
        fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED] + 5 * _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth() + 159 - len / 2,
            karma,
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _colorTable[14723]);

        len = fontGetStringWidth(kills);
        fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED] + 5 * _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth() + 257 - len / 2,
            kills,
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _colorTable[14723]);

        // karma selected
        len = fontGetStringWidth(perks);
        fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_KARMA_FOLDER_SELECTED] + 5 * _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth() + 61 - len / 2,
            perks,
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _colorTable[14723]);

        len = fontGetStringWidth(karma);
        fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_KARMA_FOLDER_SELECTED] + 5 * _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth() + 159 - len / 2,
            karma,
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _colorTable[18979]);

        len = fontGetStringWidth(kills);
        fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_KARMA_FOLDER_SELECTED] + 5 * _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth() + 257 - len / 2,
            kills,
            _editorFrmImages[46].getWidth(),
            _editorFrmImages[46].getWidth(),
            _colorTable[14723]);

        // kills selected
        len = fontGetStringWidth(perks);
        fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_KILLS_FOLDER_SELECTED] + 5 * _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth() + 61 - len / 2,
            perks,
            _editorFrmImages[46].getWidth(),
            _editorFrmImages[46].getWidth(),
            _colorTable[14723]);

        len = fontGetStringWidth(karma);
        fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_KILLS_FOLDER_SELECTED] + 5 * _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth() + 159 - len / 2,
            karma,
            _editorFrmImages[46].getWidth(),
            _editorFrmImages[46].getWidth(),
            _colorTable[14723]);

        len = fontGetStringWidth(kills);
        fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_KILLS_FOLDER_SELECTED] + 5 * _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth() + 257 - len / 2,
            kills,
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _colorTable[18979]);

        characterEditorDrawFolders();

        fontSetCurrent(103);

        // PRINT
        str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 103);
        fontDrawText(gCharacterEditorWindowBuffer + (EDITOR_WINDOW_WIDTH * PRINT_BTN_Y) + PRINT_BTN_X, str, EDITOR_WINDOW_WIDTH, EDITOR_WINDOW_WIDTH, _colorTable[18979]);

        characterEditorDrawPcStats();
        characterEditorFolderViewInit();
    }

    fontSetCurrent(103);

    // CANCEL
    str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 102);
    fontDrawText(gCharacterEditorWindowBuffer + (EDITOR_WINDOW_WIDTH * CANCEL_BTN_Y) + CANCEL_BTN_X, str, EDITOR_WINDOW_WIDTH, EDITOR_WINDOW_WIDTH, _colorTable[18979]);

    // DONE
    str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 100);
    fontDrawText(gCharacterEditorWindowBuffer + (EDITOR_WINDOW_WIDTH * DONE_BTN_Y) + DONE_BTN_X, str, EDITOR_WINDOW_WIDTH, EDITOR_WINDOW_WIDTH, _colorTable[18979]);

    characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
    characterEditorDrawDerivedStats();

    if (!gCharacterEditorIsCreationMode) {
        gCharacterEditorSliderPlusBtn = buttonCreate(
            gCharacterEditorWindow,
            614,
            20,
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_ON].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_ON].getHeight(),
            -1,
            522,
            521,
            522,
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_OFF].getData(),
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_ON].getData(),
            nullptr,
            96);
        gCharacterEditorSliderMinusBtn = buttonCreate(
            gCharacterEditorWindow,
            614,
            20 + _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_ON].getHeight() - 1,
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_ON].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_OFF].getHeight(),
            -1,
            524,
            523,
            524,
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_OFF].getData(),
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_ON].getData(),
            nullptr,
            96);
        buttonSetCallbacks(gCharacterEditorSliderPlusBtn, _gsound_red_butt_press, nullptr);
        buttonSetCallbacks(gCharacterEditorSliderMinusBtn, _gsound_red_butt_press, nullptr);
    }

    characterEditorDrawSkills(0);
    characterEditorDrawCard();
    soundContinueAll();
    characterEditorDrawName();
    characterEditorDrawAge();
    characterEditorDrawGender();

    if (gCharacterEditorIsCreationMode) {
        x = NAME_BUTTON_X;
        btn = buttonCreate(
            gCharacterEditorWindow,
            x,
            NAME_BUTTON_Y,
            _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getHeight(),
            -1,
            -1,
            -1,
            NAME_BTN_CODE,
            gCharacterEditorFrmCopy[EDITOR_GRAPHIC_NAME_OFF],
            gCharacterEditorFrmCopy[EDITOR_GRAPHIC_NAME_ON],
            nullptr,
            32);
        if (btn != -1) {
            buttonSetMask(btn, _editorFrmImages[EDITOR_GRAPHIC_NAME_MASK].getData());
            buttonSetCallbacks(btn, _gsound_lrg_butt_press, nullptr);
        }

        x += _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getWidth();
        btn = buttonCreate(
            gCharacterEditorWindow,
            x,
            NAME_BUTTON_Y,
            _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getHeight(),
            -1,
            -1,
            -1,
            AGE_BTN_CODE,
            gCharacterEditorFrmCopy[EDITOR_GRAPHIC_AGE_OFF],
            gCharacterEditorFrmCopy[EDITOR_GRAPHIC_AGE_ON],
            nullptr,
            32);
        if (btn != -1) {
            buttonSetMask(btn, _editorFrmImages[EDITOR_GRAPHIC_AGE_MASK].getData());
            buttonSetCallbacks(btn, _gsound_lrg_butt_press, nullptr);
        }

        x += _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getWidth();
        btn = buttonCreate(
            gCharacterEditorWindow,
            x,
            NAME_BUTTON_Y,
            _editorFrmImages[EDITOR_GRAPHIC_SEX_ON].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_SEX_ON].getHeight(),
            -1,
            -1,
            -1,
            SEX_BTN_CODE,
            gCharacterEditorFrmCopy[EDITOR_GRAPHIC_SEX_OFF],
            gCharacterEditorFrmCopy[EDITOR_GRAPHIC_SEX_ON],
            nullptr,
            32);
        if (btn != -1) {
            buttonSetMask(btn, _editorFrmImages[EDITOR_GRAPHIC_SEX_MASK].getData());
            buttonSetCallbacks(btn, _gsound_lrg_butt_press, nullptr);
        }

        y = TAG_SKILLS_BUTTON_Y;
        for (i = 0; i < SKILL_COUNT; i++) {
            gCharacterEditorTagSkillBtns[i] = buttonCreate(
                gCharacterEditorWindow,
                TAG_SKILLS_BUTTON_X,
                y,
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getWidth(),
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getHeight(),
                -1,
                -1,
                -1,
                TAG_SKILLS_BUTTON_CODE + i,
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_OFF].getData(),
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getData(),
                nullptr,
                32);
            y += _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getHeight();
        }

        y = OPTIONAL_TRAITS_BTN_Y;
        for (i = 0; i < TRAIT_COUNT / 2; i++) {
            gCharacterEditorOptionalTraitBtns[i] = buttonCreate(
                gCharacterEditorWindow,
                OPTIONAL_TRAITS_LEFT_BTN_X,
                y,
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getWidth(),
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getHeight(),
                -1,
                -1,
                -1,
                OPTIONAL_TRAITS_BTN_CODE + i,
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_OFF].getData(),
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getData(),
                nullptr,
                32);
            y += _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getHeight() + OPTIONAL_TRAITS_BTN_SPACE;
        }

        y = OPTIONAL_TRAITS_BTN_Y;
        for (i = TRAIT_COUNT / 2; i < TRAIT_COUNT; i++) {
            gCharacterEditorOptionalTraitBtns[i] = buttonCreate(
                gCharacterEditorWindow,
                OPTIONAL_TRAITS_RIGHT_BTN_X,
                y,
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getWidth(),
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getHeight(),
                -1,
                -1,
                -1,
                OPTIONAL_TRAITS_BTN_CODE + i,
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_OFF].getData(),
                _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getData(),
                nullptr,
                32);
            y += _editorFrmImages[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].getHeight() + OPTIONAL_TRAITS_BTN_SPACE;
        }

        characterEditorDrawOptionalTraits();
    } else {
        x = NAME_BUTTON_X;
        blitBufferToBufferTrans(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_NAME_OFF],
            _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getHeight(),
            _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getWidth(),
            gCharacterEditorWindowBuffer + (EDITOR_WINDOW_WIDTH * NAME_BUTTON_Y) + x,
            EDITOR_WINDOW_WIDTH);

        x += _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getWidth();
        blitBufferToBufferTrans(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_AGE_OFF],
            _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getHeight(),
            _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getWidth(),
            gCharacterEditorWindowBuffer + (EDITOR_WINDOW_WIDTH * NAME_BUTTON_Y) + x,
            EDITOR_WINDOW_WIDTH);

        x += _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getWidth();
        blitBufferToBufferTrans(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_SEX_OFF],
            _editorFrmImages[EDITOR_GRAPHIC_SEX_ON].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_SEX_ON].getHeight(),
            _editorFrmImages[EDITOR_GRAPHIC_SEX_ON].getWidth(),
            gCharacterEditorWindowBuffer + (EDITOR_WINDOW_WIDTH * NAME_BUTTON_Y) + x,
            EDITOR_WINDOW_WIDTH);

        btn = buttonCreate(gCharacterEditorWindow,
            11,
            327,
            _editorFrmImages[EDITOR_GRAPHIC_FOLDER_MASK].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_FOLDER_MASK].getHeight(),
            -1,
            -1,
            -1,
            535,
            nullptr,
            nullptr,
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetMask(btn, _editorFrmImages[EDITOR_GRAPHIC_FOLDER_MASK].getData());
        }
    }

    if (gCharacterEditorIsCreationMode) {
        // +/- buttons for stats
        for (i = 0; i < 7; i++) {
            gCharacterEditorPrimaryStatPlusBtns[i] = buttonCreate(gCharacterEditorWindow,
                SPECIAL_STATS_BTN_X,
                gCharacterEditorPrimaryStatY[i],
                _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_ON].getWidth(),
                _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_ON].getHeight(),
                -1,
                518,
                503 + i,
                518,
                _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_OFF].getData(),
                _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_ON].getData(),
                nullptr,
                32);
            if (gCharacterEditorPrimaryStatPlusBtns[i] != -1) {
                buttonSetCallbacks(gCharacterEditorPrimaryStatPlusBtns[i], _gsound_red_butt_press, nullptr);
            }

            gCharacterEditorPrimaryStatMinusBtns[i] = buttonCreate(gCharacterEditorWindow,
                SPECIAL_STATS_BTN_X,
                gCharacterEditorPrimaryStatY[i] + _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_ON].getHeight() - 1,
                _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_ON].getWidth(),
                _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_ON].getHeight(),
                -1,
                518,
                510 + i,
                518,
                _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_OFF].getData(),
                _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_ON].getData(),
                nullptr,
                32);
            if (gCharacterEditorPrimaryStatMinusBtns[i] != -1) {
                buttonSetCallbacks(gCharacterEditorPrimaryStatMinusBtns[i], _gsound_red_butt_press, nullptr);
            }
        }
    }

    characterEditorRegisterInfoAreas();
    soundContinueAll();

    btn = buttonCreate(
        gCharacterEditorWindow,
        344, // one pixel right shift to align with background
        454,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getHeight(),
        -1,
        -1,
        -1,
        501,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(
        gCharacterEditorWindow,
        553, // one pixel right shift to align with background
        454,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getHeight(),
        -1,
        -1,
        -1,
        502,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(
        gCharacterEditorWindow,
        456, // one pixel right shift to align with background
        454,
        _editorFrmImages[23].getWidth(),
        _editorFrmImages[23].getHeight(),
        -1,
        -1,
        -1,
        500,
        _editorFrmImages[23].getData(),
        _editorFrmImages[24].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(gCharacterEditorWindow);
    indicatorBarHide();

    return 0;
}

// 0x433AA8
static void characterEditorWindowFree()
{
    if (gCharacterEditorFolderViewScrollDownBtn != -1) {
        buttonDestroy(gCharacterEditorFolderViewScrollDownBtn);
        gCharacterEditorFolderViewScrollDownBtn = -1;
    }

    if (gCharacterEditorFolderViewScrollUpBtn != -1) {
        buttonDestroy(gCharacterEditorFolderViewScrollUpBtn);
        gCharacterEditorFolderViewScrollUpBtn = -1;
    }

    windowDestroy(gCharacterEditorWindow);

    for (int index = 0; index < EDITOR_GRAPHIC_COUNT; index++) {
        _editorFrmImages[index].unlock();

        if (gCharacterEditorFrmShouldCopy[index]) {
            internal_free(gCharacterEditorFrmCopy[index]);
        }
    }

    _editorBackgroundFrmImage.unlock();

    // NOTE: Uninline.
    genericReputationFree();

    // NOTE: Uninline.
    karmaFree();

    // SFALL: Custom karma folder.
    customKarmaFolderFree();

    // SFALL: Custom town reputation.
    customTownReputationFree();

    messageListFree(&gCharacterEditorMessageList);

    interfaceBarRefresh();

    if (gCharacterEditorIsoWasEnabled) {
        isoEnable();
    }

    colorCycleEnable();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    fontSetCurrent(gCharacterEditorOldFont);

    if (gCharacterEditorIsCreationMode) {
        skillsSetTagged(gCharacterEditorTempTaggedSkills, 3);
        traitsSetSelected(gCharacterEditorTempTraits[0], gCharacterEditorTempTraits[1]);
        characterEditorSelectedItem = 0;
        critterAdjustHitPoints(gDude, 1000);
    }

    indicatorBarShow();
}

// CharEditInit
// 0x433C0C
void characterEditorInit()
{
    int i;

    characterEditorSelectedItem = 0;
    gCharacterEditorCurrentSkill = 0;
    gCharacterEditorSkillValueAdjustmentSliderY = 27;
    gCharacterEditorHasFreePerk = 0;
    characterEditorWindowSelectedFolder = EDITOR_FOLDER_PERKS;

    for (i = 0; i < 2; i++) {
        gCharacterEditorTempTraits[i] = -1;
        gCharacterEditorOptionalTraitsBackup[i] = -1;
    }

    gCharacterEditorRemainingCharacterPoints = 5;
    gCharacterEditorLastLevel = 1;
}

// handle name input
static int _get_input_str(int win, int cancelKeyCode, char* text, int maxLength, int x, int y, int textColor, int backgroundColor, int flags)
{
    int cursorWidth = fontGetStringWidth("_") - 4;
    int windowWidth = windowGetWidth(win);
    int v60 = fontGetLineHeight();
    unsigned char* windowBuffer = windowGetBuffer(win);
    if (maxLength > 255) {
        maxLength = 255;
    }

    char copy[257];
    strcpy(copy, text);

    size_t nameLength = strlen(text);
    copy[nameLength] = ' ';
    copy[nameLength + 1] = '\0';

    int nameWidth = fontGetStringWidth(copy);

    bufferFill(windowBuffer + windowWidth * y + x, nameWidth, fontGetLineHeight(), windowWidth, backgroundColor);
    fontDrawText(windowBuffer + windowWidth * y + x, copy, windowWidth, windowWidth, textColor);

    windowRefresh(win);

    beginTextInput();

    int blinkingCounter = 3;
    bool blink = false;

    int rc = 1;
    while (rc == 1) {
        sharedFpsLimiter.mark();

        _frame_time = getTicks();

        int keyCode = inputGetInput();
        if (keyCode == cancelKeyCode) {
            rc = 0;
        } else if (keyCode == KEY_RETURN) {
            soundPlayFile("ib1p1xx1");
            rc = 0;
        } else if (keyCode == KEY_ESCAPE || _game_user_wants_to_quit != 0) {
            rc = -1;
        } else {
            if ((keyCode == KEY_DELETE || keyCode == KEY_BACKSPACE) && nameLength >= 1) {
                bufferFill(windowBuffer + windowWidth * y + x, fontGetStringWidth(copy), v60, windowWidth, backgroundColor);
                copy[nameLength - 1] = ' ';
                copy[nameLength] = '\0';
                fontDrawText(windowBuffer + windowWidth * y + x, copy, windowWidth, windowWidth, textColor);
                nameLength--;

                windowRefresh(win);
            } else if ((keyCode >= KEY_FIRST_INPUT_CHARACTER && keyCode <= KEY_LAST_INPUT_CHARACTER) && nameLength < maxLength) {
                if ((flags & 0x01) != 0) {
                    if (!_isdoschar(keyCode)) {
                        break;
                    }
                }

                bufferFill(windowBuffer + windowWidth * y + x, fontGetStringWidth(copy), v60, windowWidth, backgroundColor);

                copy[nameLength] = keyCode & 0xFF;
                copy[nameLength + 1] = ' ';
                copy[nameLength + 2] = '\0';
                fontDrawText(windowBuffer + windowWidth * y + x, copy, windowWidth, windowWidth, textColor);
                nameLength++;

                windowRefresh(win);
            }
        }

        blinkingCounter -= 1;
        if (blinkingCounter == 0) {
            blinkingCounter = 3;

            int color = blink ? backgroundColor : textColor;
            blink = !blink;

            bufferFill(windowBuffer + windowWidth * y + x + fontGetStringWidth(copy) - cursorWidth, cursorWidth, v60 - 2, windowWidth, color);
        }

        windowRefresh(win);

        delay_ms(1000 / 24 - (getTicks() - _frame_time));

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    endTextInput();

    if (rc == 0 || nameLength > 0) {
        copy[nameLength] = '\0';
        strcpy(text, copy);
    }

    return rc;
}

// 0x434060
bool _isdoschar(int ch)
{
    const char* punctuations = "#@!$`'~^&()-_=[]{}";

    if (isalnum(ch)) {
        return true;
    }

    size_t length = strlen(punctuations);
    for (size_t index = 0; index < length; index++) {
        if (punctuations[index] == ch) {
            return true;
        }
    }

    return false;
}

// copy filename replacing extension
//
// 0x4340D0
char* _strmfe(char* dest, const char* name, const char* ext)
{
    char* save = dest;

    while (*name != '\0' && *name != '.') {
        *dest++ = *name++;
    }

    *dest++ = '.';

    strcpy(dest, ext);

    return save;
}

// 0x43410C
static void characterEditorDrawFolders()
{
    if (gCharacterEditorIsCreationMode) {
        return;
    }

    blitBufferToBuffer(_editorBackgroundFrmImage.getData() + (360 * 640) + 34, 280, 120, 640, gCharacterEditorWindowBuffer + (360 * 640) + 34, 640);

    fontSetCurrent(101);

    switch (characterEditorWindowSelectedFolder) {
    case EDITOR_FOLDER_PERKS:
        blitBufferToBuffer(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED],
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getHeight(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            gCharacterEditorWindowBuffer + (327 * 640) + 11,
            640);
        characterEditorDrawPerksFolder();
        break;
    case EDITOR_FOLDER_KARMA:
        blitBufferToBuffer(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_KARMA_FOLDER_SELECTED],
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getHeight(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            gCharacterEditorWindowBuffer + (327 * 640) + 11,
            640);
        characterEditorDrawKarmaFolder();
        break;
    case EDITOR_FOLDER_KILLS:
        blitBufferToBuffer(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_KILLS_FOLDER_SELECTED],
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getHeight(),
            _editorFrmImages[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].getWidth(),
            gCharacterEditorWindowBuffer + (327 * 640) + 11,
            640);
        gCharacterEditorKillsCount = characterEditorDrawKillsFolder();
        break;
    default:
        debugPrint("\n ** Unknown folder type! **\n");
        break;
    }
}

// 0x434238
static void characterEditorDrawPerksFolder()
{
    const char* string;
    char perkName[80];
    int perk;
    int perkLevel;
    bool hasContent = false;

    characterEditorFolderViewClear();

    if (gCharacterEditorTempTraits[0] != -1) {
        // TRAITS
        string = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 156);
        if (characterEditorFolderViewDrawHeading(string)) {
            gCharacterEditorFolderCardFrmId = 54;
            // Optional Traits
            gCharacterEditorFolderCardTitle = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 146);
            gCharacterEditorFolderCardSubtitle = nullptr;
            // Optional traits describe your character in more detail. All traits will have positive and negative effects. You may choose up to two traits during creation.
            gCharacterEditorFolderCardDescription = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 147);
            hasContent = true;
        }

        if (gCharacterEditorTempTraits[0] != -1) {
            string = traitGetName(gCharacterEditorTempTraits[0]);
            if (characterEditorFolderViewDrawString(string)) {
                gCharacterEditorFolderCardFrmId = traitGetFrmId(gCharacterEditorTempTraits[0]);
                gCharacterEditorFolderCardTitle = traitGetName(gCharacterEditorTempTraits[0]);
                gCharacterEditorFolderCardSubtitle = nullptr;
                gCharacterEditorFolderCardDescription = traitGetDescription(gCharacterEditorTempTraits[0]);
                hasContent = true;
            }
        }

        if (gCharacterEditorTempTraits[1] != -1) {
            string = traitGetName(gCharacterEditorTempTraits[1]);
            if (characterEditorFolderViewDrawString(string)) {
                gCharacterEditorFolderCardFrmId = traitGetFrmId(gCharacterEditorTempTraits[1]);
                gCharacterEditorFolderCardTitle = traitGetName(gCharacterEditorTempTraits[1]);
                gCharacterEditorFolderCardSubtitle = nullptr;
                gCharacterEditorFolderCardDescription = traitGetDescription(gCharacterEditorTempTraits[1]);
                hasContent = true;
            }
        }
    }

    for (perk = 0; perk < PERK_COUNT; perk++) {
        if (perkGetRank(gDude, perk) != 0) {
            break;
        }
    }

    if (perk != PERK_COUNT) {
        // PERKS
        string = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 109);
        characterEditorFolderViewDrawHeading(string);

        for (perk = 0; perk < PERK_COUNT; perk++) {
            perkLevel = perkGetRank(gDude, perk);
            if (perkLevel != 0) {
                string = perkGetName(perk);

                if (perkLevel == 1) {
                    snprintf(perkName, sizeof(perkName), "%s", string);
                } else {
                    snprintf(perkName, sizeof(perkName), "%s (%d)", string, perkLevel);
                }

                if (characterEditorFolderViewDrawString(perkName)) {
                    gCharacterEditorFolderCardFrmId = perkGetFrmId(perk);
                    gCharacterEditorFolderCardTitle = perkGetName(perk);
                    gCharacterEditorFolderCardSubtitle = nullptr;
                    gCharacterEditorFolderCardDescription = perkGetDescription(perk);
                    hasContent = true;
                }
            }
        }
    }

    if (!hasContent) {
        gCharacterEditorFolderCardFrmId = 71;
        // Perks
        gCharacterEditorFolderCardTitle = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 124);
        gCharacterEditorFolderCardSubtitle = nullptr;
        // Perks add additional abilities. Every third experience level, you can choose one perk.
        gCharacterEditorFolderCardDescription = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 127);
    }
}

// 0x434498
static int characterEditorKillsCompare(const void* a1, const void* a2)
{
    const KillInfo* v1 = (const KillInfo*)a1;
    const KillInfo* v2 = (const KillInfo*)a2;
    return compat_stricmp(v1->name, v2->name);
}

// 0x4344A4
static int characterEditorDrawKillsFolder()
{
    int i;
    int killsCount;
    KillInfo kills[19];
    int usedKills = 0;
    bool hasContent = false;

    characterEditorFolderViewClear();

    for (i = 0; i < KILL_TYPE_COUNT; i++) {
        killsCount = killsGetByType(i);
        if (killsCount != 0) {
            KillInfo* killInfo = &(kills[usedKills]);
            killInfo->name = killTypeGetName(i);
            killInfo->killTypeId = i;
            killInfo->kills = killsCount;
            usedKills++;
        }
    }

    if (usedKills != 0) {
        qsort(kills, usedKills, sizeof(*kills), characterEditorKillsCompare);

        for (i = 0; i < usedKills; i++) {
            KillInfo* killInfo = &(kills[i]);
            if (characterEditorFolderViewDrawKillsEntry(killInfo->name, killInfo->kills)) {
                gCharacterEditorFolderCardFrmId = 46;
                gCharacterEditorFolderCardTitle = gCharacterEditorFolderCardString;
                gCharacterEditorFolderCardSubtitle = nullptr;
                gCharacterEditorFolderCardDescription = killTypeGetDescription(kills[i].killTypeId);
                snprintf(gCharacterEditorFolderCardString, sizeof(gCharacterEditorFolderCardString), "%s %s", killInfo->name, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 126));
                hasContent = true;
            }
        }
    }

    if (!hasContent) {
        gCharacterEditorFolderCardFrmId = 46;
        gCharacterEditorFolderCardTitle = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 126);
        gCharacterEditorFolderCardSubtitle = nullptr;
        gCharacterEditorFolderCardDescription = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 129);
    }

    return usedKills;
}

// 0x4345DC
static void characterEditorDrawBigNumber(int x, int y, int flags, int value, int previousValue, int windowHandle)
{
    Rect rect;
    int windowWidth;
    unsigned char* windowBuf;
    int tens;
    int ones;
    unsigned char* tensBufferPtr;
    unsigned char* onesBufferPtr;
    unsigned char* numbersGraphicBufferPtr;

    windowWidth = windowGetWidth(windowHandle);
    windowBuf = windowGetBuffer(windowHandle);

    rect.left = x;
    rect.top = y;
    rect.right = x + BIG_NUM_WIDTH * 2;
    rect.bottom = y + BIG_NUM_HEIGHT;

    numbersGraphicBufferPtr = _editorFrmImages[0].getData();

    if (flags & RED_NUMBERS) {
        // First half of the bignum.frm is white,
        // second half is red.
        numbersGraphicBufferPtr += _editorFrmImages[EDITOR_GRAPHIC_BIG_NUMBERS].getWidth() / 2;
    }

    tensBufferPtr = windowBuf + windowWidth * y + x;
    onesBufferPtr = tensBufferPtr + BIG_NUM_WIDTH;

    if (value >= 0 && value <= 99 && previousValue >= 0 && previousValue <= 99) {
        tens = value / 10;
        ones = value % 10;

        if (flags & ANIMATE) {
            if (previousValue % 10 != ones) {
                _frame_time = getTicks();
                blitBufferToBuffer(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 11,
                    BIG_NUM_WIDTH,
                    BIG_NUM_HEIGHT,
                    _editorFrmImages[EDITOR_GRAPHIC_BIG_NUMBERS].getWidth(),
                    onesBufferPtr,
                    windowWidth);
                windowRefreshRect(windowHandle, &rect);
                renderPresent();
                delay_ms(BIG_NUM_ANIMATION_DELAY - (getTicks() - _frame_time));
            }

            blitBufferToBuffer(numbersGraphicBufferPtr + BIG_NUM_WIDTH * ones,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                _editorFrmImages[EDITOR_GRAPHIC_BIG_NUMBERS].getWidth(),
                onesBufferPtr,
                windowWidth);
            windowRefreshRect(windowHandle, &rect);
            renderPresent();

            if (previousValue / 10 != tens) {
                _frame_time = getTicks();
                blitBufferToBuffer(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 11,
                    BIG_NUM_WIDTH,
                    BIG_NUM_HEIGHT,
                    _editorFrmImages[EDITOR_GRAPHIC_BIG_NUMBERS].getWidth(),
                    tensBufferPtr,
                    windowWidth);
                windowRefreshRect(windowHandle, &rect);
                renderPresent();
                delay_ms(BIG_NUM_ANIMATION_DELAY - (getTicks() - _frame_time));
            }

            blitBufferToBuffer(numbersGraphicBufferPtr + BIG_NUM_WIDTH * tens,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                _editorFrmImages[EDITOR_GRAPHIC_BIG_NUMBERS].getWidth(),
                tensBufferPtr,
                windowWidth);
            windowRefreshRect(windowHandle, &rect);
            renderPresent();
        } else {
            blitBufferToBuffer(numbersGraphicBufferPtr + BIG_NUM_WIDTH * tens,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                _editorFrmImages[EDITOR_GRAPHIC_BIG_NUMBERS].getWidth(),
                tensBufferPtr,
                windowWidth);
            blitBufferToBuffer(numbersGraphicBufferPtr + BIG_NUM_WIDTH * ones,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                _editorFrmImages[EDITOR_GRAPHIC_BIG_NUMBERS].getWidth(),
                onesBufferPtr,
                windowWidth);
        }
    } else {

        blitBufferToBuffer(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 9,
            BIG_NUM_WIDTH,
            BIG_NUM_HEIGHT,
            _editorFrmImages[EDITOR_GRAPHIC_BIG_NUMBERS].getWidth(),
            tensBufferPtr,
            windowWidth);
        blitBufferToBuffer(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 9,
            BIG_NUM_WIDTH,
            BIG_NUM_HEIGHT,
            _editorFrmImages[EDITOR_GRAPHIC_BIG_NUMBERS].getWidth(),
            onesBufferPtr,
            windowWidth);
    }
}

// 0x434920
static void characterEditorDrawPcStats()
{
    int color;
    int y;
    char* formattedValue;
    // NOTE: The length of this buffer is 8 bytes, which is enough to display
    // 999,999 (7 bytes NULL-terminated) experience points. Usually a player
    // will never gain that much during normal gameplay.
    //
    // However it's possible to use one of the F2 modding tools and savegame
    // editors to receive rediculous amount of experience points. Vanilla is
    // able to handle it, because `stringBuffer` acts as continuation of
    // `formattedValueBuffer`. This is not the case with MSVC, where
    // insufficient space for xp greater then 999,999 ruins the stack. In order
    // to fix the `formattedValueBuffer` is expanded to 16 bytes, so it should
    // be possible to store max 32-bit integer (4,294,967,295).
    char formattedValueBuffer[16];
    char stringBuffer[128];

    if (gCharacterEditorIsCreationMode) {
        return;
    }

    fontSetCurrent(101);

    blitBufferToBuffer(_editorBackgroundFrmImage.getData() + 640 * 280 + 32, 124, 32, 640, gCharacterEditorWindowBuffer + 640 * 280 + 32, 640);

    // LEVEL
    y = 280;
    if (characterEditorSelectedItem != 7) {
        color = _colorTable[992];
    } else {
        color = _colorTable[32747];
    }

    int level = pcGetStat(PC_STAT_LEVEL);
    snprintf(stringBuffer, sizeof(stringBuffer), "%s %d",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 113),
        level);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 32, stringBuffer, 640, 640, color);

    // EXPERIENCE
    y += fontGetLineHeight() + 1;
    if (characterEditorSelectedItem != 8) {
        color = _colorTable[992];
    } else {
        color = _colorTable[32747];
    }

    int exp = pcGetStat(PC_STAT_EXPERIENCE);
    snprintf(stringBuffer, sizeof(stringBuffer), "%s %s",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 114),
        _itostndn(exp, formattedValueBuffer));
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 32, stringBuffer, 640, 640, color);

    // EXP NEEDED TO NEXT LEVEL
    y += fontGetLineHeight() + 1;
    if (characterEditorSelectedItem != 9) {
        color = _colorTable[992];
    } else {
        color = _colorTable[32747];
    }

    int expToNextLevel = pcGetExperienceForNextLevel();
    int expMsgId;
    if (expToNextLevel == -1) {
        expMsgId = 115;
        formattedValue = byte_5016E4;
    } else {
        expMsgId = 115;
        if (expToNextLevel > 999999) {
            expMsgId = 175;
        }
        formattedValue = _itostndn(expToNextLevel, formattedValueBuffer);
    }

    snprintf(stringBuffer, sizeof(stringBuffer), "%s %s",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, expMsgId),
        formattedValue);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 32, stringBuffer, 640, 640, color);
}

// 0x434B38
static void characterEditorDrawPrimaryStat(int stat, bool animate, int previousValue)
{
    int off;
    int color;
    const char* description;
    int value;
    int flags;
    int messageListItemId;

    fontSetCurrent(101);

    if (stat == RENDER_ALL_STATS) {
        // NOTE: Original code is different, looks like tail recursion
        // optimization.
        for (stat = 0; stat < 7; stat++) {
            characterEditorDrawPrimaryStat(stat, 0, 0);
        }
        return;
    }

    if (characterEditorSelectedItem == stat) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    off = 640 * (gCharacterEditorPrimaryStatY[stat] + 8) + 103;

    // TODO: The original code is different.
    if (gCharacterEditorIsCreationMode) {
        value = critterGetBaseStatWithTraitModifier(gDude, stat) + critterGetBonusStat(gDude, stat);

        flags = 0;

        if (animate) {
            flags |= ANIMATE;
        }

        if (value > 10) {
            flags |= RED_NUMBERS;
        }

        characterEditorDrawBigNumber(58, gCharacterEditorPrimaryStatY[stat], flags, value, previousValue, gCharacterEditorWindow);

        blitBufferToBuffer(_editorBackgroundFrmImage.getData() + off, 40, fontGetLineHeight(), 640, gCharacterEditorWindowBuffer + off, 640);

        messageListItemId = critterGetStat(gDude, stat) + 199;
        if (messageListItemId > 210) {
            messageListItemId = 210;
        }

        description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, messageListItemId);
        fontDrawText(gCharacterEditorWindowBuffer + 640 * (gCharacterEditorPrimaryStatY[stat] + 8) + 103, description, 640, 640, color);
    } else {
        value = critterGetStat(gDude, stat);
        characterEditorDrawBigNumber(58, gCharacterEditorPrimaryStatY[stat], 0, value, 0, gCharacterEditorWindow);
        blitBufferToBuffer(_editorBackgroundFrmImage.getData() + off, 40, fontGetLineHeight(), 640, gCharacterEditorWindowBuffer + off, 640);

        value = critterGetStat(gDude, stat);
        if (value > 10) {
            value = 10;
        }

        description = statGetValueDescription(value);
        fontDrawText(gCharacterEditorWindowBuffer + off, description, 640, 640, color);
    }
}

// 0x434F18
static void characterEditorDrawGender()
{
    int gender;
    char* str;
    char text[32];
    int x, width;

    fontSetCurrent(103);

    gender = critterGetStat(gDude, STAT_GENDER);
    str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 107 + gender);

    strcpy(text, str);

    width = _editorFrmImages[EDITOR_GRAPHIC_SEX_ON].getWidth();
    x = (width / 2) - (fontGetStringWidth(text) / 2);

    memcpy(gCharacterEditorFrmCopy[11],
        _editorFrmImages[EDITOR_GRAPHIC_SEX_ON].getData(),
        width * _editorFrmImages[EDITOR_GRAPHIC_SEX_ON].getHeight());
    memcpy(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_SEX_OFF],
        _editorFrmImages[10].getData(),
        width * _editorFrmImages[EDITOR_GRAPHIC_SEX_OFF].getHeight());

    x += 6 * width;
    fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_SEX_ON] + x, text, width, width, _colorTable[14723]);
    fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_SEX_OFF] + x, text, width, width, _colorTable[18979]);
}

// 0x43501C
static void characterEditorDrawAge()
{
    int age;
    char* str;
    char text[32];
    int x, width;

    fontSetCurrent(103);

    age = critterGetStat(gDude, STAT_AGE);
    str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 104);

    snprintf(text, sizeof(text), "%s %d", str, age);

    width = _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getWidth();
    x = (width / 2) + 1 - (fontGetStringWidth(text) / 2);

    memcpy(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_AGE_ON],
        _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getData(),
        width * _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getHeight());
    memcpy(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_AGE_OFF],
        _editorFrmImages[EDITOR_GRAPHIC_AGE_OFF].getData(),
        width * _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getHeight());

    x += 6 * width;
    fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_AGE_ON] + x, text, width, width, _colorTable[14723]);
    fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_AGE_OFF] + x, text, width, width, _colorTable[18979]);
}

// 0x435118
static void characterEditorDrawName()
{
    char* str;
    char text[32];
    int x, width;
    char *pch, tmp;
    bool has_space;

    fontSetCurrent(103);

    str = critterGetName(gDude);
    strcpy(text, str);

    if (fontGetStringWidth(text) > 100) {
        pch = text;
        has_space = false;
        while (*pch != '\0') {
            tmp = *pch;
            *pch = '\0';
            if (tmp == ' ') {
                has_space = true;
            }

            if (fontGetStringWidth(text) > 100) {
                break;
            }

            *pch = tmp;
            pch++;
        }

        if (has_space) {
            pch = text + strlen(text);
            while (pch != text && *pch != ' ') {
                *pch = '\0';
                pch--;
            }
        }
    }

    width = _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getWidth();
    x = (width / 2) + 3 - (fontGetStringWidth(text) / 2);

    memcpy(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_NAME_ON],
        _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getWidth() * _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getHeight());
    memcpy(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_NAME_OFF],
        _editorFrmImages[EDITOR_GRAPHIC_NAME_OFF].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_NAME_OFF].getWidth() * _editorFrmImages[EDITOR_GRAPHIC_NAME_OFF].getHeight());

    x += 6 * width;
    fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_NAME_ON] + x, text, width, width, _colorTable[14723]);
    fontDrawText(gCharacterEditorFrmCopy[EDITOR_GRAPHIC_NAME_OFF] + x, text, width, width, _colorTable[18979]);
}

// 0x43527C
static void characterEditorDrawDerivedStats()
{
    int conditions;
    int color;
    const char* messageListItemText;
    char t[420]; // TODO: Size is wrong.
    int y;

    conditions = gDude->data.critter.combat.results;

    fontSetCurrent(101);

    y = 46;

    blitBufferToBuffer(_editorBackgroundFrmImage.getData() + 640 * y + 194, 118, 108, 640, gCharacterEditorWindowBuffer + 640 * y + 194, 640);

    // Hit Points
    if (characterEditorSelectedItem == EDITOR_HIT_POINTS) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    int currHp;
    int maxHp;
    if (gCharacterEditorIsCreationMode) {
        maxHp = critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS);
        currHp = maxHp;
    } else {
        maxHp = critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS);
        currHp = critterGetHitPoints(gDude);
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 300);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    snprintf(t, sizeof(t), "%d/%d", currHp, maxHp);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 263, t, 640, 640, color);

    // Poisoned
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_POISONED) {
        color = critterGetPoison(gDude) != 0 ? _colorTable[32747] : _colorTable[15845];
    } else {
        color = critterGetPoison(gDude) != 0 ? _colorTable[992] : _colorTable[1313];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 312);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    // Radiated
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_RADIATED) {
        color = critterGetRadiation(gDude) != 0 ? _colorTable[32747] : _colorTable[15845];
    } else {
        color = critterGetRadiation(gDude) != 0 ? _colorTable[992] : _colorTable[1313];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 313);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    // Eye Damage
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_EYE_DAMAGE) {
        color = (conditions & DAM_BLIND) ? _colorTable[32747] : _colorTable[15845];
    } else {
        color = (conditions & DAM_BLIND) ? _colorTable[992] : _colorTable[1313];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 314);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    // Crippled Right Arm
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_CRIPPLED_RIGHT_ARM) {
        color = (conditions & DAM_CRIP_ARM_RIGHT) ? _colorTable[32747] : _colorTable[15845];
    } else {
        color = (conditions & DAM_CRIP_ARM_RIGHT) ? _colorTable[992] : _colorTable[1313];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 315);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    // Crippled Left Arm
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_CRIPPLED_LEFT_ARM) {
        color = (conditions & DAM_CRIP_ARM_LEFT) ? _colorTable[32747] : _colorTable[15845];
    } else {
        color = (conditions & DAM_CRIP_ARM_LEFT) ? _colorTable[992] : _colorTable[1313];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 316);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    // Crippled Right Leg
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_CRIPPLED_RIGHT_LEG) {
        color = (conditions & DAM_CRIP_LEG_RIGHT) ? _colorTable[32747] : _colorTable[15845];
    } else {
        color = (conditions & DAM_CRIP_LEG_RIGHT) ? _colorTable[992] : _colorTable[1313];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 317);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    // Crippled Left Leg
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_CRIPPLED_LEFT_LEG) {
        color = (conditions & DAM_CRIP_LEG_LEFT) ? _colorTable[32747] : _colorTable[15845];
    } else {
        color = (conditions & DAM_CRIP_LEG_LEFT) ? _colorTable[992] : _colorTable[1313];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 318);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    y = 179;

    blitBufferToBuffer(_editorBackgroundFrmImage.getData() + 640 * y + 194, 116, 130, 640, gCharacterEditorWindowBuffer + 640 * y + 194, 640);

    // Armor Class
    if (characterEditorSelectedItem == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_ARMOR_CLASS) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 302);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    compat_itoa(critterGetStat(gDude, STAT_ARMOR_CLASS), t, 10);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 288, t, 640, 640, color);

    // Action Points
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_ACTION_POINTS) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 301);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    compat_itoa(critterGetStat(gDude, STAT_MAXIMUM_ACTION_POINTS), t, 10);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 288, t, 640, 640, color);

    // Carry Weight
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_CARRY_WEIGHT) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 311);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    compat_itoa(critterGetStat(gDude, STAT_CARRY_WEIGHT), t, 10);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 288, t, 640, 640, critterIsEncumbered(gDude) ? _colorTable[31744] : color);

    // Melee Damage
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_MELEE_DAMAGE) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 304);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    // SFALL: Display melee damage without "Bonus HtH Damage" bonus.
    int meleeDamage = critterGetStat(gDude, STAT_MELEE_DAMAGE);
    if (!damageModGetDisplayBonusDamage()) {
        meleeDamage -= 2 * perkGetRank(gDude, PERK_BONUS_HTH_DAMAGE);
    }

    compat_itoa(meleeDamage, t, 10);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 288, t, 640, 640, color);

    // Damage Resistance
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_DAMAGE_RESISTANCE) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 305);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    snprintf(t, sizeof(t), "%d%%", critterGetStat(gDude, STAT_DAMAGE_RESISTANCE));
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 288, t, 640, 640, color);

    // Poison Resistance
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_POISON_RESISTANCE) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 306);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    snprintf(t, sizeof(t), "%d%%", critterGetStat(gDude, STAT_POISON_RESISTANCE));
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 288, t, 640, 640, color);

    // Radiation Resistance
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_RADIATION_RESISTANCE) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 307);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    snprintf(t, sizeof(t), "%d%%", critterGetStat(gDude, STAT_RADIATION_RESISTANCE));
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 288, t, 640, 640, color);

    // Sequence
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_SEQUENCE) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 308);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    compat_itoa(critterGetStat(gDude, STAT_SEQUENCE), t, 10);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 288, t, 640, 640, color);

    // Healing Rate
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_HEALING_RATE) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 309);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    compat_itoa(critterGetStat(gDude, STAT_HEALING_RATE), t, 10);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 288, t, 640, 640, color);

    // Critical Chance
    y += fontGetLineHeight() + 3;

    if (characterEditorSelectedItem == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_CRITICAL_CHANCE) {
        color = _colorTable[32747];
    } else {
        color = _colorTable[992];
    }

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 310);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 194, t, 640, 640, color);

    snprintf(t, sizeof(t), "%d%%", critterGetStat(gDude, STAT_CRITICAL_CHANCE));
    fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 288, t, 640, 640, color);
}

// 0x436154
static void characterEditorDrawSkills(int a1)
{
    int selectedSkill = -1;
    const char* str;
    int i;
    int color;
    int y;
    int value;
    char valueString[32];

    if (characterEditorSelectedItem >= EDITOR_FIRST_SKILL && characterEditorSelectedItem < 79) {
        selectedSkill = characterEditorSelectedItem - EDITOR_FIRST_SKILL;
    }

    if (!gCharacterEditorIsCreationMode && a1 == 0) {
        buttonDestroy(gCharacterEditorSliderPlusBtn);
        buttonDestroy(gCharacterEditorSliderMinusBtn);
        gCharacterEditorSliderMinusBtn = -1;
        gCharacterEditorSliderPlusBtn = -1;
    }

    blitBufferToBuffer(_editorBackgroundFrmImage.getData() + 370, 270, 252, 640, gCharacterEditorWindowBuffer + 370, 640);

    fontSetCurrent(103);

    // SKILLS
    str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 117);
    fontDrawText(gCharacterEditorWindowBuffer + 640 * 5 + 380, str, 640, 640, _colorTable[18979]);

    if (!gCharacterEditorIsCreationMode) {
        // SKILL POINTS
        str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 112);
        fontDrawText(gCharacterEditorWindowBuffer + 640 * 233 + 400, str, 640, 640, _colorTable[18979]);

        value = pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS);
        characterEditorDrawBigNumber(522, 228, 0, value, 0, gCharacterEditorWindow);
    } else {
        // TAG SKILLS
        str = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 138);
        fontDrawText(gCharacterEditorWindowBuffer + 640 * 233 + 422, str, 640, 640, _colorTable[18979]);

        if (a1 == 2 && !gCharacterEditorIsSkillsFirstDraw) {
            characterEditorDrawBigNumber(522, 228, ANIMATE, gCharacterEditorTaggedSkillCount, gCharacterEditorOldTaggedSkillCount, gCharacterEditorWindow);
        } else {
            characterEditorDrawBigNumber(522, 228, 0, gCharacterEditorTaggedSkillCount, 0, gCharacterEditorWindow);
            gCharacterEditorIsSkillsFirstDraw = 0;
        }
    }

    skillsSetTagged(gCharacterEditorTempTaggedSkills, NUM_TAGGED_SKILLS);

    fontSetCurrent(101);

    y = 27;
    for (i = 0; i < SKILL_COUNT; i++) {
        if (i == selectedSkill) {
            if (i != gCharacterEditorTempTaggedSkills[0] && i != gCharacterEditorTempTaggedSkills[1] && i != gCharacterEditorTempTaggedSkills[2] && i != gCharacterEditorTempTaggedSkills[3]) {
                color = _colorTable[32747];
            } else {
                color = _colorTable[32767];
            }
        } else {
            if (i != gCharacterEditorTempTaggedSkills[0] && i != gCharacterEditorTempTaggedSkills[1] && i != gCharacterEditorTempTaggedSkills[2] && i != gCharacterEditorTempTaggedSkills[3]) {
                color = _colorTable[992];
            } else {
                color = _colorTable[21140];
            }
        }

        str = skillGetName(i);
        fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 380, str, 640, 640, color);

        value = skillGetValue(gDude, i);
        snprintf(valueString, sizeof(valueString), "%d%%", value);

        fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 573, valueString, 640, 640, color);

        y += fontGetLineHeight() + 1;
    }

    if (!gCharacterEditorIsCreationMode) {
        y = gCharacterEditorCurrentSkill * (fontGetLineHeight() + 1);
        gCharacterEditorSkillValueAdjustmentSliderY = y + 27;

        blitBufferToBufferTrans(
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER].getData(),
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER].getWidth(),
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER].getHeight(),
            _editorFrmImages[EDITOR_GRAPHIC_SLIDER].getWidth(),
            gCharacterEditorWindowBuffer + 640 * (y + 16) + 592,
            640);

        if (a1 == 0) {
            if (gCharacterEditorSliderPlusBtn == -1) {
                gCharacterEditorSliderPlusBtn = buttonCreate(
                    gCharacterEditorWindow,
                    614,
                    gCharacterEditorSkillValueAdjustmentSliderY - 7,
                    _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_ON].getWidth(),
                    _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_ON].getHeight(),
                    -1,
                    522,
                    521,
                    522,
                    _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_OFF].getData(),
                    _editorFrmImages[EDITOR_GRAPHIC_SLIDER_PLUS_ON].getData(),
                    nullptr,
                    96);
                buttonSetCallbacks(gCharacterEditorSliderPlusBtn, _gsound_red_butt_press, nullptr);
            }

            if (gCharacterEditorSliderMinusBtn == -1) {
                gCharacterEditorSliderMinusBtn = buttonCreate(
                    gCharacterEditorWindow,
                    614,
                    gCharacterEditorSkillValueAdjustmentSliderY + 4 - 12 + _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_ON].getHeight(),
                    _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_ON].getWidth(),
                    _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_OFF].getHeight(),
                    -1,
                    524,
                    523,
                    524,
                    _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_OFF].getData(),
                    _editorFrmImages[EDITOR_GRAPHIC_SLIDER_MINUS_ON].getData(),
                    nullptr,
                    96);
                buttonSetCallbacks(gCharacterEditorSliderMinusBtn, _gsound_red_butt_press, nullptr);
            }
        }
    }
}

// 0x4365AC
static void characterEditorDrawCard()
{
    int graphicId;
    char* title;
    char* description;

    if (characterEditorSelectedItem < 0 || characterEditorSelectedItem >= 98) {
        return;
    }

    blitBufferToBuffer(_editorBackgroundFrmImage.getData() + (640 * 267) + 345, 277, 170, 640, gCharacterEditorWindowBuffer + (267 * 640) + 345, 640);

    if (characterEditorSelectedItem >= 0 && characterEditorSelectedItem < 7) {
        description = statGetDescription(characterEditorSelectedItem);
        title = statGetName(characterEditorSelectedItem);
        graphicId = statGetFrmId(characterEditorSelectedItem);
        characterEditorDrawCardWithOptions(graphicId, title, nullptr, description);
    } else if (characterEditorSelectedItem >= 7 && characterEditorSelectedItem < 10) {
        if (gCharacterEditorIsCreationMode) {
            switch (characterEditorSelectedItem) {
            case 7:
                // Character Points
                description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 121);
                title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 120);
                characterEditorDrawCardWithOptions(7, title, nullptr, description);
                break;
            }
        } else {
            switch (characterEditorSelectedItem) {
            case 7:
                description = pcStatGetDescription(PC_STAT_LEVEL);
                title = pcStatGetName(PC_STAT_LEVEL);
                characterEditorDrawCardWithOptions(7, title, nullptr, description);
                break;
            case 8:
                description = pcStatGetDescription(PC_STAT_EXPERIENCE);
                title = pcStatGetName(PC_STAT_EXPERIENCE);
                characterEditorDrawCardWithOptions(8, title, nullptr, description);
                break;
            case 9:
                // Next Level
                description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 123);
                title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 122);
                characterEditorDrawCardWithOptions(9, title, nullptr, description);
                break;
            }
        }
    } else if ((characterEditorSelectedItem >= 10 && characterEditorSelectedItem < 43) || (characterEditorSelectedItem >= 82 && characterEditorSelectedItem < 98)) {
        characterEditorDrawCardWithOptions(gCharacterEditorFolderCardFrmId, gCharacterEditorFolderCardTitle, gCharacterEditorFolderCardSubtitle, gCharacterEditorFolderCardDescription);
    } else if (characterEditorSelectedItem >= 43 && characterEditorSelectedItem < 51) {
        switch (characterEditorSelectedItem) {
        case EDITOR_HIT_POINTS:
            description = statGetDescription(STAT_MAXIMUM_HIT_POINTS);
            title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 300);
            graphicId = statGetFrmId(STAT_MAXIMUM_HIT_POINTS);
            characterEditorDrawCardWithOptions(graphicId, title, nullptr, description);
            break;
        case EDITOR_POISONED:
            description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 400);
            title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 312);
            characterEditorDrawCardWithOptions(11, title, nullptr, description);
            break;
        case EDITOR_RADIATED:
            description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 401);
            title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 313);
            characterEditorDrawCardWithOptions(12, title, nullptr, description);
            break;
        case EDITOR_EYE_DAMAGE:
            description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 402);
            title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 314);
            characterEditorDrawCardWithOptions(13, title, nullptr, description);
            break;
        case EDITOR_CRIPPLED_RIGHT_ARM:
            description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 403);
            title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 315);
            characterEditorDrawCardWithOptions(14, title, nullptr, description);
            break;
        case EDITOR_CRIPPLED_LEFT_ARM:
            description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 404);
            title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 316);
            characterEditorDrawCardWithOptions(15, title, nullptr, description);
            break;
        case EDITOR_CRIPPLED_RIGHT_LEG:
            description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 405);
            title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 317);
            characterEditorDrawCardWithOptions(16, title, nullptr, description);
            break;
        case EDITOR_CRIPPLED_LEFT_LEG:
            description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 406);
            title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 318);
            characterEditorDrawCardWithOptions(17, title, nullptr, description);
            break;
        }
    } else if (characterEditorSelectedItem >= EDITOR_FIRST_DERIVED_STAT && characterEditorSelectedItem < 61) {
        int derivedStatIndex = characterEditorSelectedItem - 51;
        int stat = gCharacterEditorDerivedStatsMap[derivedStatIndex];
        description = statGetDescription(stat);
        title = statGetName(stat);
        graphicId = gCharacterEditorDerivedStatFrmIds[derivedStatIndex];
        characterEditorDrawCardWithOptions(graphicId, title, nullptr, description);
    } else if (characterEditorSelectedItem >= EDITOR_FIRST_SKILL && characterEditorSelectedItem < 79) {
        int skill = characterEditorSelectedItem - 61;
        const char* attributesDescription = skillGetAttributes(skill);

        char formatted[150]; // TODO: Size is probably wrong.
        const char* base = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 137);
        int defaultValue = skillGetDefaultValue(skill);
        snprintf(formatted, sizeof(formatted), "%s %d%% %s", base, defaultValue, attributesDescription);

        graphicId = skillGetFrmId(skill);
        title = skillGetName(skill);
        description = skillGetDescription(skill);
        characterEditorDrawCardWithOptions(graphicId, title, formatted, description);
    } else if (characterEditorSelectedItem >= 79 && characterEditorSelectedItem < 82) {
        switch (characterEditorSelectedItem) {
        case EDITOR_TAG_SKILL:
            if (gCharacterEditorIsCreationMode) {
                // Tag Skill
                description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 145);
                title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 144);
                characterEditorDrawCardWithOptions(27, title, nullptr, description);
            } else {
                // Skill Points
                description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 131);
                title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 130);
                characterEditorDrawCardWithOptions(27, title, nullptr, description);
            }
            break;
        case EDITOR_SKILLS:
            // Skills
            description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 151);
            title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 150);
            characterEditorDrawCardWithOptions(27, title, nullptr, description);
            break;
        case EDITOR_OPTIONAL_TRAITS:
            // Optional Traits
            description = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 147);
            title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 146);
            characterEditorDrawCardWithOptions(27, title, nullptr, description);
            break;
        }
    }
}

// 0x436C4C
static int characterEditorEditName()
{
    char* text;

    int windowWidth = _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getWidth();
    int windowHeight = _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getHeight();

    int nameWindowX = (screenGetWidth() - EDITOR_WINDOW_WIDTH) / 2 + 17;
    int nameWindowY = (screenGetHeight() - EDITOR_WINDOW_HEIGHT) / 2;
    int win = windowCreate(nameWindowX, nameWindowY, windowWidth, windowHeight, 256, WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (win == -1) {
        return -1;
    }

    unsigned char* windowBuf = windowGetBuffer(win);

    // Copy background
    memcpy(windowBuf, _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getData(), windowWidth * windowHeight);

    blitBufferToBufferTrans(
        _editorFrmImages[EDITOR_GRAPHIC_NAME_BOX].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_NAME_BOX].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_NAME_BOX].getHeight(),
        _editorFrmImages[EDITOR_GRAPHIC_NAME_BOX].getWidth(),
        windowBuf + windowWidth * 13 + 13,
        windowWidth);
    blitBufferToBufferTrans(_editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getHeight(),
        _editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getWidth(),
        windowBuf + windowWidth * 40 + 13,
        windowWidth);

    fontSetCurrent(103);

    text = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 100);
    fontDrawText(windowBuf + windowWidth * 44 + 50, text, windowWidth, windowWidth, _colorTable[18979]);

    int doneBtn = buttonCreate(win,
        26,
        44,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getHeight(),
        -1,
        -1,
        -1,
        500,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(win);

    fontSetCurrent(101);

    char name[64];
    strcpy(name, critterGetName(gDude));

    if (strcmp(name, "None") == 0) {
        name[0] = '\0';
    }

    // NOTE: I don't understand the nameCopy, not sure what it is used for. It's
    // definitely there, but I just don' get it.
    char nameCopy[64];
    strcpy(nameCopy, name);

    if (_get_input_str(win, 500, nameCopy, 11, 23, 19, _colorTable[992], 100, 0) != -1) {
        if (nameCopy[0] != '\0') {
            dudeSetName(nameCopy);
            characterEditorDrawName();
            windowDestroy(win);
            return 0;
        }
    }

    // NOTE: original code is a bit different, the following chunk of code written two times.

    fontSetCurrent(101);
    blitBufferToBuffer(_editorFrmImages[EDITOR_GRAPHIC_NAME_BOX].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_NAME_BOX].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_NAME_BOX].getHeight(),
        _editorFrmImages[EDITOR_GRAPHIC_NAME_BOX].getWidth(),
        windowBuf + _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getWidth() * 13 + 13,
        _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getWidth());

    _PrintName(windowBuf, _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getWidth());

    strcpy(nameCopy, name);

    windowDestroy(win);

    return 0;
}

// 0x436F70
static void _PrintName(unsigned char* buf, int pitch)
{
    char str[64];
    char* v4;

    memcpy(str, byte_431D93, 64);

    fontSetCurrent(101);

    v4 = critterGetName(gDude);

    // TODO: Check.
    strcpy(str, v4);

    fontDrawText(buf + 19 * pitch + 21, str, pitch, pitch, _colorTable[992]);
}

// 0x436FEC
static int characterEditorEditAge()
{
    int win;
    unsigned char* windowBuf;
    int windowWidth;
    int windowHeight;
    const char* messageListItemText;
    int previousAge;
    int age;
    int doneBtn;
    int prevBtn;
    int nextBtn;
    int keyCode;
    int change;
    int flags;

    int savedAge = critterGetStat(gDude, STAT_AGE);

    windowWidth = _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getWidth();
    windowHeight = _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getHeight();

    int ageWindowX = (screenGetWidth() - EDITOR_WINDOW_WIDTH) / 2 + _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getWidth() + 9;
    int ageWindowY = (screenGetHeight() - EDITOR_WINDOW_HEIGHT) / 2;
    win = windowCreate(ageWindowX, ageWindowY, windowWidth, windowHeight, 256, WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (win == -1) {
        return -1;
    }

    windowBuf = windowGetBuffer(win);

    memcpy(windowBuf, _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getData(), windowWidth * windowHeight);

    blitBufferToBufferTrans(
        _editorFrmImages[EDITOR_GRAPHIC_AGE_BOX].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_AGE_BOX].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_AGE_BOX].getHeight(),
        _editorFrmImages[EDITOR_GRAPHIC_AGE_BOX].getWidth(),
        windowBuf + windowWidth * 7 + 8,
        windowWidth);
    blitBufferToBufferTrans(
        _editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getHeight(),
        _editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getWidth(),
        windowBuf + windowWidth * 40 + 13,
        _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getWidth());

    fontSetCurrent(103);

    messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 100);
    fontDrawText(windowBuf + windowWidth * 44 + 50, messageListItemText, windowWidth, windowWidth, _colorTable[18979]);

    age = critterGetStat(gDude, STAT_AGE);
    characterEditorDrawBigNumber(55, 10, 0, age, 0, win);

    doneBtn = buttonCreate(win,
        26,
        44,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getHeight(),
        -1,
        -1,
        -1,
        500,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    nextBtn = buttonCreate(win,
        105,
        13,
        _editorFrmImages[EDITOR_GRAPHIC_LEFT_ARROW_DOWN].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_LEFT_ARROW_DOWN].getHeight(),
        -1,
        503,
        501,
        503,
        _editorFrmImages[EDITOR_GRAPHIC_RIGHT_ARROW_UP].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_RIGHT_ARROW_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (nextBtn != -1) {
        buttonSetCallbacks(nextBtn, _gsound_med_butt_press, nullptr);
    }

    prevBtn = buttonCreate(win,
        19,
        13,
        _editorFrmImages[EDITOR_GRAPHIC_RIGHT_ARROW_DOWN].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_RIGHT_ARROW_DOWN].getHeight(),
        -1,
        504,
        502,
        504,
        _editorFrmImages[EDITOR_GRAPHIC_LEFT_ARROW_UP].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_LEFT_ARROW_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (prevBtn != -1) {
        buttonSetCallbacks(prevBtn, _gsound_med_butt_press, nullptr);
    }

    while (true) {
        sharedFpsLimiter.mark();

        _frame_time = getTicks();
        change = 0;
        flags = 0;
        int v32 = 0;

        keyCode = inputGetInput();

        if (keyCode == KEY_RETURN || keyCode == 500) {
            if (keyCode != 500) {
                soundPlayFile("ib1p1xx1");
            }

            windowDestroy(win);
            return 0;
        } else if (keyCode == KEY_ESCAPE || _game_user_wants_to_quit != 0) {
            break;
        } else if (keyCode == 501) {
            age = critterGetStat(gDude, STAT_AGE);
            if (age < 35) {
                change = 1;
            }
        } else if (keyCode == 502) {
            age = critterGetStat(gDude, STAT_AGE);
            if (age > 16) {
                change = -1;
            }
        } else if (keyCode == KEY_PLUS || keyCode == KEY_UPPERCASE_N || keyCode == KEY_ARROW_UP) {
            previousAge = critterGetStat(gDude, STAT_AGE);
            if (previousAge < 35) {
                flags = ANIMATE;
                if (critterIncBaseStat(gDude, STAT_AGE) != 0) {
                    flags = 0;
                }
                age = critterGetStat(gDude, STAT_AGE);
                characterEditorDrawBigNumber(55, 10, flags, age, previousAge, win);
            }
        } else if (keyCode == KEY_MINUS || keyCode == KEY_UPPERCASE_J || keyCode == KEY_ARROW_DOWN) {
            previousAge = critterGetStat(gDude, STAT_AGE);
            if (previousAge > 16) {
                flags = ANIMATE;
                if (critterDecBaseStat(gDude, STAT_AGE) != 0) {
                    flags = 0;
                }
                age = critterGetStat(gDude, STAT_AGE);

                characterEditorDrawBigNumber(55, 10, flags, age, previousAge, win);
            }
        }

        if (flags == ANIMATE) {
            characterEditorDrawAge();
            characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
            characterEditorDrawDerivedStats();
            windowRefresh(gCharacterEditorWindow);
            windowRefresh(win);
        }

        if (change != 0) {
            int v33 = 0;

            _repFtime = 4;

            while (true) {
                sharedFpsLimiter.mark();

                _frame_time = getTicks();

                v33++;

                if ((!v32 && v33 == 1) || (v32 && v33 > dbl_50170B)) {
                    v32 = true;

                    if (v33 > dbl_50170B) {
                        _repFtime++;
                        if (_repFtime > 24) {
                            _repFtime = 24;
                        }
                    }

                    flags = ANIMATE;
                    previousAge = critterGetStat(gDude, STAT_AGE);

                    if (change == 1) {
                        if (previousAge < 35) {
                            if (critterIncBaseStat(gDude, STAT_AGE) != 0) {
                                flags = 0;
                            }
                        }
                    } else {
                        if (previousAge >= 16) {
                            if (critterDecBaseStat(gDude, STAT_AGE) != 0) {
                                flags = 0;
                            }
                        }
                    }

                    age = critterGetStat(gDude, STAT_AGE);
                    characterEditorDrawBigNumber(55, 10, flags, age, previousAge, win);
                    if (flags == ANIMATE) {
                        characterEditorDrawAge();
                        characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
                        characterEditorDrawDerivedStats();
                        windowRefresh(gCharacterEditorWindow);
                        windowRefresh(win);
                    }
                }

                if (v33 > dbl_50170B) {
                    delay_ms(1000 / _repFtime - (getTicks() - _frame_time));
                } else {
                    delay_ms(1000 / 24 - (getTicks() - _frame_time));
                }

                keyCode = inputGetInput();
                if (keyCode == 503 || keyCode == 504 || _game_user_wants_to_quit != 0) {
                    break;
                }

                renderPresent();
                sharedFpsLimiter.throttle();
            }
        } else {
            windowRefresh(win);

            delay_ms(1000 / 24 - (getTicks() - _frame_time));
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    critterSetBaseStat(gDude, STAT_AGE, savedAge);
    characterEditorDrawAge();
    characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
    characterEditorDrawDerivedStats();
    windowRefresh(gCharacterEditorWindow);
    windowRefresh(win);
    windowDestroy(win);
    return 0;
}

// 0x437664
static void characterEditorEditGender()
{
    char* text;

    int windowWidth = _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getWidth();
    int windowHeight = _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getHeight();

    int genderWindowX = (screenGetWidth() - EDITOR_WINDOW_WIDTH) / 2 + 9
        + _editorFrmImages[EDITOR_GRAPHIC_NAME_ON].getWidth()
        + _editorFrmImages[EDITOR_GRAPHIC_AGE_ON].getWidth();
    int genderWindowY = (screenGetHeight() - EDITOR_WINDOW_HEIGHT) / 2;
    int win = windowCreate(genderWindowX, genderWindowY, windowWidth, windowHeight, 256, WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);

    if (win == -1) {
        return;
    }

    unsigned char* windowBuf = windowGetBuffer(win);

    // Copy background
    memcpy(windowBuf, _editorFrmImages[EDITOR_GRAPHIC_CHARWIN].getData(), windowWidth * windowHeight);

    blitBufferToBufferTrans(_editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getHeight(),
        _editorFrmImages[EDITOR_GRAPHIC_DONE_BOX].getWidth(),
        windowBuf + windowWidth * 44 + 15,
        windowWidth);

    fontSetCurrent(103);

    text = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 100);
    fontDrawText(windowBuf + windowWidth * 48 + 52, text, windowWidth, windowWidth, _colorTable[18979]);

    int doneBtn = buttonCreate(win,
        28,
        48,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getHeight(),
        -1,
        -1,
        -1,
        500,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int btns[2];
    btns[0] = buttonCreate(win,
        22,
        2,
        _editorFrmImages[EDITOR_GRAPHIC_MALE_ON].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_MALE_ON].getHeight(),
        -1,
        -1,
        501,
        -1,
        _editorFrmImages[EDITOR_GRAPHIC_MALE_OFF].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_MALE_ON].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_0x04 | BUTTON_FLAG_0x02 | BUTTON_FLAG_0x01);
    if (btns[0] != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, nullptr);
    }

    btns[1] = buttonCreate(win,
        71,
        3,
        _editorFrmImages[EDITOR_GRAPHIC_FEMALE_ON].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_FEMALE_ON].getHeight(),
        -1,
        -1,
        502,
        -1,
        _editorFrmImages[EDITOR_GRAPHIC_FEMALE_OFF].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_FEMALE_ON].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_0x04 | BUTTON_FLAG_0x02 | BUTTON_FLAG_0x01);
    if (btns[1] != -1) {
        _win_group_radio_buttons(2, btns);
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, nullptr);
    }

    int savedGender = critterGetStat(gDude, STAT_GENDER);
    _win_set_button_rest_state(btns[savedGender], 1, 0);

    while (true) {
        sharedFpsLimiter.mark();

        _frame_time = getTicks();

        int eventCode = inputGetInput();

        if (eventCode == KEY_RETURN || eventCode == 500) {
            if (eventCode == KEY_RETURN) {
                soundPlayFile("ib1p1xx1");
            }
            break;
        }

        if (eventCode == KEY_ESCAPE || _game_user_wants_to_quit != 0) {
            critterSetBaseStat(gDude, STAT_GENDER, savedGender);
            characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
            characterEditorDrawDerivedStats();
            windowRefresh(gCharacterEditorWindow);
            break;
        }

        switch (eventCode) {
        case KEY_ARROW_LEFT:
        case KEY_ARROW_RIGHT:
            if (1) {
                bool wasMale = _win_button_down(btns[0]);
                _win_set_button_rest_state(btns[0], !wasMale, 1);
                _win_set_button_rest_state(btns[1], wasMale, 1);
            }
            break;
        case 501:
        case 502:
            // TODO: Original code is slightly different.
            critterSetBaseStat(gDude, STAT_GENDER, eventCode - 501);
            characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
            characterEditorDrawDerivedStats();
            break;
        }

        windowRefresh(win);

        delay_ms(41 - (getTicks() - _frame_time));

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    characterEditorDrawGender();
    windowDestroy(win);
}

// 0x4379BC
static void characterEditorAdjustPrimaryStat(int eventCode)
{
    _repFtime = 4;

    int savedRemainingCharacterPoints = gCharacterEditorRemainingCharacterPoints;

    if (!gCharacterEditorIsCreationMode) {
        return;
    }

    int incrementingStat = eventCode - 503;
    int decrementingStat = eventCode - 510;

    int v11 = 0;

    bool cont = true;
    do {
        sharedFpsLimiter.mark();

        _frame_time = getTicks();
        if (v11 <= 19.2) {
            v11++;
        }

        if (v11 == 1 || v11 > 19.2) {
            if (v11 > 19.2) {
                _repFtime++;
                if (_repFtime > 24) {
                    _repFtime = 24;
                }
            }

            if (eventCode >= 510) {
                int previousValue = critterGetStat(gDude, decrementingStat);
                if (critterDecBaseStat(gDude, decrementingStat) == 0) {
                    gCharacterEditorRemainingCharacterPoints++;
                } else {
                    cont = false;
                }

                characterEditorDrawPrimaryStat(decrementingStat, cont ? ANIMATE : 0, previousValue);
                characterEditorDrawBigNumber(126, 282, cont ? ANIMATE : 0, gCharacterEditorRemainingCharacterPoints, savedRemainingCharacterPoints, gCharacterEditorWindow);
                critterUpdateDerivedStats(gDude);
                characterEditorDrawDerivedStats();
                characterEditorDrawSkills(0);
                characterEditorSelectedItem = decrementingStat;
            } else {
                int previousValue = critterGetBaseStatWithTraitModifier(gDude, incrementingStat);
                previousValue += critterGetBonusStat(gDude, incrementingStat);
                if (gCharacterEditorRemainingCharacterPoints > 0 && previousValue < 10 && critterIncBaseStat(gDude, incrementingStat) == 0) {
                    gCharacterEditorRemainingCharacterPoints--;
                } else {
                    cont = false;
                }

                characterEditorDrawPrimaryStat(incrementingStat, cont ? ANIMATE : 0, previousValue);
                characterEditorDrawBigNumber(126, 282, cont ? ANIMATE : 0, gCharacterEditorRemainingCharacterPoints, savedRemainingCharacterPoints, gCharacterEditorWindow);
                critterUpdateDerivedStats(gDude);
                characterEditorDrawDerivedStats();
                characterEditorDrawSkills(0);
                characterEditorSelectedItem = incrementingStat;
            }

            windowRefresh(gCharacterEditorWindow);
        }

        if (v11 >= 19.2) {
            delay_ms(1000 / _repFtime - (getTicks() - _frame_time));
        } else {
            delay_ms(1000 / 24 - (getTicks() - _frame_time));
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    } while (inputGetInput() != 518 && cont);

    characterEditorDrawCard();
}

// handle options dialog
//
// 0x437C08
static int characterEditorShowOptions()
{
    int width = _editorFrmImages[43].getWidth();
    int height = _editorFrmImages[43].getHeight();

    // NOTE: The following is a block of general purpose string buffers used in
    // this function. They are either store path, or strings from .msg files. I
    // don't know if such usage was intentional in the original code or it's a
    // result of some kind of compiler optimization.
    char string1[512];
    char string2[512];
    char string3[512];
    char string4[512];
    char string5[512];

    // Only two of the these blocks are used as a dialog body. Depending on the
    // dialog either 1 or 2 strings used from this array.
    const char* dialogBody[2] = {
        string5,
        string2,
    };

    if (gCharacterEditorIsCreationMode) {
        int optionsWindowX = (screenGetWidth() != 640)
            ? (screenGetWidth() - _editorFrmImages[41].getWidth()) / 2
            : 238;
        int optionsWindowY = (screenGetHeight() != 480)
            ? (screenGetHeight() - _editorFrmImages[41].getHeight()) / 2
            : 90;
        int win = windowCreate(optionsWindowX, optionsWindowY, _editorFrmImages[41].getWidth(), _editorFrmImages[41].getHeight(), 256, WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
        if (win == -1) {
            return -1;
        }

        unsigned char* windowBuffer = windowGetBuffer(win);
        memcpy(windowBuffer, _editorFrmImages[41].getData(), _editorFrmImages[41].getWidth() * _editorFrmImages[41].getHeight());

        fontSetCurrent(103);

        int err = 0;
        unsigned char* down[5];
        unsigned char* up[5];
        int size = width * height;
        int y = 17;
        int index;

        for (index = 0; index < 5; index++) {
            if (err != 0) {
                break;
            }

            do {
                down[index] = (unsigned char*)internal_malloc(size);
                if (down[index] == nullptr) {
                    err = 1;
                    break;
                }

                up[index] = (unsigned char*)internal_malloc(size);
                if (up[index] == nullptr) {
                    err = 2;
                    break;
                }

                memcpy(down[index], _editorFrmImages[43].getData(), size);
                memcpy(up[index], _editorFrmImages[42].getData(), size);

                strcpy(string4, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 600 + index));

                int offset = width * 7 + width / 2 - fontGetStringWidth(string4) / 2;
                fontDrawText(up[index] + offset, string4, width, width, _colorTable[18979]);
                fontDrawText(down[index] + offset, string4, width, width, _colorTable[14723]);

                int btn = buttonCreate(win, 13, y, width, height, -1, -1, -1, 500 + index, up[index], down[index], nullptr, BUTTON_FLAG_TRANSPARENT);
                if (btn != -1) {
                    buttonSetCallbacks(btn, _gsound_lrg_butt_press, nullptr);
                }
            } while (0);

            y += height + 3;
        }

        if (err != 0) {
            if (err == 2) {
                internal_free(down[index]);
            }

            while (--index >= 0) {
                internal_free(up[index]);
                internal_free(down[index]);
            }

            return -1;
        }

        fontSetCurrent(101);

        int rc = 0;
        while (rc == 0) {
            sharedFpsLimiter.mark();

            int keyCode = inputGetInput();

            if (_game_user_wants_to_quit != 0) {
                rc = 2;
            } else if (keyCode == 504) {
                rc = 2;
            } else if (keyCode == KEY_RETURN || keyCode == KEY_UPPERCASE_D || keyCode == KEY_LOWERCASE_D) {
                // DONE
                rc = 2;
                soundPlayFile("ib1p1xx1");
            } else if (keyCode == KEY_ESCAPE) {
                rc = 2;
            } else if (keyCode == 503 || keyCode == KEY_UPPERCASE_E || keyCode == KEY_LOWERCASE_E) {
                // ERASE
                strcpy(string5, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 605));
                strcpy(string2, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 606));

                if (showDialogBox(nullptr, dialogBody, 2, 169, 126, _colorTable[992], nullptr, _colorTable[992], DIALOG_BOX_YES_NO) != 0) {
                    _ResetPlayer();
                    skillsGetTagged(gCharacterEditorTempTaggedSkills, NUM_TAGGED_SKILLS);

                    int taggedSkillCount = 0;
                    for (int index = 3; index >= 0; index--) {
                        if (gCharacterEditorTempTaggedSkills[index] != -1) {
                            break;
                        }
                        taggedSkillCount++;
                    }

                    if (gCharacterEditorIsCreationMode) {
                        taggedSkillCount--;
                    }

                    gCharacterEditorTaggedSkillCount = taggedSkillCount;

                    traitsGetSelected(&gCharacterEditorTempTraits[0], &gCharacterEditorTempTraits[1]);

                    int traitCount = 0;
                    for (int index = 1; index >= 0; index--) {
                        if (gCharacterEditorTempTraits[index] != -1) {
                            break;
                        }
                        traitCount++;
                    }

                    gCharacterEditorTempTraitCount = traitCount;
                    critterUpdateDerivedStats(gDude);
                    characterEditorResetScreen();
                }
            } else if (keyCode == 502 || keyCode == KEY_UPPERCASE_P || keyCode == KEY_LOWERCASE_P) {
                // PRINT TO FILE
                string4[0] = '\0';

                strcat(string4, "*.");
                strcat(string4, "TXT");

                char** fileList;
                int fileListLength = fileNameListInit(string4, &fileList, 0, 0);
                if (fileListLength != -1) {
                    // PRINT
                    strcpy(string1, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 616));

                    // PRINT TO FILE
                    strcpy(string4, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 602));

                    if (showSaveFileDialog(string4, fileList, string1, fileListLength, 168, 80, 0) == 0) {
                        strcat(string1, ".");
                        strcat(string1, "TXT");

                        string4[0] = '\0';
                        strcat(string4, string1);

                        if (characterFileExists(string4)) {
                            // already exists
                            snprintf(string4, sizeof(string4),
                                "%s %s",
                                compat_strupr(string1),
                                getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 609));

                            strcpy(string5, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 610));

                            if (showDialogBox(string4, dialogBody, 1, 169, 126, _colorTable[32328], nullptr, _colorTable[32328], 0x10) != 0) {
                                rc = 1;
                            } else {
                                rc = 0;
                            }
                        } else {
                            rc = 1;
                        }

                        if (rc != 0) {
                            string4[0] = '\0';
                            strcat(string4, string1);

                            if (characterPrintToFile(string4) == 0) {
                                snprintf(string4, sizeof(string4),
                                    "%s%s",
                                    compat_strupr(string1),
                                    getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 607));
                                showDialogBox(string4, nullptr, 0, 169, 126, _colorTable[992], nullptr, _colorTable[992], 0);
                            } else {
                                soundPlayFile("iisxxxx1");

                                snprintf(string4, sizeof(string4),
                                    "%s%s%s",
                                    getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 611),
                                    compat_strupr(string1),
                                    "!");
                                showDialogBox(string4, nullptr, 0, 169, 126, _colorTable[32328], nullptr, _colorTable[992], 0x01);
                            }
                        }
                    }

                    fileNameListFree(&fileList, 0);
                } else {
                    soundPlayFile("iisxxxx1");

                    strcpy(string4, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 615));
                    showDialogBox(string4, nullptr, 0, 169, 126, _colorTable[32328], nullptr, _colorTable[32328], 0);

                    rc = 0;
                }
            } else if (keyCode == 501 || keyCode == KEY_UPPERCASE_L || keyCode == KEY_LOWERCASE_L) {
                // LOAD
                string4[0] = '\0';
                strcat(string4, "*.");
                strcat(string4, "GCD");

                char** fileNameList;
                int fileNameListLength = fileNameListInit(string4, &fileNameList, 0, 0);
                if (fileNameListLength != -1) {
                    // NOTE: This value is not copied as in save dialog.
                    char* title = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 601);
                    int loadFileDialogRc = showLoadFileDialog(title, fileNameList, string3, fileNameListLength, 168, 80, 0);
                    if (loadFileDialogRc == -1) {
                        fileNameListFree(&fileNameList, 0);
                        // FIXME: This branch ignores cleanup at the end of the loop.
                        return -1;
                    }

                    if (loadFileDialogRc == 0) {
                        string4[0] = '\0';
                        strcat(string4, string3);

                        int oldRemainingCharacterPoints = gCharacterEditorRemainingCharacterPoints;

                        _ResetPlayer();

                        if (gcdLoad(string4) == 0) {
                            critterUpdateDerivedStats(gDude);
                            pcStatsReset();
                            for (int stat = 0; stat < SAVEABLE_STAT_COUNT; stat++) {
                                critterSetBonusStat(gDude, stat, 0);
                            }
                            perksReset();
                            critterUpdateDerivedStats(gDude);
                            skillsGetTagged(gCharacterEditorTempTaggedSkills, 4);

                            int taggedSkillCount = 0;
                            for (int index = 3; index >= 0; index--) {
                                if (gCharacterEditorTempTaggedSkills[index] != -1) {
                                    break;
                                }
                                taggedSkillCount++;
                            }

                            if (gCharacterEditorIsCreationMode) {
                                taggedSkillCount--;
                            }

                            gCharacterEditorTaggedSkillCount = taggedSkillCount;

                            traitsGetSelected(&(gCharacterEditorTempTraits[0]), &(gCharacterEditorTempTraits[1]));

                            int traitCount = 0;
                            for (int index = 1; index >= 0; index--) {
                                if (gCharacterEditorTempTraits[index] != -1) {
                                    break;
                                }
                                traitCount++;
                            }

                            gCharacterEditorTempTraitCount = traitCount;

                            critterUpdateDerivedStats(gDude);

                            critterAdjustHitPoints(gDude, 1000);

                            rc = 1;
                        } else {
                            characterEditorRestorePlayer();
                            gCharacterEditorRemainingCharacterPoints = oldRemainingCharacterPoints;
                            critterAdjustHitPoints(gDude, 1000);
                            soundPlayFile("iisxxxx1");

                            strcpy(string4, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 612));
                            strcat(string4, string3);
                            strcat(string4, "!");

                            showDialogBox(string4, nullptr, 0, 169, 126, _colorTable[32328], nullptr, _colorTable[32328], 0);
                        }

                        characterEditorResetScreen();
                    }

                    fileNameListFree(&fileNameList, 0);
                } else {
                    soundPlayFile("iisxxxx1");

                    // Error reading file list!
                    strcpy(string4, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 615));
                    rc = 0;

                    showDialogBox(string4, nullptr, 0, 169, 126, _colorTable[32328], nullptr, _colorTable[32328], 0);
                }
            } else if (keyCode == 500 || keyCode == KEY_UPPERCASE_S || keyCode == KEY_LOWERCASE_S) {
                // SAVE
                string4[0] = '\0';
                strcat(string4, "*.");
                strcat(string4, "GCD");

                char** fileNameList;
                int fileNameListLength = fileNameListInit(string4, &fileNameList, 0, 0);
                if (fileNameListLength != -1) {
                    strcpy(string1, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 617));
                    strcpy(string4, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 600));

                    if (showSaveFileDialog(string4, fileNameList, string1, fileNameListLength, 168, 80, 0) == 0) {
                        strcat(string1, ".");
                        strcat(string1, "GCD");

                        string4[0] = '\0';
                        strcat(string4, string1);

                        bool shouldSave;
                        if (characterFileExists(string4)) {
                            snprintf(string4, sizeof(string4), "%s %s",
                                compat_strupr(string1),
                                getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 609));
                            strcpy(string5, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 610));

                            if (showDialogBox(string4, dialogBody, 1, 169, 126, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_YES_NO) != 0) {
                                shouldSave = true;
                            } else {
                                shouldSave = false;
                            }
                        } else {
                            shouldSave = true;
                        }

                        if (shouldSave) {
                            skillsSetTagged(gCharacterEditorTempTaggedSkills, 4);
                            traitsSetSelected(gCharacterEditorTempTraits[0], gCharacterEditorTempTraits[1]);

                            string4[0] = '\0';
                            strcat(string4, string1);

                            if (gcdSave(string4) != 0) {
                                soundPlayFile("iisxxxx1");
                                snprintf(string4, sizeof(string4), "%s%s!",
                                    compat_strupr(string1),
                                    getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 611));
                                showDialogBox(string4, nullptr, 0, 169, 126, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);
                                rc = 0;
                            } else {
                                snprintf(string4, sizeof(string4), "%s%s",
                                    compat_strupr(string1),
                                    getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 607));
                                showDialogBox(string4, nullptr, 0, 169, 126, _colorTable[992], nullptr, _colorTable[992], DIALOG_BOX_LARGE);
                                rc = 1;
                            }
                        }
                    }

                    fileNameListFree(&fileNameList, 0);
                } else {
                    soundPlayFile("iisxxxx1");

                    // Error reading file list!
                    char* msg = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 615);
                    showDialogBox(msg, nullptr, 0, 169, 126, _colorTable[32328], nullptr, _colorTable[32328], 0);

                    rc = 0;
                }
            }

            windowRefresh(win);

            renderPresent();
            sharedFpsLimiter.throttle();
        }

        windowDestroy(win);

        for (index = 0; index < 5; index++) {
            internal_free(up[index]);
            internal_free(down[index]);
        }

        return 0;
    }

    // Character Editor is not in creation mode - this button is only for
    // printing character details.

    char pattern[512];
    strcpy(pattern, "*.TXT");

    char** fileNames;
    int filesCount = fileNameListInit(pattern, &fileNames, 0, 0);
    if (filesCount == -1) {
        soundPlayFile("iisxxxx1");

        // Error reading file list!
        strcpy(pattern, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 615));
        showDialogBox(pattern, nullptr, 0, 169, 126, _colorTable[32328], nullptr, _colorTable[32328], 0);
        return 0;
    }

    // PRINT
    char fileName[512];
    strcpy(fileName, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 616));

    char title[512];
    strcpy(title, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 602));

    if (showSaveFileDialog(title, fileNames, fileName, filesCount, 168, 80, 0) == 0) {
        strcat(fileName, ".TXT");

        title[0] = '\0';
        strcat(title, fileName);

        int v42 = 0;
        if (characterFileExists(title)) {
            snprintf(title, sizeof(title),
                "%s %s",
                compat_strupr(fileName),
                getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 609));

            char line2[512];
            strcpy(line2, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 610));

            const char* lines[] = { line2 };
            v42 = showDialogBox(title, lines, 1, 169, 126, _colorTable[32328], nullptr, _colorTable[32328], 0x10);
            if (v42) {
                v42 = 1;
            }
        } else {
            v42 = 1;
        }

        if (v42) {
            title[0] = '\0';
            strcpy(title, fileName);

            if (characterPrintToFile(title) != 0) {
                soundPlayFile("iisxxxx1");

                snprintf(title, sizeof(title),
                    "%s%s%s",
                    getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 611),
                    compat_strupr(fileName),
                    "!");
                showDialogBox(title, nullptr, 0, 169, 126, _colorTable[32328], nullptr, _colorTable[32328], 1);
            }
        }
    }

    fileNameListFree(&fileNames, 0);

    return 0;
}

// 0x4390B4
static bool characterFileExists(const char* fname)
{
    File* stream = fileOpen(fname, "rb");
    if (stream == nullptr) {
        return false;
    }

    fileClose(stream);
    return true;
}

// 0x4390D0
static int characterPrintToFile(const char* fileName)
{
    File* stream = fileOpen(fileName, "wt");
    if (stream == nullptr) {
        return -1;
    }

    fileWriteString("\n", stream);
    fileWriteString("\n", stream);

    char title1[256];
    char title2[256];
    char title3[256];
    char padding[256];

    // FALLOUT
    strcpy(title1, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 620));

    // NOTE: Uninline.
    padding[0] = '\0';
    _AddSpaces(padding, (80 - static_cast<int>(strlen(title1))) / 2 - 2);

    strcat(padding, title1);
    strcat(padding, "\n");
    fileWriteString(padding, stream);

    // VAULT-13 PERSONNEL RECORD
    strcpy(title1, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 621));

    // NOTE: Uninline.
    padding[0] = '\0';
    _AddSpaces(padding, (80 - static_cast<int>(strlen(title1))) / 2 - 2);

    strcat(padding, title1);
    strcat(padding, "\n");
    fileWriteString(padding, stream);

    int month;
    int day;
    int year;
    gameTimeGetDate(&month, &day, &year);

    snprintf(title1, sizeof(title1), "%.2d %s %d  %.4d %s",
        day,
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 500 + month - 1),
        year,
        gameTimeGetHour(),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 622));

    // NOTE: Uninline.
    padding[0] = '\0';
    _AddSpaces(padding, (80 - static_cast<int>(strlen(title1))) / 2 - 2);

    strcat(padding, title1);
    strcat(padding, "\n");
    fileWriteString(padding, stream);

    // Blank line
    fileWriteString("\n", stream);

    // Name
    snprintf(title1, sizeof(title1),
        "%s %s",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 642),
        critterGetName(gDude));

    int paddingLength = 27 - static_cast<int>(strlen(title1));
    if (paddingLength > 0) {
        // NOTE: Uninline.
        padding[0] = '\0';
        _AddSpaces(padding, paddingLength);

        strcat(title1, padding);
    }

    // Age
    snprintf(title2, sizeof(title2),
        "%s%s %d",
        title1,
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 643),
        critterGetStat(gDude, STAT_AGE));

    // Gender
    snprintf(title3, sizeof(title3),
        "%s%s %s",
        title2,
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 644),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 645 + critterGetStat(gDude, STAT_GENDER)));

    fileWriteString(title3, stream);
    fileWriteString("\n", stream);

    snprintf(title1, sizeof(title1),
        "%s %.2d %s %s ",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 647),
        pcGetStat(PC_STAT_LEVEL),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 648),
        _itostndn(pcGetStat(PC_STAT_EXPERIENCE), title3));

    paddingLength = 12 - static_cast<int>(strlen(title3));
    if (paddingLength > 0) {
        // NOTE: Uninline.
        padding[0] = '\0';
        _AddSpaces(padding, paddingLength);

        strcat(title1, padding);
    }

    snprintf(title2, sizeof(title2),
        "%s%s %s",
        title1,
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 649),
        _itostndn(pcGetExperienceForNextLevel(), title3));
    fileWriteString(title2, stream);
    fileWriteString("\n", stream);
    fileWriteString("\n", stream);

    // Statistics
    snprintf(title1, sizeof(title1), "%s\n", getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 623));

    // Strength / Hit Points / Sequence
    //
    // FIXME: There is bug - it shows strength instead of sequence.
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.3d/%.3d %s %.2d",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 624),
        critterGetStat(gDude, STAT_STRENGTH),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 625),
        critterGetHitPoints(gDude),
        critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 626),
        critterGetStat(gDude, STAT_STRENGTH));
    fileWriteString(title1, stream);
    fileWriteString("\n", stream);

    // Perception / Armor Class / Healing Rate
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.3d %s %.2d",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 627),
        critterGetStat(gDude, STAT_PERCEPTION),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 628),
        critterGetStat(gDude, STAT_ARMOR_CLASS),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 629),
        critterGetStat(gDude, STAT_HEALING_RATE));
    fileWriteString(title1, stream);
    fileWriteString("\n", stream);

    // Endurance / Action Points / Critical Chance
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.2d %s %.3d%%",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 630),
        critterGetStat(gDude, STAT_ENDURANCE),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 631),
        critterGetStat(gDude, STAT_MAXIMUM_ACTION_POINTS),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 632),
        critterGetStat(gDude, STAT_CRITICAL_CHANCE));
    fileWriteString(title1, stream);
    fileWriteString("\n", stream);

    // SFALL: Display melee damage without "Bonus HtH Damage" bonus.
    int meleeDamage = critterGetStat(gDude, STAT_MELEE_DAMAGE);
    if (!damageModGetDisplayBonusDamage()) {
        meleeDamage -= 2 * perkGetRank(gDude, PERK_BONUS_HTH_DAMAGE);
    }

    // Charisma / Melee Damage / Carry Weight
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.2d %s %.3d lbs.",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 633),
        critterGetStat(gDude, STAT_CHARISMA),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 634),
        meleeDamage,
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 635),
        critterGetStat(gDude, STAT_CARRY_WEIGHT));
    fileWriteString(title1, stream);
    fileWriteString("\n", stream);

    // Intelligence / Damage Resistance
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.3d%%",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 636),
        critterGetStat(gDude, STAT_INTELLIGENCE),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 637),
        critterGetStat(gDude, STAT_DAMAGE_RESISTANCE));
    fileWriteString(title1, stream);
    fileWriteString("\n", stream);

    // Agility / Radiation Resistance
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.3d%%",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 638),
        critterGetStat(gDude, STAT_AGILITY),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 639),
        critterGetStat(gDude, STAT_RADIATION_RESISTANCE));
    fileWriteString(title1, stream);
    fileWriteString("\n", stream);

    // Luck / Poison Resistance
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.3d%%",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 640),
        critterGetStat(gDude, STAT_LUCK),
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 641),
        critterGetStat(gDude, STAT_POISON_RESISTANCE));
    fileWriteString(title1, stream);
    fileWriteString("\n", stream);

    fileWriteString("\n", stream);
    fileWriteString("\n", stream);

    if (gCharacterEditorTempTraits[0] != -1) {
        // ::: Traits :::
        snprintf(title1, sizeof(title1), "%s\n", getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 650));
        fileWriteString(title1, stream);

        // NOTE: The original code does not use loop, or it was optimized away.
        for (int index = 0; index < TRAITS_MAX_SELECTED_COUNT; index++) {
            if (gCharacterEditorTempTraits[index] != -1) {
                snprintf(title1, sizeof(title1), "  %s", traitGetName(gCharacterEditorTempTraits[index]));
                fileWriteString(title1, stream);
                fileWriteString("\n", stream);
            }
        }
    }

    int perk = 0;
    for (; perk < PERK_COUNT; perk++) {
        if (perkGetRank(gDude, perk) != 0) {
            break;
        }
    }

    if (perk < PERK_COUNT) {
        // ::: Perks :::
        snprintf(title1, sizeof(title1), "%s\n", getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 651));
        fileWriteString(title1, stream);

        for (perk = 0; perk < PERK_COUNT; perk++) {
            int rank = perkGetRank(gDude, perk);
            if (rank != 0) {
                if (rank == 1) {
                    snprintf(title1, sizeof(title1), "  %s", perkGetName(perk));
                } else {
                    snprintf(title1, sizeof(title1), "  %s (%d)", perkGetName(perk), rank);
                }

                fileWriteString(title1, stream);
                fileWriteString("\n", stream);
            }
        }
    }

    fileWriteString("\n", stream);

    // ::: Karma :::
    snprintf(title1, sizeof(title1), "%s\n", getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 652));
    fileWriteString(title1, stream);

    for (int index = 0; index < gKarmaEntriesLength; index++) {
        KarmaEntry* karmaEntry = &(gKarmaEntries[index]);
        if (karmaEntry->gvar == GVAR_PLAYER_REPUTATION) {
            int reputation = 0;
            for (; reputation < gGenericReputationEntriesLength; reputation++) {
                GenericReputationEntry* reputationDescription = &(gGenericReputationEntries[reputation]);
                if (gGameGlobalVars[GVAR_PLAYER_REPUTATION] >= reputationDescription->threshold) {
                    break;
                }
            }

            if (reputation < gGenericReputationEntriesLength) {
                GenericReputationEntry* reputationDescription = &(gGenericReputationEntries[reputation]);
                snprintf(title1, sizeof(title1),
                    "  %s: %s (%s)",
                    getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 125),
                    compat_itoa(gGameGlobalVars[GVAR_PLAYER_REPUTATION], title2, 10),
                    getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, reputationDescription->name));
                fileWriteString(title1, stream);
                fileWriteString("\n", stream);
            }
        } else {
            if (gGameGlobalVars[karmaEntry->gvar] != 0) {
                snprintf(title1, sizeof(title1), "  %s", getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, karmaEntry->name));
                fileWriteString(title1, stream);
                fileWriteString("\n", stream);
            }
        }
    }

    bool hasTownReputationHeading = false;
    // SFALL
    for (int index = 0; index < gCustomTownReputationEntries.size(); index++) {
        const TownReputationEntry* pair = &(gCustomTownReputationEntries[index]);
        if (wmAreaIsKnown(pair->city)) {
            if (!hasTownReputationHeading) {
                fileWriteString("\n", stream);

                // ::: Reputation :::
                snprintf(title1, sizeof(title1), "%s\n", getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 657));
                fileWriteString(title1, stream);
                hasTownReputationHeading = true;
            }

            wmGetAreaIdxName(pair->city, title2);

            int townReputation = gGameGlobalVars[pair->gvar];

            int townReputationMessageId;

            if (townReputation < -30) {
                townReputationMessageId = 2006; // Vilified
            } else if (townReputation < -15) {
                townReputationMessageId = 2005; // Hated
            } else if (townReputation < 0) {
                townReputationMessageId = 2004; // Antipathy
            } else if (townReputation == 0) {
                townReputationMessageId = 2003; // Neutral
            } else if (townReputation < 15) {
                townReputationMessageId = 2002; // Accepted
            } else if (townReputation < 30) {
                townReputationMessageId = 2001; // Liked
            } else {
                townReputationMessageId = 2000; // Idolized
            }

            snprintf(title1, sizeof(title1),
                "  %s: %s",
                title2,
                getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, townReputationMessageId));
            fileWriteString(title1, stream);
            fileWriteString("\n", stream);
        }
    }

    bool hasAddictionsHeading = false;
    for (int index = 0; index < ADDICTION_REPUTATION_COUNT; index++) {
        if (gGameGlobalVars[gAddictionReputationVars[index]] != 0) {
            if (!hasAddictionsHeading) {
                fileWriteString("\n", stream);

                // ::: Addictions :::
                snprintf(title1, sizeof(title1), "%s\n", getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 656));
                fileWriteString(title1, stream);
                hasAddictionsHeading = true;
            }

            snprintf(title1, sizeof(title1),
                "  %s",
                getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 1004 + index));
            fileWriteString(title1, stream);
            fileWriteString("\n", stream);
        }
    }

    fileWriteString("\n", stream);

    // ::: Skills ::: / ::: Kills :::
    snprintf(title1, sizeof(title1), "%s\n", getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 653));
    fileWriteString(title1, stream);

    int killType = 0;
    for (int skill = 0; skill < SKILL_COUNT; skill++) {
        snprintf(title1, sizeof(title1), "%s ", skillGetName(skill));

        // NOTE: Uninline.
        _AddDots(title1 + strlen(title1), 16 - static_cast<int>(strlen(title1)));

        bool hasKillType = false;

        for (; killType < KILL_TYPE_COUNT; killType++) {
            int killsCount = killsGetByType(killType);
            if (killsCount > 0) {
                snprintf(title2, sizeof(title2), "%s ", killTypeGetName(killType));

                // NOTE: Uninline.
                _AddDots(title2 + strlen(title2), 16 - static_cast<int>(strlen(title2)));

                snprintf(title3, sizeof(title3),
                    "  %s %.3d%%        %s %.3d\n",
                    title1,
                    skillGetValue(gDude, skill),
                    title2,
                    killsCount);
                hasKillType = true;
                break;
            }
        }

        if (!hasKillType) {
            snprintf(title3, sizeof(title3),
                "  %s %.3d%%\n",
                title1,
                skillGetValue(gDude, skill));
        }
    }

    fileWriteString("\n", stream);
    fileWriteString("\n", stream);

    // ::: Inventory :::
    snprintf(title1, sizeof(title1), "%s\n", getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 654));
    fileWriteString(title1, stream);

    Inventory* inventory = &(gDude->data.inventory);
    for (int index = 0; index < inventory->length; index += 3) {
        title1[0] = '\0';

        for (int column = 0; column < 3; column++) {
            int inventoryItemIndex = index + column;
            if (inventoryItemIndex >= inventory->length) {
                break;
            }

            InventoryItem* inventoryItem = &(inventory->items[inventoryItemIndex]);

            snprintf(title2, sizeof(title2),
                "  %sx %s",
                _itostndn(inventoryItem->quantity, title3),
                objectGetName(inventoryItem->item));

            int length = 25 - static_cast<int>(strlen(title2));
            if (length < 0) {
                length = 0;
            }

            _AddSpaces(title2, length);

            strcat(title1, title2);
        }

        strcat(title1, "\n");
        fileWriteString(title1, stream);
    }

    fileWriteString("\n", stream);

    // Total Weight:
    snprintf(title1, sizeof(title1),
        "%s %d lbs.",
        getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 655),
        objectGetInventoryWeight(gDude));
    fileWriteString(title1, stream);

    fileWriteString("\n", stream);
    fileWriteString("\n", stream);
    fileWriteString("\n", stream);
    fileClose(stream);

    return 0;
}

// 0x43A55C
static char* _AddSpaces(char* string, int length)
{
    char* pch = string + strlen(string);

    for (int index = 0; index < length; index++) {
        *pch++ = ' ';
    }

    *pch = '\0';

    return string;
}

// NOTE: Inlined.
//
// 0x43A58C
static char* _AddDots(char* string, int length)
{
    char* pch = string + strlen(string);

    for (int index = 0; index < length; index++) {
        *pch++ = '.';
    }

    *pch = '\0';

    return string;
}

// 0x43A4BC
static void characterEditorResetScreen()
{
    characterEditorSelectedItem = 0;
    gCharacterEditorCurrentSkill = 0;
    gCharacterEditorSkillValueAdjustmentSliderY = 27;
    characterEditorWindowSelectedFolder = 0;

    if (gCharacterEditorIsCreationMode) {
        characterEditorDrawBigNumber(126, 282, 0, gCharacterEditorRemainingCharacterPoints, 0, gCharacterEditorWindow);
    } else {
        characterEditorDrawFolders();
        characterEditorDrawPcStats();
    }

    characterEditorDrawName();
    characterEditorDrawAge();
    characterEditorDrawGender();
    characterEditorDrawOptionalTraits();
    characterEditorDrawSkills(0);
    characterEditorDrawPrimaryStat(7, 0, 0);
    characterEditorDrawDerivedStats();
    characterEditorDrawCard();
    windowRefresh(gCharacterEditorWindow);
}

// 0x43A5BC
static void characterEditorRegisterInfoAreas()
{
    buttonCreate(gCharacterEditorWindow, 19, 38, 125, 227, -1, -1, 525, -1, nullptr, nullptr, nullptr, 0);
    buttonCreate(gCharacterEditorWindow, 28, 280, 124, 32, -1, -1, 526, -1, nullptr, nullptr, nullptr, 0);

    if (gCharacterEditorIsCreationMode) {
        buttonCreate(gCharacterEditorWindow, 52, 324, 169, 20, -1, -1, 533, -1, nullptr, nullptr, nullptr, 0);
        buttonCreate(gCharacterEditorWindow, 47, 353, 245, 100, -1, -1, 534, -1, nullptr, nullptr, nullptr, 0);
    } else {
        buttonCreate(gCharacterEditorWindow, 28, 363, 283, 105, -1, -1, 527, -1, nullptr, nullptr, nullptr, 0);
    }

    buttonCreate(gCharacterEditorWindow, 191, 41, 122, 110, -1, -1, 528, -1, nullptr, nullptr, nullptr, 0);
    buttonCreate(gCharacterEditorWindow, 191, 175, 122, 135, -1, -1, 529, -1, nullptr, nullptr, nullptr, 0);
    buttonCreate(gCharacterEditorWindow, 376, 5, 223, 20, -1, -1, 530, -1, nullptr, nullptr, nullptr, 0);
    buttonCreate(gCharacterEditorWindow, 370, 27, 223, 195, -1, -1, 531, -1, nullptr, nullptr, nullptr, 0);
    buttonCreate(gCharacterEditorWindow, 396, 228, 171, 25, -1, -1, 532, -1, nullptr, nullptr, nullptr, 0);
}

// copy character to editor
//
// 0x43A7DC
static void characterEditorSavePlayer()
{
    Proto* proto;
    protoGetProto(gDude->pid, &proto);
    critterProtoDataCopy(&gCharacterEditorDudeDataBackup, &(proto->critter.data));

    gCharacterEditorHitPointsBackup = critterGetHitPoints(gDude);

    strncpy(gCharacterEditorNameBackup, critterGetName(gDude), 32);

    gCharacterEditorLastLevelBackup = gCharacterEditorLastLevel;
    for (int perk = 0; perk < PERK_COUNT; perk++) {
        gCharacterEditorPerksBackup[perk] = perkGetRank(gDude, perk);
    }

    gCharacterEditorHasFreePerkBackup = gCharacterEditorHasFreePerk;

    gCharacterEditorUnspentSkillPointsBackup = pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS);

    skillsGetTagged(gCharacterEditorTaggedSkillsBackup, NUM_TAGGED_SKILLS);

    traitsGetSelected(&(gCharacterEditorOptionalTraitsBackup[0]), &(gCharacterEditorOptionalTraitsBackup[1]));

    for (int skill = 0; skill < SKILL_COUNT; skill++) {
        gCharacterEditorSkillsBackup[skill] = skillGetValue(gDude, skill);
    }
}

// copy editor to character
//
// 0x43A8BC
static void characterEditorRestorePlayer()
{
    Proto* proto;
    int i;
    int v3;
    int cur_hp;

    _pop_perks();

    protoGetProto(gDude->pid, &proto);
    critterProtoDataCopy(&(proto->critter.data), &gCharacterEditorDudeDataBackup);

    dudeSetName(gCharacterEditorNameBackup);

    gCharacterEditorLastLevel = gCharacterEditorLastLevelBackup;
    gCharacterEditorHasFreePerk = gCharacterEditorHasFreePerkBackup;

    pcSetStat(PC_STAT_UNSPENT_SKILL_POINTS, gCharacterEditorUnspentSkillPointsBackup);

    skillsSetTagged(gCharacterEditorTaggedSkillsBackup, NUM_TAGGED_SKILLS);

    traitsSetSelected(gCharacterEditorOptionalTraitsBackup[0], gCharacterEditorOptionalTraitsBackup[1]);

    skillsGetTagged(gCharacterEditorTempTaggedSkills, NUM_TAGGED_SKILLS);

    i = 4;
    v3 = 0;
    for (v3 = 0; v3 < 4; v3++) {
        if (gCharacterEditorTempTaggedSkills[--i] != -1) {
            break;
        }
    }

    if (gCharacterEditorIsCreationMode) {
        v3 -= gCharacterEditorIsCreationMode;
    }

    gCharacterEditorTaggedSkillCount = v3;

    traitsGetSelected(&(gCharacterEditorTempTraits[0]), &(gCharacterEditorTempTraits[1]));

    i = 2;
    v3 = 0;
    for (v3 = 0; v3 < 2; v3++) {
        if (gCharacterEditorTempTraits[v3] != -1) {
            break;
        }
    }

    gCharacterEditorTempTraitCount = v3;

    critterUpdateDerivedStats(gDude);

    cur_hp = critterGetHitPoints(gDude);
    critterAdjustHitPoints(gDude, gCharacterEditorHitPointsBackup - cur_hp);
}

// 0x43A9CC
static char* _itostndn(int value, char* dest)
{
    int v16[7];
    memcpy(v16, dword_431DD4, sizeof(v16));

    char* savedDest = dest;

    if (value != 0) {
        *dest = '\0';

        bool v3 = false;
        for (int index = 0; index < 7; index++) {
            int v18 = value / v16[index];
            if (v18 > 0 || v3) {
                char temp[64]; // TODO: Size is probably wrong.
                compat_itoa(v18, temp, 10);
                strcat(dest, temp);

                v3 = true;

                value -= v16[index] * v18;

                if (index == 0 || index == 3) {
                    strcat(dest, ",");
                }
            }
        }
    } else {
        strcpy(dest, "0");
    }

    return savedDest;
}

// 0x43AAEC
static int characterEditorDrawCardWithOptions(int graphicId, const char* name, const char* attributes, char* description)
{
    FrmImage frmImage;
    int fid = buildFid(OBJ_TYPE_SKILLDEX, graphicId, 0, 0, 0);
    if (!frmImage.lock(fid)) {
        return -1;
    }

    blitBufferToBuffer(frmImage.getData(),
        frmImage.getWidth(),
        frmImage.getHeight(),
        frmImage.getWidth(),
        gCharacterEditorWindowBuffer + 640 * 309 + 484,
        640);

    int extraDescriptionWidth = 150;
    unsigned char* data = frmImage.getData();
    for (int y = 0; y < frmImage.getHeight(); y++) {
        for (int x = 0; x < frmImage.getWidth(); x++) {
            if (HighRGB(*data) < 2) {
                extraDescriptionWidth = std::min(extraDescriptionWidth, x);
            }
            data++;
        }
    }

    extraDescriptionWidth -= 8;
    if (extraDescriptionWidth < 0) {
        extraDescriptionWidth = 0;
    }

    fontSetCurrent(102);

    fontDrawText(gCharacterEditorWindowBuffer + 640 * 272 + 348, name, 640, 640, _colorTable[0]);
    int nameFontLineHeight = fontGetLineHeight();
    if (attributes != nullptr) {
        int nameWidth = fontGetStringWidth(name);

        fontSetCurrent(101);
        int attributesFontLineHeight = fontGetLineHeight();
        fontDrawText(gCharacterEditorWindowBuffer + 640 * (268 + nameFontLineHeight - attributesFontLineHeight) + 348 + nameWidth + 8, attributes, 640, 640, _colorTable[0]);
    }

    windowDrawLine(gCharacterEditorWindow, 348, nameFontLineHeight + 272, 613, nameFontLineHeight + 272, _colorTable[0]);
    windowDrawLine(gCharacterEditorWindow, 348, nameFontLineHeight + 273, 613, nameFontLineHeight + 273, _colorTable[0]);

    fontSetCurrent(101);

    int descriptionFontLineHeight = fontGetLineHeight();

    short beginnings[WORD_WRAP_MAX_COUNT];
    short beginningsCount;
    if (wordWrap(description, extraDescriptionWidth + 136, beginnings, &beginningsCount) != 0) {
        // TODO: Leaking graphic handle.
        return -1;
    }

    int y = 315;
    for (short i = 0; i < beginningsCount - 1; i++) {
        short beginning = beginnings[i];
        short ending = beginnings[i + 1];
        char c = description[ending];
        description[ending] = '\0';
        fontDrawText(gCharacterEditorWindowBuffer + 640 * y + 348, description + beginning, 640, 640, _colorTable[0]);
        description[ending] = c;
        y += descriptionFontLineHeight;
    }

    if (graphicId != gCharacterEditorCardFrmId || strcmp(name, gCharacterEditorCardTitle) != 0) {
        if (gCharacterEditorCardDrawn) {
            soundPlayFile("isdxxxx1");
        }
    }

    strcpy(gCharacterEditorCardTitle, name);
    gCharacterEditorCardFrmId = graphicId;
    gCharacterEditorCardDrawn = true;

    return 0;
}

// 0x43AE8
static void characterEditorHandleFolderButtonPressed()
{
    mouseGetPositionInWindow(gCharacterEditorWindow, &gCharacterEditorMouseX, &gCharacterEditorMouseY);
    soundPlayFile("ib3p1xx1");

    if (gCharacterEditorMouseX >= 208) {
        characterEditorSelectedItem = 41;
        characterEditorWindowSelectedFolder = EDITOR_FOLDER_KILLS;
    } else if (gCharacterEditorMouseX > 110) {
        characterEditorSelectedItem = 42;
        characterEditorWindowSelectedFolder = EDITOR_FOLDER_KARMA;
    } else {
        characterEditorSelectedItem = 40;
        characterEditorWindowSelectedFolder = EDITOR_FOLDER_PERKS;
    }

    characterEditorDrawFolders();
    characterEditorDrawCard();
}

// 0x43AF40
static void characterEditorHandleInfoButtonPressed(int eventCode)
{
    mouseGetPositionInWindow(gCharacterEditorWindow, &gCharacterEditorMouseX, &gCharacterEditorMouseY);

    switch (eventCode) {
    case 525:
        if (1) {
            // TODO: Original code is slightly different.
            double mouseY = gCharacterEditorMouseY;
            for (int index = 0; index < 7; index++) {
                double buttonTop = gCharacterEditorPrimaryStatY[index];
                double buttonBottom = gCharacterEditorPrimaryStatY[index] + 22;
                double allowance = 5.0 - index * 0.25;
                if (mouseY >= buttonTop - allowance && mouseY <= buttonBottom + allowance) {
                    characterEditorSelectedItem = index;
                    break;
                }
            }
        }
        break;
    case 526:
        if (gCharacterEditorIsCreationMode) {
            characterEditorSelectedItem = 7;
        } else {
            int offset = gCharacterEditorMouseY - 280;
            if (offset < 0) {
                offset = 0;
            }

            characterEditorSelectedItem = offset / 10 + 7;
        }
        break;
    case 527:
        if (!gCharacterEditorIsCreationMode) {
            fontSetCurrent(101);
            int offset = gCharacterEditorMouseY - 364;
            if (offset < 0) {
                offset = 0;
            }
            characterEditorSelectedItem = offset / (fontGetLineHeight() + 1) + 10;
        }
        break;
    case 528:
        if (1) {
            int offset = gCharacterEditorMouseY - 41;
            if (offset < 0) {
                offset = 0;
            }

            characterEditorSelectedItem = offset / 13 + 43;
        }
        break;
    case 529: {
        int offset = gCharacterEditorMouseY - 175;
        if (offset < 0) {
            offset = 0;
        }

        characterEditorSelectedItem = offset / 13 + 51;
        break;
    }
    case 530:
        characterEditorSelectedItem = 80;
        break;
    case 531:
        if (1) {
            int offset = gCharacterEditorMouseY - 27;
            if (offset < 0) {
                offset = 0;
            }

            gCharacterEditorCurrentSkill = (int)(offset * 0.092307694);
            if (gCharacterEditorCurrentSkill >= 18) {
                gCharacterEditorCurrentSkill = 17;
            }

            characterEditorSelectedItem = gCharacterEditorCurrentSkill + 61;
        }
        break;
    case 532:
        characterEditorSelectedItem = 79;
        break;
    case 533:
        characterEditorSelectedItem = 81;
        break;
    case 534:
        if (1) {
            fontSetCurrent(101);

            // TODO: Original code is slightly different.
            double mouseY = gCharacterEditorMouseY;
            double fontLineHeight = fontGetLineHeight();
            double y = 353.0;
            double step = fontGetLineHeight() + 3 + 0.56;
            int index;
            for (index = 0; index < 8; index++) {
                if (mouseY >= y - 4.0 && mouseY <= y + fontLineHeight) {
                    break;
                }
                y += step;
            }

            if (index == 8) {
                index = 7;
            }

            characterEditorSelectedItem = index + 82;
            if (gCharacterEditorMouseX >= 169) {
                characterEditorSelectedItem += 8;
            }
        }
        break;
    }

    characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
    characterEditorDrawOptionalTraits();
    characterEditorDrawSkills(0);
    characterEditorDrawPcStats();
    characterEditorDrawFolders();
    characterEditorDrawDerivedStats();
    characterEditorDrawCard();
}

// 0x43B230
static void characterEditorHandleAdjustSkillButtonPressed(int keyCode)
{
    if (gCharacterEditorIsCreationMode) {
        return;
    }

    int unspentSp = pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS);
    _repFtime = 4;

    bool isUsingKeyboard = false;
    int rc = 0;

    switch (keyCode) {
    case KEY_PLUS:
    case KEY_UPPERCASE_N:
    case KEY_ARROW_RIGHT:
        isUsingKeyboard = true;
        keyCode = 521;
        break;
    case KEY_MINUS:
    case KEY_UPPERCASE_J:
    case KEY_ARROW_LEFT:
        isUsingKeyboard = true;
        keyCode = 523;
        break;
    }

    char title[64];
    char body1[64];
    char body2[64];

    const char* body[] = {
        body1,
        body2,
    };

    int repeatDelay = 0;
    for (;;) {
        sharedFpsLimiter.mark();

        _frame_time = getTicks();
        if (repeatDelay <= dbl_5018F0) {
            repeatDelay++;
        }

        if (repeatDelay == 1 || repeatDelay > dbl_5018F0) {
            if (repeatDelay > dbl_5018F0) {
                _repFtime++;
                if (_repFtime > 24) {
                    _repFtime = 24;
                }
            }

            rc = 1;
            if (keyCode == 521) {
                if (pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS) > 0) {
                    if (skillAdd(gDude, gCharacterEditorCurrentSkill) == -3) {
                        soundPlayFile("iisxxxx1");

                        snprintf(title, sizeof(title), "%s:", skillGetName(gCharacterEditorCurrentSkill));
                        // At maximum level.
                        strcpy(body1, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 132));
                        // Unable to increment it.
                        strcpy(body2, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 133));
                        showDialogBox(title, body, 2, 192, 126, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);
                        rc = -1;
                    }
                } else {
                    soundPlayFile("iisxxxx1");

                    // Not enough skill points available.
                    strcpy(title, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 136));
                    showDialogBox(title, nullptr, 0, 192, 126, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);
                    rc = -1;
                }
            } else if (keyCode == 523) {
                if (skillGetValue(gDude, gCharacterEditorCurrentSkill) <= gCharacterEditorSkillsBackup[gCharacterEditorCurrentSkill]) {
                    rc = 0;
                } else {
                    if (skillSub(gDude, gCharacterEditorCurrentSkill) == -2) {
                        rc = 0;
                    }
                }

                if (rc == 0) {
                    soundPlayFile("iisxxxx1");

                    snprintf(title, sizeof(title), "%s:", skillGetName(gCharacterEditorCurrentSkill));
                    // At minimum level.
                    strcpy(body1, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 134));
                    // Unable to decrement it.
                    strcpy(body2, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 135));
                    showDialogBox(title, body, 2, 192, 126, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);
                    rc = -1;
                }
            }

            characterEditorSelectedItem = gCharacterEditorCurrentSkill + 61;
            characterEditorDrawCard();
            characterEditorDrawSkills(1);

            int flags;
            if (rc == 1) {
                flags = ANIMATE;
            } else {
                flags = 0;
            }

            characterEditorDrawBigNumber(522, 228, flags, pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS), unspentSp, gCharacterEditorWindow);

            windowRefresh(gCharacterEditorWindow);
        }

        if (!isUsingKeyboard) {
            unspentSp = pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS);
            if (repeatDelay >= dbl_5018F0) {
                delay_ms(1000 / _repFtime - (getTicks() - _frame_time));
            } else {
                delay_ms(1000 / 24 - (getTicks() - _frame_time));
            }

            int keyCode = inputGetInput();
            if (keyCode != 522 && keyCode != 524 && rc != -1) {
                renderPresent();
                sharedFpsLimiter.throttle();
                continue;
            }
        }
        return;
    }
}

// 0x43B67C
static void characterEditorToggleTaggedSkill(int skill)
{
    int insertionIndex;

    insertionIndex = 0;
    for (int index = 3; index >= 0; index--) {
        if (gCharacterEditorTempTaggedSkills[index] != -1) {
            break;
        }
        insertionIndex++;
    }

    if (gCharacterEditorIsCreationMode) {
        insertionIndex -= 1;
    }

    gCharacterEditorOldTaggedSkillCount = insertionIndex;

    if (skill == gCharacterEditorTempTaggedSkills[0] || skill == gCharacterEditorTempTaggedSkills[1] || skill == gCharacterEditorTempTaggedSkills[2] || skill == gCharacterEditorTempTaggedSkills[3]) {
        if (skill == gCharacterEditorTempTaggedSkills[0]) {
            gCharacterEditorTempTaggedSkills[0] = gCharacterEditorTempTaggedSkills[1];
            gCharacterEditorTempTaggedSkills[1] = gCharacterEditorTempTaggedSkills[2];
            gCharacterEditorTempTaggedSkills[2] = -1;
        } else if (skill == gCharacterEditorTempTaggedSkills[1]) {
            gCharacterEditorTempTaggedSkills[1] = gCharacterEditorTempTaggedSkills[2];
            gCharacterEditorTempTaggedSkills[2] = -1;
        } else {
            gCharacterEditorTempTaggedSkills[2] = -1;
        }
    } else {
        if (gCharacterEditorTaggedSkillCount > 0) {
            insertionIndex = 0;
            for (int index = 0; index < 3; index++) {
                if (gCharacterEditorTempTaggedSkills[index] == -1) {
                    break;
                }
                insertionIndex++;
            }
            gCharacterEditorTempTaggedSkills[insertionIndex] = skill;
        } else {
            soundPlayFile("iisxxxx1");

            char line1[128];
            strcpy(line1, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 140));

            char line2[128];
            strcpy(line2, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 141));

            const char* lines[] = { line2 };
            showDialogBox(line1, lines, 1, 192, 126, _colorTable[32328], nullptr, _colorTable[32328], 0);
        }
    }

    insertionIndex = 0;
    for (int index = 3; index >= 0; index--) {
        if (gCharacterEditorTempTaggedSkills[index] != -1) {
            break;
        }
        insertionIndex++;
    }

    if (gCharacterEditorIsCreationMode) {
        insertionIndex -= 1;
    }

    gCharacterEditorTaggedSkillCount = insertionIndex;

    characterEditorSelectedItem = skill + 61;
    characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
    characterEditorDrawDerivedStats();
    characterEditorDrawSkills(2);
    characterEditorDrawCard();
    windowRefresh(gCharacterEditorWindow);
}

// 0x43B8A8
static void characterEditorDrawOptionalTraits()
{
    int v0 = -1;
    int i;
    int color;
    const char* traitName;
    double step;
    double y;

    if (!gCharacterEditorIsCreationMode) {
        return;
    }

    if (characterEditorSelectedItem >= 82 && characterEditorSelectedItem < 98) {
        v0 = characterEditorSelectedItem - 82;
    }

    blitBufferToBuffer(_editorBackgroundFrmImage.getData() + 640 * 353 + 47, 245, 100, 640, gCharacterEditorWindowBuffer + 640 * 353 + 47, 640);

    fontSetCurrent(101);

    traitsSetSelected(gCharacterEditorTempTraits[0], gCharacterEditorTempTraits[1]);

    step = fontGetLineHeight() + 3 + 0.56;
    y = 353;
    for (i = 0; i < 8; i++) {
        if (i == v0) {
            if (i != gCharacterEditorTempTraits[0] && i != gCharacterEditorTempTraits[1]) {
                color = _colorTable[32747];
            } else {
                color = _colorTable[32767];
            }

            gCharacterEditorFolderCardFrmId = traitGetFrmId(i);
            gCharacterEditorFolderCardTitle = traitGetName(i);
            gCharacterEditorFolderCardSubtitle = nullptr;
            gCharacterEditorFolderCardDescription = traitGetDescription(i);
        } else {
            if (i != gCharacterEditorTempTraits[0] && i != gCharacterEditorTempTraits[1]) {
                color = _colorTable[992];
            } else {
                color = _colorTable[21140];
            }
        }

        traitName = traitGetName(i);
        fontDrawText(gCharacterEditorWindowBuffer + 640 * (int)y + 47, traitName, 640, 640, color);
        y += step;
    }

    y = 353;
    for (i = 8; i < 16; i++) {
        if (i == v0) {
            if (i != gCharacterEditorTempTraits[0] && i != gCharacterEditorTempTraits[1]) {
                color = _colorTable[32747];
            } else {
                color = _colorTable[32767];
            }

            gCharacterEditorFolderCardFrmId = traitGetFrmId(i);
            gCharacterEditorFolderCardTitle = traitGetName(i);
            gCharacterEditorFolderCardSubtitle = nullptr;
            gCharacterEditorFolderCardDescription = traitGetDescription(i);
        } else {
            if (i != gCharacterEditorTempTraits[0] && i != gCharacterEditorTempTraits[1]) {
                color = _colorTable[992];
            } else {
                color = _colorTable[21140];
            }
        }

        traitName = traitGetName(i);
        fontDrawText(gCharacterEditorWindowBuffer + 640 * (int)y + 199, traitName, 640, 640, color);
        y += step;
    }
}

// 0x43BB0C
static void characterEditorToggleOptionalTrait(int trait)
{
    if (trait == gCharacterEditorTempTraits[0] || trait == gCharacterEditorTempTraits[1]) {
        if (trait == gCharacterEditorTempTraits[0]) {
            gCharacterEditorTempTraits[0] = gCharacterEditorTempTraits[1];
            gCharacterEditorTempTraits[1] = -1;
        } else {
            gCharacterEditorTempTraits[1] = -1;
        }
    } else {
        if (gCharacterEditorTempTraitCount == 0) {
            soundPlayFile("iisxxxx1");

            char line1[128];
            strcpy(line1, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 148));

            char line2[128];
            strcpy(line2, getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 149));

            const char* lines = { line2 };
            showDialogBox(line1, &lines, 1, 192, 126, _colorTable[32328], nullptr, _colorTable[32328], 0);
        } else {
            for (int index = 0; index < 2; index++) {
                if (gCharacterEditorTempTraits[index] == -1) {
                    gCharacterEditorTempTraits[index] = trait;
                    break;
                }
            }
        }
    }

    gCharacterEditorTempTraitCount = 0;
    for (int index = 1; index != 0; index--) {
        if (gCharacterEditorTempTraits[index] != -1) {
            break;
        }
        gCharacterEditorTempTraitCount++;
    }

    characterEditorSelectedItem = trait + EDITOR_FIRST_TRAIT;

    characterEditorDrawOptionalTraits();
    characterEditorDrawSkills(0);
    critterUpdateDerivedStats(gDude);
    characterEditorDrawBigNumber(126, 282, 0, gCharacterEditorRemainingCharacterPoints, 0, gCharacterEditorWindow);
    characterEditorDrawPrimaryStat(RENDER_ALL_STATS, false, 0);
    characterEditorDrawDerivedStats();
    characterEditorDrawCard();
    windowRefresh(gCharacterEditorWindow);
}

// 0x43BCE0
static void characterEditorDrawKarmaFolder()
{
    char* msg;
    char formattedText[256];

    characterEditorFolderViewClear();

    bool hasSelection = false;
    for (int index = 0; index < gKarmaEntriesLength; index++) {
        KarmaEntry* karmaDescription = &(gKarmaEntries[index]);
        if (karmaDescription->gvar == GVAR_PLAYER_REPUTATION) {
            int reputation;
            for (reputation = 0; reputation < gGenericReputationEntriesLength; reputation++) {
                GenericReputationEntry* reputationDescription = &(gGenericReputationEntries[reputation]);
                if (gGameGlobalVars[GVAR_PLAYER_REPUTATION] >= reputationDescription->threshold) {
                    break;
                }
            }

            if (reputation != gGenericReputationEntriesLength) {
                GenericReputationEntry* reputationDescription = &(gGenericReputationEntries[reputation]);

                char reputationValue[32];
                compat_itoa(gGameGlobalVars[GVAR_PLAYER_REPUTATION], reputationValue, 10);

                snprintf(formattedText, sizeof(formattedText),
                    "%s: %s (%s)",
                    getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 125),
                    reputationValue,
                    getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, reputationDescription->name));

                if (characterEditorFolderViewDrawString(formattedText)) {
                    gCharacterEditorFolderCardFrmId = karmaDescription->art_num;
                    gCharacterEditorFolderCardTitle = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 125);
                    gCharacterEditorFolderCardSubtitle = nullptr;
                    gCharacterEditorFolderCardDescription = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, karmaDescription->description);
                    hasSelection = true;
                }
            }
        } else {
            if (gGameGlobalVars[karmaDescription->gvar] != 0) {
                msg = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, karmaDescription->name);
                if (characterEditorFolderViewDrawString(msg)) {
                    gCharacterEditorFolderCardFrmId = karmaDescription->art_num;
                    gCharacterEditorFolderCardTitle = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, karmaDescription->name);
                    gCharacterEditorFolderCardSubtitle = nullptr;
                    gCharacterEditorFolderCardDescription = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, karmaDescription->description);
                    hasSelection = true;
                }
            }
        }
    }

    bool hasTownReputationHeading = false;
    // SFALL
    for (int index = 0; index < gCustomTownReputationEntries.size(); index++) {
        const TownReputationEntry* pair = &(gCustomTownReputationEntries[index]);
        if (wmAreaIsKnown(pair->city)) {
            if (!hasTownReputationHeading) {
                msg = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 4000);
                if (characterEditorFolderViewDrawHeading(msg)) {
                    gCharacterEditorFolderCardFrmId = 48;
                    gCharacterEditorFolderCardTitle = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 4000);
                    gCharacterEditorFolderCardSubtitle = nullptr;
                    gCharacterEditorFolderCardDescription = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 4100);
                }
                hasTownReputationHeading = true;
            }

            char cityShortName[40];
            wmGetAreaIdxName(pair->city, cityShortName);

            int townReputation = gGameGlobalVars[pair->gvar];

            int townReputationGraphicId;
            int townReputationBaseMessageId;

            if (townReputation < -30) {
                townReputationGraphicId = 150;
                townReputationBaseMessageId = 2006; // Vilified
            } else if (townReputation < -15) {
                townReputationGraphicId = 153;
                townReputationBaseMessageId = 2005; // Hated
            } else if (townReputation < 0) {
                townReputationGraphicId = 153;
                townReputationBaseMessageId = 2004; // Antipathy
            } else if (townReputation == 0) {
                townReputationGraphicId = 141;
                townReputationBaseMessageId = 2003; // Neutral
            } else if (townReputation < 15) {
                townReputationGraphicId = 137;
                townReputationBaseMessageId = 2002; // Accepted
            } else if (townReputation < 30) {
                townReputationGraphicId = 137;
                townReputationBaseMessageId = 2001; // Liked
            } else {
                townReputationGraphicId = 135;
                townReputationBaseMessageId = 2000; // Idolized
            }

            msg = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, townReputationBaseMessageId);
            snprintf(formattedText, sizeof(formattedText),
                "%s: %s",
                cityShortName,
                msg);

            if (characterEditorFolderViewDrawString(formattedText)) {
                gCharacterEditorFolderCardFrmId = townReputationGraphicId;
                gCharacterEditorFolderCardTitle = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, townReputationBaseMessageId);
                gCharacterEditorFolderCardSubtitle = nullptr;
                gCharacterEditorFolderCardDescription = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, townReputationBaseMessageId + 100);
                hasSelection = 1;
            }
        }
    }

    bool hasAddictionsHeading = false;
    for (int index = 0; index < ADDICTION_REPUTATION_COUNT; index++) {
        if (gGameGlobalVars[gAddictionReputationVars[index]] != 0) {
            if (!hasAddictionsHeading) {
                // Addictions
                msg = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 4001);
                if (characterEditorFolderViewDrawHeading(msg)) {
                    gCharacterEditorFolderCardFrmId = 53;
                    gCharacterEditorFolderCardTitle = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 4001);
                    gCharacterEditorFolderCardSubtitle = nullptr;
                    gCharacterEditorFolderCardDescription = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 4101);
                    hasSelection = 1;
                }
                hasAddictionsHeading = true;
            }

            msg = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 1004 + index);
            if (characterEditorFolderViewDrawString(msg)) {
                gCharacterEditorFolderCardFrmId = gAddictionReputationFrmIds[index];
                gCharacterEditorFolderCardTitle = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 1004 + index);
                gCharacterEditorFolderCardSubtitle = nullptr;
                gCharacterEditorFolderCardDescription = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 1104 + index);
                hasSelection = 1;
            }
        }
    }

    if (!hasSelection) {
        // SFALL: Custom karma folder.
        gCharacterEditorFolderCardFrmId = customKarmaFolderGetFrmId();
        gCharacterEditorFolderCardTitle = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 125);
        gCharacterEditorFolderCardSubtitle = nullptr;
        gCharacterEditorFolderCardDescription = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 128);
    }
}

// 0x43C1B0
int characterEditorSave(File* stream)
{
    if (fileWriteInt32(stream, gCharacterEditorLastLevel) == -1)
        return -1;
    if (fileWriteUInt8(stream, gCharacterEditorHasFreePerk) == -1)
        return -1;

    return 0;
}

// 0x43C1E0
int characterEditorLoad(File* stream)
{
    if (fileReadInt32(stream, &gCharacterEditorLastLevel) == -1)
        return -1;
    if (fileReadUInt8(stream, &gCharacterEditorHasFreePerk) == -1)
        return -1;

    return 0;
}

// 0x43C20C
void characterEditorReset()
{
    gCharacterEditorRemainingCharacterPoints = 5;
    gCharacterEditorLastLevel = 1;
}

// level up if needed
//
// 0x43C228
static int characterEditorUpdateLevel()
{
    int level = pcGetStat(PC_STAT_LEVEL);
    if (level != gCharacterEditorLastLevel && level <= PC_LEVEL_MAX) {
        for (int nextLevel = gCharacterEditorLastLevel + 1; nextLevel <= level; nextLevel++) {
            int sp = pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS);
            sp += 5;
            sp += critterGetBaseStatWithTraitModifier(gDude, STAT_INTELLIGENCE) * 2;
            sp += perkGetRank(gDude, PERK_EDUCATED) * 2;
            sp += traitIsSelected(TRAIT_SKILLED) * 5;
            if (traitIsSelected(TRAIT_GIFTED)) {
                sp -= 5;
                if (sp < 0) {
                    sp = 0;
                }
            }
            if (sp > 99) {
                sp = 99;
            }

            pcSetStat(PC_STAT_UNSPENT_SKILL_POINTS, sp);

            int selectedPerksCount = 0;
            for (int perk = 0; perk < PERK_COUNT; perk++) {
                if (perkGetRank(gDude, perk) != 0) {
                    selectedPerksCount += 1;
                    if (selectedPerksCount >= 37) {
                        break;
                    }
                }
            }

            if (selectedPerksCount < 37) {
                int progression = 3;
                if (traitIsSelected(TRAIT_SKILLED)) {
                    progression += 1;
                }

                if (nextLevel % progression == 0) {
                    gCharacterEditorHasFreePerk = 1;
                }
            }
        }
    }

    if (gCharacterEditorHasFreePerk != 0) {
        characterEditorWindowSelectedFolder = 0;
        characterEditorDrawFolders();
        windowRefresh(gCharacterEditorWindow);

        int rc = perkDialogShow();
        if (rc == -1) {
            debugPrint("\n *** Error running perks dialog! ***\n");
            return -1;
        } else if (rc == 0) {
            characterEditorDrawFolders();
        } else if (rc == 1) {
            characterEditorDrawFolders();
            gCharacterEditorHasFreePerk = 0;
        }
    }

    gCharacterEditorLastLevel = level;

    return 1;
}

// 0x43C398
static void perkDialogRefreshPerks()
{
    blitBufferToBuffer(
        _perkDialogBackgroundFrmImage.getData() + 280,
        293,
        PERK_WINDOW_HEIGHT,
        PERK_WINDOW_WIDTH,
        gPerkDialogWindowBuffer + 280,
        PERK_WINDOW_WIDTH);

    perkDialogDrawPerks();

    // NOTE: Original code is slightly different, but basically does the same thing.
    int perk = gPerkDialogOptionList[gPerkDialogTopLine + gPerkDialogCurrentLine].value;
    int perkFrmId = perkGetFrmId(perk);
    char* perkName = perkGetName(perk);
    char* perkDescription = perkGetDescription(perk);
    char* perkRank = nullptr;
    char perkRankBuffer[32];

    int rank = perkGetRank(gDude, perk);
    if (rank != 0) {
        snprintf(perkRankBuffer, sizeof(perkRankBuffer), "(%d)", rank);
        perkRank = perkRankBuffer;
    }

    perkDialogDrawCard(perkFrmId, perkName, perkRank, perkDescription);

    windowRefresh(gPerkDialogWindow);
}

// 0x43C4F0
static int perkDialogShow()
{
    gPerkDialogTopLine = 0;
    gPerkDialogCurrentLine = 0;
    gPerkDialogCardFrmId = -1;
    gPerkDialogCardTitle[0] = '\0';
    gPerkDialogCardDrawn = false;

    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 86, 0, 0, 0);
    if (!_perkDialogBackgroundFrmImage.lock(backgroundFid)) {
        debugPrint("\n *** Error running perks dialog window ***\n");
        return -1;
    }

    // Maintain original position in original resolution, otherwise center it.
    int perkWindowX = screenGetWidth() != 640
        ? (screenGetWidth() - PERK_WINDOW_WIDTH) / 2
        : PERK_WINDOW_X;
    int perkWindowY = screenGetHeight() != 480
        ? (screenGetHeight() - PERK_WINDOW_HEIGHT) / 2
        : PERK_WINDOW_Y;
    gPerkDialogWindow = windowCreate(perkWindowX, perkWindowY, PERK_WINDOW_WIDTH, PERK_WINDOW_HEIGHT, 256, WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (gPerkDialogWindow == -1) {
        _perkDialogBackgroundFrmImage.unlock();
        debugPrint("\n *** Error running perks dialog window ***\n");
        return -1;
    }

    gPerkDialogWindowBuffer = windowGetBuffer(gPerkDialogWindow);
    memcpy(gPerkDialogWindowBuffer, _perkDialogBackgroundFrmImage.getData(), PERK_WINDOW_WIDTH * PERK_WINDOW_HEIGHT);

    int btn;

    btn = buttonCreate(gPerkDialogWindow,
        48,
        186,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getHeight(),
        -1,
        -1,
        -1,
        500,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(gPerkDialogWindow,
        153,
        186,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getHeight(),
        -1,
        -1,
        -1,
        502,
        _editorFrmImages[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(gPerkDialogWindow,
        25,
        46,
        _editorFrmImages[EDITOR_GRAPHIC_UP_ARROW_ON].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_UP_ARROW_ON].getHeight(),
        -1,
        574,
        572,
        574,
        _editorFrmImages[EDITOR_GRAPHIC_UP_ARROW_OFF].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_UP_ARROW_ON].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, nullptr);
    }

    btn = buttonCreate(gPerkDialogWindow,
        25,
        47 + _editorFrmImages[EDITOR_GRAPHIC_UP_ARROW_ON].getHeight(),
        _editorFrmImages[EDITOR_GRAPHIC_UP_ARROW_ON].getWidth(),
        _editorFrmImages[EDITOR_GRAPHIC_UP_ARROW_ON].getHeight(),
        -1,
        575,
        573,
        575,
        _editorFrmImages[EDITOR_GRAPHIC_DOWN_ARROW_OFF].getData(),
        _editorFrmImages[EDITOR_GRAPHIC_DOWN_ARROW_ON].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, nullptr);
    }

    buttonCreate(gPerkDialogWindow,
        PERK_WINDOW_LIST_X,
        PERK_WINDOW_LIST_Y,
        PERK_WINDOW_LIST_WIDTH,
        PERK_WINDOW_LIST_HEIGHT,
        -1,
        -1,
        -1,
        501,
        nullptr,
        nullptr,
        nullptr,
        BUTTON_FLAG_TRANSPARENT);

    fontSetCurrent(103);

    const char* msg;

    // PICK A NEW PERK
    msg = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 152);
    fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 16 + 49, msg, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, _colorTable[18979]);

    // DONE
    msg = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 100);
    fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 186 + 69, msg, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, _colorTable[18979]);

    // CANCEL
    msg = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 102);
    fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 186 + 171, msg, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, _colorTable[18979]);

    int count = perkDialogDrawPerks();

    // NOTE: Original code is slightly different, but does the same thing.
    int perk = gPerkDialogOptionList[gPerkDialogTopLine + gPerkDialogCurrentLine].value;
    int perkFrmId = perkGetFrmId(perk);
    char* perkName = perkGetName(perk);
    char* perkDescription = perkGetDescription(perk);
    char* perkRank = nullptr;
    char perkRankBuffer[32];

    int rank = perkGetRank(gDude, perk);
    if (rank != 0) {
        snprintf(perkRankBuffer, sizeof(perkRankBuffer), "(%d)", rank);
        perkRank = perkRankBuffer;
    }

    perkDialogDrawCard(perkFrmId, perkName, perkRank, perkDescription);
    windowRefresh(gPerkDialogWindow);

    int rc = perkDialogHandleInput(count, perkDialogRefreshPerks);

    if (rc == 1) {
        if (perkAdd(gDude, gPerkDialogOptionList[gPerkDialogTopLine + gPerkDialogCurrentLine].value) == -1) {
            debugPrint("\n*** Unable to add perk! ***\n");
            rc = 2;
        }
    }

    rc &= 1;

    if (rc != 0) {
        if (perkGetRank(gDude, PERK_TAG) != 0 && gCharacterEditorPerksBackup[PERK_TAG] == 0) {
            if (!perkDialogHandleTagPerk()) {
                perkRemove(gDude, PERK_TAG);
            }
        } else if (perkGetRank(gDude, PERK_MUTATE) != 0 && gCharacterEditorPerksBackup[PERK_MUTATE] == 0) {
            if (!perkDialogHandleMutatePerk()) {
                perkRemove(gDude, PERK_MUTATE);
            }
        } else if (perkGetRank(gDude, PERK_LIFEGIVER) != gCharacterEditorPerksBackup[PERK_LIFEGIVER]) {
            int maxHp = critterGetBonusStat(gDude, STAT_MAXIMUM_HIT_POINTS);
            critterSetBonusStat(gDude, STAT_MAXIMUM_HIT_POINTS, maxHp + 4);
            critterAdjustHitPoints(gDude, 4);
        } else if (perkGetRank(gDude, PERK_EDUCATED) != gCharacterEditorPerksBackup[PERK_EDUCATED]) {
            int sp = pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS);
            pcSetStat(PC_STAT_UNSPENT_SKILL_POINTS, sp + 2);
        }
    }

    characterEditorDrawSkills(0);
    characterEditorDrawPrimaryStat(RENDER_ALL_STATS, 0, 0);
    characterEditorDrawPcStats();
    characterEditorDrawDerivedStats();
    characterEditorDrawFolders();
    characterEditorDrawCard();
    windowRefresh(gCharacterEditorWindow);

    _perkDialogBackgroundFrmImage.unlock();

    windowDestroy(gPerkDialogWindow);

    return rc;
}

// 0x43CACC
static int perkDialogHandleInput(int count, void (*refreshProc)())
{
    fontSetCurrent(101);

    int v3 = count - 11;

    int height = fontGetLineHeight();
    gPerkDialogPreviousCurrentLine = -2;
    int v16 = height + 2;

    int v7 = 0;

    int rc = 0;
    while (rc == 0) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        int v19 = 0;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == 500) {
            rc = 1;
        } else if (keyCode == KEY_RETURN) {
            soundPlayFile("ib1p1xx1");
            rc = 1;
        } else if (keyCode == 501) {
            mouseGetPositionInWindow(gPerkDialogWindow, &gCharacterEditorMouseX, &gCharacterEditorMouseY);
            gPerkDialogCurrentLine = (gCharacterEditorMouseY - PERK_WINDOW_LIST_Y) / v16;
            if (gPerkDialogCurrentLine >= 0) {
                if (count - 1 < gPerkDialogCurrentLine)
                    gPerkDialogCurrentLine = count - 1;
            } else {
                gPerkDialogCurrentLine = 0;
            }

            if (gPerkDialogCurrentLine == gPerkDialogPreviousCurrentLine) {
                soundPlayFile("ib1p1xx1");
                rc = 1;
            }
            gPerkDialogPreviousCurrentLine = gPerkDialogCurrentLine;
            refreshProc();
        } else if (keyCode == 502 || keyCode == KEY_ESCAPE || _game_user_wants_to_quit != 0) {
            rc = 2;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                gPerkDialogPreviousCurrentLine = -2;

                gPerkDialogTopLine--;
                if (gPerkDialogTopLine < 0) {
                    gPerkDialogTopLine = 0;

                    gPerkDialogCurrentLine--;
                    if (gPerkDialogCurrentLine < 0) {
                        gPerkDialogCurrentLine = 0;
                    }
                }

                refreshProc();
                break;
            case KEY_PAGE_UP:
                gPerkDialogPreviousCurrentLine = -2;

                for (int index = 0; index < 11; index++) {
                    gPerkDialogTopLine--;
                    if (gPerkDialogTopLine < 0) {
                        gPerkDialogTopLine = 0;

                        gPerkDialogCurrentLine--;
                        if (gPerkDialogCurrentLine < 0) {
                            gPerkDialogCurrentLine = 0;
                        }
                    }
                }

                refreshProc();
                break;
            case KEY_ARROW_DOWN:
                gPerkDialogPreviousCurrentLine = -2;

                if (count > 11) {
                    gPerkDialogTopLine++;
                    if (gPerkDialogTopLine > count - 11) {
                        gPerkDialogTopLine = count - 11;

                        gPerkDialogCurrentLine++;
                        if (gPerkDialogCurrentLine > 10) {
                            gPerkDialogCurrentLine = 10;
                        }
                    }
                } else {
                    gPerkDialogCurrentLine++;
                    if (gPerkDialogCurrentLine > count - 1) {
                        gPerkDialogCurrentLine = count - 1;
                    }
                }

                refreshProc();
                break;
            case KEY_PAGE_DOWN:
                gPerkDialogPreviousCurrentLine = -2;

                for (int index = 0; index < 11; index++) {
                    if (count > 11) {
                        gPerkDialogTopLine++;
                        if (gPerkDialogTopLine > count - 11) {
                            gPerkDialogTopLine = count - 11;

                            gPerkDialogCurrentLine++;
                            if (gPerkDialogCurrentLine > 10) {
                                gPerkDialogCurrentLine = 10;
                            }
                        }
                    } else {
                        gPerkDialogCurrentLine++;
                        if (gPerkDialogCurrentLine > count - 1) {
                            gPerkDialogCurrentLine = count - 1;
                        }
                    }
                }

                refreshProc();
                break;
            case 572:
                _repFtime = 4;
                gPerkDialogPreviousCurrentLine = -2;

                do {
                    sharedFpsLimiter.mark();

                    _frame_time = getTicks();
                    if (v19 <= dbl_5019BE) {
                        v19++;
                    }

                    if (v19 == 1 || v19 > dbl_5019BE) {
                        if (v19 > dbl_5019BE) {
                            _repFtime++;
                            if (_repFtime > 24) {
                                _repFtime = 24;
                            }
                        }

                        gPerkDialogTopLine--;
                        if (gPerkDialogTopLine < 0) {
                            gPerkDialogTopLine = 0;

                            gPerkDialogCurrentLine--;
                            if (gPerkDialogCurrentLine < 0) {
                                gPerkDialogCurrentLine = 0;
                            }
                        }
                        refreshProc();
                    }

                    if (v19 < dbl_5019BE) {
                        delay_ms(1000 / 24 - (getTicks() - _frame_time));
                    } else {
                        delay_ms(1000 / _repFtime - (getTicks() - _frame_time));
                    }

                    renderPresent();
                    sharedFpsLimiter.throttle();
                } while (inputGetInput() != 574);

                break;
            case 573:
                gPerkDialogPreviousCurrentLine = -2;
                _repFtime = 4;

                if (count > 11) {
                    do {
                        sharedFpsLimiter.mark();

                        _frame_time = getTicks();
                        if (v19 <= dbl_5019BE) {
                            v19++;
                        }

                        if (v19 == 1 || v19 > dbl_5019BE) {
                            if (v19 > dbl_5019BE) {
                                _repFtime++;
                                if (_repFtime > 24) {
                                    _repFtime = 24;
                                }
                            }

                            gPerkDialogTopLine++;
                            if (gPerkDialogTopLine > count - 11) {
                                gPerkDialogTopLine = count - 11;

                                gPerkDialogCurrentLine++;
                                if (gPerkDialogCurrentLine > 10) {
                                    gPerkDialogCurrentLine = 10;
                                }
                            }

                            refreshProc();
                        }

                        if (v19 < dbl_5019BE) {
                            delay_ms(1000 / 24 - (getTicks() - _frame_time));
                        } else {
                            delay_ms(1000 / _repFtime - (getTicks() - _frame_time));
                        }

                        renderPresent();
                        sharedFpsLimiter.throttle();
                    } while (inputGetInput() != 575);
                } else {
                    do {
                        sharedFpsLimiter.mark();

                        _frame_time = getTicks();
                        if (v19 <= dbl_5019BE) {
                            v19++;
                        }

                        if (v19 == 1 || v19 > dbl_5019BE) {
                            if (v19 > dbl_5019BE) {
                                _repFtime++;
                                if (_repFtime > 24) {
                                    _repFtime = 24;
                                }
                            }

                            gPerkDialogCurrentLine++;
                            if (gPerkDialogCurrentLine > count - 1) {
                                gPerkDialogCurrentLine = count - 1;
                            }

                            refreshProc();
                        }

                        if (v19 < dbl_5019BE) {
                            delay_ms(1000 / 24 - (getTicks() - _frame_time));
                        } else {
                            delay_ms(1000 / _repFtime - (getTicks() - _frame_time));
                        }

                        renderPresent();
                        sharedFpsLimiter.throttle();
                    } while (inputGetInput() != 575);
                }
                break;
            case KEY_HOME:
                gPerkDialogTopLine = 0;
                gPerkDialogCurrentLine = 0;
                gPerkDialogPreviousCurrentLine = -2;
                refreshProc();
                break;
            case KEY_END:
                gPerkDialogPreviousCurrentLine = -2;
                if (count > 11) {
                    gPerkDialogTopLine = count - 11;
                    gPerkDialogCurrentLine = 10;
                } else {
                    gPerkDialogCurrentLine = count - 1;
                }
                refreshProc();
                break;
            default:
                if (getTicksSince(_frame_time) > 700) {
                    _frame_time = getTicks();
                    gPerkDialogPreviousCurrentLine = -2;
                }
                break;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    return rc;
}

// 0x43D0BC
static int perkDialogDrawPerks()
{
    blitBufferToBuffer(
        _perkDialogBackgroundFrmImage.getData() + PERK_WINDOW_WIDTH * 43 + 45,
        192,
        129,
        PERK_WINDOW_WIDTH,
        gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 43 + 45,
        PERK_WINDOW_WIDTH);

    fontSetCurrent(101);

    int perks[PERK_COUNT];
    int count = perkGetAvailablePerks(gDude, perks);
    if (count == 0) {
        return 0;
    }

    for (int perk = 0; perk < PERK_COUNT; perk++) {
        gPerkDialogOptionList[perk].value = 0;
        gPerkDialogOptionList[perk].name = nullptr;
    }

    for (int index = 0; index < count; index++) {
        gPerkDialogOptionList[index].value = perks[index];
        gPerkDialogOptionList[index].name = perkGetName(perks[index]);
    }

    qsort(gPerkDialogOptionList, count, sizeof(*gPerkDialogOptionList), perkDialogOptionCompare);

    int v16 = count - gPerkDialogTopLine;
    if (v16 > 11) {
        v16 = 11;
    }

    v16 += gPerkDialogTopLine;

    int y = 43;
    int yStep = fontGetLineHeight() + 2;
    for (int index = gPerkDialogTopLine; index < v16; index++) {
        int color;
        if (index == gPerkDialogTopLine + gPerkDialogCurrentLine) {
            color = _colorTable[32747];
        } else {
            color = _colorTable[992];
        }

        fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * y + 45, gPerkDialogOptionList[index].name, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, color);

        if (perkGetRank(gDude, gPerkDialogOptionList[index].value) != 0) {
            char rankString[256];
            snprintf(rankString, sizeof(rankString), "(%d)", perkGetRank(gDude, gPerkDialogOptionList[index].value));
            fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * y + 207, rankString, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, color);
        }

        y += yStep;
    }

    return count;
}

// 0x43D2F8
static void perkDialogRefreshTraits()
{
    blitBufferToBuffer(_perkDialogBackgroundFrmImage.getData() + 280,
        293,
        PERK_WINDOW_HEIGHT,
        PERK_WINDOW_WIDTH,
        gPerkDialogWindowBuffer + 280,
        PERK_WINDOW_WIDTH);

    perkDialogDrawTraits(gPerkDialogOptionCount);

    char* traitName = gPerkDialogOptionList[gPerkDialogTopLine + gPerkDialogCurrentLine].name;
    char* tratDescription = traitGetDescription(gPerkDialogOptionList[gPerkDialogTopLine + gPerkDialogCurrentLine].value);
    int frmId = traitGetFrmId(gPerkDialogOptionList[gPerkDialogTopLine + gPerkDialogCurrentLine].value);
    perkDialogDrawCard(frmId, traitName, nullptr, tratDescription);

    windowRefresh(gPerkDialogWindow);
}

// 0x43D38C
static bool perkDialogHandleMutatePerk()
{
    gPerkDialogCardFrmId = -1;
    gPerkDialogCardTitle[0] = '\0';
    gPerkDialogCardDrawn = false;

    int traitCount = TRAITS_MAX_SELECTED_COUNT - 1;
    int traitIndex = 0;
    while (traitCount >= 0) {
        if (gCharacterEditorTempTraits[traitIndex] != -1) {
            break;
        }
        traitCount--;
        traitIndex++;
    }

    gCharacterEditorTempTraitCount = TRAITS_MAX_SELECTED_COUNT - traitIndex;

    bool result = true;
    if (gCharacterEditorTempTraitCount >= 1) {
        fontSetCurrent(103);

        blitBufferToBuffer(_perkDialogBackgroundFrmImage.getData() + PERK_WINDOW_WIDTH * 14 + 49,
            206,
            fontGetLineHeight() + 2,
            PERK_WINDOW_WIDTH,
            gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 15 + 49,
            PERK_WINDOW_WIDTH);

        // LOSE A TRAIT
        char* msg = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 154);
        fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 16 + 49, msg, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, _colorTable[18979]);

        gPerkDialogOptionCount = 0;
        gPerkDialogCurrentLine = 0;
        gPerkDialogTopLine = 0;
        perkDialogRefreshTraits();

        int rc = perkDialogHandleInput(gCharacterEditorTempTraitCount, perkDialogRefreshTraits);
        if (rc == 1) {
            if (gPerkDialogCurrentLine == 0) {
                if (gCharacterEditorTempTraitCount == 1) {
                    gCharacterEditorTempTraits[0] = -1;
                    gCharacterEditorTempTraits[1] = -1;
                } else {
                    if (gPerkDialogOptionList[0].value == gCharacterEditorTempTraits[0]) {
                        gCharacterEditorTempTraits[0] = gCharacterEditorTempTraits[1];
                        gCharacterEditorTempTraits[1] = -1;
                    } else {
                        gCharacterEditorTempTraits[1] = -1;
                    }
                }
            } else {
                if (gPerkDialogOptionList[0].value == gCharacterEditorTempTraits[0]) {
                    gCharacterEditorTempTraits[1] = -1;
                } else {
                    gCharacterEditorTempTraits[0] = gCharacterEditorTempTraits[1];
                    gCharacterEditorTempTraits[1] = -1;
                }
            }
        } else {
            result = false;
        }
    }

    if (result) {
        fontSetCurrent(103);

        blitBufferToBuffer(_perkDialogBackgroundFrmImage.getData() + PERK_WINDOW_WIDTH * 14 + 49,
            206,
            fontGetLineHeight() + 2,
            PERK_WINDOW_WIDTH,
            gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 15 + 49,
            PERK_WINDOW_WIDTH);

        // PICK A NEW TRAIT
        char* msg = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 153);
        fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 16 + 49, msg, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, _colorTable[18979]);

        gPerkDialogCurrentLine = 0;
        gPerkDialogTopLine = 0;
        gPerkDialogOptionCount = 1;

        perkDialogRefreshTraits();

        int count = 16 - gCharacterEditorTempTraitCount;
        if (count > 16) {
            count = 16;
        }

        int rc = perkDialogHandleInput(count, perkDialogRefreshTraits);
        if (rc == 1) {
            if (gCharacterEditorTempTraitCount != 0) {
                gCharacterEditorTempTraits[1] = gPerkDialogOptionList[gPerkDialogCurrentLine + gPerkDialogTopLine].value;
            } else {
                gCharacterEditorTempTraits[0] = gPerkDialogOptionList[gPerkDialogCurrentLine + gPerkDialogTopLine].value;
                gCharacterEditorTempTraits[1] = -1;
            }

            traitsSetSelected(gCharacterEditorTempTraits[0], gCharacterEditorTempTraits[1]);
        } else {
            result = false;
        }
    }

    if (!result) {
        memcpy(gCharacterEditorTempTraits, gCharacterEditorOptionalTraitsBackup, sizeof(gCharacterEditorTempTraits));
    }

    return result;
}

// 0x43D668
static void perkDialogRefreshSkills()
{
    blitBufferToBuffer(_perkDialogBackgroundFrmImage.getData() + 280,
        293,
        PERK_WINDOW_HEIGHT,
        PERK_WINDOW_WIDTH,
        gPerkDialogWindowBuffer + 280,
        PERK_WINDOW_WIDTH);

    perkDialogDrawSkills();

    char* name = gPerkDialogOptionList[gPerkDialogTopLine + gPerkDialogCurrentLine].name;
    char* description = skillGetDescription(gPerkDialogOptionList[gPerkDialogTopLine + gPerkDialogCurrentLine].value);
    int frmId = skillGetFrmId(gPerkDialogOptionList[gPerkDialogTopLine + gPerkDialogCurrentLine].value);
    perkDialogDrawCard(frmId, name, nullptr, description);

    windowRefresh(gPerkDialogWindow);
}

// 0x43D6F8
static bool perkDialogHandleTagPerk()
{
    fontSetCurrent(103);

    blitBufferToBuffer(_perkDialogBackgroundFrmImage.getData() + 573 * 14 + 49,
        206,
        fontGetLineHeight() + 2,
        573,
        gPerkDialogWindowBuffer + 573 * 15 + 49,
        573);

    // PICK A NEW TAG SKILL
    char* messageListItemText = getmsg(&gCharacterEditorMessageList, &gCharacterEditorMessageListItem, 155);
    fontDrawText(gPerkDialogWindowBuffer + 573 * 16 + 49, messageListItemText, 573, 573, _colorTable[18979]);

    gPerkDialogCurrentLine = 0;
    gPerkDialogTopLine = 0;
    gPerkDialogCardFrmId = -1;
    gPerkDialogCardTitle[0] = '\0';
    gPerkDialogCardDrawn = false;
    perkDialogRefreshSkills();

    int rc = perkDialogHandleInput(gPerkDialogOptionCount, perkDialogRefreshSkills);
    if (rc != 1) {
        memcpy(gCharacterEditorTempTaggedSkills, gCharacterEditorTaggedSkillsBackup, sizeof(gCharacterEditorTempTaggedSkills));
        skillsSetTagged(gCharacterEditorTaggedSkillsBackup, NUM_TAGGED_SKILLS);
        return false;
    }

    gCharacterEditorTempTaggedSkills[3] = gPerkDialogOptionList[gPerkDialogTopLine + gPerkDialogCurrentLine].value;
    skillsSetTagged(gCharacterEditorTempTaggedSkills, NUM_TAGGED_SKILLS);

    return true;
}

// 0x43D81C
static void perkDialogDrawSkills()
{
    blitBufferToBuffer(_perkDialogBackgroundFrmImage.getData() + PERK_WINDOW_WIDTH * 43 + 45,
        192,
        129,
        PERK_WINDOW_WIDTH,
        gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 43 + 45,
        PERK_WINDOW_WIDTH);

    fontSetCurrent(101);

    gPerkDialogOptionCount = 0;

    int y = 43;
    int yStep = fontGetLineHeight() + 2;

    for (int skill = 0; skill < SKILL_COUNT; skill++) {
        if (skill != gCharacterEditorTempTaggedSkills[0] && skill != gCharacterEditorTempTaggedSkills[1] && skill != gCharacterEditorTempTaggedSkills[2] && skill != gCharacterEditorTempTaggedSkills[3]) {
            gPerkDialogOptionList[gPerkDialogOptionCount].value = skill;
            gPerkDialogOptionList[gPerkDialogOptionCount].name = skillGetName(skill);
            gPerkDialogOptionCount++;
        }
    }

    qsort(gPerkDialogOptionList, gPerkDialogOptionCount, sizeof(*gPerkDialogOptionList), perkDialogOptionCompare);

    for (int index = gPerkDialogTopLine; index < gPerkDialogTopLine + 11; index++) {
        int color;
        if (index == gPerkDialogCurrentLine + gPerkDialogTopLine) {
            color = _colorTable[32747];
        } else {
            color = _colorTable[992];
        }

        fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * y + 45, gPerkDialogOptionList[index].name, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, color);
        y += yStep;
    }
}

// 0x43D960
static int perkDialogDrawTraits(int a1)
{
    blitBufferToBuffer(_perkDialogBackgroundFrmImage.getData() + PERK_WINDOW_WIDTH * 43 + 45,
        192,
        129,
        PERK_WINDOW_WIDTH,
        gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 43 + 45,
        PERK_WINDOW_WIDTH);

    fontSetCurrent(101);

    int y = 43;
    int yStep = fontGetLineHeight() + 2;

    if (a1 != 0) {
        int count = 0;
        for (int trait = 0; trait < TRAIT_COUNT; trait++) {
            if (trait != gCharacterEditorOptionalTraitsBackup[0] && trait != gCharacterEditorOptionalTraitsBackup[1]) {
                gPerkDialogOptionList[count].value = trait;
                gPerkDialogOptionList[count].name = traitGetName(trait);
                count++;
            }
        }

        qsort(gPerkDialogOptionList, count, sizeof(*gPerkDialogOptionList), perkDialogOptionCompare);

        for (int index = gPerkDialogTopLine; index < gPerkDialogTopLine + 11; index++) {
            int color;
            if (index == gPerkDialogCurrentLine + gPerkDialogTopLine) {
                color = _colorTable[32747];
            } else {
                color = _colorTable[992];
            }

            fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * y + 45, gPerkDialogOptionList[index].name, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, color);
            y += yStep;
        }
    } else {
        // NOTE: Original code does not use loop.
        for (int index = 0; index < TRAITS_MAX_SELECTED_COUNT; index++) {
            gPerkDialogOptionList[index].value = gCharacterEditorTempTraits[index];
            gPerkDialogOptionList[index].name = traitGetName(gCharacterEditorTempTraits[index]);
        }

        if (gCharacterEditorTempTraitCount > 1) {
            qsort(gPerkDialogOptionList, gCharacterEditorTempTraitCount, sizeof(*gPerkDialogOptionList), perkDialogOptionCompare);
        }

        for (int index = 0; index < gCharacterEditorTempTraitCount; index++) {
            int color;
            if (index == gPerkDialogCurrentLine) {
                color = _colorTable[32747];
            } else {
                color = _colorTable[992];
            }

            fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * y + 45, gPerkDialogOptionList[index].name, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, color);
            y += yStep;
        }
    }
    return 0;
}

// 0x43DB48
static int perkDialogOptionCompare(const void* a1, const void* a2)
{
    PerkDialogOption* v1 = (PerkDialogOption*)a1;
    PerkDialogOption* v2 = (PerkDialogOption*)a2;
    return strcmp(v1->name, v2->name);
}

// 0x43DB54
static int perkDialogDrawCard(int frmId, const char* name, const char* rank, char* description)
{
    FrmImage frmImage;
    int fid = buildFid(OBJ_TYPE_SKILLDEX, frmId, 0, 0, 0);
    if (!frmImage.lock(fid)) {
        return -1;
    }

    blitBufferToBuffer(frmImage.getData(),
        frmImage.getWidth(),
        frmImage.getHeight(),
        frmImage.getWidth(),
        gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 64 + 413,
        PERK_WINDOW_WIDTH);

    // Calculate width of transparent pixels on the left side of the image. This
    // space will be occupied by description (in addition to fixed width).
    int extraDescriptionWidth = 150;
    unsigned char* data = frmImage.getData();
    for (int y = 0; y < frmImage.getHeight(); y++) {
        unsigned char* stride = data;
        for (int x = 0; x < frmImage.getWidth(); x++) {
            if (HighRGB(*stride) < 2) {
                extraDescriptionWidth = std::min(extraDescriptionWidth, x);
            }
            stride++;
        }
        data += frmImage.getWidth();
    }

    // Add gap between description and image.
    extraDescriptionWidth -= 8;
    if (extraDescriptionWidth < 0) {
        extraDescriptionWidth = 0;
    }

    fontSetCurrent(102);
    int nameHeight = fontGetLineHeight();

    fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * 27 + 280, name, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, _colorTable[0]);

    if (rank != nullptr) {
        int rankX = fontGetStringWidth(name) + 280 + 8;
        fontSetCurrent(101);

        int rankHeight = fontGetLineHeight();
        fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * (23 + nameHeight - rankHeight) + rankX, rank, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, _colorTable[0]);
    }

    windowDrawLine(gPerkDialogWindow, 280, 27 + nameHeight, 545, 27 + nameHeight, _colorTable[0]);
    windowDrawLine(gPerkDialogWindow, 280, 28 + nameHeight, 545, 28 + nameHeight, _colorTable[0]);

    fontSetCurrent(101);

    int yStep = fontGetLineHeight() + 1;
    int y = 70;

    short beginnings[WORD_WRAP_MAX_COUNT];
    short count;
    if (wordWrap(description, 133 + extraDescriptionWidth, beginnings, &count) != 0) {
        // FIXME: Leaks handle.
        return -1;
    }

    for (int index = 0; index < count - 1; index++) {
        char* beginning = description + beginnings[index];
        char* ending = description + beginnings[index + 1];

        char ch = *ending;
        *ending = '\0';

        fontDrawText(gPerkDialogWindowBuffer + PERK_WINDOW_WIDTH * y + 280, beginning, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, _colorTable[0]);

        *ending = ch;

        y += yStep;
    }

    if (frmId != gPerkDialogCardFrmId || strcmp(gPerkDialogCardTitle, name) != 0) {
        if (gPerkDialogCardDrawn) {
            soundPlayFile("isdxxxx1");
        }
    }

    strcpy(gPerkDialogCardTitle, name);
    gPerkDialogCardFrmId = frmId;
    gPerkDialogCardDrawn = true;

    return 0;
}

// copy editor perks to character
//
// 0x43DEBC
static void _pop_perks()
{
    for (int perk = 0; perk < PERK_COUNT; perk++) {
        for (;;) {
            int rank = perkGetRank(gDude, perk);
            if (rank <= gCharacterEditorPerksBackup[perk]) {
                break;
            }

            perkRemove(gDude, perk);
        }
    }

    for (int i = 0; i < PERK_COUNT; i++) {
        for (;;) {
            int rank = perkGetRank(gDude, i);
            if (rank >= gCharacterEditorPerksBackup[i]) {
                break;
            }

            perkAdd(gDude, i);
        }
    }
}

// validate SPECIAL stats are <= 10
//
// 0x43DF50
static int _is_supper_bonus()
{
    for (int stat = 0; stat < 7; stat++) {
        int v1 = critterGetBaseStatWithTraitModifier(gDude, stat);
        int v2 = critterGetBonusStat(gDude, stat);
        if (v1 + v2 > 10) {
            return 1;
        }
    }

    return 0;
}

// 0x43DF8C
static int characterEditorFolderViewInit()
{
    gCharacterEditorKarmaFolderTopLine = 0;
    gCharacterEditorPerkFolderTopLine = 0;
    gCharacterEditorKillsFolderTopLine = 0;

    if (gCharacterEditorFolderViewScrollUpBtn == -1) {
        gCharacterEditorFolderViewScrollUpBtn = buttonCreate(gCharacterEditorWindow, 317, 364, _editorFrmImages[22].getWidth(), _editorFrmImages[22].getHeight(), -1, -1, -1, 17000, _editorFrmImages[21].getData(), _editorFrmImages[22].getData(), nullptr, 32);
        if (gCharacterEditorFolderViewScrollUpBtn == -1) {
            return -1;
        }

        buttonSetCallbacks(gCharacterEditorFolderViewScrollUpBtn, _gsound_red_butt_press, nullptr);
    }

    if (gCharacterEditorFolderViewScrollDownBtn == -1) {
        gCharacterEditorFolderViewScrollDownBtn = buttonCreate(gCharacterEditorWindow,
            317,
            365 + _editorFrmImages[22].getHeight(),
            _editorFrmImages[4].getWidth(),
            _editorFrmImages[4].getHeight(),
            gCharacterEditorFolderViewScrollDownBtn,
            gCharacterEditorFolderViewScrollDownBtn,
            gCharacterEditorFolderViewScrollDownBtn,
            17001,
            _editorFrmImages[3].getData(),
            _editorFrmImages[4].getData(),
            nullptr,
            32);
        if (gCharacterEditorFolderViewScrollDownBtn == -1) {
            buttonDestroy(gCharacterEditorFolderViewScrollUpBtn);
            return -1;
        }

        buttonSetCallbacks(gCharacterEditorFolderViewScrollDownBtn, _gsound_red_butt_press, nullptr);
    }

    return 0;
}

// 0x43E0D4
static void characterEditorFolderViewScroll(int direction)
{
    int* v1;

    switch (characterEditorWindowSelectedFolder) {
    case EDITOR_FOLDER_PERKS:
        v1 = &gCharacterEditorPerkFolderTopLine;
        break;
    case EDITOR_FOLDER_KARMA:
        v1 = &gCharacterEditorKarmaFolderTopLine;
        break;
    case EDITOR_FOLDER_KILLS:
        v1 = &gCharacterEditorKillsFolderTopLine;
        break;
    default:
        return;
    }

    if (direction >= 0) {
        if (gCharacterEditorFolderViewMaxLines + gCharacterEditorFolderViewTopLine <= gCharacterEditorFolderViewCurrentLine) {
            gCharacterEditorFolderViewTopLine++;
            if (characterEditorSelectedItem >= 10 && characterEditorSelectedItem < 43 && characterEditorSelectedItem != 10) {
                characterEditorSelectedItem--;
            }
        }
    } else {
        if (gCharacterEditorFolderViewTopLine > 0) {
            gCharacterEditorFolderViewTopLine--;
            if (characterEditorSelectedItem >= 10 && characterEditorSelectedItem < 43 && gCharacterEditorFolderViewMaxLines + 9 > characterEditorSelectedItem) {
                characterEditorSelectedItem++;
            }
        }
    }

    *v1 = gCharacterEditorFolderViewTopLine;
    characterEditorDrawFolders();

    if (characterEditorSelectedItem >= 10 && characterEditorSelectedItem < 43) {
        blitBufferToBuffer(
            _editorBackgroundFrmImage.getData() + 640 * 267 + 345,
            277,
            170,
            640,
            gCharacterEditorWindowBuffer + 640 * 267 + 345,
            640);
        characterEditorDrawCardWithOptions(gCharacterEditorFolderCardFrmId, gCharacterEditorFolderCardTitle, gCharacterEditorFolderCardSubtitle, gCharacterEditorFolderCardDescription);
    }
}

// 0x43E200
static void characterEditorFolderViewClear()
{
    int v0;

    gCharacterEditorFolderViewCurrentLine = 0;
    gCharacterEditorFolderViewNextY = 364;

    v0 = fontGetLineHeight();

    gCharacterEditorFolderViewMaxLines = 9;
    gCharacterEditorFolderViewOffsetY = v0 + 1;

    if (characterEditorSelectedItem < 10 || characterEditorSelectedItem >= 43)
        gCharacterEditorFolderViewHighlightedLine = -1;
    else
        gCharacterEditorFolderViewHighlightedLine = characterEditorSelectedItem - 10;

    if (characterEditorWindowSelectedFolder < 1) {
        if (characterEditorWindowSelectedFolder)
            return;

        gCharacterEditorFolderViewTopLine = gCharacterEditorPerkFolderTopLine;
    } else if (characterEditorWindowSelectedFolder == 1) {
        gCharacterEditorFolderViewTopLine = gCharacterEditorKarmaFolderTopLine;
    } else if (characterEditorWindowSelectedFolder == 2) {
        gCharacterEditorFolderViewTopLine = gCharacterEditorKillsFolderTopLine;
    }
}

// render heading string with line
//
// 0x43E28C
static int characterEditorFolderViewDrawHeading(const char* string)
{
    int lineHeight;
    int x;
    int y;
    int lineLen;
    int gap;
    int v8 = 0;

    if (gCharacterEditorFolderViewMaxLines + gCharacterEditorFolderViewTopLine > gCharacterEditorFolderViewCurrentLine) {
        if (gCharacterEditorFolderViewCurrentLine >= gCharacterEditorFolderViewTopLine) {
            if (gCharacterEditorFolderViewCurrentLine - gCharacterEditorFolderViewTopLine == gCharacterEditorFolderViewHighlightedLine) {
                v8 = 1;
            }
            lineHeight = fontGetLineHeight();
            x = 280;
            y = gCharacterEditorFolderViewNextY + lineHeight / 2;
            if (string != nullptr) {
                gap = fontGetLetterSpacing();
                // TODO: Not sure about this.
                lineLen = fontGetStringWidth(string) + gap * 4;
                x = (x - lineLen) / 2;
                fontDrawText(gCharacterEditorWindowBuffer + 640 * gCharacterEditorFolderViewNextY + 34 + x + gap * 2, string, 640, 640, _colorTable[992]);
                windowDrawLine(gCharacterEditorWindow, 34 + x + lineLen, y, 34 + 280, y, _colorTable[992]);
            }
            windowDrawLine(gCharacterEditorWindow, 34, y, 34 + x, y, _colorTable[992]);
            gCharacterEditorFolderViewNextY += gCharacterEditorFolderViewOffsetY;
        }
        gCharacterEditorFolderViewCurrentLine++;
        return v8;
    } else {
        return 0;
    }
}

// 0x43E3D8
static bool characterEditorFolderViewDrawString(const char* string)
{
    bool success = false;
    int color;

    if (gCharacterEditorFolderViewMaxLines + gCharacterEditorFolderViewTopLine > gCharacterEditorFolderViewCurrentLine) {
        if (gCharacterEditorFolderViewCurrentLine >= gCharacterEditorFolderViewTopLine) {
            if (gCharacterEditorFolderViewCurrentLine - gCharacterEditorFolderViewTopLine == gCharacterEditorFolderViewHighlightedLine) {
                success = true;
                color = _colorTable[32747];
            } else {
                color = _colorTable[992];
            }

            fontDrawText(gCharacterEditorWindowBuffer + 640 * gCharacterEditorFolderViewNextY + 34, string, 640, 640, color);
            gCharacterEditorFolderViewNextY += gCharacterEditorFolderViewOffsetY;
        }

        gCharacterEditorFolderViewCurrentLine++;
    }

    return success;
}

// 0x43E470
static bool characterEditorFolderViewDrawKillsEntry(const char* name, int kills)
{
    char killsString[8];
    int color;
    int gap;

    bool success = false;
    if (gCharacterEditorFolderViewMaxLines + gCharacterEditorFolderViewTopLine > gCharacterEditorFolderViewCurrentLine) {
        if (gCharacterEditorFolderViewCurrentLine >= gCharacterEditorFolderViewTopLine) {
            if (gCharacterEditorFolderViewCurrentLine - gCharacterEditorFolderViewTopLine == gCharacterEditorFolderViewHighlightedLine) {
                color = _colorTable[32747];
                success = true;
            } else {
                color = _colorTable[992];
            }

            compat_itoa(kills, killsString, 10);
            int v6 = fontGetStringWidth(killsString);

            // TODO: Check.
            gap = fontGetLetterSpacing();
            int v11 = gCharacterEditorFolderViewNextY + fontGetLineHeight() / 2;

            fontDrawText(gCharacterEditorWindowBuffer + 640 * gCharacterEditorFolderViewNextY + 34, name, 640, 640, color);

            int v12 = fontGetStringWidth(name);
            windowDrawLine(gCharacterEditorWindow, 34 + v12 + gap, v11, 314 - v6 - gap, v11, color);

            fontDrawText(gCharacterEditorWindowBuffer + 640 * gCharacterEditorFolderViewNextY + 314 - v6, killsString, 640, 640, color);
            gCharacterEditorFolderViewNextY += gCharacterEditorFolderViewOffsetY;
        }

        gCharacterEditorFolderViewCurrentLine++;
    }

    return success;
}

// 0x43E5C4
static int karmaInit()
{
    const char* delim = " \t,";

    if (gKarmaEntries != nullptr) {
        internal_free(gKarmaEntries);
        gKarmaEntries = nullptr;
    }

    gKarmaEntriesLength = 0;

    File* stream = fileOpen("data\\karmavar.txt", "rt");
    if (stream == nullptr) {
        return -1;
    }

    char string[256];
    while (fileReadString(string, 256, stream)) {
        KarmaEntry entry;

        char* pch = string;
        while (isspace(*pch & 0xFF)) {
            pch++;
        }

        if (*pch == '#') {
            continue;
        }

        char* tok = strtok(pch, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.gvar = atoi(tok);

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.art_num = atoi(tok);

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.name = atoi(tok);

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.description = atoi(tok);

        KarmaEntry* entries = (KarmaEntry*)internal_realloc(gKarmaEntries, sizeof(*entries) * (gKarmaEntriesLength + 1));
        if (entries == nullptr) {
            fileClose(stream);

            return -1;
        }

        memcpy(&(entries[gKarmaEntriesLength]), &entry, sizeof(entry));

        gKarmaEntries = entries;
        gKarmaEntriesLength++;
    }

    qsort(gKarmaEntries, gKarmaEntriesLength, sizeof(*gKarmaEntries), karmaEntryCompare);

    fileClose(stream);

    return 0;
}

// NOTE: Inlined.
//
// 0x43E764
static void karmaFree()
{
    if (gKarmaEntries != nullptr) {
        internal_free(gKarmaEntries);
        gKarmaEntries = nullptr;
    }

    gKarmaEntriesLength = 0;
}

// 0x43E78C
static int karmaEntryCompare(const void* a1, const void* a2)
{
    KarmaEntry* v1 = (KarmaEntry*)a1;
    KarmaEntry* v2 = (KarmaEntry*)a2;
    return v1->gvar - v2->gvar;
}

// 0x43E798
static int genericReputationInit()
{
    const char* delim = " \t,";

    if (gGenericReputationEntries != nullptr) {
        internal_free(gGenericReputationEntries);
        gGenericReputationEntries = nullptr;
    }

    gGenericReputationEntriesLength = 0;

    File* stream = fileOpen("data\\genrep.txt", "rt");
    if (stream == nullptr) {
        return -1;
    }

    char string[256];
    while (fileReadString(string, 256, stream)) {
        GenericReputationEntry entry;

        char* pch = string;
        while (isspace(*pch & 0xFF)) {
            pch++;
        }

        if (*pch == '#') {
            continue;
        }

        char* tok = strtok(pch, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.threshold = atoi(tok);

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.name = atoi(tok);

        GenericReputationEntry* entries = (GenericReputationEntry*)internal_realloc(gGenericReputationEntries, sizeof(*entries) * (gGenericReputationEntriesLength + 1));
        if (entries == nullptr) {
            fileClose(stream);

            return -1;
        }

        memcpy(&(entries[gGenericReputationEntriesLength]), &entry, sizeof(entry));

        gGenericReputationEntries = entries;
        gGenericReputationEntriesLength++;
    }

    qsort(gGenericReputationEntries, gGenericReputationEntriesLength, sizeof(*gGenericReputationEntries), genericReputationCompare);

    fileClose(stream);

    return 0;
}

// NOTE: Inlined.
//
// 0x43E914
static void genericReputationFree()
{
    if (gGenericReputationEntries != nullptr) {
        internal_free(gGenericReputationEntries);
        gGenericReputationEntries = nullptr;
    }

    gGenericReputationEntriesLength = 0;
}

// 0x43E93C
static int genericReputationCompare(const void* a1, const void* a2)
{
    GenericReputationEntry* v1 = (GenericReputationEntry*)a1;
    GenericReputationEntry* v2 = (GenericReputationEntry*)a2;

    if (v2->threshold > v1->threshold) {
        return 1;
    } else if (v2->threshold < v1->threshold) {
        return -1;
    }
    return 0;
}

static void customKarmaFolderInit()
{
    char* karmaFrms = nullptr;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_KARMA_FRMS_KEY, &karmaFrms);
    if (karmaFrms != nullptr && karmaFrms[0] == '\0') {
        karmaFrms = nullptr;
    }

    if (karmaFrms == nullptr) {
        return;
    }

    char* karmaPoints = nullptr;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_KARMA_POINTS_KEY, &karmaPoints);
    if (karmaPoints != nullptr && karmaPoints[0] == '\0') {
        karmaPoints = nullptr;
    }

    if (karmaPoints == nullptr) {
        return;
    }

    int karmaFrmsCount = 0;
    for (char* pch = karmaFrms; pch != nullptr; pch = strchr(pch + 1, ',')) {
        karmaFrmsCount++;
    }

    int karmaPointsCount = 0;
    for (char* pch = karmaPoints; pch != nullptr; pch = strchr(pch + 1, ',')) {
        karmaPointsCount++;
    }

    gCustomKarmaFolderDescriptions.resize(karmaFrmsCount);

    for (int index = 0; index < karmaFrmsCount; index++) {
        char* pch;

        pch = strchr(karmaFrms, ',');
        if (pch != nullptr) {
            *pch = '\0';
        }

        gCustomKarmaFolderDescriptions[index].frmId = atoi(karmaFrms);

        if (pch != nullptr) {
            *pch = ',';
        }

        karmaFrms = pch + 1;

        if (index < karmaPointsCount) {
            pch = strchr(karmaPoints, ',');
            if (pch != nullptr) {
                *pch = '\0';
            }

            gCustomKarmaFolderDescriptions[index].threshold = atoi(karmaPoints);

            if (pch != nullptr) {
                *pch = ',';
            }

            karmaPoints = pch + 1;
        } else {
            gCustomKarmaFolderDescriptions[index].threshold = INT_MAX;
        }
    }
}

static void customKarmaFolderFree()
{
    gCustomKarmaFolderDescriptions.clear();
}

static int customKarmaFolderGetFrmId()
{
    if (gCustomKarmaFolderDescriptions.empty()) {
        return 47;
    }

    int reputation = gGameGlobalVars[GVAR_PLAYER_REPUTATION];
    for (auto& entry : gCustomKarmaFolderDescriptions) {
        if (reputation < entry.threshold) {
            return entry.frmId;
        }
    }
    return gCustomKarmaFolderDescriptions.back().frmId;
}

static void customTownReputationInit()
{
    char* reputationList = nullptr;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_CITY_REPUTATION_LIST_KEY, &reputationList);
    if (reputationList != nullptr && *reputationList == '\0') {
        reputationList = nullptr;
    }

    char* curr = reputationList;
    while (curr != nullptr) {
        char* next = strchr(curr, ',');
        if (next != nullptr) {
            *next = '\0';
        }

        char* sep = strchr(curr, ':');
        if (sep != nullptr) {
            *sep = '\0';

            TownReputationEntry entry;
            entry.city = atoi(curr);
            entry.gvar = atoi(sep + 1);
            gCustomTownReputationEntries.push_back(std::move(entry));

            *sep = ':';
        }

        if (next != nullptr) {
            *next = ',';
            curr = next + 1;
        } else {
            curr = nullptr;
        }
    }

    if (gCustomTownReputationEntries.empty()) {
        gCustomTownReputationEntries.resize(TOWN_REPUTATION_COUNT);

        for (int index = 0; index < TOWN_REPUTATION_COUNT; index++) {
            gCustomTownReputationEntries[index].gvar = gTownReputationEntries[index].gvar;
            gCustomTownReputationEntries[index].city = gTownReputationEntries[index].city;
        }
    }
}

static void customTownReputationFree()
{
    gCustomTownReputationEntries.clear();
}

} // namespace fallout
