#include "character_selector.h"

#include "character_editor.h"
#include "color.h"
#include "core.h"
#include "critter.h"
#include "db.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "options.h"
#include "palette.h"
#include "platform_compat.h"
#include "proto.h"
#include "skill.h"
#include "stat.h"
#include "text_font.h"
#include "trait.h"
#include "window_manager.h"

#include <stdio.h>
#include <string.h>

// 0x51C84C
int gCurrentPremadeCharacter = PREMADE_CHARACTER_NARG;

// 0x51C850
PremadeCharacterDescription gPremadeCharacterDescriptions[PREMADE_CHARACTER_COUNT] = {
    { "premade\\combat", 201, "VID 208-197-88-125" },
    { "premade\\stealth", 202, "VID 208-206-49-229" },
    { "premade\\diplomat", 203, "VID 208-206-49-227" },
};

// 0x51C8D4
const int gPremadeCharacterCount = PREMADE_CHARACTER_COUNT;

// 0x51C7F8
int gCharacterSelectorWindow = -1;

// 0x51C7FC
unsigned char* gCharacterSelectorWindowBuffer = NULL;

// 0x51C800
unsigned char* gCharacterSelectorBackground = NULL;

// 0x51C804
int gCharacterSelectorWindowPreviousButton = -1;

// 0x51C808
CacheEntry* gCharacterSelectorWindowPreviousButtonUpFrmHandle = NULL;

// 0x51C80C
CacheEntry* gCharacterSelectorWindowPreviousButtonDownFrmHandle = NULL;

// 0x51C810
int gCharacterSelectorWindowNextButton = -1;

// 0x51C814
CacheEntry* gCharacterSelectorWindowNextButtonUpFrmHandle = NULL;

// 0x51C818
CacheEntry* gCharacterSelectorWindowNextButtonDownFrmHandle = NULL;

// 0x51C81C
int gCharacterSelectorWindowTakeButton = -1;

// 0x51C820
CacheEntry* gCharacterSelectorWindowTakeButtonUpFrmHandle = NULL;

// 0x51C824
CacheEntry* gCharacterSelectorWindowTakeButtonDownFrmHandle = NULL;

// 0x51C828
int gCharacterSelectorWindowModifyButton = -1;

// 0x51C82C
CacheEntry* gCharacterSelectorWindowModifyButtonUpFrmHandle = NULL;

// 0x51C830
CacheEntry* gCharacterSelectorWindowModifyButtonDownFrmHandle = NULL;

// 0x51C834
int gCharacterSelectorWindowCreateButton = -1;

// 0x51C838
CacheEntry* gCharacterSelectorWindowCreateButtonUpFrmHandle = NULL;

// 0x51C83C
CacheEntry* gCharacterSelectorWindowCreateButtonDownFrmHandle = NULL;

// 0x51C840
int gCharacterSelectorWindowBackButton = -1;

// 0x51C844
CacheEntry* gCharacterSelectorWindowBackButtonUpFrmHandle = NULL;

// 0x51C848
CacheEntry* gCharacterSelectorWindowBackButtonDownFrmHandle = NULL;

// 0x667764
unsigned char* gCharacterSelectorWindowTakeButtonUpFrmData;

// 0x667768
unsigned char* gCharacterSelectorWindowModifyButtonDownFrmData;

// 0x66776C
unsigned char* gCharacterSelectorWindowBackButtonUpFrmData;

// 0x667770
unsigned char* gCharacterSelectorWindowCreateButtonUpFrmData;

// 0x667774
unsigned char* gCharacterSelectorWindowModifyButtonUpFrmData;

// 0x667778
unsigned char* gCharacterSelectorWindowBackButtonDownFrmData;

// 0x66777C
unsigned char* gCharacterSelectorWindowCreateButtonDownFrmData;

// 0x667780
unsigned char* gCharacterSelectorWindowTakeButtonDownFrmData;

// 0x667784
unsigned char* gCharacterSelectorWindowNextButtonDownFrmData;

// 0x667788
unsigned char* gCharacterSelectorWindowNextButtonUpFrmData;

// 0x66778C
unsigned char* gCharacterSelectorWindowPreviousButtonUpFrmData;

// 0x667790
unsigned char* gCharacterSelectorWindowPreviousButtonDownFrmData;

