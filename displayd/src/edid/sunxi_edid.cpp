#include "edid.h"
#include "cea_vic.h"
#include "sunxi_edid.h"
#include "platform-sun8iw7p1.h"

SunxiEdidHandle SunxiEdidLoad(const char *path) {
	EdidParser *edid = new EdidParser(path);
	edid->reload();
	edid->getVideoFormatSupported();
	return edid;
}

void SunxiEdidReload(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	edid->reload();
	edid->getVideoFormatSupported();
}

void SunxiEdidUnload(SunxiEdidHandle handle) {
	delete (EdidParser *) handle;
}

const char * SunxiEdidGetMonitorName(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	return edid->mMonitorName;
}

unsigned short SunxiEdidGetDispSizeWidth(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	return edid->mDisplaySizeH * 10;
}

unsigned short SunxiEdidGetDispSizeHeight(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	return edid->mDisplaySizeV * 10;
}

int SunxiEdidGetSinkType(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	return edid->mSinkType;
}

int SunxiEdidSupportedRGBOnly(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	return edid->mRGBOnly;
}

bool SunxiEdidIsSupportY420Sampling(SunxiEdidHandle handle, int mode) {
	EdidParser *edid = (EdidParser *) handle;
	int i;

	int vic = mode;
	for (i = 0; i < edid->mSupportedVIC.size(); i++) {
		if ((edid->mSupportedVIC[i]->vic == vic)
				&& edid->mSupportedVIC[i]->ycbcr420_sampling)
			return true;
	}
	for (i = 0; i < edid->mY420VIC.size(); i++) {
		if (edid->mY420VIC[i]->vic == vic)
			return true;
	}
	return false;
}

bool SunxiEdidIsSupportRegularSampling(SunxiEdidHandle handle, int mode) {
	EdidParser *edid = (EdidParser *) handle;
	int i;

	int vic = mode;
	for (i = 0; i < edid->mSupportedVIC.size(); i++) {
		if ((edid->mSupportedVIC[i]->vic == vic)
				&& edid->mSupportedVIC[i]->regular_sampling)
			return true;
	}
	for (i = 0; i < edid->mY420VIC.size(); i++) {
		if (edid->mY420VIC[i]->regular_sampling)
			return true;
	}
	return false;
}

int * SunxiEdidGetSupportedVideoFormat(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	return edid->VideoFormatSupported;
}

int SunxiEdidGetMaxAudioPcmChs(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	return edid->mMaxAudioPcmChs;
}

int SunxiEdidGetPreferredVideoFormat(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	return edid->mPreferredVideoFormat;
}

void SunxiEdidDump(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	edid->dump();
}

std::vector<int> SunxiEdidGetStandardTiming(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	return edid->mStandardTiming;
}

disp_tv_mode SunxiEdidGetTvModeByVic(int vic) {
	int size = sizeof(hdmi_disp_vics) / sizeof(*(hdmi_disp_vics));
	for (size_t i = 0; i < size; ++i) {
		if (hdmi_disp_vics[i].vic == vic)
			return hdmi_disp_vics[i].tvMode;
	}
	return DISP_TV_MODE_NUM;
}

int SunxiEdidGetVendorId(SunxiEdidHandle handle) {
	EdidParser *edid = (EdidParser *) handle;
	int vendorId = edid->mProductId + edid->mSerialNumber + edid->mManufactureWeek
			+ edid->mManufactureYear;
	return vendorId;
}
