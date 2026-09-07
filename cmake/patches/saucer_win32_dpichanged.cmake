# Patches saucer (src/win32.window.impl.cpp, WM_DPICHANGED) so a live user
# drag across monitors with different DPI does not fight Windows: while the
# window holds the mouse capture (modal move/resize loop), Windows keeps the
# window under the cursor on its own. Hosts preserving physical size across
# monitors opt in per window via the "UnblockKeepPhysicalSize" property
# (absent == stock behavior). DPI bookkeeping (and min/max refresh) always
# runs; only the forced geometry is guarded.
#
# Supports both v8.0.5 and ver/8.2.0 handler layouts (auto-detected).
# Idempotent: re-runs are a no-op (marker SAUCER_DPICHANGED_LIVEDRAG_FIX).
# Usage:
#   cmake -DSAUCER_SRC=<path to saucer-src> -P saucer_win32_dpichanged.cmake
if(NOT DEFINED SAUCER_SRC)
	message(FATAL_ERROR "SAUCER_SRC not set")
endif()

set(target "${SAUCER_SRC}/src/win32.window.impl.cpp")
if(NOT EXISTS "${target}")
	message(FATAL_ERROR "saucer source not found: ${target}")
endif()

file(READ "${target}" content)

if(content MATCHES "SAUCER_DPICHANGED_LIVEDRAG_FIX")
	message(STATUS "saucer WM_DPICHANGED live-drag fix already applied")
	return()
endif()

# --- v8.0.5 layout: set_size, then set_position, returns 0 ---
string(CONCAT old_805
	"        case WM_DPICHANGED:\n"
	"            const auto size     = self->size();\n"
	"            self->platform->dpi = HIWORD(w_param);\n"
	"\n"
	"            self->set_size(size);\n"
	"\n"
	"            auto *const rect = reinterpret_cast<RECT *>(l_param);\n"
	"            self->set_position({.x = rect->left, .y = rect->top});\n"
	"\n"
	"            return 0;"
)

string(CONCAT new_805
	"        case WM_DPICHANGED:\n"
	"            const auto size     = self->size();\n"
	"            self->platform->dpi = HIWORD(w_param);\n"
	"\n"
	"            // SAUCER_DPICHANGED_LIVEDRAG_FIX: during a live user drag the\n"
	"            // window holds the mouse capture and Windows keeps it under\n"
	"            // the cursor itself. Hosts preserving physical size opt in\n"
	"            // via the UnblockKeepPhysicalSize window property.\n"
	"            const bool keep_physical = GetPropW(hwnd, L\"UnblockKeepPhysicalSize\") != nullptr;\n"
	"            if (GetCapture() != hwnd || !keep_physical)\n"
	"            {\n"
	"                self->set_size(size);\n"
	"\n"
	"                auto *const rect = reinterpret_cast<RECT *>(l_param);\n"
	"                self->set_position({.x = rect->left, .y = rect->top});\n"
	"            }\n"
	"\n"
	"            return 0;"
)

# --- ver/8.2.0 layout: min/max refresh, set_position, then set_size, breaks ---
string(CONCAT old_820
	"        case WM_DPICHANGED:\n"
	"            const auto size     = self->size();\n"
	"            self->platform->dpi = HIWORD(w_param);\n"
	"\n"
	"            if (auto &size = self->platform->min_size; size)\n"
	"            {\n"
	"                size = self->platform->client_size<mode::add>(size->original);\n"
	"            }\n"
	"\n"
	"            if (auto &size = self->platform->max_size; size)\n"
	"            {\n"
	"                size = self->platform->client_size<mode::add>(size->original);\n"
	"            }\n"
	"\n"
	"            auto *const rect = reinterpret_cast<RECT *>(l_param);\n"
	"            self->set_position({.x = rect->left, .y = rect->top});\n"
	"            self->set_size(size); // We need to set the size after the position to avoid feedback loops\n"
	"\n"
	"            break;"
)

string(CONCAT new_820
	"        case WM_DPICHANGED:\n"
	"            const auto size     = self->size();\n"
	"            self->platform->dpi = HIWORD(w_param);\n"
	"\n"
	"            if (auto &size = self->platform->min_size; size)\n"
	"            {\n"
	"                size = self->platform->client_size<mode::add>(size->original);\n"
	"            }\n"
	"\n"
	"            if (auto &size = self->platform->max_size; size)\n"
	"            {\n"
	"                size = self->platform->client_size<mode::add>(size->original);\n"
	"            }\n"
	"\n"
	"            // SAUCER_DPICHANGED_LIVEDRAG_FIX: during a live user drag the\n"
	"            // window holds the mouse capture and Windows keeps it under\n"
	"            // the cursor itself. Hosts preserving physical size opt in\n"
	"            // via the UnblockKeepPhysicalSize window property.\n"
	"            const bool keep_physical = GetPropW(hwnd, L\"UnblockKeepPhysicalSize\") != nullptr;\n"
	"            if (GetCapture() != hwnd || !keep_physical)\n"
	"            {\n"
	"                auto *const rect = reinterpret_cast<RECT *>(l_param);\n"
	"                self->set_position({.x = rect->left, .y = rect->top});\n"
	"                self->set_size(size); // We need to set the size after the position to avoid feedback loops\n"
	"            }\n"
	"\n"
	"            break;"
)

string(FIND "${content}" "${old_805}" pos_805)
string(FIND "${content}" "${old_820}" pos_820)

if(NOT pos_805 EQUAL -1)
	string(REPLACE "${old_805}" "${new_805}" content "${content}")
	file(WRITE "${target}" "${content}")
	message(STATUS "saucer WM_DPICHANGED live-drag fix applied (v8.0.5 layout)")
	return()
endif()

if(NOT pos_820 EQUAL -1)
	string(REPLACE "${old_820}" "${new_820}" content "${content}")
	file(WRITE "${target}" "${content}")
	message(STATUS "saucer WM_DPICHANGED live-drag fix applied (ver/8.2.0 layout)")
	return()
endif()

message(FATAL_ERROR "saucer WM_DPICHANGED block not recognized; refusing to patch (upstream changed?)")
