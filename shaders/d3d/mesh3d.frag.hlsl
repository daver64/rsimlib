Texture2D texture_value : register(t0);
SamplerState texture_sampler : register(s0);
Texture2D normal_value : register(t1);
SamplerState normal_sampler : register(s1);

cbuffer MeshParameters : register(b0)
{
    float4x4 mvp;
    float4 normal_rows[3];
    float4 light;
    // rgb: colour of the directional light.
    float4 light_colour;
    // Fog: colour fogged surfaces fade to, per-channel extinction per metre, and (tan x, tan y, 1/width, 1/height) of
    // the view, which turn view depth into distance along the ray.
    float4 fog_colour;
    float4 fog_beta;
    float4 fog_view;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
    float4 tangent : TEXCOORD1;
    float4 terrain : TEXCOORD2;
    float view_depth : TEXCOORD3;
};

float4 main(PixelInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 albedo;
    if (input.terrain.x >= 0.0)
    {
        albedo = input.color.rgb;
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
        float4 sampled = texture_value.Sample(texture_sampler, input.texcoord);
        albedo = input.color.rgb * sampled.rgb;
        // light_colour.w is the alpha cutoff of cut-out materials such as foliage and impostors (0 disables it).
        if (light_colour.w > 0.0 && input.color.a * sampled.a < light_colour.w)
            discard;
    }
    float diffuse = max(dot(normal, light.xyz), 0.0);
    float3 colour = albedo * (light.w + (1.0 - light.w) * diffuse * light_colour.rgb);
    if (fog_beta.r + fog_beta.g + fog_beta.b > 0.0)
    {
        float2 ndc = input.position.xy * fog_view.zw * 2.0 - 1.0;
        float distance_to_eye = input.view_depth * length(float3(ndc * fog_view.xy, 1.0));
        colour = lerp(fog_colour.rgb, colour, exp(-fog_beta.rgb * distance_to_eye));
    }
    return float4(colour, 1.0);
}
