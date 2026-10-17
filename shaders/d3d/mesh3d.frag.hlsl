Texture2D texture_value : register(t0);
SamplerState texture_sampler : register(s0);

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
    float3 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

float4 main(PixelInput input) : SV_TARGET
{
    float diffuse = max(dot(normalize(input.normal), light.xyz), 0.0);
    float brightness = light.w + (1.0 - light.w) * diffuse;
    return float4(input.color * texture_value.Sample(texture_sampler, input.texcoord).rgb * brightness, 1.0);
}
