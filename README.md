# Rogue Assistant

Rogue Assistant is a companion application for Pokémon Emerald Rogue. It reads
and writes the running game's emulated GBA memory through mGBA, provides the
Home Box persistence feature, and relays Emerald Rogue multiplayer state.

Varun Arora ([`avarun42`](https://github.com/avarun42)) maintains this
cross-platform fork. It is based on
[Pokabbie's original Rogue Assistant](https://github.com/Pokabbie/pokeemerald-rogue-assistant),
which is preserved in the Git history. Pokabbie does not maintain this fork.

The legacy 2.2-era application supported only Windows and loaded a native DLL
inside mGBA. The 1.0 series uses one standalone C++20 application and one
portable mGBA Lua bridge on Windows, macOS, and Linux. The game-facing feature
logic remains in C++.

## Compatibility

- Emulator: mGBA 0.10.5 or newer
- ROM Assistant API: exactly version 3
- ROM editions: Vanilla and EX
- Initial desktop targets: Windows x64, macOS arm64, Linux x86_64

The application version, mGBA bridge protocol, multiplayer protocol, Home Box
format, and ROM Assistant API are versioned independently. See
[Compatibility and versioning](docs/compatibility.md).

## Configure and test

The modernization build requires CMake 3.25 or newer and a C++20 compiler.

```sh
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug
```

See [Architecture](docs/architecture.md) for the runtime design and
[Development](docs/development.md) for local quality gates. See
[Standalone desktop application](docs/desktop-application.md) for bridge
controls, resource locations, and command-line behavior. The
[Home Box storage format](docs/home-box-format.md) and
[Multiplayer protocol](docs/multiplayer-protocol.md) pages define the durable
data and peer compatibility contracts.

## Install and release

See [Installation and first run](docs/installation.md) for checksum
verification, mGBA script loading, and data locations. To create packages, use
the platform release presets and draft-only workflow in
[Release engineering](docs/release.md).

The original Rogue Assistant creator has approved this fork and its GitHub
releases. The repository does not yet define a general license for downstream
reuse. See [Distribution status](docs/asset-provenance.md) and
[Third-party notices](THIRD_PARTY_NOTICES.md). The
[beta release notes](docs/release-notes-1.0.0-beta.1.md) describe verified
behavior and the remaining test coverage.
