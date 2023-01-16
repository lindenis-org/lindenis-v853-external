/*
 * @Author: Wink
 * @Date: 2020-04-14 14:19:04
 * @LastEditTime: 2020-05-13 16:37:34
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
#include "networkd_api.h"

int UbusRegisterService(struct ubus_object *method);
int UbusRegisterEvent(const char *event);
int UBusSendEvent(const char *id, struct blob_attr *data);
int UBusLookupId(const char *path, uint32_t *id);
int UBusInvoke(uint32_t obj, const char *method, struct blob_attr *msg, ubus_data_handler_t callback,
                                                                void *priv, int timeout);
bool UbusIsInited();
bool UBusIsRunning();
int UbusInit(const char *path, ubus_event_handler_t ubus_msg_cb);
int UbusRun();
#endif
