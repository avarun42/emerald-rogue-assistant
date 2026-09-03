# Home Box storage format 1

Home Box data is stored below the platform data directory as
`<edition>/<trainer-id>/boxes.dat`. All integers are unsigned little-endian.
The file is tied to the exact ROM Assistant API, ROM edition, trainer, remote
box count, minimal-metadata size, and Pokémon-data size that created it.

## Header

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII magic `RABX` |
| 4 | 2 | format version, exactly `1` |
| 6 | 2 | reserved, zero |
| 8 | 4 | ROM Assistant API, exactly `3` |
| 12 | 1 | ROM edition: Vanilla `0`, EX `1` |
| 13 | 3 | reserved, zero |
| 16 | 4 | trainer ID |
| 20 | 4 | remote-box record count |
| 24 | 4 | minimal-metadata bytes per record |
| 28 | 4 | Pokémon-data bytes per record |

The record count is at most 255. Each per-record data dimension is nonzero and
at most 1 MiB, and the complete file is at most 64 MiB.

## Records and footer

Exactly `remote-box record count` records follow the header. Each contains:

| Size | Field |
| ---: | --- |
| 4 | zero-based remote-box index |
| header-defined | minimal metadata |
| header-defined | Pokémon data |
| 4 | IEEE CRC32 of the index and both data fields |

Every index must occur exactly once. A final four-byte IEEE CRC32 covers the
header and all complete records, including their record CRC values. Readers
reject unknown versions, nonzero reserved bytes, duplicate or missing indices,
dimension mismatches, corruption, truncation, and trailing data.

## Migration and recovery

The reader retains strict support for legacy format 0. After a successful
legacy load, it preserves the original bytes as `boxes.dat.v0.bak`. The next
successful save writes format 1. Every legacy record checksum and the legacy
footer must be valid. The reader never silently replaces damaged records with
empty boxes.

For each save, Rogue Assistant writes, flushes, and closes a unique temporary
sibling before it atomically renames the file. Before replacing a valid primary
file, it atomically retains the complete previous file as `boxes.dat.bak`. It
never overwrites an invalid primary file. If the primary is missing or invalid,
the loader attempts the backup, reports a successful recovery in the UI, and
leaves an invalid primary file available for inspection.
