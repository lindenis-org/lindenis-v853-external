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
#include <linux_wpa.h>
#include <udhcpc.h>
#include <utils.h>
#include <wifi_log.h>
#include <unistd.h>
#include <wmg_sta.h>
#include <linux/event.h>
#include <linux/udhcpc.h>
#include <linux/scan.h>
#include <linux_common.h>

static wmg_sta_wpa_inf_object_t sta_wpa_inf_object;

#define IFACE_VALUE_MAX 32
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define LIST_ENTRY_NUME_MAX 64

static struct wpa_ctrl *ctrl_conn;
static struct wpa_ctrl *monitor_conn;

/* socket pair used to exit from a blocking read */
static int exit_sockets[2];

static const char primary_iface1[] = "wlan0";
static const char IFACE_DIR[]           = "/etc/wifi/wpa_supplicant/sockets";
static char primary_iface[IFACE_VALUE_MAX];
static const char SUPP_CONFIG_TEMPLATE[]= "/etc/wifi/wpa_supplicant/wpa_supplicant_src.conf";
static const char SUPP_CONFIG_FILE[]    = "/etc/wifi/wpa_supplicant/wpa_supplicant.conf";
static const char CONTROL_IFACE_PATH[]  = "/etc/wifi/wpa_supplicant/sockets";

static const char SUPP_ENTROPY_FILE[]   = WIFI_ENTROPY_FILE;
static unsigned char dummy_key[21] = { 0x02, 0x11, 0xbe, 0x33, 0x43, 0x35,
                                       0x68, 0x47, 0x84, 0x99, 0xa9, 0x2b,
                                       0x1c, 0xd3, 0xee, 0xff, 0xf1, 0xe2,
                                       0xf3, 0xf4, 0xf5 };

static const char IFNAME[]              = "IFNAME=";
#define IFNAMELEN			(sizeof(IFNAME) - 1)
static const char WPA_EVENT_IGNORE[]    = "CTRL-EVENT-IGNORE ";

const char *wmg_sta_event_to_str(wifi_sta_event_t event)
{
	switch (event) {
	case WIFI_DISCONNECTED:
		return "DISCONNECTED";
	case WIFI_SCAN_STARTED:
		return "SCAN_STARTED";
	case WIFI_SCAN_FAILED:
		return "SCAN_FAILED";
	case WIFI_SCAN_RESULTS:
		return "SCAN_RESULTS";
	case WIFI_NETWORK_NOT_FOUND:
		return "NETWORK_NOT_FOUND";
	case WIFI_PASSWORD_INCORRECT:
		return "PASSWORD_INCORRECT";
	case WIFI_ASSOC_REJECT:
		return "ASSOC_REJECT";
	case WIFI_CONNECTED:
		return "CONNECTED";
	case WIFI_TERMINATING:
		return "TERMINATING";
	default:
		return "UNKNOWN";
	}
}

static int ensure_entropy_file_exists()
{
    int ret;
    int destfd;

    ret = access(SUPP_ENTROPY_FILE, R_OK|W_OK);
    if ((ret == 0) || (errno == EACCES)) {
        if ((ret != 0) &&
            (chmod(SUPP_ENTROPY_FILE, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) != 0)) {
            WMG_ERROR("Cannot set RW to \"%s\": %s\n", SUPP_ENTROPY_FILE, strerror(errno));
            return -1;
        }
        return 0;
    }
    destfd = TEMP_FAILURE_RETRY(open(SUPP_ENTROPY_FILE, O_CREAT|O_RDWR, 0660));
    if (destfd < 0) {
        WMG_EXCESSIVE("Cannot create \"%s\": %s\n", SUPP_ENTROPY_FILE, strerror(errno));
        return -1;
    }

    if (TEMP_FAILURE_RETRY(write(destfd, dummy_key, sizeof(dummy_key))) != sizeof(dummy_key)) {
        WMG_ERROR("Error writing \"%s\": %s\n", SUPP_ENTROPY_FILE, strerror(errno));
        close(destfd);
        return -1;
    }
    close(destfd);

    /* chmod is needed because open() didn't set permisions properly */
    if (chmod(SUPP_ENTROPY_FILE, 0660) < 0) {
        WMG_ERROR("Error changing permissions of %s to 0660: %s\n",
             SUPP_ENTROPY_FILE, strerror(errno));
        unlink(SUPP_ENTROPY_FILE);
        return -1;
    }

    return 0;
}

