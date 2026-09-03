set(NAME ui)

file(GLOB src_cpp "*.cpp")
file(GLOB src_h "*.h")
file(GLOB src_hpp "*.hpp")
#file(GLOB ${NAME}_src_modules "*.ixx")

add_library(${NAME} STATIC ${src_h} ${src_hpp} ${src_cpp})

target_precompile_headers(${NAME} PRIVATE pch.h)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

# Link
target_link_libraries(${NAME} PRIVATE
        core
        unblock
        saucer::saucer
)

target_compile_definitions(${NAME} PRIVATE ${ALL_MACRO_CPP})