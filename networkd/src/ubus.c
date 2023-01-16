/*
 * @Author: Wink
 * @Date: 2020-04-14 14:17:26
 * @LastEditTime: 2020-05-13 16:33:10
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /networkd/src/ubus.c
 */
#include "log.h"
#include "ubus.h"

static struct ubus_context *g_pUbusContext= NULL;
//static struct blob_buf g_mBlobBuf;
static const char *g_pSockPath = NULL;
static bool g_bRunning = false;
static ubus_event_handler_t g_mCallback;
static struct ubus_event_handler g_mListener;


int UbusRegisterService(struct ubus_object *method)
{
    return ubus_add_object(g_pUbusContext, method);
}

int UbusRegisterEvent(const char *event)
{
    int ret = 0;

    ret = ubus_register_event_handler(g_pUbusContext, &g_mListener, event);
    if (ret)
    {
        TLOGW("register event failed.\n");
        return -1;
    }

/*
    ret = ubus_register_event_handler(g_pUbusContext, &listener, UBUS_EVENT_WIFI_STATUS);
    if (ret)
    {
        ubus_unregister_event_handler(g_pUbusContext, &listener);
        TLOGW("register event failed.\n");
        return -1;
    }
*/
    return 0;
}

static void ubusAddFd(void)
{
    ubus_add_uloop(g_pUbusContext);
#ifdef FD_CLOEXEC
    fcntl(g_pUbusContext->sock.fd, F_SETFD,
          fcntl(g_pUbusContext->sock.fd, F_GETFD) | FD_CLOEXEC);
#endif
}

static void ubusReconnTimer(struct uloop_timeout *timeout)
{
    static struct uloop_timeout retry =
        {
            .cb = ubusReconnTimer,
        };
    int t = 2;

    if (ubus_reconnect(g_pUbusContext, g_pSockPath) != 0)
    {
        uloop_timeout_set(&retry, t * 1000);
        return;
    }

    ubusAddFd();
}

static void ubusConnectLost(struct ubus_context *ctx)
{
    ubusReconnTimer(NULL);
}

bool UbusIsInited()
{
    return (g_pUbusContext != NULL ? true : false);
}

int UbusInit(const char *path, ubus_event_handler_t ubus_msg_cb)
{
    g_mCallback = ubus_msg_cb;
    memset(&g_mListener, 0, sizeof(g_mListener));
    if (g_mCallback != NULL)
        g_mListener.cb = g_mCallback;

    uloop_init();
    g_pSockPath = path;

    signal(SIGPIPE, SIG_IGN);
    g_pUbusContext = ubus_connect(path);
    if (!g_pUbusContext)
    {
        TLOGW("ubus connect failed\n");
        return -1;
    }

    g_pUbusContext->connection_lost = ubusConnectLost;

    TLOGI("ubus connected, UbusContext is 0x%08x, local_id is 0x%08x\n", g_pUbusContext, g_pUbusContext->local_id);

    ubusAddFd();

    return 0;
}

static void ubusDeInit(void)
{
    if (g_pUbusContext)
        ubus_free(g_pUbusContext);
}

int UBusSendEvent(const char *id, struct blob_attr *data)
{
    char *json = blobmsg_format_json(data, true);
    char *cmd = malloc(strlen(id) + strlen(json) + 64);

	if(g_pUbusContext== NULL || id == NULL){
		TLOGE("UBusSendEvent failed, g_pUbusContext== NULL, id == NULL\n");
		free(json);
	    free(cmd);
		return -1;
	}

    if (data == NULL)
    {
        char null_cmd[128];
        sprintf(null_cmd, "ubus send %s '{}'", id);
        system(null_cmd);
		free(json);
	    free(cmd);
        return 0;
    }

    sprintf(cmd, "ubus send %s '%s'", id, json);
    system(cmd);
    // ubus_send_event(g_pUbusContext, id, data);
    free(json);
    free(cmd);
    return 0;
}

int UBusInvoke(uint32_t obj, const char *method, struct blob_attr *msg, ubus_data_handler_t callback,
    void *priv, int timeout)
{
    return ubus_invoke(g_pUbusContext,obj, method, msg, callback, priv, timeout);
}

int UBusLookupId(const char *path, uint32_t *id)
{
    return ubus_lookup_id(g_pUbusContext, path, id);
}

bool UBusIsRunning()
{
    if (g_bRunning && g_pUbusContext)
        return true;
    return false;
}

int UbusRun()
{
    g_bRunning = true;
    uloop_run();
    g_bRunning = false;

    ubusDeInit();
    return 0;
}
