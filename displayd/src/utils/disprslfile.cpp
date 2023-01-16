#include <stdlib.h>
#include <cstring>
#include <errno.h>
#include <cutils/log.h>
#include "utils.h"
#include "sunxi_display2.h"

#define MAX_MODE_STRING_LEN (64)
#define MODE_FILE_NAME		"/mnt/Reserve0/disp_rsl.fex"
#define VENDOR_ID_FILE_NAME	"/mnt/Reserve0/tv_vdid.fex"

extern int write_to_file(const char *path, const char *buffer, int i);
extern int read_from_file(const char *path, char *buf, size_t size);

static inline int typeToLineNumbers(int type) {
	switch (type) {
	case DISP_OUTPUT_TYPE_TV:
		return 0;
	case DISP_OUTPUT_TYPE_HDMI:
		return 1;
	case DISP_OUTPUT_TYPE_VGA:
		return 2;
	default:
		return -1;
	}
}

int getDispModeFormFile(int type) {
	char valueString[MAX_MODE_STRING_LEN] = { 0 };
	char datas[MAX_MODE_STRING_LEN] = { 0 };
	int i = 0;
	int j = 0;
	int data = 0;
	int ret = 0;

	memset(valueString, 0, MAX_MODE_STRING_LEN);
	ret = read_from_file(MODE_FILE_NAME, valueString, MAX_MODE_STRING_LEN);
	if (0 >= ret)
		return -1;

	for (i = 0; valueString[i] != '\0'; i++) {
		if ('\n' == valueString[i]) {
			datas[j] = '\0';
			//ALOGD("datas = %s\n", datas);
			data = (int) strtoul(datas, NULL, 16);
			if (type == ((data >> 8) & 0xFF)) {
				return (data & 0xFF);
			}
			j = 0;
		} else {
			datas[j++] = valueString[i];
		}
	}
	return -1;
}

// save device config to /Resolve0::disp_rsl.fex
void saveDispModeToFile(int saveType, int saveMode) {
	int type = saveType;
	int mode = saveMode;
	int pack = ((type & 0xFF) << 8) | (mode & 0xFF);
	int lineNumbers = typeToLineNumbers(type);

	if (lineNumbers < 0) {
		// unknow output type
		return;
	}

	char valueString[MAX_MODE_STRING_LEN] = { 0 };
	int length = read_from_file(MODE_FILE_NAME, valueString,
			MAX_MODE_STRING_LEN);
	if (length < 0) {
		if (length != -ENOENT)
			return;
	}

	// pack fromat decode from file
	int values[3] = { 0, 0, 0 };
	if (length > 0) {
		char *begin = valueString;
		char *iter = valueString;
		char *end = valueString + length;
		for (int i = 0; (i < 3) && (iter != end); iter++) {
			if ((*iter == '\n') || (*iter == '\0')) {
				*iter = '\0';
				values[i] = (int) strtoul(begin, NULL, 16);
				begin = iter + 1;
				i++;
			}
		}
	}

	values[lineNumbers] = pack;
	memset(valueString, 0, MAX_MODE_STRING_LEN);
	int slen = sprintf(valueString, "%x\n%x\n%x\n", values[0], values[1],
			values[2]);
	write_to_file(MODE_FILE_NAME, valueString, slen);
}

int getSavedVendorIdFromFile() {
	char valueString[MAX_MODE_STRING_LEN] = { 0 };
	char *pVendorId, *pValueString, *pValueStringEnd;
	int vendorID = 0;
	int i = 0;
	int ret = 0;

	memset(valueString, 0, MAX_MODE_STRING_LEN);
	ret = read_from_file(VENDOR_ID_FILE_NAME, valueString, MAX_MODE_STRING_LEN);
	if (0 >= ret)
		return 0;
	pVendorId = valueString;
	pValueString = valueString;
	pValueStringEnd = pValueString + ret;
	for (; (i < 4) && (pValueString != pValueStringEnd); pValueString++) {
		if ('\n' == *pValueString) {
			*pValueString = '\0';
			ret = (int) strtoul(pVendorId, NULL, 16);
			vendorID |= ((ret & 0xFF) << (8 * (3 - i)));
			//ALOGD("pVendorId=%s, ret=0x%x, vendorID=0x%x",
			//    pVendorId, ret, vendorID);
			pVendorId = pValueString + 1;
			i++;
		}
	}
	return vendorID;
}

void savedVendorIdToFile(int vendorId) {
	char value[4] = { 0 };
	char valueString[MAX_MODE_STRING_LEN] = { 0 };
	int len = 0;

	value[0] = (vendorId >> 24) & 0xFF;
	value[1] = (vendorId >> 16) & 0xFF;
	value[2] = (vendorId >> 8) & 0xFF;
	value[3] = vendorId & 0xFF;
	len = sprintf(valueString, "%x\n%x\n%x\n%x\n", value[0], value[1], value[2],
			value[3]);

	write_to_file(VENDOR_ID_FILE_NAME, valueString, len);
}
