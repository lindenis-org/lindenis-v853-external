#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <pthread.h>
#include <wpa_ctrl.h>
#include <udhcpc.h>
#include <utils.h>
#include <wifi_log.h>
#include <wmg_ap.h>
#include <unistd.h>
#include <linux_hapd.h>
#include <linux_common.h>

static wmg_ap_hapd_inf_object_t ap_hapd_inf_object;

#define IFACE_VALUE_MAX 32
#define HAPD_SEC_NONE        0
#define HAPD_SEC_WPA         1
#define HAPD_SEC_WPA2        2
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static struct wpa_ctrl *ctrl_conn;
static struct wpa_ctrl *monitor_conn;

/* socket pair used to exit from a blocking read */
static int exit_sockets[2];

static const char primary_iface1[] = "wlan0";
static const char IFACE_DIR[]           = "/etc/wifi/hostapd/sockets";
static char primary_iface[IFACE_VALUE_MAX];
static const char HAPD_CONFIG_FILE[]    = "/etc/wifi/hostapd/hostapd.conf";
static const char CONTROL_IFACE_PATH[]  = "/etc/wifi/hostapd/sockets";

static const char IFNAME[]              = "IFNAME=";
#define IFNAMELEN    (sizeof(IFNAME) - 1)
static const char WPA_EVENT_IGNORE[]    = "CTRL-EVENT-IGNORE ";

#define HOSTAPD_TIMEOUT      3000000  // microseconds
#define HOSTAPD_TIMEOUT_STEP  100000  // microseconds

typedef struct {
	wifi_ap_config_t *ap_config;
	bool ap_config_enable;
} global_ap_config_t;

static char global_ssid[SSID_MAX_LEN] = {0};
static char global_psk[PSK_MAX_LEN] = {0};

static wifi_ap_config_t global_wifi_ap_config = {
	.ssid = global_ssid,
	.psk = global_psk,
	.sec = WIFI_SEC_NONE,
	.channel = 0,
	.dev_list = NULL,
	.sta_num = 0,
};

static global_ap_config_t global_ap_config = {
	.ap_config = &global_wifi_ap_config,
	.ap_config_enable = false,
};

static int hostapd_connect_on_socket_path(const char *path)
{
	int  hostapd_timeout = HOSTAPD_TIMEOUT;

	ctrl_conn = wpa_ctrl_open(path);
	while (ctrl_conn == NULL && hostapd_timeout > 0){
		usleep(HOSTAPD_TIMEOUT_STEP);
		hostapd_timeout -= HOSTAPD_TIMEOUT_STEP;
		ctrl_conn = wpa_ctrl_open(path);
	}
	if (ctrl_conn == NULL) {
		WMG_ERROR("Unable to open connection to hostapd on \"%s\": %s\n",
			path, strerror(errno));
		return -1;
	}
	monitor_conn = wpa_ctrl_open(path);
	if (monitor_conn == NULL) {
	WMG_ERROR("monitor_conn is NULL!\n");
		wpa_ctrl_close(ctrl_conn);
		ctrl_conn = NULL;
		return -1;
	}
	if (wpa_ctrl_attach(monitor_conn) != 0) {
		WMG_ERROR("attach monitor_conn error!\n");
		wpa_ctrl_close(monitor_conn);
		wpa_ctrl_close(ctrl_conn);
		ctrl_conn = monitor_conn = NULL;
		return -1;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, exit_sockets) == -1) {
		WMG_ERROR("create socketpair error!\n");
		wpa_ctrl_close(monitor_conn);
		wpa_ctrl_close(ctrl_conn);
		ctrl_conn = monitor_conn = NULL;
		return -1;
	}

	WMG_EXCESSIVE("connect to hostapd ok!\n");
	return 0;
}

static int wifi_send_command(const char *cmd, char *reply, size_t *reply_len)
{
	int ret;
	if (ctrl_conn == NULL) {
		WMG_ERROR("Not connected to hostapd - \"%s\" command dropped.\n", cmd);
		return -1;
	}

	ret = wpa_ctrl_request(ctrl_conn, cmd, strlen(cmd), reply, reply_len, NULL);
	if (ret == -2) {
		WMG_ERROR("'%s' command timed out.\n", cmd);
		/* unblocks the monitor receive socket for termination */
		TEMP_FAILURE_RETRY(write(exit_sockets[0], "T", 1));
		return -2;
	} else if (ret < 0 || strncmp(reply, "FAIL", 4) == 0) {
		return -1;
	}
	if (strncmp(cmd, "PING", 4) == 0) {
		reply[*reply_len] = '\0';
	}
	return 0;
}

