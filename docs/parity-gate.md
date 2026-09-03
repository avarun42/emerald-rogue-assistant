# Parity gate

The socket bridge must match the API-3 behavior of the legacy native bridge
before the project tags a final release. This gate combines deterministic
automated tests with manual mGBA and cross-platform checks.

## Automated gate

Every supported compiler build must pass the normal test suite. Clang builds
must also pass the AddressSanitizer and UndefinedBehaviorSanitizer suite. The
tests cover:

- Strict ROM Assistant API 3 acceptance for the Vanilla and EX editions.
- ROM handshake validation, dynamic state observation, confirmation writes,
  Home Box initialization, and multiplayer request transitions.
- Request-ID matching, malformed responses, the 256-request limit, and
  paused-emulator backpressure.
- Bridge handshakes, fragmented and coalesced frames, partial sends,
  incompatible peers, busy-peer rejection, and reconnections.
- Home Box format 0 import, format 1 round trips, corruption rejection, backup
  recovery, and interrupted atomic writes.
- Multiplayer protocol and ROM-layout compatibility checks.
- Deterministic session shutdown and UI command and snapshot confinement.
- Production and test-mode Lua loading against shared golden byte vectors.

## Verified mGBA 0.10.5 smoke test

The September 2, 2026, macOS parity run used mGBA 0.10.5 and an Emerald Rogue
2.2.0 Vanilla ROM with Assistant API 3. It verified these behaviors:

- mGBA can start before Rogue Assistant and wait on loopback without crashing.
- Rogue Assistant can start before mGBA and accept the later script connection.
- The bridge reads the API-3 headers and sends the ROM's two-byte confirmation
  write.
- Pausing emulation for 10 seconds does not disconnect or create an unbounded
  queue. The normal frame rate resumes after the bounded work drains.
- A ROM reset disconnects once and then reconnects cleanly.
- Resetting and reloading the mGBA scripting context reconnects cleanly.
- Restarting Rogue Assistant leaves mGBA waiting. The script reconnects when
  the listener returns.
- Closing mGBA returns Rogue Assistant to its listening state.
- Closing Rogue Assistant stops the worker and listener cleanly.

This run found an mGBA-specific Lua lifecycle defect: a top-level script return
value remained on the Lua callback stack in mGBA 0.10.5. The production script
now returns no values, wraps callback boundaries with `xpcall`, and registers
explicit handlers for start, reset, shutdown, and stop events. Lua regression
tests enforce this behavior.

## Release-only manual matrix

These checks require clean machines or virtual machines. They remain final
release blockers and are not claims from the single-host development run:

- Launch the packaged Windows x64, macOS arm64, and Linux x86_64 artifacts.
- Verify resource discovery, script export, settings persistence, and documented
  filesystem roots on every platform.
- Exercise Home Box load, save, and migration with representative user data.
- Exercise multiplayer in both host and client directions between Windows and
  macOS and in at least one pairing that includes Linux.
- Confirm that Rogue Assistant rejects multiplayer protocol, ROM API, edition,
  player-count, and layout mismatches with visible diagnostics.
- Repeat the pause, reset, script reload, application restart, and shutdown
  checks on each platform.

For every release-candidate run, record the application version, mGBA version,
ROM edition and API, operating system, architecture, and result. Do not tag the
final `v1.0.0` release until every item passes without an mGBA crash, silent
data loss, or unbounded queue.
