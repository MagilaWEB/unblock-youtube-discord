set(NAME core)

add_library(${NAME} STATIC)

target_sources(${NAME}
  PRIVATE
    core.cpp
    debug.cpp
    file_system.cpp
    hidden_process.cpp
    input_console.cpp
    localization.cpp
    lock_thread.cpp
    scheduler.cpp
    service.cpp
    thread_pool.cpp
    timer.cpp
    utils.cpp
)

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES
      concepts.cppm
      core.cppm
      debug.cppm
      file_system.cppm
      hidden_process.cppm
      input_console.cppm
      localization.cppm
      lock_thread.cppm
      macro.cppm
      ptr.cppm
      scheduler.cppm
      service.cppm
      thread_pool.cppm
      timer.cppm
      types.cppm
      utils.cppm
      virtual_key_codes.cppm
)

# Link

target_link_libraries(${NAME} PUBLIC ub_std)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

target_compile_definitions(${NAME} PRIVATE ${ALL_MACRO_CPP})

if (BUILD_TESTS)
  add_subdirectory(tests)
endif ()
