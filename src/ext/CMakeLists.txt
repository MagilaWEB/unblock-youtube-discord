set(NAME ext)

# Wrappers around third-party headers. The project does not include them
# textually, it imports these modules instead.
#
# Declarations that come from a header included in the global module fragment
# cannot be exported as such, so every name the project uses is re-exported by
# hand. That list doubles as the surface to re-check when a dependency is
# upgraded.

add_library(${NAME} STATIC)

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES
      saucer.cppm
      coco.cppm
      curl.cppm
      bit7z.cppm
)

# saucer PUBLIC: its types appear in the exported declarations.
target_link_libraries(${NAME} PUBLIC ub_std saucer::saucer CURL::libcurl ${BIT7Z_LIB})
target_include_directories(${NAME} PUBLIC ${BIT7Z_INCLUDE})

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

# WebView2.h has an enum value that does not fit into int
target_compile_options(${NAME} PRIVATE -Wno-microsoft-enum-value)

# WebView2 headers (saucer/modules/stable/webview2.hpp pulls in WebView2.h)
if(WEBVIEW2_INCLUDE_DIRS)
    target_include_directories(${NAME} PRIVATE ${WEBVIEW2_INCLUDE_DIRS})
endif()
