#ifndef PBR_H
#define PBR_H

struct MaterialUniform
{
    vec4 baseColorFactor;
};

vec3 getNormal(vec3 world_pos, vec3 vertex_normal, in sampler2D normal_texture, vec2 uv)
{
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    vec3 tangentNormal = texture(normal_texture, uv).xyz * 2.0 - 1.0;

    vec3 q1 = dFdx(world_pos);
    vec3 q2 = dFdy(world_pos);
    vec2 st1 = dFdx(uv);
    vec2 st2 = dFdy(uv);

    vec3 N = normalize(vertex_normal);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}


#endif
