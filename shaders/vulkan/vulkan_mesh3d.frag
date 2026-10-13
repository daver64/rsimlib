#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vColour;
layout(location = 2) in vec2 vUv;
layout(set = 0, binding = 0) uniform sampler2D uTexture;
// xyz: unit vector toward the light, w: ambient. Unlit draws use (0, 0, 0, 1).
layout(push_constant) uniform Light { layout(offset = 112) vec4 light; } lighting;
layout(location = 0) out vec4 fragColor;
void main()
{
    float diffuse = max(dot(normalize(vNormal), lighting.light.xyz), 0.0);
    float brightness = lighting.light.w + (1.0 - lighting.light.w) * diffuse;
    fragColor = vec4(vColour * texture(uTexture, vUv).rgb * brightness, 1.0);
}
