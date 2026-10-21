cmake_minimum_required(VERSION 3.16)
project(simlib LANGUAGES C CXX)

include(CTest)
include(GNUInstallDirs)

# Single-config generators (Makefiles, Ninja) default to an unoptimized build if
# this is left unset, which is easily mistaken for a genuine performance bug.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SIMLIB_GUI "imgui" CACHE STRING "GUI backend: imgui or none")
set_property(CACHE SIMLIB_GUI PROPERTY STRINGS imgui none)
if(NOT SIMLIB_GUI STREQUAL "imgui" AND NOT SIMLIB_GUI STREQUAL "none")
    message(FATAL_ERROR "SIMLIB_GUI must be 'imgui' or 'none' (got '${SIMLIB_GUI}')")
endif()

set(SIMLIB_VIDEO "plmpeg" CACHE STRING "Video decoder: plmpeg (MPEG1/MP2 only), ffmpeg (most formats and URLs) or none")
set_property(CACHE SIMLIB_VIDEO PROPERTY STRINGS plmpeg ffmpeg none)
if(NOT SIMLIB_VIDEO STREQUAL "plmpeg" AND NOT SIMLIB_VIDEO STREQUAL "ffmpeg" AND NOT SIMLIB_VIDEO STREQUAL "none")
    message(FATAL_ERROR "SIMLIB_VIDEO must be 'plmpeg', 'ffmpeg' or 'none' (got '${SIMLIB_VIDEO}')")
endif()

if(WIN32)
    set(_SIMLIB_DEFAULT_RENDERERS "gl;vulkan;d3d12")
else()
    set(_SIMLIB_DEFAULT_RENDERERS "gl;vulkan")
endif()
set(SIMLIB_RENDERERS "${_SIMLIB_DEFAULT_RENDERERS}" CACHE STRING
    "Semicolon-separated renderers to build: gl, vulkan, d3d12 (Windows only). At least one is required.")
set(SIMLIB_RENDER_GL OFF)
set(SIMLIB_RENDER_VULKAN OFF)
set(SIMLIB_RENDER_D3D12 OFF)
if(NOT SIMLIB_RENDERERS)
    message(FATAL_ERROR "SIMLIB_RENDERERS must list at least one of: gl, vulkan, d3d12")
endif()
foreach(_renderer IN LISTS SIMLIB_RENDERERS)
    if(_renderer STREQUAL "gl")
        set(SIMLIB_RENDER_GL ON)
    elseif(_renderer STREQUAL "vulkan")
        set(SIMLIB_RENDER_VULKAN ON)
    elseif(_renderer STREQUAL "d3d12")
        if(NOT WIN32)
            message(FATAL_ERROR "SIMLIB_RENDERERS: 'd3d12' is only available on Windows")
        endif()
        set(SIMLIB_RENDER_D3D12 ON)
    else()
        message(FATAL_ERROR "SIMLIB_RENDERERS: unknown renderer '${_renderer}' (expected gl, vulkan, d3d12)")
    endif()
endforeach()
message(STATUS "simlib renderers: ${SIMLIB_RENDERERS}")

option(SIMLIB_VALIDATE_SHADERS "Validate checked-in GLSL shaders with glslangValidator" ON)

