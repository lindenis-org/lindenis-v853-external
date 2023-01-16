
#define LOG_TAG "miracast"
#include "cdx_log.h"
#include "miracast2.h"

#include <string.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

//#include "wfd_log.h"
#include "WFDMessageQueue.h"
#include "wpa_cli.h"

#include "WFDPlayer2.h"

#define SET_DEVICE_TYPE_CMD "set device_type"
#define SET_DEVICE_NAME_CMD "set device_name"
#define WFD_SUBELEM_SET_CMD "wfd_subelem_set 0"
#define SET_WIFI_DISPLAY_CMD "set wifi_display"
//#define P2P_FIND_CMD "p2p_find"
#define P2P_LISTEN_CMD "p2p_listen 300"
#define P2P_STOP_FIND "p2p_stop_find"
#define QUIT "quit"

#define PROC_NET_ROUTE "/proc/net/route"
#define ARP_NODEFILE "/proc/net/arp"
#define DEFAULT_DEVICE_NAME "wfd-aw"

#define WFD_SETUP  1
#define WFD_PLAY  2
#define WFD_PAUSE 3
#define WFD_TEARDOWN 4

enum WFDEventE
{
	WFD_CB_EVENT_QUIT_FROM_P2P = 0x001,
	WFD_CB_EVENT_SETUP_FROM_P2P = 0x002,
	WFD_CB_EVENT_RESET_FROM_P2P = 0x003,
	WFD_CB_EVENT_RESET_FROM_MANAGER = 0x004,
	WFD_CB_EVENT_GO_NEG_REQUEST,
	WFD_CB_EVENT_INVITATION_ACCEPTED,
};

static char miracast_channel[8];	/* p2p0 or wlan0 */

static struct MiracastCtxS *GetWFDInstance(void);
static void handleException();
static void *P2PThread(void *arg);

class WFDIterTina : public WFDPInterface
{
public:
    WFDIterTina(){}
    ~WFDIterTina(){}

	int handleRTSPStatus(int status);

	void ExceptionHandler(void);

	void *requestSurface(void);

	int EventNotify(int event, void *param);

};

struct WFDMessage // asynchronous
{
    WFDMessage_COMMON_MEMBERS
    uintptr_t params[3]; // TODO
};

struct MiracastCtxS
{
	WFDPlayer2 *mWFDPlayer;
//	int mP2PThreadCreated;
	pthread_t mP2PThreadId;
	int mWFDCBThreadCreated;
	pthread_t mWFDCBThreadId;
	int mWFDCBQuit;
	WFDMessageQueue *mMessageQueue;
	WFDIterTina *wfd_player_iter;

	int isHdcp;
	MIRACAST_STATE state;
	int connecting;
	Miracast_Event_CallBack callback;
	pthread_mutex_t lock;
	char device_name[64];
	char rtsp_url[128];
};

int WFDIterTina::handleRTSPStatus(int status)
{
	struct MiracastCtxS *ctx = GetWFDInstance();
	CDX_LOGD("handleRTSPStatus %d.", status);
	int ret = -1;
	switch (status)
	{
		case WFD_SETUP:
			CDX_LOGD("WFD_SETUP. Time to set surface and start playing.");
			if (ctx->state != START)
			{
				CDX_LOGE("invalid state:'%d'", ctx->state);
			}
			ctx->state = CONNECT;
			ctx->connecting = 0;
			CDX_LOGD("WFD_PLAY. ");
			if (ctx->callback)
			{
				ctx->callback(MIRACAST_CBK_WFD_START_FINISHED, NULL);
			}
			break;
		case WFD_PLAY:
			CDX_LOGD("WFD_PLAY. .");
			break;

		case WFD_TEARDOWN:
			CDX_LOGW("WFD_TEARDOWN");
			break;

		case WFD_PAUSE:
		default:
			CDX_LOGD("unknown WFD_RTSP_Status: %d", status);
			break;
	}
	return ret;
}
void WFDIterTina::ExceptionHandler(void)
{
	CDX_LOGD("ExceptionHandler... ");
	handleException();
	return ;
}