#define SUPPLICANT_TIMEOUT      3000000  // microseconds
#define SUPPLICANT_TIMEOUT_STEP  100000  // microseconds
static int wifi_connect_on_socket_path(const char *path)
{
    int  supplicant_timeout = SUPPLICANT_TIMEOUT;

    ctrl_conn = wpa_ctrl_open(path);
    while (ctrl_conn == NULL && supplicant_timeout > 0){
        usleep(SUPPLICANT_TIMEOUT_STEP);
        supplicant_timeout -= SUPPLICANT_TIMEOUT_STEP;
        ctrl_conn = wpa_ctrl_open(path);
    }
    if (ctrl_conn == NULL) {
        WMG_ERROR("Unable to open connection to supplicant on \"%s\": %s\n",
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

    WMG_EXCESSIVE("connect to wpa_supplicant ok!\n");
    return 0;
}

static int wifi_send_command(const char *cmd, char *reply, size_t *reply_len)
{
    int ret;
    if (ctrl_conn == NULL) {
        WMG_ERROR("Not connected to wpa_supplicant - \"%s\" command dropped.\n", cmd);
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

static int command_to_supplicant(char const *cmd, char *reply, size_t reply_len)
{
	pthread_mutex_lock(&sta_wpa_inf_object.sta_wpa_mutex);
	if(!cmd || !cmd[0]){
		pthread_mutex_unlock(&sta_wpa_inf_object.sta_wpa_mutex);
		return -1;
	}

	WMG_EXCESSIVE("do cmd %s\n", cmd);

	--reply_len; // Ensure we have room to add NUL termination.
	if (wifi_send_command(cmd, reply, &reply_len) != 0) {
		pthread_mutex_unlock(&sta_wpa_inf_object.sta_wpa_mutex);
		return -1;
	}

	WMG_EXCESSIVE("do cmd %s, reply: %s\n", cmd, reply);
	// Strip off trailing newline.
	if (reply_len > 0 && reply[reply_len-1] == '\n') {
		reply[reply_len-1] = '\0';
	} else {
		reply[reply_len] = '\0';
	}
	pthread_mutex_unlock(&sta_wpa_inf_object.sta_wpa_mutex);
	return 0;
}

static int wifi_ctrl_recv(char *reply, size_t *reply_len)
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

static int start_supplicant(int try_times)
{
	int ret = 0;
	int i = 0;
	char cmd[128] = {0};
	cmd[127] = '\0';
	int try_cnt;
	for(try_cnt = 0; try_cnt < try_times; try_cnt++) {
		sprintf(cmd, "wpa_supplicant -i%s -Dnl80211 -c%s -O%s &",
				primary_iface1, SUPP_CONFIG_FILE, IFACE_DIR);
		system(cmd);
		while (i < 5) {
			sleep(1);
			ret = check_process_is_exist("wpa_supplicant", 14);
			if (ret)
				break;
			i++;
		}
		if (ret == 0) {
			WMG_ERROR("failed to start wpa_supplicant times %d\n", (try_cnt + 1));
		} else {
			WMG_DEBUG("start wpa_supplicant times %d success\n", (try_cnt + 1));
			return 0;
		}
	}
	return -1;
}

//====================================下面的wpa_conf的代码==============================================
static int search_status_str(char *status, const char *str, int offset, char *obj)
{
	int ret, len, max_len;
	char *pos = NULL, *pre = NULL;

	if (status == NULL || *status == '\0' || str == NULL || offset == 0 || obj == NULL) {
		WMG_ERROR("invalid parameters\n");
		return -1;
	}

	if (strncmp(str, "ssid=", 5) == 0)
		max_len = SSID_MAX_LEN;
	else
		max_len = BSSID_MAX_LEN;

	pos = strstr(status, str);
	if (pos != NULL) {
		pos += offset;
		pre = pos;
		pos = strchr(pos, '\n');
		len = (pos - pre > max_len) ? max_len : (pos - pre);
		strncpy(obj, pre, len);
		ret = 0;
	} else {
		WMG_ERROR("invalid parameters\n");
		ret = -1;
	}

	return ret;
}

static int wpa_parse_status_info(char *status, wifi_sta_info_t *sta_info)
{
	int ret = -1, len;
	char *pos = NULL, *pre = NULL;
	char tmp[SSID_MAX_LEN] = {0};

	char id[32] = {0};
	char freq[32] = {0};
	char bssid[BSSID_MAX_LEN] = {0};
	char ssid[SSID_MAX_LEN] = {0};
	char mac_addr[32] = {0};
	char ip_addr[32] = {0};
	char sec[32] = {0};
	int i;
	char *pch;

	if (status == NULL || sta_info == NULL) {
		WMG_ERROR("invalid parameters\n");
		return ret;
	}

	pos = strstr(status, "wpa_state");
	if (pos != NULL) {
		pos += 10;
		if (strncmp(pos, "COMPLETED", 9) != 0) {
			WMG_WARNG("Warning: wpa state is inactive\n");
			return -1;
		} else {
			WMG_DUMP("wpa state is completed\n");
			pch = strtok(status, "'\n'");
			while (pch != NULL) {
				if (strncmp(pch, "id=", 3) == 0) {
					strcpy(id, (pch + 3));
					WMG_DEBUG("%s\n", id);
				}
				if (strncmp(pch, "freq=", 5) == 0) {
					strcpy(freq, (pch + 5));
					WMG_DEBUG("%s\n", freq);
				}
				if (strncmp(pch, "bssid=", 6) == 0) {
					strcpy(bssid, (pch + 6));
					WMG_DEBUG("%s\n", bssid);
				}
				if (strncmp(pch, "ssid=", 5) == 0) {
					if((strlen(pch) - 5) > SSID_MAX_LEN) {
						WMG_DEBUG("ssid is too long:%d", (strlen(pch) - 5));
						return -1;
					}
					strcpy(ssid, (pch + 5));
					WMG_DEBUG("%s\n", ssid);
				}
				if (strncmp(pch, "address=", 8) == 0) {
					strcpy(mac_addr, (pch + 8));
					WMG_DEBUG("%s\n", mac_addr);
				}
				if (strncmp(pch, "ip_address=", 11) == 0) {
					strcpy(ip_addr, (pch + 11));
					WMG_DEBUG("%s\n", ip_addr);
				}
				if (strncmp(pch, "key_mgmt=", 9) == 0) {
					strcpy(sec, (pch + 9));
					WMG_DEBUG("%s\n", sec);
				}
				pch = strtok(NULL, "'\n'");
			}
			sta_info->id = atoi(id);
			sta_info->freq = atoi(freq);
			i = 0;
			pch = strtok(bssid, ":");
			for(;(pch != NULL) && (i < 6); i++){
				sta_info->bssid[i] = char2uint8(pch);
				pch = strtok(NULL, ":");
			}
			strcpy(sta_info->ssid, ssid);
			i = 0;
			pch = strtok(mac_addr, ":");
			for(;(pch != NULL) && (i < 6); i++){
				sta_info->mac_addr[i] = char2uint8(pch);
				pch = strtok(NULL, ":");
			}
			i = 0;
			pch = strtok(ip_addr, ".");
			for(;(pch != NULL) && (i < 4); i++){
				sta_info->ip_addr[i] = atoi(pch);
				pch = strtok(NULL, ".");
			}
			if (strncmp(sec, "WPA2-PSK", 7) == 0) {
				sta_info->sec = WIFI_SEC_WPA2_PSK;
			} else if (strncmp(tmp, "WPA-PSK", 6) == 0) {
				sta_info->sec = WIFI_SEC_WPA_PSK;
			} else {
				sta_info->sec = WIFI_SEC_NONE;
			}
			return 0;
		}
	} else {
		WMG_ERROR("status info is invalid\n");
		return -1;
	}
	return ret;
}

/**
 * get ap(ssid/key_mgmt) status in wpa_supplicant.conf
 * return
 * -1: not exist
 *  1: exist but not connected
 *  3: exist and connected; network id in buffer net_id
 */
static int wpa_conf_is_ap_exist(const char *ssid, wifi_secure_t key_mgmt, char *net_id, int *len)
{
	int ret = -1;
	char cmd[CMD_LEN + 1] = {0};
	char reply[REPLY_BUF_SIZE] = {0}, key_type[128], key_reply[128];
	char *pssid_start = NULL, *pssid_end = NULL, *ptr = NULL;
	int flag = 0;

	if (!ssid || !ssid[0]) {
		WMG_ERROR("ssid is NULL!\n");
		return -1;
	}

	/* parse key_type */
	if (key_mgmt == WIFI_SEC_WPA_PSK || key_mgmt == WIFI_SEC_WPA2_PSK) {
		strncpy(key_type, "WPA-PSK", 128);
	} else {
		strncpy(key_type, "NONE", 128);
	}

	strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
	cmd[CMD_LEN] = '\0';
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("do list networks error!\n");
		return -1;
	}

	ptr = reply;
	while ((pssid_start = strstr(ptr, ssid)) != NULL) {
		char *p_s = NULL, *p_e = NULL, *p = NULL;

		pssid_end = pssid_start + strlen(ssid);
		/* ssid is presuffix of searched network */
		if (*pssid_end != '\t') {
			p_e = strchr(pssid_start, '\n');
			if (p_e != NULL) {
				ptr = p_e;
				continue;
			} else {
				break;
			}
		}

		flag = 0;

		p_e = strchr(pssid_start, '\n');
		if (p_e) {
			*p_e = '\0';
		}
		p_s = strrchr(ptr, '\n');
		p_s++;

		if (strstr(p_s, "CURRENT")) {
			flag = 2;
		}

		p = strtok(p_s, "\t");
		if (p) {
			if (net_id != NULL && *len > 0) {
				strncpy(net_id, p, *len - 1);
				net_id[*len - 1] = '\0';
			}
		}

		/* get key_mgmt */
		sprintf(cmd, "GET_NETWORK %s key_mgmt", net_id);
		cmd[CMD_LEN] = '\0';
		ret = command_to_supplicant(cmd, key_reply, sizeof(key_reply));
		if (ret) {
			WMG_ERROR("do get network %s key_mgmt error!\n", net_id);
			return -1;
		}

		WMG_EXCESSIVE("GET_NETWORK %s key_mgmt reply %s\n", net_id, key_reply);
		WMG_EXCESSIVE("key type %s\n", key_type);

		if (strcmp(key_reply, key_type) == 0) {
			flag += 1;
			*len = strlen(net_id);
			break;
		}

		if (p_e == NULL) {
			break;
		} else {
			*p_e = '\n';
			ptr = p_e;
		}
	}

	return flag;
}

/*
 * ssid to netid
 */
static int wpa_conf_ssid2netid(const char *ssid, int *net_id)
{
	int ret = -1,i = 0;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[REPLY_BUF_SIZE] = {0};
	char *pch = NULL;
	char *delim = "'\n''\t'";
	int id = -1;
	int flags = 0;

	if (ssid == NULL) {
		WMG_ERROR("invalid parameters\n");
		return ret;
	}

	sprintf(cmd, "%s", "LIST_NETWORKS");
	cmd[CMD_LEN] = '\0';
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("failed to list networks, reply %s\n", reply);
		return ret;
	}

	if(strlen(reply) < 34){
		WMG_INFO("Here has no entry save\n");
		return -1;
	}

	pch = strtok((reply + 34), delim);
	while(pch != NULL){
		id = atoi(pch);
		pch = strtok(NULL, delim);
		if(strcmp(ssid, pch) == 0){
			*net_id = id;
			flags = 1;
			break;
		}
		pch = strtok(NULL, delim);
		pch = strtok(NULL, delim);
		pch = strtok(NULL, delim);
	};

	WMG_DEBUG("%s net_id is:%d\n", ssid, *net_id);
	if(flags == 1) {
		return 0;
	} else {
		return -1;
	}
}

/**
 * Get max priority val in wpa_supplicant.conf
 * return
 *-1: error
 * 0: no network
 * >0: max val
 */
static int wpa_conf_get_max_priority()
{
	int ret = -1;
	int val = -1, max_val = 0, len = 0;
	char cmd[CMD_LEN + 1] = {0}, reply[REPLY_BUF_SIZE] = {0}, priority[32] = {0};
	char net_id[NET_ID_LEN + 1];
	char *p_n = NULL, *p_t = NULL;

	/* list ap in wpa_supplicant.conf */
	strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
	cmd[CMD_LEN] = '\0';
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
        WMG_ERROR("do list networks error!\n");
        return -1;
	}

	p_n = strchr(reply, '\n');
	while(p_n != NULL) {
		p_n++;
		if ((p_t = strchr(p_n, '\t')) != NULL) {
			len = p_t - p_n;
			if (len <= NET_ID_LEN) {
				strncpy(net_id, p_n, len);
				net_id[len] = '\0';
			}
		}

        sprintf(cmd, "GET_NETWORK %s priority", net_id);
		ret = command_to_supplicant(cmd, priority, sizeof(priority));
		if (ret) {
			WMG_ERROR("do get network priority error!\n");
			return -1;
		}

		val = atoi(priority);
		if (val >= max_val) {
			max_val = val;
		}

		p_n = strchr(p_n, '\n');
	}

	return max_val;
}

static int wpa_conf_is_ap_connected(char *ssid, int *len)
{
	int ret = -1;
	char cmd[CMD_LEN+1] = {0};
	char reply[REPLY_BUF_SIZE] = {0};
	char *p_c = NULL, *p_str = NULL;
    char *p_s = NULL, *p_e = NULL, *p = NULL;
	int is_ap_connected = 0;
	int ssid_len = strlen(ssid);

    strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
	cmd[CMD_LEN] = '\0';

	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("do list networks error!\n");
		return -1;
	}

	p_str = (char *)reply;
	while ((p_c = strstr(p_str, "[CURRENT]")) != NULL) {
		if (*(p_c + 9) != '\n' && *(p_c + 9) != '\0') {
			p_str = p_c + 9;
			continue;
		}

		p_e = strchr(p_c, '\n');
		if (p_e) {
			*p_e = '\0';
		}

		p_s = strrchr(p_str, '\n');
		p_s++;
		p = strtok(p_s, "\t");
		p = strtok(NULL, "\t");
		ssid_len = strlen(p);
		if (p) {
			if (ssid != NULL && ssid_len > 0) {
                strncpy(ssid, p, ssid_len);
				ssid[ssid_len] = '\0';
				*len = strlen(ssid);
				is_ap_connected = 1;
				break;
			}
		}
	}

	/* check ip exist */
	ret = is_ip_exist();
	if (ret > 0 && is_ap_connected == 1) {
		return ret;
	} else {
		return is_ap_connected;
	}
}

