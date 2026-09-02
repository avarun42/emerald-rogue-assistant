# Game-memory transport seam

`GameSession` is the only layer allowed to associate an application callback
with a game-memory operation. It assigns a nonzero 32-bit request ID, retains
the callback in an ID-keyed table, and invokes callbacks only when its owner
calls `Poll()`. A transport may receive data on another thread, but it must
return value-only `MemoryResult` objects from `PollResults()`; it cannot invoke
game or UI code directly.

## Request contract

- At most 256 operations may be outstanding per session.
- A single read or write is limited to 1 MiB.
- Reads may target EWRAM, IWRAM, palette RAM, VRAM, OAM, or cartridge ROM.
- Writes may target only EWRAM or IWRAM.
- Zero-length operations, malformed read/write combinations, request ID zero,
  arithmetic overflow, and spans crossing an allowed region are rejected.
- A successful read must contain exactly the requested byte count. A
  successful write must contain no response bytes.

When the transport is disconnected or the outstanding-request limit is
reached, `CanSubmit()` becomes false. Observation code suspends new polling
rather than accumulating unbounded work.

`GameSession::Stop()` completes only that session's callbacks. Transport
lifetime belongs to `GameConnectionManager`, which stops the listener before
detaching behaviors during application shutdown. This separation lets a Lua
peer disconnect, reload, reset its ROM, or reconnect without replacing the
worker or application.

## Portable TCP adapter

`TcpLuaTransport` owns a nonblocking listener bound specifically to
`127.0.0.1`. It has no network thread: the session worker drives accept,
receive, protocol dispatch, and partial sends through `PollResults()`. One peer
may finish the protocol 1.0 hello. Additional peers receive a rejected hello,
a stable busy error, and an orderly close.

The adapter preserves request IDs until a strictly matching read result,
write result, or error arrives. A malformed or unexpected response fails all
pending work closed and returns the listener to its reconnectable state. There
is no inactivity timeout, so a paused emulator cannot be mistaken for a lost
connection.

The retired in-process DLL adapter and its queue ABI were removed after the TCP
path passed the documented parity gate. `TcpLuaTransport` is the sole production
game-memory transport on every supported platform.
