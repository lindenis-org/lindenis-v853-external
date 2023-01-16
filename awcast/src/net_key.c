#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <poll.h>
#include <linux/input.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "log.h"
#include "awcast.h"
#include "net_key.h"
#include "wireless_display.h"

#define NET_PATH_MAX 64
#define NET_KEY_LONG_KEY_TIMEOUT 100

#define NET_KEY_VALUE_UP        0
#define NET_KEY_VALUE_DOWN      1
#define NET_KEY_VALUE_REPEAT    2

#define NET_KEY_REPEAT_SHORT_KEY_TIME  1
#define NET_KEY_REPEAT_LONG_KEY_TIME   3

struct net_key{
    pthread_t pid;
    int run;

    char device_path[NET_PATH_MAX]; /* eg. /dev/input/event0 */
    int key_fd;
};

extern struct awcast g_awcast;
static struct net_key g_net_key;

static int find_key_device(const char *device_path, char *key_name)
{
    int fd = 0;
    char name[80];

    LOGD("%s:%d: device_path=%s, key_name=%s\n", __func__,__LINE__, device_path, key_name);

    fd = open(device_path, O_RDWR);
    if(fd < 0) {
        LOGE("could not open %s, %s\n", device_path, strerror(errno));
        goto err;
    }

    if(ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
        LOGE("could not get device name for %s, %s\n", device_path, strerror(errno));
        name[0] = '\0';
        goto err;
    }

    if(!strcmp(name, key_name)){
        LOGD("net key is found, device_path=%s, key_name=%s", device_path, key_name);
        g_net_key.key_fd = fd;
        strcpy(g_net_key.device_path, device_path);
        return 1;
    }

err:
    return 0;
}

static int open_device(char *key_name)
{
    char dirname[] = "/dev/input";
    char devname[NET_PATH_MAX] = {0};
    char *filename = NULL;
    DIR *dir = NULL;
    struct dirent *de = NULL;
    int find = 0;

    dir = opendir(dirname);
    if(dir == NULL){
        LOGE("%s:%d: opendir /dev/input failed\n", __func__,__LINE__);
        return -1;
    }

    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';

    LOGD("%s:%d: filename=%s, devname=%s\n", __func__,__LINE__, filename, devname);

    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
           (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;

        strcpy(filename, de->d_name);
        LOGD("%s:%d: filename=%s, de->d_name=%s\n", __func__,__LINE__, filename, de->d_name);

        if(find_key_device(devname, key_name)){
            find = 1;
            break;
        }
    }

    closedir(dir);

    if(find){
        return 0;
    }else{
        return -1;
    }
}

static int close_device()
{
    if(g_net_key.key_fd){
        close(g_net_key.key_fd);
        g_net_key.key_fd = 0;
    }

    return 0;
}

