@echo off
rem Builds the simlib SDK with Microsoft Visual Studio (MSVC) and zips it as simlib-sdk-<VERSION>-Windows-MSVC.zip.
rem Run it from a "x64 Native Tools Command Prompt for VS" (or any prompt after vcvars64.bat) on Windows.
rem
rem Needs: Visual Studio 2022 (C++ workload), CMake, Git, and vcpkg. Set VCPKG_ROOT to your vcpkg checkout; if it is
rem unset, vcpkg is cloned to %USERPROFILE%\vcpkg. Install the Vulkan SDK (sets VULKAN_SDK) to include the Vulkan
rem renderer; without it only OpenGL and Direct3D 12 are built.
rem
rem   tools\build-sdk-windows.bat [output-dir]      output-dir defaults to dist\ under the repository
setlocal enableextensions
cd /d "%~dp0\.."

set "OUT=%~1"
if "%OUT%"=="" set "OUT=%CD%\dist"
if not exist "%OUT%" mkdir "%OUT%"
for %%I in ("%OUT%") do set "OUT=%%~fI"

set /p SIMVER=<VERSION
for /f "tokens=* delims= " %%a in ("%SIMVER%") do set "SIMVER=%%a"
set "NAME=simlib-sdk-%SIMVER%-Windows-MSVC"

where cl >nul 2>nul || (echo cl.exe not found: run this from a Visual Studio x64 Native Tools prompt.& exit /b 1)
where cmake >nul 2>nul || (echo cmake not found.& exit /b 1)
where git >nul 2>nul || (echo git not found.& exit /b 1)

if "%VCPKG_ROOT%"=="" set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    if not exist "%VCPKG_ROOT%\bootstrap-vcpkg.bat" git clone https://github.com/microsoft/vcpkg "%VCPKG_ROOT%" || exit /b 1
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics || exit /b 1
)

set "RENDERERS=gl;d3d12"
if defined VULKAN_SDK set "RENDERERS=gl;vulkan;d3d12"

echo == Installing dependencies with vcpkg ==
"%VCPKG_ROOT%\vcpkg.exe" install --triplet x64-windows ^
    sdl2 sdl2-image sdl2-mixer sdl2-ttf libpng zlib sqlite3 glew ^
    glslang spirv-cross directx-dxc || exit /b 1

echo == Configuring simlib %SIMVER% (renderers: %RENDERERS%) ==
cmake -S . -B build-msvc -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    -DSIMLIB_RENDERERS="%RENDERERS%" ^
    -DCMAKE_INSTALL_PREFIX="%CD%\build-msvc\sdk\%NAME%" || exit /b 1

echo == Building ==
cmake --build build-msvc --parallel || exit /b 1

echo == Installing ==
if exist "build-msvc\sdk" rmdir /s /q "build-msvc\sdk"
cmake --install build-msvc || exit /b 1

echo == Packaging %NAME%.zip ==
if exist "%OUT%\%NAME%.zip" del "%OUT%\%NAME%.zip"
tar -a -c -f "%OUT%\%NAME%.zip" -C "build-msvc\sdk" "%NAME%" || exit /b 1
powershell -NoProfile -Command "(Get-FileHash -Algorithm SHA256 '%OUT%\%NAME%.zip').Hash.ToLower() + '  %NAME%.zip' | Set-Content -Encoding ascii '%OUT%\%NAME%.zip.sha256'" || exit /b 1

echo.
echo Done: %OUT%\%NAME%.zip
echo Copy it to the web root's simlib\download folder to publish it.
endlocal
