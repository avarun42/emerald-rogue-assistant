# Rogue Assistant

Rogue Assistant is a companion app for Pokémon Emerald Rogue. It adds two
features while the game runs in mGBA:

- Home Box storage that remains available between runs
- Multiplayer data sharing between supported games

The app reads and writes emulated Game Boy Advance memory through a small Lua
script in mGBA. It does not include, open, or change a ROM file.

This project is a cross-platform port of
[Pokabbie's original Windows app](https://github.com/Pokabbie/pokeemerald-rogue-assistant).

## Supported systems

- mGBA 0.10.5 or later
- Emerald Rogue with ROM Assistant API 3
- Vanilla and EX editions
- Windows x64
- macOS 11 or later on Apple silicon
- Linux x86_64

The current beta has been tested in the game on Apple silicon with mGBA 0.10.5
and Emerald Rogue 2.2.0 Vanilla. Windows, Linux, EX, Home Box recovery, and
cross-platform multiplayer still need more live testing.

## Get started

1. Download the package for your system from GitHub Releases.
2. Follow the steps in [Install Rogue Assistant](docs/installation.md).
3. Start Rogue Assistant.
4. Open Emerald Rogue in mGBA.
5. Press `R` in Rogue Assistant to open its script folder.
6. In mGBA, select **Tools > Scripting**, then load
   `RogueAssistant_mGBA.lua` from that folder.

The app exports the script when it starts. The script connects only to the
local computer. Port `30125` is used by default.

If the app does not connect, see [Troubleshooting](docs/troubleshooting.md).

## Build from source

You need CMake 3.25 or later and a C++20 compiler.

```sh
cmake --preset dev-debug
cmake --build --preset dev-debug --parallel
ctest --preset dev-debug
```

See [Development](docs/development.md) for platform packages and other test
presets. See [Architecture](docs/architecture.md) for the main parts of the
app.

## Technical documents

- [Bridge protocol](docs/bridge-protocol.md)
- [Home Box file format](docs/home-box-format.md)
- [Multiplayer protocol](docs/multiplayer-protocol.md)
- [Release process](docs/release.md)
- [1.0.0 beta 1 notes](docs/release-notes-1.0.0-beta.1.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)
