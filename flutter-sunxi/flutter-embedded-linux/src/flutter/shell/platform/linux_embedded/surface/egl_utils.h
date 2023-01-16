// Copyright 2021 Sony Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_LINUX_EMBEDDED_SURFACE_EGL_UTILS_H_
#define FLUTTER_SHELL_PLATFORM_LINUX_EMBEDDED_SURFACE_EGL_UTILS_H_

#include <string>
#include <GLES2/gl2.h>

namespace flutter {

std::string get_egl_error_cause();
void cut_word_from_string(char *string, const char *word);
const GLubyte* hacked_glGetString(GLenum name);

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_LINUX_EMBEDDED_SURFACE_EGL_UTILS_H_
