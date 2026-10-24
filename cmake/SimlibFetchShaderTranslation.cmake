# Builds glslang and SPIRV-Cross from source for the Direct3D 12 GLSL-to-HLSL translator.
# Used when they are not installed (for example through vcpkg). Defines the targets
# simlib_glslang, simlib_spirv_cross and the variable SIMLIB_TRANSLATION_FETCHED_INCLUDES.
# Set SIMLIB_FETCH_GLSLANG_BINARIES to ON before including to also build the glslang-standalone
# compiler (needs Python 3 at build time); it is used to generate the checked-in shaders' SPIR-V.
# Safe to include more than once.
include_guard(GLOBAL)
include(FetchContent)

set(_saved_shared "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF)

if(NOT SIMLIB_FETCH_GLSLANG_BINARIES)
    set(SIMLIB_FETCH_GLSLANG_BINARIES OFF)
endif()
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES ${SIMLIB_FETCH_GLSLANG_BINARIES} CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG 15.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glslang)

set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    spirv_cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
    GIT_TAG vulkan-sdk-1.4.357.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spirv_cross)

set(BUILD_SHARED_LIBS "${_saved_shared}")

add_library(simlib_glslang INTERFACE)
target_link_libraries(simlib_glslang INTERFACE glslang SPIRV glslang-default-resource-limits)
add_library(simlib_spirv_cross INTERFACE)
target_link_libraries(simlib_spirv_cross INTERFACE spirv-cross-hlsl spirv-cross-glsl spirv-cross-core)

# The source trees keep headers at their roots; the translator accepts both layouts.
set(SIMLIB_TRANSLATION_FETCHED_INCLUDES "${glslang_SOURCE_DIR}" "${spirv_cross_SOURCE_DIR}")
