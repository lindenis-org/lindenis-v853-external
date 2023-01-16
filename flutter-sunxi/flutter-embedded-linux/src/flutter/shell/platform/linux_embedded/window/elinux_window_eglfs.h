// Copyright 2021 Sony Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_LINUX_EMBEDDED_WINDOW_ELINUX_WINDOW_EGLFS_H_
#define FLUTTER_SHELL_PLATFORM_LINUX_EMBEDDED_WINDOW_ELINUX_WINDOW_EGLFS_H_

#include <memory>

#include "flutter/shell/platform/linux_embedded/surface/surface_gl.h"
#include "flutter/shell/platform/linux_embedded/window/elinux_window.h"
#include "flutter/shell/platform/linux_embedded/window/native_window_eglfs.h"
#include "flutter/shell/platform/linux_embedded/window_binding_handler.h"
#include "flutter/shell/platform/linux_embedded/logger.h"

#include <linux/input-event-codes.h>
#include <linux/input.h>

namespace flutter {

#define DEVPREFIX "/dev/input/event"
#define LABEL(constant) { #constant, constant }
#define LABEL_END { NULL, -1 }

class ELinuxWindowEglfs : public ELinuxWindow, public WindowBindingHandler {
 public:
  ELinuxWindowEglfs(FlutterDesktopViewProperties view_properties);
  ~ELinuxWindowEglfs();

  // |ELinuxWindow|
  bool IsValid() const override;

  // |FlutterWindowBindingHandler|
  bool DispatchEvent() override;

  // |FlutterWindowBindingHandler|
  bool CreateRenderSurface(int32_t width, int32_t height) override;

  // |FlutterWindowBindingHandler|
  void DestroyRenderSurface() override;

  // |FlutterWindowBindingHandler|
  void SetView(WindowBindingHandlerDelegate* view) override;

  // |FlutterWindowBindingHandler|
  ELinuxRenderSurfaceTarget* GetRenderSurfaceTarget() const override;

  // |FlutterWindowBindingHandler|
  double GetDpiScale() override;

  // |FlutterWindowBindingHandler|
  PhysicalWindowBounds GetPhysicalWindowBounds() override;

  // |FlutterWindowBindingHandler|
  int32_t GetFrameRate() override;

  // |FlutterWindowBindingHandler|
  void UpdateFlutterCursor(const std::string& cursor_name) override;

  // |FlutterWindowBindingHandler|
  void UpdateVirtualKeyboardStatus(const bool show) override;

  // |FlutterWindowBindingHandler|
  std::string GetClipboardData() override;

  // |FlutterWindowBindingHandler|
  void SetClipboardData(const std::string& data) override;

 private:
  // Handles the events of the mouse button.
  void HandlePointerButtonEvent(uint32_t button, bool button_pressed, int16_t x,
                                int16_t y);

  // A pointer to a FlutterWindowsView that can be used to update engine
  // windowing and input state.
  WindowBindingHandlerDelegate* binding_handler_delegate_ = nullptr;

  std::unique_ptr<NativeWindowEglfs> native_window_;
  std::unique_ptr<SurfaceGl> render_surface_;

  bool display_valid_;
  int evdev_fd;

 protected:
  struct label {
    const char *name;
    int value;
  };

  struct label input_prop_labels[5] = {
      LABEL(INPUT_PROP_POINTER),
      LABEL(INPUT_PROP_DIRECT),
      LABEL(INPUT_PROP_BUTTONPAD),
      LABEL(INPUT_PROP_SEMI_MT),
      LABEL_END,
  };

  const char* get_label(const struct label *labels, int value) {
    while (labels->name && value != labels->value) {
      labels++;
    }
    return labels->name;
  }

  int find_input_props(int fd) {
    uint8_t bits[INPUT_PROP_CNT / 8];
    int i, j, res, count, ret;
    const char *bit_label;

    res = ioctl(fd, EVIOCGPROP(sizeof(bits)), bits);
    if (res < 0) {
      return 1;
    }

    ret = 0;
    count = 0;
    for (i = 0; i < res; i++) {
      for (j = 0; j < 8; j++) {
        if (bits[i] & 1 << j) {
          bit_label = get_label(input_prop_labels, i * 8 + j);
          if (bit_label) {
            /* INPUT_PROP_DIRECT is a touch device */
            if (i * 8 + j == input_prop_labels[1].value)
              ret = 2;
          }
          count++;
        }
      }
    }
    return ret;
  }

  bool debounce(double old_x, double old_y, double x, double y) {
    double offset_x = x - old_x;
    double offset_y = y - old_y;

    if (offset_x < 0)
      offset_x = -offset_x;
    if (offset_y < 0)
      offset_y = -offset_y;

    if (offset_x <= 2 && offset_y <= 2)
      return true;

    return false;
  }
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_LINUX_EMBEDDED_WINDOW_ELINUX_WINDOW_X11_H_
