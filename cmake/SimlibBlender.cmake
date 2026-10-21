# Helpers for building models with Blender.
#
#   include(SimlibBlender)   # after find_package(simlib) or with CMAKE_MODULE_PATH pointing at this directory
#   simlib_add_blend_model(TARGET ship_model SOURCE assets/ship.blend OUTPUT ${CMAKE_BINARY_DIR}/assets/ship.glb)
#   add_dependencies(my_game ship_model)
#
# Blender is optional: when it is not installed the target is created but does nothing, and an existing
# OUTPUT file (for example a checked-in .glb) is left as it is. Set BLENDER_EXECUTABLE to pick a specific Blender.

find_program(BLENDER_EXECUTABLE NAMES blender)

# Installed next to this file; in the source tree it lives in tools/blender.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/export_glb.py")
    set(_simlib_export_default "${CMAKE_CURRENT_LIST_DIR}/export_glb.py")
else()
    set(_simlib_export_default "${CMAKE_CURRENT_LIST_DIR}/../tools/blender/export_glb.py")
endif()
set(SIMLIB_BLENDER_EXPORT_SCRIPT "${_simlib_export_default}"
    CACHE FILEPATH "Blender script that exports a .blend as glTF for simlib")

# simlib_add_blend_model(TARGET <name> SOURCE <file.blend> OUTPUT <file.glb> [NO_ANIMATION] [SELECTED_ONLY])
function(simlib_add_blend_model)
    cmake_parse_arguments(ARG "NO_ANIMATION;SELECTED_ONLY" "TARGET;SOURCE;OUTPUT" "" ${ARGN})
    if(NOT ARG_TARGET OR NOT ARG_SOURCE OR NOT ARG_OUTPUT)
        message(FATAL_ERROR "simlib_add_blend_model needs TARGET, SOURCE and OUTPUT")
    endif()
    if(NOT BLENDER_EXECUTABLE)
        message(STATUS "Blender not found; ${ARG_SOURCE} will not be exported to ${ARG_OUTPUT}")
        add_custom_target(${ARG_TARGET})
        return()
    endif()
    if(NOT EXISTS "${SIMLIB_BLENDER_EXPORT_SCRIPT}")
        message(FATAL_ERROR "SIMLIB_BLENDER_EXPORT_SCRIPT does not exist: ${SIMLIB_BLENDER_EXPORT_SCRIPT}")
    endif()
    cmake_path(ABSOLUTE_PATH ARG_SOURCE NORMALIZE OUTPUT_VARIABLE source)
    cmake_path(ABSOLUTE_PATH ARG_OUTPUT BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" NORMALIZE OUTPUT_VARIABLE output)
    set(flags)
    if(ARG_NO_ANIMATION)
        list(APPEND flags --no-anim)
    endif()
    if(ARG_SELECTED_ONLY)
        list(APPEND flags --selected)
    endif()
    cmake_path(GET output PARENT_PATH output_dir)
    add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
        COMMAND "${BLENDER_EXECUTABLE}" --background --factory-startup "${source}"
                --python "${SIMLIB_BLENDER_EXPORT_SCRIPT}" -- "${output}" ${flags}
        DEPENDS "${source}" "${SIMLIB_BLENDER_EXPORT_SCRIPT}"
        COMMENT "Exporting ${ARG_SOURCE} to glTF"
        VERBATIM)
    add_custom_target(${ARG_TARGET} ALL DEPENDS "${output}")
endfunction()
