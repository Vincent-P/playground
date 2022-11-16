#pragma once
#include "painter/rect.h"

namespace ui
{
struct Ui;
}
namespace custom_ui
{
struct FpsHistogram
{
	float frame_times[512] = {};
	void  push_time(float dt);
};

struct FpsHistogramWidget
{
	Rect          rect;
	FpsHistogram *histogram;
};

void histogram(ui::Ui &ui, FpsHistogramWidget widget);
} // namespace custom_ui
