/**
 * Copyright (c) 2017-2020 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File Name: displayInterface.h
 * Description : Display engine interface,compatible DE2
 * History :
 * Author  : anruliu
 * Date    : 2019/09/23
 * Comment : first version
 */
#ifndef __DISPLAYINTERFACE_H_
#define __DISPLAYINTERFACE_H_

#include <stdbool.h>
#include "sunxi_display2.h"

typedef enum {
	LUAPI_ZORDER_TOP = 0, LUAPI_ZORDER_MIDDLE = 1, LUAPI_ZORDER_BOTTOM = 2
} luapi_zorder;

/* ----disp global---- */
int DispSetBackColor(int dispFb, unsigned int screenId, unsigned int color);
int DispGetScrWidth(int dispFb, unsigned int screenId);
int DispGetScrHeight(int dispFb, unsigned int screenId);
int DispGetOutPutType(int dispFb, unsigned int screenId);
int DispVsyncEventEnable(int dispFb, unsigned int screenId, bool enable);
int DispSetBlankEnable(int dispFb, unsigned int screenId, bool enable);
int DispShadowProtect(int dispFb, unsigned int screenId, bool protect);
int DispDeviceSwitch(int dispFb, unsigned int screenId,
		disp_output_type outPutType, disp_tv_mode tvMode);
int DispDeviceSwitchAndSaveConfig(int dispFb, unsigned int screenId,
		disp_output_type outPutType, disp_tv_mode tvMode);
int DispSetColorRange(int dispFb, unsigned int screenId,
		unsigned int colorRange);
int DispGetColorRange(int dispFb, unsigned int screenId);

/* ----layer---- */
int DispSetLayerEnable(int dispFb, unsigned int screenId, unsigned int layerId,
		unsigned int channelId, unsigned int layerNum, bool enable);
int DispSetUILayerScale(int dispFb, unsigned int screenId, double scaleValue);
int DispSetLayerConfigScale(int dispFb, unsigned int screenId,
		unsigned int layerNum, disp_layer_config *layerConfig,
		double scaleValue);
int DispSetLayerConfig(int dispFb, unsigned int screenId, unsigned int layerNum,
		disp_layer_config *layerConfig);
int DispGetLayerConfig(int dispFb, unsigned int screenId, unsigned int layerId,
		unsigned int channelId, unsigned int layerNum,
		disp_layer_config *layerConfig);
int DispSetLayerZorder(int dispFb, unsigned int screenId, unsigned int layerId,
		unsigned int channelId, unsigned int layerNum, luapi_zorder zorder);
int DispGetLayerFrameId(int dispFb, unsigned int screenId, unsigned int layerId,
		unsigned int channelId, unsigned int layerNum);

/* ----hdmi---- */
int DispCheckHdmiSupportMode(int dispFb, unsigned int screenId,
		disp_tv_mode tvMode);
int DispGetHdmiEdid(int dispFb, unsigned int screenId, unsigned char *buf,
		unsigned int bytes);

/* ----lcd---- */
int DispGetBrightness(int dispFb, unsigned int screenId);
int DispSetBrightness(int dispFb, unsigned int screenId,
		unsigned int brightness);
int DispSetBackLightEnable(int dispFb, unsigned int screenId, bool enable);

/* ----capture---- */
int DispCaptureSatrt(int dispFb, unsigned int screenId,
		disp_capture_info *captureInfo);
int DispCaptureStop(int dispFb, unsigned int screenId);

/* ---enhance--- */
int DispSetEnhanceEnable(int dispFb, unsigned int screenId, bool enable);
int DispSetEnhanceDemoEnable(int dispFb, unsigned int screenId, bool enable);
int DispSetEnhanceMode(int dispFb, unsigned int screenId, unsigned int mode);
int DispGetEnhanceMode(int dispFb, unsigned int screenId);

/* ---smart backlight--- */
int DispSetSMBLEnable(int dispFb, unsigned int screenId, bool enable);
int DispSetSMBLWindow(int dispFb, unsigned int screenId, disp_rect dispWindow);

/* ---mem--- */
int DispMemRequest(int dispFb, unsigned int memId, unsigned int memSize);
int DispMemRelease(int dispFb, unsigned int memId);
unsigned long DispMemGetAdrress(int dispFb, unsigned int memId);

#endif /* __DISPLAYINTERFACE_H_ */