static int command_to_hostapd(char const *cmd, char *reply, size_t reply_len)
{
	pthread_mutex_lock(&ap_hapd_inf_object.ap_hapd_mutex);
	if(!cmd || !cmd[0]){
		pthread_mutex_unlock(&ap_hapd_inf_object.ap_hapd_mutex);
		return -1;
	}

	WMG_DUMP("do cmd %s\n", cmd);

	--reply_len; // Ensure we have room to add NUL termination.
	if (wifi_send_command(cmd, reply, &reply_len) != 0) {
		pthread_mutex_unlock(&ap_hapd_inf_object.ap_hapd_mutex);
		return -1;
	}

	WMG_DUMP("do cmd %s, reply: %s\n", cmd, reply);
	// Strip off trailing newline.
	if (reply_len > 0 && reply[reply_len-1] == '\n') {
		reply[reply_len-1] = '\0';
	} else {
		reply[reply_len] = '\0';
	}
	pthread_mutex_unlock(&ap_hapd_inf_object.ap_hapd_mutex);
	return 0;
}

static int parse_ap_config(char *reply, wifi_ap_config_t *ap_config)
{
	int ret = 0;
	char *pos = NULL;
	char tmp[SSID_MAX_LEN] = {0};
	char *pch;

	if (reply == NULL || ap_config == NULL) {
		WMG_ERROR("invalid parameters\n");
		return ret;
	}

	pos = strstr(reply, "state");
	if (pos != NULL) {
		pos += 6;
		if (strncmp(pos, "ENABLED", 7) != 0) {
			WMG_WARNG("Warning: ap state is inactive\n");
			return -1;
		} else {
			WMG_DUMP("ap state is enable\n");
			pch = strtok(reply, "'\n'");
			while (pch != NULL) {
				if (strncmp(pch, "ssid[0]=", 8) == 0) {
					if(strlen(pch + 8) > SSID_MAX_LEN) {
						WMG_DEBUG("ssid is too long:%d",strlen(pch + 8));
						return -1;
					}
					strcpy(ap_config->ssid, (pch + 8));
					WMG_DEBUG("%s\n", ap_config->ssid);
				}
				if (strncmp(pch, "channel=", 8) == 0) {
					ap_config->channel = atoi(pch + 8);
					WMG_DEBUG("%d\n", ap_config->channel);
				}
				if (strncmp(pch, "num_sta[0]=", 11) == 0) {
					ap_config->sta_num = atoi(pch + 11);
					WMG_DEBUG("%d\n", ap_config->sta_num);
				}
				pch = strtok(NULL, "'\n'");
			}
		}
	} else {
		WMG_ERROR("invalid ap config info %s\n", reply);
	}

	return ret;
}

