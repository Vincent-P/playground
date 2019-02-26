#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec3 inCamPos;

layout (set = 1, binding = 0) uniform sampler2D colorMap;
layout (set = 1, binding = 1) uniform sampler2D physicalDescriptorMap;
layout (set = 1, binding = 2) uniform sampler2D normalMap;
layout (set = 1, binding = 3) uniform sampler2D aoMap;
layout (set = 1, binding = 4) uniform sampler2D emissiveMap;

layout (push_constant) uniform Material
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 diffuseFactor;
    vec4 specularFactor;
    float workflow;
    int baseColorTextureSet;
    int physicalDescriptorTextureSet;
    int normalTextureSet;
    int occlusionTextureSet;
    int emissiveTextureSet;
    float metallicFactor;
    float roughnessFactor;
    float alphaMask;
    float alphaMaskCutoff;
} material;

layout (location = 0) out vec4 outColor;

const float M_PI = 3.141592653589793;
const float c_MinRoughness = 0.04;

const float PBR_WORKFLOW_METALLIC_ROUGHNESS = 0.0;
const float PBR_WORKFLOW_SPECULAR_GLOSINESS = 1.0f;

const vec3 LightPos = vec3(1.0, 5.0, 1.0);
const vec3 LightColor = vec3(500.0);

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal()
{
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    vec3 tangentNormal = texture(normalMap, material.normalTextureSet == 0 ? inUV0 : inUV1).xyz * 2.0 - 1.0;

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

// Gets metallic factor from specular glossiness workflow inputs
float convertMetallic(vec3 diffuse, vec3 specular, float maxSpecular)
{
    float perceivedDiffuse = sqrt(0.299 * diffuse.r * diffuse.r + 0.587 * diffuse.g * diffuse.g + 0.114 * diffuse.b * diffuse.b);
    float perceivedSpecular = sqrt(0.299 * specular.r * specular.r + 0.587 * specular.g * specular.g + 0.114 * specular.b * specular.b);
    if (perceivedSpecular < c_MinRoughness)
        return 0.0;
    float a = c_MinRoughness;
    float b = perceivedDiffuse * (1.0 - maxSpecular) / (1.0 - c_MinRoughness) + perceivedSpecular - 2.0 * c_MinRoughness;
    float c = c_MinRoughness - perceivedSpecular;
    float D = max(b * b - 4.0 * a * c, 0.0);
    return clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
}

// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;

    return nom / max(denom, 0.001); // prevent divide by zero for roughness=0.0 and NdotH=1.0
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
// ----------------------------------------------------------------------------

void main()
{
    float perceptualRoughness;
    float metallic;
    vec3 diffuseColor;
    vec4 baseColor;

    if (material.alphaMask == 1.0f)
    {
        if (material.baseColorTextureSet > -1)
            baseColor = texture(colorMap, material.baseColorTextureSet == 0 ? inUV0 : inUV1) * material.baseColorFactor;
        else
            baseColor = material.baseColorFactor;

        if (baseColor.a < material.alphaMaskCutoff)
            discard;
    }

    if (material.workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS)
    {
        // Metallic and Roughness material properties are packed together
        // In glTF, these factors can be specified by fixed scalar values
        // or from a metallic-roughness map
        perceptualRoughness = material.roughnessFactor;
        metallic = material.metallicFactor;
        if (material.physicalDescriptorTextureSet > -1)
        {
            // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
            // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
            vec4 mrSample = texture(physicalDescriptorMap, material.physicalDescriptorTextureSet == 0 ? inUV0 : inUV1);
            perceptualRoughness = mrSample.g * perceptualRoughness;
            metallic = mrSample.b * metallic;
        }
        else
        {
            perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);
            metallic = clamp(metallic, 0.0, 1.0);
        }
        // Roughness is authored as perceptual roughness; as is convention,
        // convert to material roughness by squaring the perceptual roughness [2].

        // The albedo may be defined from a base texture or a flat color
        if (material.baseColorTextureSet > -1)
            baseColor = texture(colorMap, material.baseColorTextureSet == 0 ? inUV0 : inUV1) * material.baseColorFactor;
        else
            baseColor = material.baseColorFactor;
    }
    else if (material.workflow == PBR_WORKFLOW_SPECULAR_GLOSINESS)
    {
        // Values from specular glossiness workflow are converted to metallic roughness
        if (material.physicalDescriptorTextureSet > -1)
            perceptualRoughness = 1.0 - texture(physicalDescriptorMap, material.physicalDescriptorTextureSet == 0 ? inUV0 : inUV1).a;
        else
            perceptualRoughness = 0.0;

        const float epsilon = 1e-6;

        vec4 diffuse = texture(colorMap, inUV0);
        vec3 specular = texture(physicalDescriptorMap, inUV0).xyz;

        float maxSpecular = max(max(specular.r, specular.g), specular.b);

        // Convert metallic value from specular glossiness inputs
        metallic = convertMetallic(diffuse.rgb, specular, maxSpecular);

        vec3 baseColorDiffusePart = diffuse.rgb * ((1.0 - maxSpecular) / (1 - c_MinRoughness) / max(1 - metallic, epsilon)) * material.diffuseFactor.rgb;
        vec3 baseColorSpecularPart = specular - (vec3(c_MinRoughness) * (1 - metallic) * (1 / max(metallic, epsilon))) * material.specularFactor.rgb;
        baseColor = vec4(mix(baseColorDiffusePart, baseColorSpecularPart, metallic * metallic), diffuse.a);
    }

    vec3 F0 = vec3(0.04);
    diffuseColor = baseColor.rgb * (vec3(1.0) - F0);
    diffuseColor *= 1.0 - metallic;

    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    vec3 specularColor = mix(F0, baseColor.rgb, metallic);

    vec3 N = getNormal();
    vec3 V = normalize(inCamPos - inWorldPos);

    // -----------------------------------------------------------------------------------------------------------------
    // reflectance equation
    vec3 Lo = vec3(0.0);

    // FOR EACH LIGHT
    // calculate per-light radiance
    vec3 L = normalize(LightPos - inWorldPos);
    vec3 H = normalize(V + L);
    float distance = length(LightPos - inWorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = LightColor * attenuation;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, alphaRoughness);
    float G   = GeometrySmith(N, V, L, alphaRoughness);
    vec3 F    = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

    vec3 nominator    = NDF * G * F;
    float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular = nominator / max(denominator, 0.001); // prevent divide by zero for NdotV=0.0 or NdotL=0.0

    // kS is equal to Fresnel
    vec3 kS = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    vec3 kD = vec3(1.0) - kS;
    // multiply kD by the inverse metalness such that only non-metals
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;

    // scale light by NdotL
    float NdotL = max(dot(N, L), 0.0);

    // add to outgoing radiance Lo
    Lo += (kD * baseColor.xyz / M_PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    // END FOR EACH LIGHT

    vec3 color = Lo;

    // Apply optional PBR terms for additional (optional) shading
    if (material.occlusionTextureSet > -1)
    {
        const float u_OcclusionStrength = 1.0f;
        float ao = texture(aoMap, (material.occlusionTextureSet == 0 ? inUV0 : inUV1)).r;
        color = mix(color, color * ao, u_OcclusionStrength);
    }

    if (material.emissiveTextureSet > -1)
    {
        const float u_EmissiveFactor = 1.0f;
        vec3 emissive = texture(emissiveMap, material.emissiveTextureSet == 0 ? inUV0 : inUV1).xyz * u_EmissiveFactor;
        color += emissive;
    }


    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, baseColor.a);
}
