/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 */

#include <wifimg.h>
#include <wmg_common.h>
#include <wifi_log.h>
#include <wmg_sta.h>
#include <wmg_ap.h>
#include <wmg_monitor.h>

#define UNK_BITMAP        0x0
#define STA_BITMAP        0x1
#define AP_BITMAP         0x2
#define MONITOR_BITMAP    0x4

#define AP_STA_BITMAP     0x3
#define AP_MONITOR_BITMAP 0x6
#define BITMAP_MARK       0xF

#define STA_MODE_NUM      0
#define AP_MODE_NUM       1
#define MONITOR_MODE_NUM  2
#define P2P_MODE_NUM      3

static wifimg_object_t wifimg_object;

const char *wmg_sec_to_str(wifi_secure_t sec)
{
	switch (sec) {
		case WIFI_SEC_NONE:
		return "NONE";
		case WIFI_SEC_WEP:
		return "WEP";
		case WIFI_SEC_WPA_PSK:
		return "WPAPSK";
		case WIFI_SEC_WPA2_PSK:
		return "WPA2PSK";
		default:
		return "UNKNOWN";
	}
}

const char *wmg_wifi_mode_to_str(wifi_mode_t mode)
{
	switch (mode) {
	case WIFI_STATION:
		return "station";
	case WIFI_AP:
		return "ap";
	case WIFI_MONITOR:
		return "monitor";
	case WIFI_AP_STATION:
		return "ap-station";
	case WIFI_AP_MONITOR:
		return "ap-monitor";
	default:
		return "unknown";
	}
}

uint8_t wmg_wifi_mode_to_bitmap(wifi_mode_t mode)
{
	switch (mode) {
	case WIFI_MODE_UNKNOWN:
		return UNK_BITMAP;
	case WIFI_STATION:
		return STA_BITMAP;
	case WIFI_AP:
		return AP_BITMAP;
	case WIFI_MONITOR:
		return MONITOR_BITMAP;
	case WIFI_AP_STATION:
		return AP_STA_BITMAP;
	case WIFI_AP_MONITOR:
		return AP_MONITOR_BITMAP;
	default:
		return UNK_BITMAP;
	}
}

wifi_mode_t wmg_wifi_bitmap_to_mode(uint8_t bitmap)
{
	switch (bitmap) {
	case UNK_BITMAP:
		return WIFI_MODE_UNKNOWN;
	case STA_BITMAP:
		return WIFI_STATION;
	case AP_BITMAP:
		return WIFI_AP;
	case MONITOR_BITMAP:
		return WIFI_MONITOR;
	case AP_STA_BITMAP:
		return WIFI_AP_STATION;
	case AP_MONITOR_BITMAP:
		return WIFI_AP_MONITOR;
	default:
		return WIFI_MODE_UNKNOWN;
	}
}

/* init wifimg */
static wmg_status_t wifimg_init(void)
{
	wmg_status_t ret = WMG_STATUS_FAIL;

	if(wifimg_object.init_flag == WMG_FALSE) {
		os_net_mutex_create(&wifimg_object.mutex_lock);

		//===这里需要实现wifi支持什么模式===/
		//===这里需要优化注册方式===/
		wifimg_object.mode_object[STA_MODE_NUM] = NULL;
		wifimg_object.mode_object[AP_MODE_NUM] = NULL;
		wifimg_object.mode_object[MONITOR_MODE_NUM] = NULL;
		wifimg_object.mode_object[P2P_MODE_NUM] = NULL;

		wifimg_object.mode_object[STA_MODE_NUM] = wmg_sta_register_object();
		if(wifimg_object.mode_object[STA_MODE_NUM]->init != NULL) {
			if(wifimg_object.mode_object[STA_MODE_NUM]->init()){
				WMG_ERROR("sta mode init faile.\n");
				return WMG_STATUS_FAIL;
			}
		}

		wifimg_object.mode_object[AP_MODE_NUM] = wmg_ap_register_object();
		if(wifimg_object.mode_object[AP_MODE_NUM]->init != NULL) {
			if(wifimg_object.mode_object[AP_MODE_NUM]->init()){
				WMG_ERROR("ap mode init faile.\n");
				return WMG_STATUS_FAIL;
			}
		}

		wifimg_object.mode_object[MONITOR_MODE_NUM] = wmg_monitor_register_object();
		if(wifimg_object.mode_object[MONITOR_MODE_NUM]->init != NULL) {
			if(wifimg_object.mode_object[MONITOR_MODE_NUM]->init()){
				WMG_ERROR("ap mode init faile.\n");
				return WMG_STATUS_FAIL;
			}
		}

		wifimg_object.init_flag = WMG_TRUE;
		WMG_DEBUG("wifi_manager initialize, version: %s\n%s\n",
			WMG_VERSION, WMG_DECLARE);

		ret = WMG_STATUS_SUCCESS;
	} else {
		WMG_DEBUG("wifimg is already initialized\n");
		ret = WMG_STATUS_SUCCESS;
	}

	return ret;
}

