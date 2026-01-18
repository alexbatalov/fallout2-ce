#include "scan_unimplemented.h"
#include "config.h"
#include "dfile.h"
#include "dictionary.h"
#include "interpreter.h"
#include "platform_compat.h"
#include "scan_unimplemented_configs.h"
#include "scan_unimplemented_opcodes.h"
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

bool gScanUnimplementedEnabled = false;

void scanUnimplementdParseCommandLineArguments(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--scan-unimplemented") == 0) {
            gScanUnimplementedEnabled = true;
            printf("== Scanning of unimplemented is enabled == \n");
        };
    }
}