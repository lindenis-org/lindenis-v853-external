#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include "DisplayPolicy.h"
#include "platform-sun8iw7p1.h"
#include "displayInterface.h"
#include "utils.h"

#define HDMI_HPD_STATE_FILENAME "/sys/class/switch/hdmi/state"
#define HDMI_STTR_EDID_FILENAME "/sys/class/hdmi/hdmi/attr/edid"

DisplayPolicy * DisplayPolicy::instance = NULL;
void DisplayPolicy::hotplugCallbakcEntry(const char *name, bool connect) {
	if (instance)
		instance->notifyDispDevicePlugChange(name, connect);
}

DisplayPolicy::DisplayPolicy(int dispFd) :
		mDispFd(dispFd) {
	g_edid = NULL;
	instance = this;
}

DisplayPolicy::~DisplayPolicy() {
	if (g_edid)
		SunxiEdidUnload(g_edid);
}

void DisplayPolicy::notifyDispDevicePlugChange(int type, bool isPlugIn) {
	if (0 == type)
		return;

	if (type == DISP_OUTPUT_TYPE_HDMI) {
		/* HDMI */
		if (isPlugIn) {
			std::unique_lock < std::mutex > lock(mLock);
			dispDeviceChange();
		}
	}
	return;
}

void DisplayPolicy::notifyDispDevicePlugChange(const char *name,
		bool isPlugIn) {
	struct _deviceIdentify {
		const char *identify;
		int type;
	};
	const _deviceIdentify __maps[] = { { "hdmi", DISP_OUTPUT_TYPE_HDMI }, {
			"cvbs", DISP_OUTPUT_TYPE_TV }, { NULL, -1 }, };
	int i = 0;
	int find = 0;
	while (__maps[i].identify != NULL) {
		if (!strcmp(__maps[i].identify, name)) {
			find = 1;
			break;
		}
		i++;
	}
	if (find)
		notifyDispDevicePlugChange(__maps[i].type, isPlugIn);
	return;
}

int DisplayPolicy::getSupportModeList(std::vector<int>& out) {
	int size = sizeof(_hdmi_supported_modes) / sizeof(_hdmi_supported_modes[0]);
	for (int i = size - 1; i >= 0; i--) {
		disp_tv_mode mode = (disp_tv_mode) _hdmi_supported_modes[i];
		if (DispCheckHdmiSupportMode(mDispFd, 0, mode))
			out.push_back(mode);
	}
	return 0;
}

int DisplayPolicy::setSupportDispTvMode() {
	std::vector<int> supportTiming;
	getSupportModeList(supportTiming);
	if (supportTiming.size() > 0) {
		for (std::vector<int>::iterator it = supportTiming.begin();
				it != supportTiming.end(); ++it) {
			int ret = DispDeviceSwitch(mDispFd, 0, DISP_OUTPUT_TYPE_HDMI,
					(disp_tv_mode) *it);
			if (ret < 0) {
				ALOGE("DispDeviceSwitch failure disp_tv_mode is %d\n", *it);
			} else {
				ALOGD("DispDeviceSwitch success disp_tv_mode is %d\n", *it);
				return 0;
			}
		}
	} else {
		ALOGE("No support mode list found\n");
	}
	return -1;
}

int DisplayPolicy::setEdidDispTvMode(std::vector<int> standardTiming) {
	int switchFlag = -1;
	for (std::vector<int>::iterator it = standardTiming.begin();
			it != standardTiming.end(); ++it) {
		disp_tv_mode tvMode = SunxiEdidGetTvModeByVic(*it);
		if (tvMode != DISP_TV_MODE_NUM) {
			if (DispCheckHdmiSupportMode(mDispFd, 0, tvMode)) {
				int ret = DispDeviceSwitch(mDispFd, 0, DISP_OUTPUT_TYPE_HDMI,
						tvMode);
				if (ret < 0) {
					ALOGE("DispDeviceSwitch failure disp_tv_mode is %d\n",
							tvMode);
				} else {
					ALOGD("DispDeviceSwitch success disp_tv_mode is %d\n",
							tvMode);
					switchFlag = 1;
					break;
				}
			}
		} else {
			ALOGE("SunxiEdidGetTvModeByVic failure vic is %d\n", *it);
		}
	}

	if (switchFlag)
		return 0;
	else if (setSupportDispTvMode() == 0)
		return 0;
	else
		return -1;
}

int DisplayPolicy::setResolution(disp_tv_mode tvMode) {
	int ret = -1;
	if (DispCheckHdmiSupportMode(mDispFd, 0, tvMode)) {
		ret = DispDeviceSwitch(mDispFd, 0, DISP_OUTPUT_TYPE_HDMI, tvMode);
		if (ret < 0) {
			ALOGE("DispDeviceSwitch failure disp_tv_mode is %d\n", tvMode);
		} else {
			ALOGD("DispDeviceSwitch success disp_tv_mode is %d\n", tvMode);
			ret = 0;
		}
	}
	return ret;
}

int DisplayPolicy::dispDeviceChange() {
	if (g_edid == NULL)
		g_edid = SunxiEdidLoad(HDMI_STTR_EDID_FILENAME);
	else
		SunxiEdidReload(g_edid);

	//SunxiEdidDump(g_edid);

	int vendorId = SunxiEdidGetVendorId(g_edid);
	if (vendorId != 0) { /* Successfully read EDID information */
		std::vector<int> standardTiming = SunxiEdidGetStandardTiming(g_edid);
		int saveVendorId = getSavedVendorIdFromFile();
		if (saveVendorId == vendorId) {
			int tvMode = getDispModeFormFile(DISP_OUTPUT_TYPE_HDMI);
			if (tvMode < 0) {
				setEdidDispTvMode(standardTiming);
			} else {
				if (setResolution((disp_tv_mode) tvMode) != 0) {
					setEdidDispTvMode(standardTiming);
				}
			}
		} else {
			setEdidDispTvMode(standardTiming);
		}
	} else {
		setSupportDispTvMode();
	}
	return 0;
}

int DisplayPolicy::dispDeviceInit() {
	if (getConnectStateFromFile(HDMI_HPD_STATE_FILENAME)) {
		ALOGD("Hdmi connection status is 1\n");
		dispDeviceChange();
	} else {
		ALOGD("Hdmi connection status is 0\n");
	}
	return 0;
}

