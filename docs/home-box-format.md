# Home Box storage format 1

Home Box data is stored below the platform data directory as
`<edition>/<trainer-id>/boxes.dat`. All integers are unsigned little-endian.
The file is tied to the exact ROM Assistant API, ROM edition, trainer, remote
box count, minimal-metadata size, and Pokemon-data size that created it.

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
| 28 | 4 | Pokemon-data bytes per record |

The record count is at most 255. Each per-record data dimension is nonzero and
at most 1 MiB, and the complete file is at most 64 MiB.

## Records and footer

Exactly `remote-box record count` records follow the header. Each contains:

| Size | Field |
| ---: | --- |
| 4 | zero-based remote-box index |
| header-defined | minimal metadata |
| header-defined | Pokemon data |
| 4 | IEEE CRC32 of the index and both data fields |

Every index must occur exactly once. A final four-byte IEEE CRC32 covers the
header and all complete records, including their record CRC values. Readers
reject unknown versions, nonzero reserved bytes, duplicate or missing indices,
dimension mismatches, corruption, truncation, and trailing data.

## Migration and recovery

The reader retains strict support for legacy format 0. A successful legacy
load preserves the original bytes as `boxes.dat.v0.bak`; the next successful
save writes format 1. Legacy record checksums and the legacy footer must all be
valid—damaged records are not silently replaced with empty boxes.

Each save is written to a unique temporary sibling, flushed and closed, then
atomically renamed. Before replacing a valid primary, its complete bytes are
atomically retained as `boxes.dat.bak`. If the primary cannot be validated, it
is never overwritten. Loading attempts the backup after a missing or invalid
primary, reports recovery visibly, and leaves an invalid primary untouched for
manual inspection.
