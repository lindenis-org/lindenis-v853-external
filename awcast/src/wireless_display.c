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
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <awcast_main_ui.h>

#include <aw_dlna.h>
#include <miracast2.h>

#include <tina_log.h>
#include "awcast.h"
#include "wireless_display.h"

extern struct awcast g_awcast;
static int dlna_add_stop_time = 0;

static void wakeup_wireless_display_thread(void);
static int prepare_for_miracast(void);

//#ifdef CONFIG_AWCAST_DLNA_ENABLE
#ifdef  AWCAST_DLNA
int wd_start_dlna(void)
{
    int ret = 0;

    if(g_awcast.dlna_working == 0){
        if(g_awcast.awd){
            TLOGD("AWD_Start......\n");

            g_awcast.dlna_working = 1;
            ret = AWD_Start(g_awcast.awd);
            if(ret != 0){
                TLOGE("AWD_Start failed\n");
                g_awcast.dlna_working = 0;
                return -1;
            }
            TLOGD("AWD_Start end......\n");
        }
    }else{
        TLOGE("awcast is already in dlna mode\n");
        //return -1;
    }

    return 0;
}

int wd_stop_dlna(void)
{
    int ret = 0;

    if(g_awcast.dlna_working){
        if(g_awcast.awd){
            TLOGD("AWD_Stop......\n");
            ret = AWD_Stop(g_awcast.awd);
            if(ret != 0){
                TLOGE("AWD_Stop failed\n");
                return -1;
            }
            TLOGD("AWD_Stop end\n");
        }

        g_awcast.dlna_working = 0;
    }else{
        TLOGE("awcast is not in dlna mode\n");
        return -1;
    }

    return 0;
}

static int __DLNA_Notify(int event, void *param)
{
    TLOGD("DLNA event: '%d'", event);

    switch (event)
    {
        case AWD_EVENT_ENTRY:
            TLOGD("------AWD_EVENT_ENTRY------");
            g_awcast.dlna_quit_to_start_miracast = 0;
            g_awcast.dlna_playing = 1;
            dlna_add_stop_time = 0;
            if(g_awcast.work_mode == AWCAST_WORK_MODE_ALL && g_awcast.net_key == 0){
                wd_stop_miracast();
            }
        break;

        case AWD_EVENT_QUIT:
            TLOGD("------AWD_EVENT_QUIT------");
            g_awcast.dlna_playing = 0;
            if(g_awcast.dlna_quit_to_start_miracast){
                TLOGD("------dlna quit message is dealing, nothing to do------");
                dlna_add_stop_time = 1;
                return 0;
            }
            g_awcast.dlna_quit_to_start_miracast = 1;
            wakeup_wireless_display_thread();
        break;

        default:
            TLOGW("__DLNA_Notify: unknow event: '%d'", event);
            break;
        }
        return 0;
}

static struct AWD_LinstenerS dlna_linstener =
{
    .notify = __DLNA_Notify
};

int wd_dlna_init(void)
{
    TLOGI("%s:%d: -------------------\n", __func__, __LINE__);

    if(g_awcast.awd == NULL){
        g_awcast.awd = AWD_Instance(g_awcast.device_name, g_awcast.uuid, &dlna_linstener);
        if(g_awcast.awd == NULL){
            TLOGE("AWD_Instance failed\n");
            return -1;
        }
    }else{
        TLOGE("dlna is already init\n");
    }

    return 0;
}

int wd_dlna_exit(void)
{
    int ret = 0;

    TLOGI("%s:%d: -------------------\n", __func__, __LINE__);

    if(g_awcast.awd){
        ret = AWD_Destroy(g_awcast.awd);
        if(ret != 0){
            TLOGE("AWD_Destroy failed\n");
            return -1;
        }
        g_awcast.awd = NULL;
    }

    return 0;
}

#else

int wd_start_dlna(void)
{
    return 0;
}

int wd_stop_dlna(void)
{
    return 0;
}

int wd_dlna_init(void)
{
    return 0;
}

int wd_dlna_exit(void)
{
    return 0;
}


#endif

