set(NAME engine)

file(GLOB src_cpp "*.cpp")
file(GLOB src_h "*.h")
file(GLOB src_hpp "*.hpp")

set(CMAKE_CXX_EXTENSIONS OFF)

if(CUPY_LIBS_DLL_TO_BIN)
    # Full-rebuild cleanup. These run at configure time (i.e. a full rebuild in
    # the IDE triggers a reconfigure), NOT on every incremental ninja run.
    # Otherwise incremental builds would wipe bin/ and binaries/ and leave the
    # unchanged DLLs/exes missing until the next full relink.
    file(REMOVE_RECURSE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    file(REMOVE ${CMAKE_SOURCE_DIR}/binaries/winws2.exe)
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

add_executable(${NAME} ${src_h} ${src_hpp} ${src_cpp} engine.rc)

target_precompile_headers(${NAME} PRIVATE pch.h)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})
# WebView2.h has an enum value that does not fit into int
target_compile_options(${NAME} PRIVATE -Wno-microsoft-enum-value)

# WebView2 headers (saucer/modules/stable/webview2.hpp pulls in WebView2.h)
if(WEBVIEW2_INCLUDE_DIRS)
    target_include_directories(${NAME} PRIVATE ${WEBVIEW2_INCLUDE_DIRS})
endif()

# Link
target_link_libraries(${NAME} PRIVATE
        core
        ui
        unblock
        saucer::saucer
        dwmapi.lib
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
            ${ZAPRET2_BIN}/cygwin1.dll ${CMAKE_SOURCE_DIR}/binaries || ${CMAKE_COMMAND} -E echo "Warning: failed to copy cygwin1.dll (maybe in use)"
        COMMAND ${CMAKE_COMMAND} -E copy_directory 
            ${ZAPRET2_LUA}/ ${CMAKE_SOURCE_DIR}/binaries/lua
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/resources/lua/unblock_auto_strategy.lua ${CMAKE_SOURCE_DIR}/binaries/lua/unblock_auto_strategy.lua
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/resources/lua/unblock_signal.lua ${CMAKE_SOURCE_DIR}/binaries/lua/unblock_signal.lua
    )
endif()
