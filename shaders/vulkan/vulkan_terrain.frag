#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vPosition;
layout(location = 2) in vec4 vWeightsA;
layout(location = 3) in vec4 vWeightsB;
layout(location = 4) in vec2 vUv;
layout(location = 5) in float vFade;
layout(location = 6) in float vViewDepth;
layout(set = 0, binding = 0) uniform sampler2DArray uAlbedo;
layout(set = 1, binding = 0) uniform sampler2DArray uNormals;
layout(push_constant) uniform Light { layout(offset = 112) vec4 light; } lighting;
layout(set = 2, binding = 0) uniform Environment
{
    vec4 lightColour;
    vec4 fogColour;
    vec4 fogBeta;
    vec4 fogView;
    vec4 layers[8];
    vec4 average[8];
    vec4 macro;
} environment;
layout(location = 0) out vec4 fragColor;

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
    vec2 uv_dx = dFdx(vUv);
    vec2 uv_dy = dFdy(vUv);
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
    bool has_normals = environment.macro.w > 0.5;
    vec3 albedo = vec3(0.0);
    vec2 slope = vec2(0.0);
    float total = 0.0;
    for (int layer = 0; layer < 8; ++layer)
    {
        float weight = weights[layer];
        if (weight < 0.004)
            continue;
        vec4 info = environment.layers[layer];
        vec2 st = vUv / info.z;
        vec2 cell = mod(floor(st), info.w);
        int variant = int(hash3(uvec3(uvec2(cell), uint(layer))) % uint(info.y));
        vec3 coord = vec3(st, info.x + float(variant));
        vec2 grad_x = uv_dx / info.z;
        vec2 grad_y = uv_dy / info.z;
        vec3 sampled = textureGrad(uAlbedo, coord, grad_x, grad_y).rgb;
        albedo += weight * mix(sampled, environment.average[layer].rgb, vFade);
        total += weight;
        if (has_normals)
        {
            vec2 n = textureGrad(uNormals, coord, grad_x, grad_y).xy * 2.0 - 1.0;
            slope += weight * n * (1.0 - vFade);
        }
    }
    albedo /= max(total, 1e-4);
    slope /= max(total, 1e-4);

    if (environment.macro.x >= 0.0)
    {
        vec2 macro_uv = vUv / environment.macro.y;
        vec3 macro_colour = textureGrad(uAlbedo, vec3(macro_uv, environment.macro.x),
                                        uv_dx / environment.macro.y, uv_dy / environment.macro.y).rgb;
        float brightness = dot(macro_colour, vec3(0.299, 0.587, 0.114));
        albedo *= max(1.0 + environment.macro.z * (brightness - 0.5) * 2.0, 0.0);
    }

    if (has_normals)
    {
        vec3 mapped = vec3(slope, sqrt(max(1.0 - dot(slope, slope), 0.0)));
        normal = normalize(tangent * mapped.x - bitangent * mapped.y + normal * mapped.z);
    }

    float diffuse = max(dot(normal, lighting.light.xyz), 0.0);
    vec3 colour = albedo * (lighting.light.w + (1.0 - lighting.light.w) * diffuse * environment.lightColour.rgb);
    if (environment.fogBeta.r + environment.fogBeta.g + environment.fogBeta.b > 0.0)
    {
        vec2 ndc = gl_FragCoord.xy * environment.fogView.zw * 2.0 - 1.0;
        float distance_to_eye = vViewDepth * length(vec3(ndc * environment.fogView.xy, 1.0));
        colour = mix(environment.fogColour.rgb, colour, exp(-environment.fogBeta.rgb * distance_to_eye));
    }
    fragColor = vec4(colour, 1.0);
}
