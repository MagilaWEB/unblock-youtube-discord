set(NAME zapret_helper)

file(GLOB src_cpp "*.cpp")
file(GLOB src_h "*.h")
file(GLOB src_hpp "*.hpp")

add_executable(${NAME} ${src_h} ${src_hpp} ${src_cpp})
target_link_libraries(${NAME} PRIVATE ${CURL_LIBRARY} ws2_32)
target_include_directories(${NAME} PRIVATE ${CURL_INCLUDE_DIR})
target_compile_definitions(${NAME} PRIVATE CONSOLE WIN32_LEAN_AND_MEAN)
target_compile_options(${NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror
	-Wno-padded -Wno-exit-time-destructors -Wno-unsafe-buffer-usage
)

if (BUILD_TESTS)
  target_compile_definitions(${NAME} PRIVATE HELPER_TESTS)
  add_subdirectory(tests)
endif ()
