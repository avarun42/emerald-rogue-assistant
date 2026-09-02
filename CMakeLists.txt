cmake_minimum_required(VERSION 3.25)

project(
    RogueAssistant
    VERSION 1.0.0
    DESCRIPTION "Cross-platform companion application for Pokemon Emerald Rogue"
    LANGUAGES CXX
)

include(CTest)

option(ROGUE_BUILD_TESTS "Build Rogue Assistant tests" ${BUILD_TESTING})
option(ROGUE_WARNINGS_AS_ERRORS "Treat Rogue Assistant warnings as errors" OFF)

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
        RogueAssistantCpp/Src/Timer.cpp
)
target_include_directories(
    rogue_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/RogueAssistantCpp/Inc
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(rogue_core PUBLIC rogue_project_options PRIVATE rogue_project_warnings)

if(ROGUE_BUILD_TESTS)
    include(cmake/Dependencies.cmake)
    add_subdirectory(tests)
endif()
