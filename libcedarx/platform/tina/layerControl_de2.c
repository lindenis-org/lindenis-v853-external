/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : layerControl_de.cpp
* Description : display DE -- for H133
* History :
*/


#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <pthread.h>
#include <cdx_log.h>
#include <errno.h>
#include "cdx_config.h"
#include "layerControl.h"
#include "list.h"
#include "disputils2.h"
#include "vbasetype.h"

//#define GLOBAL_LOG_LEVEL    LOG_LEVEL_VERBOSE

#include <CdxIon.h>

#include "sunxi_display2.h"

#include "iniparserapi.h"
#include "uci.h" //add for get display regionScale value

#define SAVE_PIC (0)

#define GPU_BUFFER_NUM 32

static VideoPicture* gLastPicture = NULL;

#define BUF_MANAGE (0)

#define NUM_OF_PICTURE_NODES 16
#define JUDGE_DIFF_THRESHOLD 0.000001
#define SWITCH_BUFFER_SIZE 2

#define DISPLAY_UCI_CONFIG_PATH "/etc/config/system"

int LayerCtrlHideVideo(LayerCtrl* l);

typedef struct VPictureNode_t VPictureNode;
struct VPictureNode_t
{
    struct list_head list;
    VideoPicture* pPicture;
    int           bUsed; // whether node is in list
    bool          dequeueable;//node can be dequeued from node list after it displayed,
    bool          bValid;// whether picture node is valid to display
};

typedef struct BufferInfoS
{
    VideoPicture pPicture;
    int          nUsedFlag;
    void*        pMetaDataVirAddr;
    void*        pMetaDataPhyAddr;
    int          nMetaDataVirAddrSize;
    int          nMetaDataMapFd;
}BufferInfoT;

typedef struct LayerContext
{
    LayerCtrl            base;
    enum EPIXELFORMAT    eDisplayPixelFormat;
    int                  nWidth;
    int                  nHeight;
    int                  nLeftOff;
    int                  nTopOff;
    int                  nDisplayWidth;
    int                  nDisplayHeight;

    int                  bHoldLastPictureFlag;
    int                  bVideoWithTwoStreamFlag;
    int                  bIsSoftDecoderFlag;

    int                  bLayerInitialized;
    int                  bProtectFlag;

    void*                pUserData;

    //* use when render derect to hardware layer.
    VPictureNode         picNodes[NUM_OF_PICTURE_NODES];

    int                  nGpuBufferCount;
    BufferInfoT          mBufferInfo[GPU_BUFFER_NUM];
    int                  bLayerShowed;

    int                  fdDisplay;
    int                  nScreenWidth;
    int                  nScreenHeight;
    int screen_x,screen_y,screen_w,screen_h;
    int regionScale;
    int bHdrVideoFlag;
    int b10BitVideoFlag;
    int bAfbcModeFlag;

    // render thread
    pthread_mutex_t      mutex;
    pthread_cond_t       cond_dequeue;
    pthread_cond_t       cond_queue;
    bool                 running;
    pthread_t            renderThread;
    int                  fdFrameBuffer;  // for waiting vsync
    VPictureNode*        prePicNode;
    VPictureNode*        currentPicNode;
    int                  currentFenceFd;
    int                  preFenceFd;
    struct list_head     picNodeListHead;
    unsigned int         count;// member of struct sync_info
    int                  bResolutionChange;
}LayerContext;

static int display_uci_get_config(const char *name, char *value, size_t value_max_len)
{
    logv("display_uci_get_config %s", name);

    struct uci_context *ctx = uci_alloc_context();
    if(!ctx) return -1;

    struct uci_package *pkg = NULL;

    if(UCI_OK != uci_load(ctx, DISPLAY_UCI_CONFIG_PATH, &pkg))
    {
        loge("uci_load config fail,path %s",DISPLAY_UCI_CONFIG_PATH);
        if(pkg)
        {
            uci_unload(ctx, pkg);
        }
        uci_free_context(ctx);
        return -1;
    }

    struct uci_section *section = uci_lookup_section(ctx, pkg, "setting");
    if(!section || strcmp(section->type, "system") != 0)
    {
        uci_unload(ctx, pkg);
        uci_free_context(ctx);
        return -1;
    }

    const char *content = uci_lookup_option_string(ctx, section, name);
    if(!content || value_max_len <= strlen(content))
    {
        uci_unload(ctx, pkg);
        uci_free_context(ctx);
        return -1;
    }
    else
    {
        memcpy(value, content, strlen(content));
    }

    uci_unload(ctx, pkg);
    uci_free_context(ctx);

    logv("display_uci_get_config done: %s", value);

    return 0;
}

