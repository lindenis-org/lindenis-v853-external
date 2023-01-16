/*
 * @Author: Wink
 * @Date: 2020-04-13 17:16:54
 * @LastEditTime: 2020-05-20 17:09:16
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /networkd/src/main.c
 */
#include <stdio.h>
#include "log.h"
#include "networkd.h"
#include "ubus.h"
#include "wifi_manager.h"

enum
{
    UBUS_POLICY_WIFI_SSID,
    UBUS_POLICY_WIFI_PASSWORD,
    __POLICY_ATTR_MAX
};

static const struct blobmsg_policy s_PolicyAttrs[__POLICY_ATTR_MAX] = {
    [UBUS_POLICY_WIFI_SSID] = {.name = "wifi_ssid", .type = BLOBMSG_TYPE_STRING},
    [UBUS_POLICY_WIFI_PASSWORD] = {.name = "wifi_password", .type = BLOBMSG_TYPE_STRING},
};



static void ubusEventHandler(struct ubus_context *ctx, struct ubus_event_handler *ev,
                                   const char *type, struct blob_attr *msg)
{
    char *str;
    struct blob_attr *attr[__POLICY_ATTR_MAX];
    // char mqtt_topic[MQTTD_MAX_SVC_NAME_LEN + 1];
    if (!msg)
        return;

	TLOGI("%s:%d: ------type=%s---------\n", __func__, __LINE__, type);

    if (!strcmp(type, UBUS_EVENT_WIFI_CONNECT))
    {
        char *wifi_ssid = NULL;
        char *wifi_password = NULL;

        blobmsg_parse(s_PolicyAttrs, __POLICY_ATTR_MAX, attr, blob_data(msg), blob_len(msg));
        if (attr[UBUS_POLICY_WIFI_SSID])
        {
            wifi_ssid = blobmsg_data(attr[UBUS_POLICY_WIFI_SSID]);
            TLOGD("wifi ssid is %s\n", wifi_ssid);
        }

        if (attr[UBUS_POLICY_WIFI_PASSWORD])
        {
            wifi_password = blobmsg_data(attr[UBUS_POLICY_WIFI_PASSWORD]);
            TLOGD("wifi password is %s\n", wifi_password);
        }

        if (wifi_ssid && wifi_password)
        {
            TLOGD("Connecting AP = %s, password = %s\n", wifi_ssid, wifi_password);
            WifiConnectAp(wifi_ssid, wifi_password);
        }
    }
    else if (!strcmp(type, UBUS_EVENT_WIFI_SCANLIST))
    {
        char scan_results[4096] = {0};
        int results_len = 4096;

        if (WifiScanResults(scan_results, &results_len) == 0) {
            static struct blob_buf buf;
            memset(&buf, 0, sizeof(buf));

            blobmsg_buf_init(&buf);
            blobmsg_add_string(&buf, "scanlist", scan_results);
            UBusSendEvent(UBUS_REPLY_EVENT_WIFI_SCANLIST, buf.head);
            blob_buf_free(&buf);
        }else{
	        TLOGD("err: WifiScanResults failed\n");
        }

    }
    str = blobmsg_format_json(msg, true);
    TLOGD("{ \"%s\": %s }\n", type, str);
    free(str);
}

static int is_wifi_password_exist(char *file)
{
	//char *file = "/etc/wifi/wpa_supplicant.conf";
	char *ssid = "sid=";
	char *password = "psk=";
    char line[1024];
    FILE *fp = NULL;
	int ssid_found = 0;
	int password_found = 0;

    fp = fopen(file,"r");
    if(fp == NULL){
		TLOGE("err: fopen failed");
        return 0;
    }

    while (fgets(line, 1023, fp)){
        if(strstr(line, ssid)){
			ssid_found = 1;
        }

		if(strstr(line, password)){
			password_found = 1;
        }
    }

	fclose(fp);

    return (ssid_found & password_found);
}

static int is_softap_ready(void)
{
    FILE* fp=NULL;
    char command_hostapd[] = "ps | grep \"hostapd\" | grep -v \"grep\" | awk 'NR==1{print $1}'";
    char command_dnsmasq[] = "ps | grep \"dnsmasq\" | grep -v \"grep\" | awk 'NR==1{print $1}'";
    char buf[1024] = {0};
    int hostapd_exist = 0;
    int dnsmasq_exist = 0;
    int ready = 1;

    fp = popen(command_hostapd, "r");
    if(fp == NULL){
        TLOGE("%s:%d: popen command_hostapd failed");
        return 1;
    }

    if(fgets(buf, sizeof(buf), fp) == NULL){
        hostapd_exist = 0;
    }else{
        hostapd_exist = 1;
    }

    fp = popen(command_dnsmasq, "r");
    if(fp == NULL){
        TLOGE("%s:%d: popen command_dnsmasq failed");
        return 1;
    }

    if(fgets(buf, sizeof(buf), fp) == NULL){
        dnsmasq_exist = 0;
    }else{
        dnsmasq_exist = 1;
    }

    TLOGI("hostapd_exist=%d, dnsmasq_exist=%d\n", hostapd_exist, dnsmasq_exist);

    if(hostapd_exist && dnsmasq_exist){
        ready = 1;
    }else{
        ready = 0;
    }

    return ready;
}

int main (void)
{
	int ret = 0;
	int wifi_on = 0;

    TLOGI("Network Daemon Starting....\n");

    UbusInit(NULL, ubusEventHandler);
	if (UbusIsInited() == false)
	{
		TLOGE("NEED TO INIT UBUS FIRSET!\n");
	}

	wifiRegisterServices();

	if(is_wifi_password_exist("/etc/wifi/wpa_supplicant.conf")
        || is_wifi_password_exist("/etc/wifi/wpa_supplicant_p2p.conf")){
		wifi_on = 1;
		disable_softap();
	}else{
        disable_softap();
        enable_softap();
	}

	ret = WifiInit(wifi_on);
	if(ret != 0){
		TLOGD("err: WifiInit failed\n");
		return -1;
	}

    UbusRun();

    return 0;
}
