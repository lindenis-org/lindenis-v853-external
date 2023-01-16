/*
 * @Author: your name
 * @Date: 2020-05-09 16:44:21
 * @LastEditTime: 2020-06-24 16:44:34
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /networkd/src/client.c
 */

#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/ioctl.h>
#include <linux/wireless.h>
#include <linux/if_ether.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <netlink/socket.h>

#include <linux/nl80211.h>
#include "networkd_api.h"
#include "log.h"

//#define WIFI_SSID_DEBUG

#define WIFI_SCAN_LIST_LEN      4096
#define WIFI_SSID_LEN           512
static char g_wifi_ssid[WIFI_SCAN_LIST_LEN];

char *optstring = "s:p:cdl";
static struct option long_options[] = {
    {"ssid", required_argument, NULL, 's'},
    {"password", required_argument, NULL, 'p'},
    {"connect_ap", no_argument, NULL, 'c'},
    {"daemon", no_argument, NULL, 'd'},
    {"scanlist", no_argument, NULL, 'l'},
    {0, 0, 0, 0}
};

void networkdCallback(NetworkType type, char *event, void *object)
{
    TLOGD("Type = %d, Event = %s, Object = %s\n", type, event, object);
}

struct trigger_results {
    int done;
    int aborted;
};

struct handler_args {
    const char *group;
    int id;
};

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg) {
    // Callback for errors.
    TLOGI("error_handler() called.\n");
    int *ret = arg;
    *ret = err->error;
    return NL_STOP;
}


static int finish_handler(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_FINISH.
    int *ret = arg;
    *ret = 0;
    return NL_SKIP;
}


static int ack_handler(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_ACK.
    int *ret = arg;
    *ret = 0;
    return NL_STOP;
}


static int no_seq_check(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_SEQ_CHECK.
    return NL_OK;
}


void mac_addr_n2a(char *mac_addr, unsigned char *arg) {
    // From http://git.kernel.org/cgit/linux/kernel/git/jberg/iw.git/tree/util.c.
    int i, l;

    l = 0;
    for (i = 0; i < 6; i++) {
        if (i == 0) {
            sprintf(mac_addr+l, "%02x", arg[i]);
            l += 2;
        } else {
            sprintf(mac_addr+l, ":%02x", arg[i]);
            l += 3;
        }
    }
}

static int parse_ssid(unsigned char *ie, int ielen, char *ssid)
{
    uint8_t len;
    uint8_t *data;
    int i;
	char *temp = ssid;

    while (ielen >= 2 && ielen >= ie[1]) {
        if (ie[0] == 0 && ie[1] >= 0 && ie[1] <= 32) {
            len = ie[1];
            data = ie + 2;

            for (i = 0; i < len; i++) {
                if(strlen(temp) >= (WIFI_SSID_LEN-1)){
                    break;
                }

                if (isprint(data[i]) && data[i] != ' ' && data[i] != '\\') {
                    sprintf(ssid++, "%c", data[i]);
                } else if (data[i] == ' ' && (i != 0 && i != len -1)) {
                    sprintf(ssid++, " ");
                } else {
                    sprintf(ssid, "\\x%.2x", data[i]);
                    ssid+=4;
                }
                //TLOGI("ssid=%s, ssid_len=%d", temp, strlen(temp));
            }

            break;
        }

        ielen -= ie[1] + 2;
        ie += ie[1] + 2;
    }

	return 0;
}

void print_ssid(unsigned char *ie, int ielen) {
    uint8_t len;
    uint8_t *data;
    int i;

    while (ielen >= 2 && ielen >= ie[1]) {
        if (ie[0] == 0 && ie[1] >= 0 && ie[1] <= 32) {
            len = ie[1];
            data = ie + 2;
            for (i = 0; i < len; i++) {
                if (isprint(data[i]) && data[i] != ' ' && data[i] != '\\') {
                    TLOGI("%c", data[i]);
                } else if (data[i] == ' ' && (i != 0 && i != len -1)) {
                    TLOGI(" ");
                } else {
                    TLOGI("\\x%.2x", data[i]);
                }
            }
            break;
        }
        ielen -= ie[1] + 2;
        ie += ie[1] + 2;
    }
}