static int linux_ap_hapd_set_config(wifi_ap_config_t *ap_config)
{
	int ret;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};
	int passwd_len = 0;

	if((ap_config == NULL) || (ap_config->ssid == NULL)){
		WMG_ERROR("ap config format is error, ssid can't NULL\n");
		return WMG_STATUS_FAIL;
	}

	WMG_DUMP("set ssid %s\n", ap_config->ssid);
	sprintf(cmd, "SET ssid %s", ap_config->ssid);
	ret = command_to_hostapd(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("failed to set ssid '%s', reply %s\n",ap_config->ssid, reply);
		return WMG_STATUS_FAIL;
	}

	if (ap_config->channel < 14) {
		WMG_DUMP("set channel %u\n", ap_config->channel);
		sprintf(cmd, "SET channel %u", ap_config->channel);
		ret = command_to_hostapd(cmd, reply, sizeof(reply));
		if (ret) {
			WMG_ERROR("failed to set channel '%u', reply %s\n", ap_config->channel, reply);
			return WMG_STATUS_FAIL;
		}
	} else {
		WMG_ERROR("invalid channel\n");
		return WMG_STATUS_FAIL;
	}

	switch (ap_config->sec){
		case WIFI_SEC_NONE:
			WMG_DUMP("set psk NULL\n");
			WMG_DUMP("set wpa sec: %s\n", wmg_sec_to_str(ap_config->sec));
			sprintf(cmd, "SET wpa %d", HAPD_SEC_NONE);
			ret = command_to_hostapd(cmd, reply, sizeof(reply));
			if (ret) {
				WMG_ERROR("failed to set wpa '%s', reply %s\n", wmg_sec_to_str(ap_config->sec), reply);
				return WMG_STATUS_FAIL;
			}
			break;
		case WIFI_SEC_WEP:
			sprintf(cmd, "SET wep_default_key 0");
			ret = command_to_hostapd(cmd, reply, sizeof(reply));
			if (ret) {
				WMG_ERROR("failed to set wpa_default_key 0, reply %s\n");
				return WMG_STATUS_FAIL;
			}
			WMG_DUMP("set wep_default_key 0\n");
			passwd_len = strlen(ap_config->psk);
			if((passwd_len == 10) || (passwd_len == 26)) {
				sprintf(cmd, "SET wep_key0 %s", ap_config->psk);
				WMG_DEBUG("set passwd HEX format!\n");
			} else if((passwd_len == 5) || (passwd_len == 13)) {
				sprintf(cmd, "SET wep_key0 \"%s\"", ap_config->psk);
				WMG_DEBUG("The passwd is ASCII format!\n");
			} else {
				WMG_ERROR("The password does not conform to the specification!\n");
				return WMG_STATUS_FAIL;
			}
			ret = command_to_hostapd(cmd, reply, sizeof(reply));
			if (ret){
				WMG_ERROR("failed to set wep_key0 '%s', reply %s\n", ap_config->psk, reply);
				return WMG_STATUS_FAIL;
			}
			WMG_DUMP("set wep_key0 %s\n", ap_config->psk);
			break;
		case WIFI_SEC_WPA_PSK:
			if (ap_config->psk != NULL) {
				sprintf(cmd, "SET wpa %d", HAPD_SEC_WPA);
				ret = command_to_hostapd(cmd, reply, sizeof(reply));
				if (ret){
					WMG_ERROR("failed to set wpa '%s', reply %s\n", wmg_sec_to_str(ap_config->sec), reply);
					return WMG_STATUS_FAIL;
				}
				WMG_DUMP("set wpa %s\n", wmg_sec_to_str(ap_config->sec));
				sprintf(cmd, "SET wpa_key_mgmt WPA-PSK");
				ret = command_to_hostapd(cmd, reply, sizeof(reply));
				if (ret) {
				WMG_ERROR("failed to set wpa key mgmt WPA-PSK, reply %s\n", reply);
					return WMG_STATUS_FAIL;
				}
				passwd_len = strlen(ap_config->psk);
				if(passwd_len < 8) {
					WMG_ERROR("Password shorter than 8 characters!\n");
					return WMG_STATUS_FAIL;
				}
				sprintf(cmd, "SET rsn_pairwise CCMP");
				ret = command_to_hostapd(cmd, reply, sizeof(reply));
				if (ret) {
					WMG_ERROR("failed to set rsn_pairwise CCMP");
					return WMG_STATUS_FAIL;
				}
				sprintf(cmd, "SET wpa_passphrase %s", ap_config->psk);
				ret = command_to_hostapd(cmd, reply, sizeof(reply));
				if (ret) {
					WMG_ERROR("failed to set psk '%s', reply %s\n",ap_config->psk, reply);
					return WMG_STATUS_FAIL;
				}
				WMG_DUMP("set psk %s\n", ap_config->psk);
			} else {
				WMG_ERROR("psk is NULL, but sec not WIFI_SEC_NONE(WIFI_SEC_WPA_PSK)\n");
				WMG_ERROR("Please check ap config format\n");
				return WMG_STATUS_FAIL;
			}
			break;
		case WIFI_SEC_WPA2_PSK:
			if (ap_config->psk != NULL) {
				sprintf(cmd, "SET wpa %d", HAPD_SEC_WPA2);
				ret = command_to_hostapd(cmd, reply, sizeof(reply));
				if (ret){
					WMG_ERROR("failed to set wpa '%s', reply %s\n", wmg_sec_to_str(ap_config->sec), reply);
					return WMG_STATUS_FAIL;
				}
				WMG_DUMP("set wpa %s\n", wmg_sec_to_str(ap_config->sec));
				sprintf(cmd, "SET wpa_key_mgmt WPA-PSK");
				ret = command_to_hostapd(cmd, reply, sizeof(reply));
				if (ret) {
				WMG_ERROR("failed to set wpa key mgmt WPA-PSK, reply %s\n", reply);
					return WMG_STATUS_FAIL;
				}
				passwd_len = strlen(ap_config->psk);
				if(passwd_len < 8) {
					WMG_ERROR("Password shorter than 8 characters!\n");
					return WMG_STATUS_FAIL;
				}
				sprintf(cmd, "SET rsn_pairwise CCMP");
				ret = command_to_hostapd(cmd, reply, sizeof(reply));
				if (ret) {
					WMG_ERROR("failed to set rsn_pairwise CCMP");
					return WMG_STATUS_FAIL;
				}
				sprintf(cmd, "SET wpa_passphrase %s", ap_config->psk);
				ret = command_to_hostapd(cmd, reply, sizeof(reply));
				if (ret) {
					WMG_ERROR("failed to set psk '%s', reply %s\n",ap_config->psk, reply);
					return WMG_STATUS_FAIL;
				}
				WMG_DUMP("set psk %s\n", ap_config->psk);
			} else {
				WMG_ERROR("psk is NULL, but sec not WIFI_SEC_NONE(WIFI_SEC_WPA2_PSK)\n");
				WMG_ERROR("Please check ap config format\n");
				return WMG_STATUS_FAIL;
			}
			break;
		default:
			WMG_ERROR("not support sec\n");
			WMG_ERROR("Please check ap config format\n");
			return WMG_STATUS_FAIL;
			break;
	}

	strcpy(global_ap_config.ap_config->ssid, ap_config->ssid);
	strcpy(global_ap_config.ap_config->psk, ap_config->psk);
	global_ap_config.ap_config->sec = ap_config->sec;
	global_ap_config.ap_config->channel = ap_config->channel;
	global_ap_config.ap_config_enable = true;

	return WMG_STATUS_SUCCESS;
}

