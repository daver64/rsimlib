cmake_minimum_required(VERSION 3.16)
project(simlib LANGUAGES C CXX)

include(CTest)

# Single-config generators (Makefiles, Ninja) default to an unoptimized build if
# this is left unset, which is easily mistaken for a genuine performance bug.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
        foreach(VULKAN_SHADER_SOURCE vulkan_2d.vert vulkan_2d.frag vulkan_fullscreen.vert vulkan_light_cull.comp vulkan_lighting.frag vulkan_vignette.frag vulkan_bright_pass.frag vulkan_blur.frag vulkan_colour_adjust.frag vulkan_chromatic_aberration.frag vulkan_pixelate.frag vulkan_radial_blur.frag vulkan_heat_haze.frag vulkan_shockwave.frag vulkan_crt.frag vulkan_dither.frag vulkan_film_grain.frag vulkan_composite.frag vulkan_ex3d.vert vulkan_ex3d.frag)
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

find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(SDL2_ttf QUIET CONFIG)

include(FetchContent)
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
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL Vulkan::Vulkan ${CMAKE_DL_LIBS})

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
)
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC ${lua_SOURCE_DIR})
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
    src/engine/atlas.cpp
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
    src/engine/system.cpp
    src/engine/input.cpp
    src/engine/lua_canvas.cpp
    src/engine/lua_runtime.cpp
    src/engine/error.cpp
    src/engine/resource.cpp
    src/engine/config.cpp
    src/engine/noise.cpp
    src/engine/fluid.cpp
    src/engine/gui.cpp
    src/engine/gl2d.cpp
    src/engine/particles.cpp
    src/engine/physics.cpp
    src/engine/renderer.cpp
    src/engine/gl_context.cpp
    src/engine/gl_renderer.cpp
    src/engine/vulkan_context.cpp
    src/engine/vulkan_renderer.cpp
)
if(WIN32)
    target_sources(simlib PRIVATE
        src/engine/d3d11_context.cpp
        src/engine/d3d11_renderer.cpp
        src/engine/d3d12_context.cpp
        src/engine/d3d12_renderer.cpp
    )
endif()
target_include_directories(simlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(NOT GLSLANG_VALIDATOR)
    find_program(GLSLANG_VALIDATOR glslangValidator)
endif()
if(NOT GLSLANG_VALIDATOR)
    set(GLSLANG_VALIDATOR "")
endif()
target_compile_definitions(simlib PRIVATE
    SIMLIB_GLSL_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/glsl"
    SIMLIB_D3D_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/d3d"
    SIMLIB_SHADER_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders"
    SIMLIB_GLSLANG_VALIDATOR="${GLSLANG_VALIDATOR}"
)
target_link_libraries(simlib PUBLIC OpenGL::GL PNG::PNG ZLIB::ZLIB imgui lua sol2)
target_link_libraries(simlib PUBLIC box2d)
target_link_libraries(simlib PRIVATE Vulkan::Vulkan)
if(WIN32)
    target_link_libraries(simlib PRIVATE d3d11 d3d12 dxgi d3dcompiler)
endif()
if(TARGET shader_validation)
    add_dependencies(simlib shader_validation)
endif()
if(TARGET vulkan_shader_validation)
    add_dependencies(simlib vulkan_shader_validation)
endif()

if(TARGET SDL2::SDL2main)
    target_link_libraries(simlib PUBLIC SDL2::SDL2 SDL2::SDL2main)
else()
    target_link_libraries(simlib PUBLIC SDL2::SDL2)
endif()

target_link_libraries(simlib PUBLIC ${SIMLIB_TTF_TARGET})

find_package(SDL2_image QUIET CONFIG)
if(TARGET SDL2_image::SDL2_image)
    target_link_libraries(simlib PUBLIC SDL2_image::SDL2_image)
elseif(TARGET SDL2_image::SDL2_image-static)
    target_link_libraries(simlib PUBLIC SDL2_image::SDL2_image-static)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
    target_link_libraries(simlib PUBLIC PkgConfig::SDL2_IMAGE)
endif()

find_package(SDL2_mixer QUIET CONFIG)
if(TARGET SDL2_mixer::SDL2_mixer)
    target_link_libraries(simlib PUBLIC SDL2_mixer::SDL2_mixer)
elseif(TARGET SDL2_mixer::SDL2_mixer-static)
    target_link_libraries(simlib PUBLIC SDL2_mixer::SDL2_mixer-static)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)
    target_link_libraries(simlib PUBLIC PkgConfig::SDL2_MIXER)
endif()

add_executable(sltest
    src/examples/sltest/main.cpp
    src/examples/sltest/game.cpp
    src/examples/sltest/game_menu.cpp
    src/examples/sltest/game_playing.cpp
    src/examples/sltest/entity.cpp
    src/examples/sltest/game_paused.cpp
    src/examples/sltest/game_help.cpp
    src/examples/sltest/game_gameover.cpp
    src/examples/sltest/game_settings.cpp
    src/examples/sltest/game_lua.cpp
)
target_link_libraries(sltest PRIVATE simlib lua sol2)

add_executable(exhello src/examples/exhello/exhello.cpp)
target_link_libraries(exhello PRIVATE simlib)
set_target_properties(exhello PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exfont src/examples/exfont/exfont.cpp)
target_link_libraries(exfont PRIVATE simlib)
set_target_properties(exfont PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)


