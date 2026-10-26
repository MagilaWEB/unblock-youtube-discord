set(NAME helper)

add_library(${NAME} STATIC)

target_sources(${NAME}
  PRIVATE
    curl_client.cpp
    helper_config.cpp
    net.cpp
    zapret_helper.cpp
)

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES
      curl_client.cppm
      helper_config.cppm
      net.cppm
      types.cppm
      zapret_helper.cppm
)

# ext PUBLIC: curl comes from the ext.curl wrapper; ub_std is the std module.
target_link_libraries(${NAME} PUBLIC ub_std ext ws2_32)

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
