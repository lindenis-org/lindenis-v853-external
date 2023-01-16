/*
** $Id: awcast.c 741 2009-03-31 07:16:18Z weiym $
**
** awcast.c: Sample program for MiniGUI Programming Guide
**         Load and display a bitmap.
**
** Copyright (C) 2003 ~ 2017 FMSoft (http://www.fmsoft.cn).
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define CONFIG_TLOG_LEVEL OPTION_TLOG_LEVEL_DEBUG
#include "tina_log.h"

#include "ubus.h"
#include "network_monitor.h"

#define UBUS_REPLY_EVENT_WIFI_STATUS "wifi_status_reply"

#define STR_SSID_PREFIX "SSID  : "
#define STR_SSID_UNKNOWN STR_SSID_PREFIX "Unknown"

#define STR_CONN_STATUS_PREFIX "Status: "
#define STR_CONN_STATUS_DISCONNECTED STR_CONN_STATUS_PREFIX "Disconnected"
#define STR_CONN_STATUS_CONNECTED STR_CONN_STATUS_PREFIX "Connected"
#define STR_CONN_STATUS_CONNECTEING STR_CONN_STATUS_PREFIX "Connecting"
#define STR_CONN_STATUS_DHCP STR_CONN_STATUS_PREFIX "Getting IP"

static char ssid_text [512];
static char wifi_status_text [256];

enum
{
    UBUS_POLICY_WIFI_STATUS,
    UBUS_POLICY_WIFI_SSID,
    __POLICY_ATTR_MAX
};

static const struct blobmsg_policy s_PolicyAttrs[__POLICY_ATTR_MAX] = {
    [UBUS_POLICY_WIFI_STATUS] = {.name = "wifi_status", .type = BLOBMSG_TYPE_INT32},
    [UBUS_POLICY_WIFI_SSID] = {.name = "ssid", .type = BLOBMSG_TYPE_STRING},
};

typedef enum WifiStatus
{
    WM_NETWORK_CONNECTED = 0x1,
    WM_CONNECTING,
    WM_OBTAINING_IP,
    WM_DISCONNECTED,
    WM_CONNECTED,
    WM_WIRELESS_INIT_MIRACAST_WLAN0,
    WM_WIRELESS_INIT_MIRACAST_P2P0,
    WM_STOP_MIRACAST,
} WifiStatus;

static struct Network_LinstenerS *net_linstener = NULL;

const char * network_state_txt(enum WifiStatus state)
{
    switch (state) {
        case WM_NETWORK_CONNECTED:
            return "WM_NETWORK_CONNECTED";
        case WM_CONNECTING:
            return "WM_CONNECTING";
        case WM_OBTAINING_IP:
            return "WM_OBTAINING_IP";
        case WM_DISCONNECTED:
            return "WM_DISCONNECTED";
        case WM_CONNECTED:
            return "WM_CONNECTED";
        case WM_WIRELESS_INIT_MIRACAST_WLAN0:
            return "WM_WIRELESS_INIT_MIRACAST_WLAN0";
        case WM_WIRELESS_INIT_MIRACAST_P2P0:
            return "WM_WIRELESS_INIT_MIRACAST_P2P0";
        case WM_STOP_MIRACAST:
            return "WM_STOP_MIRACAST";
        default:
            return "UNKNOWN";
    }
}

static void ubusEventHandler(struct ubus_context *ctx, struct ubus_event_handler *ev,
                             const char *type, struct blob_attr *msg)
{
    char *str;
    struct blob_attr *attr[__POLICY_ATTR_MAX];
    struct NM_EventParamS param = {{0}};

    if (!msg) {
        TLOGE("msg is null\n");
        return;
    }

    if (!strcmp(type, UBUS_REPLY_EVENT_WIFI_STATUS))
    {
        char *wifi_ssid = NULL;
        int wifi_status = 0;

        blobmsg_parse(s_PolicyAttrs, __POLICY_ATTR_MAX, attr, blob_data(msg), blob_len(msg));
        if (attr[UBUS_POLICY_WIFI_SSID])
        {
            wifi_ssid = blobmsg_data(attr[UBUS_POLICY_WIFI_SSID]);
            TLOGD("wifi ssid is %s\n", wifi_ssid);

            if(wifi_ssid){
                sprintf(param.ssid, "%s", wifi_ssid);
            }else{
                sprintf(param.ssid, "net connect successful, but ssid is unkow");
            }
        }

        if (attr[UBUS_POLICY_WIFI_STATUS])
        {
            wifi_status =  blobmsg_get_u32(attr[UBUS_POLICY_WIFI_STATUS]);
            TLOGD("wifi status is %d\n", wifi_status);
        }else{
            TLOGW("get wifi_status failed\n");
        }

        TLOGD("wifi_status --------> %s\n", network_state_txt(wifi_status));

        switch (wifi_status)
        {
            case WM_CONNECTING:
            {
                TLOGD("wifi status is WM_CONNECTING \n");
                net_linstener->notify(NM_EVENT_STATE_CONNECTING_START, &param);
                break;
            }
            case WM_OBTAINING_IP:
            case WM_CONNECTED:
            {
                TLOGD("wifi status is WM_CONNECTED \n");
                net_linstener->notify(NM_EVENT_STATE_CONNECTING, &param);
                break;
            }
            case WM_NETWORK_CONNECTED:
            {
                TLOGD("wifi status is WM_NETWORK_CONNECTED \n");
                net_linstener->notify(NM_EVENT_STATE_CONNECTED, &param);
                break;
            }
            case WM_DISCONNECTED:
            {
                TLOGD("wifi status is WM_DISCONNECTED \n");
                net_linstener->notify(NM_EVENT_STATE_DISCONNECTED, &param);
                break;
            }
            case WM_WIRELESS_INIT_MIRACAST_WLAN0:
            {
                TLOGD("wifi status is WM_WIRELESS_INIT_MIRACAST_WLAN0 \n");
                net_linstener->notify(NM_EVENT_WIRELESS_INIT_MIRACAST_WLAN0, &param);
                break;
            }
            case WM_WIRELESS_INIT_MIRACAST_P2P0:
            {
                TLOGD("wifi status is WM_WIRELESS_INIT_MIRACAST_P2P0 \n");
                net_linstener->notify(NM_EVENT_WIRELESS_INIT_MIRACAST_P2P0, &param);
                break;
            }
            case WM_STOP_MIRACAST:
            {
                TLOGD("wifi status is WM_STOP_MIRACAST \n");
                net_linstener->notify(NM_EVENT_STOP_MIRACAST, &param);
                break;
            }
            default:
                TLOGE("wifi_status(%s, %d) is unkown\n", network_state_txt(wifi_status), wifi_status);
                break;
        }

        TLOGD("net_linstener notify end --------> %s\n", network_state_txt(wifi_status));
    }else{
        TLOGE("type(%s) is unkown\n", type);
    }
    //TLOGD("status text is %s, ssid_test is %s\n", wifi_status_text, ssid_text);

    str = blobmsg_format_json(msg, true);
    //TLOGD("{ \"%s\": %s }\n", type, str);
    free(str);
}


static void *ubus_thread_func(void *arg)
{
    TLOGD("Prepare to run ubus thread.\n");
    UbusInit(NULL, ubusEventHandler);
    UbusRegisterEvent(UBUS_REPLY_EVENT_WIFI_STATUS);
    UbusRun();
    TLOGD("Ubus thread exited, aborted.\n");
}

pthread_t ubus_thread;

int NetworkMonitor_SingleInstance(struct Network_LinstenerS *l)
{
    net_linstener = l;
    pthread_create(&ubus_thread, NULL, ubus_thread_func, NULL);

    return 0;
}

