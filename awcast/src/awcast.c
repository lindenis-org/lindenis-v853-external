/*

Copyright (c) 2008-2019 Allwinner Technology Co. Ltd.. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>

#include <tina_log.h>
#include <awcast_main_ui.h>

#include <aw_dlna.h>
#include <miracast2.h>

#include "awcast.h"
#include "network_monitor.h"
#include "wireless_display.h"
#include "net_key.h"
#include "ini_config.h"
#include <signal.h>

#define AWCAST_WIFI_ADDRESS "/sys/class/net/wlan0/address"

struct awcast g_awcast;

static int awcast_get_wifi_addr(char *str, int cnt)
{
    unsigned char file[] = AWCAST_WIFI_ADDRESS;
    unsigned char wifi_addr[32] = {0};
    unsigned char mac[13] = {0};
    int fd = 0;
    int ret = 0, err = 0;

    fd = open(file, O_RDONLY);
    if(fd < 0) {
        TLOGE("err: awcast_get_wifi_addr: could not open %s, %s", file, strerror(errno));
        return -1;
    }

    ret = read(fd, (void *)&wifi_addr, sizeof(wifi_addr));
    if(ret <= 0) {
        TLOGE("awcast_get_wifi_addr: read failed, %s", strerror(errno));
        err = -2;
        goto end;
    }

    sscanf(wifi_addr, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5],
                     &mac[6], &mac[7], &mac[8], &mac[9], &mac[10], &mac[11]);

    //mac have 12 digits
    snprintf(str, (cnt+1), "%s", &mac[12-cnt]);

    TLOGI("mac str is '%s,%s'.", str,  &mac[12-cnt]);

end:
    close(fd);

    return err;
}

static int awcast_get_rand(char *str, int cnt)
{
    int i = 0, n = 0;
    time_t t;
    int temp = 0;

    //srand((unsigned) time(&t));
    temp = rand();
    snprintf(str, (cnt+1), "%02x", temp);

    TLOGI("rand str is '%s'.", str);

    return 0;
}

static void awcast_str_toupper(char *str)
{
    unsigned char len, i;

    len = strlen(str);
    for(i=0; i<len; i++) {
        str[i] = toupper(str[i]);
    }
    }

static int awcast_get_device_name(void)
{
    unsigned char hostapd_file[] = "/etc/hostapd.conf";
    unsigned char hostapd_ssid[] = "ssid";
    char ssid[64]={0};
    unsigned char ext[7] = {0};
    int ret = 0;

    //set default value
    memset(g_awcast.device_name, 0, sizeof(g_awcast.device_name));

    ret = read_string_value(AWCAST_DEVICE_NAME, g_awcast.device_name, AWCAST_CONFIG_FILE);
    if(ret != 0){
        ret = awcast_get_wifi_addr(ext, 4);
        if(ret != 0){
            TLOGE("awcast_get_wifi_addr failed");
            snprintf(g_awcast.device_name, sizeof(g_awcast.device_name), "%s", AWCAST_DEVICE_NAME_DEFAULT);
            //awcast_get_rand(ext, 6);
            return -1;
        }
        awcast_str_toupper(ext);

        TLOGI("awcast_get_device_name: ext='%s'.", ext);

        snprintf(g_awcast.device_name, sizeof(g_awcast.device_name), "%s-%s", AWCAST_DEVICE_NAME_DEFAULT, ext);

        //write device name to awcast.cfg
        write_string_value(AWCAST_DEVICE_NAME, g_awcast.device_name, AWCAST_CONFIG_FILE);
    }

    TLOGI("awcast_get_device_name: device_name is '%s'.", g_awcast.device_name);

    return 0;
}

/* eg: 0b6741ac-7f21-c0c2-2aa0-7ad4f5d40441 */
static int awcast_get_uuid(char *uuid)
{
    int len = 0;
    char *temp_str = 0;

    temp_str = uuid;
    awcast_get_rand(temp_str, 8);
    temp_str += 8;

    strcat(temp_str, "-");
    temp_str += 1;

    awcast_get_rand(temp_str, 4);
    temp_str += 4;

    strcat(temp_str, "-");
    temp_str += 1;

    awcast_get_rand(temp_str, 4);
    temp_str += 4;

    strcat(temp_str, "-");
    temp_str += 1;

    awcast_get_rand(temp_str, 4);
    temp_str += 4;

    strcat(temp_str, "-");
    temp_str += 1;

    awcast_get_rand(temp_str, 12);

    TLOGI("uuid str is '%s'.", uuid);

    return 0;
}

