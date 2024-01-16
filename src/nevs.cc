#include "nevs.h"

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "interpreter_lib.h"
#include "memory_manager.h"
#include "platform_compat.h"

namespace fallout {

#define NEVS_COUNT 40

typedef struct Nevs {
    bool used;
    char name[32];
    Program* program;
    int proc;
    int type;
    int hits;
    bool busy;
    void (*field_38)();
} Nevs;

static Nevs* _nevs_alloc();
static void _nevs_reset(Nevs* nevs);
static void _nevs_removeprogramreferences(Program* program);
static Nevs* _nevs_find(const char* name);

// 0x6391C8
static Nevs* gNevs;

// 0x6391CC
static int gNevsHits;

// nevs_alloc
// 0x488340
static Nevs* _nevs_alloc()
{
    if (gNevs == nullptr) {
        debugPrint("nevs_alloc(): nevs_initonce() not called!");
        exit(99);
    }

    for (int index = 0; index < NEVS_COUNT; index++) {
        Nevs* nevs = &(gNevs[index]);
        if (!nevs->used) {
            // NOTE: Uninline.
            _nevs_reset(nevs);
            return nevs;
        }
    }

    return nullptr;
}

// NOTE: Inlined.
//
// 0x488394
static void _nevs_reset(Nevs* nevs)
{
    nevs->used = false;
    memset(nevs, 0, sizeof(*nevs));
}

// 0x4883AC
void _nevs_close()
{
    if (gNevs != nullptr) {
        internal_free_safe(gNevs, __FILE__, __LINE__); // "..\\int\\NEVS.C", 97
        gNevs = nullptr;
    }
}

// 0x4883D4
static void _nevs_removeprogramreferences(Program* program)
{
    if (gNevs != nullptr) {
        for (int i = 0; i < NEVS_COUNT; i++) {
            Nevs* nevs = &(gNevs[i]);
            if (nevs->used && nevs->program == program) {
                // NOTE: Uninline.
                _nevs_reset(nevs);
            }
        }
    }
}

// nevs_initonce
// 0x488418
void _nevs_initonce()
{
    intLibRegisterProgramDeleteCallback(_nevs_removeprogramreferences);

    if (gNevs == nullptr) {
        gNevs = (Nevs*)internal_calloc_safe(sizeof(Nevs), NEVS_COUNT, __FILE__, __LINE__); // "..\\int\\NEVS.C", 131
        if (gNevs == nullptr) {
            debugPrint("nevs_initonce(): out of memory");
            exit(99);
        }
    }
}

// nevs_find
// 0x48846C
static Nevs* _nevs_find(const char* name)
{
    if (gNevs == nullptr) {
        debugPrint("nevs_find(): nevs_initonce() not called!");
        exit(99);
    }

    for (int index = 0; index < NEVS_COUNT; index++) {
        Nevs* nevs = &(gNevs[index]);
        if (nevs->used && compat_stricmp(nevs->name, name) == 0) {
            return nevs;
        }
    }

    return nullptr;
}

// 0x4884C8
int _nevs_addevent(const char* name, Program* program, int proc, int type)
{
    Nevs* nevs = _nevs_find(name);
    if (nevs == nullptr) {
        nevs = _nevs_alloc();
    }

    if (nevs == nullptr) {
        return 1;
    }

    nevs->used = true;
    strcpy(nevs->name, name);
    nevs->program = program;
    nevs->proc = proc;
    nevs->type = type;
    nevs->field_38 = nullptr;

    return 0;
}

// nevs_clearevent
// 0x48859C
int _nevs_clearevent(const char* a1)
{
    debugPrint("nevs_clearevent( '%s');\n", a1);

    Nevs* nevs = _nevs_find(a1);
    if (nevs != nullptr) {
        // NOTE: Uninline.
        _nevs_reset(nevs);
        return 0;
    }

    return 1;
}

// nevs_signal
// 0x48862C
int _nevs_signal(const char* name)
{
    debugPrint("nevs_signal( '%s');\n", name);

    Nevs* nevs = _nevs_find(name);
    if (nevs == nullptr) {
        return 1;
    }

    debugPrint("nep: %p,  used = %u, prog = %p, proc = %d", nevs, nevs->used, nevs->program, nevs->proc);

    if (nevs->used
        && ((nevs->program != nullptr && nevs->proc != 0) || nevs->field_38 != nullptr)
        && !nevs->busy) {
        nevs->hits++;
        gNevsHits++;
        return 0;
    }

    return 1;
}

// nevs_update
// 0x4886AC
void _nevs_update()
{
    if (gNevsHits == 0) {
        return;
    }

    debugPrint("nevs_update(): we have anyhits = %u\n", gNevsHits);

    gNevsHits = 0;

    for (int index = 0; index < NEVS_COUNT; index++) {
        Nevs* nevs = &(gNevs[index]);
        if (nevs->used
            && ((nevs->program != nullptr && nevs->proc != 0) || nevs->field_38 != nullptr)
            && !nevs->busy) {
            if (nevs->hits > 0) {
                nevs->busy = true;

                nevs->hits -= 1;
                gNevsHits += nevs->hits;

                if (nevs->field_38 == nullptr) {
                    _executeProc(nevs->program, nevs->proc);
                } else {
                    nevs->field_38();
                }

                nevs->busy = false;

                if (nevs->type == NEVS_TYPE_EVENT) {
                    // NOTE: Uninline.
                    _nevs_reset(nevs);
                }
            }
        }
    }
}

} // namespace fallout
