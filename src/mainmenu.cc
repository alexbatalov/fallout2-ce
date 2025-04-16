#include "mainmenu.h"

#include <ctype.h>

#include "art.h"
#include "color.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "palette.h"
#include "preferences.h"
#include "sfall_config.h"
#include "svga.h"
#include "text_font.h"
#include "version.h"
#include "window_manager.h"

namespace fallout {

#define MAIN_MENU_WINDOW_WIDTH 640
#define MAIN_MENU_WINDOW_HEIGHT 480

typedef enum MainMenuButton {
    MAIN_MENU_BUTTON_INTRO,
    MAIN_MENU_BUTTON_NEW_GAME,
    MAIN_MENU_BUTTON_LOAD_GAME,
    MAIN_MENU_BUTTON_OPTIONS,
    MAIN_MENU_BUTTON_CREDITS,
    MAIN_MENU_BUTTON_EXIT,
    MAIN_MENU_BUTTON_COUNT,
} MainMenuButton;

static int main_menu_fatal_error();
static void main_menu_play_sound(const char* fileName);

// 0x5194F0
static int gMainMenuWindow = -1;

// 0x5194F4
static unsigned char* gMainMenuWindowBuffer = nullptr;

// 0x519504
static bool _in_main_menu = false;

// 0x519508
static bool gMainMenuWindowInitialized = false;

// 0x51950C
static unsigned int gMainMenuScreensaverDelay = 120000;

// 0x519510
static const int gMainMenuButtonKeyBindings[MAIN_MENU_BUTTON_COUNT] = {
    KEY_LOWERCASE_I, // intro
    KEY_LOWERCASE_N, // new game
    KEY_LOWERCASE_L, // load game
    KEY_LOWERCASE_O, // options
    KEY_LOWERCASE_C, // credits
    KEY_LOWERCASE_E, // exit
};

// 0x519528
static const int _return_values[MAIN_MENU_BUTTON_COUNT] = {
    MAIN_MENU_INTRO,
    MAIN_MENU_NEW_GAME,
    MAIN_MENU_LOAD_GAME,
    MAIN_MENU_OPTIONS,
    MAIN_MENU_CREDITS,
    MAIN_MENU_EXIT,
};

// 0x614840
static int gMainMenuButtons[MAIN_MENU_BUTTON_COUNT];

// 0x614858
static bool gMainMenuWindowHidden;

static FrmImage _mainMenuBackgroundFrmImage;
static FrmImage _mainMenuButtonNormalFrmImage;
static FrmImage _mainMenuButtonPressedFrmImage;

// 0x481650
int mainMenuWindowInit()
{
    int fid;
    MessageListItem msg;
    int len;

    if (gMainMenuWindowInitialized) {
        return 0;
    }

    colorPaletteLoad("color.pal");

    // Get the screen/window size
    int screenWidth = screenGetWidth();
    int screenHeight = screenGetHeight();

    // Figure out how we want to scale the main menu (from .ini setting)
    int menuStretchMode = 0;
    Config config;
    if (configInit(&config)) {
        if (configRead(&config, "f2_res.ini", false)) {
            configGetInt(&config, "STATIC_SCREENS", "MAIN_MENU_SIZE", &menuStretchMode);
        }
        configFree(&config);
    }

    // Original menu dimensions (classic 640x480)
    int originalWidth = MAIN_MENU_WINDOW_WIDTH;
    int originalHeight = MAIN_MENU_WINDOW_HEIGHT;

    // These will be the possibly scaled versions of the above
    int scaledWidth = originalWidth;
    int scaledHeight = originalHeight;

    // Figure out how to stretch the menu depending on resolution + settings
    if (menuStretchMode != 0 || screenWidth < originalWidth || screenHeight < originalHeight) {
        if (menuStretchMode == 2) {
            // Fullscreen stretch
            scaledWidth = screenWidth;
            scaledHeight = screenHeight;
        } else {
            // Preserve aspect ratio
            if (screenHeight * originalWidth >= screenWidth * originalHeight) {
                scaledWidth = screenWidth;
                scaledHeight = screenWidth * originalHeight / originalWidth;
            } else {
                scaledWidth = screenHeight * originalWidth / originalHeight;
                scaledHeight = screenHeight;
            }
        }
    }

    // Center the menu window on screen
    int mainMenuWindowX = (screenWidth - scaledWidth) / 2;
    int mainMenuWindowY = (screenHeight - scaledHeight) / 2;

    gMainMenuWindow = windowCreate(mainMenuWindowX,
        mainMenuWindowY,
        scaledWidth,
        scaledHeight,
        0,
        WINDOW_HIDDEN | WINDOW_MOVE_ON_TOP);
    if (gMainMenuWindow == -1) {
        return main_menu_fatal_error();
    }

    gMainMenuWindowBuffer = windowGetBuffer(gMainMenuWindow);

    // Load the background image
    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 140, 0, 0, 0);
    if (!_mainMenuBackgroundFrmImage.lock(backgroundFid)) {
        return main_menu_fatal_error();
    }