static int wpa_conf_get_netid_connected(char *net_id, int *len)
{
	int ret = -1;
	char cmd[CMD_LEN + 1] = {0};
	char reply[REPLY_BUF_SIZE] = {0};
	char *p_c = NULL;

	strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
	cmd[CMD_LEN] = '\0';
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("do list networks error!\n");
		return -1;
	}

	if ((p_c = strstr(reply, "CURRENT")) != NULL) {
		char *p_s = NULL, *p_e = NULL, *p = NULL;
		p_e = strchr(p_c, '\n');
		if (p_e) {
			*p_e = '\0';
		}

		p_s = strrchr(reply, '\n');
		p_s++;
		p = strtok(p_s, "\t");
		if (p) {
			if (net_id != NULL && *len > 0) {
				strncpy(net_id, p, *len-1);
				net_id[*len - 1] = '\0';
				*len = strlen(net_id);
			}
		}
		return 1;
	} else {
		return 0;
	}
}

/*
 * 1. link to ap
 * 2. get ip addr
 *
*/
static int wpa_conf_get_ap_connected(char *netid, int *len)
{
	int ret = -1;
	char cmd[CMD_LEN + 1] = {0};
	char reply[REPLY_BUF_SIZE] = {0};
	char *p_c = NULL;

	strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
	cmd[CMD_LEN] = '\0';

	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("do list networks error!\n");
		return -1;
	}

	if ((p_c=strstr(reply, "CURRENT")) != NULL) {
		char *p_s = NULL, *p_e = NULL, *p = NULL;
		p_e = strchr(p_c, '\n');
		if (p_e) {
			*p_e = '\0';
		}

		p_s = strrchr(reply, '\n');
		p_s++;
		p = strtok(p_s, "\t");
		if (p) {
			if (netid != NULL && *len > 0) {
				strncpy(netid, p, *len - 1);
				netid[*len - 1] = '\0';
				*len = strlen(netid);
			}
		}
		WMG_EXCESSIVE("net id %s\n", netid);
		return 1;
	} else {
		WMG_EXCESSIVE("no CURRENT\n");
		return 0;
    }
}

