set(NAME core)

file(GLOB src_cpp "*.cpp")
file(GLOB src_h "*.h")
file(GLOB src_hpp "*.hpp")
#file(GLOB ${NAME}_src_modules "*.ixx")

add_library(${NAME} SHARED ${src_h} ${src_hpp} ${src_cpp})
#target_sources(${NAME} PUBLIC FILE_SET CXX_MODULES FILES ${src_modules})
target_precompile_headers(${NAME} PRIVATE pch.h)

# Include files

# Link
#target_link_libraries(${NAME} PRIVATE)

# Define EXPORTS when building the libraries
target_compile_definitions(${NAME} PRIVATE CORE_EXPORTS ${ALL_MACRO_CPP})