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
    sl::Bitmap *processed_scene = sl::create_render_target(800, 600);
    sl::Bitmap *effect_scene = sl::create_render_target(800, 600);
    if (!balloon || !scene || !processed_scene || !effect_scene)
    {
        sl::destroy_bitmap(balloon);
        sl::destroy_bitmap(scene);
        sl::destroy_bitmap(processed_scene);
        sl::destroy_bitmap(effect_scene);
        sl::shutdown();
        return -1;
    }

    sl::Bloom bloom;
    sl::Vignette vignette;
    sl::ColourAdjust colour_adjust;
    sl::Blur blur;
    const bool bloom_ready = bloom.initialise();
    const bool vignette_ready = vignette.initialise();
    const bool colour_adjust_ready = colour_adjust.initialise();
    const bool blur_ready = blur.initialise();
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

    bool use_bloom = bloom_ready;
    bool use_vignette = vignette_ready;
    bool use_colour_adjust = false;
    bool use_blur = false;
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
            }
            sl::display_handle_event(event);
        }

        const float time = static_cast<float>(sl::time_ms()) * 0.001f;
        sl::begin_render_target(scene);
        sl::clear_render_target({18, 23, 34});
        sl::gprintf_center(30, {232, 236, 244}, "Post-processing example");
        sl::gprintf_center(56, {170, 185, 205},
                "B: Bloom %s  V: Vignette %s  C: Colour %s  L: Blur %s  Escape: exit",
            use_bloom ? "on" : "off", use_vignette ? "on" : "off",
                use_colour_adjust ? "on" : "off", use_blur ? "on" : "off");
        sl::circlefill(sl::screen, 400.0f + std::cos(time) * 180.0f,
            280.0f + std::sin(time * 1.4f) * 110.0f, 58.0f, {255, 215, 92});
        sl::circlefill(sl::screen, 180.0f, 420.0f, 34.0f, {80, 190, 255});
        sl::circlefill(sl::screen, 625.0f, 405.0f, 42.0f, {255, 90, 125});
        sl::draw_sprite_rotated(balloon, 400.0f, 310.0f, time * 35.0f);
        sl::end_render_target();

        sl::clear_to_colour(sl::screen, {8, 11, 18});
        const bool flip_source = sl::graphics_backend() == sl::GraphicsBackend::opengl;
        sl::Bitmap *effect_source = scene;
        if (use_colour_adjust)
        {
            sl::begin_render_target(processed_scene);
            sl::clear_render_target({8, 11, 18});
            colour_adjust.apply(effect_source, 0, 0, 800, 600, flip_source);
            sl::end_render_target();
            effect_source = processed_scene;
        }
        if (use_blur)
        {
            sl::begin_render_target(effect_scene);
            sl::clear_render_target({8, 11, 18});
            blur.apply(effect_source, 0, 0, 800, 600, flip_source);
            sl::end_render_target();
            effect_source = effect_scene;
        }
        if (use_vignette)
        {
            sl::Bitmap *vignette_target = effect_source == processed_scene ? effect_scene : processed_scene;
            sl::begin_render_target(vignette_target);
            sl::clear_render_target({8, 11, 18});
            vignette.apply(effect_source, 0, 0, 800, 600, flip_source);
            sl::end_render_target();
            effect_source = vignette_target;
        }
        if (use_bloom) bloom.apply(effect_source, 0, 0, 800, 600, flip_source);
        else if (flip_source) sl::draw_sprite_v_flip(effect_source, 0.0f, 0.0f);
        else sl::draw_sprite(effect_source, 0.0f, 0.0f);
        sl::show_video_bitmap();
        sl::end_frame();
    }

    vignette.shutdown();
    bloom.shutdown();
    colour_adjust.shutdown();
    blur.shutdown();
    sl::destroy_bitmap(effect_scene);
    sl::destroy_bitmap(processed_scene);
    sl::destroy_bitmap(scene);
    sl::destroy_bitmap(balloon);
    sl::shutdown();
    return 0;
}