if(SIMLIB_VALIDATE_SHADERS)
    find_program(GLSLANG_VALIDATOR glslangValidator)
    find_program(SPIRV_VAL spirv-val)
    if(GLSLANG_VALIDATOR)
        set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
        set(GLSL_SHADER_BINARY_DIR "${SHADER_BINARY_DIR}/glsl")
        set(VULKAN_SHADER_BINARY_DIR "${SHADER_BINARY_DIR}/vulkan")
        file(MAKE_DIRECTORY "${GLSL_SHADER_BINARY_DIR}" "${VULKAN_SHADER_BINARY_DIR}")
        set(SHADER_SOURCES
            default_2d.vert
            default_2d.frag
            fullscreen.vert
            bright_pass.frag
            blur.frag
            composite.frag
            vignette.frag
            colour_adjust.frag
            chromatic_aberration.frag
            pixelate.frag
            radial_blur.frag
            heat_haze.frag
            shockwave.frag
            crt.frag
            dither.frag
            film_grain.frag
            colour_adjust.frag
            tint.frag
            ex3d.vert
            ex3d.frag
            mesh3d.vert
            mesh3d_instanced.vert
            mesh3d.frag
            terrain.vert
            terrain.frag
            sky.vert
            sky.frag
            lighting.frag
            light_cull.comp
        )
        set(SHADER_BINARIES)
        foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
            get_filename_component(SHADER_NAME "${SHADER_SOURCE}" NAME)
            set(SHADER_BINARY "${GLSL_SHADER_BINARY_DIR}/${SHADER_NAME}.spv")
            get_filename_component(SHADER_EXTENSION "${SHADER_SOURCE}" EXT)
            if(SHADER_EXTENSION STREQUAL ".vert")
                set(SHADER_STAGE vert)
            elseif(SHADER_EXTENSION STREQUAL ".frag")
                set(SHADER_STAGE frag)
            elseif(SHADER_EXTENSION STREQUAL ".comp")
                set(SHADER_STAGE comp)
            endif()
            add_custom_command(
                OUTPUT "${SHADER_BINARY}"
                COMMAND "${GLSLANG_VALIDATOR}" -G --auto-map-locations --auto-map-bindings
                        -S "${SHADER_STAGE}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glsl/${SHADER_SOURCE}"
                        -o "${SHADER_BINARY}"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glsl/${SHADER_SOURCE}"
                VERBATIM
            )
            if(SPIRV_VAL)
                add_custom_command(
                    OUTPUT "${SHADER_BINARY}.validated"
                    COMMAND "${SPIRV_VAL}" "${SHADER_BINARY}"
                    COMMAND "${CMAKE_COMMAND}" -E touch "${SHADER_BINARY}.validated"
                    DEPENDS "${SHADER_BINARY}"
                    VERBATIM
                )
                list(APPEND SHADER_BINARIES "${SHADER_BINARY}.validated")
            else()
                list(APPEND SHADER_BINARIES "${SHADER_BINARY}")
            endif()
        endforeach()
        add_custom_target(shader_validation DEPENDS ${SHADER_BINARIES})
        set(VULKAN_SHADER_BINARIES)
        foreach(VULKAN_SHADER_SOURCE vulkan_2d.vert vulkan_2d.frag vulkan_fullscreen.vert vulkan_light_cull.comp vulkan_lighting.frag vulkan_vignette.frag vulkan_bright_pass.frag vulkan_blur.frag vulkan_colour_adjust.frag vulkan_chromatic_aberration.frag vulkan_pixelate.frag vulkan_radial_blur.frag vulkan_heat_haze.frag vulkan_shockwave.frag vulkan_crt.frag vulkan_dither.frag vulkan_film_grain.frag vulkan_composite.frag vulkan_ex3d.vert vulkan_ex3d.frag vulkan_mesh3d.vert vulkan_mesh3d_instanced.vert vulkan_mesh3d.frag vulkan_terrain.vert vulkan_terrain.frag vulkan_sky.vert vulkan_sky.frag)
            get_filename_component(VULKAN_SHADER_EXTENSION "${VULKAN_SHADER_SOURCE}" EXT)
            if(VULKAN_SHADER_EXTENSION STREQUAL ".vert")
                set(VULKAN_SHADER_STAGE vert)
            elseif(VULKAN_SHADER_EXTENSION STREQUAL ".comp")
                set(VULKAN_SHADER_STAGE comp)
            else()
                set(VULKAN_SHADER_STAGE frag)
            endif()
            set(VULKAN_SHADER_BINARY "${VULKAN_SHADER_BINARY_DIR}/${VULKAN_SHADER_SOURCE}.spv")
            add_custom_command(
                OUTPUT "${VULKAN_SHADER_BINARY}"
                COMMAND "${GLSLANG_VALIDATOR}" -V --auto-map-locations --auto-map-bindings
                        -S "${VULKAN_SHADER_STAGE}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vulkan/${VULKAN_SHADER_SOURCE}"
                        -o "${VULKAN_SHADER_BINARY}"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vulkan/${VULKAN_SHADER_SOURCE}"
                VERBATIM
            )
            if(SPIRV_VAL)
                add_custom_command(
                    OUTPUT "${VULKAN_SHADER_BINARY}.validated"
                    COMMAND "${SPIRV_VAL}" "${VULKAN_SHADER_BINARY}"
                    COMMAND "${CMAKE_COMMAND}" -E touch "${VULKAN_SHADER_BINARY}.validated"
                    DEPENDS "${VULKAN_SHADER_BINARY}"
                    VERBATIM
                )
                list(APPEND VULKAN_SHADER_BINARIES "${VULKAN_SHADER_BINARY}.validated")
            else()
                list(APPEND VULKAN_SHADER_BINARIES "${VULKAN_SHADER_BINARY}")
            endif()
        endforeach()
        add_custom_target(vulkan_shader_validation DEPENDS ${VULKAN_SHADER_BINARIES})
    else()
        add_custom_target(shader_validation
            COMMAND "${CMAKE_COMMAND}" -E echo
                    "glslangValidator not found; shader validation skipped"
        )
    endif()
endif()

if(SIMLIB_RENDER_GL)
    find_package(OpenGL REQUIRED)
endif()
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(SDL2 REQUIRED)
if(SIMLIB_RENDER_VULKAN)
    find_package(Vulkan REQUIRED)
endif()
find_package(SDL2_ttf QUIET CONFIG)
if(WIN32)
    find_package(GLEW REQUIRED)
endif()

