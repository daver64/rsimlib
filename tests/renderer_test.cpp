#include "sl.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

namespace
{
    void test_model_loading()
    {
        const std::string fixture = std::string(SIMLIB_SOURCE_DIR) + "/tests/fixtures/model3d_fixture";
        sl::Model *model = sl::load_model(fixture + ".ac");
        if (!model)
            std::cerr << "AC3D fixture failed to load: " << sl::last_error() << std::endl;
        assert(model != nullptr);
        assert(model->parts.size() == 3);
        assert(model->parts.front().name == "fuselage");
        assert(model->parts.front().material.tint.red == 204);
        assert(std::fabs(model->parts.front().transform[3].x - 1.0f) < 0.001f);
        assert(std::fabs(model->parts.front().transform[3].y - 2.0f) < 0.001f);
        assert(std::fabs(model->parts.front().transform[3].z - 3.0f) < 0.001f);
        assert(model->parts[1].name == "hstab");
        assert(model->parts[1].mesh->index_count == 3);
        sl::destroy_model(model);

        model = sl::load_model(std::string(SIMLIB_SOURCE_DIR) + "/tests/fixtures/gltf_blender.glb");
        if (!model)
            std::cerr << "glTF fixture failed to load: " << sl::last_error() << std::endl;
        assert(model != nullptr);
        assert(model->parts.size() == 1);
        assert(model->parts[0].name == "Hand");
        assert(model->parts[0].material.texture != nullptr);
        assert(model->parts[0].material.cull == sl::CullMode::none);
        assert(model->parts[0].has_bounds);
        assert(std::fabs((model->parts[0].bounds_max.y - model->parts[0].bounds_min.y) - 3.0f) < 0.001f);
        const auto near_pos = [&](float x, float y, float z) {
            const glm::vec4 p = model->parts[0].transform[3];
            return std::fabs(p.x - x) < 0.01f && std::fabs(p.y - y) < 0.01f && std::fabs(p.z - z) < 0.01f;
        };
        assert(near_pos(1, 1, 0));
        assert(model->clips.size() == 3);
        const int swing = sl::find_model_clip(model, "Swing");
        const int slide = sl::find_model_clip(model, "Slide");
        const int hop = sl::find_model_clip(model, "Hop");
        assert(swing >= 0 && slide >= 0 && hop >= 0 && sl::find_model_clip(model, "nope") < 0);
        assert(std::fabs(sl::model_clip_duration(model, swing) - 1.0417f) < 0.01f);
        assert(std::fabs(sl::model_clip_duration(model, slide) - 2.0417f) < 0.01f);
        assert(std::fabs(sl::model_clip_duration(model, hop) - 1.0417f) < 0.01f);
        assert(sl::sample_model_clip(model, "Swing", 1.0417f, false));
        assert(near_pos(0, 1, -1));
        assert(sl::sample_model_clip(model, "Swing", 5.0f, false));
        assert(near_pos(0, 1, -1));
        assert(sl::sample_model_clip(model, "Hop", 0.3f, false));
        assert(near_pos(1, 1, 0));
        assert(sl::sample_model_clip(model, "Hop", 0.6f, false));
        assert(near_pos(2, 1, 0));
        assert(sl::sample_model_clip(model, "Hop", 1.1f, false));
        assert(near_pos(3, 1, 0));
        assert(sl::sample_model_clip(model, "Slide", 0.5417f, false));
        assert(near_pos(2, 1, 0));
        assert(sl::sample_model_clip(model, "Slide", 1.0417f, false));
        assert(near_pos(3, 1, 0));
        assert(sl::sample_model_clip(model, "Slide", 2.0417f + 0.5417f, true));
        assert(near_pos(2, 1, 0));
        assert(!sl::sample_model_clip(model, "nope", 0.0f));
        sl::reset_model_pose(model);
        assert(near_pos(1, 1, 0));
        assert(sl::play_model_clip(model, "Hop", false));
        sl::update_model_clip(model, 10.0f);
        assert(near_pos(3, 1, 0) && !sl::model_clip_playing(model));
        // Round trip: save to glTF with retained CPU geometry and load it back.
        sl::destroy_model(model);
        const auto resaved = std::filesystem::temp_directory_path() / "simlib-roundtrip.glb";
        sl::set_mesh_cpu_data_retained(true);
        model = sl::load_model(std::string(SIMLIB_SOURCE_DIR) + "/tests/fixtures/gltf_blender.glb");
        sl::set_mesh_cpu_data_retained(false);
        assert(model && model->parts[0].mesh->vertices.size() == 24);
        assert(sl::save_model_gltf(model, resaved.string()));
        sl::destroy_model(model);
        model = sl::load_model(resaved.string());
        assert(model != nullptr && model->parts.size() == 1 && model->parts[0].name == "Hand");
        assert(model->parts[0].mesh->vertex_count == 24 && model->parts[0].mesh->index_count == 36);
        assert(model->parts[0].material.texture != nullptr && model->parts[0].material.cull == sl::CullMode::none);
        assert(model->nodes.size() == 2 && model->clips.size() == 3);
        assert(std::fabs((model->parts[0].bounds_max.y - model->parts[0].bounds_min.y) - 3.0f) < 0.001f);
        assert(sl::sample_model_clip(model, "Swing", 1.0417f, false));
        {
            const glm::vec4 p = model->parts[0].transform[3];
            assert(std::fabs(p.x) < 0.01f && std::fabs(p.y - 1.0f) < 0.01f && std::fabs(p.z + 1.0f) < 0.01f);
        }
        sl::destroy_model(model);
        std::filesystem::remove(resaved);

        {
            // PBR: metallic/roughness factors and emission survive loading and a save/load round trip.
            const auto check = [](const sl::Model *m) {
                assert(m && m->parts.size() == 3);
                for (const sl::ModelPart &part : m->parts)
                {
                    assert(part.material.pbr);
                    if (part.name == "Metal")
                        assert(part.material.metallic == 1.0f && std::fabs(part.material.roughness - 0.2f) < 0.001f);
                    else if (part.name == "Plastic")
                        assert(part.material.metallic == 0.0f && std::fabs(part.material.roughness - 0.3f) < 0.001f);
                    else
                    {
                        assert(part.name == "Glow");
                        assert(std::fabs(part.material.emissive[1] - 2.0f) < 0.01f &&
                               std::fabs(part.material.emissive[0] - 0.4f) < 0.01f);
                    }
                }
            };
            const auto pbr_path = std::filesystem::temp_directory_path() / "simlib-pbr.glb";
            sl::set_mesh_cpu_data_retained(true);
            sl::Model *pbr = sl::load_model(std::string(SIMLIB_SOURCE_DIR) + "/tests/fixtures/gltf_pbr.glb");
            sl::set_mesh_cpu_data_retained(false);
            check(pbr);
            assert(sl::save_model_gltf(pbr, pbr_path.string()));
            sl::destroy_model(pbr);
            pbr = sl::load_model(pbr_path.string());
            check(pbr);
            sl::destroy_model(pbr);
            std::filesystem::remove(pbr_path);
        }

        {
            // Skinning: a bar with two bones; "Bend" folds the top half over by 90 degrees.
            sl::Model *skinned = sl::load_model(std::string(SIMLIB_SOURCE_DIR) + "/tests/fixtures/gltf_skin.glb");
            assert(skinned && skinned->skins.size() == 1 && skinned->skins[0].joints.size() == 2);
            assert(skinned->parts.size() == 1 && skinned->parts[0].skin_binding == 0);
            const sl::ModelSkinBinding &binding = skinned->skin_bindings[0];
            assert(binding.skinned.size() == skinned->parts[0].mesh->vertices.size() && !binding.skinned.empty());
            const auto extent = [&](int axis, bool top) {
                float value = top ? -1e9f : 1e9f;
                for (const sl::Vertex3D &v : binding.skinned)
                    value = top ? std::max(value, v.position[axis]) : std::min(value, v.position[axis]);
                return value;
            };
            assert(std::fabs(extent(1, true) - 4.0f) < 0.01f && std::fabs(skinned->parts[0].bounds_max.y - 4.0f) < 0.01f);
            assert(sl::sample_model_clip(skinned, "Bend", 1.0417f, false));
            // The top half now points sideways: height 2 plus half the bar's thickness, and 2 deep.
            assert(extent(1, true) < 2.6f);
            assert(std::fabs(extent(2, true)) > 1.9f || std::fabs(extent(2, false)) > 1.9f);
            for (const sl::Vertex3D &v : binding.skinned)
            {
                const float length = std::sqrt(v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1] + v.normal[2] * v.normal[2]);
                assert(std::fabs(length - 1.0f) < 0.01f);
            }
            sl::reset_model_pose(skinned);
            assert(std::fabs(extent(1, true) - 4.0f) < 0.01f);
            sl::destroy_model(skinned);
            // Skins survive a save and load.
            const auto skin_path = std::filesystem::temp_directory_path() / "simlib-skin.glb";
            sl::set_mesh_cpu_data_retained(true);
            skinned = sl::load_model(std::string(SIMLIB_SOURCE_DIR) + "/tests/fixtures/gltf_skin.glb");
            sl::set_mesh_cpu_data_retained(false);
            assert(skinned && sl::save_model_gltf(skinned, skin_path.string()));
            sl::destroy_model(skinned);
            skinned = sl::load_model(skin_path.string());
            assert(skinned && skinned->skins.size() == 1 && skinned->skin_bindings.size() == 1 && skinned->clips.size() == 1);
            assert(sl::sample_model_clip(skinned, "Bend", 1.0417f, false));
            {
                float top = -1e9f;
                for (const sl::Vertex3D &v : skinned->skin_bindings[0].skinned)
                    top = std::max(top, v.position[1]);
                assert(top < 2.6f);
            }
            sl::destroy_model(skinned);
            std::filesystem::remove(skin_path);
        }

