#pragma once

// Widget-declaration macros. Macros cannot cross a module boundary, so the
// units that declare widget members include this header textually. The bodies
// name types (Ptr<...>) visible at the point of expansion through the unit's
// own imports.

#define BUTTON(name)  \
	Ptr<Button>##name \
	{                 \
		#name         \
	}

#define CHECK_BOX(name) \
	Ptr<CheckBox>##name \
	{                   \
		#name           \
	}

#define EDITABLE_LIST(name) \
	Ptr<EditableList>##name \
	{                       \
		#name               \
	}

#define STATUS(name)           \
	Ptr<StatusIndicator>##name \
	{                          \
		#name                  \
	}

#define SELECT_LIST(name) \
	Ptr<SelectList>##name \
	{                     \
		#name             \
	}

#define SECONDARY_WINDOW(name) \
	Ptr<SecondaryWindow>##name \
	{                          \
		#name                  \
	}

#define TEXT_LABEL(name) \
	Ptr<TextLabel>##name \
	{                    \
		#name            \
	}

#define UL_LIST(name) \
	Ptr<ListUl>##name \
	{                 \
		#name         \
	}

#define INPUT(name)  \
	Ptr<Input>##name \
	{                \
		#name        \
	}
