#include "sfall_ext.h"

#include <algorithm>
#include <string>

#include "db.h"
#include "debug.h"
#include "platform_compat.h"

namespace fallout {

/**
 * Load mods from the mod directory
 */
void sfallLoadMods()
{
    // SFALL: additional mods from the mods directory / mods_order.txt
    const char* modsPath = "mods";
    const char* loadOrderFilename = "mods_order.txt";

    char loadOrderFilepath[COMPAT_MAX_PATH];
    compat_makepath(loadOrderFilepath, nullptr, modsPath, loadOrderFilename, nullptr);

    // If the mods folder does not exist, create it.
    compat_mkdir(modsPath);

    // If load order file does not exist, initialize it automatically with mods already in the mods folder.
    if (compat_access(loadOrderFilepath, 0) != 0) {
        debugPrint("Generating Mods Order file based on the contents of Mods folder: %s\n", loadOrderFilepath);

        File* stream = fileOpen(loadOrderFilepath, "wt");
        if (stream != nullptr) {
            char** fileList;
            int fileListLength = fileNameListInit("mods\\*.dat", &fileList, 0, 0);

            for (int index = 0; index < fileListLength; index++) {
                fileWriteString(fileList[index], stream);
                fileWriteString("\n", stream);
            }
            fileClose(stream);
            fileNameListFree(&fileList, 0);
        }
    }

    // Add mods from load order file.
    File* stream = fileOpen(loadOrderFilepath, "r");
    if (stream != nullptr) {
        char mod[COMPAT_MAX_PATH];
        while (fileReadString(mod, COMPAT_MAX_PATH, stream)) {
            std::string modPath { mod };

            if (modPath.find_first_of(";#") != std::string::npos)
                continue; // skip comments

            // ltrim
            modPath.erase(modPath.begin(), std::find_if(modPath.begin(), modPath.end(), [](unsigned char ch) {
                return !isspace(ch);
            }));

            // rtrim
            modPath.erase(std::find_if(modPath.rbegin(), modPath.rend(), [](unsigned char ch) {
                return !isspace(ch);
            }).base(),
                modPath.end());

            if (modPath.empty())
                continue; // skip empty lines

            char normalizedModPath[COMPAT_MAX_PATH];
            compat_makepath(normalizedModPath, nullptr, modsPath, modPath.c_str(), nullptr);

            if (compat_access(normalizedModPath, 0) == 0) {
                debugPrint("Loading mod %s\n", normalizedModPath);
                dbOpen(normalizedModPath, 0, nullptr, 1);
            } else {
                debugPrint("Skipping invalid mod entry %s in %s\n", normalizedModPath, loadOrderFilepath);
            }
        }
        fileClose(stream);
    } else {
        debugPrint("Error opening %s for read\n", loadOrderFilepath);
    }
}

} // namespace fallout
