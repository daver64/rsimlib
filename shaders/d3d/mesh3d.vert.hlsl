cbuffer MeshParameters : register(b0)
{
    float4x4 mvp;
    float4 normal_rows[3];
    float4 light;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
    float4 tangent : TANGENT;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR0;
    float2 texcoord : TEXCOORD0;
    float4 tangent : TEXCOORD1;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.position.z = output.position.z * 0.5 + output.position.w * 0.5;
    output.normal = float3(dot(normal_rows[0].xyz, input.normal),
                           dot(normal_rows[1].xyz, input.normal),
                           dot(normal_rows[2].xyz, input.normal));
    output.color = input.color.rgb * float3(normal_rows[0].w, normal_rows[1].w, normal_rows[2].w);
    output.tangent = float4(dot(normal_rows[0].xyz, input.tangent.xyz),
                            dot(normal_rows[1].xyz, input.tangent.xyz),
                            dot(normal_rows[2].xyz, input.tangent.xyz), input.tangent.w);
    output.texcoord = input.texcoord;
    return output;
}
