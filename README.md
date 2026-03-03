# Fallout 2: Community Edition — Xbox Port & Controller Support

[![CI Build](https://github.com/MrMilenko/fallout2-ce/actions/workflows/ci-build.yml/badge.svg)](https://github.com/MrMilenko/fallout2-ce/actions/workflows/ci-build.yml)
[![Latest Release](https://img.shields.io/github/v/release/MrMilenko/fallout2-ce)](https://github.com/MrMilenko/fallout2-ce/releases/latest)

A fork of [Fallout 2 Community Edition](https://github.com/alexbatalov/fallout2-ce) adding:

- **Original Xbox support** — built with [NXDK](https://github.com/XboxDev/nxdk) and SDL
- **Steam Deck support** — packaged as an AppImage at 1280×800
- **Controller support on all platforms** — SDL_GameController works on PC, Mac, Linux, Steam Deck, and Xbox with the same mapping

The upstream project supports Windows, Linux, macOS, Android, and iOS via standard CMake. This fork builds on that with the Xbox target, Steam Deck AppImage, and exposes controller input on desktop platforms as well.

Some Xbox build infrastructure was adapted from [Justy's fallout1-ce Xbox port](https://github.com/jroc-hb/fallout1-ce-xbox).

---

## Getting the Game Files

You need `master.dat`, `critter.dat`, and the `data/` folder from a legitimate copy of Fallout 2 ([GOG](https://www.gog.com/game/fallout_2) · [Steam](https://store.steampowered.com/app/38410) · [Epic](https://store.epicgames.com/p/fallout-2)).

Use the included script to extract them automatically:

```sh
bash tools/setup-game-data.sh <path/to/installer.exe|installer.sh|install-dir> [output-dir]
```

Supports GOG Windows installers (`.exe`, requires [`innoextract`](https://constexpr.org/innoextract/)), GOG Linux installers (`.sh`), and existing install directories. The script prints per-platform instructions for where to put the files once extracted.

---

## Installation

### Windows

1. Download `fallout2-ce-windows-x64.zip` from the [latest release](https://github.com/MrMilenko/fallout2-ce/releases/latest) and unzip it
2. Run `tools/setup-game-data.sh` (WSL or Git Bash) or manually copy `master.dat`, `critter.dat`, and `data/` into the same folder as `fallout2-ce.exe`
3. Run `fallout2-ce.exe`

### macOS

1. Download `fallout2-ce-macos.dmg` from the [latest release](https://github.com/MrMilenko/fallout2-ce/releases/latest) and open it
2. Drag the app to Applications
3. Run `tools/setup-game-data.sh` and copy the output files into `Fallout II Community Edition.app/Contents/MacOS/`
4. Launch the app

### Linux

1. Download the Linux tarball from the [latest release](https://github.com/MrMilenko/fallout2-ce/releases/latest) and extract it
2. Run `tools/setup-game-data.sh` and copy `master.dat`, `critter.dat`, and `data/` next to the binary
3. Run `./fallout2-ce`

### Steam Deck

1. Download `Fallout2-CE-SteamDeck-x86_64.AppImage` from the [latest release](https://github.com/MrMilenko/fallout2-ce/releases/latest)
2. Switch to Desktop Mode. Run `tools/setup-game-data.sh` pointing at your GOG installer or install folder
3. Copy `master.dat`, `critter.dat`, and `data/` to the same folder as the AppImage
4. `chmod +x Fallout2-CE-SteamDeck-x86_64.AppImage` and run it — or add to Steam as a Non-Steam game
5. The game defaults to 1280×800 for the Deck screen

### Xbox

Game files are split between the Xbox DVD drive (`D:\`) and the hard drive (`E:\`).

#### On the DVD / ISO (`D:\`)

Copy these files into the root of the ISO (the `fallout2-ce/` directory in this repo is packed into the ISO automatically):

| File | Description |
|---|---|
| `default.xbe` | Game executable (built automatically) |
| `master.dat` | Main game data archive |
| `critter.dat` | Critter/NPC data archive |
| `f2_res.ini` | Resolution and display settings |
| `fallout2.cfg` | Main game configuration |
| `ddraw.ini` | Sfall configuration |
| `TitleImage.xbx` | Xbox dashboard thumbnail |

#### On the Hard Drive (`E:\UDATA\FALLOUT2\`)

Copy the `data\` folder from your Fallout 2 installation here. Saves will also be written here.

#### Running from HDD (softmod/chip)

`D:\` is always the directory containing `default.xbe`. Copy `default.xbe` and all game data to any folder on `E:\` (e.g. `E:\Games\Fallout2\`) and `D:\` maps there automatically — no config changes needed.

---

## Building

### Desktop (Windows / Linux / macOS)

Standard CMake build — same as upstream:

```sh
cmake -B build -S .
cmake --build build
```

Place `master.dat`, `critter.dat`, and the `data/` folder from your Fallout 2 installation in the same directory as the binary before running.

### Xbox (NXDK)

Set up the NXDK toolchain, then:

```sh
export NXDK_DIR=/path/to/nxdk
export PATH=$NXDK_DIR/bin:$PATH
nxdk-cmake -B build-nxdk -S . -DNXDK_DIR=$NXDK_DIR
cmake --build build-nxdk
```

This produces `fallout2-ce.iso`, which can be burned to a disc or loaded in [xemu](https://xemu.app).

> **Note:** Several NXDK libraries must be built once before the first build (`libc++.lib`, `libSDL2.lib`). See the NXDK documentation for details.

---

## Configuration (Xbox)

### `f2_res.ini` — Display settings

```ini
[MAIN]
SCR_WIDTH=640
SCR_HEIGHT=480
WINDOWED=0
```

The original Xbox outputs 480p (640×480) by default. Higher resolutions may work depending on your TV and cable, but 640×480 is the safe default.

### `fallout2.cfg` — Game settings

```ini
[system]
master_dat=D:\master.dat
critter_dat=D:\critter.dat
master_patches=E:\UDATA\FALLOUT2\data
critter_patches=E:\UDATA\FALLOUT2\data

[sound]
music_path1=sound\music\
```

---

## Controller Layout

The Xbox controller mapping also applies when using a gamepad on desktop platforms.

| Button | Action |
|---|---|
| **Left stick** | Move mouse cursor |
| **A** | Left mouse click |
| **B** | Right mouse click (cycle cursor mode) |
| **X** | Inventory |
| **Y** | Pip-Boy |
| **Back** | Character sheet |
| **Start** | Options / ESC |
| **D-Pad Up / Down** | Scroll |
| **D-Pad Left** | Skilldex |
| **D-Pad Right** | Quick Save |
| **White (LB)** | End Turn |
| **Black (RB)** | End Combat |
| **L3 (left stick click)** | Center camera on player |
| **R3 (right stick click)** | Activate combat mode |

Mouse cursor sensitivity and deadzone are tunable — see `sensitivity` and `deadzone` in `src/dinput.cc`.

---

## Known Issues

- Some actions that require a keyboard have no controller equivalent yet (e.g. naming characters, free text input)

---

## License

Source code in this repository is available under the [Sustainable Use License](LICENSE.md).
