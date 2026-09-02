# Development

Rogue Assistant uses CMake 3.25 or newer and C++20. The checked-in presets keep
local and continuous-integration builds aligned.

## Local build

```sh
cmake --preset dev-debug
cmake --build --preset dev-debug --parallel
ctest --preset dev-debug
```

`dev-release` provides the corresponding optimized build. Dependencies are
downloaded from checksum-pinned release archives during configuration.

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
