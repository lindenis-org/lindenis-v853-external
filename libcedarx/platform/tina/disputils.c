#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <poll.h>
#include "disputils.h"
#include "cdx_log.h"

int openDispDev() {
    int fd = open("/dev/disp", O_RDWR);
    if (fd < 0) {
        loge("Open '/dev/disp' failed, %s", strerror(errno));
    }
    return fd;
}

/**
 * @dispId: display screen ID, start from 0
 * @dispFd: fd of /dev/disp
 */
int createSyncTimeline(int dispId, int dispFd)
{
    unsigned long arg[4] = {0};

    arg[0] = dispId;
    arg[1] = HWC_START;
    arg[2] = START_CLIENT_HWC_1_X;
    if (ioctl(dispFd, DISP_HWC_COMMIT, (unsigned long)arg)) {
        loge("start devcomposer failed !!!");
        return -1;
    }

    arg[0] = dispId;
    arg[1] = HWC_SYNC_TIMELINE_SET_OFFSET;
    arg[2] = HWC_SYNC_TIMELINE_DEFAULT;
    arg[3] = 1;
    if (ioctl(dispFd, DISP_HWC_COMMIT, (unsigned long)arg)) {
        loge("HWC_SYNC_TIMELINE_SET_OFFSET disp0_0 failed !");
        return -1;
    }

    return 0;

}

/**
 * @dispId: display screen ID, start from 0
 * @dispFd: fd of /dev/disp
 * @return: fence fd
 */
int createSyncPoint(int dispId, int dispFd)
{
    unsigned long args[4] = {0};

    int fencefd = -1;
	args[0] = dispId;
    args[1] = HWC_COMMIT_FRAME_NUM_INC;
    args[2] = HWC_SYNC_TIMELINE_DEFAULT;
    args[3] = 1;// how many frame we commit one time
    fencefd = ioctl(dispFd, DISP_HWC_COMMIT, (unsigned long)args);
    if (0 <= fencefd) {
        return fencefd;
    }else{
        loge("createSyncpoint failed!");
        return -ENODEV;
    }
}

int destroySyncTimeline(int dispId, int dispFd)
{
    unsigned long arg[4] = {0};

    arg[0] = dispId;
    arg[1] = HWC_STOP;
    arg[2] = START_CLIENT_HWC_1_X;
    if (ioctl(dispFd, DISP_HWC_COMMIT, (unsigned long)arg)) {
        loge("destroySyncTimeline failed!");
        return -1;
    }
    return 0;
}

/**
 * @dispId: display screen ID, start from 0
 * @dispFd: fd of /dev/disp
 * @syncnum: fill count of struct sync_info
 * @disp_layer_config2:
 * @configCount: the number of disp_layer_config2
 */
int submitLayer(int dispId, int dispFd, struct disp_layer_config *configs,
        int configCount)
{
    unsigned long args[4] = {0};

    args[0] = dispId;
    args[1] = 1;
    if (ioctl(dispFd, DISP_SHADOW_PROTECT, (unsigned long)args)) {
        loge("ioctl DISP_SHADOW_PROTECT error");
        return -1;
    }

    args[0] = dispId;
    args[1] = (unsigned long)(configs);
    args[2] = configCount;
    args[3] = 0;

    if (ioctl(dispFd, DISP_LAYER_SET_CONFIG, (unsigned long)args)) {
        loge("ioctl DISP_LAYER_SET_CONFIG2 error");
    }

    args[0] = dispId;
    args[1] = 0;
    args[2] = 0;
    args[3] = 0;
    if (ioctl(dispFd, DISP_SHADOW_PROTECT, (unsigned long)args)) {
        loge("ioctl DISP_SHADOW_PROTECT error");
        return -1;
    }
    return 0;
}

int vsyncCtrl(int dispId, int dispFd, int enable)
{
    unsigned long args[4] = {0};
    args[0] = dispId;
    args[1] = enable;
    if (ioctl(dispFd, DISP_VSYNC_EVENT_EN, (unsigned long)args)) {
        loge("ioctl DISP_VSYNC_EVENT_EN error");
        return -1;
    }
    return 0;
}

int blankCtrl(int dispId, int dispFd, int enable)
{
    unsigned long args[4] = {0};
    args[0] = dispId;
    args[1] = enable;
    if (ioctl(dispFd, DISP_BLANK, (unsigned long)args)) {
        loge("ioctl DISP_BLANK error");
        return -1;
    }
    return 0;
}

int getDisplayOutputType(int dispId, int dispFd)
{
    struct disp_output info;
    unsigned long args[4] = {0};
    args[0] = dispId;
    args[1] = (unsigned long)&info;
    if (ioctl(dispFd, DISP_GET_OUTPUT, (unsigned long)args) == 0) {
        return info.type;
    }
    loge("get display output info failed, display=%d", dispId);
    return -1;
}

