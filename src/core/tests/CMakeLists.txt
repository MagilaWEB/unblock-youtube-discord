add_executable(test_file_system
    test_file_system.cpp
)

target_link_libraries(test_file_system PRIVATE
    Catch2::Catch2WithMain
    core
)

target_compile_options(test_file_system PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-global-constructors -Wno-c2y-extensions -Wno-header-hygiene -Wno-shadow-field-in-constructor -Wno-nonportable-system-include-path -Wno-newline-eof -Wno-language-extension-token -Wno-sign-conversion -Wno-dangling-else -Wno-undefined-func-template>
)

add_test(NAME test_file_system COMMAND test_file_system)