static int linux_ap_hapd_cmd_enable()
{
	int ret;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	WMG_DUMP("enable ap\n");
	cmd[CMD_MAX_LEN] = '\0';
	strncpy(cmd, "ENABLE", CMD_MAX_LEN);
	ret = command_to_hostapd(cmd, reply, sizeof(reply));
	if (ret)
		WMG_ERROR("failed to enable ap, reply %s\n", reply);

	return ret;
}

static int linux_ap_hapd_cmd_disable()
{
	int ret;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	WMG_DUMP("disable ap\n");
	cmd[CMD_MAX_LEN] = '\0';
	strncpy(cmd, "DISABLE", CMD_MAX_LEN);
	ret = command_to_hostapd(cmd, reply, sizeof(reply));
	if (ret)
		WMG_ERROR("failed to disable ap, reply %s\n", reply);

	return ret;
}

static int linux_ap_hapd_cmd_status()
{
	int ret;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	WMG_DUMP("hapd status\n");
	cmd[CMD_MAX_LEN] = '\0';
	strncpy(cmd, "STATUS", CMD_MAX_LEN);
	ret = command_to_hostapd(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("failed to get ap status\n");
		return -1;
	}
	if (strstr(reply, "state=ENABLED") != NULL) {
		WMG_DUMP("ap status is: enable\n");
		return 0;
	} else if (strstr(reply, "state=DISABLED") != NULL) {
		WMG_DUMP("ap status is: disable\n");
		return 1;
	} else {
		WMG_DUMP("ap status is unknowd\n");
		return -1;
	}
}

static int linux_ap_hapd_cmd_reload()
{
	int ret;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	WMG_DUMP("reload ap config\n");
	cmd[CMD_MAX_LEN] = '\0';
	strncpy(cmd, "RELOAD", CMD_MAX_LEN);
	ret = command_to_hostapd(cmd, reply, sizeof(reply));
	if (ret)
		WMG_ERROR("failed to reload ap config, reply %s\n", reply);

	return ret;
}

int wifi_ctrl_recv(char *reply, size_t *reply_len)
{
	int res;
	int ctrlfd = wpa_ctrl_get_fd(monitor_conn);
	struct pollfd rfds[2];

	memset(rfds, 0, 2 * sizeof(struct pollfd));
	rfds[0].fd = ctrlfd;
	rfds[0].events |= POLLIN;
	rfds[1].fd = exit_sockets[1];
	rfds[1].events |= POLLIN;
	pthread_testcancel();
	res = TEMP_FAILURE_RETRY(poll(rfds, 2, -1));
	pthread_testcancel();
	if (res < 0) {
	    WMG_ERROR("Error poll = %d\n", res);
	    return res;
	}
	if (rfds[0].revents & POLLIN) {
	    return wpa_ctrl_recv(monitor_conn, reply, reply_len);
	}

	/* it is not rfds[0], then it must be rfts[1] (i.e. the exit socket)
	 * or we timed out. In either case, this call has failed ..
	 */
	return -2;
}

static int wifi_wait_on_socket(char *buf, size_t buflen)
{
	size_t nread = buflen - 1;
	int result;
	char *match, *match2;

	if (monitor_conn == NULL) {
	    return snprintf(buf, buflen, WPA_EVENT_TERMINATING " - connection closed");
	}

	result = wifi_ctrl_recv(buf, &nread);

	/* Terminate reception on exit socket */
	if (result == -2) {
	    return snprintf(buf, buflen, WPA_EVENT_TERMINATING " - connection closed");
	}

	if (result < 0) {
	    WMG_ERROR("wifi_ctrl_recv failed: %s\n", strerror(errno));
	    return snprintf(buf, buflen, WPA_EVENT_TERMINATING " - recv error");
	}
	WMG_EXCESSIVE("wifi_ctrl_recv: %s\n", buf);
	buf[nread] = '\0';
	/* Check for EOF on the socket */
	if (result == 0 && nread == 0) {
	    /* Fabricate an event to pass up */
	    WMG_EXCESSIVE("Received EOF on supplicant socket\n");
	    return snprintf(buf, buflen, WPA_EVENT_TERMINATING " - signal 0 received");
	}
	/*
	 * Events strings are in the format
	 *
	 *     IFNAME=iface <N>CTRL-EVENT-XXX
	 *        or
	 *     <N>CTRL-EVENT-XXX
	 *
	 * where N is the message level in numerical form (0=VERBOSE, 1=Excessive,
	 * etc.) and XXX is the event nae. The level information is not useful
	 * to us, so strip it off.
	 */

	if (strncmp(buf, IFNAME, IFNAMELEN) == 0) {
	    match = strchr(buf, ' ');
	    if (match != NULL) {
	        if (match[1] == '<') {
	            match2 = strchr(match + 2, '>');
	            if (match2 != NULL) {
	                nread -= (match2 - match);
	                memmove(match + 1, match2 + 1, nread - (match - buf) + 1);
	            }
	        }
	    } else {
	        return snprintf(buf, buflen, "%s", WPA_EVENT_IGNORE);
	    }
	} else if (buf[0] == '<') {
	    match = strchr(buf, '>');
	    if (match != NULL) {
	        nread -= (match + 1 - buf);
	        memmove(buf, match + 1, nread + 1);
	        //printf("supplicant generated event without interface - %s\n", buf);
		}
	} else {
	    /* let the event go as is! */
	    //printf("supplicant generated event without interface and without message level - %s\n", buf);
	}

	return nread;
}


