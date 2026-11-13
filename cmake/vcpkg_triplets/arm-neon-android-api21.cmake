set(VCPKG_TARGET_ARCHITECTURE arm)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_CMAKE_SYSTEM_VERSION 21)
set(VCPKG_MAKE_BUILD_TRIPLET "--host=armv7a-linux-androideabi")
# WavPack's 64-bit seek functions are unavailable before Android API 24.
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    -DANDROID_ABI=armeabi-v7a
    -DANDROID_ARM_NEON=ON
    -DANDROID_PLATFORM=android-21
    -DHAVE_FSEEKO=OFF
)
