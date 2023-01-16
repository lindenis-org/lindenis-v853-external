#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <wifi_intf.h>
#include <pthread.h>
#include "wmg_debug.h"
#include "wifi_udhcpc.h"
#include "log.h"
#include "ubus.h"
#include "networkd_api.h"
#include "wifi_manager.h"

struct wifi_manager{
	WifiManager WifiManager;
	aw_wifi_interface_t *WifiInterface;

	pthread_t thread_pid;
	pthread_mutex_t thread_lock;
	char thread_run;
	char thread_stop;
};

static struct wifi_manager g_wifi_manager;

typedef enum
{
	CONNECTAP_SSID,
	CONNECTAP_PASSWORD,
	CONNECTAPI_POLICY_MAX,
} PolicyConnList;

static const struct blobmsg_policy connectap_policy[CONNECTAPI_POLICY_MAX] = {
	[CONNECTAP_SSID] = {.name = "wifi_ssid", .type = BLOBMSG_TYPE_STRING},
	[CONNECTAP_PASSWORD] = {.name = "wifi_password", .type = BLOBMSG_TYPE_STRING},
};

static void *thread_get_ssid(void *arg);
static int networkd_wifi_on(void);
static int networkd_wifi_off(void);

static int wmServiceScanList(struct ubus_context *ctx, struct ubus_object *obj,
		      struct ubus_request_data *req, const char *method,
		      struct blob_attr *msg)
{
	static struct blob_buf sBuffer;
	char results[4096] = {0};
	int len = 4096;

	TLOGI("%s:%d: ---------------\n", __func__, __LINE__);

	blob_buf_init(&sBuffer, 0);

	WifiScanResults(results, &len);

	blobmsg_add_string(&sBuffer, "scan_results", results);
	blobmsg_add_u32(&sBuffer, "length", len);
	ubus_send_reply(ctx, req, sBuffer.head);

	blob_buf_free(&sBuffer);

	return 0;
}

static int wmServiceConnectAp(struct ubus_context *ctx, struct ubus_object *obj,
		      struct ubus_request_data *req, const char *method,
		      struct blob_attr *msg)
{
	static struct blob_buf sBuffer;
	struct blob_attr *attr[CONNECTAPI_POLICY_MAX];
	char *wifi_ssid = NULL;
	char *wifi_password = NULL;
	int ret = 0;

	disable_softap();

	ret = networkd_wifi_on();
	if(ret != 0){
		TLOGD("err: networkd_wifi_on failed\n");
		return -1;
	}

	blob_buf_init(&sBuffer, 0);

	blobmsg_parse(connectap_policy, CONNECTAPI_POLICY_MAX, attr, blob_data(msg), blob_len(msg));
	if (attr[CONNECTAP_SSID])
	{
		wifi_ssid = blobmsg_data(attr[CONNECTAP_SSID]);
		TLOGD("wifi ssid is %s\n", wifi_ssid);
	}

	if (attr[CONNECTAP_PASSWORD])
	{
		wifi_password = blobmsg_data(attr[CONNECTAP_PASSWORD]);
		TLOGD("wifi password is %s\n", wifi_password);
	}

	if (wifi_ssid && wifi_password)
	{
		TLOGD("Connecting AP = %s, password = %s\n", wifi_ssid, wifi_password);
		ret = WifiConnectAp(wifi_ssid, wifi_password);
        if(ret){
            TLOGD("WifiConnectAp ret = %d", ret);
            networkd_wifi_off();
            enable_softap();
        }
	}

	blob_buf_free(&sBuffer);

	return 0;
}

static struct ubus_method s_WmMethods[] =
{
	UBUS_METHOD_NOARG(UBUS_EVENT_WIFI_GET_SCANLIST, wmServiceScanList),
	UBUS_METHOD(UBUS_EVENT_WIFI_CONNECT_AP, wmServiceConnectAp, connectap_policy),
};

static struct ubus_object_type s_WmObjType = UBUS_OBJECT_TYPE("wifimanager", s_WmMethods);

