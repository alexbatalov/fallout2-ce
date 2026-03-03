#!/usr/bin/env bash
# Fallout 2 CE — Steam Deck launcher
# master.dat, critter.dat, and data/ must be in the same directory as this script.
HERE="$(cd "$(dirname "$0")" && pwd)"
exec "$HERE/fallout2-ce" "$@"
