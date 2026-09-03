# Architecture

## Retired legacy runtime

The 2.2-era Windows release started as a small executable that wrote a Lua
script. mGBA loads that script, which loads `RogueAssistant.dll` into the
emulator process. The DLL starts an SFML window thread and exchanges memory
requests with the emulator's Lua thread through guarded queues. A connection
thread observes game memory and runs Home Box and multiplayer behaviors.

The game contract remains self-describing. Rogue Assistant reads the Game
Freak ROM header at `0x08000100`, validates the Emerald Rogue handshakes, and
follows the Rogue Assistant header pointer. That header supplies all relevant
addresses, offsets, sizes, box counts, and player counts. The assistant does
not parse or patch the ROM file.

## Current runtime

```text
mGBA + ROM + portable Lua       Rogue Assistant desktop process
-------------------------       -------------------------------
frame callback                  main thread: SFML UI
readRange / write8/16/32  TCP   worker: bridge + game session
bounded request execution <-->  Home Box + ENet multiplayer
```

The Lua bridge is deliberately limited to transport and emulated-memory
operations. C++ remains the single implementation of game observation, Home
Box behavior, multiplayer behavior, persistence, and presentation.

The application main thread owns the SFML window. One session worker owns all
mutable game state and networking. The UI submits value commands and renders
immutable snapshots; it never reaches into mutable behavior objects.

## Maintenance rule

The project removed the native DLL, Lua C bridge, stub, and generated-resource
path after the socket path passed its automated tests and the documented macOS
smoke test. The cross-platform release matrix remains open. Every later change
must leave its applicable targets buildable and tested. Keep formatting,
dependency upgrades, protocol changes, and behavior changes in separate review
units.

For related details, see:

- [Platform services](platform-services.md) for filesystem roots, settings,
  resource lookup, and Windows legacy import.
- [Game-memory transport](game-memory-transport.md) for the bounded request
  model and TCP adapter.
- [Application lifecycle and thread ownership](lifecycle.md) for UI boundaries
  and deterministic shutdown.
- [Bridge protocol](bridge-protocol.md) for the loopback wire format.
- [Standalone desktop application](desktop-application.md) for packaged
  resources and bridge controls.
- [Home Box storage format](home-box-format.md) for persistence and recovery.
- [Multiplayer protocol](multiplayer-protocol.md) for peer compatibility and
  opaque ROM payload channels.