include(FetchContent)
if(SIMLIB_GUI STREQUAL "imgui")
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    )
    if(SIMLIB_RENDER_GL)
        target_sources(imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
        target_link_libraries(imgui PUBLIC OpenGL::GL)
    endif()
    if(SIMLIB_RENDER_VULKAN)
        target_sources(imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
        target_link_libraries(imgui PUBLIC Vulkan::Vulkan)
    endif()
    target_include_directories(imgui PUBLIC
        $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
        $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/backends>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/backends>
    )
    target_link_libraries(imgui PUBLIC SDL2::SDL2 ${CMAKE_DL_LIBS})
    if(SIMLIB_RENDER_D3D12)
        target_sources(imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp)
        target_link_libraries(imgui PUBLIC d3d12 dxgi d3dcompiler)
    endif()
endif()

FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.7
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(lua)

file(GLOB LUA_SOURCES ${lua_SOURCE_DIR}/*.c)
list(REMOVE_ITEM LUA_SOURCES
    ${lua_SOURCE_DIR}/lua.c
    ${lua_SOURCE_DIR}/luac.c
    ${lua_SOURCE_DIR}/onelua.c
)
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC
    $<BUILD_INTERFACE:${lua_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
if(UNIX AND NOT APPLE)
    target_compile_definitions(lua PRIVATE LUA_USE_LINUX)
    target_link_libraries(lua PUBLIC m dl)
elseif(APPLE)
    target_compile_definitions(lua PRIVATE LUA_USE_MACOSX)
    target_link_libraries(lua PUBLIC m dl)
endif()

FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.3.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(sol2)

FetchContent_Declare(
    box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG v2.4.1
    GIT_SHALLOW TRUE
)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "Disable Box2D unit tests" FORCE)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "Disable Box2D testbed" FORCE)
FetchContent_MakeAvailable(box2d)
set_target_properties(box2d PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
if(NOT TARGET box2d::box2d)
    add_library(box2d::box2d ALIAS box2d)
endif()

# XML parser for FlightGear model descriptions (model_animation.cpp).
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG v1.15
    GIT_SHALLOW TRUE
)
set(PUGIXML_INSTALL OFF CACHE BOOL "Do not install pugixml" FORCE)
FetchContent_MakeAvailable(pugixml)

if(SIMLIB_VIDEO STREQUAL "plmpeg")
    # Single-header MPEG1/MP2 decoder used by video_plmpeg.cpp for cutscene-style video playback.
    FetchContent_Declare(
        pl_mpeg
        GIT_REPOSITORY https://github.com/phoboslab/pl_mpeg.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(pl_mpeg)
elseif(SIMLIB_VIDEO STREQUAL "ffmpeg")
    # FFmpeg 5.1+ (libavformat 59.27, libavcodec 59.37, libavutil 57.28, libswresample 4.7).
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavformat>=59.27 libavcodec>=59.37 libavutil>=57.28 libswscale>=6.7 libswresample>=4.7)
endif()

set(SIMLIB_TTF_TARGET "")
if(TARGET SDL2_ttf::SDL2_ttf)
    set(SIMLIB_TTF_TARGET SDL2_ttf::SDL2_ttf)
elseif(TARGET SDL2_ttf::SDL2_ttf-static)
    set(SIMLIB_TTF_TARGET SDL2_ttf::SDL2_ttf-static)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
    set(SIMLIB_TTF_TARGET PkgConfig::SDL2_TTF)
endif()

add_library(simlib STATIC
    src/engine/display.cpp
    src/engine/event.cpp
    src/engine/draw.cpp
    src/engine/surface3d.cpp
    src/engine/model3d.cpp
    src/engine/model_gltf_writer.cpp
    src/engine/ac3d.cpp
    src/engine/model_animation.cpp
    src/engine/atlas.cpp
    src/engine/tilemap.cpp
    src/engine/font.cpp
    src/engine/graphics_fx.cpp
    src/engine/graphics_fx_bloom.cpp
    src/engine/graphics_fx_blur.cpp
    src/engine/graphics_fx_vignette.cpp
    src/engine/graphics_fx_colour_adjust.cpp
    src/engine/graphics_fx_chromatic_aberration.cpp
    src/engine/graphics_fx_pixelate.cpp
    src/engine/graphics_fx_radial_blur.cpp
    src/engine/graphics_fx_heat_haze.cpp
    src/engine/graphics_fx_shockwave.cpp
    src/engine/graphics_fx_crt.cpp
    src/engine/graphics_fx_dither.cpp
    src/engine/graphics_fx_film_grain.cpp
    src/engine/graphics_fx_screen_shake.cpp
    src/engine/graphics_fx_lighting.cpp
    src/engine/gamepad.cpp
    src/engine/audio_backend.cpp
    src/engine/audio_fx.cpp
    src/engine/audio_music.cpp
    src/engine/audio_synth.cpp
    src/engine/system.cpp
    src/engine/input.cpp
    src/engine/lua_canvas.cpp
    src/engine/lua_runtime.cpp
    src/engine/lua_world.cpp
    src/engine/error.cpp
    src/engine/resource.cpp
    src/engine/config.cpp
    src/engine/noise.cpp
    src/engine/fluid.cpp
    src/engine/fluid3d.cpp
    src/engine/gui.cpp
    src/engine/shader_paths.cpp
    src/engine/gl2d.cpp
    src/engine/particles.cpp
    src/engine/physics.cpp
    src/engine/flight.cpp
    src/engine/atmosphere.cpp
    src/engine/clutter.cpp
    src/engine/clutter_layer.cpp
    src/engine/terrain.cpp
    src/engine/terrain_sources.cpp
    src/engine/renderer.cpp
    src/engine/animated_sprite.cpp
    src/engine/voxel.cpp
    src/engine/net.cpp
)
if(SIMLIB_RENDER_GL)
    target_sources(simlib PRIVATE src/engine/gl_context.cpp src/engine/gl_renderer.cpp)
    target_compile_definitions(simlib PUBLIC SIMLIB_RENDERER_GL)
endif()
if(SIMLIB_RENDER_VULKAN)
    target_sources(simlib PRIVATE src/engine/vulkan_context.cpp src/engine/vulkan_renderer.cpp)
    target_compile_definitions(simlib PUBLIC SIMLIB_RENDERER_VULKAN)
endif()
if(SIMLIB_RENDER_D3D12)
    target_compile_definitions(simlib PUBLIC SIMLIB_RENDERER_D3D12)
    target_sources(simlib PRIVATE
        src/engine/d3d12_context.cpp
        src/engine/d3d12_renderer.cpp
        src/engine/d3d12_shader_translator.cpp
    )
endif()
target_include_directories(simlib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
if(SIMLIB_VIDEO STREQUAL "plmpeg")
    target_sources(simlib PRIVATE src/engine/video_plmpeg.cpp)
    target_include_directories(simlib PRIVATE ${pl_mpeg_SOURCE_DIR})
elseif(SIMLIB_VIDEO STREQUAL "ffmpeg")
    target_sources(simlib PRIVATE src/engine/video_ffmpeg.cpp)
    target_link_libraries(simlib PUBLIC PkgConfig::FFMPEG)
else()
    target_sources(simlib PRIVATE src/engine/video_none.cpp)
endif()
target_compile_definitions(simlib PUBLIC SIMLIB_VIDEO_BACKEND_${SIMLIB_VIDEO})
if(NOT GLSLANG_VALIDATOR)
    find_program(GLSLANG_VALIDATOR glslangValidator)
endif()
if(NOT GLSLANG_VALIDATOR)
    set(GLSLANG_VALIDATOR "")
endif()
target_compile_definitions(simlib PRIVATE
    SIMLIB_INSTALL_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/simlib"
    SIMLIB_BUILD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    SIMLIB_BUILD_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    SIMLIB_GLSLANG_VALIDATOR="${GLSLANG_VALIDATOR}"
)
target_link_libraries(simlib PUBLIC PNG::PNG ZLIB::ZLIB lua sol2)
if(SIMLIB_RENDER_GL)
    target_link_libraries(simlib PUBLIC OpenGL::GL)
endif()
if(SIMLIB_GUI STREQUAL "imgui")
    target_link_libraries(simlib PUBLIC imgui)
    target_compile_definitions(simlib PUBLIC SIMLIB_GUI_IMGUI)
endif()
target_link_libraries(simlib PUBLIC box2d::box2d)
target_link_libraries(simlib PRIVATE pugixml-static)
if(SIMLIB_RENDER_VULKAN)
    target_link_libraries(simlib PRIVATE Vulkan::Vulkan)
endif()
find_package(Threads REQUIRED)
target_link_libraries(simlib PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(simlib PRIVATE ws2_32 winmm)
endif()
if(WIN32)
    target_link_libraries(simlib PRIVATE GLEW::GLEW)
endif()
if(SIMLIB_RENDER_D3D12)
    target_link_libraries(simlib PRIVATE d3d12 dxgi d3dcompiler)
    # The D3D12 backend translates GLSL to HLSL (glslang -> SPIR-V -> SPIRV-Cross) at runtime.
    # Install with: vcpkg install glslang:x64-windows spirv-cross:x64-windows
    find_package(glslang CONFIG QUIET)
    set(_sc_prefix "")
    if(DEFINED _VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_sc_prefix "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    endif()
    find_path(SPIRV_CROSS_INCLUDE_DIR spirv_cross/spirv_hlsl.hpp HINTS "${_sc_prefix}/include")
    set(_sc_found TRUE)
    set(_sc_libs "")
    foreach(_sc_name spirv-cross-hlsl spirv-cross-glsl spirv-cross-core)
        find_library(SPIRV_CROSS_${_sc_name}_RELEASE NAMES ${_sc_name} HINTS "${_sc_prefix}/lib" NO_DEFAULT_PATH)
        find_library(SPIRV_CROSS_${_sc_name}_DEBUG NAMES ${_sc_name}d HINTS "${_sc_prefix}/debug/lib" NO_DEFAULT_PATH)
        if(NOT SPIRV_CROSS_${_sc_name}_RELEASE)
            set(_sc_found FALSE)
        else()
            if(NOT SPIRV_CROSS_${_sc_name}_DEBUG)
                set(SPIRV_CROSS_${_sc_name}_DEBUG "${SPIRV_CROSS_${_sc_name}_RELEASE}")
            endif()
            list(APPEND _sc_libs optimized "${SPIRV_CROSS_${_sc_name}_RELEASE}" debug "${SPIRV_CROSS_${_sc_name}_DEBUG}")
        endif()
    endforeach()
    if(TARGET glslang::glslang AND TARGET glslang::SPIRV AND SPIRV_CROSS_INCLUDE_DIR AND _sc_found)
        target_compile_definitions(simlib PRIVATE SIMLIB_D3D12_SHADER_TRANSLATION=1)
        target_include_directories(simlib PRIVATE "${SPIRV_CROSS_INCLUDE_DIR}")
        target_link_libraries(simlib PRIVATE glslang::glslang glslang::SPIRV
                              glslang::glslang-default-resource-limits ${_sc_libs})
    else()
        message(WARNING "glslang/spirv-cross not found: D3D12 user shaders and effects will be unavailable. "
                        "Run: vcpkg install glslang:x64-windows spirv-cross:x64-windows")
    endif()
    find_package(directx-dxc CONFIG QUIET)
    find_path(DXC_INCLUDE_DIR dxcapi.h HINTS "${_sc_prefix}/include" PATH_SUFFIXES directx-dxc)
    if(DXC_INCLUDE_DIR)
        target_include_directories(simlib PRIVATE "${DXC_INCLUDE_DIR}")
        target_compile_definitions(simlib PRIVATE SIMLIB_HAVE_DXC=1)
        if(_sc_prefix)
            target_compile_definitions(simlib PRIVATE
                "SIMLIB_DXC_DIR=\"$<IF:$<CONFIG:Debug>,${_sc_prefix}/debug/bin,${_sc_prefix}/bin>\"")
        endif()
    else()
        message(WARNING "directx-dxc not found: D3D12 shaders will use shader model 5.1 (16 samplers). "
                        "Run: vcpkg install directx-dxc:x64-windows")
    endif()
endif()
if(TARGET shader_validation)
    add_dependencies(simlib shader_validation)
endif()
if(SIMLIB_RENDER_VULKAN AND TARGET vulkan_shader_validation)
    add_dependencies(simlib vulkan_shader_validation)
endif()

target_link_libraries(simlib PUBLIC SDL2::SDL2)

target_link_libraries(simlib PUBLIC ${SIMLIB_TTF_TARGET})

find_package(SDL2_image QUIET CONFIG)
if(TARGET SDL2_image::SDL2_image)
    set(SIMLIB_IMAGE_TARGET SDL2_image::SDL2_image)
    target_link_libraries(simlib PUBLIC ${SIMLIB_IMAGE_TARGET})
elseif(TARGET SDL2_image::SDL2_image-static)
    set(SIMLIB_IMAGE_TARGET SDL2_image::SDL2_image-static)
    target_link_libraries(simlib PUBLIC ${SIMLIB_IMAGE_TARGET})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
    set(SIMLIB_IMAGE_TARGET PkgConfig::SDL2_IMAGE)
    target_link_libraries(simlib PUBLIC ${SIMLIB_IMAGE_TARGET})
endif()

find_package(SDL2_mixer QUIET CONFIG)
if(TARGET SDL2_mixer::SDL2_mixer)
    set(SIMLIB_MIXER_TARGET SDL2_mixer::SDL2_mixer)
    target_link_libraries(simlib PUBLIC ${SIMLIB_MIXER_TARGET})
elseif(TARGET SDL2_mixer::SDL2_mixer-static)
    set(SIMLIB_MIXER_TARGET SDL2_mixer::SDL2_mixer-static)
    target_link_libraries(simlib PUBLIC ${SIMLIB_MIXER_TARGET})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)
    set(SIMLIB_MIXER_TARGET PkgConfig::SDL2_MIXER)
    target_link_libraries(simlib PUBLIC ${SIMLIB_MIXER_TARGET})
endif()

add_executable(exhello src/examples/exhello/exhello.cpp)
target_link_libraries(exhello PRIVATE simlib)

add_executable(exfont src/examples/exfont/exfont.cpp)
target_link_libraries(exfont PRIVATE simlib)

add_executable(extriangle src/examples/extriangle/extriangle.cpp)
target_link_libraries(extriangle PRIVATE simlib)

add_executable(exrotatesprite src/examples/exrotatesprite/exrotatesprite.cpp)
target_link_libraries(exrotatesprite PRIVATE simlib)

add_executable(exlighting src/examples/exlighting/exlighting.cpp)
target_link_libraries(exlighting PRIVATE simlib)

add_executable(exphysics src/examples/exphysics/exphysics.cpp)
target_link_libraries(exphysics PRIVATE simlib)

add_executable(showcase2d src/examples/showcase2d/showcase2d.cpp)
target_link_libraries(showcase2d PRIVATE simlib)

add_executable(showcase3d src/examples/showcase3d/showcase3d.cpp)
target_link_libraries(showcase3d PRIVATE simlib)

add_executable(showcasevoxel src/examples/showcasevoxel/showcasevoxel.cpp)
target_link_libraries(showcasevoxel PRIVATE simlib)

add_executable(exorbit src/examples/exorbit/exorbit.cpp)
target_link_libraries(exorbit PRIVATE simlib)

add_executable(exflight src/examples/exflight/exflight.cpp)
target_link_libraries(exflight PRIVATE simlib)

add_executable(exterrain src/examples/exterrain/exterrain.cpp)
target_link_libraries(exterrain PRIVATE simlib)

if(SIMLIB_RENDER_VULKAN)
    add_executable(exvulkan src/examples/exvulkan/exvulkan.cpp)
    target_link_libraries(exvulkan PRIVATE simlib)
endif()

add_executable(exlua src/examples/exlua/exlua.cpp)
target_link_libraries(exlua PRIVATE simlib lua sol2)

add_executable(exluaconsole src/examples/exluaconsole/exluaconsole.cpp)
target_link_libraries(exluaconsole PRIVATE simlib lua sol2)

add_executable(exbitmap src/examples/exbitmap/exbitmap.cpp)
target_link_libraries(exbitmap PRIVATE simlib)

add_executable(exatlas src/examples/exatlas/exatlas.cpp)
target_link_libraries(exatlas PRIVATE simlib)

add_executable(excafluid src/examples/excafluid/excafluid.cpp)
target_link_libraries(excafluid PRIVATE simlib)

add_executable(exdb src/examples/exdb/exdb.cpp)
target_include_directories(exdb PRIVATE src/examples/exdb)
target_link_libraries(exdb PRIVATE simlib SQLite::SQLite3)

add_executable(exshader src/examples/exshader/exshader.cpp)
target_link_libraries(exshader PRIVATE simlib)

add_executable(exrendertarget src/examples/exrendertarget/exrendertarget.cpp)
target_link_libraries(exrendertarget PRIVATE simlib)

add_executable(exaudio src/examples/exaudio/exaudio.cpp)
target_link_libraries(exaudio PRIVATE simlib)

add_executable(exsynth src/examples/exsynth/exsynth.cpp)
target_link_libraries(exsynth PRIVATE simlib)

add_executable(exinput src/examples/exinput/exinput.cpp)
target_link_libraries(exinput PRIVATE simlib)

if(SIMLIB_GUI STREQUAL "imgui")
    add_executable(exgui src/examples/exgui/exgui.cpp)
    target_link_libraries(exgui PRIVATE simlib imgui)
endif()

add_executable(exparticles src/examples/exparticles/exparticles.cpp)
target_link_libraries(exparticles PRIVATE simlib)
add_executable(excamera2d src/examples/excamera2d/excamera2d.cpp)
target_link_libraries(excamera2d PRIVATE simlib)
add_executable(ex2dfeatures src/examples/ex2dfeatures/ex2dfeatures.cpp)
target_link_libraries(ex2dfeatures PRIVATE simlib)
add_executable(exjoints src/examples/exjoints/exjoints.cpp)
target_link_libraries(exjoints PRIVATE simlib)

add_executable(exresources src/examples/exresources/exresources.cpp)
target_link_libraries(exresources PRIVATE simlib)

add_executable(expostprocess src/examples/expostprocess/expostprocess.cpp)
target_link_libraries(expostprocess PRIVATE simlib)

if(SIMLIB_RENDER_GL)
    add_executable(ex3d_gl src/examples/ex3d_gl/ex3d_gl.cpp)
    target_link_libraries(ex3d_gl PRIVATE simlib)
    if(WIN32)
        target_link_libraries(ex3d_gl PRIVATE GLEW::GLEW)
    endif()
endif()

add_executable(ex3d_surface src/examples/ex3d_surface/ex3d_surface.cpp)
target_link_libraries(ex3d_surface PRIVATE simlib)

add_executable(simlib_convert tools/simlib_convert.cpp)
target_link_libraries(simlib_convert PRIVATE simlib)
target_compile_definitions(simlib_convert PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
install(TARGETS simlib_convert RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(exmodel3d src/examples/exmodel3d/exmodel3d.cpp)
target_link_libraries(exmodel3d PRIVATE simlib)
target_compile_definitions(exmodel3d PRIVATE SIMLIB_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(exclouds src/examples/exclouds/exclouds.cpp)
target_link_libraries(exclouds PRIVATE simlib)

add_executable(expbr src/examples/expbr/expbr.cpp)
target_link_libraries(expbr PRIVATE simlib)

add_executable(exvoxel src/examples/exvoxel/exvoxel.cpp)
target_link_libraries(exvoxel PRIVATE simlib)

add_executable(exfluid3d src/examples/exfluid3d/exfluid3d.cpp)
target_link_libraries(exfluid3d PRIVATE simlib)

if(SIMLIB_RENDER_VULKAN)
    add_executable(ex3d_vulkan src/examples/ex3d_vulkan/ex3d_vulkan.cpp)
    target_include_directories(ex3d_vulkan PRIVATE src/engine)
    target_compile_definitions(ex3d_vulkan PRIVATE
        SIMLIB_SHADER_BINARY_DIR="${CMAKE_BINARY_DIR}/shaders"
    )
    target_link_libraries(ex3d_vulkan PRIVATE simlib)
endif()

if(SIMLIB_RENDER_D3D12)
    add_executable(ex3d_d3d12 src/examples/ex3d_d3d12/ex3d_d3d12.cpp)
    target_include_directories(ex3d_d3d12 PRIVATE src/engine)
    target_link_libraries(ex3d_d3d12 PRIVATE simlib d3d12 dxgi d3dcompiler)
elseif(NOT WIN32)
    add_executable(ex3d_d3d12 src/examples/ex3d_d3d12/ex3d_d3d12.cpp)
    target_include_directories(ex3d_d3d12 PRIVATE src/engine)
    target_link_libraries(ex3d_d3d12 PRIVATE simlib)
endif()

add_executable(client_template src/examples/client_template/client_template.cpp)
target_link_libraries(client_template PRIVATE simlib)

add_executable(slpaint src/examples/slpaint/slpaint.cpp)
target_link_libraries(slpaint PRIVATE simlib)

if(SIMLIB_GUI STREQUAL "imgui")
    add_executable(exspriteeditor src/examples/exspriteeditor/exspriteeditor.cpp)
    target_link_libraries(exspriteeditor PRIVATE simlib imgui)
endif()

if(NOT SIMLIB_VIDEO STREQUAL "none")
    add_executable(exvideo src/examples/exvideo/exvideo.cpp)
    target_link_libraries(exvideo PRIVATE simlib)
endif()

add_executable(slpack src/examples/slpack/slpack.cpp)
target_link_libraries(slpack PRIVATE ZLIB::ZLIB)

add_executable(slunpack src/examples/slunpack/slunpack.cpp)
target_link_libraries(slunpack PRIVATE simlib)

add_executable(excompute src/examples/excompute/excompute.cpp)
target_link_libraries(excompute PRIVATE simlib)

add_executable(exgpuparticles src/examples/exgpuparticles/exgpuparticles.cpp)
target_link_libraries(exgpuparticles PRIVATE simlib)

add_executable(execs src/examples/execs/execs.cpp)
target_include_directories(execs PRIVATE src/examples/execs)
target_link_libraries(execs PRIVATE simlib)

add_executable(exroguelike
    src/examples/exroguelike/exroguelike.cpp
    src/examples/exroguelike/game_types.cpp
    src/examples/exroguelike/game_logic.cpp
    src/examples/exroguelike/world_gen.cpp
    src/examples/exroguelike/render.cpp
)
target_include_directories(exroguelike PRIVATE src/examples/exroguelike)
target_link_libraries(exroguelike PRIVATE simlib)

add_executable(exisometric src/examples/exisometric/exisometric.cpp)
target_link_libraries(exisometric PRIVATE simlib)

add_executable(extilemap src/examples/extilemap/extilemap.cpp)
target_link_libraries(extilemap PRIVATE simlib)
add_executable(exnormalmap src/examples/exnormalmap/exnormalmap.cpp)
target_link_libraries(exnormalmap PRIVATE simlib)
add_executable(exnormalmap3d src/examples/exnormalmap3d/exnormalmap3d.cpp)
target_link_libraries(exnormalmap3d PRIVATE simlib)
add_executable(exmipmap src/examples/exmipmap/exmipmap.cpp)
target_link_libraries(exmipmap PRIVATE simlib)
add_executable(exinstancing src/examples/exinstancing/exinstancing.cpp)
target_link_libraries(exinstancing PRIVATE simlib)
if(SIMLIB_GUI STREQUAL "imgui")
    add_executable(exlobby src/examples/exlobby/exlobby.cpp)
    target_link_libraries(exlobby PRIVATE simlib imgui)
endif()
add_executable(expathfinding src/examples/expathfinding/expathfinding.cpp)
target_link_libraries(expathfinding PRIVATE simlib)
add_executable(exanimsprite src/examples/exanimsprite/exanimsprite.cpp)
target_link_libraries(exanimsprite PRIVATE simlib)

add_executable(exconfig src/examples/exconfig/exconfig.cpp)
target_link_libraries(exconfig PRIVATE simlib)

if(BUILD_TESTING)
    add_executable(config_test tests/config_test.cpp)
    target_link_libraries(config_test PRIVATE simlib)
    target_compile_definitions(config_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME config COMMAND config_test)

    add_executable(net_test tests/net_test.cpp)
    target_link_libraries(net_test PRIVATE simlib)
    target_compile_definitions(net_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME net COMMAND net_test)

    add_executable(lobby_test tests/lobby_test.cpp)
    target_include_directories(lobby_test PRIVATE src/examples/exlobby)
    target_link_libraries(lobby_test PRIVATE simlib)
    target_compile_definitions(lobby_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME lobby COMMAND lobby_test)

    add_executable(shader_paths_test tests/shader_paths_test.cpp)
    target_include_directories(shader_paths_test PRIVATE src/engine)
    target_link_libraries(shader_paths_test PRIVATE simlib)
    target_compile_definitions(shader_paths_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME shader_paths COMMAND shader_paths_test)

    add_executable(video_test tests/video_test.cpp)
    target_link_libraries(video_test PRIVATE simlib)
    target_compile_definitions(video_test PRIVATE
        SIMLIB_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
        $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME video COMMAND video_test)

    add_executable(pathfinder_test tests/pathfinder_test.cpp)
    target_link_libraries(pathfinder_test PRIVATE simlib)
    target_compile_definitions(pathfinder_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME pathfinder COMMAND pathfinder_test)

    add_executable(isometric_test tests/isometric_test.cpp)
    target_link_libraries(isometric_test PRIVATE simlib)
    target_compile_definitions(isometric_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME isometric COMMAND isometric_test)

    add_executable(camera_collision_test tests/camera_collision_test.cpp)
    target_link_libraries(camera_collision_test PRIVATE simlib)
    target_compile_definitions(camera_collision_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME camera_collision COMMAND camera_collision_test)

    add_executable(tilemap_test tests/tilemap_test.cpp)
    target_link_libraries(tilemap_test PRIVATE simlib)
    target_compile_definitions(tilemap_test PRIVATE SIMLIB_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME tilemap COMMAND tilemap_test)

    add_executable(normalmap_test tests/normalmap_test.cpp)
    target_link_libraries(normalmap_test PRIVATE simlib)
    target_compile_definitions(normalmap_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME normalmap COMMAND normalmap_test)
    add_executable(normalmap3d_test tests/normalmap3d_test.cpp)
    target_link_libraries(normalmap3d_test PRIVATE simlib)
    target_compile_definitions(normalmap3d_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME normalmap3d COMMAND normalmap3d_test)

    add_executable(rdb_test tests/rdb_test.cpp)
    target_include_directories(rdb_test PRIVATE src/examples/exdb)
    target_link_libraries(rdb_test PRIVATE simlib lua sol2 SQLite::SQLite3)
    target_compile_definitions(rdb_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME rdb COMMAND rdb_test)

    add_executable(backend_selection_test tests/backend_selection_test.cpp)
    target_link_libraries(backend_selection_test PRIVATE simlib)
    target_compile_definitions(backend_selection_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME backend_selection COMMAND backend_selection_test)

    add_executable(event_test tests/event_test.cpp)
    target_link_libraries(event_test PRIVATE simlib)
    target_compile_definitions(event_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME event COMMAND event_test)

    add_executable(lua_canvas_test tests/lua_canvas_test.cpp)
    target_link_libraries(lua_canvas_test PRIVATE simlib)
    target_compile_definitions(lua_canvas_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME lua_canvas COMMAND lua_canvas_test)

    add_executable(resource_test
        tests/resource_test.cpp
        src/engine/resource.cpp
        src/engine/error.cpp
    )
    target_include_directories(resource_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(resource_test PRIVATE ZLIB::ZLIB)

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resource_test_fixture)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/resource_test_fixture/stored.txt "stored entry\n")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/resource_test_fixture/deflated.txt "deflated entry with enough repeated text to exercise compression\n")
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resource_test_fixture.zip
        COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_CURRENT_BINARY_DIR}/resource_test_fixture.zip
                --format=zip stored.txt deflated.txt
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resource_test_fixture
        DEPENDS
            ${CMAKE_CURRENT_BINARY_DIR}/resource_test_fixture/stored.txt
            ${CMAKE_CURRENT_BINARY_DIR}/resource_test_fixture/deflated.txt
        VERBATIM
    )
    add_custom_target(resource_test_fixture DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/resource_test_fixture.zip)
    add_dependencies(resource_test resource_test_fixture)
    add_test(NAME resource_archive COMMAND resource_test ${CMAKE_CURRENT_BINARY_DIR}/resource_test_fixture.zip)

    add_executable(physics_test tests/physics_test.cpp)
    target_link_libraries(physics_test PRIVATE simlib)
    target_compile_definitions(physics_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME physics COMMAND physics_test)

    add_executable(flight_test tests/flight_test.cpp)
    target_link_libraries(flight_test PRIVATE simlib)
    target_compile_definitions(flight_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME flight COMMAND flight_test)

    add_executable(terrain_test tests/terrain_test.cpp)
    target_link_libraries(terrain_test PRIVATE simlib)
    target_compile_definitions(terrain_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME terrain COMMAND terrain_test)

    add_executable(world_flavour_test tests/world_flavour_test.cpp)
    target_link_libraries(world_flavour_test PRIVATE simlib)
    target_compile_definitions(world_flavour_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME world_flavour COMMAND world_flavour_test)

    add_executable(renderer_test tests/renderer_test.cpp)
    target_link_libraries(renderer_test PRIVATE simlib)
    target_compile_definitions(renderer_test PRIVATE SIMLIB_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME renderer COMMAND renderer_test)
    add_test(NAME convert_ac3d_to_glb
        COMMAND simlib_convert ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/model3d_fixture.ac
                ${CMAKE_CURRENT_BINARY_DIR}/convert_test_ac3d.glb)
    include(cmake/SimlibBlender.cmake)
    if(BLENDER_EXECUTABLE)
        # Exports Blender's default scene with tools/blender/export_glb.py, then loads and re-saves it through simlib.
        add_test(NAME blender_export
            COMMAND ${BLENDER_EXECUTABLE} --background --factory-startup --python
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools/blender/export_glb.py -- ${CMAKE_CURRENT_BINARY_DIR}/blender_default.glb)
        add_test(NAME blender_export_loads
            COMMAND simlib_convert ${CMAKE_CURRENT_BINARY_DIR}/blender_default.glb ${CMAKE_CURRENT_BINARY_DIR}/blender_default_resaved.glb)
        set_tests_properties(blender_export PROPERTIES FIXTURES_SETUP blender_default_glb)
        set_tests_properties(blender_export_loads PROPERTIES FIXTURES_REQUIRED blender_default_glb)
    endif()

    add_executable(voxel_test tests/voxel_test.cpp)
    target_link_libraries(voxel_test PRIVATE simlib)
    target_compile_definitions(voxel_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME voxel COMMAND voxel_test)

    add_executable(fluid3d_test tests/fluid3d_test.cpp)
    target_link_libraries(fluid3d_test PRIVATE simlib)
    target_compile_definitions(fluid3d_test PRIVATE $<$<PLATFORM_ID:Windows>:SDL_MAIN_HANDLED>)
    add_test(NAME fluid3d COMMAND fluid3d_test)

    if(SIMLIB_RENDER_D3D12)
        add_executable(d3d12_renderer_test tests/d3d12_renderer_test.cpp)
        target_link_libraries(d3d12_renderer_test PRIVATE simlib)
        target_compile_definitions(d3d12_renderer_test PRIVATE SDL_MAIN_HANDLED)
        add_test(NAME d3d12_renderer COMMAND d3d12_renderer_test)
    endif()

    # Tests rely on assert(); keep it active in Release builds, where NDEBUG would turn every check into a no-op.
    foreach(SIMLIB_TEST_TARGET
            config_test isometric_test camera_collision_test tilemap_test normalmap_test normalmap3d_test rdb_test
            backend_selection_test event_test lua_canvas_test resource_test physics_test
            renderer_test voxel_test fluid3d_test flight_test terrain_test world_flavour_test net_test lobby_test pathfinder_test d3d12_renderer_test)
        if(TARGET ${SIMLIB_TEST_TARGET})
            if(MSVC)
                # A force-included #undef avoids MSVC's D9025 warning that /UNDEBUG would trigger.
                target_compile_options(${SIMLIB_TEST_TARGET} PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/cmake/force_assert.h")
            else()
                target_compile_options(${SIMLIB_TEST_TARGET} PRIVATE -UNDEBUG)
            endif()
        endif()
    endforeach()
endif()

if(WIN32 AND TARGET SDL2::SDL2main)
    get_property(SIMLIB_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(SIMLIB_TARGET IN LISTS SIMLIB_TARGETS)
        get_target_property(SIMLIB_TARGET_TYPE ${SIMLIB_TARGET} TYPE)
        if(SIMLIB_TARGET_TYPE STREQUAL "EXECUTABLE")
            get_target_property(SIMLIB_TARGET_SOURCES ${SIMLIB_TARGET} SOURCES)
            foreach(SIMLIB_SOURCE IN LISTS SIMLIB_TARGET_SOURCES)
                if(SIMLIB_SOURCE MATCHES "src/examples/")
                    target_link_libraries(${SIMLIB_TARGET} PRIVATE SDL2::SDL2main)
                    break()
                endif()
            endforeach()
        endif()
    endforeach()
endif()

find_package(Doxygen QUIET)
if(Doxygen_FOUND)
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}
                ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        COMMENT "Generating simlib API documentation"
        VERBATIM
    )
    install(CODE "
        execute_process(
            COMMAND \"${DOXYGEN_EXECUTABLE}\" \"${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile\"
            WORKING_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}\"
            RESULT_VARIABLE simlib_doxygen_result
        )
        if(NOT simlib_doxygen_result EQUAL 0)
            message(FATAL_ERROR \"Doxygen failed while generating simlib man pages\")
        endif()
    ")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/man/man3/"
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
        OPTIONAL
        FILES_MATCHING PATTERN "*.3"
    )
endif()
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/man/simlib.3"
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
)

set(SIMLIB_INSTALL_TARGETS simlib lua pugixml-static)
if(SIMLIB_GUI STREQUAL "imgui")
    list(APPEND SIMLIB_INSTALL_TARGETS imgui)
endif()
install(TARGETS ${SIMLIB_INSTALL_TARGETS}
    EXPORT simlibTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT simlibTargets
    FILE simlibTargets.cmake
    NAMESPACE simlib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/simlib
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(SIMLIB_GUI STREQUAL "imgui")
    file(GLOB IMGUI_PUBLIC_HEADERS "${imgui_SOURCE_DIR}/*.h")
    file(GLOB IMGUI_BACKEND_HEADERS "${imgui_SOURCE_DIR}/backends/*.h")
    install(FILES ${IMGUI_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES ${IMGUI_BACKEND_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/backends)
endif()
install(FILES
    "${lua_SOURCE_DIR}/lua.h"
    "${lua_SOURCE_DIR}/luaconf.h"
    "${lua_SOURCE_DIR}/lauxlib.h"
    "${lua_SOURCE_DIR}/lualib.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY "${box2d_SOURCE_DIR}/include/box2d"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/simlib
    RENAME simlib-LICENSE
)
if(SIMLIB_GUI STREQUAL "imgui")
    install(FILES "${imgui_SOURCE_DIR}/LICENSE.txt"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/simlib
        RENAME imgui-LICENSE.txt
    )
endif()
install(FILES "${sol2_SOURCE_DIR}/LICENSE.txt"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/simlib
    RENAME sol2-LICENSE.txt
)
install(FILES "${box2d_SOURCE_DIR}/LICENSE"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/simlib
    RENAME box2d-LICENSE
)
install(FILES "${pugixml_SOURCE_DIR}/LICENSE.md"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/simlib
    RENAME pugixml-LICENSE.md
)
install(DIRECTORY shaders/glsl/ DESTINATION ${CMAKE_INSTALL_DATADIR}/simlib/shaders/glsl)
install(DIRECTORY shaders/d3d/ DESTINATION ${CMAKE_INSTALL_DATADIR}/simlib/shaders/d3d)
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders/"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/simlib/shaders
    OPTIONAL
    FILES_MATCHING PATTERN "*.spv"
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/simlibConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/simlibConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/simlib
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/simlibConfig.cmake"
    cmake/SimlibBlender.cmake
    tools/blender/export_glb.py
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/simlib
)
