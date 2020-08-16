#include "types.h"
#include "globals.h"
#include "sky.h"

layout(location = 0) out vec4 outColor;

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

    float3 ClipSpace = float3((float2(pixPos) / float2(global.resolution))*float2(2.0, 2.0) - float2(1.0, 1.0), 1.0);
    float4 HPos = inverse(global.camera_proj * global.camera_view) * float4(ClipSpace, 1.0);

    float3 WorldDir = normalize(HPos.xyz / HPos.w - global.camera_pos);
    float3 WorldPos = global.camera_pos + float3(0, Atmosphere.BottomRadius, 0);

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
    if (viewHeight < Atmosphere.TopRadius && DepthBufferValue == 1.0f)
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
    if (DepthBufferValue == 1.0f)
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
