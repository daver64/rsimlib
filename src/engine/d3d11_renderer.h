#pragma once

#ifdef _WIN32

#include "renderer.h"

#include <memory>

namespace sl::detail
{
    std::unique_ptr<Renderer> create_d3d11_renderer();
}

#endif // _WIN32
