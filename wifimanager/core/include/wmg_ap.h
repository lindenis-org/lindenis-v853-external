/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 */

#ifndef __WMG_AP_H__
#define __WMG_AP_H__

#include <wmg_common.h>
#include <linux/event.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATFORM_MAX    2
#define PLATFORM_LINUX  0
#define PLATFORM_RTOS   1

typedef void(*ap_hapd_event_cb_t)(wifi_ap_event_t event);

typedef struct dev_node {
	char bssid[BSSID_MAX_LEN];
	struct dev_node *next;
} dev_node_t;

//这里定义的是平台接口,与特定平台实现有关
typedef struct {
	wmg_bool_t ap_hapd_init_flag;
	wmg_bool_t ap_hapd_exist;
	wmg_bool_t ap_hapd_connected;
	pthread_mutex_t ap_hapd_mutex;
	os_net_thread_t ap_pid;
	event_handle_t *ap_event_handle;
	ap_hapd_event_cb_t ap_hapd_event_cb;
	dev_node_t *dev_list;
	uint32_t sta_num;
	int (*ap_hapd_init)(ap_hapd_event_cb_t);
	void (*ap_hapd_deinit)(void);
	int (*ap_hapd_connect)(void);
	int (*ap_hapd_enable)(wifi_ap_config_t *);
	int (*ap_hapd_disable)(void);
	int (*ap_hapd_get_config)(wifi_ap_config_t *);
	int (*ap_hapd_command)(char const *, char *, size_t);
	int (*ap_platform_extension)(int,void*,int*);
} wmg_ap_hapd_inf_object_t;

/**
 * wmg_ap_common_t - wifi ap private core data
 *
 * @enable: indicate wifi ap is initialized or not
 * @hapd_run: indicate hostapd is running or not
 * @ap_state: current state of wifi ap
 * @event_handle: socket pair handle
 * @pid: the id of message handle thread
 * @sta_num: number of stations connected to this ap
 * @dev_list: device list of stations
 * @msg_cb: message callback function
 */
//这里定义的是ap模式下通用的数据以及操作函数
typedef struct {
	wmg_bool_t init_flag;
	wmg_bool_t ap_enable;
	wmg_bool_t hapd_run;
	wifi_ap_state_t ap_state;
	wifi_msg_cb_t ap_msg_cb;
	wifi_ap_config_t ap_config;
	wmg_ap_hapd_inf_object_t* platform_inf[PLATFORM_MAX];
	wmg_status_t (*wmg_ap_enable)(wifi_ap_config_t *);
	wmg_status_t (*wmg_ap_disable)(void);
	wmg_status_t (*wmg_ap_get_config)(wifi_ap_config_t *);
	wmg_status_t (*wmg_ap_register_msg_cb)(wifi_msg_cb_t);
	wmg_status_t (*wmg_ap_set_mac)(const char *ifname, uint8_t *mac_addr);
	wmg_status_t (*wmg_ap_get_mac)(const char *ifname, uint8_t *mac_addr);
} wmg_ap_object_t;

mode_object_t* wmg_ap_register_object(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WMG_AP_H__ */
