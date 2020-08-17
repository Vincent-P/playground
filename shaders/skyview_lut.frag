#include "types.h"
#include "globals.h"


layout(location = 0) out vec4 outColor;

#if 1
#include "atmosphere.h"

layout (set = 1, binding = 0) uniform AtmosphereUniform {
    AtmosphereParameters atmosphere;
};

layout(set = 1, binding = 1) uniform sampler2D transmittance_lut; // sampler linear clamp
layout(set = 1, binding = 2) uniform sampler2D multiscattering_lut;

float3 get_multiple_scattering(AtmosphereParameters atmosphere, float3 world_pos, float mu)
{
    float2 multiscattering_lut_size = float2(textureSize(multiscattering_lut, 0));
    float2 uv = (float2(mu * 0.5f + 0.5f, (length(world_pos) - atmosphere.bottom_radius) / (atmosphere.top_radius - atmosphere.bottom_radius)));
    uv = clamp(uv, 0.0, 1.0);
    uv = unit_to_uv(uv, multiscattering_lut_size);

    float3 psi_ms = textureLod(multiscattering_lut, uv, 0).rgb;
    return psi_ms;
}

float3 integrate_luminance(AtmosphereParameters atmosphere, float3 p, float3 dir, float3 sun_dir)
{
    float3 throughput = float3(1.0);
    float3 L  = float3(0.0);

    float r    = length(p);
    float mu   = dot(p / r, dir);
    float mu_s = dot(p, sun_dir) / r;
    float v    = dot(dir, sun_dir);

    float mie_phase      = cornette_shanks_phase_function(atmosphere.mie_phase_function_g, -v);
    float rayleigh_phase = rayleigh_phase_function(v);

    const int SAMPLE_COUNT = 100;
    bool  intersects_ground  = intersects_ground(atmosphere, r, mu);
    float nearest_atmosphere = distance_to_nearest_atmosphere(atmosphere, r, mu, intersects_ground);

    float dx =  nearest_atmosphere / SAMPLE_COUNT;

    for (int i = 0; i <= SAMPLE_COUNT; i++)
    {
        float d = i * dx;
        float rd = safe_sqrt(d * d + r * r + 2 * r * d * mu);
        float mu_s_d = (r * mu_s + d * v) / rd;

        MediumRGB medium = sample_medium(atmosphere, rd);
        float3 sample_transmittance = exp(-medium.extinction * dx);

        float3 sun_transmittance = get_transmittance_to_sun(atmosphere, transmittance_lut, rd, mu_s_d);

        float3 T  = sun_transmittance;
        float  S  = intersects_ground ? 0.0 : 1.0; // todo?
        float3 p  = medium.mie_scattering * mie_phase + medium.rayleigh_scattering * rayleigh_phase;
        float3 psi_ms = global.multiple_scattering > 0.0 ? get_multiple_scattering(atmosphere, float3(0.0, rd, 0.0), mu_s_d) : float3(0.0);
        float3 Ei = global.sun_illuminance;

        // L_scat(c,x,v) = σs(x) ∑_{i=1}^{N_light} (T(c,x) S(x,li) p(v,li) + Ψms) Ei   (11)
        float3 Lscat = Ei * (T * S * p + psi_ms * medium.scattering);

        /// --- Integrate scattering

        // Integration formula, see slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
        // integrate along the current step segment
        float3 L_int = (Lscat - Lscat * sample_transmittance) / medium.extinction;
        // accumulate and also take into account the transmittance from previous steps
        L += throughput * L_int;

        throughput *= sample_transmittance;
    }

    return L;
}

