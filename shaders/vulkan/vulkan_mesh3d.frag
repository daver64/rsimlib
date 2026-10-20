#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec4 vColour;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec4 vTangent;
layout(location = 4) in float vTerrain;
layout(location = 5) in float vViewDepth;
layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 1, binding = 0) uniform sampler2D uNormalMap;
layout(push_constant) uniform Light { layout(offset = 112) vec4 light; } lighting;
layout(set = 2, binding = 0) uniform Environment { vec4 lightColour; vec4 fogColour; vec4 fogBeta; vec4 fogView; } environment;
layout(location = 0) out vec4 fragColor;
void main()
{
    vec3 normal = normalize(vNormal);
    vec3 albedo;
    if (vTerrain >= 0.0)
    {
        albedo = vColour.rgb;
    }
    else
    {
        vec3 tangent = vTangent.xyz - normal * dot(normal, vTangent.xyz);
        if (dot(tangent, tangent) > 1e-8)
        {
            tangent = normalize(tangent);
            vec3 bitangent = cross(normal, tangent) * vTangent.w;
            vec3 mapped = texture(uNormalMap, vUv).xyz * 2.0 - 1.0;
            normal = normalize(tangent * mapped.x + bitangent * mapped.y + normal * mapped.z);
        }
        vec4 sampled = texture(uTexture, vUv);
        albedo = vColour.rgb * sampled.rgb;
        if (environment.lightColour.w > 0.0 && vColour.a * sampled.a < environment.lightColour.w)
            discard;
    }
    float diffuse = max(dot(normal, lighting.light.xyz), 0.0);
    vec3 colour = albedo * (lighting.light.w + (1.0 - lighting.light.w) * diffuse * environment.lightColour.rgb);
    if (environment.fogBeta.r + environment.fogBeta.g + environment.fogBeta.b > 0.0)
    {
        vec2 ndc = gl_FragCoord.xy * environment.fogView.zw * 2.0 - 1.0;
        float distance_to_eye = vViewDepth * length(vec3(ndc * environment.fogView.xy, 1.0));
        colour = mix(environment.fogColour.rgb, colour, exp(-environment.fogBeta.rgb * distance_to_eye));
    }
    fragColor = vec4(colour, 1.0);
}
