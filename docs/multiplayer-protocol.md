# Multiplayer protocol 1.0

Rogue Assistant continues to use ENet and treats the ROM-provided handshake,
game state, player profiles, and player state as opaque byte sequences. Modern
peers first establish an application-owned compatibility envelope. Released
legacy assistants do not implement this envelope and are intentionally not
interoperable with version 1.0.

## Channels

| ENet channel | Purpose | Delivery |
| ---: | --- | --- |
| 0 | compatibility control | reliable |
| 1 | opaque ROM handshake | reliable |
| 2 | opaque host game state | reliable |
| 3 | opaque player state | unreliable fragment |
| 4 | opaque player profiles | reliable |

Rogue Assistant does not process an opaque ROM or gameplay payload until the
sending peer passes the compatibility gate. The host sends broadcasts only to
compatible peers that have also completed the ROM handshake.

## Compatibility hello

The first control-channel payload is exactly 40 bytes. All integers are
unsigned little-endian.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII magic `RAMP` |
| 4 | 2 | protocol major, `1` |
| 6 | 2 | protocol minor, `0` |
| 8 | 4 | ROM Assistant API, exactly `3` |
| 12 | 1 | ROM edition: Vanilla `0`, EX `1` |
| 13 | 3 | reserved, zero |
| 16 | 4 | ROM-provided player count |
| 20 | 4 | complete multiplayer-state size |
| 24 | 4 | ROM handshake size |
| 28 | 4 | host game-state size |
| 32 | 4 | player-profile size |
| 36 | 4 | player-state size |

Both peers must match the protocol major, API, edition, player count, and all
five structure sizes. Minor negotiation selects the lower compatible minor.
Protocol 1.0 defines no optional minor features. A peer has five seconds after
the ENet connection to provide one valid hello.

Rogue Assistant disconnects a peer that sends a malformed, duplicate,
mismatched, out-of-order, oversized, or directionally invalid payload. It also
shows a local diagnostic. Because ENet orders each reliable channel
independently, Rogue Assistant can retain one correctly sized early ROM
handshake until the compatibility hello arrives. It validates every
network-derived player ID before using the ID to index ROM state.
