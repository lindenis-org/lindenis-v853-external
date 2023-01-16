/*
 * Copyright (C) 2017 XRADIO TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of XRADIO TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include <bt_manager.h>

#include <pthread.h>
#include <semaphore.h>

#include <wifimg.h>
#include <wifi_log.h>
#include <linkd.h>

#define BLINK_MSG_LEN_MAX             (108)
#define BLINK_MSG_SYNC                (0x5A6E3C6F)
#define BLINK_MSG_VERSION             (0x10)
#define BLINK_MSG_SERVICE_ID          (0x0B)

#define BLINK_MSG_VERSION_INDEX       (4)
#define BLINK_MSG_FMT_INDEX           (5)
#define BLINK_MSG_SSID_INDEX          (9)

#define BLINK_MSG_FMT_SUBC            (0)

#define BLINK_IND_LEN_MAX             (4)
#define BLINK_IND_SYNC                (0x0A)
#define BLINK_IND_STATUS_DEFAULT      (0x10)
#define BLINK_IND_STATUS_COMPLETE     (0x11)
#define BLINK_IND_STATUS_CONNECT      (0x12)

typedef enum {
	BLINK_INDICATE_SUCCESS = 0,
	BLINK_INDICATE_FAIL,
} blink_indicate_t;

typedef void (*set_state_callback)(blink_indicate_t state);

typedef struct blink_param {
	uint8_t dev_filter;
	set_state_callback cb;
} blink_param_t;

typedef struct blink_result {
	uint8_t ssid[SSID_MAX_LEN];
	uint8_t ssid_len;
	uint8_t passphrase[PSK_MAX_LEN + 1]; /* ASCII string ending with '\0' */
	uint8_t passphrase_len;
} blink_result_t;

typedef enum {
	BLINK_STATE_SUCCESS = 0,
	BLINK_STATE_FAIL,
} blink_state_t;

typedef enum {
	BLINK_OK      = 0,     /* success */
	BLINK_ERROR   = -1,    /* general error */
	BLINK_BUSY    = -2,    /* device or resource busy */
	BLINK_TIMEOUT = -3,    /* wait timeout */
	BLINK_INVALID = -4,    /* invalid argument */
} blink_ret_t;


typedef enum {
	BLINK_STATUS_NORMAL = 0,
	BLINK_STATUS_READY,
	BLINK_STATUS_BUSY,
	BLINK_STATUS_TIMEOUT,
	BLINK_STATUS_COMPLETE,
} blink_status_t;

typedef struct blink_priv {
	uint8_t devFilter;
	blink_status_t status;
	sem_t sem;
	uint8_t indicate;
	uint8_t indValue[BLINK_IND_LEN_MAX];
	uint8_t message[BLINK_MSG_LEN_MAX];
	set_state_callback cb;
} blink_priv_t;

static blink_priv_t *blink = NULL;

static uint16_t service_handle;
static int char_handle;
static int rx_handle;
static int tx_handle;
static int ccc_handle;

#define BLINK_SERVICE_UUID	((char *)"FF01")
#define BLINK_CHAR_RX_UUID	((char *)"FF02")
#define BLINK_CHAR_TX_UUID	((char *)"FF03")
#define CCCD_UUID           ((char *)"2902")

static int blink_parse_message(uint8_t *value, uint8_t *buf, uint16_t len, uint16_t offset);

void bt_blink_gatt_connection_cb(char *addr, gatts_connection_event_t event, int err)
{
	if(event == BT_GATT_CONNECTION) {
		WMG_DEBUG("gatt server connected to device: %s.\n", addr);
	} else if(event == BT_GATT_DISCONNECT) {
		WMG_DEBUG("gatt server disconnected to device: %s.\n", addr);
	} else {
		WMG_DEBUG("gatt server event unkown.\n");
	}
}

void bt_blink_gatts_add_service_cb(gatts_add_svc_msg_t *msg)
{
	if(msg != NULL) {
		service_handle = msg->svc_handle;
		WMG_DEBUG("add service handle is %d of number_hanle: %d\n", service_handle, msg->handle_num);
	}
}

