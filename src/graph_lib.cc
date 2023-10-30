#include "graph_lib.h"

#include <string.h>

#include <algorithm>

#include "color.h"
#include "debug.h"
#include "memory.h"

namespace fallout {

static void _InitTree();
static void _InsertNode(int a1);
static void _DeleteNode(int a1);

// 0x596D90
static unsigned char _GreyTable[256];

// 0x596E90
static int* _dad_2;

// 0x596E94
static int _match_length;

// 0x596E98
static int _textsize;

// 0x596E9C
static int* _rson;

// 0x596EA0
static int* _lson;

// 0x596EA4
static unsigned char* _text_buf;

// 0x596EA8
static int _codesize;

// 0x596EAC
static int _match_position;

// 0x44EBC0
unsigned char HighRGB(unsigned char color)
{
    int rgb = Color2RGB(color);
    int r = (rgb & 0x7C00) >> 10;
    int g = (rgb & 0x3E0) >> 5;
    int b = (rgb & 0x1F);

    return std::max(std::max(r, g), b);
}

// 0x44ED98
int load_lbm_to_buf(const char* path, unsigned char* buffer, int a3, int a4, int a5, int a6, int a7)
{
    // TODO: Incomplete.

    return -1;
}

// 0x44F250
int graphCompress(unsigned char* a1, unsigned char* a2, int a3)
{
    _dad_2 = nullptr;
    _rson = nullptr;
    _lson = nullptr;
    _text_buf = nullptr;

    // NOTE: Original code is slightly different, it uses deep nesting or a
    // bunch of gotos.
    _lson = (int*)internal_malloc(sizeof(*_lson) * 4104);
    _rson = (int*)internal_malloc(sizeof(*_rson) * 4376);
    _dad_2 = (int*)internal_malloc(sizeof(*_dad_2) * 4104);
    _text_buf = (unsigned char*)internal_malloc(sizeof(*_text_buf) * 4122);

    if (_lson == nullptr || _rson == nullptr || _dad_2 == nullptr || _text_buf == nullptr) {
        debugPrint("\nGRAPHLIB: Error allocating compression buffers!\n");

        if (_dad_2 != nullptr) {
            internal_free(_dad_2);
        }

        if (_rson != nullptr) {
            internal_free(_rson);
        }
        if (_lson != nullptr) {
            internal_free(_lson);
        }
        if (_text_buf != nullptr) {
            internal_free(_text_buf);
        }

        return -1;
    }

    _InitTree();

    memset(_text_buf, ' ', 4078);

    int count = 0;
    int v30 = 0;
    for (int index = 4078; index < 4096; index++) {
        _text_buf[index] = *a1++;
        int v8 = v30++;
        if (v8 > a3) {
            break;
        }
        count++;
    }

    _textsize = count;

    for (int index = 4077; index > 4059; index--) {
        _InsertNode(index);
    }

    _InsertNode(4078);

    unsigned char v29[32];
    v29[1] = 0;

    int v3 = 4078;
    int v4 = 0;
    int v10 = 0;
    int v36 = 1;
    unsigned char v41 = 1;
    int rc = 0;
    while (count != 0) {
        if (count < _match_length) {
            _match_length = count;
        }

        int v11 = v36 + 1;
        if (_match_length > 2) {
            v29[v36 + 1] = _match_position;
            v29[v36 + 2] = ((_match_length - 3) | ((_match_position >> 4) & 0xF0));
            v36 = v11 + 1;
        } else {
            _match_length = 1;
            v29[1] |= v41;
            int v13 = v36++;
            v29[v13 + 1] = _text_buf[v3];
        }

        v41 *= 2;

        if (v41 == 0) {
            v11 = 0;
            if (v36 != 0) {
                for (;;) {
                    v4++;
                    *a2++ = v29[v11 + 1];
                    if (v4 > a3) {
                        rc = -1;
                        break;
                    }

                    v11++;
                    if (v11 >= v36) {
                        break;
                    }
                }

                if (rc == -1) {
                    break;
                }
            }

            _codesize += v36;
            v29[1] = 0;
            v36 = 1;
            v41 = 1;
        }

        int v16;
        int v38 = _match_length;
        for (v16 = 0; v16 < v38; v16++) {
            unsigned char v34 = *a1++;
            int v17 = v30++;
            if (v17 >= a3) {
                break;
            }

            _DeleteNode(v10);

            unsigned char* v19 = _text_buf + v10;
            _text_buf[v10] = v34;

            if (v10 < 17) {
                v19[4096] = v34;
            }

            v3 = (v3 + 1) & 0xFFF;
            v10 = (v10 + 1) & 0xFFF;
            _InsertNode(v3);
        }

        for (; v16 < v38; v16++) {
            _DeleteNode(v10);
            v3 = (v3 + 1) & 0xFFF;
            v10 = (v10 + 1) & 0xFFF;
            if (--count != 0) {
                _InsertNode(v3);
            }
        }
    }

    if (rc != -1) {
        for (int v23 = 0; v23 < v36; v23++) {
            v4++;
            v10++;
            *a2++ = v29[v23 + 1];
            if (v10 > a3) {
                rc = -1;
                break;
            }
        }

        _codesize += v36;
    }

    internal_free(_lson);
    internal_free(_rson);
    internal_free(_dad_2);
    internal_free(_text_buf);

    if (rc == -1) {
        v4 = -1;
    }

    return v4;
}

// 0x44F5F0
static void _InitTree()
{
    for (int index = 4097; index < 4353; index++) {
        _rson[index] = 4096;
    }

    for (int index = 0; index < 4096; index++) {
        _dad_2[index] = 4096;
    }
}

// 0x44F63C
static void _InsertNode(int a1)
{
    _lson[a1] = 4096;
    _rson[a1] = 4096;
    _match_length = 0;

    unsigned char* v2 = _text_buf + a1;

    int v21 = 4097 + _text_buf[a1];
    int v5 = 1;
    for (;;) {
        int v6 = v21;
        if (v5 < 0) {
            if (_lson[v6] == 4096) {
                _lson[v6] = a1;
                _dad_2[a1] = v21;
                return;
            }
            v21 = _lson[v6];
        } else {
            if (_rson[v6] == 4096) {
                _rson[v6] = a1;
                _dad_2[a1] = v21;
                return;
            }
            v21 = _rson[v6];
        }

        int v9;
        unsigned char* v10 = v2 + 1;
        int v11 = v21 + 1;
        for (v9 = 1; v9 < 18; v9++) {
            v5 = *v10 - _text_buf[v11];
            if (v5 != 0) {
                break;
            }
            v10++;
            v11++;
        }

        if (v9 > _match_length) {
            _match_length = v9;
            _match_position = v21;
            if (v9 >= 18) {
                break;
            }
        }
    }

    _dad_2[a1] = _dad_2[v21];
    _lson[a1] = _lson[v21];
    _rson[a1] = _rson[v21];

    _dad_2[_lson[v21]] = a1;
    _dad_2[_rson[v21]] = a1;

    if (_rson[_dad_2[v21]] == v21) {
        _rson[_dad_2[v21]] = a1;
    } else {
        _lson[_dad_2[v21]] = a1;
    }

    _dad_2[v21] = 4096;
}

// 0x44F7EC
static void _DeleteNode(int a1)
{
    if (_dad_2[a1] != 4096) {
        int v5;
        if (_rson[a1] == 4096) {
            v5 = _lson[a1];
        } else {
            if (_lson[a1] == 4096) {
                v5 = _rson[a1];
            } else {
                v5 = _lson[a1];

                if (_rson[v5] != 4096) {
                    do {
                        v5 = _rson[v5];
                    } while (_rson[v5] != 4096);

                    _rson[_dad_2[v5]] = _lson[v5];
                    _dad_2[_lson[v5]] = _dad_2[v5];
                    _lson[v5] = _lson[a1];
                    _dad_2[_lson[a1]] = v5;
                }

                _rson[v5] = _rson[a1];
                _dad_2[_rson[a1]] = v5;
            }
        }

        _dad_2[v5] = _dad_2[a1];

        if (_rson[_dad_2[a1]] == a1) {
            _rson[_dad_2[a1]] = v5;
        } else {
            _lson[_dad_2[a1]] = v5;
        }
        _dad_2[a1] = 4096;
    }
}

// 0x44F92C
int graphDecompress(unsigned char* src, unsigned char* dest, int length)
{
    _text_buf = (unsigned char*)internal_malloc(sizeof(*_text_buf) * 4122);
    if (_text_buf == nullptr) {
        debugPrint("\nGRAPHLIB: Error allocating decompression buffer!\n");
        return -1;
    }

    int v8 = 4078;
    memset(_text_buf, ' ', v8);

    int v21 = 0;
    int index = 0;
    while (index < length) {
        v21 >>= 1;
        if ((v21 & 0x100) == 0) {
            v21 = *src++;
            v21 |= 0xFF00;
        }

        if ((v21 & 0x01) == 0) {
            int v10 = *src++;
            int v11 = *src++;

            v10 |= (v11 & 0xF0) << 4;
            v11 &= 0x0F;
            v11 += 2;

            for (int v16 = 0; v16 <= v11; v16++) {
                int v17 = (v10 + v16) & 0xFFF;

                unsigned char ch = _text_buf[v17];
                _text_buf[v8] = ch;
                *dest++ = ch;

                v8 = (v8 + 1) & 0xFFF;

                index++;
                if (index >= length) {
                    break;
                }
            }
        } else {
            unsigned char ch = *src++;
            _text_buf[v8] = ch;
            *dest++ = ch;

            v8 = (v8 + 1) & 0xFFF;

            index++;
        }
    }

    internal_free(_text_buf);

    return 0;
}

// 0x44FA78
void grayscalePaletteUpdate(int a1, int a2)
{
    if (a1 >= 0 && a2 <= 255) {
        for (int index = a1; index <= a2; index++) {
            // NOTE: Calls `Color2RGB` many times due to `min` and `max` macro
            // uses.
            int v1 = std::max((Color2RGB(index) & 0x7C00) >> 10, std::max((Color2RGB(index) & 0x3E0) >> 5, Color2RGB(index) & 0x1F));
            int v2 = std::min((Color2RGB(index) & 0x7C00) >> 10, std::min((Color2RGB(index) & 0x3E0) >> 5, Color2RGB(index) & 0x1F));
            int v3 = v1 + v2;
            int v4 = (int)((double)v3 * 240.0 / 510.0);

            int paletteIndex = ((v4 & 0xFF) << 10) | ((v4 & 0xFF) << 5) | (v4 & 0xFF);
            _GreyTable[index] = _colorTable[paletteIndex];
        }
    }
}

// 0x44FC40
void grayscalePaletteApply(unsigned char* buffer, int width, int height, int pitch)
{
    unsigned char* ptr = buffer;
    int skip = pitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char c = *ptr;
            *ptr++ = _GreyTable[c];
        }
        ptr += skip;
    }
}

} // namespace fallout