//* set usage, scaling_mode, pixelFormat, buffers_geometry, buffers_count, crop
static int setLayerBuffer(LayerContext* lc, int width, int height)
{
    logd("setLayerBuffer: PixelFormat(%d), nW(%d), nH(%d)",
          lc->eDisplayPixelFormat, width, height);
    logd("setLayerBuffer: buffercount(%d), bProtectFlag(%d), bIsSoftDecoderFlag(%d)",
          lc->nGpuBufferCount,lc->bProtectFlag,lc->bIsSoftDecoderFlag);

    int          pixelFormat;
    unsigned int nGpuBufWidth;
    unsigned int nGpuBufHeight;
    int i = 0;
    char* pVirBuf;
    char* pPhyBuf;
    int   nBufFd;

    int   nMemSizeY;
    int   nMemSizeC;

    char* pMeteVirBuf;
    char* pMetePhyBuf;
    int   nMeteBufFd;
    int   nTotalPicPhyBufSize = 0;

    nGpuBufWidth  = width;  //* restore nGpuBufWidth to mWidth;
    nGpuBufHeight = height;

    //* We should double the height if the video with two stream,
    //* so the nativeWindow will malloc double buffer
    if(lc->bVideoWithTwoStreamFlag == 1)
    {
        nGpuBufHeight = 2*nGpuBufHeight;
    }

    if(lc->nGpuBufferCount <= 0)
    {
        loge("error: the lc->nGpuBufferCount[%d] is invalid!",lc->nGpuBufferCount);
        return -1;
    }

    for(i=0; i<lc->nGpuBufferCount; i++)
    {
        nMemSizeY = nGpuBufWidth*nGpuBufHeight;

        if(lc->eDisplayPixelFormat == PIXEL_FORMAT_YUV_PLANER_420 ||
           lc->eDisplayPixelFormat == PIXEL_FORMAT_YV12 ||
           lc->eDisplayPixelFormat == PIXEL_FORMAT_NV21)
            nMemSizeC = nMemSizeY>>2;
        else if(lc->eDisplayPixelFormat == PIXEL_FORMAT_YUV_PLANER_422)
            nMemSizeC = nMemSizeY>>1;
        else
            nMemSizeC = nMemSizeY;  //* PIXEL_FORMAT_YUV_PLANER_444

        int nLower2BitBufSize = 0;
        int nAfbcBufSize = 0;
        int nNormalYuvBufSize = nMemSizeY + nMemSizeC*2;
        int nTotalPicPhyBufSize = 0;
        int frmbuf_c_size = 0;

        nLower2BitBufSize = ((((nGpuBufWidth+3)>>2) + 31) & 0xffffffe0) * nGpuBufHeight * 3/2;
        //int PriChromaStride = ((nGpuBufWidth/2) + 31)&0xffffffe0;
        //frmbuf_c_size = 2 * (PriChromaStride * (((nGpuBufHeight/2)+15)&0xfffffff0)/4);
        if(lc->b10BitVideoFlag)
        {
            if(lc->bAfbcModeFlag == 1)
            {
                nAfbcBufSize = ((nGpuBufWidth+15)>>4) * ((nGpuBufHeight+4+15)>>4) * (512 + 16) + 32 + 1024;
                nTotalPicPhyBufSize = nAfbcBufSize + frmbuf_c_size + nLower2BitBufSize;
                logd("nTotalSize = %d, nAfbcBufSize = %d,\
                     frmbuf_c_size = %d, nLower2BitBufSize = %d",
                     nTotalPicPhyBufSize, nAfbcBufSize,
                     frmbuf_c_size, nLower2BitBufSize);
            }
            else
            {
                nTotalPicPhyBufSize = nNormalYuvBufSize + nLower2BitBufSize;
            }
        }
        else
        {
            if(lc->bAfbcModeFlag == 1)
            {
                nAfbcBufSize = ((nGpuBufWidth+15)>>4) * ((nGpuBufHeight+4+15)>>4) * (384 + 16) + 32 + 1024;
                nTotalPicPhyBufSize = nAfbcBufSize + frmbuf_c_size;
                logd("the_afbc,nAfbcBufSize=%d",nAfbcBufSize);
            }
            else
            {
                nTotalPicPhyBufSize = nNormalYuvBufSize;
            }
        }

        pVirBuf = CdxIonPalloc(nTotalPicPhyBufSize);

        if (pVirBuf == NULL)
        {
			loge("CdxIonPalloc failure");
        	goto err_out;
        }

        pPhyBuf = CdxIonVir2Phy(pVirBuf);
        nBufFd  = CdxIonVir2Fd(pVirBuf);

        if(lc->b10BitVideoFlag)
        {
            if(lc->bAfbcModeFlag== 1)
            {
                lc->mBufferInfo[i].pPicture.nLower2BitBufSize  = nLower2BitBufSize;
                lc->mBufferInfo[i].pPicture.nLower2BitBufOffset = nAfbcBufSize + frmbuf_c_size;
                lc->mBufferInfo[i].pPicture.nLower2BitBufStride = ((((nGpuBufWidth+3)>>2) + 31) & 0xffffffe0);
                lc->mBufferInfo[i].pPicture.nAfbcSize = nAfbcBufSize;
            }
            else
            {
                lc->mBufferInfo[i].pPicture.pData1 = pVirBuf + nMemSizeY;
                lc->mBufferInfo[i].pPicture.nLower2BitBufSize  = nLower2BitBufSize;
                lc->mBufferInfo[i].pPicture.nLower2BitBufOffset = nNormalYuvBufSize;
                lc->mBufferInfo[i].pPicture.nLower2BitBufStride = ((((nGpuBufWidth+3)>>2) + 31) & 0xffffffe0);
            }
        }
        else
        {
            if(lc->bAfbcModeFlag== 1)
            {
                lc->mBufferInfo[i].pPicture.nAfbcSize = nAfbcBufSize;
                logd("the_afbc,pVirBuf=%p",pVirBuf);
            }
            else
            {
                lc->mBufferInfo[i].pPicture.pData1 = pVirBuf + nMemSizeY;
                if(lc->eDisplayPixelFormat != PIXEL_FORMAT_NV21)
                    lc->mBufferInfo[i].pPicture.pData2 = pVirBuf + nMemSizeY + nMemSizeC;
                else
                    lc->mBufferInfo[i].pPicture.pData2 = NULL;
            }
        }

        lc->mBufferInfo[i].nUsedFlag    = 0;
        lc->mBufferInfo[i].pPicture.nWidth  = width;
        lc->mBufferInfo[i].pPicture.nHeight = height;
        lc->mBufferInfo[i].pPicture.nLineStride  = width;
        lc->mBufferInfo[i].pPicture.pData0       = pVirBuf;
        lc->mBufferInfo[i].pPicture.pData3 = NULL;
        lc->mBufferInfo[i].pPicture.phyYBufAddr  = (uintptr_t)pPhyBuf;
        lc->mBufferInfo[i].pPicture.phyCBufAddr  =
              lc->mBufferInfo[i].pPicture.phyYBufAddr + (height * width);
        lc->mBufferInfo[i].pPicture.nBufSize = nTotalPicPhyBufSize;
        lc->mBufferInfo[i].pPicture.nBufId       = i;
        lc->mBufferInfo[i].pPicture.nBufFd       = nBufFd;
        lc->mBufferInfo[i].pPicture.ePixelFormat = lc->eDisplayPixelFormat;

        pMeteVirBuf = CdxIonPalloc(4096);
        pMetePhyBuf = CdxIonVir2Phy(pMeteVirBuf);
        nMeteBufFd  = CdxIonVir2Fd(pMeteVirBuf);
        logv("pMetePhyBuf %p",pMetePhyBuf);

        if (pMeteVirBuf == NULL)
        {
            loge("error: ion buff allocate fail!");
            goto err_out;
        }

        lc->mBufferInfo[i].pPicture.pMetaData   = pMeteVirBuf;
        lc->mBufferInfo[i].pMetaDataVirAddr = pMeteVirBuf;
        lc->mBufferInfo[i].pMetaDataPhyAddr = pMetePhyBuf;
        lc->mBufferInfo[i].nMetaDataMapFd   = nMeteBufFd;


        logd("=== init id:%d pVirBuf: %p", i, pVirBuf);
    }

    return 0;

err_out:
    for (i=0; i<lc->nGpuBufferCount; i++)
    {
    	if (lc->mBufferInfo[i].pPicture.pData0)
        {
        	CdxIonPfree(lc->mBufferInfo[i].pPicture.pData0);
        	lc->mBufferInfo[i].pPicture.pData0 = NULL;
        }

        if (lc->mBufferInfo[i].pPicture.pMetaData)
        {
            CdxIonPfree(lc->mBufferInfo[i].pPicture.pMetaData);
            lc->mBufferInfo[i].pPicture.pMetaData = NULL;
        }
    }

	return -1;
}

