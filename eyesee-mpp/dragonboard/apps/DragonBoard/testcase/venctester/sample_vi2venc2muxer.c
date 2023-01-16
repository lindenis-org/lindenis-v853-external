/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/11/4
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "SampleVirVi2Venc2Muxer"

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "plat_log.h"
#include <mm_common.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_region.h>
#include "sample_vi2venc2muxer.h"
#include "sample_vi2venc2muxer_conf.h"

#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (64*1024)
//#define DOUBLE_ENCODER_FILE_OUT
#define ISP_RUN (1)

static SAMPLE_VI2VENC2MUXER_S *gpVi2Venc2MuxerData;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpVi2Venc2MuxerData)
    {
        cdx_sem_up(&gpVi2Venc2MuxerData->mSemExit);
    }
}

static int setOutputFileSync(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData, char* path, int64_t fallocateLength, int muxerId);


static ERRORTYPE InitVi2Venc2MuxerData(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData)
{
    if (pVi2Venc2MuxerData == NULL)
    {
        aloge("malloc struct fail");
        return FAILURE;
    }
    memset(pVi2Venc2MuxerData, 0, sizeof(SAMPLE_VI2VENC2MUXER_S));
    pVi2Venc2MuxerData->mMuxGrp = MM_INVALID_CHN;
    pVi2Venc2MuxerData->mVeChn = MM_INVALID_CHN;
    pVi2Venc2MuxerData->mViChn = MM_INVALID_CHN;
    pVi2Venc2MuxerData->mViDev = MM_INVALID_DEV;

    int i=0;
    for (i = 0; i < 2; i++)
    {
        INIT_LIST_HEAD(&pVi2Venc2MuxerData->mMuxerFileListArray[i]);
    }
    alogd("&pVi2Venc2MuxerData->mMuxerFileListArray[0][%p], &pVi2Venc2MuxerData->mMuxerFileListArray[1][%p]",
        &pVi2Venc2MuxerData->mMuxerFileListArray[0], &pVi2Venc2MuxerData->mMuxerFileListArray[1]);

    pVi2Venc2MuxerData->mCurrentState = REC_NOT_PREPARED;

    if (message_create(&pVi2Venc2MuxerData->mMsgQueue) < 0)
    {
        aloge("message create fail!");
        return FAILURE;
    }

    return SUCCESS;
}

