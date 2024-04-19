#include "debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "memory.h"
#include "platform_compat.h"
#include "window_manager_private.h"

namespace fallout {

static int _debug_puts(char* string);
static void _debug_clear();
static int _debug_mono(char* string);
static int _debug_log(char* string);
static int _debug_screen(char* string);
static void _debug_putc(int ch);
static void _debug_scroll();

// 0x51DEF8
static FILE* _fd = nullptr;

// 0x51DEFC
static int _curx = 0;

// 0x51DF00
static int _cury = 0;

// 0x51DF04
static DebugPrintProc* gDebugPrintProc = nullptr;

// 0x4C6CD0
void _GNW_debug_init()
{
    atexit(_debug_exit);
}

// 0x4C6CDC
void _debug_register_mono()
{
    if (gDebugPrintProc != _debug_mono) {
        if (_fd != nullptr) {
            fclose(_fd);
            _fd = nullptr;
        }

        gDebugPrintProc = _debug_mono;
        _debug_clear();
    }
}

// 0x4C6D18
void _debug_register_log(const char* fileName, const char* mode)
{
    if ((mode[0] == 'w' || mode[0] == 'a') && mode[1] == 't') {
        if (_fd != nullptr) {
            fclose(_fd);
        }

        _fd = compat_fopen(fileName, mode);
        gDebugPrintProc = _debug_log;
    }
}

// 0x4C6D5C
void _debug_register_screen()
{
    if (gDebugPrintProc != _debug_screen) {
        if (_fd != nullptr) {
            fclose(_fd);
            _fd = nullptr;
        }

        gDebugPrintProc = _debug_screen;
    }
}

// 0x4C6D90
void _debug_register_env()
{
    const char* type = getenv("DEBUGACTIVE");
    if (type == nullptr) {
        return;
    }

    char* copy = (char*)internal_malloc(strlen(type) + 1);
    if (copy == nullptr) {
        return;
    }

    strcpy(copy, type);
    compat_strlwr(copy);

    if (strcmp(copy, "mono") == 0) {
        // NOTE: Uninline.
        _debug_register_mono();
    } else if (strcmp(copy, "log") == 0) {
        _debug_register_log("debug.log", "wt");
    } else if (strcmp(copy, "screen") == 0) {
        // NOTE: Uninline.
        _debug_register_screen();
    } else if (strcmp(copy, "gnw") == 0) {
        // NOTE: Uninline.
        _debug_register_func(_win_debug);
    }

    internal_free(copy);
}

// 0x4C6F18
void _debug_register_func(DebugPrintProc* proc)
{
    if (gDebugPrintProc != proc) {
        if (_fd != nullptr) {
            fclose(_fd);
            _fd = nullptr;
        }

        gDebugPrintProc = proc;
    }
}

// 0x4C6F48
int debugPrint(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    int rc;

    if (gDebugPrintProc != nullptr) {
        char string[260];
        vsnprintf(string, sizeof(string), format, args);

        rc = gDebugPrintProc(string);
    } else {
#ifdef _DEBUG
        SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, format, args);
#endif
        rc = -1;
    }

    va_end(args);

    return rc;
}

// 0x4C6F94
static int _debug_puts(char* string)
{
    if (gDebugPrintProc != nullptr) {
        return gDebugPrintProc(string);
    }

    return -1;
}

// 0x4C6FAC
static void _debug_clear()
{
    char* buffer;
    int x;
    int y;

    buffer = nullptr;

    if (gDebugPrintProc == _debug_mono) {
        buffer = (char*)0xB0000;
    } else if (gDebugPrintProc == _debug_screen) {
        buffer = (char*)0xB8000;
    }

    if (buffer != nullptr) {
        for (y = 0; y < 25; y++) {
            for (x = 0; x < 80; x++) {
                *buffer++ = ' ';
                *buffer++ = 7;
            }
        }
        _cury = 0;
        _curx = 0;
    }
}

// 0x4C7004
static int _debug_mono(char* string)
{
    if (gDebugPrintProc == _debug_mono) {
        while (*string != '\0') {
            char ch = *string++;
            _debug_putc(ch);
        }
    }
    return 0;
}

// 0x4C7028
static int _debug_log(char* string)
{
    if (gDebugPrintProc == _debug_log) {
        if (_fd == nullptr) {
            return -1;
        }

        if (fprintf(_fd, "%s", string) < 0) {
            return -1;
        }

        if (fflush(_fd) == EOF) {
            return -1;
        }
    }

    return 0;
}

// 0x4C7068
static int _debug_screen(char* string)
{
    if (gDebugPrintProc == _debug_screen) {
        printf("%s", string);
    }

    return 0;
}

// 0x4C709C
static void _debug_putc(int ch)
{
    char* buffer;

    buffer = (char*)0xB0000;

    switch (ch) {
    case 7:
        printf("\x07");
        return;
    case 8:
        if (_curx > 0) {
            _curx--;
            buffer += 2 * _curx + 2 * 80 * _cury;
            *buffer++ = ' ';
            *buffer = 7;
        }
        return;
    case 9:
        do {
            _debug_putc(' ');
        } while ((_curx - 1) % 4 != 0);
        return;
    case 13:
        _curx = 0;
        return;
    default:
        buffer += 2 * _curx + 2 * 80 * _cury;
        *buffer++ = ch;
        *buffer = 7;
        _curx++;
        if (_curx < 80) {
            return;
        }
        // FALLTHROUGH
    case 10:
        _curx = 0;
        _cury++;
        if (_cury > 24) {
            _cury = 24;
            _debug_scroll();
        }
        return;
    }
}

// 0x4C71AC
static void _debug_scroll()
{
    char* buffer;
    int x;
    int y;

    buffer = (char*)0xB0000;

    for (y = 0; y < 24; y++) {
        for (x = 0; x < 80 * 2; x++) {
            buffer[0] = buffer[80 * 2];
            buffer++;
        }
    }

    for (x = 0; x < 80; x++) {
        *buffer++ = ' ';
        *buffer++ = 7;
    }
}

// 0x4C71E8
void _debug_exit(void)
{
    if (_fd != nullptr) {
        fclose(_fd);
    }
}

} // namespace fallout
