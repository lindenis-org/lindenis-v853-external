#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/ether.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <wifi_log.h>
#include <wmg_monitor.h>
#include <linux_nl.h>
#include <linux_common.h>
#include <netlink/netlink.h>
#include <netlink/genl/family.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

static wmg_monitor_nl_inf_object_t monitor_nl_inf_object;

static int nl80211_init(nl80211_state_t *state)
{
	int err;

	state->nl_sock = nl_socket_alloc();
	if (!state->nl_sock) {
		WMG_ERROR("failed to allocate netlink socket\n");
		return -ENOMEM;
	}

	if (genl_connect(state->nl_sock)) {
		WMG_ERROR("failed to connect to generic netlink\n");
		err = -ENOLINK;
		goto out_handle_destroy;
	}

	nl_socket_set_buffer_size(state->nl_sock, 8192, 8192);

	state->nl80211_id = genl_ctrl_resolve(state->nl_sock, "nl80211");
	if (state->nl80211_id < 0) {
		WMG_ERROR("nl80211 not found\n");
		err = -ENOENT;
		goto out_handle_destroy;
	}

	return 0;

out_handle_destroy:
	nl_socket_free(state->nl_sock);
	return err;
}

static void nl80211_cleanup(nl80211_state_t *state)
{
	nl_socket_free(state->nl_sock);
}

static int read_mon_if_from_conf(void)
{
	FILE *f = NULL;
	char buf[MON_BUF_SIZE] = {0};
	int ret = -1, file_size = 0, len = 0;
	char *pos = NULL, *pre = NULL;

	f = fopen(MON_CONF_PATH, "r");
	if (f != NULL) {
		fseek(f, 0, SEEK_END);
		file_size = (ftell(f) > MON_BUF_SIZE) ? MON_BUF_SIZE : ftell(f);
		fseek(f, 0, SEEK_SET);
		ret = fread(buf, 1, file_size, f);
		if (ret == file_size) {
			WMG_DEBUG("read wifi monitor interface from %s\n", MON_CONF_PATH);
			pos = strstr(buf, "mon_if=");
			if (pos != NULL) {
				pos += 7;
				pre = pos;
				pos = strchr(pos, '\n'); /* exist other parameters ? */
				if (pos != NULL)
					len = pos - pre;
				else
					len = strlen(pre);

				if (len != 0 && len < MON_IF_SIZE) {
					strncpy(monitor_nl_inf_object.monitor_if, pre, len);
					WMG_DEBUG("monitor interface is '%s'\n", monitor_nl_inf_object.monitor_if);
					ret = 0;
				} else {
					WMG_WARNG("monitor interface name maybe error\n");
				}
			}
		}

		fclose(f);
		f = NULL;
	}

	return ret;
}

