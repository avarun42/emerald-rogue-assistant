cmake_minimum_required(VERSION 3.25)

project(
    RogueAssistant
    VERSION 1.0.0
    DESCRIPTION "Cross-platform companion application for Pokemon Emerald Rogue"
    LANGUAGES CXX
)

include(CTest)
find_package(Threads REQUIRED)

option(ROGUE_BUILD_TESTS "Build Rogue Assistant tests" ${BUILD_TESTING})
option(ROGUE_WARNINGS_AS_ERRORS "Treat Rogue Assistant warnings as errors" OFF)
option(ROGUE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ROGUE_ENABLE_CLANG_TIDY "Run clang-tidy on Rogue Assistant targets" OFF)

add_library(rogue_project_options INTERFACE)
target_compile_features(rogue_project_options INTERFACE cxx_std_20)

add_library(rogue_project_warnings INTERFACE)
if(MSVC)
    target_compile_options(rogue_project_warnings INTERFACE /W4 /permissive-)
    if(ROGUE_WARNINGS_AS_ERRORS)
        target_compile_options(rogue_project_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(rogue_project_warnings INTERFACE -Wall -Wextra -Wpedantic)
    if(ROGUE_WARNINGS_AS_ERRORS)
        target_compile_options(rogue_project_warnings INTERFACE -Werror)
    endif()
endif()

add_library(rogue_sanitizers INTERFACE)
if(ROGUE_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            rogue_sanitizers
            INTERFACE
                -fno-omit-frame-pointer
                -fsanitize=address,undefined
        )
        target_link_options(rogue_sanitizers INTERFACE -fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "ROGUE_ENABLE_SANITIZERS requires GCC or Clang")
    endif()
endif()

if(ROGUE_ENABLE_CLANG_TIDY)
    find_program(ROGUE_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
endif()

function(rogue_enable_clang_tidy target)
    if(ROGUE_ENABLE_CLANG_TIDY)
        set_property(
            TARGET ${target}
            PROPERTY CXX_CLANG_TIDY
                "${ROGUE_CLANG_TIDY_EXECUTABLE};--config-file=${PROJECT_SOURCE_DIR}/.clang-tidy"
        )
    endif()
endfunction()

configure_file(
    cmake/RogueAssistantVersion.h.in
    generated/RogueAssistantVersion.h
    @ONLY
)

add_library(
    rogue_core
    STATIC
        RogueAssistantCpp/Src/DataStream.cpp
        RogueAssistantCpp/Src/GameData.cpp
        RogueAssistantCpp/Src/Log.cpp
        RogueAssistantCpp/Src/Platform/AppPaths.cpp
        RogueAssistantCpp/Src/Platform/Configuration.cpp
        RogueAssistantCpp/Src/Platform/FileSystem.cpp
        RogueAssistantCpp/Src/Platform/ResourceLocator.cpp
        RogueAssistantCpp/Src/Platform/Utf8.cpp
        RogueAssistantCpp/Src/Timer.cpp
        RogueAssistantCpp/Src/UserData.cpp
)
target_include_directories(
    rogue_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/RogueAssistantCpp/Inc
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(
    rogue_core
    PUBLIC
        rogue_project_options
        rogue_sanitizers
    PRIVATE
        rogue_project_warnings
)
rogue_enable_clang_tidy(rogue_core)

add_library(
    rogue_bridge
    STATIC
        RogueAssistantCpp/Src/Bridge/GameMemoryTransport.cpp
        RogueAssistantCpp/Src/Bridge/GameSession.cpp
        RogueAssistantCpp/Src/Bridge/NativeLuaTransport.cpp
)
target_include_directories(rogue_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/RogueAssistantCpp/Inc)
target_link_libraries(
    rogue_bridge
    PUBLIC
        rogue_core
        rogue_project_options
        rogue_sanitizers
    PRIVATE
        rogue_project_warnings
)
rogue_enable_clang_tidy(rogue_bridge)

add_library(
    rogue_app
    STATIC
        RogueAssistantCpp/Src/Application/Application.cpp
        RogueAssistantCpp/Src/Application/SessionWorker.cpp
)
target_include_directories(rogue_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/RogueAssistantCpp/Inc)
target_link_libraries(
    rogue_app
    PUBLIC
        rogue_bridge
        rogue_project_options
        rogue_sanitizers
        Threads::Threads
    PRIVATE
        rogue_project_warnings
)
rogue_enable_clang_tidy(rogue_app)

set(
    ROGUE_RESOURCE_FILES
    RogueAssistantCpp/Assets/WobbuffetImage.png
    RogueAssistantCpp/Assets/poketch_frame.png
    RogueAssistantCpp/Assets/pokemon-emerald-pro.ttf
    RogueAssistantCpp/Assets/RogueAssistant_mGBA.lua
)
install(FILES ${ROGUE_RESOURCE_FILES} DESTINATION resources)

if(ROGUE_BUILD_TESTS)
    include(cmake/Dependencies.cmake)
    add_subdirectory(tests)
endif()
