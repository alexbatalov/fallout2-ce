#include "sfall_global_vars.h"

#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace fallout {

struct SfallGlobalVarsState {
    std::unordered_map<uint64_t, int> vars;
};

static bool sfallGlobalVarsStore(uint64_t key, int value);
static bool sfallGlobalVarsFetch(uint64_t key, int& value);

static SfallGlobalVarsState* _state;

bool sfallGlobalVarsInit()
{
    _state = new (std::nothrow) SfallGlobalVarsState();
    if (_state == nullptr) {
        return false;
    }

    return true;
}

void sfallGlobalVarsReset()
{
    _state->vars.clear();
}

void sfallGlobalVarsExit()
{
    if (_state != nullptr) {
        delete _state;
        _state = nullptr;
    }
}

bool sfallGlobalVarsStore(const char* key, int value)
{
    if (strlen(key) != 8) {
        return false;
    }

    uint64_t numericKey = *(reinterpret_cast<const uint64_t*>(key));
    return sfallGlobalVarsStore(numericKey, value);
}

bool sfallGlobalVarsStore(int key, int value)
{
    return sfallGlobalVarsStore(static_cast<uint64_t>(key), value);
}

bool sfallGlobalVarsFetch(const char* key, int& value)
{
    if (strlen(key) != 8) {
        return false;
    }

    uint64_t numericKey = *(reinterpret_cast<const uint64_t*>(key));
    return sfallGlobalVarsFetch(numericKey, value);
}

bool sfallGlobalVarsFetch(int key, int& value)
{
    return sfallGlobalVarsFetch(static_cast<uint64_t>(key), value);
}

static bool sfallGlobalVarsStore(uint64_t key, int value)
{
    auto it = _state->vars.find(key);
    if (it == _state->vars.end()) {
        _state->vars.emplace(key, value);
    } else {
        if (value == 0) {
            _state->vars.erase(it);
        } else {
            it->second = value;
        }
    }

    return true;
}

static bool sfallGlobalVarsFetch(uint64_t key, int& value)
{
    auto it = _state->vars.find(key);
    if (it == _state->vars.end()) {
        return false;
    }

    value = it->second;

    return true;
}

} // namespace fallout
