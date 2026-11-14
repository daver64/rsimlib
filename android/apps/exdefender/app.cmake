set(_exdefender_version_file "${SIMLIB_ROOT}/src/examples/exdefender/VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_exdefender_version_file}")
file(STRINGS "${_exdefender_version_file}" _exdefender_version LIMIT_COUNT 1)

simlib_add_android_app(exdefender "${SIMLIB_ROOT}/src/examples/exdefender/exdefender.cpp")
target_compile_definitions(exdefender_main PRIVATE EXDEFENDER_VERSION="${_exdefender_version}")
