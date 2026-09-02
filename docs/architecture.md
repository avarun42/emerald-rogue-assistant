# Architecture

## Retired legacy runtime

The 2.2-era Windows release started as a small executable that wrote a Lua
script. mGBA loads that script, which loads `RogueAssistant.dll` into the
emulator process. The DLL starts an SFML window thread and exchanges memory
requests with the emulator's Lua thread through guarded queues. A connection
thread observes game memory and runs Home Box and multiplayer behaviors.

The game contract remains self-describing. Rogue Assistant reads the Game Freak ROM
header at `0x08000100`, validates the Emerald Rogue handshakes, follows the
Rogue Assistant header pointer, and obtains all relevant addresses, offsets,
sizes, box counts, and player counts from that header. The assistant does not
parse or patch the ROM file.

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

The socket path passed its parity gate before the native DLL, Lua C bridge,
stub, and generated-resource path were removed. Each subsequent increment must
leave its applicable targets buildable and tested. Formatting, dependency
upgrades, protocol changes, and behavior changes remain separate review units.

Filesystem roots, strict settings behavior, resource lookup, and the Windows
legacy import policy are documented in [platform-services.md](platform-services.md).
The bounded request model and TCP adapter are documented in
[game-memory-transport.md](game-memory-transport.md).
Thread ownership, value-only UI boundaries, and deterministic shutdown are
documented in [lifecycle.md](lifecycle.md).
The frozen loopback wire format is documented in
[bridge-protocol.md](bridge-protocol.md).
The standalone process, packaged-resource model, and bridge controls are
documented in [desktop-application.md](desktop-application.md).
The durable Home Box representation and recovery policy are documented in
[home-box-format.md](home-box-format.md). The peer compatibility envelope and
opaque ROM payload channels are documented in
[multiplayer-protocol.md](multiplayer-protocol.md).
