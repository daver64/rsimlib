#version 430 core
uniform sampler2D source;
uniform float threshold;
in vec2 uv;
out vec4 fragColor;
void main() { vec3 colour = texture(source, uv).rgb; fragColor = vec4(max(colour - vec3(threshold), vec3(0.0)), 1.0); }