static void wifimg_deinit(void)
{
	uint8_t current_mode_bitmap;
	int erro_code;

	if(wifimg_object.init_flag == WMG_TRUE) {
		WMG_INFO("wifimg deinit now\n");
		current_mode_bitmap = wifimg_object.current_mode_bitmap;

		if (current_mode_bitmap & STA_BITMAP) {
			if (wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_disable(&erro_code)) {
				WMG_ERROR("wifi sta mode disable faile\n");
				return;
			} else {
				wifimg_object.current_mode_bitmap &= (!STA_BITMAP);
				WMG_DEBUG("wifi sta mode disable success\n");
			}
		}
		if (current_mode_bitmap & AP_BITMAP) {
			if (wifimg_object.mode_object[AP_MODE_NUM]->mode_opt->mode_disable(&erro_code)) {
				WMG_ERROR("wifi ap mode disable faile\n");
				return;
			} else {
				wifimg_object.current_mode_bitmap &= (!AP_BITMAP);
				WMG_DEBUG("wifi ap mode disable success\n");
			}
		}
		if (current_mode_bitmap & MONITOR_BITMAP) {
			if (wifimg_object.mode_object[MONITOR_MODE_NUM]->mode_opt->mode_disable(&erro_code)) {
				WMG_ERROR("wifi monitor mode disable\n");
				return;
			} else {
				wifimg_object.current_mode_bitmap &= (!MONITOR_BITMAP);
				WMG_DEBUG("wifi monitor mode disable success\n");
			}
		}
		wifimg_object.current_mode_bitmap = UNK_BITMAP;

		if((wifimg_object.mode_object[STA_MODE_NUM] != NULL) &&
				(wifimg_object.mode_object[STA_MODE_NUM]->deinit != NULL)) {
			WMG_DEBUG("deinit sta mode now\n");
			wifimg_object.mode_object[STA_MODE_NUM]->deinit();
		}
		if((wifimg_object.mode_object[AP_MODE_NUM] != NULL) &&
				(wifimg_object.mode_object[AP_MODE_NUM]->deinit != NULL)) {
			WMG_DEBUG("deinit ap mode now\n");
			wifimg_object.mode_object[AP_MODE_NUM]->deinit();
		}
		if((wifimg_object.mode_object[MONITOR_MODE_NUM] != NULL) &&
				(wifimg_object.mode_object[MONITOR_MODE_NUM]->deinit != NULL)) {
			WMG_DEBUG("deinit monitor mode now\n");
			wifimg_object.mode_object[MONITOR_MODE_NUM]->deinit();
		}
		if((wifimg_object.mode_object[P2P_MODE_NUM] != NULL) &&
			(wifimg_object.mode_object[P2P_MODE_NUM]->deinit != NULL)) {
			WMG_DEBUG("deinit p2p mode now\n");
			wifimg_object.mode_object[P2P_MODE_NUM]->deinit();
		}

		wifimg_object.init_flag = WMG_FALSE;
		wifimg_object.wifi_status = WLAN_STATUS_DOWN,
		wifimg_object.current_mode_bitmap = UNK_BITMAP,
		wifimg_object.support_mode_bitmap = UNK_BITMAP,

		WMG_INFO("wifimg deinit success\n");
		return;
	} else {
		WMG_INFO("wifimg already deinit\n");
		return;
	}
}