static int SetLayerParam(LayerContext* lc, VideoPicture* pPicture)
{
    struct disp_layer_config2 config;
    unsigned long     args[4];
    int ret = 0;
    if (pPicture == NULL) {
        loge("pPicture == NULL, just return.");
        return -1;
    }
    if (pPicture->pData0 == NULL) {
        loge("pPicture->pData0 == NULL, maybe resolution changed, just return.");
        return -1;
    }
    if (lc->bResolutionChange == 1) {
        loge("resolution is changing, ignore the last picture!");
        return -1;
    }

    //* close the layer first, otherwise, in case when last frame is kept showing,
    //* the picture showed will not valid because parameters changed.
    memset(&config.info, 0, sizeof(struct disp_layer_info2));
    if(lc->bLayerShowed == 1)
    {
        lc->bLayerShowed = 0;
        //TO DO.
    }

    //* transform pixel format.
    switch(lc->eDisplayPixelFormat)
    {
        case PIXEL_FORMAT_YUV_PLANER_420:
            config.info.fb.format = DISP_FORMAT_YUV420_P;
            config.info.fb.size[0].width     = pPicture->nWidth;
            config.info.fb.size[0].height    = pPicture->nHeight;
            config.info.fb.size[1].width     = pPicture->nWidth/2;
            config.info.fb.size[1].height    = pPicture->nHeight/2;
            config.info.fb.size[2].width     = pPicture->nWidth/2;
            config.info.fb.size[2].height    = pPicture->nHeight/2;
        break;

        case PIXEL_FORMAT_YV12:
            config.info.fb.format = DISP_FORMAT_YUV420_P;
            config.info.fb.size[0].width     = pPicture->nWidth;
            config.info.fb.size[0].height    = pPicture->nHeight;
            config.info.fb.size[1].width     = pPicture->nWidth/2;
            config.info.fb.size[1].height    = pPicture->nHeight/2;
            config.info.fb.size[2].width     = pPicture->nWidth/2;
            config.info.fb.size[2].height    = pPicture->nHeight/2;
        break;

        case PIXEL_FORMAT_NV12:
            config.info.fb.format = DISP_FORMAT_YUV420_SP_UVUV;
            config.info.fb.size[0].width     = pPicture->nWidth;
            config.info.fb.size[0].height    = pPicture->nHeight;

            config.info.fb.size[1].width     = pPicture->nWidth/2;
            config.info.fb.size[1].height    = pPicture->nHeight/2;
        break;

        case PIXEL_FORMAT_NV21:
            config.info.fb.format = DISP_FORMAT_YUV420_SP_VUVU;
            config.info.fb.size[0].width     = pPicture->nWidth;
            config.info.fb.size[0].height    = pPicture->nHeight;
            config.info.fb.size[1].width     = pPicture->nWidth/2;
            config.info.fb.size[1].height    = pPicture->nHeight/2;
        break;

        default:
        {
            loge("unsupported pixel format.");
            return -1;
        }
    }

    //* initialize the layerInfo.

    //* image size.
    //if((lc->nDisplayWidth != pPicture->nRightOffset)||(lc->nDisplayHeight != pPicture->nBottomOffset))
    //{
    //    lc->nDisplayWidth = pPicture->nRightOffset;
    //    lc->nDisplayHeight = pPicture->nBottomOffset;
    //}

    config.info.fb.crop.x = (unsigned long long)lc->nLeftOff << 32;
    config.info.fb.crop.y = (unsigned long long)lc->nTopOff << 32;
    config.info.fb.crop.width   = (unsigned long long)lc->nDisplayWidth << 32;
    config.info.fb.crop.height  = (unsigned long long)lc->nDisplayHeight << 32;
    config.info.fb.color_space  = (pPicture->nHeight < 720) ? DISP_BT601 : DISP_BT709;

    //set screen display region
    if (lc->screen_w == 0 || lc->screen_h == 0) {
        int scaleW = lc->nScreenWidth  * lc->regionScale / 100;
        int scaleH = lc->nScreenHeight * lc->regionScale / 100;

        config.info.screen_win.x        = 0 + (lc->nScreenWidth - scaleW)/2;
        config.info.screen_win.y        = 0 + (lc->nScreenHeight -  scaleH)/2;
        config.info.screen_win.width    = scaleW;
        config.info.screen_win.height   = scaleH;

        float displayRatio = lc->nDisplayWidth/lc->nDisplayHeight;
        float screenRatio  = lc->nScreenWidth/lc->nScreenHeight;
        if(displayRatio - screenRatio > JUDGE_DIFF_THRESHOLD)
        {
            config.info.screen_win.height  = lc->nDisplayHeight * scaleW / lc->nDisplayWidth;
            config.info.screen_win.y       = (lc->nScreenHeight - config.info.screen_win.height)/2;
        }
        else if(screenRatio - displayRatio > JUDGE_DIFF_THRESHOLD)
        {
            config.info.screen_win.width   = lc->nDisplayWidth * scaleH /lc->nDisplayHeight;
            config.info.screen_win.x       = (lc->nScreenWidth - config.info.screen_win.width )/2;
        }
    } else {
        config.info.screen_win.x        = lc->screen_x;
        config.info.screen_win.y        = lc->screen_y;
        config.info.screen_win.width    = lc->screen_w;
        config.info.screen_win.height   = lc->screen_h;
    }

#if 0
    logd("crop '%d', '%d', '%d', '%d'",
		config.info.screen_win.x, config.info.screen_win.y,
		config.info.screen_win.width, config.info.screen_win.height);
#endif

    int i;
    for(i = 0; i < GPU_BUFFER_NUM; i++)
    {
        if(lc->mBufferInfo[i].pMetaDataVirAddr == pPicture->pMetaData)
        {
            config.info.fb.metadata_fd = lc->mBufferInfo[i].nMetaDataMapFd;
            config.info.fb.metadata_size = 4096;
            if (lc->bAfbcModeFlag)
            {
                config.info.fb.metadata_flag = 1<<4;
                config.info.fb.fbd_en = 1;
            }
            if (lc->bHdrVideoFlag)
            {
                config.info.fb.metadata_flag = config.info.fb.metadata_flag|(1<<1);
            }
            break;
        }
    }

    config.info.fb.fd = pPicture->nBufFd;
    config.info.alpha_mode          = 1;
    if (pPicture->nWidth == SWITCH_BUFFER_SIZE) {
        config.info.screen_win.x        = 0;
        config.info.screen_win.y        = 0;
        config.info.screen_win.width    = SWITCH_BUFFER_SIZE;
        config.info.screen_win.height   = SWITCH_BUFFER_SIZE;
        config.info.zorder          = 0;//try to hide the switchbuffer layer
    } else {
        config.info.zorder          = 1;
    }
    /*
     * alpha_mode: 0: pixel alpha;  1: global alpha;  2: global pixel alpha (mixed alpha)
     * alpha value effect when alpha_mode = 1(global alpha) or alpha_mode = 2(global pixel alpha)
     * video channel 0 no alpha mode, other video channel only support global alpha;
     * ui channel support both pixel and global alpha;
     */
    config.info.alpha_value         = 0xff;
    config.info.mode            = LAYER_MODE_BUFFER;
    config.channel = 0;
    config.enable = 1;
    config.layer_id = 0;
    //* set layerInfo to the driver.
    //args[1] = (unsigned long)(&config);
    //args[2] = 1;
    //ret = ioctl(lc->fdDisplay, DISP_LAYER_SET_CONFIG2, (void*)args);

    ret = submitLayer(0, lc->fdDisplay, lc->count, &config, 1);

    char displayChars[4]  = "";
//    if (display_uci_get_config("region_scale", displayChars, sizeof(displayChars)) != -1)
//        lc->regionScale = atoi(displayChars);
//    else
    {
        lc->regionScale = 100;
        logv("get display region scale fail,use default value %d !",lc->regionScale);
    }
    args[0] = 0;
    lc->nScreenWidth  = ioctl(lc->fdDisplay, DISP_GET_SCN_WIDTH, (void *)args);
    lc->nScreenHeight = ioctl(lc->fdDisplay, DISP_GET_SCN_HEIGHT,(void *)args);
    if(0 != ret)
    	loge("fail to set layer info!");

    return 0;
}

