#include "sl.h"

int main(int argc, char *argv[])
{
    if (!simlib::set_gfx_mode(simlib::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    simlib::Bitmap *balloon = simlib::load_bitmap("assets/textures/balloon_red.png");
    if (!balloon)
    {
        simlib::shutdown();
        return -1;
    }

    simlib::set_fps(60);
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
            {
                running = false;
            }
        }

        const float angle_degrees = static_cast<float>(simlib::time_ms() % 3600) * 0.1f;
        simlib::clear_to_colour(simlib::screen, {45, 48, 56});
        simlib::gprintf_center(32, {0, 255, 0}, "Rotating sprite example - press Escape to exit");
        simlib::draw_sprite_rotated(balloon, 400.0f, 300.0f, angle_degrees);
        simlib::show_video_bitmap();
        simlib::end_frame();
    }

    simlib::destroy_bitmap(balloon);
    simlib::shutdown();
    return 0;
}