static void wifi_close_sockets()
{
	char reply[4096] = {0};
	int ret = 0;

    if (monitor_conn != NULL) {
        wpa_ctrl_detach(monitor_conn);
        wpa_ctrl_close(monitor_conn);
        monitor_conn = NULL;
    }

/*
>---ret = wifi_command("TERMINATE", reply, sizeof(reply));
>---if(ret) {
>--->---WMG_EXCESSIVE("do terminate error!");
>---}
*/
    if (ctrl_conn != NULL) {
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = NULL;
    }

    if (exit_sockets[0] >= 0) {
        close(exit_sockets[0]);
        exit_sockets[0] = -1;
    }

    if (exit_sockets[1] >= 0) {
        close(exit_sockets[1]);
        exit_sockets[1] = -1;
    }
}

static int start_hostapd(int try_times)
{
	int ret = 0;
	int i = 0;
	char cmd[128] = {0};
	cmd[127] = '\0';
	int try_cnt;
	for(try_cnt = 0; try_cnt < try_times; try_cnt++, i = 0) {
		sprintf(cmd, "hostapd %s &", HAPD_CONFIG_FILE);
		system(cmd);
		while (i < 5) {
			sleep(1);
			ret = check_process_is_exist("hostapd", 7);
			if (ret)
				break;
			i++;
		}
		if (ret == 0) {
			WMG_ERROR("failed to start hostapd\n");
		} else {
			WMG_DEBUG("start hostapd times %d success\n", (try_cnt + 1));
			return 0;
		}
	}
	return -1;
}

static int wmg_sta_device_add(dev_node_t *list_head, char *bssid)
{
	dev_node_t *new = NULL;

	if (list_head == NULL) {
		WMG_ERROR("head node is NULL!\n");
		return -1;
	}

	new = (dev_node_t *)malloc(sizeof(dev_node_t));
	if (new == NULL) {
		WMG_ERROR("failed to allocate memory for device node\n");
		return -1;
	}
	new->next = NULL;
	memset(new->bssid, 0, BSSID_MAX_LEN);
	strncpy(new->bssid, bssid, BSSID_MAX_LEN - 1);
	WMG_DUMP("new device '%s' added\n", new->bssid);
	new->next = list_head->next;
	list_head->next = new;

	return 0;
}

static int wmg_sta_device_remove(dev_node_t *list_head, char *bssid)
{
	int ret = -1;
	dev_node_t *pre = list_head;
	dev_node_t *p = list_head->next;

	while (p != NULL) {
		if (0 == strncmp(p->bssid, bssid, BSSID_MAX_LEN - 1)) {
			WMG_DUMP("remove device '%s'\n", p->bssid);
			pre->next = p->next;
			free(p);
			ret = 0;
			break;
		}
		pre = p;
		p = p->next;
	}

	if (ret == -1)
		WMG_WARNG("%s is not found, remove device failed\n");

	return ret;
}
static void wmg_sta_device_print(dev_node_t *list_head)
{
	dev_node_t *p = list_head->next;
	int i = 1;

	while (p != NULL) {
		WMG_DUMP("the %dth device is '%s'\n", i, p->bssid);
		p = p->next;
		i++;
	}
}

static void hapd_event_notify_to_ap_dev(wifi_ap_event_t event)
{
	if (ap_hapd_inf_object.ap_hapd_event_cb) {
		ap_hapd_inf_object.ap_hapd_event_cb(event);
	}
}

static void hapd_event_notify(wifi_ap_event_t event)
{
	evt_send(ap_hapd_inf_object.ap_event_handle, event);
	hapd_event_notify_to_ap_dev(event);
}

