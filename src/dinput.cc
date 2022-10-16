#include "dinput.h"
#include "display_monitor.h"
#include "game_mouse.h"

namespace fallout {

enum InputType {
    INPUT_TYPE_MOUSE,
    INPUT_TYPE_TOUCH,
} InputType;

static int gLastInputType = INPUT_TYPE_MOUSE;

static int gTouchMouseLastX = 0;
static int gTouchMouseLastY = 0;
static int gTouchMouseDeltaX = 0;
static int gTouchMouseDeltaY = 0;

static int gTouchFingers = 0;
static unsigned int gTouchGestureLastTouchDownTimestamp = 0;
static unsigned int gTouchGestureLastTouchUpTimestamp = 0;
static int gTouchGestureTaps = 0;
static bool gTouchGestureHandled = false;

static int gMouseWheelDeltaX = 0;
static int gMouseWheelDeltaY = 0;

extern int screenGetWidth();
extern int screenGetHeight();

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

void handleMouseEvent(SDL_Event* event)
{
    // Mouse movement and buttons are accumulated in SDL itself and will be
    // processed later in `mouseDeviceGetData` via `SDL_GetRelativeMouseState`.

    if (event->type == SDL_MOUSEWHEEL) {
        gMouseWheelDeltaX += event->wheel.x;
        gMouseWheelDeltaY += event->wheel.y;
    }

    if (gLastInputType != INPUT_TYPE_MOUSE) {
        // Reset touch data.
        gTouchMouseLastX = 0;
        gTouchMouseLastY = 0;
        gTouchMouseDeltaX = 0;
        gTouchMouseDeltaY = 0;

        gTouchFingers = 0;
        gTouchGestureLastTouchDownTimestamp = 0;
        gTouchGestureLastTouchUpTimestamp = 0;
        gTouchGestureTaps = 0;
        gTouchGestureHandled = false;

        gLastInputType = INPUT_TYPE_MOUSE;
    }
}

static bool em_mode = false;
static SDL_FingerID lastTouchId = -1;
static Uint32 lastType = -1;

static bool longPressed = false;
static bool clicked = false;

bool mouseDeviceGetData(MouseData* mouseState)
{
    mouseState->em_mode = false;

    if (gLastInputType == INPUT_TYPE_TOUCH) {
        mouseState->x = gTouchMouseDeltaX;
        mouseState->y = gTouchMouseDeltaY;
        mouseState->buttons[0] = 0;
        mouseState->buttons[1] = 0;
        mouseState->wheelX = 0;
        mouseState->wheelY = 0;
        gTouchMouseDeltaX = 0;
        gTouchMouseDeltaY = 0;

        if (lastType == SDL_FINGERDOWN && SDL_GetTicks() - gTouchGestureLastTouchUpTimestamp > 250 && gTouchMouseLastX > screenGetWidth() / 4) {
            longPressed = true;
        }

        if (longPressed) {
            mouseState->buttons[0] = 1;
            clicked = false;
        } else if (clicked) {
            clicked = false;
            if (em_mode) {
                mouseState->em_mode = em_mode;
                mouseState->rawx = gTouchMouseLastX;
                mouseState->rawy = gTouchMouseLastY;
            }

            if (gTouchMouseLastX < screenGetWidth() / 4) {
                mouseState->buttons[1] = 1;
            } else {
                mouseState->buttons[0] = 1;
            }
        }

    } else {
        Uint32 buttons = SDL_GetRelativeMouseState(&(mouseState->x), &(mouseState->y));
        mouseState->buttons[0] = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        mouseState->buttons[1] = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
        mouseState->wheelX = gMouseWheelDeltaX;
        mouseState->wheelY = gMouseWheelDeltaY;

        gMouseWheelDeltaX = 0;
        gMouseWheelDeltaY = 0;
    }

    return true;
}

void handleTouchEvent(SDL_Event* event)
{
    int windowWidth = screenGetWidth();
    int windowHeight = screenGetHeight();

    Uint32 type = event->tfinger.type;
    SDL_FingerID id = event->tfinger.fingerId;

    if (id >= 2)
        return;

    if (type == SDL_FINGERDOWN) {
        if (lastTouchId != -1 && id != lastTouchId) {
            //displayMonitorAddMessage("em true");
            em_mode = true;
        }
        lastTouchId = id;

        gTouchGestureLastTouchUpTimestamp = event->tfinger.timestamp;

        gTouchMouseLastX = (int)(event->tfinger.x * windowWidth);
        gTouchMouseLastY = (int)(event->tfinger.y * windowHeight);
        gTouchMouseDeltaX = 0;
        gTouchMouseDeltaY = 0;

        lastType = SDL_FINGERDOWN;
        clicked = false;
    } else if (type == SDL_FINGERMOTION) {
        if (id == lastTouchId) {
            int prevX = gTouchMouseLastX;
            int prevY = gTouchMouseLastY;
            gTouchMouseLastX = (int)(event->tfinger.x * windowWidth);
            gTouchMouseLastY = (int)(event->tfinger.y * windowHeight);
            if(event->tfinger.x < 0.5){
                gTouchMouseDeltaX += (gTouchMouseLastX - prevX) * 2;
                gTouchMouseDeltaY += (gTouchMouseLastY - prevY) * 2;
            } else {
                gTouchMouseDeltaX += gTouchMouseLastX - prevX;
                gTouchMouseDeltaY += gTouchMouseLastY - prevY;
            }
            gTouchGestureLastTouchUpTimestamp = -1;

            lastType = SDL_FINGERMOTION;
            clicked = false;
        }
    } else if (type == SDL_FINGERUP) {
        if (id != lastTouchId) {
            em_mode = false;
        } else {
            clicked = false;

            if (lastType == SDL_FINGERDOWN && longPressed == false) {
                clicked = true;
            }
            longPressed = false;
            gTouchGestureLastTouchUpTimestamp = -1;
            lastTouchId = -1;
            lastType = SDL_FINGERUP;
        }
    }

    if (gLastInputType != INPUT_TYPE_TOUCH) {
        // Reset mouse data.
        SDL_GetRelativeMouseState(NULL, NULL);

        gLastInputType = INPUT_TYPE_TOUCH;
    }
}

} // namespace fallout
