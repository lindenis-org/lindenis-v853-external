/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 *
 * File: wifimg.c
 *
 * Description:
 *		This file implements the core APIs of wifi manager.
 */
#include <wifimg.h>
#include <wmg_common.h>
#include <wifi_log.h>
#include <string.h>

static int __wifi_on(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifimg_object_t* wifimg_object = get_wifimg_object();

	wifi_mode_t *mode = (wifi_mode_t *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		ret = wifimg_object->init();
		if (ret != WMG_STATUS_SUCCESS) {
			return ret;
		}
	}

	ret = wifimg_object->switch_mode(*mode);
	if ((ret == WMG_STATUS_UNHANDLED) || (ret == WMG_STATUS_SUCCESS)) {
		WMG_DEBUG("switch wifi mode success\n");
		ret = WMG_STATUS_SUCCESS;
	} else {
		WMG_ERROR("failed to switch wifi mode\n");
	}

	*cb_status = ret;
	return ret;
}

static int __wifi_off(void **call_argv,void **cb_argv)
{
	wifimg_object_t* wifimg_object = get_wifimg_object();

	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(wifimg_object->is_init()){
		wifimg_object->deinit();
	} else {
		WMG_DEBUG("wifimg is already deinit\n");
	}

	*cb_status = WMG_STATUS_SUCCESS;
	return WMG_STATUS_SUCCESS;
}

static int __wifi_sta_connect(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	wifi_sta_cn_para_t *cn_para = (wifi_sta_cn_para_t *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_STATION) {
		WMG_ERROR("wifi station mode is not enabled\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	if (cn_para == NULL) {
		WMG_ERROR("invalid connect parameters\n");
		*cb_status = WMG_STATUS_INVALID;
		return WMG_STATUS_INVALID;
	}

	ret = wifimg_object->sta_connect(cn_para);
	if (!ret)
		WMG_DEBUG("wifi station connect success\n");
	else
		WMG_ERROR("wifi station connect fail\n");

	*cb_status = ret;
	return ret;
}

static int __wifi_sta_disconnect(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifimg_object_t* wifimg_object = get_wifimg_object();
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_DEBUG("wifi manager is not running\n");
	}

	ret = wifimg_object->sta_disconnect();
	*cb_status = ret;
	return ret;
}

