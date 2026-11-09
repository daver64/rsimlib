#pragma once

#include "surface3d.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <vector>

namespace sl::detail
{
    inline glm::dvec3 planet_tile_sphere_point(double u, double v)
    {
        if (v == 0.0)
            return {0.0, 1.0, 0.0};
        if (v == 1.0)
            return {0.0, -1.0, 0.0};
        const double theta = 2.0 * glm::pi<double>() * (u == 1.0 ? 0.0 : u), phi = glm::pi<double>() * v;
        return {std::sin(phi) * std::cos(theta), std::cos(phi), -std::sin(phi) * std::sin(theta)};
    }

    inline bool adjacent_planet_tile(int column, int row, int columns, int rows, int dx, int dy,
                                     int &neighbour_column, int &neighbour_row)
    {
        neighbour_row = row + dy;
        if (neighbour_row < 0 || neighbour_row >= rows)
            return false;
        neighbour_column = (column + dx + columns) % columns;
        return true;
    }

    struct PlanetTileEdge
    {
        const std::vector<Vertex3D> *vertices = nullptr;
        glm::dvec3 centre{0.0};
        int levels_up = 0;
    };

    struct PlanetTileCorner
    {
        PlanetTileEdge neighbour;
        double x = 0.0, y = 0.0;
        const std::vector<std::uint8_t> *diagonals = nullptr;
    };

    inline bool planet_tile_flipped_diagonal(double a, double b, double c, double d,
                                              double outer_a, double outer_b, double outer_c, double outer_d)
    {
        const double bc = std::fabs(2.0 * b - c - outer_b) + std::fabs(2.0 * c - b - outer_c);
        const double ad = std::fabs(2.0 * a - d - outer_a) + std::fabs(2.0 * d - a - outer_d);
        return ad < bc;
    }

    inline std::array<std::uint32_t, 6> planet_tile_cell_indices(std::uint32_t a, std::uint32_t stride, bool flipped)
    {
        const std::uint32_t b = a + stride;
        if (flipped)
            return {a, b + 1, a + 1, b + 1, a, b};
        return {a, b, a + 1, a + 1, b, b + 1};
    }

    inline glm::dvec3 planet_tile_elevation_normal(double u, double v, double du, double dv,
                                                   double west, double east, double north, double south)
    {
        const glm::dvec3 radial = planet_tile_sphere_point(u, v);
        const double theta = 2.0 * glm::pi<double>() * u;
        const double phi = glm::pi<double>() * v;
        const double sin_phi = std::sin(phi);
        if (std::fabs(sin_phi) < 1.0e-12)
            return radial;
        const glm::dvec3 southward(std::cos(phi) * std::cos(theta), -sin_phi,
                                    -std::cos(phi) * std::sin(theta));
        const glm::dvec3 eastward(-std::sin(theta), 0.0, -std::cos(theta));
        const double slope_south = (south - north) / (2.0 * glm::pi<double>() * dv);
        const double slope_east = (east - west) / (4.0 * glm::pi<double>() * du * sin_phi);
        return glm::normalize(radial - slope_south * southward - slope_east * eastward);
    }

    // Include every coarse vertex crossed by this fine edge, even when segments is not divisible by span.
    inline double planet_tile_edge_sample(int segments, int tile_offset, std::int64_t span, int k)
    {
        const double start = static_cast<double>(std::int64_t(tile_offset) * segments) / span;
        const double end = start + static_cast<double>(segments) / span;
        double previous = start;
        int previous_index = 0;
        for (int knot = static_cast<int>(std::floor(start)) + 1; knot < end; ++knot)
        {
            const int index = static_cast<int>(std::lround((knot - start) * segments / (end - start)));
            if (k <= index)
                return previous + (knot - previous) * static_cast<double>(k - previous_index) / (index - previous_index);
            previous = knot;
            previous_index = index;
        }
        return previous + (end - previous) * static_cast<double>(k - previous_index) / (segments - previous_index);
    }

