add_executable(test_unblock
    test_strategies_zapret1.cpp
    test_auto_round.cpp
    test_dns_pins.cpp
    test_update_version.cpp
    ${CMAKE_SOURCE_DIR}/src/core/tests/tap_main.cpp
)
target_link_libraries(test_unblock PRIVATE Catch2::Catch2 unblock core ext ws2_32)
target_compile_definitions(test_unblock PRIVATE WIN32_LEAN_AND_MEAN CONSOLE UNBLOCK_TESTS)
target_compile_options(test_unblock PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-global-constructors -Wno-c2y-extensions -Wno-header-hygiene -Wno-shadow-field-in-constructor -Wno-nonportable-system-include-path -Wno-newline-eof -Wno-language-extension-token -Wno-sign-conversion -Wno-dangling-else -Wno-undefined-func-template>
)
# The strategy builders resolve real repo paths (configs/, binaries/) through
# Core, so the test runs with the repository root as the working directory.
add_test(NAME test_unblock COMMAND test_unblock --reporter TAP WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
