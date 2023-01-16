// Copyright 2021 The Allwinnertech. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @file sunxiegl.cc
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "sunxiegl.h"
#if USE_SUNXIEGL != 0

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void egl_error_check();
static const GLubyte* hacked_glGetString(GLenum name);

/**********************
 *  STATIC VARIABLES
 **********************/
static EGLDisplay dpy;
static EGLSurface surface;
static EGLSurface res_surface;
static EGLContext context;
static EGLContext res_context;
extern bool fps_print;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Initialize the egl
 */
void sunxiegl_init(void) {
  /**********************
   * EGL INITIALIZATION *
   **********************/
  EGLint attrib_list[64];
  int attribIndex = 0;
  EGLint max_num_config;
  EGLint num_configs;
  EGLConfig *configs = NULL;
  int i;
  int cfg_found = 0;
  GLuint red = 8;
  GLuint green = 8;
  GLuint blue = 8;
  GLuint alpha = 8;
  EGLint major, minor;
  EGLConfig config;
  const EGLint context_attributes[] =
      { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

  attrib_list[attribIndex++] = EGL_RED_SIZE;
  attrib_list[attribIndex++] = red;
  attrib_list[attribIndex++] = EGL_GREEN_SIZE;
  attrib_list[attribIndex++] = green;
  attrib_list[attribIndex++] = EGL_BLUE_SIZE;
  attrib_list[attribIndex++] = blue;
  attrib_list[attribIndex++] = EGL_ALPHA_SIZE;
  attrib_list[attribIndex++] = alpha;
  attrib_list[attribIndex++] = EGL_RENDERABLE_TYPE;
  attrib_list[attribIndex++] = EGL_OPENGL_ES2_BIT;

  attrib_list[attribIndex++] = EGL_NONE;

  dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (dpy == EGL_NO_DISPLAY) {
    printf("flutter: no display\n");
    return;
  }

  if (!eglInitialize(dpy, &major, &minor)) {
    egl_error_check();
    return;
  }

  printf("flutter: egl version: %i.%i (%s)\nflutter: egl vendor: %s\n", major,
      minor, eglQueryString(dpy, EGL_VERSION), eglQueryString(dpy, EGL_VENDOR));

  if (!eglGetConfigs(dpy, NULL, 0, &max_num_config)) {
    egl_error_check();
    return;
  }

  configs = (EGLConfig*) malloc(sizeof(EGLConfig) * max_num_config);
  if (NULL == configs) {
    printf("flutter: out of memory\n");
    return;
  }

  if (!eglChooseConfig(dpy, attrib_list, configs, max_num_config,
      &num_configs)) {
    egl_error_check();
    return;
  }

  if (num_configs == 0) {
    printf("flutter: no matching config found\n");
    return;
  }

  for (i = 0; i < num_configs; i++) {
    EGLint value;
    /*Use this to explicitly check that the EGL config has the expected color depths */
    eglGetConfigAttrib(dpy, configs[i], EGL_RED_SIZE, &value);
    if (red != value)
      continue;
    printf("flutter: red OK: %d \n", value);
    eglGetConfigAttrib(dpy, configs[i], EGL_GREEN_SIZE, &value);
    if (green != value)
      continue;
    printf("flutter: green OK: %d \n", value);
    eglGetConfigAttrib(dpy, configs[i], EGL_BLUE_SIZE, &value);
    if (blue != value)
      continue;
    printf("flutter: blue OK: %d \n", value);
    eglGetConfigAttrib(dpy, configs[i], EGL_ALPHA_SIZE, &value);
    if (alpha != value)
      continue;
    printf("flutter: alpha OK: %d \n", value);
    eglGetConfigAttrib(dpy, configs[i], EGL_SAMPLES, &value);

    config = configs[i];
    cfg_found = 1;
    break;
  }

  if (!cfg_found) {
    printf("flutter: no matching config found\n");
    return;
  }

  /* Works with most things: */
  surface = eglCreateWindowSurface(dpy, config, NULL, 0);
  if (surface == EGL_NO_SURFACE) {
    egl_error_check();
    return;
  }

  res_surface = eglCreatePbufferSurface(dpy, config, 0);
  if (res_surface == EGL_NO_SURFACE) {
    egl_error_check();
    return;
  }

  context = eglCreateContext(dpy, config, EGL_NO_CONTEXT, context_attributes);
  if (context == EGL_NO_CONTEXT) {
    egl_error_check();
    return;
  }

  res_context = eglCreateContext(dpy, config, context, context_attributes);
  if (context == EGL_NO_CONTEXT) {
    egl_error_check();
    return;
  }
}

/// Called by flutter
void* sunxiegl_proc_resolver(void *user_data, const char *name) {
  void *address;

  if (name == NULL)
    return NULL;

  // if we do, and the symbol to resolve is glGetString, we return our hacked_glGetString.
  if (strcmp(name, "glGetString") == 0)
    return (void*) hacked_glGetString;

  if ((address = dlsym(RTLD_DEFAULT, name)) || (address =
      (void*) eglGetProcAddress(name)))
    return address;

  printf("flutter: could not resolve symbol \"%s\"\n", name);

  return NULL;
}

bool sunxiegl_make_current(void *user_data) {
  if (!eglMakeCurrent(dpy, surface, surface, context)) {
    egl_error_check();
    return false;
  }
  return true;
}

bool sunxiegl_make_resource_current(void *user_data) {
  if (!eglMakeCurrent(dpy, res_surface, res_surface, res_context)) {
    egl_error_check();
    return false;
  }
  return true;
}

bool sunxiegl_clear_current(void *user_data) {
  if (eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)
      != EGL_TRUE) {
    egl_error_check();
    return false;
  }
  return true;
}