//* Description: set initial param -- video whether have hdr info or not
static int LayerSetHdrInfo(LayerCtrl *l, const FbmBufInfo *fbmInfo)
{
	if (!fbmInfo)
	{
		loge("fbmInfo is null");
		return -1;
	}
	LayerContext* lc;
	lc = (LayerContext*)l;
	lc->bHdrVideoFlag = fbmInfo->bHdrVideoFlag;
	lc->b10BitVideoFlag = fbmInfo->b10bitVideoFlag;
	lc->bAfbcModeFlag = fbmInfo->bAfbcModeFlag;
	return 0;
}

static int __LayerReset(LayerCtrl* l)
{
    LayerContext* lc;
    int i;

    logd("LayerReset.");

    lc = (LayerContext*)l;

    struct disp_layer_config2 config;
    unsigned long args[4];
    memset(&config.info, 0, sizeof(struct disp_layer_info2));
    config.channel = 0;
    config.enable = 0;
    config.layer_id = 0;
    //* set layerInfo to the driver.
    args[0] = 0;
    args[1] = (unsigned long)(&config);
    args[2] = 1;

    ioctl(lc->fdDisplay, DISP_LAYER_SET_CONFIG2, (void*)args);
    usleep(20*1000);//make sure display disabled
    logd("layer reset,display disabled...");

    for(i=0; i<lc->nGpuBufferCount; i++)
    {
        CdxIonPfree(lc->mBufferInfo[i].pPicture.pData0);
        CdxIonPfree(lc->mBufferInfo[i].pPicture.pMetaData);
        lc->mBufferInfo[i].nUsedFlag = 0;
        lc->picNodes[i].bUsed        = 0;
        lc->picNodes[i].bValid       = false;
        lc->picNodes[i].dequeueable  = true;
        lc->picNodes[i].pPicture     = NULL;
    }
    INIT_LIST_HEAD(&lc->picNodeListHead);
    return 0;
}


