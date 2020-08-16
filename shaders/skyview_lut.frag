#include "types.h"
#include "globals.h"
#include "sky.h"

layout(location = 0) out vec4 outColor;

void main()
{
    float2 pixPos = gl_FragCoord.xy;
    AtmosphereParameters Atmosphere = GetAtmosphereParameters();

    float3 ClipSpace = float3((pixPos / float2(192.0,108.0))*float2(2.0, 2.0) - float2(1.0, 1.0), 1.0);
    float4 HPos = inverse(global.camera_proj * global.camera_view) * float4(ClipSpace, 1.0);

    float3 WorldDir = normalize(HPos.xyz / HPos.w - global.camera_pos);
    float3 WorldPos = global.camera_pos + float3(0, Atmosphere.BottomRadius, 0);


    float2 uv = pixPos / float2(192.0, 108.0);

    float viewHeight = length(WorldPos);

    float viewZenithCosAngle;
    float lightViewCosAngle;
    UvToSkyViewLutParams(Atmosphere, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv);

    float3 SunDir;
    {
        float3 UpVector = WorldPos / viewHeight;
        float sunZenithCosAngle = dot(UpVector, global.sun_direction);
        SunDir = normalize(float3(sqrt(1.0 - sunZenithCosAngle * sunZenithCosAngle), 0.0, sunZenithCosAngle));
    }

    WorldPos = float3(0.0f, 0.0f, viewHeight);

    float viewZenithSinAngle = sqrt(1 - viewZenithCosAngle * viewZenithCosAngle);
    WorldDir = float3(
        viewZenithSinAngle * lightViewCosAngle,
        viewZenithSinAngle * sqrt(1.0 - lightViewCosAngle * lightViewCosAngle),
        viewZenithCosAngle);


    // Move to top atmospehre
    if (!MoveToTopAtmosphere(WorldPos, WorldDir, Atmosphere.TopRadius))
    {
        // Ray is not intersecting the atmosphere
        outColor = float4(1, 0, 0, 1);
        return;
    }

    const bool ground = false;
    const float SampleCountIni = 30;
    const float DepthBufferValue = -1.0;
    const bool VariableSampleCount = true;
    const bool MieRayPhase = true;
    SingleScatteringResult ss = IntegrateScatteredLuminance(pixPos, WorldPos, WorldDir, SunDir, Atmosphere, ground, SampleCountIni, DepthBufferValue, VariableSampleCount, MieRayPhase, 9000000.0f);

    float3 L = ss.L;

    outColor = float4(L, 1);
}
