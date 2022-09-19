#pragma shader_stage(fragment)

#include "base/types.h"
#include "2d/sdf.h"
#include "2d/rects.h"

layout(set = SHADER_UNIFORM_SET, binding = 0) uniform Options {
    float2 scale;
    float2 translation;
    u32 vertices_descriptor_index;
    u32 primitive_bytes_offset;
};

bool is_in_rect(float2 pos, Rect rect)
{
    return !(
        pos.x < rect.position.x
     || pos.x > rect.position.x + rect.size.x
     || pos.y < rect.position.y
     || pos.y > rect.position.y + rect.size.y
        );
}

float3 linear_to_srgb(float3 linear) { return pow(linear, float3(1.0/2.2)); }
float3 srgb_to_linear(float3 srgb)   { return pow(srgb,   float3(2.2)); }

void clip_rect(u32 i_clip_rect)
{
    u32 primitive_offset = primitive_bytes_offset / sizeof_rect;
    Rect clipping_rect = global_buffers_rects[vertices_descriptor_index].rects[primitive_offset + i_clip_rect];
    if (i_clip_rect != u32_invalid && !is_in_rect(gl_FragCoord.xy, clipping_rect))
    {
        discard;
    }
}

float4 textured_rect(u32 i_primitive, u32 corner, float2 uv)
{
    u32 primitive_offset = primitive_bytes_offset / sizeof_textured_rect;
    TexturedRect rect = global_buffers_textured_rects[vertices_descriptor_index].rects[primitive_offset + i_primitive];
    clip_rect(rect.i_clip_rect);

    float alpha = texture(global_textures[nonuniformEXT(rect.texture_descriptor)], uv).r;
    float3 color = float3(1.0);
    color = srgb_to_linear(color);
    return float4(color, alpha);
}

float4 color_rect(u32 i_primitive, u32 corner, float2 uv)
{
    u32 primitive_offset = primitive_bytes_offset / sizeof_color_rect;
    ColorRect rect = global_buffers_color_rects[vertices_descriptor_index].rects[primitive_offset + i_primitive];
    clip_rect(rect.i_clip_rect);

    float4 color = unpackUnorm4x8(rect.color);
    color.rgb = srgb_to_linear(color.rgb);
    return color;
}

float4 sdf_round_rect(u32 i_primitive, u32 corner, float2 uv)
{
    u32 primitive_offset = primitive_bytes_offset / sizeof_color_rect;
    SdfRect rect = global_buffers_sdf_rects[vertices_descriptor_index].rects[primitive_offset + i_primitive];
    clip_rect(rect.i_clip_rect);

    const float radius = 5.0;
    float d = sdRoundedBox((uv - 0.5) * rect.rect.size, rect.rect.size * 0.5, float4(radius));

    float bg_opacity     = clamp(0.5 - d, 0, 1);
    float border_opacity = clamp(0.5 - (d+rect.border_thickness), 0, 1);

    float4 bg_color     = unpackUnorm4x8(rect.color);
    float4 border_color = unpackUnorm4x8(rect.border_color);

    bg_color.rgb = srgb_to_linear(bg_color.rgb);
    border_color.rgb = srgb_to_linear(border_color.rgb);

    float4 res = mix(border_color, bg_color, border_opacity);
    res.a   = res.a * bg_opacity; // restrict shape outside of border
    return res;
}


layout(location = 0) in float2 i_uv;
layout(location = 1) in flat u32 i_primitive_index;
layout(location = 0) out float4 o_color;
void main()
{
    u32 i_primitive    = i_primitive_index & 0x00ffffff;
    u32 corner         = (i_primitive_index & 0x03000000) >> 24;
    u32 primitive_type = (i_primitive_index & 0xfc000000) >> 26;

    if (primitive_type == RectType_Textured)
    {
        o_color = textured_rect(i_primitive, corner, i_uv);
    }
    else if (primitive_type == RectType_Color)
    {
        o_color = color_rect(i_primitive, corner, i_uv);
    }
    else if (primitive_type == RectType_Sdf_RoundRectangle)
    {
	    o_color = sdf_round_rect(i_primitive, corner, i_uv);
    }
    else
    {
        o_color = float4(1, 0, 0, 1);
    }

    // Premultiply alpha
    o_color.rgb = o_color.a * o_color.rgb;
}
