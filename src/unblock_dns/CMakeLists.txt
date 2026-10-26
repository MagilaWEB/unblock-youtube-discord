set(NAME unblock_dns)

add_executable(${NAME}
    unblock_dns.cpp
    dns_adapters.cpp
    dns_proxy.cpp
    dns_telemetry.cpp
    dns_util.cpp
)

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES
      dns_adapters.cppm
      dns_proxy.cppm
      dns_telemetry.cppm
      dns_util.cppm
)

# common is a module (dns_config); ub_std is the std module.
target_link_libraries(${NAME} PRIVATE common ub_std ws2_32 iphlpapi crypt32)

target_compile_definitions(${NAME} PRIVATE CONSOLE WIN32_LEAN_AND_MEAN)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

# The runtime loads AdguardDns64.dll from its own directory (bin/).
if(UNBLOCK_DNS_DLL_PATH)
  add_custom_command(TARGET ${NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${UNBLOCK_DNS_DLL_PATH}" "$<TARGET_FILE_DIR:${NAME}>/AdguardDns64.dll"
    COMMENT "Copying AdguardDns64.dll"
  )
endif()

if (BUILD_TESTS)
  add_subdirectory(tests)
endif ()
