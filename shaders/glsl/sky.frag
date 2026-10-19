#version 430 core

// Procedural sky: gradient or scattered-light atmosphere, sun disc and a 2D cloud layer. The row layout of uSky is
// documented in src/engine/atmosphere.cpp (SkyRows) and must match.
in vec2 vNdc;
uniform vec4 uSky[14];
out vec4 fragColor;

const float PI = 3.14159265;
const int VIEW_STEPS = 16;
const int LIGHT_STEPS = 6;

bool ray_sphere(vec3 ro, vec3 rd, float radius, out float t0, out float t1)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float d = b * b - c;
    if (d < 0.0)
        return false;
    d = sqrt(d);
    t0 = -b - d;
    t1 = -b + d;
    return true;
}

// Rayleigh and Mie optical depth along a path of length `len` from `p` toward `d`.
void optical_depth(vec3 p, vec3 d, float len, float planet_radius, float hr, float hm, out float od_r, out float od_m)
{
    od_r = 0.0;
    od_m = 0.0;
    float ds = len / float(LIGHT_STEPS);
    for (int i = 0; i < LIGHT_STEPS; ++i)
    {
        float h = max(length(p + d * (float(i) + 0.5) * ds) - planet_radius, 0.0);
        od_r += exp(-h / hr) * ds;
        od_m += exp(-h / hm) * ds;
    }
}

// Transmittance of sunlight from `p` out of the atmosphere (zero when the planet blocks it).
vec3 sun_transmittance(vec3 p, vec3 sun, vec3 br, float bm, float hr, float hm, float planet_radius, float atmosphere_radius)
{
    float q0, q1;
    if (ray_sphere(p, sun, planet_radius, q0, q1) && q1 > 0.0 && q0 > 0.0)
        return vec3(0.0);
    float s0, s1;
    if (!ray_sphere(p, sun, atmosphere_radius, s0, s1))
        return vec3(1.0);
    float od_r, od_m;
    optical_depth(p, sun, s1, planet_radius, hr, hm, od_r, od_m);
    return exp(-(br * od_r + 1.11 * bm * od_m));
}