static int callback_trigger(struct nl_msg *msg, void *arg) {
    // Called by the kernel when the scan is done or has been aborted.
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct trigger_results *results = arg;

    //TLOGI("Got something.\n");
    //TLOGI("%d\n", arg);
    //nl_msg_dump(msg, stdout);

    if (gnlh->cmd == NL80211_CMD_SCAN_ABORTED) {
        TLOGI("Got NL80211_CMD_SCAN_ABORTED.\n");
        results->done = 1;
        results->aborted = 1;
    } else if (gnlh->cmd == NL80211_CMD_NEW_SCAN_RESULTS) {
        TLOGI("Got NL80211_CMD_NEW_SCAN_RESULTS.\n");
        results->done = 1;
        results->aborted = 0;
    }  // else probably an uninteresting multicast message.

    return NL_SKIP;
}

static int callback_dump(struct nl_msg *msg, void *arg) {
    // Called by the kernel with a dump of the successful scan's data. Called for each SSID.
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    char mac_addr[20] = {0};
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct nlattr *bss[NL80211_BSS_MAX + 1];
    static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
		[NL80211_BSS_TSF] = { .type = NLA_U64 },
		[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_BSS_BSSID] = { },
		[NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
		[NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
		[NL80211_BSS_INFORMATION_ELEMENTS] = { },
		[NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
		[NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
		[NL80211_BSS_STATUS] = { .type = NLA_U32 },
		[NL80211_BSS_SEEN_MS_AGO] = { .type = NLA_U32 },
		[NL80211_BSS_BEACON_IES] = { },
	};
	char flags[] = "[WPA2-PSK-CCMP][WPS][ESS]";
	char wifi_info[1024] = {0};
	int len = strlen(g_wifi_ssid);

    // Parse and error check.
    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);
    if (!tb[NL80211_ATTR_BSS]) {
        TLOGI("bss info missing!\n");
        return NL_SKIP;
    }
    if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy)) {
        TLOGI("failed to parse nested attributes!\n");
        return NL_SKIP;
    }
    if (!bss[NL80211_BSS_BSSID]) return NL_SKIP;
    if (!bss[NL80211_BSS_INFORMATION_ELEMENTS]) return NL_SKIP;

	mac_addr_n2a(mac_addr, nla_data(bss[NL80211_BSS_BSSID]));
	//strcat(wifi_info, mac_addr);
	//strcat(wifi_info, "	");
	sprintf(wifi_info, "%s\t", mac_addr);
	//TLOGI("mac_addr=%s", mac_addr);

	if (bss[NL80211_BSS_FREQUENCY]) {
		int freq = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
		char value[128] = {0};

		sprintf(value, "%d\t", freq);
		strcat(wifi_info, value);
		//TLOGI("freq=%s", value);
	}

	if (bss[NL80211_BSS_SIGNAL_MBM]) {
		int s = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
		char value[128] = {0};

		sprintf(value, "%d\t", s/100);
		strcat(wifi_info, value);
		//TLOGI("signal=%s", value);

		//flags
		strcat(wifi_info, flags);
		strcat(wifi_info, "\t");
	}

	if (bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
		char ssid[WIFI_SSID_LEN] = {0};

		parse_ssid(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]), nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]), ssid);
		//print_ssid(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]), nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]));

		//if(ssid[0] != '\0'){
			//TLOGI("ssid=%s", ssid);
			//sprintf(value, "%s\n", ssid);
			//strcat(wifi_info, value);
			strcat(wifi_info, ssid);
			strcat(wifi_info, "\n");
			//TLOGI("ssid=%s", ssid);
			if((len + strlen(wifi_info)) < WIFI_SCAN_LIST_LEN){
				strcat(g_wifi_ssid, wifi_info);
				len = strlen(g_wifi_ssid);
			}
		//}
	}

#ifdef WIFI_SSID_DEBUG
	TLOGI("%s", wifi_info);
#endif

    return NL_SKIP;
}

