#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <linux/event.h>
#include <linux/linux_wpa.h>
#include <wifi_log.h>

static int get_net_ip(const char *if_name, char *ip, int *len, int *vflag)
{
	struct ifaddrs *ifAddrStruct = NULL, *pifaddr = NULL;
	void *tmpAddrPtr = NULL;

	*vflag = 0;
	getifaddrs(&ifAddrStruct);
	pifaddr = ifAddrStruct;
	while (pifaddr != NULL) {
		if (NULL == pifaddr->ifa_addr) {
			pifaddr=pifaddr->ifa_next;
			continue;
		} else if (pifaddr->ifa_addr->sa_family == AF_INET) { // check it is IP4
			tmpAddrPtr = &((struct sockaddr_in *)pifaddr->ifa_addr)->sin_addr;
			if (strcmp(pifaddr->ifa_name, if_name) == 0) {
				inet_ntop(AF_INET, tmpAddrPtr, ip, INET_ADDRSTRLEN);
				*vflag = 4;
				break;
			}
		} else if (pifaddr->ifa_addr->sa_family == AF_INET6) { // check it is IP6
			// is a valid IP6 Address
			tmpAddrPtr = &((struct sockaddr_in *)pifaddr->ifa_addr)->sin_addr;
			if (strcmp(pifaddr->ifa_name, if_name) == 0) {
				inet_ntop(AF_INET6, tmpAddrPtr, ip, INET6_ADDRSTRLEN);
				*vflag=6;
				break;
			}
		}
		pifaddr=pifaddr->ifa_next;
	}

	if (ifAddrStruct != NULL) {
		freeifaddrs(ifAddrStruct);
	}

    return 0;
}

int is_ip_exist()
{
	int len = 0, vflag = 0;
	char ipaddr[INET6_ADDRSTRLEN];

	get_net_ip("wlan0", ipaddr, &len, &vflag);

    return vflag;
}

void start_udhcpc()
{
	int len = 0, vflag = 0, times = 0;
	char ipaddr[INET6_ADDRSTRLEN];
	char cmd[256] = {0}, reply[8] = {0};

	/* restart udhcpc */
	system("/etc/wifi/udhcpc_wlan0 start >/dev/null");

	/* check ip exist */
	len = INET6_ADDRSTRLEN;
	usleep(1000000);
	times = 0;
	do {
		usleep(1000000);
		get_net_ip("wlan0", ipaddr, &len, &vflag);
		times++;
    } while ((vflag == 0) && (times < 30));

	WMG_DEBUG("vflag= %d\n", vflag);
	if (vflag != 0) {
		WMG_INFO("get ip addr %s\n", ipaddr);
	} else {
		WMG_ERROR("udhcpc wlan0 timeout\n");
	}
}
