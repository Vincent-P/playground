#include "custom_ui.h"


#include <exo/collections/array.h>
#include <painter/painter.h>
#include <ui/ui.h>

#include <algorithm>
#include <cmath>
#include <fmt/format.h>

namespace custom_ui
{
static float3 turbo_colormap(float x)
{
	const float4 kRedFloat4   = float4(0.13572138f, 4.61539260f, -42.66032258f, 132.13108234f);
	const float4 kGreenFloat4 = float4(0.09140261f, 2.19418839f, 4.84296658f, -14.18503333f);
	const float4 kBlueFloat4  = float4(0.10667330f, 12.64194608f, -60.58204836f, 110.36276771f);
	const float2 kRedFloat2   = float2(-152.94239396f, 59.28637943f);
	const float2 kGreenFloat2 = float2(4.27729857f, 2.82956604f);
	const float2 kBlueFloat2  = float2(-89.90310912f, 27.34824973f);

	x         = std::clamp(x, 0.0f, 1.0f);
	float4 v4 = float4(1.0, x, x * x, x * x * x);
	float2 v2 = v4.zw() * v4.z;
	return float3(dot(v4, kRedFloat4) + dot(v2, kRedFloat2),
		dot(v4, kGreenFloat4) + dot(v2, kGreenFloat2),
		dot(v4, kBlueFloat4) + dot(v2, kBlueFloat2));
}

void histogram(ui::Ui &ui, FpsHistogramWidget widget)
{
	auto cursor = widget.rect.pos + widget.rect.size;
	painter_draw_color_rect(*ui.painter, widget.rect, u32_invalid, ColorU32::from_floats(0.0, 0.0, 0.0, 0.5));

	for (u32 i_time = 0; i_time < exo::Array::len(widget.histogram->frame_times); ++i_time) {
		auto dt = widget.histogram->frame_times[i_time];
		if (cursor.x < widget.rect.pos.x) {
			break;
		}

		const auto target_fps     = 144.0f;
		const auto max_frame_time = 1.0f / 15.0f; //  in seconds

		float  rect_width = dt / (1.0f / target_fps);
		double height_factor =
			(std::log2(dt) - std::log2(1.0f / target_fps)) / (std::log2(max_frame_time) - std::log2(1.0 / target_fps));
		float  rect_height  = std::clamp(float(height_factor), 0.1f, 1.0f) * widget.rect.size[1];
		float3 rect_color   = turbo_colormap(dt / (1.0f / 120.0f));
		auto   rect_color_u = ColorU32::from_floats(rect_color[0], rect_color[1], rect_color[2], 1.0f);

		rect_width  = std::max(rect_width, 1.0f);
		rect_height = std::max(rect_height, 1.0f);

		cursor[0] -= rect_width;

		auto rect = Rect{
			.pos  = {std::ceil(cursor.x), std::ceil(cursor.y - rect_height)},
			.size = {rect_width, rect_height},
		};
		painter_draw_color_rect(*ui.painter, rect, u32_invalid, rect_color_u);
	}

	const usize FRAMES_FOR_FPS = 30;
	float       fps            = 0.0f;
	for (u32 i_time = 0; i_time < exo::Array::len(widget.histogram->frame_times) && i_time < FRAMES_FOR_FPS; ++i_time) {
		fps += widget.histogram->frame_times[i_time];
	}
	fps = float(std::min(exo::Array::len(widget.histogram->frame_times), FRAMES_FOR_FPS)) / fps;

	auto fps_string = fmt::format("{}", fps);
	painter_draw_label(*ui.painter, widget.rect, u32_invalid, *ui.theme.main_font, fps_string.c_str());
}

void FpsHistogram::push_time(float dt)
{
	for (u32 i_time = 1; i_time < exo::Array::len(this->frame_times); ++i_time) {
		this->frame_times[exo::Array::len(this->frame_times) - i_time] =
			this->frame_times[exo::Array::len(this->frame_times) - i_time - 1];
	}
	this->frame_times[0] = dt;
}
} // namespace custom_ui