void bt_blink_gatts_add_char_cb(gatts_add_char_msg_t *msg)
{
	if(msg != NULL) {
		WMG_DEBUG("add char,uuid: %s,chr handle is %d\n",msg->uuid, msg->char_handle);
		if (strcmp(msg->uuid, BLINK_CHAR_RX_UUID) == 0) {
			rx_handle = msg->char_handle;
		} else if (strcmp(msg->uuid, BLINK_CHAR_TX_UUID) == 0) {
			tx_handle = msg->char_handle;
		} else {
			char_handle = msg->char_handle;
		}
	}
}

void bt_blink_gatts_add_desc_cb(gatts_add_desc_msg_t *msg)
{
	if (msg != NULL) {
		WMG_DEBUG("desc handle is %d\n", msg->desc_handle);
		ccc_handle = msg->desc_handle;
	}
}

void bt_blink_gatt_char_read_request_cb(gatts_char_read_req_t *chr_read) //todo
{
	char value[1];
	static unsigned char count = 0;
	char dev_name[] = "aw_ble_test_1149";

	WMG_DEBUG("trans_id:%d,attr_handle:%d,offset:%d\n",chr_read->trans_id,
		chr_read->attr_handle,chr_read->offset);

	if(chr_read) {
		if (chr_read->attr_handle == rx_handle) {
			gatts_send_read_rsp_t data;
			data.trans_id = chr_read->trans_id;
			data.attr_handle = chr_read->attr_handle;
			data.status = 0x0b;
			data.auth_req = 0x00;
			value[0]= count;
			data.value = dev_name;
			data.value_len = strlen(dev_name);
			bt_manager_gatt_server_send_read_response(&data);
			return;
		} else if (chr_read->attr_handle == tx_handle) {
			gatts_send_read_rsp_t data;
			data.trans_id = chr_read->trans_id;
			data.attr_handle = chr_read->attr_handle;
			data.status = 0x0b;
			data.auth_req = 0x00;
			value[0]= count;
			data.value = dev_name;
			data.value_len = strlen(dev_name);
			bt_manager_gatt_server_send_read_response(&data);
			return;
		}

		gatts_send_read_rsp_t data;
		data.trans_id = chr_read->trans_id;
		data.attr_handle = chr_read->attr_handle;
		data.status = 0x0b;
		data.auth_req = 0x00;
		value[0]= count;
		data.value = value;
		data.value_len = 1;
		bt_manager_gatt_server_send_read_response(&data);
		count ++;
	}
}

void bt_blink_gatt_send_indication_cb(gatts_send_indication_t *send_ind)
{
	WMG_DEBUG("enter_func %d\n", __LINE__);
	blink_priv_t *priv = blink;
	blink_indicate_t ind = BLINK_INDICATE_SUCCESS;
}

void bt_blink_gatt_char_write_request_cb(gatts_char_write_req_t *char_write)
{
	blink_priv_t *priv = blink;

	WMG_DEBUG("enter_func %d\n", __LINE__);
	int ret = 0;

	if(char_write) {
		WMG_DEBUG("attr_handle: %d,tran_id: %d\n",char_write->attr_handle,
			char_write->trans_id);
		WMG_DEBUG("Value:");
		bt_manager_hex_dump(" ", 20, (unsigned char *)char_write->value, char_write->value_len);
	}

	WMG_DEBUG("char_write->need_rsp: %d\n", char_write->need_rsp);
	if (char_write->need_rsp) {
		gatts_write_rsp_t data;
		data.trans_id = char_write->trans_id;
		data.attr_handle = char_write->attr_handle;
		data.state = BT_GATT_SUCCESS;
		ret = bt_manager_gatt_server_send_write_response(&data);
		if (ret != 0)
			WMG_DEBUG("send write response failed!\n");
		else
			WMG_DEBUG("send write response success!\n");
	}

	if (char_write->value_len > BLINK_MSG_LEN_MAX) {
		WMG_DEBUG("exec blink buffer!\n");
		return;
	}

	blink_parse_message((uint8_t *)priv->message, (uint8_t *)char_write->value, char_write->value_len, 0);
}

