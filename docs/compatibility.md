# Compatibility and versioning

Rogue Assistant uses independent version domains:

| Domain | Initial modern version | Rule |
| --- | --- | --- |
| Desktop application | 1.0.0 | Semantic versioning |
| mGBA bridge protocol | 1.0 | Major must match; minor is negotiated |
| Multiplayer envelope | 1.0 | Major must match between peers |
| Home Box file | 1 | Legacy format 0 remains readable |
| ROM Assistant API | 3 | Exact match for application 1.0.0 |

The ROM's `rogueVersion` field identifies Vanilla (`0`) versus EX (`1`); it is
not the Emerald Rogue release number. The current application intentionally
retains the strict API-3 gate even though API 1-3 headers are structurally
similar. Supporting older or future APIs requires separate fixtures and live
validation.

The 1.0.0 support contract is mGBA 0.10.5 or newer, ROM Assistant API 3, and
both Vanilla and EX editions. Other emulators are out of scope.
