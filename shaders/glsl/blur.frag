#version 430 core
uniform sampler2D source;
uniform vec2 texel;
uniform vec2 direction;
uniform float radius;
in vec2 uv;
out vec4 fragColor;
void main() { float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216); vec3 result = texture(source, uv).rgb * weights[0]; for (int i = 1; i < 5; ++i) { vec2 offset = direction * texel * radius * float(i); result += texture(source, uv + offset).rgb * weights[i]; result += texture(source, uv - offset).rgb * weights[i]; } fragColor = vec4(result, 1.0); }