static ERRORTYPE parseCmdLine(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData, int argc, char** argv)
{
    ERRORTYPE ret = FAILURE;

    if(argc <= 1)
    {
        alogd("use default config.");
        return SUCCESS;
    }
    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = SUCCESS;
              if (strlen(*argv) >= MAX_FILE_PATH_LEN)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pVi2Venc2MuxerData->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pVi2Venc2MuxerData->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_vi2venc2muxer.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData, const char *conf_path)
{
    int ret;
    char *ptr;
    pVi2Venc2MuxerData->mConfigPara.mDstFileMaxCnt = 3;
    pVi2Venc2MuxerData->mConfigPara.srcWidth = 1920;
    pVi2Venc2MuxerData->mConfigPara.srcHeight = 1080;
    pVi2Venc2MuxerData->mConfigPara.dstWidth = 1920;
    pVi2Venc2MuxerData->mConfigPara.dstHeight = 1080;

    pVi2Venc2MuxerData->mConfigPara.mViBufferNum = 4;

    pVi2Venc2MuxerData->mConfigPara.mMaxFileDuration = 0;
    pVi2Venc2MuxerData->mConfigPara.mTestDuration = 30;  // unit:s
    pVi2Venc2MuxerData->mConfigPara.mVideoFrameRate = 20; // unit:fps
    pVi2Venc2MuxerData->mConfigPara.mVideoBitRate = 15728640; // 1.5M
    pVi2Venc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    pVi2Venc2MuxerData->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
    pVi2Venc2MuxerData->mConfigPara.mVideoEncoderFmt = PT_H264;
    pVi2Venc2MuxerData->mConfigPara.mField = VIDEO_FIELD_FRAME;

    pVi2Venc2MuxerData->mConfigPara.mHorizonFlipFlag = FALSE;
    pVi2Venc2MuxerData->mConfigPara.mColor2Grey = FALSE;
    pVi2Venc2MuxerData->mConfigPara.m3DNR = 0;
    strcpy(pVi2Venc2MuxerData->mConfigPara.dstVideoFile, "/mnt/extsd/1080p.mp4");
    pVi2Venc2MuxerData->mConfigPara.mProductMode = (int)VENC_PRODUCT_IPC_MODE;
    pVi2Venc2MuxerData->mConfigPara.mSensorType = (int)VENC_ST_EN_WDR;
    pVi2Venc2MuxerData->mConfigPara.mKeyFrameInterval = 0;
    pVi2Venc2MuxerData->mConfigPara.mRcMode = 0;
    pVi2Venc2MuxerData->mConfigPara.mQp0 = 10;
    pVi2Venc2MuxerData->mConfigPara.mQp1 = 40;

    pVi2Venc2MuxerData->mConfigPara.mViDropFrameNum = 0;
    pVi2Venc2MuxerData->mConfigPara.mVencDropFrameNum = 0;
    pVi2Venc2MuxerData->mConfigPara.wdr_en = 0;
    pVi2Venc2MuxerData->mConfigPara.mEnMbQpLimit = 0;
    pVi2Venc2MuxerData->mConfigPara.mEnableGdc = 0;

    if(conf_path != NULL)
    {
        CONFPARSER_S mConf;

        ret = createConfParser(conf_path, &mConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }

        pVi2Venc2MuxerData->mConfigPara.mVippDev = GetConfParaInt(&mConf, CFG_VIPP_DEV_ID, 0);
        pVi2Venc2MuxerData->mConfigPara.mVeChn = GetConfParaInt(&mConf, CFG_VENC_CH_ID, 0);
        alogd("vippDev: %d, veChn: %d", pVi2Venc2MuxerData->mConfigPara.mVippDev, pVi2Venc2MuxerData->mConfigPara.mVeChn);

        pVi2Venc2MuxerData->mConfigPara.srcWidth = GetConfParaInt(&mConf, CFG_SRC_WIDTH, 0);
        pVi2Venc2MuxerData->mConfigPara.srcHeight = GetConfParaInt(&mConf, CFG_SRC_HEIGHT, 0);
        alogd("srcWidth: %d, srcHeight: %d", pVi2Venc2MuxerData->mConfigPara.srcWidth, pVi2Venc2MuxerData->mConfigPara.srcHeight);

        pVi2Venc2MuxerData->mConfigPara.dstWidth = GetConfParaInt(&mConf, CFG_DST_VIDEO_WIDTH, 0);
        pVi2Venc2MuxerData->mConfigPara.dstHeight = GetConfParaInt(&mConf, CFG_DST_VIDEO_HEIGHT, 0);
        alogd("dstWidth: %d, dstHeight: %d", pVi2Venc2MuxerData->mConfigPara.dstWidth, pVi2Venc2MuxerData->mConfigPara.dstHeight);

        ptr = (char *)GetConfParaString(&mConf, CFG_SRC_PIXFMT, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "nv21"))
            {
                pVi2Venc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
            }
            else if (!strcmp(ptr, "yv12"))
            {
                pVi2Venc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_PLANAR_420;
            }
            else if (!strcmp(ptr, "nv12"))
            {
                pVi2Venc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
            }
            else if (!strcmp(ptr, "yu12"))
            {
                pVi2Venc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_PLANAR_420;
            }
            /* aw compression format */
            else if (!strcmp(ptr, "aw_afbc"))
            {
                pVi2Venc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_AFBC;
            }
            else if (!strcmp(ptr, "aw_lbc_2_0x"))
            {
                pVi2Venc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
            }
            else if (!strcmp(ptr, "aw_lbc_2_5x"))
            {
                pVi2Venc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_5x"))
            {
                pVi2Venc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_0x"))
            {
                pVi2Venc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
            }
            /* aw_package_422 NOT support */
            // else if (!strcmp(ptr, "aw_package_422"))
            // {
            //     pVi2Venc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVYU_AW_PACKAGE_422;
            // }
            else
            {
                aloge("fatal error! wrong src pixfmt:%s", ptr);
                alogw("use the default pixfmt %d", pVi2Venc2MuxerData->mConfigPara.srcPixFmt);
            }
        }

        ptr	= (char *)GetConfParaString(&mConf, CFG_COLOR_SPACE, NULL);
        if (!strcmp(ptr, "jpeg"))
        {
            pVi2Venc2MuxerData->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
        }
        else if (!strcmp(ptr, "rec709"))
        {
            pVi2Venc2MuxerData->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709;
        }
        else if (!strcmp(ptr, "rec709_part_range"))
        {
            pVi2Venc2MuxerData->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
        }
        else
        {
            aloge("fatal error! wrong color space:%s", ptr);
            pVi2Venc2MuxerData->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
        }

        alogd("srcPixFmt=%d, ColorSpace=%d", pVi2Venc2MuxerData->mConfigPara.srcPixFmt, pVi2Venc2MuxerData->mConfigPara.mColorSpace);

        ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_FILE_STR, NULL);
        strcpy(pVi2Venc2MuxerData->mConfigPara.dstVideoFile, ptr);
        pVi2Venc2MuxerData->mConfigPara.mbAddRepairInfo = GetConfParaInt(&mConf, CFG_ADD_REPAIR_INFO, 0);
        pVi2Venc2MuxerData->mConfigPara.mMaxFrmsTagInterval = GetConfParaInt(&mConf, CFG_FRMSTAG_BACKUP_INTERVAL, 0);
        pVi2Venc2MuxerData->mConfigPara.mDstFileMaxCnt = GetConfParaInt(&mConf, CFG_DST_FILE_MAX_CNT, 0);
        pVi2Venc2MuxerData->mConfigPara.mVideoFrameRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_FRAMERATE, 0);
        pVi2Venc2MuxerData->mConfigPara.mViBufferNum = GetConfParaInt(&mConf, CFG_DST_VI_BUFFER_NUM, 0);
        pVi2Venc2MuxerData->mConfigPara.mVideoBitRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_BITRATE, 0);
        pVi2Venc2MuxerData->mConfigPara.mMaxFileDuration = GetConfParaInt(&mConf, CFG_DST_VIDEO_DURATION, 0);

        pVi2Venc2MuxerData->mConfigPara.mProductMode = GetConfParaInt(&mConf, CFG_PRODUCT_MODE, 0);
        pVi2Venc2MuxerData->mConfigPara.mSensorType = GetConfParaInt(&mConf, CFG_SENSOR_TYPE, 0);
        pVi2Venc2MuxerData->mConfigPara.mKeyFrameInterval = GetConfParaInt(&mConf, CFG_KEY_FRAME_INTERVAL, 0);
        pVi2Venc2MuxerData->mConfigPara.mRcMode = GetConfParaInt(&mConf, CFG_RC_MODE, 0);
        pVi2Venc2MuxerData->mConfigPara.mQp0 = GetConfParaInt(&mConf, CFG_RC_QP0, 0);
        pVi2Venc2MuxerData->mConfigPara.mQp1 = GetConfParaInt(&mConf, CFG_RC_QP1, 0);
        pVi2Venc2MuxerData->mConfigPara.mGopMode = GetConfParaInt(&mConf, CFG_GOP_MODE, 0);
        pVi2Venc2MuxerData->mConfigPara.mGopSize = GetConfParaInt(&mConf, CFG_GOP_SIZE, 0);
        pVi2Venc2MuxerData->mConfigPara.mAdvancedRef_Base = GetConfParaInt(&mConf, CFG_AdvancedRef_Base, 0);
        pVi2Venc2MuxerData->mConfigPara.mAdvancedRef_Enhance = GetConfParaInt(&mConf, CFG_AdvancedRef_Enhance, 0);
        pVi2Venc2MuxerData->mConfigPara.mAdvancedRef_RefBaseEn = GetConfParaInt(&mConf, CFG_AdvancedRef_RefBaseEn, 0);
        pVi2Venc2MuxerData->mConfigPara.mEnableFastEnc = GetConfParaInt(&mConf, CFG_FAST_ENC, 0);
        pVi2Venc2MuxerData->mConfigPara.mbEnableSmart = GetConfParaBoolean(&mConf, CFG_ENABLE_SMART, 0);
        pVi2Venc2MuxerData->mConfigPara.mSVCLayer = GetConfParaInt(&mConf, CFG_SVC_LAYER, 0);
        pVi2Venc2MuxerData->mConfigPara.mEncodeRotate = GetConfParaInt(&mConf, CFG_ENCODE_ROTATE, 0);

        ptr	= (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_ENCODER, NULL);
        if (!strcmp(ptr, "H.264"))
        {
            pVi2Venc2MuxerData->mConfigPara.mVideoEncoderFmt = PT_H264;
            alogd("H.264");
        }
        else if (!strcmp(ptr, "H.265"))
        {
            pVi2Venc2MuxerData->mConfigPara.mVideoEncoderFmt = PT_H265;
            alogd("H.265");
        }
        else if (!strcmp(ptr, "MJPEG"))
        {
            pVi2Venc2MuxerData->mConfigPara.mVideoEncoderFmt = PT_MJPEG;
            alogd("MJPEG");
        }
        else
        {
            aloge("error conf encoder type");
        }
        pVi2Venc2MuxerData->mConfigPara.mTestDuration = GetConfParaInt(&mConf, CFG_TEST_DURATION, 0);

        pVi2Venc2MuxerData->mConfigPara.mEncUseProfile = GetConfParaInt(&mConf, CFG_DST_ENCODE_PROFILE, 0);

        alogd("vipp:%d, frame rate:%d, bitrate:%d, video_duration=%d, test_time=%d, profile=%d", pVi2Venc2MuxerData->mConfigPara.mVippDev,\
            pVi2Venc2MuxerData->mConfigPara.mVideoFrameRate, pVi2Venc2MuxerData->mConfigPara.mVideoBitRate,\
            pVi2Venc2MuxerData->mConfigPara.mMaxFileDuration, pVi2Venc2MuxerData->mConfigPara.mTestDuration,\
            pVi2Venc2MuxerData->mConfigPara.mEncUseProfile);

        pVi2Venc2MuxerData->mConfigPara.mHorizonFlipFlag = GetConfParaInt(&mConf, CFG_MIRROR, 0);

        ptr	= (char *)GetConfParaString(&mConf, CFG_COLOR2GREY, NULL);
        if(!strcmp(ptr, "yes"))
        {
            pVi2Venc2MuxerData->mConfigPara.mColor2Grey = TRUE;
        }
        else
        {
            pVi2Venc2MuxerData->mConfigPara.mColor2Grey = FALSE;
        }

        pVi2Venc2MuxerData->mConfigPara.m3DNR = GetConfParaInt(&mConf, CFG_3DNR, 0);
        pVi2Venc2MuxerData->mConfigPara.mRoiNum = GetConfParaInt(&mConf, CFG_ROI_NUM, 0);
        pVi2Venc2MuxerData->mConfigPara.mRoiQp = GetConfParaInt(&mConf, CFG_ROI_QP, 0);
        pVi2Venc2MuxerData->mConfigPara.mRoiBgFrameRateEnable = GetConfParaBoolean(&mConf, CFG_ROI_BgFrameRateEnable, 0);
        pVi2Venc2MuxerData->mConfigPara.mRoiBgFrameRateAttenuation = GetConfParaInt(&mConf, CFG_ROI_BgFrameRateAttenuation, 0);
        pVi2Venc2MuxerData->mConfigPara.mIntraRefreshBlockNum = GetConfParaInt(&mConf, CFG_IntraRefresh_BlockNum, 0);
        pVi2Venc2MuxerData->mConfigPara.mOrlNum = GetConfParaInt(&mConf, CFG_ORL_NUM, 0);
        pVi2Venc2MuxerData->mConfigPara.mVbvBufferSize = GetConfParaInt(&mConf, CFG_vbvBufferSize, 0);
        pVi2Venc2MuxerData->mConfigPara.mVbvThreshSize = GetConfParaInt(&mConf, CFG_vbvThreshSize, 0);

        alogd("mirror:%d, Color2Grey:%d, 3DNR:%d, RoiNum:%d, RoiQp:%d, RoiBgFrameRate Enable:%d Attenuation:%d, IntraRefreshBlockNum:%d, OrlNum:%d"
            "VbvBufferSize:%d, VbvThreshSize:%d",
            pVi2Venc2MuxerData->mConfigPara.mHorizonFlipFlag, pVi2Venc2MuxerData->mConfigPara.mColor2Grey,
            pVi2Venc2MuxerData->mConfigPara.m3DNR, pVi2Venc2MuxerData->mConfigPara.mRoiNum, pVi2Venc2MuxerData->mConfigPara.mRoiQp,
            pVi2Venc2MuxerData->mConfigPara.mRoiBgFrameRateEnable, pVi2Venc2MuxerData->mConfigPara.mRoiBgFrameRateAttenuation,
            pVi2Venc2MuxerData->mConfigPara.mIntraRefreshBlockNum,
            pVi2Venc2MuxerData->mConfigPara.mOrlNum, pVi2Venc2MuxerData->mConfigPara.mVbvBufferSize,
            pVi2Venc2MuxerData->mConfigPara.mVbvThreshSize);

        pVi2Venc2MuxerData->mConfigPara.mCropEnable = GetConfParaInt(&mConf, CFG_CROP_ENABLE, 0);
        pVi2Venc2MuxerData->mConfigPara.mCropRectX = GetConfParaInt(&mConf, CFG_CROP_RECT_X, 0);
        pVi2Venc2MuxerData->mConfigPara.mCropRectY = GetConfParaInt(&mConf, CFG_CROP_RECT_Y, 0);
        pVi2Venc2MuxerData->mConfigPara.mCropRectWidth = GetConfParaInt(&mConf, CFG_CROP_RECT_WIDTH, 0);
        pVi2Venc2MuxerData->mConfigPara.mCropRectHeight = GetConfParaInt(&mConf, CFG_CROP_RECT_HEIGHT, 0);

        alogd("venc crop enable:%d, X:%d, Y:%d, Width:%d, Height:%d",
            pVi2Venc2MuxerData->mConfigPara.mCropEnable, pVi2Venc2MuxerData->mConfigPara.mCropRectX,
            pVi2Venc2MuxerData->mConfigPara.mCropRectY, pVi2Venc2MuxerData->mConfigPara.mCropRectWidth,
            pVi2Venc2MuxerData->mConfigPara.mCropRectHeight);

        pVi2Venc2MuxerData->mConfigPara.mVuiTimingInfoPresentFlag = GetConfParaInt(&mConf, CFG_vui_timing_info_present_flag, 0);
        alogd("VuiTimingInfoPresentFlag:%d", pVi2Venc2MuxerData->mConfigPara.mVuiTimingInfoPresentFlag);

        pVi2Venc2MuxerData->mConfigPara.mVeFreq = GetConfParaInt(&mConf, CFG_Ve_Freq, 0);
        alogd("mVeFreq:%d MHz", pVi2Venc2MuxerData->mConfigPara.mVeFreq);

        pVi2Venc2MuxerData->mConfigPara.mOnlineEnable = GetConfParaInt(&mConf, CFG_online_en, 0);
        pVi2Venc2MuxerData->mConfigPara.mOnlineShareBufNum = GetConfParaInt(&mConf, CFG_online_share_buf_num, 0);
        alogd("OnlineEnable: %d, OnlineShareBufNum: %d", pVi2Venc2MuxerData->mConfigPara.mOnlineEnable,
            pVi2Venc2MuxerData->mConfigPara.mOnlineShareBufNum);

        if (0 == pVi2Venc2MuxerData->mConfigPara.mOnlineEnable)
        {
            // venc drop frame only support offline.
            pVi2Venc2MuxerData->mConfigPara.mViDropFrameNum = GetConfParaInt(&mConf, CFG_DROP_FRAME_NUM, 0);
            alogd("ViDropFrameNum: %d", pVi2Venc2MuxerData->mConfigPara.mViDropFrameNum);
        }
        else
        {
            // venc drop frame support online and offline.
            pVi2Venc2MuxerData->mConfigPara.mVencDropFrameNum = GetConfParaInt(&mConf, CFG_DROP_FRAME_NUM, 0);
            alogd("VencDropFrameNum: %d", pVi2Venc2MuxerData->mConfigPara.mVencDropFrameNum);
        }

        pVi2Venc2MuxerData->mConfigPara.wdr_en = GetConfParaInt(&mConf, CFG_WDR_EN, 0);
        alogd("wdr_en: %d", pVi2Venc2MuxerData->mConfigPara.wdr_en);

        pVi2Venc2MuxerData->mConfigPara.mEnMbQpLimit = GetConfParaInt(&mConf, CFG_EnMbQpLimit, 0);
        alogd("EnMbQpLimit: %d", pVi2Venc2MuxerData->mConfigPara.mEnMbQpLimit);

        pVi2Venc2MuxerData->mConfigPara.mEnableGdc = GetConfParaInt(&mConf, CFG_EnableGdc, 0);
        alogd("EnableGdc: %d", pVi2Venc2MuxerData->mConfigPara.mEnableGdc);

        destroyConfParser(&mConf);
    }

    //parse dst directory form dst file path.
    char *pLastSlash = strrchr(pVi2Venc2MuxerData->mConfigPara.dstVideoFile, '/');
    if(pLastSlash != NULL)
    {
        int dirLen = pLastSlash-pVi2Venc2MuxerData->mConfigPara.dstVideoFile;
        strncpy(pVi2Venc2MuxerData->mDstDir, pVi2Venc2MuxerData->mConfigPara.dstVideoFile, dirLen);
        pVi2Venc2MuxerData->mDstDir[dirLen] = '\0';

        char *pFileName = pLastSlash+1;
        strcpy(pVi2Venc2MuxerData->mFirstFileName, pFileName);
    }
    else
    {
        strcpy(pVi2Venc2MuxerData->mDstDir, "");
        strcpy(pVi2Venc2MuxerData->mFirstFileName, pVi2Venc2MuxerData->mConfigPara.dstVideoFile);
    }
    return SUCCESS;
}

