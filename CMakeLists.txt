cmake_minimum_required(VERSION 3.16)
project(simlib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf QUIET CONFIG)

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

add_executable(sltest
    src/main.cpp
    src/display.cpp
    src/draw.cpp
    src/graphics_fx_test.cpp
    src/font.cpp
    src/graphics_fx.cpp
    src/audio_backend.cpp
    src/audio_fx.cpp
    src/audio_music.cpp
    src/audio_test.cpp
    src/system.cpp
    src/input.cpp
    src/error.cpp
)

target_include_directories(sltest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Doxygen QUIET)
if(Doxygen_FOUND)
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}
                ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        COMMENT "Generating simlib API documentation"
        VERBATIM
    )
endif()

target_link_libraries(sltest PRIVATE OpenGL::GL PNG::PNG ZLIB::ZLIB)

if(TARGET SDL2::SDL2main)
    target_link_libraries(sltest PRIVATE SDL2::SDL2 SDL2::SDL2main)
else()
    target_link_libraries(sltest PRIVATE SDL2::SDL2)
endif()

target_link_libraries(sltest PRIVATE ${SIMLIB_TTF_TARGET})

find_package(SDL2_image QUIET CONFIG)
if(TARGET SDL2_image::SDL2_image)
    target_link_libraries(sltest PRIVATE SDL2_image::SDL2_image)
elseif(TARGET SDL2_image::SDL2_image-static)
    target_link_libraries(sltest PRIVATE SDL2_image::SDL2_image-static)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
    target_link_libraries(sltest PRIVATE PkgConfig::SDL2_IMAGE)
endif()

find_package(SDL2_mixer QUIET CONFIG)
if(TARGET SDL2_mixer::SDL2_mixer)
    target_link_libraries(sltest PRIVATE SDL2_mixer::SDL2_mixer)
elseif(TARGET SDL2_mixer::SDL2_mixer-static)
    target_link_libraries(sltest PRIVATE SDL2_mixer::SDL2_mixer-static)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)
    target_link_libraries(sltest PRIVATE PkgConfig::SDL2_MIXER)
endif()

set_target_properties(sltest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
