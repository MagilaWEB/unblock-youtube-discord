set(NAME unblock_dns)

add_executable(${NAME}
    unblock_dns.cpp
    ag_bind.h
    ag_dns.h
    dns_config.h
)
target_link_libraries(${NAME} PRIVATE ws2_32 iphlpapi crypt32)
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
