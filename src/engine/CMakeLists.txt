set(NAME engine)

set(CMAKE_CXX_EXTENSIONS OFF)

if(CUPY_LIBS_DLL_TO_BIN)
    # Full-rebuild cleanup. These run at configure time (i.e. a full rebuild in
    # the IDE triggers a reconfigure), NOT on every incremental ninja run.
    # Otherwise incremental builds would wipe bin/ and binaries/ and leave the
    # unchanged DLLs/exes missing until the next full relink.
    file(REMOVE_RECURSE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    file(REMOVE ${CMAKE_SOURCE_DIR}/binaries/winws2.exe)
    file(REMOVE ${CMAKE_SOURCE_DIR}/binaries/winws.exe)
    file(REMOVE ${CMAKE_SOURCE_DIR}/binaries/cygwin1.dll)
    file(REMOVE
        ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-lib.lua
        ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-auto.lua
        ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-antidpi.lua
        ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-obfs.lua
        ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-pcap.lua
        ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-tests.lua
    )
endif()

# The version module lives in its own library: version.hpp is generated during
# the build (target unblock_version), so the module interface includes it
# textually, and ui/unblock/engine import the module from here. An executable
# cannot hand out module files, hence a static library.
add_library(ub_version STATIC)

target_sources(ub_version
  PUBLIC
    FILE_SET CXX_MODULES FILES version.cppm
)

target_link_libraries(ub_version PUBLIC ub_std)
target_compile_options(ub_version PRIVATE ${PROJECT_WARNING_FLAGS})
add_dependencies(ub_version unblock_version)

# The engine interface (IEngineAPI) is implemented by the engine and consumed by
# ui. `engine` is an executable, and an executable cannot hand out module files,
# so the interface lives in its own library - same reason as the version module
# above.
add_library(ub_engine_api STATIC)

target_sources(ub_engine_api
  PUBLIC
    FILE_SET CXX_MODULES FILES api.cppm
)

# ext/core PUBLIC: saucer::smartview, saucer::window and File appear in the
# exported declarations.
target_link_libraries(ub_engine_api PUBLIC ub_std ext core)
target_compile_options(ub_engine_api PRIVATE ${PROJECT_WARNING_FLAGS})

# The .cpp files are globbed, but the module files are not: this directory holds
# the interface units of three targets (ub_version, ub_engine_api and the engine
# itself), and a glob cannot tell which unit belongs to which target.
file(GLOB src_cpp CONFIGURE_DEPENDS "*.cpp")

add_executable(${NAME} ${src_cpp} engine.rc)

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES
      engine.cppm
      window_geometry.cppm
)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

# Link
target_link_libraries(${NAME} PRIVATE
        core
        ui
        unblock
        ext
        ub_version
        ub_engine_api
        saucer::saucer
        win32
        dwmapi.lib
        shcore.lib
)

target_compile_definitions(${NAME} PRIVATE CONSOLE ${ALL_MACRO_CPP})

target_link_options(${NAME} PRIVATE
        "LINKER:/ENTRY:mainCRTStartup"
        "LINKER:/SUBSYSTEM:WINDOWS"
    )

# Copy library binaries to the folder of the compiled project
if(CUPY_LIBS_DLL_TO_BIN)
    add_custom_command(TARGET ${NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${7ZA_BIN} $<TARGET_FILE_DIR:${NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/engine/unblock.ico"
            $<TARGET_FILE_DIR:${NAME}>
        COMMENT "Copying runtime DLLs to output directory"

        COMMAND ${CMAKE_COMMAND} -E copy
            ${ZAPRET2_BIN}/winws2.exe ${CMAKE_SOURCE_DIR}/binaries || ${CMAKE_COMMAND} -E echo "Warning: failed to copy winws2.exe (maybe in use)"
        COMMAND ${CMAKE_COMMAND} -E copy
            ${ZAPRET1_BIN}/winws.exe ${CMAKE_SOURCE_DIR}/binaries || ${CMAKE_COMMAND} -E echo "Warning: failed to copy winws.exe (maybe in use)"
        COMMAND ${CMAKE_COMMAND} -E copy
            ${ZAPRET2_BIN}/cygwin1.dll ${CMAKE_SOURCE_DIR}/binaries || ${CMAKE_COMMAND} -E echo "Warning: failed to copy cygwin1.dll (maybe in use)"
        COMMAND ${CMAKE_COMMAND} -E copy_directory 
            ${ZAPRET2_LUA}/ ${CMAKE_SOURCE_DIR}/binaries/lua
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/resources/lua/unblock_auto_strategy.lua ${CMAKE_SOURCE_DIR}/binaries/lua/unblock_auto_strategy.lua
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/resources/lua/unblock_signal.lua ${CMAKE_SOURCE_DIR}/binaries/lua/unblock_signal.lua
    )
endif()
