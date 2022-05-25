#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>

typedef struct CommandLineArguments {
    int argc;
    char** argv;
} CommandLineArguments;

void argsInit(CommandLineArguments* commandLineArguments);
bool argsParse(CommandLineArguments* commandLineArguments, int argc, char* argv[]);
void argsFree(CommandLineArguments* commandLineArguments);

#endif /* ARGS_H */
