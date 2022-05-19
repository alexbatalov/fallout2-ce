#include "args.h"

#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// 0x4E3B90
void argsInit(CommandLineArguments* commandLineArguments)
{
    if (commandLineArguments != NULL) {
        commandLineArguments->argc = 0;
        commandLineArguments->argv = NULL;
    }
}

// 0x4E3BA4
bool argsParse(CommandLineArguments* commandLineArguments, char* commandLine)
{
    const char* delim = " \t";

    int argc = 0;

    // Get the number of arguments in command line.
    if (*commandLine != '\0') {
        char* copy = strdup(commandLine);
        if (copy == NULL) {
            argsFree(commandLineArguments);
            return false;
        }

        char* tok = strtok(copy, delim);
        while (tok != NULL) {
            argc++;
            tok = strtok(NULL, delim);
        }

        free(copy);
    }

    // Make a room for argv[0] - program name.
    argc++;

    commandLineArguments->argc = argc;
    commandLineArguments->argv = malloc(sizeof(*commandLineArguments->argv) * argc);
    if (commandLineArguments->argv == NULL) {
        argsFree(commandLineArguments);
        return false;
    }

    for (int arg = 0; arg < argc; arg++) {
        commandLineArguments->argv[arg] = NULL;
    }

    // Copy program name into argv[0].
    char moduleFileName[MAX_PATH];
    int moduleFileNameLength = GetModuleFileNameA(NULL, moduleFileName, MAX_PATH);
    if (moduleFileNameLength == 0) {
        argsFree(commandLineArguments);
        return false;
    }

    if (moduleFileNameLength >= MAX_PATH) {
        moduleFileNameLength = MAX_PATH - 1;
    }

    moduleFileName[moduleFileNameLength] = '\0';

    commandLineArguments->argv[0] = strdup(moduleFileName);
    if (commandLineArguments->argv[0] == NULL) {
        argsFree(commandLineArguments);
        return false;
    }

    // Copy arguments from command line into argv.
    if (*commandLine != '\0') {
        char* copy = strdup(commandLine);
        if (copy == NULL) {
            argsFree(commandLineArguments);
            return false;
        }

        int arg = 1;

        char* tok = strtok(copy, delim);
        while (tok != NULL) {
            commandLineArguments->argv[arg] = strdup(tok);
            tok = strtok(NULL, delim);
            arg++;
        }

        free(copy);
    }

    return true;
}

// 0x4E3D3C
void argsFree(CommandLineArguments* commandLineArguments)
{
    if (commandLineArguments->argv != NULL) {
        // NOTE: Compiled code is slightly different - it decrements argc.
        for (int index = 0; index < commandLineArguments->argc; index++) {
            if (commandLineArguments->argv[index] != NULL) {
                free(commandLineArguments->argv[index]);
            }
        }
        free(commandLineArguments->argv);
    }

    commandLineArguments->argc = 0;
    commandLineArguments->argv = NULL;
}
