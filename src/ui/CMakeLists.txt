set(NAME ui)

# Sources are globbed, not listed: a new .cpp/.cppm in this directory is picked
# up on the next build (CONFIGURE_DEPENDS re-globs at build time).
file(GLOB src_cpp CONFIGURE_DEPENDS "*.cpp")
file(GLOB src_cppm CONFIGURE_DEPENDS "*.cppm")

add_library(${NAME} STATIC ${src_cpp})

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES ${src_cppm}
)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

# ext PUBLIC: saucer::smartview leaks into the module interface (Ui/BaseElement).
# core/common/dom/unblock PUBLIC: their types appear in the exported declarations
# (ui::dom::Element, File, Unblock, DnsProxy/TgProxy).
# ub_engine_api: the ui code calls the engine through IEngineAPI.
target_link_libraries(${NAME} PUBLIC ub_std ext core common dom unblock ub_version ub_engine_api)

target_compile_definitions(${NAME} PRIVATE ${ALL_MACRO_CPP})
