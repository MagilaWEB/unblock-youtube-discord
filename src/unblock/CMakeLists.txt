set(NAME unblock)

file(GLOB src_cpp "*.cpp")
file(GLOB src_h "*.h")
file(GLOB src_hpp "*.hpp")
#file(GLOB ${NAME}_src_modules "*.ixx")

add_library(${NAME} SHARED ${src_h} ${src_hpp} ${src_cpp})
#target_sources(${NAME} PUBLIC FILE_SET CXX_MODULES FILES ${src_modules})
target_precompile_headers(${NAME} PRIVATE pch.h)

# Include files
target_include_directories(${NAME} PRIVATE
        ${CURL_INCLUDE_DIR}
        ${BIT7Z_INCLUDE}
)

# Link
target_link_libraries(${NAME} PRIVATE
        core
        ${CURL_LIBRARY}
        ${BIT7Z_LIB}
)

# Define EXPORTS when building the libraries
target_compile_definitions(${NAME} PRIVATE UNBLOCK_EXPORTS ${ALL_MACRO_CPP})