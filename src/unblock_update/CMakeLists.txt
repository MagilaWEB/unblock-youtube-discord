set(NAME unblock_update)

file(GLOB src_cpp "*.cpp")

add_executable(${NAME} ${src_cpp})

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})