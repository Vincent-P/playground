#include "types.h"
#include "constants.h"
#include "globals.h"
#include "raytracing.h"
#include "bvh.h"
#include "pbr.h"

layout(set = 1, binding = 1) buffer VertexBuffer {
    Vertex vertices[];
};

layout(set = 1, binding = 2) buffer RenderMeshesBuffer {
    RenderMeshData render_meshes[];
};

layout(set = 1, binding = 3) buffer IndexBuffer {
    u32 indices[];
};

layout(set = 1, binding = 4) buffer MaterialBuffer {
    Material materials[];
};

layout(set = 1, binding = 5) buffer BVHNodesBuffer {
    BVHNode nodes[];
};

layout(set = 1, binding = 6) buffer BVHFacesBuffer {
    Face faces[];
};


struct HitInfo
{
    float d;
    u32 i_material;
    u32 i_face;
    float3 barycentrics;
    u32 box_inter_count;
};

bool bvh_closest_hit(Ray ray, out HitInfo hit_info)
{
    float3 inv_ray_dir = 1.0 / ray.direction;

    hit_info.d = 1.0 / 0.0;
    hit_info.i_material = 0;
    hit_info.i_face = 0;
    hit_info.barycentrics = float3(0.0);
    hit_info.box_inter_count = 0;

    uint i_node = 0;
    while (i_node != u32_invalid)
    {
        BVHNode node = nodes[i_node];

        // leaf
        if (node.face_index != u32_invalid)
        {
            Face face                  = faces[node.face_index];
            RenderMeshData render_mesh = render_meshes[face.mesh_id];

            Vertex v0 = vertices[indices[face.first_index + 0]];
            Vertex v1 = vertices[indices[face.first_index + 1]];
            Vertex v2 = vertices[indices[face.first_index + 2]];

            float3 p0 = (render_mesh.transform * float4(v0.position, 1.0)).xyz;
            float3 p1 = (render_mesh.transform * float4(v1.position, 1.0)).xyz;
            float3 p2 = (render_mesh.transform * float4(v2.position, 1.0)).xyz;

            float d = 0.0;
            Triangle tri;
            tri.v0 = p0;
            tri.e0 = p1 - p0;
            tri.e1 = p2 - p0;
            float3 barycentrics = triangle_intersection(ray, tri, d);

            if (0.0 < d && d < hit_info.d)
            {
                hit_info.d            = d;
                hit_info.barycentrics = barycentrics;
                hit_info.i_material   = render_mesh.i_material;
                hit_info.i_face       = node.face_index;
            }
        }

        // internal node
        else
        {
            if (fast_box_intersection(node.bbox_min, node.bbox_max, ray, inv_ray_dir))
            {
                hit_info.box_inter_count += 1;
                // if the ray intersects this node bounding box then i_node += 1 will continue depth first traversal
                i_node += 1;
                continue;
            }
        }

        // the ray missed the triangle or the node's bounding box, skip the subtree
        i_node = node.next_node;
    }

    return hit_info.d < 1.0 / 0.0;
}

vec3 heatmap(float value)
{
    vec3 heat;
    heat.r = smoothstep(0.5, 0.8, value);
    if(value >= 0.90) {
        heat.r *= (1.1 - value) * 5.0;
    }
    if(value > 0.7) {
        heat.g = smoothstep(1.0, 0.7, value);
    } else {
        heat.g = smoothstep(0.0, 0.7, value);
    }
    heat.b = smoothstep(1.0, 0.0, value);
    if(value <= 0.3) {
        heat.b *= value / 0.3;
    }
    return heat;
}

vec3 TurboColormap(in float x) {
  const vec4 kRedVec4 = vec4(0.13572138, 4.61539260, -42.66032258, 132.13108234);
  const vec4 kGreenVec4 = vec4(0.09140261, 2.19418839, 4.84296658, -14.18503333);
  const vec4 kBlueVec4 = vec4(0.10667330, 12.64194608, -60.58204836, 110.36276771);
  const vec2 kRedVec2 = vec2(-152.94239396, 59.28637943);
  const vec2 kGreenVec2 = vec2(4.27729857, 2.82956604);
  const vec2 kBlueVec2 = vec2(-89.90310912, 27.34824973);

  x = clamp(x, 0.0, 1.0);
  vec4 v4 = vec4( 1.0, x, x * x, x * x * x);
  vec2 v2 = v4.zw * v4.z;
  return vec3(
    dot(v4, kRedVec4)   + dot(v2, kRedVec2),
    dot(v4, kGreenVec4) + dot(v2, kGreenVec2),
    dot(v4, kBlueVec4)  + dot(v2, kBlueVec2)
  );
}


