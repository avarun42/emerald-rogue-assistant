# Bridge Protocol 1.0

The portable mGBA adapter and Rogue Assistant communicate over a single TCP
connection. Rogue Assistant is the server and listens only on loopback. Every
integer is little-endian and every peer must tolerate fragmented frames,
coalesced frames, and partial socket sends.

## Frame

```text
offset  size  field
0       4     body length (8 through 1,048,576)
4       1     message type
5       1     flags (zero in protocol 1.0)
6       2     reserved (zero)
8       4     request ID
12      n     payload
```

The body length covers the eight-byte message header and payload but excludes
its own four-byte length. A decoder rejects unknown message IDs, nonzero flags
or reserved bits, invalid request-ID classes, and oversized input. Receive and
send queues each retain at most 256 frames and 4 MiB of wire data. The send
queue advances only by the byte count actually accepted by the socket. These
bounds are protocol implementation limits, not flow-control signals.

Because the 1 MiB body ceiling includes headers, a `ReadResult` can carry at
most 1,048,568 memory bytes and a `WriteRequest` at most 1,048,560 memory bytes.
The smaller write limit also accounts for its address and byte-count fields.

| ID | Name | Request ID | Payload |
|---:|---|---|---|
| 1 | `ClientHello` | zero | `RAB1`, protocol `u16 major, u16 minor`, script version `u32` |
| 2 | `ServerHello` | zero | status `u8`, reserved `u8`, protocol `u16, u16`, app SemVer `u16, u16, u16` |
| 3 | `ReadRequest` | nonzero | address `u32`, byte count `u32` |
| 4 | `WriteRequest` | nonzero | address `u32`, byte count `u32`, bytes |
| 5 | `ReadResult` | nonzero | requested bytes |
| 6 | `WriteResult` | nonzero | empty |
| 7 | `Error` | zero or originating request | stable code `u16`, diagnostic length `u16`, UTF-8 diagnostic |
| 8 | `Close` | zero | empty |

Protocol 1.0 uses script version `1`. Error diagnostics are limited to 1,024
bytes. Stable error codes are unsupported protocol (1), busy (2), malformed
frame (3), invalid request ID (4), invalid address (5), invalid size (6), queue
full (7), and internal error (8).

The handshake must finish before memory messages are accepted. Protocol majors
must match. The server may accept an older protocol minor only when all used
features are defined by that minor; the initial implementation negotiates
exactly 1.0. A rejected hello is followed by `Close`.

The canonical byte vectors live in
`tests/fixtures/bridge_protocol_1.golden`. Both the C++ codec suite and the Lua
adapter suite consume that file so the two implementations cannot silently
drift.

## mGBA execution bounds

The script receives and sends no more than 256 KiB per emulated frame. It
starts or advances at most 64 memory operations and 256 KiB of memory per
frame; larger individual requests are carried across multiple frames. Reads
use `emu:readRange`. Writes select aligned `write32` and `write16` calls with
`write8` for byte-safe leading and trailing data. The script independently
allows reads only from documented GBA RAM, video memory, and ROM ranges and
writes only to EWRAM or IWRAM.

mGBA owns socket event polling. The script buffers received bytes and retains
the unsent suffix reported by `socket:send`. A failed connection is attempted
again no more than once per wall-clock second. Pausing emulation performs no
work and has no timeout; bounded queues provide backpressure until frames
resume.
