Texture2D texture_value : register(t0);
SamplerState texture_sampler : register(s0);

struct PixelInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PixelInput input) : SV_TARGET
{
    return texture_value.Sample(texture_sampler, input.texcoord) * input.color;
}