static void *key_event_func(void *arg)
{
    struct input_event event;
    int ret = 0;
    int res = 0;
    char short_key = 0;
    char long_key = 0;
    char key_down = 0;
    char key_repeat = 0;
    char key_repeat_cnt = 0;
    __kernel_time_t first_repeat_time = 0;
    int wireless_mode = 0;

    ret = open_device(g_awcast.key_name);
    if(ret){
        LOGE("open_device failed\n");
        return NULL;
    }

    while(g_net_key.run){
        res = read(g_net_key.key_fd, &event, sizeof(struct input_event));
        if(res < sizeof(struct input_event)) {
            if(errno == EINTR){
                LOGE("Interrupted system call, %s\n", strerror(errno));
                return NULL;
            }

            LOGE("could not get event, %s\n", strerror(errno));
            return NULL;
        }

        /*
         * short key
         * 1. 短按
         * 2. 长按1s以内
         *
         * long key
         * 1. 长按3s以上
         *
         */
        //if(event.type == EV_KEY){
        if(event.type == EV_KEY && event.code == g_awcast.key_code){
            //LOGD("[%8ld.%06ld],%04x %04x %08x ", event.time.tv_sec, event.time.tv_usec, event.type, event.code, event.value);
            if(event.value == NET_KEY_VALUE_DOWN){
                key_down = 1;
                key_repeat_cnt = 0;
            }else if (event.value == NET_KEY_VALUE_REPEAT){
                key_repeat = 1;

                if(key_repeat_cnt == 0){
                    first_repeat_time = event.time.tv_sec;
                }
                key_repeat_cnt++;
            }else if (event.value == NET_KEY_VALUE_UP){
                if(key_down && !key_repeat){
                    LOGD("\nnet key event is short key\n");
                    short_key = 1;
                }

                if(key_repeat){
                    __kernel_time_t time = 0;

                    time = event.time.tv_sec - first_repeat_time;
                    LOGD("[%8ld, %8ld ,%8ld]\n", first_repeat_time, event.time.tv_sec, time);
                    if(time <= NET_KEY_REPEAT_SHORT_KEY_TIME){
                        LOGD("net key repeat event is short key\n");
                        short_key = 1;
                    }else if(time >= NET_KEY_REPEAT_LONG_KEY_TIME){
                        LOGD("net key repeat event is long key\n");
                        long_key = 1;
                    }else{
                        LOGD("net key repeat event time(%d) is too short, need more than %d second\n", time, NET_KEY_REPEAT_LONG_KEY_TIME);
                    }
                }

                wireless_mode = get_wireless_display_mode();

                if(short_key && (g_awcast.net_key && g_awcast.work_mode == AWCAST_WORK_MODE_ALL)){
                    if(wireless_mode == WIRELESS_DISPLAY_MODE_IDLE){
                        LOGD("mode is idle, switch wireless display mode to miracast\n\n");
                        switch_dlna_to_miracast();
                    }else if(wireless_mode == WIRELESS_DISPLAY_MODE_DLNA){
                        LOGD("in dlna mode, switch wireless display mode to miracast\n\n");
                        switch_dlna_to_miracast();
                    }else if(wireless_mode == WIRELESS_DISPLAY_MODE_MIRACAST){
                        LOGD("in miracast mode, switch wireless display mode to dlna\n\n");
                        switch_miracast_to_dlna();
                    }else{
                        LOGE("unkown wireless display mode(%d)\n", get_wireless_display_mode());
                    }
                }
#if 0
                if(long_key){
                    LOGD("\n\nclean wifi config\n\n");
                    clear_net_config();

                    if(g_awcast.net_key && g_awcast.work_mode == AWCAST_WORK_MODE_ALL){
                        if(wireless_mode == WIRELESS_DISPLAY_MODE_IDLE){
                            LOGD("mode is idle, need not switch mode\n\n");
                        }if(wireless_mode == WIRELESS_DISPLAY_MODE_DLNA){
                            LOGD("in dlna mode, need not switch mode\n");
                        }else if(wireless_mode == WIRELESS_DISPLAY_MODE_MIRACAST){
                            LOGD("in miracst mode, switch wireless display mode to dlna\n\n");
                            switch_miracast_to_dlna();
                        }else{
                            LOGE("unkown wireless display mode(%d)\n", get_wireless_display_mode());
                        }
                    }
                    enable_softap();
                }
#else
                if(long_key){
                    LOGD("\n\nclean wifi config\n\n");
                    MUI_CLear_Net_Config(g_awcast.main_ui);
                    clear_net_config();
                }
#endif
                key_down = 0;
                key_repeat = 0;
                key_repeat_cnt = 0;
                first_repeat_time = 0;
                short_key = 0;
                long_key = 0;
            }else{
                LOGE("unkown key value(0x%x)\n", event.value);
                key_down = 0;
                key_repeat = 0;
                key_repeat_cnt = 0;
                first_repeat_time = 0;
                short_key = 0;
                long_key = 0;
            }
        }
    }

    close_device();

    return NULL;
}

int net_key_init(void)
{
    LOGD("%s:%d:\n", __func__,__LINE__);

    if(g_awcast.key_name[0] == 0){
        LOGE("net key name is null\n");
        return -1;
    }

    memset(&g_net_key, 0, sizeof(struct net_key));

    /* 创建线程pthread */
    g_net_key.run = 1;
    if ((pthread_create(&g_net_key.pid, NULL, key_event_func, NULL) == -1)) {
        LOGE("pthread_create failed, %s!\n", strerror(errno));
        return 1;
    }

    return 0;
}

int net_key_exit(void)
{
    g_net_key.run = 0;

    /* 等待线程pthread释放 */
    if (pthread_join(g_net_key.pid, NULL))
    {
        LOGW("thread is not exit...\n");
        return -1;
    }

    LOGD("net_key_exit!\n");

    return 0;
}