    // Match fine-edge positions to the actual straight edges of the drawn coarser mesh.
    inline void match_planet_tile_edges(std::vector<Vertex3D> &fine, const glm::dvec3 &fine_centre,
                                        int segments, int column, int row,
                                        const std::array<PlanetTileEdge, 4> &edges,
                                        const std::array<PlanetTileCorner, 4> &corners = {})
    {
        const int stride = segments + 1;
        std::vector<int> priority(static_cast<std::size_t>(stride * stride), 0);
        const auto move_vertex = [&](int i, int j, const glm::dvec3 &position, const glm::dvec3 &normal)
        {
            const std::size_t at = static_cast<std::size_t>(j * stride + i);
            const int ring = j == 0 && i < segments ? i :
                             i == segments && j < segments ? segments + j :
                             j == segments && i > 0 ? 3 * segments - i : 4 * segments - j;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float matched = static_cast<float>(position[axis] - fine_centre[axis]);
                fine[static_cast<std::size_t>(stride * stride + ring)].position[axis] += matched - fine[at].position[axis];
                fine[at].position[axis] = matched;
                fine[at].normal[axis] = static_cast<float>(normal[axis]);
                fine[static_cast<std::size_t>(stride * stride + ring)].normal[axis] = static_cast<float>(normal[axis]);
            }
        };
        for (int side = 0; side < 4; ++side)
        {
            const PlanetTileEdge &edge = edges[side];
            if (!edge.vertices || edge.levels_up <= 0)
                continue;
            const std::int64_t span = std::int64_t(1) << edge.levels_up;
            const int tile_offset = static_cast<int>((side < 2 ? row : column) % span);
            const int uv_axis = side < 2 ? 1 : 0;
            const int first = side == 0 ? 0 : side == 1 ? segments : side == 2 ? 0 : segments * stride;
            const int last = side == 0 ? segments * stride : side == 1 ? segments * stride + segments :
                             side == 2 ? segments : segments * stride + segments;
            const float uv_first = fine[static_cast<std::size_t>(first)].uv[uv_axis];
            const float uv_last = fine[static_cast<std::size_t>(last)].uv[uv_axis];
            const double start = static_cast<double>(std::int64_t(tile_offset) * segments) / span;
            for (int k = 0; k <= segments; ++k)
            {
                const int i = side == 0 ? 0 : side == 1 ? segments : k;
                const int j = side == 2 ? 0 : side == 3 ? segments : k;
                const std::size_t at = static_cast<std::size_t>(j * stride + i);
                if (priority[at] >= edge.levels_up)
                    continue;
                const double position = planet_tile_edge_sample(segments, tile_offset, span, k);
                const int a = std::min(static_cast<int>(position), segments - 1);
                const int b = a + 1;
                const double t = position - a;
                const auto coarse_index = [&](int p)
                {
                    if (side == 0)
                        return p * stride + segments;
                    if (side == 1)
                        return p * stride;
                    if (side == 2)
                        return segments * stride + p;
                    return p;
                };
                const Vertex3D &va = (*edge.vertices)[static_cast<std::size_t>(coarse_index(a))];
                const Vertex3D &vb = (*edge.vertices)[static_cast<std::size_t>(coarse_index(b))];
                glm::dvec3 matched_position = edge.centre;
                glm::dvec3 matched_normal(0.0);
                for (int axis = 0; axis < 3; ++axis)
                {
                    matched_position[axis] += (1.0 - t) * va.position[axis] + t * vb.position[axis];
                    matched_normal[axis] = (1.0 - t) * va.normal[axis] + t * vb.normal[axis];
                }
                move_vertex(i, j, matched_position, matched_normal);
                const float along = static_cast<float>((position - start) * span / segments);
                fine[at].uv[uv_axis] = uv_first + (uv_last - uv_first) * along;
                fine[at].extra[uv_axis] = along;
                const int ring = j == 0 && i < segments ? i :
                                 i == segments && j < segments ? segments + j :
                                 j == segments && i > 0 ? 3 * segments - i : 4 * segments - j;
                fine[static_cast<std::size_t>(stride * stride + ring)].uv[uv_axis] = fine[at].uv[uv_axis];
                fine[static_cast<std::size_t>(stride * stride + ring)].extra[uv_axis] = along;
                priority[at] = edge.levels_up;
            }
        }
        for (int corner = 0; corner < 4; ++corner)
        {
            const PlanetTileCorner &source = corners[corner];
            const int i = (corner & 1) ? segments : 0;
            const int j = (corner & 2) ? segments : 0;
            const std::size_t at = static_cast<std::size_t>(j * stride + i);
            if (!source.neighbour.vertices || source.neighbour.levels_up <= priority[at])
                continue;
            const double x = std::clamp(source.x, 0.0, static_cast<double>(segments));
            const double y = std::clamp(source.y, 0.0, static_cast<double>(segments));
            const int ix = std::min(static_cast<int>(x), segments - 1);
            const int iy = std::min(static_cast<int>(y), segments - 1);
            const double fx = x - ix, fy = y - iy;
            const auto &vertices = *source.neighbour.vertices;
            const Vertex3D &a = vertices[static_cast<std::size_t>(iy * stride + ix)];
            const Vertex3D &b = vertices[static_cast<std::size_t>((iy + 1) * stride + ix)];
            const Vertex3D &c = vertices[static_cast<std::size_t>(iy * stride + ix + 1)];
            const Vertex3D &d = vertices[static_cast<std::size_t>((iy + 1) * stride + ix + 1)];
            const bool flipped = source.diagonals &&
                                 (*source.diagonals)[static_cast<std::size_t>(iy * segments + ix)] != 0;
            glm::dvec3 position = source.neighbour.centre;
            glm::dvec3 normal(0.0);
            for (int axis = 0; axis < 3; ++axis)
            {
                const auto interpolate = [&](float av, float bv, float cv, float dv)
                {
                    if (flipped)
                        return fx >= fy ? (1.0 - fx) * av + (fx - fy) * cv + fy * dv
                                        : (1.0 - fy) * av + (fy - fx) * bv + fx * dv;
                    return fx + fy <= 1.0 ? (1.0 - fx - fy) * av + fy * bv + fx * cv
                                          : (1.0 - fx) * bv + (1.0 - fy) * cv + (fx + fy - 1.0) * dv;
                };
                position[axis] += interpolate(a.position[axis], b.position[axis], c.position[axis], d.position[axis]);
                normal[axis] = interpolate(a.normal[axis], b.normal[axis], c.normal[axis], d.normal[axis]);
            }
            move_vertex(i, j, position, normal);
        }
    }
}