    unsigned char* backgroundData = _mainMenuBackgroundFrmImage.getData();

    // Clone the background so we can draw text on it (before scaling)
    unsigned char* compositeBuffer = reinterpret_cast<unsigned char*>(SDL_malloc(originalWidth * originalHeight));
    if (!compositeBuffer) {
        return main_menu_fatal_error();
    }

    memcpy(compositeBuffer, backgroundData, originalWidth * originalHeight);

    int oldFont = fontGetCurrent();

    // Draw the "Credits" text (bottom left)
    fontSetCurrent(100);
    int fontColor = _colorTable[21091], sfallOverride = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_FONT_COLOR_KEY, &sfallOverride);
    if (sfallOverride && !(sfallOverride & 0x010000))
        fontColor = sfallOverride & 0xFF;

    int offsetX = 0, offsetY = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_X_KEY, &offsetX);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_Y_KEY, &offsetY);

    msg.num = 20;
    if (messageListGetItem(&gMiscMessageList, &msg)) {
        fontDrawText(
            compositeBuffer + (offsetY + originalHeight - 20) * originalWidth + offsetX + 15,
            msg.text,
            originalWidth - offsetX - 15,
            originalWidth,
            fontColor | 0x06000000);
    }

    // Draw version number (bottom right)
    if (sfallOverride)
        fontColor = sfallOverride;

    char version[VERSION_MAX];
    versionGetVersion(version, sizeof(version));
    len = fontGetStringWidth(version);
    fontDrawText(
        compositeBuffer + (originalHeight - 20) * originalWidth + (originalWidth - 25 - len),
        version,
        originalWidth - (originalWidth - 25 - len),
        originalWidth,
        fontColor | 0x06000000);

    // Draw Main Menu labels for buttons (Start, Load, Exit...)
    fontSetCurrent(104);
    fontColor = _colorTable[21091];
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_BIG_FONT_COLOR_KEY, &sfallOverride);
    if (sfallOverride)
        fontColor = sfallOverride & 0xFF;

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        msg.num = 9 + index;
        if (messageListGetItem(&gMiscMessageList, &msg)) {
            len = fontGetStringWidth(msg.text);
            int textX = 126 - (len / 2);
            int textY = 20 + index * 42 - index;
            fontDrawText(
                compositeBuffer + textY * originalWidth + textX,
                msg.text,
                originalWidth - textX,
                originalWidth,
                fontColor);
        }
    }

    // Stretch and blit the text+background into our menu window
    if (scaledWidth != originalWidth || scaledHeight != originalHeight) {
        unsigned char* stretched = reinterpret_cast<unsigned char*>(SDL_malloc(scaledWidth * scaledHeight));
        if (!stretched) {
            SDL_free(compositeBuffer);
            return main_menu_fatal_error();
        }

        blitBufferToBufferStretch(
            compositeBuffer, originalWidth, originalHeight, originalWidth,
            stretched, scaledWidth, scaledHeight, scaledWidth);

        blitBufferToBuffer(stretched, scaledWidth, scaledHeight, scaledWidth, gMainMenuWindowBuffer, scaledWidth);
        SDL_free(stretched);
    } else {
        blitBufferToBuffer(compositeBuffer, originalWidth, originalHeight, originalWidth, gMainMenuWindowBuffer, scaledWidth);
    }

    SDL_free(compositeBuffer);
    _mainMenuBackgroundFrmImage.unlock();
    fontSetCurrent(oldFont);

    // Load and scale button graphics (menuup.frm / menudown.frm)
    fid = buildFid(OBJ_TYPE_INTERFACE, 299, 0, 0, 0);
    if (!_mainMenuButtonNormalFrmImage.lock(fid)) {
        return main_menu_fatal_error();
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 300, 0, 0, 0);
    if (!_mainMenuButtonPressedFrmImage.lock(fid)) {
        return main_menu_fatal_error();
    }

    // Base button dimensions
    int buttonBaseWidth = 26;
    int buttonBaseHeight = 26;

    // We'll scale based on Y to keep the buttons round - works best for all res
    float scaleX = static_cast<float>(scaledWidth) / originalWidth;
    float scaleY = static_cast<float>(scaledHeight) / originalHeight;
    float buttonScale = scaleY;

    int buttonWidth = static_cast<int>(buttonBaseWidth * buttonScale);
    int buttonHeight = static_cast<int>(buttonBaseHeight * buttonScale);

    unsigned char* scaledNormal = reinterpret_cast<unsigned char*>(SDL_malloc(buttonWidth * buttonHeight));
    unsigned char* scaledPressed = reinterpret_cast<unsigned char*>(SDL_malloc(buttonWidth * buttonHeight));
    if (!scaledNormal || !scaledPressed) {
        return main_menu_fatal_error();
    }

    // Stretch the button images
    blitBufferToBufferStretch(
        _mainMenuButtonNormalFrmImage.getData(), buttonBaseWidth, buttonBaseHeight, buttonBaseWidth,
        scaledNormal, buttonWidth, buttonHeight, buttonWidth);
    blitBufferToBufferStretch(
        _mainMenuButtonPressedFrmImage.getData(), buttonBaseWidth, buttonBaseHeight, buttonBaseWidth,
        scaledPressed, buttonWidth, buttonHeight, buttonWidth);

    // Fix the edge pixels to avoid glitches
    for (int y = 0; y < buttonHeight; y++) {
        scaledPressed[y * buttonWidth + (buttonWidth - 1)] = scaledPressed[y * buttonWidth + (buttonWidth - 2)];
        scaledNormal[y * buttonWidth + (buttonWidth - 1)] = scaledNormal[y * buttonWidth + (buttonWidth - 2)];
    }
    for (int x = 0; x < buttonWidth; x++) {
        scaledPressed[(buttonHeight - 1) * buttonWidth + x] = scaledPressed[(buttonHeight - 2) * buttonWidth + x];
        scaledNormal[(buttonHeight - 1) * buttonWidth + x] = scaledNormal[(buttonHeight - 2) * buttonWidth + x];
    }

    // Apply any user-configured button offsets from Sfall (for button position tweaking)
    offsetX = offsetY = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_X_KEY, &offsetX);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_Y_KEY, &offsetY);
    offsetX = static_cast<int>(offsetX * scaleX);
    offsetY = static_cast<int>(offsetY * scaleY);

    // Create the actual buttons
    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = buttonCreate(
            gMainMenuWindow,
            offsetX + static_cast<int>(30 * scaleX),
            offsetY + static_cast<int>((19 + index * 42 - index) * scaleY),
            buttonWidth,
            buttonHeight,
            -1,
            -1,
            1111,
            gMainMenuButtonKeyBindings[index],
            scaledNormal,
            scaledPressed,
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (gMainMenuButtons[index] == -1) {
            return main_menu_fatal_error();
        }

        buttonSetMask(gMainMenuButtons[index], scaledNormal);
    }

    gMainMenuWindowInitialized = true;
    gMainMenuWindowHidden = true;

    return 0;
}

