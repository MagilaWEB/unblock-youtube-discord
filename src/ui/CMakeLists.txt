set(NAME ui)

add_library(${NAME} STATIC)

target_sources(${NAME}
  PRIVATE
    tutorial.cpp
    ui.cpp
    ui_background_tasks.cpp
    ui_base_element.cpp
    ui_button.cpp
    ui_check.cpp
    ui_check_box.cpp
    ui_dns_proxy.cpp
    ui_editable_list.cpp
    ui_input.cpp
    ui_list_ul.cpp
    ui_proxy_tg.cpp
    ui_remove_app.cpp
    ui_secondary_window.cpp
    ui_select_list.cpp
    ui_status.cpp
    ui_text.cpp
    ui_unblock.cpp
    ui_update_app.cpp
    ui_zapret_helper.cpp
    ui_zapret_page.cpp
)

target_sources(${NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES
      ui.cppm
      ui.engine_api.cppm
      ui.tutorial.cppm
      ui.ui_background_tasks.cppm
      ui.ui_base_element.cppm
      ui.ui_button.cppm
      ui.ui_check_box.cppm
      ui.ui_editable_list.cppm
      ui.ui_input.cppm
      ui.ui_list_ul.cppm
      ui.ui_secondary_window.cppm
      ui.ui_select_list.cppm
      ui.ui_status.cppm
      ui.ui_text.cppm
)

target_compile_options(${NAME} PRIVATE ${PROJECT_WARNING_FLAGS})

# ext PUBLIC: saucer::smartview leaks into the module interface (Ui/BaseElement).
# core/common/dom/unblock PUBLIC: their types appear in the exported declarations
# (ui::dom::Element, File, Unblock, DnsProxy/TgProxy).
target_link_libraries(${NAME} PUBLIC ub_std ext core common dom unblock ub_version)

target_compile_definitions(${NAME} PRIVATE ${ALL_MACRO_CPP})
