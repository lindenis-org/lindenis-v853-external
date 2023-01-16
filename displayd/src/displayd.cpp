#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <cutils/log.h>
#include "hotplugListener.h"
#include "DisplayPolicy.h"

static int disphd;
static hotplugListener *hotplugMonitor;

void Stop(int signo) {
	if (hotplugMonitor) {
		hotplugMonitor->stop();
		delete hotplugMonitor;
		hotplugMonitor = NULL;
	}

	close(disphd);
}

int main() {

	int ret;

	signal(SIGINT, Stop);

	disphd = open("/dev/disp", O_RDWR);
	if (disphd == -1) {
		ALOGE("open display device faild ( %s )", strerror(errno));
		return -1;
	}

	ALOGI("Start hdmi hot plug service\n");
	/* display policy mode */
	DisplayPolicy *runtime_policy = new DisplayPolicy(disphd);
	/* Change the resolution once during initialization
	 * to ensure that the screen has a display */
	runtime_policy->dispDeviceInit();
	/* hotplugListener: monitor the hotplug uevent and dispatch it */
	hotplugMonitor = new hotplugListener(hotplugListener::createUeventSocket());

	hotplugMonitor->registerHotplugCallback(
			reinterpret_cast<void *>(runtime_policy->hotplugCallbakcEntry));

	hotplugMonitor->start();

	while (1) {
		sleep(10);
		if (NULL == hotplugMonitor)
			break;
	}
	return 0;
}