void *WFDIterTina::requestSurface(void)
{
	CDX_LOGE("not call here... ");
	return NULL;
}

int WFDIterTina::EventNotify(int event, void *param)
{
	struct MiracastCtxS *ctx = GetWFDInstance();

	switch (event)
	{
		case MIRACAST_CBK_PLAYER_FIRST_SHOW:
		{
			if (ctx->callback)
			{
				ctx->callback(MIRACAST_CBK_PLAYER_FIRST_SHOW, NULL);
			}
			break;
		}
		default:
		{
			CDX_LOGW("not handle event '%d'... ", event);
			break;
		}
	}
	return 0;
}

static struct MiracastCtxS *wfd_ctx = NULL; /* single instance */

#define P2P_LISTEN_TIMEOUT (300000000LL)

//#define TIME_30MIN (1800000000LL)

static int64_t timestamp_listen = 0;

static int64_t Utils_GetTimeUS()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (int64_t)tv.tv_sec * 1000000ll + tv.tv_usec;
}

static struct MiracastCtxS *GetWFDInstance(void)
{
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

	if (wfd_ctx)
	{
		return wfd_ctx;
	}

	pthread_mutex_lock(&lock);
	if (!wfd_ctx)
	{
		CDX_LOGW("WFD Single Instance Start...");

		wfd_ctx = (struct MiracastCtxS *)malloc(sizeof(struct MiracastCtxS));
		memset(wfd_ctx, 0x00, sizeof(struct MiracastCtxS));
		wfd_ctx->state = INVALID;
		wfd_ctx->connecting = 0;

		pthread_mutex_init(&wfd_ctx->lock, NULL);
		pthread_create(&wfd_ctx->mP2PThreadId, NULL, P2PThread, NULL);

		CDX_LOGW("WFD Single Instance End...");

	}
	pthread_mutex_unlock(&lock);

	return wfd_ctx;
}

int GetPeerDeviceName(const char *info, char *device_name)
{
	char *ssid, *ssid_end, *dn_start;
	char ssid_buf[512] = {0};

	ssid = strstr((char *)info, "ssid=");
	if (!ssid)
	{
		CDX_LOGE("ssid not found '%s'", info);
		return -1;
	}

	ssid_end = strchr(ssid+6, '\"');
	if (!ssid_end) /* ssid is last item */
	{
		CDX_LOGE("invalid ssid '%s'", info);
		return -1;
	}

	/* ssid="xxx-xxx" */
	memcpy(ssid_buf, ssid+6, (ssid_end-ssid)-6);
	CDX_LOGD("ssid_buf '%s'", ssid_buf);

	dn_start = strrchr(ssid_buf, '-');
	if (dn_start)
	{
		strcpy(device_name, dn_start+1);
	}
	else
	{
		strcpy(device_name, ssid_buf);
	}
	CDX_LOGD("device name '%s'", device_name);
	return 0;
}

