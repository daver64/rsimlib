#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec4 vColour;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec4 vTangent;
layout(location = 4) in float vTerrain;
layout(location = 5) in float vViewDepth;
layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 1, binding = 0) uniform sampler2D uNormalMap;
layout(set = 3, binding = 0) uniform sampler2D uMaterial;
layout(push_constant) uniform Light { layout(offset = 112) vec4 light; } lighting;
layout(set = 2, binding = 0) uniform Environment { vec4 lightColour; vec4 fogColour; vec4 fogBeta; vec4 fogView; vec4 pbr; vec4 emissive; vec4 cameraRight; vec4 cameraUp; vec4 cameraForward; } environment;
layout(location = 0) out vec4 fragColor;

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
    float textureAlpha = 1.0;
    if (vTerrain >= 0.0)
    {
        albedo = vColour.rgb;
    }
    else
    {
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
        textureAlpha = sampled.a;
        if (environment.lightColour.w > 0.0 && vColour.a * sampled.a < environment.lightColour.w)
            discard;
    }
    vec3 colour;
    if (environment.pbr.x > 0.5 && vTerrain < 0.0)
    {
        vec3 orm = texture(uMaterial, vUv).rgb;
        // The viewport is flipped, so framebuffer y grows downward.
        vec2 pixel = gl_FragCoord.xy * environment.fogView.zw * 2.0;
        vec2 ndc = vec2(pixel.x - 1.0, 1.0 - pixel.y);
        vec3 view = normalize(-(environment.cameraForward.xyz + ndc.x * environment.cameraRight.xyz + ndc.y * environment.cameraUp.xyz));
        colour = shade_pbr(normal, view, lighting.light.xyz, albedo, clamp(environment.pbr.y * orm.b, 0.0, 1.0),
                           clamp(environment.pbr.z * orm.g, 0.04, 1.0), mix(1.0, orm.r, environment.pbr.w),
                           lighting.light.w, environment.lightColour.rgb) + environment.emissive.rgb;
    }
    else
    {
        float diffuse = max(dot(normal, lighting.light.xyz), 0.0);
        colour = albedo * (lighting.light.w + (1.0 - lighting.light.w) * diffuse * environment.lightColour.rgb);
    }
    if (environment.fogBeta.r + environment.fogBeta.g + environment.fogBeta.b > 0.0)
    {
        vec2 ndc = gl_FragCoord.xy * environment.fogView.zw * 2.0 - 1.0;
        float distance_to_eye = vViewDepth * length(vec3(ndc * environment.fogView.xy, 1.0));
        colour = mix(environment.fogColour.rgb, colour, exp(-environment.fogBeta.rgb * distance_to_eye));
    }
    // emissive.w > 0 marks a blended draw: its opacity multiplies the vertex and texture alpha.
    float alpha = 1.0;
    if (environment.emissive.w > 0.0 && vTerrain < 0.0)
        alpha = clamp(vColour.a * textureAlpha * environment.emissive.w, 0.0, 1.0);
    fragColor = vec4(colour, alpha);
}