static unsigned long long GetNowTimeUs(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

static int getFileNameByCurTime(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData, char *pNameBuf)
{
#if 0
    sprintf(pNameBuf, "%s", "/mnt/extsd/sample_mux/");
    sprintf(pNameBuf, "%s%llud.mp4", pNameBuf, GetNowTimeUs());
#else
    static int file_cnt = 0;
    char strStemPath[MAX_FILE_PATH_LEN] = {0};
    int len = strlen(pVi2Venc2MuxerData->mConfigPara.dstVideoFile);
    char *ptr = pVi2Venc2MuxerData->mConfigPara.dstVideoFile;
    while (*(ptr+len-1) != '.')
    {
        len--;
    }

    ++file_cnt;
    strncpy(strStemPath, pVi2Venc2MuxerData->mConfigPara.dstVideoFile, len-1);
    sprintf(pNameBuf, "%s_%d.mp4", strStemPath, file_cnt);
#endif
    return 0;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S *)cookie;

    if(MOD_ID_VENC == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else if(MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                message_t stCmdMsg;
                Vi2Venc2Muxer_MessageData stMsgData;

                alogd("MuxerId[%d] record file done.", *(int*)pEventData);
                stMsgData.pVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S*)cookie;
                stCmdMsg.command = Rec_FileDone;
                stCmdMsg.para0 = *(int*)pEventData;
                stCmdMsg.mDataSize = sizeof(Vi2Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;

                putMessageWithData(&gpVi2Venc2MuxerData->mMsgQueue, &stCmdMsg);
                /*
                int ret;
                int muxerId = *(int*)pEventData;
                alogd("file done, mux_id=%d", muxerId);
                int idx=-1;
                if (muxerId == pVi2Venc2MuxerData->mMuxId[0])
                {
                    idx = 0;
                }
            #ifdef DOUBLE_ENCODER_FILE_OUT
                else if(muxerId == pVi2Venc2MuxerData->mMuxId[1])
                {
                    idx = 1;
                }
            #endif
                if (idx >= 0)
                {
                    int cnt = 0;
                    struct list_head *pList = NULL;
                    list_for_each(pList, &pVi2Venc2MuxerData->mMuxerFileListArray[idx]){cnt++;}
                    FilePathNode *pNode = NULL;
                    while(cnt > pVi2Venc2MuxerData->mConfigPara.mDstFileMaxCnt)
                    {
                        pNode = list_first_entry(&pVi2Venc2MuxerData->mMuxerFileListArray[idx], FilePathNode, mList);
                        if ((ret = remove(pNode->strFilePath)) != 0)
                        {
                            aloge("fatal error! delete file[%s] failed:%s", pNode->strFilePath, strerror(errno));
                        }
                        else
                        {
                            alogd("delete file[%s] success", pNode->strFilePath);
                        }
                        cnt--;
                        list_del(&pNode->mList);
                        free(pNode);
                    }
                }
                */
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                message_t stCmdMsg;
                Vi2Venc2Muxer_MessageData stMsgData;

                alogd("MuxerId[%d] need next fd.", *(int*)pEventData);
                stMsgData.pVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S*)cookie;
                stCmdMsg.command = Rec_NeedSetNextFd;
                stCmdMsg.para0 = *(int*)pEventData;
                stCmdMsg.mDataSize = sizeof(Vi2Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;

                putMessageWithData(&gpVi2Venc2MuxerData->mMsgQueue, &stCmdMsg);
                /*
                int muxerId = *(int*)pEventData;
                SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S *)cookie;
                char fileName[MAX_FILE_PATH_LEN] = {0};

                if (muxerId == pVi2Venc2MuxerData->mMuxId[0])
                {
                    getFileNameByCurTime(pVi2Venc2MuxerData, fileName);
                    FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                    memset(pFilePathNode, 0, sizeof(FilePathNode));
                    strncpy(pFilePathNode->strFilePath, fileName, MAX_FILE_PATH_LEN-1);
                    list_add_tail(&pFilePathNode->mList, &pVi2Venc2MuxerData->mMuxerFileListArray[0]);
                }
            #ifdef DOUBLE_ENCODER_FILE_OUT
                else if(muxerId == pVi2Venc2MuxerData->mMuxId[1])
                {
                    //strcpy(fileName, "/mnt/extsd/sample_vi2venc2muxer/");
                    static int cnt = 0;
                    cnt++;
                    sprintf(fileName, "/mnt/extsd/sample_vi2venc2muxer/%d.ts", cnt);
                    FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                    memset(pFilePathNode, 0, sizeof(FilePathNode));
                    strncpy(pFilePathNode->strFilePath, fileName, MAX_FILE_PATH_LEN-1);
                    list_add_tail(&pFilePathNode->mList, &pVi2Venc2MuxerData->mMuxerFileListArray[1]);
                }
            #endif
                alogd("muxId[%d] set next fd, filepath=%s", muxerId, fileName);
                setOutputFileSync(pVi2Venc2MuxerData, fileName, 0, muxerId);
                */
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("mux bs frame available");
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return SUCCESS;
}

static ERRORTYPE configMuxGrpAttr(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData)
{
    memset(&pVi2Venc2MuxerData->mMuxGrpAttr, 0, sizeof(MUX_GRP_ATTR_S));

    pVi2Venc2MuxerData->mMuxGrpAttr.mVideoAttrValidNum = 1;
    pVi2Venc2MuxerData->mMuxGrpAttr.mVideoAttr[0].mVideoEncodeType = pVi2Venc2MuxerData->mConfigPara.mVideoEncoderFmt;
    pVi2Venc2MuxerData->mMuxGrpAttr.mVideoAttr[0].mWidth = pVi2Venc2MuxerData->mConfigPara.dstWidth;
    pVi2Venc2MuxerData->mMuxGrpAttr.mVideoAttr[0].mHeight = pVi2Venc2MuxerData->mConfigPara.dstHeight;
    pVi2Venc2MuxerData->mMuxGrpAttr.mVideoAttr[0].mVideoFrmRate = pVi2Venc2MuxerData->mConfigPara.mVideoFrameRate*1000;
    //pVi2Venc2MuxerData->mMuxGrpAttr.mMaxKeyInterval =
    pVi2Venc2MuxerData->mMuxGrpAttr.mVideoAttr[0].mVeChn = pVi2Venc2MuxerData->mVeChn;
    pVi2Venc2MuxerData->mMuxGrpAttr.mAudioEncodeType = PT_MAX;

    return SUCCESS;
}

static ERRORTYPE createMuxGrp(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configMuxGrpAttr(pVi2Venc2MuxerData);
    pVi2Venc2MuxerData->mMuxGrp = 0;
    while (pVi2Venc2MuxerData->mMuxGrp < MUX_MAX_GRP_NUM)
    {
        ret = AW_MPI_MUX_CreateGrp(pVi2Venc2MuxerData->mMuxGrp, &pVi2Venc2MuxerData->mMuxGrpAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create mux group[%d] success!", pVi2Venc2MuxerData->mMuxGrp);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            alogd("mux group[%d] is exist, find next!", pVi2Venc2MuxerData->mMuxGrp);
            pVi2Venc2MuxerData->mMuxGrp++;
        }
        else
        {
            alogd("create mux group[%d] ret[0x%x], find next!", pVi2Venc2MuxerData->mMuxGrp, ret);
            pVi2Venc2MuxerData->mMuxGrp++;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pVi2Venc2MuxerData->mMuxGrp = MM_INVALID_CHN;
        aloge("fatal error! create mux group fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pVi2Venc2MuxerData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(pVi2Venc2MuxerData->mMuxGrp, &cbInfo);
        return SUCCESS;
    }
}

static int addOutputFormatAndOutputSink_l(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData, OUTSINKINFO_S *pSinkInfo)
{
    int retMuxerId = -1;
    MUX_CHN_INFO_S *pEntry, *pTmp;

    alogd("fmt:0x%x, fd:%d, FallocateLen:%d, callback_out_flag:%d", pSinkInfo->mOutputFormat, pSinkInfo->mOutputFd, pSinkInfo->mFallocateLen, pSinkInfo->mCallbackOutFlag);
    if(pSinkInfo->mOutputFd >= 0 && TRUE == pSinkInfo->mCallbackOutFlag)
    {
        aloge("fatal error! one muxer cannot support two sink methods!");
        return -1;
    }

    //find if the same output_format sinkInfo exist or callback out stream is exist.
    pthread_mutex_lock(&pVi2Venc2MuxerData->mMuxChnListLock);
    if (!list_empty(&pVi2Venc2MuxerData->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pVi2Venc2MuxerData->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFormat == pSinkInfo->mOutputFormat)
            {
                alogd("Be careful! same outputForamt[0x%x] exist in array", pSinkInfo->mOutputFormat);
            }
//            if (pEntry->mSinkInfo.mCallbackOutFlag == pSinkInfo->mCallbackOutFlag)
//            {
//                aloge("fatal error! only support one callback out stream");
//            }
        }
    }
    pthread_mutex_unlock(&pVi2Venc2MuxerData->mMuxChnListLock);

    MUX_CHN_INFO_S *p_node = (MUX_CHN_INFO_S *)malloc(sizeof(MUX_CHN_INFO_S));
    if (p_node == NULL)
    {
        aloge("alloc mux chn info node fail");
        return -1;
    }

    memset(p_node, 0, sizeof(MUX_CHN_INFO_S));
    p_node->mSinkInfo.mMuxerId = pVi2Venc2MuxerData->mMuxerIdCounter;
    p_node->mSinkInfo.mOutputFormat = pSinkInfo->mOutputFormat;
    if (pSinkInfo->mOutputFd > 0)
    {
        p_node->mSinkInfo.mOutputFd = dup(pSinkInfo->mOutputFd);
    }
    else
    {
        p_node->mSinkInfo.mOutputFd = -1;
    }
    p_node->mSinkInfo.mFallocateLen = pSinkInfo->mFallocateLen;
    p_node->mSinkInfo.mCallbackOutFlag = pSinkInfo->mCallbackOutFlag;

    p_node->mMuxChnAttr.mMuxerId = p_node->mSinkInfo.mMuxerId;
    p_node->mMuxChnAttr.mMediaFileFormat = p_node->mSinkInfo.mOutputFormat;
    p_node->mMuxChnAttr.mMaxFileDuration = pVi2Venc2MuxerData->mConfigPara.mMaxFileDuration *1000;
    p_node->mMuxChnAttr.mFallocateLen = p_node->mSinkInfo.mFallocateLen;
    p_node->mMuxChnAttr.mCallbackOutFlag = p_node->mSinkInfo.mCallbackOutFlag;
    p_node->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    p_node->mMuxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
    p_node->mMuxChnAttr.mAddRepairInfo = pVi2Venc2MuxerData->mConfigPara.mbAddRepairInfo;
    p_node->mMuxChnAttr.mMaxFrmsTagInterval = pVi2Venc2MuxerData->mConfigPara.mMaxFrmsTagInterval;

    p_node->mMuxChn = MM_INVALID_CHN;

    if ((pVi2Venc2MuxerData->mCurrentState == REC_PREPARED) || (pVi2Venc2MuxerData->mCurrentState == REC_RECORDING))
    {
        ERRORTYPE ret;
        BOOL nSuccessFlag = FALSE;
        MUX_CHN nMuxChn = 0;
        while (nMuxChn < MUX_MAX_CHN_NUM)
        {
            ret = AW_MPI_MUX_CreateChn(pVi2Venc2MuxerData->mMuxGrp, nMuxChn, &p_node->mMuxChnAttr, p_node->mSinkInfo.mOutputFd);
            if (SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pVi2Venc2MuxerData->mMuxGrp, nMuxChn, p_node->mMuxChnAttr.mMuxerId);
                break;
            }
            else if (ERR_MUX_EXIST == ret)
            {
                alogd("mux group[%d] channel[%d] is exist, find next!", pVi2Venc2MuxerData->mMuxGrp, nMuxChn);
                nMuxChn++;
            }
            else
            {
                aloge("fatal error! create mux group[%d] channel[%d] fail ret[0x%x], find next!", pVi2Venc2MuxerData->mMuxGrp, nMuxChn, ret);
                nMuxChn++;
            }
        }

        if (nSuccessFlag)
        {
            retMuxerId = p_node->mSinkInfo.mMuxerId;
            p_node->mMuxChn = nMuxChn;
            pVi2Venc2MuxerData->mMuxerIdCounter++;
        }
        else
        {
            aloge("fatal error! create mux group[%d] channel fail!", pVi2Venc2MuxerData->mMuxGrp);
            if (p_node->mSinkInfo.mOutputFd >= 0)
            {
                close(p_node->mSinkInfo.mOutputFd);
                p_node->mSinkInfo.mOutputFd = -1;
            }

            retMuxerId = -1;
        }

        pthread_mutex_lock(&pVi2Venc2MuxerData->mMuxChnListLock);
        list_add_tail(&p_node->mList, &pVi2Venc2MuxerData->mMuxChnList);
        pthread_mutex_unlock(&pVi2Venc2MuxerData->mMuxChnListLock);
    }
    else
    {
        retMuxerId = p_node->mSinkInfo.mMuxerId;
        pVi2Venc2MuxerData->mMuxerIdCounter++;
        pthread_mutex_lock(&pVi2Venc2MuxerData->mMuxChnListLock);
        list_add_tail(&p_node->mList, &pVi2Venc2MuxerData->mMuxChnList);
        pthread_mutex_unlock(&pVi2Venc2MuxerData->mMuxChnListLock);
    }

    return retMuxerId;
}

static int addOutputFormatAndOutputSink(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData, char* path, MEDIA_FILE_FORMAT_E format)
{
    int muxerId = -1;
    OUTSINKINFO_S sinkInfo = {0};

    if (path != NULL)
    {
        sinkInfo.mFallocateLen = 0;
        sinkInfo.mCallbackOutFlag = FALSE;
        sinkInfo.mOutputFormat = format;
        sinkInfo.mOutputFd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (sinkInfo.mOutputFd < 0)
        {
            aloge("Failed to open %s", path);
            return -1;
        }

        muxerId = addOutputFormatAndOutputSink_l(pVi2Venc2MuxerData, &sinkInfo);
        close(sinkInfo.mOutputFd);
    }

    return muxerId;
}

static int setOutputFileSync_l(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData, int fd, int64_t fallocateLength, int muxerId)
{
    MUX_CHN_INFO_S *pEntry, *pTmp;

    if (pVi2Venc2MuxerData->mCurrentState != REC_RECORDING)
    {
        aloge("must be in recording state");
        return -1;
    }

    alogv("setOutputFileSync fd=%d", fd);
    if (fd < 0)
    {
        aloge("Invalid parameter");
        return -1;
    }

    MUX_CHN muxChn = MM_INVALID_CHN;
    pthread_mutex_lock(&pVi2Venc2MuxerData->mMuxChnListLock);
    if (!list_empty(&pVi2Venc2MuxerData->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pVi2Venc2MuxerData->mMuxChnList, mList)
        {
            if (pEntry->mMuxChnAttr.mMuxerId == muxerId)
            {
                muxChn = pEntry->mMuxChn;
                break;
            }
        }
    }
    pthread_mutex_unlock(&pVi2Venc2MuxerData->mMuxChnListLock);

    if (muxChn != MM_INVALID_CHN)
    {
        alogd("switch fd");
        AW_MPI_MUX_SwitchFd(pVi2Venc2MuxerData->mMuxGrp, muxChn, fd, fallocateLength);
        return 0;
    }
    else
    {
        aloge("fatal error! can't find muxChn which muxerId[%d]", muxerId);
        return -1;
    }
}

static int setOutputFileSync(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData, char* path, int64_t fallocateLength, int muxerId)
{
    int ret;

    if (pVi2Venc2MuxerData->mCurrentState != REC_RECORDING)
    {
        aloge("not in recording state");
        return -1;
    }

    if(path != NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (fd < 0)
        {
            aloge("fail to open %s", path);
            return -1;
        }
        ret = setOutputFileSync_l(pVi2Venc2MuxerData, fd, fallocateLength, muxerId);
        close(fd);

        return ret;
    }
    else
    {
        return -1;
    }
}

static inline unsigned int map_H264_UserSet2Profile(int val)
{
    unsigned int profile = (unsigned int)H264_PROFILE_HIGH;
    switch (val)
    {
    case 0:
        profile = (unsigned int)H264_PROFILE_BASE;
        break;

    case 1:
        profile = (unsigned int)H264_PROFILE_MAIN;
        break;

    case 2:
        profile = (unsigned int)H264_PROFILE_HIGH;
        break;

    default:
        break;
    }

    return profile;
}

static inline unsigned int map_H265_UserSet2Profile(int val)
{
    unsigned int profile = H265_PROFILE_MAIN;
    switch (val)
    {
    case 0:
        profile = (unsigned int)H265_PROFILE_MAIN;
        break;

    case 1:
        profile = (unsigned int)H265_PROFILE_MAIN10;
        break;

    case 2:
        profile = (unsigned int)H265_PROFILE_STI11;
        break;

    default:
        break;
    }
    return profile;
}

static void initGdcParam(sGdcParam *pGdcParam)
{
    pGdcParam->bGDC_en = 1;
    pGdcParam->eWarpMode = Gdc_Warp_LDC;
    pGdcParam->eMountMode = Gdc_Mount_Wall;
    pGdcParam->bMirror = 0;

    pGdcParam->fx = 2417.19;
    pGdcParam->fy = 2408.43;
    pGdcParam->cx = 1631.50;
    pGdcParam->cy = 1223.50;
    pGdcParam->fx_scale = 2161.82;
    pGdcParam->fy_scale = 2153.99;
    pGdcParam->cx_scale = 1631.50;
    pGdcParam->cy_scale = 1223.50;

    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    pGdcParam->distCoef_wide_ra[0] = -0.3849;
    pGdcParam->distCoef_wide_ra[1] = 0.1567;
    pGdcParam->distCoef_wide_ra[2] = -0.0030;
    pGdcParam->distCoef_wide_ta[0] = -0.00005;
    pGdcParam->distCoef_wide_ta[1] = 0.0016;

    pGdcParam->distCoef_fish_k[0]  = -0.0024;
    pGdcParam->distCoef_fish_k[1]  = 0.141;
    pGdcParam->distCoef_fish_k[2]  = -0.3;
    pGdcParam->distCoef_fish_k[3]  = 0.2328;

    pGdcParam->centerOffsetX         =      -255;  //[-255,0]
    pGdcParam->centerOffsetY         =      0;     //[-255,0]
    pGdcParam->rotateAngle           =      0;     //[0,360]
    pGdcParam->radialDistortCoef     =      0;     //[-255,255]
    pGdcParam->trapezoidDistortCoef  =      0;     //[-255,255]
    pGdcParam->fanDistortCoef        =      0;     //[-255,255]
    pGdcParam->pan                   =      0;     //pano360:[0,360]; others:[-90,90]
    pGdcParam->tilt                  =      0;     //[-90,90]
    pGdcParam->zoomH                 =      100;   //[0,100]
    pGdcParam->zoomV                 =      100;   //[0,100]
    pGdcParam->scale                 =      100;   //[0,100]
    pGdcParam->innerRadius           =      0;     //[0,width/2]
    pGdcParam->roll                  =      0;     //[-90,90]
    pGdcParam->pitch                 =      0;     //[-90,90]
    pGdcParam->yaw                   =      0;     //[-90,90]

    pGdcParam->perspFunc             =    Gdc_Persp_Only;
    pGdcParam->perspectiveProjMat[0] =    1.0;
    pGdcParam->perspectiveProjMat[1] =    0.0;
    pGdcParam->perspectiveProjMat[2] =    0.0;
    pGdcParam->perspectiveProjMat[3] =    0.0;
    pGdcParam->perspectiveProjMat[4] =    1.0;
    pGdcParam->perspectiveProjMat[5] =    0.0;
    pGdcParam->perspectiveProjMat[6] =    0.0;
    pGdcParam->perspectiveProjMat[7] =    0.0;
    pGdcParam->perspectiveProjMat[8] =    1.0;

    pGdcParam->mountHeight           =      0.85; //meters
    pGdcParam->roiDist_ahead         =      4.5;  //meters
    pGdcParam->roiDist_left          =     -1.5;  //meters
    pGdcParam->roiDist_right         =      1.5;  //meters
    pGdcParam->roiDist_bottom        =      0.65; //meters

    pGdcParam->peaking_en            =      1;    //0/1
    pGdcParam->peaking_clamp         =      1;    //0/1
    pGdcParam->peak_m                =     16;    //[0,63]
    pGdcParam->th_strong_edge        =      6;    //[0,15]
    pGdcParam->peak_weights_strength =      2;    //[0,15]

    if (pGdcParam->eWarpMode == Gdc_Warp_LDC)
    {
        pGdcParam->birdsImg_width    = 768;
        pGdcParam->birdsImg_height   = 1080;
    }
}

static ERRORTYPE configVencChnAttr(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData)
{
    memset(&pVi2Venc2MuxerData->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    if (pVi2Venc2MuxerData->mConfigPara.mOnlineEnable)
    {
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.mOnlineEnable = 1;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.mOnlineShareBufNum = pVi2Venc2MuxerData->mConfigPara.mOnlineShareBufNum;
    }
    pVi2Venc2MuxerData->mVencChnAttr.VeAttr.Type = pVi2Venc2MuxerData->mConfigPara.mVideoEncoderFmt;
    pVi2Venc2MuxerData->mVencChnAttr.VeAttr.MaxKeyInterval = pVi2Venc2MuxerData->mConfigPara.mKeyFrameInterval;
    pVi2Venc2MuxerData->mVencChnAttr.VeAttr.SrcPicWidth  = pVi2Venc2MuxerData->mConfigPara.srcWidth;
    pVi2Venc2MuxerData->mVencChnAttr.VeAttr.SrcPicHeight = pVi2Venc2MuxerData->mConfigPara.srcHeight;
    pVi2Venc2MuxerData->mVencChnAttr.VeAttr.Field = pVi2Venc2MuxerData->mConfigPara.mField;
    pVi2Venc2MuxerData->mVencChnAttr.VeAttr.PixelFormat = pVi2Venc2MuxerData->mConfigPara.srcPixFmt;
    pVi2Venc2MuxerData->mVencChnAttr.VeAttr.mColorSpace = pVi2Venc2MuxerData->mConfigPara.mColorSpace;
    alogd("pixfmt:0x%x, colorSpace:0x%x", pVi2Venc2MuxerData->mVencChnAttr.VeAttr.PixelFormat, pVi2Venc2MuxerData->mVencChnAttr.VeAttr.mColorSpace);
    pVi2Venc2MuxerData->mVencChnAttr.VeAttr.mDropFrameNum = pVi2Venc2MuxerData->mConfigPara.mVencDropFrameNum;
    alogd("DropFrameNum:%d", pVi2Venc2MuxerData->mVencChnAttr.VeAttr.mDropFrameNum);
    switch(pVi2Venc2MuxerData->mConfigPara.mEncodeRotate)
    {
        case 90:
            pVi2Venc2MuxerData->mVencChnAttr.VeAttr.Rotate = ROTATE_90;
            break;
        case 180:
            pVi2Venc2MuxerData->mVencChnAttr.VeAttr.Rotate = ROTATE_180;
            break;
        case 270:
            pVi2Venc2MuxerData->mVencChnAttr.VeAttr.Rotate = ROTATE_270;
            break;
        default:
            pVi2Venc2MuxerData->mVencChnAttr.VeAttr.Rotate = ROTATE_NONE;
            break;
    }

    pVi2Venc2MuxerData->mVencRcParam.product_mode = pVi2Venc2MuxerData->mConfigPara.mProductMode;
    pVi2Venc2MuxerData->mVencRcParam.sensor_type = pVi2Venc2MuxerData->mConfigPara.mSensorType;
    if (PT_H264 == pVi2Venc2MuxerData->mVencChnAttr.VeAttr.Type)
    {
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.BufSize = pVi2Venc2MuxerData->mConfigPara.mVbvBufferSize;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.mThreshSize = pVi2Venc2MuxerData->mConfigPara.mVbvThreshSize;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.Profile = map_H264_UserSet2Profile(pVi2Venc2MuxerData->mConfigPara.mEncUseProfile);
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51; //H264_LEVEL_32
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pVi2Venc2MuxerData->mConfigPara.dstWidth;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pVi2Venc2MuxerData->mConfigPara.dstHeight;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        switch (pVi2Venc2MuxerData->mConfigPara.mRcMode)
        {
        case 1:
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Vbr.mMinQp = pVi2Venc2MuxerData->mConfigPara.mQp0;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Vbr.mMaxQp = pVi2Venc2MuxerData->mConfigPara.mQp1;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pVi2Venc2MuxerData->mConfigPara.mVideoBitRate;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Vbr.mMaxPqp = 50;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Vbr.mMinPqp = 10;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Vbr.mQpInit = 30;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Vbr.mbEnMbQpLimit = pVi2Venc2MuxerData->mConfigPara.mEnMbQpLimit;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Vbr.mMovingTh = 20;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Vbr.mQuality = 8;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Vbr.mIFrmBitsCoef = 15;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
            break;
        case 2:
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = pVi2Venc2MuxerData->mConfigPara.mQp0;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = pVi2Venc2MuxerData->mConfigPara.mQp1;
            break;
        case 3:
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264ABR;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxBitRate = pVi2Venc2MuxerData->mConfigPara.mVideoBitRate;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Abr.mRatioChangeQp = 85;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Abr.mQuality = 8;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Abr.mMinIQp = 20;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Abr.mMinQp = pVi2Venc2MuxerData->mConfigPara.mQp0;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxQp = pVi2Venc2MuxerData->mConfigPara.mQp1;
            break;
        case 0:
        default:
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pVi2Venc2MuxerData->mConfigPara.mVideoBitRate;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Cbr.mMaxQp = pVi2Venc2MuxerData->mConfigPara.mQp1;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Cbr.mMinQp = pVi2Venc2MuxerData->mConfigPara.mQp0;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Cbr.mMaxPqp = 50;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Cbr.mMinPqp = 10;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Cbr.mQpInit = 30;
            pVi2Venc2MuxerData->mVencRcParam.ParamH264Cbr.mbEnMbQpLimit = pVi2Venc2MuxerData->mConfigPara.mEnMbQpLimit;
            break;
        }
        if (pVi2Venc2MuxerData->mConfigPara.mEnableFastEnc)
        {
            pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.FastEncFlag = TRUE;
        }
    }
    else if (PT_H265 == pVi2Venc2MuxerData->mVencChnAttr.VeAttr.Type)
    {
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mBufSize = pVi2Venc2MuxerData->mConfigPara.mVbvBufferSize;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mThreshSize = pVi2Venc2MuxerData->mConfigPara.mVbvThreshSize;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mProfile = map_H265_UserSet2Profile(pVi2Venc2MuxerData->mConfigPara.mEncUseProfile);
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pVi2Venc2MuxerData->mConfigPara.dstWidth;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pVi2Venc2MuxerData->mConfigPara.dstHeight;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        switch (pVi2Venc2MuxerData->mConfigPara.mRcMode)
        {
        case 1:
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Vbr.mMinQp = pVi2Venc2MuxerData->mConfigPara.mQp0;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Vbr.mMaxQp = pVi2Venc2MuxerData->mConfigPara.mQp1;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pVi2Venc2MuxerData->mConfigPara.mVideoBitRate;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Vbr.mMaxPqp = 50;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Vbr.mMinPqp = 10;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Vbr.mQpInit = 30;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Vbr.mbEnMbQpLimit = pVi2Venc2MuxerData->mConfigPara.mEnMbQpLimit;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Vbr.mMovingTh = 20;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Vbr.mQuality = 5;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Vbr.mIFrmBitsCoef = 15;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
            break;
        case 2:
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = pVi2Venc2MuxerData->mConfigPara.mQp0;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = pVi2Venc2MuxerData->mConfigPara.mQp1;
            break;
        case 3:
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265ABR;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxBitRate = pVi2Venc2MuxerData->mConfigPara.mVideoBitRate;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Abr.mRatioChangeQp = 85;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Abr.mQuality = 8;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Abr.mMinIQp = 20;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Abr.mMinQp = pVi2Venc2MuxerData->mConfigPara.mQp0;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxQp = pVi2Venc2MuxerData->mConfigPara.mQp1;
            break;
        case 0:
        default:
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pVi2Venc2MuxerData->mConfigPara.mVideoBitRate;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Cbr.mMaxQp = pVi2Venc2MuxerData->mConfigPara.mQp1;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Cbr.mMinQp = pVi2Venc2MuxerData->mConfigPara.mQp0;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Cbr.mMaxPqp = 50;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Cbr.mMinPqp = 10;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Cbr.mQpInit = 30;
            pVi2Venc2MuxerData->mVencRcParam.ParamH265Cbr.mbEnMbQpLimit = pVi2Venc2MuxerData->mConfigPara.mEnMbQpLimit;
            break;
        }
        if (pVi2Venc2MuxerData->mConfigPara.mEnableFastEnc)
        {
            pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mFastEncFlag = TRUE;
        }
    }
    else if (PT_MJPEG == pVi2Venc2MuxerData->mVencChnAttr.VeAttr.Type)
    {
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrMjpeg.mBufSize = pVi2Venc2MuxerData->mConfigPara.mVbvBufferSize;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pVi2Venc2MuxerData->mConfigPara.dstWidth;
        pVi2Venc2MuxerData->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pVi2Venc2MuxerData->mConfigPara.dstHeight;
        switch (pVi2Venc2MuxerData->mConfigPara.mRcMode)
        {
        case 0:
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pVi2Venc2MuxerData->mConfigPara.mVideoBitRate;
            break;
        case 1:
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = 40;
            break;
        case 2:
        case 3:
            aloge("not support! use default cbr mode");
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            break;
        default:
            pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            break;
        }
        pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pVi2Venc2MuxerData->mConfigPara.mVideoBitRate;
    }

    alogd("venc set Rcmode=%d", pVi2Venc2MuxerData->mVencChnAttr.RcAttr.mRcMode);

    if(0 == pVi2Venc2MuxerData->mConfigPara.mGopMode)
    {
        pVi2Venc2MuxerData->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    }
    else if(1 == pVi2Venc2MuxerData->mConfigPara.mGopMode)
    {
        pVi2Venc2MuxerData->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_DUALP;
    }
    else if(2 == pVi2Venc2MuxerData->mConfigPara.mGopMode)
    {
        pVi2Venc2MuxerData->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_SMARTP;
        pVi2Venc2MuxerData->mVencChnAttr.GopAttr.stSmartP.mVirtualIFrameInterval = 15;
    }
    pVi2Venc2MuxerData->mVencChnAttr.GopAttr.mGopSize = pVi2Venc2MuxerData->mConfigPara.mGopSize;

    if (pVi2Venc2MuxerData->mConfigPara.mEnableGdc)
    {
        alogd("enable GDC and init GDC params");
        initGdcParam(&pVi2Venc2MuxerData->mVencChnAttr.GdcAttr);
    }

    return SUCCESS;
}

static ERRORTYPE createVencChn(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configVencChnAttr(pVi2Venc2MuxerData);
    if (pVi2Venc2MuxerData->mConfigPara.mOnlineEnable)
    {
        pVi2Venc2MuxerData->mVeChn = 0;
        alogd("online: only vipp0 & Vechn0 support online.");
    }
    else
    {
        pVi2Venc2MuxerData->mVeChn = pVi2Venc2MuxerData->mConfigPara.mVeChn;
    }

    while (pVi2Venc2MuxerData->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pVi2Venc2MuxerData->mVeChn, &pVi2Venc2MuxerData->mVencChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pVi2Venc2MuxerData->mVeChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", pVi2Venc2MuxerData->mVeChn);
            pVi2Venc2MuxerData->mVeChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!", pVi2Venc2MuxerData->mVeChn, ret);
            pVi2Venc2MuxerData->mVeChn++;
        }
    }

    if (nSuccessFlag == FALSE)
    {
        pVi2Venc2MuxerData->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return FAILURE;
    }
    else
    {
        int nFreq = pVi2Venc2MuxerData->mConfigPara.mVeFreq;
        if (0 != nFreq)
        {
            alogd("set VE freq %d MHz", nFreq);
            AW_MPI_VENC_SetVEFreq(pVi2Venc2MuxerData->mVeChn, nFreq);
        }
        AW_MPI_VENC_SetRcParam(pVi2Venc2MuxerData->mVeChn, &pVi2Venc2MuxerData->mVencRcParam);
        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = stFrameRate.DstFrmRate = pVi2Venc2MuxerData->mConfigPara.mVideoFrameRate;
        alogd("set venc framerate:%d", stFrameRate.DstFrmRate);
        AW_MPI_VENC_SetFrameRate(pVi2Venc2MuxerData->mVeChn, &stFrameRate);

        VENC_PARAM_REF_S stRefParam;
        memset(&stRefParam, 0, sizeof(VENC_PARAM_REF_S));
        stRefParam.Base = pVi2Venc2MuxerData->mConfigPara.mAdvancedRef_Base;
        stRefParam.Enhance = pVi2Venc2MuxerData->mConfigPara.mAdvancedRef_Enhance;
        stRefParam.bEnablePred = pVi2Venc2MuxerData->mConfigPara.mAdvancedRef_RefBaseEn;
        AW_MPI_VENC_SetRefParam(pVi2Venc2MuxerData->mVeChn, &stRefParam);

        AW_MPI_VENC_Set3DNR(pVi2Venc2MuxerData->mVeChn, pVi2Venc2MuxerData->mConfigPara.m3DNR);
        alogd("set 3DNR %d", pVi2Venc2MuxerData->mConfigPara.m3DNR);

        VENC_COLOR2GREY_S bColor2Grey;
        memset(&bColor2Grey, 0, sizeof(VENC_COLOR2GREY_S));
        bColor2Grey.bColor2Grey = pVi2Venc2MuxerData->mConfigPara.mColor2Grey;
        AW_MPI_VENC_SetColor2Grey(pVi2Venc2MuxerData->mVeChn, &bColor2Grey);
        alogd("set Color2Grey %d", pVi2Venc2MuxerData->mConfigPara.mColor2Grey);

        AW_MPI_VENC_SetHorizonFlip(pVi2Venc2MuxerData->mVeChn, pVi2Venc2MuxerData->mConfigPara.mHorizonFlipFlag);
        alogd("set HorizonFlip %d", pVi2Venc2MuxerData->mConfigPara.mHorizonFlipFlag);

        VENC_CROP_CFG_S stCropCfg;
        memset(&stCropCfg, 0, sizeof(VENC_CROP_CFG_S));
        stCropCfg.bEnable = pVi2Venc2MuxerData->mConfigPara.mCropEnable;
        stCropCfg.Rect.X = pVi2Venc2MuxerData->mConfigPara.mCropRectX;
        stCropCfg.Rect.Y = pVi2Venc2MuxerData->mConfigPara.mCropRectY;
        stCropCfg.Rect.Width = pVi2Venc2MuxerData->mConfigPara.mCropRectWidth;
        stCropCfg.Rect.Height = pVi2Venc2MuxerData->mConfigPara.mCropRectHeight;
        AW_MPI_VENC_SetCrop(pVi2Venc2MuxerData->mVeChn, &stCropCfg);
        alogd("set Crop %d, [%d][%d][%d][%d]", stCropCfg.bEnable, stCropCfg.Rect.X, stCropCfg.Rect.Y, stCropCfg.Rect.Width, stCropCfg.Rect.Height);

        //test PIntraRefresh
        if(pVi2Venc2MuxerData->mConfigPara.mIntraRefreshBlockNum > 0)
        {
            VENC_PARAM_INTRA_REFRESH_S stIntraRefresh;
            memset(&stIntraRefresh, 0, sizeof(VENC_PARAM_INTRA_REFRESH_S));
            stIntraRefresh.bRefreshEnable = TRUE;
            stIntraRefresh.RefreshLineNum = pVi2Venc2MuxerData->mConfigPara.mIntraRefreshBlockNum;
            ret = AW_MPI_VENC_SetIntraRefresh(pVi2Venc2MuxerData->mVeChn, &stIntraRefresh);
            if(ret != SUCCESS)
            {
                aloge("fatal error! set roiBgFrameRate fail[0x%x]!", ret);
            }
            else
            {
                alogd("set intra refresh:%d", stIntraRefresh.RefreshLineNum);
            }
        }

        if(pVi2Venc2MuxerData->mConfigPara.mbEnableSmart)
        {
            VencSmartFun smartParam;
            memset(&smartParam, 0, sizeof(VencSmartFun));
            smartParam.smart_fun_en = 1;
            smartParam.img_bin_en = 1;
            smartParam.img_bin_th = 0;
            smartParam.shift_bits = 2;
            AW_MPI_VENC_SetSmartP(pVi2Venc2MuxerData->mVeChn, &smartParam);
        }

        if(pVi2Venc2MuxerData->mConfigPara.mSVCLayer > 0)
        {
            VencH264SVCSkip stSVCSkip;
            memset(&stSVCSkip, 0, sizeof(VencH264SVCSkip));
            stSVCSkip.nTemporalSVC = pVi2Venc2MuxerData->mConfigPara.mSVCLayer;
            AW_MPI_VENC_SetH264SVCSkip(pVi2Venc2MuxerData->mVeChn, &stSVCSkip);
        }

        if (pVi2Venc2MuxerData->mConfigPara.mVuiTimingInfoPresentFlag)
        {
            /** must be call it before AW_MPI_VENC_GetH264SpsPpsInfo(unbind) and AW_MPI_VENC_StartRecvPic. */
            if(PT_H264 == pVi2Venc2MuxerData->mVencChnAttr.VeAttr.Type)
            {
                VENC_PARAM_H264_VUI_S H264Vui;
                memset(&H264Vui, 0, sizeof(VENC_PARAM_H264_VUI_S));
                AW_MPI_VENC_GetH264Vui(pVi2Venc2MuxerData->mVeChn, &H264Vui);
                H264Vui.VuiTimeInfo.timing_info_present_flag = 1;
                H264Vui.VuiTimeInfo.fixed_frame_rate_flag = 0;
                H264Vui.VuiTimeInfo.num_units_in_tick = 1000;
                H264Vui.VuiTimeInfo.time_scale = H264Vui.VuiTimeInfo.num_units_in_tick * pVi2Venc2MuxerData->mConfigPara.mVideoFrameRate * 2;
                AW_MPI_VENC_SetH264Vui(pVi2Venc2MuxerData->mVeChn, &H264Vui);
            }
            else if(PT_H265 == pVi2Venc2MuxerData->mVencChnAttr.VeAttr.Type)
            {
                VENC_PARAM_H265_VUI_S H265Vui;
                memset(&H265Vui, 0, sizeof(VENC_PARAM_H265_VUI_S));
                AW_MPI_VENC_GetH265Vui(pVi2Venc2MuxerData->mVeChn, &H265Vui);
                H265Vui.VuiTimeInfo.timing_info_present_flag = 1;
                H265Vui.VuiTimeInfo.num_units_in_tick = 1000;
                /* Notices: the protocol syntax states that h265 does not need to be multiplied by 2. */
                H265Vui.VuiTimeInfo.time_scale = H265Vui.VuiTimeInfo.num_units_in_tick * pVi2Venc2MuxerData->mConfigPara.mVideoFrameRate;
                H265Vui.VuiTimeInfo.num_ticks_poc_diff_one_minus1 = H265Vui.VuiTimeInfo.num_units_in_tick;
                AW_MPI_VENC_SetH265Vui(pVi2Venc2MuxerData->mVeChn, &H265Vui);
            }
        }

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pVi2Venc2MuxerData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pVi2Venc2MuxerData->mVeChn, &cbInfo);

        return SUCCESS;
    }
}

static ERRORTYPE createViChn(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData)
{
    ERRORTYPE ret;

    //create vi channel
    if (pVi2Venc2MuxerData->mConfigPara.mOnlineEnable)
    {
        pVi2Venc2MuxerData->mViDev = 0;
        alogd("online: only vipp0 & Vechn0 support online.");
    }
    else
    {
        pVi2Venc2MuxerData->mViDev = pVi2Venc2MuxerData->mConfigPara.mVippDev;
    }
    pVi2Venc2MuxerData->mIspDev = 0;
    pVi2Venc2MuxerData->mViChn = 0;

    ret = AW_MPI_VI_CreateVipp(pVi2Venc2MuxerData->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp failed");
    }

    memset(&pVi2Venc2MuxerData->mViAttr, 0, sizeof(VI_ATTR_S));
    if (pVi2Venc2MuxerData->mConfigPara.mOnlineEnable)
    {
        pVi2Venc2MuxerData->mViAttr.mOnlineEnable = 1;
        pVi2Venc2MuxerData->mViAttr.mOnlineShareBufNum = pVi2Venc2MuxerData->mConfigPara.mOnlineShareBufNum;
    }
    pVi2Venc2MuxerData->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pVi2Venc2MuxerData->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pVi2Venc2MuxerData->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pVi2Venc2MuxerData->mConfigPara.srcPixFmt);
    pVi2Venc2MuxerData->mViAttr.format.field = V4L2_FIELD_NONE;
    pVi2Venc2MuxerData->mViAttr.format.colorspace = pVi2Venc2MuxerData->mConfigPara.mColorSpace;
    pVi2Venc2MuxerData->mViAttr.format.width = pVi2Venc2MuxerData->mConfigPara.srcWidth;
    pVi2Venc2MuxerData->mViAttr.format.height = pVi2Venc2MuxerData->mConfigPara.srcHeight;
    pVi2Venc2MuxerData->mViAttr.nbufs =  pVi2Venc2MuxerData->mConfigPara.mViBufferNum;
    alogd("vipp use %d v4l2 buffers, colorspace: 0x%x", pVi2Venc2MuxerData->mViAttr.nbufs, pVi2Venc2MuxerData->mViAttr.format.colorspace);
    pVi2Venc2MuxerData->mViAttr.nplanes = 2;
    pVi2Venc2MuxerData->mViAttr.wdr_mode = pVi2Venc2MuxerData->mConfigPara.wdr_en;
    alogd("wdr_mode %d", pVi2Venc2MuxerData->mViAttr.wdr_mode);
    pVi2Venc2MuxerData->mViAttr.fps = pVi2Venc2MuxerData->mConfigPara.mVideoFrameRate;
    pVi2Venc2MuxerData->mViAttr.drop_frame_num = pVi2Venc2MuxerData->mConfigPara.mViDropFrameNum;

    ret = AW_MPI_VI_SetVippAttr(pVi2Venc2MuxerData->mViDev, &pVi2Venc2MuxerData->mViAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr failed");
    }
#if ISP_RUN
    AW_MPI_ISP_Run(pVi2Venc2MuxerData->mIspDev);
#endif
    ViVirChnAttrS stVirChnAttr;
    memset(&stVirChnAttr, 0, sizeof(ViVirChnAttrS));
    stVirChnAttr.mbRecvInIdleState = TRUE;
    ret = AW_MPI_VI_CreateVirChn(pVi2Venc2MuxerData->mViDev, pVi2Venc2MuxerData->mViChn, &stVirChnAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!", pVi2Venc2MuxerData->mViChn);
    }
    ret = AW_MPI_VI_EnableVipp(pVi2Venc2MuxerData->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!");
    }
    return ret;
}

