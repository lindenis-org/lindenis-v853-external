#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#define NM_EVENT_STATE_CONNECTING               1
#define NM_EVENT_STATE_CONNECTED                2
#define NM_EVENT_STATE_DISCONNECTED             3
#define NM_EVENT_WIRELESS_INIT_MIRACAST_WLAN0  4
#define NM_EVENT_WIRELESS_INIT_MIRACAST_P2P0   5
#define NM_EVENT_STOP_MIRACAST                   6
#define NM_EVENT_STATE_CONNECTING_START         10

struct NM_EventParamS
{
    char ssid[256];
};

struct Network_LinstenerS
{
    int (*notify)(int /*event*/, void * /*param*/);
};

#ifdef __cplusplus
extern "C"
{
#endif

int NetworkMonitor_SingleInstance(struct Network_LinstenerS *l);

#ifdef __cplusplus
}
#endif


#endif

