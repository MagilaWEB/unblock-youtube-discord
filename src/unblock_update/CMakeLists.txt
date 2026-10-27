set(NAME unblock_update)

# Sources are globbed, not listed: a new .cpp in this directory is picked up on
# the next build (CONFIGURE_DEPENDS re-globs at build time).
file(GLOB src_cpp CONFIGURE_DEPENDS "*.cpp")

add_executable(${NAME} ${src_cpp})

target_link_libraries(${NAME} PRIVATE ub_std win32)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})
