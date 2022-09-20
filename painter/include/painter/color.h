#pragma once
#include <exo/maths/numerics.h>

union ColorU32
{
	struct Components
	{
		u8 r;
		u8 g;
		u8 b;
		u8 a;
	} comps;
	u32 raw;

	static constexpr ColorU32 from_raw(u32 raw)
	{
		ColorU32 res;
		res.raw = raw;
		return res;
	}

	static constexpr ColorU32 from_uints(u8 r, u8 g, u8 b, u8 a = 255)
	{
		ColorU32 res;
		res.comps.r = r;
		res.comps.g = g;
		res.comps.b = b;
		res.comps.a = a;
		return res;
	}

	static constexpr ColorU32 from_floats(float r, float g, float b, float a = 1.0f)
	{
		return ColorU32::from_uints(u8(r * 255.0f), u8(g * 255.0f), u8(b * 255.0f), u8(a * 255.0f));
	}

	static constexpr ColorU32 from_greyscale(float grey) { return ColorU32::from_floats(grey, grey, grey); }
	static constexpr ColorU32 from_greyscale(u8 grey) { return ColorU32::from_uints(grey, grey, grey); }

	static constexpr auto red() { return ColorU32::from_uints(255, 0, 0); }
	static constexpr auto green() { return ColorU32::from_uints(0, 255, 0); }
	static constexpr auto blue() { return ColorU32::from_uints(0, 0, 255); }
};
