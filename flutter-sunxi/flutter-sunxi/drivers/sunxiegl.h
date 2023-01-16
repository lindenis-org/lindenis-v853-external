// Copyright 2021 The Allwinnertech. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @file sunxiegl.h
 *
 */

#ifndef SUNXIEGL_H
#define SUNXIEGL_H

#define USE_SUNXIEGL 1

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#if USE_SUNXIEGL
#include <stdint.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize the egl
 */
void sunxiegl_init(void);
void* sunxiegl_proc_resolver(void *user_data, const char *name);
bool sunxiegl_make_current(void *user_data);
bool sunxiegl_make_resource_current(void *user_data);
bool sunxiegl_clear_current(void *user_data);
bool sunxiegl_present(void *user_data);
uint32_t sunxiegl_fbo_callback(void *user_data);
void sunxiegl_get_sizes(uint32_t *width, uint32_t *height);

/**********************
 *      MACROS
 **********************/

#endif /* USE_SUNXIEGL */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SUNXIEGL_H */
