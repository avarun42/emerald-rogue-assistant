include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)
set(SFML_USE_SYSTEM_DEPS OFF CACHE BOOL "" FORCE)

set(ROGUE_SFML_PATCH_ARGUMENTS)
if(APPLE)
    # SFML 3.1.0 has Retina coordinate conversion internally but disables the
    # high-resolution OpenGL surface that Emerald Rogue Assistant's scaled UI needs.
    list(
        APPEND ROGUE_SFML_PATCH_ARGUMENTS
        PATCH_COMMAND
            "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=<SOURCE_DIR>"
            -P "${CMAKE_CURRENT_LIST_DIR}/patches/EnableSfmlMacosHighDpi.cmake"
    )
endif()

FetchContent_Declare(
    SFML
    URL https://github.com/SFML/SFML/archive/refs/tags/3.1.0.tar.gz
    URL_HASH SHA256=91209a112c2bd0bc6f4ce0d5f3e413cfb48b57c0de59f5507dc81f71b1ad7a5c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    ${ROGUE_SFML_PATCH_ARGUMENTS}
)
unset(ROGUE_SFML_PATCH_ARGUMENTS)

# ENet 1.3.17 still advertises compatibility with CMake 2.6. CMake 4 needs an
# explicit policy floor while evaluating that third-party project.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
FetchContent_Declare(
    ENet
    URL https://github.com/lsalzman/enet/archive/refs/tags/v1.3.17.tar.gz
    URL_HASH SHA256=1e0b4bc0b7127a2d779dd7928f0b31830f5b3dcb7ec9588c5de70033e8d2434a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(SFML ENET)

# Do not treat warnings from SFML's headers as errors in our code.
set_target_properties(sfml-system sfml-window sfml-graphics PROPERTIES SYSTEM TRUE)
