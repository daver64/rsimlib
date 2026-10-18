#version 430 core

in vec3 vNormal;
in vec4 vTangent;
in vec3 vColour;
in vec2 vUv;

uniform sampler2D uTexture;
uniform sampler2D uNormalMap;

// xyz: unit vector toward the light, w: ambient. Unlit draws use (0, 0, 0, 1).
uniform vec4 uLight;

out vec4 fragColor;

void main()
{
    vec3 normal = normalize(vNormal);
    // Tangent-space normal mapping; a zero tangent (no UVs) or a flat map leaves the vertex normal unchanged.
    vec3 tangent = vTangent.xyz - normal * dot(normal, vTangent.xyz);
    if (dot(tangent, tangent) > 1e-8)
    {
        tangent = normalize(tangent);
        vec3 bitangent = cross(normal, tangent) * vTangent.w;
        vec3 mapped = texture(uNormalMap, vUv).xyz * 2.0 - 1.0;
        normal = normalize(tangent * mapped.x + bitangent * mapped.y + normal * mapped.z);
    }
    float diffuse = max(dot(normal, uLight.xyz), 0.0);
    float brightness = uLight.w + (1.0 - uLight.w) * diffuse;
    fragColor = vec4(vColour * texture(uTexture, vUv).rgb * brightness, 1.0);
}