void bt_blink_gatt_desc_read_requset_cb(gatts_desc_read_req_t *desc_read)
{
	WMG_DEBUG("enter\n");
	char value[1];
	static unsigned char count = 0;
	WMG_DEBUG("enter\n");

	WMG_DEBUG("trans_id:%d,attr_handle:%d,offset:%d\n",desc_read->trans_id,
		desc_read->attr_handle,desc_read->offset);

	if(desc_read) {
		gatts_send_read_rsp_t data;
		data.trans_id = desc_read->trans_id;
		data.attr_handle = desc_read->attr_handle;
		data.status = 0x0b;
		data.auth_req = 0x00;
		value[0]= count;
		data.value = value;
		data.value_len = 1;
		bt_manager_gatt_server_send_read_response(&data);
		count ++;
	}
}

void bt_blink_gatt_desc_write_request_cb(gatts_desc_write_req_t *desc_write)
{
	int ret = 0;
	blink_priv_t *priv = blink;

	WMG_DEBUG("enter\n");
	WMG_DEBUG("desc_write->need_rsp: %d\n", desc_write->need_rsp);
	WMG_DEBUG("desc_write->attr_handle: %d\n", desc_write->attr_handle);

	if (desc_write->need_rsp) {
		gatts_write_rsp_t data;
		data.trans_id = desc_write->trans_id;
		data.attr_handle = desc_write->attr_handle;
		data.state = BT_GATT_SUCCESS;
		ret = bt_manager_gatt_server_send_write_response(&data);
		if (ret != 0) {
			WMG_DEBUG("send write response failed!\n");
		} else {
			WMG_DEBUG("send write response success!\n");
		}
	}
	if (desc_write->attr_handle == ccc_handle) {
		WMG_DEBUG("ccc handle set %s",(desc_write->value[0]==1?"NOTIFY\n":(
			desc_write->value[0]==2?"INDICATE":"NONE")));
		priv->indicate = 1;
	}
}

void bt_gatt_server_register_callback(btmg_callback_t *cb)
{
	cb->btmg_gatt_server_cb.gatts_add_svc_cb = bt_blink_gatts_add_service_cb;
	cb->btmg_gatt_server_cb.gatts_add_char_cb = bt_blink_gatts_add_char_cb;
	cb->btmg_gatt_server_cb.gatts_add_desc_cb = bt_blink_gatts_add_desc_cb;

	cb->btmg_gatt_server_cb.gatts_connection_event_cb = bt_blink_gatt_connection_cb;
	cb->btmg_gatt_server_cb.gatts_char_read_req_cb = bt_blink_gatt_char_read_request_cb;
	cb->btmg_gatt_server_cb.gatts_char_write_req_cb = bt_blink_gatt_char_write_request_cb;

	cb->btmg_gatt_server_cb.gatts_desc_read_req_cb = bt_blink_gatt_desc_read_requset_cb;
	cb->btmg_gatt_server_cb.gatts_desc_write_req_cb = bt_blink_gatt_desc_write_request_cb;
	cb->btmg_gatt_server_cb.gatts_send_indication_cb = bt_blink_gatt_send_indication_cb;
}

int bt_blink_send_indication(int attr_handle, uint8_t *data, int len)
{
	gatts_indication_data_t indication_data;

	indication_data.attr_handle = attr_handle;
	indication_data.value = data;
	indication_data.value_len = len;

	bt_manager_gatt_server_send_indication(&indication_data);
	return 0;
}

static void le_set_adv_param(void)
{
	btmg_le_advertising_parameters_t adv_params;

	adv_params.min_interval = 0x0020;
	adv_params.max_interval = 0x01E0; /*range from 0x0020 to 0x4000*/
	adv_params.own_addr_type = BTMG_LE_RANDOM_ADDRESS;
	adv_params.adv_type =  BTMG_LE_ADV_IND; /*ADV_IND*/
	adv_params.chan_map = BTMG_LE_ADV_CHANNEL_ALL; /*0x07, *bit0:channel 37, bit1: channel 38, bit2: channel39*/
	adv_params.filter = BTMG_LE_PROCESS_ALL_REQ;
	bt_manager_le_set_adv_param(&adv_params);
}

