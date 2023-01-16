// Copyright 2021 Sony Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_LINUX_EMBEDDED_WINDOW_NATIVE_WINDOW_EGLFS_H_
#define FLUTTER_SHELL_PLATFORM_LINUX_EMBEDDED_WINDOW_NATIVE_WINDOW_EGLFS_H_

#include <string>

#include "flutter/shell/platform/linux_embedded/window/native_window.h"

namespace flutter {

class NativeWindowEglfs : public NativeWindow {
 public:
  NativeWindowEglfs(const size_t width, const size_t height);
  ~NativeWindowEglfs() = default;

  // |NativeWindow|
  bool Resize(const size_t width, const size_t height) override;

  void Destroy();

 private:
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_LINUX_EMBEDDED_WINDOW_NATIVE_WINDOW_EGLFS_H_
