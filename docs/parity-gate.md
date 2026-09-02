# Parity gate

The socket bridge must match the API-3 behavior of the legacy native bridge before a release is tagged. This gate combines deterministic automated tests with real mGBA and cross-platform manual checks.

## Automated gate

Every supported compiler build must pass the normal test suite. Clang builds must also pass the AddressSanitizer and UndefinedBehaviorSanitizer suite. The suite characterizes:

- strict ROM Assistant API 3 acceptance for both Vanilla and EX editions;
- ROM handshake validation, dynamic state observation, confirmation writes, Home Box initialization, and multiplayer request transitions;
- request-ID matching, malformed responses, the 256-request limit, and paused-emulator backpressure;
- bridge handshakes, fragmented and coalesced frames, partial sends, incompatible peers, busy-peer rejection, and reconnects;
- Home Box format 0 import, format 1 round trips, corruption rejection, backup recovery, and interrupted atomic writes;
- multiplayer protocol and ROM-layout compatibility checks;
- deterministic session shutdown and UI command/snapshot confinement; and
- production and test-mode Lua loading against shared golden byte vectors.

## Verified mGBA 0.10.5 smoke test

The 2026-09-02 macOS parity run used mGBA 0.10.5 and an Emerald Rogue 2.2.0 Vanilla ROM exposing Assistant API 3. It verified:

- mGBA may start before the assistant and waits on loopback without crashing;
- the assistant may start before mGBA and accepts the later script connection;
- the bridge reads the API-3 headers and emits the ROM's two-byte confirmation write;
- pausing emulation for ten seconds does not disconnect or grow an unbounded queue, and normal frame rate resumes after bounded work drains;
- ROM reset disconnects once, then reconnects cleanly;
- resetting and reloading the mGBA scripting context reconnects cleanly;
- restarting the assistant leaves mGBA waiting and reconnects when the listener returns;
- closing mGBA returns the assistant to its listening state; and
- closing the assistant stops the worker and listener cleanly.

This run found an mGBA-specific Lua lifecycle defect: a top-level script return value remained on mGBA 0.10.5's Lua callback stack. The production script now returns no values, wraps callback boundaries with `xpcall`, and registers explicit start, reset, shutdown, and stop lifecycle handlers. Lua regression tests enforce this behavior.

## Release-only manual matrix

These checks require clean machines or VMs and remain release blockers rather than claims made by a single-host development run:

- launch packaged Windows x64, universal macOS arm64 and x86_64, and Linux x86_64 artifacts;
- verify resource discovery, script export, settings persistence, and documented filesystem roots on every platform;
- exercise Home Box load/save/migration with representative existing user data;
- exercise multiplayer in both host/client directions for Windows to macOS and at least one Linux pairing;
- reject multiplayer protocol, ROM API, edition, player-count, and layout mismatches with visible diagnostics; and
- repeat pause, reset, script reload, application restart, and shutdown checks on each platform.

Record the application version, mGBA version, ROM edition/API, operating systems, architectures, and result for every release-candidate run. Do not tag `v1.0.0` until every item passes without an mGBA crash, silent data loss, or unbounded queue.
