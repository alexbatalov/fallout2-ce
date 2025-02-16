#include "kb.h"

#include <string.h>

#include "input.h"
#include "svga.h"

namespace fallout {

typedef struct LogicalKeyEntry {
    short field_0;
    short unmodified;
    short shift;
    short lmenu;
    short rmenu;
    short ctrl;
} LogicalKeyEntry;

typedef struct KeyboardEvent {
    int scanCode;
    unsigned short modifiers;
} KeyboardEvent;

static int _kb_next_ascii_English_US();
static int keyboardDequeueLogicalKeyCode();
static void keyboardBuildQwertyConfiguration();
static void keyboardBuildFrenchConfiguration();
static void keyboardBuildGermanConfiguration();
static void keyboardBuildItalianConfiguration();
static void keyboardBuildSpanishConfiguration();
static void _kb_init_lock_status();
static int keyboardPeekEvent(int index, KeyboardEvent** keyboardEventPtr);

// 0x51E2D0
static unsigned char _kb_installed = 0;

// 0x51E2D4
static bool gKeyboardDisabled = false;

// 0x51E2D8
static bool gKeyboardNumpadDisabled = false;

// 0x51E2DC
static bool gKeyboardNumlockDisabled = false;

// 0x51E2E0
static int gKeyboardEventQueueWriteIndex = 0;

// 0x51E2E4
static int gKeyboardEventQueueReadIndex = 0;

// 0x51E2E8
static short word_51E2E8 = 0;

// 0x51E2EA
static int gModifierKeysState = 0;

// TODO: It's _kb_next_ascii_English_US (not implemented yet).
//
// 0x51E2EC
static int (*_kb_scan_to_ascii)() = keyboardDequeueLogicalKeyCode;

// Ring buffer of keyboard events.
//
// 0x6ACB30
static KeyboardEvent gKeyboardEventsQueue[64];

// A map of logical key configurations for physical scan codes [SDL_SCANCODE_*].
//
// 0x6ACC30
static LogicalKeyEntry gLogicalKeyEntries[SDL_NUM_SCANCODES];

// A state of physical keys [SDL_SCANCODE_*] currently pressed.
//
// 0 - key is not pressed.
// 1 - key pressed.
//
// 0x6AD830
unsigned char gPressedPhysicalKeys[SDL_NUM_SCANCODES];

// 0x6AD930
static unsigned int _kb_idle_start_time;

// 0x6AD934
static KeyboardEvent gLastKeyboardEvent;

// 0x6AD938
int gKeyboardLayout;

// The number of keys currently pressed.
//
// 0x6AD93C
unsigned char gPressedPhysicalKeysCount;

// 0x4CBC90
int keyboardInit()
{
    if (_kb_installed) {
        return -1;
    }

    _kb_installed = 1;

    // NOTE: Uninline.
    keyboardReset();

    _kb_init_lock_status();
    keyboardSetLayout(KEYBOARD_LAYOUT_QWERTY);

    _kb_idle_start_time = getTicks();

    return 0;
}

// 0x4CBD00
void keyboardFree()
{
    if (_kb_installed) {
        _kb_installed = 0;
    }
}

// 0x4CBDA8
void keyboardReset()
{
    if (_kb_installed) {
        gPressedPhysicalKeysCount = 0;

        memset(gPressedPhysicalKeys, 0, sizeof(gPressedPhysicalKeys));

        gKeyboardEventQueueWriteIndex = 0;
        gKeyboardEventQueueReadIndex = 0;
    }

    keyboardDeviceReset();
    _GNW95_clear_time_stamps();
}

int _kb_getch()
{
    int rc = -1;

    if (_kb_installed != 0) {
        rc = _kb_scan_to_ascii();
    }

    return rc;
}

// 0x4CBE00
void keyboardDisable()
{
    gKeyboardDisabled = true;
}

// 0x4CBE0C
void keyboardEnable()
{
    gKeyboardDisabled = false;
}

// 0x4CBE18
int keyboardIsDisabled()
{
    return gKeyboardDisabled;
}

// 0x4CBE74
void keyboardSetLayout(int keyboardLayout)
{
    int oldKeyboardLayout = gKeyboardLayout;
    gKeyboardLayout = keyboardLayout;

    switch (keyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        _kb_scan_to_ascii = _kb_next_ascii_English_US;
        keyboardBuildQwertyConfiguration();
        break;
    // case KEYBOARD_LAYOUT_FRENCH:
    //    _kb_scan_to_ascii = sub_4CC5BC;
    //    _kb_map_ascii_French();
    //    break;
    // case KEYBOARD_LAYOUT_GERMAN:
    //    _kb_scan_to_ascii = sub_4CC94C;
    //    _kb_map_ascii_German();
    //    break;
    // case KEYBOARD_LAYOUT_ITALIAN:
    //    _kb_scan_to_ascii = sub_4CCE14;
    //    _kb_map_ascii_Italian();
    //    break;
    // case KEYBOARD_LAYOUT_SPANISH:
    //    _kb_scan_to_ascii = sub_4CD0E0;
    //    _kb_map_ascii_Spanish();
    //    break;
    default:
        gKeyboardLayout = oldKeyboardLayout;
        break;
    }
}

// 0x4CBEEC
int keyboardGetLayout()
{
    return gKeyboardLayout;
}

// TODO: Key type is likely short.
void _kb_simulate_key(KeyboardData* data)
{
    _kb_idle_start_time = _get_bk_time();

    int key = data->key;
    int keyState = data->down == 1 ? KEY_STATE_DOWN : KEY_STATE_UP;

    int physicalKey = key;

    if (keyState != KEY_STATE_UP && gPressedPhysicalKeys[physicalKey] != KEY_STATE_UP) {
        keyState = KEY_STATE_REPEAT;
    }

    if (gPressedPhysicalKeys[physicalKey] != keyState) {
        gPressedPhysicalKeys[physicalKey] = keyState;
        if (keyState == KEY_STATE_DOWN) {
            gPressedPhysicalKeysCount++;
        } else if (keyState == KEY_STATE_UP) {
            gPressedPhysicalKeysCount--;
        }
    }

    if (keyState != KEY_STATE_UP) {
        gLastKeyboardEvent.scanCode = physicalKey;
        gLastKeyboardEvent.modifiers = 0;

        if (physicalKey == SDL_SCANCODE_CAPSLOCK) {
            if (gPressedPhysicalKeys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && gPressedPhysicalKeys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                // TODO: Missing check for QWERTY keyboard layout.
                if ((gModifierKeysState & MODIFIER_KEY_STATE_CAPS_LOCK) != 0) {
                    // TODO: There is some strange code checking for _kb_layout, check in
                    // debugger.
                    gModifierKeysState &= ~MODIFIER_KEY_STATE_CAPS_LOCK;
                } else {
                    gModifierKeysState |= MODIFIER_KEY_STATE_CAPS_LOCK;
                }
            }
        } else if (physicalKey == SDL_SCANCODE_NUMLOCKCLEAR) {
            if (gPressedPhysicalKeys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && gPressedPhysicalKeys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                if ((gModifierKeysState & MODIFIER_KEY_STATE_NUM_LOCK) != 0) {
                    gModifierKeysState &= ~MODIFIER_KEY_STATE_NUM_LOCK;
                } else {
                    gModifierKeysState |= MODIFIER_KEY_STATE_NUM_LOCK;
                }
            }
        } else if (physicalKey == SDL_SCANCODE_SCROLLLOCK) {
            if (gPressedPhysicalKeys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && gPressedPhysicalKeys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                if ((gModifierKeysState & MODIFIER_KEY_STATE_SCROLL_LOCK) != 0) {
                    gModifierKeysState &= ~MODIFIER_KEY_STATE_SCROLL_LOCK;
                } else {
                    gModifierKeysState |= MODIFIER_KEY_STATE_SCROLL_LOCK;
                }
            }
        } else if ((physicalKey == SDL_SCANCODE_LSHIFT || physicalKey == SDL_SCANCODE_RSHIFT) && (gModifierKeysState & MODIFIER_KEY_STATE_CAPS_LOCK) != 0 && gKeyboardLayout != 0) {
            if (gPressedPhysicalKeys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && gPressedPhysicalKeys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                if (gModifierKeysState & MODIFIER_KEY_STATE_CAPS_LOCK) {
                    gModifierKeysState &= ~MODIFIER_KEY_STATE_CAPS_LOCK;
                } else {
                    gModifierKeysState |= MODIFIER_KEY_STATE_CAPS_LOCK;
                }
            }
        }

        if (gModifierKeysState != 0) {
            if ((gModifierKeysState & MODIFIER_KEY_STATE_NUM_LOCK) != 0 && !gKeyboardNumlockDisabled) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_NUM_LOCK;
            }

            if ((gModifierKeysState & MODIFIER_KEY_STATE_CAPS_LOCK) != 0) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_CAPS_LOCK;
            }

            if ((gModifierKeysState & MODIFIER_KEY_STATE_SCROLL_LOCK) != 0) {
                gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_SCROLL_LOCK;
            }
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_LSHIFT] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_RSHIFT] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_SHIFT;
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_LALT] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_ALT;
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_RALT] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_ALT;
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_LCTRL] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_CONTROL;
        }

        if (gPressedPhysicalKeys[SDL_SCANCODE_RCTRL] != KEY_STATE_UP) {
            gLastKeyboardEvent.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_CONTROL;
        }

        if (((gKeyboardEventQueueWriteIndex + 1) & 0x3F) != gKeyboardEventQueueReadIndex) {
            gKeyboardEventsQueue[gKeyboardEventQueueWriteIndex] = gLastKeyboardEvent;
            gKeyboardEventQueueWriteIndex++;
            gKeyboardEventQueueWriteIndex &= 0x3F;
        }
    }
}

