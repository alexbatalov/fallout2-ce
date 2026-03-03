#include "dinput.h"
#include "kb.h"

#ifdef NXDK
#include "xbox_debug.h"
#endif

#include <algorithm>
#include <cmath>

namespace fallout {

static int gMouseWheelDeltaX = 0;
static int gMouseWheelDeltaY = 0;

// Cached controller handle — opened once on SDL_CONTROLLERDEVICEADDED,
// closed on SDL_CONTROLLERDEVICEREMOVED or directInputFree().
SDL_GameController* gController = nullptr;

// Accumulated mouse deltas from left stick, consumed by mouseDeviceGetData()
int gLeftStickDeltaX = 0;
int gLeftStickDeltaY = 0;

// Mouse button state driven by A/B buttons
int gGamepadLeftClick = 0;
int gGamepadRightClick = 0;

// Scaled analog stick values in [-1.0, 1.0]
static float leftStickX = 0.0f;
static float leftStickY = 0.0f;
static float rightStickX = 0.0f;
static float rightStickY = 0.0f;

// Radial deadzone: scales the stick output so movement outside the deadzone
// maps smoothly to the full [-1, 1] range, preserving direction.
static void ScaleJoystickAxes(float* x, float* y, float deadzone)
{
    if (deadzone <= 0.0f) return;
    if (deadzone >= 1.0f) {
        *x = 0;
        *y = 0;
        return;
    }

    const float maximum = 32767.0f;
    float ax = *x;
    float ay = *y;
    float deadZone = deadzone * maximum;

    float magnitude = std::sqrt(ax * ax + ay * ay);
    if (magnitude >= deadZone) {
        float scalingFactor = (magnitude - deadZone) / ((maximum - deadZone) * magnitude);
        ax *= scalingFactor;
        ay *= scalingFactor;

        // Clamp to [-1, 1]
        float absX = std::fabs(ax);
        float absY = std::fabs(ay);
        if (absX > 1.0f || absY > 1.0f) {
            float clamp = 1.0f / std::max(absX, absY);
            ax *= clamp;
            ay *= clamp;
        }
        *x = ax;
        *y = ay;
    } else {
        *x = 0;
        *y = 0;
    }
}

static void ScaleLeftJoystick(float rawX, float rawY)
{
    leftStickX = rawX;
    leftStickY = rawY;
    ScaleJoystickAxes(&leftStickX, &leftStickY, 0.24f);
}

static void ScaleRightJoystick(float rawX, float rawY)
{
    rightStickX = rawX;
    rightStickY = rawY;
    ScaleJoystickAxes(&rightStickX, &rightStickY, 0.24f);
}

// Called from HandleControllerAxisMotion()
static float leftStickXRaw = 0.0f;
static float leftStickYRaw = 0.0f;
static float rightStickXRaw = 0.0f;
static float rightStickYRaw = 0.0f;

// Injects a scancode directly into the keyboard pipeline, bypassing QWERTY
// normalization (correct for synthetic controller-generated keys).
static void InjectKeyEvent(SDL_Scancode scancode, int down)
{
    KeyboardData kd;
    kd.key = static_cast<int>(scancode);
    kd.down = static_cast<char>(down);
    _kb_simulate_key(&kd);
}

void HandleControllerDeviceAdded(const SDL_Event& event)
{
    const int32_t joystickIndex = event.cdevice.which;
    if (!SDL_IsGameController(joystickIndex)) {
#ifdef NXDK
        DbgPrint("[dinput] Device %d is not a compatible game controller\n", joystickIndex);
#endif
        return;
    }

    gController = SDL_GameControllerOpen(joystickIndex);
    if (gController == nullptr) {
#ifdef NXDK
        DbgPrint("[dinput] Failed to open controller %d: %s\n", joystickIndex, SDL_GetError());
#endif
        SDL_ClearError();
        return;
    }
#ifdef NXDK
    DbgPrint("[dinput] Opened game controller at index %d\n", joystickIndex);
#endif
}

void HandleControllerDeviceRemoved(const SDL_Event& event)
{
    if (gController == nullptr) return;

    SDL_GameController* removed = SDL_GameControllerFromInstanceID(event.cdevice.which);
    if (removed == gController) {
#ifdef NXDK
        DbgPrint("[dinput] Game controller disconnected\n");
#endif
        SDL_GameControllerClose(gController);
        gController = nullptr;
    }
}

void HandleControllerAxisMotion(const SDL_Event& event)
{
    static bool leftTriggerPressed = false;
    static bool rightTriggerPressed = false;
    const int triggerThreshold = 30000;

    switch (event.caxis.axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        leftStickXRaw = static_cast<float>(event.caxis.value);
        ScaleLeftJoystick(leftStickXRaw, leftStickYRaw);
        break;
    case SDL_CONTROLLER_AXIS_LEFTY:
        // Negate Y so stick-up = positive
        leftStickYRaw = static_cast<float>(-event.caxis.value);
        ScaleLeftJoystick(leftStickXRaw, leftStickYRaw);
        break;
    case SDL_CONTROLLER_AXIS_RIGHTX:
        rightStickXRaw = static_cast<float>(event.caxis.value);
        ScaleRightJoystick(rightStickXRaw, rightStickYRaw);
        break;
    case SDL_CONTROLLER_AXIS_RIGHTY:
        rightStickYRaw = static_cast<float>(-event.caxis.value);
        ScaleRightJoystick(rightStickXRaw, rightStickYRaw);
        break;
    // Triggers are analog axes — emit keyboard events at threshold
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        if (event.caxis.value > triggerThreshold) {
            if (!leftTriggerPressed) {
                leftTriggerPressed = true;
                InjectKeyEvent(SDL_SCANCODE_TAB, 1);
            }
        } else if (leftTriggerPressed) {
            leftTriggerPressed = false;
            InjectKeyEvent(SDL_SCANCODE_TAB, 0);
        }
        break;
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        if (event.caxis.value > triggerThreshold) {
            if (!rightTriggerPressed) {
                rightTriggerPressed = true;
                InjectKeyEvent(SDL_SCANCODE_RETURN, 1);
            }
        } else if (rightTriggerPressed) {
            rightTriggerPressed = false;
            InjectKeyEvent(SDL_SCANCODE_RETURN, 0);
        }
        break;
    }
}

// Time-delta accumulator for smooth, frame-rate-independent cursor movement.
struct LeftStickAccumulator {
    Uint32 lastTc;
    float hiresDX;
    float hiresDY;

