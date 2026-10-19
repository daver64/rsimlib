#pragma once

#include "atmosphere.h"
#include "renderer.h"
#include "scene3d.h"

namespace sl::detail
{
    /** The camera and size of the active 3D surface. */
    struct ActiveCameraInfo
    {
        Camera camera;
        int width = 0;
        int height = 0;
    };

    /** False when no 3D surface is being drawn into. */
    bool active_camera_info(ActiveCameraInfo &info);

    /** Fill the fog fields of a mesh draw from the current environment (no fog when `fog` is false or none is set). */
    void apply_environment(MeshDrawParams &params, bool fog);
} // namespace sl::detail
