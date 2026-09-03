# Compatibility and versioning

Rogue Assistant uses independent version domains:

| Domain | Initial modern version | Rule |
| --- | --- | --- |
| Desktop application | 1.0.0 | Semantic versioning |
| mGBA bridge protocol | 1.0 | Major must match; minor is negotiated |
| Multiplayer envelope | 1.0 | Major must match between peers |
| Home Box file | 1 | Legacy format 0 remains readable |
| ROM Assistant API | 3 | Exact match for application 1.0.0 |

The ROM's `rogueVersion` field identifies Vanilla (`0`) or EX (`1`). It is not
the Emerald Rogue release number. The application requires API 3 even though
the headers for APIs 1 through 3 have similar structures. Support for another
API requires separate fixtures and live validation.

The 1.0.0 support contract is mGBA 0.10.5 or newer, ROM Assistant API 3, and
both Vanilla and EX editions. Other emulators are out of scope.

The concrete storage and peer wire contracts are frozen in
[Home Box storage format](home-box-format.md) and
[Multiplayer protocol](multiplayer-protocol.md).

## Beta validation status

Automated tests exercise the Vanilla and EX edition values and their dynamic
ROM layouts. The completed live test used mGBA 0.10.5, Emerald Rogue 2.2.0
Vanilla with Assistant API 3, and macOS on Apple silicon. Live testing has not
yet covered the EX edition, Windows, Linux, or cross-platform multiplayer.
These gaps are beta test targets and remain blockers for the final 1.0.0
release. See the [Parity gate](parity-gate.md) for the full matrix.
