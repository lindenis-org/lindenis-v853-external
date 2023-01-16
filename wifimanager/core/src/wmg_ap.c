#include <wmg_ap.h>
#include <wifi_log.h>
#include <event.h>
#include <linux_hapd.h>

#include <string.h>

static wmg_ap_object_t ap_object;

char ap_config_ssid[100] = {0};
char ap_config_psk[100] = {0};

static wifi_ap_config_t ap_config = {
	.ssid = ap_config_ssid,
	.psk = ap_config_psk,
	.sec = WIFI_SEC_WPA2_PSK,
	.channel = 6,
	.sta_num = 0,
};

static void ap_hapd_event_notify(wifi_ap_event_t event)
{
	wifi_msg_data_t msg;

	if (ap_object.ap_msg_cb) {
		msg.id = WIFI_MSG_ID_AP_CN_EVENT;
		msg.data.ap_event = event;
		ap_object.ap_msg_cb(&msg);
	}
}

static int ap_enable(void *param, void *cb_msg)
{
	wmg_status_t ret;

	wifi_ap_config_t *ap_config = (wifi_ap_config_t *)param;
	wmg_status_t *cb_args = (wmg_status_t *)cb_msg;

	if(ap_object.init_flag == WMG_FALSE) {
		WMG_ERROR("ap has not been initialized\n");
		*cb_args = WMG_STATUS_FAIL;
		return WMG_STATUS_FAIL;
	}

	if(!ap_object.hapd_run) {
		WMG_WARNG("has not connect to hostapd\n");
		*cb_args = WMG_STATUS_FAIL;
		return WMG_STATUS_FAIL;
	}

	if (ap_object.ap_state == WIFI_AP_ENABLE) {
		if (ap_config == NULL) {
			WMG_WARNG("wifi ap is already enabled\n");
			*cb_args = WMG_STATUS_SUCCESS;
			return WMG_STATUS_SUCCESS;
		}
	} else {
		WMG_DUMP("wifi ap enabling...\n");
		if (ap_config == NULL) {
			WMG_ERROR("ap config is NULL, not set ap config\n");
			*cb_args = WMG_STATUS_FAIL;
			return WMG_STATUS_FAIL;
		} else {
			WMG_DEBUG("ap config is valid, set ap config\n");
		}
	}

	ret = ap_object.platform_inf[PLATFORM_LINUX]->ap_hapd_enable(ap_config);
	if (ret) {
		WMG_ERROR("failed to enable ap hapd, when set ap config\n");
		*cb_args = WMG_STATUS_FAIL;
		return WMG_STATUS_FAIL;
	}
	ap_object.ap_state = WIFI_AP_ENABLE;

	WMG_DUMP("wifi ap enable success\n");

	*cb_args = ret;
	return ret;
}

static int ap_disable(void *param, void *cb_msg)
{
	wmg_status_t ret;
	wmg_status_t *cb_args = (wmg_status_t *)cb_msg;

	if(ap_object.init_flag == WMG_FALSE) {
		WMG_WARNG("wifi ap is not initialized\n");
		*cb_args = WMG_STATUS_UNHANDLED;
		return WMG_STATUS_UNHANDLED;
	}

	if (ap_object.ap_state == WIFI_AP_DISABLE) {
		WMG_WARNG("wifi ap already disabled\n");
		*cb_args = WMG_STATUS_SUCCESS;
		return WMG_STATUS_SUCCESS;
	}

	WMG_DUMP("wifi ap disabling...\n");

	ret = ap_object.platform_inf[PLATFORM_LINUX]->ap_hapd_disable();
	if (ret)
		WMG_ERROR("failed to disable wifi ap\n");
    else
		WMG_DUMP("wifi ap disabled\n");

	ap_object.ap_state = WIFI_AP_DISABLE;

	*cb_args = ret;
	return ret;
}

