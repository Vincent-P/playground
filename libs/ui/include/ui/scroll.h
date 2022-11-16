#pragma once
#include "painter/rect.h"

namespace exo { struct float2; }

namespace ui
{
struct Ui;

Rect begin_scroll_area(Ui &ui, const Rect &content_rect, exo::float2 &offset);
void end_scroll_area(Ui &ui, const Rect &ending_rect);
} // namespace ui