static int wpa_conf_enable_all_networks()
{
	int ret = -1, len = 0;
	char cmd[CMD_LEN + 1] = {0};
	char reply[REPLY_BUF_SIZE] = {0};
	char net_id[NET_ID_LEN+1] = {0};
	char *p_n = NULL, *p_t = NULL;

	/* list ap in wpa_supplicant.conf */
    strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
	cmd[CMD_LEN] = '\0';
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("do list networks error!\n");
		return -1;
	}

	p_n = strchr(reply, '\n');
	while (p_n != NULL) {
		p_n++;
		if ((p_t = strchr(p_n, '\t')) != NULL) {
			len = p_t - p_n;
			if (len <= NET_ID_LEN) {
				strncpy(net_id, p_n, len);
				net_id[len] = '\0';
			}
		}

		/* cancel saved in wpa_supplicant.conf */
        sprintf(cmd, "ENABLE_NETWORK %s", net_id);
		ret = command_to_supplicant(cmd, reply, sizeof(reply));
		if (ret) {
			WMG_ERROR("do enable network %s error!\n", net_id);
			return -1;
		}

		p_n = strchr(p_n, '\n');
	}

	/* save config */
	sprintf(cmd, "%s", "SAVE_CONFIG");
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("do save config error!\n");
		return -1;
	}

	return 0;
}

static int check_wpa_password(const char *passwd)
{
	int ret = -1, i;

	if (!passwd || *passwd =='\0') {
		WMG_ERROR("password is NULL\n");
		return ret;
	}

	for (i = 0; passwd[i] !='\0'; i++) {
		/* non printable char */
		if ((passwd[i] < 32) || (passwd[i] > 126)) {
			ret = -1;
			break;
		}
	}

	if (passwd[i] == '\0') {
		ret = 0;
	}

	return ret;
}

static int wpa_conf_remove_maxnetid_network()
{
	int ret = -1, len = 0;
	char cmd[CMD_LEN + 1] = {0};
	char reply[REPLY_BUF_SIZE] = {0};
	char net_id[NET_ID_LEN + 1] = {0};
	char *p_n = NULL, *p_t = NULL;

	/* list ap in wpa_supplicant.conf */
    strncpy(cmd, "LIST_NETWORKS", CMD_LEN);
	cmd[CMD_LEN] = '\0';
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("do list networks error!\n");
		return -1;
	}

	p_n = strchr(reply, '\n');
	while(p_n != NULL){
		p_n++;
		if ((p_t = strchr(p_n, '\t')) != NULL) {
			len = p_t - p_n;
			if (len <= NET_ID_LEN) {
				strncpy(net_id, p_n, len);
				net_id[len] = '\0';
			}
		}
		p_n = strchr(p_n, '\n');
	}

	/* cancel saved in wpa_supplicant.conf */
	sprintf(cmd, "REMOVE_NETWORK %s", net_id);
	WMG_EXCESSIVE("remove network %s!\n", net_id);
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("do remove network %s error!\n", net_id);
		return -1;
	}

	/* save config */
	sprintf(cmd, "%s", "SAVE_CONFIG");
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("do save config error!\n");
		return -1;
	}

	return 0;
}

//====================================上面的wpa_conf的代码==============================================
static int linux_sta_wpa_cmd_disconnect()
{
	int ret;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	WMG_DUMP("disconnect ap\n");
	sprintf(cmd, "%s", "DISCONNECT");
	cmd[CMD_MAX_LEN] = '\0';
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret)
		WMG_ERROR("failed to disconnect ap, reply %s\n", reply);
	return ret;
}

static int linux_sta_wpa_cmd_reconnect()
{
	int ret;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	WMG_DUMP("reconnect ap\n");
	sprintf(cmd, "%s", "RECONNECT");
	cmd[CMD_MAX_LEN] = '\0';
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret)
		WMG_ERROR("failed to reconnect ap, reply %s\n", reply);
	return ret;
}

static int linux_sta_wpa_cmd_status()
{
	int ret;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	WMG_DUMP("wpa status\n");
	sprintf(cmd, "%s", "STATUS");
	cmd[CMD_MAX_LEN] = '\0';
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("failed to get sta status\n");
		return -1;
	}
	if (strstr(reply, "wpa_state=COMPLETED") != NULL) {
		WMG_DUMP("sta status is: connection\n");
		return 0;
	} else if (strstr(reply, "wpa_state=DISCONNECTED") != NULL) {
		WMG_DUMP("sta status is: disconnect\n");
		return 1;
	} else if (strstr(reply, "wpa_state=SCANNING") != NULL) {
		WMG_DUMP("sta status is: scanning\n");
		return 2;
	} else {
		WMG_DUMP("ap status is unknowd\n");
		return -1;
	}
}

static int wpas_select_network(wifi_sta_cn_para_t *cn_para, char *netid)
{
	int ret;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};
	int passwd_len = 0;

	sprintf(cmd, "%s", "ADD_NETWORK");
	cmd[CMD_MAX_LEN] = '\0';
	ret = command_to_supplicant(cmd, netid, sizeof(netid));
	if (ret) {
		WMG_ERROR("failed to add network, reply %s\n", netid);
		return WMG_STATUS_FAIL;
	}

	WMG_DEBUG("add network id=%s\n", netid);
	sprintf(cmd, "SET_NETWORK %s ssid \"%s\"", netid, cn_para->ssid);
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("failed to set network ssid '%s', reply %s\n", cn_para->ssid, reply);
		return WMG_STATUS_FAIL;
	}

	switch (cn_para->sec) {
	case WIFI_SEC_NONE:
		sprintf(cmd, "SET_NETWORK %s key_mgmt NONE", netid);
		ret = command_to_supplicant(cmd, reply, sizeof(reply));
		if (ret) {
			WMG_ERROR("failed to set network key_mgmt, reply %s\n", reply);
		return WMG_STATUS_FAIL;
		}
		break;
	case WIFI_SEC_WPA_PSK:
	case WIFI_SEC_WPA2_PSK:
		sprintf(cmd, "SET_NETWORK %s key_mgmt WPA-PSK", netid);
		ret = command_to_supplicant(cmd, reply, sizeof(reply));
		if (ret) {
			WMG_ERROR("failed to set network key_mgmt, reply %s\n", reply);
			return WMG_STATUS_FAIL;
		}

		/* check password */
		ret = check_wpa_password(cn_para->password);
		if (ret) {
			WMG_ERROR("password is invalid\n");
			return WMG_STATUS_INVALID;
		}
		sprintf(cmd, "SET_NETWORK %s psk \"%s\"", netid, cn_para->password);
		ret = command_to_supplicant(cmd, reply, sizeof(reply));
		if (ret) {
			WMG_ERROR("failed to set network psk, reply %s\n", reply);
			return WMG_STATUS_FAIL;
		}
		break;
	case WIFI_SEC_WEP:
		sprintf(cmd, "SET_NETWORK %s key_mgmt NONE", netid);
		ret = command_to_supplicant(cmd, reply, sizeof(reply));
		if (ret) {
			WMG_ERROR("failed to set network key_mgmt, reply %s\n", reply);
		return WMG_STATUS_FAIL;
		}

		passwd_len = strlen(cn_para->password);
		if((passwd_len == 10) || (passwd_len == 26)) {
			sprintf(cmd, "SET_NETWORK %s wep_key0 %s", netid, cn_para->password);
			WMG_DEBUG("The passwd is HEX format!\n");
		} else if((passwd_len == 5) || (passwd_len == 13)) {
			sprintf(cmd, "SET_NETWORK %s wep_key0 \"%s\"", netid, cn_para->password);
			WMG_DEBUG("The passwd is ASCII format!\n");
		} else {
			WMG_ERROR("The password does not conform to the specification!\n");
			return WMG_STATUS_FAIL;
		}
		ret = command_to_supplicant(cmd, reply, sizeof(reply));
		if (ret) {
			WMG_ERROR("failed to set network psk, reply %s\n", reply);
			return WMG_STATUS_FAIL;
		}
		sprintf(cmd, "SET_NETWORK %s auth_alg OPEN SHARED", netid);
		ret = command_to_supplicant(cmd, reply, sizeof(reply));
		if (ret) {
			WMG_ERROR("failed to set network auth_alg OPEN SHARED, reply %s\n", reply);
			return WMG_STATUS_FAIL;
		}
		break;
	default:
		WMG_ERROR("unknown key mgmt\n");
		return WMG_STATUS_FAIL;
	}

	sprintf(cmd, "SELECT_NETWORK %s", netid);
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("failed to select network %s, reply %s\n", netid, reply);
		return WMG_STATUS_FAIL;
	}

	return ret;
}

