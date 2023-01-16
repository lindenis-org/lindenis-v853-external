#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include "displayInterface.h"
#include "utils.h"
#include "sunxi_edid.h"

/* ----disp global---- */
/* ----Set the background color---- */
int DispSetBackColor(int dispFb, unsigned int screenId, unsigned int color) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	disp_color ck;
	unsigned int r;
	unsigned int g;
	unsigned int b;
	r = (color >> 16) & 0xff;
	g = (color >> 8) & 0xff;
	b = (color >> 0) & 0xff;
	ck.alpha = 0xff;
	ck.red = r;
	ck.green = g;
	ck.blue = b;
	ioctlParam[1] = (unsigned long) &ck;
	return ioctl(dispFb, DISP_SET_BKCOLOR, ioctlParam);
}

/* ----Get the screen width---- */
int DispGetScrWidth(int dispFb, unsigned int screenId) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	return ioctl(dispFb, DISP_GET_SCN_WIDTH, ioctlParam);
}

/* ----Get the screen height---- */
int DispGetScrHeight(int dispFb, unsigned int screenId) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	return ioctl(dispFb, DISP_GET_SCN_HEIGHT, ioctlParam);
}

/* ----Get the out put type---- */
int DispGetOutPutType(int dispFb, unsigned int screenId) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	return ioctl(dispFb, DISP_GET_OUTPUT_TYPE, ioctlParam);
}

/* ----Set Vsync event enable---- */
int DispVsyncEventEnable(int dispFb, unsigned int screenId, bool enable) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) enable;
	return ioctl(dispFb, DISP_VSYNC_EVENT_EN, ioctlParam);
}

/* ----Set blank enable---- */
int DispSetBlankEnable(int dispFb, unsigned int screenId, bool enable) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) enable;
	return ioctl(dispFb, DISP_BLANK, ioctlParam);
}

/* ----Set shadow protect---- */
int DispShadowProtect(int dispFb, unsigned int screenId, bool protect) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) protect;
	return ioctl(dispFb, DISP_SHADOW_PROTECT, ioctlParam);
}

/**
 * Device switch
 * You can set lcd/tv/hdmi/vga, and set tv mode
 *
 * @outPutType lcd/tv/hdmi/vga
 * @tvMode Screen support for the tv mode
 */
int DispDeviceSwitch(int dispFb, unsigned int screenId,
		disp_output_type outPutType, disp_tv_mode tvMode) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) outPutType;
	ioctlParam[2] = (unsigned long) tvMode;
	int ret = ioctl(dispFb, DISP_DEVICE_SWITCH, ioctlParam);
	if (ret < 0)
		return ret;

	/* Change the UI layer output area */
	if (outPutType == DISP_OUTPUT_TYPE_HDMI) {
		disp_layer_config layerConfig;
		memset(&layerConfig, 0, sizeof(disp_layer_config));
		int ret = DispGetLayerConfig(dispFb, screenId, 0, 1, 1, &layerConfig);
		if (ret < 0)
			return ret;

		layerConfig.info.screen_win.x = 0;
		layerConfig.info.screen_win.y = 0;
		layerConfig.info.screen_win.width = DispGetScrWidth(dispFb, screenId);
		layerConfig.info.screen_win.height = DispGetScrHeight(dispFb, screenId);

		ret = DispSetLayerConfig(dispFb, screenId, 1, &layerConfig);
	}

	return ret;
}

/**
 * Device switch
 * You can set lcd/tv/hdmi/vga, and set tv mode
 * Tv mode and vendorId will be saved to the Reserve0 partition and read in the uboot phase
 *
 * @outPutType lcd/tv/hdmi/vga
 * @tvMode Screen support for the tv mode
 */
