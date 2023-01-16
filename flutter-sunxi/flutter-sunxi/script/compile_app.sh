#!/bin/bash

#./compile.sh app_path app_name
set -e

readonly FLUTTER_DIR="/home/anruliu/workspace/flutter-workspace/flutter"
readonly ENGINE_DIR="/home/anruliu/workspace/flutter-workspace/engine"

cd $1
mkdir -p .dart_tool/flutter_build/flutter-embedded-linux
mkdir -p build/linux-embedded-arm64/release/bundle/lib/
mkdir -p build/linux-embedded-arm64/release/bundle/data/

$FLUTTER_DIR/bin/flutter build bundle --asset-dir=build/linux-embedded-arm64/release/bundle/data/flutter_assets

cp $FLUTTER_DIR/bin/cache/artifacts/engine/linux-x64/icudtl.dat build/linux-embedded-arm64/release/bundle/data/

$FLUTTER_DIR/bin/dart \
  --verbose \
  --disable-dart-dev $FLUTTER_DIR/bin/cache/dart-sdk/bin/snapshots/frontend_server.dart.snapshot \
  --sdk-root $FLUTTER_DIR/bin/cache/artifacts/engine/common/flutter_patched_sdk \
  --target=flutter \
  --no-print-incremental-dependencies \
  -Ddart.vm.profile=false \
  -Ddart.vm.product=true \
  --aot \
  --tfa \
  --packages .dart_tool/package_config.json \
  --output-dill .dart_tool/flutter_build/flutter-embedded-linux/app.dill \
  --depfile .dart_tool/flutter_build/flutter-embedded-linux/kernel_snapshot.d \
  package:$2/main.dart

$ENGINE_DIR/src/out/linux_release_arm64/clang_x64/gen_snapshot \
  --deterministic \
  --snapshot_kind=app-aot-elf \
  --elf=.dart_tool/flutter_build/flutter-embedded-linux/libapp.so \
  --strip \
  .dart_tool/flutter_build/flutter-embedded-linux/app.dill

cp .dart_tool/flutter_build/flutter-embedded-linux/libapp.so build/linux-embedded-arm64/release/bundle/lib/

