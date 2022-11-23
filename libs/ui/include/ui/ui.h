#pragma once
#include "cross/buttons.h"
#include "exo/collections/dynamic_array.h"
#include "exo/collections/map.h"
#include "exo/maths/numerics.h"
#include "exo/option.h"
#include "exo/string_view.h"
#include "painter/color.h"
#include "painter/rect.h"

struct Font;
struct Painter;

inline constexpr u32 UI_MAX_DEPTH = 32;

namespace ui
{
struct Theme
{
	// Color scheme
	ColorU32 accent_color = ColorU32::from_uints(0x10, 0x75, 0xB2);

	// Widget colors
	ColorU32 button_bg_color = ColorU32::from_floats(1.0f, 1.0f, 1.0f, 0.3f);
	ColorU32 button_hover_bg_color = ColorU32::from_uints(0, 0, 0, 0x06);
	ColorU32 button_pressed_bg_color = ColorU32::from_uints(0, 0, 0, 0x09);
	ColorU32 button_label_color = ColorU32::from_uints(0, 0, 0, 0xFF);

	float input_thickness = 10.0f;
	float splitter_thickness = 2.0f;
	float splitter_hover_thickness = 4.0f;
	ColorU32 splitter_color = ColorU32::from_greyscale(u8(0xE5));
	ColorU32 splitter_hover_color = ColorU32::from_greyscale(u8(0xD1));

	ColorU32 scroll_area_bg_color = ColorU32::from_uints(0, 0, 0, 0x44);
	ColorU32 scroll_bar_bg_color = ColorU32::from_uints(0xDD, 0xDD, 0xFF, 0x22);
	ColorU32 scroll_thumb_bg_color = accent_color;

	Font *main_font = nullptr;
	float font_size = 14.0f;
};

struct Inputs
{
	exo::EnumArray<bool, exo::MouseButton> mouse_buttons_pressed = {};
	exo::EnumArray<bool, exo::MouseButton> mouse_buttons_pressed_last_frame = {};
	int2 mouse_position = {0};
	Option<int2> mouse_wheel = {};
};

struct Activation
{
	u64 focused = 0;
	u64 active = 0;
	u64 gen = 0;
};

struct State
{
	exo::DynamicArray<u32, UI_MAX_DEPTH> clip_stack;
	u32 current_clip_rect = u32_invalid;

	exo::DynamicArray<u64, UI_MAX_DEPTH> scroll_id_stack;
	exo::Map<u64, Rect> scroll_starting_rects;
	exo::Map<u64, Rect> scroll_ending_rects;

	// custom mouse cursor
	int cursor = 0;

	float2 active_drag_offset = {};
};

struct Ui
{
	Theme theme;
	Inputs inputs;
	Activation activation;
	State state;
	Painter *painter;
	// --

	static Ui create(Font *font, float font_size, Painter *painter);
	void new_frame();
	void end_frame();

	// helpers
	inline float2 mouse_position() const { return float2(this->inputs.mouse_position); }
	bool is_hovering(const Rect &rect) const;
	u64 make_id();

	// widget api
	bool has_pressed(exo::MouseButton button = exo::MouseButton::Left) const;
	bool has_pressed_and_released(exo::MouseButton button = exo::MouseButton::Left) const;
	bool has_clicked(u64 id, exo::MouseButton button = exo::MouseButton::Left) const;

	const Rect &current_clip_rect() const;
	bool is_clipped(const Rect &rect) const;

	u32 register_clip_rect(const Rect &clip_rect);
	void push_clip_rect(u32 i_clip_rect);
	void pop_clip_rect();
};

struct Button
{
	const char *label;
	Rect rect;
};

bool button(Ui &ui, const Button &button);

struct RectPair
{
	Rect first;
	Rect second;
};
RectPair splitter_x(Ui &ui, const Rect &view_rect, float &value);
RectPair splitter_y(Ui &ui, const Rect &view_rect, float &value);
void rect(Ui &ui, const Rect &rect);

enum struct Alignment
{
	Center
};
void label_in_rect(Ui &ui, const Rect &view_rect, exo::StringView label, Alignment alignment = Alignment::Center);
Rect label_split(Ui &ui, RectSplit &rectsplit, exo::StringView label);
bool button_split(Ui &ui, RectSplit &rectsplit, exo::StringView label);

bool invisible_button(Ui &ui, const Rect &rect);

} // namespace ui