static int wpa_set_auto_reconn(wmg_bool_t enable)
{
	int ret;
	WMG_DEBUG("get reconnn flag:%d\n",enable);
	sta_wpa_inf_object.sta_wpa_auto_reconn = enable;
	if(enable == false){
		WMG_DEBUG("send disconnet cmd\n");
		if(linux_sta_wpa_cmd_status() == 2){
			ret = linux_sta_wpa_cmd_disconnect();
			if (ret) {
				WMG_ERROR("failed to send disconnect cmd\n");
				return ret;
			}
		}
	} else {
		linux_sta_wpa_cmd_reconnect();
	}
}

static int wpa_get_scan_results(sta_get_scan_results_para_t *sta_scan_results_para)
{
	wmg_status_t ret;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[SCAN_BUF_LEN] = {0};
	int event = -1, ret_tmp;
	int try_cnt = 0;

scan_once:
	evt_socket_clear(sta_wpa_inf_object.sta_event_handle);
	sprintf(cmd, "%s", "SCAN");
	cmd[CMD_MAX_LEN] = '\0';
	ret_tmp = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret_tmp) {
		if (strncmp(reply, "FAIL-BUSY", 9) == 0) {
			WMG_DUMP("wpa_supplicant is scanning internally\n");
			sleep(2);
			goto scan_results;
		}
		ret = WMG_STATUS_FAIL;
		goto err;
	}

	while (try_cnt < SCAN_TRY_MAX) {
		ret_tmp = evt_read(sta_wpa_inf_object.sta_event_handle, &event);
		if (ret_tmp > 0) {
			WMG_DUMP("receive wpas event '%s'\n", wmg_sta_event_to_str(event));
			if (event == WIFI_SCAN_RESULTS) {
				break;
			} else if (event == WIFI_SCAN_FAILED) {
				if (try_cnt >= 2) {
					ret = WMG_STATUS_FAIL;
					goto err;
				}
				try_cnt++;
				goto scan_once;
			} else {
				try_cnt++;
			}
		} else {
			try_cnt++;
		}
	}

	if (try_cnt == SCAN_TRY_MAX && event != WIFI_SCAN_RESULTS) {
		ret = WMG_STATUS_FAIL;
		goto err;
	}

scan_results:
	try_cnt = 0;
	sprintf(cmd, "%s", "SCAN_RESULTS");
	cmd[CMD_MAX_LEN] = '\0';
	ret_tmp = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret_tmp) {
		sleep(1);
		try_cnt++;
		if (try_cnt == 2) {
			ret = WMG_STATUS_FAIL;
			goto err;
		}
		goto scan_results;
	}

	remove_slash_from_scan_results(reply);
	ret_tmp = parse_scan_results(reply, sta_scan_results_para->scan_results, sta_scan_results_para->bss_num, sta_scan_results_para->arr_size);
	if (ret_tmp) {
		WMG_ERROR("failed to parse scan results\n");
		ret = WMG_STATUS_FAIL;
	} else {
		ret = WMG_STATUS_SUCCESS;
	}

err:
	return ret;
}

static void wpas_event_notify_to_sta_dev(wifi_sta_event_t event)
{
	if (sta_wpa_inf_object.sta_wpa_event_cb) {
		sta_wpa_inf_object.sta_wpa_event_cb(event);
	}
}

static void wpas_event_notify(wifi_sta_event_t event)
{
	evt_send(sta_wpa_inf_object.sta_event_handle, event);
	wpas_event_notify_to_sta_dev(event);
}

static int wpas_dispatch_event(const char *event_str, int nread)
{
	int ret, i = 0, event = 0;
	char event_nae[CMD_MAX_LEN];
	char cmd[CMD_MAX_LEN + 1] = {0}, reply[EVENT_BUF_SIZE] = {0};
	char *nae_start = NULL, *nae_end = NULL;

	if (!event_str || !event_str[0]) {
		WMG_WARNG("wpa_supplicant event is NULL!\n");
		return 0;
	}

	WMG_DUMP("receive wpa event: %s\n", event_str);
	if (strncmp(event_str, "CTRL-EVENT-", 11) != 0) {
		//这里还要看下
		if (!strncmp(event_str, "WPA:", 4)) {
			if (strstr(event_str, "pre-shared key may be incorrect")){
				sta_wpa_inf_object.sta_auth_fail_cnt++;
				event = WIFI_PASSWORD_INCORRECT;
				wpas_event_notify(event);
				return 0;
			}
		}
		return 0;
	}

	nae_start = (char *)((unsigned long)event_str + 11);
	nae_end = strchr(nae_start, ' ');
	if (nae_end) {
		while ((nae_start < nae_end) && (i < 18)) {
			event_nae[i] = *nae_start++;
			i++;
		}
		event_nae[i] = '\0';
	} else {
		WMG_DUMP("received wpa_supplicant event with empty event nae!\n");
		return 0;
	}

	WMG_DUMP("event name:%s\n", event_nae);
	if (!strcmp(event_nae, "DISCONNECTED")) {
		event = WIFI_DISCONNECTED;
		wpas_event_notify(event);
		if(sta_wpa_inf_object.sta_wpa_auto_reconn == WMG_TRUE) {
			linux_sta_wpa_cmd_reconnect();
		}
	} else if (!strcmp(event_nae, "SCAN-STARTED")) {
		event = WIFI_SCAN_STARTED;
		wpas_event_notify(event);
	} else if (!strcmp(event_nae, "SCAN-RESULTS")) {
		event = WIFI_SCAN_RESULTS;
		wpas_event_notify(event);
	} else if (!strcmp(event_nae, "SCAN-FAILED")) {
		event = WIFI_SCAN_FAILED;
		wpas_event_notify(event);
	} else if (!strcmp(event_nae, "NETWORK-NOT-FOUND")) {
		event = WIFI_NETWORK_NOT_FOUND;
		wpas_event_notify(event);
	} else if (!strcmp(event_nae, "AUTH-REJECT")) {
		event = WIFI_AUTH_REJECT;
		wpas_event_notify(event);
	} else if(!strcmp(event_nae, "ASSOC-REJECT")) {
		event = WIFI_ASSOC_REJECT;
		wpas_event_notify(event);
		sprintf(cmd, "%s", "DISCONNECT");
		cmd[CMD_MAX_LEN] = '\0';
		ret = command_to_supplicant(cmd, reply, sizeof(reply));
		if (ret) {
			WMG_ERROR("failed to disconnect from ap, reply %s\n", reply);
		}
	} else if (!strcmp(event_nae, "CONNECTED")) {
		event = WIFI_CONNECTED;
		wpas_event_notify(event);
		wpas_event_notify(WIFI_DHCP_START);
		start_udhcpc();
		if (is_ip_exist()) {
			wpas_event_notify_to_sta_dev(WIFI_DHCP_SUCCESS);
		} else {
			wpas_event_notify_to_sta_dev(WIFI_DHCP_TIMEOUT);
		}
	} else if (!strcmp(event_nae, "TERMINATING")) {
		event = WIFI_TERMINATING;
		wpas_event_notify(event);
		return 1;
	} else {
		event = WIFI_UNKNOWN;
	}

	return 0;
}

