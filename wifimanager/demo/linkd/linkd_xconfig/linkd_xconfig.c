#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <wifimg.h>
#include <wifi_log.h>
#include <linkd.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <endian.h>
#include <wifi_log.h>
#include "wifimg.h"
#include "decode.h"

#define BUF_SIZ           10240
#define le16_to_cpu       le16toh

struct ieee80211_radiotap_header {
	u8  it_version;     /* set to 0 */
	u8  it_pad;
	u16 it_len;         /* entire length */
	u32 it_present;     /* fields present */
} __attribute__((__packed__));

enum loglevel{
	ALWAYS=0,
	NOTICE=1,
	INFO=2,
	DETAIL=3,
	NOTICE_=4,
	INFO_=5,
	DETAIL_=6
};

int ch_idx = 0;
int g_ap_channel = -1;
int while_test = 0;
int chan_list[13] = {1,6,11,2,3,4,5,7,8,9,10,12,13};
int g_stop = 0;

enum loglevel g_debuglevel;
//g_debuglevel = NOTICE;

void wifi_set_channel(const int ch){
	char cmd[64];
//	sprintf(cmd, "iw %s set channel %d", "wlan0", ch);
//	system(cmd);
//	if(errno > 0) {
//		WMG_ERROR("%s, %s\n", cmd, strerror(errno));
//	}
	if(wifi_monitor_set_channel((uint8_t)(ch))){
		WMG_ERROR("set monitor mode channel %d failed\n",ch);
	} else {
		WMG_INFO("set monitor mode channel %d success\n",ch);
	}
}

static int get_rtheader_len(u8 *buf, size_t len){
	struct ieee80211_radiotap_header *rt_header;
	u16 rt_header_size;

	rt_header = (struct ieee80211_radiotap_header *)buf;
	/* check the radiotap header can actually be present */
	if(len < sizeof(struct ieee80211_radiotap_header)) {
		return -EILSEQ;
	}
	/* Linux only supports version 0 radiotap format */
	if(rt_header->it_version) {
		return -EILSEQ;
	}
	rt_header_size = le16toh(rt_header->it_len);
	/* sanity check for allowed length and radiotap length field */
	if(len < rt_header_size) {
		return -EILSEQ;
	}
	return rt_header_size;
}

static void wifi_state_handle(wifi_msg_data_t *msg)
{
	int rt_header_len = 0;
	int complete_count = 0;
	int i;
	unsigned char *pkt;
	switch(msg->id) {
	case WIFI_MSG_ID_STA_CN_EVENT:
		WMG_INFO("linkd-xconfig: Info: receive wifi station message\n");
		break;
	case WIFI_MSG_ID_AP_CN_EVENT:
		WMG_INFO("linkd-xconfig: Info: receive wifi ap message\n");
		break;
	case WIFI_MSG_ID_MONITOR:
		WMG_INFO("linkd-xconfig: Info: receive wifi monitor message\n");
		WMG_INFO("linkd-xconfig: Info: receive data len=%d, channel=%d\n", msg->data.frame->len, msg->data.frame->channel);
//		printf("\n");
//		for (i = 0; i < msg->data.frame->len; i++) {
//			printf("0x%02x ", *(msg->data.frame->data + i));
//			if (((i + 1) % 20) == 0) {
//				printf("\n");
//			}
//		}
//		printf("\n");

		if((rt_header_len = get_rtheader_len(msg->data.frame->data, (size_t)msg->data.frame->len)) < 1){
			WMG_INFO("skip packet with rt_header_len = %d\n", rt_header_len);
			break;
		}

		/*ow process the packet*/
		pkt = (u8*)msg->data.frame->data + rt_header_len;

		if(packet_filter(pkt, p_lead_code()) == 0) {
			break;
		}

		if(packet_deoced (pkt, p_lead_code()) == 1){
			/*those is debug info*/
			complete_count += 1;
			WMG_INFO("SUCCESS!!!!!!\n");
			WMG_INFO("________________________________\n");

			struct ssidpwd_complete *data = p_ssidpwd_complete_();
			WMG_INFO("SSID_SIZE : %d\n", data->ssid_size);
			WMG_INFO("SSID : %s\n",data->ssid);

			WMG_INFO("PWD_SIZE : %d\n", data->pwd_size);
			WMG_INFO("PWD : %s\n",data->pwd);

		} else {
			WMG_INFO("Can't get ssid and psk\n");
		}
		break;
	default:
		WMG_ERROR("Error: unknown message type\n");
	}
}

void *_xconfig_mode_main_loop(void *arg)
{
	int ret;
	WMG_INFO("support xconfig mode config net\n");
	proto_main_loop_para_t *main_loop_para = (proto_main_loop_para_t *)arg;

	ret = wifi_on(WIFI_MONITOR);
	if (ret == 0) {
		WMG_INFO("wifi on monitor mode success\n");
	} else {
		WMG_ERROR("wifi on monitor mode failed\n");
		return -1;
	}
	ret = wifi_monitor_enable(6);
	if (ret == 0) {
		WMG_INFO("monitor enable success, channel=%d\n", 6);
	} else {
		WMG_ERROR("ap enable failed\n");
		return -1;
	}
	ret = wifi_register_msg_cb(wifi_state_handle);
	if (ret == 0) {
		WMG_INFO("Info: wifi register msg cb success\n");
	} else {
		WMG_ERROR("wifi register msg cb failed\n");
		return -1;
	}

	while(1) {
		usleep(1000 * 20000);
		static int curr_ch = 0;
		WMG_INFO("exitting main...\n");
		if(get_status() == XRSC_STATUS_SRC_LOCKED){
			if(g_ap_channel == -1) {
				g_ap_channel = get_channel();
				WMG_INFO("now channel %d\n", curr_ch);
				WMG_INFO("stop in channel %d\n", g_ap_channel);
				if(g_ap_channel != curr_ch && g_ap_channel > 0 && g_ap_channel<=13) {
					wifi_set_channel(g_ap_channel);
				}
				continue;
			}
		}else if(get_status() == XRSC_STATUS_SEARCHING) {
			/*device is sourch leadcode, now need change channel*/
				curr_ch = chan_list[ch_idx];
				wifi_set_channel(curr_ch);
				if(ch_idx == 12) ch_idx = 0;
				else ch_idx++;
				continue;
		}else if(get_status() == XRSC_STATUS_COMPLETE) {
			/*device is get ssid and password, now need to connect ap*/
				WMG_INFO("seems completed\n");
				if (while_test == 0) {
					struct ssidpwd_complete *complete;
					int i = 0, j = 5;
					complete = p_ssidpwd_complete_();
					//g_stop = 1;
					//wifi_station_on();
					//strcpy(netInfo.ssid,(char *)complete->ssid);
					//strcpy(netInfo.password, (char *)complete->pwd);
					//is_receive = true;
					WMG_INFO("-----------------------------------------------SUCCESS \n");
				}
		}else {
				WMG_INFO("invalid status return \n");
		}
	}

	//linkd_result.ssid = resource.ssid;
	//linkd_result.psk = resource.psk;
	//main_loop_para->result_cb(&linkd_result);

	return NULL;
}