static wmg_status_t wifimg_switch_mode(wifi_mode_t switch_mode)
{
	/* ------------------------------ */
	/* 这里还要判断是否支持切换的模式 */
	wmg_status_t ret;
	wmg_bool_t mode_ret;
	int erro_code;
	uint8_t switch_mode_bitmap, current_mode_bitmap, disable_mode_bitmap, enable_mode_bitmap;
	wifi_mode_t current_mode = wmg_wifi_bitmap_to_mode(wifimg_object.current_mode_bitmap);

	if (switch_mode < WIFI_STATION || switch_mode >= WIFI_MODE_UNKNOWN) {
		WMG_ERROR("unknown wifi mode\n");
		return WMG_STATUS_INVALID;
	}

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	if(switch_mode == current_mode) {
		WMG_DEBUG("switch_mode '%s' equals to current_mode '%s'"
			"do not need switch wifi mode\n",
			wmg_wifi_mode_to_str(current_mode),
			wmg_wifi_mode_to_str(switch_mode));
		return WMG_STATUS_UNHANDLED;
	}

	WMG_DEBUG("switch wifi mode from current_mode '%s' to new_mode '%s'\n",
		wmg_wifi_mode_to_str(current_mode),
		wmg_wifi_mode_to_str(switch_mode));

	switch_mode_bitmap = wmg_wifi_mode_to_bitmap(switch_mode);
	current_mode_bitmap = wmg_wifi_mode_to_bitmap(current_mode);
	WMG_DEBUG("switch_mode_bitmap:0x%x\n",switch_mode_bitmap);
	WMG_DEBUG("current_mode_bitmap:0x%x\n",current_mode_bitmap);

	disable_mode_bitmap = (switch_mode_bitmap | current_mode_bitmap) ^ switch_mode_bitmap;
	WMG_DEBUG("disable_mode_bitmap:0x%x\n",disable_mode_bitmap);

	os_net_mutex_lock(&wifimg_object.mutex_lock);

	if (disable_mode_bitmap & STA_BITMAP) {
		if (wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_disable(&erro_code)) {
			WMG_DEBUG("wifi sta mode disable faile\n");
		} else {
			wifimg_object.current_mode_bitmap &= (!STA_BITMAP);
			WMG_DEBUG("wifi sta mode disable success\n");
		}
	}
	if (disable_mode_bitmap & AP_BITMAP) {
		if (wifimg_object.mode_object[AP_MODE_NUM]->mode_opt->mode_disable(&erro_code)) {
			WMG_DEBUG("wifi ap mode disable faile\n");
		} else {
			wifimg_object.current_mode_bitmap &= (!AP_BITMAP);
			WMG_DEBUG("wifi ap mode disable success\n");
		}
	}
	if (disable_mode_bitmap & MONITOR_BITMAP) {
		if (wifimg_object.mode_object[MONITOR_MODE_NUM]->mode_opt->mode_disable(&erro_code)) {
			WMG_DEBUG("wifi monitor mode disable\n");
		} else {
			wifimg_object.current_mode_bitmap &= (!MONITOR_BITMAP);
			WMG_DEBUG("wifi monitor mode disable success\n");
		}
	}

	enable_mode_bitmap = (switch_mode_bitmap | current_mode_bitmap) ^ current_mode_bitmap;
	WMG_DEBUG("enable_mode_bitmap:0x%x\n",enable_mode_bitmap);
	if (enable_mode_bitmap & STA_BITMAP) {
		if(wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_enable(&erro_code)){
			WMG_DEBUG("wifi sta mode enable faile\n");
		} else {
			wifimg_object.current_mode_bitmap |= STA_BITMAP;
			WMG_DEBUG("wifi sta mode enable success\n");
		}
	}
	if (enable_mode_bitmap & AP_BITMAP) {
		if(wifimg_object.mode_object[AP_MODE_NUM]->mode_opt->mode_enable(&erro_code)){
			WMG_DEBUG("wifi ap mode enable faile\n");
		} else {
			wifimg_object.current_mode_bitmap |= AP_BITMAP;
			WMG_DEBUG("wifi ap mode enable success\n");
		}
	}
	if (enable_mode_bitmap & MONITOR_BITMAP) {
		if(wifimg_object.mode_object[MONITOR_MODE_NUM]->mode_opt->mode_enable(&erro_code)){
			WMG_DEBUG("wifi monitor mode enable faile\n");
		} else {
			wifimg_object.current_mode_bitmap |= MONITOR_BITMAP;
			WMG_DEBUG("wifi monitor mode enable success\n");
		}
	}

	WMG_DEBUG("switch after current_mode_bitmap:0x%x\n",wifimg_object.current_mode_bitmap);

	os_net_mutex_unlock(&wifimg_object.mutex_lock);

	if(switch_mode_bitmap == wifimg_object.current_mode_bitmap){
		return WMG_STATUS_SUCCESS;
	} else {
		WMG_ERROR("switch mode faile\n");
		return WMG_STATUS_FAIL;
	}
}