static int WFDP2PCB(WFDP2PCBEVENT wfdP2PCBEvent, const char *elm1, const int elm2)
{
	struct MiracastCtxS *ctx = GetWFDInstance();
    CDX_LOGD("wfdP2PCB: %d, ", wfdP2PCBEvent);

	if (ctx->state == INVALID)
	{
		CDX_LOGW("state invalid...");
		return 0;
	}

	/* event 0,1 */
	if (wfdP2PCBEvent == WFD_CB_CONNECTED_CLIENT || wfdP2PCBEvent == WFD_CB_CONNECTED_GO)
	{
        if (!elm1 || elm2 == 0) {
            CDX_LOGE("wfdP2PCB: %s, %d, ignore", elm1, elm2);
            return -1;
        }

        WFDMessage msg;
        memset(&msg, 0, sizeof(WFDMessage));
        msg.messageId = WFD_CB_EVENT_SETUP_FROM_P2P;
        msg.params[0] = (uintptr_t)wfdP2PCBEvent;
        msg.params[1] = (uintptr_t)elm1;
        msg.params[2] = (uintptr_t)elm2;
        WFDMessageQueuePostMessage(ctx->mMessageQueue, &msg);
		if (ctx->callback)
		{
			ctx->callback(MIRACAST_CBK_P2P_CONNECTED, NULL);
		}

	}
	else if (wfdP2PCBEvent == WFD_CB_GROUP_STARTED)
	{
		ctx->connecting = 1;
		if (ctx->callback)
		{
			MIRACAST_P2P_CONNECTING_INFO info = {{0}};
//			memcpy(info.sourceName, elm1, 64);
			GetPeerDeviceName(elm1, info.sourceName);
			//ctx->callback(MIRACAST_CBK_P2P_FOUND, (void *)&info);
		}
	}
    else if (wfdP2PCBEvent == WFD_CB_DISCONNECTED)
    {
    	ctx->connecting = 0;
        WFDMessage msg;
        memset(&msg, 0, sizeof(WFDMessage));
        msg.messageId = WFD_CB_EVENT_RESET_FROM_P2P;
        WFDMessageQueuePostMessage(ctx->mMessageQueue, &msg);
    }
	else if(wfdP2PCBEvent == WFD_CB_QUITTED)
	{
		WFDMessage msg;
		memset(&msg, 0, sizeof(WFDMessage));
		msg.messageId = WFD_CB_EVENT_QUIT_FROM_P2P;
		WFDMessageQueuePostMessage(ctx->mMessageQueue, &msg);
	}
	else if(wfdP2PCBEvent == WFD_CB_GO_NEG_REQUEST)
	{
		WFDMessage msg;
		memset(&msg, 0, sizeof(WFDMessage));
		msg.messageId = WFD_CB_EVENT_GO_NEG_REQUEST;
		WFDMessageQueuePostMessage(ctx->mMessageQueue, &msg);
	}
	else if(wfdP2PCBEvent == WFD_CB_INVITATION_ACCEPTED)
	{
		WFDMessage msg;
		memset(&msg, 0, sizeof(WFDMessage));
		msg.messageId = WFD_CB_EVENT_INVITATION_ACCEPTED;
		WFDMessageQueuePostMessage(ctx->mMessageQueue, &msg);
	}
	else if(wfdP2PCBEvent == WFD_CB_DEVICE_FOUND)
	{
		if (ctx->callback)
		{
			ctx->callback(MIRACAST_CBK_P2P_FOUND, NULL);
		}
	}
    else
    {
    }

    return 0;
}

#if 0
static void *P2PThread(void *arg)
{
//	struct MiracastCtxS *ctx = GetWFDInstance();

	static const char *ctrl_iface_dir = "-p/var/run/wpa_supplicant";

    CDX_LOGD("P2P-Thread Start.");

    prctl(PR_SET_NAME, (unsigned long)"P2PThread", 0, 0, 0);

    char wpa_cli_cmd[] = "wpa_cli";
    char wpa_cli_ifname[] = "-ip2p0";
    char wpa_cli_ctrl_iface_dir[] = ""; // TODO
    char *argv[] = {wpa_cli_cmd, wpa_cli_ifname, (char *)ctrl_iface_dir};
    int argc = 3;

    int ret = wpa_cli_main(argc, argv, WFDP2PCB);

    CDX_LOGD("P2P-Thread End! ret %d", ret);

    return NULL;
}
#else
static void *P2PThread(void *arg)
{
    static const char *ctrl_iface_dir = "-p/var/run/wpa_supplicant";
    char wpa_cli_cmd[] = "wpa_cli";
    char wpa_cli_ifname[8] = {0};
    char wpa_cli_ctrl_iface_dir[] = ""; // TODO
    char *argv[3];
    int ret = 0;

    CDX_LOGD("P2P-Thread Start.");
    prctl(PR_SET_NAME, (unsigned long)"P2PThread", 0, 0, 0);

	snprintf(wpa_cli_ifname, sizeof(wpa_cli_ifname), "-i%s", miracast_channel);

	argv[0] = wpa_cli_cmd;
	argv[1] = wpa_cli_ifname;
	argv[2] = (char *)ctrl_iface_dir;

	CDX_LOGD("argv[0]=%s, argv[1]=%s, argv[2]=%s", argv[0], argv[1], argv[2]);

	ret = wpa_cli_main(3, argv, WFDP2PCB);

    CDX_LOGD("P2P-Thread End! ret %d", ret);

    return NULL;
}

