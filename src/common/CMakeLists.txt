set(NAME common)

add_library(${NAME} STATIC)

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES
      dns_config.cppm
)

# Link

target_link_libraries(${NAME} PUBLIC ub_std)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

target_compile_definitions(${NAME} PRIVATE ${ALL_MACRO_CPP})