static void *mon_recv_thread(void *p)
{
	int sockfd, sockopt;
	ssize_t numbytes;
	struct ifreq ifopts;    /* set promiscuous mode */
	static unsigned char buf[MON_BUF_SIZE];
	unsigned char *pkt = NULL;
	wifi_monitor_data_t monitor_data_frame;

	/* Create a raw socket that shall sniff */
	/* Open PF_PACKET socket, listening for EtherType ETHER_TYPE */
	if ((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
		WMG_ERROR("socket - %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Set interface to promiscuous mode - do we need to do this every time? */
	strncpy(ifopts.ifr_name, monitor_nl_inf_object.monitor_if, IFNAMSIZ - 1);
	ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
	ifopts.ifr_flags |= IFF_PROMISC;
	ioctl(sockfd, SIOCSIFFLAGS, &ifopts);

	/* Allow the socket to be reused - incase connection is closed prematurely */
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1) {
		WMG_ERROR("setsockopt - %s\n", strerror(errno));
		close(sockfd);
		exit(EXIT_FAILURE);
	}

	/* Bind to device */
	if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, monitor_nl_inf_object.monitor_if, IFNAMSIZ-1) == -1) {
		WMG_ERROR("setsocket SO_BINDTODEVICE - %s\n", strerror(errno));
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	for (;;) {
		if (monitor_nl_inf_object.monitor_enable == WMG_TRUE) {
			if(monitor_nl_inf_object.nl_data_frame_cb != NULL) {
				numbytes = recvfrom(sockfd, buf, MON_BUF_SIZE, 0, NULL, NULL);
				if (numbytes < 0) {
					continue;
				} else {
					WMG_DUMP("receive data, size = %d\n", numbytes);
					monitor_data_frame.data = buf;
					monitor_data_frame.len = numbytes;
					monitor_data_frame.channel = monitor_nl_inf_object.monitor_nl_channel;
					monitor_data_frame.info = NULL;
					monitor_nl_inf_object.nl_data_frame_cb(&monitor_data_frame);
				}
			}
		}
	}

	//ifopts.ifr_flags &= ~IFF_PROMISC;
	//ioctl(sockfd, SIOCSIFFLAGS, &ifopts);
	//close(sockfd);
	//free(msg.data.frame);
	//pthread_exit(NULL);
	//WMG_DUMP("monitor data handle thread exit\n");
}

static int ieee80211_channel_to_frequency(int chan, enum nl80211_band band)
{
	/* see 802.11 17.3.8.3.2 and Annex J
	 * there are overlapping channel numbers in 5GHz and 2GHz bands */
	if (chan <= 0) {
		return 0; /* not supported */
	}
	switch (band) {
	case NL80211_BAND_2GHZ:
		if (chan == 14)
			return 2484;
		else if (chan < 14)
			return 2407 + chan * 5;
		break;
	case NL80211_BAND_5GHZ:
		if (chan >= 182 && chan <= 196)
			return 4000 + chan * 5;
		else
			return 5000 + chan * 5;
		break;
	case NL80211_BAND_60GHZ:
		if (chan < 5)
			return 56160 + chan * 2160;
		break;
	default:
		;
	}
	return 0; /* not supported */
}

static wmg_status_t linux_monitor_nl_set_channel(uint8_t channel)
{
	wmg_status_t ret;
	struct nl_msg *msg;
	signed long long devidx = 0;
	uint32_t freq;
	enum nl80211_band band;

	if (monitor_nl_inf_object.monitor_nl_init_flag == WMG_FALSE) {
		WMG_ERROR("wifi monitor is not initialized\n");
		return WMG_STATUS_FAIL;
	}

	if (monitor_nl_inf_object.monitor_enable == WMG_FALSE) {
		WMG_ERROR("set monitor channel failed because wifi monitor is disabled\n");
		return WMG_STATUS_FAIL;
	}

	msg = nlmsg_alloc();
	if (msg == NULL) {
		WMG_ERROR("failed to allocate netlink message!\n");
		return WMG_STATUS_FAIL;
	}

	genlmsg_put(msg, 0, 0, monitor_nl_inf_object.nl_state->nl80211_id, 0,
			0, NL80211_CMD_SET_WIPHY, 0);

	devidx = if_nametoindex(monitor_nl_inf_object.monitor_if);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);

	band = (channel <= 14 ? NL80211_BAND_2GHZ : NL80211_BAND_5GHZ);
	freq = ieee80211_channel_to_frequency(channel, band);
	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, freq);
	ret = nl_send_auto_complete(monitor_nl_inf_object.nl_state->nl_sock, msg);
	if (ret < 0) {
		WMG_ERROR("failed to send netlink message for set channel of monitor\n");
		ret = WMG_STATUS_FAIL;
		goto nla_put_failure;
	} else {
		monitor_nl_inf_object.monitor_nl_channel = channel;
		ret = WMG_STATUS_SUCCESS;
	}

	WMG_DEBUG("wifi monitor set channel(%d) success\n", channel);

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}

