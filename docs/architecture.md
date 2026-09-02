# Architecture

## Current released runtime

The 2.2-era Windows release starts as a small executable that writes a Lua
script. mGBA loads that script, which loads `RogueAssistant.dll` into the
emulator process. The DLL starts an SFML window thread and exchanges memory
requests with the emulator's Lua thread through guarded queues. A connection
thread observes game memory and runs Home Box and multiplayer behaviors.

The game contract is self-describing. Rogue Assistant reads the Game Freak ROM
header at `0x08000100`, validates the Emerald Rogue handshakes, follows the
Rogue Assistant header pointer, and obtains all relevant addresses, offsets,
sizes, box counts, and player counts from that header. The assistant does not
parse or patch the ROM file.

## Target runtime

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

## Migration rule

The native DLL transport remains a temporary adapter until the socket path has
passed feature parity. Each migration increment must leave its applicable
targets buildable and tested. Formatting, dependency upgrades, protocol
changes, and behavior changes are separate review units.

Filesystem roots, strict settings behavior, resource lookup, and the Windows
legacy import policy are documented in [platform-services.md](platform-services.md).
