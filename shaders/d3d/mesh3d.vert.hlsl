cbuffer MeshParameters : register(b0)
{
    float4x4 mvp;
    // Normal-matrix rows; w holds the tint colour. A negative normal_rows[0].w selects terrain shading, where the
    // rows carry: [0] xyz: use atlas, inset u, inset v. [1] xyz: eye (mesh space), w: morph start.
    // [2] x: morph end, y: fade start, z: fade end.
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
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
    float4 tangent : TEXCOORD1;
    // x: layer fade (-1 for ordinary meshes), y: atlas in use, zw: atlas inset.
    float4 terrain : TEXCOORD2;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    if (normal_rows[0].w < 0.0)
    {
        float3 eye = normal_rows[1].xyz;
        float morph = smoothstep(normal_rows[1].w, normal_rows[2].x, distance(input.position, eye));
        float3 position = input.position + input.tangent.xyz * morph;
        output.position = mul(mvp, float4(position, 1.0));
        output.position.z = output.position.z * 0.5 + output.position.w * 0.5;
        output.normal = input.normal;
        output.color = input.color;
        output.tangent = float4(0.0, 0.0, 0.0, 0.0);
        output.texcoord = input.texcoord;
        float fade = normal_rows[2].z > normal_rows[2].y
            ? smoothstep(normal_rows[2].y, normal_rows[2].z, distance(position, eye)) : 0.0;
        output.terrain = float4(fade, normal_rows[0].x, normal_rows[0].y, normal_rows[0].z);
        return output;
    }
    output.position = mul(mvp, float4(input.position, 1.0));
    output.position.z = output.position.z * 0.5 + output.position.w * 0.5;
    output.normal = float3(dot(normal_rows[0].xyz, input.normal),
                           dot(normal_rows[1].xyz, input.normal),
                           dot(normal_rows[2].xyz, input.normal));
    output.color = float4(input.color.rgb * float3(normal_rows[0].w, normal_rows[1].w, normal_rows[2].w), input.color.a);
    output.tangent = float4(dot(normal_rows[0].xyz, input.tangent.xyz),
                            dot(normal_rows[1].xyz, input.tangent.xyz),
                            dot(normal_rows[2].xyz, input.tangent.xyz), input.tangent.w);
    output.texcoord = input.texcoord;
    output.terrain = float4(-1.0, 0.0, 0.0, 0.0);
    return output;
}
