#!/bin/bash
set -e

cd ..
if [ ! -d build/fbdev_release_arm64 ]; then
    mkdir -p build/fbdev_release_arm64
fi
cd build/fbdev_release_arm64
/home/anruliu/workspace/tools/cmake-3.21.1-linux-x86_64/bin/cmake -DUSE_GLES3=OFF -DCMAKE_BUILD_TYPE=Release ../..
make

cd ..
if [ ! -d eglfs_release_arm64 ]; then
    mkdir eglfs_release_arm64
fi
cd eglfs_release_arm64
/home/anruliu/workspace/tools/cmake-3.21.1-linux-x86_64/bin/cmake -DUSE_GLES3=ON -DCMAKE_BUILD_TYPE=Release ../..
make
