#pragma once
#include "exo/macros/assert.h"
#include "exo/maths/vectors.h"

enum struct SplitDirection : u8
{
	Top,
	Bottom,
	Left,
	Right,
};

struct Rect
{
	float2 pos;
	float2 size;

	// -- Posing

	Rect ceil() const
	{
		return {
			.pos = exo::ceil(this->pos),
			.size = exo::ceil(this->size),
		};
	}

	Rect center(const float2 &element_size) const
	{
		return {
			.pos = this->pos + 0.5 * (this->size - element_size),
			.size = element_size,
		};
	}

	Rect offset(float2 offset) const
	{
		return {
			.pos = this->pos + offset,
			.size = this->size,
		};
	}

	// -- Testing

	bool is_point_inside(const float2 &point) const
	{
		return this->pos.x <= point.x && point.x <= this->pos.x + this->size.x && this->pos.y <= point.y &&
		       point.y <= this->pos.y + this->size.y;
	}

	bool intersects(const Rect &other) const
	{
		return !(other.pos.x > (this->pos.x + this->size.x) || (other.pos.x + other.size.x) < this->pos.x ||
				 other.pos.y > (this->pos.y + this->size.y) || (other.pos.y + other.size.y) < this->pos.y);
	}

	// -- Margins

	Rect outset(float2 margin) const
	{
		return {
			.pos = this->pos - margin,
			.size = this->size + 2.0f * margin,
		};
	}

	Rect inset(float2 margin) const { return this->outset(float2(-margin.x, -margin.y)); }

	// -- Splitting

	Rect split_top(float height)
	{
		auto top = Rect{.pos = this->pos, .size = {this->size[0], height}};
		auto bottom =
			Rect{.pos = {this->pos[0], this->pos[1] + height}, .size = {this->size[0], this->size[1] - height}};
		*this = bottom;
		return top;
	}

	Rect split_bottom(float height)
	{
		auto top = Rect{.pos = this->pos, .size = {this->size[0], this->size[1] - height}};
		auto bottom = Rect{.pos = {this->pos[0], this->pos[1] + top.size[1]}, .size = {this->size[0], height}};
		*this = top;
		return bottom;
	}

	Rect split_left(float width)
	{
		auto left = Rect{.pos = this->pos, .size = {width, this->size[1]}};
		auto right = Rect{.pos = {this->pos[0] + width, this->pos[1]}, .size = {this->size[0] - width, this->size[1]}};
		*this = right;
		return left;
	}

	Rect split_right(float width)
	{
		auto left = Rect{.pos = this->pos, .size = {this->size[0] - width, this->size[1]}};
		auto right = Rect{.pos = {this->pos[0] + left.size[0], this->pos[1]}, .size = {width, this->size[1]}};
		*this = left;
		return right;
	}
};

struct RectSplit
{
	Rect &rect;
	SplitDirection direction = SplitDirection::Top;

	Rect split(float value)
	{
		switch (this->direction) {
		default:
		case SplitDirection::Top:
			return this->rect.split_top(value);
		case SplitDirection::Bottom:
			return this->rect.split_bottom(value);
		case SplitDirection::Left:
			return this->rect.split_left(value);
		case SplitDirection::Right:
			return this->rect.split_right(value);
		}
	}

	Rect split(float2 non_uniform_value)
	{
		switch (this->direction) {
		default:
		case SplitDirection::Top:
			return this->rect.split_top(non_uniform_value.y);
		case SplitDirection::Bottom:
			return this->rect.split_bottom(non_uniform_value.y);
		case SplitDirection::Left:
			return this->rect.split_left(non_uniform_value.x);
		case SplitDirection::Right:
			return this->rect.split_right(non_uniform_value.x);
		}
	}
};
