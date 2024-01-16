#include "mapper/mapper.h"

#include <ctype.h>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "color.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "graph_lib.h"
#include "inventory.h"
#include "kb.h"
#include "loadsave.h"
#include "mapper/map_func.h"
#include "mapper/mp_proto.h"
#include "mapper/mp_targt.h"
#include "mapper/mp_text.h"
#include "memory.h"
#include "mouse.h"
#include "object.h"
#include "proto.h"
#include "settings.h"
#include "svga.h"
#include "tile.h"
#include "window_manager.h"
#include "window_manager_private.h"

namespace fallout {

static void MapperInit();
static int mapper_edit_init(int argc, char** argv);
static void mapper_edit_exit();
static int bookmarkInit();
static int bookmarkExit();
static void bookmarkHide();
static void bookmarkUnHide();
static int categoryInit();
static int categoryExit();
static int categoryHide();
static int categoryToggleState();
static int categoryUnhide();
static bool proto_user_is_librarian();
static void edit_mapper();
static void mapper_load_toolbar(int a1, int a2);
static void redraw_toolname();
static void clear_toolname();
static void update_toolname(int* pid, int type, int id);
static void update_high_obj_name(Object* obj);
static void mapper_destroy_highlight_obj(Object** a1, Object** a2);
static void mapper_refresh_rotation();
static void update_art(int a1, int a2);
static void handle_new_map(int* a1, int* a2);
static int mapper_mark_exit_grid();
static void mapper_mark_all_exit_grids();

// TODO: Underlying menu/pulldown interface wants menu items to be non-const,
// needs some refactoring.

static char kSeparator[] = "";

static char kNew[] = " New ";
static char kOpen[] = " Open ";
static char kSave[] = " Save ";
static char kSaveAs[] = " Save As... ";
static char kInfo[] = " Info ";
static char kOpenFromText[] = " Open From Text ";
static char kQuit[] = " Quit ";

static char kCreatePattern[] = " Create Pattern ";
static char kUsePattern[] = " Use Pattern ";
static char kMoveMap[] = " Move Map ";
static char kMoveMapElev[] = " Move Map Elev ";
static char kCopyMapElev[] = " Copy Map Elev ";
static char kEditObjDude[] = " Edit Obj_dude ";
static char kFlushCache[] = " Flush Cache ";
static char kToggleAnimStepping[] = " Toggle Anim Stepping ";
static char kFixMapObjectsToPids[] = " Fix map-objects to pids ";
static char kSetBookmark[] = " Set Bookmark ";
static char kToggleBlockObjView[] = " Toggle Block Obj View ";
static char kToggleClickToScroll[] = " Toggle Click-To-Scroll ";
static char kSetExitGridData[] = " Set Exit-Grid Data ";
static char kMarkExitGrids[] = " Mark Exit-Grids ";
static char kMarkAllExitGrids[] = " Mark *ALL* Exit Grids ";
static char kClearMapLevel[] = " *Clear Map Level* ";
static char kCreateAllMapTexts[] = " Create ALL MAP TEXTS ";
static char kRebuildAllMaps[] = " Rebuild ALL MAPS ";

static char kListAllScripts[] = " List all Scripts ";
static char kSetStartHex[] = " Set Start Hex ";
static char kPlaceSpatialScript[] = " Place Spatial Script ";
static char kDeleteSpatialScript[] = " Delete Spatial Script ";
static char kDeleteAllSpatialScripts[] = " Delete *ALL* Spatial SCRIPTS! ";
static char kCreateScript[] = " Create Script ";
static char kSetMapScript[] = " Set Map Script ";
static char kShowMapScript[] = " Show Map Script ";

static char kRebuildWeapons[] = " Rebuild Weapons ";
static char kRebuildProtoList[] = " Rebuild Proto List ";
static char kRebuildAll[] = " Rebuild ALL ";
static char kRebuildBinary[] = " Rebuild Binary ";
static char kArtToProtos[] = " Art => New Protos ";
static char kSwapPrototypse[] = " Swap Prototypes ";

static char kTmpMapName[] = "TMP$MAP#.MAP";

// 0x559618
int rotate_arrows_x_offs[] = {
    31,
    38,
    31,
    11,
    3,
    11,
};

// 0x559630
int rotate_arrows_y_offs[] = {
    7,
    23,
    37,
    37,
    23,
    7,
};

// 0x559648
char* menu_0[] = {
    kNew,
    kOpen,
    kSave,
    kSaveAs,
    kSeparator,
    kInfo,
    kOpenFromText,
    kQuit,
};

// 0x559668
char* menu_1[] = {
    kCreatePattern,
    kUsePattern,
    kSeparator,
    kMoveMap,
    kMoveMapElev,
    kCopyMapElev,
    kSeparator,
    kEditObjDude,
    kFlushCache,
    kToggleAnimStepping,
    kFixMapObjectsToPids,
    kSetBookmark,
    kToggleBlockObjView,
    kToggleClickToScroll,
    kSetExitGridData,
    kMarkExitGrids,
    kMarkAllExitGrids,
    kClearMapLevel,
    kSeparator,
    kCreateAllMapTexts,
    kRebuildAllMaps,
};

// 0x5596BC
char* menu_2[] = {
    kListAllScripts,
    kSetStartHex,
    kPlaceSpatialScript,
    kDeleteSpatialScript,
    kDeleteAllSpatialScripts,
    kCreateScript,
    kSetMapScript,
    kShowMapScript,
};

// 0x5596DC
char* menu_3[] = {
    kRebuildWeapons,
    kRebuildProtoList,
    kRebuildAll,
    kRebuildBinary,
    kSeparator,
    kArtToProtos,
    kSwapPrototypse,
};

// 0x5596F8
char** menu_names[] = {
    menu_0,
    menu_1,
    menu_2,
    menu_3,
};

// 0x559748
MapTransition mapInfo = { -1, -1, 0, 0 };

// 0x559880
int max_art_buttons = 7;

// 0x559884
int art_scale_width = 49;

// 0x559888
int art_scale_height = 48;

// 0x5598A0
static bool map_entered = false;

// 0x5598A4
static char* tmp_map_name = kTmpMapName;

// 0x5598A8
static int bookmarkWin = -1;

// 0x5598AC
static int categoryWin = -1;

// 0x5598B0
static bool categoryIsHidden = false;

// 0x6EAA40
int menu_val_0[8];

// 0x6EAA60
int menu_val_2[8];

// 0x6EAA80
unsigned char e_num[4][19 * 26];

// 0x6EBD28
unsigned char rotate_arrows[2][6][10 * 10];

// 0x6EC408
int menu_val_1[21];

// 0x6EC468
unsigned char* art_shape;

// 0x6EC46C
int to_paint_bid;

// 0x6EC470
int edit_bid;

// 0x6EC474
int paste_bid;

// 0x6EC478
int misc_bid;

// 0x6EC47C
int tile_bid;

// 0x6EC480
int copy_bid;

// 0x6EC484
int delete_bid;

// 0x6EC488
int wall_bid;

// 0x6EC48C
int obj_bid;

// 0x6EC490
int to_topdown_bid;

// 0x6EC494
int roof_bid;

// 0x6EC498
int hex_bid;

// 0x6EC49C
int to_iso_bid;

// 0x6EC4A0
int scen_bid;

// 0x6EC4A4
int crit_bid;

// 0x6EC4A8
unsigned char* tool;

// 0x6EC4AC
int tool_win;

// 0x6EC4B0
int menu_bar;

// 0x6EC4B4
unsigned char* lbm_buf;

// 0x6EC4B8
unsigned char height_inc_up[18 * 23];

// 0x6EC656
unsigned char height_dec_up[18 * 23];

// 0x6EC7F4
unsigned char height_dec_down[18 * 23];

// 0x6EC992
unsigned char height_inc_down[18 * 23];

// 0x6ECB30
unsigned char obj_down[66 * 13];

// 0x6ECE8A
unsigned char to_iso_down[58 * 13];

// 0x6ED17C
unsigned char scen_up[66 * 13];

// 0x6ED4D6
unsigned char roof_up[58 * 13];

// 0x6ED7C8
unsigned char crit_down[66 * 13];

// 0x6EDB22
unsigned char obj_up[66 * 13];

// 0x6EDE7C
unsigned char crit_up[66 * 13];

// 0x6EE1D6
unsigned char to_topdown_down[58 * 13];

// 0x6EE4C8
unsigned char hex_up[58 * 13];

// 0x6EE7BA
unsigned char hex_down[58 * 13];

// 0x6EEAAC
unsigned char to_topdown_up[58 * 13];

// 0x6EED9E
unsigned char scen_down[66 * 13];

// 0x6EF0F8
unsigned char edec_down[18 * 23];

// 0x6EF296
unsigned char to_iso_up[58 * 13];

// 0x6EF588
unsigned char roof_down[58 * 13];

// 0x6EF87A
unsigned char r_up[18 * 23];

// 0x6EFA18
unsigned char einc_down[18 * 23];

// 0x6EFBB6
unsigned char shift_l_up[18 * 23];

// 0x6EFD54
unsigned char edec_up[18 * 23];

// 0x6EFEF2
unsigned char shift_r_up[18 * 23];

// 0x6F0090
unsigned char shift_r_down[18 * 23];

// 0x6F022E
unsigned char r_down[18 * 23];

// 0x6F03CC
unsigned char einc_up[18 * 23];

// 0x6F056A
unsigned char l_down[18 * 23];

// 0x6F0708
unsigned char shift_l_down[18 * 23];

// 0x6F08A6
unsigned char l_up[18 * 23];

// 0x6F0A44
unsigned char to_edit_up[45 * 43];

// 0x6F11D3
unsigned char erase_up[45 * 43];

// 0x6F1962
unsigned char copy_group_up[45 * 43];

// 0x6F20F1
unsigned char to_paint_down[45 * 43];

// 0x6F2880
unsigned char erase_down[45 * 43];

// 0x6F300F
unsigned char copy_group_down[45 * 43];

// 0x6F379E
unsigned char to_edit_down[45 * 43];

// 0x6F3F2D
unsigned char copy_up[49 * 19];

// 0x6F42D0
unsigned char misc_down[53 * 18];

// 0x6F4581
unsigned char wall_down[53 * 18];

// 0x6F4832
unsigned char delete_up[49 * 19];

// 0x6F4BD5
unsigned char edit_up[49 * 19];

// 0x6F4F78
unsigned char tile_up[53 * 18];

// 0x6F5229
unsigned char edit_down[49 * 19];

// 0x6F55CC
unsigned char paste_down[49 * 19];

// 0x6F596F
unsigned char delete_down[49 * 19];

// 0x6F5D12
unsigned char tile_down[53 * 18];

// 0x6F5FC3
unsigned char copy_down[49 * 19];

// 0x6F6366
unsigned char misc_up[53 * 18];

// 0x6F6617
unsigned char paste_up[49 * 19];

// 0x6F69BA
unsigned char to_paint_up[1935];

// 0x6F7149
unsigned char wall_up[53 * 18];

// 0x6F73FA
bool draw_mode;

// 0x6F73FB
bool view_mode;

// gnw_main
// 0x485DD0
int mapper_main(int argc, char** argv)
{
    MapperInit();

    if (mapper_edit_init(argc, argv) == -1) {
        mem_check();
        return 0;
    }

    edit_mapper();
    mapper_edit_exit();
    mem_check();

    return 0;
}

// 0x485E00
void MapperInit()
{
    menu_val_0[0] = KEY_ALT_N;
    menu_val_0[1] = KEY_ALT_O;
    menu_val_0[2] = KEY_ALT_S;
    menu_val_0[3] = KEY_ALT_A;
    menu_val_0[4] = KEY_ESCAPE;
    menu_val_0[5] = KEY_ALT_K;
    menu_val_0[6] = KEY_ALT_I;
    menu_val_0[7] = KEY_ESCAPE;

    menu_val_1[0] = KEY_ALT_U;
    menu_val_1[1] = KEY_ALT_Y;
    menu_val_1[2] = KEY_ESCAPE;
    menu_val_1[3] = KEY_ALT_G;
    menu_val_1[4] = 4186;
    menu_val_1[5] = 4188;
    menu_val_1[6] = KEY_ESCAPE;
    menu_val_1[7] = KEY_ALT_B;
    menu_val_1[8] = KEY_ALT_E;
    menu_val_1[9] = KEY_ALT_D;
    menu_val_1[10] = KEY_LOWERCASE_B;
    menu_val_1[11] = 2165;
    menu_val_1[12] = 3123;
    menu_val_1[13] = KEY_ALT_Z;
    menu_val_1[14] = 5677;
    menu_val_1[15] = 5678;
    menu_val_1[16] = 5679;
    menu_val_1[17] = 5666;
    menu_val_1[18] = KEY_ESCAPE;
    menu_val_1[19] = 5406;
    menu_val_1[20] = 5405;

    menu_val_2[0] = KEY_LOWERCASE_I;
    menu_val_2[1] = 5400;
    menu_val_2[2] = KEY_LOWERCASE_S;
    menu_val_2[3] = KEY_CTRL_F8;
    menu_val_2[4] = 5410;
    menu_val_2[5] = KEY_GRAVE;
    menu_val_2[6] = KEY_ALT_W;
    menu_val_2[7] = 5544;
}

// 0x485F94
int mapper_edit_init(int argc, char** argv)
{
    int index;

    if (gameInitWithOptions("FALLOUT Mapper", true, 2, 0, argc, argv) == -1) {
        return -1;
    }

    tileEnable();
    gmouse_set_mapper_mode(true);
    settings.system.executable = "mapper";

    if (settings.mapper.override_librarian) {
        can_modify_protos = true;
        target_override_protection();
    }

    setup_map_dirs();
    mapper_load_toolbar(4, 0);

    max_art_buttons = (_scr_size.right - _scr_size.left - 136) / 50;
    art_shape = (unsigned char*)internal_malloc(art_scale_height * art_scale_width);
    if (art_shape == NULL) {
        printf("Can't malloc memory!!\n");
        exit(1);
    }

    menu_bar = windowCreate(0,
        0,
        rectGetWidth(&_scr_size),
        16,
        _colorTable[0],
        WINDOW_HIDDEN);
    _win_register_menu_bar(menu_bar,
        0,
        0,
        rectGetWidth(&_scr_size),
        16,
        260,
        _colorTable[8456]);
    _win_register_menu_pulldown(menu_bar,
        8,
        "FILE",
        289,
        8,
        menu_names[0],
        260,
        _colorTable[8456]);
    _win_register_menu_pulldown(menu_bar,
        40,
        "TOOLS",
        303,
        21,
        menu_names[1],
        260,
        _colorTable[8456]);
    _win_register_menu_pulldown(menu_bar,
        80,
        "SCRIPTS",
        276,
        8,
        menu_names[2],
        260,
        _colorTable[8456]);

    if (can_modify_protos) {
        _win_register_menu_pulldown(menu_bar,
            130,
            "LIBRARIAN",
            292,
            6,
            &(menu_1[14]),
            260,
            _colorTable[8456]);
    }

    tool_win = windowCreate(0,
        _scr_size.bottom - 99,
        rectGetWidth(&_scr_size),
        100,
        256,
        0);
    tool = windowGetBuffer(tool_win);

    lbm_buf = (unsigned char*)internal_malloc(640 * 480);
    load_lbm_to_buf("data\\mapper2.lbm",
        lbm_buf,
        rectGetWidth(&_scr_size),
        0,
        0,
        _scr_size.right - _scr_size.left,
        479);

    //
    blitBufferToBuffer(lbm_buf + 380 * rectGetWidth(&_scr_size),
        rectGetWidth(&_scr_size),
        100,
        rectGetWidth(&_scr_size),
        tool,
        rectGetWidth(&_scr_size));

    //
    blitBufferToBuffer(lbm_buf + 406 * (rectGetWidth(&_scr_size)) + 101,
        18,
        23,
        rectGetWidth(&_scr_size),
        l_up,
        18);
    blitBufferToBuffer(lbm_buf + 253 * (rectGetWidth(&_scr_size)) + 101,
        18,
        23,
        rectGetWidth(&_scr_size),
        l_down,
        18);
    buttonCreate(tool_win,
        101,
        26,
        18,
        23,
        -1,
        -1,
        45,
        -1,
        l_up,
        l_down,
        NULL,
        0);

    //
    blitBufferToBuffer(lbm_buf + 406 * (rectGetWidth(&_scr_size)) + 622,
        18,
        23,
        rectGetWidth(&_scr_size),
        r_up,
        18);
    blitBufferToBuffer(lbm_buf + 253 * (rectGetWidth(&_scr_size)) + 622,
        18,
        23,
        rectGetWidth(&_scr_size),
        r_down,
        18);
    buttonCreate(tool_win,
        _scr_size.right - 18,
        1,
        18,
        23,
        -1,
        -1,
        61,
        -1,
        r_up,
        r_down,
        NULL,
        0);

    //
    blitBufferToBuffer(lbm_buf + 381 * (rectGetWidth(&_scr_size)) + 101,
        18,
        23,
        rectGetWidth(&_scr_size),
        shift_l_up,
        18);
    blitBufferToBuffer(lbm_buf + 228 * (rectGetWidth(&_scr_size)) + 101,
        18,
        23,
        rectGetWidth(&_scr_size),
        shift_l_down,
        18);
    buttonCreate(tool_win,
        101,
        1,
        18,
        23,
        -1,
        -1,
        95,
        -1,
        shift_l_up,
        shift_l_down,
        NULL,
        0);

    //
    blitBufferToBuffer(lbm_buf + 381 * (rectGetWidth(&_scr_size)) + 622,
        18,
        23,
        rectGetWidth(&_scr_size),
        shift_r_up,
        18);
    blitBufferToBuffer(lbm_buf + 228 * (rectGetWidth(&_scr_size)) + 622,
        18,
        23,
        rectGetWidth(&_scr_size),
        shift_r_down,
        18);
    buttonCreate(tool_win,
        _scr_size.right - 18,
        1,
        18,
        23,
        -1,
        -1,
        43,
        -1,
        shift_r_up,
        shift_r_down,
        NULL,
        0);

    //
    for (index = 0; index < max_art_buttons; index++) {
        int btn = buttonCreate(tool_win,
            index * (art_scale_width + 1) + 121,
            1,
            art_scale_width,
            art_scale_height,
            index + max_art_buttons + 161,
            58,
            160 + index,
            -1,
            NULL,
            NULL,
            NULL,
            0);
        buttonSetRightMouseCallbacks(btn, 160 + index, -1, NULL, NULL);
    }

    // ELEVATION INC
    blitBufferToBuffer(lbm_buf + 431 * (rectGetWidth(&_scr_size)) + 1,
        18,
        23,
        rectGetWidth(&_scr_size),
        einc_up,
        18);
    blitBufferToBuffer(lbm_buf + 325 * (rectGetWidth(&_scr_size)) + 1,
        18,
        23,
        rectGetWidth(&_scr_size),
        einc_down,
        18);
    buttonCreate(tool_win,
        1,
        51,
        18,
        23,
        -1,
        -1,
        329,
        -1,
        einc_up,
        einc_down,
        NULL,
        0);

    // ELEVATION DEC
    blitBufferToBuffer(lbm_buf + 456 * (rectGetWidth(&_scr_size)) + 1,
        18,
        23,
        rectGetWidth(&_scr_size),
        edec_up,
        18);
    blitBufferToBuffer(lbm_buf + 350 * (rectGetWidth(&_scr_size)) + 1,
        18,
        23,
        rectGetWidth(&_scr_size),
        edec_down,
        18);
    buttonCreate(tool_win,
        1,
        76,
        18,
        23,
        -1,
        -1,
        337,
        -1,
        edec_up,
        edec_down,
        NULL,
        0);

    // ELEVATION
    for (index = 0; index < 4; index++) {
        blitBufferToBuffer(lbm_buf + 293 * rectGetWidth(&_scr_size) + 19 * index,
            19,
            26,
            rectGetWidth(&_scr_size),
            e_num[1],
            19);
    }

    view_mode = false;

    //
    blitBufferToBuffer(lbm_buf + 169 * (rectGetWidth(&_scr_size)) + 64,
        58,
        13,
        rectGetWidth(&_scr_size),
        to_iso_up,
        58);
    blitBufferToBuffer(lbm_buf + 108 * (rectGetWidth(&_scr_size)) + 64,
        58,
        13,
        rectGetWidth(&_scr_size),
        to_iso_down,
        58);

    // ROOF
    blitBufferToBuffer(lbm_buf + 464 * (rectGetWidth(&_scr_size)) + 64,
        58,
        13,
        rectGetWidth(&_scr_size),
        roof_up,
        58);
    blitBufferToBuffer(lbm_buf + 358 * (rectGetWidth(&_scr_size)) + 64,
        58,
        13,
        rectGetWidth(&_scr_size),
        roof_down,
        58);
    roof_bid = buttonCreate(tool_win,
        64,
        69,
        58,
        13,
        -1,
        -1,
        'r',
        'r',
        roof_up,
        roof_down,
        NULL,
        BUTTON_FLAG_0x01);

    if (tileRoofIsVisible()) {
        tile_toggle_roof(false);
    }

    // HEX
    blitBufferToBuffer(lbm_buf + 464 * (rectGetWidth(&_scr_size)) + 64,
        58,
        13,
        rectGetWidth(&_scr_size),
        hex_up,
        58);
    blitBufferToBuffer(lbm_buf + 358 * (rectGetWidth(&_scr_size)) + 64,
        58,
        13,
        rectGetWidth(&_scr_size),
        hex_down,
        58);
    hex_bid = buttonCreate(tool_win,
        64,
        84,
        58,
        13,
        -1,
        -1,
        350,
        350,
        hex_up,
        hex_down,
        NULL,
        BUTTON_FLAG_0x01);

    // OBJ
    blitBufferToBuffer(lbm_buf + 434 * (rectGetWidth(&_scr_size)) + 125,
        66,
        13,
        rectGetWidth(&_scr_size),
        obj_up,
        66);
    blitBufferToBuffer(lbm_buf + 328 * (rectGetWidth(&_scr_size)) + 125,
        66,
        13,
        rectGetWidth(&_scr_size),
        obj_down,
        66);
    obj_bid = buttonCreate(tool_win,
        125,
        54,
        66,
        13,
        -1,
        -1,
        350,
        350,
        obj_up,
        obj_down,
        NULL,
        BUTTON_FLAG_0x01);

    // CRIT
    blitBufferToBuffer(lbm_buf + 449 * (rectGetWidth(&_scr_size)) + 125,
        66,
        13,
        rectGetWidth(&_scr_size),
        crit_up,
        66);
    blitBufferToBuffer(lbm_buf + 343 * (rectGetWidth(&_scr_size)) + 125,
        66,
        13,
        rectGetWidth(&_scr_size),
        crit_down,
        66);
    crit_bid = buttonCreate(tool_win,
        125,
        69,
        66,
        13,
        -1,
        -1,
        351,
        351,
        crit_up,
        crit_down,
        NULL,
        BUTTON_FLAG_0x01);

    // SCEN
    blitBufferToBuffer(lbm_buf + 434 * (rectGetWidth(&_scr_size)) + 194,
        53,
        13,
        rectGetWidth(&_scr_size),
        scen_up,
        53);
    blitBufferToBuffer(lbm_buf + 328 * (rectGetWidth(&_scr_size)) + 194,
        53,
        13,
        rectGetWidth(&_scr_size),
        scen_down,
        53);
    scen_bid = buttonCreate(tool_win,
        125,
        84,
        66,
        13,
        -1,
        -1,
        352,
        352,
        scen_up,
        scen_down,
        NULL,
        BUTTON_FLAG_0x01);

    // WALL
    blitBufferToBuffer(lbm_buf + 434 * (rectGetWidth(&_scr_size)) + 194,
        53,
        13,
        rectGetWidth(&_scr_size),
        wall_up,
        53);
    blitBufferToBuffer(lbm_buf + 328 * (rectGetWidth(&_scr_size)) + 194,
        53,
        13,
        rectGetWidth(&_scr_size),
        wall_down,
        53);
    wall_bid = buttonCreate(tool_win,
        194,
        54,
        53,
        13,
        -1,
        -1,
        355,
        355,
        wall_up,
        wall_down,
        NULL,
        BUTTON_FLAG_0x01);

    // MISC
    blitBufferToBuffer(lbm_buf + 464 * (rectGetWidth(&_scr_size)) + 194,
        53,
        13,
        rectGetWidth(&_scr_size),
        misc_up,
        53);
    blitBufferToBuffer(lbm_buf + 358 * (rectGetWidth(&_scr_size)) + 194,
        53,
        13,
        rectGetWidth(&_scr_size),
        misc_down,
        53);
    misc_bid = buttonCreate(tool_win,
        194,
        84,
        53,
        13,
        -1,
        -1,
        355,
        355,
        misc_up,
        misc_down,
        NULL,
        BUTTON_FLAG_0x01);

    // HEIGHT INC
    blitBufferToBuffer(lbm_buf + 431 * rectGetWidth(&_scr_size) + 251,
        18,
        23,
        rectGetWidth(&_scr_size),
        height_inc_up,
        18);
    blitBufferToBuffer(lbm_buf + 325 * rectGetWidth(&_scr_size) + 251,
        18,
        23,
        rectGetWidth(&_scr_size),
        height_inc_down,
        18);
    buttonCreate(tool_win,
        251,
        51,
        18,
        23,
        -1,
        -1,
        371,
        -1,
        height_dec_up,
        height_dec_down,
        NULL,
        0);

    // HEIGHT DEC
    blitBufferToBuffer(lbm_buf + 456 * rectGetWidth(&_scr_size) + 251,
        18,
        23,
        rectGetWidth(&_scr_size),
        height_dec_up,
        18);
    blitBufferToBuffer(lbm_buf + 350 * rectGetWidth(&_scr_size) + 251,
        18,
        23,
        rectGetWidth(&_scr_size),
        height_dec_down,
        18);
    buttonCreate(tool_win,
        251,
        76,
        18,
        23,
        -1,
        -1,
        371,
        -1,
        height_dec_up,
        height_dec_down,
        NULL,
        0);

    // ARROWS
    for (index = 0; index < ROTATION_COUNT; index++) {
        int x = rotate_arrows_x_offs[index] + 285;
        int y = rotate_arrows_y_offs[index] + 25;
        unsigned char v1 = lbm_buf[27 * (_scr_size.right + 1) + 287];
        int k;

        blitBufferToBuffer(lbm_buf + y * rectGetWidth(&_scr_size) + x,
            10,
            10,
            rectGetWidth(&_scr_size),
            rotate_arrows[1][index],
            10);

        for (k = 0; k < 100; k++) {
            if (rotate_arrows[1][index][k] == v1) {
                rotate_arrows[1][index][k] = 0;
            }
        }

        blitBufferToBuffer(lbm_buf + y * rectGetWidth(&_scr_size) + x - 52,
            10,
            10,
            rectGetWidth(&_scr_size),
            rotate_arrows[0][index],
            10);

        for (k = 0; k < 100; k++) {
            if (rotate_arrows[1][index][k] == v1) {
                rotate_arrows[1][index][k] = 0;
            }
        }
    }

    // COPY
    blitBufferToBuffer(lbm_buf + 435 * (rectGetWidth(&_scr_size)) + 325,
        49,
        19,
        rectGetWidth(&_scr_size),
        copy_up,
        49);
    blitBufferToBuffer(lbm_buf + 329 * (rectGetWidth(&_scr_size)) + 325,
        49,
        19,
        rectGetWidth(&_scr_size),
        copy_down,
        49);
    copy_bid = buttonCreate(tool_win,
        325,
        55,
        49,
        19,
        -1,
        -1,
        99,
        -1,
        copy_up,
        copy_down,
        0,
        0);

    // PASTE
    blitBufferToBuffer(lbm_buf + 457 * (rectGetWidth(&_scr_size)) + 325,
        49,
        19,
        rectGetWidth(&_scr_size),
        paste_up,
        49);
    blitBufferToBuffer(lbm_buf + 351 * (rectGetWidth(&_scr_size)) + 325,
        49,
        19,
        rectGetWidth(&_scr_size),
        paste_down,
        49);
    paste_bid = buttonCreate(tool_win,
        325,
        77,
        49,
        19,
        -1,
        -1,
        67,
        -1,
        paste_up,
        paste_down,
        NULL,
        0);

    // EDIT
    blitBufferToBuffer(lbm_buf + 435 * (rectGetWidth(&_scr_size)) + 378,
        49,
        19,
        rectGetWidth(&_scr_size),
        edit_up,
        49);
    blitBufferToBuffer(lbm_buf + 329 * (rectGetWidth(&_scr_size)) + 378,
        49,
        19,
        rectGetWidth(&_scr_size),
        edit_down,
        49);
    edit_bid = buttonCreate(tool_win,
        378,
        55,
        49,
        19,
        -1,
        -1,
        101,
        -1,
        edit_up,
        edit_down,
        NULL,
        0);

    // DELETE
    blitBufferToBuffer(lbm_buf + 457 * rectGetWidth(&_scr_size) + 378,
        49,
        19,
        rectGetWidth(&_scr_size),
        delete_up,
        49);
    blitBufferToBuffer(lbm_buf + 351 * rectGetWidth(&_scr_size) + 378,
        49,
        19,
        rectGetWidth(&_scr_size),
        delete_down,
        49);
    delete_bid = buttonCreate(tool_win,
        378,
        77,
        49,
        19,
        -1,
        -1,
        339,
        -1,
        delete_up,
        delete_down,
        NULL,
        0);

    draw_mode = false;

    blitBufferToBuffer(lbm_buf + 169 * rectGetWidth(&_scr_size) + 430,
        45,
        43,
        rectGetWidth(&_scr_size),
        to_edit_up,
        45);
    blitBufferToBuffer(lbm_buf + 108 * rectGetWidth(&_scr_size) + 430,
        45,
        43,
        rectGetWidth(&_scr_size),
        to_edit_down,
        45);

    blitBufferToBuffer(lbm_buf + 169 * rectGetWidth(&_scr_size) + 327,
        45,
        43,
        rectGetWidth(&_scr_size),
        copy_group_up,
        45);
    blitBufferToBuffer(lbm_buf + 108 * rectGetWidth(&_scr_size) + 327,
        45,
        43,
        rectGetWidth(&_scr_size),
        copy_group_down,
        45);

    blitBufferToBuffer(lbm_buf + 169 * rectGetWidth(&_scr_size) + 379,
        45,
        43,
        rectGetWidth(&_scr_size),
        erase_up,
        45);
    blitBufferToBuffer(lbm_buf + 108 * rectGetWidth(&_scr_size) + 379,
        45,
        43,
        rectGetWidth(&_scr_size),
        erase_down,
        45);

    internal_free(lbm_buf);
    windowRefresh(tool_win);

    if (bookmarkInit() == -1) {
        debugPrint("\nbookmarkInit() Failed!");
    }

    if (categoryInit() == -1) {
        debugPrint("\ncategoryInit() Failed!");
    }

    tileScrollBlockingDisable();
    tileScrollLimitingDisable();
    init_mapper_protos();
    _map_init();
    target_init();
    mouseShowCursor();

    if (settings.mapper.rebuild_protos) {
        proto_build_all_texts();
        settings.mapper.rebuild_protos = false;
    }

    return 0;
}

// 0x48752C
void mapper_edit_exit()
{
    remove(tmp_map_name);
    remove("\\fallout\\cd\\data\\maps\\TMP$MAP#.MAP");
    remove("\\fallout\\cd\\data\\maps\\TMP$MAP#.CFG");

    MapDirErase("MAPS\\", "SAV");

    if (can_modify_protos) {
        copy_proto_lists();

        // NOTE: There is a call to an ambiguous function at `0x4B9ACC`, likely
        // `proto_save`.
    }

    target_exit();
    _map_exit();
    bookmarkExit();
    categoryExit();

    windowDestroy(tool_win);
    tool = NULL;

    windowDestroy(menu_bar);

    internal_free(art_shape);
    gameExit();
}

// 0x4875B4
int bookmarkInit()
{
    return 0;
}

// 0x4875B8
int bookmarkExit()
{
    if (bookmarkWin == -1) {
        return -1;
    }

    windowDestroy(bookmarkWin);
    bookmarkWin = -1;

    return 0;
}

// 0x4875E0
void bookmarkHide()
{
    if (bookmarkWin != -1) {
        windowHide(bookmarkWin);
    }
}

// 0x4875F8
void bookmarkUnHide()
{
    if (bookmarkWin != -1) {
        windowShow(bookmarkWin);
    }
}

// 0x4875B4
int categoryInit()
{
    return 0;
}

// 0x487700
int categoryExit()
{
    if (categoryWin == -1) {
        return -1;
    }

    windowDestroy(categoryWin);
    categoryWin = -1;

    return 0;
}

// 0x487728
int categoryHide()
{
    if (categoryWin == -1) {
        return -1;
    }

    windowHide(categoryWin);
    categoryIsHidden = true;

    return 0;
}

// 0x487768
int categoryToggleState()
{
    if (categoryIsHidden) {
        return categoryUnhide();
    } else {
        return categoryHide();
    }
}

// 0x487774
int categoryUnhide()
{
    if (categoryWin == -1) {
        return -1;
    }

    windowShow(categoryWin);
    categoryIsHidden = false;

    return 0;
}

// 0x487784
bool proto_user_is_librarian()
{
    if (!settings.mapper.librarian) {
        return false;
    }

    can_modify_protos = true;
    return true;
}

// 0x4877D0
void edit_mapper()
{
    // TODO: Incomplete.
}

// 0x48AFFC
void mapper_load_toolbar(int a1, int a2)
{
    // TODO: Incomplete.
}

// 0x48B16C
void print_toolbar_name(int object_type)
{
    Rect rect;
    char name[80];

    rect.left = 0;
    rect.top = 0;
    rect.right = 0;
    rect.bottom = 22;
    bufferFill(tool + 2 + 2 * (_scr_size.right - _scr_size.left) + 2,
        96,
        _scr_size.right - _scr_size.left + 1,
        19,
        _colorTable[21140]);

    sprintf(name, "%s", artGetObjectTypeName(object_type));
    name[0] = toupper(name[0]);
    windowDrawText(tool_win, name, 0, 7, 7, _colorTable[32747] | 0x2000000);
    windowRefreshRect(tool_win, &rect);
}

// 0x48B230
void redraw_toolname()
{
    Rect rect;

    rect.left = _scr_size.right - _scr_size.left - 149;
    rect.top = 60;
    rect.right = _scr_size.right - _scr_size.left + 1;
    rect.bottom = 95;
    windowRefreshRect(tool_win, &rect);
}

// 0x48B278
void clear_toolname()
{
    windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 60, 260);
    windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 70, 260);
    windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 80, 260);
    redraw_toolname();
}