// 0x4CC2F0
static int _kb_next_ascii_English_US()
{
    KeyboardEvent* keyboardEvent;
    if (keyboardPeekEvent(0, &keyboardEvent) != 0) {
        return -1;
    }

    if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_CAPS_LOCK) != 0) {
        int a = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_A : SDL_SCANCODE_Q);
        int m = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_M : SDL_SCANCODE_SEMICOLON);
        int q = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_Q : SDL_SCANCODE_A);
        int w = (gKeyboardLayout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_W : SDL_SCANCODE_Z);

        int y;
        switch (gKeyboardLayout) {
        case KEYBOARD_LAYOUT_QWERTY:
        case KEYBOARD_LAYOUT_FRENCH:
        case KEYBOARD_LAYOUT_ITALIAN:
        case KEYBOARD_LAYOUT_SPANISH:
            y = SDL_SCANCODE_Y;
            break;
        default:
            // GERMAN
            y = SDL_SCANCODE_Z;
            break;
        }

        int z;
        switch (gKeyboardLayout) {
        case KEYBOARD_LAYOUT_QWERTY:
        case KEYBOARD_LAYOUT_ITALIAN:
        case KEYBOARD_LAYOUT_SPANISH:
            z = SDL_SCANCODE_Z;
            break;
        case KEYBOARD_LAYOUT_FRENCH:
            z = SDL_SCANCODE_W;
            break;
        default:
            // GERMAN
            z = SDL_SCANCODE_Y;
            break;
        }

        int scanCode = keyboardEvent->scanCode;
        if (scanCode == a
            || scanCode == SDL_SCANCODE_B
            || scanCode == SDL_SCANCODE_C
            || scanCode == SDL_SCANCODE_D
            || scanCode == SDL_SCANCODE_E
            || scanCode == SDL_SCANCODE_F
            || scanCode == SDL_SCANCODE_G
            || scanCode == SDL_SCANCODE_H
            || scanCode == SDL_SCANCODE_I
            || scanCode == SDL_SCANCODE_J
            || scanCode == SDL_SCANCODE_K
            || scanCode == SDL_SCANCODE_L
            || scanCode == m
            || scanCode == SDL_SCANCODE_N
            || scanCode == SDL_SCANCODE_O
            || scanCode == SDL_SCANCODE_P
            || scanCode == q
            || scanCode == SDL_SCANCODE_R
            || scanCode == SDL_SCANCODE_S
            || scanCode == SDL_SCANCODE_T
            || scanCode == SDL_SCANCODE_U
            || scanCode == SDL_SCANCODE_V
            || scanCode == w
            || scanCode == SDL_SCANCODE_X
            || scanCode == y
            || scanCode == z) {
            if (keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) {
                keyboardEvent->modifiers &= ~KEYBOARD_EVENT_MODIFIER_ANY_SHIFT;
            } else {
                keyboardEvent->modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
            }
        }
    }

    return keyboardDequeueLogicalKeyCode();
}

// 0x4CDA4C
static int keyboardDequeueLogicalKeyCode()
{
    KeyboardEvent* keyboardEvent;
    if (keyboardPeekEvent(0, &keyboardEvent) != 0) {
        return -1;
    }

    switch (keyboardEvent->scanCode) {
    case SDL_SCANCODE_KP_DIVIDE:
    case SDL_SCANCODE_KP_MULTIPLY:
    case SDL_SCANCODE_KP_MINUS:
    case SDL_SCANCODE_KP_PLUS:
    case SDL_SCANCODE_KP_ENTER:
        if (gKeyboardNumpadDisabled) {
            if (gKeyboardEventQueueReadIndex != gKeyboardEventQueueWriteIndex) {
                gKeyboardEventQueueReadIndex++;
                gKeyboardEventQueueReadIndex &= (KEY_QUEUE_SIZE - 1);
            }
            return -1;
        }
        break;
    case SDL_SCANCODE_KP_0:
    case SDL_SCANCODE_KP_1:
    case SDL_SCANCODE_KP_2:
    case SDL_SCANCODE_KP_3:
    case SDL_SCANCODE_KP_4:
    case SDL_SCANCODE_KP_5:
    case SDL_SCANCODE_KP_6:
    case SDL_SCANCODE_KP_7:
    case SDL_SCANCODE_KP_8:
    case SDL_SCANCODE_KP_9:
        if (gKeyboardNumpadDisabled) {
            if (gKeyboardEventQueueReadIndex != gKeyboardEventQueueWriteIndex) {
                gKeyboardEventQueueReadIndex++;
                gKeyboardEventQueueReadIndex &= (KEY_QUEUE_SIZE - 1);
            }
            return -1;
        }

        if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_ALT) == 0 && (keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_NUM_LOCK) != 0) {
            if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) != 0) {
                keyboardEvent->modifiers &= ~KEYBOARD_EVENT_MODIFIER_ANY_SHIFT;
            } else {
                keyboardEvent->modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
            }
        }

        break;
    }

    int logicalKey = -1;

    LogicalKeyEntry* logicalKeyDescription = &(gLogicalKeyEntries[keyboardEvent->scanCode]);
    if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_CONTROL) != 0) {
        logicalKey = logicalKeyDescription->ctrl;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_RIGHT_ALT) != 0) {
        logicalKey = logicalKeyDescription->rmenu;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_LEFT_ALT) != 0) {
        logicalKey = logicalKeyDescription->lmenu;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) != 0) {
        logicalKey = logicalKeyDescription->shift;
    } else {
        logicalKey = logicalKeyDescription->unmodified;
    }

    if (gKeyboardEventQueueReadIndex != gKeyboardEventQueueWriteIndex) {
        gKeyboardEventQueueReadIndex++;
        gKeyboardEventQueueReadIndex &= (KEY_QUEUE_SIZE - 1);
    }

    return logicalKey;
}

