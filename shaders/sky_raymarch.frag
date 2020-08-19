#include "types.h"
#include "globals.h"

layout(set = 1, binding = 1) uniform sampler2D TransmittanceLutTexture; // sampler linear clamp
layout(set = 1, binding = 2) uniform sampler2D SkyViewLutTexture; // sampler linear clamp
layout(set = 1, binding = 3) uniform sampler2D ViewDepthTexture; // texel fetch

layout(location = 1) in vec3 inViewRay;
layout(location = 0) out vec4 outColor;

#if 0
#include "atmosphere.h"

layout (set = 1, binding = 0) uniform AtmosphereUniform {
    AtmosphereParameters atmosphere;
};

void main()
{
    float2 pixel_pos = gl_FragCoord.xy;
    float2 uv = pixel_pos / global.resolution;

    float3 clip_space = float3(uv * 2.0 - float2(1.0), 0.01);
    float4 h_pos      = global.camera_inv_view_proj * float4(clip_space, 1.0);
    h_pos.xyz /= h_pos.w;

    float3 world_dir = normalize(h_pos.xyz - global.camera_pos);
    float3 world_pos = global.camera_pos + float3(0.0, atmosphere.bottom_radius, 0.0);

    outColor = float4(world_dir, 1.0);
    return;

    float r = length(world_pos);
    float3 up = normalize(world_pos);
    float mu = dot(world_dir, up);

    float cos_lightview;

    float3 side = normalize(cross(up, world_dir));		// assumes non parallel vectors
    float3 forward = normalize(cross(side, up));	// aligns toward the sun light but perpendicular to up vector
    float2 light_on_plane = float2(dot(global.sun_direction, forward), dot(global.sun_direction, side));
    light_on_plane = normalize(light_on_plane);
    cos_lightview = light_on_plane.x;

    bool intersects_ground = intersects_ground(atmosphere, r, mu);
    float2 skyview_uv = mu_coslightview_to_uv(atmosphere, intersects_ground, r, mu, cos_lightview);
    outColor = float4(skyview_uv.y, intersects_ground ? 1.0 : 0.0, 0.0, 1.0);
}

#else

#include "sky.h"

#define COLORED_TRANSMITTANCE_ENABLED 0
#define FASTSKY_ENABLED 1
#define FASTAERIALPERSPECTIVE_ENABLED 0

