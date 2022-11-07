#include "sfall_opcodes.h"

#include "art.h"
#include "interface.h"
#include "interpreter.h"
#include "mouse.h"
#include "svga.h"

namespace fallout {

// active_hand
static void opGetCurrentHand(Program* program)
{
    programStackPushInteger(program, interfaceGetCurrentHand());
}

// get_mouse_x
static void opGetMouseX(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);
    programStackPushInteger(program, x);
}

// get_mouse_y
static void opGetMouseY(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);
    programStackPushInteger(program, y);
}

// get_screen_width
static void opGetScreenWidth(Program* program)
{
    programStackPushInteger(program, screenGetWidth());
}

// get_screen_height
static void opGetScreenHeight(Program* program)
{
    programStackPushInteger(program, screenGetHeight());
}

// atoi
static void opParseInt(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, static_cast<int>(strtol(string, nullptr, 0)));
}

// strlen
static void opGetStringLength(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, static_cast<int>(strlen(string)));
}

// round
static void opRound(Program* program)
{
    float floatValue = programStackPopFloat(program);
    int integerValue = static_cast<int>(floatValue);
    float mod = floatValue - static_cast<float>(integerValue);
    if (abs(mod) >= 0.5) {
        integerValue += mod > 0.0 ? 1 : -1;
    }
    programStackPushInteger(program, integerValue);
}

// art_exists
static void opArtExists(Program* program)
{
    int fid = programStackPopInteger(program);
    programStackPushInteger(program, artExists(fid));
}

void sfallOpcodesInit()
{
    interpreterRegisterOpcode(0x8193, opGetCurrentHand);
    interpreterRegisterOpcode(0x821C, opGetMouseX);
    interpreterRegisterOpcode(0x821D, opGetMouseY);
    interpreterRegisterOpcode(0x8220, opGetScreenWidth);
    interpreterRegisterOpcode(0x8221, opGetScreenHeight);
    interpreterRegisterOpcode(0x8237, opParseInt);
    interpreterRegisterOpcode(0x824F, opGetStringLength);
    interpreterRegisterOpcode(0x8267, opRound);
    interpreterRegisterOpcode(0x8274, opArtExists);
}

void sfallOpcodesExit()
{
}

} // namespace fallout
