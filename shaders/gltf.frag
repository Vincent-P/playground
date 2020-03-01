#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;

layout (push_constant) uniform Material
{
    vec4 baseColorFactor;
} material;

layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;

layout (location = 0) out vec4 outColor;

void main()
{
    vec3 light_dir = normalize(vec3(1, 2, 1));

    float ambient = 0.5;
    float diffuse = max(dot(inNormal, light_dir), 0.0f);

    vec3 color = (ambient + diffuse) * texture(baseColorTexture, inUV0).xyz;
    outColor = vec4(color, 1);
}