static struct ubus_object wm_obj =
	{
		.name = "wifimanager",
		.type = &s_WmObjType,
		.methods = s_WmMethods,
		.n_methods = ARRAY_SIZE(s_WmMethods),
};

const char * wifi_state_txt(enum wmgState state)
{
	switch (state) {
		case DISCONNECTED:
			return "DISCONNECTED";
		case CONNECTING:
			return "CONNECTING";
		case CONNECTED:
			return "CONNECTED";
		case OBTAINING_IP:
			return "OBTAINING_IP";
		case NETWORK_CONNECTED:
			return "NETWORK_CONNECTED";
		default:
			return "UNKNOWN";
	}
}

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

static void wifi_state_handle(struct Manager *w, int event_label)
{
	static struct blob_buf sBuffer;

	blob_buf_init(&sBuffer, 0);

	TLOGD("w->StaEvt.state=0x%x\n", w->StaEvt.state);
	TLOGD("wifi_state_handle-------------> %s\n", wifi_state_txt(w->StaEvt.state));

	switch(w->StaEvt.state){
		case CONNECTING:
			g_wifi_manager.WifiManager.nStatus = WM_CONNECTING;
		break;

		case CONNECTED:
			if(w->ssid && w->ssid[0] != '\0'){
				TLOGI("CONNECTED: ssid=%s\n", w->ssid);
				memset(g_wifi_manager.WifiManager.sCurSSID, 0, MAX_SSID_LEN);
				strcpy(g_wifi_manager.WifiManager.sCurSSID, w->ssid);
			}
			g_wifi_manager.WifiManager.nStatus = WM_CONNECTED;
			start_udhcpc();
		return;

		case OBTAINING_IP:
			g_wifi_manager.WifiManager.nStatus = WM_OBTAINING_IP;
		break;

		case NETWORK_CONNECTED:
			g_wifi_manager.WifiManager.nStatus = WM_NETWORK_CONNECTED;
#if 0
			if(w->ssid && w->ssid[0] != '\0'){
				TLOGI("CONNECTED: ssid=%s\n", w->ssid);
				memset(g_wifi_manager.WifiManager.sCurSSID, 0, MAX_SSID_LEN);
				strcpy(g_wifi_manager.WifiManager.sCurSSID, w->ssid);
			}

			if(g_wifi_manager.WifiManager.sCurSSID[0] != '\0'){
				TLOGI("Successful network connection(%s)\n", g_wifi_manager.WifiManager.sCurSSID);
				blobmsg_add_string(&sBuffer, "ssid", g_wifi_manager.WifiManager.sCurSSID);
			}else{
				TLOGE("err: ---NETWORK_CONNECTED--ssid is null\n");
				if ((pthread_create(&g_wifi_manager.thread_pid, NULL, thread_get_ssid, NULL) == 0)) {
					TLOGE("---NETWORK_CONNECTED--thread_get_ssid----\n");
					blob_buf_free(&sBuffer);
					return;
				}
			}
#else
            memset(g_wifi_manager.WifiManager.sCurSSID, 0, MAX_SSID_LEN);
            if ((pthread_create(&g_wifi_manager.thread_pid, NULL, thread_get_ssid, NULL) == 0)) {
                TLOGE("---NETWORK_CONNECTED--thread_get_ssid----\n");
                blob_buf_free(&sBuffer);
                return;
            }
#endif
		break;

        case DISCONNECTED:
            g_wifi_manager.WifiManager.nStatus = WM_DISCONNECTED;
            TLOGI("Disconnected,the reason:%s\n", wmg_event_txt(w->StaEvt.event));

            memset(g_wifi_manager.WifiManager.sCurSSID, 0, MAX_SSID_LEN);

            switch (w->StaEvt.event) {
                case WSE_NETWORK_NOT_EXIST:
                    strcpy(g_wifi_manager.WifiManager.sCurSSID, "network not found");
                break;

                case WSE_PASSWORD_INCORRECT:
                    strcpy(g_wifi_manager.WifiManager.sCurSSID, "password error");
                break;

                case WSE_OBTAINED_IP_TIMEOUT:
                    strcpy(g_wifi_manager.WifiManager.sCurSSID, "obtained ip timeout");
                break;

                case WSE_CONNECTED_TIMEOUT:
                    strcpy(g_wifi_manager.WifiManager.sCurSSID, "connected timeout");
                break;

                case WSE_AUTO_DISCONNECTED:
                    strcpy(g_wifi_manager.WifiManager.sCurSSID, "network auto disconnected");
                break;

                case WSE_AP_ASSOC_REJECT:
                    strcpy(g_wifi_manager.WifiManager.sCurSSID, "ap assoc reject");
                break;
            }

            if(g_wifi_manager.WifiManager.sCurSSID[0] != '\0'){
                TLOGI("network disconnection(%s)\n", g_wifi_manager.WifiManager.sCurSSID);
                blobmsg_add_string(&sBuffer, "ssid", g_wifi_manager.WifiManager.sCurSSID);
            }
		break;

		default:
			TLOGI("unkown network state:%s, %d\n",wmg_event_txt(w->StaEvt.event), w->StaEvt.event);
			//g_wifi_manager.WifiManager.nStatus = WM_STATE_UNKNOWN;
    }

	blobmsg_add_u32(&sBuffer, "wifi_status", g_wifi_manager.WifiManager.nStatus);

	TLOGD("UBusSendEvent: nStatus=%s, sCurSSID=%s\n",
	network_state_txt(g_wifi_manager.WifiManager.nStatus), g_wifi_manager.WifiManager.sCurSSID);
	UBusSendEvent(UBUS_REPLY_EVENT_WIFI_STATUS, sBuffer.head);

	blob_buf_free(&sBuffer);
}

