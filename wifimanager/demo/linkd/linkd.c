#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <wifimg.h>
#include <wifi_log.h>
#include <linkd.h>

static wmg_linkd_object_t wmg_linkd_object;

static void linkd_mode_result_cb(wmg_linkd_result_t *linkd_result)
{
	wmg_linkd_erro_code_t ret = WMG_LINKD_FAIL;
	int ssid_len, psk_len;

	if(linkd_result == NULL){
		WMG_ERROR("get ssid and psk fail\n");
		wmg_linkd_object.result_state = WMG_LINKD_RESULT_FAIL;
		return;
	}

	ssid_len = strlen(linkd_result->ssid);
	psk_len = strlen(linkd_result->psk);
	if(((ssid_len < 0) || (ssid_len > SSID_MAX_LEN)) || ((psk_len < 0) || (psk_len > SSID_MAX_LEN)))
	{
		WMG_ERROR("get ssid and psk size larger than buff size\n");
		wmg_linkd_object.result_state = WMG_LINKD_RESULT_FAIL;
	} else {
		strncpy(wmg_linkd_object.ssid_result, linkd_result->ssid, ssid_len);
		strncpy(wmg_linkd_object.psk_result, linkd_result->psk, psk_len);
		wmg_linkd_object.result_state = WMG_LINKD_RESULT_SUCCESS;
		WMG_DEBUG("get ssid(%s) and psk(%s) success\n", wmg_linkd_object.ssid_result, wmg_linkd_object.psk_result);
	}
}

void *__attribute__((weak)) _ble_mode_main_loop(void *arg)
{
	WMG_WARNG("weak: not't support ble mode config net\n");
	proto_main_loop_para_t *main_loop_para = (proto_main_loop_para_t *)arg;
	main_loop_para->result_cb(NULL);
}

void *__attribute__((weak)) _softap_mode_main_loop(void *arg)
{
	WMG_WARNG("weak: not't support softap mode config net\n");
	proto_main_loop_para_t *main_loop_para = (proto_main_loop_para_t *)arg;
	main_loop_para->result_cb(NULL);
}

void *__attribute__((weak)) _xconfig_mode_main_loop(void *arg)
{
	WMG_WARNG("weak: not't support xconfig mode config net\n");
	proto_main_loop_para_t *main_loop_para = (proto_main_loop_para_t *)arg;
	main_loop_para->result_cb(NULL);
}

void *__attribute__((weak)) _soundwave_mode_main_loop(void *arg)
{
	WMG_WARNG("weak: not't support sounddwave mode config net\n");
	proto_main_loop_para_t *main_loop_para = (proto_main_loop_para_t *)arg;
	main_loop_para->result_cb(NULL);
}

proto_main_loop main_loop[WMG_LINKD_PROTO_MAX] = {
	_ble_mode_main_loop,
	_softap_mode_main_loop,
	_xconfig_mode_main_loop,
	_soundwave_mode_main_loop,
};

static wmg_linkd_erro_code_t wmg_linkd_init()
{
	wmg_linkd_erro_code_t ret = WMG_LINKD_FAIL;
	wmg_linkd_object.linkd_state = WMG_LINKD_IDEL;
	wmg_linkd_object.main_loop_para.result_cb = linkd_mode_result_cb;
	return ret;
}

static wmg_linkd_erro_code_t wmg_linkd_protocol_enable(wmg_linkd_mode_t mode, void *proto_param)
{
	wmg_linkd_erro_code_t ret = WMG_LINKD_FAIL;
	if(wmg_linkd_object.linkd_state == WMG_LINKD_OFF){
		WMG_ERROR("Wmg linkd isn't init, please init first\n");
		return ret;
	}

	if(wmg_linkd_object.linkd_state != WMG_LINKD_IDEL) {
		WMG_ERROR("Wmg linkd busy, running %d mode now\n",wmg_linkd_object.linkd_mode_state);
		return WMG_LINKD_BUSY;
	}

	switch (mode) {
		case WMG_LINKD_MODE_BLE:
			if(pthread_create(&wmg_linkd_object.thread,NULL, _ble_mode_main_loop, (void *)&wmg_linkd_object.main_loop_para)){
				WMG_ERROR("create ble mode main loop pthread fail\n");
				goto enable_erro;
			} else {
				WMG_DEBUG("create ble mode main loop pthread success\n");
				ret = WMG_LINKD_SUCCESS;
			}
			break;
		case WMG_LINKD_MODE_SOFTAP:
			if(pthread_create(&wmg_linkd_object.thread,NULL, _softap_mode_main_loop, (void *)&wmg_linkd_object.main_loop_para)){
				WMG_ERROR("create softap mode main loop pthread fail\n");
				goto enable_erro;
			} else {
				WMG_DEBUG("create softap mode main loop pthread success\n");
				ret = WMG_LINKD_SUCCESS;
			}
			break;
		case WMG_LINKD_MODE_XCONFIG:
			if(pthread_create(&wmg_linkd_object.thread,NULL, _xconfig_mode_main_loop, (void *)&wmg_linkd_object.main_loop_para)){
				WMG_ERROR("create xconfig mode main loop pthread fail\n");
				goto enable_erro;
			} else {
				WMG_DEBUG("create xconfig mode main loop pthread success\n");
				ret = WMG_LINKD_SUCCESS;
			}
			break;
		case WMG_LINKD_MODE_SOUNDWAVE:
			if(pthread_create(&wmg_linkd_object.thread,NULL, _soundwave_mode_main_loop, (void *)&wmg_linkd_object.main_loop_para)){
				WMG_ERROR("create soundwave mode main loop pthread fail\n");
				goto enable_erro;
			} else {
				WMG_DEBUG("create soundwave mode main loop pthread success\n");
				ret = WMG_LINKD_SUCCESS;
			}
			break;
		default:
			goto enable_erro;
			break;
	}

	wmg_linkd_object.linkd_state = WMG_LINKD_RUNNING;
	wmg_linkd_object.linkd_mode_state = mode;

	return ret;

enable_erro:
			wmg_linkd_object.linkd_state = WMG_LINKD_IDEL;
			wmg_linkd_object.linkd_mode_state = WMG_LINKD_MODE_NONE;
	return ret;
}

