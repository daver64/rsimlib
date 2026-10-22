#include "sl.h"

#include <cmath>
#include <cstdio>

namespace
{
    int failures = 0;

    void check(bool ok, const char *what)
    {
        if (!ok)
        {
            std::fprintf(stderr, "FAIL: %s\n", what);
            ++failures;
        }
    }

    sl::Camera test_camera()
    {
        sl::Camera camera;
        camera.position = {0.0f, 2.0f, 0.0f};
        camera.target = {0.0f, 2.0f, -10.0f};
        return camera;
    }
}

int main()
{
    using sl::PrecipitationType;
    using sl::PrecipitationLevel;

    // Presets scale with level and wind stiffens for stronger weather.
    {
        const auto light = sl::precipitation_preset(PrecipitationType::rain, PrecipitationLevel::light);
        const auto storm = sl::precipitation_preset(PrecipitationType::rain, PrecipitationLevel::storm);
        check(light.type == PrecipitationType::rain && storm.intensity == 1.0f, "preset type and intensity");
        check(light.intensity < storm.intensity && light.wind.x < storm.wind.x, "storm is stronger than light");
        check(sl::precipitation_preset(PrecipitationType::none, PrecipitationLevel::storm).intensity == 0.0f, "none preset is empty");
    }

    // The controller fades the old type out before the new one in, and ends at the target.
    {
        sl::PrecipitationController weather;
        check(weather.current().type == PrecipitationType::none, "controller starts empty");
        weather.set_immediately(sl::precipitation_preset(PrecipitationType::rain, PrecipitationLevel::heavy));
        const float start = weather.current().intensity;
        weather.transition_to(sl::precipitation_preset(PrecipitationType::snow, PrecipitationLevel::light), 2.0f);
        check(weather.transitioning(), "transition is pending");
        weather.update(0.5f);
        check(weather.current().type == PrecipitationType::rain && weather.current().intensity < start, "rain fades first");
        bool saw_zero = false;
        for (int i = 0; i < 400 && weather.transitioning(); ++i)
        {
            weather.update(0.02f);
            if (weather.current().intensity <= 0.0f)
                saw_zero = true;
            if (weather.current().type == PrecipitationType::snow)
                check(saw_zero, "snow only starts once rain has faded out");
        }
        check(!weather.transitioning() && weather.current().type == PrecipitationType::snow, "reaches snow");
        check(std::fabs(weather.current().intensity - 0.25f) < 1e-4f, "reaches target intensity");

        weather.transition_to({PrecipitationType::none, 1.0f}, 0.0f);
        check(weather.current().type == PrecipitationType::none && weather.current().intensity == 0.0f, "zero seconds snaps");

        // Changing only one property must still transition, and finish.
        auto settles = [](sl::PrecipitationConfig from, sl::PrecipitationConfig to, float seconds)
        {
            sl::PrecipitationController c;
            c.set_immediately(from);
            c.transition_to(to, seconds);
            const bool started = c.transitioning();
            for (int i = 0; i < 2000 && c.transitioning(); ++i)
                c.update(1.0f / 60.0f);
            return started && !c.transitioning();
        };
        sl::PrecipitationConfig base = sl::precipitation_preset(PrecipitationType::rain, PrecipitationLevel::moderate);
        sl::PrecipitationConfig changed = base;
        changed.fall_speed = base.fall_speed + 300.0f;
        check(settles(base, changed, 2.0f), "fall speed alone transitions");
        changed = base;
        changed.colour = {10, 200, 30, 255};
        check(settles(base, changed, 3.0f), "colour alone transitions");
        changed = base;
        changed.splashes = !base.splashes;
        check(settles(base, changed, 2.0f), "splash flag alone transitions");
        {
            sl::PrecipitationController c;
            c.set_immediately(base);
            changed = base;
            changed.colour = {10, 200, 30, 255};
            c.transition_to(changed, 0.0f);
            c.update(0.016f);
            check(c.current().colour.green == 200, "zero-second colour change snaps");
        }
    }

    // 2D: particle count follows intensity, snow piles up, then melts.
    {
        sl::Precipitation2D rain(1280, 720);
        rain.weather.set_immediately({PrecipitationType::rain, 0.5f});
        for (int i = 0; i < 30; ++i)
            rain.update(0.016f);
        const int half = rain.particle_count();
        rain.weather.set_immediately({PrecipitationType::rain, 1.0f});
        rain.update(0.016f);
        check(half > 0 && rain.particle_count() > half, "more intensity, more particles");
        for (int i = 0; i < 120; ++i)
            rain.update(0.016f);
        check(rain.splash_count() > 0, "rain makes splashes");
        rain.weather.set_immediately({PrecipitationType::none, 0.0f});
        for (int i = 0; i < 5; ++i)
            rain.update(0.016f);
        check(rain.particle_count() == 0, "clear weather has no particles");

        sl::Precipitation2D snow(1280, 720);
        snow.set_max_particles(3000);
        snow.weather.set_immediately({PrecipitationType::snow, 1.0f});
        float depth = 0.0f;
        for (int i = 0; i < 60 * 40; ++i)
            snow.update(0.016f);
        for (float x = 0.0f; x < 1280.0f; x += 4.0f)
            depth = std::max(depth, snow.snow_depth_at(x));
        check(depth > 1.0f, "snow accumulates");
        snow.weather.set_immediately({PrecipitationType::rain, 0.2f});
        for (int i = 0; i < 60 * 15; ++i)
            snow.update(0.016f);
        float after = 0.0f;
        for (float x = 0.0f; x < 1280.0f; x += 4.0f)
            after = std::max(after, snow.snow_depth_at(x));
        check(after < depth * 0.2f, "snow melts in rain");

        // Particles always stay inside the area.
        sl::Precipitation2D hail(400, 300);
        hail.weather.set_immediately({PrecipitationType::hail, 1.0f});
        bool bounced = false;
        for (int i = 0; i < 600; ++i)
        {
            hail.update(0.016f);
            if (hail.splash_count() > 0)
                bounced = true;
        }
        check(bounced, "hail bounces");
    }

    // 3D: simulation without a display.
    {
        sl::Precipitation3D rain;
        rain.set_max_particles(2000);
        rain.weather.set_immediately({PrecipitationType::rain, 0.5f});
        const sl::Camera camera = test_camera();
        for (int i = 0; i < 200; ++i)
            rain.update(0.016f, camera);
        check(rain.particle_count() == 1000, "3D count follows intensity");
        check(rain.splash_count() > 0, "3D rain splashes on the ground");

        sl::Precipitation3D snow;
        snow.weather.set_immediately({PrecipitationType::snow, 1.0f});
        snow.set_accumulation(0.5f, 0.1f);
        for (int i = 0; i < 300; ++i)
            snow.update(0.016f, camera);
        check(snow.snow_cover() > 0.5f, "3D snow cover builds");
        snow.weather.set_immediately({PrecipitationType::none, 0.0f});
        for (int i = 0; i < 600; ++i)
            snow.update(0.016f, camera);
        check(snow.snow_cover() < 0.5f && snow.particle_count() == 0, "3D snow cover melts");

        sl::CloudLayer clouds;
        clouds.wind = {10.0f, -4.0f};
        const glm::vec2 wind = sl::Precipitation3D::wind_from_clouds(clouds, 0.5f);
        check(wind.x == 5.0f && wind.y == -2.0f, "wind follows clouds");
    }

    // Environment, tint and sound level helpers.
    {
        sl::Environment3D environment;
        environment.fog_density = 0.0002f;
        const sl::Environment3D clear = environment;
        sl::precipitation_apply_environment(environment, {PrecipitationType::rain, 1.0f});
        check(environment.fog_density > clear.fog_density, "weather thickens fog");
        check(environment.clouds.enabled && environment.clouds.coverage >= 0.9f, "weather clouds over");
        sl::Environment3D untouched;
        sl::precipitation_apply_environment(untouched, {PrecipitationType::none, 1.0f});
        check(untouched.fog_density == sl::Environment3D().fog_density, "no weather leaves the environment alone");

        const sl::Colour white = sl::snow_tinted({20, 100, 30, 255}, 1.0f);
        check(white.red > 230 && white.green > 230, "full cover is white");
        check(sl::snow_tinted({20, 100, 30, 255}, 0.0f).green == 100, "no cover keeps the colour");

        check(sl::PrecipitationAudio::volume_for({PrecipitationType::none, 1.0f}) == 0, "no weather is silent");
        check(sl::PrecipitationAudio::volume_for({PrecipitationType::rain, 0.2f}) <
                  sl::PrecipitationAudio::volume_for({PrecipitationType::rain, 1.0f}),
              "heavier rain is louder");
        check(sl::PrecipitationAudio::volume_for({PrecipitationType::snow, 1.0f}) <
                  sl::PrecipitationAudio::volume_for({PrecipitationType::rain, 1.0f}),
              "snow is quieter than rain");
    }

    if (failures)
    {
        std::fprintf(stderr, "%d precipitation checks failed\n", failures);
        return 1;
    }
    std::puts("precipitation_test: OK");
    return 0;
}