// 0x4A71D0
int characterSelectorOpen()
{
    if (!characterSelectorWindowInit()) {
        return 0;
    }

    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        mouseShowCursor();
    }

    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);

    int rc = 0;
    bool done = false;
    while (!done) {
        if (_game_user_wants_to_quit != 0) {
            break;
        }

        int keyCode = _get_input();

        switch (keyCode) {
        case KEY_MINUS:
        case KEY_UNDERSCORE:
            brightnessDecrease();
            break;
        case KEY_EQUAL:
        case KEY_PLUS:
            brightnessIncrease();
            break;
        case KEY_UPPERCASE_B:
        case KEY_LOWERCASE_B:
        case KEY_ESCAPE:
            rc = 3;
            done = true;
            break;
        case KEY_UPPERCASE_C:
        case KEY_LOWERCASE_C:
            _ResetPlayer();
            if (_editor_design(1) == 0) {
                rc = 2;
                done = true;
            }

            break;
        case KEY_UPPERCASE_M:
        case KEY_LOWERCASE_M:
            if (!_editor_design(1)) {
                rc = 2;
                done = true;
            }

            break;
        case KEY_UPPERCASE_T:
        case KEY_LOWERCASE_T:
            rc = 2;
            done = true;

            break;
        case KEY_F10:
            showQuitConfirmationDialog();
            break;
        case KEY_ARROW_LEFT:
            soundPlayFile("ib2p1xx1");
            // FALLTHROUGH
        case 500:
            gCurrentPremadeCharacter -= 1;
            if (gCurrentPremadeCharacter < 0) {
                gCurrentPremadeCharacter = gPremadeCharacterCount - 1;
            }

            characterSelectorWindowRefresh();
            break;
        case KEY_ARROW_RIGHT:
            soundPlayFile("ib2p1xx1");
            // FALLTHROUGH
        case 501:
            gCurrentPremadeCharacter += 1;
            if (gCurrentPremadeCharacter >= gPremadeCharacterCount) {
                gCurrentPremadeCharacter = 0;
            }

            characterSelectorWindowRefresh();
            break;
        }
    }

    paletteFadeTo(gPaletteBlack);
    characterSelectorWindowFree();

    if (cursorWasHidden) {
        mouseHideCursor();
    }

    return rc;
}

