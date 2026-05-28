#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build_mingw}"
config="${2:-Release}"
version="${3:-v1.1}"
ffmpeg_dir="${4:-ffmpeg-8.1.1-full_build-shared}"
ffmpeg_url="https://www.gyan.dev/ffmpeg/builds/packages/ffmpeg-8.1.1-full_build-shared.7z"
download_dir="ffmpeg-download"
archive="${download_dir}/ffmpeg-shared.7z"

for tool in x86_64-w64-mingw32-gcc x86_64-w64-mingw32-g++ x86_64-w64-mingw32-windres 7z; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "Missing required tool: $tool" >&2
        exit 1
    }
done

if [[ ! -d "${ffmpeg_dir}/include" || ! -d "${ffmpeg_dir}/lib" || ! -d "${ffmpeg_dir}/bin" ]]; then
    mkdir -p "$download_dir"

    if [[ ! -f "$archive" ]]; then
        echo "Downloading FFmpeg 8.1.1 shared SDK..."
        curl -L "$ffmpeg_url" -o "$archive"
    fi

    echo "Extracting FFmpeg shared SDK..."
    7z x "$archive" "-o${download_dir}" -y >/dev/null

    extracted_dir="$(
        find "$download_dir" -type d |
        while IFS= read -r dir; do
            if [[ -d "${dir}/include" && -d "${dir}/lib" && -d "${dir}/bin" ]]; then
                printf '%s\n' "$dir"
                break
            fi
        done
    )"

    if [[ -z "$extracted_dir" ]]; then
        echo "Cannot find extracted FFmpeg directory containing include/, lib/, and bin/." >&2
        exit 1
    fi

    mkdir -p "$ffmpeg_dir"
    cp -R "${extracted_dir}/include" "$ffmpeg_dir/"
    cp -R "${extracted_dir}/lib" "$ffmpeg_dir/"
    cp -R "${extracted_dir}/bin" "$ffmpeg_dir/"
fi

cmake -S . -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$config" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake \
    -DFFMPEG_WIN_DIR="$PWD/$ffmpeg_dir"
cmake --build "$build_dir" --config "$config"

output_dir="build_win/ytoncmd-win-${version}"
package_dir="package/ytoncmd-${version}-windows-mingw"
mkdir -p "$package_dir"

if [[ -f "${output_dir}/${config}/ytoncmd.exe" ]]; then
    cp -R "${output_dir}/${config}/." "$package_dir/"
else
    cp -R "${output_dir}/." "$package_dir/"
fi

echo "Package ready: $package_dir"