static wmg_linkd_erro_code_t wmg_linkd_protocol_get_results(wmg_linkd_result_t *linkd_result, int second)
{
	wmg_linkd_erro_code_t ret = WMG_LINKD_FAIL;
	int wait_second = DEFAULT_SECOND;
	if(second > 0) {
		wait_second = second;
	}
	while((wait_second > 0) && (wmg_linkd_object.result_state == WMG_LINKD_RESULT_INVALIN)) {
		sleep(1);
		wait_second--;
	}
	if(wait_second < 0) {
		linkd_result->ssid = NULL;
		linkd_result->psk = NULL;
		pthread_cancel(wmg_linkd_object.thread);
		return WMG_LINKD_TIME_OUT;
	} else if (wmg_linkd_object.result_state == WMG_LINKD_RESULT_SUCCESS) {
		strncpy(linkd_result->ssid, wmg_linkd_object.ssid_result, strlen(wmg_linkd_object.ssid_result));
		strncpy(linkd_result->psk, wmg_linkd_object.psk_result, strlen(wmg_linkd_object.psk_result));
		ret  = WMG_LINKD_SUCCESS;
	} else {
		ret = WMG_LINKD_FAIL;
	}
	wmg_linkd_object.result_state == WMG_LINKD_RESULT_INVALIN;

	return ret;
}

static void wmg_linkd_deinit(void)
{
	wmg_linkd_object.thread = -1;
	wmg_linkd_object.linkd_mode_state = WMG_LINKD_MODE_NONE;
	memset(wmg_linkd_object.ssid_result, 0, SSID_MAX_LEN);
	memset(wmg_linkd_object.psk_result, 0, PSK_MAX_LEN);
	wmg_linkd_object.result_state = WMG_LINKD_RESULT_INVALIN;
	wmg_linkd_object.linkd_state = WMG_LINKD_OFF;
}

static wmg_linkd_object_t wmg_linkd_object = {
	.ssid_result = {0},
	.psk_result = {0},
	.result_state = WMG_LINKD_RESULT_INVALIN,
	.thread = -1,
	.linkd_state = WMG_LINKD_OFF,
	.linkd_mode_state = WMG_LINKD_MODE_NONE,
	.linkd_init = wmg_linkd_init,
	.linkd_protocol_enable = wmg_linkd_protocol_enable,
	.linkd_protocol_get_results = wmg_linkd_protocol_get_results,
	.linkd_deinit = wmg_linkd_deinit,
};

wmg_linkd_erro_code_t wmg_linkd_protocol(wmg_linkd_mode_t mode, void *params, int second, wmg_linkd_result_t *linkd_result)
{
	wmg_linkd_erro_code_t ret = WMG_LINKD_FAIL;

	if(wmg_linkd_object.linkd_state == WMG_LINKD_OFF) {
		WMG_DEBUG("Wmg link isn't init, init now\n");
		wmg_linkd_object.linkd_init();
	}

	switch(mode) {
		case WMG_LINKD_MODE_BLE :
			WMG_INFO("Ble protocol linkd mode\n");
			ret = wmg_linkd_object.linkd_protocol_enable(WMG_LINKD_MODE_BLE, params);
			if(ret != WMG_LINKD_SUCCESS){
				WMG_ERROR("Ble protocol linkd mode get result fail\n");
				goto enable_erro;
			}
			break;
		case WMG_LINKD_MODE_SOFTAP :
			WMG_INFO("Softap protocol linkd mode\n");
			ret = wmg_linkd_object.linkd_protocol_enable(WMG_LINKD_MODE_SOFTAP, params);
			if(ret != WMG_LINKD_SUCCESS){
				WMG_ERROR("Softap protocol linkd mode get result fail\n");
				goto enable_erro;
			}
			break;
		case WMG_LINKD_MODE_XCONFIG :
			WMG_INFO("Xconfig protocol linkd mode\n");
			ret = wmg_linkd_object.linkd_protocol_enable(WMG_LINKD_MODE_XCONFIG, params);
			if(ret != WMG_LINKD_SUCCESS){
				WMG_ERROR("Xconfig protocol linkd mode get result fail\n");
				goto enable_erro;
			}
			break;
		case WMG_LINKD_MODE_SOUNDWAVE :
			WMG_INFO("Soundwave protocol linkd mode\n");
			ret = wmg_linkd_object.linkd_protocol_enable(WMG_LINKD_MODE_SOUNDWAVE, params);
			if(ret != WMG_LINKD_SUCCESS){
				WMG_ERROR("Soundwave protocol linkd mode get result fail\n");
				goto enable_erro;
			}
			break;
		default :
			WMG_INFO("Not support protocol linkd mode\n");
			goto enable_erro;
			break;
	}
	ret = wmg_linkd_object.linkd_protocol_get_results(linkd_result, second);
	wmg_linkd_object.linkd_deinit();
enable_erro:
	return ret;
}