#endif

static void handleException()
{
	struct MiracastCtxS *ctx = GetWFDInstance();
    CDX_LOGD("handleException");

    WFDMessage msg;
    memset(&msg, 0, sizeof(WFDMessage));
    msg.messageId = WFD_CB_EVENT_RESET_FROM_MANAGER;
    WFDMessageQueuePostMessage(ctx->mMessageQueue, &msg);
}

int GC_GetPeerIP(char *peerIP)
{
    FILE* fd = NULL;
    char line[512] = {0};
	char tables[4][64] = {{0}};
	int gw_found = 0;

    fd = fopen(PROC_NET_ROUTE, "r");
    if(fd == NULL)
    {
        CDX_LOGD("open %s fail, error: %s", PROC_NET_ROUTE, strerror(errno));
        return -1;
    }

    rewind(fd);

    while (fgets(line, 512, fd) != NULL)
    {
		CDX_LOGD("get:'%s'", line);

		int ret;
		ret = sscanf(line, "%63s %63s %63s",
							tables[0], tables[1], tables[2]);
		if (ret == 3)
		{
			//if ((strncmp(tables[0], "p2p0", 4) == 0) && (strncmp(tables[2], "00000000", 8) != 0))
			if ((strncmp(tables[0], miracast_channel, 4) == 0) && (strncmp(tables[2], "00000000", 8) != 0))
			{
				CDX_LOGD("get %s gateway. '%s'", tables[0],tables[2]);
				gw_found = 1;
				break;
			}
			else
			{
				CDX_LOGD("not %s gw, continue.", tables[0]);
			}
		}
		else
		{
			CDX_LOGW("ret(%d), sscanf found nothing..., continue.", ret);
		}
    }

	if (gw_found)
	{
		char ip_part[4][4] = {{0}};
		memset (ip_part, 0x00, sizeof(ip_part));
		ip_part[0][0] = tables[2][0];
		ip_part[0][1] = tables[2][1];

		ip_part[1][0] = tables[2][2];
		ip_part[1][1] = tables[2][3];

		ip_part[2][0] = tables[2][4];
		ip_part[2][1] = tables[2][5];

		ip_part[3][0] = tables[2][6];
		ip_part[3][1] = tables[2][7];

		char *endptr0, *endptr1, *endptr2, *endptr3;
		sprintf(peerIP, "%ld.%ld.%ld.%ld", strtol(ip_part[3], &endptr3, 16),
										strtol(ip_part[2], &endptr2, 16),
										strtol(ip_part[1], &endptr1, 16),
										strtol(ip_part[0], &endptr0, 16));
	}
	fclose(fd);

    return gw_found ? 0 : -1;
}


char *GO_GetPeerIP(char const* mac)
{
    char* peerIP = (char*)malloc(256);
    memset(peerIP, '\0', 256);
    FILE* fd = NULL;
    char (*tables)[32] = (char (*)[32])malloc(32*6);
    memset(tables, '\0', 32*6);

    fd = fopen(ARP_NODEFILE, "r");
    if(fd == NULL)
    {
        CDX_LOGD("open %s fail, error: %s\n", ARP_NODEFILE, strerror(errno));
		free(peerIP);
        return NULL;
    }

    rewind(fd);

    char* line = (char*)malloc(256);
    memset(line, '\0', 256);
    int length = 0;
    bool ignore = true;
    bool end = false;
    int8_t tmp = '\0';

    while(!end)
    {
        tmp = fgetc(fd);
        if(!feof(fd) && tmp != EOF && tmp != '\n')
        {
            line[length++] = tmp;
        }
        else //get a line
        {
            if(length > 0)
                CDX_LOGD("%s", line);

            if(!ignore && length > 0)
            {
                if(sscanf(line, "%31s %31s %31s %31s %31s %31s", tables[0], tables[1], tables[2], tables[3], tables[4], tables[5]) == 6)
                {
                    if(strstr(tables[5], "p2p"))
                    {
                        /*
                        if((!strncmp(tables[3], mac, 12) && !strcmp(&tables[3][13], &mac[13])) ||
                            (!strncmp(tables[3], mac, 10) && !strcmp(&tables[3][11], &mac[11])) ||
                            (!strncmp(tables[3], mac, 1) && !strncmp(&tables[3][2], &mac[2], 14))) //TODO
                        */
                        {
                            memcpy(peerIP, tables[0], strlen(tables[0]));
                            end = true;
                        }
                    }
                }
            }
            if(feof(fd) || tmp == EOF)
            {
                end = true;
            }
            length = 0;
            memset(line, '\0', 256);
            memset(tables, '\0', 32*6);
            ignore = false;
        }
    }

    if(line)
    {
        free(line);
        line = NULL;
    }
    if(fd)
    {
        fclose(fd);
        fd = NULL;
    }
    if(tables)
    {
        free(tables);
        tables = NULL;
    }

    return peerIP;
}

