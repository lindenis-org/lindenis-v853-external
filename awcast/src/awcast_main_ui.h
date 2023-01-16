#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifndef AWCAST_MAIN_UI_H
#define AWCAST_MAIN_UI_H

#include <tinyui.h>

struct MainUIS
{
    TinyUIT *tui;
    char wifi_info[128];
    char mode[64];

    char miracast_dev_info[128];
};

typedef struct MainUIS MainUIT;

#ifdef __cplusplus
extern "C"
{
#endif

MainUIT *MUI_Instance();

int MUI_Dlna_Update_Info(MainUIT *mui, char *info);
int MUI_Dlna_BGShow(MainUIT *mui);

int MUI_Miracast_Update_Info(MainUIT *mui, char *info);
int MUI_Miracast_BGShow(MainUIT *mui);
int MUI_Miracast_Connected(MainUIT *mui);
int MUI_CLear_Net_Config(MainUIT *mui);
int MUI_Wifi_List_Show(MainUIT *mui, char* wifi_list, int cnt);


#ifdef __cplusplus
}
#endif

#endif // AWCAST_MAIN_UI_H