static int ap_get_config(void *param, void *cb_msg)
{
	wmg_status_t ret = WMG_STATUS_SUCCESS;
	wifi_sta_info_t *ap_config = (wifi_sta_info_t *)param;
	wmg_status_t *cb_args = (wmg_status_t *)cb_msg;

	if(ap_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi ap mode has not been initialized\n");
		*cb_args = WMG_STATUS_FAIL;
		return WMG_STATUS_FAIL;
	}
	if(ap_object.ap_enable == WMG_FALSE) {
		WMG_WARNG("wifi ap mode already disabled\n");
		*cb_args = WMG_STATUS_FAIL;
		return WMG_STATUS_FAIL;
	}

	if(ap_object.ap_state != WIFI_AP_ENABLE) {
		WMG_WARNG("wifi ap has not been enable now\n");
		*cb_args = WMG_STATUS_UNHANDLED;
		return WMG_STATUS_UNHANDLED;
	}

	if(ap_config == NULL) {
		WMG_ERROR("ap config is NULL\n");
		*cb_args = WMG_STATUS_FAIL;
		return WMG_STATUS_FAIL;
	}

	int erro_code;
	ret = ap_object.platform_inf[PLATFORM_LINUX]->ap_platform_extension(HAPD_CMD_GET_CONFIG, (void *)ap_config, &erro_code);
	if(ret) {
		WMG_ERROR("failed to get ap config\n");
		*cb_args = WMG_STATUS_FAIL;
		return WMG_STATUS_FAIL;
	}

	*cb_args = ret;
	return ret;
}

static int ap_register_msg_cb(void *param, void *cb_msg)
{
	if(ap_object.init_flag == WMG_FALSE) {
		WMG_ERROR("ap has not been initialized\n");
		return WMG_STATUS_FAIL;
	}

	wifi_msg_cb_t *msg_cb = (wifi_msg_cb_t *)param;
	wmg_status_t *cb_args = (wmg_status_t *)cb_msg;

	if (*msg_cb == NULL) {
		WMG_WARNG("message callback is NULL\n");
		*cb_args = WMG_STATUS_UNHANDLED;
		return WMG_STATUS_UNHANDLED;
	}

	ap_object.ap_msg_cb = *msg_cb;

	*cb_args = WMG_STATUS_SUCCESS;
	return WMG_STATUS_SUCCESS;
}

static int ap_set_mac(void *param,void *cb_msg)
{
	wmg_status_t ret;

	if(ap_object.init_flag == WMG_FALSE) {
		WMG_ERROR("ap has not been initialized\n");
		return WMG_STATUS_FAIL;
	}

	common_mac_para_t *common_mac_para = (common_mac_para_t *)param;
	wmg_status_t *cb_args = (wmg_status_t *)cb_msg;

	ret = ap_object.platform_inf[PLATFORM_LINUX]->ap_platform_extension(HAPD_CMD_SET_MAC, (void *)common_mac_para, NULL);

	*cb_args = ret;
	return ret;
}

static int ap_get_mac(void *param,void *cb_msg)
{
	wmg_status_t ret;

	if(ap_object.init_flag == WMG_FALSE) {
		WMG_ERROR("ap has not been initialized\n");
		return WMG_STATUS_FAIL;
	}

	common_mac_para_t *common_mac_para = (common_mac_para_t *)param;
	wmg_status_t *cb_args = (wmg_status_t *)cb_msg;

	ret = ap_object.platform_inf[PLATFORM_LINUX]->ap_platform_extension(HAPD_CMD_SET_MAC, (void *)common_mac_para, NULL);

	*cb_args = ret;
	return ret;
}

