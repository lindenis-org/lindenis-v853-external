/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 */

#ifndef __LINKD_H__
#define __LINKD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <wifimg.h>
#include <pthread.h>

#define WMG_LINKD_PROTO_MAX     (4)
#define DEFAULT_SECOND          180

typedef enum {
	WMG_LINKD_SUCCESS,
	WMG_LINKD_BUSY,
	WMG_LINKD_TIME_OUT,
	WMG_LINKD_FAIL,
} wmg_linkd_erro_code_t;

typedef struct {
	char *ssid;
	char *psk;
} wmg_linkd_result_t;

typedef void (*proto_result_cb)(wmg_linkd_result_t *linkd_result);
typedef void *(*proto_main_loop)(void *arg);
typedef struct {
	proto_result_cb result_cb;
	void *private;
} proto_main_loop_para_t;

typedef enum {
	WMG_LINKD_MODE_NONE,
	WMG_LINKD_MODE_BLE,
	WMG_LINKD_MODE_SOFTAP,
	WMG_LINKD_MODE_XCONFIG,
	WMG_LINKD_MODE_SOUNDWAVE,
} wmg_linkd_mode_t;

typedef enum {
	WMG_LINKD_IDEL,
	WMG_LINKD_RUNNING,
	WMG_LINKD_OFF,
} wmg_linkd_state_t;

typedef enum {
	WMG_LINKD_RESULT_SUCCESS,
	WMG_LINKD_RESULT_FAIL,
	WMG_LINKD_RESULT_INVALIN,
} wmg_linkd_result_state_t;

typedef struct {
	wmg_linkd_state_t linkd_state;
	wmg_linkd_mode_t linkd_mode_state;
	proto_main_loop *proto_main_loop_list;
	proto_main_loop_para_t main_loop_para;
	char ssid_result[SSID_MAX_LEN];
	char psk_result[PSK_MAX_LEN];
	wmg_linkd_result_state_t result_state;
	pthread_t thread;
	wmg_linkd_erro_code_t (*linkd_init)(void);
	wmg_linkd_erro_code_t (*linkd_protocol_enable)(wmg_linkd_mode_t protocol_mode, void *proto_param);
	wmg_linkd_erro_code_t (*linkd_protocol_get_results)(wmg_linkd_result_t *linkd_result, int second);
	void (*linkd_deinit)(void);
} wmg_linkd_object_t;

wmg_linkd_erro_code_t wmg_linkd_protocol(wmg_linkd_mode_t mode, void *params, int second, wmg_linkd_result_t *linkd_result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __LINKD_H__ */
