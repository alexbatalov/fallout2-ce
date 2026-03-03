#include "win32.h"

#include <stdlib.h>

#include <SDL.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "main.h"
#include "platform_compat.h"
#include "svga.h"
#include "window_manager.h"

#if __APPLE__ && TARGET_OS_IOS
#include "platform/ios/paths.h"
#endif

#ifdef NXDK
#include <assert.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <nxdk/mount.h>
#include <nxdk/path.h>
#include <string.h>
#include <windows.h>
#include "xbox_debug.h"
typedef struct {
    int width;
    int height;
    int scale;
    bool fullscreen;
} VideoOptions;

namespace fallout {
extern SDL_Window* gSdlWindow;
extern SDL_Surface* gSdlSurface;
} // namespace fallout

#endif

namespace fallout {
#ifdef NXDK
bool createRenderer(int width, int height);
void destroyRenderer();
#endif
// 0x51E444
bool gProgramIsActive = false;

#ifdef _WIN32
HANDLE GNW95_mutex = nullptr;
#endif

static const char* GNW95_title = "Fallout2-Xbox"; // or pull from config

#ifdef NXDK
bool SDL_Xbox_Init(VideoOptions* video_options)
{
#ifdef NXDK
    Sleep(1000);

    if (!XVideoSetMode(video_options->width, video_options->height, 32, REFRESH_DEFAULT)) {
        DbgPrint("[SDL_Xbox_Init] XVideoSetMode failed, falling back to 640x480\n");
        if (XVideoSetMode(640, 480, 32, REFRESH_DEFAULT)) {
            video_options->width = 640;
            video_options->height = 480;
        } else {
            DbgPrint("[SDL_Xbox_Init] FATAL: fallback XVideoSetMode failed\n");
            return false;
        }
    }
#endif

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

    DbgPrint("[SDL_Xbox_Init] Initializing SDL video subsystem\n");
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        DbgPrint("[SDL_Xbox_Init] SDL_InitSubSystem failed: %s\n", SDL_GetError());
        return false;
    }

#ifdef NXDK
    Uint32 windowFlags = SDL_WINDOW_FULLSCREEN;
#else
    Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
    if (video_options->fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    }
#endif

    gSdlWindow = SDL_CreateWindow(
        GNW95_title,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        video_options->width * video_options->scale,
        video_options->height * video_options->scale,
        windowFlags);

    if (!gSdlWindow) {
        DbgPrint("[SDL_Xbox_Init] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // Create renderer, texture, and texture surface
    if (!createRenderer(video_options->width, video_options->height)) {
        DbgPrint("[SDL_Xbox_Init] createRenderer failed\n");
        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = nullptr;
        return false;
    }

    gSdlSurface = SDL_CreateRGBSurface(
        0,
        video_options->width,
        video_options->height,
        8, // 8-bit palettized
        0, 0, 0, 0);

    if (!gSdlSurface) {
        DbgPrint("[SDL_Xbox_Init] SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
        destroyRenderer(); // clean up texture/renderer/surface
        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = nullptr;
        return false;
    }

    SDL_Color colors[256];
    for (int i = 0; i < 256; ++i) {
        colors[i].r = i;
        colors[i].g = i;
        colors[i].b = i;
        colors[i].a = 255;
    }

    SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);

    DbgPrint("[SDL_Xbox_Init] SDL_Xbox_Init completed successfully\n");
    return true;
}
#endif // NXDK

int main(int argc, char* argv[])
{
    int rc = -1;

    DbgPrint("\n[main] Entered main()\n");

#if _WIN32
    GNW95_mutex = CreateMutexA(0, TRUE, "GNW95MUTEX");
    if (GetLastError() != ERROR_SUCCESS) {
        fprintf(stderr, "[main] CreateMutexA failed\n");
        return 0;
    }
    fprintf(stderr, "[main] Mutex created successfully\n");
#endif

#if __APPLE__ && TARGET_OS_IOS
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(iOSGetDocumentsPath());
    fprintf(stderr, "[main] iOS path set\n");
#endif

#if __APPLE__ && TARGET_OS_OSX
    {
        char* basePath = SDL_GetBasePath();
        if (basePath) {
            chdir(basePath);
            SDL_free(basePath);
            fprintf(stderr, "[main] OSX base path set\n");
        } else {
            fprintf(stderr, "[main] SDL_GetBasePath failed on OSX\n");
        }
    }
#endif

#if __ANDROID__
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(SDL_AndroidGetExternalStoragePath());
    fprintf(stderr, "[main] Android storage path set\n");
#endif

#ifdef NXDK
    DbgPrint("[main] Using SDL_Xbox_Init instead of SDL_Init\n");

    VideoOptions xboxVideoOptions = {
        .width = 640,
        .height = 480,
        .scale = 1,
        .fullscreen = true
    };

    if (!SDL_Xbox_Init(&xboxVideoOptions)) {
        DbgPrint("[main] SDL_Xbox_Init failed\n");
        return EXIT_FAILURE;
    }
#else
    DbgPrint("[main] Initializing SDL (AUDIO | VIDEO | EVENTS)\n");

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        const char* sdlError = SDL_GetError();
        DbgPrint("[main] SDL_Init failed: %s\n", sdlError);
        fprintf(stderr, "[main] SDL_Init failed: %s\n", sdlError);
        return EXIT_FAILURE;
    }
#endif

    atexit(SDL_Quit);
    DbgPrint("[main] SDL initialized successfully\n");

    SDL_ShowCursor(SDL_DISABLE);
    DbgPrint("[main] Cursor disabled\n");

    gProgramIsActive = true;
    DbgPrint("[main] gProgramIsActive = true\n");

    DbgPrint("[main] Calling falloutMain(argc, argv)\n");
    rc = falloutMain(argc, argv);
    DbgPrint("[main] falloutMain() returned %d\n", rc);

#if _WIN32
    CloseHandle(GNW95_mutex);
    fprintf(stderr, "[main] Mutex released\n");
#endif

    DbgPrint("[main] Exiting main() with code %d\n", rc);
    return rc;
}

} // namespace fallout

