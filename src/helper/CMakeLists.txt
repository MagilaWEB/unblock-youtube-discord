set(NAME helper)

add_executable(${NAME} main.cpp)
target_link_libraries(${NAME} PRIVATE ${CURL_LIBRARY} ws2_32)
target_include_directories(${NAME} PRIVATE ${CURL_INCLUDE_DIR})
target_compile_definitions(${NAME} PRIVATE CONSOLE)
target_compile_options(${NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror
	-Wno-padded -Wno-exit-time-destructors -Wno-unsafe-buffer-usage
)

add_custom_command(TARGET ${NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${NAME}> ${CMAKE_SOURCE_DIR}/binaries/zapret-helper.exe
	COMMENT "Copying zapret-helper.exe to binaries/"
)
