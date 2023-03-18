# Webassembly

## Installation
Install emscripten for example version `3.1.32`.


## Build

### 1. Build static web files

```
mkdir build
cd build
emcmake cmake .. -DCMAKE_BUILD_TYPE="Release"
emmake make VERBOSE=1
cd web
```

### 2. Copy game data

Create a `game` subfolder and copy all game data into into subfolder folder (`build/web/game/`)

### 3. (optional) Unpack game data

Due to async loading it is recommended to unpack game data. https://github.com/falltergeist/dat-unpacker.git can be used for this:
```
test -f master.dat && mkdir master.dat.dir && dat-unpacker -s master.dat -d master.dat.dir && rm master.dat && mv master.dat.dir master.dat
test -f critter.dat && mkdir critter.dat.dir && dat-unpacker -s critter.dat -d critter.dat.dir && rm critter.dat && mv critter.dat.dir critter.dat
```


### 4. Update game configuration

Skip intro videos by adding those lines in `ddraw.ini`:
```
[Misc]
SkipOpeningMovies=1
```

Force screen size in `f2_res.ini`
```
[MAIN]
SCR_WIDTH=640
SCR_HEIGHT=480
WINDOWED=1
```

### 5. Change line endings in text files (if .dat files were unpacked)

We aware of some issues with text files with CLRF line endings:
```
# This might cause game to have incompatible save game format due to wrong global vars count
dos2unix master.dat/data/VAULT13.GAM

# This fixes no sound effects issue
dos2unix master.dat/sound/SFX/SNDLIST.LST

# This fixes issue with death scene subtitles
dos2unix master.dat/data/enddeath.txt
 
```

This bash code replaces CLRF endings in all text files:
```
find . -type f -exec bash -c "(file -i -b {} | grep '^text/plain;') && dos2unix {}" \;
```


### 6. Add empty files to keep empty folders
This is a simple workaround for `asyncfetchfs` because it ignores empty folders. Just do this:

```
test -d data/SAVEGAME || mkdir data/SAVEGAME
touch data/SAVEGAME/.empty
touch data/MAPS/.empty
touch data/proto/items/.empty
touch data/proto/critters/.empty
```

### 7. Create files index
This list is used by `asyncfetchfs`

```
cd build/web/game
find . -type f -printf '%s\t%P\n' > ../index.txt
```

Do not forget to re-generate list if game files are changed, for example patch is applied or configuration is updated.

### 8. (Optional) Create archive with small files for background fetching

Fallout engine have lots of small files. Fetching them one-by-one can take lots of time due to network delay. But we can pack all small files and fetch them in background. To pack them do this:

```
cd build/web/game
rm ../preloadfiles.tar || true
find . -type f -size -20k ! -name '.empty' ! -path '*/data/SAVEGAME/*' -exec tar -rvf ../preloadfiles.tar {} \;
cd ..
gzip --best preloadfiles.tar
```

If you do not want to preload files then simply remove call to that function in the code.


### 9. Compress game data

In order to reduce game size we compress each file separately using ordinary `gz`:

```
gzip -v -r --best game
```

If you do not want to compress game files then change `useGzip` option when mounting `asyncfetchfs`

### 10. Add missing script file for Nevada

Fallout Of Nevada have issue in Zone 51 - there is no file `NCStrZxB.int`. Original game complains about missing file but works, this implementation crashes (as for me this is better behavior rather that silently ignore error). To fix this just add this file:

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


### 11. Done!

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