// 0x4CDC08
static void keyboardBuildQwertyConfiguration()
{
    int k;

    for (k = 0; k < SDL_NUM_SCANCODES; k++) {
        gLogicalKeyEntries[k].field_0 = -1;
        gLogicalKeyEntries[k].unmodified = -1;
        gLogicalKeyEntries[k].shift = -1;
        gLogicalKeyEntries[k].lmenu = -1;
        gLogicalKeyEntries[k].rmenu = -1;
        gLogicalKeyEntries[k].ctrl = -1;
    }

    gLogicalKeyEntries[SDL_SCANCODE_ESCAPE].unmodified = KEY_ESCAPE;
    gLogicalKeyEntries[SDL_SCANCODE_ESCAPE].shift = KEY_ESCAPE;
    gLogicalKeyEntries[SDL_SCANCODE_ESCAPE].lmenu = KEY_ESCAPE;
    gLogicalKeyEntries[SDL_SCANCODE_ESCAPE].rmenu = KEY_ESCAPE;
    gLogicalKeyEntries[SDL_SCANCODE_ESCAPE].ctrl = KEY_ESCAPE;

    gLogicalKeyEntries[SDL_SCANCODE_F1].unmodified = KEY_F1;
    gLogicalKeyEntries[SDL_SCANCODE_F1].shift = KEY_SHIFT_F1;
    gLogicalKeyEntries[SDL_SCANCODE_F1].lmenu = KEY_ALT_F1;
    gLogicalKeyEntries[SDL_SCANCODE_F1].rmenu = KEY_ALT_F1;
    gLogicalKeyEntries[SDL_SCANCODE_F1].ctrl = KEY_CTRL_F1;

    gLogicalKeyEntries[SDL_SCANCODE_F2].unmodified = KEY_F2;
    gLogicalKeyEntries[SDL_SCANCODE_F2].shift = KEY_SHIFT_F2;
    gLogicalKeyEntries[SDL_SCANCODE_F2].lmenu = KEY_ALT_F2;
    gLogicalKeyEntries[SDL_SCANCODE_F2].rmenu = KEY_ALT_F2;
    gLogicalKeyEntries[SDL_SCANCODE_F2].ctrl = KEY_CTRL_F2;

    gLogicalKeyEntries[SDL_SCANCODE_F3].unmodified = KEY_F3;
    gLogicalKeyEntries[SDL_SCANCODE_F3].shift = KEY_SHIFT_F3;
    gLogicalKeyEntries[SDL_SCANCODE_F3].lmenu = KEY_ALT_F3;
    gLogicalKeyEntries[SDL_SCANCODE_F3].rmenu = KEY_ALT_F3;
    gLogicalKeyEntries[SDL_SCANCODE_F3].ctrl = KEY_CTRL_F3;

    gLogicalKeyEntries[SDL_SCANCODE_F4].unmodified = KEY_F4;
    gLogicalKeyEntries[SDL_SCANCODE_F4].shift = KEY_SHIFT_F4;
    gLogicalKeyEntries[SDL_SCANCODE_F4].lmenu = KEY_ALT_F4;
    gLogicalKeyEntries[SDL_SCANCODE_F4].rmenu = KEY_ALT_F4;
    gLogicalKeyEntries[SDL_SCANCODE_F4].ctrl = KEY_CTRL_F4;

    gLogicalKeyEntries[SDL_SCANCODE_F5].unmodified = KEY_F5;
    gLogicalKeyEntries[SDL_SCANCODE_F5].shift = KEY_SHIFT_F5;
    gLogicalKeyEntries[SDL_SCANCODE_F5].lmenu = KEY_ALT_F5;
    gLogicalKeyEntries[SDL_SCANCODE_F5].rmenu = KEY_ALT_F5;
    gLogicalKeyEntries[SDL_SCANCODE_F5].ctrl = KEY_CTRL_F5;

    gLogicalKeyEntries[SDL_SCANCODE_F6].unmodified = KEY_F6;
    gLogicalKeyEntries[SDL_SCANCODE_F6].shift = KEY_SHIFT_F6;
    gLogicalKeyEntries[SDL_SCANCODE_F6].lmenu = KEY_ALT_F6;
    gLogicalKeyEntries[SDL_SCANCODE_F6].rmenu = KEY_ALT_F6;
    gLogicalKeyEntries[SDL_SCANCODE_F6].ctrl = KEY_CTRL_F6;

    gLogicalKeyEntries[SDL_SCANCODE_F7].unmodified = KEY_F7;
    gLogicalKeyEntries[SDL_SCANCODE_F7].shift = KEY_SHIFT_F7;
    gLogicalKeyEntries[SDL_SCANCODE_F7].lmenu = KEY_ALT_F7;
    gLogicalKeyEntries[SDL_SCANCODE_F7].rmenu = KEY_ALT_F7;
    gLogicalKeyEntries[SDL_SCANCODE_F7].ctrl = KEY_CTRL_F7;

    gLogicalKeyEntries[SDL_SCANCODE_F8].unmodified = KEY_F8;
    gLogicalKeyEntries[SDL_SCANCODE_F8].shift = KEY_SHIFT_F8;
    gLogicalKeyEntries[SDL_SCANCODE_F8].lmenu = KEY_ALT_F8;
    gLogicalKeyEntries[SDL_SCANCODE_F8].rmenu = KEY_ALT_F8;
    gLogicalKeyEntries[SDL_SCANCODE_F8].ctrl = KEY_CTRL_F8;

    gLogicalKeyEntries[SDL_SCANCODE_F9].unmodified = KEY_F9;
    gLogicalKeyEntries[SDL_SCANCODE_F9].shift = KEY_SHIFT_F9;
    gLogicalKeyEntries[SDL_SCANCODE_F9].lmenu = KEY_ALT_F9;
    gLogicalKeyEntries[SDL_SCANCODE_F9].rmenu = KEY_ALT_F9;
    gLogicalKeyEntries[SDL_SCANCODE_F9].ctrl = KEY_CTRL_F9;

    gLogicalKeyEntries[SDL_SCANCODE_F10].unmodified = KEY_F10;
    gLogicalKeyEntries[SDL_SCANCODE_F10].shift = KEY_SHIFT_F10;
    gLogicalKeyEntries[SDL_SCANCODE_F10].lmenu = KEY_ALT_F10;
    gLogicalKeyEntries[SDL_SCANCODE_F10].rmenu = KEY_ALT_F10;
    gLogicalKeyEntries[SDL_SCANCODE_F10].ctrl = KEY_CTRL_F10;

    gLogicalKeyEntries[SDL_SCANCODE_F11].unmodified = KEY_F11;
    gLogicalKeyEntries[SDL_SCANCODE_F11].shift = KEY_SHIFT_F11;
    gLogicalKeyEntries[SDL_SCANCODE_F11].lmenu = KEY_ALT_F11;
    gLogicalKeyEntries[SDL_SCANCODE_F11].rmenu = KEY_ALT_F11;
    gLogicalKeyEntries[SDL_SCANCODE_F11].ctrl = KEY_CTRL_F11;

    gLogicalKeyEntries[SDL_SCANCODE_F12].unmodified = KEY_F12;
    gLogicalKeyEntries[SDL_SCANCODE_F12].shift = KEY_SHIFT_F12;
    gLogicalKeyEntries[SDL_SCANCODE_F12].lmenu = KEY_ALT_F12;
    gLogicalKeyEntries[SDL_SCANCODE_F12].rmenu = KEY_ALT_F12;
    gLogicalKeyEntries[SDL_SCANCODE_F12].ctrl = KEY_CTRL_F12;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_GRAVE;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_2;
        break;
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = 0;
        break;
    default:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_GRAVE;
    gLogicalKeyEntries[k].shift = KEY_TILDE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_1].unmodified = KEY_1;
    gLogicalKeyEntries[SDL_SCANCODE_1].shift = KEY_EXCLAMATION;
    gLogicalKeyEntries[SDL_SCANCODE_1].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_2].unmodified = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_2].shift = KEY_AT;
    gLogicalKeyEntries[SDL_SCANCODE_2].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_3].unmodified = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_3].shift = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[SDL_SCANCODE_3].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_4].unmodified = KEY_4;
    gLogicalKeyEntries[SDL_SCANCODE_4].shift = KEY_DOLLAR;
    gLogicalKeyEntries[SDL_SCANCODE_4].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_4].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_4].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_5].unmodified = KEY_5;
    gLogicalKeyEntries[SDL_SCANCODE_5].shift = KEY_PERCENT;
    gLogicalKeyEntries[SDL_SCANCODE_5].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_5].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_5].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_6].unmodified = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_6].shift = KEY_CARET;
    gLogicalKeyEntries[SDL_SCANCODE_6].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_7].unmodified = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_7].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[SDL_SCANCODE_7].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_8].unmodified = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_8].shift = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_8].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_9].unmodified = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_9].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_9].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_0].unmodified = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_0].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_0].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_MINUS;
    gLogicalKeyEntries[k].shift = KEY_UNDERSCORE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_EQUALS;
        break;
    default:
        k = SDL_SCANCODE_0;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_EQUAL;
    gLogicalKeyEntries[k].shift = KEY_PLUS;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_BACKSPACE].unmodified = KEY_BACKSPACE;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSPACE].shift = KEY_BACKSPACE;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSPACE].lmenu = KEY_BACKSPACE;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSPACE].rmenu = KEY_BACKSPACE;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSPACE].ctrl = KEY_DEL;

    gLogicalKeyEntries[SDL_SCANCODE_TAB].unmodified = KEY_TAB;
    gLogicalKeyEntries[SDL_SCANCODE_TAB].shift = KEY_TAB;
    gLogicalKeyEntries[SDL_SCANCODE_TAB].lmenu = KEY_TAB;
    gLogicalKeyEntries[SDL_SCANCODE_TAB].rmenu = KEY_TAB;
    gLogicalKeyEntries[SDL_SCANCODE_TAB].ctrl = KEY_TAB;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_A;
        break;
    default:
        k = SDL_SCANCODE_Q;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Q;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Q;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Q;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_Q;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Q;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_Z;
        break;
    default:
        k = SDL_SCANCODE_W;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_W;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_W;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_W;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_W;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_W;

    gLogicalKeyEntries[SDL_SCANCODE_E].unmodified = KEY_LOWERCASE_E;
    gLogicalKeyEntries[SDL_SCANCODE_E].shift = KEY_UPPERCASE_E;
    gLogicalKeyEntries[SDL_SCANCODE_E].lmenu = KEY_ALT_E;
    gLogicalKeyEntries[SDL_SCANCODE_E].rmenu = KEY_ALT_E;
    gLogicalKeyEntries[SDL_SCANCODE_E].ctrl = KEY_CTRL_E;

    gLogicalKeyEntries[SDL_SCANCODE_R].unmodified = KEY_LOWERCASE_R;
    gLogicalKeyEntries[SDL_SCANCODE_R].shift = KEY_UPPERCASE_R;
    gLogicalKeyEntries[SDL_SCANCODE_R].lmenu = KEY_ALT_R;
    gLogicalKeyEntries[SDL_SCANCODE_R].rmenu = KEY_ALT_R;
    gLogicalKeyEntries[SDL_SCANCODE_R].ctrl = KEY_CTRL_R;

    gLogicalKeyEntries[SDL_SCANCODE_T].unmodified = KEY_LOWERCASE_T;
    gLogicalKeyEntries[SDL_SCANCODE_T].shift = KEY_UPPERCASE_T;
    gLogicalKeyEntries[SDL_SCANCODE_T].lmenu = KEY_ALT_T;
    gLogicalKeyEntries[SDL_SCANCODE_T].rmenu = KEY_ALT_T;
    gLogicalKeyEntries[SDL_SCANCODE_T].ctrl = KEY_CTRL_T;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_Y;
        break;
    default:
        k = SDL_SCANCODE_Z;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Y;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Y;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Y;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_Y;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Y;

    gLogicalKeyEntries[SDL_SCANCODE_U].unmodified = KEY_LOWERCASE_U;
    gLogicalKeyEntries[SDL_SCANCODE_U].shift = KEY_UPPERCASE_U;
    gLogicalKeyEntries[SDL_SCANCODE_U].lmenu = KEY_ALT_U;
    gLogicalKeyEntries[SDL_SCANCODE_U].rmenu = KEY_ALT_U;
    gLogicalKeyEntries[SDL_SCANCODE_U].ctrl = KEY_CTRL_U;

    gLogicalKeyEntries[SDL_SCANCODE_I].unmodified = KEY_LOWERCASE_I;
    gLogicalKeyEntries[SDL_SCANCODE_I].shift = KEY_UPPERCASE_I;
    gLogicalKeyEntries[SDL_SCANCODE_I].lmenu = KEY_ALT_I;
    gLogicalKeyEntries[SDL_SCANCODE_I].rmenu = KEY_ALT_I;
    gLogicalKeyEntries[SDL_SCANCODE_I].ctrl = KEY_CTRL_I;

    gLogicalKeyEntries[SDL_SCANCODE_O].unmodified = KEY_LOWERCASE_O;
    gLogicalKeyEntries[SDL_SCANCODE_O].shift = KEY_UPPERCASE_O;
    gLogicalKeyEntries[SDL_SCANCODE_O].lmenu = KEY_ALT_O;
    gLogicalKeyEntries[SDL_SCANCODE_O].rmenu = KEY_ALT_O;
    gLogicalKeyEntries[SDL_SCANCODE_O].ctrl = KEY_CTRL_O;

    gLogicalKeyEntries[SDL_SCANCODE_P].unmodified = KEY_LOWERCASE_P;
    gLogicalKeyEntries[SDL_SCANCODE_P].shift = KEY_UPPERCASE_P;
    gLogicalKeyEntries[SDL_SCANCODE_P].lmenu = KEY_ALT_P;
    gLogicalKeyEntries[SDL_SCANCODE_P].rmenu = KEY_ALT_P;
    gLogicalKeyEntries[SDL_SCANCODE_P].ctrl = KEY_CTRL_P;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_LEFTBRACKET;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_5;
        break;
    default:
        k = SDL_SCANCODE_8;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[k].shift = KEY_BRACE_LEFT;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_MINUS;
        break;
    default:
        k = SDL_SCANCODE_9;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[k].shift = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_BACKSLASH;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_8;
        break;
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_GRAVE;
        break;
    default:
        k = SDL_SCANCODE_MINUS;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_BACKSLASH;
    gLogicalKeyEntries[k].shift = KEY_BAR;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_BACKSLASH;

    gLogicalKeyEntries[SDL_SCANCODE_CAPSLOCK].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_CAPSLOCK].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_CAPSLOCK].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_CAPSLOCK].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_CAPSLOCK].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_Q;
        break;
    default:
        k = SDL_SCANCODE_A;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_A;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_A;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_A;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_A;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_A;

    gLogicalKeyEntries[SDL_SCANCODE_S].unmodified = KEY_LOWERCASE_S;
    gLogicalKeyEntries[SDL_SCANCODE_S].shift = KEY_UPPERCASE_S;
    gLogicalKeyEntries[SDL_SCANCODE_S].lmenu = KEY_ALT_S;
    gLogicalKeyEntries[SDL_SCANCODE_S].rmenu = KEY_ALT_S;
    gLogicalKeyEntries[SDL_SCANCODE_S].ctrl = KEY_CTRL_S;

    gLogicalKeyEntries[SDL_SCANCODE_D].unmodified = KEY_LOWERCASE_D;
    gLogicalKeyEntries[SDL_SCANCODE_D].shift = KEY_UPPERCASE_D;
    gLogicalKeyEntries[SDL_SCANCODE_D].lmenu = KEY_ALT_D;
    gLogicalKeyEntries[SDL_SCANCODE_D].rmenu = KEY_ALT_D;
    gLogicalKeyEntries[SDL_SCANCODE_D].ctrl = KEY_CTRL_D;

    gLogicalKeyEntries[SDL_SCANCODE_F].unmodified = KEY_LOWERCASE_F;
    gLogicalKeyEntries[SDL_SCANCODE_F].shift = KEY_UPPERCASE_F;
    gLogicalKeyEntries[SDL_SCANCODE_F].lmenu = KEY_ALT_F;
    gLogicalKeyEntries[SDL_SCANCODE_F].rmenu = KEY_ALT_F;
    gLogicalKeyEntries[SDL_SCANCODE_F].ctrl = KEY_CTRL_F;

    gLogicalKeyEntries[SDL_SCANCODE_G].unmodified = KEY_LOWERCASE_G;
    gLogicalKeyEntries[SDL_SCANCODE_G].shift = KEY_UPPERCASE_G;
    gLogicalKeyEntries[SDL_SCANCODE_G].lmenu = KEY_ALT_G;
    gLogicalKeyEntries[SDL_SCANCODE_G].rmenu = KEY_ALT_G;
    gLogicalKeyEntries[SDL_SCANCODE_G].ctrl = KEY_CTRL_G;

    gLogicalKeyEntries[SDL_SCANCODE_H].unmodified = KEY_LOWERCASE_H;
    gLogicalKeyEntries[SDL_SCANCODE_H].shift = KEY_UPPERCASE_H;
    gLogicalKeyEntries[SDL_SCANCODE_H].lmenu = KEY_ALT_H;
    gLogicalKeyEntries[SDL_SCANCODE_H].rmenu = KEY_ALT_H;
    gLogicalKeyEntries[SDL_SCANCODE_H].ctrl = KEY_CTRL_H;

    gLogicalKeyEntries[SDL_SCANCODE_J].unmodified = KEY_LOWERCASE_J;
    gLogicalKeyEntries[SDL_SCANCODE_J].shift = KEY_UPPERCASE_J;
    gLogicalKeyEntries[SDL_SCANCODE_J].lmenu = KEY_ALT_J;
    gLogicalKeyEntries[SDL_SCANCODE_J].rmenu = KEY_ALT_J;
    gLogicalKeyEntries[SDL_SCANCODE_J].ctrl = KEY_CTRL_J;

    gLogicalKeyEntries[SDL_SCANCODE_K].unmodified = KEY_LOWERCASE_K;
    gLogicalKeyEntries[SDL_SCANCODE_K].shift = KEY_UPPERCASE_K;
    gLogicalKeyEntries[SDL_SCANCODE_K].lmenu = KEY_ALT_K;
    gLogicalKeyEntries[SDL_SCANCODE_K].rmenu = KEY_ALT_K;
    gLogicalKeyEntries[SDL_SCANCODE_K].ctrl = KEY_CTRL_K;

    gLogicalKeyEntries[SDL_SCANCODE_L].unmodified = KEY_LOWERCASE_L;
    gLogicalKeyEntries[SDL_SCANCODE_L].shift = KEY_UPPERCASE_L;
    gLogicalKeyEntries[SDL_SCANCODE_L].lmenu = KEY_ALT_L;
    gLogicalKeyEntries[SDL_SCANCODE_L].rmenu = KEY_ALT_L;
    gLogicalKeyEntries[SDL_SCANCODE_L].ctrl = KEY_CTRL_L;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_SEMICOLON;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_APOSTROPHE;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_3;
        break;
    default:
        k = SDL_SCANCODE_2;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[k].shift = KEY_QUOTE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RETURN].unmodified = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_RETURN].shift = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_RETURN].lmenu = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_RETURN].rmenu = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_RETURN].ctrl = KEY_CTRL_J;

    gLogicalKeyEntries[SDL_SCANCODE_LSHIFT].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LSHIFT].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LSHIFT].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LSHIFT].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LSHIFT].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_Z;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_W;
        break;
    default:
        k = SDL_SCANCODE_Y;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Z;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Z;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Z;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_Z;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Z;

    gLogicalKeyEntries[SDL_SCANCODE_X].unmodified = KEY_LOWERCASE_X;
    gLogicalKeyEntries[SDL_SCANCODE_X].shift = KEY_UPPERCASE_X;
    gLogicalKeyEntries[SDL_SCANCODE_X].lmenu = KEY_ALT_X;
    gLogicalKeyEntries[SDL_SCANCODE_X].rmenu = KEY_ALT_X;
    gLogicalKeyEntries[SDL_SCANCODE_X].ctrl = KEY_CTRL_X;

    gLogicalKeyEntries[SDL_SCANCODE_C].unmodified = KEY_LOWERCASE_C;
    gLogicalKeyEntries[SDL_SCANCODE_C].shift = KEY_UPPERCASE_C;
    gLogicalKeyEntries[SDL_SCANCODE_C].lmenu = KEY_ALT_C;
    gLogicalKeyEntries[SDL_SCANCODE_C].rmenu = KEY_ALT_C;
    gLogicalKeyEntries[SDL_SCANCODE_C].ctrl = KEY_CTRL_C;

    gLogicalKeyEntries[SDL_SCANCODE_V].unmodified = KEY_LOWERCASE_V;
    gLogicalKeyEntries[SDL_SCANCODE_V].shift = KEY_UPPERCASE_V;
    gLogicalKeyEntries[SDL_SCANCODE_V].lmenu = KEY_ALT_V;
    gLogicalKeyEntries[SDL_SCANCODE_V].rmenu = KEY_ALT_V;
    gLogicalKeyEntries[SDL_SCANCODE_V].ctrl = KEY_CTRL_V;

    gLogicalKeyEntries[SDL_SCANCODE_B].unmodified = KEY_LOWERCASE_B;
    gLogicalKeyEntries[SDL_SCANCODE_B].shift = KEY_UPPERCASE_B;
    gLogicalKeyEntries[SDL_SCANCODE_B].lmenu = KEY_ALT_B;
    gLogicalKeyEntries[SDL_SCANCODE_B].rmenu = KEY_ALT_B;
    gLogicalKeyEntries[SDL_SCANCODE_B].ctrl = KEY_CTRL_B;

    gLogicalKeyEntries[SDL_SCANCODE_N].unmodified = KEY_LOWERCASE_N;
    gLogicalKeyEntries[SDL_SCANCODE_N].shift = KEY_UPPERCASE_N;
    gLogicalKeyEntries[SDL_SCANCODE_N].lmenu = KEY_ALT_N;
    gLogicalKeyEntries[SDL_SCANCODE_N].rmenu = KEY_ALT_N;
    gLogicalKeyEntries[SDL_SCANCODE_N].ctrl = KEY_CTRL_N;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_M;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_M;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_M;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_M;
    gLogicalKeyEntries[k].rmenu = KEY_ALT_M;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_M;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_LESS;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_GREATER;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_SLASH;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_PERIOD;
        break;
    default:
        k = SDL_SCANCODE_7;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_SLASH;
    gLogicalKeyEntries[k].shift = KEY_QUESTION;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RSHIFT].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RSHIFT].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RSHIFT].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RSHIFT].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RSHIFT].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_LCTRL].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LCTRL].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LCTRL].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LCTRL].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LCTRL].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_LALT].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LALT].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LALT].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LALT].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LALT].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_SPACE].unmodified = KEY_SPACE;
    gLogicalKeyEntries[SDL_SCANCODE_SPACE].shift = KEY_SPACE;
    gLogicalKeyEntries[SDL_SCANCODE_SPACE].lmenu = KEY_SPACE;
    gLogicalKeyEntries[SDL_SCANCODE_SPACE].rmenu = KEY_SPACE;
    gLogicalKeyEntries[SDL_SCANCODE_SPACE].ctrl = KEY_SPACE;

    gLogicalKeyEntries[SDL_SCANCODE_RALT].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RALT].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RALT].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RALT].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RALT].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RCTRL].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RCTRL].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RCTRL].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RCTRL].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RCTRL].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_INSERT].unmodified = KEY_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_INSERT].shift = KEY_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_INSERT].lmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_INSERT].rmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_INSERT].ctrl = KEY_CTRL_INSERT;

    gLogicalKeyEntries[SDL_SCANCODE_HOME].unmodified = KEY_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_HOME].shift = KEY_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_HOME].lmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_HOME].rmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_HOME].ctrl = KEY_CTRL_HOME;

    gLogicalKeyEntries[SDL_SCANCODE_PRIOR].unmodified = KEY_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_PRIOR].shift = KEY_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_PRIOR].lmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_PRIOR].rmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_PRIOR].ctrl = KEY_CTRL_PAGE_UP;

    gLogicalKeyEntries[SDL_SCANCODE_DELETE].unmodified = KEY_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_DELETE].shift = KEY_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_DELETE].lmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_DELETE].rmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_DELETE].ctrl = KEY_CTRL_DELETE;

    gLogicalKeyEntries[SDL_SCANCODE_END].unmodified = KEY_END;
    gLogicalKeyEntries[SDL_SCANCODE_END].shift = KEY_END;
    gLogicalKeyEntries[SDL_SCANCODE_END].lmenu = KEY_ALT_END;
    gLogicalKeyEntries[SDL_SCANCODE_END].rmenu = KEY_ALT_END;
    gLogicalKeyEntries[SDL_SCANCODE_END].ctrl = KEY_CTRL_END;

    gLogicalKeyEntries[SDL_SCANCODE_PAGEDOWN].unmodified = KEY_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_PAGEDOWN].shift = KEY_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_PAGEDOWN].lmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_PAGEDOWN].rmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_PAGEDOWN].ctrl = KEY_CTRL_PAGE_DOWN;

    gLogicalKeyEntries[SDL_SCANCODE_UP].unmodified = KEY_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_UP].shift = KEY_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_UP].lmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_UP].rmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_UP].ctrl = KEY_CTRL_ARROW_UP;

    gLogicalKeyEntries[SDL_SCANCODE_DOWN].unmodified = KEY_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_DOWN].shift = KEY_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_DOWN].lmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_DOWN].rmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_DOWN].ctrl = KEY_CTRL_ARROW_DOWN;

    gLogicalKeyEntries[SDL_SCANCODE_LEFT].unmodified = KEY_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFT].shift = KEY_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFT].lmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFT].rmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFT].ctrl = KEY_CTRL_ARROW_LEFT;

    gLogicalKeyEntries[SDL_SCANCODE_RIGHT].unmodified = KEY_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHT].shift = KEY_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHT].lmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHT].rmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHT].ctrl = KEY_CTRL_ARROW_RIGHT;

    gLogicalKeyEntries[SDL_SCANCODE_NUMLOCKCLEAR].unmodified = -1;
    gLogicalKeyEntries[SDL_SCANCODE_NUMLOCKCLEAR].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_NUMLOCKCLEAR].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_NUMLOCKCLEAR].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_NUMLOCKCLEAR].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].unmodified = KEY_SLASH;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].shift = KEY_SLASH;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].ctrl = 3;

    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].unmodified = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].shift = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_MINUS].unmodified = KEY_MINUS;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MINUS].shift = KEY_MINUS;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MINUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MINUS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MINUS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_7].unmodified = KEY_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_KP_7].shift = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_KP_7].lmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_KP_7].rmenu = KEY_ALT_HOME;
    gLogicalKeyEntries[SDL_SCANCODE_KP_7].ctrl = KEY_CTRL_HOME;

    gLogicalKeyEntries[SDL_SCANCODE_KP_8].unmodified = KEY_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_8].shift = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_KP_8].lmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_8].rmenu = KEY_ALT_ARROW_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_8].ctrl = KEY_CTRL_ARROW_UP;

    gLogicalKeyEntries[SDL_SCANCODE_KP_9].unmodified = KEY_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_9].shift = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_KP_9].lmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_9].rmenu = KEY_ALT_PAGE_UP;
    gLogicalKeyEntries[SDL_SCANCODE_KP_9].ctrl = KEY_CTRL_PAGE_UP;

    gLogicalKeyEntries[SDL_SCANCODE_KP_PLUS].unmodified = KEY_PLUS;
    gLogicalKeyEntries[SDL_SCANCODE_KP_PLUS].shift = KEY_PLUS;
    gLogicalKeyEntries[SDL_SCANCODE_KP_PLUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_PLUS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_PLUS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_4].unmodified = KEY_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_4].shift = KEY_4;
    gLogicalKeyEntries[SDL_SCANCODE_KP_4].lmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_4].rmenu = KEY_ALT_ARROW_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_4].ctrl = KEY_CTRL_ARROW_LEFT;

    gLogicalKeyEntries[SDL_SCANCODE_KP_5].unmodified = KEY_NUMBERPAD_5;
    gLogicalKeyEntries[SDL_SCANCODE_KP_5].shift = KEY_5;
    gLogicalKeyEntries[SDL_SCANCODE_KP_5].lmenu = KEY_ALT_NUMBERPAD_5;
    gLogicalKeyEntries[SDL_SCANCODE_KP_5].rmenu = KEY_ALT_NUMBERPAD_5;
    gLogicalKeyEntries[SDL_SCANCODE_KP_5].ctrl = KEY_CTRL_NUMBERPAD_5;

    gLogicalKeyEntries[SDL_SCANCODE_KP_6].unmodified = KEY_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_6].shift = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_KP_6].lmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_6].rmenu = KEY_ALT_ARROW_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_6].ctrl = KEY_CTRL_ARROW_RIGHT;

    gLogicalKeyEntries[SDL_SCANCODE_KP_1].unmodified = KEY_END;
    gLogicalKeyEntries[SDL_SCANCODE_KP_1].shift = KEY_1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_1].lmenu = KEY_ALT_END;
    gLogicalKeyEntries[SDL_SCANCODE_KP_1].rmenu = KEY_ALT_END;
    gLogicalKeyEntries[SDL_SCANCODE_KP_1].ctrl = KEY_CTRL_END;

    gLogicalKeyEntries[SDL_SCANCODE_KP_2].unmodified = KEY_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_2].shift = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_KP_2].lmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_2].rmenu = KEY_ALT_ARROW_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_2].ctrl = KEY_CTRL_ARROW_DOWN;

    gLogicalKeyEntries[SDL_SCANCODE_KP_3].unmodified = KEY_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_3].shift = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_KP_3].lmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_3].rmenu = KEY_ALT_PAGE_DOWN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_3].ctrl = KEY_CTRL_PAGE_DOWN;

    gLogicalKeyEntries[SDL_SCANCODE_KP_ENTER].unmodified = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_ENTER].shift = KEY_RETURN;
    gLogicalKeyEntries[SDL_SCANCODE_KP_ENTER].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_ENTER].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_ENTER].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_0].unmodified = KEY_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_0].shift = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_KP_0].lmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_0].rmenu = KEY_ALT_INSERT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_0].ctrl = KEY_CTRL_INSERT;

    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].unmodified = KEY_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].shift = KEY_DOT;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].rmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].ctrl = KEY_CTRL_DELETE;
}