static ERRORTYPE prepare(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData)
{
    BOOL nSuccessFlag;
    MUX_CHN nMuxChn;
    MUX_CHN_INFO_S *pEntry, *pTmp;
    ERRORTYPE ret;
    ERRORTYPE result = FAILURE;

    if (createViChn(pVi2Venc2MuxerData) != SUCCESS)
    {
        aloge("create vi chn fail");
        return result;
    }

    if (createVencChn(pVi2Venc2MuxerData) != SUCCESS)
    {
        aloge("create venc chn fail");
        return result;
    }

    if (createMuxGrp(pVi2Venc2MuxerData) != SUCCESS)
    {
        aloge("create mux group fail");
        return result;
    }

    //set spspps
    if (pVi2Venc2MuxerData->mConfigPara.mVideoEncoderFmt == PT_H264)
    {
        VencHeaderData H264SpsPpsInfo;
        memset(&H264SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(pVi2Venc2MuxerData->mVeChn, &H264SpsPpsInfo);
        if (SUCCESS != ret)
        {
            aloge("fatal error, venc GetH264SpsPpsInfo failed! ret=%d", ret);
            return result;
        }
        AW_MPI_MUX_SetH264SpsPpsInfo(pVi2Venc2MuxerData->mMuxGrp, pVi2Venc2MuxerData->mVeChn, &H264SpsPpsInfo);
    }
    else if(pVi2Venc2MuxerData->mConfigPara.mVideoEncoderFmt == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        memset(&H265SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(pVi2Venc2MuxerData->mVeChn, &H265SpsPpsInfo);
        if (SUCCESS != ret)
        {
            aloge("fatal error, venc GetH265SpsPpsInfo failed! ret=%d", ret);
            return result;
        }
        AW_MPI_MUX_SetH265SpsPpsInfo(pVi2Venc2MuxerData->mMuxGrp, pVi2Venc2MuxerData->mVeChn, &H265SpsPpsInfo);
    }

    pthread_mutex_lock(&pVi2Venc2MuxerData->mMuxChnListLock);
    if (!list_empty(&pVi2Venc2MuxerData->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pVi2Venc2MuxerData->mMuxChnList, mList)
        {
            nMuxChn = 0;
            nSuccessFlag = FALSE;
            while (pEntry->mMuxChn < MUX_MAX_CHN_NUM)
            {
                ret = AW_MPI_MUX_CreateChn(pVi2Venc2MuxerData->mMuxGrp, nMuxChn, &pEntry->mMuxChnAttr, pEntry->mSinkInfo.mOutputFd);
                if (SUCCESS == ret)
                {
                    nSuccessFlag = TRUE;
                    alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pVi2Venc2MuxerData->mMuxGrp, \
                        nMuxChn, pEntry->mMuxChnAttr.mMuxerId);
                    break;
                }
                else if(ERR_MUX_EXIST == ret)
                {
                    nMuxChn++;
                    //break;
                }
                else
                {
                    nMuxChn++;
                }
            }

            if (FALSE == nSuccessFlag)
            {
                pEntry->mMuxChn = MM_INVALID_CHN;
                aloge("fatal error! create mux group[%d] channel fail!", pVi2Venc2MuxerData->mMuxGrp);
            }
            else
            {
                result = SUCCESS;
                pEntry->mMuxChn = nMuxChn;
            }
        }
    }
    else
    {
        aloge("maybe something wrong,mux chn list is empty");
    }
    pthread_mutex_unlock(&pVi2Venc2MuxerData->mMuxChnListLock);

    if ((pVi2Venc2MuxerData->mViDev >= 0 && pVi2Venc2MuxerData->mViChn >= 0) && pVi2Venc2MuxerData->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pVi2Venc2MuxerData->mViDev, pVi2Venc2MuxerData->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pVi2Venc2MuxerData->mVeChn};

        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }

    if (pVi2Venc2MuxerData->mVeChn >= 0 && pVi2Venc2MuxerData->mMuxGrp >= 0)
    {
        MPP_CHN_S MuxGrp = {MOD_ID_MUX, 0, pVi2Venc2MuxerData->mMuxGrp};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pVi2Venc2MuxerData->mVeChn};

        AW_MPI_SYS_Bind(&VeChn, &MuxGrp);
        pVi2Venc2MuxerData->mCurrentState = REC_PREPARED;
    }

    return result;
}

