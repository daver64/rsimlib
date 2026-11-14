set(_exinvaders_version_file "${SIMLIB_ROOT}/src/examples/exinvaders/VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_exinvaders_version_file}")
file(STRINGS "${_exinvaders_version_file}" _exinvaders_version LIMIT_COUNT 1)

simlib_add_android_app(exinvaders "${SIMLIB_ROOT}/src/examples/exinvaders/exinvaders.cpp")
target_compile_definitions(exinvaders_main PRIVATE EXINVADERS_VERSION="${_exinvaders_version}")
