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
    SFallScriptValue(ProgramValue& value);

    SFallScriptValue(SFallScriptValue&& other) noexcept; // move constructor    
 
    bool isInt() const;
    bool isFloat() const;
    bool isPointer() const;
    int asInt() const;

    ~SFallScriptValue();
};

}

#endif /* SFALL_SCRIPT_VALUE */