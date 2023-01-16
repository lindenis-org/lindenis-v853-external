#include "sunxi_display2.h"

// dev_composer defines -->
enum {
    // create a fence timeline
    HWC_NEW_CLIENT = 1,//new a timeline and get the hwc source.
    HWC_DESTROY_CLIENT,
    HWC_ACQUIRE_FENCE,
    HWC_SUBMIT_FENCE,
};
// dev_composer defines <--

struct sync_info{
    int fd;
    unsigned int count;
};

int openDispDev();
int createSyncTimeline(int dispId, int dispFd);
int createSyncPoint(int dispId, int dispFd, struct sync_info *info);
int destroySyncTimeline(int dispId, int dispFd);
int submitLayer(int dispId, int dispFd, unsigned int syncnum, struct disp_layer_config2 *configs, int configCount);
int vsyncCtrl(int dispId, int dispFd, int enable);
int blankCtrl(int dispId, int dispFd, int enable);
int getDeFrequency();
int getDisplayOutputType(int dispId, int dispFd);
int getDisplayOutputMode(int dispId, int dispFd);
int getDisplayOutputSize(int dispId, int dispFd, int* width, int* height);
int sync_wait(int fenceFd, int timeout);
int getDispFreq(int mode);
