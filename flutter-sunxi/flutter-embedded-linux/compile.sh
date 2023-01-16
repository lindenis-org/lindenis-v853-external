#!/bin/bash

if [ ! -d build ]; then
    mkdir build
fi
cd build

readonly ENGINE_DIR="/home/anruliu/workspace/flutter-workspace/engine"

cp $ENGINE_DIR/src/out/linux_release_arm64/libflutter_engine.so ./

/home/anruliu/workspace/tools/cmake-3.21.1-linux-x86_64/bin/cmake \
    -DFLUTTER_RELEASE=ON \
    -DUSER_PROJECT_PATH=examples/flutter-video-player-plugin \
    -DBACKEND_TYPE=EGLFS \
    -DUSE_GLES3=OFF \
    -DBUILD_ELINUX_SO=OFF\
    -DENABLE_ELINUX_EMBEDDER_LOG=ON \
    -DCMAKE_BUILD_TYPE=Release ..

make

# Strip binary for release builds
$ENGINE_DIR/src/buildtools/linux-x64/clang/bin/llvm-strip --strip-all flutter-client
