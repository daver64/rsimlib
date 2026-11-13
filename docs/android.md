# Using simlib on Android

[← Back to README](../README.md) | [Android build prerequisites](building.md#android-opengl-es)

**Status: new and in development.** The Android backend is experimental and less tested than the desktop backends. Features, device compatibility and packaging may change. Test your application on its target devices; a successful desktop build does not establish Android support.

The current Android build uses SDL2 and OpenGL ES 3.1, with Android API 21 as the minimum. The supplied debug APKs contain `arm64-v8a`, `armeabi-v7a` and `x86_64` libraries. Devices must provide OpenGL ES 3.1; an emulator exposing only ES 3.0 cannot run these builds.

## Set up and build

Follow [Android build prerequisites](building.md#android-opengl-es) for the SDK, NDK, JDK, vcpkg and SDL2 submodule setup. From the repository root:

```bash
# Build one registered app, or omit its name to build all registered apps.
./android/build.sh exasteroids
```

The result is `build/android/apks/exasteroids.apk`. The separate `showcase` app builds with `./android/build.sh showcase`; `./android/build.sh` builds both. APKs are named after their registered app and have separate application IDs, so they can coexist. `fullbuild.sh` also runs the Android build for all registered apps.

These are debug-signed APKs for testing, not Play Store releases. You can share an APK with a tester, who may need to allow their file-opening app to install unknown apps. An existing app signed with a different key cannot be updated with `adb install -r`; it must be removed first, which also removes its app data.

## Install and test

On a phone, enable developer options and USB debugging, connect it, approve the computer's debugging authorization prompt, and check that `adb devices -l` reports it as `device`. With one connected device:

```bash
adb devices -l
adb install -r build/android/apks/exasteroids.apk
adb shell am start -n com.daver64.simlib.exasteroids/com.daver64.simlib.android.SimlibActivity
```

If several devices are connected, use `adb -s SERIAL ...` for each command. For another registered app, use its `applicationId` from `android/apps/NAME/app.properties` in place of `com.daver64.simlib.exasteroids`; the activity class remains `com.daver64.simlib.android.SimlibActivity`.

The [build guide](building.md#android-opengl-es) also covers creating an x86_64 emulator. A host GPU may be needed for GLES 3.1. Use `adb logcat` to investigate startup, rendering or native failures, and exercise touch, orientation, background/resume and audio on real devices as well as an emulator.

## Register your own program

The C++ source can stay in its existing example or project directory. Register an Android-ready program by creating `android/apps/NAME/app.cmake` and `android/apps/NAME/app.properties`. `NAME` must start with a lowercase letter and contain only lowercase letters, digits or underscores. For example, for `mygame`:

```cmake
# android/apps/mygame/app.cmake
simlib_add_android_app(mygame "${SIMLIB_ROOT}/src/examples/mygame/mygame.cpp")
```

```properties
# android/apps/mygame/app.properties
applicationId=com.example.mygame
label=My Game
orientation=landscape
```

Choose a unique, valid Android application ID you control. The build rejects duplicate registered IDs. Use an Android `screenOrientation` value such as `landscape` or `unspecified`. Add any other source files to the CMake call and, if needed, configure `mygame_main` with target-specific includes, definitions or libraries, as in [exasteroids](../android/apps/exasteroids/app.cmake). Then run `./android/build.sh mygame` to produce `build/android/apks/mygame.apk`.

The shared Android project packages each program's `main` as its own `libmain.so`; SDL's activity starts it. Each app has separate native and Gradle outputs, while the simlib/SDL runtime is built for each ABI. The shared Gradle configuration currently sets the same version code and version name for all apps. Registration alone does not port a desktop application: adapt and test its controls, screen layout, assets and Android lifecycle.

## Input, display and lifecycle

Use the normal simlib event loop, including `sl::display_handle_event(event)`. Touch events include `sl::Event::Type::finger_down`, `finger_motion` and `finger_up`. For held on-screen controls, `sl::touch_points()` returns current finger positions in virtual-screen coordinates; `sl::touch_to_screen()` converts normalized SDL coordinates. [exasteroids](../src/examples/exasteroids/exasteroids.cpp) demonstrates simultaneous touch controls and Android-specific code guarded by `__ANDROID__`.

Do not assume the desktop window dimensions or keyboard are available. Choose a suitable logical size with the virtual width and height arguments to `sl::set_gfx_mode`, and test the layout and touch targets on devices with different aspect ratios. Exasteroids uses a fixed logical size and requests landscape both in its registration and through an SDL orientation environment hint before display initialization.

Android can pause, resume or terminate an app independently of its game loop. Test interruptions such as switching apps, locking the screen and rotation. The event API exposes application background and foreground events; handle application-specific pause and state preservation as needed.

## Assets and current limitations

The shared Gradle project currently packages simlib's `shaders` and `assets/fonts` directories. Add your own assets to the Gradle asset-copy configuration before relying on them. APK assets are not ordinary writable desktop files: check that each loader supports packaged assets, and use an appropriate Android app-private writable location for saves. Some simlib loaders use ordinary filesystem streams, so don't assume every desktop path works inside an APK. Test asset loading on the device.

The supplied Android CMake configuration enables only `gles` and disables ImGui integration and video decoding. Desktop polygon wireframe mode and GPU timer diagnostics are unavailable on GLES. Other desktop examples and APIs are not automatically Android-validated; adapt them and test rendering, input, audio, assets and lifecycle on the intended hardware.