        sl::set_mesh_cpu_data_retained(true);
        model = sl::load_model(fixture + ".ac");
        sl::set_mesh_cpu_data_retained(false);
        assert(model != nullptr);
        assert(sl::save_model_gltf(model, resaved.string()));
        const std::size_t ac_parts = model->parts.size();
        const sl::Colour ac_tint = model->parts.front().material.tint;
        sl::destroy_model(model);
        model = sl::load_model(resaved.string());
        assert(model != nullptr && model->parts.size() == ac_parts);
        assert(model->parts.front().name == "fuselage" && model->parts.front().material.tint.red == ac_tint.red);
        assert(std::fabs(model->parts.front().transform[3].z - 3.0f) < 0.001f);
        {
            // A mesh created without retained data cannot be saved.
            sl::Model empty;
            sl::ModelPart part;
            part.mesh = model->parts.front().mesh;
            empty.parts.push_back(part);
            assert(!sl::save_model_gltf(&empty, resaved.string()));
        }
        sl::destroy_model(model);
        std::filesystem::remove(resaved);

        model = sl::load_flightgear_model(fixture + ".xml");
        assert(model != nullptr);
        assert(model->animations.size() == 5);
        assert(model->unsupported_animations.size() == 1);
        assert(model->unsupported_animations.front() == "pick");
        assert(model->visibility.size() == 1);
        assert(model->animations.front().property == "controls/aileron");
        assert(model->animations[2].type == sl::ModelAnimationType::translation);
        assert(model->animations[2].property.empty());
        assert(std::fabs(model->animations[2].axis.x - 0.6f) < 0.001f);
        assert(std::fabs(model->animations[2].axis.z - 0.8f) < 0.001f);
        assert(model->animations[3].type == sl::ModelAnimationType::spin);
        assert(model->animations[4].type == sl::ModelAnimationType::blend);
        assert(std::fabs(model->animations[4].maximum - 0.5f) < 0.001f);
        assert(sl::set_model_property(model, "controls/aileron", 0.5f));
        assert(sl::set_model_property(model, "gear/extension", 1.0f));
        assert(sl::set_model_property(model, "/params/engine/rpm", 2000.0f));
        assert(sl::set_model_property(model, "/params/engine/crashed", 0.0f));
        assert(!sl::set_model_property(model, "not/a/property", 0.0f));
        const auto converted = std::filesystem::temp_directory_path() / "simlib-model3d-animations.json";
        assert(sl::save_model_animations_json(model, converted.string()));
        sl::destroy_model(model);

