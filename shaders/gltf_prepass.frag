layout (location = 2) in vec2 inUV0;
layout (location = 6) in vec4 inLightPosition;

layout (location = 0) out uint outMipLevel;

layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;


float mipmap_level(vec2 uv, float texture_size)
{
    vec2 dx = dFdx(uv * texture_size);
    vec2 dy = dFdy(uv * texture_size);
    float d = max(dot(dx, dx), dot(dy, dy));
    return 0.5 * log2(d);
}

void main()
{
    vec4 base_color = texture(baseColorTexture, inUV0);
    if (base_color.a < 0.5) {
        discard;
    }

    vec3 shadowmap_uv = inLightPosition.xyz / inLightPosition.w;
    shadowmap_uv = shadowmap_uv * 0.5 + 0.5;

    float mip_level = mipmap_level(shadowmap_uv.xy, 16.0 * 1024.0);
    mip_level = round(max(mip_level, 0.0));

    outMipLevel = uint(mip_level);
}