static int hapd_dispatch_event(const char *event_str, int nread)
{
	int i = 0, event = 0, ret;
	char event_nae[18];
	char cmd[255] = {0}, reply[16] = {0};
	char *nae_start = NULL, *nae_end = NULL;
	char *event_data = NULL;

	if (!event_str || !event_str[0]) {
		WMG_WARNG("hostapd event is NULL!\n");
		return 0;
	}

	WMG_DUMP("receive hapd event: '%s'\n", event_str);
	if (strstr(event_str, "AP-ENABLED")) {
		event = WIFI_AP_ENABLED;
		hapd_event_notify(event);
	} else if (strstr(event_str, "AP-DISABLED")) {
		event = WIFI_AP_DISABLED;
		hapd_event_notify(event);
	} else if (strstr(event_str, "AP-STA-CONNECTED")) {
		event_data = strchr(event_str, ' ');
		if (event_data) {
			event_data++;
			WMG_DUMP("bssid=%s\n", event_data);
			ret = wmg_sta_device_add(ap_hapd_inf_object.dev_list, event_data);
			if (ret == 0)
				ap_hapd_inf_object.sta_num++;
			wmg_sta_device_print(ap_hapd_inf_object.dev_list);
		}
		event = WIFI_AP_STA_CONNECTED;
		hapd_event_notify(event);
	} else if (strstr(event_str, "AP-STA-DISCONNECTED")) {
		event_data = strchr(event_str, ' ');
		if (event_data) {
			event_data++;
			ret = wmg_sta_device_remove(ap_hapd_inf_object.dev_list, event_data);
			if (ret == 0)
				ap_hapd_inf_object.sta_num--;
			wmg_sta_device_print(ap_hapd_inf_object.dev_list);
		}
		event = WIFI_AP_STA_DISCONNECTED;
		hapd_event_notify(event);
	} else {
		event = WIFI_AP_UNKNOWN;
	}

	return 0;
}

static void *hapd_event_thread(void *args)
{
	char buf[EVENT_BUF_SIZE] = {0};
	int size, ret;

	for (;;) {
		size = wifi_wait_on_socket(buf, sizeof(buf));
		if (size > 0) {
			ret = hapd_dispatch_event(buf, size);
			if (ret) {
				WMG_DUMP("hostapd terminated\n");
				break;
			}
		} else {
			continue;
		}
	}

	pthread_exit(NULL);
}

static int linux_ap_hapd_init(ap_hapd_event_cb_t ap_event_cb)
{
	if(ap_hapd_inf_object.ap_hapd_init_flag == WMG_FALSE) {
		WMG_INFO("linux hostapd init now\n");
		ap_hapd_inf_object.dev_list = (dev_node_t *)malloc(sizeof(dev_node_t));
		if (ap_hapd_inf_object.dev_list != NULL) {
			memset(ap_hapd_inf_object.dev_list->bssid, 0, BSSID_MAX_LEN);
			ap_hapd_inf_object.dev_list->next = NULL;
		} else {
			WMG_ERROR("failed to allocate memory for device list\n");
			return WMG_STATUS_NOMEM;
		}
		ap_hapd_inf_object.ap_event_handle = (event_handle_t *)malloc(sizeof(event_handle_t));
		if(ap_hapd_inf_object.ap_event_handle != NULL) {
			memset(ap_hapd_inf_object.ap_event_handle, 0, sizeof(event_handle_t));
			ap_hapd_inf_object.ap_event_handle->evt_socket[0] = -1;
			ap_hapd_inf_object.ap_event_handle->evt_socket[1] = -1;
			ap_hapd_inf_object.ap_event_handle->evt_socket_enable = WMG_FALSE;
		} else {
			WMG_ERROR("failed to allocate memory for linux hapd event_handle\n");
			goto event_erro;
		}
		if(ap_event_cb != NULL){
			ap_hapd_inf_object.ap_hapd_event_cb = ap_event_cb;
		}
		ap_hapd_inf_object.ap_hapd_init_flag = WMG_TRUE;
	} else {
		WMG_INFO("linux hostapd already init\n");
	}
	return WMG_STATUS_SUCCESS;

event_erro:
	free(ap_hapd_inf_object.dev_list);
	return WMG_STATUS_FAIL;
}