int wifiRegisterServices(void)
{
	return UbusRegisterService(&wm_obj);
}

int WifiConnectAp(const char *ssid, const char *password)
{
	int ret = -1;

	if (!g_wifi_manager.WifiInterface)
	{
		TLOGE("WifiManager is not init yet.\n");
		return -1;
	}

	if (!ssid && !password)
	{
		TLOGW("SSID or Password can not be null.\n");
		return -1;
	}

	if (strlen(ssid) > MAX_SSID_LEN)
	{
		TLOGW("SSID too long.\n");
		return -1;
	}
	strcpy(g_wifi_manager.WifiManager.sCurSSID, ssid);

	if(g_wifi_manager.WifiInterface->connect_ap){
		ret = g_wifi_manager.WifiInterface->connect_ap(ssid, password,g_wifi_manager.WifiManager.nEventLable);
		if(ret < 0){
            TLOGW("err: connect_ap failed, ret=%d\n", ret);
            return -1;
		}
	}

	return 0;
}

static int wifi_device_connect(void)
{
    char file[] = "/sys/class/net/wlan0/address";
	int fd = 0;

	fd = open(file, O_RDONLY);
	if(fd < 0) {
		TLOGD("could not open %s, %s\n", file, strerror(errno));
		return 0;
	}

	return 1;
}

static int try_wifi_on(void)
{
    int time = 0;

    g_wifi_manager.thread_run = 1;

	while(!g_wifi_manager.thread_stop && g_wifi_manager.WifiInterface == NULL && time < 10){
		if(wifi_device_connect()){
			g_wifi_manager.WifiManager.nEventLable = rand();
			g_wifi_manager.WifiInterface = (aw_wifi_interface_t *)aw_wifi_on(wifi_state_handle, g_wifi_manager.WifiManager.nEventLable);
			if(g_wifi_manager.WifiInterface == NULL){
				TLOGE("err: aw_wifi_on failed, try agin\n");
			}else{
				TLOGD("WifiInit, nEventLable is 0x%08x, WifiInterface is 0x%08x\n",
					g_wifi_manager.WifiManager.nEventLable, g_wifi_manager.WifiInterface);
				return 0;
			}
		}else{
			TLOGD("wifi device is disconnect, end try\n");
			break;
		}

		sleep(1);
		time++;
	}

    g_wifi_manager.thread_run = 0;

    return -1;
}