static void __LayerRelease(LayerCtrl* l)
{
    LayerContext* lc;
    int i;

    lc = (LayerContext*)l;

    logv("Layer release");
    struct disp_layer_config2 config;
    unsigned long args[4];
    memset(&config.info, 0, sizeof(struct disp_layer_info2));
    config.channel = 0;
    config.enable = 0;
    config.layer_id = 0;
    //* set layerInfo to the driver.
    args[0] = 0;
    args[1] = (unsigned long)(&config);
    args[2] = 1;

    ioctl(lc->fdDisplay, DISP_LAYER_SET_CONFIG2, (void*)args);
    usleep(20*1000);//make sure display disabled
    logd("layer release,display disabled...");

    for(i=0; i<lc->nGpuBufferCount; i++)
    {
        CdxIonPfree(lc->mBufferInfo[i].pPicture.pData0);
        CdxIonPfree(lc->mBufferInfo[i].pPicture.pMetaData);
        lc->mBufferInfo[i].nUsedFlag = 0;
        lc->picNodes[i].bUsed        = 0;
        lc->picNodes[i].bValid       = false;
        lc->picNodes[i].dequeueable  = true;
        lc->picNodes[i].pPicture     = NULL;
    }
    INIT_LIST_HEAD(&lc->picNodeListHead);
}

static void __LayerDestroy(LayerCtrl* l)
{
    logd("layer destroy");
    LayerContext* lc;
    lc = (LayerContext*)l;

    if (destroySyncTimeline(0, lc->fdDisplay) < 0){
        loge("destroySyncTimeline failed!");
    }
    if(lc->fdDisplay>=0)
        close(lc->fdDisplay);
    lc->fdDisplay=-1;
    CdxIonClose();
    lc->running = false;
    pthread_mutex_lock(&lc->mutex);
    pthread_cond_signal(&lc->cond_queue);
    pthread_mutex_unlock(&lc->mutex);

    pthread_join(lc->renderThread, NULL);
    pthread_mutex_destroy(&lc->mutex);
    pthread_cond_destroy(&lc->cond_dequeue);
    pthread_cond_destroy(&lc->cond_queue);
    free(lc);
}


static int __LayerSetDisplayBufferSize(LayerCtrl* l, int nWidth, int nHeight)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

    lc->nWidth         = nWidth;
    lc->nHeight        = nHeight;
    lc->bLayerInitialized = 0;

    if(lc->bVideoWithTwoStreamFlag == 1)
    {
        //* display the whole buffer region when 3D
        //* as we had init align-edge-region to black. so it will be look ok.
        int nScaler = 2;
        lc->nDisplayHeight = lc->nDisplayHeight*nScaler;
    }

    return 0;
}

//* Description: set initial param -- display region
static int __LayerSetDisplayRegion(LayerCtrl* l, int nLeftOff, int nTopOff,
                                        int nDisplayWidth, int nDisplayHeight)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    logd("Layer set display region, leftOffset = %d, topOffset = %d, "
    	"displayWidth = %d, displayHeight = %d",
            nLeftOff, nTopOff, nDisplayWidth, nDisplayHeight);

    if(nDisplayWidth == 0 && nDisplayHeight == 0)
    {
        return -1;
    }

    lc->nDisplayWidth = nDisplayWidth;
    lc->nDisplayHeight = nDisplayHeight;
    lc->nLeftOff = nLeftOff;
    lc->nTopOff = nTopOff;

    if(lc->bVideoWithTwoStreamFlag == 1)
    {
        //* display the whole buffer region when 3D
        //* as we had init align-edge-region to black. so it will be look ok.
        int nScaler = 2;
        lc->nDisplayHeight = lc->nHeight*nScaler;
    }

    return 0;
}

//* Description: set initial param -- screen display region
static int __LayerSetScreenRegion(LayerCtrl* l, int x, int y,
                                        int width, int height)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    logd("Layer set screen region, leftOffset = %d, topOffset = %d, "
    	"screenWidth = %d, screenHeight = %d",
            x, y, width, height);

    if(width == 0 || height == 0)
    {
        return -1;
    }

    lc->screen_x = x;
    lc->screen_y = y;
    lc->screen_w = width;
    lc->screen_h = height;

    return 0;
}

static int switchBuffer(LayerContext* lc)
{
    int64_t start=CdxGetNowUs();
    int i=0, j=0, err=0;
    int width = SWITCH_BUFFER_SIZE;
    int height = SWITCH_BUFFER_SIZE;
    int YSize = width * height;
    void* pDataBuf = NULL;

    //set buffer display region of switchBuffer
    __LayerSetDisplayRegion(lc, 0, 0, width, height);

    //allocate buffers
    if(setLayerBuffer(lc, width, height) != 0)
    {
        loge("can not alloc %dx%d buffer to replace the last video larger buffers, switchBuffer failed", SWITCH_BUFFER_SIZE, SWITCH_BUFFER_SIZE);
        return -1;
    }

    for(i = 0; i < lc->nGpuBufferCount; i++)
    {
        pDataBuf = lc->mBufferInfo[i].pPicture.pData0;
        if(lc->eDisplayPixelFormat == PIXEL_FORMAT_P010_UV ||
           lc->eDisplayPixelFormat == PIXEL_FORMAT_P010_VU)
        {
            memset(pDataBuf, 0x10, YSize*2);
            memset((char*)pDataBuf+YSize*2, 0x80, YSize);
        }
        else
        {//go here
            memset(pDataBuf, 0x10, YSize);
            memset((char*)pDataBuf+YSize, 0x80, YSize/2);
        }

        if (SetLayerParam(lc, &lc->mBufferInfo[i].pPicture) != 0) {
            loge("switchBuffer: can not render frame");
        }
        //wait for display driver to replace the last play buffer.
        usleep(10*1000);
    }
    __LayerRelease(lc);
    logw("switchBuffer cost time : %lldus", CdxGetNowUs()-start);
    return 0;
}

//* Description: set initial param -- display pixelFormat
static int __LayerSetDisplayPixelFormat(LayerCtrl* l, enum EPIXELFORMAT ePixelFormat)
{
    LayerContext* lc;

    lc = (LayerContext*)l;
    logd("Layer set expected pixel format, format = %d", (int)ePixelFormat);

    if(ePixelFormat == PIXEL_FORMAT_NV12 ||
       ePixelFormat == PIXEL_FORMAT_NV21 ||
       ePixelFormat == PIXEL_FORMAT_YV12)           //* add new pixel formats supported by gpu here.
    {
        lc->eDisplayPixelFormat = ePixelFormat;
    }
    else
    {
        logv("receive pixel format is %d, not match.", lc->eDisplayPixelFormat);
        return -1;
    }

    return 0;
}