vec3 planet_sky(vec3 ro, vec3 rd, vec3 sun, float sun_intensity, vec3 ground, float cos_sun_radius, out vec3 view_t)
{
    vec3 br = uSky[7].rgb;
    float hr = uSky[7].w;
    float bm = uSky[8].x;
    float hm = uSky[8].y;
    float planet_radius = uSky[8].z;
    float atmosphere_radius = planet_radius + uSky[8].w;
    float g = uSky[6].w;
    view_t = vec3(1.0);

    float t0, t1;
    if (!ray_sphere(ro, rd, atmosphere_radius, t0, t1))
        return vec3(0.0);
    t0 = max(t0, 0.0);
    bool hits_planet = false;
    float p0, p1;
    if (ray_sphere(ro, rd, planet_radius, p0, p1) && p1 > 0.0)
    {
        float hit = p0 > 0.0 ? p0 : p1;
        if (hit > t0)
        {
            t1 = min(t1, hit);
            hits_planet = true;
        }
    }
    float len = t1 - t0;
    if (len <= 0.0)
        return vec3(0.0);

    float ds = len / float(VIEW_STEPS);
    float od_r = 0.0;
    float od_m = 0.0;
    vec3 sum_r = vec3(0.0);
    vec3 sum_m = vec3(0.0);
    for (int i = 0; i < VIEW_STEPS; ++i)
    {
        vec3 p = ro + rd * (t0 + (float(i) + 0.5) * ds);
        float h = max(length(p) - planet_radius, 0.0);
        float dr = exp(-h / hr) * ds;
        float dm = exp(-h / hm) * ds;
        od_r += dr;
        od_m += dm;
        float s0, s1;
        if (!ray_sphere(p, sun, atmosphere_radius, s0, s1))
            continue;
        float q0, q1;
        if (ray_sphere(p, sun, planet_radius, q0, q1) && q1 > 0.0 && q0 > 0.0)
            continue;
        float light_r, light_m;
        optical_depth(p, sun, s1, planet_radius, hr, hm, light_r, light_m);
        vec3 attenuation = exp(-(br * (od_r + light_r) + 1.11 * bm * (od_m + light_m)));
        sum_r += attenuation * dr;
        sum_m += attenuation * dm;
    }
    float mu = dot(rd, sun);
    float phase_r = 3.0 / (16.0 * PI) * (1.0 + mu * mu);
    float phase_m = 3.0 / (8.0 * PI) * ((1.0 - g * g) * (1.0 + mu * mu)) /
                    ((2.0 + g * g) * pow(max(1.0 + g * g - 2.0 * g * mu, 1e-4), 1.5));
    vec3 colour = sun_intensity * (sum_r * br * phase_r + sum_m * bm * phase_m);
    view_t = exp(-(br * od_r + 1.11 * bm * od_m));

    if (hits_planet)
    {
        vec3 hit_point = ro + rd * t1;
        vec3 normal = normalize(hit_point);
        float lit = max(dot(normal, sun), 0.0);
        vec3 arriving = sun_transmittance(hit_point + normal * 2.0, sun, br, bm, hr, hm, planet_radius, atmosphere_radius);
        colour += ground * lit * arriving * view_t * sun_intensity * 0.12;
    }
    else
    {
        float disc = smoothstep(cos_sun_radius - 0.00004, cos_sun_radius + 0.00002, mu);
        vec3 arriving = sun_transmittance(ro, sun, br, bm, hr, hm, planet_radius, atmosphere_radius);
        colour += disc * arriving * view_t * sun_intensity * 4.0;
    }
    return colour;
}

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float hash31(vec3 p)
{
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

float value_noise2(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1, 0)), f.x), mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x), f.y);
}

float value_noise3(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float x00 = mix(hash31(i), hash31(i + vec3(1, 0, 0)), f.x);
    float x10 = mix(hash31(i + vec3(0, 1, 0)), hash31(i + vec3(1, 1, 0)), f.x);
    float x01 = mix(hash31(i + vec3(0, 0, 1)), hash31(i + vec3(1, 0, 1)), f.x);
    float x11 = mix(hash31(i + vec3(0, 1, 1)), hash31(i + vec3(1, 1, 1)), f.x);
    return mix(mix(x00, x10, f.y), mix(x01, x11, f.y), f.z);
}

float cloud_fbm2(vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; ++i)
    {
        v += a * value_noise2(p);
        p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.03 + 17.1;
        a *= 0.5;
    }
    return v;
}

float cloud_fbm3(vec3 p)
{
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; ++i)
    {
        v += a * value_noise3(p);
        p = p * 2.03 + vec3(17.1, 9.2, 31.7);
        a *= 0.5;
    }
    return v;
}

// Cloud opacity, 0-1, at a layer position. `p` holds either plane coordinates (flat) or a world position (planet).
float cloud_density(vec3 p, bool planet)
{
    float scale = uSky[9].w;
    float coverage = uSky[9].y;
    float softness = uSky[10].x;
    vec2 wind = uSky[10].yz;
    float n;
    if (planet)
        n = cloud_fbm3(p / scale + vec3(wind.x, 0.0, wind.y) / scale);
    else
        n = cloud_fbm2(p.xy / scale + wind / scale);
    float edge = mix(0.78, 0.30, coverage);
    return smoothstep(edge, edge + 0.05 + softness * 0.35, n);
}

