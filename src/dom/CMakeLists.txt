set(NAME dom)

add_library(${NAME} STATIC)

target_sources(${NAME}
  PRIVATE
    dom_element.cpp
    dom_events.cpp
    dom_geometry.cpp
    dom_props.cpp
    dom_tour.cpp
    dom_view.cpp
)

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES
      dom.cppm
      dom_element.cppm
      dom_tour.cppm
      dom_view.cppm
      utils_saucer.cppm
)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

# ext PUBLIC: saucer::smartview* leaks into the module interface (dom_view).
# core PUBLIC: utils_saucer (js::Value/jsToCpp) uses s64/concepts/Debug.
target_link_libraries(${NAME} PUBLIC ext core)

target_compile_definitions(${NAME} PRIVATE ${ALL_MACRO_CPP})

if (BUILD_TESTS)
  add_subdirectory(tests)
endif ()
