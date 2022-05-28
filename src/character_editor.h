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

typedef struct STRUCT_56FCB0 {
    int field_0;
    char* field_4;
} STRUCT_56FCB0;

// TODO: Field order is probably wrong.
typedef struct KillInfo {
    const char* name;
    int killTypeId;
    int kills;
} KillInfo;

extern int _grph_id[50];
extern const unsigned char _copyflag[EDITOR_GRAPHIC_COUNT];
extern const int word_431D3A[EDITOR_DERIVED_STAT_COUNT];
extern const int _StatYpos[7];
extern const int word_431D6C[EDITOR_DERIVED_STAT_COUNT];
extern char byte_431D93[64];
extern const int dword_431DD4[7];

extern const double dbl_50170B;
extern const double dbl_501713;
extern const double dbl_5018F0;
extern const double dbl_5019BE;

extern bool _bk_enable_0;
extern int _skill_cursor;
extern int _slider_y;
extern int characterEditorRemainingCharacterPoints;
extern KarmaEntry* gKarmaEntries;
extern int gKarmaEntriesLength;
extern GenericReputationEntry* gGenericReputationEntries;
extern int gGenericReputationEntriesLength;
extern const TownReputationEntry gTownReputationEntries[TOWN_REPUTATION_COUNT];
extern const int gAddictionReputationVars[ADDICTION_REPUTATION_COUNT];
extern const int gAddictionReputationFrmIds[ADDICTION_REPUTATION_COUNT];
extern int _folder_up_button;
extern int _folder_down_button;

extern char _folder_card_string[256];
extern int _skillsav[SKILL_COUNT];
extern MessageList editorMessageList;
extern STRUCT_56FCB0 _name_sort_list[DIALOG_PICKER_NUM_OPTIONS];
extern int _trait_bids[TRAIT_COUNT];
extern MessageListItem editorMessageListItem;
extern char _old_str1[48];
extern char _old_str2[48];
extern int _tag_bids[SKILL_COUNT];
extern char _name_save[32];
extern Size _GInfo[EDITOR_GRAPHIC_COUNT];
extern CacheEntry* _grph_key[EDITOR_GRAPHIC_COUNT];
extern unsigned char* _grphcpy[EDITOR_GRAPHIC_COUNT];
extern unsigned char* _grphbmp[EDITOR_GRAPHIC_COUNT];
extern int _folder_max_lines;
extern int _folder_line;
extern int _folder_card_fid;
extern int _folder_top_line;
extern char* _folder_card_title;
extern char* _folder_card_title2;
extern int _folder_yoffset;
extern int _folder_karma_top_line;
extern int _folder_highlight_line;
extern char* _folder_card_desc;
extern int _folder_ypos;
extern int _folder_kills_top_line;
extern int _folder_perk_top_line;
extern unsigned char* gEditorPerkBackgroundBuffer;
extern int gEditorPerkWindow;
extern int _SliderPlusID;
extern int _SliderNegID;
extern int _stat_bids_minus[7];
extern unsigned char* characterEditorWindowBuf;
extern int characterEditorWindowHandle;
extern int _stat_bids_plus[7];
extern unsigned char* gEditorPerkWindowBuffer;
extern CritterProtoData _dude_data;
extern unsigned char* characterEditorWindowBackgroundBuf;
extern int _cline;
extern int _oldsline;
extern int _upsent_points_back;
extern int _last_level;
extern int characterEditorWindowOldFont;
extern int _kills_count;
extern CacheEntry* _bck_key;
extern int _hp_back;
extern int _mouse_ypos;
extern int _mouse_xpos;
extern int characterEditorSelectedItem;
extern int characterEditorWindowSelectedFolder;
extern int _frstc_draw1;
extern int _crow;
extern int _frstc_draw2;
extern int _perk_back[PERK_COUNT];
extern unsigned int _repFtime;
extern unsigned int _frame_time;
extern int _old_tags;
extern int _last_level_back;
extern bool gCharacterEditorIsCreationMode;
extern int _tag_skill_back[NUM_TAGGED_SKILLS];
extern int _card_old_fid2;
extern int _card_old_fid1;
extern int _trait_back[3];
extern int _trait_count;
extern int _optrt_count;
extern int _temp_trait[3];
extern int _tagskill_count;
extern int _temp_tag_skill[NUM_TAGGED_SKILLS];
extern char _free_perk_back;
extern unsigned char _free_perk;
extern unsigned char _first_skill_list;

int _editor_design(bool isCreationMode);
int characterEditorWindowInit();
void characterEditorWindowFree();
void _CharEditInit();
int _get_input_str(int win, int cancelKeyCode, char* text, int maxLength, int x, int y, int textColor, int backgroundColor, int flags);
bool _isdoschar(int ch);
char* _strmfe(char* dest, const char* name, const char* ext);
void editorRenderFolders();
void editorRenderPerks();
int _kills_list_comp(const KillInfo* a, const KillInfo* b);
int editorRenderKills();
void characterEditorRenderBigNumber(int x, int y, int flags, int value, int previousValue, int windowHandle);
void editorRenderPcStats();
void editorRenderPrimaryStat(int stat, bool animate, int previousValue);
void editorRenderGender();
void editorRenderAge();
void editorRenderName();
void editorRenderSecondaryStats();
void editorRenderSkills(int a1);
void editorRenderDetails();
int characterEditorEditName();
void _PrintName(unsigned char* buf, int a2);
int characterEditorRunEditAgeDialog();
void characterEditorEditGender();
void characterEditorHandleIncDecPrimaryStat(int eventCode);
int _OptionWindow();
bool characterFileExists(const char* fname);
int characterPrintToFile(const char* fileName);
char* _AddSpaces(char* string, int length);
char* _AddDots(char* string, int length);
void _ResetScreen();
void _RegInfoAreas();
void _SavePlayer();
void _RestorePlayer();
char* _itostndn(int value, char* dest);
int _DrawCard(int graphicId, const char* name, const char* attributes, char* description);
void _FldrButton();
void _InfoButton(int eventCode);
void editorAdjustSkill(int a1);
void characterEditorToggleTaggedSkill(int skill);
void characterEditorWindowRenderTraits();
void characterEditorToggleOptionalTrait(int trait);
void editorRenderKarma();
int _editor_save(File* stream);
int _editor_load(File* stream);
void _editor_reset();
int _UpdateLevel();
void _RedrwDPrks();
int editorSelectPerk();
int _InputPDLoop(int count, void (*refreshProc)());
int _ListDPerks();
void _RedrwDMPrk();
bool editorHandleMutate();
void _RedrwDMTagSkl();
bool editorHandleTag();
void _ListNewTagSkills();
int _ListMyTraits(int a1);
int _name_sort_comp(const void* a1, const void* a2);
int _DrawCard2(int frmId, const char* name, const char* rank, char* description);
void _pop_perks();
int _is_supper_bonus();
int _folder_init();
void _folder_scroll(int direction);
void _folder_clear();
int _folder_print_seperator(const char* string);
bool _folder_print_line(const char* string);
bool editorDrawKillsEntry(const char* name, int kills);
int karmaInit();
void karmaFree();
int karmaEntryCompare(const void* a1, const void* a2);
int genericReputationInit();
void genericReputationFree();
int genericReputationCompare(const void* a1, const void* a2);

#endif /* CHARACTER_EDITOR_H */
