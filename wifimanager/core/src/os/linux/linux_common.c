#include <wmg_sta.h>
#include <wifi_log.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <net/if_arp.h>

uint8_t char2uint8(char* trs)
{
	uint8_t ret = 0;
	uint8_t tmp_ret[2] = {0};
	int i = 0;
	for(; i < 2; i++) {
		switch (*(trs + i)) {
			case '0' :
				tmp_ret[i] = 0x0;
				break;
			case '1' :
				tmp_ret[i] = 0x1;
				break;
			case '2' :
				tmp_ret[i] = 0x2;
				break;
			case '3' :
				tmp_ret[i] = 0x3;
				break;
			case '4' :
				tmp_ret[i] = 0x4;
				break;
			case '5' :
				tmp_ret[i] = 0x5;
				break;
			case '6' :
				tmp_ret[i] = 0x6;
				break;
			case '7' :
				tmp_ret[i] = 0x7;
				break;
			case '8' :
				tmp_ret[i] = 0x8;
				break;
			case '9' :
				tmp_ret[i] = 0x9;
				break;
			case 'a' :
				tmp_ret[i] = 0xa;
				break;
			case 'b' :
				tmp_ret[i] = 0xb;
				break;
			case 'c' :
				tmp_ret[i] = 0xc;
				break;
			case 'd' :
				tmp_ret[i] = 0xd;
				break;
			case 'e' :
				tmp_ret[i] = 0xe;
				break;
			case 'f' :
				tmp_ret[i] = 0xf;
		break;
	}
	WMG_DEBUG("change num[%d]: %d\n", i, tmp_ret[i]);
	}
	ret = ((tmp_ret[0] << 4) | tmp_ret[1]);
	return ret;
}

wmg_status_t linux_common_set_mac(const char *ifname, uint8_t *mac_addr)
{
	int sock_mac = -1;
	struct ifreq ifr_mac;
	int ret, i;

	/***** down the network *****/
	WMG_INFO("ioctl %s down\n", ifname);
	sock_mac = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_mac == -1) {
		WMG_ERROR("down network(%s): create mac socket faile\n", ifname);
		return WMG_STATUS_FAIL;
	}
	strncpy(ifr_mac.ifr_name, ifname, (sizeof(ifr_mac.ifr_name) - 1));
	ifr_mac.ifr_flags &= ~IFF_UP;  //ifconfig   donw
	if((ioctl(sock_mac, SIOCSIFFLAGS, &ifr_mac)) < 0) {
		WMG_ERROR("down network(%s): mac ioctl error\n", ifname);
		close(sock_mac);
		return WMG_STATUS_FAIL;
	}
	close(sock_mac);
	sock_mac = -1;
	WMG_DEBUG("wait 3 second\n");
	sleep(3);

	/***** set mac addr to network *****/
	WMG_INFO("ioctl set %s mac: %02x:%02x:%02x:%02x:%02x:%02x\n",ifname, mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
	sock_mac = socket(AF_INET, SOCK_STREAM, 0);
	ifr_mac.ifr_addr.sa_family = ARPHRD_ETHER;
	strcpy(ifr_mac.ifr_name, ifname);

	for(i=0; i<6; i++) {
		ifr_mac.ifr_hwaddr.sa_data[i]= (char)mac_addr[i];
	}
	if((ioctl(sock_mac, SIOCSIFHWADDR, &ifr_mac)) < 0) {
		WMG_ERROR("set network(%s): mac ioctl error\n", ifname);
		close(sock_mac);
		return WMG_STATUS_FAIL;
	}
	close(sock_mac);
	sock_mac = -1;

	/***** up the network *****/
	WMG_INFO("ioctl %s up\n", ifname);
	sock_mac = socket(AF_INET, SOCK_STREAM, 0);
	strncpy(ifr_mac.ifr_name, ifname, (sizeof(ifr_mac.ifr_name) - 1));
	ifr_mac.ifr_flags |= IFF_UP;    // ifconfig   up
	if((ioctl(sock_mac, SIOCSIFFLAGS, &ifr_mac)) < 0) {
		WMG_ERROR("up network(%s): mac ioctl error\n", ifname);
		close(sock_mac);
		return WMG_STATUS_FAIL;
	}

	close(sock_mac);
	return WMG_STATUS_SUCCESS;
}

wmg_status_t linux_common_get_mac(const char *ifname, uint8_t *mac_addr)
{
	int sock_mac = -1;
	char *pch;
	int i;

	struct ifreq ifr_mac;
	char mac_addr_buf[32];

	if(ifname == NULL) {
		WMG_ERROR("ifname is NULL\n");
		return WMG_STATUS_FAIL;
	}

	sock_mac = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_mac == -1)
	{
		WMG_ERROR("create mac socket faile\n");
		return WMG_STATUS_FAIL;
	}

	memset(&ifr_mac,0,sizeof(ifr_mac));
	strncpy(ifr_mac.ifr_name, ifname, (sizeof(ifr_mac.ifr_name) - 1));
	if((ioctl(sock_mac, SIOCGIFHWADDR, &ifr_mac)) < 0)
	{
		WMG_ERROR("mac ioctl(ifname:%s) error\n", ifname);
		close(sock_mac);
		return WMG_STATUS_FAIL;
	}

	sprintf(mac_addr_buf,"%02x:%02x:%02x:%02x:%02x:%02x",
			(unsigned char)ifr_mac.ifr_hwaddr.sa_data[0],
			(unsigned char)ifr_mac.ifr_hwaddr.sa_data[1],
			(unsigned char)ifr_mac.ifr_hwaddr.sa_data[2],
			(unsigned char)ifr_mac.ifr_hwaddr.sa_data[3],
			(unsigned char)ifr_mac.ifr_hwaddr.sa_data[4],
			(unsigned char)ifr_mac.ifr_hwaddr.sa_data[5]);

	pch = strtok(mac_addr_buf, ":");
	for(i = 0;(pch != NULL) && (i < 6); i++){
		mac_addr[i] = char2uint8(pch);
		pch = strtok(NULL, ":");
	}

	WMG_DEBUG("local mac:%s\n",mac_addr_buf);

	close(sock_mac);
	return WMG_STATUS_SUCCESS;
}
