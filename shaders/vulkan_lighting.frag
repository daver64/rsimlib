#version 450
layout(set = 0, binding = 0) uniform sampler2D source;
layout(set = 0, binding = 1) uniform sampler2D shadowMasks[8];
struct GpuLight { vec4 positionRadius; vec4 colourIntensity; vec4 shadowSoftness; };
layout(set = 1, binding = 0, std430) readonly buffer LightBuffer { GpuLight lights[]; } lightBuffer;
layout(set = 1, binding = 1, std430) readonly buffer TileCounts { uint tileCounts[]; } tileCountsBuffer;
layout(set = 1, binding = 2, std430) readonly buffer TileIndices { uint tileIndices[]; } tileIndicesBuffer;
layout(push_constant) uniform Lighting { ivec2 tileCount; int lightCount; int shadowLightCount; float ambient; int flipVertical; } lighting;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;
void main() {
    vec2 lightUv = uv;
    if (lighting.flipVertical != 0) lightUv.y = 1.0 - lightUv.y;
    vec4 base = texture(source, uv);
    vec2 pixelPosition = lightUv * vec2(textureSize(source, 0));
    ivec2 tile = ivec2(pixelPosition / 16.0);
    int tileIndex = tile.y * lighting.tileCount.x + tile.x;
    uint count = tileCountsBuffer.tileCounts[tileIndex];
    vec3 illumination = vec3(lighting.ambient);
    for (uint tileLight = 0u; tileLight < count; ++tileLight) {
        int index = int(tileIndicesBuffer.tileIndices[tileIndex * 128 + tileLight]);
        GpuLight light = lightBuffer.lights[index];
        float distanceToLight = distance(pixelPosition, light.positionRadius.xy);
        float falloff = 1.0 - smoothstep(0.0, max(light.positionRadius.z, 0.0001), distanceToLight);
        illumination += light.colourIntensity.rgb * falloff * light.colourIntensity.a;
    }
    fragColor = vec4(base.rgb * illumination, base.a);
}