static int le_set_adv_data(const char *ble_name,uint16_t uuid)
{
	int index = 0;
	char advdata[31] = { 0 };
	char uuid_buf[5] = {0};

	advdata[index] = 0x02;
	advdata[index + 1] = 0x01;
	advdata[index + 2] = 0x1A;

	index += advdata[index] + 1;

	advdata[index] = strlen(ble_name) + 1;
	advdata[index + 1] = 0x09;
	int name_len;
	name_len = strlen(ble_name);
	strcpy(&(advdata[index + 2]), ble_name);
	index += advdata[index] + 1;

	advdata[index] = 0x03;
	advdata[index + 1] = 0x03;
	advdata[index + 2] = (char)(uuid&0xFF);
	advdata[index + 3] = (char)((uuid>>8)&0xFF);
	index += advdata[index] + 1;

	btmg_adv_data_t adv;
	adv.data_len = index;
	memcpy(adv.data, advdata, 31);

	return bt_manager_le_set_adv_data(&adv);
}

static int bt_gatt_server_register_blink_svc()
{
	gatts_add_svc_t svc;
	gatts_add_char_t chr_rx;
	gatts_add_char_t chr_tx;
	gatts_add_desc_t desc;
	gatts_star_svc_t start_svc;

	bt_manager_gatt_server_init();

	WMG_DEBUG("add service,uuid:%s\n",BLINK_SERVICE_UUID);
	svc.number = 10;
	svc.uuid = BLINK_SERVICE_UUID;
	svc.primary = true;
	bt_manager_gatt_server_create_service(&svc);

	chr_rx.permissions = BT_GATT_PERM_READ | BT_GATT_PERM_WRITE;
	chr_rx.properties =  BT_GATT_CHAR_PROPERTY_READ
		| BT_GATT_CHAR_PROPERTY_WRITE
		| BT_GATT_CHAR_PROPERTY_SIGNED_WRITE;
	chr_rx.svc_handle = service_handle;
	chr_rx.uuid = BLINK_CHAR_RX_UUID;
	bt_manager_gatt_server_add_characteristic(&chr_rx);
	chr_tx.permissions = BT_GATT_PERM_READ | BT_GATT_PERM_WRITE;
	chr_tx.properties = BT_GATT_CHAR_PROPERTY_READ
		| BT_GATT_CHAR_PROPERTY_WRITE
		| BT_GATT_CHAR_PROPERTY_NOTIFY
		| BT_GATT_CHAR_PROPERTY_INDICATE;
	chr_tx.svc_handle = service_handle;
	chr_tx.uuid = BLINK_CHAR_TX_UUID;
	bt_manager_gatt_server_add_characteristic(&chr_tx);

	desc.permissions = BT_GATT_PERM_READ | BT_GATT_PERM_WRITE;
	desc.uuid = CCCD_UUID;
	desc.svc_handle = service_handle;
	bt_manager_gatt_server_add_descriptor(&desc);
	bt_manager_gatt_server_start_service(&start_svc);
	return 0;
}

static btmg_callback_t *bt_callback = NULL;
static int preinit_only_onece = 0;
static int __bt_init(void)
{
	bt_manager_set_loglevel(BTMG_LOG_LEVEL_DEBUG);
			WMG_DEBUG("bt preinit %d\n", preinit_only_onece);
		if(bt_manager_preinit(&bt_callback) != 0) {
			WMG_DEBUG("bt preinit failed!\n");
			return -1;
		}
		preinit_only_onece++;

	bt_manager_enable_profile(BTMG_GATT_SERVER_ENABLE);

	if(bt_manager_init(bt_callback) != 0) {
		WMG_DEBUG("bt manager init failed.\n");
		return 0;
	}

	bt_manager_enable(true);
	bt_manager_gatt_server_init();

	bt_gatt_server_register_callback(bt_callback);

	bt_gatt_server_register_blink_svc();

    return 0;
}