static void *wpas_event_thread(void *args)
{
	char buf[EVENT_BUF_SIZE] = {0};
	int size, ret;

	for (;;) {
		size = wifi_wait_on_socket(buf, sizeof(buf));
		if (size > 0) {
			ret = wpas_dispatch_event(buf, size);
			if (ret) {
				WMG_DUMP("wpa_supplicant terminated\n");
				break;
			}
		} else {
			continue;
		}
	}

	pthread_exit(NULL);
}

static int linux_supplicant_init(sta_wpa_event_cb_t sta_event_cb)
{
	if(sta_wpa_inf_object.sta_wpa_init_flag == WMG_FALSE) {
		WMG_INFO("linux supplicant init now\n");
		sta_wpa_inf_object.sta_event_handle = (event_handle_t *)malloc(sizeof(event_handle_t));
		if(sta_wpa_inf_object.sta_event_handle != NULL) {
			memset(sta_wpa_inf_object.sta_event_handle, 0, sizeof(event_handle_t));
			sta_wpa_inf_object.sta_event_handle->evt_socket[0] = -1;
			sta_wpa_inf_object.sta_event_handle->evt_socket[1] = -1;
			sta_wpa_inf_object.sta_event_handle->evt_socket_enable = WMG_FALSE;
		} else {
			WMG_ERROR("failed to allocate memory for linux wpa event_handle\n");
			return WMG_STATUS_FAIL;
		}
		if(sta_event_cb != NULL){
			sta_wpa_inf_object.sta_wpa_event_cb = sta_event_cb;
		}
		sta_wpa_inf_object.sta_wpa_init_flag = WMG_TRUE;
	} else {
		WMG_INFO("linux supplicant already init\n");
	}
	return WMG_STATUS_SUCCESS;
}

static void linux_supplicant_deinit(void)
{
	if(sta_wpa_inf_object.sta_wpa_init_flag == WMG_TRUE) {
		WMG_INFO("linux supplicant deinit now\n");
		if(sta_wpa_inf_object.sta_pid != -1) {
			os_net_thread_delete(&sta_wpa_inf_object.sta_pid);
			sta_wpa_inf_object.sta_pid = -1;
		}
		evt_socket_exit(sta_wpa_inf_object.sta_event_handle);
		wifi_close_sockets();

		if (check_process_is_exist("wpa_supplicant", 14) == 1) {
			system("pidof wpa_supplicant | xargs kill -9");
			sleep(1);
			if (check_process_is_exist("wpa_supplicant", 14) == 1) {
				WMG_ERROR("kill supplicant failed\n");
				return;
			}
			sta_wpa_inf_object.sta_wpa_exist = WMG_FALSE;
		}
		system("ifconfig wlan0 down");
		free(sta_wpa_inf_object.sta_event_handle);
		sta_wpa_inf_object.sta_wpa_init_flag = WMG_FALSE;
		sta_wpa_inf_object.sta_wpa_auto_reconn = WMG_FALSE;
		sta_wpa_inf_object.sta_auth_fail_cnt = 0;
		sta_wpa_inf_object.sta_net_not_found_cnt = 0;
		sta_wpa_inf_object.sta_assoc_reject_cnt = 0;
		sta_wpa_inf_object.sta_wpa_event_cb = NULL;
	} else {
		WMG_DEBUG("linux supplicant already deinit\n");
	}
	return;
}

/* Establishes the control and monitor socket connections on the interface */
static int linux_connect_to_supplicant()
{
    static char path[PATH_MAX];
	int ret = 0;

	if (check_process_is_exist("hostapd", 7) == 1) {
		WMG_DEBUG("hostapd is running, need to stop it\n");
		system("pidof hostapd | xargs kill -9");
		if (check_process_is_exist("hostapd", 7) == 1) {
			sleep(1);
			if (check_process_is_exist("hostapd", 7) == 1) {
				WMG_ERROR("kill hostapd failed\n");
				return WMG_STATUS_FAIL;
			}
		}
	}

	if (check_process_is_exist("wpa_supplicant", 14) == 0) {
		WMG_DEBUG("wpa_supplicant is not running, need to start it\n");

		if(start_supplicant(2)){
			WMG_ERROR("start wpa_supplicant failed\n");
			return WMG_STATUS_FAIL;
		}

		WMG_EXCESSIVE("start wpa_supplicant success\n");
	} else {
		WMG_EXCESSIVE("wpa_supplicant is already running\n");
	}

	//strncpy(primary_iface, "wlan0", IFACE_VALUE_MAX);
	if (access(IFACE_DIR, F_OK) == 0) {
		snprintf(path, sizeof(path), "%s/%s", IFACE_DIR, primary_iface1);
	} else {
		WMG_ERROR("wpa_supplicant socket interface not exists\n");
		return WMG_STATUS_FAIL;
	}
	if(wifi_connect_on_socket_path(path))
	{
		return WMG_STATUS_FAIL;
	}

	ret = evt_socket_init(sta_wpa_inf_object.sta_event_handle);
	if (ret) {
		WMG_ERROR("failed to initialize linux sta event socket\n");
		ret = WMG_STATUS_FAIL;
		goto wpa_socket_err;
	}

	ret = evt_socket_clear(sta_wpa_inf_object.sta_event_handle);
	if (ret) {
		WMG_WARNG("failed to clear linux sta event socket\n");
	}

	ret = os_net_thread_create(&sta_wpa_inf_object.sta_pid, NULL, wpas_event_thread, NULL, 0, 4096);
	if (ret) {
		WMG_ERROR("failed to create linux sta event handle thread\n");
		ret = WMG_STATUS_FAIL;
		goto socket_err;
	}
	WMG_DUMP("create linux sta event handle thread success\n");

	return WMG_STATUS_SUCCESS;

socket_err:
	evt_socket_exit(sta_wpa_inf_object.sta_event_handle);
wpa_socket_err:
	wifi_close_sockets();
	return ret;
}

