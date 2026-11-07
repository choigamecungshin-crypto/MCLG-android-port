# Android port status

This directory is an independent Android platform bootstrap. The legacy Visual Studio projects and the existing Minecraft source directories are intentionally left unchanged.

## Current milestone: Android host + native graphics bootstrap

- Android Activity hosted by a GLES 3.0 GLSurfaceView.
- JNI bridge into a C++17 shared library.
- ARM64-only NDK configuration.
- GitHub Actions workflow producing a debug APK artifact.

The current native renderer only clears the screen. It is a build and device-surface smoke test, NOT yet the Minecraft game port.

## Required porting work

1. Audit the original project files, platform macros, third-party dependencies, and asset pipeline.
2. Separate portable world/gameplay code from Win32, DirectX, Xbox, PlayStation, and Vita platform code.
3. Implement Android filesystem, threading, timing, input, audio, and lifecycle adapters.
4. Port the rendering backend to OpenGL ES 3.x and replace platform-specific shader/resource handling.
5. Integrate portable gameplay/client modules incrementally, resolving compiler and linker errors per subsystem.
6. Add touch controls, controller support, save paths, and performance profiles for mobile GPUs.

## Build

GitHub Actions installs JDK 17, Android SDK 35, NDK r27, and CMake 3.22.1, then runs:

    gradle --no-daemon :app:assembleDebug

The APK is uploaded as the MCLG-Android-arm64-debug workflow artifact.

## Important

A successful bootstrap APK does not imply that Minecraft LCE has been ported. The original Visual Studio projects target desktop/console APIs and still require substantial source-level platform adaptation before the game can run on Android.


## Application identity and signing

- Stable application ID: `dev.mc.legacy`.
- Release signing alias: `mclg`; store and key passwords: `mclg123` (Java `keytool` requires at least 6 characters).
- The first workflow run without `MCLG_KEYSTORE_BASE64` creates a one-time keystore and uploads it as `MCLG-signing-keystore-SETUP-KEEP-SAFE`. Download and preserve that file.
- Convert the keystore to Base64, then add GitHub Actions repository secrets:
  - `MCLG_KEYSTORE_BASE64`: Base64 contents of the exact generated `mclg-release.jks` file.
  - `MCLG_STORE_PASSWORD`: `mclg`.
  - `MCLG_KEY_PASSWORD`: `mclg`.
- Subsequent workflow runs restore that same keystore and publish a signed release APK. Never regenerate or lose the keystore: Android updates require the same signing certificate.
- The first install after changing from the old package ID requires installing `dev.mc.legacy` as a new app. Future signed builds can update it in place.
