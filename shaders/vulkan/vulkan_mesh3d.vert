#version 450
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColour;
// Normal-matrix rows; w holds the tint colour.
layout(push_constant) uniform Transform { mat4 mvp; vec4 normalRows[3]; } transform;
layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vColour;
void main()
{
    gl_Position = transform.mvp * vec4(aPosition, 1.0);
    vNormal = vec3(dot(transform.normalRows[0].xyz, aNormal),
                   dot(transform.normalRows[1].xyz, aNormal),
                   dot(transform.normalRows[2].xyz, aNormal));
    vColour = aColour.rgb * vec3(transform.normalRows[0].w, transform.normalRows[1].w, transform.normalRows[2].w);
}
