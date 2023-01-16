// Copyright 2021 Sony Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/linux_embedded/window/elinux_window_eglfs.h"

#include "flutter/shell/platform/linux_embedded/surface/context_egl.h"

#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>

namespace flutter {

ELinuxWindowEglfs::ELinuxWindowEglfs(
    FlutterDesktopViewProperties view_properties) {
  view_properties_ = view_properties;

  int fbfd;
  struct fb_var_screeninfo vinfo;

  // open the file for reading and writing
  fbfd = open("/dev/fb0", O_RDWR);
  if (fbfd == -1) {
    ELINUX_LOG(ERROR) << "Cannot open framebuffer device";
    return;
  }

  // get variable screen information
  if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
    ELINUX_LOG(ERROR) << "Cannot reading variable information";
    return;
  }

  close(fbfd);

  ELINUX_LOG(INFO) << "Wh=" << vinfo.xres << "x" << vinfo.yres << " Vwh="
      << vinfo.xres_virtual << "x" << vinfo.yres_virtual << " Bpp="
      << vinfo.bits_per_pixel;

  // full screen by default
  view_properties_.width = vinfo.xres;
  view_properties_.height = vinfo.yres;

  char *pathvar;
  pathvar = getenv("NULLWS_WINDOW_ROTATION");
  if (pathvar && (strcmp(pathvar, "90") == 0 || strcmp(pathvar, "270") == 0)) {
    view_properties_.width = vinfo.yres;
    view_properties_.height = vinfo.xres;
  }

  if (access("/dev/input/touchscreen", F_OK) != 0) {
    int i = 0, event_id = 0;
    char ibuf[sizeof(DEVPREFIX) + 4];
    char name[256];
    /* aggressively open/lock devices */
    do {
      sprintf(ibuf, "%s%i", DEVPREFIX, i);

      int fd = open(ibuf, O_RDONLY);
      if (-1 == fd) {
        if (errno == ENOENT)
          break;
        continue;
      }
      ioctl(fd, EVIOCGNAME(sizeof(name)), name);

      ELINUX_LOG(INFO) << "Found input device: " << name;
      if (find_input_props(fd) == 2) {
        event_id = i;
      }

      close(fd);
    } while (++i);

    sprintf(ibuf, "%s%i", DEVPREFIX, event_id);
    evdev_fd = open(ibuf, O_RDWR | O_NOCTTY | O_NDELAY);
  } else {
    ELINUX_LOG(INFO) << "Found input device: /dev/input/touchscreen";
    evdev_fd = open("/dev/input/touchscreen", O_RDWR | O_NOCTTY | O_NDELAY);
  }

  if (evdev_fd == -1) {
    ELINUX_LOG(ERROR) << "Unable open evdev interface";
    return;
  }

  fcntl(evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);

  display_valid_ = true;
}

ELinuxWindowEglfs::~ELinuxWindowEglfs() {
  display_valid_ = false;
}

bool ELinuxWindowEglfs::IsValid() const {
  if (!display_valid_ || !native_window_ || !render_surface_
      || !native_window_->IsValid() || !render_surface_->IsValid()) {
    return false;
  }
  return true;
}

