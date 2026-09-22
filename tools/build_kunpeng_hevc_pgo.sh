#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <HEVC-video> <work-directory> <install-prefix>" >&2
    exit 2
fi

sourceRoot=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
trainingVideo=$(realpath "$1")
workRoot=$(realpath -m "$2")
installPrefix=$(realpath -m "$3")
buildRoot="$workRoot/build"
profileRoot="$workRoot/profile"
jobs=${JOBS:-64}
ltoJobs=${LTO_JOBS:-32}
trainingFrames=${TRAINING_FRAMES:-15000}

if [ ! -f "$trainingVideo" ]; then
    echo "Training video does not exist: $trainingVideo" >&2
    exit 1
fi

if [ -e "$workRoot" ] && [ -n "$(find "$workRoot" -mindepth 1 -maxdepth 1 -print -quit)" ]; then
    echo "Work directory must be absent or empty: $workRoot" >&2
    exit 1
fi

mkdir -p "$buildRoot" "$profileRoot" "$installPrefix"

libraryPath="$buildRoot/libavdevice:$buildRoot/libavfilter:$buildRoot/libavformat"
libraryPath="$libraryPath:$buildRoot/libavcodec:$buildRoot/libswresample"
libraryPath="$libraryPath:$buildRoot/libswscale:$buildRoot/libavutil"

cd "$buildRoot"
"$sourceRoot/configure" \
    --prefix="$workRoot/generate-install" \
    --enable-shared \
    --enable-pthreads \
    --enable-gpl \
    --extra-cflags="-fopenmp -O3 -mcpu=tsv110 -fprofile-generate=$profileRoot" \
    --extra-ldflags="-fopenmp -fprofile-generate=$profileRoot"
make -j"$jobs"

LD_LIBRARY_PATH="$libraryPath:${LD_LIBRARY_PATH:-}" \
    "$buildRoot/ffmpeg_g" -v error -nostdin \
    -i "$trainingVideo" -frames:v "$trainingFrames" -an -sn \
    -vf format=rgb24 -f null - >/dev/null

profileCount=$(find "$profileRoot" -type f -name '*.gcda' | wc -l)
if [ "$profileCount" -eq 0 ]; then
    echo "PGO training produced no .gcda files" >&2
    exit 1
fi

make distclean
"$sourceRoot/configure" \
    --prefix="$installPrefix" \
    --enable-shared \
    --enable-pthreads \
    --enable-gpl \
    --extra-cflags="-fopenmp -O3 -mcpu=tsv110 -fprofile-use=$profileRoot -fprofile-correction -Wno-missing-profile -flto=$ltoJobs" \
    --extra-ldflags="-fopenmp -fprofile-use=$profileRoot -fprofile-correction -flto=$ltoJobs"
make -j"$jobs"
make install

echo "Installed PGO/LTO build to $installPrefix"
