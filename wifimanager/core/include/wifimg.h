/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 */

#ifndef __WIFI_MG_H__
#define __WIFI_MG_H__

#ifdef __cplusplus
extern "C" {
#endif

/*---------linux---------*/
#include <stdint.h>
#include <stdbool.h>

#ifndef WMG_VERSION
#define WMG_VERSION     "2.0.1"
#endif


#ifndef WMG_DECLARE
#define WMG_DECLARE		"Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved"
#endif

#ifndef WMG_DEFAULT_INF
#define WMG_DEFAULT_INF "wlan0"
#endif

#ifndef BSSID_MAX_LEN
#define BSSID_MAX_LEN 	48
#endif

#ifndef SSID_MAX_LEN
#define SSID_MAX_LEN 	64
#endif

#ifndef PSK_MAX_LEN
#define PSK_MAX_LEN 	32
#endif

#ifndef STA_MAX_NUM
#define STA_MAX_NUM	20
#endif

/**
 * wmg_bool_t - bool type
 */
typedef enum {
	WMG_FALSE,
	WMG_TRUE,
} wmg_bool_t;

/**
 * wmg_status_t - indicate error code
 */
typedef enum {
	WMG_STATUS_SUCCESS = 0,
	WMG_STATUS_FAIL = -1,
	WMG_STATUS_NOT_READY = -2,
	WMG_STATUS_NOMEM = -3,
	WMG_STATUS_BUSY = -4,
	WMG_STATUS_UNSUPPORTED = -5,
	WMG_STATUS_INVALID = -6,
	WMG_STATUS_TIMEOUT = -7,
	WMG_STATUS_UNHANDLED = -8,
} wmg_status_t;

/**
 * wifi_mode_t - indicate current wifi mode
 *
 * @WIFI_STATION: wifi station mode
 * @WIFI_AP: wifi softap mode
 * @WIFI_MONITOR: wifi monitor mode
 * @WIFI_AP_STATION: concurrent mode of station and softap
 * @WIFI_AP_MONITOR: concurrent mode of softap and monitor
 * @WIFI_MODE_UNKNOWN: unknown wifi mode
 */
typedef enum {
	WIFI_STATION,
	WIFI_AP,
	WIFI_MONITOR,
	WIFI_AP_STATION,
	WIFI_AP_MONITOR,
	WIFI_MODE_UNKNOWN,
} wifi_mode_t;

/**
 * wifi_dev_status_t - indicate state of netif
 */
typedef enum {
	WLAN_STATUS_DOWN,
	WLAN_STATUS_UP,
} wifi_dev_status_t;

/**
 * wifi_msg_id_t - identify message for different conditions
 *
 * @WIFI_MSG_ID_DEV_STATUS: indicate network interface state change
 * @WIFI_MSG_ID_STA_CN_EVENT: indicate station connect event change
 * @WIFI_MSG_ID_STA_STATE_CHANGE: indicate station state change
 * @WIFI_MSG_ID_AP_CN_EVENT: indicate ap connect event change
 * @WIFI_MSG_ID_MONITOER: indicate receive data in monitor mode
 * @WIFI_MSG_ID_MAX: number of message id
 */
typedef enum {
	WIFI_MSG_ID_DEV_STATUS,
	WIFI_MSG_ID_STA_CN_EVENT,
	WIFI_MSG_ID_STA_STATE_CHANGE,
	WIFI_MSG_ID_AP_CN_EVENT,
	WIFI_MSG_ID_MONITOR,
	WIFI_MSG_ID_MAX,
} wifi_msg_id_t;

/**
 * wifi_secure_t - type of encryption
 */
typedef enum {
	WIFI_SEC_NONE,
	WIFI_SEC_WEP,
	WIFI_SEC_WPA_PSK,
	WIFI_SEC_WPA2_PSK,
} wifi_secure_t;

/**
 * wifi_sta_state_t - indicate wifi station state
 *
 * @WIFI_STA_IDLE: initial state, means that successfully completion of
 *		loading driver, running wpa_s and wifi manager connecting to wpa_s
 * @WIFI_STA_CONNECTING: start to connect specified ap
 * @WIFI_STA_CONNECTED: complete the connection
 * @WIFI_STA_OBTAINING_IP: start udhcpc to get ip address
 * @WIFI_STA_NET_CONNECTED: get ip address success
 * @WIFI_STA_DISCONNECTING: start to disconnect from ap
 * @WIFI_STA_DISCONNECTED: complete the disconnection
 */
typedef enum {
	WIFI_STA_IDLE,
	WIFI_STA_CONNECTING,
	WIFI_STA_CONNECTED,
	WIFI_STA_OBTAINING_IP,
	WIFI_STA_NET_CONNECTED,
	WIFI_STA_DHCP_TIMEOUT,
	WIFI_STA_DISCONNECTING,
	WIFI_STA_DISCONNECTED,
} wifi_sta_state_t;

/**
 * wifi_sta_event_t - event type for the flow of connecting to ap
 *
 * @WIFI_DISCONNECTED: complete disconnection from ap
 * @WIFI_SCAN_STARTED: trigger scan flow
 * @WIFI_SCAN_FAILED: fail to complete scanning, maybe scan param is
 *		invalid, lower layer is not ready or other reason
 * @WIFI_SCAN_RESULTS: complete scan flow
 * @WIFI_NETWORK_NOT_FOUND: complete scanning but none of ap is found
 * @WIFI_PASSWORD_INCORRECT: password is not correct
 * @WIFI_ASSOC_REJECT: association reject by ap
 * @WIFI_CONNECTED: complete connection with ap
 * @WIFI_TERMINATING: terminating event from wpa_supplicant
 * @WIFI_UNKNOWN: unknown connection event
 */
typedef enum {
	WIFI_DISCONNECTED,
	WIFI_SCAN_STARTED,
	WIFI_SCAN_FAILED,
	WIFI_SCAN_RESULTS,
	WIFI_NETWORK_NOT_FOUND,
	WIFI_PASSWORD_INCORRECT,
	WIFI_AUTHENTIACATION,
	WIFI_AUTH_REJECT,
	WIFI_ASSOCIATING,
	WIFI_ASSOC_REJECT,
	WIFI_ASSOCIATED,
	WIFI_4WAY_HANDSHAKE,
	WIFI_GROUNP_HANDSHAKE,
	WIFI_GROUNP_HANDSHAKE_DONE,
	WIFI_CONNECTED,
	WIFI_CONNECT_TIMEOUT,
	WIFI_DEAUTH,
	WIFI_DHCP_START,
	WIFI_DHCP_TIMEOUT,
	WIFI_DHCP_SUCCESS,
	WIFI_TERMINATING,
	WIFI_UNKNOWN,
} wifi_sta_event_t;

/**
 * wifi_sta_info_t - store wifi station information
 *
 * @bssid: BSSID
 * @ssid: SSID
 * @mac_addr: mac address
 * @ip_addr: ip address
 * @sec: type of encryption
 * @id: network id
 * @freq: frequency of the channel in MHz (e.g., 2412 = channel 1)
 */
typedef struct {
	int id;
	int freq;
	uint8_t bssid[6];
	char ssid[SSID_MAX_LEN];
	uint8_t mac_addr[6];
	uint8_t ip_addr[4];
	wifi_secure_t sec;
} wifi_sta_info_t;

/**
 * wifi_sta_list_t - store wifi list information
 *
 * @id: network id
 * @ssid: network ssid
 * @bssid: network bssid
 * @flags: network flags
 */
typedef struct {
	int id;
	char ssid[SSID_MAX_LEN];
	char bssid[BSSID_MAX_LEN];
	char flags[16];
} wifi_sta_list_nod_t;

/**
 * wifi_sta_list_t - store wifi list information
 *
 * @list_nod: list node
 * @list_num: define how many node can be saved
 * @sys_list_num: define How much does the system actually save(Called back by the system)
 */
typedef struct {
	wifi_sta_list_nod_t * list_nod;
	int list_num;
	int sys_list_num;
} wifi_sta_list_t;

/**
 * wifi_cn_para_t - connect parameters
 *
 * @ssid: SSID
 * @password: password of string format
 * @sec: type of encryption
 * @fast_connect: 1 for enable fast connect, 0 for disable
 */
typedef struct {
	const char *ssid;
	const char *password;
	wifi_secure_t sec;
	bool fast_connect;
} wifi_sta_cn_para_t;

/**
 * wifi_scan_param_t - scan parameters
 *
 * @ssid: specify SSID to scan
 */
typedef struct {
	const char *ssid;
} wifi_scan_param_t;

/**
 * wifi_scan_result_t - scan results
 *
 * @bssid: BSSID
 * @ssid: SSID
 * @freq: current frequency
 * @rssi: signal level
 * @key_mgmt: type of encryption
 */
typedef struct {
	char bssid[BSSID_MAX_LEN];
	char ssid[SSID_MAX_LEN];
	uint32_t freq;
	int rssi;
	wifi_secure_t key_mgmt;
} wifi_scan_result_t;

/**
 * wifi_ap_state_t - indicate wifi ap state
 */
typedef enum {
	WIFI_AP_DISABLE,
	WIFI_AP_ENABLE,
} wifi_ap_state_t;

/**
 * wifi_ap_event_t_t - event type for ap
 */
typedef enum {
	WIFI_AP_ENABLED = 1,
	WIFI_AP_DISABLED,
	WIFI_AP_STA_DISCONNECTED,
	WIFI_AP_STA_CONNECTED,
	WIFI_AP_UNKNOWN,
} wifi_ap_event_t;

/**
 * wifi_ap_config_t - store wifi ap configuration
 *
 * @ssid: SSID
 * @psk: password
 * @sec: type of encryption
 * @channel: current channel
 * @dev_list: device list of stations connected to this ap
 * @sta_num: number of station connected this ap
 */
typedef struct {
	char *ssid;
	char *psk;
	wifi_secure_t sec;
	uint8_t channel;
	char *dev_list[STA_MAX_NUM];
	uint8_t sta_num;
} wifi_ap_config_t;

/**
 * wifi_monitor_state_t - indicate wifi monitor state
 */
typedef enum {
	WIFI_MONITOR_DISABLE,
	WIFI_MONITOR_ENABLE,
} wifi_monitor_state_t;

/**
 * wifi_monitor_data_t - received data in wifi monitor mode
 *
 * @data: store frame
 * @len: length of frame
 * @channel: which channel data,
 * @info: custom private data,
 */
typedef struct {
	uint8_t *data;
	uint32_t len;
	uint8_t channel;
	void *info;
} wifi_monitor_data_t;

/**
 * wifi_msg_data_t - store message data
 *
 * @id: message id, specify the type of message
 * @data: valid data
 */
typedef struct {
	wifi_msg_id_t id;
	union {
		wifi_dev_status_t d_status;
		wifi_sta_event_t event;
		wifi_sta_state_t state;
		wifi_ap_event_t ap_event;
		wifi_ap_state_t ap_state;
		wifi_monitor_state_t mon_state;
		wifi_monitor_data_t *frame;
	} data;
} wifi_msg_data_t;

/**
 * wifi_msg_cb_t - callback to handle message
 */
typedef void (*wifi_msg_cb_t)(wifi_msg_data_t *msg);

/**
 * wifimanager_init - wifi manager init
 *
 * return: 0 - wifi manager init success, others - fail
 */
wmg_status_t wifimanager_init(void);

/**
 * wifimanager_deinit - wifi manager deinit
 *
 * return: 0 - wifi manager deinit success, others - fail
 */
wmg_status_t wifimanager_deinit(void);

/**
 * wifi_on - wifi manager init with specified wifi mode
 *
 * @mode: specified wifi mode
 * return: 0 - wifi manager init success, others - fail
 */
wmg_status_t wifi_on(wifi_mode_t mode);

/**
 * wifi_off - wifi manager deinit
 *
 * return: 0 - wifi manager deinit success, others - fail
 */
wmg_status_t wifi_off(void);

/**
 * wifi_sta_connect - connect to specified ap with connect parameters
 *
 * @cn_para: specified connect parameters, containing ssid, psk...
 * return: 0 - sta connect to ap success, others - fail
 */
wmg_status_t wifi_sta_connect(wifi_sta_cn_para_t *cn_para);

/**
 * wifi_sta_disconnect - disconnect from ap
 *
 * return: 0 - sta disconnect from ap success, others - fail
 */
wmg_status_t wifi_sta_disconnect(void);

/**
 * wifi_sta_auto_reconnect - set auto reconnect
 *
 * @enable: true - enable auto reconnect, false - disable auto reconnect
 * return: 0 - set auto reconnect success, others - fail
 */
wmg_status_t wifi_sta_auto_reconnect(wmg_bool_t enable);

/**
 * wifi_sta_get_info - get station information, such as bssid, ssid, ip addr...
 *
 * @sta_info: used to store station information
 * return: 0 - get station information success, others - fail
 */
wmg_status_t wifi_sta_get_info(wifi_sta_info_t *sta_info);

/**
 * wifi_sta_list_networks -  list saved network information, such as id, ssid, bssid, flags
 *
 * @sta_list_info: used to store list networks information
 * return: 0 - get station information success, others - fail
 */
wmg_status_t wifi_sta_list_networks(wifi_sta_list_t *sta_list);

/**
 * wifi_sta_remove_networks -  remove a networks or all networks
 *
 * @ssid: the networks want to remove (If input NULL it will remove all networks)
 * return: 0 - get station information success, others - fail
 */
wmg_status_t wifi_sta_remove_networks(char *ssid);

/**
 * wifi_ap_enable - bring up ap with specified config information
 *
 * @ap_config: specified ap config information, such as ssid, psk, channel...
 * return: 0 - bring up ap success, others - fail
 */
wmg_status_t wifi_ap_enable(wifi_ap_config_t *ap_config);

/**
 * wifi_ap_disable - bring down ap
 *
 * return: 0 - bring down ap success, others - fail
 */
wmg_status_t wifi_ap_disable(void);

/**
 * wifi_ap_get_config - get ap's current config
 *
 * @ap_config: used to store current ap config information, such as ssid, psk, channel...
 * return: 0 - get ap config success, others - fail
 */
wmg_status_t wifi_ap_get_config(wifi_ap_config_t *ap_config);

/**
 * wifi_monitor_enable - enter promisc mode
 *
 * return: 0 - enter promisc mode success, others - fail
 */
wmg_status_t wifi_monitor_enable(uint8_t channel);

/**
 * wifi_monitor_set_channel - monitor mode set channel
 *
 * return: 0 - monitor mode set channel success, others - fail
 */
wmg_status_t wifi_monitor_set_channel(uint8_t channel);

/**
 * wifi_monitor_disable - exit promisc mode
 *
 * return: 0 - exit promisc mode success, others - fail
 */
wmg_status_t wifi_monitor_disable(void);



/**
 * wifi_register_msg_cb - register message callback function
 *
 * @msg_cb: pointer to message callback function
 * return: 0 - register callback function success, others - fail
 */
wmg_status_t wifi_register_msg_cb(wifi_msg_cb_t msg_cb);

/**
 * wifi_set_scan_param - set scan parameters in station mode
 *
 * @scan_param: specify scan parameters
 * return: 0 - set scan parameters success, others - fail
 */
wmg_status_t wifi_set_scan_param(wifi_scan_param_t *scan_param);

/**
 * wifi_get_scan_results - get scan results
 *
 * @result: an array used to store scan results, it contains bssid, ssid, channel, rssi...
 * @bss_num: indicate the number of bss contained in scan results
 * @arr_size: indicate the capacity of @result to avoid memory overflow
 * return: 0 - get scan results success, others - fail
 */
wmg_status_t wifi_get_scan_results(wifi_scan_result_t *result, uint32_t *bss_num, uint32_t arr_size);

/**
 * wifi_set_mac - set mac address for ifname
 *
 * @mac_addr: mac address
 * return: 0 - set mac address success, others - fail
 */
wmg_status_t wifi_set_mac(const char *ifname, uint8_t *mac_addr);

/**
 * wifi_get_mac - get ifname's mac address
 *
 * @mac_addr: used to store mac address
 * return: 0 - get mac address success, others - fail
 */
wmg_status_t wifi_get_mac(const char *ifname, uint8_t *mac_addr);

/**
 * wifi_send_80211_rframe - send 80211 raw frame
 *
 * @data: store 80211 raw frame
 * @len: the length of 80211 raw frame
 * @priv: custom private data
 * return: 0 - send 80211 raw frame success, others - fail
 */
wmg_status_t wifi_send_80211_rframe(uint8_t *data, uint32_t len, void *priv);

wmg_status_t wifi_set_country_code(const char *country_code);

wmg_status_t wifi_get_country_code(char *country_code);

wmg_status_t wifi_set_ps_mode(wmg_bool_t enable);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WIFI_MG_H__ */
