# Patches saucer v8.0.5 (src/win32.window.impl.cpp, WM_DPICHANGED) so a live
# user drag across monitors with different DPI does not fight Windows:
# while the window holds the mouse capture (modal move/resize loop), Windows
# keeps the window under the cursor on its own. Forcing the suggested
# position/size there makes the window jump between monitors until drop.
# Only the DPI bookkeeping is updated mid-drag; the host settles the final
# physical size once via WM_EXITSIZEMOVE (single SetWindowPos roundtrip).
#
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

string(CONCAT old_block
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

string(FIND "${content}" "${old_block}" pos)
if(pos EQUAL -1)
	message(FATAL_ERROR "saucer WM_DPICHANGED block not recognized; refusing to patch (upstream changed?)")
endif()

string(CONCAT new_block
	"        case WM_DPICHANGED:\n"
	"            // SAUCER_DPICHANGED_LIVEDRAG_FIX: during a live user drag the\n"
	"            // window holds the mouse capture and Windows keeps it under\n"
	"            // the cursor itself. Forcing the suggested rect here fights\n"
	"            // the drag loop and the window jumps between monitors.\n"
	"            self->platform->dpi = HIWORD(w_param);\n"
	"\n"
	"            if (GetCapture() != hwnd)\n"
	"            {\n"
	"                const auto size = self->size();\n"
	"                self->set_size(size);\n"
	"\n"
	"                auto *const rect = reinterpret_cast<RECT *>(l_param);\n"
	"                self->set_position({.x = rect->left, .y = rect->top});\n"
	"            }\n"
	"\n"
	"            return 0;"
)

string(REPLACE "${old_block}" "${new_block}" content "${content}")
file(WRITE "${target}" "${content}")
message(STATUS "saucer WM_DPICHANGED live-drag fix applied")
