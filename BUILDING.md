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

The GitHub Actions Windows package uses MSVC and the gyan FFmpeg shared build.
That path should be built with MSVC on Windows, locally or in GitHub Actions.

Linux-to-Windows cross-compilation is available as a separate MinGW target:

```bash
./scripts/build-windows-mingw.sh
```

Prerequisites:

- `mingw-w64`
- `cmake`
- `curl`
- `7z`

Output:

```text
package/ytoncmd-v1.1-windows-mingw/
```

The MinGW build uses:

```text
cmake/toolchains/mingw-w64-x86_64.cmake
ffmpeg-8.1.1-full_build-shared/
```

The script downloads the FFmpeg package if that directory is missing. This is
for local validation before pushing. The official GitHub Actions package still
uses the MSVC workflow.
