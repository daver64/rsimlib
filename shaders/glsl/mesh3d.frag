#version 430 core

in vec3 vNormal;
in vec4 vTangent;
in vec4 vColour;
in vec2 vUv;
in vec4 vTerrain;

uniform sampler2D uTexture;
uniform sampler2D uNormalMap;

// xyz: unit vector toward the light, w: ambient. Unlit draws use (0, 0, 0, 1).
uniform vec4 uLight;

out vec4 fragColor;

// One layer of the 2x2 atlas, repeated by hand so there is no filtering across tile borders.
vec3 layer_colour(int layer, vec2 uv, vec2 inset, float scale)
{
    vec2 cell = vec2(float(layer & 1), float(layer >> 1)) * 0.5;
    vec2 local = fract(uv * scale) * (0.5 - 2.0 * inset) + inset;
    return textureGrad(uTexture, cell + local, dFdx(uv) * scale * 0.5, dFdy(uv) * scale * 0.5).rgb;
}

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 albedo;
    if (vTerrain.x >= 0.0)
    {
        albedo = vColour.rgb;
        if (vTerrain.y > 0.5)
        {
            vec2 inset = vTerrain.zw;
            float weights[4] = float[4](vColour.r, vColour.g, vColour.b, vColour.a);
            albedo = vec3(0.0);
            for (int layer = 0; layer < 4; ++layer)
            {
                vec3 detail = layer_colour(layer, vUv, inset, 1.0);
                vec3 broad = layer_colour(layer, vUv, inset, 0.137);
                albedo += weights[layer] * mix(detail, broad, vTerrain.x);
            }
        }
    }
    else
    {
        // Tangent-space normal mapping; a zero tangent (no UVs) or a flat map leaves the vertex normal unchanged.
        vec3 tangent = vTangent.xyz - normal * dot(normal, vTangent.xyz);
        if (dot(tangent, tangent) > 1e-8)
        {
            tangent = normalize(tangent);
            vec3 bitangent = cross(normal, tangent) * vTangent.w;
            vec3 mapped = texture(uNormalMap, vUv).xyz * 2.0 - 1.0;
            normal = normalize(tangent * mapped.x + bitangent * mapped.y + normal * mapped.z);
        }
        albedo = vColour.rgb * texture(uTexture, vUv).rgb;
    }
    float diffuse = max(dot(normal, uLight.xyz), 0.0);
    float brightness = uLight.w + (1.0 - uLight.w) * diffuse;
    fragColor = vec4(albedo * brightness, 1.0);
}