int getDisplayOutputMode(int dispId, int dispFd)
{
    struct disp_output info;
    unsigned long args[4] = {0};
    args[0] = dispId;
    args[1] = (unsigned long)&info;
    if (ioctl(dispFd, DISP_GET_OUTPUT, (unsigned long)args) == 0) {
        return info.mode;
    }
    loge("get display output info failed, display=%d", dispId);
    return -1;
}

int getDisplayOutputSize(int dispId, int dispFd, int* width, int* height)
{
    int size[2] = {0};
    unsigned long args[4] = {0};
    args[0] = dispId;
    size[0] = ioctl(dispFd, DISP_GET_SCN_WIDTH,  (unsigned long)args);
    size[1] = ioctl(dispFd, DISP_GET_SCN_HEIGHT, (unsigned long)args);

    if (size[0] <= 0 || size[1] <= 0) {
        loge("get display output size failed, display=%d, size:%dx%d",
                dispId, size[0], size[1]);
        *width  = 720;
        *height = 480;
        return -1;
    }

    *width  = size[0];
    *height = size[1];
    return 0;
}

/**
 * wait fenceFd to release
 * @fenceFd: fenceFd which is acquired by ioctl command HWC_ACQUIRE_FENCE
 * @timeout: the max time to wait fenceFd to release
 */
int sync_wait(int fenceFd, int timeout)
{
    struct pollfd fds;
    int ret;

    if (fenceFd < 0) {
        errno = EINVAL;
        return -1;
    }

    fds.fd = fenceFd;
    fds.events = POLLIN;

    do {
        ret = poll(&fds, 1, timeout);
        if (ret > 0) {
            if (fds.revents & (POLLERR | POLLNVAL)) {
                errno = EINVAL;
                return -1;
            }
            return 0;
        } else if (ret == 0) {
            errno = ETIME;
            return -1;
        }
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

    return ret;
}

int getDispFreq(int mode) {
    switch(mode)
    {
        case DISP_TV_MOD_480I:
            return 60;
        case DISP_TV_MOD_576I:
            return 50;
        case DISP_TV_MOD_480P:
            return 60;
        case DISP_TV_MOD_576P:
            return 50;
        case DISP_TV_MOD_720P_50HZ:
            return 50;
        case DISP_TV_MOD_720P_60HZ:
            return 60;
        case DISP_TV_MOD_1080I_50HZ:
            return 50;
        case DISP_TV_MOD_1080I_60HZ:
            return 60;
        case DISP_TV_MOD_1080P_24HZ:
            return 24;
        case DISP_TV_MOD_1080P_50HZ:
            return 50;
        case DISP_TV_MOD_1080P_60HZ:
            return 60;
        case DISP_TV_MOD_1080P_24HZ_3D_FP:
            return 24;
        case DISP_TV_MOD_720P_50HZ_3D_FP:
            return 50;
        case DISP_TV_MOD_720P_60HZ_3D_FP:
            return 60;
        case DISP_TV_MOD_1080P_25HZ:
            return 25;
        case DISP_TV_MOD_1080P_30HZ:
            return 30;
        case DISP_TV_MOD_PAL:
        case DISP_TV_MOD_PAL_SVIDEO:
        case DISP_TV_MOD_NTSC:
        case DISP_TV_MOD_NTSC_SVIDEO:
        case DISP_TV_MOD_PAL_M:
        case DISP_TV_MOD_PAL_M_SVIDEO:
        case DISP_TV_MOD_PAL_NC:
        case DISP_TV_MOD_PAL_NC_SVIDEO:
            return -1;
        case DISP_TV_MOD_3840_2160P_30HZ:
            return 30;
        case DISP_TV_MOD_3840_2160P_25HZ:
            return 25;
        case DISP_TV_MOD_3840_2160P_24HZ:
            return 24;
        case DISP_TV_MOD_4096_2160P_24HZ:
            return 24;
            /* vga */
        case DISP_VGA_MOD_640_480P_60:
        case DISP_VGA_MOD_800_600P_60:
        case DISP_VGA_MOD_1024_768P_60:
        case DISP_VGA_MOD_1280_768P_60:
        case DISP_VGA_MOD_1280_800P_60:
        case DISP_VGA_MOD_1366_768P_60:
        case DISP_VGA_MOD_1440_900P_60:
        case DISP_VGA_MOD_1920_1080P_60:
        case DISP_VGA_MOD_1920_1200P_60:
            return -1;
        default:
            return -1;
        }
}