static int __bt_deinit(void)
{
    memset(bt_callback, 0, sizeof(btmg_callback_t));
	bt_manager_gatt_server_deinit();
    bt_manager_enable(false);
	bt_manager_deinit(bt_callback);
    return 0;
}

static int blink_parse_message(uint8_t *value, uint8_t *buf, uint16_t len, uint16_t offset)
{
	blink_priv_t *priv = blink;
	uint8_t *message = buf;
	uint32_t sync = (uint32_t)message[0] << 24 | (uint32_t)message[1] << 16 |
					(uint32_t)message[2] << 8 | message[3];
	uint8_t version = message[BLINK_MSG_VERSION_INDEX];

	if (priv->status != BLINK_STATUS_COMPLETE &&
		sync == BLINK_MSG_SYNC && version == BLINK_MSG_VERSION) {
		WMG_DEBUG("%s %d\n", __func__, __LINE__);
		if (message[BLINK_MSG_FMT_INDEX] == BLINK_MSG_FMT_SUBC) {
			/* TO DO.. */
			return -1;
		}
		memset(value, 0, BLINK_MSG_LEN_MAX);
		memcpy(value + offset, buf, len);
		*(value + offset + len) = '\0';

		priv->status = BLINK_STATUS_COMPLETE;
		sem_post(&priv->sem);
	}

	return 0;
}

static inline int blink_adv_start(void)
{
	int err = 0;
	le_set_adv_param();
	err = bt_manager_le_set_random_address();
	if (err) {
		WMG_ERROR("set addr failed\n");
	}
	le_set_adv_data("aw_bt_blink", 0xff01);
	err = bt_manager_le_enable_adv(true);
	if (err) {
		WMG_ERROR("set adv on failed\n");
	}
	return err;
}

static inline int blink_adv_stop(void)
{
	bt_manager_le_enable_adv(false);
	return 0;
}

blink_ret_t blink_start(blink_param_t *param)
{
	blink_priv_t *priv = blink;

	WMG_DEBUG("%s %d\n", __func__, __LINE__);

	if (priv != NULL) {
		WMG_WARNG("blink has started\n");
		return BLINK_ERROR;
	}
	if (param == NULL) {
		WMG_ERROR("invalid param\n");
		return BLINK_INVALID;
	}

	priv = malloc(sizeof(blink_priv_t));
	if (priv == NULL) {
		WMG_ERROR("malloc fail\n");
		return BLINK_ERROR;
	}
	memset(priv, 0, sizeof(blink_priv_t));

	__bt_init();

	if (blink_adv_start() != 0) {
		WMG_ERROR("adv start fail\n");
		goto err;
	}

	if (sem_init(&priv->sem, 0, 0) != 0) {
		WMG_ERROR("sem create fail\n");
		goto err;
	}

	priv->devFilter = param->dev_filter;
	priv->cb = param->cb;
	priv->status = BLINK_STATUS_READY;
	blink = priv;

	return BLINK_OK;

err:
	free(priv);
	return BLINK_ERROR;
}

blink_ret_t blink_wait(uint32_t timeout)
{
	blink_priv_t *priv = blink;

	if (!priv || priv->status != BLINK_STATUS_READY) {
		WMG_ERROR("blink not ready\n");
		return BLINK_ERROR;
	}

	if (timeout <= 0) {
		if (priv->status == BLINK_STATUS_COMPLETE)
			return BLINK_OK;
		return BLINK_ERROR;
	}

	if (sem_wait(&priv->sem) != 0) {
		WMG_WARNG("sem wait fail\n");
		return BLINK_TIMEOUT;
	}
	if (priv->status != BLINK_STATUS_COMPLETE)
		return BLINK_ERROR;

	return BLINK_OK;
}

