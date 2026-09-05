# Emerald Rogue Assistant 1.0.0 beta 1

This beta brings Emerald Rogue Assistant to Apple silicon. It replaces the original
Windows-only mGBA bridge with a portable Lua script and a local TCP connection.
The same source also builds for Windows x64 and Linux x86_64.

Home Box files use the original assistant's format. You can copy these files
between this app and the original Windows app. The apps use separate data
folders; see [Move Home Box data](installation.md#move-home-box-data).

## Tested on macOS

The Apple silicon package was tested with mGBA 0.10.5 and Pokémon Emerald
Rogue 2.2.0 Vanilla with ROM Assistant API 3.

The test covered:

- Starting mGBA before and after Emerald Rogue Assistant
- The ROM connection and confirmation write
- Pause and resume
- ROM reset
- Lua script reload
- Emerald Rogue Assistant restart
- mGBA shutdown
- Emerald Rogue Assistant shutdown
- Native Retina resolution and clear text
- An arm64-only app package

## More beta testing is needed

Please help test:

- Windows x64 and Linux x86_64 packages
- Emerald Rogue EX with ROM Assistant API 3
- Home Box load, save, old file import, and backup recovery with real game data
- Multiplayer between supported systems
- Long play sessions and repeated reconnects

These areas have automated tests. They have not all been used in a real game
on every supported system. They must be tested before the final 1.0.0 release.

## Report a problem

Include:

- Your operating system and CPU type
- The versions of Emerald Rogue Assistant, mGBA, and Emerald Rogue
- The Vanilla or EX edition
- The ROM Assistant API version
- What you expected
- What happened
- The relevant `RogueAssistant.log` lines, after you check them for private
  data

See [Install Emerald Rogue Assistant](installation.md) for setup and file locations.
See [Troubleshooting](troubleshooting.md) for common problems.
