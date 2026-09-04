# Home Box file format 1

Emerald Rogue Assistant stores Home Box data in
`<data folder>/<edition>/<trainer ID>/boxes.dat`. All integers use unsigned
little-endian byte order.

A file belongs to one ROM Assistant API, ROM edition, trainer, box count,
metadata size, and Pokémon data size. The app rejects a file when any of these
values do not match the running game.

## Header

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII magic `RABX` |
| 4 | 2 | File format, exactly `1` |
| 6 | 2 | Reserved, zero |
| 8 | 4 | ROM Assistant API, exactly `3` |
| 12 | 1 | ROM edition: Vanilla `0`, EX `1` |
| 13 | 3 | Reserved, zero |
| 16 | 4 | Trainer ID |
| 20 | 4 | Number of box records |
| 24 | 4 | Metadata bytes in each record |
| 28 | 4 | Pokémon data bytes in each record |

The file can contain at most 255 records. Each record data size must be from 1
byte through 1 MiB. The complete file can be at most 64 MiB.

## Records

The header is followed by the exact number of records named in the header.
Each record contains:

| Size | Field |
| ---: | --- |
| 4 | Box index, starting at zero |
| Header value | Metadata |
| Header value | Pokémon data |
| 4 | IEEE CRC32 of the index, metadata, and Pokémon data |

Each box index must appear once. No index can be missing or repeated.

The last four bytes are an IEEE CRC32 of the header and all records, including
each record CRC32.

The reader rejects:

- Unknown file formats
- Nonzero reserved bytes
- A wrong ROM API, edition, trainer ID, box count, or record size
- Missing or repeated box indexes
- A bad record or file CRC32
- A file that is cut short, too large, or has extra bytes

## Save and recovery

The app writes a new file beside the current file with a unique temporary
name. It flushes and closes the new file before it uses an atomic rename.

If the current `boxes.dat` file is valid, the app keeps it as `boxes.dat.bak`
before it installs the new file. The backup is also replaced with an atomic
rename.

The app does not overwrite an invalid `boxes.dat`. When the main file is
missing or invalid, it tries `boxes.dat.bak`. If the backup is valid, the app
loads it, shows a warning, and leaves the invalid main file unchanged.

## Old format import

The reader still accepts the original format 0 when every record checksum and
the file checksum are valid.

After a successful import, the app keeps the original bytes as
`boxes.dat.v0.bak`. The next successful save writes format 1. The app never
replaces a damaged record with an empty box.
