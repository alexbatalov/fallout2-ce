#include "config.h"
#include "dfile.h"
#include "dictionary.h"
#include "interpreter.h"
#include "platform_compat.h"
#include "scan_unimplemented.h"
#include "scan_unimplemented_sfall.h"
#include "scan_unimplemented_utils.h"
#include "sfall_metarules.h"
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>

auto config_to_maps(fallout::Config& config)
{
    ConfigMap out;
    for (int sectionIndex = 0; sectionIndex < config.entriesLength; sectionIndex++) {
        auto section = &(config.entries[sectionIndex]);
        // std::cout << "Section = " << section->key << std::endl;
        auto sectionDict = (fallout::ConfigSection*)section->value;

        for (int entryIndex = 0; entryIndex < sectionDict->entriesLength; entryIndex++) {
            auto sectionEntry = &(sectionDict->entries[entryIndex]);
            auto val = *(char**)sectionEntry->value;

            std::string sectionKey = section->key;
            std::string entryKey = sectionEntry->key;
            out[sectionKey][entryKey] = std::string(val);
            // std::cout << "  Key = " << sectionEntry->key << "; value " << val << std::endl;
        }
    }

    /*
    for (auto [k1, sec]: out) {
        for (auto [k2, v]: sec) {
            std::cout << "  " << k1 << "." << k2 << " = " << v << std::endl;
        }
    }
        */
    return out;
}

ConfigChecker::ConfigChecker(fallout::Config& configDefaults, std::string configFileName)
    : configFileName(configFileName)
    , defaultsMap(config_to_maps(configDefaults))
{
}
ConfigChecker::ConfigChecker(ConfigMap configDefaults, std::string configFileName)
    : configFileName(configFileName)
    , defaultsMap(configDefaults)
{
}

void ConfigChecker::check(fallout::Config& readedConfig)
{
    if (!gScanUnimplementedEnabled) {
        return;
    }

    std::cout << "Checking config file: " << configFileName << std::endl;
    int errorsCount = 0;
    auto readedMap = config_to_maps(readedConfig);

    for (const auto& [readedSection, readedEntries] : readedMap) {
        if (defaultsMap.find((readedSection)) == defaultsMap.end()) {
            std::cout << "- ERROR: Section '" << readedSection << "' is not defined in defaults" << std::endl;
            for (const auto& [key, value] : readedEntries) {
                std::cout << "    - " << key << "=" << value << std::endl;
            };
            errorsCount++;
            continue;
        }
        for (const auto& [readedKey, readedValue] : readedEntries) {
            if (defaultsMap[(readedSection)].find((readedKey)) == defaultsMap[(readedSection)].end()) {
                std::cout << "- Unknown key in [" << readedSection << "]: " << readedKey << "=" << readedValue << std::endl;
                errorsCount++;
            } else if (defaultsMap[readedSection][readedKey] != readedValue) {
                // copilot suggested this, but I think it is not needed
            }
        }
    }
    if (errorsCount == 0) {
        std::cout << "- Everything is ok, no errors found" << std::endl;
    } else {
        std::cout << "- Found " << errorsCount << " errors in config file" << std::endl;
    }
    std::cout << std::endl;
}
