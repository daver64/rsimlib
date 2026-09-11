#version 450
layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 fragColor;
void main() { vec4 texel = texture(uTexture, vTexCoord); vec3 colour = texel.a > 0.001 ? texel.rgb / texel.a : vec3(0.0); fragColor = vec4(colour, texel.a) * vColor; }