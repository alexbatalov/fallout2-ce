#ifndef SFALL_SCRIPT_VALUE
#define SFALL_SCRIPT_VALUE

#include "interpreter.h"
#include "object.h"

namespace fallout {

class SFallScriptValue : public ProgramValue {
public:
    SFallScriptValue();
    SFallScriptValue(int value);
    SFallScriptValue(Object* value);
    SFallScriptValue(const ProgramValue& value);

    bool isInt() const;
    bool isFloat() const;
    bool isPointer() const;
    int asInt() const;

    bool operator<(SFallScriptValue const& other) const;
    bool operator==(SFallScriptValue const& other) const;
};

}

#endif /* SFALL_SCRIPT_VALUE */