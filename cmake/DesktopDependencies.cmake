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

FetchContent_Declare(
    ENet
    URL https://github.com/lsalzman/enet/archive/refs/tags/v1.3.18.tar.gz
    URL_HASH SHA256=28603c895f9ed24a846478180ee72c7376b39b4bb1287b73877e5eae7d96b0dd
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(SFML ENET)

# Do not treat warnings from SFML's headers as errors in our code.
set_target_properties(sfml-system sfml-window sfml-graphics PROPERTIES SYSTEM TRUE)