static int family_handler(struct nl_msg *msg, void *arg)
{
	struct handler_args *grp = arg;
	struct nlattr *tb[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *mcgrp;
	int rem_mcgrp;

	nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[CTRL_ATTR_MCAST_GROUPS])
		return NL_SKIP;

	nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], rem_mcgrp) {
		struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];

		nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX,
			  nla_data(mcgrp), nla_len(mcgrp), NULL);

		if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME] ||
		    !tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID])
			continue;
		if (strncmp(nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]),
			    grp->group, nla_len(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME])))
			continue;
		grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
		break;
	}

	return NL_SKIP;
}

static int nl_get_multicast_id(struct nl_sock *sock, const char *family, const char *group)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	int ret, ctrlid;
	struct handler_args grp = {
		.group = group,
		.id = -ENOENT,
	};

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		ret = -ENOMEM;
		goto out_fail_cb;
	}

	ctrlid = genl_ctrl_resolve(sock, "nlctrl");

	genlmsg_put(msg, 0, 0, ctrlid, 0,
		    0, CTRL_CMD_GETFAMILY, 0);

	ret = -ENOBUFS;
	NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

	ret = nl_send_auto_complete(sock, msg);
	if (ret < 0)
		goto out;

	ret = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &ret);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &ret);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, family_handler, &grp);

	while (ret > 0)
		nl_recvmsgs(sock, cb);

	if (ret == 0)
		ret = grp.id;
 nla_put_failure:
 out:
	nl_cb_put(cb);
 out_fail_cb:
	nlmsg_free(msg);
	return ret;
}


int do_scan_trigger(struct nl_sock *socket, int if_index, int driver_id) {
    // Starts the scan and waits for it to finish. Does not return until the scan is done or has been aborted.
    struct trigger_results results = { .done = 0, .aborted = 0 };
    struct nl_msg *msg;
    struct nl_cb *cb;
    struct nl_msg *ssids_to_scan;
    int err;
    int ret;
    //int mcid = genl_ctrl_resolve_grp(socket, "nl80211", "scan");
    int mcid = nl_get_multicast_id(socket, "nl80211", "scan");
    nl_socket_add_membership(socket, mcid);  // Without this, callback_trigger() won't be called.

    // Allocate the messages and callback handler.
    msg = nlmsg_alloc();
    if (!msg) {
        TLOGE("ERROR: Failed to allocate netlink message for msg.\n");
        return -ENOMEM;
    }
    ssids_to_scan = nlmsg_alloc();
    if (!ssids_to_scan) {
        TLOGE("ERROR: Failed to allocate netlink message for ssids_to_scan.\n");
        nlmsg_free(msg);
        return -ENOMEM;
    }
    cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb) {
        TLOGE("ERROR: Failed to allocate netlink callbacks.\n");
        nlmsg_free(msg);
        nlmsg_free(ssids_to_scan);
        return -ENOMEM;
    }

    // Setup the messages and callback handler.
    genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_TRIGGER_SCAN, 0);  // Setup which command to run.
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);  // Add message attribute, which interface to use.
    nla_put(ssids_to_scan, 1, 0, "");  // Scan all SSIDs.
    nla_put_nested(msg, NL80211_ATTR_SCAN_SSIDS, ssids_to_scan);  // Add message attribute, which SSIDs to scan for.
    nlmsg_free(ssids_to_scan);  // Copied to `msg` above, no longer need this.
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, callback_trigger, &results);  // Add the callback.
    nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
    nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);  // No sequence checking for multicast messages.

    // Send NL80211_CMD_TRIGGER_SCAN to start the scan. The kernel may reply with NL80211_CMD_NEW_SCAN_RESULTS on
    // success or NL80211_CMD_SCAN_ABORTED if another scan was started by another process.
    err = 1;
    ret = nl_send_auto_complete(socket, msg);  // Send the message.
    TLOGI("NL80211_CMD_TRIGGER_SCAN sent %d bytes to the kernel.\n", ret);
    TLOGI("Waiting for scan to complete...\n");
    while (err > 0) ret = nl_recvmsgs(socket, cb);  // First wait for ack_handler(). This helps with basic errors.
    if (err < 0) {
        TLOGE("WARNING: err has a value of %d.\n", err);
    }
    if (ret < 0) {
        TLOGE("ERROR: nl_recvmsgs() returned %d (%s).\n", ret, nl_geterror(-ret));
        return ret;
    }
    while (!results.done) nl_recvmsgs(socket, cb);  // Now wait until the scan is done or aborted.
    if (results.aborted) {
        TLOGE("ERROR: Kernel aborted scan.\n");
        return 1;
    }
    TLOGI("Scan is done.\n");

    // Cleanup.
    nlmsg_free(msg);
    nl_cb_put(cb);
    nl_socket_drop_membership(socket, mcid);  // No longer need this.
    return 0;
}