static ERRORTYPE start(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData)
{
    ERRORTYPE ret = SUCCESS;

    alogd("start");

    ret = AW_MPI_VI_EnableVirChn(pVi2Venc2MuxerData->mViDev, pVi2Venc2MuxerData->mViChn);
    if (ret != SUCCESS)
    {
        alogd("VI enable error!");
        return FAILURE;
    }

    if (pVi2Venc2MuxerData->mVeChn >= 0)
    {
        AW_MPI_VENC_StartRecvPic(pVi2Venc2MuxerData->mVeChn);
    }

    if (pVi2Venc2MuxerData->mMuxGrp >= 0)
    {
        AW_MPI_MUX_StartGrp(pVi2Venc2MuxerData->mMuxGrp);
    }

    pVi2Venc2MuxerData->mCurrentState = REC_RECORDING;

    return ret;
}

static ERRORTYPE stop(SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData)
{
    MUX_CHN_INFO_S *pEntry, *pTmp;
    ERRORTYPE ret = SUCCESS;

    alogd("stop");

    if (pVi2Venc2MuxerData->mViChn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pVi2Venc2MuxerData->mViDev, pVi2Venc2MuxerData->mViChn);
    }

    if (pVi2Venc2MuxerData->mVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pVi2Venc2MuxerData->mVeChn);
    }

    if (pVi2Venc2MuxerData->mMuxGrp >= 0)
    {
        alogd("stop mux grp");
        AW_MPI_MUX_StopGrp(pVi2Venc2MuxerData->mMuxGrp);
    }
    if (pVi2Venc2MuxerData->mMuxGrp >= 0)
    {
        alogd("destory mux grp");
        AW_MPI_MUX_DestroyGrp(pVi2Venc2MuxerData->mMuxGrp);
        pVi2Venc2MuxerData->mMuxGrp = MM_INVALID_CHN;
    }
    if (pVi2Venc2MuxerData->mVeChn >= 0)
    {
        alogd("destory venc");
        //AW_MPI_VENC_ResetChn(pVi2Venc2MuxerData->mVeChn);
        AW_MPI_VENC_DestroyChn(pVi2Venc2MuxerData->mVeChn);
        pVi2Venc2MuxerData->mVeChn = MM_INVALID_CHN;
    }
    if (pVi2Venc2MuxerData->mViChn >= 0)
    {
        AW_MPI_VI_DestroyVirChn(pVi2Venc2MuxerData->mViDev, pVi2Venc2MuxerData->mViChn);
        AW_MPI_VI_DisableVipp(pVi2Venc2MuxerData->mViDev);
    #if ISP_RUN
        AW_MPI_ISP_Stop(pVi2Venc2MuxerData->mIspDev);
    #endif
        AW_MPI_VI_DestroyVipp(pVi2Venc2MuxerData->mViDev);
    }

    pthread_mutex_lock(&pVi2Venc2MuxerData->mMuxChnListLock);
    if (!list_empty(&pVi2Venc2MuxerData->mMuxChnList))
    {
        alogd("free chn list node");
        list_for_each_entry_safe(pEntry, pTmp, &pVi2Venc2MuxerData->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFd > 0)
            {
                alogd("close file");
                close(pEntry->mSinkInfo.mOutputFd);
                pEntry->mSinkInfo.mOutputFd = -1;
            }

            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    pthread_mutex_unlock(&pVi2Venc2MuxerData->mMuxChnListLock);

    return SUCCESS;
}

