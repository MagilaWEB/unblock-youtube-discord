set(NAME engine)

file(GLOB src_cpp "*.cpp")
file(GLOB src_h "*.h")
file(GLOB src_hpp "*.hpp")

set(CMAKE_CXX_EXTENSIONS OFF)

if(CUPY_LIBS_DLL_TO_BIN)
    add_custom_target(global_pre_build ALL
        COMMAND ${CMAKE_COMMAND} -E echo "=== Global Pre-Build Cleanup ==="
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMAND ${CMAKE_COMMAND} -E echo "Cleanup complete"
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/binaries/winws2.exe || ${CMAKE_COMMAND} -E echo "Warning: failed to remove winws2.exe (maybe in use)"
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/binaries/cygwin1.dll || ${CMAKE_COMMAND} -E echo "Warning: failed to remove cygwin1.dll (maybe in use)"
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-lib.lua
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-auto.lua
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-antidpi.lua
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-obfs.lua
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-pcap.lua
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/binaries/lua/zapret-tests.lua
        COMMENT "Global pre-build cleanup"
    )
endif()

add_executable(${NAME} ${src_h} ${src_hpp} ${src_cpp} engine.rc)

if(CUPY_LIBS_DLL_TO_BIN)
    add_dependencies(${target} global_pre_build)
endif()

target_precompile_headers(${NAME} PRIVATE pch.h)

# Include files
target_include_directories(${NAME} PRIVATE ${ULTRALIGHT_INCLUDE})

# Link
target_link_libraries(${NAME} PRIVATE
        core
        ui
        unblock
        ${ULTRALIGHT_LIB}/AppCore.lib
        ${ULTRALIGHT_LIB}/UltralightCore.lib
        ${ULTRALIGHT_LIB}/Ultralight.lib
        dwmapi.lib
)

# Define EXPORTS when building the libraries
target_compile_definitions(${NAME} PRIVATE CONSOLE ENGINE_EXPORTS ${ALL_MACRO_CPP})

target_link_options(${NAME} PRIVATE
        "LINKER:/ENTRY:mainCRTStartup"
        "LINKER:/SUBSYSTEM:WINDOWS"
    )

# Copy library binaries to the folder of the compiled project
if(CUPY_LIBS_DLL_TO_BIN)
    set(ultralight_files
        ${ULTRALIGHT_BIN}/AppCore.dll
        ${ULTRALIGHT_BIN}/Ultralight.dll
        ${ULTRALIGHT_BIN}/UltralightCore.dll
        ${ULTRALIGHT_BIN}/WebCore.dll
    )

    add_custom_command(TARGET ${NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${7ZA_BIN} $<TARGET_FILE_DIR:${NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${CURL_DIR_DEBUG}/bin/libcurl-d.dll,${CURL_DIR_RELEASE}/bin/libcurl.dll>"
            $<TARGET_FILE_DIR:${NAME}>
        COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Debug>,copy_if_different,echo>
            "${CURL_DIR_DEBUG}/bin/libcurl-d.pdb"
            $<TARGET_FILE_DIR:${NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${CURL_DIR_DEBUG}/bin/zlibd1.dll,${CURL_DIR_RELEASE}/bin/zlib1.dll>"
            $<TARGET_FILE_DIR:${NAME}>
        COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Debug>,copy_if_different,echo>
            "${CURL_DIR_DEBUG}/bin/zlibd.pdb"
            $<TARGET_FILE_DIR:${NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/engine/unblock.ico"
            $<TARGET_FILE_DIR:${NAME}>


        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ultralight_files} $<TARGET_FILE_DIR:${NAME}>
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
