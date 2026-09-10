#version 450
layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = texture(uTexture, vTexCoord) * vColor;
}
