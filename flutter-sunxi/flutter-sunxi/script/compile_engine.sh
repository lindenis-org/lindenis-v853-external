#!/bin/bash
set -e

cd /home/anruliu/workspace/flutter-workspace/engine/src

./flutter/tools/gn \
    --target-os linux \
    --linux-cpu arm64 \
    --runtime-mode release \
    --embedder-for-target \
    --disable-desktop-embeddings

/home/anruliu/workspace/flutter-workspace/depot_tools/ninja -C out/linux_release_arm64

