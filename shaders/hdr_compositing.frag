layout(set = 1, binding = 0) uniform sampler2D hdr_buffer;

layout(set = 1, binding = 1) uniform DO
{
    uint selected;
    float exposure;
} debug;


layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

void main()
{
    vec3 hdr = texture(hdr_buffer, inUV).rgb;

    if (any(lessThan(hdr, vec3(0.0))))
    {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }

    if (debug.selected == 1)
    {
        hdr = vec3(1.0) - exp(-hdr * debug.exposure);
    }
    else if (debug.selected == 2)
    {
        hdr = clamp(hdr, 0.0, 1.0);
    }
    else
    {
        hdr = hdr / (hdr + 1.0);
    }

    outColor = vec4(hdr, 1);
}