//#ifdef CONFIG_AWCAST_MIRACAST_ENABLE
#ifdef AWCAST_MIRACAST
static int __Miracast_Event_CB(MIRACAST_EVENT_CALLBACK_E enEvent, void* pvPrivateData)
{
    TLOGD("event '%d'", enEvent);
    switch (enEvent)
    {
        case MIRACAST_CBK_P2P_FOUND:
        {
            TLOGD("MIRACAST_CBK_P2P_FOUND...");
            prepare_for_miracast();
            break;
        }
        case MIRACAST_CBK_P2P_INVITATION_ACCEPTED:
        {
            TLOGD("MIRACAST_CBK_P2P_INVITATION_ACCEPTED...");
            break;
        }
        case MIRACAST_CBK_P2P_CONNECTED:
        {
            TLOGD("Connected...");
            g_awcast.ignor_wifi_event = 1;
            g_awcast.miracast_playing = 1;
            MUI_Miracast_Connected(g_awcast.main_ui);
            if(g_awcast.work_mode == AWCAST_WORK_MODE_ALL && g_awcast.net_key == 0){
                g_awcast.miracast_connected_to_stop_dlna = 1;
                wakeup_wireless_display_thread();
            }
            break;
        }
        case MIRACAST_CBK_P2P_DISCONNECTED:
        {
            TLOGD("disconneted...");
            g_awcast.miracast_playing = 0;
            if(g_awcast.work_mode == AWCAST_WORK_MODE_ALL && g_awcast.net_key == 0){
                g_awcast.miracast_disconnected_to_start_dlna = 1;
                wakeup_wireless_display_thread();
                MUI_Dlna_BGShow(g_awcast.main_ui);
            }else{
                MUI_Miracast_BGShow(g_awcast.main_ui);
            }
            g_awcast.ignor_wifi_event = 0;
            break;
        }
        default:
            TLOGW("__Miracast_Event_CB: unknow event: '%d'", enEvent);
        break;
    }
    return 0;
}

int wd_start_miracast(void)
{
    int ret = 0;

    if(g_awcast.miracast_working == 0){
        TLOGD("Miracast_Start......\n");

        g_awcast.miracast_working = 1;
        ret = Miracast_Start(g_awcast.device_name, __Miracast_Event_CB);
        if(ret != 0){
            TLOGE("Miracast_Start failed\n");
            g_awcast.miracast_working = 0;
            return -1;
        }
        TLOGD("Miracast_Start end......\n");
    }else{
        TLOGE("awcast is already in miracast mode\n");
        return -1;
    }

    return 0;
}

int wd_stop_miracast(void)
{
    int ret = 0;

    if(g_awcast.miracast_working){
        TLOGD("Miracast_Stop......\n");
        ret = Miracast_Stop();
        if(ret != 0){
            TLOGE("Miracast_Stop failed\n");
            return -1;
        }
        TLOGD("Miracast_Stop end\n");

        g_awcast.miracast_working = 0;
    }else{
        TLOGE("awcast is not in miracast mode\n");
        return -1;
    }

    return 0;
}

int wd_miracast_init(void)
{
    int ret = 0;
    char miracast_channel[8] = {0};

    ret = Miracast_Init_ex(0, g_awcast.p2p_interface);
    if(ret != 0){
        TLOGE("Miracast_Init failed\n");
        return -1;
    }

    return 0;
}

int wd_miracast_exit(void)
{
    int ret = 0;

    TLOGI("%s:%d: -------------------\n", __func__, __LINE__);

    ret = Miracast_DeInit();
    if(ret != 0){
        TLOGE("Miracast_Init failed\n");
        return -1;
    }

    return 0;
}

#else

int wd_start_miracast(void)
{
    return 0;
}

int wd_stop_miracast(void)
{
    return 0;
}

int wd_miracast_init(void)
{
    return 0;
}

int wd_miracast_exit(void)
{
    return 0;
}


#endif

int clear_net_config(void)
{
    TLOGD("-------------------clear_net_config-----------------\n");

    memset(g_awcast.main_ui->wifi_info, 0, sizeof(g_awcast.main_ui->wifi_info));
    system("source /usr/bin/clear_net_config.sh &");
    TLOGD("clear_net_config, system will restart\n");
    sleep(3);
    system("reboot");

    return 0;
}

int enable_softap(void)
{
    TLOGD("-------------------enable_softap-----------------\n");

    if(g_awcast.softap_enable == 0){
        g_awcast.softap_enable = 1;
        system("source /usr/bin/enable_softap.sh &");
    }else{
        TLOGE("awcast is already enable softap\n");
    }

    return 0;
}

int disable_softap(void)
{
    TLOGD("-------------------%s-----------------\n", __func__);

    if(g_awcast.softap_enable == 1){
        system("source /usr/bin/disable_softap.sh &");
        g_awcast.softap_enable = 0;
        sleep(1);
    }else{
        TLOGE("awcast is already disable softap\n");
    }

    return 0;
}

