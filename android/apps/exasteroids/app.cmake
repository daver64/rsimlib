set(_asteroids_version_file "${SIMLIB_ROOT}/src/examples/exasteroids/VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_asteroids_version_file}")
file(STRINGS "${_asteroids_version_file}" _asteroids_version LIMIT_COUNT 1)

simlib_add_android_app(exasteroids "${SIMLIB_ROOT}/src/examples/exasteroids/exasteroids.cpp")
target_compile_definitions(exasteroids_main PRIVATE EXASTEROIDS_VERSION="${_asteroids_version}")
