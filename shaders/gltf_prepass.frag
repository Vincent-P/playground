layout (location = 2) in vec2 inUV0;

layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;

void main()
{
    vec4 base_color = texture(baseColorTexture, inUV0);
    if (base_color.a < 0.5) {
        discard;
    }
}
