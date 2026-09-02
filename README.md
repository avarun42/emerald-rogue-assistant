# Rogue Assistant

Rogue Assistant is a companion application for Pokemon Emerald Rogue. It reads
and writes the running game's emulated GBA memory through mGBA, provides the
Home Box persistence feature, and relays Emerald Rogue multiplayer state.

The released 2.2-era application is Windows-only and loads a native DLL inside
mGBA. Development on the `1.0.0` line is replacing that boundary with one
standalone C++20 application and one portable mGBA Lua bridge for Windows,
macOS, and Linux. The game-facing feature logic remains in C++.

## Compatibility

- Emulator: mGBA 0.10.5 or newer
- ROM Assistant API: exactly version 3
- ROM editions: Vanilla and EX
- Initial desktop targets: Windows x64, macOS arm64/x86_64, Linux x86_64

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

The existing Visual Studio solution remains the reference Windows build until
the standalone socket application passes feature parity. Architecture and
migration details live in [`docs/architecture.md`](docs/architecture.md).
