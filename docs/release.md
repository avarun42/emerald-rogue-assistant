# Release engineering

This page describes how to produce a release candidate. It does not authorize
publication or choose a license for the project.

## Version source and supported artifacts

`cmake/Version.cmake` is the single application-version source. CMake uses it
for executable metadata, `--version`, UI/log text, handshakes, bundle metadata,
and package names. Protocol and storage versions remain independent constants
and must not be changed merely because the application patch version changes.

The 1.0.0 release set is:

- `RogueAssistant-1.0.0-windows-x64.zip`
- `RogueAssistant-1.0.0-macos-arm64.zip`
- `RogueAssistant-1.0.0-macos-arm64.dmg`
- `RogueAssistant-1.0.0-linux-x86_64.AppImage`
- `RogueAssistant-1.0.0-linux-x86_64.tar.gz`
- `THIRD_PARTY_NOTICES.md`
- `SHA256SUMS`

The macOS ZIP is the `.app` bundle transport; the DMG is the normal user-facing
installer. Linux ARM64 and Windows x86 are not produced for 1.0.0.
Every platform package includes the README, complete `docs` tree, third-party
notices, and the applicable dependency license texts.

## Local release builds

Use the platform preset that matches the host:

```sh
cmake --preset release-windows-x64 --fresh -G Ninja
cmake --build --preset release-windows-x64 --parallel
ctest --preset release-windows-x64
cpack --preset release-windows-x64

cmake --preset release-macos-arm64 --fresh -G Ninja
cmake --build --preset release-macos-arm64 --parallel
ctest --preset release-macos-arm64
bash packaging/macos/package.sh build/release-macos-arm64 dist 1.0.0

cmake --preset release-linux-x86_64 --fresh -G Ninja
cmake --build --preset release-linux-x86_64 --parallel
ctest --preset release-linux-x86_64
cpack --preset release-linux-x86_64
```

The Linux AppImage script additionally requires the immutable linuxdeploy
`1-alpha-20251107-1` x86_64 artifact. Its expected SHA-256 is
`c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d`.
The release workflow downloads and verifies that exact input before invoking
`packaging/linux/build-appimage.sh`.

All release presets enable warnings as errors and tests. The macOS preset sets
the deployment target to macOS 11 and builds arm64 only. Confirm with:

```sh
lipo -archs build/release-macos-arm64/RogueAssistant.app/Contents/MacOS/RogueAssistant
```

MSVC builds use the static C/C++ runtime because the Windows artifact is a
self-contained ZIP rather than an installer. Keep every statically linked
Windows target on the same runtime setting.

## GitHub Actions flow

`.github/workflows/release.yml` can be run manually to obtain development
artifacts without creating a GitHub release. A tag matching the exact CMake
version (`v1.0.0`) builds all three platforms, verifies their install trees,
assembles checksums, and creates or updates a **draft** GitHub release. It never
publishes the draft.

The macOS job always signs the bundle. Without credentials it uses ad-hoc
signing. Signed and notarized artifacts require all of these repository secrets:

- `MACOS_CERTIFICATE_P12`: base64-encoded Developer ID Application certificate
- `MACOS_CERTIFICATE_PASSWORD`: password protecting that P12
- `MACOS_SIGNING_IDENTITY`: the Developer ID Application identity
- `APPLE_ID`: notarization Apple ID
- `APPLE_TEAM_ID`: Apple Developer team ID
- `APPLE_APP_PASSWORD`: app-specific password for `notarytool`

Supplying an incomplete signing or notarization set fails the job instead of
quietly emitting a partially claimed artifact. The CI keychain is temporary.

Direct build dependencies are checksum-pinned. GitHub Actions are pinned to
immutable commits, and the AppImage builder is pinned by both release tag and
checksum. A manual workflow run therefore remains a repeatable way to produce
unsigned/ad-hoc development artifacts when release credentials are unavailable.
Release checksums describe the exact outputs; ZIP and DMG container metadata
means byte-for-byte identity is not promised across different runner images.

## Mandatory release gate

Before creating the version tag:

1. Complete every automated and manual item in [parity-gate.md](parity-gate.md),
   including packaged clean-machine starts and the Windows/macOS/Linux
   multiplayer pairings.
2. Resolve every item in [asset-provenance.md](asset-provenance.md), add the
   owner-selected project license, and review third-party notices against the
   actual packaged libraries.
3. Confirm `RogueAssistant --version`, application metadata, protocol hellos,
   package names, and the intended tag all agree.
4. Inspect the Windows executable signature, the arm64 Mach-O architecture,
   macOS signing/notarization/stapling, and AppImage contents.
5. Install each artifact on a clean supported machine or VM, run the complete
   acceptance matrix, and record versions and results in the release notes.
6. Verify `SHA256SUMS` from a fresh download of the draft assets.

Only the repository owner should then publish the draft. Do not tag `v1.0.0`
while any gate remains open: a tag is treated as a release-candidate assertion,
not as a substitute for validation.
