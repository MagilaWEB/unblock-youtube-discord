add_executable(test_helper
    test_helper.cpp
    ${CMAKE_SOURCE_DIR}/src/core/tests/tap_main.cpp
    ../zapret_helper.cpp
    ../net.cpp
    ../curl_client.cpp
)
target_link_libraries(test_helper PRIVATE Catch2::Catch2 ${CURL_LIBRARY} ws2_32)
target_include_directories(test_helper PRIVATE ${CURL_INCLUDE_DIR} ..)
target_compile_definitions(test_helper PRIVATE WIN32_LEAN_AND_MEAN CONSOLE)
target_compile_options(test_helper PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-global-constructors -Wno-c2y-extensions -Wno-header-hygiene -Wno-shadow-field-in-constructor -Wno-nonportable-system-include-path -Wno-newline-eof -Wno-language-extension-token -Wno-sign-conversion -Wno-dangling-else -Wno-undefined-func-template>
)
add_test(NAME test_helper COMMAND test_helper --reporter TAP)