int get_wifi_scan_list(char *result, int *length)
{
    int if_index = if_nametoindex("wlan0"); // Use this wireless interface for scanning.

    // Open socket to kernel.
    struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
    genl_connect(socket);  // Create file descriptor and bind socket.
    int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.

    // Issue NL80211_CMD_TRIGGER_SCAN to the kernel and wait for it to finish.
    int err = do_scan_trigger(socket, if_index, driver_id);
    if (err != 0) {
        TLOGE("do_scan_trigger() failed with %d.\n", err);
        return err;
    }

    // Now get info for all SSIDs detected.
    struct nl_msg *msg = nlmsg_alloc();  // Allocate a message.
    genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0);  // Setup which command to run.
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);  // Add message attribute, which interface to use.
    nl_socket_modify_cb(socket, NL_CB_VALID, NL_CB_CUSTOM, callback_dump, NULL);  // Add the callback.
    int ret = nl_send_auto_complete(socket, msg);  // Send the message.
    TLOGI("NL80211_CMD_GET_SCAN sent %d bytes to the kernel.\n", ret);
    ret = nl_recvmsgs_default(socket);  // Retrieve the kernel's answer. callback_dump() prints SSIDs to stdout.
    nlmsg_free(msg);
    if (ret < 0) {
        TLOGE("ERROR: nl_recvmsgs_default() returned %d (%s).\n", ret, nl_geterror(-ret));
        return ret;
    }

	*length = strlen(result);

#ifdef WIFI_SSID_DEBUG
	TLOGI("Scan Result(%d):\n%s\n", *length, result);
#endif

    return 0;
}

int main(int argc, char **argv)
{
    int option_index = 0;
    int opt = 0;
    int ret = 0;
    int is_connect_ap = 0;
    int is_daemon = 0;
    int is_scanlist = 0;
    char *wifi_ssid = NULL;
    char *wifi_password = NULL;

	memset(g_wifi_ssid, 0, sizeof(g_wifi_ssid));

    while ((opt = getopt_long(argc, argv, optstring, long_options, &option_index)) != -1)
    {
/*
        printf("opt = %c\n", opt);
        printf("optarg = %s\n", optarg);
        printf("optind = %d\n", optind);
        printf("argv[optind - 1] = %s\n", argv[optind - 1]);
        printf("option_index = %d\n", option_index);
*/
		switch (opt)
		{
			case 's':
				wifi_ssid = argv[optind - 1];
			break;

			case 'p':
				wifi_password = argv[optind - 1];
			break;

			case 'c':
				is_connect_ap = 1;
			break;

			case 'd':
				is_daemon = 1;
			break;

			case 'l':
				is_scanlist = 1;
			break;
		}

    }

    NetdApiInit(networkdCallback);

    if (is_connect_ap) {
        TLOGI("ssid=%s, wifi_password=%s\n", wifi_ssid, wifi_password);
        NetdApiStaConnect(wifi_ssid, wifi_password);
	}

    if (is_scanlist)
    {
        char *result = g_wifi_ssid;
        int length = 0;

        ret = get_wifi_scan_list(result, &length);
        if (ret == 0 && length != 0){
            LOG_STDOUT("Scan Result(%d):\n%s\n", length, result);
        }
    }

    if (is_daemon)
    {
        NetdApiRun();
    }
    return 0;
}
