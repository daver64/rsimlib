Texture2D texture_value : register(t0);
SamplerState texture_sampler : register(s0);
Texture2D normal_value : register(t1);
SamplerState normal_sampler : register(s1);
Texture2D material_value : register(t2);
SamplerState material_sampler : register(s2);

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

// PBR (x = 1 when enabled): metallic, roughness, occlusion strength in yzw. Emissive rgb. Camera right/up (scaled by the
// field of view) and forward in world space.
cbuffer PbrParameters : register(b1)
{
    float4 pbr;
    float4 emissive;
    float4 camera_right;
    float4 camera_up;
    float4 camera_forward;
};

static const float PI = 3.14159265359;

// Cook-Torrance (GGX, Smith-Schlick, Schlick Fresnel) for the one directional light plus an ambient term.
float3 shade_pbr(float3 N, float3 V, float3 L, float3 albedo, float metallic, float roughness, float occlusion,
                 float ambient, float3 light_rgb)
{
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 H = normalize(L + V);
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
    float3 F = F0 + (1.0 - F0) * pow(1.0 - VdH, 5.0);
    float3 specular = D * G * F / (4.0 * NdV * max(NdL, 1e-4));
    float3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo;
    float3 direct = (diffuse + specular * PI) * NdL * light_rgb * (1.0 - ambient);
    float3 Fa = F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(1.0 - NdV, 5.0);
    float3 indirect = ambient * occlusion * ((1.0 - Fa) * (1.0 - metallic) * albedo + Fa * (1.0 - 0.5 * roughness));
    return direct + indirect;
}

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
    float texture_alpha = 1.0;
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
        texture_alpha = sampled.a;
        // light_colour.w is the alpha cutoff of cut-out materials such as foliage and impostors (0 disables it).
        if (light_colour.w > 0.0 && input.color.a * sampled.a < light_colour.w)
            discard;
    }
    float3 colour;
    if (pbr.x > 0.5 && input.terrain.x < 0.0)
    {
        float3 orm = material_value.Sample(material_sampler, input.texcoord).rgb;
        // Framebuffer y grows downward in D3D.
        float2 pixel = input.position.xy * fog_view.zw * 2.0;
        float2 ndc = float2(pixel.x - 1.0, 1.0 - pixel.y);
        float3 view = normalize(-(camera_forward.xyz + ndc.x * camera_right.xyz + ndc.y * camera_up.xyz));
        colour = shade_pbr(normal, view, light.xyz, albedo, saturate(pbr.y * orm.b),
                           clamp(pbr.z * orm.g, 0.04, 1.0), lerp(1.0, orm.r, pbr.w), light.w, light_colour.rgb) +
                 emissive.rgb;
    }
    else
    {
        float diffuse = max(dot(normal, light.xyz), 0.0);
        colour = albedo * (light.w + (1.0 - light.w) * diffuse * light_colour.rgb);
    }
    if (fog_beta.r + fog_beta.g + fog_beta.b > 0.0)
    {
        float2 ndc = input.position.xy * fog_view.zw * 2.0 - 1.0;
        float distance_to_eye = input.view_depth * length(float3(ndc * fog_view.xy, 1.0));
        colour = lerp(fog_colour.rgb, colour, exp(-fog_beta.rgb * distance_to_eye));
    }
    // emissive.w > 0 marks a blended draw: its opacity multiplies the vertex and texture alpha.
    float alpha = 1.0;
    if (emissive.w > 0.0 && input.terrain.x < 0.0)
        alpha = saturate(input.color.a * texture_alpha * emissive.w);
    return float4(colour, alpha);
}
