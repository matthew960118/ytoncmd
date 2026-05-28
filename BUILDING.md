# Building

## Windows package

The Windows release is built as `ytoncmd.exe` plus the FFmpeg DLLs beside it.
This keeps the package ready to run without requiring users to install FFmpeg,
while still using Windows system DLLs from the OS.

Prerequisites:

- Visual Studio 2022 with C++ tools
- CMake
- 7-Zip available as `7z` in `PATH`

Build:

```powershell
.\scripts\build-windows.ps1
```

Output:

```text
package/ytoncmd-v1.1-windows/
```

## Linux package

Prerequisites:

- CMake
- pkg-config
- FFmpeg development packages for `libavformat`, `libavcodec`, `libavutil`, `libswscale`, and `libswresample`

Build:

```bash
./scripts/build-linux.sh
```

Output:

```text
package/ytoncmd-v1.1-linux/
```

## Cross-compilation note

The current Windows package uses the MSVC import libraries from the gyan FFmpeg
shared build. That path should be built with MSVC on Windows, locally or in
GitHub Actions.

Linux-to-Windows cross-compilation is a separate MinGW target and needs a MinGW
compatible FFmpeg package. It cannot reuse the MSVC `.lib` files directly.
