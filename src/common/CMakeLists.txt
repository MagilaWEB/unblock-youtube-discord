set(NAME common)

# Sources are globbed, not listed: a new .cpp/.cppm in this directory is picked
# up on the next build (CONFIGURE_DEPENDS re-globs at build time).
file(GLOB src_cpp CONFIGURE_DEPENDS "*.cpp")
file(GLOB src_cppm CONFIGURE_DEPENDS "*.cppm")

add_library(${NAME} STATIC ${src_cpp})

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES ${src_cppm}
)

# Link

target_link_libraries(${NAME} PUBLIC ub_std)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

target_compile_definitions(${NAME} PRIVATE ${ALL_MACRO_CPP})
