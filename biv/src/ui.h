#pragma once
#include "rect.h"
#include <exo/maths/numerics.h>

struct Font;
struct Painter;
class Inputs;

inline constexpr u32 UI_MAX_DEPTH = 128;

struct UiTheme
{
	// colors are in 0xAABBGGRR
	u32 button_bg_color         = 0xB2FFFF;
	u32 button_hover_bg_color   = 0x06000000;
	u32 button_pressed_bg_color = 0x09000000;
	u32 button_label_color      = 0xFF000000;

	float input_thickness          = 10.0f;
	float splitter_thickness       = 2.0f;
	float splitter_hover_thickness = 4.0f;
	u32   splitter_color           = 0xFFE5E5E5;
	u32   splitter_hover_color     = 0xFFD1D1D1;

	Font *main_font = nullptr;
};

struct UiState
{
	u64 focused                  = 0;
	u64 active                   = 0;
	u64 gen                      = 0;
	u32 i_clip_rect              = u32_invalid;
	u32 clip_stack[UI_MAX_DEPTH] = {};
	u32 i_clip_stack             = 0;
	int cursor                   = 0;

	Inputs  *inputs  = nullptr;
	Painter *painter = nullptr;
};

struct UiButton
{
	const char *label;
	Rect        rect;
};

struct UiLabel
{
	const char *text;
	Rect        rect;
};

struct UiRect
{
	u32  color;
	Rect rect;
};

bool ui_is_hovering(const UiState &ui_state, const Rect &rect);
u64  ui_make_id(UiState &state);

void ui_new_frame(UiState &ui_state);
void ui_end_frame(UiState &ui_state);

u32  ui_register_clip_rect(UiState &ui_state, const Rect &clip_rect);
void ui_push_clip_rect(UiState &ui_state, u32 i_clip_rect);
void ui_pop_clip_rect(UiState &ui_state);

bool ui_button(UiState &ui_state, const UiTheme &ui_theme, const UiButton &button);
void ui_splitter_x(
	UiState &ui_state, const UiTheme &ui_theme, const Rect &view_rect, float &value, Rect &left, Rect &right);
void ui_splitter_y(
	UiState &ui_state, const UiTheme &ui_theme, const Rect &view_rect, float &value, Rect &top, Rect &bottom);
void ui_label(UiState &ui_state, const UiTheme &ui_theme, const UiLabel &label);
void ui_rect(UiState &ui_state, const UiTheme &ui_theme, const UiRect &rect);
