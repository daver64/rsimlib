#version 450
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColour;
layout(location = 3) in vec2 aUv;
layout(location = 4) in vec4 aTangent;
// Normal-matrix rows; w holds the tint colour. A negative normalRows[0].w selects terrain shading, where the rows
// carry: [0] xyz: atlas code (mip levels + 16 if a normal atlas + 32 * tile size, 0 for none), 1/width, 1/height. [1] xyz: eye (mesh space), w: morph start.
// [2] x: morph end, y: fade start, z: fade end.
layout(push_constant) uniform Transform { mat4 mvp; vec4 normalRows[3]; } transform;
layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec4 vColour;
layout(location = 2) out vec2 vUv;
layout(location = 3) out vec4 vTangent;
// x: layer fade (-1 for ordinary meshes), y: atlas code, zw: 1/texture size.
layout(location = 4) out vec4 vTerrain;
void main()
{
    if (transform.normalRows[0].w < 0.0)
    {
        vec3 eye = transform.normalRows[1].xyz;
        float morph = smoothstep(transform.normalRows[1].w, transform.normalRows[2].x, distance(aPosition, eye));
        vec3 position = aPosition + aTangent.xyz * morph;
        gl_Position = transform.mvp * vec4(position, 1.0);
        vNormal = aNormal;
        vTangent = vec4(aPosition, 1.0);
        vColour = aColour;
        vUv = aUv;
        float fade = transform.normalRows[2].z > transform.normalRows[2].y
            ? smoothstep(transform.normalRows[2].y, transform.normalRows[2].z, distance(position, eye)) : 0.0;
        vTerrain = vec4(fade, transform.normalRows[0].x, transform.normalRows[0].y, transform.normalRows[0].z);
        return;
    }
    gl_Position = transform.mvp * vec4(aPosition, 1.0);
    vNormal = vec3(dot(transform.normalRows[0].xyz, aNormal),
                   dot(transform.normalRows[1].xyz, aNormal),
                   dot(transform.normalRows[2].xyz, aNormal));
    vTangent = vec4(dot(transform.normalRows[0].xyz, aTangent.xyz),
                    dot(transform.normalRows[1].xyz, aTangent.xyz),
                    dot(transform.normalRows[2].xyz, aTangent.xyz), aTangent.w);
    vUv = aUv;
    vColour = vec4(aColour.rgb * vec3(transform.normalRows[0].w, transform.normalRows[1].w, transform.normalRows[2].w), aColour.a);
    vTerrain = vec4(-1.0, 0.0, 0.0, 0.0);
}
