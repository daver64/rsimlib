set(work "${SIMLIB_BUILD_DIR}/package-sol2-test")
set(prefix "${work}/prefix")
file(MAKE_DIRECTORY "${work}/consumer/child")

execute_process(COMMAND "${CMAKE_COMMAND}" --install "${SIMLIB_BUILD_DIR}"
                        --prefix "${prefix}" --config "${SIMLIB_CONFIG}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Test installation failed:\n${output}\n${error}")
endif()

file(WRITE "${work}/consumer/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.16)
project(simlib_package_test LANGUAGES CXX)
find_package(simlib CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE simlib::simlib sol2 sol2::sol2)
add_subdirectory(child)
]=])
file(WRITE "${work}/consumer/main.cpp" "int main() { return 0; }\n")
file(WRITE "${work}/consumer/child/CMakeLists.txt" [=[
add_executable(child_consumer ../main.cpp)
target_link_libraries(child_consumer PRIVATE simlib::simlib sol2 sol2::sol2)
]=])

foreach(target sol2 sol2::sol2)
    if(target STREQUAL "sol2")
        set(name unnamespaced)
    else()
        set(name namespaced)
    endif()
    file(MAKE_DIRECTORY "${work}/${name}/sol2")
    file(WRITE "${work}/${name}/sol2/sol2-config.cmake"
         "add_library(${target} INTERFACE IMPORTED)\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${work}/consumer" -B "${work}/${name}/build"
                -G "${SIMLIB_GENERATOR}" "-DCMAKE_CXX_COMPILER=${SIMLIB_CXX_COMPILER}"
                "-DCMAKE_PREFIX_PATH=${prefix};${SIMLIB_PREFIX_PATH}"
                "-Dsol2_DIR=${work}/${name}/sol2"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${name} sol2 consumer configure failed:\n${output}\n${error}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${work}/${name}/build" --config "${SIMLIB_CONFIG}" -j2
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${name} sol2 consumer build failed:\n${output}\n${error}")
    endif()
endforeach()