// 0x4A7468
bool characterSelectorWindowInit()
{
    int backgroundFid;
    unsigned char* backgroundFrmData;

    if (gCharacterSelectorWindow != -1) {
        return false;
    }

    int characterSelectorWindowX = (screenGetWidth() - CS_WINDOW_WIDTH) / 2;
    int characterSelectorWindowY = (screenGetHeight() - CS_WINDOW_HEIGHT) / 2;
    gCharacterSelectorWindow = windowCreate(characterSelectorWindowX, characterSelectorWindowY, CS_WINDOW_WIDTH, CS_WINDOW_HEIGHT, _colorTable[0], 0);
    if (gCharacterSelectorWindow == -1) {
        goto err;
    }

    gCharacterSelectorWindowBuffer = windowGetBuffer(gCharacterSelectorWindow);
    if (gCharacterSelectorWindowBuffer == NULL) {
        goto err;
    }

    CacheEntry* backgroundFrmHandle;
    backgroundFid = buildFid(6, 174, 0, 0, 0);
    backgroundFrmData = artLockFrameData(backgroundFid, 0, 0, &backgroundFrmHandle);
    if (backgroundFrmData == NULL) {
        goto err;
    }

    blitBufferToBuffer(backgroundFrmData,
        CS_WINDOW_WIDTH,
        CS_WINDOW_HEIGHT,
        CS_WINDOW_WIDTH,
        gCharacterSelectorWindowBuffer,
        CS_WINDOW_WIDTH);

    gCharacterSelectorBackground = (unsigned char*)internal_malloc(CS_WINDOW_BACKGROUND_WIDTH * CS_WINDOW_BACKGROUND_HEIGHT);
    if (gCharacterSelectorBackground == NULL)
        goto err;

    blitBufferToBuffer(backgroundFrmData + CS_WINDOW_WIDTH * CS_WINDOW_BACKGROUND_Y + CS_WINDOW_BACKGROUND_X,
        CS_WINDOW_BACKGROUND_WIDTH,
        CS_WINDOW_BACKGROUND_HEIGHT,
        CS_WINDOW_WIDTH,
        gCharacterSelectorBackground,
        CS_WINDOW_BACKGROUND_WIDTH);

    artUnlock(backgroundFrmHandle);

    int fid;

    // Setup "Previous" button.
    fid = buildFid(6, 122, 0, 0, 0);
    gCharacterSelectorWindowPreviousButtonUpFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowPreviousButtonUpFrmHandle);
    if (gCharacterSelectorWindowPreviousButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 123, 0, 0, 0);
    gCharacterSelectorWindowPreviousButtonDownFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowPreviousButtonDownFrmHandle);
    if (gCharacterSelectorWindowPreviousButtonDownFrmData == NULL) {
        goto err;
    }

    gCharacterSelectorWindowPreviousButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_PREVIOUS_BUTTON_X,
        CS_WINDOW_PREVIOUS_BUTTON_Y,
        20,
        18,
        -1,
        -1,
        -1,
        500,
        gCharacterSelectorWindowPreviousButtonUpFrmData,
        gCharacterSelectorWindowPreviousButtonDownFrmData,
        NULL,
        0);
    if (gCharacterSelectorWindowPreviousButton == -1) {
        goto err;
    }

    buttonSetCallbacks(gCharacterSelectorWindowPreviousButton, _gsound_med_butt_press, _gsound_med_butt_release);

    // Setup "Next" button.
    fid = buildFid(6, 124, 0, 0, 0);
    gCharacterSelectorWindowNextButtonUpFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowNextButtonUpFrmHandle);
    if (gCharacterSelectorWindowNextButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 125, 0, 0, 0);
    gCharacterSelectorWindowNextButtonDownFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowNextButtonDownFrmHandle);
    if (gCharacterSelectorWindowNextButtonDownFrmData == NULL) {
        goto err;
    }

    gCharacterSelectorWindowNextButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_NEXT_BUTTON_X,
        CS_WINDOW_NEXT_BUTTON_Y,
        20,
        18,
        -1,
        -1,
        -1,
        501,
        gCharacterSelectorWindowNextButtonUpFrmData,
        gCharacterSelectorWindowNextButtonDownFrmData,
        NULL,
        0);
    if (gCharacterSelectorWindowNextButton == -1) {
        goto err;
    }

    buttonSetCallbacks(gCharacterSelectorWindowNextButton, _gsound_med_butt_press, _gsound_med_butt_release);

    // Setup "Take" button.
    fid = buildFid(6, 8, 0, 0, 0);
    gCharacterSelectorWindowTakeButtonUpFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowTakeButtonUpFrmHandle);
    if (gCharacterSelectorWindowTakeButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 9, 0, 0, 0);
    gCharacterSelectorWindowTakeButtonDownFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowTakeButtonDownFrmHandle);
    if (gCharacterSelectorWindowTakeButtonDownFrmData == NULL) {
        goto err;
    }

    gCharacterSelectorWindowTakeButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_TAKE_BUTTON_X,
        CS_WINDOW_TAKE_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_T,
        gCharacterSelectorWindowTakeButtonUpFrmData,
        gCharacterSelectorWindowTakeButtonDownFrmData,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowTakeButton == -1) {
        goto err;
    }

    buttonSetCallbacks(gCharacterSelectorWindowTakeButton, _gsound_red_butt_press, _gsound_red_butt_release);

    // Setup "Modify" button.
    fid = buildFid(6, 8, 0, 0, 0);
    gCharacterSelectorWindowModifyButtonUpFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowModifyButtonUpFrmHandle);
    if (gCharacterSelectorWindowModifyButtonUpFrmData == NULL)
        goto err;

    fid = buildFid(6, 9, 0, 0, 0);
    gCharacterSelectorWindowModifyButtonDownFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowModifyButtonDownFrmHandle);
    if (gCharacterSelectorWindowModifyButtonDownFrmData == NULL) {
        goto err;
    }

    gCharacterSelectorWindowModifyButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_MODIFY_BUTTON_X,
        CS_WINDOW_MODIFY_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_M,
        gCharacterSelectorWindowModifyButtonUpFrmData,
        gCharacterSelectorWindowModifyButtonDownFrmData,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowModifyButton == -1) {
        goto err;
    }

    buttonSetCallbacks(gCharacterSelectorWindowModifyButton, _gsound_red_butt_press, _gsound_red_butt_release);

    // Setup "Create" button.
    fid = buildFid(6, 8, 0, 0, 0);
    gCharacterSelectorWindowCreateButtonUpFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowCreateButtonUpFrmHandle);
    if (gCharacterSelectorWindowCreateButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 9, 0, 0, 0);
    gCharacterSelectorWindowCreateButtonDownFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowCreateButtonDownFrmHandle);
    if (gCharacterSelectorWindowCreateButtonDownFrmData == NULL) {
        goto err;
    }

    gCharacterSelectorWindowCreateButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_CREATE_BUTTON_X,
        CS_WINDOW_CREATE_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_C,
        gCharacterSelectorWindowCreateButtonUpFrmData,
        gCharacterSelectorWindowCreateButtonDownFrmData,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowCreateButton == -1) {
        goto err;
    }

    buttonSetCallbacks(gCharacterSelectorWindowCreateButton, _gsound_red_butt_press, _gsound_red_butt_release);

    // Setup "Back" button.
    fid = buildFid(6, 8, 0, 0, 0);
    gCharacterSelectorWindowBackButtonUpFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowBackButtonUpFrmHandle);
    if (gCharacterSelectorWindowBackButtonUpFrmData == NULL) {
        goto err;
    }

    fid = buildFid(6, 9, 0, 0, 0);
    gCharacterSelectorWindowBackButtonDownFrmData = artLockFrameData(fid, 0, 0, &gCharacterSelectorWindowBackButtonDownFrmHandle);
    if (gCharacterSelectorWindowBackButtonDownFrmData == NULL) {
        goto err;
    }

    gCharacterSelectorWindowBackButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_BACK_BUTTON_X,
        CS_WINDOW_BACK_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        gCharacterSelectorWindowBackButtonUpFrmData,
        gCharacterSelectorWindowBackButtonDownFrmData,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowBackButton == -1) {
        goto err;
    }

    buttonSetCallbacks(gCharacterSelectorWindowBackButton, _gsound_red_butt_press, _gsound_red_butt_release);

    gCurrentPremadeCharacter = PREMADE_CHARACTER_NARG;

    windowRefresh(gCharacterSelectorWindow);

    if (!characterSelectorWindowRefresh()) {
        goto err;
    }

    return true;

