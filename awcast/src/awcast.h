#ifndef AWCAST_H
#define AWCAST_H

#include <pthread.h>
#include <semaphore.h>

#include <awcast_main_ui.h>
#include <aw_dlna.h>
#include <miracast2.h>

#define AWCAST_DLNA
#define AWCAST_MIRACAST


#define AWCAST_WORK_MODE_ALL            0
#define AWCAST_WORK_MODE_DLNA_ONLY      1
#define AWCAST_WORK_MODE_MIRACAST_ONLY  2
#define AWCAST_WORK_MODE_AIRPLAY_ONLY   3
#define AWCAST_WORK_MODE_NULL           99

#define WIRELESS_DISPLAY_MODE_IDLE      0    /* idle             */
#define WIRELESS_DISPLAY_MODE_DLNA      1    /* dlna working     */
#define WIRELESS_DISPLAY_MODE_MIRACAST  2    /* Miracast working     */
#define WIRELESS_DISPLAY_MODE_AIRPLAY   3    /* airplay working     */
#define WIRELESS_DISPLAY_MODE_ALL       4    /* dlna, Miracast, airplay working     */

#define AWCAST_SWITCH_MODE_AUTO     0
#define AWCAST_SWITCH_MODE_KEY      1

#define AWCAST_DEVICE_NAME_DEFAULT "AWCast"
#define AWCAST_WIFI_INFO_DEFAULT   "WiFi: ..."

struct awcast{
    MainUIT *main_ui;
    struct MainUIS mui;
    char device_name[64];
    char uuid[64];

    AWDlnaT *awd;
    int work_mode;                    /* 0 : all; 1 : dlna only ; 2 : miracast only ; 3 : airplay only */
    int net_key;
    int key_code;
    char key_name[64];
    char p2p_interface[16];

    sem_t sem;
    pthread_t wd_thread;
    char wd_thread_run;
    char dlna_entry_to_stop_miracast;
    char dlna_quit_to_start_miracast;
    char miracast_connected_to_stop_dlna;
    char miracast_disconnected_to_start_dlna;

    char wireless_display_state;
    char net_connecting_start;
    char is_net_connected;
    char softap_enable;
    char dlna_working;          /* 1: dlna start; 0: dlna stop */
    char airplay_working;       /* 1: airplay start; 0: airplay stop */
    char miracast_working;      /* 1: miracast start; 0: miracast stop */
    char ignor_wifi_event;
    char dlna_playing;          /* 1: dlna is playing; 0: dlna is not playing; */
    char miracast_playing;      /* 1: miracast is playing; 0: miracast is not playing; */
    char airplay_playing;       /* 1: airplay is playing; 0: airplay is not playing; */

    char share_interface;       /* only one interface is wlan0 */
};

#endif //AWCAST_H