ERRORTYPE SampleVI2Venc2Muxer_CreateFolder(const char* pStrFolderPath)
{
    if(NULL == pStrFolderPath || 0 == strlen(pStrFolderPath))
    {
        aloge("folder path is wrong!");
        return FAILURE;
    }
    //check folder existence
    struct stat sb;
    if (stat(pStrFolderPath, &sb) == 0)
    {
        if(S_ISDIR(sb.st_mode))
        {
            return SUCCESS;
        }
        else
        {
            aloge("fatal error! [%s] is exist, but mode[0x%x] is not directory!", pStrFolderPath, (int)sb.st_mode);
            return FAILURE;
        }
    }
    //create folder if necessary
    int ret = mkdir(pStrFolderPath, S_IRWXU | S_IRWXG | S_IRWXO);
    if(!ret)
    {
        alogd("create folder[%s] success", pStrFolderPath);
        return SUCCESS;
    }
    else
    {
        aloge("fatal error! create folder[%s] failed!", pStrFolderPath);
        return FAILURE;
    }
}

void *MsgQueueThread(void *pThreadData)
{
    SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S*)pThreadData;
    message_t stCmdMsg;
    Vi2Venc2MuxerMsgType cmd;
    int nCmdPara;

    alogd("msg queue thread start run!");
    while (1)
    {
        if (0 == get_message(&pVi2Venc2MuxerData->mMsgQueue, &stCmdMsg))
        {
            cmd = stCmdMsg.command;
            nCmdPara = stCmdMsg.para0;

            switch (cmd)
            {
                case Rec_NeedSetNextFd:
                {
                    int muxerId = nCmdPara;
                    char fileName[MAX_FILE_PATH_LEN] = {0};
                    Vi2Venc2Muxer_MessageData *pMsgData = (Vi2Venc2Muxer_MessageData*)stCmdMsg.mpData;

                    if (muxerId == pMsgData->pVi2Venc2MuxerData->mMuxId[0])
                    {
                        getFileNameByCurTime(pMsgData->pVi2Venc2MuxerData, fileName);
                        FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                        memset(pFilePathNode, 0, sizeof(FilePathNode));
                        strncpy(pFilePathNode->strFilePath, fileName, MAX_FILE_PATH_LEN-1);
                        list_add_tail(&pFilePathNode->mList, &pMsgData->pVi2Venc2MuxerData->mMuxerFileListArray[0]);
                    }
                    #ifdef DOUBLE_ENCODER_FILE_OUT
                        static int cnt = 0;
                        cnt++;
                        sprintf(fileName, "/mnt/extsd/sample_vi2venc2muxer/%d.ts", cnt);
                        FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                        memset(pFilePathNode->strFilePath, fileName, MAX_FILE_PATH_LEN-1);
                        list_add_tail(&pFilePathNode->mList, &pVi2Venc2MuxerData->mMuxerFileListArray[1]);
                    #endif
                    alogd("muxId[%d] set next fd, filepath=%s", muxerId, fileName);
                    setOutputFileSync(pVi2Venc2MuxerData, fileName, 0, muxerId);
                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case Rec_FileDone:
                {
                    int ret;
                    int muxerId = nCmdPara;
                    Vi2Venc2Muxer_MessageData *pMsgData = (Vi2Venc2Muxer_MessageData*)stCmdMsg.mpData;
                    int idx = -1;

                    if (muxerId == pMsgData->pVi2Venc2MuxerData->mMuxId[0])
                    {
                        idx = 0;
                    }
                    #ifdef DOUBLE_ENCODER_FILE_OUT
                    else if
                    {
                        idx = 1;
                    }
                    #endif
                    if (idx >= 0)
                    {
                        int cnt = 0;
                        struct list_head *pList;
                        list_for_each(pList, &pMsgData->pVi2Venc2MuxerData->mMuxerFileListArray[idx]){cnt++;}
                        FilePathNode *pNode = NULL;
                        while (cnt > pMsgData->pVi2Venc2MuxerData->mConfigPara.mDstFileMaxCnt)
                        {
                            pNode = list_first_entry(&pMsgData->pVi2Venc2MuxerData->mMuxerFileListArray[idx], FilePathNode, mList);
                            if ((ret = remove(pNode->strFilePath)) != 0)
                            {
                                aloge("fatal error! delete file[%s] failed:%s", pNode->strFilePath, strerror(errno));
                            }
                            else
                            {
                                alogd("delete file[%s] success", pNode->strFilePath);
                            }
                            cnt--;
                            list_del(&pNode->mList);
                            free(pNode);
                        }
                    }
                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case MsgQueue_Stop:
                {
                    goto _Exit;
                }
                default:
                {
                    break;
                }
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pVi2Venc2MuxerData->mMsgQueue, 0);
        }
    }
_Exit:
    alogd("msg queue thread exit!");
    return NULL;
}

int main(int argc, char** argv)
{
    int result = -1;
    MUX_CHN_INFO_S *pEntry, *pTmp;
    GLogConfig stGLogConfig =
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);

	printf("sample_virvi2venc2muxer running!\n");
    SAMPLE_VI2VENC2MUXER_S *pVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S* )malloc(sizeof(SAMPLE_VI2VENC2MUXER_S));

    if (pVi2Venc2MuxerData == NULL)
    {
        aloge("malloc struct fail");
        result = FAILURE;
        goto _err0;
    }
    if (InitVi2Venc2MuxerData(pVi2Venc2MuxerData) != SUCCESS)
    {
        return -1;
    }
    gpVi2Venc2MuxerData = pVi2Venc2MuxerData;
    cdx_sem_init(&pVi2Venc2MuxerData->mSemExit, 0);

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

    if (parseCmdLine(pVi2Venc2MuxerData, argc, argv) != SUCCESS)
    {
        aloge("parse cmdline fail");
        result = FAILURE;
        goto err_out_0;
    }
    char *pConfPath = NULL;
    if(argc > 1)
    {
        pConfPath = pVi2Venc2MuxerData->mCmdLinePara.mConfigFilePath;
    }

    if (loadConfigPara(pVi2Venc2MuxerData, pConfPath) != SUCCESS)
    {
        aloge("load config file fail");
        result = FAILURE;
        goto err_out_0;
    }
    alogd("ViDropFrameNum=%d", pVi2Venc2MuxerData->mConfigPara.mViDropFrameNum);
    result = SampleVI2Venc2Muxer_CreateFolder(pVi2Venc2MuxerData->mDstDir);
    if (result)
    {
        goto err_out_0;
    }

    INIT_LIST_HEAD(&pVi2Venc2MuxerData->mMuxChnList);
    pthread_mutex_init(&pVi2Venc2MuxerData->mMuxChnListLock, NULL);

    pVi2Venc2MuxerData->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pVi2Venc2MuxerData->mSysConf);
    result = AW_MPI_SYS_Init();
    if (SUCCESS != result)
    {
        goto err_out_0;
    }

    pVi2Venc2MuxerData->mMuxId[0] = addOutputFormatAndOutputSink(pVi2Venc2MuxerData, pVi2Venc2MuxerData->mConfigPara.dstVideoFile, MEDIA_FILE_FORMAT_MP4);
    if (pVi2Venc2MuxerData->mMuxId[0] < 0)
    {
        result = -1;
        aloge("add first out file fail");
        goto err_out_1;
    }
    FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
    memset(pFilePathNode, 0, sizeof(FilePathNode));
    strncpy(pFilePathNode->strFilePath, pVi2Venc2MuxerData->mConfigPara.dstVideoFile, MAX_FILE_PATH_LEN-1);
    list_add_tail(&pFilePathNode->mList, &pVi2Venc2MuxerData->mMuxerFileListArray[0]);

