#pragma once
#include <exo/macros/packed.h>
#include <exo/maths/vectors.h>

PACKED(struct Rect {
    exo::float2 position;
    exo::float2 size;
})

inline Rect rect_center(const Rect &container, const exo::float2 &element_size)
{
    return {
        .position = container.position + 0.5 * (container.size - element_size),
        .size     = element_size,
    };
}

inline Rect rect_center(const Rect &container, const Rect &rect)
{
    return rect_center(container, rect.size);
}

inline bool rect_is_point_inside(const Rect &container, const exo::float2 &point)
{
    return container.position.x <= point.x && point.x <= container.position.x + container.size.x
           && container.position.y <= point.y && point.y <= container.position.y + container.size.y;
}

inline Rect rect_split_off_top(const Rect &r, float height, float margin)
{
    // __________
    // | split  |
    // ----------
    // | margin |
    // ----------
    // | r      |
    // ----------
    return {
        .position = r.position,
        .size     = {r.size.x, height + margin},
    };
}

inline Rect rect_split_off_bottom(const Rect &r, float height, float margin)
{
    // __________
    // | r      |
    // ----------
    // | margin |
    // ----------
    // | split  |
    // ----------
    return {
        .position = r.position + exo::float2{0.0, (r.size.y - height) + margin},
        .size     = {r.size.x, height},
    };
}

inline Rect rect_split_off_left(const Rect &r, float width, float /*margin*/)
{
    // ______________________
    // | split | margin | r |
    // ----------------------
    return {
        .position = r.position,
        .size     = {width, r.size.y},
    };
}

inline Rect rect_split_off_right(const Rect &r, float width, float margin)
{
    // ______________________
    // | r | margin | split |
    // ----------------------
    return {
        .position = r.position + exo::float2{(r.size.x - width) + margin, 0},
        .size     = {width, r.size.y},
    };
}
