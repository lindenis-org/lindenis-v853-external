/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 */

#ifndef __WMG_STATION_H__
#define __WMG_STATION_H__

#include <wmg_common.h>
#include <linux/event.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EVENT_BUF_SIZE
#define EVENT_BUF_SIZE 1024
#endif

#define PLATFORM_MAX    2
#define PLATFORM_LINUX  0
#define PLATFORM_RTOS   1

typedef void(*sta_wpa_event_cb_t)(wifi_sta_event_t event);

typedef struct {
	wmg_bool_t sta_wpa_init_flag;
	wmg_bool_t sta_wpa_exist;
	wmg_bool_t sta_wpa_connected;
	pthread_mutex_t sta_wpa_mutex;
	os_net_thread_t sta_pid;
	event_handle_t *sta_event_handle;
	sta_wpa_event_cb_t sta_wpa_event_cb;
	wmg_bool_t sta_wpa_auto_reconn;
	uint32_t sta_auth_fail_cnt;
	uint32_t sta_net_not_found_cnt;
	uint32_t sta_assoc_reject_cnt;
	int (*sta_wpa_init)(sta_wpa_event_cb_t);
	void (*sta_wpa_deinit)(void);
	int (*sta_wpa_connect)(void);
	int (*sta_wpa_connect_to_ap)(wifi_sta_cn_para_t *);
	int (*sta_wpa_disconnect)(void);
	int (*sta_wpa_command)(char const *, char *, size_t);
	int (*sta_platform_extension)(int,void*,int*);
} wmg_sta_wpa_inf_object_t;

/**
 * wmg_sta_common_t - wifi station private core data
 *
 * @enable: indicate wifi station is initialized or not
 * @wpas_run: indicate wpa_supplicant is running or not
 * @state: current state of wifi station
 * @event_handle: socket pair handle
 * @pid: the id of message handle thread
 * @msg_cb: message callback function
 * @auto_reconn: auto reconnect
 * @active_recv: active receive event from wpa_supplicant or not
 * @assoc_reject_cnt: association reject counter
 * @net_not_found_cnt: network not found counter
 * @auth_fail_cnt: authentication fail counter
 */

typedef struct {
	wmg_bool_t init_flag;
	wmg_bool_t sta_enable;
	wmg_bool_t wpas_run;
	wifi_sta_state_t sta_state;
	wifi_msg_cb_t sta_msg_cb;
	wmg_bool_t sta_auto_reconn;
	wmg_bool_t sta_active_recv;
	wmg_sta_wpa_inf_object_t* platform_inf[PLATFORM_MAX];
	//返回参数需要考虑
	wmg_status_t (*wmg_sta_connect)(wifi_sta_cn_para_t *);
	wmg_status_t (*wmg_sta_disconnect)(void);
	wmg_status_t (*wmg_sta_auto_reconnect)(wmg_bool_t);
	wmg_status_t (*wmg_sta_get_info)(wifi_sta_info_t *);
	wmg_status_t (*wmg_sta_list_networks)(wifi_sta_list_t *);
	wmg_status_t (*wmg_sta_remove_networks)(char *);
	wmg_status_t (*wmg_sta_set_scan_param)(wifi_scan_param_t *);
	wmg_status_t (*wmg_sta_get_scan_results)(wifi_scan_result_t *, uint32_t *, uint32_t);
	wmg_status_t (*wmg_sta_register_msg_cb)(wifi_msg_cb_t);
	wmg_status_t (*wmg_sta_set_mac)(const char *ifname, uint8_t *);
	wmg_status_t (*wmg_sta_get_mac)(const char *ifname, uint8_t *);
} wmg_sta_object_t;

mode_object_t* wmg_sta_register_object(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WMG_STATION_H__ */