#ifdef DOUBLE_ENCODER_FILE_OUT
    char mov_path[MAX_FILE_PATH_LEN];
    strcpy(mov_path, "/mnt/extsd/sample_vi2venc2muxer/0.ts");
    pVi2Venc2MuxerData->mMuxId[1] = addOutputFormatAndOutputSink(pVi2Venc2MuxerData, mov_path, MEDIA_FILE_FORMAT_TS);
    if (pVi2Venc2MuxerData->mMuxId[1] < 0)
    {
        alogd("add mMuxId[1] ts file sink fail");
    }
    else
    {
        FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
        memset(pFilePathNode, 0, sizeof(FilePathNode));
        strncpy(pFilePathNode->strFilePath, mov_path, MAX_FILE_PATH_LEN-1);
        list_add_tail(&pFilePathNode->mList, &pVi2Venc2MuxerData->mMuxerFileListArray[1]);
    }
#endif

    if (prepare(pVi2Venc2MuxerData) != SUCCESS)
    {
        result = -1;
        aloge("prepare fail!");
        goto err_out_2;
    }

    //create msg queue thread
    result = pthread_create(&pVi2Venc2MuxerData->mMsgQueueThreadId, NULL, MsgQueueThread, pVi2Venc2MuxerData);
    if (result != 0)
    {
        result = -1;
        aloge("fatal error! create Msg Queue Thread fail[%d]", result);
        goto err_out_3;
    }
    else
    {
        alogd("create Msg Queue Thread success! threadId[0x%x]", &pVi2Venc2MuxerData->mMsgQueueThreadId);
    }

    start(pVi2Venc2MuxerData);

    if (pVi2Venc2MuxerData->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pVi2Venc2MuxerData->mSemExit, pVi2Venc2MuxerData->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pVi2Venc2MuxerData->mSemExit);
    }

    //stop msg queue thread
    message_t stMsgCmd;
    stMsgCmd.command = MsgQueue_Stop;
    put_message(&pVi2Venc2MuxerData->mMsgQueue, &stMsgCmd);
    pthread_join(pVi2Venc2MuxerData->mMsgQueueThreadId, NULL);
    alogd("start to free res");
