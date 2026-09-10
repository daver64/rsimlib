#include "game.h"

#include <string_view>

/**
 * @brief Initialise the sample game, run its event/render loop, and release it.
 *
 * The game namespace owns the selected screen and calls into simlib for display,
 * timing, input, rendering, audio, and GUI integration.
 */
int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) || !game::initialise())
    {
        return -1;
    }

    // Main loop
    while (game::is_running())
    {
        game::handle_events();
        game::update_and_render();
    }

    // Clean up
    game::shutdown();
    return 0;
}