//* Description: set initial param -- deinterlace flag
static int __LayerSetDeinterlaceFlag(LayerCtrl* l,int bFlag)
{
    LayerContext* lc;
    (void)bFlag;
    lc = (LayerContext*)l;

    return 0;
}

//* Description: set buffer timestamp -- set this param every frame
static int __LayerSetBufferTimeStamp(LayerCtrl* l, int64_t nPtsAbs)
{
    LayerContext* lc;
    (void)nPtsAbs;

    lc = (LayerContext*)l;

    return 0;
}

static int __LayerGetRotationAngle(LayerCtrl* l)
{
    LayerContext* lc;
    int nRotationAngle = 0;

    lc = (LayerContext*)l;

    return 0;
}

static int __LayerCtrlShowVideo(LayerCtrl* l)
{
    LayerContext* lc;
    int               i;

    lc = (LayerContext*)l;

    lc->bLayerShowed = 1;

    return 0;
}

static int __LayerCtrlHideVideo(LayerCtrl* l)
{
    LayerContext* lc;
    int               i;

    lc = (LayerContext*)l;

    lc->bLayerShowed = 0;

    return 0;
}

static int __LayerCtrlIsVideoShow(LayerCtrl* l)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

     return lc->bLayerShowed;
}

static int  __LayerCtrlHoldLastPicture(LayerCtrl* l, int bHold)
{
    logd("LayerCtrlHoldLastPicture, bHold = %d", bHold);

    LayerContext* lc;
    lc = (LayerContext*)l;

    return 0;
}

static VPictureNode* getDequeueableNode(LayerContext* lc) {
    VPictureNode* node = NULL;
    list_for_each_entry(node, &lc->picNodeListHead, list){
        if(node->dequeueable){
            list_del(&node->list);
            node->bUsed = 0;
            return node;
        }
    }
    return NULL;
}

static int LayerDequeueBufferLocked(LayerCtrl* l, VideoPicture** ppVideoPicture, int bInitFlag)
{
    LayerContext* lc;
    int i = 0;
    VPictureNode*     nodePtr;
    BufferInfoT bufInfo;
    VideoPicture* pPicture = NULL;

    lc = (LayerContext*)l;

    if(lc->bLayerInitialized == 0)
    {
        if (lc->nWidth * lc->nHeight > 1500000){//1080p
            switchBuffer(lc);
        }
        if(setLayerBuffer(lc, lc->nWidth, lc->nHeight) != 0)
        {
            loge("can not initialize layer.");
            return -1;
        }

        lc->bLayerInitialized = 1;
    }

    if(bInitFlag == 1)
    {
        for(i = 0; i < lc->nGpuBufferCount; i++)
        {
            if(lc->mBufferInfo[i].nUsedFlag == 0)
            {
                //* set the buffer address
                pPicture = *ppVideoPicture;
                pPicture = &lc->mBufferInfo[i].pPicture;

                lc->mBufferInfo[i].nUsedFlag = 1;
                break;
            }
        }
    }
    else
    {
        struct list_head *p;
        i = 0;
        list_for_each(p, &lc->picNodeListHead) {
            i++;
        }
        if(i > GetConfigParamterInt("pic_4list_num", 3))
        {
            VPictureNode* node = getDequeueableNode(lc);
            while (!node) {
                pthread_cond_wait(&lc->cond_dequeue, &lc->mutex);
                node = getDequeueableNode(lc);
            }
            pPicture = node->pPicture;
        }

    }

    *ppVideoPicture = pPicture;
    if(pPicture)
    {
        logv("** dequeue  pPicture(%p), id(%d)", pPicture, pPicture->nBufId);
        return 0;
    }
    else
    {
        logv("** dequeue  pPicture(%p)", pPicture);
        return -1;
    }
}

static int __LayerDequeueBuffer(LayerCtrl* l, VideoPicture** ppVideoPicture, int bInitFlag)
{
    int result;
    LayerContext* lc = (LayerContext*)l;
    pthread_mutex_lock(&lc->mutex);
    result = LayerDequeueBufferLocked(l, ppVideoPicture, bInitFlag);
    pthread_mutex_unlock(&lc->mutex);
    return result;
}

// this method should block here,
static int LayerQueueBufferLocked(LayerCtrl* l, VideoPicture* pBuf, int bValid)
{
    LayerContext* lc  = NULL;

    int               i;
    VPictureNode*     newNode;
    VPictureNode*     nodePtr;
    BufferInfoT    bufInfo;

    lc = (LayerContext*)l;

    if(pBuf)
        logv("** queue , pPicture(%p), id(%d)", pBuf, pBuf->nBufId);
    else
        logv("** queue , pPicture(%p)", pBuf);

    if(lc->bLayerInitialized == 0)
    {
        if(setLayerBuffer(lc, lc->nWidth, lc->nHeight) != 0)
        {
            loge("can not initialize layer.");
            return -1;
        }

        lc->bLayerInitialized = 1;
    }

    newNode = NULL;
    for(i = 0; i< NUM_OF_PICTURE_NODES; i++)
    {
        if(lc->picNodes[i].bUsed == 0)
        {
            newNode = &lc->picNodes[i];
            newNode->bUsed = 1;
            newNode->pPicture = pBuf;
            newNode->dequeueable = true;
            newNode->bValid = bValid;
            break;
        }
    }
    if(i == NUM_OF_PICTURE_NODES)
    {
        loge("*** picNode is full when queue buffer");
        return -1;
    }

    list_add_tail(&newNode->list,&lc->picNodeListHead);
    pthread_cond_signal(&lc->cond_queue);
    return 0;
}

