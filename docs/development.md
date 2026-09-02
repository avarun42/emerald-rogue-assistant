# Development

The required release-validation matrix is documented in [parity-gate.md](parity-gate.md).

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

Only touched C++ files should be formatted. Repository-wide formatting and
dependency upgrades belong in separate changes.
