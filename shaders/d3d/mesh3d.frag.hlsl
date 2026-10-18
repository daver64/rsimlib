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
    float3 color : COLOR0;
    float2 texcoord : TEXCOORD0;
    float4 tangent : TEXCOORD1;
};

float4 main(PixelInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 mapped = normal_value.Sample(normal_sampler, input.texcoord).xyz * 2.0 - 1.0;
    float3 tangent = input.tangent.xyz - normal * dot(normal, input.tangent.xyz);
    if (dot(tangent, tangent) > 1e-8)
    {
        tangent = normalize(tangent);
        float3 bitangent = cross(normal, tangent) * input.tangent.w;
        normal = normalize(tangent * mapped.x + bitangent * mapped.y + normal * mapped.z);
    }
    float diffuse = max(dot(normal, light.xyz), 0.0);
    float brightness = light.w + (1.0 - light.w) * diffuse;
    return float4(input.color * texture_value.Sample(texture_sampler, input.texcoord).rgb * brightness, 1.0);
}
