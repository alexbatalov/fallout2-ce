// NOTE: This module is completely standalone. It does not have external
// dependencies and uses __cdecl calling convention, which probably means it
// was implemented as a separate library and linked statically.

#include "movie_lib.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "audio_engine.h"
#include "delay.h"
#include "platform_compat.h"

namespace fallout {

typedef struct MveMem {
    void* ptr;
    unsigned int size;
    int alloced;
} MveMem;

#pragma pack(2)
typedef struct MveHeader {
    char sig[20];
    short size;
    short ver;
    short id;
    unsigned int chunk_hdr;
} MveHeader;
#pragma pack()

static void MVE_MemInit(MveMem* mem, unsigned int size, void* ptr);
static void MVE_MemFree(MveMem* mem);
static int _sub_4F4B5();
static int ioReset(void* handle);
static void* ioRead(int size);
static void* MVE_MemAlloc(MveMem* mem, unsigned int size);
static unsigned char* ioNextRecord();
static int syncWait();
static void _MVE_sndPause();
static int syncInit(int rate, int resolution);
static void syncReset(int quanta);
static int _MVE_sndConfigure(int a1, int a2, int a3, int a4, int a5, int a6);
static void MVE_syncSync();
static void _MVE_sndReset();
static void _MVE_sndSync();
static int syncWaitLevel(int wait);
static void _CallsSndBuff_Loc(unsigned char* a1, int a2);
static int _MVE_sndAdd(unsigned char* dest, unsigned char** src_ptr, int a3, int a4, int a5);
static void _MVE_sndResume();
static int nfConfig(int a1, int a2, int a3, int is_16_bpp);
static void movieSwapSurfaces();
static void sfShowFrame(int a1, int a2, int a3);
static void _do_nothing_(int a1, int a2, unsigned short* a3);
static void palSetPalette(int start, int count);
static void palClrPalette(int start, int count);
static void palMakeSynthPalette(int a1, int a2, int a3, int a4, int a5, int a6);
static void palLoadPalette(unsigned char* palette, int start, int count);
static void syncRelease();
static void ioRelease();
static void _MVE_sndRelease();
static void nfRelease();
static int _MVE_sndDecompM16(unsigned short* a1, unsigned char* a2, int a3, int a4);
static int _MVE_sndDecompS16(unsigned short* a1, unsigned char* a2, int a3, int a4);
static void _nfPkConfig();
static void _nfPkDecomp(unsigned char* buf, unsigned char* a2, int a3, int a4, int a5, int a6);

// 0x51EBE0
static unsigned short word_51EBE0[256] = {
    // clang-format off
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002A, 0x002B, 0x002F, 0x0033, 0x0038, 0x003D,
    0x0042, 0x0048, 0x004F, 0x0056, 0x005E, 0x0066, 0x0070, 0x007A,
    0x0085, 0x0091, 0x009E, 0x00AD, 0x00BD, 0x00CE, 0x00E1, 0x00F5,
    0x010B, 0x0124, 0x013E, 0x015C, 0x017B, 0x019E, 0x01C4, 0x01ED,
    0x021A, 0x024B, 0x0280, 0x02BB, 0x02FB, 0x0340, 0x038C, 0x03DF,
    0x0439, 0x049C, 0x0508, 0x057D, 0x05FE, 0x0689, 0x0722, 0x07C9,
    0x087F, 0x0945, 0x0A1E, 0x0B0A, 0x0C0C, 0x0D25, 0x0E58, 0x0FA8,
    0x1115, 0x12A4, 0x1458, 0x1633, 0x183A, 0x1A6F, 0x1CD9, 0x1F7B,
    0x225A, 0x257D, 0x28E8, 0x2CA4, 0x30B7, 0x3529, 0x3A03, 0x3F4E,
    0x4515, 0x4B62, 0x5244, 0x59C5, 0x61F6, 0x6AE7, 0x74A8, 0x7F4D,
    0x8AEB, 0x9798, 0xA56E, 0xB486, 0xC4FF, 0xD6F9, 0xEA97, 0xFFFF,
    0x0001, 0x0001, 0x1569, 0x2907, 0x3B01, 0x4B7A, 0x5A92, 0x6868,
    0x7515, 0x80B3, 0x8B58, 0x9519, 0x9E0A, 0xA63B, 0xADBC, 0xB49E,
    0xBAEB, 0xC0B2, 0xC5FD, 0xCAD7, 0xCF49, 0xD35C, 0xD718, 0xDA83,
    0xDDA6, 0xE085, 0xE327, 0xE591, 0xE7C6, 0xE9CD, 0xEBA8, 0xED5C,
    0xEEEB, 0xF058, 0xF1A8, 0xF2DB, 0xF3F4, 0xF4F6, 0xF5E2, 0xF6BB,
    0xF781, 0xF837, 0xF8DE, 0xF977, 0xFA02, 0xFA83, 0xFAF8, 0xFB64,
    0xFBC7, 0xFC21, 0xFC74, 0xFCC0, 0xFD05, 0xFD45, 0xFD80, 0xFDB5,
    0xFDE6, 0xFE13, 0xFE3C, 0xFE62, 0xFE85, 0xFEA4, 0xFEC2, 0xFEDC,
    0xFEF5, 0xFF0B, 0xFF1F, 0xFF32, 0xFF43, 0xFF53, 0xFF62, 0xFF6F,
    0xFF7B, 0xFF86, 0xFF90, 0xFF9A, 0xFFA2, 0xFFAA, 0xFFB1, 0xFFB8,
    0xFFBE, 0xFFC3, 0xFFC8, 0xFFCD, 0xFFD1, 0xFFD5, 0xFFD6, 0xFFD7,
    0xFFD8, 0xFFD9, 0xFFDA, 0xFFDB, 0xFFDC, 0xFFDD, 0xFFDE, 0xFFDF,
    0xFFE0, 0xFFE1, 0xFFE2, 0xFFE3, 0xFFE4, 0xFFE5, 0xFFE6, 0xFFE7,
    0xFFE8, 0xFFE9, 0xFFEA, 0xFFEB, 0xFFEC, 0xFFEE, 0xFFED, 0xFFEF,
    0xFFF0, 0xFFF1, 0xFFF2, 0xFFF3, 0xFFF4, 0xFFF5, 0xFFF6, 0xFFF7,
    0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE, 0xFFFF,
    // clang-format on
};

// 0x51EDE4
static int sync_active = 0;

// 0x51EDE8
static int sync_late = 0;

// 0x51EDEC
static int sync_FrameDropped = 0;

// 0x51EDF8
static int mve_volume = 0;

// 0x51EE08
static MveShowFrameFunc* sf_ShowFrame;

// 0x51EE14
static MveSetPaletteFunc* pal_SetPalette;

// 0x51EE1C
static int rm_active = 0;

// 0x51EE20
static bool dword_51EE20 = false;

// 0x51F018
static int dword_51F018[256];

// 0x51F418
static unsigned short word_51F418[256] = {
    // clang-format off
    0xF8F8, 0xF8F9, 0xF8FA, 0xF8FB, 0xF8FC, 0xF8FD, 0xF8FE, 0xF8FF,
    0xF800, 0xF801, 0xF802, 0xF803, 0xF804, 0xF805, 0xF806, 0xF807,
    0xF9F8, 0xF9F9, 0xF9FA, 0xF9FB, 0xF9FC, 0xF9FD, 0xF9FE, 0xF9FF,
    0xF900, 0xF901, 0xF902, 0xF903, 0xF904, 0xF905, 0xF906, 0xF907,
    0xFAF8, 0xFAF9, 0xFAFA, 0xFAFB, 0xFAFC, 0xFAFD, 0xFAFE, 0xFAFF,
    0xFA00, 0xFA01, 0xFA02, 0xFA03, 0xFA04, 0xFA05, 0xFA06, 0xFA07,
    0xFBF8, 0xFBF9, 0xFBFA, 0xFBFB, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF,
    0xFB00, 0xFB01, 0xFB02, 0xFB03, 0xFB04, 0xFB05, 0xFB06, 0xFB07,
    0xFCF8, 0xFCF9, 0xFCFA, 0xFCFB, 0xFCFC, 0xFCFD, 0xFCFE, 0xFCFF,
    0xFC00, 0xFC01, 0xFC02, 0xFC03, 0xFC04, 0xFC05, 0xFC06, 0xFC07,
    0xFDF8, 0xFDF9, 0xFDFA, 0xFDFB, 0xFDFC, 0xFDFD, 0xFDFE, 0xFDFF,
    0xFD00, 0xFD01, 0xFD02, 0xFD03, 0xFD04, 0xFD05, 0xFD06, 0xFD07,
    0xFEF8, 0xFEF9, 0xFEFA, 0xFEFB, 0xFEFC, 0xFEFD, 0xFEFE, 0xFEFF,
    0xFE00, 0xFE01, 0xFE02, 0xFE03, 0xFE04, 0xFE05, 0xFE06, 0xFE07,
    0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE, 0xFFFF,
    0xFF00, 0xFF01, 0xFF02, 0xFF03, 0xFF04, 0xFF05, 0xFF06, 0xFF07,
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF,
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x01F8, 0x01F9, 0x01FA, 0x01FB, 0x01FC, 0x01FD, 0x01FE, 0x01FF,
    0x0100, 0x0101, 0x0102, 0x0103, 0x0104, 0x0105, 0x0106, 0x0107,
    0x02F8, 0x02F9, 0x02FA, 0x02FB, 0x02FC, 0x02FD, 0x02FE, 0x02FF,
    0x0200, 0x0201, 0x0202, 0x0203, 0x0204, 0x0205, 0x0206, 0x0207,
    0x03F8, 0x03F9, 0x03FA, 0x03FB, 0x03FC, 0x03FD, 0x03FE, 0x03FF,
    0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0x0305, 0x0306, 0x0307,
    0x04F8, 0x04F9, 0x04FA, 0x04FB, 0x04FC, 0x04FD, 0x04FE, 0x04FF,
    0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407,
    0x05F8, 0x05F9, 0x05FA, 0x05FB, 0x05FC, 0x05FD, 0x05FE, 0x05FF,
    0x0500, 0x0501, 0x0502, 0x0503, 0x0504, 0x0505, 0x0506, 0x0507,
    0x06F8, 0x06F9, 0x06FA, 0x06FB, 0x06FC, 0x06FD, 0x06FE, 0x06FF,
    0x0600, 0x0601, 0x0602, 0x0603, 0x0604, 0x0605, 0x0606, 0x0607,
    0x07F8, 0x07F9, 0x07FA, 0x07FB, 0x07FC, 0x07FD, 0x07FE, 0x07FF,
    0x0700, 0x0701, 0x0702, 0x0703, 0x0704, 0x0705, 0x0706, 0x0707,
    // clang-format on
};

// 0x51F618
static unsigned short word_51F618[256] = {
    // clang-format off
    0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x0108,
    0x0109, 0x010A, 0x010B, 0x010C, 0x010D, 0x010E, 0x0208, 0x0209,
    0x020A, 0x020B, 0x020C, 0x020D, 0x020E, 0x0308, 0x0309, 0x030A,
    0x030B, 0x030C, 0x030D, 0x030E, 0x0408, 0x0409, 0x040A, 0x040B,
    0x040C, 0x040D, 0x040E, 0x0508, 0x0509, 0x050A, 0x050B, 0x050C,
    0x050D, 0x050E, 0x0608, 0x0609, 0x060A, 0x060B, 0x060C, 0x060D,
    0x060E, 0x0708, 0x0709, 0x070A, 0x070B, 0x070C, 0x070D, 0x070E,
    0x08F2, 0x08F3, 0x08F4, 0x08F5, 0x08F6, 0x08F7, 0x08F8, 0x08F9,
    0x08FA, 0x08FB, 0x08FC, 0x08FD, 0x08FE, 0x08FF, 0x0800, 0x0801,
    0x0802, 0x0803, 0x0804, 0x0805, 0x0806, 0x0807, 0x0808, 0x0809,
    0x080A, 0x080B, 0x080C, 0x080D, 0x080E, 0x09F2, 0x09F3, 0x09F4,
    0x09F5, 0x09F6, 0x09F7, 0x09F8, 0x09F9, 0x09FA, 0x09FB, 0x09FC,
    0x09FD, 0x09FE, 0x09FF, 0x0900, 0x0901, 0x0902, 0x0903, 0x0904,
    0x0905, 0x0906, 0x0907, 0x0908, 0x0909, 0x090A, 0x090B, 0x090C,
    0x090D, 0x090E, 0x0AF2, 0x0AF3, 0x0AF4, 0x0AF5, 0x0AF6, 0x0AF7,
    0x0AF8, 0x0AF9, 0x0AFA, 0x0AFB, 0x0AFC, 0x0AFD, 0x0AFE, 0x0AFF,
    0x0A00, 0x0A01, 0x0A02, 0x0A03, 0x0A04, 0x0A05, 0x0A06, 0x0A07,
    0x0A08, 0x0A09, 0x0A0A, 0x0A0B, 0x0A0C, 0x0A0D, 0x0A0E, 0x0BF2,
    0x0BF3, 0x0BF4, 0x0BF5, 0x0BF6, 0x0BF7, 0x0BF8, 0x0BF9, 0x0BFA,
    0x0BFB, 0x0BFC, 0x0BFD, 0x0BFE, 0x0BFF, 0x0B00, 0x0B01, 0x0B02,
    0x0B03, 0x0B04, 0x0B05, 0x0B06, 0x0B07, 0x0B08, 0x0B09, 0x0B0A,
    0x0B0B, 0x0B0C, 0x0B0D, 0x0B0E, 0x0CF2, 0x0CF3, 0x0CF4, 0x0CF5,
    0x0CF6, 0x0CF7, 0x0CF8, 0x0CF9, 0x0CFA, 0x0CFB, 0x0CFC, 0x0CFD,
    0x0CFE, 0x0CFF, 0x0C00, 0x0C01, 0x0C02, 0x0C03, 0x0C04, 0x0C05,
    0x0C06, 0x0C07, 0x0C08, 0x0C09, 0x0C0A, 0x0C0B, 0x0C0C, 0x0C0D,
    0x0C0E, 0x0DF2, 0x0DF3, 0x0DF4, 0x0DF5, 0x0DF6, 0x0DF7, 0x0DF8,
    0x0DF9, 0x0DFA, 0x0DFB, 0x0DFC, 0x0DFD, 0x0DFE, 0x0DFF, 0x0D00,
    0x0D01, 0x0D02, 0x0D03, 0x0D04, 0x0D05, 0x0D06, 0x0D07, 0x0D08,
    0x0D09, 0x0D0A, 0x0D0B, 0x0D0C, 0x0D0D, 0x0D0E, 0x0EF2, 0x0EF3,
    0x0EF4, 0x0EF5, 0x0EF6, 0x0EF7, 0x0EF8, 0x0EF9, 0x0EFA, 0x0EFB,
    0x0EFC, 0x0EFD, 0x0EFE, 0x0EFF, 0x0E00, 0x0E01, 0x0E02, 0x0E03,
    0x0E04, 0x0E05, 0x0E06, 0x0E07, 0x0E08, 0x0E09, 0x0E0A, 0x0E0B,
    // clang-format on
};

// 0x51F818
static unsigned int _$$R0053[16] = {
    // clang-format off
    0xC3C3C3C3, 0xC3C3C1C3, 0xC3C3C3C1, 0xC3C3C1C1, 0xC1C3C3C3, 0xC1C3C1C3, 0xC1C3C3C1, 0xC1C3C1C1,
    0xC3C1C3C3, 0xC3C1C1C3, 0xC3C1C3C1, 0xC3C1C1C1, 0xC1C1C3C3, 0xC1C1C1C3, 0xC1C1C3C1, 0xC1C1C1C1,
    // clang-format on
};

// 0x51F858
static unsigned int _$$R0004[256] = {
    // clang-format off
    0xC3C3C3C3, 0xC3C3C2C3, 0xC3C3C1C3, 0xC3C3C5C3, 0xC3C3C3C2, 0xC3C3C2C2, 0xC3C3C1C2, 0xC3C3C5C2,
    0xC3C3C3C1, 0xC3C3C2C1, 0xC3C3C1C1, 0xC3C3C5C1, 0xC3C3C3C5, 0xC3C3C2C5, 0xC3C3C1C5, 0xC3C3C5C5,
    0xC2C3C3C3, 0xC2C3C2C3, 0xC2C3C1C3, 0xC2C3C5C3, 0xC2C3C3C2, 0xC2C3C2C2, 0xC2C3C1C2, 0xC2C3C5C2,
    0xC2C3C3C1, 0xC2C3C2C1, 0xC2C3C1C1, 0xC2C3C5C1, 0xC2C3C3C5, 0xC2C3C2C5, 0xC2C3C1C5, 0xC2C3C5C5,
    0xC1C3C3C3, 0xC1C3C2C3, 0xC1C3C1C3, 0xC1C3C5C3, 0xC1C3C3C2, 0xC1C3C2C2, 0xC1C3C1C2, 0xC1C3C5C2,
    0xC1C3C3C1, 0xC1C3C2C1, 0xC1C3C1C1, 0xC1C3C5C1, 0xC1C3C3C5, 0xC1C3C2C5, 0xC1C3C1C5, 0xC1C3C5C5,
    0xC5C3C3C3, 0xC5C3C2C3, 0xC5C3C1C3, 0xC5C3C5C3, 0xC5C3C3C2, 0xC5C3C2C2, 0xC5C3C1C2, 0xC5C3C5C2,
    0xC5C3C3C1, 0xC5C3C2C1, 0xC5C3C1C1, 0xC5C3C5C1, 0xC5C3C3C5, 0xC5C3C2C5, 0xC5C3C1C5, 0xC5C3C5C5,
    0xC3C2C3C3, 0xC3C2C2C3, 0xC3C2C1C3, 0xC3C2C5C3, 0xC3C2C3C2, 0xC3C2C2C2, 0xC3C2C1C2, 0xC3C2C5C2,
    0xC3C2C3C1, 0xC3C2C2C1, 0xC3C2C1C1, 0xC3C2C5C1, 0xC3C2C3C5, 0xC3C2C2C5, 0xC3C2C1C5, 0xC3C2C5C5,
    0xC2C2C3C3, 0xC2C2C2C3, 0xC2C2C1C3, 0xC2C2C5C3, 0xC2C2C3C2, 0xC2C2C2C2, 0xC2C2C1C2, 0xC2C2C5C2,
    0xC2C2C3C1, 0xC2C2C2C1, 0xC2C2C1C1, 0xC2C2C5C1, 0xC2C2C3C5, 0xC2C2C2C5, 0xC2C2C1C5, 0xC2C2C5C5,
    0xC1C2C3C3, 0xC1C2C2C3, 0xC1C2C1C3, 0xC1C2C5C3, 0xC1C2C3C2, 0xC1C2C2C2, 0xC1C2C1C2, 0xC1C2C5C2,
    0xC1C2C3C1, 0xC1C2C2C1, 0xC1C2C1C1, 0xC1C2C5C1, 0xC1C2C3C5, 0xC1C2C2C5, 0xC1C2C1C5, 0xC1C2C5C5,
    0xC5C2C3C3, 0xC5C2C2C3, 0xC5C2C1C3, 0xC5C2C5C3, 0xC5C2C3C2, 0xC5C2C2C2, 0xC5C2C1C2, 0xC5C2C5C2,
    0xC5C2C3C1, 0xC5C2C2C1, 0xC5C2C1C1, 0xC5C2C5C1, 0xC5C2C3C5, 0xC5C2C2C5, 0xC5C2C1C5, 0xC5C2C5C5,
    0xC3C1C3C3, 0xC3C1C2C3, 0xC3C1C1C3, 0xC3C1C5C3, 0xC3C1C3C2, 0xC3C1C2C2, 0xC3C1C1C2, 0xC3C1C5C2,
    0xC3C1C3C1, 0xC3C1C2C1, 0xC3C1C1C1, 0xC3C1C5C1, 0xC3C1C3C5, 0xC3C1C2C5, 0xC3C1C1C5, 0xC3C1C5C5,
    0xC2C1C3C3, 0xC2C1C2C3, 0xC2C1C1C3, 0xC2C1C5C3, 0xC2C1C3C2, 0xC2C1C2C2, 0xC2C1C1C2, 0xC2C1C5C2,
    0xC2C1C3C1, 0xC2C1C2C1, 0xC2C1C1C1, 0xC2C1C5C1, 0xC2C1C3C5, 0xC2C1C2C5, 0xC2C1C1C5, 0xC2C1C5C5,
    0xC1C1C3C3, 0xC1C1C2C3, 0xC1C1C1C3, 0xC1C1C5C3, 0xC1C1C3C2, 0xC1C1C2C2, 0xC1C1C1C2, 0xC1C1C5C2,
    0xC1C1C3C1, 0xC1C1C2C1, 0xC1C1C1C1, 0xC1C1C5C1, 0xC1C1C3C5, 0xC1C1C2C5, 0xC1C1C1C5, 0xC1C1C5C5,
    0xC5C1C3C3, 0xC5C1C2C3, 0xC5C1C1C3, 0xC5C1C5C3, 0xC5C1C3C2, 0xC5C1C2C2, 0xC5C1C1C2, 0xC5C1C5C2,
    0xC5C1C3C1, 0xC5C1C2C1, 0xC5C1C1C1, 0xC5C1C5C1, 0xC5C1C3C5, 0xC5C1C2C5, 0xC5C1C1C5, 0xC5C1C5C5,
    0xC3C5C3C3, 0xC3C5C2C3, 0xC3C5C1C3, 0xC3C5C5C3, 0xC3C5C3C2, 0xC3C5C2C2, 0xC3C5C1C2, 0xC3C5C5C2,
    0xC3C5C3C1, 0xC3C5C2C1, 0xC3C5C1C1, 0xC3C5C5C1, 0xC3C5C3C5, 0xC3C5C2C5, 0xC3C5C1C5, 0xC3C5C5C5,
    0xC2C5C3C3, 0xC2C5C2C3, 0xC2C5C1C3, 0xC2C5C5C3, 0xC2C5C3C2, 0xC2C5C2C2, 0xC2C5C1C2, 0xC2C5C5C2,
    0xC2C5C3C1, 0xC2C5C2C1, 0xC2C5C1C1, 0xC2C5C5C1, 0xC2C5C3C5, 0xC2C5C2C5, 0xC2C5C1C5, 0xC2C5C5C5,
    0xC1C5C3C3, 0xC1C5C2C3, 0xC1C5C1C3, 0xC1C5C5C3, 0xC1C5C3C2, 0xC1C5C2C2, 0xC1C5C1C2, 0xC1C5C5C2,
    0xC1C5C3C1, 0xC1C5C2C1, 0xC1C5C1C1, 0xC1C5C5C1, 0xC1C5C3C5, 0xC1C5C2C5, 0xC1C5C1C5, 0xC1C5C5C5,
    0xC5C5C3C3, 0xC5C5C2C3, 0xC5C5C1C3, 0xC5C5C5C3, 0xC5C5C3C2, 0xC5C5C2C2, 0xC5C5C1C2, 0xC5C5C5C2,
    0xC5C5C3C1, 0xC5C5C2C1, 0xC5C5C1C1, 0xC5C5C5C1, 0xC5C5C3C5, 0xC5C5C2C5, 0xC5C5C1C5, 0xC5C5C5C5,
    // clang-format on
};

// 0x51FC58
static unsigned int _$$R0063[256] = {
    // clang-format off
    0xE3C3E3C3, 0xE3C7E3C3, 0xE3C1E3C3, 0xE3C5E3C3, 0xE7C3E3C3, 0xE7C7E3C3, 0xE7C1E3C3, 0xE7C5E3C3,
    0xE1C3E3C3, 0xE1C7E3C3, 0xE1C1E3C3, 0xE1C5E3C3, 0xE5C3E3C3, 0xE5C7E3C3, 0xE5C1E3C3, 0xE5C5E3C3,
    0xE3C3E3C7, 0xE3C7E3C7, 0xE3C1E3C7, 0xE3C5E3C7, 0xE7C3E3C7, 0xE7C7E3C7, 0xE7C1E3C7, 0xE7C5E3C7,
    0xE1C3E3C7, 0xE1C7E3C7, 0xE1C1E3C7, 0xE1C5E3C7, 0xE5C3E3C7, 0xE5C7E3C7, 0xE5C1E3C7, 0xE5C5E3C7,
    0xE3C3E3C1, 0xE3C7E3C1, 0xE3C1E3C1, 0xE3C5E3C1, 0xE7C3E3C1, 0xE7C7E3C1, 0xE7C1E3C1, 0xE7C5E3C1,
    0xE1C3E3C1, 0xE1C7E3C1, 0xE1C1E3C1, 0xE1C5E3C1, 0xE5C3E3C1, 0xE5C7E3C1, 0xE5C1E3C1, 0xE5C5E3C1,
    0xE3C3E3C5, 0xE3C7E3C5, 0xE3C1E3C5, 0xE3C5E3C5, 0xE7C3E3C5, 0xE7C7E3C5, 0xE7C1E3C5, 0xE7C5E3C5,
    0xE1C3E3C5, 0xE1C7E3C5, 0xE1C1E3C5, 0xE1C5E3C5, 0xE5C3E3C5, 0xE5C7E3C5, 0xE5C1E3C5, 0xE5C5E3C5,
    0xE3C3E7C3, 0xE3C7E7C3, 0xE3C1E7C3, 0xE3C5E7C3, 0xE7C3E7C3, 0xE7C7E7C3, 0xE7C1E7C3, 0xE7C5E7C3,
    0xE1C3E7C3, 0xE1C7E7C3, 0xE1C1E7C3, 0xE1C5E7C3, 0xE5C3E7C3, 0xE5C7E7C3, 0xE5C1E7C3, 0xE5C5E7C3,
    0xE3C3E7C7, 0xE3C7E7C7, 0xE3C1E7C7, 0xE3C5E7C7, 0xE7C3E7C7, 0xE7C7E7C7, 0xE7C1E7C7, 0xE7C5E7C7,
    0xE1C3E7C7, 0xE1C7E7C7, 0xE1C1E7C7, 0xE1C5E7C7, 0xE5C3E7C7, 0xE5C7E7C7, 0xE5C1E7C7, 0xE5C5E7C7,
    0xE3C3E7C1, 0xE3C7E7C1, 0xE3C1E7C1, 0xE3C5E7C1, 0xE7C3E7C1, 0xE7C7E7C1, 0xE7C1E7C1, 0xE7C5E7C1,
    0xE1C3E7C1, 0xE1C7E7C1, 0xE1C1E7C1, 0xE1C5E7C1, 0xE5C3E7C1, 0xE5C7E7C1, 0xE5C1E7C1, 0xE5C5E7C1,
    0xE3C3E7C5, 0xE3C7E7C5, 0xE3C1E7C5, 0xE3C5E7C5, 0xE7C3E7C5, 0xE7C7E7C5, 0xE7C1E7C5, 0xE7C5E7C5,
    0xE1C3E7C5, 0xE1C7E7C5, 0xE1C1E7C5, 0xE1C5E7C5, 0xE5C3E7C5, 0xE5C7E7C5, 0xE5C1E7C5, 0xE5C5E7C5,
    0xE3C3E1C3, 0xE3C7E1C3, 0xE3C1E1C3, 0xE3C5E1C3, 0xE7C3E1C3, 0xE7C7E1C3, 0xE7C1E1C3, 0xE7C5E1C3,
    0xE1C3E1C3, 0xE1C7E1C3, 0xE1C1E1C3, 0xE1C5E1C3, 0xE5C3E1C3, 0xE5C7E1C3, 0xE5C1E1C3, 0xE5C5E1C3,
    0xE3C3E1C7, 0xE3C7E1C7, 0xE3C1E1C7, 0xE3C5E1C7, 0xE7C3E1C7, 0xE7C7E1C7, 0xE7C1E1C7, 0xE7C5E1C7,
    0xE1C3E1C7, 0xE1C7E1C7, 0xE1C1E1C7, 0xE1C5E1C7, 0xE5C3E1C7, 0xE5C7E1C7, 0xE5C1E1C7, 0xE5C5E1C7,
    0xE3C3E1C1, 0xE3C7E1C1, 0xE3C1E1C1, 0xE3C5E1C1, 0xE7C3E1C1, 0xE7C7E1C1, 0xE7C1E1C1, 0xE7C5E1C1,
    0xE1C3E1C1, 0xE1C7E1C1, 0xE1C1E1C1, 0xE1C5E1C1, 0xE5C3E1C1, 0xE5C7E1C1, 0xE5C1E1C1, 0xE5C5E1C1,
    0xE3C3E1C5, 0xE3C7E1C5, 0xE3C1E1C5, 0xE3C5E1C5, 0xE7C3E1C5, 0xE7C7E1C5, 0xE7C1E1C5, 0xE7C5E1C5,
    0xE1C3E1C5, 0xE1C7E1C5, 0xE1C1E1C5, 0xE1C5E1C5, 0xE5C3E1C5, 0xE5C7E1C5, 0xE5C1E1C5, 0xE5C5E1C5,
    0xE3C3E5C3, 0xE3C7E5C3, 0xE3C1E5C3, 0xE3C5E5C3, 0xE7C3E5C3, 0xE7C7E5C3, 0xE7C1E5C3, 0xE7C5E5C3,
    0xE1C3E5C3, 0xE1C7E5C3, 0xE1C1E5C3, 0xE1C5E5C3, 0xE5C3E5C3, 0xE5C7E5C3, 0xE5C1E5C3, 0xE5C5E5C3,
    0xE3C3E5C7, 0xE3C7E5C7, 0xE3C1E5C7, 0xE3C5E5C7, 0xE7C3E5C7, 0xE7C7E5C7, 0xE7C1E5C7, 0xE7C5E5C7,
    0xE1C3E5C7, 0xE1C7E5C7, 0xE1C1E5C7, 0xE1C5E5C7, 0xE5C3E5C7, 0xE5C7E5C7, 0xE5C1E5C7, 0xE5C5E5C7,
    0xE3C3E5C1, 0xE3C7E5C1, 0xE3C1E5C1, 0xE3C5E5C1, 0xE7C3E5C1, 0xE7C7E5C1, 0xE7C1E5C1, 0xE7C5E5C1,
    0xE1C3E5C1, 0xE1C7E5C1, 0xE1C1E5C1, 0xE1C5E5C1, 0xE5C3E5C1, 0xE5C7E5C1, 0xE5C1E5C1, 0xE5C5E5C1,
    0xE3C3E5C5, 0xE3C7E5C5, 0xE3C1E5C5, 0xE3C5E5C5, 0xE7C3E5C5, 0xE7C7E5C5, 0xE7C1E5C5, 0xE7C5E5C5,
    0xE1C3E5C5, 0xE1C7E5C5, 0xE1C1E5C5, 0xE1C5E5C5, 0xE5C3E5C5, 0xE5C7E5C5, 0xE5C1E5C5, 0xE5C5E5C5,
    // clang-format on
};

// 0x6B3660
static int dword_6B3660;

// 0x6B367C
static int sf_ScreenWidth;

// 0x6B3684
static int rm_FrameDropCount;

// 0x6B3688
static int _snd_buf;

// 0x6B3690
static MveMem io_mem_buf;

// 0x6B369C
static unsigned int io_next_hdr;

// 0x6B36A0
static int dword_6B36A0;

// 0x6B36A4
static unsigned int dword_6B36A4;

// 0x6B36A8
static int rm_FrameCount;

// 0x6B36AC
static int sf_ScreenHeight;

// 0x6B39B8
static MveMallocFunc* mve_malloc_func;

// 0x6B39C0
static int rm_dx;

// 0x6B39C4
static int rm_dy;

// 0x6B39C8
static int _gSoundTimeBase;

// 0x6B39CC
static void* io_handle;

// 0x6B39D0
static int rm_len;

// 0x6B39D4
static MveFreeFunc* mve_free_func;

// 0x6B39D8
static int _snd_comp;

// 0x6B39DC
static unsigned char* rm_p;

// 0x6B39E0
static int dword_6B39E0[60];

// 0x6B3AD0
static int sync_wait_quanta;

// 0x6B3AD8
static int rm_track_bit;

// 0x6B3ADC
static int sync_time;

// 0x6B3AE0
static MveReadFunc* mve_read_func;

// 0x6B3AE4
static int dword_6B3AE4;

// 0x6B3CEC
static int dword_6B3CEC;

// 0x6B3CF8
static int dword_6B3CF8;

// 0x6B3CFC
static int nf_width;

// 0x6B3D00
static int dword_6B3D00;

// 0x6B3D0C
static unsigned char pal_tbl[768];

// 0x6B4016
static unsigned char byte_6B4016;

// 0x6B4017
static int dword_6B4017;

// 0x6B401B
static int dword_6B401B;

// 0x6B401F
static int dword_6B401F;

// 0x6B4023
static int dword_6B4023;

// 0x6B402B
static int dword_6B402B;

// 0x6B402F
static int nf_height;

static MveMem nf_mem_cur;
static unsigned char* nf_buf_cur;
static MveMem nf_mem_prv;
static unsigned char* nf_buf_prv;

static int gMveSoundBuffer = -1;
static unsigned int gMveBufferBytes;

// 0x4F4800
void MveSetMemory(MveMallocFunc* malloc_func, MveFreeFunc* free_func)
{
    mve_malloc_func = malloc_func;
    mve_free_func = free_func;
}

// 0x4F4860
void MveSetIO(MveReadFunc* read_func)
{
    mve_read_func = read_func;
}

// 0x4F4890
static void MVE_MemInit(MveMem* mem, unsigned int size, void* ptr)
{
    if (ptr == nullptr) {
        return;
    }

    MVE_MemFree(mem);

    mem->ptr = ptr;
    mem->size = size;
    mem->alloced = 0;
}

// 0x4F48C0
static void MVE_MemFree(MveMem* mem)
{
    if (mem->alloced && mve_free_func != nullptr) {
        mve_free_func(mem->ptr);
        mem->alloced = 0;
    }
    mem->size = 0;
}

// 0x4F4900
void MveSetVolume(int volume)
{
    mve_volume = volume;

    if (gMveSoundBuffer != -1) {
        audioEngineSoundBufferSetVolume(gMveSoundBuffer, volume);
    }
}

// 0x4F4940
void MveSetScreenSize(int width, int height)
{
    sf_ScreenWidth = width;
    sf_ScreenHeight = height;
}

// 0x4F49F0
void MveSetShowFrame(MveShowFrameFunc* show_frame_func)
{
    sf_ShowFrame = show_frame_func;
}

// 0x4F4A10
void MveSetPalette(MveSetPaletteFunc* set_palette_func)
{
    pal_SetPalette = set_palette_func;
}

// 0x4F4B50
static int _sub_4F4B5()
{
    return 0;
}

// 0x4F4BD0
void MVE_rmFrameCounts(int* frame_count_ptr, int* frame_drop_count_ptr)
{
    *frame_count_ptr = rm_FrameCount;
    *frame_drop_count_ptr = rm_FrameDropCount;
}

// 0x4F4BF0
int MVE_rmPrepMovie(void* handle, int dx, int dy, unsigned char track)
{
    rm_dx = dx;
    rm_dy = dy;
    rm_track_bit = 1 << track;

    if (rm_track_bit == 0) {
        rm_track_bit = 1;
    }

    if (!ioReset(handle)) {
        MVE_rmEndMovie();
        return -8;
    }

    rm_p = ioNextRecord();
    rm_len = 0;

    if (rm_p == NULL) {
        MVE_rmEndMovie();
        return -2;
    }

    rm_active = 1;
    rm_FrameCount = 0;
    rm_FrameDropCount = 0;

    return 0;
}

// 0x4F4C90
static int ioReset(void* handle)
{
    MveHeader* mve;

    io_handle = handle;

    mve = (MveHeader*)ioRead(sizeof(MveHeader));
    if (mve == nullptr) {
        return 0;
    }

    if (strncmp(mve->sig, "Interplay MVE File\x1A\x00", 20) != 0
        || mve->id != 4659 - mve->ver
        || mve->ver != 256
        || mve->size != 26) {
        return 0;
    }

    io_next_hdr = mve->chunk_hdr;

    return 1;
}

// Reads data from movie file.
//
// 0x4F4D00
static void* ioRead(int size)
{
    void* buf;

    buf = MVE_MemAlloc(&io_mem_buf, size);
    if (buf == nullptr) {
        return nullptr;
    }

    if (mve_read_func(io_handle, buf, size) < 1) {
        return nullptr;
    }

    return buf;
}

// 0x4F4D40
static void* MVE_MemAlloc(MveMem* mem, unsigned int size)
{
    void* ptr;

    if (mem->size >= size) {
        return mem->ptr;
    }

    if (mve_malloc_func == nullptr) {
        return nullptr;
    }

    MVE_MemFree(mem);

    ptr = mve_malloc_func(size + 100);
    if (ptr == nullptr) {
        return nullptr;
    }

    MVE_MemInit(mem, size + 100, ptr);

    mem->alloced = 1;

    return mem->ptr;
}

// 0x4F4DA0
static unsigned char* ioNextRecord()
{
    unsigned char* buf;

    buf = (unsigned char*)ioRead((io_next_hdr & 0xFFFF) + 4);
    if (buf == nullptr) {
        return nullptr;
    }

    io_next_hdr = *(unsigned int*)(buf + (io_next_hdr & 0xFFFF));

    return buf;
}

// 0x4F4E40
static int syncWait()
{
    int late;

    late = 0;
    if (sync_active) {
        while ((sync_time + 1000 * compat_timeGetTime()) < 0) {
            late = 1;
            delay_ms(-(sync_time + 1000 * compat_timeGetTime()) / 1000 - 3);
        }
        sync_time += sync_wait_quanta;
    }

    return late;
}

// 0x4F4EA0
static void _MVE_sndPause()
{
    if (gMveSoundBuffer != -1) {
        audioEngineSoundBufferStop(gMveSoundBuffer);
    }
}

// 0x4F4EC0
int _MVE_rmStepMovie()
{
    int v0;
    unsigned short* v1;
    unsigned int v5;
    int v6;
    int v7;
    int v8;
    int v9;
    int v10;
    unsigned short* v3;
    unsigned short* v21;
    int v18;
    int v19;
    int v20;
    unsigned char* v14;

    v0 = rm_len;
    v1 = (unsigned short*)rm_p;

    if (!rm_active) {
        return -10;
    }

LABEL_5:
    v21 = nullptr;
    v3 = nullptr;
    if (!v1) {
        v6 = -2;
        MVE_rmEndMovie();
        return v6;
    }

    while (1) {
        v5 = *(unsigned int*)((unsigned char*)v1 + v0);
        v1 = (unsigned short*)((unsigned char*)v1 + v0 + 4);
        v0 = v5 & 0xFFFF;

        switch ((v5 >> 16) & 0xFF) {
        case 0:
            return -1;
        case 1:
            v0 = 0;
            v1 = (unsigned short*)ioNextRecord();
            goto LABEL_5;
        case 2:
            if (!syncInit(v1[0], v1[2])) {
                v6 = -3;
                break;
            }
            continue;
        case 3:
            if ((v5 >> 24) < 1) {
                v7 = 0;
            } else {
                v7 = (v1[1] & 0x04) >> 2;
            }
            v8 = *(unsigned int*)((unsigned char*)v1 + 6);
            if ((v5 >> 24) == 0) {
                v8 &= 0xFFFF;
            }

            if (_MVE_sndConfigure(v1[0], v8, v1[1] & 0x01, v1[2], (v1[1] & 0x02) >> 1, v7)) {
                continue;
            }

            v6 = -4;
            break;
        case 4:
            // initialize audio buffers
            _MVE_sndSync();
            continue;
        case 5:
            v9 = 0;
            if ((v5 >> 24) >= 2) {
                v9 = v1[3];
            }

            v10 = 1;
            if ((v5 >> 24) >= 1) {
                v10 = v1[2];
            }

            if (!nfConfig(v1[0], v1[1], v10, v9)) {
                MVE_rmEndMovie();
                return -5;
            }

            if (rm_dx + nf_width > sf_ScreenWidth
                || rm_dy + nf_height > sf_ScreenHeight) {
                MVE_rmEndMovie();
                return -6;
            }

            continue;
        case 7:
            ++rm_FrameCount;

            v18 = 0;
            if ((v5 >> 24) >= 1) {
                v18 = v1[2];
            }

            v19 = v1[1];
            if (v19 == 0 || v21) {
                palSetPalette(v1[0], v19);
            } else {
                palClrPalette(v1[0], v19);
            }

            if (v21) {
                _do_nothing_(rm_dx, rm_dy, v21);
            } else if (!sync_late || v1[1]) {
                sfShowFrame(rm_dx, rm_dy, v18);
            } else {
                sync_FrameDropped = 1;
                ++rm_FrameDropCount;
            }

            v20 = v1[1];
            if (v20 && !v21) {
                palSetPalette(v1[0], v20);
            }

            rm_p = (unsigned char*)v1;
            rm_len = v0;

            return 0;
        case 8:
        case 9:
            // push data to audio buffers?
            if (v1[1] & rm_track_bit) {
                v14 = (unsigned char*)v1 + 6;
                if ((v5 >> 16) != 8) {
                    v14 = nullptr;
                }
                _CallsSndBuff_Loc(v14, v1[2]);
            }
            continue;
        case 10:
            // TODO: Probably never reached.
            continue;
        case 11:
            // some kind of palette rotation
            palMakeSynthPalette(v1[0], v1[1], v1[2], v1[3], v1[4], v1[5]);
            continue;
        case 12:
            // palette
            palLoadPalette((unsigned char*)v1 + 4, v1[0], v1[1]);
            continue;
        case 14:
            // save current position
            v21 = v1;
            continue;
        case 15:
            // save current position
            v3 = v1;
            continue;
        case 17:
            // decode video chunk
            if ((v5 >> 24) < 3) {
                v6 = -8;
                break;
            }

            // swap movie surfaces
            if (v1[6] & 0x01) {
                movieSwapSurfaces();
            }

            _nfPkDecomp((unsigned char*)v3, (unsigned char*)&v1[7], v1[2], v1[3], v1[4], v1[5]);

            continue;
        default:
            // unknown chunk
            continue;
        }
    }

    MVE_rmEndMovie();
    return v6;
}

// 0x4F54F0
static int syncInit(int rate, int resolution)
{
    int quanta;

    quanta = -(rate * resolution + resolution / 2);

    if (!sync_active || sync_wait_quanta != quanta) {
        syncWait();
        sync_wait_quanta = quanta;
        syncReset(quanta);
    }

    return 1;
}

// 0x4F5540
static void syncReset(int quanta)
{
    sync_active = 1;
    sync_time = -1000 * compat_timeGetTime() + quanta;
}

// 0x4F5570
static int _MVE_sndConfigure(int a1, int a2, int a3, int a4, int a5, int a6)
{
    MVE_syncSync();
    _MVE_sndReset();

    _snd_comp = a3;
    dword_6B36A0 = a5;
    _snd_buf = a6;

    gMveBufferBytes = (a2 + (a2 >> 1)) & 0xFFFFFFFC;

    dword_6B3AE4 = 0;
    dword_6B3660 = 0;

    gMveSoundBuffer = audioEngineCreateSoundBuffer(gMveBufferBytes, a5 < 1 ? 8 : 16, 2 - (a3 < 1), a4);
    if (gMveSoundBuffer == -1) {
        return 0;
    }

    audioEngineSoundBufferSetVolume(gMveSoundBuffer, mve_volume);

    dword_6B36A4 = 0;

    return 1;
}

// 0x4F56C0
static void MVE_syncSync()
{
    if (sync_active) {
        while (sync_time + 1000 * compat_timeGetTime() < 0) {
        }
    }
}

// 0x4F56F0
static void _MVE_sndReset()
{
    if (gMveSoundBuffer != -1) {
        audioEngineSoundBufferStop(gMveSoundBuffer);
        audioEngineSoundBufferRelease(gMveSoundBuffer);
        gMveSoundBuffer = -1;
    }
}

// 0x4F5720
static void _MVE_sndSync()
{
    unsigned int dwCurrentPlayCursor;
    unsigned int dwCurrentWriteCursor;
    bool v10;
    unsigned int dwStatus;
    unsigned int v1;
    bool v2;
    int v3;
    unsigned int v4;
    bool v5;
    bool v0;
    unsigned int v6;
    int v7;
    int v8;
    unsigned int v9;

    v0 = false;

    sync_late = syncWaitLevel(sync_wait_quanta / 4) > -sync_wait_quanta / 2 && !sync_FrameDropped;
    sync_FrameDropped = 0;

    if (gMveSoundBuffer == -1) {
        return;
    }

    while (1) {
        if (!audioEngineSoundBufferGetStatus(gMveSoundBuffer, &dwStatus)) {
            return;
        }

        if (!audioEngineSoundBufferGetCurrentPosition(gMveSoundBuffer, &dwCurrentPlayCursor, &dwCurrentWriteCursor)) {
            return;
        }

        dwCurrentWriteCursor = dword_6B36A4;

        v1 = (gMveBufferBytes + dword_6B39E0[dword_6B3660] - _gSoundTimeBase)
            % gMveBufferBytes;

        if (dwCurrentPlayCursor <= dword_6B36A4) {
            if (v1 < dwCurrentPlayCursor || v1 >= dword_6B36A4) {
                v2 = false;
            } else {
                v2 = true;
            }
        } else {
            if (v1 < dwCurrentPlayCursor && v1 >= dword_6B36A4) {
                v2 = false;
            } else {
                v2 = true;
            }
        }

        if (!v2 || !(dwStatus & AUDIO_ENGINE_SOUND_BUFFER_STATUS_PLAYING)) {
            if (v0) {
                syncReset(sync_wait_quanta + (sync_wait_quanta >> 2));
            }

            v3 = dword_6B39E0[dword_6B3660];

            if (!(dwStatus & AUDIO_ENGINE_SOUND_BUFFER_STATUS_PLAYING)) {
                v4 = (gMveBufferBytes + v3) % gMveBufferBytes;

                if (dwCurrentWriteCursor >= dwCurrentPlayCursor) {
                    if (v4 >= dwCurrentPlayCursor && v4 < dwCurrentWriteCursor) {
                        v5 = true;
                    } else {
                        v5 = false;
                    }
                } else if (v4 >= dwCurrentPlayCursor || v4 < dwCurrentWriteCursor) {
                    v5 = true;
                } else {
                    v5 = false;
                }

                if (v5) {
                    if (!audioEngineSoundBufferSetCurrentPosition(gMveSoundBuffer, v4)) {
                        return;
                    }

                    if (!audioEngineSoundBufferPlay(gMveSoundBuffer, 1)) {
                        return;
                    }
                }

                break;
            }

            v6 = (gMveBufferBytes + _gSoundTimeBase + v3) % gMveBufferBytes;
            v7 = dwCurrentWriteCursor - dwCurrentPlayCursor;

            if (((dwCurrentWriteCursor - dwCurrentPlayCursor) & 0x80000000) != 0) {
                v7 += gMveBufferBytes;
            }

            v8 = gMveBufferBytes - v7 - 1;
            // NOTE: Original code uses signed comparison.
            if ((int)gMveBufferBytes / 2 < v8) {
                v8 = gMveBufferBytes >> 1;
            }

            v9 = (gMveBufferBytes + dwCurrentPlayCursor - v8) % gMveBufferBytes;

            dwCurrentPlayCursor = v9;

            if (dwCurrentWriteCursor >= v9) {
                if (v6 < dwCurrentPlayCursor || v6 >= dwCurrentWriteCursor) {
                    v10 = false;
                } else {
                    v10 = true;
                }
            } else {
                if (v6 >= v9 || v6 < dwCurrentWriteCursor) {
                    v10 = true;
                } else {
                    v10 = false;
                }
            }

            if (!v10) {
                audioEngineSoundBufferStop(gMveSoundBuffer);
            }

            break;
        }
        v0 = true;

#if defined(__EMSCRIPTEN__)
        delay_ms(1);
#endif
    }

    if (dword_6B3660 != dword_6B3AE4) {
        if (dword_6B3660 == 59) {
            dword_6B3660 = 0;
        } else {
            ++dword_6B3660;
        }
    }
}

// 0x4F59B0
static int syncWaitLevel(int wait)
{
    int deadline;
    int diff;

    if (!sync_active) {
        return 0;
    }

    deadline = sync_time + wait;
    do {
        diff = deadline + 1000 * compat_timeGetTime();
        if (diff < 0) {
            delay_ms(-diff / 1000 - 3);
        }
        diff = deadline + 1000 * compat_timeGetTime();
    } while (diff < 0);

    sync_time += sync_wait_quanta;

    return diff;
}

// 0x4F5A00
static void _CallsSndBuff_Loc(unsigned char* a1, int a2)
{
    int v2;
    int v3;
    int v5;
    unsigned int dwCurrentPlayCursor;
    unsigned int dwCurrentWriteCursor;
    void* lpvAudioPtr1;
    unsigned int dwAudioBytes1;
    void* lpvAudioPtr2;
    unsigned int dwAudioBytes2;

    _gSoundTimeBase = a2;

    if (gMveSoundBuffer == -1) {
        return;
    }

    v5 = 60;
    if (dword_6B3660) {
        v5 = dword_6B3660;
    }

    if (dword_6B3AE4 - v5 == -1) {
        return;
    }

    if (!audioEngineSoundBufferGetCurrentPosition(gMveSoundBuffer, &dwCurrentPlayCursor, &dwCurrentWriteCursor)) {
        return;
    }

    dwCurrentWriteCursor = dword_6B36A4;

    if (!audioEngineSoundBufferLock(gMveSoundBuffer, dword_6B36A4, a2, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2, 0)) {
        return;
    }

    v2 = 0;
    v3 = 1;
    if (dwAudioBytes1 != 0) {
        v2 = _MVE_sndAdd((unsigned char*)lpvAudioPtr1, &a1, dwAudioBytes1, 0, 1);
        v3 = 0;
        dword_6B36A4 += dwAudioBytes1;
    }

    if (dwAudioBytes2 != 0) {
        _MVE_sndAdd((unsigned char*)lpvAudioPtr2, &a1, dwAudioBytes2, v2, v3);
        dword_6B36A4 = dwAudioBytes2;
    }

    if (dword_6B36A4 == gMveBufferBytes) {
        dword_6B36A4 = 0;
    }

    audioEngineSoundBufferUnlock(gMveSoundBuffer, lpvAudioPtr1, dwAudioBytes1, lpvAudioPtr2, dwAudioBytes2);

    dword_6B39E0[dword_6B3AE4] = dwCurrentWriteCursor;

    if (dword_6B3AE4 == 59) {
        dword_6B3AE4 = 0;
    } else {
        ++dword_6B3AE4;
    }
}

// 0x4F5B70
static int _MVE_sndAdd(unsigned char* dest, unsigned char** src_ptr, int a3, int a4, int a5)
{
    unsigned char* src;
    int v9;
    unsigned short* v10;
    int v11;
    int result;

    int v12;
    unsigned short* v13;
    int v14;

    src = *src_ptr;

    if (*src_ptr == nullptr) {
        memset(dest, dword_6B36A0 < 1 ? 0x80 : 0, a3);
        *src_ptr = nullptr;
        return a4;
    }

    if (!_snd_buf) {
        memcpy(dest, src_ptr, a3);
        *src_ptr += a3;
        return a4;
    }

    if (!_snd_comp) {
        if (a5) {
            v9 = *(unsigned short*)src;
            src += 2;

            *(unsigned short*)dest = v9;
            v10 = (unsigned short*)(dest + 2);
            v11 = a3 - 2;
        } else {
            v9 = a4;
            v10 = (unsigned short*)dest;
            v11 = a3;
        }

        result = _MVE_sndDecompM16(v10, src, v11 >> 1, v9);
        *src_ptr = src + (v11 >> 1);
        return result;
    }

    if (a5) {
        v12 = *(unsigned int*)src;
        src += 4;

        *(unsigned int*)dest = v12;
        v13 = (unsigned short*)(dest + 4);
        v14 = a3 - 4;
    } else {
        v13 = (unsigned short*)dest;
        v14 = a3;
        v12 = a4;
    }

    result = _MVE_sndDecompS16(v13, src, v14 >> 2, v12);
    *src_ptr = src + (v14 >> 1);

    return result;
}

// 0x4F5CA0
static void _MVE_sndResume()
{
}

// 0x4F5CB0
static int nfConfig(int width, int height, int a3, int is_16_bpp)
{
    byte_6B4016 = a3;
    nf_width = 8 * width;
    nf_height = 8 * height * a3;

    nf_buf_cur = (unsigned char*)MVE_MemAlloc(&nf_mem_cur, nf_width * nf_height);
    if (nf_buf_cur == nullptr) {
        return 0;
    }

    nf_buf_prv = (unsigned char*)MVE_MemAlloc(&nf_mem_prv, nf_width * nf_height);
    if (nf_buf_prv == nullptr) {
        return 0;
    }

    dword_6B3D00 = 8 * a3 * nf_width;
    dword_6B3CEC = 7 * a3 * nf_width;

    _nfPkConfig();

    return 1;
}

// 0x4F5F20
static void movieSwapSurfaces()
{
    unsigned char* tmp = nf_buf_prv;
    nf_buf_prv = nf_buf_cur;
    nf_buf_cur = tmp;
}

// 0x4F5F40
static void sfShowFrame(int dst_x, int dst_y, int a3)
{
    dst_x = (sf_ScreenWidth - nf_width) / 2;
    dst_y = (sf_ScreenHeight - nf_height) / 2;

    if (a3 == 0) {
        sf_ShowFrame(nf_buf_cur, nf_width, nf_height, 0, 0, nf_width, nf_height, dst_x, dst_y);
    }
}

// 0x4F6080
static void _do_nothing_(int a1, int a2, unsigned short* a3)
{
}

// 0x4F6090
static void palSetPalette(int start, int count)
{
    pal_SetPalette(pal_tbl, start, count);
}

// 0x4F60C0
static void palClrPalette(int start, int count)
{
    unsigned char palette[768];

    memset(palette, 0, sizeof(palette));
    pal_SetPalette(palette, start, count);
}

// 0x4F60F0
static void palMakeSynthPalette(int a1, int a2, int a3, int a4, int a5, int a6)
{
    int i;
    int j;

    for (i = 0; i < a2; i++) {
        for (j = 0; j < a3; j++) {
            pal_tbl[3 * a1 + 3 * j] = (63 * i) / (a2 - 1);
            pal_tbl[3 * a1 + 3 * j + 1] = 0;
            pal_tbl[3 * a1 + 3 * j + 2] = 5 * ((63 * j) / (a3 - 1)) / 8;
        }
    }

    for (i = 0; i < a5; i++) {
        for (j = 0; j < a6; j++) {
            pal_tbl[3 * a4 + 3 * j] = 0;
            pal_tbl[3 * a4 + 3 * j + 1] = (63 * i) / (a5 - 1);
            pal_tbl[3 * a1 + 3 * j + 2] = 5 * ((63 * j) / (a6 - 1)) / 8;
        }
    }
}

// 0x4F6210
static void palLoadPalette(unsigned char* palette, int start, int count)
{
    memcpy(&(pal_tbl[start * 3]), palette, count * 3);
}

// 0x4F6240
void MVE_rmEndMovie()
{
    if (rm_active) {
        syncWait();
        syncRelease();
        _MVE_sndReset();
        rm_active = 0;
    }
}

// 0x4F6270
static void syncRelease()
{
    sync_active = 0;
}

// 0x4F6350
void MVE_ReleaseMem()
{
    MVE_rmEndMovie();
    ioRelease();
    _MVE_sndRelease();
    nfRelease();
}

// 0x4F6370
static void ioRelease()
{
    MVE_MemFree(&io_mem_buf);
}

// 0x4F6380
static void _MVE_sndRelease()
{
}

// 0x4F6390
static void nfRelease()
{
    MVE_MemFree(&nf_mem_cur);
    nf_buf_cur = nullptr;

    MVE_MemFree(&nf_mem_prv);
    nf_buf_prv = nullptr;
}

// 0x4F697C
static int _MVE_sndDecompM16(unsigned short* a1, unsigned char* a2, int a3, int a4)
{
    int i;
    int v8;
    unsigned short result;

    result = a4;

    v8 = 0;
    for (i = 0; i < a3; i++) {
        v8 = *a2++;
        result += word_51EBE0[v8];
        *a1++ = result;
    }

    return result;
}

// 0x4F69AD
static int _MVE_sndDecompS16(unsigned short* a1, unsigned char* a2, int a3, int a4)
{
    int i;
    unsigned short v4;
    unsigned short v5;
    unsigned short v9;

    v4 = a4 & 0xFFFF;
    v5 = (a4 >> 16) & 0xFFFF;

    v9 = 0;
    for (i = 0; i < a3; i++) {
        v9 = *a2++;
        v4 = (word_51EBE0[v9] + v4) & 0xFFFF;
        *a1++ = v4;

        v9 = *a2++;
        v5 = (word_51EBE0[v9] + v5) & 0xFFFF;
        *a1++ = v5;
    }

    return (v5 << 16) | v4;
}

// 0x4F731D
static void _nfPkConfig()
{
    int* ptr;
    int v1;
    int v2;
    int v3;
    int v4;
    int v5;

    ptr = dword_51F018;
    v1 = nf_width;
    v2 = 0;

    v3 = 128;
    do {
        *ptr++ = v2;
        v2 += v1;
        --v3;
    } while (v3);

    v4 = -128 * v1;
    v5 = 128;
    do {
        *ptr++ = v4;
        v4 += v1;
        --v5;
    } while (v5);
}

// 0x4F7359
static void _nfPkDecomp(unsigned char* a1, unsigned char* a2, int a3, int a4, int a5, int a6)
{
    int v49;
    unsigned char* dest;
    int v8;
    int v7;
    int i;
    int j;
    ptrdiff_t v10;
    int v11;
    int v13;
    int byte;
    unsigned int value1;
    unsigned int value2;
    int var_10;
    unsigned char map1[512];
    unsigned int map2[256];
    int var_8;
    unsigned int* src_ptr;
    unsigned int* dest_ptr;
    unsigned int nibbles[2];

    dword_6B401B = 8 * a3;
    dword_6B4017 = 8 * a5;
    dword_6B401F = 8 * a4 * byte_6B4016;
    dword_6B4023 = 8 * a6 * byte_6B4016;

    var_8 = dword_6B3D00 - dword_6B4017;
    dest = nf_buf_cur;

    var_10 = dword_6B3CEC - 8;

    if (a3 || a4) {
        dest = nf_buf_cur + dword_6B401B + nf_width * dword_6B401F;
    }

    while (a6--) {
        v49 = a5 >> 1;
        while (v49--) {
            v8 = *a1++;
            nibbles[0] = v8 & 0xF;
            nibbles[1] = v8 >> 4;
            for (j = 0; j < 2; j++) {
                v7 = nibbles[j];

                switch (v7) {
                case 1:
                    dest += 8;
                    break;
                case 0:
                case 2:
                case 3:
                case 4:
                case 5:
                    switch (v7) {
                    case 0:
                        v10 = nf_buf_prv - nf_buf_cur;
                        break;
                    case 2:
                    case 3:
                        byte = *a2++;
                        v11 = word_51F618[byte];
                        if (v7 == 3) {
                            v11 = ((-(v11 & 0xFF)) & 0xFF) | ((-(v11 >> 8) & 0xFF) << 8);
                        } else {
                            v11 = v11;
                        }
                        v10 = ((v11 << 24) >> 24) + dword_51F018[v11 >> 8];
                        break;
                    case 4:
                    case 5:
                        if (v7 == 4) {
                            byte = *a2++;
                            v13 = word_51F418[byte];
                        } else {
                            v13 = *(unsigned short*)a2;
                            a2 += 2;
                        }

                        v10 = ((v13 << 24) >> 24) + dword_51F018[v13 >> 8] + (nf_buf_prv - nf_buf_cur);
                        break;
                    }

                    value2 = nf_width;

                    for (i = 0; i < 8; i++) {
                        src_ptr = (unsigned int*)(dest + v10);
                        dest_ptr = (unsigned int*)dest;

                        dest_ptr[0] = src_ptr[0];
                        dest_ptr[1] = src_ptr[1];

                        dest += value2;
                    }

                    dest -= value2;

                    dest -= var_10;

                    break;
                case 6:
                    nibbles[0] += 2;
                    while (nibbles[0]--) {
                        dest += 16;

                        if (v49--) {
                            continue;
                        }

                        dest += var_8;

                        a6--;
                        v49 = (a5 >> 1) - 1;
                    }
                    break;
                case 7:
                    if (a2[0] > a2[1]) {
                        // 7/1
                        for (i = 0; i < 2; i++) {
                            value1 = _$$R0053[a2[2 + i] & 0xF];
                            map1[i * 8] = value1 & 0xFF;
                            map1[i * 8 + 1] = (value1 >> 8) & 0xFF;
                            map1[i * 8 + 2] = (value1 >> 16) & 0xFF;
                            map1[i * 8 + 3] = (value1 >> 24) & 0xFF;

                            value1 = _$$R0053[a2[2 + i] >> 4];
                            map1[i * 8 + 4] = value1 & 0xFF;
                            map1[i * 8 + 5] = (value1 >> 8) & 0xFF;
                            map1[i * 8 + 6] = (value1 >> 16) & 0xFF;
                            map1[i * 8 + 7] = (value1 >> 24) & 0xFF;
                        }

                        map2[0xC1] = (a2[1] << 8) | a2[1]; // cx
                        map2[0xC3] = (a2[0] << 8) | a2[0]; // bx

                        value2 = nf_width;

                        for (i = 0; i < 4; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 4]] << 16) | (map2[map1[i * 4 + 1]]);

                            dest_ptr = (unsigned int*)(dest + value2);
                            dest_ptr[0] = (map2[map1[i * 4]] << 16) | (map2[map1[i * 4 + 1]]);

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[1] = (map2[map1[i * 4 + 2]] << 16) | (map2[map1[i * 4 + 3]]);

                            dest_ptr = (unsigned int*)(dest + value2);
                            dest_ptr[1] = (map2[map1[i * 4 + 2]] << 16) | (map2[map1[i * 4 + 3]]);

                            dest += value2 * 2;
                        }

                        dest -= value2;

                        a2 += 4;
                        dest -= var_10;
                    } else {
                        // 7/2
                        // VERIFIED
                        for (i = 0; i < 8; i++) {
                            value1 = _$$R0004[a2[2 + i]];
                            map1[i * 4] = value1 & 0xFF;
                            map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        map2[0xC1] = (a2[1] << 8) | a2[0]; // cx
                        map2[0xC3] = (a2[0] << 8) | a2[0]; // bx
                        map2[0xC2] = (a2[0] << 8) | a2[1]; // dx
                        map2[0xC5] = (a2[1] << 8) | a2[1]; // bp

                        value2 = nf_width;

                        for (i = 0; i < 8; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 4]] << 16) | map2[map1[i * 4 + 1]];
                            dest_ptr[1] = (map2[map1[i * 4 + 2]] << 16) | map2[map1[i * 4 + 3]];

                            dest += value2;
                        }

                        dest -= value2;

                        a2 += 10;
                        dest -= var_10;
                    }

