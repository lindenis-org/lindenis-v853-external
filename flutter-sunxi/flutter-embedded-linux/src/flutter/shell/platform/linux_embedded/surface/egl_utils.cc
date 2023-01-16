
// Copyright 2021 Sony Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/linux_embedded/surface/egl_utils.h"

#include "flutter/shell/platform/linux_embedded/logger.h"

#include <EGL/egl.h>

#include <string>
#include <vector>

namespace flutter {

std::string get_egl_error_cause() {
  static const std::vector<std::pair<EGLint, std::string>> table = {
      {EGL_SUCCESS, "EGL_SUCCESS"},
      {EGL_NOT_INITIALIZED, "EGL_NOT_INITIALIZED"},
      {EGL_BAD_ACCESS, "EGL_BAD_ACCESS"},
      {EGL_BAD_ALLOC, "EGL_BAD_ALLOC"},
      {EGL_BAD_ATTRIBUTE, "EGL_BAD_ATTRIBUTE"},
      {EGL_BAD_CONTEXT, "EGL_BAD_CONTEXT"},
      {EGL_BAD_CONFIG, "EGL_BAD_CONFIG"},
      {EGL_BAD_CURRENT_SURFACE, "EGL_BAD_CURRENT_SURFACE"},
      {EGL_BAD_DISPLAY, "EGL_BAD_DISPLAY"},
      {EGL_BAD_SURFACE, "EGL_BAD_SURFACE"},
      {EGL_BAD_MATCH, "EGL_BAD_MATCH"},
      {EGL_BAD_PARAMETER, "EGL_BAD_PARAMETER"},
      {EGL_BAD_NATIVE_PIXMAP, "EGL_BAD_NATIVE_PIXMAP"},
      {EGL_BAD_NATIVE_WINDOW, "EGL_BAD_NATIVE_WINDOW"},
      {EGL_CONTEXT_LOST, "EGL_CONTEXT_LOST"},
  };

  auto egl_error = eglGetError();
  for (auto t : table) {
    if (egl_error == t.first) {
      return std::string("eglGetError: " + t.second);
    }
  }
  return nullptr;
}

/// Cut a word from a string, mutating "string"
void cut_word_from_string(char *string, const char *word) {
  size_t word_length = strlen(word);
  char *word_in_str = strstr(string, word);

  // check if the given word is surrounded by spaces in the string
  if (word_in_str && ((word_in_str == string) || (word_in_str[-1] == ' '))
      && ((word_in_str[word_length] == 0) || (word_in_str[word_length] == ' '))) {
    if (word_in_str[word_length] == ' ')
      word_length++;

    int i = 0;
    do {
      word_in_str[i] = word_in_str[i + word_length];
    } while (word_in_str[i++ + word_length] != 0);
  }
}

/// An override for glGetString since the real glGetString
/// won't work.
const GLubyte* hacked_glGetString(GLenum name) {
  static GLubyte *extensions = nullptr;

  if (name != GL_EXTENSIONS)
    return glGetString(name);

  if (extensions == nullptr) {
    GLubyte *orig_extensions = (GLubyte*) glGetString(GL_EXTENSIONS);

    extensions = (GLubyte*) malloc(strlen((const char*) orig_extensions) + 1);
    if (!extensions) {
      ELINUX_LOG(ERROR) << "Could not allocate memory for modified GL_EXTENSIONS string";
      return NULL;
    }

    strcpy((char*) extensions, (const char*) orig_extensions);

    /*
     * should be working, but isn't for R818
     */
    cut_word_from_string((char*) extensions, "GL_KHR_blend_equation_advanced");
    cut_word_from_string((char*) extensions, "GL_NV_blend_equation_advanced");
    cut_word_from_string((char*) extensions, "GL_EXT_discard_framebuffer");
    cut_word_from_string((char*) extensions,
        "GL_EXT_multisampled_render_to_texture");
    cut_word_from_string((char*) extensions,
        "GL_IMG_multisampled_render_to_texture");
    cut_word_from_string((char*) extensions, "GL_OES_mapbuffer");

    /*
     * definitely broken
     */
    //cut_word_from_string((char*) extensions, "GL_OES_element_index_uint");
  }

  return extensions;
}

}  // namespace flutter
