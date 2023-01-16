/*
 * @Author: your name
 * @Date: 2020-04-30 11:06:19
 * @LastEditTime: 2020-05-20 15:31:53
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /networkd/src/include/networkd_api.h
 */

#ifndef NETWORKD_API_CONFIG_H
#define NETWORKD_API_CONFIG_H
#include "ubus.h"

#define UBUS_EVENT_WIFI_CONNECT 			"wifi_conn"
#define UBUS_EVENT_WIFI_DISCONNECT 		"wifi_disconn"
#define UBUS_EVENT_WIFI_STATUS 			"wifi_status"
#define UBUS_EVENT_WIFI_SCANLIST			"wifi_scanlist"
#define UBUS_EVENT_WIFI_CONNECT_AP 		"connectap"
#define UBUS_EVENT_WIFI_GET_SCANLIST		"scanlist"
#define UBUS_EVENT_WIFI_MANAGER_INIT 	"wifi_manager_init"

#define UBUS_REPLY_EVENT_WIFI_STATUS 	"wifi_status_reply"
#define UBUS_REPLY_EVENT_WIFI_SCANLIST 	"wifi_scanlist_reply"

typedef enum NetworkType
{
    NETWORK_TYPE_UNKNOWN = 0x0,
    NETWORK_TYPE_STATION,
    NETWORK_TYPE_SOFTAP,
    NETWORK_TYPE_BLUETOOTH,
} NetworkType;

typedef struct ScanResultPointer
{
    char *results;
    int *length;
} ScanResultPointer;

typedef void (*NetworkdCallback)(NetworkType type, char *event, void *object);

int NetdApiInit(NetworkdCallback callback);
int NetdApiRun();

int NetdApiStaConnect(const char *ssid, const char *password);
int NetdApiStaScanList(char *results, int *len);
#endif