static int __wifi_sta_auto_reconnect(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	wmg_bool_t *enable = (wmg_bool_t *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_STATION) {
		WMG_ERROR("wifi station mode is not enabled\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object->sta_auto_reconnect(*enable);
	*cb_status = ret;
	return ret;
}

static int __wifi_sta_get_info(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	wifi_sta_info_t *sta_info = (wifi_sta_info_t *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_STATION) {
		WMG_ERROR("wifi station mode is not enabled\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	if (sta_info == NULL) {
		WMG_ERROR("invalid station info parameters\n");
		*cb_status = WMG_STATUS_INVALID;
		return WMG_STATUS_INVALID;
	}

	ret = wifimg_object->sta_get_info(sta_info);
	*cb_status = ret;
	return ret;
}

static int __wifi_sta_list_networks(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	wifi_sta_list_t *sta_list_networks = (wifi_sta_list_t *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_STATION) {
		WMG_ERROR("wifi station mode is not enabled\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	if (sta_list_networks == NULL) {
		WMG_ERROR("invalid list networks parameters\n");
		*cb_status = WMG_STATUS_INVALID;
		return WMG_STATUS_INVALID;
	}

	ret = wifimg_object->sta_list_networks(sta_list_networks);
	*cb_status = ret;
	return ret;
}

static int __wifi_sta_remove_networks(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	char *ssid = (char *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_STATION) {
		WMG_ERROR("wifi station mode is not enabled\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object->sta_remove_networks(ssid);
	*cb_status = ret;
	return ret;
}

static int __wifi_ap_enable(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	wifi_ap_config_t *ap_config = (wifi_ap_config_t *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_AP) {
		WMG_ERROR("wifi ap mode is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	if (ap_config == NULL) {
		WMG_ERROR("invalid ap config parameters\n");
		*cb_status = WMG_STATUS_INVALID;
		return WMG_STATUS_INVALID;
	}

	ret = wifimg_object->ap_enable(ap_config);
	*cb_status = ret;
	return ret;
}

static int __wifi_ap_disable(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_DEBUG("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_AP) {
		WMG_ERROR("wifi ap mode is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object->ap_disable();
	*cb_status = ret;
	return ret;
}

static int __wifi_ap_get_config(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	wifi_ap_config_t *ap_config = (wifi_ap_config_t *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_AP) {
		WMG_ERROR("wifi ap mode is not enabled\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object->ap_get_config(ap_config);
	*cb_status = ret;
	return ret;
}

static int __wifi_monitor_enable(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	uint8_t *channel = (uint8_t *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_MONITOR) {
		WMG_ERROR("wifi monitor mode is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object->monitor_enable(*channel);
	*cb_status = ret;
	return ret;
}

static int __wifi_monitor_set_channel(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	uint8_t *channel = (uint8_t *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_MONITOR) {
		WMG_ERROR("wifi monitor mode is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object->monitor_set_channel(*channel);
	*cb_status = ret;
	return ret;
}

static int __wifi_monitor_disable(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_DEBUG("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_MONITOR) {
		WMG_ERROR("wifi monitor mode is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	ret = wifimg_object->monitor_disable();
	*cb_status = ret;
	return ret;
}

static int __wifi_register_msg_cb(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	wifi_msg_cb_t *msg_cb = (wifi_msg_cb_t *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	if (msg_cb == NULL) {
		WMG_ERROR("invalid parameters, msg_cb is NULL\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_INVALID;
	}

	ret = wifimg_object->register_msg_cb(*msg_cb);
	if (!ret)
		WMG_DEBUG("register msg cb success\n");
	else
		WMG_ERROR("failed to register msg cb\n");

	*cb_status = ret;
	return ret;
}

static int __wifi_set_scan_param(void **call_argv,void **cb_argv)
{
	wifimg_object_t* wifimg_object = get_wifimg_object();
	wifi_scan_param_t *msg_cb = (wifi_scan_param_t *)call_argv[0];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	*cb_status = WMG_STATUS_FAIL;
	return WMG_STATUS_FAIL;
}

static int __wifi_get_scan_results(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;
	wifi_mode_t current_mode;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	wifi_scan_result_t *scan_results = (wifi_scan_result_t *)call_argv[0];
	uint32_t *bss_num = (uint32_t *)call_argv[1];
	uint32_t *arr_size = (uint32_t *)call_argv[2];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		return WMG_STATUS_NOT_READY;
	}

	current_mode = wifimg_object->get_current_mode();
	if (current_mode != WIFI_STATION) {
		WMG_ERROR("wifi station is not enabled\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	if (scan_results == NULL || arr_size == 0 || bss_num == NULL) {
		WMG_ERROR("invalid parameters\n");
		*cb_status = WMG_STATUS_INVALID;
		return WMG_STATUS_INVALID;
	}

	ret = wifimg_object->get_scan_results(scan_results, bss_num, *arr_size);
	if (!ret)
		WMG_DEBUG("get scan results success\n");
	else
		WMG_ERROR("failed to get scan results\n");

	*cb_status = ret;
	return ret;
}

static int __wifi_set_mac(void **call_argv,void **cb_argv)
{
	wmg_status_t ret;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	char *ifname = (char *)call_argv[0];
	uint8_t *mac_addr = (uint8_t *)call_argv[1];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	char wmg_ifname[32];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	if(ifname == NULL) {
		strncpy(wmg_ifname, WMG_DEFAULT_INF, 6);
	} else {
		if(strlen(ifname) > 32){
			WMG_ERROR("infname longer than 32\n");
			*cb_status = WMG_STATUS_FAIL;
			return WMG_STATUS_FAIL;
		} else {
			strncpy(wmg_ifname, ifname, strlen(ifname));
		}
	}

	ret = wifimg_object->set_mac(wmg_ifname, mac_addr);
	if (!ret)
		WMG_DEBUG("set mac addr success\n");
	else
		WMG_ERROR("failed to set mac addr\n");

	*cb_status = ret;
	return ret;
}

static int __wifi_get_mac(void **call_argv,void **cb_argv)
{
	wmg_status_t ret = WMG_STATUS_FAIL;

	wifimg_object_t* wifimg_object = get_wifimg_object();
	char *ifname = (char *)call_argv[0];
	uint8_t *mac_addr = (uint8_t *)call_argv[1];
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];

	char wmg_ifname[32];

	if(!wifimg_object->is_init()){
		WMG_ERROR("wifi manager is not running\n");
		*cb_status = WMG_STATUS_NOT_READY;
		return WMG_STATUS_NOT_READY;
	}

	if(ifname == NULL) {
		strncpy(wmg_ifname, WMG_DEFAULT_INF, 6);
	} else {
		if(strlen(ifname) > 32){
			WMG_ERROR("infname longer than 32\n");
			*cb_status = WMG_STATUS_FAIL;
			return WMG_STATUS_FAIL;
		} else {
			strncpy(wmg_ifname, ifname, strlen(ifname));
		}
	}

	ret = wifimg_object->get_mac(wmg_ifname, mac_addr);
	if (!ret)
		WMG_DEBUG("get mac addr success\n");
	else
		WMG_ERROR("failed to get mac addr\n");

	*cb_status = ret;
	return ret;
}

static int __wifi_send_80211_raw_frame(void **call_argv,void **cb_argv)
{
	wifimg_object_t* wifimg_object = get_wifimg_object();
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];
	*cb_status = WMG_STATUS_FAIL;
	return WMG_STATUS_FAIL;
}

static int __wifi_set_country_code(void **call_argv,void **cb_argv)
{
	wifimg_object_t* wifimg_object = get_wifimg_object();
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];
	*cb_status = WMG_STATUS_FAIL;
	return WMG_STATUS_FAIL;
}

static int __wifi_get_country_code(void **call_argv,void **cb_argv)
{
	wifimg_object_t* wifimg_object = get_wifimg_object();
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];
	*cb_status = WMG_STATUS_FAIL;
	return WMG_STATUS_FAIL;
}

static int __wifi_set_ps_mode(void **call_argv,void **cb_argv)
{
	wifimg_object_t* wifimg_object = get_wifimg_object();
	wmg_status_t *cb_status = (wmg_status_t *)cb_argv[0];
	*cb_status = WMG_STATUS_FAIL;
	return WMG_STATUS_FAIL;
}

act_func_t pla_action_table[] = {
	[WMG_PLA_ACT_WIFI_ON] = {__wifi_on, "__wifi_on"},
	[WMG_PLA_ACT_WIFI_OFF] = {__wifi_off, "__wifi_off"},
	[WMG_PLA_ACT_WIFI_REGISTER_MSG_CB] = {__wifi_register_msg_cb, "__wifi_register_msg_cb"},
	[WMG_PLA_ACT_WIFI_SET_MAC] = {__wifi_set_mac, "__wifi_set_mac"},
	[WMG_PLA_ACT_WIFI_GET_MAC] = {__wifi_get_mac, "__wifi_get_mac"},
	[WMG_PLA_ACT_WIFI_SEND_80211_RAW_FRAME] = {__wifi_send_80211_raw_frame, "__wifi_send_80211_raw_frame"},
	[WMG_PLA_ACT_WIFI_SET_COUNTRY_CODE] = {__wifi_set_country_code, "__wifi_set_country_code"},
	[WMG_PLA_ACT_WIFI_GET_COUNTRY_CODE] = {__wifi_get_country_code, "__wifi_get_country_code"},
	[WMG_PLA_ACT_WIFI_SET_PS_MODE] = {__wifi_set_ps_mode, "__wifi_set_ps_mode"},
};

act_func_t sta_action_table[] = {
	[WMG_STA_ACT_CONNECT] = {__wifi_sta_connect, "__wifi_sta_connect"},
	[WMG_STA_ACT_DISCONNECT] = {__wifi_sta_disconnect, "__wifi_sta_disconnect"},
	[WMG_STA_ACT_AUTO_RECONNECT] = {__wifi_sta_auto_reconnect, "__wifi_sta_auto_reconnect"},
	[WMG_STA_ACT_GET_INFO] = {__wifi_sta_get_info, "__wifi_sta_get_info"},
	[WMG_STA_ACT_LIST_NETWORKS] = {__wifi_sta_list_networks, "__wifi_sta_list_networks"},
	[WMG_STA_ACT_REMOVE_NETWORKS] = {__wifi_sta_remove_networks, "__wifi_sta_remove_networks"},
	[WMG_STA_ACT_SCAN_PARAM] = {__wifi_set_scan_param, "__wifi_set_scan_param"},
	[WMG_STA_ACT_SCAN_RESULTS] = {__wifi_get_scan_results, "__wifi_get_scan_results"},
};

act_func_t ap_action_table[] = {
	[WMG_AP_ACT_ENABLE] = {__wifi_ap_enable, "__wifi_ap_enable"},
	[WMG_AP_ACT_DISABLE] = {__wifi_ap_disable, "__wifi_ap_disable"},
	[WMG_AP_ACT_GET_CONFIG] = {__wifi_ap_get_config, "__wifi_ap_get_config"},
};

act_func_t monitor_action_table[] = {
	[WMG_MONITOR_ACT_ENABLE] = {__wifi_monitor_enable, "__wifi_monitor_enable"},
	[WMG_MONITOR_ACT_SET_CHANNEL] = {__wifi_monitor_set_channel, "__wifi_monitor_set_channel"},
	[WMG_MONITOR_ACT_DISABLE] = {__wifi_monitor_disable, "__wifi_monitor_disable"},
};

//=====================================================================================================
wmg_status_t wifimanager_init(void)
{
	if(act_init(&wmg_act_handle,"wmg_act_handle",true) != OS_NET_STATUS_OK) {
		WMG_ERROR("act init failed.\n");
		return WMG_STATUS_FAIL;
	}

	act_register_handler(&wmg_act_handle,WMG_ACT_TABLE_PLA_ID,pla_action_table);
	act_register_handler(&wmg_act_handle,WMG_ACT_TABLE_STA_ID,sta_action_table);
	act_register_handler(&wmg_act_handle,WMG_ACT_TABLE_AP_ID,ap_action_table);
	act_register_handler(&wmg_act_handle,WMG_ACT_TABLE_MONITOR_ID,monitor_action_table);

	return WMG_STATUS_SUCCESS;
}

wmg_status_t wifimanager_deinit(void)
{
	act_deinit(&wmg_act_handle);
	return WMG_STATUS_SUCCESS;
}

wmg_status_t wifi_on(wifi_mode_t mode)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_PLA_ID,WMG_PLA_ACT_WIFI_ON,1,1,&mode,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_off(void)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_PLA_ID,WMG_PLA_ACT_WIFI_OFF,1,1,NULL,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_sta_connect(wifi_sta_cn_para_t *cn_para)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_STA_ID,WMG_STA_ACT_CONNECT,1,1,cn_para,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_sta_disconnect(void)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_STA_ID,WMG_STA_ACT_DISCONNECT,1,1,NULL,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_sta_auto_reconnect(wmg_bool_t enable)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_STA_ID,WMG_STA_ACT_AUTO_RECONNECT,1,1,&enable,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_sta_get_info(wifi_sta_info_t *sta_info)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_STA_ID,WMG_STA_ACT_GET_INFO,1,1,sta_info,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_sta_list_networks(wifi_sta_list_t *sta_list_networks)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_STA_ID,WMG_STA_ACT_LIST_NETWORKS,1,1,sta_list_networks,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_sta_remove_networks(char *ssid)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_STA_ID,WMG_STA_ACT_REMOVE_NETWORKS,1,1,ssid,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_ap_enable(wifi_ap_config_t *ap_config)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_AP_ID,WMG_AP_ACT_ENABLE,1,1,ap_config,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_ap_disable(void)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_AP_ID,WMG_AP_ACT_DISABLE,1,1,NULL,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_ap_get_config(wifi_ap_config_t *ap_config)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_AP_ID,WMG_AP_ACT_GET_CONFIG,1,1,ap_config,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_monitor_enable(uint8_t channel)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_MONITOR_ID,WMG_MONITOR_ACT_ENABLE,1,1,&channel,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_monitor_set_channel(uint8_t channel)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_MONITOR_ID,WMG_MONITOR_ACT_SET_CHANNEL,1,1,&channel,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_monitor_disable(void)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_MONITOR_ID,WMG_MONITOR_ACT_DISABLE,1,1,NULL,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_register_msg_cb(wifi_msg_cb_t msg_cb)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_PLA_ID,WMG_PLA_ACT_WIFI_REGISTER_MSG_CB,1,1,&msg_cb,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_set_scan_param(wifi_scan_param_t *scan_param)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_STA_ID,WMG_STA_ACT_SCAN_PARAM,1,1,scan_param,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_get_scan_results(wifi_scan_result_t *scan_results, uint32_t *bss_num, uint32_t arr_size)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;

	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_STA_ID,WMG_STA_ACT_SCAN_RESULTS,3,1,
				(void *)scan_results,(void *)bss_num,(void *)&arr_size,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_set_mac(const char *ifname, uint8_t *mac_addr)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;

	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_PLA_ID,WMG_PLA_ACT_WIFI_SET_MAC,2,1,
				(void *)ifname,(void *)mac_addr,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_get_mac(const char *ifname, uint8_t *mac_addr)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;

	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_PLA_ID,WMG_PLA_ACT_WIFI_GET_MAC,2,1,
				(void *)ifname,(void *)mac_addr,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_send_80211_raw_frame(uint8_t *data, uint32_t len, void *priv)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_PLA_ID,WMG_PLA_ACT_WIFI_SEND_80211_RAW_FRAME,1,1,NULL,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_set_country_code(const char *country_code)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_PLA_ID,WMG_PLA_ACT_WIFI_SET_COUNTRY_CODE,1,1,country_code,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_get_country_code(char *country_code)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_PLA_ID,WMG_PLA_ACT_WIFI_GET_COUNTRY_CODE,1,1,country_code,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}

wmg_status_t wifi_set_ps_mode(wmg_bool_t enable)
{
	wmg_status_t cb_msg = WMG_STATUS_FAIL;
	if(act_transfer(&wmg_act_handle,WMG_ACT_TABLE_PLA_ID,WMG_PLA_ACT_WIFI_SET_PS_MODE,1,1,&enable,&cb_msg)){
		WMG_ERROR("act_transfer fail\n");
		return WMG_STATUS_FAIL;
	} else {
		return cb_msg;
	}
}
