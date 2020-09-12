layout (location = 0) in vec2 inUV0;

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
}