int awcast_parse_config(struct awcast *cast)
{
    unsigned char work_mode[] = "work_mode";
    unsigned char net_key[] = "net_key";
    unsigned char key_name[] = "key_name";
    unsigned char key_code[] = "key_code";
    unsigned char interface[] = "p2p_interface";
    unsigned char uuid[] = "uuid";
    unsigned char share_interface[] = "share_interface";
    int ret = 0;

    ret = read_int_value(work_mode, &cast->work_mode, AWCAST_CONFIG_FILE);
    if(ret != 0){
        cast->work_mode = AWCAST_WORK_MODE_NULL;
        TLOGI("get work_mode failed");
    }else{
        TLOGI("work_mode=%d", cast->work_mode);
    }

    ret = read_int_value(net_key, &cast->net_key, AWCAST_CONFIG_FILE);
    if(ret != 0){
        cast->net_key = 0;
        TLOGI("get net_key failed");
    }else{
        TLOGI("net_key=%d", cast->net_key);
    }


    ret = read_string_value(key_name, cast->key_name, AWCAST_CONFIG_FILE);
    if(ret != 0){
        cast->key_name[0] = 0;
        TLOGI("get key_name failed");
    }else{
        TLOGI("key_name=%s", cast->key_name);
    }

    ret = read_int_value(key_code, &cast->key_code, AWCAST_CONFIG_FILE);
    if(ret != 0){
        cast->key_code = 0;
        TLOGI("get key_code failed");
    }else{
        TLOGI("key_code=%d", cast->key_code);
    }

    ret = read_string_value(AWCAST_DEVICE_NAME, cast->device_name, AWCAST_CONFIG_FILE);
    if(ret != 0){
        snprintf(cast->device_name, sizeof(g_awcast.device_name), "%s", AWCAST_DEVICE_NAME_DEFAULT);
    }else{
        TLOGI("device_name=%s", cast->device_name);
    }

    ret = read_string_value(interface, cast->p2p_interface, AWCAST_CONFIG_FILE);
    if(ret != 0){
        strcpy(cast->p2p_interface, "p2p0");
    }else{
        TLOGI("p2p_interface=%s", cast->p2p_interface);
    }

    ret = read_string_value(uuid, cast->uuid, AWCAST_CONFIG_FILE);
    if(ret != 0){
        awcast_get_uuid(cast->uuid);
        write_string_value(uuid, g_awcast.uuid, AWCAST_CONFIG_FILE);
    }else{
        TLOGI("uuid=%s", cast->uuid);
    }

    ret = read_int_value(share_interface, &cast->share_interface, AWCAST_CONFIG_FILE);
    if(ret != 0){
        cast->share_interface = 0;
        TLOGI("get share_interface failed");
    }else{
        TLOGI("share_interface=%d", cast->share_interface);
    }

    return 0;
}

static int awcast_init(void)
{
    int ret = 0;
    int value = 0;

    memset(&g_awcast, 0, sizeof(struct awcast));

    /* softap is default enable */
    g_awcast.softap_enable = 1;

    awcast_parse_config(&g_awcast);
    g_awcast.wireless_display_state = WIRELESS_DISPLAY_MODE_IDLE;

    TLOGI("device_name=%s", g_awcast.device_name);

    return 0;
}

int show_work_mode_ui(struct awcast *cast)
{
    switch(cast->work_mode){
        case AWCAST_WORK_MODE_ALL:
        case AWCAST_WORK_MODE_DLNA_ONLY:
            MUI_Dlna_BGShow(cast->main_ui);
        break;

        case AWCAST_WORK_MODE_MIRACAST_ONLY:
            MUI_Miracast_BGShow(cast->main_ui);
        break;

        default:
            TLOGI("unkow work mode(%d)", cast->work_mode);
            MUI_Dlna_BGShow(cast->main_ui);
    }

    return 0;
}

