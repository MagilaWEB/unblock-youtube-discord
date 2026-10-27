set(NAME dom)

# Sources are globbed, not listed: a new .cpp/.cppm in this directory is picked
# up on the next build (CONFIGURE_DEPENDS re-globs at build time).
file(GLOB src_cpp CONFIGURE_DEPENDS "*.cpp")
file(GLOB src_cppm CONFIGURE_DEPENDS "*.cppm")

add_library(${NAME} STATIC ${src_cpp})

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES ${src_cppm}
)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

# ext PUBLIC: saucer::smartview* leaks into the module interface (dom.view).
# core PUBLIC: utils_saucer (js::Value/jsToCpp) uses s64/concepts/Debug.
target_link_libraries(${NAME} PUBLIC ext core)

target_compile_definitions(${NAME} PRIVATE ${ALL_MACRO_CPP})

if (BUILD_TESTS)
  add_subdirectory(tests)
endif ()
