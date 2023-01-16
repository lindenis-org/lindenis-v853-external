#!/bin/bash
set -e

readonly FLUTTER_DIR="/home/anruliu/workspace/flutter-workspace/flutter"
readonly ENGINE_DIR="/home/anruliu/workspace/flutter-workspace/engine"
readonly APP_VERSION="1.0.6"

# Delete binary files
rm -rf ../flutter-sunxi-release/arm
rm -rf ../flutter-sunxi-release/arm64
rm -rf ../build

mkdir ../flutter-sunxi-release/arm
mkdir ../flutter-sunxi-release/arm64

# Compile
./compile_engine.sh
./compile_app.sh $FLUTTER_DIR/dev/benchmarks/complex_layout complex_layout
./compile_app.sh $FLUTTER_DIR/../gallery gallery
./compile_sunxi.sh

cd ../flutter-sunxi-release
# Copy client
cp -a ../build/fbdev_release_arm64/flutter_fbdev ./arm64
cp -a ../build/eglfs_release_arm64/flutter_eglfs ./arm64

# Copy dynamic library
cp -a $ENGINE_DIR/src/out/linux_release_arm64/libflutter_engine.so ./arm64
cp -a $ENGINE_DIR/src/out/linux_release_arm64/clang_x64/gen_snapshot ./arm64

# Copy application complex_layout
cp -a $FLUTTER_DIR/dev/benchmarks/complex_layout/build/linux-embedded-arm64/release/bundle ./
mv ./bundle ./bundle_complex_layout
rm ./bundle_complex_layout/data/flutter_assets/isolate_snapshot_data
rm ./bundle_complex_layout/data/flutter_assets/kernel_blob.bin
rm ./bundle_complex_layout/data/flutter_assets/NOTICES.Z
rm ./bundle_complex_layout/data/flutter_assets/vm_snapshot_data
tar -zcvf ./complex_layout.tar.gz ./bundle_complex_layout
rm -rf ./bundle_complex_layout

# Copy application gallery
cp -a $FLUTTER_DIR/../gallery/build/linux-embedded-arm64/release/bundle ./
mv ./bundle ./bundle_gallery
rm ./bundle_gallery/data/flutter_assets/isolate_snapshot_data
rm ./bundle_gallery/data/flutter_assets/kernel_blob.bin
rm ./bundle_gallery/data/flutter_assets/NOTICES.Z
rm ./bundle_gallery/data/flutter_assets/vm_snapshot_data
tar -zcvf ./gallery.tar.gz ./bundle_gallery
rm -rf ./bundle_gallery

# Make a release package
cd ..
cp -a ./flutter-sunxi-release ./build/flutter-sunxi-$APP_VERSION
cd build
tar -zcvf ./flutter-sunxi-$APP_VERSION.tar.gz ./flutter-sunxi-$APP_VERSION