static int linux_monitor_nl_init(nl_data_frame_cb_t nl_data_frame_cb)
{
	wmg_status_t ret;
	if(monitor_nl_inf_object.monitor_nl_init_flag == WMG_FALSE) {
		WMG_INFO("linux monitor nl init now\n");
		monitor_nl_inf_object.nl_state = (nl80211_state_t *)malloc(sizeof(nl80211_state_t));
		if (monitor_nl_inf_object.nl_state != NULL) {
			ret = nl80211_init(monitor_nl_inf_object.nl_state);
			if (!ret) {
				WMG_DEBUG("nl80211 init success\n");
			} else {
				WMG_ERROR("failed to init nl80211\n");
				return WMG_STATUS_FAIL;
			}
		}
		if(nl_data_frame_cb != NULL){
			monitor_nl_inf_object.nl_data_frame_cb = nl_data_frame_cb;
		}
		monitor_nl_inf_object.monitor_nl_init_flag = WMG_TRUE;
	} else {
		WMG_INFO("linux monitor nl already init\n");
	}
	return WMG_STATUS_SUCCESS;
}

static int linux_monitor_nl_deinit(void)
{
	if(monitor_nl_inf_object.monitor_nl_init_flag == WMG_TRUE){
		WMG_INFO("linux monitor nl deinit now\n");
		nl80211_cleanup(monitor_nl_inf_object.nl_state);
		monitor_nl_inf_object.nl_data_frame_cb = NULL;
		free(monitor_nl_inf_object.nl_state);
		monitor_nl_inf_object.monitor_nl_channel = 255;
		monitor_nl_inf_object.monitor_nl_init_flag = WMG_FALSE;
	} else {
		WMG_INFO("linux monitor nl already deinit\n");
	}
	return 0;
}

static int linux_monitor_nl_enable(uint8_t channel)
{
	wmg_status_t ret;
	struct nl_msg *msg;
	signed long long devidx = 0;
	uint32_t freq;
	enum nl80211_band band;

	ret = os_net_thread_create(&monitor_nl_inf_object.monitor_pid, NULL, mon_recv_thread, NULL, 0, 4096);
	if (ret) {
		WMG_ERROR("failed to create linux nl dnata handle thread\n");
		return WMG_STATUS_FAIL;
	}

	msg = nlmsg_alloc();
	if (msg == NULL) {
		WMG_ERROR("failed to allocate netlink message\n");
		return WMG_STATUS_FAIL;
	}

	genlmsg_put(msg, 0, 0, monitor_nl_inf_object.nl_state->nl80211_id, 0,
			0, NL80211_CMD_SET_INTERFACE, 0);
	devidx = if_nametoindex(monitor_nl_inf_object.monitor_if);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);
	NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_MONITOR);
	ret = nl_send_auto_complete(monitor_nl_inf_object.nl_state->nl_sock, msg);
	if (ret < 0) {
		WMG_ERROR("failed to send netlink message for enable wifi monitor\n");
		ret = WMG_STATUS_FAIL;
		goto nla_put_failure;
	} else {
		ret = WMG_STATUS_SUCCESS;
	}

	genlmsg_put(msg, 0, 0, monitor_nl_inf_object.nl_state->nl80211_id, 0,
			0, NL80211_CMD_SET_WIPHY, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);
	band = (channel <= 14 ? NL80211_BAND_2GHZ : NL80211_BAND_5GHZ);
	freq = ieee80211_channel_to_frequency(channel, band);
	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, freq);
	ret = nl_send_auto_complete(monitor_nl_inf_object.nl_state->nl_sock, msg);
	if (ret < 0) {
		WMG_ERROR("failed to send netlink message for set channel of monitor\n");
		ret = WMG_STATUS_FAIL;
		goto nla_put_failure;
	} else {
		monitor_nl_inf_object.monitor_nl_channel = channel;
		ret = WMG_STATUS_SUCCESS;
	}

	system("ifconfig wlan0 up");
	monitor_nl_inf_object.monitor_enable = WMG_TRUE;

	WMG_DEBUG("wifi monitor enable success\n");

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}

