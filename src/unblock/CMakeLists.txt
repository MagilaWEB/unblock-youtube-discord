set(NAME unblock)

add_library(${NAME} STATIC)

target_sources(${NAME}
  PRIVATE
    app_update.cpp
    auto_strategy_runner.cpp
    dns_host.cpp
    dns_proxy.cpp
    domain_testing.cpp
    helper_state.cpp
    http_load_content.cpp
    ipc_signals.cpp
    strategies_zapret1.cpp
    strategies_zapret2.cpp
    strategy_config_base.cpp
    strategy_generator.cpp
    tg_proxy.cpp
    unblock.cpp
    update_version.cpp
    zapret1_engine.cpp
    zapret2_engine.cpp
)

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES
      app_update.cppm
      auto_strategy_runner.cppm
      constants.cppm
      dns_host.cppm
      dns_proxy.cppm
      domain_testing.cppm
      helper_state.cppm
      http_load_content.cppm
      ipc_signals.cppm
      strategies_zapret1.cppm
      strategies_zapret2.cppm
      strategy_config_base.cppm
      strategy_generator.cppm
      tg_proxy.cppm
      unblock.cppm
      update_version.cppm
      zapret1_engine.cppm
      zapret2_engine.cppm
      zapret_engine.cppm
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
