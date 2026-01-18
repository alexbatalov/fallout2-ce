# Testing sFall scripting features

## Basic info

This folder contains scripts which can be used for sFall opcodes.

## Building

Get latest `compile.exe` from [modders pack](https://sourceforge.net/projects/sfall/files/Modders%20pack/) or from [repository](https://github.com/sfall-team/sslc) and run it this way:

```sh
compile.exe -q -p -l -O2 -d -s -n -I<sfall_headers_id> <script_name.ssl>
```

(sfall headers can be found in modderspack)


### Build script using sFall Script Editor

- Get "Fallout sFall Script Editor v4.1.7.RC1", for example from https://nuclear-grot.ucoz.net/forum/12-20-1

- Run script editor, add "sfall_headers" into "Location folder of headers files" in the options

- Open any script here and compile it

- This shoud create compiled script <file.int>


### Build script using VSCode extension

- Install [VSCode Extension](https://marketplace.visualstudio.com/items?itemName=BGforge.bgforge-mls)



## Run test script

1. Move compiled `.int` file into game folder as `data/scripts/gl_<script_name>.int`

2. Change `ddraw.ini` and add this section:
```ini
[Scripts]
GlobalScriptPaths=data/scripts/gl*.int
```

(or add new path using comma as separator)

Note that on non-Windows it have to be `/` as folder separator

3. Run game, check that game displays message about tests


