#version 450
layout(set = 0, binding = 0) uniform sampler2D source;
layout(push_constant) uniform Bright { layout(offset = 64) float threshold; } bright;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;
void main() {
    vec4 sourceColour = texture(source, uv);
    fragColor = vec4(max(sourceColour.rgb - vec3(bright.threshold), vec3(0.0)), sourceColour.a);
}
