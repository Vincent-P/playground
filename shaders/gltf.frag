#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;

layout (push_constant) uniform Material
{
    vec4 baseColorFactor;
} material;


layout (set = 1, binding = 1) uniform UBODebug {
    uint selected;
} debug;


layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 2, binding = 2) uniform sampler2D normalTexture;
layout(set = 2, binding = 3) uniform sampler2D metallicRoughnessTexture;

layout (location = 0) out vec4 outColor;

vec3 getNormal()
{
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    vec3 tangentNormal = texture(normalTexture, inUV0).xyz * 2.0 - 1.0;

    vec3 q1 = dFdx(inWorldPos);
    vec3 q2 = dFdy(inWorldPos);
    vec2 st1 = dFdx(inUV0);
    vec2 st2 = dFdy(inUV0);

    vec3 N = normalize(inNormal);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main()
{
    vec3 light_dir = normalize(vec3(1, 2, 1));

    vec3 normal = getNormal();
    outColor = vec4(normal, 1);

    float ambient = 0.5;
    float diffuse = max(dot(normal, light_dir), 0.0f);
    vec3 color = (ambient + diffuse) * texture(baseColorTexture, inUV0).xyz;

    switch (debug.selected)
    {
        case 0: outColor = vec4(color, 1); break;
        case 1: outColor = vec4(texture(baseColorTexture, inUV0).xyz, 1); break;
        case 2: outColor = vec4(texture(normalTexture, inUV0).xyz, 1); break;
        case 3: outColor = vec4(texture(metallicRoughnessTexture, inUV0).xyz, 1); break;
    }
}
