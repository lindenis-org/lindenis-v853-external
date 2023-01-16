/*
 * @Author: Wink
 * @Date: 2020-04-14 10:01:09
 * @LastEditTime: 2020-04-14 13:56:47
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /networkd/src/include/log.h
 */
#ifndef NET_KEY_H
#define NET_KEY_H

#define WIRELESS_DISPLAY_MODE_UNKOWN 	0
#define WIRELESS_DISPLAY_MODE_DLNA 		1
#define WIRELESS_DISPLAY_MODE_MIRACAST 	2

int net_key_init(void);
int net_key_exit(void);

#endif
