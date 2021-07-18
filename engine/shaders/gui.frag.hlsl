typedef uint32_t u32;
typedef uint64_t u64;

[[vk::binding(0, 1)]]
Texture2D<float4> bindless_textures[];

[[vk::binding(0, 3)]]
cbuffer Options
{
    float2 scale;
    float2 translation;
    u64 vertices_ptr_ptr;
    u32 first_vertex;
    u32 pad4;
    uint texture_binding_per_draw[64];
};

struct ImGuiVertex
{
    float2 position;
    float2 uv;
    uint color;
    uint pad00;
    uint pad01;
    uint pad10;
};

[[vk::binding(1, 3)]]
ByteAddressBuffer vertices;

struct PushConstant
{
    u32 draw_id;
    u32 gui_texture_id;
};

[[vk::push_constant]]
PushConstant push_constants;

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PS_INPUT input) : SV_Target
{
    float4 color = bindless_textures[push_constants.gui_texture_id].Sample(input.uv);
    return float4(color.rgb * color.a, color.a);
}