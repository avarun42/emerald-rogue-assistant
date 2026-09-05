# Home Box file format

Emerald Rogue Assistant reads and writes the original Windows assistant's
format, version 0. No conversion is needed when moving files between the apps.

Files are stored at `<data folder>/<edition>/<trainer ID>/boxes.dat`. The
edition and trainer ID come from the folder path, not the file contents. Keep
that path when copying a file. The ROM API is not stored in the file either.

All integers are unsigned and use little-endian byte order. Pokémon and box
metadata are copied as bytes without changing the game's data.

## Header

The header contains five 32-bit integers:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | Header marker, `3497814` |
| 4 | 4 | File format, `0` |
| 8 | 4 | Number of stored boxes |
| 12 | 4 | Metadata bytes per box |
| 16 | 4 | Pokémon bytes per box |

The box count and byte sizes must match the running game's layout.

## Box records and footer

Each stored box has one record. Records appear in box order and contain:

| Size | Field |
| --- | --- |
| Header value | Metadata bytes |
| Header value | Pokémon bytes |
| 4 | Sum of all metadata and Pokémon bytes in this record |

The checksum is an unsigned 32-bit sum. It is the same checksum used by the
original assistant. It detects some changes to the bytes, but not all changes.
There is no stored box index or whole-file checksum.

The final four bytes are the footer marker, `7893612`.

The reader rejects a wrong marker, unknown version, mismatched dimensions,
incorrect checksum, truncated file, or trailing data. It also limits files
to 64 MiB and box counts to 255 before allocating storage.

## Save and recovery

The app writes a temporary file beside `boxes.dat`, then replaces `boxes.dat`
with an atomic rename. It keeps the previous valid file as `boxes.dat.bak`.
These safeguards do not change the file format.

If `boxes.dat` is missing or invalid, the app tries `boxes.dat.bak`. It shows
a warning when it loads the backup and does not overwrite an invalid main
file. Copy the complete trainer folder before attempting recovery.

The original assistant uses a different data folder. See
[Move Home Box data](installation.md#move-home-box-data) before switching apps.
