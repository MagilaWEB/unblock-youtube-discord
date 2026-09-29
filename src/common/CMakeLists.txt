set(NAME common)

file(GLOB src_h "*.h")
file(GLOB src_hpp "*.hpp")

# Header-only module shared across executables (ui, unblock_dns, ...).
add_library(${NAME} INTERFACE)
target_sources(${NAME} INTERFACE ${src_h} ${src_hpp})
target_include_directories(${NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
