param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [string]$Version = "v1.1"
)

$ErrorActionPreference = "Stop"

$ffmpegSharedDir = Join-Path $PSScriptRoot "..\ffmpeg-shared"
$ffmpegDownloadDir = Join-Path $PSScriptRoot "..\ffmpeg-download"
$ffmpegArchive = Join-Path $ffmpegDownloadDir "ffmpeg-shared.7z"
$ffmpegUrl = "https://www.gyan.dev/ffmpeg/builds/packages/ffmpeg-8.1.1-full_build-shared.7z"

if (-not (Test-Path (Join-Path $ffmpegSharedDir "include")) -or
    -not (Test-Path (Join-Path $ffmpegSharedDir "lib")) -or
    -not (Test-Path (Join-Path $ffmpegSharedDir "bin"))) {
    if (-not (Get-Command 7z -ErrorAction SilentlyContinue)) {
        throw "7z was not found in PATH. Install 7-Zip before running this script."
    }

    Write-Host "Downloading FFmpeg 8.1.1 shared SDK..."
    New-Item -ItemType Directory -Force -Path $ffmpegDownloadDir | Out-Null
    Invoke-WebRequest -Uri $ffmpegUrl -OutFile $ffmpegArchive

    Write-Host "Extracting FFmpeg shared SDK..."
    & 7z x $ffmpegArchive "-o$ffmpegDownloadDir" -y | Out-Host

    $ffmpegRoot = Get-Item -Path $ffmpegDownloadDir
    $ffmpegDir = @($ffmpegRoot) + @(Get-ChildItem -Directory -Path $ffmpegDownloadDir -Recurse) | Where-Object {
        (Test-Path (Join-Path $_.FullName "include")) -and
        (Test-Path (Join-Path $_.FullName "lib")) -and
        (Test-Path (Join-Path $_.FullName "bin"))
    } | Select-Object -First 1

    if (-not $ffmpegDir) {
        throw "Cannot find extracted FFmpeg directory containing include/, lib/, and bin/."
    }

    New-Item -ItemType Directory -Force -Path $ffmpegSharedDir | Out-Null
    Copy-Item -Path (Join-Path $ffmpegDir.FullName "include") -Destination (Join-Path $ffmpegSharedDir "include") -Recurse -Force
    Copy-Item -Path (Join-Path $ffmpegDir.FullName "lib") -Destination (Join-Path $ffmpegSharedDir "lib") -Recurse -Force
    Copy-Item -Path (Join-Path $ffmpegDir.FullName "bin") -Destination (Join-Path $ffmpegSharedDir "bin") -Recurse -Force
}

cmake -S . -B $BuildDir -DCMAKE_BUILD_TYPE=$Config --fresh
cmake --build $BuildDir --config $Config

$packageDir = "package/ytoncmd-$Version-windows"
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null
Copy-Item -Path "build_win/ytoncmd-win-$Version/$Config/*" -Destination $packageDir -Recurse -Force

Write-Host "Package ready: $packageDir"
