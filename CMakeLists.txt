cmake_minimum_required(VERSION 3.16)
project(simlib LANGUAGES CXX)

include(CTest)

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
    src/resource.cpp
)

add_executable(slpack src/slpack.cpp)
target_link_libraries(slpack PRIVATE ZLIB::ZLIB)
set_target_properties(slpack PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(BUILD_TESTING)
    add_executable(resource_test
        src/resource_test.cpp
        src/resource.cpp
        src/error.cpp
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
endif()

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
