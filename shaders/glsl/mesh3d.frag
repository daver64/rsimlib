#version 430 core

in vec3 vNormal;
in vec4 vTangent;
in vec4 vColour;
in vec2 vUv;
in float vTerrain;
in float vViewDepth;

uniform sampler2D uTexture;
uniform sampler2D uNormalMap;
uniform sampler2D uMaterial;

// xyz: unit vector toward the light, w: ambient. Unlit draws use (0, 0, 0, 1).
uniform vec4 uLight;
// rgb: colour of the directional light.
uniform vec4 uLightColour;
// Fog: colour fogged surfaces fade to, per-channel extinction per metre, and (tan x, tan y, 1/width, 1/height) of the
// view, which turn view depth into distance along the ray.
uniform vec4 uFogColour;
uniform vec4 uFogBeta;
uniform vec4 uFogView;

// PBR (x = 1 when enabled): metallic, roughness, occlusion strength in yzw. Emissive rgb. Camera right/up (scaled by the
// field of view) and forward in world space.
uniform vec4 uPbr;
uniform vec4 uEmissive;
uniform vec4 uCameraRight;
uniform vec4 uCameraUp;
uniform vec4 uCameraForward;

out vec4 fragColor;

const float PI = 3.14159265359;

// Cook-Torrance (GGX, Smith-Schlick, Schlick Fresnel) for the one directional light plus an ambient term.
// Light colour and ambient follow the unlit-looking scale of the Lambert path: white light gives albedo at normal incidence.
vec3 shade_pbr(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness, float occlusion,
               float ambient, vec3 lightColour)
{
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 H = normalize(L + V);
    float NdL = max(dot(N, L), 0.0);
    float NdV = max(dot(N, V), 1e-4);
    float NdH = max(dot(N, H), 0.0);
    float VdH = max(dot(V, H), 0.0);
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdH * NdH * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * d * d);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G = (NdV / (NdV * (1.0 - k) + k)) * (NdL / (NdL * (1.0 - k) + k));
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdH, 5.0);
    vec3 specular = D * G * F / (4.0 * NdV * max(NdL, 1e-4));
    vec3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo;
    vec3 direct = (diffuse + specular * PI) * NdL * lightColour * (1.0 - ambient);
    vec3 Fa = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - NdV, 5.0);
    vec3 indirect = ambient * occlusion * ((1.0 - Fa) * (1.0 - metallic) * albedo + Fa * (1.0 - 0.5 * roughness));
    return direct + indirect;
}

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 albedo;
    if (vTerrain >= 0.0)
    {
        albedo = vColour.rgb;
    }
    else
    {
        // Tangent-space normal mapping; a zero tangent (no UVs) or a flat map leaves the vertex normal unchanged.
        vec3 tangent = vTangent.xyz - normal * dot(normal, vTangent.xyz);
        if (dot(tangent, tangent) > 1e-8)
        {
            tangent = normalize(tangent);
            vec3 bitangent = cross(normal, tangent) * vTangent.w;
            vec3 mapped = texture(uNormalMap, vUv).xyz * 2.0 - 1.0;
            normal = normalize(tangent * mapped.x + bitangent * mapped.y + normal * mapped.z);
        }
        vec4 sampled = texture(uTexture, vUv);
        albedo = vColour.rgb * sampled.rgb;
        // light colour w is the alpha cutoff of cut-out materials such as foliage and impostors (0 disables it).
        if (uLightColour.w > 0.0 && vColour.a * sampled.a < uLightColour.w)
            discard;
    }
    vec3 colour;
    if (uPbr.x > 0.5 && vTerrain < 0.0)
    {
        vec3 orm = texture(uMaterial, vUv).rgb;
        vec2 ndc = gl_FragCoord.xy * uFogView.zw * 2.0 - 1.0;
        vec3 view = normalize(-(uCameraForward.xyz + ndc.x * uCameraRight.xyz + ndc.y * uCameraUp.xyz));
        colour = shade_pbr(normal, view, uLight.xyz, albedo, clamp(uPbr.y * orm.b, 0.0, 1.0),
                           clamp(uPbr.z * orm.g, 0.04, 1.0), mix(1.0, orm.r, uPbr.w), uLight.w, uLightColour.rgb) +
                 uEmissive.rgb;
    }
    else
    {
        float diffuse = max(dot(normal, uLight.xyz), 0.0);
        colour = albedo * (uLight.w + (1.0 - uLight.w) * diffuse * uLightColour.rgb);
    }
    if (uFogBeta.r + uFogBeta.g + uFogBeta.b > 0.0)
    {
        vec2 ndc = gl_FragCoord.xy * uFogView.zw * 2.0 - 1.0;
        float distance_to_eye = vViewDepth * length(vec3(ndc * uFogView.xy, 1.0));
        colour = mix(uFogColour.rgb, colour, exp(-uFogBeta.rgb * distance_to_eye));
    }
    fragColor = vec4(colour, 1.0);
}
