#pragma once

#include "clutter.h"

#include <cstdint>

namespace sl
{
    /**
     * A library of low-poly scenery parts, generated in code (no asset files). Every builder returns CPU-side
     * MeshData in the same style as make_conifer_data(): flat-shaded facets with shades in the vertex colours, so
     * parts draw with a plain Material, bake with bake_instances() and cast ordinary shadows.
     *
     * Conventions, shared by every part:
     *   - Units are metres, +Y is up and the part stands on y = 0, centred on x and z.
     *   - The front of a part (house door, car bonnet, ship nose) faces +Z. Long parts (fence, wall, road,
     *     container) run along X, except roads, which run along Z.
     *   - Parameters are sizes and colours; `seed` varies colours, which windows are lit and small details. The same
     *     arguments always give the same geometry.
     *   - Windows are baked into vertex colours as dark or lit quads (no textures).
     *   - Parts are static; moving pieces such as wheels and propellers are not separate.
     */

    // Buildings.
    /** A cottage with a gable roof, chimney, door and windows. The ridge runs along X. */
    MeshData make_house_data(float width = 6.0f, float depth = 5.0f, float height = 3.0f, Colour wall = {214, 196, 160, 255},
                             Colour roof = {150, 64, 48, 255}, std::uint32_t seed = 1);
    /** A flat-roofed office block of `floors` storeys (3.5 m each) with a window grid, entrance and rooftop plant. */
    MeshData make_office_data(float width = 14.0f, float depth = 10.0f, int floors = 4, Colour wall = {170, 172, 176, 255},
                              std::uint32_t seed = 1);
    /** A stepped glass tower of `floors` storeys (3.6 m each) with setbacks and a lit antenna. */
    MeshData make_skyscraper_data(float width = 14.0f, int floors = 30, Colour glass = {66, 96, 128, 255}, std::uint32_t seed = 1);
    /** An aircraft hangar with a vaulted roof and a large dark door on the +Z front. */
    MeshData make_hangar_data(float width = 24.0f, float depth = 30.0f, float height = 9.0f, Colour wall = {170, 175, 180, 255},
                              Colour roof = {118, 128, 140, 255});

    // Structures.
    /** A stone wall of the given length along X, with capstones and slightly raised end piers so runs tile neatly. */
    MeshData make_wall_data(float length = 6.0f, float height = 2.5f, float thickness = 0.4f, Colour colour = {150, 144, 134, 255});
    /** A picket fence section along X: posts, two rails and pointed pickets. */
    MeshData make_fence_data(float length = 6.0f, float height = 1.2f, Colour colour = {176, 140, 96, 255});
    /** A water tower: four braced legs, a tank and a conical lid. */
    MeshData make_water_tower_data(float height = 16.0f, float tank_radius = 3.0f, Colour tank = {120, 140, 160, 255});
    /** A radar: a plinth, a mast and a tilted parabolic dish facing +Z with a feed horn. */
    MeshData make_radar_data(float height = 8.0f, float dish_radius = 3.0f, Colour colour = {205, 208, 212, 255});
    /** A radar dome: a plinth and a faceted white radome. */
    MeshData make_domed_radar_data(float radius = 4.0f, Colour colour = {236, 238, 240, 255});
    /** An airport control tower: a terminal block, a shaft and a flared glazed cab with a roof antenna. */
    MeshData make_control_tower_data(float height = 24.0f, std::uint32_t seed = 1);
    /** A street lamp with a pole and an arm reaching +Z; the lamp head glows underneath at (0, height, arm length). */
    MeshData make_street_lamp_data(float height = 6.0f, Colour colour = {70, 74, 80, 255});
    /** A road segment along Z with kerbs, edge lines and a dashed centre line, 0.06 m thick. */
    MeshData make_road_data(float length = 10.0f, float width = 6.0f);

    // Small props.
    MeshData make_barrel_data(float radius = 0.4f, float height = 1.0f, Colour colour = {60, 90, 150, 255});
    /** A wooden crate with corner posts and edge boards. */
    MeshData make_crate_data(float size = 1.0f, Colour colour = {168, 128, 80, 255});
    /** A shipping container, doors at +Z. Corrugated sides; `seed` shifts the paint. */
    MeshData make_container_data(float length = 6.1f, float width = 2.44f, float height = 2.6f, Colour colour = {160, 52, 42, 255},
                                 std::uint32_t seed = 1);
    /** A vertical storage tank with a domed top, ladder and pipe. */
    MeshData make_tank_data(float radius = 3.0f, float height = 6.0f, Colour colour = {196, 198, 200, 255});

    // Nature. Rocks already exist as make_rock_data() and conifers as make_conifer_data().
    /** A broadleaf tree: a short trunk and a canopy of lumpy blobs. */
    MeshData make_tree_data(float height = 6.0f, float radius = 2.0f, Colour trunk = {104, 74, 48, 255}, Colour foliage = {64, 134, 58, 255},
                            std::uint32_t seed = 1);
    MeshData make_bush_data(float radius = 0.8f, Colour colour = {58, 118, 52, 255}, std::uint32_t seed = 1);
    /** Three to five boulders of mixed size gathered together. */
    MeshData make_rock_cluster_data(float radius = 1.5f, Colour colour = {120, 118, 112, 255}, std::uint32_t seed = 1);

    // Vehicles. Their nose points along +Z.
    /** A small car with glazed cabin, wheels and lights. */
    MeshData make_car_data(float length = 4.4f, Colour body = {190, 44, 40, 255}, std::uint32_t seed = 1);
    /** A landed spaceship on three legs: a streamlined hull, canopy, swept wings, a fin and glowing engines. */
    MeshData make_spaceship_data(float length = 8.0f, Colour hull = {170, 176, 190, 255});
    /** A parked twin-engine airliner on its gear, with a window row, swept wings and a tail. */
    MeshData make_aeroplane_data(float wingspan = 12.0f, Colour body = {232, 234, 238, 255}, Colour stripe = {40, 90, 170, 255});
} // namespace sl
