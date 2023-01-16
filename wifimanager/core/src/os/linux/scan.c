#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "event.h"
#include "scan.h"
#include "linux_wpa.h"
#include "wifi_log.h"
#include "utils.h"
#include "wmg_sta.h"

#define WAITING_CLK_COUNTS   50
#define SSID_LEN	512
#define TRY_SAN_MAX 6

static struct net_scan scan = {
	.results_len = 0,
	.try_scan_count = 0,
	.enable = 0,
};

/**
 * Format of scan results are following:
 * bssid / frequency / signal level / flags / ssid
 * 00:1c:a3:14:6a:de       2462    -32     [WPA2-PSK-CCMP][ESS]    AW-PD4-R818
 * 50:d2:f5:f1:b7:08       2412    -56     [WPA-PSK-CCMP+TKIP][WPA2-PSK-CCMP+TKIP][WPS][ESS]       AW-PDC-PD2-fly2.4g
 */
int parse_scan_results(char *recv_results, wifi_scan_result_t *scan_results,
		uint32_t *bss_num, uint32_t arr_size)
{
	int ret, i, len = 0, res_len;
	char *pos = NULL, *pre = NULL;
	char tmp[20] = {0};

	if (recv_results == NULL || scan_results == NULL ||
		bss_num == NULL || arr_size == 0) {
		WMG_ERROR("invalid parameters\n");
		return -1;
	} else {
		*bss_num = 0;
		WMG_DEBUG("scan arr size buff len: %d\n", arr_size);
	}

	res_len = strlen(recv_results);
	WMG_EXCESSIVE("scan results length is %d\n", res_len);

	pos = strchr(recv_results, '\n');
	if (pos == NULL) {
		WMG_WARNG("scan results is NULL\n");
		return -1;
	}
	pos++;  /* pointer to the first bss info */
	while (*pos != '\0') {
		pre = pos;
		pos = strchr(pos, '\t');  /* pointer to the tab at the end of bssid */
		len = pos - pre;  /* calculate the length of bssid */
		if(*bss_num < arr_size) {
			strncpy(scan_results->bssid, pre, len);
			memset(scan_results->bssid + len, '\0', 1);
			WMG_EXCESSIVE("bssid=%s\n", scan_results->bssid);
		}

		pos++;  /* pointer to frequency */
		pre = pos;
		pos = strchr(pos, '\t');
		len = pos - pre;
		strncpy(tmp, pre, len);
		if(*bss_num < arr_size) {
			scan_results->freq = atoi(tmp);
			WMG_EXCESSIVE("freq=%d\n", scan_results->freq);
		}

		memset(tmp, 0, sizeof(tmp));
		pos++;  /* pointer to signal level */
		pre = pos;
		pos = strchr(pos, '\t');
		len = pos - pre;
		strncpy(tmp, pre, len);
		if(*bss_num < arr_size) {
			scan_results->rssi = atoi(tmp);
			WMG_EXCESSIVE("rssi=%d\n", scan_results->rssi);
		}

		pos++;  /* pointer to encryption flags */
		pos++;  /* ignore '[' */
		if(*bss_num < arr_size) {
			if (strncmp(pos, "WPA2-PSK", 8) == 0)
				scan_results->key_mgmt = WIFI_SEC_WPA2_PSK;
			else if (strncmp(pos, "WPA-PSK", 7) == 0)
				scan_results->key_mgmt = WIFI_SEC_WPA_PSK;
			else
				scan_results->key_mgmt = WIFI_SEC_NONE;
			WMG_EXCESSIVE("key_mgmt=%d\n", scan_results->key_mgmt);
		}

		pos = strchr(pos, '\t');
		pos++;  /* pointer to ssid */
		pre = pos;
		pos = strchr(pos, '\n');
		if (pos == NULL) {
			/* current bss is the last one */
			if(*bss_num < arr_size) {
				len = strlen(pre);
				if (len >= SSID_MAX_LEN)
					len = 0;
				if (len != 0) {
					strncpy(scan_results->ssid, pre, len);
					memset(scan_results->ssid + len, '\0', 1);
				} else {
					memset(scan_results->ssid, '\0', 1);
				}
				WMG_EXCESSIVE("ssid=%s len=%d\n", scan_results->ssid, len);
			}
			(*bss_num)++;
			break;
		} else {
			if(*bss_num < arr_size) {
				len = (pos - pre >= SSID_MAX_LEN) ? 0 : (pos - pre);
				if (len != 0) {
					strncpy(scan_results->ssid, pre, len);
					memset(scan_results->ssid + len, '\0', 1);
				} else {
					memset(scan_results->ssid, '\0', 1);
				}
				WMG_EXCESSIVE("ssid=%s len=%d\n", scan_results->ssid, len);
				scan_results++;  /* store next bss info */
			}
			pos++;  /* pointer to next bss info */
			(*bss_num)++;
		}
	}

	return 0;
}

void remove_slash_from_scan_results(char *recv_results)
{
	char *ptr = NULL;
	char *ptr_s = NULL;
	char *ftr = NULL;

	ptr_s = recv_results;
	while(1) {
		ptr = strchr(ptr_s,'\"');
		if (ptr == NULL)
			break;

		ptr_s = ptr;
		if ((*(ptr - 1)) == '\\') {
			ftr = ptr;
			ptr -= 1;
			while (*ftr != '\0')
				*(ptr++) = *(ftr++);
			*ptr = '\0';
			continue; //restart new search at ptr_s after removing slash
		} else
			ptr_s++; //restart new search at ptr_s++
    }

	ptr_s = recv_results;
	while(1) {
		ptr = strchr(ptr_s,'\\');
		if (ptr == NULL)
			break;

		ptr_s = ptr;
		if((*(ptr - 1)) == '\\') {
			ftr = ptr;
			ptr -= 1;
			while (*ftr != '\0')
				*(ptr++) = *(ftr++);
			*ptr = '\0';
			continue; //restart new search at ptr_s after removing slash
		} else
			ptr_s++; //restart new search at ptr_s++
	}

	//return 0;
}