void P2P_StartListen(char *device_name)
{
	char cmdString[512] = "";

	sprintf(cmdString, "%s", P2P_STOP_FIND);
	CDX_LOGD("cmd: %s", cmdString);
	wpa_cli_cmd_handler(cmdString);

	memset(cmdString, '\0', sizeof(cmdString));
	sprintf(cmdString, "%s %s", SET_DEVICE_NAME_CMD, device_name);
//	sprintf(cmdString, "%s %s", SET_DEVICE_NAME_CMD, "wfd-debug");
	CDX_LOGD("cmd: %s", cmdString);
	wpa_cli_cmd_handler(cmdString);

	memset(cmdString, '\0', sizeof(cmdString));
	sprintf(cmdString, "%s %s", WFD_SUBELEM_SET_CMD, "0006001100000032");
	CDX_LOGD("cmd: %s", cmdString);
	wpa_cli_cmd_handler(cmdString);

	memset(cmdString, '\0', sizeof(cmdString));
	sprintf(cmdString, "%s %d", SET_WIFI_DISPLAY_CMD, 1);
	CDX_LOGD("cmd: %s", cmdString);
	wpa_cli_cmd_handler(cmdString);

	memset(cmdString, '\0', sizeof(cmdString));
	sprintf(cmdString, "%s", P2P_LISTEN_CMD);
	CDX_LOGD("cmd: %s", cmdString);
	wpa_cli_cmd_handler(cmdString);

	timestamp_listen = Utils_GetTimeUS();

	return;
}

void P2P_StopListen()
{
	char cmdString[64] = "";

	memset(cmdString, '\0', sizeof(cmdString));
	sprintf(cmdString, "%s", P2P_STOP_FIND);
	CDX_LOGD("cmd: %s", cmdString);
	wpa_cli_cmd_handler(cmdString);

	return;
}