err:

    characterSelectorWindowFree();

    return false;
}

// 0x4A7AD4
void characterSelectorWindowFree()
{
    if (gCharacterSelectorWindow == -1) {
        return;
    }

    if (gCharacterSelectorWindowPreviousButton != -1) {
        buttonDestroy(gCharacterSelectorWindowPreviousButton);
        gCharacterSelectorWindowPreviousButton = -1;
    }

    if (gCharacterSelectorWindowPreviousButtonDownFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowPreviousButtonDownFrmHandle);
        gCharacterSelectorWindowPreviousButtonDownFrmHandle = NULL;
        gCharacterSelectorWindowPreviousButtonDownFrmData = NULL;
    }

    if (gCharacterSelectorWindowPreviousButtonUpFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowPreviousButtonUpFrmHandle);
        gCharacterSelectorWindowPreviousButtonUpFrmHandle = NULL;
        gCharacterSelectorWindowPreviousButtonUpFrmData = NULL;
    }

    if (gCharacterSelectorWindowNextButton != -1) {
        buttonDestroy(gCharacterSelectorWindowNextButton);
        gCharacterSelectorWindowNextButton = -1;
    }

    if (gCharacterSelectorWindowNextButtonDownFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowNextButtonDownFrmHandle);
        gCharacterSelectorWindowNextButtonDownFrmHandle = NULL;
        gCharacterSelectorWindowNextButtonDownFrmData = NULL;
    }

    if (gCharacterSelectorWindowNextButtonUpFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowNextButtonUpFrmHandle);
        gCharacterSelectorWindowNextButtonUpFrmHandle = NULL;
        gCharacterSelectorWindowNextButtonUpFrmData = NULL;
    }

    if (gCharacterSelectorWindowTakeButton != -1) {
        buttonDestroy(gCharacterSelectorWindowTakeButton);
        gCharacterSelectorWindowTakeButton = -1;
    }

    if (gCharacterSelectorWindowTakeButtonDownFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowTakeButtonDownFrmHandle);
        gCharacterSelectorWindowTakeButtonDownFrmHandle = NULL;
        gCharacterSelectorWindowTakeButtonDownFrmData = NULL;
    }

    if (gCharacterSelectorWindowTakeButtonUpFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowTakeButtonUpFrmHandle);
        gCharacterSelectorWindowTakeButtonUpFrmHandle = NULL;
        gCharacterSelectorWindowTakeButtonUpFrmData = NULL;
    }

    if (gCharacterSelectorWindowModifyButton != -1) {
        buttonDestroy(gCharacterSelectorWindowModifyButton);
        gCharacterSelectorWindowModifyButton = -1;
    }

    if (gCharacterSelectorWindowModifyButtonDownFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowModifyButtonDownFrmHandle);
        gCharacterSelectorWindowModifyButtonDownFrmHandle = NULL;
        gCharacterSelectorWindowModifyButtonDownFrmData = NULL;
    }

    if (gCharacterSelectorWindowModifyButtonUpFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowModifyButtonUpFrmHandle);
        gCharacterSelectorWindowModifyButtonUpFrmHandle = NULL;
        gCharacterSelectorWindowModifyButtonUpFrmData = NULL;
    }

    if (gCharacterSelectorWindowCreateButton != -1) {
        buttonDestroy(gCharacterSelectorWindowCreateButton);
        gCharacterSelectorWindowCreateButton = -1;
    }

    if (gCharacterSelectorWindowCreateButtonDownFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowCreateButtonDownFrmHandle);
        gCharacterSelectorWindowCreateButtonDownFrmHandle = NULL;
        gCharacterSelectorWindowCreateButtonDownFrmData = NULL;
    }

    if (gCharacterSelectorWindowCreateButtonUpFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowCreateButtonUpFrmHandle);
        gCharacterSelectorWindowCreateButtonUpFrmHandle = NULL;
        gCharacterSelectorWindowCreateButtonUpFrmData = NULL;
    }

    if (gCharacterSelectorWindowBackButton != -1) {
        buttonDestroy(gCharacterSelectorWindowBackButton);
        gCharacterSelectorWindowBackButton = -1;
    }

    if (gCharacterSelectorWindowBackButtonDownFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowBackButtonDownFrmHandle);
        gCharacterSelectorWindowBackButtonDownFrmHandle = NULL;
        gCharacterSelectorWindowBackButtonDownFrmData = NULL;
    }

    if (gCharacterSelectorWindowBackButtonUpFrmData != NULL) {
        artUnlock(gCharacterSelectorWindowBackButtonUpFrmHandle);
        gCharacterSelectorWindowBackButtonUpFrmHandle = NULL;
        gCharacterSelectorWindowBackButtonUpFrmData = NULL;
    }

    if (gCharacterSelectorBackground != NULL) {
        internal_free(gCharacterSelectorBackground);
        gCharacterSelectorBackground = NULL;
    }

    windowDestroy(gCharacterSelectorWindow);
    gCharacterSelectorWindow = -1;
}