int DispDeviceSwitchAndSaveConfig(int dispFb, unsigned int screenId,
		disp_output_type outPutType, disp_tv_mode tvMode) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) outPutType;
	ioctlParam[2] = (unsigned long) tvMode;
	int ret = ioctl(dispFb, DISP_DEVICE_SWITCH, ioctlParam);
	if (ret < 0)
		return ret;

	/* Change the UI layer output area */
	if (outPutType == DISP_OUTPUT_TYPE_HDMI) {
		disp_layer_config layerConfig;
		memset(&layerConfig, 0, sizeof(disp_layer_config));
		int ret = DispGetLayerConfig(dispFb, screenId, 0, 1, 1, &layerConfig);
		if (ret < 0)
			return ret;

		layerConfig.info.screen_win.x = 0;
		layerConfig.info.screen_win.y = 0;
		layerConfig.info.screen_win.width = DispGetScrWidth(dispFb, screenId);
		layerConfig.info.screen_win.height = DispGetScrHeight(dispFb, screenId);

		ret = DispSetLayerConfig(dispFb, screenId, 1, &layerConfig);
	}

	SunxiEdidHandle g_edid = SunxiEdidLoad("/sys/class/hdmi/hdmi/attr/edid");
	int vendorId = SunxiEdidGetVendorId(g_edid);
	if (vendorId != 0) { /* Successfully read EDID information */
		saveDispModeToFile(DISP_OUTPUT_TYPE_HDMI, tvMode);
		savedVendorIdToFile(vendorId);
	} else {
		ret = -1;
	}
	SunxiEdidUnload(g_edid);

	return ret;
}

/* ----Set color range---- */
int DispSetColorRange(int dispFb, unsigned int screenId,
		unsigned int colorRange) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) colorRange;
	return ioctl(dispFb, DISP_SET_COLOR_RANGE, ioctlParam);

}

/* ----Get color range---- */
int DispGetColorRange(int dispFb, unsigned int screenId) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	return ioctl(dispFb, DISP_GET_COLOR_RANGE, ioctlParam);
}

/* ----layer---- */
/* ----Set layer enable---- */
int DispSetLayerEnable(int dispFb, unsigned int screenId, unsigned int layerId,
		unsigned int channelId, unsigned int layerNum, bool enable) {
	disp_layer_config layerConfig;
	memset(&layerConfig, 0, sizeof(disp_layer_config));
	int ret = DispGetLayerConfig(dispFb, screenId, layerId, channelId, layerNum,
			&layerConfig);
	if (ret < 0)
		return ret;

	layerConfig.enable = enable;

	ret = DispSetLayerConfig(dispFb, screenId, layerNum, &layerConfig);

	return ret;
}

/**
 * Set the UI layer Scaling parameter and center the display
 *
 * @scaleValue Scaling parameter 0~1
 */
int DispSetUILayerScale(int dispFb, unsigned int screenId, double scaleValue) {
	if (scaleValue < 0 || scaleValue > 1)
		return -1;

	disp_layer_config layerConfig;
	memset(&layerConfig, 0, sizeof(disp_layer_config));
	int ret = DispGetLayerConfig(dispFb, screenId, 0, 1, 1, &layerConfig);
	if (ret < 0)
		return ret;

	int srcWidth = DispGetScrWidth(dispFb, screenId);
	int srcHeight = DispGetScrHeight(dispFb, screenId);

	layerConfig.info.screen_win.width = srcWidth * scaleValue;
	layerConfig.info.screen_win.height = srcHeight * scaleValue;
	layerConfig.info.screen_win.x = (srcWidth
			- layerConfig.info.screen_win.width) / 2;
	layerConfig.info.screen_win.y = (srcHeight
			- layerConfig.info.screen_win.height) / 2;

	ret = DispSetLayerConfig(dispFb, screenId, 1, &layerConfig);

	return ret;
}

/**
 * Set layer config
 * You need to call DispGetLayerConfig before calling DispSetLayerConfig, or construct a layerConfig
 *
 * @layerId DE2 is layer num
 * @scaleValue Scaling parameter 0~1
 */
int DispSetLayerConfigScale(int dispFb, unsigned int screenId,
		unsigned int layerNum, disp_layer_config *layerConfig,
		double scaleValue) {
	if (scaleValue < 0 || scaleValue > 1)
		return -1;

	int srcWidth = DispGetScrWidth(dispFb, screenId);
	int srcHeight = DispGetScrHeight(dispFb, screenId);

	layerConfig->info.screen_win.width = srcWidth * scaleValue;
	layerConfig->info.screen_win.height = srcHeight * scaleValue;
	layerConfig->info.screen_win.x = (srcWidth
			- layerConfig->info.screen_win.width) / 2;
	layerConfig->info.screen_win.y = (srcHeight
			- layerConfig->info.screen_win.height) / 2;

	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) layerConfig;
	ioctlParam[2] = (unsigned long) layerNum;
	return ioctl(dispFb, DISP_LAYER_SET_CONFIG, ioctlParam);
}

