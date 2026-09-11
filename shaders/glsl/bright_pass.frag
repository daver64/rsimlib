#version 430 core
uniform sampler2D source;
uniform float threshold;
in vec2 uv;
out vec4 fragColor;
void main() { vec4 sourceColour = texture(source, uv); fragColor = vec4(max(sourceColour.rgb - vec3(threshold), vec3(0.0)), sourceColour.a); }