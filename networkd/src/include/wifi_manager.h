/*
 * @Author: Wink
 * @Date: 2020-04-16 17:17:16
 * @LastEditTime: 2020-05-11 16:45:16
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /networkd/src/include/wifi_manager.h
 */

#ifndef NETWORKD_WIFIMANAGER_CONFIG_H
#define NETWORKD_WIFIMANAGER_CONFIG_H

#define MAX_SSID_LEN 128

typedef enum WifiStatus
{
	WM_NETWORK_CONNECTED = 0x1,
	WM_CONNECTING,
	WM_OBTAINING_IP,
	WM_DISCONNECTED,
	WM_CONNECTED,
	WM_WIRELESS_INIT_MIRACAST_WLAN0,
	WM_WIRELESS_INIT_MIRACAST_P2P0,
	WM_STOP_MIRACAST,
} WifiStatus;

typedef struct WifiManager
{
    WifiStatus nStatus;
    int nEventLable;
    char sCurSSID[MAX_SSID_LEN];
} WifiManager;

int wifiRegisterServices(void);
int WifiScanResults(char *results, int *len);
int WifiConnectAp(const char *ssid, const char *password);
int WifiInit(int wifi_on);
int WifiCleanAllNetworkConfig(void);
int WifiSendEvent(WifiStatus status);
int disable_softap(void);
int enable_softap(void);


#endif
