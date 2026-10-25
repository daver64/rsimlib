#pragma once

namespace sl::detail
{
    constexpr int default_icon_size = 64;
    // Straight-alpha RGBA, row-major: the sauronbrot icon every simlib window uses by default.
    extern const unsigned char default_icon_rgba[default_icon_size * default_icon_size * 4];
}