/**
 * Set layer config
 * You need to call DispGetLayerConfig before calling DispSetLayerConfig, or construct a layerConfig
 *
 * @layerId DE2 is layer num
 */
int DispSetLayerConfig(int dispFb, unsigned int screenId, unsigned int layerNum,
		disp_layer_config *layerConfig) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) layerConfig;
	ioctlParam[2] = (unsigned long) layerNum;
	return ioctl(dispFb, DISP_LAYER_SET_CONFIG, ioctlParam);
}

/* ----Get layer config---- */
int DispGetLayerConfig(int dispFb, unsigned int screenId, unsigned int layerId,
		unsigned int channelId, unsigned int layerNum,
		disp_layer_config *layerConfig) {
	layerConfig->channel = channelId;
	layerConfig->layer_id = layerId;

	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) layerConfig;
	ioctlParam[2] = (unsigned long) layerNum;
	return ioctl(dispFb, DISP_LAYER_GET_CONFIG, ioctlParam);
}

/* ----Get layer zorder---- */
int DispSetLayerZorder(int dispFb, unsigned int screenId, unsigned int layerId,
		unsigned int channelId, unsigned int layerNum, luapi_zorder zorder) {
	disp_layer_config layerConfig;
	memset(&layerConfig, 0, sizeof(disp_layer_config));
	int ret = DispGetLayerConfig(dispFb, screenId, layerId, channelId, layerNum,
			&layerConfig);
	if (ret < 0)
		return ret;

	switch (zorder) {
	case LUAPI_ZORDER_TOP:
		layerConfig.info.zorder = 11;
		break;
	case LUAPI_ZORDER_MIDDLE:
		layerConfig.info.zorder = 5;
		break;
	case LUAPI_ZORDER_BOTTOM:
		layerConfig.info.zorder = 0;
		break;
	default:
		break;
	}
	ret = DispSetLayerConfig(dispFb, screenId, layerNum, &layerConfig);
	return ret;
}

/* ----Get layer frame id---- */
int DispGetLayerFrameId(int dispFb, unsigned int screenId, unsigned int layerId,
		unsigned int channelId, unsigned int layerNum) {
	disp_layer_config layerConfig;
	memset(&layerConfig, 0, sizeof(disp_layer_config));
	int ret = DispGetLayerConfig(dispFb, screenId, layerId, channelId, layerNum,
			&layerConfig);
	if (ret < 0)
		return ret;
	else
		return layerConfig.info.id;
}

/* ----hdmi---- */
/**
 * Checkout hdmi support mode
 *
 * @tvMode The mode to check
 */
int DispCheckHdmiSupportMode(int dispFb, unsigned int screenId,
		disp_tv_mode tvMode) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) tvMode;
	return ioctl(dispFb, DISP_HDMI_SUPPORT_MODE, ioctlParam);
}

/**
 * Get hdmi edid
 * DE2 Only supported in versions using the linux3.4 kernel
 *
 * @buf Data
 * @bytes Data length, Maximum value 1024
 */
int DispGetHdmiEdid(int dispFb, unsigned int screenId, unsigned char *buf,
		unsigned int bytes) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) buf;
	ioctlParam[2] = (unsigned long) bytes;
	return ioctl(dispFb, DISP_HDMI_GET_EDID, ioctlParam);
}

/* ----lcd---- */
/* ----Get the brightness---- */
int DispGetBrightness(int dispFb, unsigned int screenId) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	return ioctl(dispFb, DISP_LCD_GET_BRIGHTNESS, ioctlParam);
}

/* ----Set the brightness---- */
int DispSetBrightness(int dispFb, unsigned int screenId,
		unsigned int brightness) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) brightness;
	return ioctl(dispFb, DISP_LCD_SET_BRIGHTNESS, ioctlParam);
}