// 0x4A7D58
bool characterSelectorWindowRefresh()
{
    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s.gcd", gPremadeCharacterDescriptions[gCurrentPremadeCharacter].fileName);
    if (_proto_dude_init(path) == -1) {
        debugPrint("\n ** Error in dude init! **\n");
        return false;
    }

    blitBufferToBuffer(gCharacterSelectorBackground,
        CS_WINDOW_BACKGROUND_WIDTH,
        CS_WINDOW_BACKGROUND_HEIGHT,
        CS_WINDOW_BACKGROUND_WIDTH,
        gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * CS_WINDOW_BACKGROUND_Y + CS_WINDOW_BACKGROUND_X,
        CS_WINDOW_WIDTH);

    bool success = false;
    if (characterSelectorWindowRenderFace()) {
        if (characterSelectorWindowRenderStats()) {
            success = characterSelectorWindowRenderBio();
        }
    }

    windowRefresh(gCharacterSelectorWindow);

    return success;
}

// 0x4A7E08
bool characterSelectorWindowRenderFace()
{
    bool success = false;

    CacheEntry* faceFrmHandle;
    int faceFid = buildFid(6, gPremadeCharacterDescriptions[gCurrentPremadeCharacter].face, 0, 0, 0);
    Art* frm = artLock(faceFid, &faceFrmHandle);
    if (frm != NULL) {
        unsigned char* data = artGetFrameData(frm, 0, 0);
        if (data != NULL) {
            int width = artGetWidth(frm, 0, 0);
            int height = artGetHeight(frm, 0, 0);
            blitBufferToBufferTrans(data, width, height, width, (gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * 23 + 27), CS_WINDOW_WIDTH);
            success = true;
        }
        artUnlock(faceFrmHandle);
    }

    return success;
}