// 0x4D0400
static void keyboardBuildFrenchConfiguration()
{
    int k;

    keyboardBuildQwertyConfiguration();

    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].unmodified = KEY_178;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].shift = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_1].unmodified = KEY_AMPERSAND;
    gLogicalKeyEntries[SDL_SCANCODE_1].shift = KEY_1;
    gLogicalKeyEntries[SDL_SCANCODE_1].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_2].unmodified = KEY_233;
    gLogicalKeyEntries[SDL_SCANCODE_2].shift = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_2].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].rmenu = KEY_152;
    gLogicalKeyEntries[SDL_SCANCODE_2].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_3].unmodified = KEY_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_3].shift = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_3].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].rmenu = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[SDL_SCANCODE_3].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_4].unmodified = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_4].shift = KEY_4;
    gLogicalKeyEntries[SDL_SCANCODE_4].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_4].rmenu = KEY_BRACE_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_4].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_5].unmodified = KEY_PAREN_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_5].shift = KEY_5;
    gLogicalKeyEntries[SDL_SCANCODE_5].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_5].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_5].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_6].unmodified = KEY_150;
    gLogicalKeyEntries[SDL_SCANCODE_6].shift = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_6].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].rmenu = KEY_166;
    gLogicalKeyEntries[SDL_SCANCODE_6].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_7].unmodified = KEY_232;
    gLogicalKeyEntries[SDL_SCANCODE_7].shift = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_7].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].rmenu = KEY_GRAVE;
    gLogicalKeyEntries[SDL_SCANCODE_7].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_8].unmodified = KEY_UNDERSCORE;
    gLogicalKeyEntries[SDL_SCANCODE_8].shift = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_8].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].rmenu = KEY_BACKSLASH;
    gLogicalKeyEntries[SDL_SCANCODE_8].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_9].unmodified = KEY_231;
    gLogicalKeyEntries[SDL_SCANCODE_9].shift = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_9].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].rmenu = KEY_136;
    gLogicalKeyEntries[SDL_SCANCODE_9].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_0].unmodified = KEY_224;
    gLogicalKeyEntries[SDL_SCANCODE_0].shift = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_0].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].rmenu = KEY_AT;
    gLogicalKeyEntries[SDL_SCANCODE_0].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_MINUS].unmodified = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].shift = KEY_176;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_EQUALS;
        break;
    default:
        k = SDL_SCANCODE_0;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_EQUAL;
    gLogicalKeyEntries[k].shift = KEY_PLUS;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].unmodified = KEY_136;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].shift = KEY_168;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].unmodified = KEY_DOLLAR;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].shift = KEY_163;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].rmenu = KEY_164;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].unmodified = KEY_249;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].shift = KEY_PERCENT;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].unmodified = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].shift = KEY_181;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    // gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    // gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    // gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    // gLogicalKeyEntries[DIK_OEM_102].rmenu = -1;
    // gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_QUESTION;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_SEMICOLON;
    gLogicalKeyEntries[k].shift = KEY_DOT;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        // FIXME: Probably error, maps semicolon to colon on QWERTY keyboards.
        // Semicolon is already mapped above, so I bet it should be SDL_SCANCODE_COLON.
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COLON;
    gLogicalKeyEntries[k].shift = KEY_SLASH;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_SLASH].unmodified = KEY_EXCLAMATION;
    gLogicalKeyEntries[SDL_SCANCODE_SLASH].shift = KEY_167;
    gLogicalKeyEntries[SDL_SCANCODE_SLASH].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SLASH].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SLASH].ctrl = -1;
}

