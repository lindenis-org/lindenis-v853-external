/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _DISPLAYPOLICY_H_
#define _DISPLAYPOLICY_H_

//#include <utils/Mutex.h>
#include <vector>
#include <mutex>
#include "sunxi_edid.h"

//using android::Mutex;
using std::vector;

class DisplayPolicy {
private:
	//Mutex mMutex;
	SunxiEdidHandle g_edid;
	int mDispFd;
	static DisplayPolicy *instance;
	std::mutex mLock;

public:
	DisplayPolicy(int dispFd);
	~DisplayPolicy();
	DisplayPolicy *getInstance() {
		return instance;
	};

	void notifyDispDevicePlugChange(const char *name, bool isPlugIn);
	void notifyDispDevicePlugChange(int type, bool isPlugIn);
	int getSupportModeList(std::vector<int>& out);
	int setSupportDispTvMode();
	int setEdidDispTvMode(std::vector<int> standardTiming);
	int setResolution(disp_tv_mode tvMode);
	int dispDeviceChange();
	int dispDeviceInit();

	static void hotplugCallbakcEntry(const char *name, bool connect);

};

#endif //ifndef _DISPLAYPOLICY_H_

