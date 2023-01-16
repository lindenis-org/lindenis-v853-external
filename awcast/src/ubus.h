/*
 * @Author: Wink
 * @Date: 2020-04-14 14:19:04
 * @LastEditTime: 2020-05-12 16:35:22
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /networkd/src/include/ubus.h
 */

#ifndef NETWORKD_UBUS_CONFIG_H
#define NETWORKD_UBUS_CONFIG_H

#include <libubox/uloop.h>
#include <libubox/ustream.h>
#include <libubox/utils.h>
#include <libubus.h>
#include <json-c/json.h>
#include <libubox/blobmsg_json.h>

int UbusRegisterEvent(const char *event);
int UBusSendEvent(const char *id, struct blob_attr *data);
bool UBusIsRunning();
bool UbusIsInited();
int UbusInit(const char *path, ubus_event_handler_t ubus_msg_cb);
int UbusRun();
#endif