static void linux_ap_hapd_deinit(void)
{
	dev_node_t *p = ap_hapd_inf_object.dev_list;
	dev_node_t *temp = NULL;

	if(ap_hapd_inf_object.ap_hapd_init_flag == WMG_TRUE) {
		WMG_INFO("linux hostapd deinit now\n");

		while (p != NULL) {
			temp = p;
			p = p->next;
			free(temp);
		}
		ap_hapd_inf_object.sta_num = 0;

		if(ap_hapd_inf_object.ap_pid != -1) {
			os_net_thread_delete(&ap_hapd_inf_object.ap_pid);
			ap_hapd_inf_object.ap_pid = -1;
		}

		evt_socket_exit(ap_hapd_inf_object.ap_event_handle);
		wifi_close_sockets();

		if (check_process_is_exist("hostapd", 7) == 1) {
			system("pidof hostapd | xargs kill -9");
			sleep(1);
			if (check_process_is_exist("hostapd", 7) == 1) {
				WMG_ERROR("kill hostapd failed\n");
				return;
			}
			ap_hapd_inf_object.ap_hapd_exist = WMG_FALSE;
		}
		if (check_process_is_exist("dnsmasq", 7) == 1) {
			system("pidof dnsmasq | xargs kill -9");
			sleep(1);
			if (check_process_is_exist("dnsmasq", 7) == 1) {
				WMG_WARNG("kill dnsmasq failed\n");
			}
		}

		system("ifconfig wlan0 down");
		free(ap_hapd_inf_object.ap_event_handle);
		ap_hapd_inf_object.ap_hapd_event_cb = NULL;
		ap_hapd_inf_object.ap_hapd_init_flag = WMG_FALSE;
	} else {
		WMG_INFO("linux hostapd already init\n");
	}
	return;
}

static int linux_connect_to_hostapd()
{
	static char path[PATH_MAX];
	int ret = 0;

	if (check_process_is_exist("wpa_supplicant", 14) == 1) {
		WMG_DEBUG("wpa_supplicant is running, need to stop it\n");
		system("pidof wpa_supplicant | xargs kill -9");
		if (check_process_is_exist("wpa_supplicant", 14) == 1) {
			sleep(1);
			if (check_process_is_exist("wpa_supplicant", 14) == 1) {
				WMG_ERROR("kill wpa_supplicant failed\n");
				return WMG_STATUS_FAIL;
			}
		}
	}

	if (check_process_is_exist("hostapd", 7) == 0) {
		WMG_DEBUG("hostapd is not running, need to start it\n");

		if(start_hostapd(2)){
			WMG_ERROR("start hostapd failed\n");
			return WMG_STATUS_FAIL;
		}

		WMG_EXCESSIVE("start hostapd success\n");
	} else {
		WMG_EXCESSIVE("hostapd is already running\n");
	}

	//strncpy(primary_iface, "wlan0", IFACE_VALUE_MAX);
	if (access(IFACE_DIR, F_OK) == 0) {
		snprintf(path, sizeof(path), "%s/%s", IFACE_DIR, primary_iface1);
	} else {
		WMG_ERROR("hostapd socket interface not exists\n");
		return -1;
	}
	if(hostapd_connect_on_socket_path(path))
	{
		return WMG_STATUS_FAIL;
	}

	ret = evt_socket_init(ap_hapd_inf_object.ap_event_handle);
	if (ret) {
		WMG_ERROR("failed to initialize linux hapd event socket\n");
		ret = WMG_STATUS_FAIL;
		goto hapd_socket_err;
	}

	ret = evt_socket_clear(ap_hapd_inf_object.ap_event_handle);
	if (ret) {
		WMG_WARNG("failed to clear linux hapd event socket\n");
	}

	ret = os_net_thread_create(&ap_hapd_inf_object.ap_pid, NULL, hapd_event_thread, NULL, 0, 4096);
	if (ret) {
		WMG_ERROR("failed to create linux ap event handle thread\n");
		ret = WMG_STATUS_FAIL;
		goto socket_err;
	}
	WMG_DUMP("create linux ap event handle thread success\n");

	return WMG_STATUS_SUCCESS;

socket_err:
	evt_socket_exit(ap_hapd_inf_object.ap_event_handle);
hapd_socket_err:
	wifi_close_sockets();
	return ret;
}

static int linux_ap_hapd_enable(wifi_ap_config_t *ap_config)
{
	int ret = -1;

	ret = linux_ap_hapd_cmd_status();
	if(ret == 1){
		WMG_DEBUG("hapd is already disable\n");
	} else if (ret == 0){
		WMG_DEBUG("hapd is already enable, disable now...\n");
		ret = linux_ap_hapd_cmd_disable();
		if(ret){
			WMG_ERROR("reload hapd config faile\n");
			return WMG_STATUS_FAIL;
		}
	} else {
		WMG_ERROR("failed to get ap hapd status\n");
		return WMG_STATUS_FAIL;
	}

	ret = linux_ap_hapd_set_config(ap_config);
	if(ret){
		WMG_ERROR("failed to set ap hapd config\n");
		return WMG_STATUS_FAIL;
	}

	WMG_DEBUG("Enable ap hapd now...\n");
	ret = linux_ap_hapd_cmd_enable();
	if(ret){
		WMG_ERROR("failed to enable ap hapd\n");
		return WMG_STATUS_FAIL;
	}

	system("ifconfig wlan0 192.168.5.1");
	system("mkdir -p /var/lib/misc/");
	system("dnsmasq -C /etc/dnsmasq.conf");

	return WMG_STATUS_SUCCESS;
}

