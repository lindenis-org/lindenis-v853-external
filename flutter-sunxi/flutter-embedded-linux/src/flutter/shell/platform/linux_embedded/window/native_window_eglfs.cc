// Copyright 2021 Sony Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/linux_embedded/window/native_window_eglfs.h"

#include "flutter/shell/platform/linux_embedded/logger.h"

namespace flutter {

//namespace {
//static constexpr char kWmDeleteWindow[] = "WM_DELETE_WINDOW";
//static constexpr char kWindowTitle[] = "Flutter for Embedded Linux";
//}  // namespace

NativeWindowEglfs::NativeWindowEglfs(const size_t width, const size_t height) {
  window_ = nullptr;
  window_offscreen_ = nullptr;
  width_ = width;
  height_ = height;
  valid_ = true;
}

bool NativeWindowEglfs::Resize(const size_t width, const size_t height) {
  width_ = width;
  height_ = height;
  return true;
}

void NativeWindowEglfs::Destroy() {
  if (window_) {
    //XDestroyWindow(display, window_);
  }
}

}  // namespace flutter
