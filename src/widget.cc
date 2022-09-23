#include "widget.h"

namespace fallout {

static void _showRegion(int a1);

// 0x66E6A0
static int _updateRegions[32];

// 0x4B5A64
void _showRegion(int a1)
{
    // TODO: Incomplete.
}

// 0x4B5C24
int _update_widgets()
{
    for (int index = 0; index < 32; index++) {
        if (_updateRegions[index]) {
            _showRegion(_updateRegions[index]);
        }
    }

    return 1;
}

// 0x4B5998
void sub_4B5998(int win)
{
    // TODO: Incomplete.
}

} // namespace fallout