#if 0
static int get_ssid(void)
{
	struct wifi_status apStatus;
	static struct blob_buf sBuffer;
	int ret = 0;

    //get ssid and send message
	blob_buf_init(&sBuffer, 0);

	if (g_wifi_manager.WifiInterface && g_wifi_manager.WifiInterface->get_status) {
		ret = g_wifi_manager.WifiInterface->get_status(&apStatus);
		if(ret){
			TLOGW("err: get_status failed, ret=%d\n", ret);
			return -1;
		}

		memset(g_wifi_manager.WifiManager.sCurSSID, 0, MAX_SSID_LEN);
		strcpy(g_wifi_manager.WifiManager.sCurSSID, apStatus.ssid);

		if(g_wifi_manager.WifiManager.sCurSSID[0] != '\0'){
			TLOGI("Successful network connection(%s)\n", g_wifi_manager.WifiManager.sCurSSID);
			blobmsg_add_string(&sBuffer, "ssid", g_wifi_manager.WifiManager.sCurSSID);
		}else{
			TLOGE("err: ssid is null\n");
		}
	}else{
		TLOGE("err: parament is null\n");
	}

	blobmsg_add_u32(&sBuffer, "wifi_status", g_wifi_manager.WifiManager.nStatus);

	UBusSendEvent(UBUS_REPLY_EVENT_WIFI_STATUS, sBuffer.head);

	blob_buf_free(&sBuffer);

    return 0;
}
#else
static int get_ssid(void)
{
	FILE* fp=NULL;
	char command[] = "iw dev wlan0 link";
	char buf[1024] = {0};
	char *q = NULL;
	char vlaue[256] = {0};

	static struct blob_buf sBuffer;

	fp = popen(command, "r");
	if(fp == NULL){
		TLOGE("%s:%d: popen failed");
		return 0;
	}

	while(fgets(buf, sizeof(buf), fp) != NULL){
		q = strstr(buf, "SSID:");
		if(q != NULL){
			char *temp_str= NULL;
			int i = 0, l = 0;

			//sscanf(buf, "%*s%255s", vlaue);
			temp_str = strchr(buf, ':');

			/* trim :*/
			temp_str++;

			/* trim left space */
			l = strlen(temp_str);
			while (isspace(temp_str[i]) && (i <= l))
				i++;
			temp_str = temp_str + i;
			TLOGD("temp_str=%s", temp_str);

			/* trim right enter */
			l = strlen(temp_str);
			if(temp_str[l -1] == '\n'){
				temp_str[l -1] = 0;
			}

			strncpy(vlaue, temp_str, 255);
			if(vlaue[0] != '\0'){
				memset(g_wifi_manager.WifiManager.sCurSSID, 0, MAX_SSID_LEN);
				strncpy(g_wifi_manager.WifiManager.sCurSSID, vlaue, (MAX_SSID_LEN-1));

				blob_buf_init(&sBuffer, 0);
				blobmsg_add_string(&sBuffer, "ssid", g_wifi_manager.WifiManager.sCurSSID);
				blobmsg_add_u32(&sBuffer, "wifi_status", g_wifi_manager.WifiManager.nStatus);
				UBusSendEvent(UBUS_REPLY_EVENT_WIFI_STATUS, sBuffer.head);

				break;
			}
		}
	}

    return 0;
}
#endif

static void *thread_get_ssid(void *arg)
{
	enum wmgState ws;

	while(1) {
		ws = aw_wifi_get_wifi_state();
		//TLOGD("thread_get_ssid: wifi_state is %s", wifi_state_txt(ws));
		if(ws == NETWORK_CONNECTED){
			TLOGD("----------------NETWORK_CONNECTED------------------\n");
			if(g_wifi_manager.WifiManager.sCurSSID[0] == '\0'){
				TLOGD("get ssid\n");
				get_ssid();
			}

			break;
		}

		sleep(1);
	}

	return NULL;
}