void main()
{
    const float2 LUT_SIZE = float2(192.0,108.0); // TODO: uniform?
    float2 pixel_pos = gl_FragCoord.xy;
    float2 uv = pixel_pos / LUT_SIZE;
    float3 clip_space;
    clip_space.xy = uv * 2.0 - float2(1.0);
    clip_space.z = 1;
    float4 h_pos = inverse(global.camera_proj * global.camera_view) * float4(clip_space, 1.0);
    h_pos.xyz /= h_pos.w;

    float3 world_dir = normalize(h_pos.xyz - global.camera_pos * 1000.0);
    float3 world_pos = global.camera_pos * 1000.0 + float3(0.0, atmosphere.bottom_radius, 0.0);

    float r = length(world_pos);
    float mu;
    float cos_lightview;
    uv_to_mu_coslightview(atmosphere, uv, r, mu, cos_lightview);


    float3 up = world_pos / r;
    float mu_s = dot(up, global.sun_direction);
    float3 sun_dir = normalize(float3(sqrt(1.0 - mu_s * mu_s), mu_s, 0.0));

    world_pos = float3(0.0, r, 0.0);

    float sin_theta = sqrt(1.0 - mu * mu);

    world_dir = float3(
        sin_theta * cos_lightview,
        mu,
        sin_theta * sqrt(1.0 - cos_lightview * cos_lightview)
        );

    if (!move_to_top_atmosphere(atmosphere, world_pos, world_dir))
    {
        // Ray is not intersecting the atmosphere
        outColor = float4(1, 0, 0, 1);
        return;
    }

    float3 L = integrate_luminance(atmosphere, world_pos, world_dir, sun_dir);
    outColor = float4(L, 1.0);
}

#else
#include "sky.h"

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
        float sunZenithCosAngle = dot(UpVector, global.sun_direction); // mu_s
        SunDir = normalize(float3(sqrt(1.0 - sunZenithCosAngle * sunZenithCosAngle), sunZenithCosAngle, 0.0));
    }

    WorldPos = float3(0.0f, viewHeight, 0.0);

    float viewZenithSinAngle = sqrt(1 - viewZenithCosAngle * viewZenithCosAngle);

    WorldDir = float3(
        viewZenithSinAngle * lightViewCosAngle,
        viewZenithCosAngle,
        viewZenithSinAngle * sqrt(1.0 - lightViewCosAngle * lightViewCosAngle)
        );

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

