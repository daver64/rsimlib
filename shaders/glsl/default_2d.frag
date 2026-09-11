#version 430 core
in vec2 vTexCoord;
in vec4 vColor;
uniform sampler2D uTexture;
out vec4 fragColor;
void main() { vec4 colour = texture(uTexture, vTexCoord) * vColor; fragColor = vec4(colour.rgb * colour.a, colour.a); }