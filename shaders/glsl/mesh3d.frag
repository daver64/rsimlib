#version 430 core

in vec3 vNormal;
in vec4 vTangent;
in vec4 vColour;
in vec2 vUv;
in float vTerrain;
in float vViewDepth;

uniform sampler2D uTexture;
uniform sampler2D uNormalMap;

// xyz: unit vector toward the light, w: ambient. Unlit draws use (0, 0, 0, 1).
uniform vec4 uLight;
// rgb: colour of the directional light.
uniform vec4 uLightColour;
// Fog: colour fogged surfaces fade to, per-channel extinction per metre, and (tan x, tan y, 1/width, 1/height) of the
// view, which turn view depth into distance along the ray.
uniform vec4 uFogColour;
uniform vec4 uFogBeta;
uniform vec4 uFogView;

out vec4 fragColor;

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
    float diffuse = max(dot(normal, uLight.xyz), 0.0);
    vec3 colour = albedo * (uLight.w + (1.0 - uLight.w) * diffuse * uLightColour.rgb);
    if (uFogBeta.r + uFogBeta.g + uFogBeta.b > 0.0)
    {
        vec2 ndc = gl_FragCoord.xy * uFogView.zw * 2.0 - 1.0;
        float distance_to_eye = vViewDepth * length(vec3(ndc * uFogView.xy, 1.0));
        colour = mix(uFogColour.rgb, colour, exp(-uFogBeta.rgb * distance_to_eye));
    }
    fragColor = vec4(colour, 1.0);
}
