#pragma once
#include <painter/rect.h>

struct Ui;
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

void histogram(Ui &ui, FpsHistogramWidget widget);
} // namespace custom_ui
