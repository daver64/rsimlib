Texture2D texture_value : register(t0);
SamplerState texture_sampler : register(s0);
Texture2D normal_value : register(t1);
SamplerState normal_sampler : register(s1);

cbuffer MeshParameters : register(b0)
{
    float4x4 mvp;
    float4 normal_rows[3];
    float4 light;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
    float4 tangent : TEXCOORD1;
    float4 terrain : TEXCOORD2;
};

// The layer pyramid texture: level 0 is a 2x2 atlas of s0-pixel tiles at the origin, and each coarser level is a 2x2
// atlas stacked in a column to its right. Every tile has a one texel wrapped border. Matches Terrain::set_layer_atlas.
float2 level_origin(int level, int s0)
{
    if (level == 0)
        return float2(0.0, 0.0);
    float y = 0.0;
    for (int k = 1; k < level; ++k)
        y += 2.0 * float((s0 >> k) + 2);
    return float2(2.0 * float(s0 + 2), y);
}

float2 layer_uv(int layer, int level, float2 tile, int s0, float2 texel)
{
    int size = s0 >> level;
    float2 origin = level_origin(level, s0) + float2(layer & 1, layer >> 1) * float(size + 2);
    return (origin + 1.0 + tile * float(size)) * texel;
}

// Mip level for a texture coordinate that repeats once per unit.
float layer_lod(float2 scaled_uv, int s0, int levels)
{
    float2 dx = ddx(scaled_uv) * float(s0);
    float2 dy = ddy(scaled_uv) * float(s0);
    return clamp(0.5 * log2(max(max(dot(dx, dx), dot(dy, dy)), 1e-8)), 0.0, float(levels - 1));
}

float4 main(PixelInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 albedo;
    if (input.terrain.x >= 0.0)
    {
        albedo = input.color.rgb;
        int code = int(input.terrain.y + 0.5);
        int levels = code & 15;
        if (levels > 0)
        {
            bool has_normals = ((code >> 4) & 1) != 0;
            int s0 = code >> 5;
            float2 texel = input.terrain.zw;
            float2 broad_uv = input.texcoord * 0.137;
            float detail_lod = layer_lod(input.texcoord, s0, levels);
            float broad_lod = layer_lod(broad_uv, s0, levels);
            float fade = input.terrain.x;
            // Screen-space tangent frame; the facing of the screen derivatives keeps it right whichever way screen y runs.
            float3 dp1 = ddx(input.tangent.xyz);
            float3 dp2 = ddy(input.tangent.xyz);
            float2 duv1 = ddx(input.texcoord);
            float2 duv2 = ddy(input.texcoord);
            float handed = dot(cross(dp1, dp2), normal) < 0.0 ? -1.0 : 1.0;
            float3 dp2perp = cross(dp2, normal);
            float3 dp1perp = cross(normal, dp1);
            float3 tangent = (dp2perp * duv1.x + dp1perp * duv2.x) * handed;
            float3 bitangent = (dp2perp * duv1.y + dp1perp * duv2.y) * handed;
            float frame_scale = rsqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-20));
            tangent *= frame_scale;
            bitangent *= frame_scale;

            float weights[4] = { input.color.r, input.color.g, input.color.b, input.color.a };
            albedo = float3(0.0, 0.0, 0.0);
            float2 slope = float2(0.0, 0.0);
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
                    float2 scaled = scale == 0 ? input.texcoord : broad_uv;
                    float lod = scale == 0 ? detail_lod : broad_lod;
                    int level0 = int(floor(lod));
                    int level1 = min(level0 + 1, levels - 1);
                    float blend = lod - float(level0);
                    float2 tile = frac(scaled);
                    float2 uv0 = layer_uv(layer, level0, tile, s0, texel);
                    float2 uv1 = layer_uv(layer, level1, tile, s0, texel);
                    albedo += weight * share * lerp(texture_value.SampleLevel(texture_sampler, uv0, 0.0).rgb,
                                                    texture_value.SampleLevel(texture_sampler, uv1, 0.0).rgb, blend);
                    if (has_normals)
                    {
                        float2 n0 = normal_value.SampleLevel(normal_sampler, uv0, 0.0).xy;
                        float2 n1 = normal_value.SampleLevel(normal_sampler, uv1, 0.0).xy;
                        slope += weight * share * (lerp(n0, n1, blend) * 2.0 - 1.0);
                    }
                }
            }
            if (has_normals)
            {
                // Only xy is stored, so averaged (shorter) mip normals flatten naturally with distance.
                float3 mapped = float3(slope, sqrt(max(1.0 - dot(slope, slope), 0.0)));
                normal = normalize(tangent * mapped.x - bitangent * mapped.y + normal * mapped.z);
            }
        }
    }
    else
    {
        float3 mapped = normal_value.Sample(normal_sampler, input.texcoord).xyz * 2.0 - 1.0;
        float3 tangent = input.tangent.xyz - normal * dot(normal, input.tangent.xyz);
        if (dot(tangent, tangent) > 1e-8)
        {
            tangent = normalize(tangent);
            float3 bitangent = cross(normal, tangent) * input.tangent.w;
            normal = normalize(tangent * mapped.x + bitangent * mapped.y + normal * mapped.z);
        }
        albedo = input.color.rgb * texture_value.Sample(texture_sampler, input.texcoord).rgb;
    }
    float diffuse = max(dot(normal, light.xyz), 0.0);
    float brightness = light.w + (1.0 - light.w) * diffuse;
    return float4(albedo * brightness, 1.0);
}