    LeftStickAccumulator()
        : lastTc(SDL_GetTicks())
        , hiresDX(0)
        , hiresDY(0)
    {
    }

    void Poll(int* outDX, int* outDY, int slowdown)
    {
        Uint32 tc = SDL_GetTicks();
        int dtc = static_cast<int>(tc - lastTc);
        lastTc = tc;

        hiresDX += leftStickX * dtc;
        hiresDY += leftStickY * dtc;

        int dx = static_cast<int>(hiresDX / slowdown);
        int dy = static_cast<int>(hiresDY / slowdown);

        // Keep sub-pixel remainder
        hiresDX -= dx * slowdown;
        hiresDY -= dy * slowdown;

        *outDX = dx;
        *outDY = -dy; // SDL Y axis is inverted relative to game coords
    }

    void Clear()
    {
        lastTc = SDL_GetTicks();
        hiresDX = 0;
        hiresDY = 0;
    }
};

void ProcessLeftStick()
{
    static LeftStickAccumulator acc;

    if (leftStickX == 0.0f && leftStickY == 0.0f) {
        acc.Clear();
        return;
    }

    int dx = 0, dy = 0;
    acc.Poll(&dx, &dy, 2);

    gLeftStickDeltaX += dx;
    gLeftStickDeltaY += dy;
}

bool GetGamepadMouseState(MouseData* mouseState)
{
    mouseState->x = gLeftStickDeltaX;
    mouseState->y = gLeftStickDeltaY;
    mouseState->buttons[0] = gGamepadLeftClick;
    mouseState->buttons[1] = gGamepadRightClick;
    mouseState->wheelX = 0;
    mouseState->wheelY = 0;

    gLeftStickDeltaX = 0;
    gLeftStickDeltaY = 0;

    return true;
}

// 0x4E0400
bool directInputInit()
{
    // Controller support for all SDL2 platforms
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0) {
        SDL_Log("[dinput] SDL_InitSubSystem(GAMECONTROLLER): %s\n", SDL_GetError());
        // Non-fatal: continue without controller support
    }

    if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
        return false;
    }

    if (!mouseDeviceInit()) {
        goto err;
    }

    if (!keyboardDeviceInit()) {
        goto err;
    }

    return true;

err:
    directInputFree();
    return false;
}

// 0x4E0478
void directInputFree()
{
    if (gController != nullptr) {
        SDL_GameControllerClose(gController);
        gController = nullptr;
    }
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
#ifdef NXDK
    return GetGamepadMouseState(mouseState);
#else
    // CE: This function is sometimes called outside loops calling `get_input`
    // and subsequently `GNW95_process_message`, so mouse events might not be
    // handled by SDL yet.
    //
    // TODO: Move mouse events processing into `GNW95_process_message` and
    // update mouse position manually.
    SDL_PumpEvents();

    Uint32 buttons = SDL_GetRelativeMouseState(&(mouseState->x), &(mouseState->y));

    // Blend gamepad cursor movement with real mouse
    mouseState->x += gLeftStickDeltaX;
    mouseState->y += gLeftStickDeltaY;
    gLeftStickDeltaX = 0;
    gLeftStickDeltaY = 0;

    // OR real mouse buttons with gamepad buttons
    mouseState->buttons[0] = ((buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0) || gGamepadLeftClick;
    mouseState->buttons[1] = ((buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0) || gGamepadRightClick;

    mouseState->wheelX = gMouseWheelDeltaX;
    mouseState->wheelY = gMouseWheelDeltaY;

    gMouseWheelDeltaX = 0;
    gMouseWheelDeltaY = 0;

    return true;
#endif
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

// 0x4E070C
bool mouseDeviceInit()
{
#ifdef NXDK
    // No relative mouse mode on Xbox — controller simulates mouse
    return true;
#else
    return SDL_SetRelativeMouseMode(SDL_TRUE) == 0;
#endif
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

void handleMouseEvent(SDL_Event* event)
{
    // Mouse movement and buttons are accumulated in SDL itself and will be
    // processed later in `mouseDeviceGetData` via `SDL_GetRelativeMouseState`.

    if (event->type == SDL_MOUSEWHEEL) {
        gMouseWheelDeltaX += event->wheel.x;
        gMouseWheelDeltaY += event->wheel.y;
    }
}

} // namespace fallout