// 0x4D0C54
static void keyboardBuildGermanConfiguration()
{
    int k;

    keyboardBuildQwertyConfiguration();

    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].unmodified = KEY_136;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].shift = KEY_186;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_2].unmodified = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_2].shift = KEY_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_2].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].rmenu = KEY_178;
    gLogicalKeyEntries[SDL_SCANCODE_2].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_3].unmodified = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_3].shift = KEY_167;
    gLogicalKeyEntries[SDL_SCANCODE_3].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].rmenu = KEY_179;
    gLogicalKeyEntries[SDL_SCANCODE_3].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_6].unmodified = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_6].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[SDL_SCANCODE_6].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_7].unmodified = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_7].shift = KEY_166;
    gLogicalKeyEntries[SDL_SCANCODE_7].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].rmenu = KEY_BRACE_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_7].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_8].unmodified = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_8].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_8].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_8].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_9].unmodified = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_9].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_9].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_9].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_0].unmodified = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_0].shift = KEY_EQUAL;
    gLogicalKeyEntries[SDL_SCANCODE_0].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].rmenu = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_0].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_MINUS].unmodified = KEY_223;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].shift = KEY_QUESTION;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].rmenu = KEY_BACKSLASH;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].unmodified = KEY_180;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].shift = KEY_GRAVE;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_A;
        break;
    default:
        k = SDL_SCANCODE_Q;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_Q;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_Q;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_Q;
    gLogicalKeyEntries[k].rmenu = KEY_AT;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_Q;

    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].unmodified = KEY_252;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].shift = KEY_220;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_EQUALS;
        break;
    default:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_PLUS;
    gLogicalKeyEntries[k].shift = KEY_ASTERISK;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = KEY_152;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].unmodified = KEY_246;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].shift = KEY_214;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].unmodified = KEY_228;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].shift = KEY_196;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].unmodified = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].shift = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    // gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    // gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    // gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    // gLogicalKeyEntries[DIK_OEM_102].rmenu = KEY_166;
    // gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_M;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_LOWERCASE_M;
    gLogicalKeyEntries[k].shift = KEY_UPPERCASE_M;
    gLogicalKeyEntries[k].lmenu = KEY_ALT_M;
    gLogicalKeyEntries[k].rmenu = KEY_181;
    gLogicalKeyEntries[k].ctrl = KEY_CTRL_M;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_SEMICOLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_150;
    gLogicalKeyEntries[k].shift = KEY_151;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].unmodified = KEY_247;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].shift = KEY_247;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DIVIDE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].unmodified = KEY_215;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].shift = KEY_215;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_MULTIPLY].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].unmodified = KEY_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].shift = KEY_COMMA;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].rmenu = KEY_ALT_DELETE;
    gLogicalKeyEntries[SDL_SCANCODE_KP_DECIMAL].ctrl = KEY_CTRL_DELETE;
}

