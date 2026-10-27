set(NAME helper)

# Sources are globbed, not listed: a new .cpp/.cppm in this directory is picked
# up on the next build (CONFIGURE_DEPENDS re-globs at build time).
file(GLOB src_cpp CONFIGURE_DEPENDS "*.cpp")
file(GLOB src_cppm CONFIGURE_DEPENDS "*.cppm")

# main.cpp belongs to the zapret_helper executable, not to the library.
list(FILTER src_cpp EXCLUDE REGEX "/main\\.cpp$")

add_library(${NAME} STATIC ${src_cpp})

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES ${src_cppm}
)

# ext PUBLIC: curl comes from the ext.curl wrapper; ub_std is the std module.
target_link_libraries(${NAME} PUBLIC ub_std ext ws2_32 win32)

target_compile_definitions(${NAME} PRIVATE CONSOLE WIN32_LEAN_AND_MEAN)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

set(NAME zapret_helper)

add_executable(${NAME} main.cpp)

target_link_libraries(${NAME} PRIVATE helper)

target_compile_definitions(${NAME} PRIVATE CONSOLE WIN32_LEAN_AND_MEAN)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

if (BUILD_TESTS)
  target_compile_definitions(helper PRIVATE HELPER_TESTS)
  add_subdirectory(tests)
endif ()
