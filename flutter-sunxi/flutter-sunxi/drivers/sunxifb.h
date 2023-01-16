// Copyright 2021 The Allwinnertech. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @file sunxifb.h
 *
 */

#ifndef SUNXIFB_H
#define SUNXIFB_H

#define USE_SUNXIFB 1
#define USE_SUNXIFB_DOUBLE_BUFFER 1
//#define USE_SUNXIFB_CACHE 1
//#define USE_SUNXIFB_G2D 1
//#define USE_SUNXIFB_G2D_ROTATE 1

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#if USE_SUNXIFB
#include <stdint.h>
#include <stddef.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void sunxifb_init(uint32_t rotated);
void sunxifb_exit(void);
bool sunxifb_present(void *user_data, const void *allocation, size_t row_bytes,
    size_t height);
void sunxifb_get_sizes(uint32_t *width, uint32_t *height);
#if USE_SUNXIFB_DOUBLE_BUFFER
bool sunxifb_get_dbuf_en();
int sunxifb_set_dbuf_en(bool dbuf_en);
#endif

/**********************
 *      MACROS
 **********************/

#endif  /*USE_SUNXIFB*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*SUNXIFB_H*/