void main()
{
    vec3 rd = normalize(uSky[0].xyz * (vNdc.x * uSky[0].w) + uSky[1].xyz * (vNdc.y * uSky[1].w) + uSky[2].xyz);
    int flags = int(uSky[2].w + 0.5);
    bool planet = (flags & 1) != 0;
    bool clouds = (flags & 2) != 0;
    vec3 sun = uSky[3].xyz;
    float sun_intensity = uSky[3].w;
    vec3 up = uSky[4].xyz;
    float altitude = uSky[4].w;
    float cos_sun_radius = uSky[5].w;
    vec3 sun_colour = uSky[13].rgb;

    vec3 sky;
    if (planet)
    {
        float planet_radius = uSky[8].z;
        vec3 view_t;
        vec3 scattered = planet_sky(up * (planet_radius + altitude), rd, sun, sun_intensity, uSky[12].rgb, cos_sun_radius, view_t);
        sky = 1.0 - exp(-scattered);
    }
    else
    {
        float elevation = dot(rd, up);
        vec3 zenith = uSky[5].rgb;
        vec3 horizon = uSky[6].rgb;
        sky = mix(horizon, zenith, pow(clamp(elevation, 0.0, 1.0), 0.45));
        if (elevation < 0.0)
            sky = mix(horizon, horizon * 0.55, smoothstep(0.0, 0.25, -elevation));
        float mu = max(dot(rd, sun), 0.0);
        sky += sun_colour * (pow(mu, 48.0) * 0.18 + pow(mu, 600.0) * 0.4);
        sky = mix(sky, sun_colour * 4.0, smoothstep(cos_sun_radius - 0.00004, cos_sun_radius + 0.00002, mu) * step(0.0, elevation));
    }

    if (clouds)
    {
        float cloud_altitude = uSky[9].x;
        float elevation = dot(rd, up);
        vec3 camera = uSky[11].xyz;
        float hit = -1.0;
        if (planet)
        {
            float planet_radius = uSky[8].z;
            vec3 ro = up * (planet_radius + altitude);
            float t0, t1;
            float shell = planet_radius + cloud_altitude;
            if (ray_sphere(ro, rd, shell, t0, t1))
                hit = altitude < cloud_altitude ? t1 : (t0 > 0.0 ? t0 : -1.0);
            // The horizon hides the part of the shell below the planet's limb.
            float q0, q1;
            if (hit > 0.0 && ray_sphere(ro, rd, planet_radius, q0, q1) && q0 > 0.0 && q0 < hit)
                hit = -1.0;
        }
        else if (altitude < cloud_altitude && elevation > 0.002)
        {
            hit = (cloud_altitude - altitude) / elevation;
        }
        if (hit > 0.0)
        {
            vec3 q = camera + rd * hit;
            vec3 e1 = normalize(cross(up, abs(up.z) < 0.9 ? vec3(0, 0, 1) : vec3(1, 0, 0)));
            vec3 e2 = cross(up, e1);
            vec3 layer = planet ? q : vec3(dot(q, e1), dot(q, e2), 0.0);
            float d = cloud_density(layer, planet);
            if (d > 0.001)
            {
                // Self shadowing: how thick the cloud is a little way toward the sun.
                vec3 step_toward_sun = planet ? sun * uSky[9].w * 0.12 : vec3(dot(sun, e1), dot(sun, e2), 0.0) * uSky[9].w * 0.2;
                float toward = cloud_density(layer + step_toward_sun, planet);
                float lit = clamp(1.0 - 0.75 * toward + 0.25 * (1.0 - d), 0.0, 1.0);
                float sun_side = clamp(dot(up, sun) * 4.0 + 0.5, 0.0, 1.0);
                vec3 ambient = mix(uSky[6].rgb, uSky[5].rgb, 0.5) * 0.75;
                if (planet)
                    ambient = sky * 0.9 + 0.05;
                vec3 colour = mix(ambient * 0.7, sun_colour * 1.15, lit * sun_side) + ambient * 0.25;
                float horizon_fade = smoothstep(0.0, planet ? 0.06 : 0.14, abs(elevation));
                float alpha = d * uSky[9].z * horizon_fade;
                sky = mix(sky, min(colour, vec3(1.5)), alpha);
            }
        }
    }
    fragColor = vec4(sky, 1.0);
}