static int __LayerQueueBuffer(LayerCtrl* l, VideoPicture* pBuf, int bValid)
{
    int result;
    LayerContext* lc = (LayerContext*)l;
    pthread_mutex_lock(&lc->mutex);
    result = LayerQueueBufferLocked(l, pBuf, bValid);
    pthread_mutex_unlock(&lc->mutex);
    return result;
}

static int __LayerSetDisplayBufferCount(LayerCtrl* l, int nBufferCount)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

    logv("LayerSetBufferCount: count = %d",nBufferCount);

    lc->nGpuBufferCount = nBufferCount;

    if(lc->nGpuBufferCount > GPU_BUFFER_NUM)
        lc->nGpuBufferCount = GPU_BUFFER_NUM;

    return lc->nGpuBufferCount;
}

static int __LayerGetBufferNumHoldByGpu(LayerCtrl* l)
{
    (void)l;
    return GetConfigParamterInt("pic_4list_num", 3);
}

static int __LayerGetDisplayFPS(LayerCtrl* l)
{
    (void)l;
    return 60;
}

static void __LayerResetNativeWindow(LayerCtrl* l,void* pNativeWindow)
{
    logd("LayerResetNativeWindow : %p ",pNativeWindow);

    LayerContext* lc;
    VideoPicture mPicBufInfo;

    lc = (LayerContext*)l;
    lc->bLayerInitialized = 0;

    return ;
}

static VideoPicture* __LayerGetBufferOwnedByGpu(LayerCtrl* l)
{
    LayerContext* lc;
    VideoPicture* pPicture = NULL;
    lc = (LayerContext*)l;

    VPictureNode* nodePtr;
    if(!list_empty(&lc->picNodeListHead)){
        nodePtr = list_entry(lc->picNodeListHead.next, struct VPictureNode_t, list);
        list_del(lc->picNodeListHead.next);
        pPicture = nodePtr->pPicture;
        nodePtr->bUsed = 0;
    }
    return pPicture;
}

static int __LayerSetVideoWithTwoStreamFlag(LayerCtrl* l, int bVideoWithTwoStreamFlag)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

    logv("LayerSetIsTwoVideoStreamFlag, flag = %d",bVideoWithTwoStreamFlag);
    lc->bVideoWithTwoStreamFlag = bVideoWithTwoStreamFlag;

    return 0;
}

static int __LayerSetIsSoftDecoderFlag(LayerCtrl* l, int bIsSoftDecoderFlag)
{
    LayerContext* lc;

    lc = (LayerContext*)l;

    logv("LayerSetIsSoftDecoderFlag, flag = %d",bIsSoftDecoderFlag);
    lc->bIsSoftDecoderFlag = bIsSoftDecoderFlag;

    return 0;
}

//* Description: the picture buf is secure
static int __LayerSetSecure(LayerCtrl* l, int bSecure)
{
    logv("__LayerSetSecure, bSecure = %d", bSecure);
    //*TODO
    LayerContext* lc;

    lc = (LayerContext*)l;

    lc->bProtectFlag = bSecure;

    return 0;
}

static int __LayerReleaseBuffer(LayerCtrl* l, VideoPicture* pPicture)
{
    logv("***LayerReleaseBuffer");
    LayerContext* lc;

    lc = (LayerContext*)l;

    CdxIonPfree(pPicture->pData0);
    CdxIonPfree(pPicture->pMetaData);
    return 0;
}

static int __LayerControl(LayerCtrl* l, int cmd, void *para)
{
    LayerContext *lc = (LayerContext*)l;

    CDX_UNUSE(para);

    switch(cmd)
    {
		case CDX_LAYER_CMD_SET_HDR_INFO:
		{
			LayerSetHdrInfo(l, (const FbmBufInfo *)para);
			break;
		}
        case CDX_LAYER_CMD_SET_VIDEO_RESOLUTION_CHANGED:
        {
            lc->bResolutionChange = *(int*)para;
            logw("resolution changed %d", lc->bResolutionChange);
            break;
        }
        default:
            break;
    }
    return 0;
}

static LayerControlOpsT mLayerControlOps =
{
    release:                    __LayerRelease                   ,

    setSecureFlag:              __LayerSetSecure                 ,
    setDisplayBufferSize:       __LayerSetDisplayBufferSize      ,
    setDisplayBufferCount:      __LayerSetDisplayBufferCount     ,
    setDisplayRegion:           __LayerSetDisplayRegion          ,
    setScreenRegion:            __LayerSetScreenRegion           ,
    setDisplayPixelFormat:      __LayerSetDisplayPixelFormat     ,
    setVideoWithTwoStreamFlag:  __LayerSetVideoWithTwoStreamFlag ,
    setIsSoftDecoderFlag:       __LayerSetIsSoftDecoderFlag      ,
    setBufferTimeStamp:         __LayerSetBufferTimeStamp        ,

    resetNativeWindow :         __LayerResetNativeWindow         ,
    getBufferOwnedByGpu:        __LayerGetBufferOwnedByGpu       ,
    getDisplayFPS:              __LayerGetDisplayFPS             ,
    getBufferNumHoldByGpu:      __LayerGetBufferNumHoldByGpu     ,

    ctrlShowVideo :             __LayerCtrlShowVideo             ,
    ctrlHideVideo:              __LayerCtrlHideVideo             ,
    ctrlIsVideoShow:            __LayerCtrlIsVideoShow           ,
    ctrlHoldLastPicture :       __LayerCtrlHoldLastPicture       ,

    dequeueBuffer:              __LayerDequeueBuffer             ,
    queueBuffer:                __LayerQueueBuffer               ,
    releaseBuffer:              __LayerReleaseBuffer             ,
    reset:                      __LayerReset                     ,
    destroy:                    __LayerDestroy                   ,
    control:                    __LayerControl
};

static VPictureNode* GetLastPictureToRender(LayerContext *lc) {
    VPictureNode* node;

    if(list_empty(&lc->picNodeListHead)){
        return NULL;
    }
    node = list_entry(lc->picNodeListHead.prev, struct VPictureNode_t, list);//always get the lastest node to display
    if(node->bValid){
        node->dequeueable = false;// the node which is displaying cannot be dequeued from the list
        return node;
    }
    logd("no valid frame to render, repeating last");
    return NULL;

}