static int linux_supplicant_connect_to_ap(wifi_sta_cn_para_t *cn_para)
{
	wmg_status_t ret;
	int try_cnt = 0, len, ret_tmp;
	int event = -1;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};
	char netid[CMD_MAX_LEN + 1] = {0};

	len = CMD_MAX_LEN + 1;
	ret_tmp = wpa_conf_is_ap_exist(cn_para->ssid, cn_para->sec, netid, &len);
	if (ret_tmp > 0) {
		WMG_DUMP("current bss is exist\n");
		if (ret_tmp == 3) {
			WMG_DUMP("wifi is already connected to %s\n", cn_para->ssid);
			wpas_event_notify_to_sta_dev(WIFI_CONNECTED);
			return WMG_STATUS_SUCCESS;
		} else {
			sprintf(cmd, "REMOVE_NETWORK %s", netid);
			cmd[CMD_MAX_LEN] = '\0';
			ret = command_to_supplicant(cmd, reply, sizeof(reply));
			if (ret) {
				WMG_ERROR("failed to remove network %s, reply %s\n", netid, reply);
				wpas_event_notify_to_sta_dev(WIFI_DISCONNECTED);
				return WMG_STATUS_FAIL;
			}
		}
	} else {
		WMG_WARNG("current bss is not exist, need to add it\n");
	}

	/* check ssid contains chinese or not */
	ret_tmp = evt_socket_clear(sta_wpa_inf_object.sta_event_handle);
	if (ret_tmp)
		WMG_WARNG("failed to clear event socket\n");

	ret_tmp = wpas_select_network(cn_para, netid);
	if (ret_tmp) {
		WMG_ERROR("failed to config network\n");
		wpas_event_notify_to_sta_dev(WIFI_DISCONNECTED);
		return WMG_STATUS_FAIL;
	}

read_event:
	ret = WMG_STATUS_FAIL;
	/* wait connect event */
	while (try_cnt < EVENT_TRY_MAX) {
		ret_tmp = evt_read(sta_wpa_inf_object.sta_event_handle, &event);
		if (ret_tmp > 0) {
			WMG_DUMP("receive wpas event '%s'\n", wmg_sta_event_to_str(event));
			if (event == WIFI_CONNECTED) {
				ret = WMG_STATUS_SUCCESS;
				break;
			} else if (event == WIFI_PASSWORD_INCORRECT) {
				sta_wpa_inf_object.sta_auth_fail_cnt++;
				break;
			} else if (event == WIFI_NETWORK_NOT_FOUND) {
				sta_wpa_inf_object.sta_net_not_found_cnt++;
			} else if (event == WIFI_ASSOC_REJECT) {
				sta_wpa_inf_object.sta_assoc_reject_cnt++;
				break;
			} else {
				/* other event */
				try_cnt++;
			}
		} else {
			try_cnt++;
		}
	}

	if (ret == WMG_STATUS_SUCCESS) {
		sprintf(cmd, "%s", "SAVE_CONFIG");
		cmd[CMD_MAX_LEN] = '\0';
		ret_tmp = command_to_supplicant(cmd, reply, sizeof(reply));
		if (!ret_tmp)
			WMG_DUMP("save config to wpa_supplicant.conf success\n");
		else
			WMG_WARNG("failed to save config to wpa_supplicant.conf\n");
	}

	if (ret != WMG_STATUS_SUCCESS) {
		sprintf(cmd, "%s %s", "REMOVE_NETWORK", netid);
		cmd[CMD_MAX_LEN] = '\0';
		ret_tmp = command_to_supplicant(cmd, reply, sizeof(reply));
		if (!ret_tmp)
			WMG_DUMP("remove network %s because of connection failure\n", netid);
		else
			WMG_WARNG("failed to remove network %s\n", netid);
	}

	return ret;
}

static int linux_supplicant_disconnect_to_ap(void)
{
	int ret;
	if(linux_sta_wpa_cmd_status() != 0){
		WMG_DEBUG("sta is disconnected, need not to disconnect to ap\n");
		return WMG_STATUS_SUCCESS;
	}
	ret = linux_sta_wpa_cmd_disconnect();
	if (ret) {
		WMG_ERROR("failed to disconnect ap\n");
		return ret;
	}
	system("ifconfig wlan0 0.0.0.0");
	sta_wpa_inf_object.sta_wpa_auto_reconn = WMG_FALSE;
	return WMG_STATUS_SUCCESS;
}

static int linux_supplicant_get_info(wifi_sta_info_t *sta_info)
{
	int ret;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	sprintf(cmd, "%s", "STATUS");
	cmd[CMD_MAX_LEN] = '\0';
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("failed to get status of wifi station, reply %s\n", reply);
		return WMG_STATUS_FAIL;
	}
	ret = wpa_parse_status_info(reply, sta_info);
	if(ret) {
		WMG_ERROR("failed to parse station info\n");
		return WMG_STATUS_FAIL;
	}
	return WMG_STATUS_SUCCESS;
}

static int linux_supplicant_list_networks(wifi_sta_list_t *sta_list)
{
	int ret = -1, i = 0, list_entry_num = 0;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[REPLY_BUF_SIZE] = {0};
	char *pch_entry_p[LIST_ENTRY_NUME_MAX] = {0};
	char *pch_entry = NULL;
	char *pch_item = NULL;
	char *delim_entry = "'\n'";
	char *delim_item = "'\t'";

	if (sta_list == NULL) {
		WMG_ERROR("invalid parameters\n");
		return ret;
	}

	if(sta_list->list_num > LIST_ENTRY_NUME_MAX) {
		WMG_WARNG("Sys only support list %d entry\n", LIST_ENTRY_NUME_MAX);
		list_entry_num = LIST_ENTRY_NUME_MAX;
	} else {
		list_entry_num = sta_list->list_num;
	}

	sprintf(cmd, "%s", "LIST_NETWORKS");
	cmd[CMD_LEN] = '\0';
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("failed to list networks, reply %s\n", reply);
		return ret;
	}

	if(strlen(reply) < 34){
		WMG_INFO("Here has no entry save\n");
		return 0;
	}

	pch_entry = strtok((reply + 34), delim_entry);
	while((pch_entry != NULL) && (i < list_entry_num)) {
		pch_entry_p[i] = pch_entry;
		i++;
		pch_entry = strtok(NULL, delim_entry);
	}

	sta_list->sys_list_num = i;
	WMG_DEBUG("sys save num is:%d, list buff num is:%d\n", i, sta_list->list_num);

	for(i = 0; pch_entry_p[i] != NULL; i++) {
		pch_item = strtok(pch_entry_p[i], delim_item);
		if(pch_item != NULL) {
			sta_list->list_nod[i].id = atoi(pch_item);
		}
		pch_item = strtok(NULL, delim_item);
		if(pch_item != NULL) {
			strcpy((sta_list->list_nod[i].ssid), pch_item);
		}
		pch_item = strtok(NULL, delim_item);
		if(pch_item != NULL) {
			strcpy((sta_list->list_nod[i].bssid), pch_item);
		}
		pch_item = strtok(NULL, delim_item);
		if(pch_item != NULL) {
			strcpy((sta_list->list_nod[i].flags), pch_item);
		} else {
			strcpy((sta_list->list_nod[i].flags), "NULL");
		}
	}

	return 0;
}

