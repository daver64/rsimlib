#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vColour;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec4 vTangent;
layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 1, binding = 0) uniform sampler2D uNormalMap;
// xyz: unit vector toward the light, w: ambient. Unlit draws use (0, 0, 0, 1).
layout(push_constant) uniform Light { layout(offset = 112) vec4 light; } lighting;
layout(location = 0) out vec4 fragColor;
void main()
{
    vec3 normal = normalize(vNormal);
    vec3 tangent = vTangent.xyz - normal * dot(normal, vTangent.xyz);
    if (dot(tangent, tangent) > 1e-8)
    {
        tangent = normalize(tangent);
        vec3 bitangent = cross(normal, tangent) * vTangent.w;
        vec3 mapped = texture(uNormalMap, vUv).xyz * 2.0 - 1.0;
        normal = normalize(tangent * mapped.x + bitangent * mapped.y + normal * mapped.z);
    }
    float diffuse = max(dot(normal, lighting.light.xyz), 0.0);
    float brightness = lighting.light.w + (1.0 - lighting.light.w) * diffuse;
    fragColor = vec4(vColour * texture(uTexture, vUv).rgb * brightness, 1.0);
}