static VPictureNode* GetPictureToRender(LayerContext *lc) {
    VPictureNode* node;

    if(list_empty(&lc->picNodeListHead)){
        return NULL;
    }

    //1. get hdmi freq
    int disp_mode = getDisplayOutputMode(0, lc->fdDisplay);
    int disp_freq = getDispFreq(disp_mode);
    logv("display output mode is %d, freq is %dhz", disp_mode, disp_freq);
    int disp_interval;
    if (disp_freq > 0) {
        disp_interval = 1000/disp_freq;//ms
    } else {
        disp_interval = 33;//ms
        logw("we donot know disp freq, just guess 30HZ, here not enough test!");
    }

    list_for_each_entry(node, &lc->picNodeListHead, list){
        if (node->bValid) {
            if (lc->currentPicNode) {
                if (node->pPicture != NULL && lc->currentPicNode->pPicture != NULL) {
                    logv("trying to get picture to render, pts diff is %lldms, pts is %lldus",
                         node->pPicture->nPts/1000 - lc->currentPicNode->pPicture->nPts/1000, node->pPicture->nPts);
                    if (node->pPicture->nPts/1000 - lc->currentPicNode->pPicture->nPts/1000 > disp_interval/2+1) {
                        node->dequeueable = false;// the node which is displaying cannot be dequeued from the list
                        return node;
                    }
                }
            } else {//the first picture
                return node;
            }
        }
    }

    logv("no valid frame to render, repeating last");
    return NULL;
}

static void Render(LayerContext* lc) {
    //1. sync_wait the second lastest fence
    if (0 <= lc->preFenceFd) {
        int timeout = 3000; /* 3000 ms */
        timeout = sync_wait(lc->preFenceFd, timeout);
        if (0 > timeout) {
            loge("error: fence(%d) timeout!", lc->preFenceFd);
        }
        close(lc->preFenceFd);
        lc->prePicNode->dequeueable = true;
    }

    //2. get picture to render
    VPictureNode* node = GetPictureToRender(lc);
    while(node == NULL || node == lc->currentPicNode){
        pthread_cond_wait(&lc->cond_queue, &lc->mutex);
        if(!lc->running)
            return;
        node = GetPictureToRender(lc);
    }

    //3. acquire the new fence
    struct sync_info sync;
    createSyncPoint(0, lc->fdDisplay, &sync);
    lc->count = sync.count;
    if (lc->currentPicNode != NULL && node->pPicture != NULL && lc->currentPicNode->pPicture != NULL) {
        logv("send picture to Render, pts diff= %lldms",node->pPicture->nPts/1000 - lc->currentPicNode->pPicture->nPts/1000);
    }
    //4. set_layer_config
    if (SetLayerParam(lc, node->pPicture) != 0) {
        loge("can not render frame");
    }

    lc->preFenceFd = lc->currentFenceFd;
    lc->currentFenceFd = sync.fd;
    lc->prePicNode = lc->currentPicNode;
    lc->currentPicNode = node;

    pthread_cond_signal(&lc->cond_dequeue);
}

static void* RenderThread(void* arg) {
    LayerContext *lc = (LayerContext *)arg;

    while (lc->running) {
        pthread_mutex_lock(&lc->mutex);
        Render(lc);
        pthread_mutex_unlock(&lc->mutex);
    }
    close(lc->preFenceFd);
    close(lc->currentFenceFd);
    logd("render thread exit");
    return NULL;
}

LayerCtrl* LayerCreate_DE()
{
    LayerContext* lc;
    unsigned long args[4];
    struct disp_layer_info2 layerInfo;
    int screen_id;

    logd("cdx sink for tina h133, LayerCreate.");

    lc = (LayerContext*)malloc(sizeof(LayerContext));
    if(lc == NULL)
    {
        loge("malloc memory fail.");
        return NULL;
    }
    memset(lc, 0, sizeof(LayerContext));

    lc->fdDisplay = openDispDev();// open node path /dev/disp
    if(lc->fdDisplay < 1)
    {
        loge("open disp failed");
        free(lc);
        return NULL;
    }
    if (createSyncTimeline(0, lc->fdDisplay) < 0){
        logw("create sync time line failed");
        if(lc->fdDisplay>=0)
            close(lc->fdDisplay);
        lc->fdDisplay=-1;
        free(lc);
        return NULL;
    }

    lc->base.ops = &mLayerControlOps;
    lc->eDisplayPixelFormat = PIXEL_FORMAT_YV12;

    memset(&layerInfo, 0, sizeof(struct disp_layer_info2));
    char displayChars[4]  = "";
//    if (display_uci_get_config("region_scale", displayChars, sizeof(displayChars)) != -1)
//        lc->regionScale = atoi(displayChars);
//    else
    {
        lc->regionScale = 100;
        logw("get display region scale fail,use default value %d !",lc->regionScale);
    }

    //  get screen size.
    args[0] = 0;
    lc->nScreenWidth  = ioctl(lc->fdDisplay, DISP_GET_SCN_WIDTH, (void *)args);
    lc->nScreenHeight = ioctl(lc->fdDisplay, DISP_GET_SCN_HEIGHT,(void *)args);
    logd("screen:w %d, screen:h %d ,region scale value %d !", lc->nScreenWidth, lc->nScreenHeight,lc->regionScale);

    CdxIonOpen();
    lc->screen_x = 0;
    lc->screen_y = 0;
    lc->screen_w = 0;
    lc->screen_h = 0;

    // create and start rendering thread
    pthread_mutex_init(&lc->mutex, NULL);
    pthread_cond_init(&lc->cond_dequeue, NULL);
    pthread_cond_init(&lc->cond_queue, NULL);
    lc->running = true;
    lc->currentFenceFd = -1;
    lc->preFenceFd = -1;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    INIT_LIST_HEAD(&lc->picNodeListHead);
    pthread_create(&lc->renderThread, &attr, RenderThread, lc);

    return &lc->base;
}