static int linux_monitor_nl_disable()
{
	wmg_status_t ret = WMG_STATUS_SUCCESS;
	struct nl_msg *msg;
	signed long long devidx = 0;

	if (monitor_nl_inf_object.monitor_nl_init_flag == WMG_FALSE) {
		WMG_WARNG("wifi monitor is not initialized\n");
		return WMG_STATUS_UNHANDLED;
	}

	if (monitor_nl_inf_object.monitor_enable == WMG_FALSE) {
		WMG_WARNG("wifi monitor already disabled\n");
		return WMG_STATUS_SUCCESS;
	}

	WMG_DEBUG("wifi monitor disableing...\n");

	system("ifconfig mon0 down");
	msg = nlmsg_alloc();
	if (msg == NULL) {
		WMG_ERROR("failed to allocate netlink message\n");
		return WMG_STATUS_FAIL;
	}

	genlmsg_put(msg, 0, 0, monitor_nl_inf_object.nl_state->nl80211_id, 0,
			0, NL80211_CMD_SET_INTERFACE, 0);

	devidx = if_nametoindex(monitor_nl_inf_object.monitor_if);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);
	NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_STATION);
	ret = nl_send_auto_complete(monitor_nl_inf_object.nl_state->nl_sock, msg);
	if (ret < 0) {
		WMG_ERROR("failed to send netlink message for disable wifi monitor\n");
		ret = WMG_STATUS_FAIL;
		goto nla_put_failure;
	} else {
		ret = WMG_STATUS_SUCCESS;
	}

	if(monitor_nl_inf_object.monitor_pid != -1) {
		os_net_thread_delete(&monitor_nl_inf_object.monitor_pid);
		monitor_nl_inf_object.monitor_pid = -1;
	}
	monitor_nl_inf_object.monitor_enable = WMG_FALSE;
	monitor_nl_inf_object.monitor_nl_channel = 255;

	WMG_DEBUG("wifi monitor disable success\n");

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}

static int linux_connect_to_nl()
{
	wmg_status_t ret;
	ret = read_mon_if_from_conf();
	if (!ret) {
		WMG_DEBUG("get monitor interface success\n");
	} else {
		WMG_WARNG("get monitor interface failed, just use default 'wlan0'\n");
		strncpy(monitor_nl_inf_object.monitor_if, "wlan0", 5);
	}
	WMG_DEBUG("connect nl success\n");
	return WMG_STATUS_SUCCESS;
}

static int linux_set_mac(const char *ifname, uint8_t *mac_addr)
{
	wmg_status_t ret;

	ret = linux_common_set_mac(ifname, mac_addr);
	if (ret) {
		WMG_ERROR("linux failed to set mac\n");
		return WMG_STATUS_FAIL;
	}

	return WMG_STATUS_SUCCESS;
}

static int linux_get_mac(const char *ifname, uint8_t *mac_addr)
{
	wmg_status_t ret;

	ret = linux_common_get_mac(ifname, mac_addr);
	if (ret) {
		WMG_ERROR("linux failed to get mac\n");
		return WMG_STATUS_FAIL;
	}

	return WMG_STATUS_SUCCESS;
}

static int linux_platform_extension(int cmd, void* cmd_para,int *erro_code)
{
	switch (cmd) {
		case NL_CMD_SET_MAC:
			{
				common_mac_para_t * common_mac_para = (common_mac_para_t *)cmd_para;
				return linux_set_mac(common_mac_para->ifname, common_mac_para->mac_addr);
			}
		case NL_CMD_GET_MAC:
			{
				common_mac_para_t * common_mac_para = (common_mac_para_t *)cmd_para;
				return linux_get_mac(common_mac_para->ifname, common_mac_para->mac_addr);
			}
		default:
		return WMG_FALSE;
	}
	return WMG_FALSE;
}

static wmg_monitor_nl_inf_object_t monitor_nl_inf_object = {
	.monitor_nl_init_flag = WMG_FALSE,
	.monitor_enable = WMG_FALSE,
	.monitor_pid = -1,
	.nl_state  = NULL,
	.nl_data_frame_cb = NULL,
	.monitor_nl_channel = 255,

	.monitor_nl_init = linux_monitor_nl_init,
	.monitor_nl_deinit = linux_monitor_nl_deinit,
	.monitor_nl_enable = linux_monitor_nl_enable,
	.monitor_nl_set_channel = linux_monitor_nl_set_channel,
	.monitor_nl_disable = linux_monitor_nl_disable,
	.monitor_nl_connect = linux_connect_to_nl,
	.monitor_platform_extension = linux_platform_extension,
};

wmg_monitor_nl_inf_object_t * monitor_linux_inf_object_register(void)
{
	return &monitor_nl_inf_object;
}
