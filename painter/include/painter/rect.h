#pragma once
#include <exo/macros/assert.h>
#include <exo/macros/packed.h>
#include <exo/maths/vectors.h>

enum struct SplitDirection : u8
{
	Top,
	Bottom,
	Left,
	Right,
};

inline bool split_is_horizontal(SplitDirection direction)
{
	switch (direction) {
	case SplitDirection::Left:
	case SplitDirection::Right:
		return true;
	case SplitDirection::Top:
	case SplitDirection::Bottom:
	default:
		return false;
	}
}

PACKED(struct Rect {
	float2 pos;
	float2 size;
})

// -- Posing

inline Rect rect_center(Rect container, const float2 &element_size)
{
	return {
		.pos  = container.pos + 0.5 * (container.size - element_size),
		.size = element_size,
	};
}

// -- Testing

inline bool rect_is_point_inside(Rect container, const float2 &point)
{
	return container.pos.x <= point.x && point.x <= container.pos.x + container.size.x && container.pos.y <= point.y &&
	       point.y <= container.pos.y + container.size.y;
}

// -- Margins

inline Rect rect_outset(Rect r, float2 margin)
{
	return {
		.pos  = r.pos - margin,
		.size = r.size + 2.0f * margin,
	};
}

inline Rect rect_inset(Rect r, float2 margin) { return rect_outset(r, float2(-margin.x, -margin.y)); }

// -- Splitting

inline Rect rect_split_top(Rect &r, float height)
{
	auto top    = Rect{.pos = r.pos, .size = {r.size[0], height}};
	auto bottom = Rect{.pos = {r.pos[0], r.pos[1] + height}, .size = {r.size[0], r.size[1] - height}};
	r           = bottom;
	return top;
}

inline Rect rect_split_bottom(Rect &r, float height)
{
	auto top    = Rect{.pos = r.pos, .size = {r.size[0], r.size[1] - height}};
	auto bottom = Rect{.pos = {r.pos[0], r.pos[1] + top.size[1]}, .size = {r.size[0], height}};
	r           = top;
	return bottom;
}

inline Rect rect_split_left(Rect &r, float width)
{
	auto left  = Rect{.pos = r.pos, .size = {width, r.size[1]}};
	auto right = Rect{.pos = {r.pos[0] + width, r.pos[1]}, .size = {r.size[0] - width, r.size[1]}};
	r          = right;
	return left;
}

inline Rect rect_split_right(Rect &r, float width)
{
	auto left  = Rect{.pos = r.pos, .size = {r.size[0] - width, r.size[1]}};
	auto right = Rect{.pos = {r.pos[0] + left.size[0], r.pos[1]}, .size = {width, r.size[1]}};
	r          = left;
	return right;
}

struct RectSplit
{
	Rect          *rect      = nullptr;
	SplitDirection direction = SplitDirection::Top;

	Rect split(float value)
	{
		ASSERT(this->rect);
		switch (this->direction) {
		case SplitDirection::Top:
			return rect_split_top(*this->rect, value);
		case SplitDirection::Bottom:
			return rect_split_bottom(*this->rect, value);
		case SplitDirection::Left:
			return rect_split_left(*this->rect, value);
		case SplitDirection::Right:
			return rect_split_right(*this->rect, value);
		}
	}
};
