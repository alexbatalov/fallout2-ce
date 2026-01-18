# Fallout 2 Community Edition

Fallout 2 Community Edition is a fully working re-implementation of Fallout 2, with the same original gameplay, engine bugfixes, and some quality of life improvements, that works (mostly) hassle-free on multiple platforms.  This is a fork of the original project, which isn't getting regular updates.

Popular Fallout 2 total conversion mods are partially supported. Original versions of Nevada and Sonora (that do not rely on extended features provided by Sfall) work. [Fallout 2 Restoration Project](https://github.com/BGforgeNet/Fallout2_Restoration_Project), [Fallout Et Tu](https://github.com/rotators/Fo1in2) and [Olympus 2207](https://olympus2207.com) are not yet supported. Other mods (particularly Resurrection and Yesterday) are not tested.

There is also [Fallout Community Edition](https://github.com/alexbatalov/fallout1-ce) (not affiliated with this fork).

## Installation

You must own the game to play. Purchase your copy on [GOG](https://www.gog.com/game/fallout_2), [Epic Games](https://store.epicgames.com/p/fallout-2) or [Steam](https://store.steampowered.com/app/38410). Download latest [release](https://github.com/fallout2-ce/fallout2-ce/releases) or build from source. 

### Windows

Download and copy `fallout2-ce.exe` to your `Fallout2` folder. It serves as a drop-in replacement for `fallout2.exe`.

### Linux

- Use Windows installation as a base - it contains data assets needed to play. Copy `Fallout2` folder somewhere, for example `/home/john/Desktop/Fallout2`.

- Alternatively you can extract the needed files from the GoG installer:

```console
$ sudo apt install innoextract
$ innoextract ~/Downloads/setup_fallout2_2.1.0.18.exe -I app
$ mv app Fallout2
```

- Download and copy `fallout2-ce` to this folder.

- Install [SDL2](https://libsdl.org/download-2.0.php):

```console
$ sudo apt install libsdl2-2.0-0
```

- Run `./fallout2-ce`.

### macOS

> **NOTE**: macOS 10.11 (El Capitan) or higher is required. Runs natively on Intel-based Macs and Apple Silicon.

- Use Windows installation as a base - it contains data assets needed to play. Copy `Fallout2` folder somewhere, for example `/Applications/Fallout2`.

- Alternatively you can use Fallout 2 from Macplay/The Omni Group as a base - you need to extract game assets from the original bundle. Mount CD/DMG, right click `Fallout 2` -> `Show Package Contents`, navigate to `Contents/Resources`. Copy `GameData` folder somewhere, for example `/Applications/Fallout2`.

- Or if you're a Terminal user and have Homebrew installed you can extract the needed files from the GoG installer.  **Note**: You must use "Offline backup game installer", not the main game installer.

```console
$ brew install innoextract
$ innoextract ~/Downloads/setup_fallout2_2.1.0.18.exe -I app
$ mv app /Applications/Fallout2
```

- Download and copy `fallout2-ce.app` to this folder.

- Run `fallout2-ce.app`.

### Android

> **NOTE**: Fallout 2 was designed with mouse in mind. There are many controls that require precise cursor positioning, which is not possible with fingers. Current control scheme resembles trackpad usage:
- One finger moves mouse cursor around.
- Tap one finger for left mouse click.
- Tap two fingers for right mouse click (switches mouse cursor mode).
- Move two fingers to scroll current view (map view, worldmap view, inventory scrollers).

> **NOTE**: From Android standpoint release and debug builds are different apps. Both apps require their own copy of game assets and have their own savegames. This is intentional. As a gamer just stick with release version and check for updates.

- Use Windows installation as a base - it contains data assets needed to play. Copy `Fallout2` folder to your device, for example to `Downloads`. You need `master.dat`, `critter.dat`, `patch000.dat`, and `data` folder. Watch for file names - keep (or make) them lowercased (see [Configuration](#configuration)).

- Download `fallout2-ce.apk` and copy it to your device. Open it with file explorer, follow instructions (install from unknown source).

- When you run the game for the first time it will immediately present file picker. Select the folder from the first step. Wait until this data is copied. A loading dialog will appear, just wait for about 30 seconds. If you're installing total conversion mod or localized version with a large number of unpacked resources in `data` folder it can take up to 20 minutes. Once copied, the game will start automatically.

### iOS

> **NOTE**: See Android note on controls.

- Download `fallout2-ce.ipa`. Use sideloading applications ([AltStore](https://altstore.io/) or [Sideloadly](https://sideloadly.io/)) to install it to your device. Alternatively you can always build from source with your own signing certificate.

- Run the game once. You'll see error message saying "Couldn't find/load text fonts". This step is needed for iOS to expose the game via File Sharing feature.

- Use Finder (macOS Catalina and later) or iTunes (Windows and macOS Mojave or earlier) to copy `master.dat`, `critter.dat`, `patch000.dat`, and `data` folder to "Fallout 2" app ([how-to](https://support.apple.com/HT210598)). Watch for file names - keep (or make) them lowercased (see [Configuration](#configuration)).

### Browser

> **NOTE**: WebAssebly build with emscripten
```
docker run --rm -v $(pwd):/src emscripten/emsdk:3.1.74 sh -c 'git config --global --add safe.directory "*" && mkdir -p build && cd build && emcmake cmake ../ -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain/Emscripten.cmake && emmake make'
```
- Demo available at https://github.com/ololoken/fallout2-ce-ems.git

## Configuration

The main configuration file is `fallout2.cfg`. There are several important settings you might need to adjust for your installation. Depending on your Fallout distribution main game assets `master.dat`, `critter.dat`, `patch000.dat`, and `data` folder might be either all lowercased, or all uppercased. You can either update `master_dat`, `critter_dat`, `master_patches` and `critter_patches` settings to match your file names, or rename files to match entries in your `fallout2.cfg`.

The `sound` folder (with `music` folder inside) might be located either in `data` folder, or be in the Fallout folder. Update `music_path1` setting to match your hierarchy, usually it's `data/sound/music/` or `sound/music/`. Make sure it matches your path exactly (so it might be `SOUND/MUSIC/` if you've installed Fallout from CD). Music files themselves (with `ACM` extension) should be all uppercased, regardless of `sound` and `music` folders.

The second configuration file is `f2_res.ini`. Use it to change game window size, enable/disable fullscreen mode and configure IFACE settings (the control bar at the bottom of the game screen).

```ini
[MAIN]
SCR_WIDTH=1280
SCR_HEIGHT=720
WINDOWED=1 ; 0 = fullscreen
SCALE_2X=1 ; 0 = original scale, 1 = 2x - requires increasing the minimum resolution from 640x480 to 1280x960.

[IFACE]
; if IFACE_BAR_MODE=0 - the bottom of the map view window sits at the top of the IFACE Bar.
; if IFACE_BAR_MODE=1 - the bottom of the map view window extends to the base of the screen and is overlapped by the IFACE Bar.
IFACE_BAR_MODE=0

;if IFACE_BAR_SIDE_ART=0 - Black, No Iface-bar side art used.
;if IFACE_BAR_SIDE_ART=1 - Metal (grey) look Iface-bar side art used.
;if IFACE_BAR_SIDE_ART=2 - Leather look Iface-bar side art used.
;if IFACE_BAR_SIDE_ART=3-6 - Alternative Metal look Iface-bar side art used.
;if IFACE_BAR_SIDE_ART=7 - Alternative Leather look Iface-bar side art used.
;if IFACE_BAR_SIDE_ART=8 - Metal (brown) look Iface-bar side art used.
IFACE_BAR_SIDE_ART=2

;if IFACE_BAR_SIDES_ORI=0 - Iface-bar side graphics extend from the Iface-Bar to the Screen edges.
;if IFACE_BAR_SIDES_ORI=1 - Iface-bar side graphics extend from the Screen edges to the Iface-Bar.
IFACE_BAR_SIDES_ORI=0

;This will increase the width of the interface bar expanding the area used to display text.
;if IFACE_BAR_WIDTH=640 - Interface bar will remain at it's original width.
;if IFACE_BAR_WIDTH=800 - Interface bar will use 800pix wide asset from f2_res.dat.
;IFACE_BAR_WIDTH=640
```

Recommendations:
- **Desktops**: Use any size you see fit.
- **Tablets**: Set these values to logical resolution of your device, for example iPad Pro 11 is 1668x2388 (pixels), but it's logical resolution is 834x1194 (points).
- **Mobile phones**: Set height to 480, calculate width according to your device screen (aspect) ratio, for example Samsung S21 is 20:9 device, so the width should be 480 * 20 / 9 = 1067.

In time this stuff will receive in-game interface, right now you have to do it manually. For a sample f2_res.ini configuration file, containing all currently working settings use this link: [f2_res.ini](https://raw.githubusercontent.com/fallout2-ce/fallout2-ce/refs/heads/mainmenu/files/f2_res.ini)

*Note*: use of the IFACE_BAR settings requires the f2_res.dat file, which contains graphical assets. Various versions are available, but one compatible with the above f2_res.ini be found here: [f2_res.dat](https://github.com/fallout2-ce/fallout2-ce/raw/refs/heads/mainmenu/files/f2_res.dat)

The third configuration file is `ddraw.ini` (part of Sfall). There are dozens of options that adjust or override engine behaviour and gameplay mechanics. This file is intended for modders and advanced users.

For a sample ddraw.ini configuration file, containing all currently working settings use this link: [ddraw.ini](https://raw.githubusercontent.com/fallout2-ce/fallout2-ce/refs/heads/main/files/ddraw.ini)

## Quality of life benefits over vanilla Fallout

* High resolution support
* Increased pathfinding nodes 5x for more accurate pathfinding
* Ctrl-click to quickly move items when bartering, looting, or stealing
* _a_ to select "all" when selecting item quantity
* _a_ to `Take All` when looting
* When bartering, caps default to the right amount to balance the trade (if possible)
* Music continues playing between maps (requires config)
* Auto open doors (requires config)

## Contributing

Integrating Sfall goodies is the top priority. Quality of life updates are OK too. Please no large scale refactorings at this time as we need to reconcile changes from Reference Edition, which will make this process slow and error-prone. In any case open up an issue with your suggestion or to notify other people that something is being worked on.

### Integrating Sfall

There are literally hundreds if not thousands of fixes and features in sfall. I guess not all of them are needed in Community Edition, but for the sake of compatibility with big mods out there, let's integrate them all.

## License

The source code is this repository is available under the [Sustainable Use License](LICENSE.md).
