
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <awcast_main_ui.h>
#include <tina_log.h>
#include "awcast.h"

#define MD_WIFI_SSID "metadata.ssid"
#define MD_WIFI_STATUS "metadata.wifi_status"
#define MD_SOURCE_INFO "metadata.device_info"
#define MD_WIFI_LIST "metadata.wifi_list"


static const char *layout_file = "/usr/res/layout/awcast_main.xml";
extern struct awcast g_awcast;

MainUIT *MUI_Instance()
{
    struct MainUIS *mui = NULL;

    mui = malloc(sizeof(*mui));
    memset(mui, 0, sizeof(struct MainUIS));

    mui->tui = TUI_Instance(layout_file);

    TUI_MetedataAdd(mui->tui, MD_WIFI_SSID);
    TUI_MetedataAdd(mui->tui, MD_WIFI_STATUS);
    TUI_MetedataAdd(mui->tui, MD_SOURCE_INFO);
    for(int i = 0; i < 10; i++){
        char metadata[128] = {0};

        snprintf(metadata, 128, "%s%d", MD_WIFI_LIST, i);
        TLOGI("metadata=%s", metadata);
        TUI_MetedataAdd(mui->tui, metadata);
    }

    return mui;
}

int MUI_Dlna_Update_Info(MainUIT *mui, char *info)
{
    char ssid[64] = {};

    if(g_awcast.device_name[0] == 0) {
        snprintf(mui->wifi_info, sizeof(mui->wifi_info), AWCAST_WIFI_INFO_DEFAULT);
    }
    TLOGI("device_name=%s", g_awcast.device_name);

    snprintf(ssid, sizeof(ssid), "ssid: %s", g_awcast.device_name);
    TUI_MetedataSet(mui->tui, MD_WIFI_SSID, ssid);

    if(info) {
        snprintf(mui->wifi_info, sizeof(mui->wifi_info), "%s", info);
    } else {
        //set default info
        if(mui->wifi_info[0] == 0){
            snprintf(mui->wifi_info, sizeof(mui->wifi_info), AWCAST_WIFI_INFO_DEFAULT);
        }
    }
    TLOGI("wifi_info=%s", mui->wifi_info);
    TUI_MetedataSet(mui->tui, MD_WIFI_STATUS, mui->wifi_info);

    TUI_WidgetShow(mui->tui, "dlna");

    return 0;
}

/* Show last background */
int MUI_Dlna_BGShow(MainUIT *mui)
{
    MUI_Dlna_Update_Info(mui, NULL);
    return 0;
}

int MUI_Miracast_Update_Info(MainUIT *mui, char *info)
{
    snprintf(mui->miracast_dev_info, sizeof(mui->miracast_dev_info), "device name: %s, Waiting for connect", info);

    TUI_MetedataSet(mui->tui, MD_SOURCE_INFO, mui->miracast_dev_info);
    TUI_WidgetShow(mui->tui, "miracast");

    return 0;
}

/* Show last background */
int MUI_Miracast_BGShow(MainUIT *mui)
{
    MUI_Miracast_Update_Info(mui, g_awcast.device_name);
    return 0;
}

/* remove backgroud */
int MUI_Miracast_Connected(MainUIT *mui)
{
    TUI_WidgetShow(mui->tui, "miracast_connected");
    return 0;
}

int MUI_CLear_Net_Config(MainUIT *mui)
{
    TUI_WidgetShow(mui->tui, "clear_net_config");
    return 0;
}

int MUI_Wifi_List_Show(MainUIT *mui, char* wifi_list, int cnt)
{
    for(int i = 0; i < cnt; i++){
        char metadata[128] = {0};

        snprintf(metadata, 128, "%s%d", MD_WIFI_LIST, i);
        TLOGI("metadata1=%s, wifi_list[%d]=%s", metadata, i, wifi_list[i]);
        TUI_MetedataSet(mui->tui, metadata, wifi_list[i]);
    }
    TUI_WidgetShow(mui->tui, "wifi_list");

    return 0;
}
