#pragma shader_stage(compute)
#include "types.h"
#include "constants.h"
#include "globals.h"
#include "raytracing.h"
#include "bvh.h"
#include "pbr.h"

layout(set = 1, binding = 0) uniform Options {
    uint storage_output_frame;
};

layout (set = 1, binding = 1, rgba16f) uniform image2D output_frame;

layout(set = 1, binding = 2) buffer VertexBuffer {
    Vertex vertices[];
};

layout(set = 1, binding = 3) buffer IndexBuffer {
    u32 indices[];
};

layout(set = 1, binding = 4) buffer RenderMeshesBuffer {
    RenderMeshData render_meshes[];
};

layout(set = 1, binding = 5) buffer MaterialBuffer {
    Material materials[];
};

layout(set = 1, binding = 6) buffer BVHNodesBuffer {
    BVHNode nodes[];
};

layout(set = 1, binding = 7) buffer BVHFacesBuffer {
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
            float d       = 0.0;
            float3 normal = float3(0.0);
            Box bbox;
            bbox.center     = (node.bbox_min + node.bbox_max) * 0.5;
            bbox.radius     = (node.bbox_max - node.bbox_min) * 0.5;
            bbox.inv_radius = 1 / bbox.radius;
            if (ray_box_intersection(bbox, ray, d, normal, inv_ray_dir))
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

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    uint  local_idx  = gl_LocalInvocationIndex;
    uint3 global_idx = gl_GlobalInvocationID;
    uint3 group_idx  = gl_WorkGroupID;

    int2 pixel_pos = int2(global_idx.xy);
    int2 output_size = imageSize(output_frame);

    bool im_out = any(greaterThan(pixel_pos, output_size));
    #if 0
    const uint stride = 10;
    im_out = im_out || (pixel_pos.x % stride != globals.frame_count % stride && pixel_pos.y % stride != globals.frame_count % stride);
    #endif
    if (im_out)
    {
        return;
    }

    // initialize a random number state based on frag coord and frame
    uint rng_seed = init_seed(pixel_pos, globals.frame_count);

    float2 screen_uv = float2(pixel_pos) / globals.resolution;
    float3 clip_space = float3(screen_uv * 2.0 - 1.0, 0.0001);
    float4 h_pos      = globals.camera_view_inverse * globals.camera_projection_inverse * float4(clip_space, 1.0);
    h_pos /= h_pos.w;

    // Create a pixel ray
    Ray ray;
    ray.origin    = globals.camera_position.xyz;
    ray.direction = normalize(h_pos.xyz - ray.origin);
    float3 inv_ray_dir = 1.0 / ray.direction;

    // accumulators
    const uint MAX_BOUNCE = 3;
    HitInfo hit_info;
    float3 o_color = float3(0.0);
    float3 throughput = float3(1.0);

    for (u32 i_bounce = 0; i_bounce < MAX_BOUNCE; i_bounce += 1)
    {
        if (!bvh_closest_hit(ray, hit_info))
        {
            float3 background_color = float3(0.846, 0.933, 0.949);
            o_color   += background_color * throughput;
            break;
        }

        // -- Fetch hit info and vertex attributes manually
        Material material          = materials[hit_info.i_material];
        Face face                  = faces[hit_info.i_face];
        RenderMeshData render_mesh = render_meshes[face.mesh_id];

        Vertex v0 = vertices[indices[face.first_index + 0]];
        Vertex v1 = vertices[indices[face.first_index + 1]];
        Vertex v2 = vertices[indices[face.first_index + 2]];

        float3 p0 = (render_mesh.transform * float4(v0.position, 1.0)).xyz;
        float3 p1 = (render_mesh.transform * float4(v1.position, 1.0)).xyz;
        float3 p2 = (render_mesh.transform * float4(v2.position, 1.0)).xyz;

        float3 e1 = p1 - p0;
        float3 e2 = p2 - p0;

        // Compute (bi)tangent for normal map
        float2 delta_uv1 = v1.uv0 - v0.uv0;
        float2 delta_uv2 = v2.uv0 - v0.uv0;
        float r = 1.0f / (delta_uv1.x * delta_uv2.y - delta_uv1.y * delta_uv2.x);
        float3 tangent = (e1 * delta_uv2.y - e2 * delta_uv1.y)*r;
        float3 bitangent = (e2 * delta_uv1.x - e1 * delta_uv2.x)*r;

        float3 normal = hit_info.barycentrics.x*v0.normal+hit_info.barycentrics.y*v1.normal+hit_info.barycentrics.z*v2.normal;
        float2 uv0    = hit_info.barycentrics.x*v0.uv0   +hit_info.barycentrics.y*v1.uv0   +hit_info.barycentrics.z*v2.uv0;
        float4 color0 = hit_info.barycentrics.x*v0.color0+hit_info.barycentrics.y*v1.color0+hit_info.barycentrics.z*v2.color0;

        // Compute normal from triangle
        // normal =  normalize(cross(e1, e2));

        if (material.normal_texture != u32_invalid)
        {
            float3 normal_map = texture(global_textures[nonuniformEXT(10 + material.normal_texture)], uv0).xyz;
            normal_map = normal_map * 2.0 + 1.0;
            mat3 TBN = mat3(normalize(tangent), -normalize(bitangent), normalize(normal));
            normal = normalize(TBN * normal_map);
        }

        float4 base_color = color0;
        if (material.base_color_texture != u32_invalid)
        {
            base_color *= texture(global_textures[nonuniformEXT(10 + material.base_color_texture)], uv0);
        }

        float3 albedo   = material.base_color_factor.rgb * base_color.rgb;
        const float emissive_strength = 10.0;
        float3 emissive = emissive_strength * material.emissive_factor.rgb;
        float metallic  = material.metallic_factor;
        float roughness = material.roughness_factor;

        // -- Sample BRDF for a new ray direction

        if (base_color.a < 0.5)
        {
            ray.origin = ray.origin + hit_info.d * 1.0001 * ray.direction;
            continue;
        }

        float3 diffuse_direction = normalize(normal + random_unit_vector(rng_seed));
        float3 specular_direction = reflect(ray.direction, normal);

        // BRDF
        // albedo: point's color
        // N: point's normal
        // V: scattered light (view vector from point to camera)
        // metallic:
        // roughness:
        // L: incoming light (light direction from light to point)
        float3 N = normal;
        float3 V = normal;
        float3 L = ray.direction;
        float3 H = normalize(L + V);

        float3 F0 = float3(0.04);
        F0        = mix(F0, albedo, metallic);
        // Fresnel Equation
        float3 F  = fresnel_shlick(max(dot(H, V), 0.0), F0);

        float3 kS = F;
        float3 kD = float3(1.0) - kS;
        // metallic materials dont have diffuse reflections
        kD *= 1.0 - metallic;

        float3 lambert_diffuse = albedo / PI;

        // implicitly contains kS
        float cookterrance_specular =  distribution_ggx(N, H, roughness) * geometry_smith(N, V, L, roughness)
                                     / /* -------------------------------------------------------------------------*/
                                        max(             4.0 * safe_dot(N, V) * safe_dot(N, L)            , 0.001);


        // (kD * lambert_diffuse + kS * cookterrance_specular);
        float3 brdf = mix(lambert_diffuse, kS * cookterrance_specular, length(kS));
        float is_specular = (random_float_01(rng_seed) < length(kS)) ? 1.0 : 0.0;
        float3 scattered_ray = normalize(mix(diffuse_direction, specular_direction, (1.0-roughness)*(1.0-roughness)));
        // BRDF end

        // -- Get the PDF of the BRDF


        o_color   += emissive * throughput;

        // -- Evaluate the BRDF divided by the PDF
        throughput *= albedo;

        // Debug output
        #if 0
        o_color = hit_normal * 0.5 + 0.5;
        o_color = albedo;
        o_color = ray.direction * 0.5 + 0.5;
        o_color = heatmap(float(box_inter_count) / 1000.0);
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
        ray.origin    = (ray.origin + hit_info.d * ray.direction) + normal * 0.001;
        ray.direction = scattered_ray;

    }

    imageStore(output_frame, pixel_pos, vec4(o_color, 1.0));
}