// 0x4D1758
static void keyboardBuildItalianConfiguration()
{
    int k;

    keyboardBuildQwertyConfiguration();

    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].unmodified = KEY_BACKSLASH;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].shift = KEY_BAR;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].ctrl = -1;

    // gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    // gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    // gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    // gLogicalKeyEntries[DIK_OEM_102].rmenu = -1;
    // gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_1].unmodified = KEY_1;
    gLogicalKeyEntries[SDL_SCANCODE_1].shift = KEY_EXCLAMATION;
    gLogicalKeyEntries[SDL_SCANCODE_1].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_2].unmodified = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_2].shift = KEY_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_2].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_3].unmodified = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_3].shift = KEY_163;
    gLogicalKeyEntries[SDL_SCANCODE_3].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_6].unmodified = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_6].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[SDL_SCANCODE_6].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_7].unmodified = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_7].shift = KEY_SLASH;
    gLogicalKeyEntries[SDL_SCANCODE_7].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_8].unmodified = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_8].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_8].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_9].unmodified = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_9].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_9].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_0].unmodified = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_0].shift = KEY_EQUAL;
    gLogicalKeyEntries[SDL_SCANCODE_0].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_MINUS].unmodified = KEY_SINGLE_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].shift = KEY_QUESTION;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].unmodified = KEY_232;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].shift = KEY_233;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].unmodified = KEY_PLUS;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].shift = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].unmodified = KEY_249;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].shift = KEY_167;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_SEMICOLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_MINUS;
    gLogicalKeyEntries[k].shift = KEY_UNDERSCORE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;
}