/**
 * Set back light enable
 * DE2 Only supported in versions using the linux3.4 kernel
 */
int DispSetBackLightEnable(int dispFb, unsigned int screenId, bool enable) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	if (enable)
		return ioctl(dispFb, DISP_LCD_BACKLIGHT_ENABLE, ioctlParam);
	else
		return ioctl(dispFb, DISP_LCD_BACKLIGHT_DISABLE, ioctlParam);
}

/* ----capture---- */
/**
 * Start screen capture
 * DE2 linux3.4 kernel not supported and return -1
 */
int DispCaptureSatrt(int dispFb, unsigned int screenId,
		disp_capture_info *captureInfo) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	int ret = ioctl(dispFb, DISP_CAPTURE_START, ioctlParam);
	if (ret < 0) {
		return ret;
	} else {
		ioctlParam[1] = (unsigned long) captureInfo;
		ret = ioctl(dispFb, DISP_CAPTURE_COMMIT, ioctlParam);
		if (ret < 0)
			ioctl(dispFb, DISP_CAPTURE_STOP, ioctlParam);
		return ret;
	}
}

/* ----Stop screen capture---- */
int DispCaptureStop(int dispFb, unsigned int screenId) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	return ioctl(dispFb, DISP_CAPTURE_STOP, ioctlParam);
}

/* ---enhance --- */
/* ----Set enhance enable---- */
int DispSetEnhanceEnable(int dispFb, unsigned int screenId, bool enable) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	if (enable)
		return ioctl(dispFb, DISP_ENHANCE_ENABLE, ioctlParam);
	else
		return ioctl(dispFb, DISP_ENHANCE_DISABLE, ioctlParam);
}

/* ----Set enhance demo enable---- */
int DispSetEnhanceDemoEnable(int dispFb, unsigned int screenId, bool enable) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	if (enable)
		return ioctl(dispFb, DISP_ENHANCE_DEMO_ENABLE, ioctlParam);
	else
		return ioctl(dispFb, DISP_ENHANCE_DEMO_DISABLE, ioctlParam);
}

/* ----Set enhance mode---- */
int DispSetEnhanceMode(int dispFb, unsigned int screenId, unsigned int mode) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	ioctlParam[1] = (unsigned long) mode;
	return ioctl(dispFb, DISP_ENHANCE_SET_MODE, ioctlParam);
}

/* ----Get enhance mode---- */
int DispGetEnhanceMode(int dispFb, unsigned int screenId) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	return ioctl(dispFb, DISP_ENHANCE_GET_MODE, ioctlParam);
}

/* ----smart backlight---- */
/* ----Set smart backlight enable---- */
int DispSetSMBLEnable(int dispFb, unsigned int screenId, bool enable) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	if (enable)
		return ioctl(dispFb, DISP_SMBL_ENABLE, ioctlParam);
	else
		return ioctl(dispFb, DISP_SMBL_DISABLE, ioctlParam);
}

/* ----Set smart backlight window---- */
int DispSetSMBLWindow(int dispFb, unsigned int screenId, disp_rect dispWindow) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) screenId;
	disp_rect window;
	window.x = dispWindow.x;
	window.y = dispWindow.y;
	window.width = dispWindow.width;
	window.height = dispWindow.height;
	ioctlParam[1] = (unsigned long) &window;
	return ioctl(dispFb, DISP_SMBL_SET_WINDOW, ioctlParam);
}

/* ---mem--- */
/* ----Mem Request---- */
int DispMemRequest(int dispFb, unsigned int memId, unsigned int memSize) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) memId;
	ioctlParam[1] = (unsigned long) memSize;
	return ioctl(dispFb, DISP_MEM_REQUEST, ioctlParam);
}

/* ----Mem Release---- */
int DispMemRelease(int dispFb, unsigned int memId) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) memId;
	return ioctl(dispFb, DISP_MEM_RELEASE, ioctlParam);
}

/* ----Mem Get Adrress---- */
unsigned long DispMemGetAdrress(int dispFb, unsigned int memId) {
	unsigned long ioctlParam[4] = { 0 };
	ioctlParam[0] = (unsigned long) memId;
	return ioctl(dispFb, DISP_MEM_GETADR, ioctlParam);
}
