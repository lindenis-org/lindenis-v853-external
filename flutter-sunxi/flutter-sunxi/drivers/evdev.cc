// Copyright 2021 The Allwinnertech. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 *
 * @file evdev.cc
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "evdev.h"
#if USE_EVDEV != 0

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>
#include <stdint.h>

/*********************
 *      DEFINES
 *********************/
#define DEVPREFIX "/dev/input/event"
#define LABEL(constant) { #constant, constant }
#define LABEL_END { NULL, -1 }

/**********************
 *      TYPEDEFS
 **********************/
struct label {
  const char *name;
  int value;
};

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int find_input_props(int fd);

/**********************
 *  STATIC VARIABLES
 **********************/
static int evdev_fd;
static struct label input_prop_labels[] = {
  LABEL(INPUT_PROP_POINTER),
  LABEL(INPUT_PROP_DIRECT),
  LABEL(INPUT_PROP_BUTTONPAD),
  LABEL(INPUT_PROP_SEMI_MT),
  LABEL_END,
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
#if 0
#include <sys/socket.h>
#include <linux/netlink.h>

#define DISP_VSYNC_EVENT_EN 0x0B

#define UEVENT_MSG_LEN  2048

int uevent_init(void) {
  int dispfd, ret;

  dispfd = open("/dev/disp", O_RDWR);
  if (dispfd == -1) {
    perror("flutter: unable open disp interface:");
    return -1;
  }

  unsigned long ioctlParam[4] = { 0, 1 };
  if (ioctl(dispfd, DISP_VSYNC_EVENT_EN, ioctlParam) < 0) {
    perror("flutter: DISP_VSYNC_EVENT_EN fail");
    return -1;
  }

  struct sockaddr_nl addr;
  int sz = 64 * 1024;

  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;
  addr.nl_pid = getpid();
  addr.nl_groups = 0xffffffff;

  int socket_fd = -1;
  socket_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
  if (socket_fd < 0) {
    return -1;
  }

  setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));

  if (bind(socket_fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
    close(socket_fd);
    return -1;
  }

  printf("flutter: socket_fd = %d\n", socket_fd);

  int recvlen;
  char msg[UEVENT_MSG_LEN + 2];

  if ((recvlen = recv(socket_fd, msg, UEVENT_MSG_LEN, 0)) > 0) {
    if (recvlen > 0 && recvlen < UEVENT_MSG_LEN) {
      msg[recvlen] = '\0';
      msg[recvlen + 1] = '\0';

      printf("%s\n", msg);
    }
  }

  return 0;
}
#endif

/**
 * Initialize the evdev interface
 */
void evdev_init(void) {
  int i = 0, event_id = 0;

  if (access("/dev/input/touchscreen", F_OK) != 0) {
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

      printf("flutter: found input device <%s>\n", name);
      if (find_input_props(fd) == 2) {
        event_id = i;
      }

      close(fd);
    } while (++i);

    sprintf(ibuf, "%s%i", DEVPREFIX, event_id);
    evdev_fd = open(ibuf, O_RDWR | O_NOCTTY | O_NDELAY);
  } else {
    printf("flutter: found input device </dev/input/touchscreen>\n");
    evdev_fd = open("/dev/input/touchscreen", O_RDWR | O_NOCTTY | O_NDELAY);
  }

  if (evdev_fd == -1) {
    perror("flutter: unable open evdev interface:");
    return;
  }

  fcntl(evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
}

/**
 * reconfigure the device file for evdev
 * @param dev_name set the evdev device filename
 * @return true: the device file set complete
 *         false: the device file doesn't exist current system
 */
bool evdev_set_file(char *dev_name) {
  if (evdev_fd != -1) {
    close(evdev_fd);
  }

  evdev_fd = open(dev_name, O_RDWR | O_NOCTTY | O_NDELAY);

  if (evdev_fd == -1) {
    perror("flutter: unable open evdev interface:");
    return false;
  }

  fcntl(evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);

  return true;
}

/**
 * Get the current position and state of the evdev
 * @param x The original x value reported by tp
 * @param y The original y value reported by tp
 * @param button Press or release
 * @param report Do you need to report the coordinates
 * @return true: because the points are not buffered, so no more data to be read
 */
bool evdev_read(double *x, double *y, int *button, bool *report) {
  struct input_event in;
  static double evdev_x;
  static double evdev_y;
  static int evdev_button;

  *report = false;

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

    if (in.code == SYN_REPORT) {
      *report = true;
      *x = evdev_x;
      *y = evdev_y;
      *button = evdev_button;
    }
  }

  return true;
}

/**
 * Debounce one pixel
 * @param old_x Old x coordinate
 * @param old_y Old y coordinate
 * @param x New x coordinate
 * @param y New y coordinate
 * @return true: The difference between x and y is within one pixel
 */
bool evdev_debounce(double old_x, double old_y, double x, double y) {
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

/**********************
 *   STATIC FUNCTIONS
 **********************/
static const char* get_label(const struct label *labels, int value) {
  while (labels->name && value != labels->value) {
    labels++;
  }
  return labels->name;
}

static int find_input_props(int fd) {
  uint8_t bits[INPUT_PROP_CNT / 8];
  int i, j, res, count, ret;
  const char *bit_label;

  printf("flutter: input props:");
  res = ioctl(fd, EVIOCGPROP(sizeof(bits)), bits);
  if (res < 0) {
    printf(" <not available>\n");
    return 1;
  }

  ret = 0;
  count = 0;
  for (i = 0; i < res; i++) {
    for (j = 0; j < 8; j++) {
      if (bits[i] & 1 << j) {
        bit_label = get_label(input_prop_labels, i * 8 + j);
        if (bit_label) {
          printf(" %s", bit_label);
          /* INPUT_PROP_DIRECT is a touch device */
          if (i * 8 + j == input_prop_labels[1].value)
            ret = 2;
        } else
          printf(" %04x", i * 8 + j);
        count++;
      }
    }
  }

  if (!count)
    printf(" <none>\n");
  else
    printf("\n");
  return ret;
}

#endif
