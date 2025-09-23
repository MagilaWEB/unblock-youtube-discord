set(NAME engine)

file(GLOB src_cpp "*.cpp")
file(GLOB src_h "*.h")
file(GLOB src_hpp "*.hpp")

add_executable(${NAME} ${src_h} ${src_hpp} ${src_cpp} engine.rc)
target_precompile_headers(${NAME} PRIVATE pch.h)

# Include files
target_include_directories(${NAME} PRIVATE ${EXPORTED_SDK_DIRECTORY}/ultralight/include)

# Link
target_link_libraries(${NAME} PRIVATE
        core
        ui
        unblock
        ${EXPORTED_SDK_DIRECTORY}/ultralight/lib/AppCore.lib
        ${EXPORTED_SDK_DIRECTORY}/ultralight/lib/UltralightCore.lib
        ${EXPORTED_SDK_DIRECTORY}/ultralight/lib/Ultralight.lib
)

# Define EXPORTS when building the libraries
target_compile_definitions(${NAME} PRIVATE CONSOLE ENGINE_EXPORTS ${ALL_MACRO_CPP})
set_target_properties(${NAME} PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")

# To hide console on startup
target_link_options(${NAME} PRIVATE /SUBSYSTEM:CONSOLE)

# Copy library binaries to the folder of the compiled project
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${NAME} PRE_BUILD
#                       COMMAND ${CMAKE_COMMAND} -E copy
#                           ${CMAKE_BINARY_DIR}/oneTBB_${CMAKE_BUILD_TYPE}/tbb12_${CMAKE_BUILD_TYPE}.dll $<TARGET_FILE_DIR:${NAME}>
#                       COMMAND ${CMAKE_COMMAND} -E copy
#                           ${CMAKE_BINARY_DIR}/oneTBB_${CMAKE_BUILD_TYPE}/tbb12_${CMAKE_BUILD_TYPE}.pdb $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${CMAKE_VCPKG_BINARY_DEBUG}/libcurl-d.dll $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${CMAKE_VCPKG_BINARY_DEBUG}/libcurl-d.pdb $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${CMAKE_VCPKG_BINARY_DEBUG}/zlibd1.dll $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${CMAKE_VCPKG_BINARY_DEBUG}/zlibd.pdb $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${EXPORTED_SDK_DIRECTORY}/ultralight/bin/AppCore.dll $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${EXPORTED_SDK_DIRECTORY}/ultralight/bin/Ultralight.dll $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${EXPORTED_SDK_DIRECTORY}/ultralight/bin/UltralightCore.dll $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${EXPORTED_SDK_DIRECTORY}/ultralight/bin/WebCore.dll $<TARGET_FILE_DIR:${NAME}>)
else()
    add_custom_command(TARGET ${NAME} PRE_BUILD
#                       COMMAND ${CMAKE_COMMAND} -E copy
#                           ${CMAKE_BINARY_DIR}/oneTBB_${CMAKE_BUILD_TYPE}/tbb12.dll $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${CMAKE_VCPKG_BINARY}/libcurl.dll $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${CMAKE_VCPKG_BINARY}/zlib1.dll $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${EXPORTED_SDK_DIRECTORY}/ultralight/bin/AppCore.dll $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${EXPORTED_SDK_DIRECTORY}/ultralight/bin/Ultralight.dll $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${EXPORTED_SDK_DIRECTORY}/ultralight/bin/UltralightCore.dll $<TARGET_FILE_DIR:${NAME}>
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${EXPORTED_SDK_DIRECTORY}/ultralight/bin/WebCore.dll $<TARGET_FILE_DIR:${NAME}>)
endif()