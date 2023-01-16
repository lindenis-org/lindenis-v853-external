#ifndef WIRELESS_DISPLAY_H
#define WIRELESS_DISPLAY_H

#include "awcast.h"

int get_wireless_display_mode(void);
int clear_net_config(void);
int enable_softap(void);
int disable_softap(void);
int switch_start_dlna(void);
int switch_stop_dlna(void);
int switch_start_miracast(void);
int switch_stop_miracast(void);
int switch_dlna_to_miracast(void);
int switch_miracast_to_dlna(void);

int wd_start_dlna(void);
int wd_stop_dlna(void);
int wd_start_miracast(void);
int wd_stop_miracast(void);
int wd_dlna_init(void);
int wd_dlna_exit(void);
int wd_miracast_init(void);
int wd_miracast_exit(void);

int show_wifi_list(void);
int wd_get_dynamic_ip(void);

int wireless_display_init(void);
int wireless_display_exit(void);



#endif //WIRELESS_DISPLAY_H
