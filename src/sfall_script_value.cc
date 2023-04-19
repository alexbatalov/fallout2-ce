
#include "sfall_script_value.h"

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
    // Assuming that pointer is the biggest in size
    static_assert(sizeof(decltype(value.floatValue)) <= sizeof(decltype(value.pointerValue)));
    static_assert(sizeof(decltype(value.integerValue)) <= sizeof(decltype(value.pointerValue)));
    opcode = value.opcode;
    pointerValue = value.pointerValue;
    // TODO: If type is string then copy string
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