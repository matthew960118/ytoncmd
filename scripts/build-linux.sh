#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build}"
config="${2:-Release}"
version="${3:-v1.1}"

pkg-config --exists libavformat libavcodec libavutil libswscale libswresample || {
    echo "Missing FFmpeg development packages. Install pkg-config and FFmpeg dev libraries first." >&2
    exit 1
}

cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE="$config"
cmake --build "$build_dir" --config "$config"

package_dir="package/ytoncmd-${version}-linux"
mkdir -p "$package_dir"
cp "build/ytoncmd-linux-${version}/ytoncmd" "$package_dir/"

echo "Package ready: $package_dir"