int switch_start_dlna(void)
{
    int ret = 0;

    TLOGD("-------------------%s-----------------\n", __func__);

    ret = wd_dlna_init();
    if(ret != 0){
        TLOGE("wd_dlna_init failed\n");
        return -1;
    }

    ret = wd_start_dlna();
    if(ret != 0){
        TLOGE("wd_start_dlna failed\n");
        wd_dlna_exit();
        return -1;
    }

    return 0;
}

int switch_stop_dlna(void)
{
    int ret = 0;

    TLOGD("-------------------%s-----------------\n", __func__);

    ret = wd_stop_dlna();
    if(ret != 0){
        TLOGE("wd_stop_dlna failed\n");
    }

    ret = wd_dlna_exit();
    if(ret != 0){
        TLOGE("wd_dlna_exit failed\n");
    }

    return 0;
}

int switch_start_miracast(void)
{
    int ret = 0;

    TLOGD("-------------------%s-----------------\n", __func__);

    //prepare network environment
    system("source /usr/bin/miracast_start_p2p0.sh &");
    sleep(3);

    ret = wd_miracast_init();
    if(ret != 0){
        TLOGE("wd_miracast_init failed\n");
        return -1;
    }

    sleep(3);

    //start miracast
    ret = wd_start_miracast();
    if(ret != 0){
        TLOGE("wd_start_miracast failed\n");
        wd_miracast_exit();
        return -1;
    }

    return 0;
}

int switch_stop_miracast(void)
{
    int ret = 0;

    TLOGD("-------------------%s-----------------\n", __func__);

    ret = wd_stop_miracast();
    if(ret != 0){
        TLOGE("stop_miracast failed\n");
    }

    ret = wd_miracast_exit();
    if(ret != 0){
        TLOGE("wd_miracast_exit failed\n");
    }

    //free network environment
    system("source /usr/bin/miracast_stop_p2p0.sh &");

    return 0;
}

int switch_dlna_to_miracast(void)
{
    int ret = 0;


    TLOGD("-------------------%s-----------------\n", __func__);
    disable_softap();
    ret = wd_stop_dlna();
    if(ret != 0){
        TLOGE("stop_dlna failed\n");
    }

    MUI_Miracast_BGShow(g_awcast.main_ui);

    switch_start_miracast();

    return 0;
}

int switch_miracast_to_dlna(void)
{
    int ret = 0;

    TLOGD("-------------------%s-----------------\n", __func__);

    switch_stop_miracast();

    if(g_awcast.is_net_connected == 0){
        enable_softap();

        MUI_Dlna_BGShow(g_awcast.main_ui);
    }else{
        wd_get_dynamic_ip();
        ret = wd_start_dlna();
        if(ret != 0){
            TLOGE("start_dlna failed\n");
        }
        MUI_Dlna_BGShow(g_awcast.main_ui);
    }

    return 0;
}

int get_wireless_display_mode(void)
{
    if(g_awcast.dlna_working && g_awcast.miracast_working){
        g_awcast.wireless_display_state = WIRELESS_DISPLAY_MODE_ALL;
    }else if(g_awcast.dlna_working){
        g_awcast.wireless_display_state = WIRELESS_DISPLAY_MODE_DLNA;
    }else if(g_awcast.miracast_working){
        g_awcast.wireless_display_state = WIRELESS_DISPLAY_MODE_MIRACAST;
    }else if(g_awcast.airplay_working){
        g_awcast.wireless_display_state = WIRELESS_DISPLAY_MODE_AIRPLAY;
    }else{
        g_awcast.wireless_display_state = WIRELESS_DISPLAY_MODE_IDLE;
    }

    TLOGD("wireless_display_state=%d\n", g_awcast.wireless_display_state);

    return g_awcast.wireless_display_state;
}

int show_wifi_list(void)
{
    return 0;
}

int wd_get_dynamic_ip(void)
{
    TLOGD("-----------udhcpc -i wlan0-----------");
    system("killall udhcpc &");
    sleep(1);
    system("udhcpc -i wlan0 &");
    sleep(1);

    return 0;
}

static int prepare_for_miracast(void)
{
    if(g_awcast.is_net_connected){
        //disable sation mode
    }else{
        //disable ap mode
        disable_softap();
    }

    return 0;
}