static void *WFDMainThread(void* arg)
{
	struct MiracastCtxS *ctx = GetWFDInstance();
    CDX_LOGD("WFD-MSG-Thread Start.");

    WFDMessage msg;

    while (!ctx->mWFDCBQuit)
    {
    	if ((ctx->state == START) && (!ctx->connecting))
    	{
    		int64_t timenow = Utils_GetTimeUS();
    		if (timenow - timestamp_listen >= P2P_LISTEN_TIMEOUT
    			|| timestamp_listen - timenow >= P2P_LISTEN_TIMEOUT)
    		{
    			CDX_LOGW("listen timeout(%lld s), re listen now...", P2P_LISTEN_TIMEOUT/1000000);
    			P2P_StartListen(ctx->device_name);
    		}
    	}

        if (WFDMessageQueueTryGetMessage(ctx->mMessageQueue, &msg, 200) < 0)
        {
        	/* 200ms timeout, no any message */
            continue;
        }

        if(msg.messageId == WFD_CB_EVENT_RESET_FROM_MANAGER || msg.messageId == WFD_CB_EVENT_RESET_FROM_P2P)
        {
			CDX_LOGD("WFD-STATE -------> '%d', reset come.", ctx->state);
			pthread_mutex_lock(&ctx->lock);
            if(ctx->mWFDPlayer)
            {
                if(ctx->mWFDPlayer->Stop() != 0)
                {
                    CDX_LOGW("mWFDPlayer->Stop() failed");
                }
                delete ctx->mWFDPlayer;
				ctx->mWFDPlayer = NULL;
            }

//			ctx->state = INIT;

			P2P_StartListen(ctx->device_name);

			ctx->state = START;

			pthread_mutex_unlock(&ctx->lock);
			if (ctx->callback)
			{
				ctx->callback(MIRACAST_CBK_P2P_DISCONNECTED, NULL);
			}
            continue;
        }
        else if(msg.messageId == WFD_CB_EVENT_SETUP_FROM_P2P)
        {
            WFDP2PCBEVENT wfdP2PCBEvent = (WFDP2PCBEVENT)msg.params[0];
            char* peerMac = (char*)msg.params[1];
            int peerCtrlPort = (int)msg.params[2];

			CDX_LOGD("SETUP_FROM_P2P, event(%d), mac(%s), port(%d)",
        			wfdP2PCBEvent, peerMac, peerCtrlPort);

            char *peerIP = NULL;
            char goIP[64] = {0};

            if(wfdP2PCBEvent == WFD_CB_CONNECTED_CLIENT)
            {
            	if (GC_GetPeerIP(goIP) == 0)
            	{
            		CDX_LOGD("get peer ip success.");
            	}
            	else
            	{
            		CDX_LOGW("get peer ip failure, use default.");
            		sprintf(goIP, "192.168.49.1");
            	}
                peerIP = goIP;
            }
            else if (wfdP2PCBEvent == WFD_CB_CONNECTED_GO)
            {
                int i = 0;
                for(; i < 10; i++) // wait 10s max
                {
                    peerIP = GO_GetPeerIP(peerMac);
                    if(!peerIP || strcmp(peerIP, "") == 0)
                    {
                        sleep(1); // 1s
                        continue;
                    }
                    break;
                }
                if(i == 10)
                {
                    CDX_LOGE("WFDManager.GetPeerIP() failed");
                    continue;
                }
            }

            sprintf(ctx->rtsp_url, "rtsp://%s:%d", peerIP, peerCtrlPort);
        	CDX_LOGD("rtsp_url: '%s'", ctx->rtsp_url);

            if (ctx->wfd_player_iter == NULL)
            {
            	ctx->wfd_player_iter = new WFDIterTina();
			}
            ctx->mWFDPlayer = new WFDPlayer2(ctx->wfd_player_iter);
            if (!ctx->mWFDPlayer)
            {
                CDX_LOGE("mWFDPlayer new failed");
                continue;
            }
            if (ctx->mWFDPlayer->Setup(NULL, NULL) != 0)
            {
                CDX_LOGE("mWFDPlayer->Setup() failed");
                continue;
            }

            if (ctx->mWFDPlayer->Start(ctx->rtsp_url) != 0)
            {
                CDX_LOGE("mWFDPlayer->Start() failed");
                continue;
            }

            continue;
        }
        else if(msg.messageId == WFD_CB_EVENT_QUIT_FROM_P2P)
        {
            ctx->mWFDCBQuit = 1;

			pthread_mutex_lock(&ctx->lock);
            if(ctx->mWFDPlayer)
            {
                if(ctx->mWFDPlayer->Stop() != 0)
                {
                    CDX_LOGW("mWFDPlayer->Stop() failed");
                }
                delete ctx->mWFDPlayer;
            }
            ctx->mWFDPlayer = NULL;
			pthread_mutex_unlock(&ctx->lock);

            break; // break the thread
        }
		else if(msg.messageId == WFD_CB_EVENT_GO_NEG_REQUEST)
        {
            if (ctx->callback)
			{
				ctx->callback(MIRACAST_CBK_P2P_GO_NEG_REQUEST, NULL);
			}
            continue;
        }
		else if(msg.messageId == WFD_CB_EVENT_INVITATION_ACCEPTED)
        {
	        CDX_LOGD("%s:%d: \n", __func__, __LINE__);
            if (ctx->callback)
			{
				CDX_LOGD("%s:%d: \n", __func__, __LINE__);
				ctx->callback(MIRACAST_CBK_P2P_INVITATION_ACCEPTED, NULL);
			}
            continue;
        }
        else
        {
            CDX_LOGW("unknow message with id %d, ignore", msg.messageId);
            continue;
        }
    }

    CDX_LOGD("end WFDMainThread, mWFDCBQuit %d", ctx->mWFDCBQuit);

    return NULL;
}

