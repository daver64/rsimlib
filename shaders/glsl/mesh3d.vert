#version 430 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColour;
layout(location = 3) in vec2 aUv;
layout(location = 4) in vec4 aTangent;

uniform mat4 uMvp;
// Rows of the normal matrix; w holds the tint colour. A negative uNormalRows[0].w selects terrain shading, where
// the rows carry: [0] xyz: atlas code (mip levels + 16 if a normal atlas + 32 * tile size, 0 for none), 1/width, 1/height. [1] xyz: eye (mesh space), w: morph start.
// [2] x: morph end, y: fade start, z: fade end.
uniform vec4 uNormalRows[3];

out vec3 vNormal;
out vec4 vTangent;
out vec4 vColour;
out vec2 vUv;
// x: layer fade (-1 for ordinary meshes), y: atlas code, zw: 1/texture size.
out vec4 vTerrain;
// View-space depth, for fog.
out float vViewDepth;

void main()
{
    if (uNormalRows[0].w < 0.0)
    {
        // The tangent attribute holds the offset to the coarser level's surface.
        vec3 eye = uNormalRows[1].xyz;
        float morph = smoothstep(uNormalRows[1].w, uNormalRows[2].x, distance(aPosition, eye));
        vec3 position = aPosition + aTangent.xyz * morph;
        gl_Position = uMvp * vec4(position, 1.0);
        vViewDepth = gl_Position.w;
        vNormal = aNormal;
        vTangent = vec4(aPosition, 1.0);
        vColour = aColour;
        vUv = aUv;
        float fade = uNormalRows[2].z > uNormalRows[2].y
            ? smoothstep(uNormalRows[2].y, uNormalRows[2].z, distance(position, eye)) : 0.0;
        vTerrain = vec4(fade, uNormalRows[0].x, uNormalRows[0].y, uNormalRows[0].z);
        return;
    }
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vViewDepth = gl_Position.w;
    vNormal = vec3(dot(uNormalRows[0].xyz, aNormal),
                   dot(uNormalRows[1].xyz, aNormal),
                   dot(uNormalRows[2].xyz, aNormal));
    vTangent = vec4(dot(uNormalRows[0].xyz, aTangent.xyz),
                    dot(uNormalRows[1].xyz, aTangent.xyz),
                    dot(uNormalRows[2].xyz, aTangent.xyz), aTangent.w);
    vUv = aUv;
    vColour = vec4(aColour.rgb * vec3(uNormalRows[0].w, uNormalRows[1].w, uNormalRows[2].w), aColour.a);
    vTerrain = vec4(-1.0, 0.0, 0.0, 0.0);
}