err_out_3:
    stop(pVi2Venc2MuxerData);
    result = 0;
err_out_2:
    pthread_mutex_lock(&pVi2Venc2MuxerData->mMuxChnListLock);
    if (!list_empty(&pVi2Venc2MuxerData->mMuxChnList))
    {
        alogd("chn list not empty");
        list_for_each_entry_safe(pEntry, pTmp, &pVi2Venc2MuxerData->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFd > 0)
            {
                close(pEntry->mSinkInfo.mOutputFd);
                pEntry->mSinkInfo.mOutputFd = -1;
            }
        }

        list_del(&pEntry->mList);
        free(pEntry);
    }
    pthread_mutex_unlock(&pVi2Venc2MuxerData->mMuxChnListLock);
err_out_1:
    AW_MPI_SYS_Exit();

    pthread_mutex_destroy(&pVi2Venc2MuxerData->mMuxChnListLock);
err_out_0:
    cdx_sem_deinit(&pVi2Venc2MuxerData->mSemExit);
    message_destroy(&pVi2Venc2MuxerData->mMsgQueue);
    free(pVi2Venc2MuxerData);
    gpVi2Venc2MuxerData = pVi2Venc2MuxerData = NULL;
_err0:
    log_quit();

    int disp_fd = open("/dev/fb0", O_RDWR);
    if (disp_fd < 0)
    {
        alogd("[%s] open fb0 fail!", LOG_TAG);
    }

    printf("%s", ((0 == result) ? "PASS" : "FAIL"));
    return result;
}