void main()
{
#if COLORED_TRANSMITTANCE_ENABLED
    output.Transmittance = float4(0, 0, 0, 1);
#endif

    ivec2 pixPos = ivec2(gl_FragCoord.xy);
    AtmosphereParameters Atmosphere = GetAtmosphereParameters();

    float3 ClipSpace = float3((float2(pixPos) / float2(global.resolution))*float2(2.0, 2.0) - float2(1.0, 1.0), 0.001);
    float4 HPos = global.camera_inv_view_proj * float4(ClipSpace, 1.0);
    HPos /= HPos.w;

    float3 WorldDir = normalize(HPos.xyz - global.camera_pos);

    /*
    outColor = float4(WorldDir, 1.0);
    return;
    */

    float3 WorldPos = global.camera_pos / 1000.0f + float3(0, Atmosphere.BottomRadius, 0);

    float DepthBufferValue = -1.0;

    //if (pixPos.x < 512 && pixPos.y < 512)
    //{
    //	output.Luminance = float4(MultiScatTexture.SampleLevel(samplerLinearClamp, pixPos / float2(512, 512), 0).rgb, 1.0);
    //	return output;
    //}

    float viewHeight = length(WorldPos);
    float3 L = float3(0.0);
    DepthBufferValue = texelFetch(ViewDepthTexture, pixPos, 0).r;
#if FASTSKY_ENABLED
    if (viewHeight < Atmosphere.TopRadius && DepthBufferValue <= 0.001)
    {
        float2 uv;
        float3 UpVector = normalize(WorldPos);
        float viewZenithCosAngle = dot(WorldDir, UpVector);

        float3 sideVector = normalize(cross(UpVector, WorldDir));		// assumes non parallel vectors
        float3 forwardVector = normalize(cross(sideVector, UpVector));	// aligns toward the sun light but perpendicular to up vector
        float2 lightOnPlane = float2(dot(global.sun_direction, forwardVector), dot(global.sun_direction, sideVector));
        lightOnPlane = normalize(lightOnPlane);
        float lightViewCosAngle = lightOnPlane.x;

        bool IntersectGround = raySphereIntersectNearest(WorldPos, WorldDir, float3(0, 0, 0), Atmosphere.BottomRadius) >= 0.0f;

        SkyViewLutParamsToUv(Atmosphere, IntersectGround, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv);

        //output.Luminance = float4(SkyViewLutTexture.SampleLevel(samplerLinearClamp, pixPos / float2(gResolution), 0).rgb + GetSunLuminance(WorldPos, WorldDir, Atmosphere.BottomRadius), 1.0);
        outColor = float4(10.0 * textureLod(SkyViewLutTexture, uv, 0).rgb + GetSunLuminance(WorldPos, WorldDir, Atmosphere.BottomRadius), 1.0);
        return;
    }
#else
    if (DepthBufferValue <= 0.1)
        L += GetSunLuminance(WorldPos, WorldDir, Atmosphere.BottomRadius);
#endif

#if FASTAERIALPERSPECTIVE_ENABLED

#if COLORED_TRANSMITTANCE_ENABLED
#error The FASTAERIALPERSPECTIVE_ENABLED path does not support COLORED_TRANSMITTANCE_ENABLED.
#else

    ClipSpace = float3((pixPos / float2(gResolution))*float2(2.0, -2.0) - float2(1.0, -1.0), DepthBufferValue);
    float4 DepthBufferWorldPos = mul(gSkyInvViewProjMat, float4(ClipSpace, 1.0));
    DepthBufferWorldPos /= DepthBufferWorldPos.w;
    float tDepth = length(DepthBufferWorldPos.xyz - (WorldPos + float3(0.0, 0.0, -Atmosphere.BottomRadius)));
    float Slice = AerialPerspectiveDepthToSlice(tDepth);
    float Weight = 1.0;
    if (Slice < 0.5)
    {
        // We multiply by weight to fade to 0 at depth 0. That works for luminance and opacity.
        Weight = clamp(Slice * 2.0, 0.0, 1.0);
        Slice = 0.5;
    }
    float w = sqrt(Slice / AP_SLICE_COUNT);	// squared distribution

    const float4 AP = Weight * AtmosphereCameraScatteringVolume.SampleLevel(samplerLinearClamp, float3(pixPos / float2(gResolution), w), 0);
    L.rgb += AP.rgb;
    float Opacity = AP.a;

    output.Luminance = float4(L, Opacity);
    //output.Luminance *= frac(clamp(w*AP_SLICE_COUNT, 0, AP_SLICE_COUNT));
#endif

#else // FASTAERIALPERSPECTIVE_ENABLED

    // Move to top atmosphere as the starting point for ray marching.
    // This is critical to be after the above to not disrupt above atmosphere tests and voxel selection.
    if (!MoveToTopAtmosphere(WorldPos, WorldDir, Atmosphere.TopRadius))
    {
        // Ray is not intersecting the atmosphere
        outColor = float4(GetSunLuminance(WorldPos, WorldDir, Atmosphere.BottomRadius), 1.0);
        return;
    }

    const bool ground = false;
    const float SampleCountIni = 0.0f;
    const bool VariableSampleCount = true;
    const bool MieRayPhase = true;
    SingleScatteringResult ss = IntegrateScatteredLuminance(pixPos, WorldPos, WorldDir, global.sun_direction, Atmosphere, ground, SampleCountIni, DepthBufferValue, VariableSampleCount, MieRayPhase, 9000000.0f);

    L += ss.L;
    float3 throughput = ss.Transmittance;

#if COLORED_TRANSMITTANCE_ENABLED
    output.Luminance = float4(L, 1.0f);
    output.Transmittance = float4(throughput, 1.0f);
#else
    const float Transmittance = dot(throughput, float3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f));
    outColor = float4(L, 1.0 - Transmittance);
#endif

#endif // FASTAERIALPERSPECTIVE_ENABLED

    return;
}
#endif
