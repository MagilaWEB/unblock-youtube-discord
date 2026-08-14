add_executable(test_helper
    test_helper.cpp
    test_net.cpp
    ${CMAKE_SOURCE_DIR}/src/core/tests/tap_main.cpp
    ../zapret_helper.cpp
    ../net.cpp
    ../curl_client.cpp
)
target_link_libraries(test_helper PRIVATE Catch2::Catch2 ${CURL_LIBRARY} ws2_32)
target_include_directories(test_helper PRIVATE ${CURL_INCLUDE_DIR} ..)
target_compile_definitions(test_helper PRIVATE WIN32_LEAN_AND_MEAN CONSOLE HELPER_TESTS)
target_compile_options(test_helper PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-global-constructors -Wno-c2y-extensions -Wno-header-hygiene -Wno-shadow-field-in-constructor -Wno-nonportable-system-include-path -Wno-newline-eof -Wno-language-extension-token -Wno-sign-conversion -Wno-dangling-else -Wno-undefined-func-template>
)
add_custom_command(TARGET test_helper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${CURL_DIR_DEBUG}/bin/libcurl-d.dll,${CURL_DIR_RELEASE}/bin/libcurl.dll>"
        $<TARGET_FILE_DIR:test_helper>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${CURL_DIR_DEBUG}/bin/zlibd1.dll,${CURL_DIR_RELEASE}/bin/zlib1.dll>"
        $<TARGET_FILE_DIR:test_helper>
)
add_test(NAME test_helper COMMAND test_helper --reporter TAP)
