# Release process

This page explains how to build, test, and publish Emerald Rogue Assistant packages.

## Version

`cmake/Version.cmake` is the only source for the app version. It contains a
three-part number and an optional prerelease name.

CMake uses the full version in the UI, logs, command output, and package names.
Windows and macOS system fields use the three-part number when they require a
number. The bridge protocol, multiplayer protocol, Home Box format, and ROM
Assistant API have their own versions.

For version 1.0.0 beta 1, the release set is:

- `RogueAssistant-1.0.0-beta.1-windows-x64.zip`
- `RogueAssistant-1.0.0-beta.1-macos-arm64.zip`
- `RogueAssistant-1.0.0-beta.1-macos-arm64.dmg`
- `RogueAssistant-1.0.0-beta.1-linux-x86_64.AppImage`
- `RogueAssistant-1.0.0-beta.1-linux-x86_64.tar.gz`
- `THIRD_PARTY_NOTICES.md`
- `SHA256SUMS`

The macOS ZIP contains the same `.app` as the DMG. Every system package also
contains the README, public guides, third-party notices, and dependency license
files.

## Local package builds

Use only the preset for the current system.

On Windows:

```sh
cmake --preset release-windows-x64 --fresh -G Ninja
cmake --build --preset release-windows-x64 --parallel
ctest --preset release-windows-x64
cpack --preset release-windows-x64
```

On macOS:

```sh
cmake --preset release-macos-arm64 --fresh -G Ninja
cmake --build --preset release-macos-arm64 --parallel
ctest --preset release-macos-arm64
bash packaging/macos/package.sh build/release-macos-arm64 dist 1.0.0-beta.1
```

On Linux:

```sh
cmake --preset release-linux-x86_64 --fresh -G Ninja
cmake --build --preset release-linux-x86_64 --parallel
ctest --preset release-linux-x86_64
cpack --preset release-linux-x86_64
```

The Linux AppImage also needs the x86_64 linuxdeploy build named
`1-alpha-20251107-1`. Its SHA-256 value is
`c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d`.
The GitHub workflow downloads and checks this exact file.

All release presets treat project warnings as errors. The macOS preset targets
macOS 11 and builds only arm64. Check the result with:

```sh
lipo -archs build/release-macos-arm64/RogueAssistant.app/Contents/MacOS/RogueAssistant
```

The output must be `arm64`.

## GitHub Actions

The `Release packages` workflow supports two ways to run:

- Start it by hand to build test packages. This creates workflow artifacts but
  no GitHub release.
- Push a tag that matches `v<app version>`, such as `v1.0.0-beta.1`. This also
  creates or updates a draft GitHub release.

The workflow checks that the tag and CMake version match. It builds and tests
all three systems, checks each installed file set, creates package files, and
writes `SHA256SUMS`. A version with a prerelease name becomes a GitHub
prerelease. The workflow always leaves the GitHub release as a draft. A person
must check and publish it.

## macOS signing

The macOS job always signs the app. Without Apple credentials, it uses an ad
hoc signature. A Developer ID signature and notarization need these repository
secrets:

- `MACOS_CERTIFICATE_P12`
- `MACOS_CERTIFICATE_PASSWORD`
- `MACOS_SIGNING_IDENTITY`
- `APPLE_ID`
- `APPLE_TEAM_ID`
- `APPLE_APP_PASSWORD`

The three certificate values must be present together. The three notarization
values must also be present together. Notarization also requires the complete
certificate set. The job fails if only part of a set is present.

The job uses a temporary keychain and deletes it at the end. It checks the app
signature and DMG before it uploads them.

## Publish a beta

Before you create the beta tag:

1. Update `cmake/Version.cmake` and the release notes.
2. Confirm that the change is on the intended default-branch commit.
3. Confirm that normal hosted CI passes on that commit.
4. Complete the live mGBA checks that are possible on the current system.
5. Create and push a signed tag that exactly matches the app version.

After the release workflow finishes:

1. Confirm that every job passed.
2. Download the complete release artifact.
3. Check every file against `SHA256SUMS`.
4. Install and start the package for the system you can test.
5. Check its version, CPU type, resources, signature, ZIP, and installer image.
6. Read the draft notes and state all live test gaps.
7. Publish the draft as a prerelease.

Do not call an untested system or ROM edition verified. Beta releases exist so
other users can help close those gaps.

## Tests required for final 1.0.0

The normal test suite must pass with MSVC x64, Apple Clang, GCC, and Clang.
The sanitizer and Lua jobs must also pass. The Clang build includes static
analysis.

Automated tests cover:

- ROM Assistant API 3 checks for Vanilla and EX
- ROM headers, changing state, confirmation writes, Home Box, and multiplayer
- Request IDs, bad results, queue limits, and paused mGBA
- Split and joined bridge frames, partial sends, rejected peers, and reconnects
- Original Home Box file compatibility, file damage, backups, and interrupted
  writes
- Multiplayer compatibility and ROM layout checks
- Worker shutdown and UI thread boundaries
- Lua and C++ use of the same byte examples

The following live checks are also required:

- Start each package on a clean Windows x64, macOS arm64, and Linux x86_64
  system.
- Check resources, script export, settings, and user file paths on each system.
- Test Home Box load, save, old file import, and backup recovery with real game
  data.
- Test both host and client between Windows and macOS.
- Test one multiplayer pair that includes Linux.
- Check clear errors for a wrong protocol, ROM API, edition, player count, and
  ROM layout.
- On each system, test both start orders, pause and resume, ROM reset, script
  reload, app restart, mGBA shutdown, and app shutdown.

Record the app version, mGBA version, ROM version, ROM edition, ROM API,
operating system, CPU type, and result for each release candidate. Do not tag
the final release if a required check fails, mGBA crashes, data can be lost
without a warning, or a queue can grow without a limit.

## Current live test record

The September 2, 2026 macOS test used mGBA 0.10.5 and Emerald Rogue 2.2.0
Vanilla with ROM Assistant API 3. It checked both start orders, the ROM header
and confirmation write, a ten-second pause, reset, script reload, app restart,
mGBA shutdown, and app shutdown.

That test found an mGBA 0.10.5 Lua callback error. The script now returns no
top-level value, guards callbacks with `xpcall`, and handles start, reset,
shutdown, and stop events. Lua tests cover this fix.

The live test did not cover EX, Windows, Linux, Home Box file recovery, or
cross-platform multiplayer.

## Work after 1.0

These changes are outside the 1.0 release:

- Automatic local pairing for more than one mGBA session
- Tested support for ROM Assistant APIs 1 and 2
- A defined test process for future ROM Assistant API versions
- Linux arm64 packages
- Windows x86 packages if users need them
- Optional update checks
