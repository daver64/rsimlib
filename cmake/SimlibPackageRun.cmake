# Script run by the <target>_package target created by simlib_add_package().
file(REMOVE_RECURSE "${STAGE}")
file(REMOVE "${ZIP}")
file(MAKE_DIRECTORY "${STAGE}")

file(COPY "${EXE}" DESTINATION "${STAGE}")

foreach(_dll IN LISTS DLLS)
    if(EXISTS "${_dll}")
        file(COPY "${_dll}" DESTINATION "${STAGE}")
    endif()
endforeach()

foreach(_dir IN LISTS SHADER_DIRS)
    if(IS_DIRECTORY "${_dir}")
        file(COPY "${_dir}/" DESTINATION "${STAGE}/shaders")
    endif()
endforeach()

if(DXC_DIR)
    foreach(_name dxcompiler.dll dxil.dll)
        if(EXISTS "${DXC_DIR}/${_name}")
            file(COPY "${DXC_DIR}/${_name}" DESTINATION "${STAGE}")
        endif()
    endforeach()
endif()

foreach(_dir IN LISTS LICENSE_DIRS)
    if(IS_DIRECTORY "${_dir}")
        file(COPY "${_dir}/" DESTINATION "${STAGE}/licenses")
    endif()
endforeach()

foreach(_asset IN LISTS ASSETS)
    if(IS_DIRECTORY "${_asset}")
        get_filename_component(_n "${_asset}" NAME)
        file(COPY "${_asset}" DESTINATION "${STAGE}")
    elseif(EXISTS "${_asset}")
        file(COPY "${_asset}" DESTINATION "${STAGE}")
    else()
        message(WARNING "Asset not found, skipped: ${_asset}")
    endif()
endforeach()

foreach(_f IN LISTS EXTRA)
    if(EXISTS "${_f}")
        file(COPY "${_f}" DESTINATION "${STAGE}")
    endif()
endforeach()

get_filename_component(_parent "${STAGE}" DIRECTORY)
get_filename_component(_folder "${STAGE}" NAME)
execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar cf "${ZIP}" --format=zip "${_folder}"
    WORKING_DIRECTORY "${_parent}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Unable to create ${ZIP}")
endif()
message(STATUS "Created ${ZIP}")
