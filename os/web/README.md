# Webassembly

## Installation
Install emscripten for example version `3.1.32`.


## Build

### 1. Build static web files

```
mkdir build
cd build
emcmake cmake ..
emmake make
cd web
```

### 2. Copy game data

Copy all game data into output folder (`build/web`)

### 3. (optional) Unpack game data

Due to async loading it is recommended to unpack game data. https://github.com/falltergeist/dat-unpacker.git can be used for this:
```
dat-unpacker -s master.dat -d . && rm master.dat
dat-unpacker -s critter.dat -d . && rm critter.dat
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

### 5. Add empty files to keep empty folders
This is a simple workaround for `asyncfetchfs` because it ignores empty folders. Just do this:

```
touch data/MAPS/.empty
touch data/proto/items/.empty
touch data/proto/critters/.empty
```

### 6. Create files index
This list is used by `asyncfetchfs`

```
find . -type f -printf '%s\t%P\n' > index.txt
```

### 7. Done!

Check that everything works by starting web server and opening webpage:
```
npx http-server .
```