// 0x481968
void mainMenuWindowFree()
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        // FIXME: Why it tries to free only invalid buttons?
        if (gMainMenuButtons[index] == -1) {
            buttonDestroy(gMainMenuButtons[index]);
        }
    }

    _mainMenuButtonPressedFrmImage.unlock();
    _mainMenuButtonNormalFrmImage.unlock();

    if (gMainMenuWindow != -1) {
        windowDestroy(gMainMenuWindow);
    }

    gMainMenuWindowInitialized = false;
}

// 0x481A00
void mainMenuWindowHide(bool animate)
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    if (gMainMenuWindowHidden) {
        return;
    }

    soundContinueAll();

    if (animate) {
        paletteFadeTo(gPaletteBlack);
        soundContinueAll();
    }

    windowHide(gMainMenuWindow);

    gMainMenuWindowHidden = true;
}

// 0x481A48
void mainMenuWindowUnhide(bool animate)
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    if (!gMainMenuWindowHidden) {
        return;
    }

    windowShow(gMainMenuWindow);

    if (animate) {
        colorPaletteLoad("color.pal");
        paletteFadeTo(_cmap);
    }

    gMainMenuWindowHidden = false;
}

// 0x481AA8
int _main_menu_is_enabled()
{
    return 1;
}

// 0x481AEC
int mainMenuWindowHandleEvents()
{
    _in_main_menu = true;

    bool oldCursorIsHidden = cursorIsHidden();
    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    unsigned int tick = getTicks();

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        for (int buttonIndex = 0; buttonIndex < MAIN_MENU_BUTTON_COUNT; buttonIndex++) {
            if (keyCode == gMainMenuButtonKeyBindings[buttonIndex] || keyCode == toupper(gMainMenuButtonKeyBindings[buttonIndex])) {
                // NOTE: Uninline.
                main_menu_play_sound("nmselec1");

                rc = _return_values[buttonIndex];

                if (buttonIndex == MAIN_MENU_BUTTON_CREDITS && (gPressedPhysicalKeys[SDL_SCANCODE_RSHIFT] != KEY_STATE_UP || gPressedPhysicalKeys[SDL_SCANCODE_LSHIFT] != KEY_STATE_UP)) {
                    rc = MAIN_MENU_QUOTES;
                }

                break;
            }
        }

        if (rc == -1) {
            if (keyCode == KEY_CTRL_R) {
                rc = MAIN_MENU_SELFRUN;
                continue;
            } else if (keyCode == KEY_PLUS || keyCode == KEY_EQUAL) {
                brightnessIncrease();
            } else if (keyCode == KEY_MINUS || keyCode == KEY_UNDERSCORE) {
                brightnessDecrease();
            } else if (keyCode == KEY_UPPERCASE_D || keyCode == KEY_LOWERCASE_D) {
                rc = MAIN_MENU_SCREENSAVER;
                continue;
            } else if (keyCode == 1111) {
                if (!(mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT)) {
                    // NOTE: Uninline.
                    main_menu_play_sound("nmselec0");
                }
                continue;
            }
        }

        if (keyCode == KEY_ESCAPE || _game_user_wants_to_quit == 3) {
            rc = MAIN_MENU_EXIT;

            // NOTE: Uninline.
            main_menu_play_sound("nmselec1");
            break;
        } else if (_game_user_wants_to_quit == 2) {
            _game_user_wants_to_quit = 0;
        } else {
            if (getTicksSince(tick) >= gMainMenuScreensaverDelay) {
                rc = MAIN_MENU_TIMEOUT;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (oldCursorIsHidden) {
        mouseHideCursor();
    }

    _in_main_menu = false;

    return rc;
}

// NOTE: Inlined.
//
// 0x481C88
static int main_menu_fatal_error()
{
    mainMenuWindowFree();

    return -1;
}

// NOTE: Inlined.
//
// 0x481C94
static void main_menu_play_sound(const char* fileName)
{
    soundPlayFile(fileName);
}

} // namespace fallout
