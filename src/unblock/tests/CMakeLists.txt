add_executable(test_unblock
    test_strategies_zapret1.cpp
    ${CMAKE_SOURCE_DIR}/src/core/tests/tap_main.cpp
    ../strategies_zapret1.cpp
    ../strategies_zapret2.cpp
    ../strategy_config_base.cpp
    ../strategy_generator.cpp
)
target_link_libraries(test_unblock PRIVATE Catch2::Catch2 core)
target_include_directories(test_unblock PRIVATE ..)
target_precompile_headers(test_unblock PRIVATE ../pch.h)
target_compile_definitions(test_unblock PRIVATE WIN32_LEAN_AND_MEAN CONSOLE UNBLOCK_TESTS)
target_compile_options(test_unblock PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-global-constructors -Wno-c2y-extensions -Wno-header-hygiene -Wno-shadow-field-in-constructor -Wno-nonportable-system-include-path -Wno-newline-eof -Wno-language-extension-token -Wno-sign-conversion -Wno-dangling-else -Wno-undefined-func-template>
)
# The strategy builders resolve real repo paths (configs/, binaries/) through
# Core, so the test runs with the repository root as the working directory.
add_test(NAME test_unblock COMMAND test_unblock --reporter TAP WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