static int wireless_init(void)
{
    //channel = MIRACAST_CHANNEL_P2P0;

    wireless_display_init();

    switch(g_awcast.work_mode){
        case AWCAST_WORK_MODE_ALL:
            if(g_awcast.key_code != 0 && g_awcast.key_name[0] != 0){
                net_key_init();
            }

            wd_dlna_init();

            if(g_awcast.net_key == 0){
                switch_start_miracast();
            }
        break;

        case AWCAST_WORK_MODE_DLNA_ONLY:
            /* nothing to do */
            wd_dlna_init();
        break;

        case AWCAST_WORK_MODE_MIRACAST_ONLY:
            switch_start_miracast();
        break;

        default:
            TLOGI("unkow work mode(%d)", g_awcast.work_mode);
    }

    return 0;
}

#if 0
static int __NM_Notify(int event, void *param)
{
    struct NM_EventParamS *nm_param = (struct NM_EventParamS *)param;
    char wifi_info[128] = {0};

    if(g_awcast.ignor_wifi_event){
        TLOGD("ignor wifi event");
        return 0;
    }

    TLOGD(" event '%d'.", event);
    switch (event)
    {
        case NM_EVENT_STATE_CONNECTING:
        {
            TLOGD("'%s' connect...", nm_param->ssid);

            snprintf(wifi_info, sizeof(wifi_info), "WiFi: %s Connecting...", nm_param->ssid);
            if(g_awcast.dlna_playing || g_awcast.miracast_playing){
                TLOGD("dlna or miracast is playing, nothing to do");
                break;
            }
            MUI_Dlna_Update_Info(g_awcast.main_ui, wifi_info);
            break;
        }

        case NM_EVENT_STATE_CONNECTED:
        {
            TLOGD("'%s' connected.111", nm_param->ssid);

            if(nm_param->ssid[0] != '\0'){
                snprintf(wifi_info, sizeof(wifi_info), "WiFi: %s", nm_param->ssid);
            }else{
                snprintf(wifi_info, sizeof(wifi_info), "WiFi: connect successful");
            }

            /* when dlna or miracast is playing, nothing to do */
            if(g_awcast.dlna_playing || g_awcast.miracast_playing){
                TLOGD("dlna or miracast is playing, nothing to do");
                break;
            }

            if(g_awcast.is_net_connected == 0){
                g_awcast.is_net_connected = 1;
                disable_softap();
                if(g_awcast.dlna_working){
                    wd_stop_dlna();
                }
                wd_get_dynamic_ip();
                wd_start_dlna();
            }

            MUI_Dlna_Update_Info(g_awcast.main_ui, wifi_info);
            break;
        }

        case NM_EVENT_STATE_DISCONNECTED:
        {
            TLOGD("'%s' disconnect.", nm_param->ssid);

            snprintf(wifi_info, sizeof(wifi_info), "WiFi: offline");
            if(g_awcast.dlna_playing || g_awcast.miracast_playing){
                TLOGD("dlna or miracast is playing, nothing to do");
                break;
            }
            MUI_Dlna_Update_Info(g_awcast.main_ui, wifi_info);
            //wd_stop_dlna();
            g_awcast.is_net_connected = 0;
            break;
        }
        case NM_EVENT_WIRELESS_INIT_MIRACAST_WLAN0:
        {
            TLOGD("------------NM_EVENT_WIRELESS_INIT_MIRACAST_WLAN0---------.");
            wireless_init();
            break;
        }
        case NM_EVENT_WIRELESS_INIT_MIRACAST_P2P0:
        {
            TLOGD("------------NM_EVENT_WIRELESS_INIT_MIRACAST_P2P0---------.");
            wireless_init();
            break;
        }
        case NM_EVENT_STOP_MIRACAST:
        {
            TLOGD("------------NM_EVENT_STOP_MIRACAST---------.");
            wd_stop_miracast();
            break;
        }

        default:
            TLOGW("unknow event '%d'.", event);
            break;
    }
    return 0;
}

