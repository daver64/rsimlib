#include "event.h"
#include "input.h"

#include <SDL2/SDL.h>

#include <iostream>

namespace
{
    bool push_event(SDL_Event event)
    {
        return SDL_PushEvent(&event) == 1;
    }
}

int main()
{
    if (SDL_Init(SDL_INIT_EVENTS) != 0)
    {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Event key_event{};
    key_event.type = SDL_KEYDOWN;
    key_event.key.keysym.sym = SDLK_ESCAPE;
    key_event.key.repeat = 1;

    SDL_Event function_key_event{};
    function_key_event.type = SDL_KEYDOWN;
    function_key_event.key.keysym.sym = SDLK_F12;

    SDL_Event keypad_key_event{};
    keypad_key_event.type = SDL_KEYDOWN;
    keypad_key_event.key.keysym.sym = SDLK_KP_7;

    SDL_Event resize_event{};
    resize_event.type = SDL_WINDOWEVENT;
    resize_event.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    resize_event.window.data1 = 640;
    resize_event.window.data2 = 480;

    SDL_Event close_event{};
    close_event.type = SDL_WINDOWEVENT;
    close_event.window.event = SDL_WINDOWEVENT_CLOSE;

    SDL_Event user_event{};
    user_event.type = SDL_USEREVENT;

    SDL_Event mouse_button_event{};
    mouse_button_event.type = SDL_MOUSEBUTTONDOWN;
    mouse_button_event.button.button = SDL_BUTTON_RIGHT;

    if (!push_event(key_event) || !push_event(function_key_event) || !push_event(keypad_key_event) || !push_event(resize_event) || !push_event(close_event) || !push_event(user_event) || !push_event(mouse_button_event))
    {
        SDL_Quit();
        return 1;
    }

    sl::Event event;
    const bool passed =
        sl::poll_event(&event) && event.type() == sl::Event::Type::key_down &&
        event.key() == sl::Event::Key::escape && event.key_repeat() &&
        sl::poll_event(&event) && event.key() == sl::Event::Key::f12 && event.key_code() == SDLK_F12 &&
        sl::poll_event(&event) && event.key() == sl::Event::Key::keypad_7 && event.key_code() == SDLK_KP_7 &&
        sl::poll_event(&event) && event.type() == sl::Event::Type::window_resized &&
        event.window_width() == 640 && event.window_height() == 480 &&
        sl::poll_event(&event) && event.type() == sl::Event::Type::quit &&
        sl::poll_event(&event) && event.type() == sl::Event::Type::user &&
        sl::poll_event(&event) && event.type() == sl::Event::Type::mouse_button_down &&
        event.mouse_button() == SDL_BUTTON_RIGHT;

    SDL_Quit();
    return passed ? 0 : 1;
}