static wmg_bool_t wifimg_is_init(void)
{
	return wifimg_object.init_flag;
}

static wifi_mode_t wifimg_get_current_mode(void)
{
	return wmg_wifi_bitmap_to_mode(wifimg_object.current_mode_bitmap);
}

static wmg_status_t wifimg_sta_connect(wifi_sta_cn_para_t *cn_para)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_ctl(WMG_STA_CMD_CONNECT, (void *)cn_para, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_sta_disconnect(void)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_ctl(WMG_STA_CMD_DISCONNECT, NULL, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_sta_auto_reconnect(wmg_bool_t enable)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	if (wifimg_object.get_current_mode() != WIFI_STATION) {
		WMG_ERROR("wifi manager is not sta mode, need to switch sta mode first\n");
		return WMG_STATUS_FAIL;
	}

	ret = wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_ctl(WMG_STA_CMD_AUTO_RECONNECT, (void *)&enable, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_sta_get_info(wifi_sta_info_t *sta_info)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	if (wifimg_object.get_current_mode() != WIFI_STATION) {
		WMG_ERROR("wifi manager is not sta mode, need to switch sta mode first\n");
		return WMG_STATUS_FAIL;
	}

	ret = wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_ctl(WMG_STA_CMD_GET_INFO, (void *)sta_info, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_sta_list_networks(wifi_sta_list_t *sta_list)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	if (wifimg_object.get_current_mode() != WIFI_STATION) {
		WMG_ERROR("wifi manager is not sta mode, need to switch sta mode first\n");
		return WMG_STATUS_FAIL;
	}

	ret = wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_ctl(WMG_STA_CMD_LIST_NETWORKS, (void *)sta_list, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_sta_remove_networks(char *ssid)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	if (wifimg_object.get_current_mode() != WIFI_STATION) {
		WMG_ERROR("wifi manager is not sta mode, need to switch sta mode first\n");
		return WMG_STATUS_FAIL;
	}

	ret = wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_ctl(WMG_STA_CMD_REMOVE_NETWORKS, (void *)ssid, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_ap_enable(wifi_ap_config_t *ap_config)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object.mode_object[AP_MODE_NUM]->mode_opt->mode_ctl(WMG_AP_CMD_ENABLE, (void *)ap_config, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_ap_disable(void)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object.mode_object[AP_MODE_NUM]->mode_opt->mode_ctl(WMG_AP_CMD_DISABLE, NULL, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_ap_get_config(wifi_ap_config_t *ap_config)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object.mode_object[AP_MODE_NUM]->mode_opt->mode_ctl(WMG_AP_CMD_GET_CONFIG, (void *)ap_config, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_monitor_enable(uint8_t channel)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object.mode_object[MONITOR_MODE_NUM]->mode_opt->mode_ctl(WMG_MONITOR_CMD_ENABLE, (void *)&channel, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_monitor_set_channel(uint8_t channel)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object.mode_object[MONITOR_MODE_NUM]->mode_opt->mode_ctl(WMG_MONITOR_CMD_SET_CHANNEL, (void *)&channel, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_monitor_disable(void)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object.mode_object[MONITOR_MODE_NUM]->mode_opt->mode_ctl(WMG_MONITOR_CMD_DISABLE, NULL, &cb_msg);
	if(ret != WMG_STATUS_SUCCESS) {
		return ret;
	}
	if(cb_msg != WMG_STATUS_SUCCESS) {
		return cb_msg;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_register_msg_cb(wifi_msg_cb_t msg_cb)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	if(wifimg_object.wmg_msg_cb != NULL) {
		WMG_DEBUG("wifi manager register new msg cb\n");
	}

	if(wifimg_object.mode_object[STA_MODE_NUM] != NULL) {
		ret = wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_ctl(WMG_STA_CMD_REGISTER_MSG_CB, (void *)&msg_cb, &cb_msg);
		if(ret != WMG_STATUS_SUCCESS) {
			return ret;
		}
		if(cb_msg != WMG_STATUS_SUCCESS) {
			return cb_msg;
		}
	}
	if(wifimg_object.mode_object[AP_MODE_NUM] != NULL) {
		ret = wifimg_object.mode_object[AP_MODE_NUM]->mode_opt->mode_ctl(WMG_AP_CMD_REGISTER_MSG_CB, (void *)&msg_cb, &cb_msg);
		if(ret != WMG_STATUS_SUCCESS) {
			return ret;
		}
		if(cb_msg != WMG_STATUS_SUCCESS) {
			return cb_msg;
		}
	}
	if(wifimg_object.mode_object[MONITOR_MODE_NUM] != NULL) {
		ret = wifimg_object.mode_object[MONITOR_MODE_NUM]->mode_opt->mode_ctl(WMG_MONITOR_CMD_REGISTER_MSG_CB, (void *)&msg_cb, &cb_msg);
		if(ret != WMG_STATUS_SUCCESS) {
			return ret;
		}
		if(cb_msg != WMG_STATUS_SUCCESS) {
			return cb_msg;
		}
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_set_scan_param(wifi_scan_param_t *scan_param)
{
	return WMG_STATUS_FAIL;
}

static wmg_status_t wifimg_get_scan_results(wifi_scan_result_t *wifi_scan_results, uint32_t *wifi_bss_num, uint32_t wifi_arr_size)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_SUCCESS;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	if (wifimg_object.get_current_mode() == WIFI_STATION) {
		int erro_code = 0;
		sta_get_scan_results_para_t cmd_para = {
			.scan_results = wifi_scan_results,
			.bss_num = wifi_bss_num,
			.arr_size = wifi_arr_size,
		};
		ret = wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_ctl(WMG_STA_CMD_SCAN_RESULTS, (void *)&cmd_para, &cb_msg);
		if(ret != WMG_STATUS_SUCCESS) {
			return ret;
		}
		if(cb_msg != WMG_STATUS_SUCCESS) {
			return cb_msg;
		}
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_set_mac(const char *ifname, uint8_t *mac_addr)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_FAIL;
	common_mac_para_t common_mac_para;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	common_mac_para.ifname = ifname;
	common_mac_para.mac_addr = mac_addr;

	if (wifimg_object.get_current_mode() == WIFI_STATION) {
		ret = wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_ctl(WMG_STA_CMD_SET_MAC, (void *)&common_mac_para, &cb_msg);
		if(ret != WMG_STATUS_SUCCESS) {
			return ret;
		}
		if(cb_msg != WMG_STATUS_SUCCESS) {
			return cb_msg;
		}
	} else if (wifimg_object.get_current_mode() == WIFI_AP) {
		ret = wifimg_object.mode_object[AP_MODE_NUM]->mode_opt->mode_ctl(WMG_AP_CMD_SET_MAC, (void *)&common_mac_para, &cb_msg);
		if(ret != WMG_STATUS_SUCCESS) {
			return ret;
		}
		if(cb_msg != WMG_STATUS_SUCCESS) {
			return cb_msg;
		}
	} else if (wifimg_object.get_current_mode() == WIFI_MONITOR) {
		ret = wifimg_object.mode_object[MONITOR_MODE_NUM]->mode_opt->mode_ctl(WMG_MONITOR_CMD_SET_MAC, (void *)&common_mac_para, &cb_msg);
		if(ret != WMG_STATUS_SUCCESS) {
			return ret;
		}
		if(cb_msg != WMG_STATUS_SUCCESS) {
			return cb_msg;
		}
	} else {
		WMG_ERROR("wifi manager mode(%s) not support set mac\n", wmg_wifi_mode_to_str(wifimg_object.get_current_mode()));
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wifimg_get_mac(const char *ifname, uint8_t *mac_addr)
{
	wmg_status_t ret, cb_msg;
	cb_msg = WMG_STATUS_FAIL;
	common_mac_para_t common_mac_para;

	if(wifimg_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi manager is not init\n");
		return WMG_STATUS_NOT_READY;
	}

	common_mac_para.ifname = ifname;
	common_mac_para.mac_addr = mac_addr;

	if (wifimg_object.get_current_mode() == WIFI_STATION) {
		ret = wifimg_object.mode_object[STA_MODE_NUM]->mode_opt->mode_ctl(WMG_STA_CMD_GET_MAC, (void *)&common_mac_para, &cb_msg);
		if(ret != WMG_STATUS_SUCCESS) {
			return ret;
		}
		if(cb_msg != WMG_STATUS_SUCCESS) {
			return cb_msg;
		}
	} else if (wifimg_object.get_current_mode() == WIFI_AP) {
		ret = wifimg_object.mode_object[AP_MODE_NUM]->mode_opt->mode_ctl(WMG_AP_CMD_GET_MAC, (void *)&common_mac_para, &cb_msg);
		if(ret != WMG_STATUS_SUCCESS) {
			return ret;
		}
		if(cb_msg != WMG_STATUS_SUCCESS) {
			return cb_msg;
		}
	} else if (wifimg_object.get_current_mode() == WIFI_MONITOR) {
		ret = wifimg_object.mode_object[MONITOR_MODE_NUM]->mode_opt->mode_ctl(WMG_MONITOR_CMD_GET_MAC, (void *)&common_mac_para, &cb_msg);
		if(ret != WMG_STATUS_SUCCESS) {
			return ret;
		}
		if(cb_msg != WMG_STATUS_SUCCESS) {
			return cb_msg;
		}
	} else {
		WMG_ERROR("wifi manager mode(%s) not support get mac\n", wmg_wifi_mode_to_str(wifimg_object.get_current_mode()));
	}
	return WMG_STATUS_SUCCESS;
}

static wifimg_object_t wifimg_object = {
	.init_flag = WMG_FALSE,
	.wifi_status = WLAN_STATUS_DOWN,
	.current_mode_bitmap = UNK_BITMAP,
	.support_mode_bitmap = UNK_BITMAP,
	.connect_timeout = 0,
	.wmg_msg_cb = NULL,

	.init = wifimg_init,
	.deinit = wifimg_deinit,
	.switch_mode = wifimg_switch_mode,

	.is_init = wifimg_is_init,
	.get_current_mode = wifimg_get_current_mode,

	.sta_connect =  wifimg_sta_connect,
	.sta_disconnect =  wifimg_sta_disconnect,
	.sta_auto_reconnect =  wifimg_sta_auto_reconnect,
	.sta_get_info =  wifimg_sta_get_info,
	.sta_list_networks =  wifimg_sta_list_networks,
	.sta_remove_networks =  wifimg_sta_remove_networks,

	.ap_enable = wifimg_ap_enable,
	.ap_disable = wifimg_ap_disable,
	.ap_get_config = wifimg_ap_get_config,

	.monitor_enable = wifimg_monitor_enable,
	.monitor_set_channel = wifimg_monitor_set_channel,
	.monitor_disable = wifimg_monitor_disable,

	.register_msg_cb = wifimg_register_msg_cb,
	.set_scan_param = wifimg_set_scan_param,
	.get_scan_results =  wifimg_get_scan_results,
	.set_mac =  wifimg_set_mac,
	.get_mac =  wifimg_get_mac,
};

wifimg_object_t* get_wifimg_object(void)
{
	return &wifimg_object;
}