#else

static int __NM_Notify(int event, void *param)
{
    struct NM_EventParamS *nm_param = (struct NM_EventParamS *)param;
    char wifi_info[128] = {0};

    if(g_awcast.ignor_wifi_event){
        TLOGD("ignor wifi event");
        return 0;
    }

    TLOGD(" event '%d'.", event);
    switch (event)
    {
        case NM_EVENT_STATE_CONNECTING_START:
        {
            g_awcast.net_connecting_start = 1;

            /* if p2p interface is wlan0, then stop miracast */
            if(g_awcast.share_interface && !g_awcast.miracast_playing && !g_awcast.net_key){
                wd_stop_miracast();
            }

            goto net_connecting;
        }

        case NM_EVENT_STATE_CONNECTING:
        {
net_connecting:
            TLOGD("'%s' connect...", nm_param->ssid);

            snprintf(wifi_info, sizeof(wifi_info), "WiFi: %s Connecting...", nm_param->ssid);
            if(g_awcast.dlna_playing || g_awcast.miracast_playing){
                TLOGD("dlna or miracast is playing, nothing to do");
                break;
            }
            MUI_Dlna_Update_Info(g_awcast.main_ui, wifi_info);
            break;
        }

        case NM_EVENT_STATE_CONNECTED:
        {
            TLOGD("'%s' connected.", nm_param->ssid);

            if(nm_param->ssid[0] != '\0'){
                snprintf(wifi_info, sizeof(wifi_info), "WiFi: %s", nm_param->ssid);
            }else{
                snprintf(wifi_info, sizeof(wifi_info), "WiFi: connect successful");
            }

            if(g_awcast.miracast_playing || g_awcast.airplay_playing
                || (g_awcast.net_key && g_awcast.miracast_working)
                || (g_awcast.net_key && g_awcast.airplay_working)){
                TLOGD("miracast or airplay is playing, nothing to do");
                break;
            }

            if(g_awcast.is_net_connected == 0){
                g_awcast.is_net_connected = 1;
                disable_softap();
                if(g_awcast.dlna_working){
                    wd_stop_dlna();
                }
                wd_get_dynamic_ip();
                wd_start_dlna();
            }else{
                if(g_awcast.dlna_playing || g_awcast.net_connecting_start){
                    wd_stop_dlna();
                    wd_get_dynamic_ip();
                    wd_start_dlna();
                }
            }

            if(g_awcast.share_interface && !g_awcast.miracast_playing && !g_awcast.net_key){
                wd_start_miracast();
            }

            MUI_Dlna_Update_Info(g_awcast.main_ui, wifi_info);
            g_awcast.net_connecting_start = 0;
            break;
        }

        case NM_EVENT_STATE_DISCONNECTED:
        {
            TLOGD("'%s' disconnect.", nm_param->ssid);

            if(nm_param->ssid[0] != '\0'){
                snprintf(wifi_info, sizeof(wifi_info), "WiFi: %s", nm_param->ssid);
            }else{
                snprintf(wifi_info, sizeof(wifi_info), "WiFi: offline");
            }

            if(g_awcast.miracast_playing || g_awcast.airplay_playing
                || (g_awcast.net_key && g_awcast.miracast_working)
                || (g_awcast.net_key && g_awcast.airplay_working)){
                TLOGD("miracast or airplay is playing, nothing to do");
                break;
            }

            if(g_awcast.share_interface && !g_awcast.miracast_playing && !g_awcast.net_key){
                wd_start_miracast();
            }

            MUI_Dlna_Update_Info(g_awcast.main_ui, wifi_info);
            wd_stop_dlna();
            g_awcast.is_net_connected = 0;
            break;
        }

        case NM_EVENT_WIRELESS_INIT_MIRACAST_WLAN0:
        {
            TLOGD("------------NM_EVENT_WIRELESS_INIT_MIRACAST_WLAN0---------.");
            wireless_init();
            break;
        }
        case NM_EVENT_WIRELESS_INIT_MIRACAST_P2P0:
        {
            TLOGD("------------NM_EVENT_WIRELESS_INIT_MIRACAST_P2P0---------.");
            wireless_init();
            break;
        }
        case NM_EVENT_STOP_MIRACAST:
        {
            TLOGD("------------NM_EVENT_STOP_MIRACAST---------.");
            wd_stop_miracast();
            break;
        }

        default:
            TLOGW("unknow event '%d'.", event);
            break;
    }
    return 0;
}

