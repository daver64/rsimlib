cmake_minimum_required(VERSION 3.22)

project(simlib_android_sample LANGUAGES C CXX)

get_filename_component(SIMLIB_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(SIMLIB_BUILD_EXAMPLES OFF CACHE BOOL "Build simlib examples and host tools" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build simlib tests" FORCE)
set(SIMLIB_RENDERERS gles CACHE STRING "Android renderer" FORCE)
set(SIMLIB_GUI none CACHE STRING "Disable desktop GUI integration on Android" FORCE)
set(SIMLIB_VIDEO none CACHE STRING "Disable video decoder on Android" FORCE)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_subdirectory("${SIMLIB_ROOT}" simlib)

# SDLActivity loads "SDL2" first; keep simlib and SDL in one native runtime and SDL_main in libmain.so.
add_library(simlib_android_sdl SHARED empty.cpp)
set_target_properties(simlib_android_sdl PROPERTIES
    OUTPUT_NAME SDL2
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/jniLibs/${ANDROID_ABI}")
target_link_libraries(simlib_android_sdl PRIVATE "-Wl,--whole-archive" simlib "-Wl,--no-whole-archive")

add_library(main SHARED main.cpp)
set(SIMLIB_ANDROID_JNI_LIBS_DIR "${CMAKE_BINARY_DIR}/jniLibs/${ANDROID_ABI}" CACHE PATH
    "Directory where the SDL Android app picks up its native library")
set_target_properties(main simlib_android_sdl PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${SIMLIB_ANDROID_JNI_LIBS_DIR}")
target_include_directories(main PRIVATE
    "${SIMLIB_ROOT}/include"
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
target_compile_definitions(main PRIVATE SIMLIB_RENDERER_GL SIMLIB_RENDERER_GLES)
target_link_libraries(main PRIVATE simlib_android_sdl)
