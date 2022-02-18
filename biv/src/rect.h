#pragma once
#include <exo/macros/assert.h>
#include <exo/macros/packed.h>
#include <exo/maths/vectors.h>

PACKED(struct Rect {
	exo::float2 position;
	exo::float2 size;
})

struct RectLeftRight
{
	Rect left;
	Rect right;
};

struct RectTopBottom
{
	Rect top;
	Rect bottom;
};

// -- Positioning

inline Rect rect_center(const Rect &container, const exo::float2 &element_size)
{
	return {
		.position = container.position + 0.5 * (container.size - element_size),
		.size = element_size,
	};
}

// -- Testing

inline bool rect_is_point_inside(const Rect &container, const exo::float2 &point)
{
	return container.position.x <= point.x && point.x <= container.position.x + container.size.x && container.position.y <= point.y && point.y <= container.position.y + container.size.y;
}

// -- Margins

inline Rect rect_outset(const Rect &r, float margin)
{
    return {
	.position = r.position - exo::float2(margin),
	.size = r.size + exo::float2(2.0f * margin),
    };
}

inline Rect rect_inset(const Rect &r, float margin)
{
    return {
	.position = r.position + exo::float2(margin),
	.size = r.size - exo::float2(2.0f * margin),
    };
}

// -- Splitting

inline Rect rect_divide_x(const Rect &r, float margin, u32 n, u32 i)
{
	ASSERT(n > 0 && i < n);
	const float split_width = r.size.x * (1.0f / float(n)) - float(n - 1) * margin;
	return {
		.position = {r.position.x + float(i) * (split_width + margin), r.position.y},
		.size = {split_width, r.size.y},
	};
}

inline Rect rect_divide_y(const Rect &r, float margin, u32 n, u32 i)
{
	ASSERT(n > 0 && i < n);
	const float split_height = r.size.y * (1.0f / float(n)) - float(n - 1) * margin;
	return {
		.position = {r.position.x, r.position.y + float(i) * (split_height + margin)},
		.size = {r.size.x, split_height},
	};
}

inline RectTopBottom rect_split_off_top(const Rect &r, float height, float margin)
{
	return {
		.top =
			{
				.position = r.position,
				.size = {r.size.x, height},
			},
		.bottom =
			{
				.position = r.position + exo::float2{0.0f, height + margin},
				.size = {r.size.x, r.size.y - height - margin},
			},
	};
}

inline RectTopBottom rect_split_off_bottom(const Rect &r, float height, float margin)
{
	return {
		.top =
			{
				.position = r.position,
				.size = {r.size.x, r.size.y - height - margin},
			},
		.bottom =
			{
				.position = r.position + exo::float2{0.0f, r.size.y - height},
				.size = {r.size.x, height},
			},
	};
}

inline RectLeftRight rect_split_off_left(const Rect &r, float width, float margin)
{
	return {
		.left =
			{
				.position = r.position,
				.size = {width, r.size.y},
			},
		.right =
			{
				.position = r.position + exo::float2{width + margin, 0.0f},
				.size = {r.size.x - width - margin, r.size.y},
			},
	};
}

inline RectLeftRight rect_split_off_right(const Rect &r, float width, float margin)
{
	return {
		.left =
			{
				.position = r.position,
				.size = {r.size.x - width - margin, r.size.y},
			},
		.right =
			{
				.position = r.position + exo::float2{r.size.x - width, 0.0f},
				.size = {width, r.size.y},
			},
	};
}