layout (location = 0) in float4 i_vertex_color;
layout (location = 1) in float2 i_uv;
layout (location = 2) in float4 i_world_pos;
layout (location = 3) in float3 i_normal;
layout(location = 0) out float4 o_color;
void main()
{
    float2 screen_uv  = gl_FragCoord.xy;
    float depth = gl_FragCoord.z;
    int2 pixel_pos = int2(floor(screen_uv * globals.resolution));

    // initialize a random number state based on frag coord and frame
    uint rng_seed = init_seed(pixel_pos, globals.frame_count);

    // Create a pixel ray
    Ray ray;
    ray.origin    = globals.camera_position.xyz;
    ray.direction = normalize(i_world_pos.xyz - ray.origin.xyz);

    // accumulators
    float3 color = float3(0.0);
    float3 throughput = float3(1.0);
    uint hit_count = 0;

    const uint MAX_BOUNCE = 3;

    HitInfo hit_info;
    hit_info.d = distance(i_world_pos.xyz, ray.origin.xyz);
    hit_info.i_material = 0;
    hit_info.i_face = 0;
    hit_info.barycentrics = float3(0.);
    hit_info.box_inter_count = 0;

    RenderMeshData render_mesh = render_meshes[push_constants.render_mesh_idx];
    Material material = materials[render_mesh.i_material];

    float3 surface_normal = i_normal;
    float2 uv0            = i_uv;
    float4 color0 = i_vertex_color;

    // The intersection and attributes interpolation is done at the end of the loop, because the first rays are rasterized
    for (u32 i_bounce = 0; i_bounce < MAX_BOUNCE; i_bounce += 1)
    {
        hit_count += hit_info.box_inter_count;

        float3 tangent;
        float3 bitangent;
        make_orthogonal_coordinate_system(surface_normal, bitangent, tangent);
        mat3 tangent_to_world = mat3(tangent, bitangent, surface_normal);
        mat3 world_to_tangent = transpose(tangent_to_world);

        float4 base_color = color0;
        if (material.base_color_texture != u32_invalid)
        {
            base_color *= texture(global_textures[nonuniformEXT(globals.render_texture_offset + material.base_color_texture)], uv0);
        }

        float3 albedo   = material.base_color_factor.rgb * base_color.rgb;
        const float emissive_strength = 50.0;
        float3 emissive = emissive_strength * material.emissive_factor.rgb;
        float metallic  = material.metallic_factor;
        float roughness = max(material.roughness_factor, 0.1);

        if (base_color.a < 0.5)
        {
            ray.origin = ray.origin + hit_info.d * 1.0001 * ray.direction;
            continue;
        }

        // -- Sample BRDF for a new ray direction

        float3 wo = world_to_tangent * -ray.direction;
        float3 diffuse_wi  = lambert_sample(rng_seed);
        float3 specular_wi = smith_ggx_sample(wo, roughness*roughness, rng_seed);

        const float specular_chance = 0.5;
        float do_specular = float(random_float_01(rng_seed) > specular_chance);

        float3 wi = mix(diffuse_wi, specular_wi, do_specular);

        float3 kD;
        float3 specular_brdf = smith_ggx_brdf(float3(0, 0, 1), wo, wi, albedo, roughness*roughness, metallic, kD);
        float3 diffuse_brdf = lambert_brdf(wo, wi, albedo);
        float3 brdf = mix(diffuse_brdf, specular_brdf, do_specular);

        float pdf  = 0.5 * lambert_pdf(wo, wi) + 0.5 * smith_ggx_pdf(wo, wi, roughness*roughness);

        // -- Evaluate the BRDF divided by the PDF

        // Accumulate light
        color    += emissive * throughput;
        throughput *= brdf / pdf;

        // Debug output
        #if 0
        color = surface_normal * 0.5 + 0.5;
        break;
        #endif

        // Russian roulette, terminate path that won't contribute much
        // Survivors have their value boosted to make up for fewer samples being in the average.
        float p = max3(throughput);
        if (random_float_01(rng_seed) > p)
            break;
        // Add the energy we 'lose' by randomly terminating paths
        throughput *= 1.0f / p;

        // Bounce the surviving ray
        ray.origin    = (ray.origin + hit_info.d * ray.direction) + surface_normal * 0.001;
        ray.direction = tangent_to_world * wi;

        if (!bvh_closest_hit(ray, hit_info))
        {
            float3 background_color = float3(0.846, 0.933, 0.949);
            background_color *= 100.0;
            color   += background_color * throughput;
            break;
        }

        // -- Fetch hit info and vertex attributes manually
        Face face   = faces[hit_info.i_face];
        material    = materials[hit_info.i_material];
        render_mesh = render_meshes[face.mesh_id];

        Vertex v0 = vertices[indices[face.first_index + 0]];
        Vertex v1 = vertices[indices[face.first_index + 1]];
        Vertex v2 = vertices[indices[face.first_index + 2]];

        float3 p0 = (render_mesh.transform * float4(v0.position, 1.0)).xyz;
        float3 p1 = (render_mesh.transform * float4(v1.position, 1.0)).xyz;
        float3 p2 = (render_mesh.transform * float4(v2.position, 1.0)).xyz;

        float3 e1 = p1 - p0;
        float3 e2 = p2 - p0;

        surface_normal = hit_info.barycentrics.x*v0.normal+hit_info.barycentrics.y*v1.normal+hit_info.barycentrics.z*v2.normal;
        uv0    = hit_info.barycentrics.x*v0.uv0   +hit_info.barycentrics.y*v1.uv0   +hit_info.barycentrics.z*v2.uv0;
        color0 = hit_info.barycentrics.x*v0.color0+hit_info.barycentrics.y*v1.color0+hit_info.barycentrics.z*v2.color0;

        // Compute normal from triangle
        float3 triangle_normal =  normalize(cross(e1, e2));
    }

    float hit_ratio = clamp(float(hit_count) / 500, 0.0, 1.0);
    color = TurboColormap(hit_ratio);

    o_color = float4(color, 1.0);
}
