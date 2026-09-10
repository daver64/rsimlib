#version 450
layout(set = 0, binding = 0) uniform sampler2D source;
layout(push_constant) uniform Blur { layout(offset = 64) vec2 texel; vec2 direction; float radius; } blur;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;
void main() {
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 result = texture(source, uv).rgb * weights[0];
    for (int i = 1; i < 5; ++i) {
        vec2 offset = blur.direction * blur.texel * blur.radius * float(i);
        result += texture(source, uv + offset).rgb * weights[i];
        result += texture(source, uv - offset).rgb * weights[i];
    }
    fragColor = vec4(result, 1.0);
}
