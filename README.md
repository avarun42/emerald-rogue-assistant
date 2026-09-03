# Rogue Assistant

Rogue Assistant is a companion application for Pokemon Emerald Rogue. It reads
and writes the running game's emulated GBA memory through mGBA, provides the
Home Box persistence feature, and relays Emerald Rogue multiplayer state.

Varun Arora ([`avarun42`](https://github.com/avarun42)) maintains this
cross-platform fork. It is based on
[Pokabbie's original Rogue Assistant](https://github.com/Pokabbie/pokeemerald-rogue-assistant),
which is preserved in the Git history. Pokabbie does not maintain this fork.

The legacy 2.2-era application was Windows-only and loaded a native DLL inside
mGBA. The `1.0.0` line is one standalone C++20 application and one portable
mGBA Lua bridge for Windows, macOS, and Linux. The game-facing feature logic
remains in C++.

## Compatibility

- Emulator: mGBA 0.10.5 or newer
- ROM Assistant API: exactly version 3
- ROM editions: Vanilla and EX
- Initial desktop targets: Windows x64, macOS arm64, Linux x86_64

The application version, mGBA bridge protocol, multiplayer protocol, Home Box
format, and ROM Assistant API are versioned independently. See
[`docs/compatibility.md`](docs/compatibility.md).

## Configure and test

The modernization build requires CMake 3.25 or newer and a C++20 compiler.

```sh
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug
```

Architecture details live in [`docs/architecture.md`](docs/architecture.md),
with local quality-gate instructions in
[`docs/development.md`](docs/development.md).
Standalone usage, bridge controls, resource locations, and command-line
behavior are documented in
[`docs/desktop-application.md`](docs/desktop-application.md).
Home Box migration/recovery and the modern multiplayer compatibility gate are
documented in [`docs/home-box-format.md`](docs/home-box-format.md) and
[`docs/multiplayer-protocol.md`](docs/multiplayer-protocol.md).

## Install and release

End-user installation, checksum verification, mGBA script loading, and data
locations are covered by [`docs/installation.md`](docs/installation.md).
Maintainers should use the platform release presets and draft-only packaging
workflow described in [`docs/release.md`](docs/release.md).

The repository owner has not yet selected a project license, and inherited
artwork does not yet have recorded redistribution provenance. These are hard
public-release gates documented in
[`docs/asset-provenance.md`](docs/asset-provenance.md); third-party dependency
terms are collected in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
