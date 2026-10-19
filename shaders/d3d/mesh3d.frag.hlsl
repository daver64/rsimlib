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

// One layer of the 2x2 atlas, repeated by hand so there is no filtering across tile borders.
float3 layer_colour(int layer, float2 uv, float2 inset, float scale)
{
    float2 cell = float2(layer & 1, layer >> 1) * 0.5;
    float2 local = frac(uv * scale) * (0.5 - 2.0 * inset) + inset;
    return texture_value.SampleGrad(texture_sampler, cell + local, ddx(uv) * scale * 0.5, ddy(uv) * scale * 0.5).rgb;
}

float4 main(PixelInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 albedo;
    if (input.terrain.x >= 0.0)
    {
        albedo = input.color.rgb;
        if (input.terrain.y > 0.5)
        {
            float2 inset = input.terrain.zw;
            float weights[4] = { input.color.r, input.color.g, input.color.b, input.color.a };
            albedo = float3(0.0, 0.0, 0.0);
            for (int layer = 0; layer < 4; ++layer)
            {
                float3 detail = layer_colour(layer, input.texcoord, inset, 1.0);
                float3 broad = layer_colour(layer, input.texcoord, inset, 0.137);
                albedo += weights[layer] * lerp(detail, broad, input.terrain.x);
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
