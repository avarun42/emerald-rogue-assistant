add_executable(
    rogue_tests
    BridgeProtocolTests.cpp
    CharacterizationTests.cpp
    CommandLineTests.cpp
    GameBehaviourTests.cpp
    HomeBoxFileTests.cpp
    LifecycleTests.cpp
    MultiplayerProtocolTests.cpp
    PlatformTests.cpp
    TcpLuaTransportTests.cpp
    TransportTests.cpp
    UiTextTests.cpp
)

target_compile_definitions(
    rogue_tests
    PRIVATE
        ROGUE_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        ROGUE_TEST_RESOURCE_DIR="${PROJECT_SOURCE_DIR}/RogueAssistantCpp/Assets"
)

target_link_libraries(
    rogue_tests
    PRIVATE
        rogue_app
        rogue_project_warnings
        Catch2::Catch2WithMain
        sfml-graphics
)
rogue_enable_clang_tidy(rogue_tests)

include(Catch)
catch_discover_tests(rogue_tests)