// 0x48B328
void update_toolname(int* pid, int type, int id)
{
    Proto* proto;

    *pid = toolbar_proto(type, id);

    if (protoGetProto(*pid, &proto) == -1) {
        return;
    }

    windowDrawText(tool_win,
        protoGetName(proto->pid),
        120,
        _scr_size.right - _scr_size.left - 149,
        60,
        260);

    switch (PID_TYPE(proto->pid)) {
    case OBJ_TYPE_ITEM:
        windowDrawText(tool_win,
            gItemTypeNames[proto->item.type],
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    case OBJ_TYPE_CRITTER:
        windowDrawText(tool_win,
            "",
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    case OBJ_TYPE_WALL:
        windowDrawText(tool_win,
            proto_wall_light_str(proto->wall.flags),
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    case OBJ_TYPE_TILE:
        windowDrawText(tool_win,
            "",
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    case OBJ_TYPE_MISC:
        windowDrawText(tool_win,
            "",
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    default:
        windowDrawText(tool_win,
            "",
            120,
            _scr_size.right - _scr_size.left - 149,
            70,
            260);
        break;
    }

    windowDrawText(tool_win,
        "",
        120,
        _scr_size.right - _scr_size.left - 149,
        80,
        260);

    redraw_toolname();
}

// 0x48B5BC
void update_high_obj_name(Object* obj)
{
    Proto* proto;

    if (protoGetProto(obj->pid, &proto) != -1) {
        windowDrawText(tool_win, protoGetName(obj->pid), 120, _scr_size.right - _scr_size.left - 149, 60, 260);
        windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 70, 260);
        windowDrawText(tool_win, "", 120, _scr_size.right - _scr_size.left - 149, 80, 260);
        redraw_toolname();
    }
}

// 0x48B680
void mapper_destroy_highlight_obj(Object** a1, Object** a2)
{
    Rect rect;
    int elevation;

    if (a2 != NULL && *a2 != NULL) {
        elevation = (*a2)->elevation;
        reg_anim_clear(*a2);
        objectDestroy(*a2, &rect);
        tileWindowRefreshRect(&rect, elevation);
        *a2 = NULL;
    }

    if (a1 != NULL && *a1 != NULL) {
        elevation = (*a1)->elevation;
        objectDestroy(*a1, &rect);
        tileWindowRefreshRect(&rect, elevation);
        *a1 = NULL;
    }
}

// 0x48B6EC
void mapper_refresh_rotation()
{
    Rect rect;
    char string[2];
    int index;

    rect.left = 270;
    rect.top = 431 - (_scr_size.bottom - 99);
    rect.right = 317;
    rect.bottom = rect.top + 47;

    sprintf(string, "%d", rotation);

    if (tool != NULL) {
        windowFill(tool_win,
            290,
            452 - (_scr_size.bottom - 99),
            10,
            12,
            tool[(452 - (_scr_size.bottom - 99)) * (_scr_size.right + 1) + 289]);
        windowDrawText(tool_win,
            string,
            10,
            292,
            452 - (_scr_size.bottom - 99),
            0x2010104);

        for (index = 0; index < 6; index++) {
            int x = rotate_arrows_x_offs[index] + 269;
            int y = rotate_arrows_y_offs[index] + (430 - (_scr_size.bottom - 99));

            blitBufferToBufferTrans(rotate_arrows[index == rotation][index],
                10,
                10,
                10,
                tool + y * (_scr_size.right + 1) + x,
                _scr_size.right + 1);
        }

        windowRefreshRect(tool_win, &rect);
    } else {
        debugPrint("Error: mapper_refresh_rotation: tool buffer invalid!");
    }
}

// 0x48B850
void update_art(int a1, int a2)
{
    // TODO: Incomplete.
}

// 0x48C524
void handle_new_map(int* a1, int* a2)
{
    Rect rect;

    rect.left = 30;
    rect.top = 62;
    rect.right = 50;
    rect.bottom = 88;
    blitBufferToBuffer(e_num[gElevation],
        19,
        26,
        19,
        tool + rect.top * rectGetWidth(&_scr_size) + rect.left,
        rectGetWidth(&_scr_size));
    windowRefreshRect(tool_win, &rect);

    if (*a1 < 0 || *a1 > 6) {
        *a1 = 4;
    }

    *a2 = 0;
    update_art(*a1, *a2);

    print_toolbar_name(OBJ_TYPE_TILE);

    map_entered = false;

    if (tileRoofIsVisible()) {
        tile_toggle_roof(true);
    }
}

// 0x48C604
int mapper_inven_unwield(Object* obj, int right_hand)
{
    Object* item;
    int fid;

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);

    if (right_hand) {
        item = critterGetItem2(obj);
    } else {
        item = critterGetItem1(obj);
    }

    if (item != NULL) {
        item->flags &= ~OBJECT_IN_ANY_HAND;
    }

    animationRegisterAnimate(obj, ANIM_PUT_AWAY, 0);

    fid = buildFid(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, 0, 0, (obj->fid & 0x70000000) >> 28);
    animationRegisterSetFid(obj, fid, 0);

    return reg_anim_end();
}

// 0x48C678
int mapper_mark_exit_grid()
{
    int y;
    int x;
    int tile;
    Object* obj;

    for (y = -2000; y != 2000; y += 200) {
        for (x = -10; x < 10; x++) {
            tile = gGameMouseBouncingCursor->tile + y + x;

            obj = objectFindFirstAtElevation(gElevation);
            while (obj != NULL) {
                if (isExitGridPid(obj->pid)) {
                    obj->data.misc.map = mapInfo.map;
                    obj->data.misc.tile = mapInfo.tile;
                    obj->data.misc.elevation = mapInfo.elevation;
                    obj->data.misc.rotation = mapInfo.rotation;
                }
                obj = objectFindNextAtElevation();
            }
        }
    }

    return 0;
}

// 0x48C704
void mapper_mark_all_exit_grids()
{
    Object* obj;

    obj = objectFindFirstAtElevation(gElevation);
    while (obj != NULL) {
        if (isExitGridPid(obj->pid)) {
            obj->data.misc.map = mapInfo.map;
            obj->data.misc.tile = mapInfo.tile;
            obj->data.misc.elevation = mapInfo.elevation;
            obj->data.misc.rotation = mapInfo.rotation;
        }
        obj = objectFindNextAtElevation();
    }
}

} // namespace fallout
