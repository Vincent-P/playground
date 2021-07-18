typedef uint32_t u32;
typedef uint64_t u64;

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

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

PS_INPUT main(uint vertex_id : SV_VertexID)
{
    ImGuiVertex vertex = vertices.Load<ImGuiVertex>((first_vertex + vertex_id) * 32);

    PS_INPUT output;
    output.position = float4(vertex.position * scale + translation, 0.0, 1.0);
    output.uv = vertex.uv;
    output.color = vertex.color;
    return output;
}