                    break;
                case 8:
                    if (a2[0] > a2[1]) {
                        if (a2[6] > a2[7]) {
                            // 8/1
                            for (i = 0; i < 4; i++) {
                                value1 = _$$R0004[a2[2 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            for (i = 0; i < 4; i++) {
                                value1 = _$$R0004[a2[8 + i]];
                                map1[16 + i * 4] = value1 & 0xFF;
                                map1[16 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[16 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[16 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            value2 = nf_width;

                            map2[0xC1] = (a2[1] << 8) | a2[0]; // cx
                            map2[0xC3] = (a2[0] << 8) | a2[0]; // bx
                            map2[0xC2] = (a2[0] << 8) | a2[1]; // dx
                            map2[0xC5] = (a2[1] << 8) | a2[1]; // bp

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4]] << 16) | map2[map1[i * 4 + 1]];
                                dest_ptr[1] = (map2[map1[i * 4 + 2]] << 16) | map2[map1[i * 4 + 3]];

                                dest += value2;
                            }

                            map2[0xC1] = (a2[6 + 1] << 8) | a2[6 + 0]; // cx
                            map2[0xC3] = (a2[6 + 0] << 8) | a2[6 + 0]; // bx
                            map2[0xC2] = (a2[6 + 0] << 8) | a2[6 + 1]; // dx
                            map2[0xC5] = (a2[6 + 1] << 8) | a2[6 + 1]; // bp

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[16 + i * 4]] << 16) | map2[map1[16 + i * 4 + 1]];
                                dest_ptr[1] = (map2[map1[16 + i * 4 + 2]] << 16) | map2[map1[16 + i * 4 + 3]];

                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 12;
                            dest -= var_10;
                        } else {
                            // 8/2
                            for (i = 0; i < 4; i++) {
                                value1 = _$$R0004[a2[2 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            for (i = 0; i < 4; i++) {
                                value1 = _$$R0004[a2[8 + i]];
                                map1[16 + i * 4] = value1 & 0xFF;
                                map1[16 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[16 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[16 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            value2 = nf_width;

                            map2[0xC1] = (a2[1] << 8) | a2[0]; // cx
                            map2[0xC3] = (a2[0] << 8) | a2[0]; // bx
                            map2[0xC2] = (a2[0] << 8) | a2[1]; // dx
                            map2[0xC5] = (a2[1] << 8) | a2[1]; // bp

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4]] << 16) | map2[map1[i * 4 + 1]];
                                dest += value2;

                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4 + 2]] << 16) | map2[map1[i * 4 + 3]];
                                dest += value2;
                            }

                            dest -= value2 * 8 - 4;

                            map2[0xC1] = (a2[6 + 1] << 8) | a2[6 + 0]; // cx
                            map2[0xC3] = (a2[6 + 0] << 8) | a2[6 + 0]; // bx
                            map2[0xC2] = (a2[6 + 0] << 8) | a2[6 + 1]; // dx
                            map2[0xC5] = (a2[6 + 1] << 8) | a2[6 + 1]; // bp

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[16 + i * 4]] << 16) | map2[map1[16 + i * 4 + 1]];
                                dest += value2;

                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[16 + i * 4 + 2]] << 16) | map2[map1[16 + i * 4 + 3]];
                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 12;
                            dest -= 4;
                            dest -= var_10;
                        }
                    } else {
                        // 8/3
                        // VERIFIED
                        for (i = 0; i < 2; i++) {
                            value1 = _$$R0004[a2[2 + i]];
                            map1[i * 4] = value1 & 0xFF;
                            map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 2; i++) {
                            value1 = _$$R0004[a2[6 + i]];
                            map1[8 + i * 4] = value1 & 0xFF;
                            map1[8 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[8 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[8 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 2; i++) {
                            value1 = _$$R0004[a2[10 + i]];
                            map1[16 + i * 4] = value1 & 0xFF;
                            map1[16 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[16 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[16 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 2; i++) {
                            value1 = _$$R0004[a2[14 + i]];
                            map1[24 + i * 4] = value1 & 0xFF;
                            map1[24 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[24 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[24 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        value2 = nf_width;

                        map2[0xC1] = (a2[1] << 8) | a2[0]; // cx
                        map2[0xC3] = (a2[0] << 8) | a2[0]; // bx
                        map2[0xC2] = (a2[0] << 8) | a2[1]; // dx
                        map2[0xC5] = (a2[1] << 8) | a2[1]; // bp

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 4]] << 16) | map2[map1[i * 4 + 1]];
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 4 + 2]] << 16) | map2[map1[i * 4 + 3]];
                            dest += value2;
                        }

                        map2[0xC1] = (a2[4 + 1] << 8) | a2[4 + 0]; // cx
                        map2[0xC3] = (a2[4 + 0] << 8) | a2[4 + 0]; // bx
                        map2[0xC2] = (a2[4 + 0] << 8) | a2[4 + 1]; // dx
                        map2[0xC5] = (a2[4 + 1] << 8) | a2[4 + 1]; // bp

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[8 + i * 4]] << 16) | map2[map1[8 + i * 4 + 1]];
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[8 + i * 4 + 2]] << 16) | map2[map1[8 + i * 4 + 3]];
                            dest += value2;
                        }

                        dest -= value2 * 8 - 4;

                        map2[0xC1] = (a2[8 + 1] << 8) | a2[8 + 0]; // cx
                        map2[0xC3] = (a2[8 + 0] << 8) | a2[8 + 0]; // bx
                        map2[0xC2] = (a2[8 + 0] << 8) | a2[8 + 1]; // dx
                        map2[0xC5] = (a2[8 + 1] << 8) | a2[8 + 1]; // bp

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[16 + i * 4]] << 16) | map2[map1[16 + i * 4 + 1]];
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[16 + i * 4 + 2]] << 16) | map2[map1[16 + i * 4 + 3]];
                            dest += value2;
                        }

                        map2[0xC1] = (a2[12 + 1] << 8) | a2[12 + 0]; // cx
                        map2[0xC3] = (a2[12 + 0] << 8) | a2[12 + 0]; // bx
                        map2[0xC2] = (a2[12 + 0] << 8) | a2[12 + 1]; // dx
                        map2[0xC5] = (a2[12 + 1] << 8) | a2[12 + 1]; // bp

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[24 + i * 4]] << 16) | map2[map1[24 + i * 4 + 1]];
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[24 + i * 4 + 2]] << 16) | map2[map1[24 + i * 4 + 3]];
                            dest += value2;
                        }

                        dest -= value2;

                        a2 += 16;
                        dest -= 4;
                        dest -= var_10;
                    }

                    break;
                case 9:
                    if (a2[0] > a2[1]) {
                        if (a2[2] > a2[3]) {
                            // 9/1
                            // VERIFIED
                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            value2 = nf_width;

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 8]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                                dest_ptr[1] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);

                                dest_ptr = (unsigned int*)(dest + value2);
                                dest_ptr[0] = (map2[map1[i * 8]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                                dest_ptr[1] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);

                                dest += value2 * 2;
                            }

                            dest -= value2;

                            a2 += 12;
                            dest -= var_10;
                        } else {
                            // 9/2
                            // VERIFIED
                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4 + 3] = value1 & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 16) & 0xFF;
                                map1[i * 4] = (value1 >> 24) & 0xFF;
                            }

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            value2 = nf_width;

                            for (i = 0; i < 8; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4]] << 24) | (map2[map1[i * 4 + 0]] << 16) | (map2[map1[i * 4 + 1]] << 8) | (map2[map1[i * 4 + 1]]);
                                dest_ptr[1] = (map2[map1[i * 4 + 2]] << 24) | (map2[map1[i * 4 + 2]] << 16) | (map2[map1[i * 4 + 3]] << 8) | (map2[map1[i * 4 + 3]]);

                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 12;
                            dest -= var_10;
                        }
                    } else {
                        if (a2[2] > a2[3]) {
                            // 9/3
                            // VERIFIED
                            for (i = 0; i < 4; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4 + 3] = value1 & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 16) & 0xFF;
                                map1[i * 4] = (value1 >> 24) & 0xFF;
                            }

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            value2 = nf_width;

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4 + 0]] << 24) | (map2[map1[i * 4 + 0]] << 16) | (map2[map1[i * 4 + 1]] << 8) | (map2[map1[i * 4 + 1]]);
                                dest_ptr[1] = (map2[map1[i * 4 + 2]] << 24) | (map2[map1[i * 4 + 2]] << 16) | (map2[map1[i * 4 + 3]] << 8) | (map2[map1[i * 4 + 3]]);

                                dest += value2;

                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4 + 0]] << 24) | (map2[map1[i * 4 + 0]] << 16) | (map2[map1[i * 4 + 1]] << 8) | (map2[map1[i * 4 + 1]]);
                                dest_ptr[1] = (map2[map1[i * 4 + 2]] << 24) | (map2[map1[i * 4 + 2]] << 16) | (map2[map1[i * 4 + 3]] << 8) | (map2[map1[i * 4 + 3]]);

                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 8;
                            dest -= var_10;
                        } else {
                            // 9/4
                            // VERIFIED
                            for (i = 0; i < 16; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            value2 = nf_width;

                            for (i = 0; i < 8; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 8 + 0]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                                dest_ptr[1] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);
                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 20;
                            dest -= var_10;
                        }
                    }
                    break;
                case 10:
                    if (a2[0] > a2[1]) {
                        if (a2[12] > a2[13]) {
                            // 10/1
                            // VERIFIED
                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[16 + i]];
                                map1[32 + i * 4] = value1 & 0xFF;
                                map1[32 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[32 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[32 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            value2 = nf_width;

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 8 + 0]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                                dest_ptr[1] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);
                                dest += value2;
                            }

                            map2[0xC1] = a2[0x0C + 2]; // mov al, cl
                            map2[0xC3] = a2[0x0C + 0]; // mov al, bl
                            map2[0xC5] = a2[0x0C + 3]; // mov al, ch
                            map2[0xC7] = a2[0x0C + 1]; // mov al, bh
                            map2[0xE1] = a2[0x0C + 2]; // mov ah, cl
                            map2[0xE3] = a2[0x0C + 0]; // mov ah, bl
                            map2[0xE5] = a2[0x0C + 3]; // mov ah, ch
                            map2[0xE7] = a2[0x0C + 1]; // mov ah, bh

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[32 + i * 8 + 0]] << 16) | (map2[map1[32 + i * 8 + 1]] << 24) | (map2[map1[32 + i * 8 + 2]]) | (map2[map1[32 + i * 8 + 3]] << 8);
                                dest_ptr[1] = (map2[map1[32 + i * 8 + 4]] << 16) | (map2[map1[32 + i * 8 + 5]] << 24) | (map2[map1[32 + i * 8 + 6]]) | (map2[map1[32 + i * 8 + 7]] << 8);
                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 24;
                            dest -= var_10;
                        } else {
                            // 10/2
                            // VERIFIED
                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[16 + i]];
                                map1[32 + i * 4] = value1 & 0xFF;
                                map1[32 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[32 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[32 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            value2 = nf_width;

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 8 + 0]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                                dest += value2;

                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);
                                dest += value2;
                            }

                            dest -= value2 * 8 - 4;

                            map2[0xC1] = a2[0x0C + 2]; // mov al, cl
                            map2[0xC3] = a2[0x0C + 0]; // mov al, bl
                            map2[0xC5] = a2[0x0C + 3]; // mov al, ch
                            map2[0xC7] = a2[0x0C + 1]; // mov al, bh
                            map2[0xE1] = a2[0x0C + 2]; // mov ah, cl
                            map2[0xE3] = a2[0x0C + 0]; // mov ah, bl
                            map2[0xE5] = a2[0x0C + 3]; // mov ah, ch
                            map2[0xE7] = a2[0x0C + 1]; // mov ah, bh

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[32 + i * 8 + 0]] << 16) | (map2[map1[32 + i * 8 + 1]] << 24) | (map2[map1[32 + i * 8 + 2]]) | (map2[map1[32 + i * 8 + 3]] << 8);
                                dest += value2;

                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[32 + i * 8 + 4]] << 16) | (map2[map1[32 + i * 8 + 5]] << 24) | (map2[map1[32 + i * 8 + 6]]) | (map2[map1[32 + i * 8 + 7]] << 8);
                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 24;
                            dest -= 4;
                            dest -= var_10;
                        }
                    } else {
                        // 10/3
                        // VERIFIED
                        for (i = 0; i < 4; i++) {
                            value1 = _$$R0063[a2[4 + i]];
                            map1[i * 4] = value1 & 0xFF;
                            map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 4; i++) {
                            value1 = _$$R0063[a2[12 + i]];
                            map1[16 + i * 4] = value1 & 0xFF;
                            map1[16 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[16 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[16 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 4; i++) {
                            value1 = _$$R0063[a2[20 + i]];
                            map1[32 + i * 4] = value1 & 0xFF;
                            map1[32 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[32 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[32 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 4; i++) {
                            value1 = _$$R0063[a2[28 + i]];
                            map1[48 + i * 4] = value1 & 0xFF;
                            map1[48 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[48 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[48 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        value2 = nf_width;

                        map2[0xC1] = a2[2]; // mov al, cl
                        map2[0xC3] = a2[0]; // mov al, bl
                        map2[0xC5] = a2[3]; // mov al, ch
                        map2[0xC7] = a2[1]; // mov al, bh
                        map2[0xE1] = a2[2]; // mov ah, cl
                        map2[0xE3] = a2[0]; // mov ah, bl
                        map2[0xE5] = a2[3]; // mov ah, ch
                        map2[0xE7] = a2[1]; // mov ah, bh

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 8 + 0]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);
                            dest += value2;
                        }

                        map2[0xC1] = a2[0x08 + 2]; // mov al, cl
                        map2[0xC3] = a2[0x08 + 0]; // mov al, bl
                        map2[0xC5] = a2[0x08 + 3]; // mov al, ch
                        map2[0xC7] = a2[0x08 + 1]; // mov al, bh
                        map2[0xE1] = a2[0x08 + 2]; // mov ah, cl
                        map2[0xE3] = a2[0x08 + 0]; // mov ah, bl
                        map2[0xE5] = a2[0x08 + 3]; // mov ah, ch
                        map2[0xE7] = a2[0x08 + 1]; // mov ah, bh

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[16 + i * 8 + 0]] << 16) | (map2[map1[16 + i * 8 + 1]] << 24) | (map2[map1[16 + i * 8 + 2]]) | (map2[map1[16 + i * 8 + 3]] << 8);
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[16 + i * 8 + 4]] << 16) | (map2[map1[16 + i * 8 + 5]] << 24) | (map2[map1[16 + i * 8 + 6]]) | (map2[map1[16 + i * 8 + 7]] << 8);
                            dest += value2;
                        }

                        dest -= value2 * 8 - 4;

                        map2[0xC1] = a2[0x10 + 2]; // mov al, cl
                        map2[0xC3] = a2[0x10 + 0]; // mov al, bl
                        map2[0xC5] = a2[0x10 + 3]; // mov al, ch
                        map2[0xC7] = a2[0x10 + 1]; // mov al, bh
                        map2[0xE1] = a2[0x10 + 2]; // mov ah, cl
                        map2[0xE3] = a2[0x10 + 0]; // mov ah, bl
                        map2[0xE5] = a2[0x10 + 3]; // mov ah, ch
                        map2[0xE7] = a2[0x10 + 1]; // mov ah, bh

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[32 + i * 8 + 0]] << 16) | (map2[map1[32 + i * 8 + 1]] << 24) | (map2[map1[32 + i * 8 + 2]]) | (map2[map1[32 + i * 8 + 3]] << 8);
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[32 + i * 8 + 4]] << 16) | (map2[map1[32 + i * 8 + 5]] << 24) | (map2[map1[32 + i * 8 + 6]]) | (map2[map1[32 + i * 8 + 7]] << 8);
                            dest += value2;
                        }

                        map2[0xC1] = a2[0x18 + 2]; // mov al, cl
                        map2[0xC3] = a2[0x18 + 0]; // mov al, bl
                        map2[0xC5] = a2[0x18 + 3]; // mov al, ch
                        map2[0xC7] = a2[0x18 + 1]; // mov al, bh
                        map2[0xE1] = a2[0x18 + 2]; // mov ah, cl
                        map2[0xE3] = a2[0x18 + 0]; // mov ah, bl
                        map2[0xE5] = a2[0x18 + 3]; // mov ah, ch
                        map2[0xE7] = a2[0x18 + 1]; // mov ah, bh

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[48 + i * 8 + 0]] << 16) | (map2[map1[48 + i * 8 + 1]] << 24) | (map2[map1[48 + i * 8 + 2]]) | (map2[map1[48 + i * 8 + 3]] << 8);
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[48 + i * 8 + 4]] << 16) | (map2[map1[48 + i * 8 + 5]] << 24) | (map2[map1[48 + i * 8 + 6]]) | (map2[map1[48 + i * 8 + 7]] << 8);
                            dest += value2;
                        }

                        dest -= value2;

                        a2 += 32;
                        dest -= 4;
                        dest -= var_10;
                    }
                    break;
                case 11:
                    value2 = nf_width;

                    src_ptr = (unsigned int*)a2;
                    for (i = 0; i < 8; i++) {
                        dest_ptr = (unsigned int*)dest;
                        dest_ptr[0] = src_ptr[i * 2];
                        dest_ptr[1] = src_ptr[i * 2 + 1];
                        dest += value2;
                    }

                    dest -= value2;

                    a2 += 64;
                    dest -= var_10;
                    break;
                case 12:
                    value2 = nf_width;

                    for (i = 0; i < 4; i++) {
                        byte = a2[i * 4 + 0];
                        value1 = byte | (byte << 8);

                        byte = a2[i * 4 + 1];
                        value1 |= (byte << 16) | (byte << 24);

                        byte = a2[i * 4 + 2];
                        value2 = byte | (byte << 8);

                        byte = a2[i * 4 + 3];
                        value2 |= (byte << 16) | (byte << 24);

                        dest_ptr = (unsigned int*)dest;
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest_ptr = (unsigned int*)(dest + nf_width);
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest += nf_width * 2;
                    }

                    dest -= nf_width;

                    a2 += 16;
                    dest -= var_10;
                    break;
                case 13:
                    byte = a2[0];
                    value1 = byte | (byte << 8) | (byte << 16) | (byte << 24);

                    byte = a2[1];
                    value2 = byte | (byte << 8) | (byte << 16) | (byte << 24);

                    for (i = 0; i < 2; i++) {
                        dest_ptr = (unsigned int*)dest;
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest_ptr = (unsigned int*)(dest + nf_width);
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest += nf_width * 2;
                    }

                    byte = a2[2];
                    value1 = byte | (byte << 8) | (byte << 16) | (byte << 24);

                    byte = a2[3];
                    value2 = byte | (byte << 8) | (byte << 16) | (byte << 24);

                    for (i = 0; i < 2; i++) {
                        dest_ptr = (unsigned int*)dest;
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest_ptr = (unsigned int*)(dest + nf_width);
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest += nf_width * 2;
                    }

                    dest -= nf_width;

                    a2 += 4;
                    dest -= var_10;
                    break;
                case 14:
                case 15:
                    if (v7 == 14) {
                        byte = *a2++;
                        value1 = byte | (byte << 8) | (byte << 16) | (byte << 24);
                        value2 = value1;
                    } else {
                        byte = *(unsigned short*)a2;
                        a2 += 2;
                        value1 = byte | (byte << 16);
                        value2 = value1;
                        value2 = (value2 << 8) | (value2 >> (32 - 8));
                    }

                    for (i = 0; i < 4; i++) {
                        dest_ptr = (unsigned int*)dest;
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value1;
                        dest += nf_width;

                        dest_ptr = (unsigned int*)dest;
                        dest_ptr[0] = value2;
                        dest_ptr[1] = value2;
                        dest += nf_width;
                    }

                    dest -= nf_width;

                    dest -= var_10;
                    break;
                }
            }
        }

        dest += var_8;
    }
}

} // namespace fallout