static int linux_supplicant_remove_networks(char *ssid)
{
	int ret = -1;
	char cmd[CMD_LEN +1 ] = {0};
	char reply[REPLY_BUF_SIZE] = {0};
	int net_id;
	char *pch = NULL;
	char *delim = "'\n''\t'";

	if(ssid != NULL) {
		WMG_DEBUG("remove network(%s) ...\n", ssid);
		/* check AP is exist in wpa_supplicant.conf */
		ret = wpa_conf_ssid2netid(ssid, &net_id);
		if (ret != 0) {
			WMG_WARNG("%s is not in wpa_supplicant.conf!\n", ssid);
			return -1;
		}

		/* cancel saved in wpa_supplicant.conf */
		cmd[CMD_MAX_LEN] = '\0';
		sprintf(cmd, "REMOVE_NETWORK %d", net_id);
		ret = command_to_supplicant(cmd, reply, sizeof(reply));
		if (ret) {
			WMG_ERROR("do remove network %s error!\n", net_id);
			return -1;
		}

	} else {
		WMG_DEBUG("remove all network ...\n", ssid);
		sprintf(cmd, "%s", "LIST_NETWORKS");
		cmd[CMD_LEN] = '\0';
		ret = command_to_supplicant(cmd, reply, sizeof(reply));
		if (ret) {
			WMG_ERROR("failed to list networks, reply %s\n", reply);
			return ret;
		}

		if(strlen(reply) < 34){
			WMG_INFO("Here has no entry save\n");
			return -1;
		}

		pch = strtok((reply + 34), delim);
		while(pch != NULL){
			cmd[CMD_MAX_LEN] = '\0';
			sprintf(cmd, "REMOVE_NETWORK %s", pch);
			ret = command_to_supplicant(cmd, reply, sizeof(reply));
			if (ret) {
				WMG_ERROR("do remove network %s error!\n", pch);
				return -1;
			}
			pch = strtok(NULL, delim);
			pch = strtok(NULL, delim);
			pch = strtok(NULL, delim);
			pch = strtok(NULL, delim);
		};
	}

	/* save config */
	sprintf(cmd, "%s", "SAVE_CONFIG");
	ret = command_to_supplicant(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("do save config error!\n");
		return -1;
	}

	return ret;
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

static int linux_command_to_supplicant(char const *cmd, char *reply, size_t reply_len)
{
	return command_to_supplicant(cmd, reply, reply_len);
}

static int linux_platform_extension(int cmd, void* cmd_para,int *erro_code)
{
	switch (cmd) {
		case WPA_CMD_GET_INFO:
			return linux_supplicant_get_info((wifi_sta_info_t *)cmd_para);
		case WPA_CMD_LIST_NETWORKS:
			return linux_supplicant_list_networks((wifi_sta_list_t *)cmd_para);
		case WPA_CMD_REMOVE_NETWORKS:
			return linux_supplicant_remove_networks((char *)cmd_para);
		case WPA_CMD_CONF_IS_AP_EXIST:
			{
				conf_is_ap_exist_para_t* is_ap_exist_para = (conf_is_ap_exist_para_t *)cmd_para;
				return wpa_conf_is_ap_exist(is_ap_exist_para->ssid, is_ap_exist_para->key_mgmt, is_ap_exist_para->net_id, is_ap_exist_para->len);
			}
		case WPA_CMD_CONF_GET_MAX_PRORITY:
			{
				int* max_val = (int*)cmd_para;
				return wpa_conf_get_max_priority();
			}
		case WPA_CMD_CONF_IS_AP_CONNECTED:
			{
				conf_is_ap_connected_para_t *is_ap_connected_para = (conf_is_ap_connected_para_t *)cmd_para;
				return wpa_conf_is_ap_connected(is_ap_connected_para->ssid, is_ap_connected_para->len);
			}
		case WPA_CMD_CONF_GET_NETID_CONNECTED:
			{
				conf_get_netid_connected_para_t *get_netid_connected_para = (conf_get_netid_connected_para_t *)cmd_para;
				return wpa_conf_get_netid_connected(get_netid_connected_para->net_id, get_netid_connected_para->len);
			}
		case WPA_CMD_CONF_GET_AP_CONNECTED:
			{
				conf_get_ap_connected_para_t *get_ap_connected_para = (conf_get_ap_connected_para_t *)cmd_para;
				return wpa_conf_get_ap_connected(get_ap_connected_para->net_id, get_ap_connected_para->len);
			}
		case WPA_CMD_CONF_ENABLE_ALL_NETWORKS:
			return wpa_conf_enable_all_networks();
		case WPA_CMD_CONF_REMOVE_MAXNETID_NETWORK:
			return wpa_conf_remove_maxnetid_network();
		case WPA_CMD_SET_AUTO_RECONN:
			{
				wmg_bool_t *enable = (wmg_bool_t *)cmd_para;
				return wpa_set_auto_reconn(*enable);
			}
		case WPA_CMD_GET_SCAN_RESULTS:
			{
				sta_get_scan_results_para_t *sta_scan_results_para = (sta_get_scan_results_para_t *)cmd_para;
				return wpa_get_scan_results(sta_scan_results_para);
			}
		case WPA_CMD_SET_MAC:
			{
				common_mac_para_t * common_mac_para = (common_mac_para_t *)cmd_para;
				return linux_set_mac(common_mac_para->ifname, common_mac_para->mac_addr);
			}
		case WPA_CMD_GET_MAC:
			{
				common_mac_para_t * common_mac_para = (common_mac_para_t *)cmd_para;
				return linux_get_mac(common_mac_para->ifname, common_mac_para->mac_addr);
			}
		default:
		return WMG_FALSE;
	}
	return WMG_FALSE;
}

static wmg_sta_wpa_inf_object_t sta_wpa_inf_object = {
	.sta_wpa_init_flag = WMG_FALSE,
	.sta_wpa_exist = WMG_FALSE,
	.sta_wpa_connected = WMG_FALSE,
	.sta_wpa_mutex = PTHREAD_MUTEX_INITIALIZER,
	.sta_pid = -1,
	.sta_wpa_auto_reconn = WMG_FALSE,
	.sta_auth_fail_cnt = 0,
	.sta_net_not_found_cnt = 0,
	.sta_assoc_reject_cnt = 0,
	.sta_wpa_event_cb = NULL,

	.sta_wpa_init = linux_supplicant_init,
	.sta_wpa_deinit = linux_supplicant_deinit,
	.sta_wpa_connect = linux_connect_to_supplicant,
	.sta_wpa_connect_to_ap = linux_supplicant_connect_to_ap,
	.sta_wpa_disconnect = linux_supplicant_disconnect_to_ap,
	.sta_wpa_command = linux_command_to_supplicant,
	.sta_platform_extension = linux_platform_extension,
};

wmg_sta_wpa_inf_object_t* sta_linux_inf_object_register(void)
{
	return &sta_wpa_inf_object;
}
