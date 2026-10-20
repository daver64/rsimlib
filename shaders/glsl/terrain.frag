#version 430 core

in vec3 vNormal;
in vec3 vPosition;
in vec4 vWeightsA;
in vec4 vWeightsB;
in vec2 vUv;
in float vFade;
in float vViewDepth;

// Layer textures: repeating, mipmapped and anisotropic. The normal array only holds data when uMacro.w is 1.
uniform sampler2DArray uAlbedo;
uniform sampler2DArray uNormals;

// Per surface type: x first array layer, y variant count, z tile metres, w tiles per UV period.
uniform vec4 uLayers[8];
// Per surface type: rgb average colour.
uniform vec4 uAverage[8];
// x: macro texture array layer (negative for none), y: macro tile metres, z: macro strength, w: 1 if normals exist.
uniform vec4 uMacro;

// xyz: unit vector toward the light, w: ambient.
uniform vec4 uLight;
// rgb: colour of the directional light.
uniform vec4 uLightColour;
// Fog: colour fogged surfaces fade to, per-channel extinction per metre, and (tan x, tan y, 1/width, 1/height) of the
// view, which turn view depth into distance along the ray.
uniform vec4 uFogColour;
uniform vec4 uFogBeta;
uniform vec4 uFogView;

out vec4 fragColor;

uint hash3(uvec3 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return v.x ^ v.y ^ v.z;
}

void main()
{
    vec3 normal = normalize(vNormal);

    // Derivatives are taken outside any branch so every sample uses well defined gradients.
    vec2 uv_dx = dFdx(vUv);
    vec2 uv_dy = dFdy(vUv);

    // Screen-space tangent frame; the facing of the screen derivatives keeps it right whichever way the backend's
    // screen y runs.
    vec3 dp1 = dFdx(vPosition);
    vec3 dp2 = dFdy(vPosition);
    float handed = dot(cross(dp1, dp2), normal) < 0.0 ? -1.0 : 1.0;
    vec3 dp2perp = cross(dp2, normal);
    vec3 dp1perp = cross(normal, dp1);
    vec3 tangent = (dp2perp * uv_dx.x + dp1perp * uv_dy.x) * handed;
    vec3 bitangent = (dp2perp * uv_dx.y + dp1perp * uv_dy.y) * handed;
    float frame_scale = inversesqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-20));
    tangent *= frame_scale;
    bitangent *= frame_scale;

    float weights[8] = float[8](vWeightsA.x, vWeightsA.y, vWeightsA.z, vWeightsA.w,
                                vWeightsB.x, vWeightsB.y, vWeightsB.z, vWeightsB.w);
    bool has_normals = uMacro.w > 0.5;
    vec3 albedo = vec3(0.0);
    vec2 slope = vec2(0.0);
    float total = 0.0;
    for (int layer = 0; layer < 8; ++layer)
    {
        float weight = weights[layer];
        if (weight < 0.004)
            continue;
        vec4 info = uLayers[layer];
        vec2 st = vUv / info.z;
        // The cell index repeats every UV period, so neighbouring chunks agree on each cell's variant.
        vec2 cell = mod(floor(st), info.w);
        int variant = int(hash3(uvec3(uvec2(cell), uint(layer))) % uint(info.y));
        vec3 coord = vec3(st, info.x + float(variant));
        vec2 grad_x = uv_dx / info.z;
        vec2 grad_y = uv_dy / info.z;
        vec3 sampled = textureGrad(uAlbedo, coord, grad_x, grad_y).rgb;
        albedo += weight * mix(sampled, uAverage[layer].rgb, vFade);
        total += weight;
        if (has_normals)
        {
            vec2 n = textureGrad(uNormals, coord, grad_x, grad_y).xy * 2.0 - 1.0;
            slope += weight * n * (1.0 - vFade);
        }
    }
    albedo /= max(total, 1e-4);
    slope /= max(total, 1e-4);

    if (uMacro.x >= 0.0)
    {
        vec2 macro_uv = vUv / uMacro.y;
        vec3 macro = textureGrad(uAlbedo, vec3(macro_uv, uMacro.x), uv_dx / uMacro.y, uv_dy / uMacro.y).rgb;
        float brightness = dot(macro, vec3(0.299, 0.587, 0.114));
        albedo *= max(1.0 + uMacro.z * (brightness - 0.5) * 2.0, 0.0);
    }

    if (has_normals)
    {
        // Only xy is stored, so averaged (shorter) mip normals flatten naturally with distance.
        vec3 mapped = vec3(slope, sqrt(max(1.0 - dot(slope, slope), 0.0)));
        normal = normalize(tangent * mapped.x - bitangent * mapped.y + normal * mapped.z);
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
