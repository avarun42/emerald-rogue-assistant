# Development

The [Parity gate](parity-gate.md) defines the required release-validation
matrix.

Rogue Assistant uses CMake 3.25 or newer and C++20. The checked-in presets keep
local and continuous-integration builds aligned.

## Local build

```sh
cmake --preset dev-debug
cmake --build --preset dev-debug --parallel
ctest --preset dev-debug
```

`dev-release` provides the corresponding optimized build. Direct dependencies
are downloaded from checksum-pinned release archives during configuration.
The desktop target builds SFML 3.1.0 and ENet 1.3.18 from source; SFML's own
transitive sources follow the revisions pinned by its release build.

The resulting executable is `RogueAssistant.exe` on Windows, `RogueAssistant`
on Linux, and `RogueAssistant.app` on macOS. A smoke check does not create a
window:

```sh
RogueAssistant --version
RogueAssistant --help
```

Linux source builds require development packages for FreeType, OpenGL, udev,
X11, Xcursor, Xext, Xi, and Xrandr. CI installs these explicitly.

Platform release builds use `release-windows-x64`,
`release-macos-arm64`, and `release-linux-x86_64`. Their archive and signing
workflow is documented in [Release engineering](release.md). Application
SemVer has one source in `cmake/Version.cmake`. That file separates the numeric
version core from an optional prerelease identifier because native operating
system metadata requires a numeric core. Bridge, multiplayer, storage, and ROM
API versions remain independent.

## Quality gates

Every review unit must build and pass its applicable tests before it is
committed. CI runs the portable targets with MSVC x64, Apple Clang, GCC, and
Clang. Project warnings are errors in all four jobs. Separate Linux jobs run
AddressSanitizer plus UndefinedBehaviorSanitizer and clang-tidy.

The sanitizer and analyzer presets can also be run locally:

```sh
cmake --preset ci-sanitizers
cmake --build --preset ci-sanitizers --parallel
ctest --preset ci-sanitizers

cmake --preset ci-clang-tidy
cmake --build --preset ci-clang-tidy --parallel
ctest --preset ci-clang-tidy
```

Format only the C++ files that you change. Keep repository-wide formatting and
dependency upgrades in separate changes.

## Technical writing

Follow the [Google developer documentation style guide](https://developers.google.com/style)
for first-party documentation, application text, command-line output, log
messages, code comments, and mGBA console messages. Do not edit quoted or
vendored third-party license text.

- Use US English, active voice, present tense, and sentence-case headings.
- Address the reader as "you" and use direct commands for instructions.
- Describe the visible state or the action that the user must take.
- Prefer short sentences and familiar, consistent terms.
- Do not expose internal architecture terms unless the user needs them to fix
  a problem.
- Write relationships as words when shorthand can be ambiguous. For example,
  write `on port 30125`, not `:30125`.
- Put detailed protocol and storage diagnostics in the log. Show a concise
  summary and a useful next action in the UI.

## Distribution review

`THIRD_PARTY_NOTICES.md` is installed with every application. Keep its versions
aligned with `cmake/DesktopDependencies.cmake` and SFML's transitive revisions.
The original Rogue Assistant creator has approved this fork and its GitHub
releases. Keep that release permission distinct from any general repository
license added later. See [Distribution status](asset-provenance.md).
