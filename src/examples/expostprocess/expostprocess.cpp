#include "sl.h"

#include <algorithm>
#include <cmath>

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::Bitmap *balloon = sl::load_bitmap("assets/textures/balloon_red.png");
    sl::Bitmap *scene = sl::create_render_target(800, 600);
    sl::PingPongBuffer post_process;
    post_process.initialise(800, 600);
    if (!balloon || !scene || !post_process.valid())
    {
        sl::destroy_bitmap(balloon);
        sl::destroy_bitmap(scene);
        post_process.shutdown();
        sl::shutdown();
        return -1;
    }

    sl::Bloom bloom;
    sl::Vignette vignette;
    sl::ColourAdjust colour_adjust;
    sl::Blur blur;
    sl::ChromaticAberration chromatic_aberration;
    sl::Pixelate pixelate;
    sl::RadialBlur radial_blur;
    sl::HeatHaze heat_haze;
    sl::CRTFilter crt_filter;
    sl::DitherFilter dither_filter;
    sl::ScreenShake screen_shake;
    const bool bloom_ready = bloom.initialise();
    const bool vignette_ready = vignette.initialise();
    const bool colour_adjust_ready = colour_adjust.initialise();
    const bool blur_ready = blur.initialise();
    const bool chromatic_ready = chromatic_aberration.initialise();
    const bool pixelate_ready = pixelate.initialise();
    const bool radial_ready = radial_blur.initialise();
    const bool heat_ready = heat_haze.initialise();
    const bool crt_ready = crt_filter.initialise();
    const bool dither_ready = dither_filter.initialise();
    bloom.set_threshold(0.35f);
    bloom.set_intensity(1.15f);
    bloom.set_radius(1.8f);
    vignette.set_radius(0.68f);
    vignette.set_softness(0.42f);
    vignette.set_intensity(0.72f);
    colour_adjust.set_brightness(0.08f);
    colour_adjust.set_contrast(1.35f);
    colour_adjust.set_saturation(1.5f);
    colour_adjust.set_exposure(0.25f);
    blur.set_radius(2.5f);
    blur.set_iterations(2);
    chromatic_aberration.set_strength(0.018f);
    pixelate.set_pixel_size(10.0f);
    radial_blur.set_centre(0.5f, 0.5f);
    radial_blur.set_strength(0.32f);
    radial_blur.set_samples(10);
    heat_haze.set_strength(0.008f);
    heat_haze.set_frequency(24.0f);
    crt_filter.set_pixel_size(4.0f);
    crt_filter.set_scanline_strength(0.35f);
    crt_filter.set_curvature(0.18f);
    dither_filter.set_pixel_size(2.0f);
    dither_filter.set_levels(4.0f);

    bool use_bloom = bloom_ready;
    bool use_vignette = vignette_ready;
    bool use_colour_adjust = false;
    bool use_blur = false;
    bool use_chromatic = false;
    bool use_pixelate = false;
    bool use_radial = false;
    bool use_heat = false;
    bool use_crt = false;
    bool use_dither = false;
    bool running = true;
    sl::set_fps(60);

    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down &&
                 event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
            if (event.type() == sl::Event::Type::key_down && !event.key_repeat())
            {
                if (event.key() == sl::Event::Key::letter_b && bloom_ready)
                    use_bloom = !use_bloom;
                else if (event.key() == sl::Event::Key::letter_v && vignette_ready)
                    use_vignette = !use_vignette;
                else if (event.key() == sl::Event::Key::letter_c && colour_adjust_ready)
                    use_colour_adjust = !use_colour_adjust;
                else if (event.key() == sl::Event::Key::letter_l && blur_ready)
                    use_blur = !use_blur;
                else if (event.key() == sl::Event::Key::letter_a && chromatic_ready)
                    use_chromatic = !use_chromatic;
                else if (event.key() == sl::Event::Key::letter_p && pixelate_ready)
                    use_pixelate = !use_pixelate;
                else if (event.key() == sl::Event::Key::letter_r && radial_ready)
                    use_radial = !use_radial;
                else if (event.key() == sl::Event::Key::letter_h && heat_ready)
                    use_heat = !use_heat;
                else if (event.key() == sl::Event::Key::letter_t && crt_ready)
                    use_crt = !use_crt;
                else if (event.key() == sl::Event::Key::letter_d && dither_ready)
                    use_dither = !use_dither;
                else if (event.key() == sl::Event::Key::letter_s)
                    screen_shake.trigger(12.0f, 0.45f);
            }
            sl::display_handle_event(event);
        }

        const float time = static_cast<float>(sl::time_ms()) * 0.001f;
        screen_shake.update(std::min(0.05f, static_cast<float>(sl::get_frame_time()) / 1000.0f));
        sl::begin_render_target(scene);
        sl::clear_render_target({18, 23, 34});
        sl::gprintf_center(30, {232, 236, 244}, "Post-processing example");
        sl::gprintf_center(56, {170, 185, 205}, "B: Bloom %s  V: Vignette %s  C: Colour %s  L: Blur %s",
            use_bloom ? "on" : "off", use_vignette ? "on" : "off",
            use_colour_adjust ? "on" : "off", use_blur ? "on" : "off");
        sl::gprintf_center(78, {170, 185, 205}, "A: Aberration %s  P: Pixelate %s  R: Radial %s  H: Haze %s  T: CRT %s",
            use_chromatic ? "on" : "off", use_pixelate ? "on" : "off", use_radial ? "on" : "off", use_heat ? "on" : "off", use_crt ? "on" : "off");
        sl::gprintf_center(100, {170, 185, 205}, "S: Shake  D: Dither %s", use_dither ? "on" : "off");
        sl::circlefill(sl::screen, 400.0f + std::cos(time) * 180.0f,
            280.0f + std::sin(time * 1.4f) * 110.0f, 58.0f, {255, 215, 92});
        sl::circlefill(sl::screen, 180.0f, 420.0f, 34.0f, {80, 190, 255});
        sl::circlefill(sl::screen, 625.0f, 405.0f, 42.0f, {255, 90, 125});
        sl::draw_sprite_rotated(balloon, 400.0f, 310.0f, time * 35.0f);
        sl::end_render_target();

        screen_shake.clear();
        sl::clear_to_colour(sl::screen, {8, 11, 18});
        sl::Bitmap *effect_source = scene;
        if (use_colour_adjust)
        {
            post_process.begin(effect_source);
            sl::begin_render_target(post_process.target());
            sl::clear_render_target({8, 11, 18});
            colour_adjust.apply(post_process.source(), 0, 0, 800, 600);
            sl::end_render_target();
            effect_source = post_process.advance();
        }
        if (use_blur)
        {
            post_process.begin(effect_source);
            sl::begin_render_target(post_process.target());
            sl::clear_render_target({8, 11, 18});
            blur.apply(post_process.source(), 0, 0, 800, 600);
            sl::end_render_target();
            effect_source = post_process.advance();
        }
        if (use_chromatic)
        {
            post_process.begin(effect_source);
            sl::begin_render_target(post_process.target());
            sl::clear_render_target({8, 11, 18});
            chromatic_aberration.apply(post_process.source(), 0, 0, 800, 600);
            sl::end_render_target();
            effect_source = post_process.advance();
        }
        if (use_pixelate)
        {
            post_process.begin(effect_source);
            sl::begin_render_target(post_process.target());
            sl::clear_render_target({8, 11, 18});
            pixelate.apply(post_process.source(), 0, 0, 800, 600);
            sl::end_render_target();
            effect_source = post_process.advance();
        }
        if (use_radial)
        {
            post_process.begin(effect_source);
            sl::begin_render_target(post_process.target());
            sl::clear_render_target({8, 11, 18});
            radial_blur.apply(post_process.source(), 0, 0, 800, 600);
            sl::end_render_target();
            effect_source = post_process.advance();
        }
        if (use_heat)
        {
            heat_haze.set_time(static_cast<float>(sl::time_ms()) * 0.001f);
            post_process.begin(effect_source);
            sl::begin_render_target(post_process.target());
            sl::clear_render_target({8, 11, 18});
            heat_haze.apply(post_process.source(), 0, 0, 800, 600);
            sl::end_render_target();
            effect_source = post_process.advance();
        }
        if (use_vignette)
        {
            post_process.begin(effect_source);
            sl::begin_render_target(post_process.target());
            sl::clear_render_target({8, 11, 18});
            vignette.apply(post_process.source(), 0, 0, 800, 600);
            sl::end_render_target();
            effect_source = post_process.advance();
        }
        if (use_crt)
        {
            post_process.begin(effect_source);
            sl::begin_render_target(post_process.target());
            sl::clear_render_target({8, 11, 18});
            crt_filter.apply(post_process.source(), 0, 0, 800, 600);
            sl::end_render_target();
            effect_source = post_process.advance();
        }
        if (use_dither)
        {
            post_process.begin(effect_source);
            sl::begin_render_target(post_process.target());
            sl::clear_render_target({8, 11, 18});
            dither_filter.apply(post_process.source(), 0, 0, 800, 600);
            sl::end_render_target();
            effect_source = post_process.advance();
        }
        if (use_bloom) bloom.apply(effect_source, 0, 0, 800, 600);
        else if (sl::graphics_backend() == sl::GraphicsBackend::opengl)
            sl::draw_sprite_v_flip(effect_source, 0.0f, 0.0f);
        else
            sl::draw_sprite(effect_source, 0.0f, 0.0f);
        sl::show_video_bitmap();
        sl::end_frame();
    }

    vignette.shutdown();
    bloom.shutdown();
    colour_adjust.shutdown();
    blur.shutdown();
    chromatic_aberration.shutdown();
    pixelate.shutdown();
    radial_blur.shutdown();
    heat_haze.shutdown();
    crt_filter.shutdown();
    dither_filter.shutdown();
    post_process.shutdown();
    sl::destroy_bitmap(scene);
    sl::destroy_bitmap(balloon);
    sl::shutdown();
    return 0;
}