#!/usr/bin/env bash
set -e

USAGE="Usage: $0 <installer.exe|installer.sh|install-dir> [output-dir]"
INPUT="${1:?$USAGE}"
OUTPUT="${2:-.}"
mkdir -p "$OUTPUT"

check_innoextract() {
    command -v innoextract &>/dev/null || {
        echo "innoextract is required for .exe installers. Install it:"
        echo "  macOS:  brew install innoextract"
        echo "  Ubuntu: sudo apt install innoextract"
        exit 1
    }
}

copy_game_files() {
    local src="$1"
    local base
    base=$(find "$src" -maxdepth 4 -iname "master.dat" -printf "%h\n" 2>/dev/null | head -1)
    # macOS find doesn't support -printf; fall back to a POSIX-compatible form
    if [ -z "$base" ]; then
        base=$(find "$src" -maxdepth 4 -iname "master.dat" 2>/dev/null | head -1 | xargs -I{} dirname {})
    fi
    [ -z "$base" ] && { echo "ERROR: Could not find master.dat in $src"; exit 1; }
    echo "Found game files at: $base"
    cp -v "$base/master.dat"  "$OUTPUT/"
    cp -v "$base/critter.dat" "$OUTPUT/"
    cp -rv "$base/data"       "$OUTPUT/"
    echo ""
    echo "=== Game files extracted to: $OUTPUT ==="
    print_platform_help
}

print_platform_help() {
    echo ""
    echo "Next steps by platform:"
    echo "  macOS:       Copy files into Fallout II Community Edition.app/Contents/MacOS/"
    echo "  Windows:     Copy files to the same folder as fallout2-ce.exe"
    echo "  Linux:       Copy files to the same folder as the fallout2-ce binary"
    echo "  Steam Deck:  Copy files to the same folder as Fallout2-CE-SteamDeck-x86_64.AppImage"
    echo "  Xbox:        master.dat + critter.dat -> ISO root (D:\\)"
    echo "               data\\ -> E:\\UDATA\\FALLOUT2\\"
}

if   [ -d "$INPUT" ];          then copy_game_files "$INPUT"
elif [[ "$INPUT" == *.exe ]];  then check_innoextract
                                    TMP=$(mktemp -d); trap "rm -rf $TMP" EXIT
                                    innoextract --output-dir "$TMP" "$INPUT"
                                    copy_game_files "$TMP"
elif [[ "$INPUT" == *.sh ]];   then TMP=$(mktemp -d); trap "rm -rf $TMP" EXIT
                                    sh "$INPUT" --tar-extract "$TMP" || true
                                    copy_game_files "$TMP"
else echo "$USAGE"; exit 1
fi