blink_ret_t blink_get_result(blink_result_t *result)
{
	blink_priv_t *priv = blink;
	if (!priv || !result) {
		WMG_ERROR("invalid state or param\n");
		return BLINK_ERROR;
	}
	if (priv->status != BLINK_STATUS_COMPLETE) {
		WMG_WARNG("blink not completed\n");
		return BLINK_ERROR;
	}

	result->ssid_len = priv->message[BLINK_MSG_SSID_INDEX];
	result->passphrase_len = priv->message[priv->message[BLINK_MSG_SSID_INDEX] + BLINK_MSG_SSID_INDEX + 1];

	if (result->ssid_len > SSID_MAX_LEN || result->ssid_len == 0 ||
		result->passphrase_len > PSK_MAX_LEN || result->passphrase_len == 0) {
		WMG_WARNG("invalid len\n");
		return BLINK_ERROR;
	}
	memcpy(result->ssid, priv->message + BLINK_MSG_SSID_INDEX + 1, result->ssid_len);
	memcpy(result->passphrase, priv->message + priv->message[BLINK_MSG_SSID_INDEX] + BLINK_MSG_SSID_INDEX + 2, result->passphrase_len);
	result->ssid[result->ssid_len] = '\0';
	result->passphrase[result->passphrase_len] = '\0';

	return BLINK_OK;
}

blink_ret_t blink_set_state(blink_state_t state)
{
	blink_priv_t *priv = blink;
	WMG_WARNG("blink_set_state\n");
	if (priv == NULL) {
		WMG_WARNG("not init\n");
		return BLINK_ERROR;
	}

	if (priv->indicate > 0) {
		priv->indValue[0] = BLINK_IND_SYNC;
		priv->indValue[1] = BLINK_MSG_SERVICE_ID;
		priv->indValue[2] = (priv->status == BLINK_STATUS_COMPLETE) ?
							BLINK_IND_STATUS_COMPLETE : BLINK_IND_STATUS_DEFAULT;
		priv->indValue[3] = (state == BLINK_STATE_SUCCESS) ?
							BLINK_IND_STATUS_CONNECT : BLINK_IND_STATUS_DEFAULT;

		bt_blink_send_indication(tx_handle, &(priv->indValue[0]), sizeof(priv->indValue));
		return BLINK_OK;
	}

	return BLINK_ERROR;
}

blink_ret_t blink_stop(void)
{
	blink_priv_t *priv = blink;

	if (priv == NULL) {
		WMG_WARNG("blink not started\n");
		return BLINK_ERROR;
	}
	if (priv->status == BLINK_STATUS_BUSY) {
		WMG_WARNG("blink busy\n");
		return BLINK_BUSY;
	}

	blink_adv_stop();
	bt_manager_le_disconnect();

	sem_destroy(&priv->sem);
	__bt_deinit();
	free(blink);
	blink = NULL;
	return BLINK_OK;
}

#define BLINK_TIMEOUT_MS    300000
#define BLINK_PRINT_EN      0

struct netif *blink_wlan_netif = NULL;

static void blink_bt_app_init(void)
{
	printf("[%s]\n", __func__);
	blink_param_t param;
	memset(&param, 0, sizeof(blink_param_t));
	blink_start(&param);
}

void *_ble_mode_main_loop(void *arg)
{
	WMG_INFO("support ble mode config net\n");
	proto_main_loop_para_t *main_loop_para = (proto_main_loop_para_t *)arg;
	wmg_linkd_result_t linkd_result;

	blink_ret_t ret;
	blink_result_t result;

	blink_bt_app_init();

	sleep(2);

	if (blink_wait(BLINK_TIMEOUT_MS) != BLINK_OK) {
		WMG_ERROR("time out\n");
		blink_set_state(BLINK_STATE_FAIL);
		main_loop_para->result_cb(NULL);
		return NULL;
	}

	ret = blink_get_result(&result);
	if (ret != BLINK_OK) {
		WMG_ERROR("get result fail\n");
		blink_set_state(BLINK_STATE_FAIL);
		main_loop_para->result_cb(NULL);
		return NULL;
	}
	WMG_INFO("ssdi = '%s';password = '%s'\n", result.ssid, result.passphrase);

	blink_set_state(BLINK_STATE_SUCCESS);

	sleep(1);

	blink_stop();

	linkd_result.ssid = result.ssid;
	linkd_result.psk = result.passphrase;
	main_loop_para->result_cb(&linkd_result);

	return NULL;
}
