
#include "sfall_script_value.h"
#include <utility>

namespace fallout {
SFallScriptValue::SFallScriptValue()
{
    opcode = VALUE_TYPE_INT;
    integerValue = 0;
}
SFallScriptValue::SFallScriptValue(int value)
{
    opcode = VALUE_TYPE_INT;
    integerValue = value;
};
SFallScriptValue::SFallScriptValue(Object* value)
{
    opcode = VALUE_TYPE_PTR;
    pointerValue = value;
};
SFallScriptValue::SFallScriptValue(ProgramValue& value)
{
    opcode = value.opcode;

    switch (opcode) {
    case VALUE_TYPE_DYNAMIC_STRING:
    case VALUE_TYPE_STRING:
        // TODO: Copy string
        pointerValue = value.pointerValue;
        break;
    case VALUE_TYPE_PTR:
        pointerValue = value.pointerValue;
        break;
    case VALUE_TYPE_INT:
        integerValue = value.integerValue;
        break;
    case VALUE_TYPE_FLOAT:
        floatValue = value.floatValue;
        break;
    default:
        throw(std::exception());
    }
}

SFallScriptValue::~SFallScriptValue()
{
    // TODO: If type is string then free it
}

bool SFallScriptValue::isInt() const
{
    return opcode == VALUE_TYPE_INT;
}
bool SFallScriptValue::isFloat() const
{
    return opcode == VALUE_TYPE_FLOAT;
}
bool SFallScriptValue::isPointer() const
{
    return opcode == VALUE_TYPE_PTR;
}

int SFallScriptValue::asInt() const
{
    switch (opcode) {
    case VALUE_TYPE_INT:
        return integerValue;
    case VALUE_TYPE_FLOAT:
        return static_cast<int>(floatValue);
    default:
        return 0;
    }
}

}