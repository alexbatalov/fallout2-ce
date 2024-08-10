#include "dinput.h"
#include "mouse.h"
#ifdef __SWITCH__
#include <switch.h>
#endif
#include <iostream>

namespace fallout {

static int gMouseWheelDeltaX = 0;
static int gMouseWheelDeltaY = 0;

#ifdef __SWITCH__
static const int JOYSTICK_DEAD_ZONE = 8000;
static PadState pad;
double cursorSpeedup = 1.0;
#endif

// 0x4E0400
bool directInputInit()
{
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
        return false;
    }

    if (!mouseDeviceInit()) {
        goto err;
    }

    if (!keyboardDeviceInit()) {
        goto err;
    }

    #ifdef __SWITCH__
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    #endif
    return true;

err:
    directInputFree();
    return false;
}

// 0x4E0478
void directInputFree()
{
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

// 0x4E04E8
bool mouseDeviceAcquire()
{
    return true;
}

// 0x4E0514
bool mouseDeviceUnacquire()
{
    return true;
}

// 0x4E053C
bool mouseDeviceGetData(MouseData* mouseState)
{
    // CE: This function is sometimes called outside loops calling `get_input`
    // and subsequently `GNW95_process_message`, so mouse events might not be
    // handled by SDL yet.
    //
    // TODO: Move mouse events processing into `GNW95_process_message` and
    // update mouse position manually.
    SDL_PumpEvents();

    Uint32 buttons = SDL_GetRelativeMouseState(&(mouseState->x), &(mouseState->y));
    mouseState->buttons[0] = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    mouseState->buttons[1] = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    mouseState->wheelX = gMouseWheelDeltaX;
    mouseState->wheelY = gMouseWheelDeltaY;

    gMouseWheelDeltaX = 0;
    gMouseWheelDeltaY = 0;

    #ifdef __SWITCH__
    padUpdate(&pad);
    
    handleLeftStickMovement(mouseState);
    handleControllerButtons(mouseState);
    #endif

    return true;
}

// 0x4E05A8
bool keyboardDeviceAcquire()
{
    return true;
}

// 0x4E05D4
bool keyboardDeviceUnacquire()
{
    return true;
}

// 0x4E05FC
bool keyboardDeviceReset()
{
    SDL_FlushEvents(SDL_KEYDOWN, SDL_TEXTINPUT);
    return true;
}

// 0x4E0650
bool keyboardDeviceGetData(KeyboardData* keyboardData)
{
    #ifdef __SWITCH__
    padUpdate(&pad);
    #endif

    return true;
}

// 0x4E070C
bool mouseDeviceInit()
{
    return SDL_SetRelativeMouseMode(SDL_TRUE) == 0;
}

// 0x4E078C
void mouseDeviceFree()
{
}

// 0x4E07B8
bool keyboardDeviceInit()
{
    return true;
}

// 0x4E0874
void keyboardDeviceFree()
{
}

void handleLeftStickMovement(MouseData* mouseState)
{
    HidAnalogStickState leftStick = padGetStickPos(&pad, 0);
    if (abs(leftStick.x) > JOYSTICK_DEAD_ZONE || abs(leftStick.y) > JOYSTICK_DEAD_ZONE) {
        mouseState->x += static_cast<int>((leftStick.x / 10000) * cursorSpeedup * (gMouseSensitivity * 1.5));
        mouseState->y -= static_cast<int>((leftStick.y / 10000) * cursorSpeedup * (gMouseSensitivity * 1.5));
    }

    // Clamp mouse coordinates to screen boundaries
    if (mouseState->x >= 1708) mouseState->x = 1707; // TODO if we're grabbing custom resolution make sure these boundaries are respected..
    if (mouseState->y >= 960) mouseState->y = 959;
}

void handleMouseEvent(SDL_Event* event)
{
    // Mouse movement and buttons are accumulated in SDL itself and will be
    // processed later in `mouseDeviceGetData` via `SDL_GetRelativeMouseState`.

    if (event->type == SDL_MOUSEWHEEL) {
        gMouseWheelDeltaX += event->wheel.x;
        gMouseWheelDeltaY += event->wheel.y;
    }
}

void handleControllerButtons(MouseData* mouseState)
{
    u64 buttons = padGetButtons(&pad);
    mouseState->buttons[0] |= (buttons & HidNpadButton_ZL) != 0;
    mouseState->buttons[1] |= (buttons & HidNpadButton_ZR) != 0;
}

} // namespace fallout