#if 0
SingleScatteringResult IntegrateScatteredLuminance(
    float2 pixPos, float3 WorldPos, float3 WorldDir, float3 SunDir, AtmosphereParameters Atmosphere,
    bool ground, float SampleCountIni, float DepthBufferValue, bool VariableSampleCount,
    bool MieRayPhase, float tMaxMax)
{
    SingleScatteringResult result;

    // Compute next intersection with atmosphere or ground
    float3 earthO = float3(0.0f, 0.0f, 0.0f);
    float tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.BottomRadius);
    float tTop = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.TopRadius);
    float tMax = 0.0f;
    if (tBottom < 0.0f)
    {
        if (tTop < 0.0f)
        {
            tMax = 0.0f; // No intersection with earth nor atmosphere: stop right away
            return result;
        }
        else
        {
            tMax = tTop;
        }
    }
    else
    {
        if (tTop > 0.0f)
        {
            tMax = min(tTop, tBottom);
        }
    }

    tMax = min(tMax, tMaxMax);

    // Sample count
    float SampleCount = SampleCountIni;
    float SampleCountFloor = SampleCountIni;
    float tMaxFloor = tMax;
    if (VariableSampleCount)
    {
        SampleCount = mix(global.raymarch_min_max_spp.x, global.raymarch_min_max_spp.y, clamp(tMax*0.01, 0.0, 1.0));
        SampleCountFloor = floor(SampleCount);
        tMaxFloor = tMax * SampleCountFloor / SampleCount;    // rescale tMax to map to the last entire step segment.
    }
    float dt = tMax / SampleCount;

    // Phase functions
    const float uniformPhase = 1.0 / (4.0 * PI);
    const float3 wi = SunDir;
    const float3 wo = WorldDir;
    float cosTheta = dot(wi, wo);
    float MiePhaseValue = hgPhase(Atmosphere.MiePhaseG, -cosTheta);    // mnegate cosTheta because due to WorldDir being a "in" direction.
    float RayleighPhaseValue = RayleighPhase(cosTheta);

    // When building the scattering factor, we assume light illuminance is 1 to compute a transfert function relative to identity illuminance of 1.
    // This make the scattering factor independent of the light. It is now only linked to the atmosphere properties.
    float3 globalL = float3(1.0f);
    // should be one for multiscat cs
    // otherwise cb
    globalL = global.sun_illuminance;

    // Ray march the atmosphere to integrate optical depth
    float3 L = float3(0.0f);
    float3 throughput = float3(1.0);
    float t = 0.0f;
    float tPrev = 0.0;
    const float SampleSegmentT = 0.3f;
    for (float s = 0.0f; s < SampleCount; s += 1.0f)
    {
        if (VariableSampleCount)
        {
            // More expenssive but artefact free
            float t0 = (s) / SampleCountFloor;
            float t1 = (s + 1.0f) / SampleCountFloor;
            // Non linear distribution of sample within the range.
            t0 = t0 * t0;
            t1 = t1 * t1;
            // Make t0 and t1 world space distances.
            t0 = tMaxFloor * t0;
            if (t1 > 1.0)
            {
                t1 = tMax;
                //    t1 = tMaxFloor;    // this reveal depth slices
            }
            else
            {
                t1 = tMaxFloor * t1;
            }
            //t = t0 + (t1 - t0) * (whangHashNoise(pixPos.x, pixPos.y, gFrameId * 1920 * 1080)); // With dithering required to hide some sampling artefact relying on TAA later? This may even allow volumetric shadow?
            t = t0 + (t1 - t0)*SampleSegmentT;
            dt = t1 - t0;
        }
        else
        {
            //t = tMax * (s + SampleSegmentT) / SampleCount;
            // Exact difference, important for accuracy of multiple scattering
            float NewT = tMax * (s + SampleSegmentT) / SampleCount;
            dt = NewT - t;
            t = NewT;
        }
        float3 P = WorldPos + t * WorldDir;

        MediumSampleRGB medium = sampleMediumRGB(P, Atmosphere);
        const float3 SampleOpticalDepth = medium.extinction * dt;
        const float3 SampleTransmittance = exp(-SampleOpticalDepth);

        float pHeight = length(P);
        const float3 UpVector = P / pHeight;
        float SunZenithCosAngle = dot(SunDir, UpVector);
        float2 uv;
        LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);
        float3 TransmittanceToSun = textureLod(TransmittanceLutTexture, uv, 0).rgb;

        float3 PhaseTimesScattering = medium.scatteringMie * MiePhaseValue + medium.scatteringRay * RayleighPhaseValue;

        // Earth shadow
        float tEarth = raySphereIntersectNearest(P, SunDir, earthO + PLANET_RADIUS_OFFSET * UpVector, Atmosphere.BottomRadius);
        float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        // Dual scattering for multi scattering

        float3 multiScatteredLuminance = float3(0.0f);
#if MULTISCATAPPROX_ENABLED
        multiScatteredLuminance = GetMultipleScattering(Atmosphere, medium.scattering, medium.extinction, P, SunZenithCosAngle);
#endif

        float shadow = 1.0f;
#if SHADOWMAP_ENABLED
        // First evaluate opaque shadow
        shadow = getShadow(Atmosphere, P);
#endif


        // Lscat(c,x,v) = σs(x) Nlight_∑_i=1  T(c,x) S(x,li) p(v,li)Ei (3)

        // Lscat?  E       *  S    (vis             *  T)                *  p
        float3 S = globalL * (earthShadow * shadow * TransmittanceToSun) * (PhaseTimesScattering + multiScatteredLuminance * medium.scattering);


        // See slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
        float3 Sint = (S - S * SampleTransmittance) / medium.extinction;    // integrate along the current step segment
        L += throughput * Sint;                                                        // accumulate and also take into account the transmittance from previous steps
        throughput *= SampleTransmittance;

        tPrev = t;
    }

    result.L = L;
    return result;
}
#endif


#endif
