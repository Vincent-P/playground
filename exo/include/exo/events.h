#pragma once
#include "exo/buttons.h"

namespace exo::events
{
struct Key
{
	VirtualKey  key;
	ButtonState state;
};

struct MouseClick
{
	MouseButton button;
	ButtonState state;
};

struct Character
{
	char sequence[4];
};

struct IMEComposition
{
	char *composition;
};

struct IMECompositionResult
{
	char *result;
};

struct Scroll
{
	int dx;
	int dy;
};

struct MouseMove
{
	int x;
	int y;
};

struct Focus
{
	bool focused;
};

struct Resize
{
	int width;
	int height;
};
} // namespace exo::events

namespace exo
{
struct Event
{
	enum
	{
		KeyType,
		MouseClickType,
		CharacterType,
		IMECompositionType,
		IMECompositionResultType,
		ScrollType,
		MouseMoveType,
		FocusType,
		ResizeType,
	} type;

	union
	{
		events::Key                  key;
		events::MouseClick           mouse_click;
		events::Character            character;
		events::IMEComposition       ime_composition;
		events::IMECompositionResult ime_composition_result;
		events::Scroll               scroll;
		events::MouseMove            mouse_move;
		events::Focus                focus;
		events::Resize               resize;
	};
};

} // namespace exo