static wmg_status_t ap_mode_init(void)
{
	if (ap_object.init_flag == WMG_FALSE) {
		//初始化平台hapd接口
		WMG_INFO("ap mode init now\n");
		ap_object.platform_inf[PLATFORM_LINUX] = NULL;
		ap_object.platform_inf[PLATFORM_RTOS] = NULL;
		//#ifde PLATFFORM_LINUX_WPA
		ap_object.platform_inf[PLATFORM_LINUX] = ap_linux_inf_object_register();
		if(ap_object.platform_inf[PLATFORM_LINUX]->ap_hapd_init != NULL){
			if(ap_object.platform_inf[PLATFORM_LINUX]->ap_hapd_init(ap_hapd_event_notify)){
				return WMG_STATUS_FAIL;
			}
		}
		ap_object.init_flag = WMG_TRUE;
	} else {
		WMG_INFO("ap mode already init\n");
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t ap_mode_deinit(void)
{
	if (ap_object.init_flag == WMG_TRUE) {
		WMG_INFO("ap mode deinit now\n");
		if(ap_object.platform_inf[PLATFORM_LINUX]->ap_hapd_deinit != NULL){
			ap_object.platform_inf[PLATFORM_LINUX]->ap_hapd_deinit();
		}
		ap_object.init_flag = WMG_FALSE;
		ap_object.ap_msg_cb = NULL;
	} else {
		WMG_INFO("ap mode already deinit\n");
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t ap_mode_enable(int* erro_code)
{
	wmg_status_t ret;

	if(ap_object.init_flag == WMG_FALSE) {
		WMG_ERROR("ap has not been initialized\n");
		return WMG_STATUS_FAIL;
	}
	if (ap_object.ap_enable == WMG_TRUE) {
		WMG_WARNG("wifi ap already enabled\n");
		return WMG_STATUS_SUCCESS;
	}

	WMG_DUMP("wifi ap enabling...\n");

	ret = ap_object.platform_inf[PLATFORM_LINUX]->ap_hapd_connect();
	if (ret) {
		WMG_ERROR("failed to connect to hostapd\n");
		return WMG_STATUS_FAIL;
	}
	ap_object.hapd_run = WMG_TRUE;
	ap_object.ap_state = WIFI_AP_ENABLE;
	WMG_DUMP("start hapd success\n");

	ap_object.ap_enable = WMG_TRUE;

	WMG_DUMP("wifi ap enable success\n");
	return ret;
}

static wmg_status_t ap_mode_disable(int* erro_code)
{
	wmg_status_t ret;

	if(ap_object.init_flag == WMG_FALSE) {
		WMG_WARNG("wifi ap has not been initialized\n");
		return WMG_STATUS_SUCCESS;
	}
	if (ap_object.ap_enable == WMG_FALSE) {
		WMG_WARNG("wifi ap already disabled\n");
		return WMG_STATUS_SUCCESS;
	}

	WMG_DUMP("wifi ap disabling...\n");

	ap_object.platform_inf[PLATFORM_LINUX]->ap_hapd_disable();
	ap_object.ap_enable = WMG_FALSE;

	WMG_DUMP("wifi ap disabled\n");

	return WMG_STATUS_SUCCESS;
}

static wmg_status_t ap_mode_ctl(int cmd, void *param, void *cb_msg)
{
	if(ap_object.init_flag == WMG_FALSE) {
		WMG_ERROR("wifi ap has not been initialized\n");
		return WMG_STATUS_FAIL;
	}
	if(ap_object.ap_enable == WMG_FALSE) {
		WMG_WARNG("wifi ap already disabled\n");
		return WMG_STATUS_SUCCESS;
	}

	WMG_DEBUG("=====ap_mode_ctl cmd: %d=====\n", cmd);

	switch (cmd) {
		case WMG_AP_CMD_ENABLE:
			if(ap_enable(param,cb_msg)){
				return WMG_STATUS_FAIL;
			}
			break;
		case WMG_AP_CMD_DISABLE:
			if(ap_disable(param,cb_msg)){
				return WMG_STATUS_FAIL;
			}
			break;
		case WMG_AP_CMD_GET_CONFIG:
			if(ap_get_config(param,cb_msg)){
				return WMG_STATUS_FAIL;
			}
			break;
		case WMG_AP_CMD_REGISTER_MSG_CB:
			if(ap_register_msg_cb(param,cb_msg)){
				return WMG_STATUS_FAIL;
			}
			break;
		case WMG_AP_CMD_SET_MAC:
			if(ap_set_mac(param,cb_msg)){
				return WMG_STATUS_FAIL;
			}
			break;
		case WMG_AP_CMD_GET_MAC:
			if(ap_get_mac(param,cb_msg)){
				return WMG_STATUS_FAIL;
			}
			break;
		default:
		return WMG_STATUS_FAIL;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_ap_object_t ap_object = {
	.init_flag = WMG_FALSE,
	.ap_enable = WMG_FALSE,
	.hapd_run = WMG_FALSE,
	.ap_state = WIFI_AP_DISABLE,
	.ap_msg_cb = NULL,
};

static mode_opt_t ap_mode_opt = {
	.mode_enable = ap_mode_enable,
	.mode_disable = ap_mode_disable,
	.mode_ctl = ap_mode_ctl,
};

static mode_object_t ap_mode_object = {
	.mode_name = "ap",
	.init = ap_mode_init,
	.deinit = ap_mode_deinit,
	.mode_opt = &ap_mode_opt,
	.private_data = &ap_object,
};

mode_object_t* wmg_ap_register_object(void)
{
	return &ap_mode_object;
}