// 0x4D1E24
static void keyboardBuildSpanishConfiguration()
{
    int k;

    keyboardBuildQwertyConfiguration();

    gLogicalKeyEntries[SDL_SCANCODE_1].unmodified = KEY_1;
    gLogicalKeyEntries[SDL_SCANCODE_1].shift = KEY_EXCLAMATION;
    gLogicalKeyEntries[SDL_SCANCODE_1].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_1].rmenu = KEY_BAR;
    gLogicalKeyEntries[SDL_SCANCODE_1].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_2].unmodified = KEY_2;
    gLogicalKeyEntries[SDL_SCANCODE_2].shift = KEY_QUOTE;
    gLogicalKeyEntries[SDL_SCANCODE_2].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_2].rmenu = KEY_AT;
    gLogicalKeyEntries[SDL_SCANCODE_2].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_3].unmodified = KEY_3;
    gLogicalKeyEntries[SDL_SCANCODE_3].shift = KEY_149;
    gLogicalKeyEntries[SDL_SCANCODE_3].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_3].rmenu = KEY_NUMBER_SIGN;
    gLogicalKeyEntries[SDL_SCANCODE_3].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_6].unmodified = KEY_6;
    gLogicalKeyEntries[SDL_SCANCODE_6].shift = KEY_AMPERSAND;
    gLogicalKeyEntries[SDL_SCANCODE_6].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_6].rmenu = KEY_172;
    gLogicalKeyEntries[SDL_SCANCODE_6].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_7].unmodified = KEY_7;
    gLogicalKeyEntries[SDL_SCANCODE_7].shift = KEY_SLASH;
    gLogicalKeyEntries[SDL_SCANCODE_7].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_7].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_8].unmodified = KEY_8;
    gLogicalKeyEntries[SDL_SCANCODE_8].shift = KEY_PAREN_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_8].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_8].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_9].unmodified = KEY_9;
    gLogicalKeyEntries[SDL_SCANCODE_9].shift = KEY_PAREN_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_9].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_9].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_0].unmodified = KEY_0;
    gLogicalKeyEntries[SDL_SCANCODE_0].shift = KEY_EQUAL;
    gLogicalKeyEntries[SDL_SCANCODE_0].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_0].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_MINUS].unmodified = KEY_146;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].shift = KEY_QUESTION;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_MINUS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].unmodified = KEY_161;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].shift = KEY_191;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_EQUALS].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].unmodified = KEY_176;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].shift = KEY_170;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].rmenu = KEY_BACKSLASH;
    gLogicalKeyEntries[SDL_SCANCODE_GRAVE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].unmodified = KEY_GRAVE;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].shift = KEY_CARET;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].rmenu = KEY_BRACKET_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].unmodified = KEY_PLUS;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].shift = KEY_ASTERISK;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].rmenu = KEY_BRACKET_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_RIGHTBRACKET].ctrl = -1;

    // gLogicalKeyEntries[DIK_OEM_102].unmodified = KEY_LESS;
    // gLogicalKeyEntries[DIK_OEM_102].shift = KEY_GREATER;
    // gLogicalKeyEntries[DIK_OEM_102].lmenu = -1;
    // gLogicalKeyEntries[DIK_OEM_102].rmenu = -1;
    // gLogicalKeyEntries[DIK_OEM_102].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].unmodified = KEY_241;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].shift = KEY_209;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].rmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_SEMICOLON].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].unmodified = KEY_168;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].shift = KEY_180;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].rmenu = KEY_BRACE_LEFT;
    gLogicalKeyEntries[SDL_SCANCODE_APOSTROPHE].ctrl = -1;

    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].unmodified = KEY_231;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].shift = KEY_199;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].lmenu = -1;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].rmenu = KEY_BRACE_RIGHT;
    gLogicalKeyEntries[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_COMMA;
    gLogicalKeyEntries[k].shift = KEY_SEMICOLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_DOT;
    gLogicalKeyEntries[k].shift = KEY_COLON;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;

    switch (gKeyboardLayout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    gLogicalKeyEntries[k].unmodified = KEY_MINUS;
    gLogicalKeyEntries[k].shift = KEY_UNDERSCORE;
    gLogicalKeyEntries[k].lmenu = -1;
    gLogicalKeyEntries[k].rmenu = -1;
    gLogicalKeyEntries[k].ctrl = -1;
}

// 0x4D24F8
static void _kb_init_lock_status()
{
    if ((SDL_GetModState() & KMOD_CAPS) != 0) {
        gModifierKeysState |= MODIFIER_KEY_STATE_CAPS_LOCK;
    }

    if ((SDL_GetModState() & KMOD_NUM) != 0) {
        gModifierKeysState |= MODIFIER_KEY_STATE_NUM_LOCK;
    }

#if SDL_VERSION_ATLEAST(2, 0, 18)
    if ((SDL_GetModState() & KMOD_SCROLL) != 0) {
        gModifierKeysState |= MODIFIER_KEY_STATE_SCROLL_LOCK;
    }
#endif
}

// Get pointer to pending key event from the queue but do not consume it.
//
// 0x4D2614
static int keyboardPeekEvent(int index, KeyboardEvent** keyboardEventPtr)
{
    int rc = -1;

    if (gKeyboardEventQueueReadIndex != gKeyboardEventQueueWriteIndex) {
        int end;
        if (gKeyboardEventQueueWriteIndex <= gKeyboardEventQueueReadIndex) {
            end = gKeyboardEventQueueWriteIndex + KEY_QUEUE_SIZE - gKeyboardEventQueueReadIndex - 1;
        } else {
            end = gKeyboardEventQueueWriteIndex - gKeyboardEventQueueReadIndex - 1;
        }

        if (index <= end) {
            int eventIndex = (gKeyboardEventQueueReadIndex + index) & (KEY_QUEUE_SIZE - 1);
            *keyboardEventPtr = &(gKeyboardEventsQueue[eventIndex]);
            rc = 0;
        }
    }

    return rc;
}

} // namespace fallout
