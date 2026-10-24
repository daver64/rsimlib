# simlib_add_package(): builds a redistributable zip of an application.
#
#   include(SimlibPackage)   # provided by find_package(simlib)
#   simlib_add_package(TARGET my_app
#       NAME my_app-1.0-windows        # optional, default <target>-<system>
#       ASSETS assets data/levels.zip  # optional files/directories copied next to the executable
#       EXTRA_FILES README.txt)        # optional files placed in the zip root
#
#   cmake --build build --config Release --target my_app_package
#
# The zip holds the executable, the shared libraries it needs at run time (SDL2 DLLs and so on), simlib's
# shaders (GLSL, HLSL and the compiled Vulkan SPIR-V), the DXC DLLs for Direct3D 12 when they are known, your
# assets, and the licence files. Unzip it anywhere and run: simlib searches beside the executable for shaders.
# Set SIMLIB_PACKAGE_DXC_DIR to a directory holding dxcompiler.dll and dxil.dll to bundle them (done
# automatically when simlib was built with vcpkg's directx-dxc).
# On Linux the system SDL2 libraries are not bundled; the target machine needs them installed.

function(simlib_add_package)
    cmake_parse_arguments(ARG "" "TARGET;NAME;OUTPUT_DIR" "ASSETS;EXTRA_FILES" ${ARGN})
    if(NOT ARG_TARGET)
        message(FATAL_ERROR "simlib_add_package needs TARGET")
    endif()
    if(NOT ARG_NAME)
        set(ARG_NAME "${ARG_TARGET}-${CMAKE_SYSTEM_NAME}")
    endif()
    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR "${CMAKE_BINARY_DIR}/package")
    endif()

    set(_stage "${ARG_OUTPUT_DIR}/${ARG_NAME}")
    set(_zip "${ARG_OUTPUT_DIR}/${ARG_NAME}.zip")

    set(_assets "")
    foreach(_a IN LISTS ARG_ASSETS)
        get_filename_component(_abs "${_a}" ABSOLUTE)
        list(APPEND _assets "${_abs}")
    endforeach()
    set(_extra "")
    foreach(_a IN LISTS ARG_EXTRA_FILES)
        get_filename_component(_abs "${_a}" ABSOLUTE)
        list(APPEND _extra "${_abs}")
    endforeach()

    set(_dxc_dir "")
    if(SIMLIB_PACKAGE_DXC_DIR)
        set(_dxc_dir "${SIMLIB_PACKAGE_DXC_DIR}")
    endif()

    add_custom_target(${ARG_TARGET}_package
        COMMAND ${CMAKE_COMMAND}
            "-DSTAGE=${_stage}"
            "-DZIP=${_zip}"
            "-DEXE=$<TARGET_FILE:${ARG_TARGET}>"
            "-DDLLS=$<TARGET_RUNTIME_DLLS:${ARG_TARGET}>"
            "-DSHADER_DIRS=${SIMLIB_PACKAGE_SHADER_DIRS}"
            "-DLICENSE_DIRS=${SIMLIB_PACKAGE_LICENSE_DIRS}"
            "-DDXC_DIR=${_dxc_dir}"
            "-DASSETS=${_assets}"
            "-DEXTRA=${_extra}"
            -P "${_SIMLIB_PACKAGE_SCRIPT}"
        DEPENDS ${ARG_TARGET}
        COMMAND_EXPAND_LISTS
        VERBATIM
        COMMENT "Packaging ${ARG_NAME}.zip")
endfunction()

set(_SIMLIB_PACKAGE_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/SimlibPackageRun.cmake")
