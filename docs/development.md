# Development

Emerald Rogue Assistant uses CMake 3.25 or later and C++20. The checked-in presets use
the same main options locally and in GitHub Actions.

## Build and test

For a debug build:

```sh
cmake --preset dev-debug
cmake --build --preset dev-debug --parallel
ctest --preset dev-debug
```

Use `dev-release` for an optimized local build. The result is
`RogueAssistant.exe` on Windows, `RogueAssistant.app` on macOS, and
`RogueAssistant` on Linux.

You can check the app without opening a window:

```sh
RogueAssistant --version
RogueAssistant --help
```

The first CMake setup downloads release archives for direct dependencies. Each
archive has a fixed checksum. The desktop app builds SFML 3.1.0 and ENet
1.3.18 from source. Tests use Catch2 3.15.3.

Linux builds need development packages for FreeType, OpenGL, udev, X11,
Xcursor, Xext, Xi, and Xrandr.

## Main targets

The code is split into these targets:

- `rogue_core`
- `rogue_bridge`
- `rogue_multiplayer`
- `rogue_app`
- `RogueAssistant`
- `rogue_tests`

See [Architecture](architecture.md) for the job of each target.

## Extra checks

GitHub Actions builds and tests with MSVC x64, Apple Clang, GCC, and Clang.
Warnings in project code are errors in these jobs. The Linux Clang build also
runs `clang-tidy`. Separate jobs test the Lua script and run AddressSanitizer
and UndefinedBehaviorSanitizer.

To run the sanitizer checks on Linux:

```sh
cmake --preset ci-sanitizers
cmake --build --preset ci-sanitizers --parallel
ctest --preset ci-sanitizers
```

To run `clang-tidy` on Linux:

```sh
cmake --preset ci-clang-tidy
cmake --build --preset ci-clang-tidy --parallel
ctest --preset ci-clang-tidy
```

Run the checks that cover your change before you commit. Let hosted CI run the
full system and compiler set. Run the full local package check when a change
affects packaging or when you prepare a release.

## Versions

`cmake/Version.cmake` sets the three-part version number. Development builds
add the current commit ID, such as `1.0.0-dev.g5c742bbe54fe`. Local changes add
`.dirty`. Without Git history, the build reports `dev.gunknown`.

Named releases get their full version from a Git tag, such as
`v1.0.0-alpha.0`. They require a clean checkout of that tag. See
[Release process](release.md) for the build option. Run CMake again after
changing commits so that the build records the current version.

The bridge protocol, multiplayer protocol, Home Box format, and ROM Assistant
API have separate versions. Do not change one only because the app version
changes.

## Change rules

- Give each commit one clear purpose.
- Use a Conventional Commit prefix such as `feat:`, `fix:`, `refactor:`,
  `test:`, `docs:`, `build:`, or `ci:`.
- Write the subject as a short command. Explain the reason in the body when it
  is not clear from the change.
- Do not mix code formatting, a dependency update, a protocol change, and a
  behavior change in one commit.
- Format only the C++ files that you change.
- Keep all supported builds working after each commit.
- Add or update tests when behavior changes.

## Release packages

The release presets are `release-windows-x64`, `release-macos-arm64`, and
`release-linux-x86_64`. See [Release process](release.md) for package commands,
GitHub Actions, and release tests.