add_executable(extriangle src/examples/extriangle/extriangle.cpp)
target_link_libraries(extriangle PRIVATE simlib)
set_target_properties(extriangle PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exrotatesprite src/examples/exrotatesprite/exrotatesprite.cpp)
target_link_libraries(exrotatesprite PRIVATE simlib)
set_target_properties(exrotatesprite PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exlighting src/examples/exlighting/exlighting.cpp)
target_link_libraries(exlighting PRIVATE simlib)
set_target_properties(exlighting PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exphysics src/examples/exphysics/exphysics.cpp)
target_link_libraries(exphysics PRIVATE simlib)
set_target_properties(exphysics PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exvulkan src/examples/exvulkan/exvulkan.cpp)
target_link_libraries(exvulkan PRIVATE simlib)
set_target_properties(exvulkan PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exlua src/examples/exlua/exlua.cpp)
target_link_libraries(exlua PRIVATE simlib lua sol2)
set_target_properties(exlua PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exluaconsole src/examples/exluaconsole/exluaconsole.cpp)
target_link_libraries(exluaconsole PRIVATE simlib lua sol2)
set_target_properties(exluaconsole PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exbitmap src/examples/exbitmap/exbitmap.cpp)
target_link_libraries(exbitmap PRIVATE simlib)
set_target_properties(exbitmap PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exatlas src/examples/exatlas/exatlas.cpp)
target_link_libraries(exatlas PRIVATE simlib)
set_target_properties(exatlas PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(excafluid src/examples/excafluid/excafluid.cpp)
target_link_libraries(excafluid PRIVATE simlib)
set_target_properties(excafluid PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exdb src/examples/exdb/exdb.cpp)
target_link_libraries(exdb PRIVATE simlib SQLite::SQLite3)
set_target_properties(exdb PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exshader src/examples/exshader/exshader.cpp)
target_link_libraries(exshader PRIVATE simlib)
set_target_properties(exshader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exrendertarget src/examples/exrendertarget/exrendertarget.cpp)
target_link_libraries(exrendertarget PRIVATE simlib)
set_target_properties(exrendertarget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exaudio src/examples/exaudio/exaudio.cpp)
target_link_libraries(exaudio PRIVATE simlib)
set_target_properties(exaudio PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exinput src/examples/exinput/exinput.cpp)
target_link_libraries(exinput PRIVATE simlib)
set_target_properties(exinput PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exgui src/examples/exgui/exgui.cpp)
target_link_libraries(exgui PRIVATE simlib imgui)
set_target_properties(exgui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exparticles src/examples/exparticles/exparticles.cpp)
target_link_libraries(exparticles PRIVATE simlib)
set_target_properties(exparticles PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(exresources src/examples/exresources/exresources.cpp)
target_link_libraries(exresources PRIVATE simlib)
set_target_properties(exresources PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(expostprocess src/examples/expostprocess/expostprocess.cpp)
target_link_libraries(expostprocess PRIVATE simlib)
set_target_properties(expostprocess PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(ex3d src/examples/ex3d/ex3d.cpp)
target_link_libraries(ex3d PRIVATE simlib)
set_target_properties(ex3d PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(ex3d_vulkan src/examples/ex3d_vulkan/ex3d_vulkan.cpp)
target_include_directories(ex3d_vulkan PRIVATE src/engine)
target_compile_definitions(ex3d_vulkan PRIVATE
    SIMLIB_SHADER_BINARY_DIR="${CMAKE_BINARY_DIR}/shaders"
)
target_link_libraries(ex3d_vulkan PRIVATE simlib)
set_target_properties(ex3d_vulkan PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)


add_executable(client_template src/examples/client_template/client_template.cpp)
target_link_libraries(client_template PRIVATE simlib)
set_target_properties(client_template PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)



add_executable(slpack src/examples/slpack/slpack.cpp)
target_link_libraries(slpack PRIVATE ZLIB::ZLIB)
set_target_properties(slpack PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(slunpack src/examples/slunpack/slunpack.cpp)
target_link_libraries(slunpack PRIVATE simlib)
set_target_properties(slunpack PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(BUILD_TESTING)
    add_executable(rdb_test tests/rdb_test.cpp)
    target_link_libraries(rdb_test PRIVATE simlib SQLite::SQLite3)
    add_test(NAME rdb COMMAND rdb_test)

    add_executable(backend_selection_test tests/backend_selection_test.cpp)
    target_link_libraries(backend_selection_test PRIVATE simlib)
    add_test(NAME backend_selection COMMAND backend_selection_test)

    add_executable(event_test tests/event_test.cpp)
    target_link_libraries(event_test PRIVATE simlib)
    add_test(NAME event COMMAND event_test)

    add_executable(lua_canvas_test tests/lua_canvas_test.cpp)
    target_link_libraries(lua_canvas_test PRIVATE simlib)
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
    add_test(NAME physics COMMAND physics_test)
endif()

find_package(Doxygen QUIET)
if(Doxygen_FOUND)
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}
                ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        COMMENT "Generating simlib API documentation"
        VERBATIM
    )
endif()

set_target_properties(sltest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