static void wakeup_wireless_display_thread(void)
{
    TLOGD("------wakeup_wireless_display_thread------");
    TLOGD("stop_miracast=%d, start_miracast=%d, stop_dlna=%d, start_dlna=%d",
        g_awcast.dlna_entry_to_stop_miracast,
        g_awcast.dlna_quit_to_start_miracast,
        g_awcast.miracast_connected_to_stop_dlna,
        g_awcast.miracast_disconnected_to_start_dlna);
    TLOGD("dlna_add_stop_time=%d", dlna_add_stop_time);
    sem_post(&g_awcast.sem);
}

static void *wireless_display_thread(void *arg)
{
    int time_s = 5;

    while(g_awcast.wd_thread_run){
        sem_wait(&g_awcast.sem);

        if(g_awcast.dlna_quit_to_start_miracast){
            TLOGD("dlna_quit_to_start_miracast, wait for %d secord", time_s);

            for(int i = 0; i < time_s; i++){
                sleep(1);
                if(g_awcast.dlna_quit_to_start_miracast == 0 || dlna_add_stop_time){
                    /* if AWD_EVENT_ENTRY or AWD_EVENT_QUIT is received, then break  */
                    break;
                }
            }

            if(g_awcast.dlna_quit_to_start_miracast){
                if(!g_awcast.dlna_playing && !dlna_add_stop_time){
                    /* If no AWD_EVENT_QUIT message is received, then show main ui */
                    MUI_Dlna_BGShow(g_awcast.main_ui);
                }

                if(dlna_add_stop_time){
                    dlna_add_stop_time = 0;
                    TLOGD("dlna_quit_to_start_miracast, dlna_add_stop_time, wait for %d secord", time_s);

                    for(int i = 0; i < time_s; i++){
                        sleep(1);
                        if(g_awcast.dlna_quit_to_start_miracast == 0){
                            /* if AWD_EVENT_ENTRY is received, then break  */
                            break;
                        }
                    }
                }

                if(!g_awcast.dlna_playing){
                    MUI_Dlna_BGShow(g_awcast.main_ui);
                    if(g_awcast.work_mode == AWCAST_WORK_MODE_ALL && g_awcast.net_key == 0){
                        TLOGD("dlna is quit, begin to start miracast");
                        wd_start_miracast();
                    }
                }
            }

            g_awcast.dlna_quit_to_start_miracast = 0;
            dlna_add_stop_time = 0;
        }else{
            dlna_add_stop_time = 0;
        }

        if(g_awcast.miracast_connected_to_stop_dlna){
            //sleep(time_s);
            if(g_awcast.work_mode == AWCAST_WORK_MODE_ALL && g_awcast.net_key == 0){
                TLOGD("miracast is connected, begin to stop dlna");
                wd_stop_dlna();
            }
            g_awcast.miracast_connected_to_stop_dlna = 0;
        }

        if(g_awcast.miracast_disconnected_to_start_dlna){
            if(g_awcast.is_net_connected){
                //enable sation mode
                if(g_awcast.work_mode == AWCAST_WORK_MODE_ALL && g_awcast.net_key == 0){
                    if(g_awcast.dlna_working){
                        int i = 10;

                        while(i){
                            TLOGD("dlna is working, wait for it stop, time=%d", i);
                            sleep(1);
                            if(g_awcast.dlna_working == 0){
                                break;
                            }
                            i--;
                        }

                        if(i == 0){
                            TLOGD("dlna is working, wait for it stop timeout");
                            g_awcast.miracast_disconnected_to_start_dlna = 0;
                            continue;
                        }
                    }

                    TLOGD("miracast is disconnected, begin to start dlna");
                    wd_get_dynamic_ip();
                    wd_start_dlna();
                }
            }else{
                //enable ap mode
                enable_softap();
            }

            g_awcast.miracast_disconnected_to_start_dlna = 0;
        }
    }

    TLOGD("wireless_display_thread exit");

    return NULL;
}

int wireless_display_init(void)
{
    int ret = 0;

    ret = sem_init(&g_awcast.sem, 0, 0);
    if (ret != 0) {
        TLOGE("sem_init failed!\n");
        return 1;
    }

    g_awcast.wd_thread_run = 1;
    ret = pthread_create(&g_awcast.wd_thread, NULL, wireless_display_thread, NULL);
    if (ret != 0) {
        TLOGE("pthread_create failed\n");
        return 1;
    }

    return 0;
}

int wireless_display_exit(void)
{
    int ret = 0;

    g_awcast.wd_thread_run = 0;
    sem_post(&g_awcast.sem);

    if (pthread_join(g_awcast.wd_thread, NULL)) {
        TLOGE("thread is not exit...\n");
    }
    sem_destroy(&g_awcast.sem);

    return 0;
}


