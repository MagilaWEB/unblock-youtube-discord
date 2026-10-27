set(NAME unblock)

# Sources are globbed, not listed: a new .cpp/.cppm in this directory is picked
# up on the next build (CONFIGURE_DEPENDS re-globs at build time).
file(GLOB src_cpp CONFIGURE_DEPENDS "*.cpp")
file(GLOB src_cppm CONFIGURE_DEPENDS "*.cppm")

add_library(${NAME} STATIC ${src_cpp})

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES ${src_cppm}
)

# ext PUBLIC: curl/bit7z come from the ext wrappers; core PUBLIC: File/Service
# and the core types leak into the module interface; ub_std is the std module.
target_link_libraries(${NAME} PUBLIC ub_std ext core ub_version)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

target_compile_definitions(${NAME} PRIVATE ${ALL_MACRO_CPP})

if (BUILD_TESTS)
  target_compile_definitions(${NAME} PRIVATE UNBLOCK_TESTS)
  add_subdirectory(tests)
endif ()
