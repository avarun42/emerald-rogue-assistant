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

## Temporary native adapter

`NativeLuaTransport` converts value requests into the existing guarded DLL/Lua
queues. The Lua thread can complete a legacy queue entry, but the adapter only
enqueues the resulting value. `GameSession::Poll()` dispatches it on the game
connection thread. This keeps the released Windows path available as a parity
reference while making the game logic independent of that path.

The native adapter is transitional. The final application uses
`TcpLuaTransport`, and the DLL, its queue ABI, and this adapter are removed only
after the socket parity gate.
