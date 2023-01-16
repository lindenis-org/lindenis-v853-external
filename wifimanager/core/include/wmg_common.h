/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 */

#ifndef __WMG_COMMON_H__
#define __WMG_COMMON_H__

#include <os_net_utils.h>
#include <os_net_mutex.h>
#include <os_net_queue.h>
#include <os_net_sem.h>
#include <os_net_thread.h>
#include <api_action.h>
#include <wifimg.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *wmg_sec_to_str(wifi_secure_t sec);
const char *wmg_sec_to_str(wifi_secure_t sec);

/* Number of modes supported by wifimanger
 * station , ap , monitor, p2p(Not yet supported)
 */
#define MODE_NUM             4
#define MODE_STA_NAME        "sta"
#define MODE_AP_NAME         "ap"
#define MODE_MONITOR_NAME    "monitor"
#define MODE_P2P_NAME        "p2p"      //Not yet supported

/*
 * mode_opt - mode operation function
 * @mode_enable: mode enable    ;int for erro code
 * @mode_disable: mode disable  ;int for erro code
 * @mode_ctl: mode control      ;int for cmd, void command parameters, int for erro code
 */
typedef struct {
	wmg_status_t (*mode_enable)(int *);
	wmg_status_t (*mode_disable)(int *);
	wmg_status_t (*mode_ctl)(int,void *,void *);
}mode_opt_t;

typedef struct {
	char mode_name[10];
	mode_opt_t *mode_opt;
	wmg_status_t (*init)(void);
	wmg_status_t (*deinit)(void);
	void *private_data;
}mode_object_t;

/* This structure is used to describe an wifimg object
 * @init: Is wifimg already initialized
 * @enable: Is wifimg already enable
 * @wifi_status: wifi dev status
 * @current_mode: wifi current mode
 * @mutex:
 * @connect_timeout: connect to supplicant timeout
 * */
typedef struct {
	wmg_bool_t init_flag;
	wmg_bool_t enable;
	wifi_dev_status_t wifi_status;
	uint8_t current_mode_bitmap;
	uint8_t support_mode_bitmap;
	os_net_mutex_t mutex_lock;
	int connect_timeout;
	mode_object_t* mode_object[MODE_NUM];
	wifi_msg_cb_t wmg_msg_cb;

	wmg_status_t (*init)(void);
	void (*deinit)(void);
	wmg_status_t (*switch_mode)(wifi_mode_t);

	wmg_bool_t (*is_init)(void);
	wifi_mode_t (*get_current_mode)(void);

	wmg_status_t (*sta_connect)(wifi_sta_cn_para_t *);
	wmg_status_t (*sta_disconnect)(void);
	wmg_status_t (*sta_auto_reconnect)(wmg_bool_t);
	wmg_status_t (*sta_get_info)(wifi_sta_info_t *);
	wmg_status_t (*sta_list_networks)(wifi_sta_list_t *);
	wmg_status_t (*sta_remove_networks)(char *);

	wmg_status_t (*ap_enable)(wifi_ap_config_t *);
	wmg_status_t (*ap_disable)(void);
	wmg_status_t (*ap_get_config)(wifi_ap_config_t *);

	wmg_status_t (*monitor_enable)(uint8_t channel);
	wmg_status_t (*monitor_set_channel)(uint8_t channel);
	wmg_status_t (*monitor_disable)(void);

	wmg_status_t (*register_msg_cb)(wifi_msg_cb_t);
	wmg_status_t (*set_scan_param)(wifi_scan_param_t *);
	wmg_status_t (*get_scan_results)(wifi_scan_result_t *, uint32_t *, uint32_t);
	wmg_status_t (*set_mac)(const char *ifname, uint8_t *);
	wmg_status_t (*get_mac)(const char *ifname, uint8_t *);
}wifimg_object_t;

wifimg_object_t* get_wifimg_object(void);

#define WMG_STA_CMD_CONNECT             0x1
#define WMG_STA_CMD_DISCONNECT          0x2
#define WMG_STA_CMD_AUTO_RECONNECT      0x3
#define WMG_STA_CMD_GET_INFO            0x4
#define WMG_STA_CMD_LIST_NETWORKS       0x5
#define WMG_STA_CMD_REMOVE_NETWORKS     0x6
#define WMG_STA_CMD_SCAN_PARAM          0x7
#define WMG_STA_CMD_SCAN_RESULTS        0x8
#define WMG_STA_CMD_REGISTER_MSG_CB     0x9
#define WMG_STA_CMD_SET_MAC             0x10
#define WMG_STA_CMD_GET_MAC             0x11
typedef struct {
	wifi_scan_result_t* scan_results;
	uint32_t *bss_num;
	uint32_t arr_size;
} sta_get_scan_results_para_t;

#define WMG_AP_CMD_ENABLE               0x1
#define WMG_AP_CMD_DISABLE              0x2
#define WMG_AP_CMD_GET_CONFIG           0x3
#define WMG_AP_CMD_REGISTER_MSG_CB      0x4
#define WMG_AP_CMD_SET_MAC              0x5
#define WMG_AP_CMD_GET_MAC              0x6

#define WMG_MONITOR_CMD_ENABLE          0x1
#define WMG_MONITOR_CMD_SET_CHANNEL     0x2
#define WMG_MONITOR_CMD_DISABLE         0x3
#define WMG_MONITOR_CMD_REGISTER_MSG_CB 0x4
#define WMG_MONITOR_CMD_SET_MAC         0x5
#define WMG_MONITOR_CMD_GET_MAC         0x6

typedef struct {
	const char* ifname;
	uint8_t *mac_addr;
} common_mac_para_t;

act_handle_t wmg_act_handle;

#define WMG_ACT_TABLE_PLA_ID          10
#define WMG_ACT_TABLE_STA_ID          11
#define WMG_ACT_TABLE_AP_ID           12
#define WMG_ACT_TABLE_MONITOR_ID      13
#define WMG_ACT_TABLE_P2P_ID          14

#define WMG_PLA_ACT_WIFI_ON                    0x0
#define WMG_PLA_ACT_WIFI_OFF                   0x1
#define WMG_PLA_ACT_WIFI_REGISTER_MSG_CB       0x2
#define WMG_PLA_ACT_WIFI_SET_MAC               0x3
#define WMG_PLA_ACT_WIFI_GET_MAC               0x4
#define WMG_PLA_ACT_WIFI_SEND_80211_RAW_FRAME  0x5
#define WMG_PLA_ACT_WIFI_SET_COUNTRY_CODE      0x6
#define WMG_PLA_ACT_WIFI_GET_COUNTRY_CODE      0x7
#define WMG_PLA_ACT_WIFI_SET_PS_MODE           0x8

#define WMG_STA_ACT_CONNECT             0x0
#define WMG_STA_ACT_DISCONNECT          0x1
#define WMG_STA_ACT_AUTO_RECONNECT      0x2
#define WMG_STA_ACT_GET_INFO            0x3
#define WMG_STA_ACT_LIST_NETWORKS       0x4
#define WMG_STA_ACT_REMOVE_NETWORKS     0x5
#define WMG_STA_ACT_SCAN_PARAM          0x6
#define WMG_STA_ACT_SCAN_RESULTS        0x7

#define WMG_AP_ACT_ENABLE               0x0
#define WMG_AP_ACT_DISABLE              0x1
#define WMG_AP_ACT_GET_CONFIG           0x2

#define WMG_MONITOR_ACT_ENABLE          0x0
#define WMG_MONITOR_ACT_SET_CHANNEL     0x1
#define WMG_MONITOR_ACT_DISABLE         0x2

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WMG_COMMON_H__ */
