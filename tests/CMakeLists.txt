add_executable(
    rogue_tests
    CharacterizationTests.cpp
    LifecycleTests.cpp
    PlatformTests.cpp
    TransportTests.cpp
)

target_link_libraries(
    rogue_tests
    PRIVATE
        rogue_app
        rogue_project_warnings
        Catch2::Catch2WithMain
)
rogue_enable_clang_tidy(rogue_tests)

include(Catch)
catch_discover_tests(rogue_tests)
