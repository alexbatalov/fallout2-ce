# Fallout 2 Community Edition

## Installation

You must own the game to play. Purchase your copy on [GOG](https://www.gog.com/game/fallout_2) or [Steam](https://store.steampowered.com/app/38410). Download latest release or build from source.

### Windows

Download and copy `fallout2-ce.exe` to your `Fallout2` folder. It serves as a drop-in replacement for `fallout2.exe`.

### Linux

- Use Windows installation as a base - it contains data assets needed to play. Copy `Fallout2` folder somewhere, for example `/home/john/Desktop/Fallout2`.

- Download and copy `fallout2-ce` to this folder.

- Install [SDL2](https://libsdl.org/download-2.0.php):

```console
$ sudo apt install libsdl2-2.0-0
```

- Run `./fallout2-ce`.

### macOS

> **NOTE**: macOS 11 or higher is required. The app is not universal. It should run on Apple Silicon under Rosetta 2, but I haven't tried it.

- Use Windows installation as a base - it contains data assets needed to play. Copy `Fallout2` folder somewhere, for example `/Applications/Fallout2`.

- Download and copy `fallout2-ce.app` to this folder.

- Run `fallout2-ce.app`.

## Contributing

For now there are three major areas.

### Intergrating Sfall

There are literally hundreds if not thousands of fixes and features in sfall. I guess not all of them are needed in Community Edition, but for the sake of compatibility with big mods out there, let's integrate them all.

### SDL

Migrate DirectX stuff to SDL. This is the shortest path to native Linux version.

### Prepare to 64-bit

Modern macOS requires apps to be 64-bit, so even if we have SDL, the scripting part of the game will not work, because of builtin SSL interpreter. It stores pointers (both functions and variables) as 32-bit integers, so 64-bit pointers will not fit into stack. Since the stack is shared for both instructions and data, it needs some attention.


## Legal & License

See [Fallout 2 Reference Edition](https://github.com/alexbatalov/fallout2-re). Same conditions apply until the source code in this repository is changed significantly.