static int networkd_wifi_on(void)
{
	if(g_wifi_manager.WifiInterface){
		TLOGE("err: wifi is already init\n");
		return 0;
	}

	system("source wifi_manager_init.sh &");
	sleep(1);

	g_wifi_manager.WifiManager.nEventLable = rand();
	g_wifi_manager.WifiInterface = (aw_wifi_interface_t *)aw_wifi_on(wifi_state_handle, g_wifi_manager.WifiManager.nEventLable);
	if(g_wifi_manager.WifiInterface == NULL){
		TLOGE("err: aw_wifi_on failed\n");

		if(try_wifi_on()){
			TLOGE("err: try aw_wifi_on failed\n");
			return -1;
		}
	}

	/*
	if(g_wifi_manager.WifiManager.sCurSSID[0] == '\0'){
		if ((pthread_create(&g_wifi_manager.thread_pid, NULL, thread_get_ssid, NULL) != 0)) {
			TLOGE("pthread_create failed, %s!\n", strerror(errno));
			return 1;
		}
	}
	*/

	TLOGD("WifiInit, nEventLable is 0x%08x, WifiInterface is 0x%08x\n",
		g_wifi_manager.WifiManager.nEventLable, g_wifi_manager.WifiInterface);

	return 0;
}

static int networkd_wifi_off(void)
{
    int ret = 0;
    int time = 100;

    g_wifi_manager.thread_stop = 1;
    while(g_wifi_manager.thread_run && time--){
        usleep(10 * 100);
        TLOGI("wait for try_wifi_on thread off, time=%d\n", time);
    }

    if(time == 0){
        TLOGE("wait for try_wifi_on thread off timeout\n");
		return -1;
    }

    if(g_wifi_manager.WifiInterface){
        ret = aw_wifi_off(g_wifi_manager.WifiInterface);
        if(ret){
            TLOGE("err: aw_wifi_off failed\n");
            return -1;
        }
        g_wifi_manager.WifiInterface = NULL;
    }

    return 0;

}

int WifiInit(int wifi_on)
{
	TLOGI("%s:%d: ---------------\n", __func__, __LINE__);

	memset(&g_wifi_manager, 0, sizeof(struct wifi_manager));

	if(wifi_on){
		return networkd_wifi_on();
	}else{
		return 0;
	}
}

int WifiScanResults(char *results, int *len)
{
    int ret = -1;

	if(g_wifi_manager.WifiInterface && g_wifi_manager.WifiInterface->get_scan_results){
		ret = g_wifi_manager.WifiInterface->get_scan_results(results, len);
		if(ret != 0){
            TLOGW("err: get_scan_results failed, ret=%d\n", ret);
		}
	}else{
		TLOGW("wifi is not ready, g_wifi_manager.WifiInterface=0x%x\n", g_wifi_manager.WifiInterface);
		if(g_wifi_manager.WifiInterface){
			TLOGW("wifi is not ready, g_wifi_manager.WifiInterface->get_scan_results=0x%x\n", g_wifi_manager.WifiInterface->get_scan_results);
		}
	}

	return ret;
}

int WifiCleanAllNetworkConfig(void)
{
	int ret = 0;

	if (g_wifi_manager.WifiInterface && g_wifi_manager.WifiInterface->remove_all_networks) {
		ret = g_wifi_manager.WifiInterface->remove_all_networks();
		if(ret){
			TLOGW("err: remove_all_networks failed, ret=%d\n", ret);
			return -1;
		}
	}else{
		TLOGE("err: parament is null\n");
		return -1;
	}

	return 0;
}

int WifiSendEvent(WifiStatus status)
{
	static struct blob_buf sBuffer;

	blob_buf_init(&sBuffer, 0);
	blobmsg_add_u32(&sBuffer, "wifi_status", status);

	TLOGI("%s:%d: ---------------\n", __func__, __LINE__);

	TLOGD("WifiSendEvent -------------> %s\n", network_state_txt(status));
	UBusSendEvent(UBUS_REPLY_EVENT_WIFI_STATUS, sBuffer.head);

	blob_buf_free(&sBuffer);

	return 0;
}

int disable_softap(void)
{
	TLOGI("%s:%d: ---------------\n", __func__, __LINE__);

	system("source /usr/bin/disable_softap.sh &");
	sleep(1);

	return 0;
}

int enable_softap(void)
{
	TLOGI("%s:%d: ---------------\n", __func__, __LINE__);

    system("source /usr/bin/enable_softap.sh &");
    sleep(1);

    return 0;
}

