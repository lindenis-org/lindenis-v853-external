/*
 * @Author: your name
 * @Date: 2020-05-09 11:17:23
 * @LastEditTime: 2020-05-20 17:00:27
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /networkd/src/networkd_api.c
 */

#include "stdio.h"

#include "ubus.h"
#include "networkd_api.h"
#include "log.h"
static NetworkdCallback g_mCallback = NULL;
static void ubusEventHandler(struct ubus_context *ctx, struct ubus_event_handler *ev,
                             const char *type, struct blob_attr *msg)
{
    char *str;
    //struct blob_attr *attr[__POLICY_ATTR_MAX];
    NetworkType netType = NETWORK_TYPE_UNKNOWN;
    //void *object = NULL;

	TLOGI("%s:%d: ---------------\n", __func__, __LINE__);

    if (!msg)
        return;

    if (!strcmp(type, UBUS_REPLY_EVENT_WIFI_STATUS))
    {
       netType = NETWORK_TYPE_STATION;
    } else if (!strcmp(type, UBUS_REPLY_EVENT_WIFI_SCANLIST))
    {
       netType = NETWORK_TYPE_STATION;
    }

    str = blobmsg_format_json(msg, true);
    TLOGD("{ \"%s\": %s }\n", type, str);
    if (g_mCallback != NULL)
        g_mCallback(netType, (char *)type, str);

    free(str);
}

int NetdApiInit(NetworkdCallback callback)
{
    if (!UbusIsInited()) {
        UbusInit(NULL, ubusEventHandler);
        UbusRegisterEvent(UBUS_REPLY_EVENT_WIFI_STATUS);
        g_mCallback = callback;
    }
    return 0;
}

int NetdApiRun()
{
    return UbusRun();
}

int NetdApiSendEvent()
{
	return 0;
}

int NetdApiStaConnect(const char *ssid, const char *password)
{
    static struct blob_buf sBuffer;
    uint32_t id;
    int ret;
    blob_buf_init(&sBuffer, 0);
    blobmsg_add_string(&sBuffer, "wifi_ssid", ssid);
    blobmsg_add_string(&sBuffer, "wifi_password", password);
    ret = UBusLookupId("wifimanager", &id);
    if (ret != UBUS_STATUS_OK)
    {
        TLOGI("lookup wifimanager failed, check the networkd service is running.\n");
        return ret;
    }

    ret = UBusInvoke(id, UBUS_EVENT_WIFI_CONNECT_AP, sBuffer.head, NULL, NULL, 3 * 1000);
    blob_buf_free(&sBuffer);

    return ret;
}

int StaDisConnect()
{
	return 0;
}

enum {
	SCANLIST_RESULT,
	SCANLIST_LENGTH,
	SCANLIST_POLICY_MAX,
};

static const struct blobmsg_policy scanlistPolicy[SCANLIST_POLICY_MAX] = {
    [SCANLIST_RESULT] = {.name = "scan_results", .type = BLOBMSG_TYPE_STRING},
    [SCANLIST_LENGTH] = {.name = "length", .type = BLOBMSG_TYPE_INT32},
};

static void scanlistUbusCallback(struct ubus_request *req, int type, struct blob_attr *msg)
{
    char *str;
    int len = 0;
    struct blob_attr *attr[SCANLIST_POLICY_MAX];
    ScanResultPointer *pointer = req->priv;
    blobmsg_parse(scanlistPolicy, SCANLIST_POLICY_MAX, attr, blob_data(msg), blob_len(msg));
    if (attr[SCANLIST_RESULT])
    {
        str = blobmsg_data(attr[SCANLIST_RESULT]);
    }
    if (attr[SCANLIST_LENGTH])
    {
        len = blobmsg_get_u32(attr[SCANLIST_LENGTH]);
    }
    if (len >= 0)
    {
        strcpy(pointer->results, str);
        *pointer->length = len;
    }

	TLOGI("%s:%d: ---------------\n", __func__, __LINE__);
}

int NetdApiStaScanList(char *results, int *len)
{
    static struct blob_buf sBuffer;
    uint32_t id = 0;
    int ret = -1;
    ScanResultPointer pointer;

	TLOGI("%s:%d: ---------------\n", __func__, __LINE__);

    ret = UBusLookupId("wifimanager", &id);
    if (ret != UBUS_STATUS_OK)
    {
        TLOGI("lookup wifimanager failed, check the networkd service is running.\n");
        return ret;
    }
    pointer.results = results;
    pointer.length = len;
    ret = UBusInvoke(id, UBUS_EVENT_WIFI_GET_SCANLIST, sBuffer.head, scanlistUbusCallback, &pointer, 10 * 1000);
    blob_buf_free(&sBuffer);
    return ret;
}