bool ELinuxWindowEglfs::DispatchEvent() {
  struct input_event in;
  static double evdev_x, evdev_y, old_evdev_x, old_evdev_y;
  static int evdev_button;
  static FlutterPointerPhase point_phase;

  while (read(evdev_fd, &in, sizeof(struct input_event)) > 0) {
    if (in.type == EV_REL) {
      if (in.code == REL_X)
        evdev_x += in.value;
      else if (in.code == REL_Y)
        evdev_y += in.value;
    } else if (in.type == EV_ABS) {
      if (in.code == ABS_X)
        evdev_x = in.value;
      else if (in.code == ABS_Y)
        evdev_y = in.value;
      else if (in.code == ABS_MT_POSITION_X)
        evdev_x = in.value;
      else if (in.code == ABS_MT_POSITION_Y)
        evdev_y = in.value;
      else if (in.code == ABS_PRESSURE) {
        if (in.value == 0)
          evdev_button = 1; //UP
        else if (in.value > 0)
          evdev_button = 2; //DOWN
      }
    } else if (in.type == EV_KEY) {
      if (in.code == BTN_MOUSE || in.code == BTN_TOUCH) {
        if (in.value == 0)
          evdev_button = 1; //UP
        else if (in.value == 1)
          evdev_button = 2; //DOWN
      }
    }

    if (in.code == SYN_REPORT && binding_handler_delegate_) {
      unsigned int timestamp =
          std::chrono::duration_cast < std::chrono::milliseconds
              > (std::chrono::high_resolution_clock::now().time_since_epoch()).count();

      if (point_phase != FlutterPointerPhase::kMove || evdev_button == 1)
        point_phase = (FlutterPointerPhase) evdev_button;

      switch (point_phase) {
      case FlutterPointerPhase::kDown:
        old_evdev_x = evdev_x;
        old_evdev_y = evdev_y;
        binding_handler_delegate_->OnTouchDown(timestamp, 0, evdev_x, evdev_y);

        point_phase = FlutterPointerPhase::kMove;
        break;
      case FlutterPointerPhase::kUp:
        binding_handler_delegate_->OnTouchUp(timestamp, 0);
        old_evdev_x = 0;
        old_evdev_y = 0;
        break;
      case FlutterPointerPhase::kMove:
        // The move event will not be reported if it is the same point as kDown
        if (!debounce(old_evdev_x, old_evdev_y, evdev_x, evdev_y))
          binding_handler_delegate_->OnTouchMotion(timestamp, 0, evdev_x,
              evdev_y);
        break;
      default:
        /* Don't do anything */
        break;
      }
    }
  }

  return true;
}

bool ELinuxWindowEglfs::CreateRenderSurface(int32_t width, int32_t height) {
  auto context_egl = std::make_unique < ContextEgl
      > (std::make_unique < EnvironmentEgl > (EGL_DEFAULT_DISPLAY));

  native_window_ = std::make_unique < NativeWindowEglfs > (width, height);
  if (!native_window_->IsValid()) {
    ELINUX_LOG(ERROR) << "Failed to create the native window";
    return false;
  }

  render_surface_ = std::make_unique < SurfaceGl > (std::move(context_egl));
  render_surface_->SetNativeWindow(native_window_.get());

  ELINUX_LOG(INFO) << "Display output resolution: " << view_properties_.width
      << "x" << view_properties_.height;

  return true;
}

void ELinuxWindowEglfs::DestroyRenderSurface() {
  // destroy the main surface before destroying the client window on X11.
  render_surface_ = nullptr;
  native_window_ = nullptr;
}

void ELinuxWindowEglfs::SetView(WindowBindingHandlerDelegate *window) {
  binding_handler_delegate_ = window;
}

ELinuxRenderSurfaceTarget* ELinuxWindowEglfs::GetRenderSurfaceTarget() const {
  return render_surface_.get();
}

double ELinuxWindowEglfs::GetDpiScale() {
  return current_scale_;
}

PhysicalWindowBounds ELinuxWindowEglfs::GetPhysicalWindowBounds() {
  return {GetCurrentWidth(), GetCurrentHeight()};
}

int32_t ELinuxWindowEglfs::GetFrameRate() {
  return 60000;
}

void ELinuxWindowEglfs::UpdateFlutterCursor(const std::string &cursor_name) {
  // TODO: implement here
}

void ELinuxWindowEglfs::UpdateVirtualKeyboardStatus(const bool show) {
  // currently not supported.
}

std::string ELinuxWindowEglfs::GetClipboardData() {
  return clipboard_data_;
}

void ELinuxWindowEglfs::SetClipboardData(const std::string &data) {
  clipboard_data_ = data;
}

void ELinuxWindowEglfs::HandlePointerButtonEvent(uint32_t button,
    bool button_pressed, int16_t x, int16_t y) {

}

}  // namespace flutter
