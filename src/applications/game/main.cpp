#include "game.h"

/** Initialize subsystems and run the SDL event/render loop. */
int main(int argc, char *argv[])
{
    if (!game::initialise())
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
