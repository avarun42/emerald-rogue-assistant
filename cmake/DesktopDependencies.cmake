include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)
set(SFML_USE_SYSTEM_DEPS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SFML
    URL https://github.com/SFML/SFML/archive/refs/tags/2.6.2.tar.gz
    URL_HASH SHA256=15ff4d608a018f287c6a885db0a2da86ea389e516d2323629e4d4407a7ce047f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

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

# SFML 2.6 intentionally remains a parity dependency until the isolated SFML 3
# migration. Treat its public headers as third-party so current libc++ warnings
# do not become Rogue Assistant project errors.
set_target_properties(sfml-system sfml-window sfml-graphics PROPERTIES SYSTEM TRUE)
