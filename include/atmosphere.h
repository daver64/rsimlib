#pragma once

#include "draw.h"
#include "surface3d.h"

#include <glm/glm.hpp>

namespace sl
{
    /** A procedural cloud layer drawn in the sky: a plane above flat worlds, a shell around planets. */
    struct CloudLayer
    {
        bool enabled = false;
        /** Height of the layer above the surface, in metres. */
        float altitude = 2500.0f;
        /** 0 = clear sky, 1 = overcast. */
        float coverage = 0.5f;
        /** Opacity of the thickest cloud, 0-1. */
        float density = 0.9f;
        /** Size of the largest cloud features, in metres. */
        float scale = 6000.0f;
        /** Edge softness, 0 (hard) to 1 (wispy). */
        float softness = 0.35f;
        /** Wind velocity along the layer in metres per second (x, z of the local horizontal plane). */
        glm::vec2 wind{12.0f, 4.0f};
    };

    /**
     * Sky, fog and light settings for a 3D surface. Flat worlds use a gradient sky and exponential height fog; with
     * `planet` set the sky is scattered Rayleigh/Mie light, and distant geometry takes on the matching haze (aerial
     * perspective). Everything is expressed in metres. Call set_environment_3d() each frame so the camera altitude and
     * time stay current.
     */
    struct Environment3D
    {
        /** Direction the sunlight travels, as for Light3D::direction. */
        glm::vec3 sun_direction{-0.4f, -1.0f, -0.6f};
        /** Brightness of the sun; planets need about 20 for the scattering to look right. */
        float sun_intensity = 1.0f;
        /** Angular diameter of the sun disc in degrees (0 hides it). */
        float sun_diameter = 1.2f;
        /** Local vertical at the camera, pointing away from the ground (away from the planet centre). */
        glm::vec3 up{0.0f, 1.0f, 0.0f};
        /** Camera height above the surface in metres; thins the fog and the planet's atmosphere. */
        float camera_altitude = 0.0f;
        /** World position of the camera in double precision; keeps clouds fixed in the world as the camera moves. */
        glm::dvec3 camera_position{0.0};
        /** Seconds since start; scrolls the clouds. */
        double time = 0.0;

        /** Gradient sky for flat worlds. */
        Colour sky_zenith{70, 120, 215, 255};
        Colour sky_horizon{175, 205, 235, 255};

        /** Fog for flat worlds: extinction per metre at ground level, thinning with the camera altitude. */
        bool fog_enabled = true;
        Colour fog_colour{175, 205, 235, 255};
        float fog_density = 0.0f;
        /** Per metre of camera altitude, the fog density falls by this exponent (0 = uniform). */
        float fog_height_falloff = 0.0004f;

        /** Scattered-light atmosphere for planets. Overrides the gradient sky and the fog colour. */
        bool planet = false;
        float planet_radius = 6.371e6f;
        float atmosphere_height = 100.0e3f;
        /** Scattering per metre at sea level for red, green and blue. */
        glm::vec3 rayleigh{5.8e-6f, 13.5e-6f, 33.1e-6f};
        float mie = 21.0e-6f;
        float rayleigh_scale_height = 8000.0f;
        float mie_scale_height = 1200.0f;
        /** Mie anisotropy, 0 (isotropic) to 1 (forward). */
        float mie_g = 0.76f;
        /** Multiplies the haze extinction on terrain so distant mountains fade at a chosen range. */
        float haze_scale = 1.0f;
        /** Colour of the ground seen from space or through gaps in terrain. */
        glm::vec3 ground_albedo{0.18f, 0.2f, 0.15f};

        CloudLayer clouds;
    };

    /**
     * Use this environment for later draws on the active 3D surface: fog and haze on meshes with Material::fog, and
     * the sky for draw_sky_3d(). Replaces the surface's light colour with a sunset-aware sun colour only if you apply
     * atmosphere_sun_colour() yourself. Call between begin_surface_3d() and the first draw.
     */
    void set_environment_3d(const Environment3D &environment);

    /** Remove the environment: no fog, and draw_sky_3d() does nothing. */
    void clear_environment_3d();

    /**
     * Draw the sky behind everything already drawn (and anything drawn later). Needs set_environment_3d() first.
     * Supported on the OpenGL, Vulkan and Direct3D 12 backends; returns false if the backend cannot draw it.
     */
    bool draw_sky_3d();

    /** Colour of the sky seen along `direction` (unit vector) with the sun in the environment. Mirrors the shader. */
    glm::vec3 atmosphere_sky_colour(const Environment3D &environment, const glm::vec3 &direction);

    /** Sunlight colour after passing through the atmosphere at `camera_altitude`; reddens toward the horizon. */
    glm::vec3 atmosphere_sun_colour(const Environment3D &environment);

    /** Fraction of light surviving a straight path of `distance` metres through fog or haze (per channel). */
    glm::vec3 atmosphere_transmittance(const Environment3D &environment, float distance);

} // namespace sl