// 0x4A7EA8
bool characterSelectorWindowRenderStats()
{
    char* str;
    char text[260];
    int length;
    int value;
    MessageListItem messageListItem;

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    fontGetCharacterWidth(0x20);

    int vh = fontGetLineHeight();
    int y = 40;

    // NAME
    str = objectGetName(gDude);
    strcpy(text, str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_NAME_MID_X - (length / 2), text, 160, CS_WINDOW_WIDTH, _colorTable[992]);

    // STRENGTH
    y += vh + vh + vh;

    value = critterGetStat(gDude, STAT_STRENGTH);
    str = statGetName(STAT_STRENGTH);

    sprintf(text, "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    sprintf(text, "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // PERCEPTION
    y += vh;

    value = critterGetStat(gDude, STAT_PERCEPTION);
    str = statGetName(STAT_PERCEPTION);

    sprintf(text, "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    sprintf(text, "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // ENDURANCE
    y += vh;

    value = critterGetStat(gDude, STAT_ENDURANCE);
    str = statGetName(STAT_PERCEPTION);

    sprintf(text, "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    sprintf(text, "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // CHARISMA
    y += vh;

    value = critterGetStat(gDude, STAT_CHARISMA);
    str = statGetName(STAT_CHARISMA);

    sprintf(text, "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    sprintf(text, "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // INTELLIGENCE
    y += vh;

    value = critterGetStat(gDude, STAT_INTELLIGENCE);
    str = statGetName(STAT_INTELLIGENCE);

    sprintf(text, "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    sprintf(text, "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // AGILITY
    y += vh;

    value = critterGetStat(gDude, STAT_AGILITY);
    str = statGetName(STAT_AGILITY);

    sprintf(text, "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    sprintf(text, "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // LUCK
    y += vh;

    value = critterGetStat(gDude, STAT_LUCK);
    str = statGetName(STAT_LUCK);

    sprintf(text, "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    sprintf(text, "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    y += vh; // blank line

    // HIT POINTS
    y += vh;

    messageListItem.num = 16;
    text[0] = '\0';
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        strcpy(text, messageListItem.text);
    }

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS);
    sprintf(text, " %d/%d", critterGetHitPoints(gDude), value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // ARMOR CLASS
    y += vh;

    str = statGetName(STAT_ARMOR_CLASS);
    strcpy(text, str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_ARMOR_CLASS);
    sprintf(text, " %d", value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // ACTION POINTS
    y += vh;

    messageListItem.num = 15;
    text[0] = '\0';
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        strcpy(text, messageListItem.text);
    }

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_MAXIMUM_ACTION_POINTS);
    sprintf(text, " %d", value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // MELEE DAMAGE
    y += vh;

    str = statGetName(STAT_ARMOR_CLASS);
    strcpy(text, str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_ARMOR_CLASS);
    sprintf(text, " %d", value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    y += vh; // blank line

    // SKILLS
    int skills[DEFAULT_TAGGED_SKILLS];
    skillsGetTagged(skills, DEFAULT_TAGGED_SKILLS);

    for (int index = 0; index < DEFAULT_TAGGED_SKILLS; index++) {
        y += vh;

        str = skillGetName(skills[index]);
        strcpy(text, str);

        length = fontGetStringWidth(text);
        fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

        value = skillGetValue(gDude, skills[index]);
        sprintf(text, " %d%%", value);

        length = fontGetStringWidth(text);
        fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);
    }

    // TRAITS
    int traits[TRAITS_MAX_SELECTED_COUNT];
    traitsGetSelected(&(traits[0]), &(traits[1]));

    for (int index = 0; index < TRAITS_MAX_SELECTED_COUNT; index++) {
        y += vh;

        str = traitGetName(traits[index]);
        strcpy(text, str);

        length = fontGetStringWidth(text);
        fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);
    }

    fontSetCurrent(oldFont);

    return true;
}

// 0x4A8AE4
bool characterSelectorWindowRenderBio()
{
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s.bio", gPremadeCharacterDescriptions[gCurrentPremadeCharacter].fileName);

    File* stream = fileOpen(path, "rt");
    if (stream != NULL) {
        int y = 40;
        int lineHeight = fontGetLineHeight();

        char string[256];
        while (fileReadString(string, 256, stream) && y < 260) {
            fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_BIO_X, string, CS_WINDOW_WIDTH - CS_WINDOW_BIO_X, CS_WINDOW_WIDTH, _colorTable[992]);
            y += lineHeight;
        }

        fileClose(stream);
    }

    fontSetCurrent(oldFont);

    return true;
}
