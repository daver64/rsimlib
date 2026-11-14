// On-screen touch buttons for the Android builds of the example games.
// The bottom fifth of the screen is split into equal-width buttons; each one drives a bool.
#pragma once

#ifdef __ANDROID__
#include "sl.h"

#include <algorithm>
#include <initializer_list>

namespace example_touch
{
    struct Button
    {
        const char *label;
        bool *state;
    };

    constexpr float strip_top = 0.80f;

    inline void poll(std::initializer_list<Button> buttons)
    {
        const int count = static_cast<int>(buttons.size());
        for (int device = 0; device < SDL_GetNumTouchDevices(); ++device)
        {
            const SDL_TouchID id = SDL_GetTouchDevice(device);
            for (int finger = 0; finger < SDL_GetNumTouchFingers(id); ++finger)
            {
                const SDL_Finger *touch = SDL_GetTouchFinger(id, finger);
                if (!touch || touch->y < strip_top)
                    continue;
                const int index = std::min(static_cast<int>(touch->x * count), count - 1);
                *(buttons.begin() + index)->state = true;
            }
        }
    }

    inline void draw(std::initializer_list<Button> buttons, int window_w, int window_h)
    {
        const int count = static_cast<int>(buttons.size());
        int i = 0;
        for (const Button &b : buttons)
        {
            const float left = i * (static_cast<float>(window_w) / count) + 4.0f;
            const float right = (i + 1) * (static_cast<float>(window_w) / count) - 4.0f;
            const float top = window_h * strip_top + 4.0f;
            sl::rectfill(sl::screen, left, top, right, window_h - 6.0f,
                         *b.state ? sl::Colour{35, 75, 110, 220} : sl::Colour{12, 20, 40, 190});
            sl::rect(sl::screen, left, top, right, window_h - 6.0f, {90, 190, 225, 220}, 2.0f);
            sl::gprintf(static_cast<int>(left + 14.0f), static_cast<int>(top + 20.0f),
                        {225, 245, 255}, "%s", b.label);
            ++i;
        }
    }
} // namespace example_touch
#endif
