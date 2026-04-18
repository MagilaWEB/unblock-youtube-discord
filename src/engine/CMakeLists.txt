set(NAME engine)

file(GLOB src_cpp "*.cpp")
file(GLOB src_h "*.h")
file(GLOB src_hpp "*.hpp")

if(CUPY_LIBS_DLL_TO_BIN)
    add_custom_target(global_pre_build ALL
        COMMAND ${CMAKE_COMMAND} -E echo "=== Global Pre-Build Cleanup ==="
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMAND ${CMAKE_COMMAND} -E echo "Cleanup complete"
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
)

# Define EXPORTS when building the libraries
target_compile_definitions(${NAME} PRIVATE CONSOLE ENGINE_EXPORTS ${ALL_MACRO_CPP})

# Tell MSVC to use main instead of WinMain for Windows subsystem executables
target_link_options(${NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/ENTRY:mainCRTStartup>
    $<$<CXX_COMPILER_ID:MSVC>:/MANIFESTUAC:level='requireAdministrator'>
    $<$<CXX_COMPILER_ID:MSVC>:/SUBSYSTEM:WINDOWS>
    $<$<CXX_COMPILER_ID:Clang>:-Wl,/ENTRY:mainCRTStartup>
    $<$<CXX_COMPILER_ID:Clang>:-Wl,/MANIFESTUAC:level='requireAdministrator'>
    $<$<CXX_COMPILER_ID:Clang>:-Wl,/SUBSYSTEM:WINDOWS>
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

        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ultralight_files} $<TARGET_FILE_DIR:${NAME}>
        COMMENT "Copying runtime DLLs to output directory"
    )
endif()