static int linux_ap_hapd_disable()
{
	int ret;

	ret = linux_ap_hapd_cmd_status();
	if(ret == 1){
		WMG_DEBUG("hapd is already disable, not need to disable\n");
		return WMG_STATUS_SUCCESS;
	}

	ret = linux_ap_hapd_cmd_disable();
	if(ret){
		WMG_ERROR("failed to disable ap hapd\n");
		return WMG_STATUS_FAIL;
	}

	system("ifconfig wlan0 0.0.0.0");
	system("rm -rf /var/lib/misc/");
	system("pidof dnsmasq | xargs kill -9");

	return WMG_STATUS_SUCCESS;
}

static int linux_ap_hapd_get_config(wifi_ap_config_t *ap_config)
{
	WMG_DEBUG("ap get config\n");

	wmg_status_t ret;
	int count = 0;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	cmd[CMD_MAX_LEN] = '\0';
	strncpy(cmd, "STATUS", CMD_MAX_LEN);
	ret = command_to_hostapd(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("failed to get ap status\n");
		return -1;
	}
	if (strstr(reply, "state=DISABLED") != NULL) {
		WMG_DEBUG("hapd is disabled, please enable first\n");
		return WMG_STATUS_FAIL;
	}

	ret = parse_ap_config(reply, ap_config);
	if (ret) {
		WMG_ERROR("failed to parse ap config\n");
		return WMG_STATUS_FAIL;
	}

	if(global_ap_config.ap_config_enable != true) {
		WMG_ERROR("failed to get ap config psk\n");
		return WMG_STATUS_FAIL;
	} else {
		strcpy(ap_config->psk, global_ap_config.ap_config->psk);
	}

	dev_node_t *p = ap_hapd_inf_object.dev_list->next;
	while (p != NULL) {
		strncpy(ap_config->dev_list[count], p->bssid, BSSID_MAX_LEN - 1);
		WMG_DUMP("copy device '%s' to device list\n", ap_config->dev_list[count]);
		p = p->next;
		count++;
		if (count >= STA_MAX_NUM)
		break;
	}
	WMG_DUMP("number of connected sta is %d\n", count);

	return WMG_STATUS_SUCCESS;
}

static int linux_set_mac(const char *ifname, uint8_t *mac_addr)
{
	wmg_status_t ret;

	ret = linux_common_set_mac(ifname, mac_addr);
	if (ret) {
		WMG_ERROR("linux failed to set mac\n");
		return WMG_STATUS_FAIL;
	}

	return WMG_STATUS_SUCCESS;
}

static int linux_get_mac(const char *ifname, uint8_t *mac_addr)
{
	wmg_status_t ret;

	ret = linux_common_get_mac(ifname, mac_addr);
	if (ret) {
		WMG_ERROR("linux failed to get mac\n");
		return WMG_STATUS_FAIL;
	}

	return WMG_STATUS_SUCCESS;
}

static int linux_command_to_hostapd(char const *cmd, char *reply, size_t reply_len)
{
	return command_to_hostapd(cmd, reply, reply_len);
}

static int linux_platform_extension(int cmd, void* cmd_para,int *erro_code)
{
	switch (cmd) {
		case HAPD_CMD_SEND_DISABLE_TO_HAPD:
		return linux_ap_hapd_cmd_disable();
		case HAPD_CMD_GET_CONFIG:
		return linux_ap_hapd_get_config((wifi_ap_config_t *)cmd_para);
		case HAPD_CMD_SET_MAC:
			{
				common_mac_para_t * common_mac_para = (common_mac_para_t *)cmd_para;
				return linux_set_mac(common_mac_para->ifname, common_mac_para->mac_addr);
			}
		case HAPD_CMD_GET_MAC:
			{
				common_mac_para_t * common_mac_para = (common_mac_para_t *)cmd_para;
				return linux_get_mac(common_mac_para->ifname, common_mac_para->mac_addr);
			}
		default:
		return WMG_FALSE;
	}
	return WMG_FALSE;
}

static wmg_ap_hapd_inf_object_t ap_hapd_inf_object = {
	.ap_hapd_init_flag = WMG_FALSE,
	.ap_hapd_exist = WMG_FALSE,
	.ap_hapd_connected = WMG_FALSE,
	.sta_num = 0,
	.ap_pid = -1,

	.ap_hapd_init = linux_ap_hapd_init,
	.ap_hapd_deinit = linux_ap_hapd_deinit,
	.ap_hapd_connect = linux_connect_to_hostapd,
	.ap_hapd_enable = linux_ap_hapd_enable,
	.ap_hapd_disable = linux_ap_hapd_disable,
	//.ap_hapd_get_config = linux_ap_hapd_get_config,
	.ap_hapd_command = linux_command_to_hostapd,
	.ap_platform_extension = linux_platform_extension,
};

wmg_ap_hapd_inf_object_t * ap_linux_inf_object_register(void)
{
	return &ap_hapd_inf_object;
}