bool sunxiegl_present(void *user_data) {
  eglSwapBuffers(dpy, surface);

  if (fps_print) {
    static struct timeval new_time, old_time;
    static int fps;
    gettimeofday(&new_time, NULL);
    if (new_time.tv_sec * 1000 - old_time.tv_sec * 1000 >= 1000) {
      printf("flutter: fps is %d\n", fps);
      old_time = new_time;
      fps = 0;
    } else {
      fps++;
    }
  }
  return true;
}

uint32_t sunxiegl_fbo_callback(void *user_data) {
  return 0;
}

void sunxiegl_get_sizes(uint32_t *width, uint32_t *height) {
  EGLint egl_width;
  EGLint egl_height;
  eglQuerySurface(dpy, surface, EGL_WIDTH, &egl_width);
  eglQuerySurface(dpy, surface, EGL_HEIGHT, &egl_height);

  if (width)
    *width = egl_width;

  if (height)
    *height = egl_height;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void egl_error_check() {
  EGLint err = eglGetError();

  switch (err) {
  case EGL_SUCCESS:
    printf("flutter: egl_error_check() = 0x%x: success\n", err);
    break;
  case EGL_NOT_INITIALIZED:
    printf("flutter: egl_error_check() = 0x%x: not initialized\n", err);
    break;
  case EGL_BAD_ACCESS:
    printf("flutter: egl_error_check() = 0x%x: bad access\n", err);
    break;
  case EGL_BAD_ALLOC:
    printf("flutter: egl_error_check() = 0x%x: bad alloc\n", err);
    break;
  case EGL_BAD_ATTRIBUTE:
    printf("flutter: egl_error_check() = 0x%x: bad attribute\n", err);
    break;
  case EGL_BAD_CONTEXT:
    printf("flutter: egl_error_check() = 0x%x: bad context\n", err);
    break;
  case EGL_BAD_CONFIG:
    printf("flutter: egl_error_check() = 0x%x: bad config\n", err);
    break;
  case EGL_BAD_CURRENT_SURFACE:
    printf("flutter: egl_error_check() = 0x%x: bad current surface\n", err);
    break;
  case EGL_BAD_DISPLAY:
    printf("flutter: egl_error_check() = 0x%x: bad display\n", err);
    break;
  case EGL_BAD_SURFACE:
    printf("flutter: egl_error_check() = 0x%x: bad surface\n", err);
    break;
  case EGL_BAD_MATCH:
    printf("flutter: egl_error_check() = 0x%x: bad match\n", err);
    break;
  case EGL_BAD_PARAMETER:
    printf("flutter: egl_error_check() = 0x%x: bad parameter\n", err);
    break;
  case EGL_BAD_NATIVE_PIXMAP:
    printf("flutter egl_error_check() = 0x%x: bad native pixmap\n", err);
    break;
  case EGL_BAD_NATIVE_WINDOW:
    printf("flutter: egl_error_check() = 0x%x: bad native window\n", err);
    break;
  default:
    printf("flutter: egl_error_check() = 0x%x\n", err);
  }
}

/// Cut a word from a string, mutating "string"
static void cut_word_from_string(char *string, const char *word) {
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
static const GLubyte* hacked_glGetString(GLenum name) {
  static GLubyte *extensions = NULL;

  if (name != GL_EXTENSIONS)
    return glGetString(name);

  if (extensions == NULL) {
    GLubyte *orig_extensions = (GLubyte*) glGetString(GL_EXTENSIONS);

    extensions = (GLubyte*) malloc(strlen((const char*) orig_extensions) + 1);
    if (!extensions) {
      fprintf(stderr,
          "flutter: Could not allocate memory for modified GL_EXTENSIONS string\n");
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

#endif
