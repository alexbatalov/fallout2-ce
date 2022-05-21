#include "datafile.h"

// 0x56D7E0
unsigned char _pal[768];

// 0x42F0E4
unsigned char* _datafileGetPalette()
{
    return _pal;
}
