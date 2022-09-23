#ifndef AUTOMAP_H
#define AUTOMAP_H

#include "db.h"
#include "map_defs.h"

namespace fallout {

#define AUTOMAP_DB ("AUTOMAP.DB")
#define AUTOMAP_TMP ("AUTOMAP.TMP")

// The number of map entries that is stored in automap.db.
//
// NOTE: I don't know why this value is not equal to the number of maps.
#define AUTOMAP_MAP_COUNT (160)

// View options for rendering automap for map window. These are stored in
// [gAutomapFlags] and is saved in save game file.
typedef enum AutomapFlags {
    // NOTE: This is a special flag to denote the map is activated in the game (as
    // opposed to the mapper). It's always on. Turning it off produces nice color
    // coded map with all objects and their types visible, however there is no way
    // you can do it within the game UI.
    AUTOMAP_IN_GAME = 0x01,

    // High details is on.
    AUTOMAP_WTH_HIGH_DETAILS = 0x02,

    // Scanner is active.
    AUTOMAP_WITH_SCANNER = 0x04,
} AutomapFlags;

typedef struct AutomapHeader {
    unsigned char version;

    // The size of entire automap database (including header itself).
    int dataSize;

    // Offsets from the beginning of the automap database file into
    // entries data.
    //
    // These offsets are specified for every map/elevation combination. A value
    // of 0 specifies that there is no data for appropriate map/elevation
    // combination.
    int offsets[AUTOMAP_MAP_COUNT][ELEVATION_COUNT];
} AutomapHeader;

int automapInit();
int automapReset();
void automapExit();
int automapLoad(File* stream);
int automapSave(File* stream);
int _automapDisplayMap(int map);
void automapShow(bool isInGame, bool isUsingScanner);
int automapRenderInPipboyWindow(int win, int map, int elevation);
int automapSaveCurrent();
int automapGetHeader(AutomapHeader** automapHeaderPtr);

void automapSetDisplayMap(int map, bool available);

} // namespace fallout

#endif /* AUTOMAP_H */
