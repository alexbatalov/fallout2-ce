# Webassembly

## Installation
Install emscripten for example version `3.1.32`.


## Build

Follow each step:

### Build static web files

```bash
mkdir build
cd build
emcmake cmake .. -DCMAKE_BUILD_TYPE="Release"
emmake make VERBOSE=1
cd web
```

### Copy game data

Create a `game` subfolder and copy each game folder there (for example `build/web/game/FalloutNevada/`)

### Update games configuration

Update `build/web/config.js` and add your games there

### (optional) Unpack game data

Due to async loading it is recommended to unpack game data. https://github.com/falltergeist/dat-unpacker.git can be used for this:
```sh
test -f master.dat && mkdir master.dat.dir && dat-unpacker -s master.dat -d master.dat.dir && rm master.dat && mv master.dat.dir master.dat
test -f critter.dat && mkdir critter.dat.dir && dat-unpacker -s critter.dat -d critter.dat.dir && rm critter.dat && mv critter.dat.dir critter.dat

# DO NOT decompress patches due to bug
# test -f patch001.dat && mkdir patch001.dat.dir && dat-unpacker -s patch001.dat -d patch001.dat.dir && rm patch001.dat && mv patch001.dat.dir patch001.dat
```


### Update game configuration

Skip intro videos by adding those lines in `ddraw.ini`:
```ini
[Misc]
SkipOpeningMovies=1
```

Force game resolution in `f2_res.ini`:
```ini
[MAIN]
SCR_WIDTH=640
SCR_HEIGHT=480
WINDOWED=1
```


### Change line endings in text files (if .dat files were unpacked)

We aware of some issues with text files with CLRF line endings:
```bash
# This might cause game to have incompatible save game format due to wrong global vars count
dos2unix master.dat/data/VAULT13.GAM

# This fixes no sound effects issue
dos2unix master.dat/sound/SFX/SNDLIST.LST

# This fixes issue with death scene subtitles
dos2unix master.dat/data/enddeath.txt
 
```

This bash code replaces CLRF endings in all text files:
```bash
find . -type f -exec bash -c "(file -i -b {} | grep '^text/plain;') && dos2unix {}" \;
```

TODO: Fix this bash snippet, it does not work if file name have "(" character

### Add empty files to keep empty folders
This is a simple workaround for `asyncfetchfs` because it ignores empty folders. Just do this:

```bash
test -d data/SAVEGAME || mkdir data/SAVEGAME
test -d data/MAPS || mkdir data/MAPS
touch data/SAVEGAME/.empty
touch data/MAPS/.empty
touch data/proto/items/.empty
touch data/proto/critters/.empty
```

### Create files index
This list is used by `asyncfetchfs`

```bash
cd build/web/game/FalloutOfNevada
find . -type f -printf '%s ' -exec sha256sum "{}" \; > index.txt
```

Do not forget to re-generate list if game files are changed, for example patch is applied or configuration is updated.


### (Optional) Compress game data

In order to reduce game size we compress each file separately using ordinary `gz`:

```bash
cd build/web/game/FalloutOfNevada
gzip -v -r --best .
```

If you do not want to compress game files then change `useGzip` option in `config.js`



### Done!

Check that everything works by starting web server and opening webpage:
```
npx http-server .
```

## Notes

I had an issue with some hosting due to incorrect permissions on game data. Solved via:
```
find game -type d -exec chmod 755 {} \;
find game -type f -exec chmod 644 {} \;
```


## Fallout: Nevada notes

### Add ddraw.ini config
```ini
[Misc]
MaleStartModel=hmjmps
FemaleStartModel=hfjmps

StartYear=2140
StartMonth=9
StartDay=10

; This is really huge timer so those events will never appear
MovieTimer_artimer1=36000 
MovieTimer_artimer2=36030
MovieTimer_artimer3=36060
MovieTimer_artimer4=36090

```

### Fix music path

Some distributions have music path equal to `sound\music\` in `fallout2.cfg`. Change in into `data\sound\music`:

```ini
[sound]
music_path1=data\sound\music\
music_path2=data\sound\music\

```

### Add missing script file for Nevada 1.00-1.02

Fallout Nevada have issue in Zone 51 - there is no file `NCStrZxB.int`. Original game complains about missing file but works, this implementation crashes (as for me this is better behavior rather that silently ignore error). To fix this just add this file:

```sh
echo '8002c00100000012800dc001000000dc80048010801a8020801a8021801a
8022801a8023802480258026000000040000000600000000000000000000
0000000000ee0000000000000018000000000000000000000000000000ee
000000000000002000000000000000000000000000000108000000000000
003600000000000000000000000000000124000000000000004200102e2e
2e2e2e2e2e2e2e2e2e2e2e2e000000067374617274000014646573637269
7074696f6e5f705f70726f63000000106c6f6f6b5f61745f705f70726f63
0000ffffffffffffffff802cc001000000008003c001000000ee8004802b
c00100000000800d8019802a8029800c801c802a8029801c802b80b9c001
00000000800d8019802a8029800c801c802a8029801c802b80b9c0010000
0000800d8019802a8029800c801c802a8029801c' | xxd -r -p > master.dat/scripts/NCStrZxB.int
```