set(NAME unblock_update)

add_executable(${NAME} main.cpp)

target_link_libraries(${NAME} PRIVATE ub_std)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})