int main(int argc, char* argv[])
{
#ifdef NXDK
    DbgPrint("\n\n\n##################### Starting Fallout II #####################\n");
    BOOL success;

    // NXDK: CMake doesn't automount the D: drive, so we need to do it manually
    if (!nxIsDriveMounted('D')) {
        DbgPrint("Mounting D because it is not mounted\n");
        // D: doesn't exist yet, so we create it
        CHAR targetPath[MAX_PATH];
        nxGetCurrentXbeNtPath(targetPath);

        // Cut off the XBE file name by inserting a null-terminator
        char* filenameStr;
        filenameStr = strrchr(targetPath, '\\');
        assert(filenameStr != NULL);
        *(filenameStr + 1) = '\0';

        // Mount the obtained path as D:
        success = nxMountDrive('D', targetPath);
        DbgPrint("Mounted D: %s\n", success ? "success" : "failed");
        assert(success);
    }

    // NXDK: Mount the E: drive for writable data
    success = nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
    DbgPrint("Mounted E: %s\n", success ? "success" : "failed");
    assert(success);

    BOOL dirSuccess;

    DbgPrint("Creating directory E:\\UDATA\n");
    dirSuccess = CreateDirectoryA("E:\\UDATA", NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
    DbgPrint("CreateDirectoryA E:\\UDATA: %s\n", dirSuccess ? "success" : "failed");
    assert(dirSuccess);

    DbgPrint("Creating directory E:\\UDATA\\Fallout2\n");
    dirSuccess = CreateDirectoryA("E:\\UDATA\\Fallout2", NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
    DbgPrint("CreateDirectoryA E:\\UDATA\\Fallout2: %s\n", dirSuccess ? "success" : "failed");
    assert(dirSuccess);

    DbgPrint("Creating directory E:\\UDATA\\Fallout2\\data\n");
    dirSuccess = CreateDirectoryA("E:\\UDATA\\Fallout2\\data", NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
    DbgPrint("CreateDirectoryA E:\\UDATA\\Fallout2\\data: %s\n", dirSuccess ? "success" : "failed");
    assert(dirSuccess);

    DbgPrint("Creating directory E:\\UDATA\\Fallout2\\sound\n");
    dirSuccess = CreateDirectoryA("E:\\UDATA\\Fallout2\\sound", NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
    DbgPrint("CreateDirectoryA E:\\UDATA\\Fallout2\\sound: %s\n", dirSuccess ? "success" : "failed");
    assert(dirSuccess);

    // Install fallout2.cfg f2_res.ini and ddraw.ini to HDD
    if (GetFileAttributesA("E:\\UDATA\\Fallout2\\fallout2.cfg") == INVALID_FILE_ATTRIBUTES) {
        DbgPrint("Copying fallout2.cfg to E:\\UDATA\\Fallout2\\fallout2.cfg\n");
        BOOL copySuccess = CopyFileA("D:\\fallout2.cfg", "E:\\UDATA\\Fallout2\\fallout2.cfg", FALSE);
        DbgPrint("CopyFileA fallout2.cfg: %s\n", copySuccess ? "success" : "failed");
        assert(copySuccess);
    }
    if (GetFileAttributesA("E:\\UDATA\\Fallout2\\f2_res.ini") == INVALID_FILE_ATTRIBUTES) {
        DbgPrint("Copying f2_res.ini to E:\\UDATA\\Fallout2\\f2_res.ini\n");
        BOOL copySuccess = CopyFileA("D:\\f2_res.ini", "E:\\UDATA\\Fallout2\\f2_res.ini", FALSE);
        DbgPrint("CopyFileA f2_res.ini: %s\n", copySuccess ? "success" : "failed");
        assert(copySuccess);
    }
    if (GetFileAttributesA("E:\\UDATA\\Fallout2\\ddraw.ini") == INVALID_FILE_ATTRIBUTES) {
        DbgPrint("Copying ddraw.ini to E:\\UDATA\\Fallout2\\ddraw.ini\n");
        BOOL copySuccess = CopyFileA("D:\\ddraw.ini", "E:\\UDATA\\Fallout2\\ddraw.ini", FALSE);
        DbgPrint("CopyFileA ddraw.ini: %s\n", copySuccess ? "success" : "failed");
        assert(copySuccess);
    }
    if (GetFileAttributesA("E:\\UDATA\\Fallout2\\TitleImage.xbx") == INVALID_FILE_ATTRIBUTES) {
        DbgPrint("Copying TitleImage.xbx to E:\\UDATA\\Fallout2\\TitleImage.xbx\n");
        BOOL copySuccess = CopyFileA("D:\\TitleImage.xbx", "E:\\UDATA\\Fallout2\\TitleImage.xbx", FALSE);
        DbgPrint("CopyFileA TitleImage.xbx: %s\n", copySuccess ? "success" : "failed");
        assert(copySuccess);
    }

    // Install DATA/TEXT files to HDD (Should work for all languages)
    auto CopyDirectoryRecursive = [](const char* srcDir, const char* dstDir, auto& self) -> void {
        WIN32_FIND_DATAA findData;
        char searchPath[MAX_PATH];
        snprintf(searchPath, MAX_PATH, "%s\\*", srcDir);

        HANDLE hFind = FindFirstFileA(searchPath, &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            DbgPrint("FindFirstFileA failed for %s\n", srcDir);
            return;
        }

        do {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                continue;
            }

            char srcPath[MAX_PATH];
            char dstPath[MAX_PATH];
            snprintf(srcPath, MAX_PATH, "%s\\%s", srcDir, findData.cFileName);
            snprintf(dstPath, MAX_PATH, "%s\\%s", dstDir, findData.cFileName);

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                DbgPrint("Creating directory: %s\n", dstPath);
                BOOL dirSuccess = CreateDirectoryA(dstPath, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
                DbgPrint("CreateDirectoryA %s: %s\n", dstPath, dirSuccess ? "success" : "failed");
                if (dirSuccess) {
                    self(srcPath, dstPath, self);
                }
            } else {
                DbgPrint("Copying file: %s -> %s\n", srcPath, dstPath);
                BOOL copySuccess = CopyFileA(srcPath, dstPath, FALSE);
                DbgPrint("CopyFileA %s: %s\n", dstPath, copySuccess ? "success" : "failed");
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    };
    // On Xbox, the dashboard will see both of these folders, so we make em pretty!

    FILE* titleMetaFile = fopen("E:\\UDATA\\FALLOUT2\\TitleMeta.xbx", "rb");
    if (!titleMetaFile) {
        DbgPrint("[lsgPerformSaveGame] Creating TitleMeta.xbx\n");
        titleMetaFile = fopen("E:\\UDATA\\FALLOUT2\\TitleMeta.xbx", "wb");
        if (titleMetaFile) {
            fprintf(titleMetaFile, "TitleName=Fallout 2\r\n");
            fclose(titleMetaFile);
        } else {
            DbgPrint("[lsgPerformSaveGame] Failed to create TitleMeta.xbx\n");
        }
    } else {
        DbgPrint("[lsgPerformSaveGame] TitleMeta.xbx already exists\n");
        fclose(titleMetaFile);
    }
    FILE* savedataMetaFile = fopen("E:\\UDATA\\FALLOUT2\\data\\SaveMeta.xbx", "rb");
    if (!savedataMetaFile) {
        DbgPrint("[lsgPerformSaveGame] Creating SaveMeta.xbx\n");
        savedataMetaFile = fopen("E:\\UDATA\\FALLOUT2\\data\\SaveMeta.xbx", "wb");
        if (savedataMetaFile) {
            fprintf(savedataMetaFile, "Name=DATA\r\n");
            fclose(savedataMetaFile);
        } else {
            DbgPrint("[lsgPerformSaveGame] Failed to create SaveMeta.xbx\n");
        }
    } else {
        DbgPrint("[lsgPerformSaveGame] SaveMeta.xbx already exists\n");
        fclose(savedataMetaFile);
    }
    FILE* saveaudioMetaFile = fopen("E:\\UDATA\\FALLOUT2\\sound\\SaveMeta.xbx", "rb");
    if (!saveaudioMetaFile) {
        DbgPrint("[lsgPerformSaveGame] Creating SaveMeta.xbx\n");
        saveaudioMetaFile = fopen("E:\\UDATA\\FALLOUT2\\sound\\SaveMeta.xbx", "wb");
        if (saveaudioMetaFile) {
            fprintf(saveaudioMetaFile, "Name=AUDIO\r\n");
            fclose(saveaudioMetaFile);
        } else {
            DbgPrint("[lsgPerformSaveGame] Failed to create SaveMeta.xbx\n");
        }
    } else {
        DbgPrint("[lsgPerformSaveGame] SaveMeta.xbx already exists\n");
        fclose(savedataMetaFile);
    }
#endif

    return fallout::main(argc, argv);
}
