# zlib

Fallout 2 uses `zlib` v1.1.1 (as seen at `0x4E18A0`, `0x4ED2F0`, and other various places) to decompress data embedded in .DAT files. There is no point in using such old version, because zlib is highlighly optimized, portable, and provides backward compatibility.
