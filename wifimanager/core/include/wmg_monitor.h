/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 */

#ifndef __WMG_MONITOR_H__
#define __WMG_MONITOR_H__

#include <wmg_common.h>
#include <wifimg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATFORM_MAX    2
#define PLATFORM_LINUX  0
#define PLATFORM_RTOS   1

#ifndef MON_BUF_SIZE
#define MON_BUF_SIZE    4096
#endif

#ifndef MON_IF_SIZE
#define MON_IF_SIZE     20
#endif

typedef void(*nl_data_frame_cb_t)(wifi_monitor_data_t *frame);

typedef struct {
	struct nl_sock *nl_sock;
	int nl80211_id;
} nl80211_state_t;

typedef struct {
	wmg_bool_t monitor_nl_init_flag;
	wmg_bool_t monitor_enable;
	os_net_thread_t monitor_pid;
	nl80211_state_t *nl_state;
	nl_data_frame_cb_t nl_data_frame_cb;
	uint8_t monitor_nl_channel;
	char monitor_if[MON_IF_SIZE];
	int (*monitor_nl_init)(nl_data_frame_cb_t);
	int (*monitor_nl_deinit)(void);
	int (*monitor_nl_enable)(uint8_t);
	int (*monitor_nl_disable)(void);
	int (*monitor_nl_connect)();
	int (*monitor_nl_set_channel)(uint8_t);
	int (*monitor_platform_extension)(int,void*,int*);
} wmg_monitor_nl_inf_object_t;

/**
 * wmg_mon_common_t - wifi monitor private core data
 *
 * @enable: indicate wifi monitor is initialized or not
 * @mon_state: current state of wifi monitor
 * @nl_state: the state of netlink connection
 * @mon_if: the name of network interface which entered promisc mode
 * @pid: id of message handle thread
 @ @pkt_cnt: packets counter
 * @msg_cb: message callback function
 */
typedef struct {
	wmg_bool_t init_flag;
	wmg_bool_t monitor_enable;
	wmg_bool_t nl_run;
	wifi_monitor_state_t monitor_state;
	uint64_t pkt_cnt;
	wifi_msg_cb_t monitor_msg_cb;
	wmg_monitor_nl_inf_object_t* platform_inf[PLATFORM_MAX];
	wmg_status_t (*wmg_monitor_enable)(uint8_t);
	wmg_status_t (*wmg_monitor_set_channel)(uint8_t);
	wmg_status_t (*wmg_monitor_disable)(void);
	wmg_status_t (*wmg_monitor_register_msg_cb)(wifi_msg_cb_t);
	wmg_status_t (*wmg_monitor_set_mac)(const char *ifname, uint8_t *mac_addr);
	wmg_status_t (*wmg_monitor_get_mac)(const char *ifname, uint8_t *mac_addr);
} wmg_monitor_object_t;

mode_object_t* wmg_monitor_register_object(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WMG_MONITOR_H__ */
