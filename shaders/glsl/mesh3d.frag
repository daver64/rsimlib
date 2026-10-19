#version 430 core

in vec3 vNormal;
in vec4 vTangent;
in vec4 vColour;
in vec2 vUv;
in vec4 vTerrain;
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

// The layer pyramid texture: level 0 is a 2x2 atlas of s0-pixel tiles at the origin, and each coarser level is a 2x2
// atlas stacked in a column to its right. Every tile has a one texel wrapped border. Matches Terrain::set_layer_atlas.
vec2 level_origin(int level, int s0)
{
    if (level == 0)
        return vec2(0.0);
    float y = 0.0;
    for (int k = 1; k < level; ++k)
        y += 2.0 * float((s0 >> k) + 2);
    return vec2(2.0 * float(s0 + 2), y);
}

vec2 layer_uv(int layer, int level, vec2 tile, int s0, vec2 texel)
{
    int size = s0 >> level;
    vec2 origin = level_origin(level, s0) + vec2(float(layer & 1), float(layer >> 1)) * float(size + 2);
    return (origin + 1.0 + tile * float(size)) * texel;
}

// Mip level for a texture coordinate that repeats once per unit.
float layer_lod(vec2 scaled_uv, int s0, int levels)
{
    vec2 dx = dFdx(scaled_uv) * float(s0);
    vec2 dy = dFdy(scaled_uv) * float(s0);
    return clamp(0.5 * log2(max(max(dot(dx, dx), dot(dy, dy)), 1e-8)), 0.0, float(levels - 1));
}

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 albedo;
    if (vTerrain.x >= 0.0)
    {
        albedo = vColour.rgb;
        int code = int(vTerrain.y + 0.5);
        int levels = code & 15;
        if (levels > 0)
        {
            bool has_normals = ((code >> 4) & 1) != 0;
            int s0 = code >> 5;
            vec2 texel = vTerrain.zw;
            vec2 broad_uv = vUv * 0.137;
            float detail_lod = layer_lod(vUv, s0, levels);
            float broad_lod = layer_lod(broad_uv, s0, levels);
            float fade = vTerrain.x;
            // Screen-space tangent frame; the facing of the screen derivatives keeps it right whichever way the backend's
            // screen y runs.
            vec3 dp1 = dFdx(vTangent.xyz);
            vec3 dp2 = dFdy(vTangent.xyz);
            vec2 duv1 = dFdx(vUv);
            vec2 duv2 = dFdy(vUv);
            float handed = dot(cross(dp1, dp2), normal) < 0.0 ? -1.0 : 1.0;
            vec3 dp2perp = cross(dp2, normal);
            vec3 dp1perp = cross(normal, dp1);
            vec3 tangent = (dp2perp * duv1.x + dp1perp * duv2.x) * handed;
            vec3 bitangent = (dp2perp * duv1.y + dp1perp * duv2.y) * handed;
            float frame_scale = inversesqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-20));
            tangent *= frame_scale;
            bitangent *= frame_scale;

            float weights[4] = float[4](vColour.r, vColour.g, vColour.b, vColour.a);
            albedo = vec3(0.0);
            vec2 slope = vec2(0.0);
            for (int layer = 0; layer < 4; ++layer)
            {
                float weight = weights[layer];
                if (weight < 0.004)
                    continue;
                for (int scale = 0; scale < 2; ++scale)
                {
                    float share = scale == 0 ? 1.0 - fade : fade;
                    if (share < 0.001)
                        continue;
                    vec2 scaled = scale == 0 ? vUv : broad_uv;
                    float lod = scale == 0 ? detail_lod : broad_lod;
                    int level0 = int(floor(lod));
                    int level1 = min(level0 + 1, levels - 1);
                    float blend = lod - float(level0);
                    vec2 tile = fract(scaled);
                    vec2 uv0 = layer_uv(layer, level0, tile, s0, texel);
                    vec2 uv1 = layer_uv(layer, level1, tile, s0, texel);
                    albedo += weight * share * mix(textureLod(uTexture, uv0, 0.0).rgb, textureLod(uTexture, uv1, 0.0).rgb, blend);
                    if (has_normals)
                    {
                        vec2 n0 = textureLod(uNormalMap, uv0, 0.0).xy;
                        vec2 n1 = textureLod(uNormalMap, uv1, 0.0).xy;
                        slope += weight * share * (mix(n0, n1, blend) * 2.0 - 1.0);
                    }
                }
            }
            if (has_normals)
            {
                // Only xy is stored, so averaged (shorter) mip normals flatten naturally with distance.
                vec3 mapped = vec3(slope, sqrt(max(1.0 - dot(slope, slope), 0.0)));
                normal = normalize(tangent * mapped.x - bitangent * mapped.y + normal * mapped.z);
            }
        }
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
