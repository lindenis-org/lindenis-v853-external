// Copyright 2021 The Allwinnertech. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @file evdev.h
 *
 */

#ifndef EVDEV_H
#define EVDEV_H

#define USE_EVDEV 1

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#if USE_EVDEV

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
 * Initialize the evdev
 */
void evdev_init(void);

/**
 * reconfigure the device file for evdev
 * @param dev_name set the evdev device filename
 * @return true: the device file set complete
 *         false: the device file doesn't exist current system
 */
bool evdev_set_file(char *dev_name);

/**
 * Get the current position and state of the evdev
 * @param x The original x value reported by tp
 * @param y The original y value reported by tp
 * @param button Press or release
 * @param report Do you need to report the coordinates
 * @return true: because the points are not buffered, so no more data to be read
 */
bool evdev_read(double *x, double *y, int *button, bool *report);

/**
 * Debounce one pixel
 * @param old_x Old x coordinate
 * @param old_y Old y coordinate
 * @param x New x coordinate
 * @param y New y coordinate
 * @return true: The difference between x and y is within one pixel
 */
bool evdev_debounce(double old_x, double old_y, double x, double y);

/**********************
 *      MACROS
 **********************/

#endif /* USE_EVDEV */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EVDEV_H */
