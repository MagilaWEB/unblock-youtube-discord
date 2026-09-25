add_executable(test_unblock_dns
    test_dns_config.cpp
    ${CMAKE_SOURCE_DIR}/src/core/tests/tap_main.cpp
)
target_link_libraries(test_unblock_dns PRIVATE Catch2::Catch2 core)
target_include_directories(test_unblock_dns PRIVATE ..)
target_compile_definitions(test_unblock_dns PRIVATE WIN32_LEAN_AND_MEAN CONSOLE UNBLOCK_DNS_TESTS)
target_compile_options(test_unblock_dns PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-global-constructors -Wno-c2y-extensions -Wno-header-hygiene -Wno-shadow-field-in-constructor -Wno-nonportable-system-include-path -Wno-newline-eof -Wno-language-extension-token -Wno-sign-conversion -Wno-dangling-else -Wno-undefined-func-template>
)
add_test(NAME test_unblock_dns COMMAND test_unblock_dns --reporter TAP)