extern "C"
{

int Miracast_Init_ex(int isHdcp, const char *p2p_interface)
{
	int ret = 0;
	struct MiracastCtxS *ctx = NULL;

	memset(miracast_channel, 0, 8);
	strcpy(miracast_channel, p2p_interface);

	CDX_LOGD("Miracast_Init_ex: isHdcp=%d, miracast_channel=%s", isHdcp, miracast_channel);

	/* state check */
	ctx = GetWFDInstance();
	if (ctx->state != INVALID)
	{
		CDX_LOGE("init err, invalid state: '%d'", ctx->state);
		return -1;
	}

	pthread_mutex_lock(&ctx->lock);

	CDX_LOGD("WFD-STATE -------> '%d'", ctx->state);
	CDX_LOGD("init, hdcp:'%d'", isHdcp);

	ctx->mWFDCBThreadCreated = 0;
	ctx->mWFDCBThreadId = -1;
	ctx->mWFDCBQuit = 0;
	ctx->mMessageQueue = WFDMessageQueueCreate(64, "MiracastReceiver");

	strncpy(ctx->device_name, DEFAULT_DEVICE_NAME, 63);

	ctx->state = INIT;
	CDX_LOGD("WFD-STATE -------> '%d'", ctx->state);

	pthread_mutex_unlock(&ctx->lock);

	return ret;
}

int Miracast_Init(int isHdcp)
{
	return Miracast_Init_ex(isHdcp, "p2p0");
}

/* P2P start scan... */
int Miracast_Start(const char* pcDeviceName, Miracast_Event_CallBack pFnEventCb)
{
	int ret = 0;
	struct MiracastCtxS *ctx = GetWFDInstance();

	pthread_mutex_lock(&ctx->lock);
	CDX_LOGD("WFD-STATE -------> '%d'", ctx->state);
	/* state check */
	if (ctx->state != INIT)
	{
		CDX_LOGE("start err, invalid state: '%d'", ctx->state);
		ret = -1;
		goto out;
	}

	CDX_LOGD("start...");
	if (pFnEventCb)
	{
		ctx->callback = pFnEventCb;
	}

	if (pcDeviceName)
	{
		strncpy(ctx->device_name, pcDeviceName, 63);
		CDX_LOGD("pcDeviceName, '%s'", ctx->device_name);
	}

	if (!ctx->mWFDCBThreadCreated)
	{
		if (pthread_create(&ctx->mWFDCBThreadId, NULL, WFDMainThread, NULL) == 0)
			ctx->mWFDCBThreadCreated = 1;
		else
			ctx->mWFDCBThreadCreated = 0;
	}

	CDX_LOGD("wait connection is ready...");
	while ((!wpa_cli_get_ctrl_conn() || !wpa_cli_get_mon_conn()) && ctx->mWFDCBQuit == 0)
	{
		CDX_LOGD("ctrl_conn: '%p', mon_conn: '%p'", wpa_cli_get_ctrl_conn(), wpa_cli_get_mon_conn());
		usleep(200000);
	}
	CDX_LOGD("connection is ready");

	P2P_StartListen(ctx->device_name);

	ctx->state = START;
	CDX_LOGD("WFD-STATE -------> '%d'", ctx->state);

out:
	pthread_mutex_unlock(&ctx->lock);

	return ret;
}

/* stop play & disconnect RTSP stream */
int Miracast_Disconnect(void)
{
	int ret = 0;
	CDX_LOGD("disconnect...");
	struct MiracastCtxS *ctx = GetWFDInstance();

	pthread_mutex_lock(&ctx->lock);
	/* state check */
	if (ctx->state != CONNECT)
	{
		CDX_LOGE("disconnect err, invalid state: '%d'", ctx->state);
		ret = -1;
		goto out;
	}

	if (ctx->mWFDPlayer)
	{
		ctx->mWFDPlayer->Stop();
		delete ctx->mWFDPlayer;
		ctx->mWFDPlayer = NULL;
	}

	ctx->state = START;
	CDX_LOGD("WFD-STATE -------> '%d'", ctx->state);

out:
	pthread_mutex_unlock(&ctx->lock);

	return ret;
}

/* all work process should be stop
  * P2P should be stop.
*/
int Miracast_Stop(void)
{
	CDX_LOGD("stop...");
	struct MiracastCtxS *ctx = GetWFDInstance();
	int ret = 0;

	pthread_mutex_lock(&ctx->lock);
	CDX_LOGD("WFD-STATE -------> '%d'", ctx->state);
    if (ctx->state == CONNECT)
    {
        ret = Miracast_Disconnect();
        if (ret == -1) {
            CDX_LOGE("disconnect error when call Miracast_Stop");
            goto out;
        }
    }
	if (ctx->state != START)
	{
		CDX_LOGE("stop err, invalid state: '%d'", ctx->state);
		ret = -1;
		goto out;
	}

	P2P_StopListen();

	ctx->state = INIT;
	CDX_LOGD("WFD-STATE -------> '%d'", ctx->state);

out:
	pthread_mutex_unlock(&ctx->lock);

	return ret;
}

/* release...  */
int Miracast_DeInit(void)
{
	CDX_LOGD("deinit...");
	struct MiracastCtxS *ctx = GetWFDInstance();

	int ret = 0;

	pthread_mutex_lock(&ctx->lock);
	CDX_LOGD("WFD-STATE -------> '%d'", ctx->state);
	if (ctx->state != INIT)
	{
		CDX_LOGE("deinit err, invalid state: '%d'", ctx->state);
		ret = -1;
		goto out;
	}

	ctx->mWFDCBQuit = 1;

    if (ctx->mWFDCBThreadCreated != 0)
    {
        pthread_join(ctx->mWFDCBThreadId, NULL);
        ctx->mWFDCBThreadCreated = 0;
    }

    if (ctx->mMessageQueue)
    {
        WFDMessageQueueDestroy(ctx->mMessageQueue);
        ctx->mMessageQueue = NULL;
    }

//	memset(ctx, 0x00, sizeof(struct MiracastCtxS));

	ctx->state = INVALID;
	CDX_LOGD("WFD-STATE -------> '%d'", ctx->state);

out:
	pthread_mutex_unlock(&ctx->lock);

	return ret;
}

MIRACAST_STATE Miracast_GetState(void)
{
	struct MiracastCtxS *ctx = GetWFDInstance();

	CDX_LOGD("getstate: '%d'", ctx->state);
	return ctx->state;
}

int Miracast_ModifyName(const char* pcDeviceName)
{
	struct MiracastCtxS *ctx = GetWFDInstance();
	pthread_mutex_lock(&ctx->lock);
	strncpy(ctx->device_name, pcDeviceName, 63);
	CDX_LOGD("modify name, '%s'", ctx->device_name);
	pthread_mutex_unlock(&ctx->lock);

	return 0;
}

int Miracast_SetP2pNetMode(MIRACAST_P2P_NETMODE_E p2pNetMode)
{
	CDX_LOGD("set p2p netmode...");
	struct MiracastCtxS *ctx = GetWFDInstance();
	CDX_LOGD("unsupport now...");
	return 0;
}

int Miracast_SetP2pGOMode(MIRACAST_P2P_GOMODE_E p2pGoMode)
{
	CDX_LOGD("set p2p go mode...");
	struct MiracastCtxS *ctx = GetWFDInstance();
	CDX_LOGD("unsupport now...");
	return 0;
}

void Miracast_SetLowDelay(MIRACAST_LOWDELAY_MODE_E lowDelayMode)
{
	CDX_LOGD("set low delay...");
	struct MiracastCtxS *ctx = GetWFDInstance();
	CDX_LOGD("unsupport now...");
	return;
}

}