        model = sl::load_ac3d_model(fixture + ".ac");
        assert(model != nullptr);
        assert(sl::load_model_animations_json(model, converted.string()));
        assert(model->animations.size() == 5);
        assert(model->animations[2].property.empty());
        assert(model->animations[3].type == sl::ModelAnimationType::spin);
        assert(model->animations[4].type == sl::ModelAnimationType::blend);
        assert(model->visibility.size() == 1);
        assert(sl::load_model_animations_json(model, fixture + ".animations.json"));
        assert(model->animations.size() == 6);
        assert(model->animations.back().interpolation.size() == 3);
        assert(sl::set_model_property(model, "controls/aileron", -1.0f));
        sl::destroy_model(model);
        std::filesystem::remove(converted);
    }

    // Vulkan records drawing into the frame's command buffer, so submit it before reading GPU images back.
    void flush_frame()
    {
        sl::show_video_bitmap();
        sl::end_frame();
    }
}

void test_backend(sl::GraphicsBackend backend)
{
    std::cout << "Testing graphics backend: "
              << (backend == sl::GraphicsBackend::opengl ? "OpenGL" : "Vulkan") << std::endl;

    sl::set_graphics_backend(backend);
    if (!sl::graphics_backend_available(backend))
    {
        std::cout << "  Backend not available, skipping." << std::endl;
        return;
    }

    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 320, 240))
    {
        std::cerr << "  Failed to initialize graphics mode: " << sl::last_error() << std::endl;
        return;
    }

    test_model_loading();

    // 1. Test render target creation and clearing
    sl::Bitmap *rt = sl::create_render_target(128, 128);
    assert(rt != nullptr);
    assert(rt->width == 128);
    assert(rt->height == 128);

    assert(sl::begin_render_target(rt));
    sl::clear_render_target(sl::Colour{50, 100, 150, 255});

    // 2. Test filled rectangle rendering
    sl::rectfill(rt, 10, 10, 50, 50, sl::Colour{255, 0, 0, 255});

    // 3. Test text rendering with font atlas
    sl::Font *font = sl::get_default_monospace_font();
    if (font)
    {
        sl::textout(font, 10, 60, sl::Colour{255, 255, 255, 255}, "Test");
    }

    sl::end_render_target();

    // 4. Test download / pixel readback from render target
    flush_frame();
    assert(sl::download_bitmap(rt));
    assert(!rt->pixels.empty());

    // Check clear color in an un-drawn region (e.g. at 100, 100)
    sl::Colour bg_pixel = sl::getpixel(rt, 100, 100);
    assert(bg_pixel.red == 50);
    assert(bg_pixel.green == 100);
    assert(bg_pixel.blue == 150);

    // Check filled rectangle color (e.g. at 30, 30)
    sl::Colour rect_pixel = sl::getpixel(rt, 30, 30);
    assert(rect_pixel.red == 255);
    assert(rect_pixel.green == 0);
    assert(rect_pixel.blue == 0);

    // 5. Test flood fill on RAM bitmap
    sl::Bitmap *ram_bmp = sl::create_bitmap(32, 32);
    assert(ram_bmp != nullptr);
    sl::clear_to_colour(ram_bmp, sl::Colour{0, 0, 0, 255});
    sl::rect(ram_bmp, 5, 5, 25, 25, sl::Colour{255, 255, 255, 255}, 1.0f);
    sl::flood_fill(ram_bmp, 10, 10, sl::Colour{0, 255, 0, 255});
    sl::Colour filled_inner = sl::getpixel(ram_bmp, 10, 10);
    assert(filled_inner.red == 0 && filled_inner.green == 255 && filled_inner.blue == 0);
    sl::Colour outside_unfilled = sl::getpixel(ram_bmp, 2, 2);
    assert(outside_unfilled.red == 0 && outside_unfilled.green == 0 && outside_unfilled.blue == 0);
    sl::destroy_bitmap(ram_bmp);

    // 6. Test texture creation, upload, and download
    sl::Bitmap *tex_bmp = sl::create_video_bitmap(32, 32);
    assert(tex_bmp != nullptr);
    tex_bmp->anisotropy = 16.0f;
    sl::clear_to_colour(tex_bmp, sl::Colour{200, 150, 50, 255});
    assert(sl::upload_bitmap(tex_bmp));

    sl::Bitmap *downloaded = sl::create_bitmap(32, 32);
    downloaded->gpu_texture = tex_bmp->gpu_texture;
    assert(sl::download_bitmap(downloaded));
    sl::Colour tex_pixel = sl::getpixel(downloaded, 16, 16);
    assert(tex_pixel.red == 200);
    assert(tex_pixel.green == 150);
    assert(tex_pixel.blue == 50);

    sl::destroy_bitmap(downloaded);
    sl::destroy_bitmap(tex_bmp);

    // 6b. A mipmapped bitmap averages when minified; without mips this 1px checkerboard would alias.
    {
        sl::Bitmap *mip_bmp = sl::create_bitmap(64, 64);
        assert(mip_bmp != nullptr);
        mip_bmp->smooth = true;
        mip_bmp->mipmaps = true;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
                sl::putpixel(mip_bmp, x, y, ((x + y) & 1) ? sl::Colour{255, 255, 255, 255} : sl::Colour{0, 0, 0, 255});
        assert(sl::upload_bitmap(mip_bmp));

        sl::Bitmap *mip_rt = sl::create_render_target(8, 8);
        assert(mip_rt != nullptr);
        assert(sl::begin_render_target(mip_rt));
        sl::clear_render_target(sl::Colour{0, 0, 0, 255});
        sl::stretch_blit(mip_bmp, mip_rt, 0, 0, 64, 64, 0, 0, 8, 8);
        sl::end_render_target();
        flush_frame();
        assert(sl::download_bitmap(mip_rt));
        const sl::Colour averaged = sl::getpixel(mip_rt, 4, 4);
        assert(averaged.red >= 100 && averaged.red <= 156);

        sl::destroy_bitmap(mip_rt);
        sl::destroy_bitmap(mip_bmp);
    }

    // 6c. draw_sprite_batch must match the same sprites drawn one at a time, and handle tint and mirroring.
    {
        sl::Bitmap *sheet = sl::create_bitmap(16, 16);
        assert(sheet != nullptr);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                sl::putpixel(sheet, x, y, x < 8 ? sl::Colour{static_cast<Uint8>(x * 30), static_cast<Uint8>(y * 15), 40, 255}
                                                : sl::Colour{40, static_cast<Uint8>(y * 15), static_cast<Uint8>(x * 15), 255});
        assert(sl::upload_bitmap(sheet));

        auto render_to = [&](auto &&draw)
        {
            sl::Bitmap *target = sl::create_render_target(64, 64);
            assert(target != nullptr);
            assert(sl::begin_render_target(target));
            sl::clear_render_target(sl::Colour{0, 0, 0, 255});
            draw();
            sl::end_render_target();
            flush_frame();
            assert(sl::download_bitmap(target));
            return target;
        };

        std::vector<sl::SpriteInstance> sprites(3);
        sprites[0].x = 16.0f;
        sprites[0].y = 16.0f;
        sprites[0].rotation = 30.0f;
        sprites[1].x = 44.0f;
        sprites[1].y = 18.0f;
        sprites[1].scale_x = 1.5f;
        sprites[1].scale_y = 0.75f;
        sprites[1].source_x = 8;
        sprites[1].source_y = 0;
        sprites[1].source_width = 8;
        sprites[1].source_height = 16;
        sprites[2].x = 32.0f;
        sprites[2].y = 46.0f;
        sprites[2].rotation = -75.0f;
        sprites[2].scale_x = 2.0f;
        sprites[2].scale_y = 2.0f;

        sl::Bitmap *batched = render_to([&] { sl::draw_sprite_batch(sheet, sprites); });
        sl::Bitmap *single = render_to(
            [&]
            {
                for (const sl::SpriteInstance &sprite : sprites)
                {
                    const int sw = sprite.source_width > 0 ? sprite.source_width : 16;
                    const int sh = sprite.source_height > 0 ? sprite.source_height : 16;
                    const float w = sw * sprite.scale_x;
                    const float h = sh * sprite.scale_y;
                    sl::draw_sprite_region_rotated(sheet, sprite.source_x, sprite.source_y, sw, sh, sprite.x - w * 0.5f,
                                                   sprite.y - h * 0.5f, w, h, w * 0.5f, h * 0.5f, sprite.rotation);
                }
            });
        int worst = 0;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
            {
                const sl::Colour a = sl::getpixel(batched, x, y);
                const sl::Colour b = sl::getpixel(single, x, y);
                worst = std::max({worst, std::abs(a.red - b.red), std::abs(a.green - b.green), std::abs(a.blue - b.blue)});
            }
        assert(worst <= 2);
        sl::destroy_bitmap(batched);
        sl::destroy_bitmap(single);

        // Left half red-ish, right half blue-ish: a mirrored, red-tinted sprite swaps them and drops green/blue.
        std::vector<sl::SpriteInstance> mirrored(1);
        mirrored[0].x = 32.0f;
        mirrored[0].y = 32.0f;
        mirrored[0].scale_x = -2.0f;
        mirrored[0].scale_y = 2.0f;
        mirrored[0].tint = {255, 0, 0, 255};
        sl::Bitmap *mirror_result = render_to([&] { sl::draw_sprite_batch(sheet, mirrored); });
        const sl::Colour mirror_left = sl::getpixel(mirror_result, 24, 32);
        const sl::Colour mirror_right = sl::getpixel(mirror_result, 40, 32);
        assert(mirror_left.green == 0 && mirror_left.blue == 0 && mirror_right.green == 0 && mirror_right.blue == 0);
        assert(mirror_left.red <= 42 && mirror_right.red > 60);
        sl::destroy_bitmap(mirror_result);
        sl::destroy_bitmap(sheet);
    }

    // 7. Test multitexturing shader
    sl::Shader multi_shader;
    const std::string vert_source =
        "#version 430 core\n"
        "layout(location = 0) in vec2 aPos;\n"
        "layout(location = 1) in vec2 aTexCoord;\n"
        "layout(location = 2) in vec4 aColor;\n"
        "uniform mat4 uProjection;\n"
        "out vec2 uv;\n"
        "void main() {\n"
        "    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);\n"
        "    uv = aTexCoord;\n"
        "}\n";
    const std::string frag_source =
        "#version 430 core\n"
        "uniform sampler2D texA;\n"
        "uniform sampler2D texB;\n"
        "in vec2 uv;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    vec4 a = texture(texA, uv);\n"
        "    vec4 b = texture(texB, uv);\n"
        "    fragColor = vec4(a.r + b.r, a.g + b.g, a.b + b.b, 1.0);\n"
        "}\n";

    if (multi_shader.load(vert_source, frag_source, {"texA", "texB"}, {}))
    {
        sl::Bitmap *bmpA = sl::create_video_bitmap(32, 32);
        sl::Bitmap *bmpB = sl::create_video_bitmap(32, 32);
        sl::clear_to_colour(bmpA, sl::Colour{100, 0, 0, 255});
        sl::clear_to_colour(bmpB, sl::Colour{50, 120, 0, 255});
        sl::upload_bitmap(bmpA);
        sl::upload_bitmap(bmpB);

        sl::Bitmap *multi_rt = sl::create_render_target(64, 64);
        assert(sl::begin_render_target(multi_rt));
        sl::clear_render_target(sl::Colour{0, 0, 0, 255});

        assert(multi_shader.set_texture("texB", 1, bmpB));
        assert(multi_shader.draw_textured_quad(bmpA, 0, 0, 32, 32));
        sl::end_render_target();
        flush_frame();

        assert(sl::download_bitmap(multi_rt));
        sl::Colour combined = sl::getpixel(multi_rt, 16, 16);
        assert(combined.red >= 148 && combined.red <= 152);
        assert(combined.green >= 118 && combined.green <= 122);
        assert(combined.blue == 0);

        sl::destroy_bitmap(bmpA);
        sl::destroy_bitmap(bmpB);
        sl::destroy_bitmap(multi_rt);
        multi_shader.reset();
    }

    // 8. Test compute shader and StorageBuffer upload/readback
    sl::StorageBuffer storage_buf(16 * sizeof(float));
    assert(storage_buf.is_valid());
    std::vector<float> input_data(16);
    for (std::size_t i = 0; i < 16; ++i) input_data[i] = static_cast<float>(i + 1);
    assert(storage_buf.upload(input_data));

    const std::string comp_source =
        "#version 430 core\n"
        "layout(local_size_x = 16) in;\n"
        "layout(std430, binding = 0) buffer DataBuffer {\n"
        "    float values[];\n"
        "};\n"
        "void main() {\n"
        "    uint id = gl_GlobalInvocationID.x;\n"
        "    if (id < values.length()) values[id] *= 2.0;\n"
        "}\n";

    sl::Shader comp_shader;
    if (comp_shader.load_compute(comp_source))
    {
        assert(comp_shader.is_valid());
        assert(storage_buf.bind(0));
        assert(sl::dispatch_compute_for(comp_shader, 16, 1, 1, 16, 1, 1));
        sl::compute_barrier();
        flush_frame();

        std::vector<float> output_data(16, 0.0f);
        assert(storage_buf.readback(output_data));
        for (std::size_t i = 0; i < 16; ++i)
        {
            assert(output_data[i] == static_cast<float>(i + 1) * 2.0f);
        }
        comp_shader.reset();
    }
    storage_buf.destroy();

    // 8b. Hardware instancing must match the same meshes drawn one by one.
    if (sl::Surface3D *surface = sl::create_surface_3d(96, 96, 1))
    {
        sl::Mesh *cube = sl::create_cube_mesh(1.0f);
        assert(cube != nullptr);
        sl::Camera camera;
        camera.position = {0.0f, 1.5f, 6.0f};
        camera.target = {0.0f, 0.0f, 0.0f};

        std::vector<sl::InstanceData3D> instances(3);
        instances[0].model = sl::model_matrix({-1.6f, 0.0f, 0.0f}, {0.3f, 0.5f, 0.0f});
        instances[1].model = sl::model_matrix({0.0f, 0.0f, -1.0f}, {0.0f, 0.2f, 0.0f}) *
                             glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.8f, 0.7f));
        instances[2].model = sl::model_matrix({1.6f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.4f});
        instances[2].tint = {255, 128, 64, 255};

        auto render = [&](auto &&draw)
        {
            sl::Bitmap *target = sl::surface_3d_bitmap(surface);
            assert(sl::begin_surface_3d(surface, camera));
            sl::clear_surface_3d({20, 30, 40});
            sl::set_light_3d({{-0.4f, -1.0f, -0.6f}, 0.25f});
            draw();
            sl::end_surface_3d();
            flush_frame();
            assert(sl::download_bitmap(target));
            return std::vector<sl::Colour>(
                [&]
                {
                    std::vector<sl::Colour> pixels;
                    for (int y = 0; y < target->height; ++y)
                        for (int x = 0; x < target->width; ++x)
                            pixels.push_back(sl::getpixel(target, x, y));
                    return pixels;
                }());
        };
        auto matches = [](const std::vector<sl::Colour> &a, const std::vector<sl::Colour> &b)
        {
            assert(a.size() == b.size());
            for (std::size_t i = 0; i < a.size(); ++i)
                if (std::abs(a[i].red - b[i].red) > 2 || std::abs(a[i].green - b[i].green) > 2 ||
                    std::abs(a[i].blue - b[i].blue) > 2)
                    return false;
            return true;
        };

        const auto reference = render(
            [&]
            {
                for (const sl::InstanceData3D &instance : instances)
                {
                    sl::Material material;
                    material.tint = instance.tint;
                    assert(sl::draw_mesh(cube, instance.model, material));
                }
            });
        // The scene is not empty: some pixel differs from the clear colour.
        bool drew = false;
        for (const sl::Colour &pixel : reference)
            drew = drew || pixel.red != 20 || pixel.green != 30 || pixel.blue != 40;
        assert(drew);

        sl::InstanceBuffer *buffer = sl::create_instance_buffer(8);
        if (buffer)
        {
            assert(sl::update_instance_buffer(buffer, instances));
            assert(!sl::update_instance_buffer(buffer, std::vector<sl::InstanceData3D>(9)));
            assert(matches(reference, render([&] { assert(sl::draw_mesh_instanced(cube, buffer)); })));
            sl::destroy_instance_buffer(buffer);
            assert(matches(reference, render([&] { assert(sl::draw_mesh_instanced(cube, instances)); })));
            // A second transient draw in the same frame must not overwrite the first.
            const auto split = render(
                [&]
                {
                    assert(sl::draw_mesh_instanced(cube, std::vector<sl::InstanceData3D>{instances[0], instances[1]}));
                    assert(sl::draw_mesh_instanced(cube, std::vector<sl::InstanceData3D>{instances[2]}));
                });
            assert(matches(reference, split));
        }
        else
        {
            std::cout << "  Instancing not supported, skipping." << std::endl;
        }
        sl::destroy_mesh(cube);
        sl::destroy_surface_3d(surface);
    }

    // 9. Test Allegro-style do_* point-plotter callbacks, calc_spline, arc, and spline
    std::vector<std::pair<int, int>> plotted_points;
    auto record_proc = [&plotted_points](sl::Bitmap *, int x, int y, int d) {
        plotted_points.push_back({x, y});
    };

    sl::do_line(rt, 10, 10, 20, 10, 42, record_proc);
    assert(!plotted_points.empty());
    assert(plotted_points.front().first == 10 && plotted_points.front().second == 10);
    assert(plotted_points.back().first == 20 && plotted_points.back().second == 10);

    plotted_points.clear();
    sl::do_circle(rt, 30, 30, 10, 0, record_proc);
    assert(!plotted_points.empty());

    plotted_points.clear();
    sl::do_ellipse(rt, 50, 50, 15, 10, 0, record_proc);
    assert(!plotted_points.empty());

    plotted_points.clear();
    sl::do_arc(rt, 50, 50, 0.0f, 90.0f, 20.0f, 0, record_proc);
    assert(!plotted_points.empty());

    const int spline_pts[8] = { 10, 10, 20, 40, 40, 40, 50, 10 };
    int xout[10], yout[10];
    int count = sl::calc_spline(spline_pts, 10, xout, yout);
    assert(count == 10);
    assert(xout[0] == 10 && yout[0] == 10);
    assert(xout[9] == 50 && yout[9] == 10);

    plotted_points.clear();
    sl::do_spline(rt, spline_pts, 0, record_proc);
    assert(!plotted_points.empty());

    // Test arc and spline primitives
    sl::arc(rt, 60.0f, 60.0f, 0.0f, 180.0f, 15.0f, sl::Colour{255, 255, 0, 255}, 2.0f);
    sl::spline(rt, spline_pts, sl::Colour{0, 255, 255, 255}, 1.5f);

    sl::destroy_bitmap(rt);

    sl::display_shutdown();
    std::cout << "  Backend tests passed successfully!" << std::endl;
}

int main(int argc, char **argv)
{
    test_backend(sl::GraphicsBackend::opengl);
    test_backend(sl::GraphicsBackend::vulkan);

    std::cout << "All renderer integration tests passed!" << std::endl;
    return 0;
}
