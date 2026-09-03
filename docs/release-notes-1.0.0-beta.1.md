# Rogue Assistant 1.0.0 beta 1

This beta brings the Rogue Assistant desktop application to Apple silicon and
replaces the Windows-only native mGBA bridge with a portable Lua and loopback
TCP bridge. The same application also builds for Windows x64 and Linux x86_64.

## Verified on macOS

The maintainer tested the Apple silicon build with mGBA 0.10.5 and Pokémon
Emerald Rogue 2.2.0 Vanilla with ROM Assistant API 3. The test covered both
startup orders, the ROM handshake, the assistant confirmation write, pause and
resume, ROM reset, script reload, application restart, mGBA shutdown, and clean
application shutdown.

The application and text render at native resolution on a Retina display. The
packaged executable contains only the arm64 architecture.

## Beta coverage still needed

Please help test:

- Windows x64 and Linux x86_64 package installation and launch
- Emerald Rogue EX with ROM Assistant API 3
- Home Box load, save, backup recovery, and legacy data import with real saves
- Multiplayer between supported operating systems
- Long play sessions, repeated resets, and repeated reconnects

These areas have automated coverage, but they have not all been tested in a
real game on every supported operating system. They remain requirements for the
final 1.0.0 release.

## Report a problem

Open a GitHub issue and include:

- Operating system and CPU architecture
- Rogue Assistant, mGBA, and Emerald Rogue versions
- Vanilla or EX edition and ROM Assistant API version
- What you expected and what happened
- The relevant `RogueAssistant.log` file, after checking it for private data

See [Installation and first run](installation.md) for setup instructions and
data locations.