#endif

static struct Network_LinstenerS nm_linstener =
{
    .notify = __NM_Notify
};

static int wait_wifi_driver_ready(struct awcast *cast)
{
    unsigned char file[] = AWCAST_WIFI_ADDRESS;
    int fd = 0;
    char command_ifconfig[] = "ifconfig wlan0 |grep HWaddr |head -n 1 | grep -o  \"[a-f0-9A-F]\\([a-f0-9A-F]\\:[a-f0-9A-F]\\)\\{5\\}[a-f0-9A-F]\"";
    FILE* fp=NULL;
    int time = 20;
    char buf[1024] = {0};
/*
    fd = access(file, F_OK);
    if(fd < 0) {
        TLOGE("err: %s is not exist, error=%s", file, strerror(errno));
        return -1;
    }
*/
    while(time){
        fd = access(file, F_OK);
        if(fd < 0) {
            TLOGE("err: %s is not exist, time=%d,  try again", file, time);
            goto wait;
        }

        TLOGD("command_ifconfig=%s, time=%d",command_ifconfig, time);

        fp = popen(command_ifconfig, "r");
        if(fp == NULL){
            TLOGE("popen command_ifconfig failed, time=%d,  try again", time);
            goto wait;
        }

        if(fgets(buf, sizeof(buf), fp) != NULL){
            TLOGD("buf=%s, time=%d",buf, time);
            return 0;
        }
wait:
        usleep(500*1000);
        time--;
    }

    MUI_Dlna_Update_Info(g_awcast.main_ui, "wifi: wifi device lost");

    return 1;
}

static int get_ssid(void)
{
    FILE* fp=NULL;
    char command[] = "iw dev wlan0 link";
    char buf[1024] = {0};
    char *q = NULL;
    char vlaue[256] = {0};
    struct NM_EventParamS nm_param;

    fp = popen(command, "r");
    if(fp == NULL){
        TLOGE("%s:%d: popen failed");
        return 0;
    }

    while(fgets(buf, sizeof(buf), fp) != NULL){
        q = strstr(buf, "SSID:");
        if(q != NULL){
            sscanf(buf, "%*s%255s", vlaue);
            if(vlaue[0] != '\0'){
                memset(&nm_param, 0, sizeof(struct NM_EventParamS));
                strncpy(nm_param.ssid, vlaue, 256);

                __NM_Notify(NM_EVENT_STATE_CONNECTED, &nm_param);

                break;
            }
        }
    }

    return 0;
}


int MiniGUIMain (int argc, const char* argv[])
{
    /* init awcast parameters */
    awcast_init();

    /* Main UI */
    g_awcast.main_ui = MUI_Instance();
    if(g_awcast.main_ui == NULL){
        TLOGE("err: MiniGUIMain, MUI_Instance failed\n");
        return -1;
    }

    show_work_mode_ui(&g_awcast);

    /* network init */
    if(wait_wifi_driver_ready(&g_awcast) != 0){
        TLOGE("err: wifi driver is not ready\n");
        goto ui_thread;
    }

    if(strcmp(g_awcast.device_name, AWCAST_DEVICE_NAME_DEFAULT) == 0){
        awcast_get_device_name();
        show_work_mode_ui(&g_awcast);
    }

    wireless_init();

    /* Network monitor */
    if(g_awcast.work_mode != WIRELESS_DISPLAY_MODE_MIRACAST){
        NetworkMonitor_SingleInstance(&nm_linstener);
    }

    /* when awcast restart, check if wifi is connected */
    get_ssid();

ui_thread:
    TLOGD("Enter UI thread\n");
    while (1)
    {
        sleep(1);
    }

    return 0;
}

