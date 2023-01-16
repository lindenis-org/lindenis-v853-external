/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : wfd_player.h
 * Description : player
 * History :
 *
 */

#ifndef WFD_PLAYER_H
#define WFD_PLAYER_H

#include "vdecoder.h"
#include "adecoder.h"
#include "layerControl.h"
#include "soundControl.h"
#include <CdxEnumCommon.h>

enum EPLAYERNOTIFY {
    PLAYBACK_NOTIFY_EOS = PLAYBACK_NOTIFY_VALID_RANGE_MIN,

    //* param == NULL;
    PLAYBACK_NOTIFY_FIRST_PICTURE,

    //* width = ((int*)param)[0]; height = ((int*)param)[1];
    PLAYBACK_NOTIFY_VIDEO_SIZE,
    //* leftOffset = ((int*)param)[0; topOffset = ((int*)param)[1];
    //* cropWidth = ((int*)param)[2]; cropHeight = ((int*)param)[3];
    PLAYBACK_NOTIFY_VIDEO_CROP,

    //* subtitle_id   = ((unsigned int*)param)[0];
    //* pSubtitleItem = (SubtitleItem*)((unsigned int*)param)[1];
    PLAYBACK_NOTIFY_SUBTITLE_ITEM_AVAILABLE,
    //* subtitle_id = (unsigned int)param;
    PLAYBACK_NOTIFY_SUBTITLE_ITEM_EXPIRED,

    //* video stream not supported, video decoder crash.
    PLAYBACK_NOTIFY_VIDEO_UNSUPPORTED,
    //* audio stream not supported, audio decoder crash.
    PLAYBACK_NOTIFY_AUDIO_UNSUPPORTED,
    //* subtitle stream not supported, subtitle decoder crash.
    PLAYBACK_NOTIFY_SUBTITLE_UNSUPPORTED,
    PLAYBACK_NOTIFY_AUDIORAWPLAY,

    PLAYBACK_NOTIFY_SET_SECURE_BUFFER_COUNT,
    PLAYBACK_NOTIFY_SET_SECURE_BUFFERS,

    PLAYBACK_NOTIFY_AUDIO_INFO,

    PLAYBACK_NOTIFY_VIDEO_RENDER_FRAME,

    PLAYBACK_NOTIFY_VIDEO_FRAME_PARAM,

    PLAYBACK_NOTIFY_MAX,
};
CHECK_PLAYBACK_NOTIFY_MAX_VALID(PLAYBACK_NOTIFY_MAX)

enum EPLAYERSTATUS
{
    PLAYER_STATUS_STOPPED = 0,
    PLAYER_STATUS_STARTED,
    PLAYER_STATUS_PAUSED
};

enum EMEDIATYPE
{
    MEDIA_TYPE_VIDEO = 0,
    MEDIA_TYPE_AUDIO,
    MEDIA_TYPE_SUBTITLE
};

enum EPICTURE3DMODE
{
    PICTURE_3D_MODE_NONE = 0,
    PICTURE_3D_MODE_TWO_SEPERATED_PICTURE,
    PICTURE_3D_MODE_SIDE_BY_SIDE,
    PICTURE_3D_MODE_TOP_TO_BOTTOM,
    PICTURE_3D_MODE_LINE_INTERLEAVE,
    PICTURE_3D_MODE_COLUME_INTERLEAVE
};

enum EDISPLAY3DMODE
{
    DISPLAY_3D_MODE_2D = 0,
    DISPLAY_3D_MODE_3D,
    DISPLAY_3D_MODE_HALF_PICTURE
};

enum EDISPLAYRATIO
{
    DISPLAY_RATIO_FULL_SCREEN,
    DISPLAY_RATIO_LETTERBOX,
    //* add new mode.
};

typedef struct MEDIASTREAMDATAINFO
{
    char*   pData;
    int     nLength;
    int64_t nPts;
    int64_t nPcr;
    int     bIsFirstPart;
    int     bIsLastPart;
    int64_t nDuration;      //* in unit of us.
    int     nStreamChangeFlag;
    int     nStreamChangeNum;
    void   *pStreamInfo;
}MediaStreamDataInfo;

typedef int (*PlayerCallback)(void* pUserData, int eMessageId, void* param);

typedef void* Player;


#ifdef __cplusplus
extern "C" {
#endif

Player* WFD_PlayerCreate(void);

void WFD_PlayerDestroy(Player* pl);

int WFD_PlayerSetCallback(Player* pl, PlayerCallback callback, void* pUserData);


//*******************************  START  **********************************//
//** Play Control APIs.
//**

int WFD_PlayerStart(Player* pl);

int WFD_PlayerStop(Player* pl);      //* media stream information is still kept by the player.

int WFD_PlayerPause(Player* pl);

enum EPLAYERSTATUS WFD_PlayerGetStatus(Player* pl);

//* for seek operation, mute be called under paused status.
int WFD_PlayerReset(Player* pl, int64_t nSeekTimeUs);

//* must be called under stopped status, all stream information cleared.
int WFD_PlayerClear(Player* pl);


//********************************  END  ***********************************//

//*******************************  START  **********************************//
//** Streaming Control APIs.
//**

int WFD_PlayerRequestStreamBuffer(Player*       pl,
                              int             nRequireSize,
                              void**          ppBuf,
                              int*            pBufSize,
                              void**          ppRingBuf,
                              int*            pRingBufSize,
                              enum EMEDIATYPE eMediaType,
                              int             nStreamIndex);

int WFD_PlayerSubmitStreamData(Player*            pl,
                           MediaStreamDataInfo* pDataInfo,
                           enum EMEDIATYPE      eMediaType,
                           int                  nStreamIndex);





//* how much video stream data in stream buffere.
int WFD_PlayerGetVideoStreamDataSize(Player* pl);

//* how many stream frame in buffer.
int WFD_PlayerGetVideoStreamFrameNum(Player* pl);


//********************************  END  ***********************************//

//*******************************  START  **********************************//
//** Video APIs.
//**

int WFD_PlayerSetVideoStreamInfo(Player* pl, VideoStreamInfo* pStreamInfo);


int WFD_PlayerHasVideo(Player* pl);

//********************************  END  ***********************************//

//*******************************  START  **********************************//
//** Audio APIs.
//**

int WFD_PlayerSetAudioStreamInfo(Player* pl, AudioStreamInfo* pStreamInfo,
        int nStreamNum, int nDefaultStream);




int WFD_PlayerHasAudio(Player* pl);


//hkw switch audio track for IPTV
int WFD_PlayerStopAudio(Player* pl);

int WFD_PlayerStartAudio(Player* pl);

//********************************  END  ***********************************//

//*******************************  START  **********************************//
//** Display Control APIs.
//**
int WFD_PlayerSetWindow(Player* pl, LayerCtrl* pLc);

//********************************  END  ***********************************//

//*******************************  START  **********************************//
//** Audio Output Control APIs.
//**
int WFD_PlayerSetAudioSink(Player* pl, SoundCtrl* pAudioSink);



//********************************  END  ***********************************//


#ifdef __cplusplus
}
#endif

#endif
