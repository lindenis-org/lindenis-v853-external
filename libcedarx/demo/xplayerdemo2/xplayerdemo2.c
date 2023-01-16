/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : xplayerdemo.c
 * Description : xplayerdemo in linux, H3-tv2next
 *               video write to DE, audio write with alsa
 * History :
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <sys/select.h>

#include "iniparserapi.h"

#include "cdx_config.h"
#include <cdx_log.h>
#include "xplayer.h"
#include "CdxTypes.h"
#include <signal.h>

#include <AwPool.h>
#include <CdxQueue.h>

extern LayerCtrl* LayerCreate();
extern SoundCtrl* TinaSoundDeviceInit();
extern SoundCtrl* SoundDeviceCreate();

extern SubCtrl* SubtitleCreate();
extern Deinterlace* DeinterlaceCreate();
extern LayerCtrl* LayerCreate_DE();

//extern int OpenSoundDevice();

//extern int CloseSoundDevice();

//void AwStreamInit(void);

enum PlayerStatusE
{
    STATUS_STOPPED = 0,
    STATUS_PREPARING = 1,
    STATUS_PREPARED = 2,
    STATUS_PLAYING = 3,
    STATUS_PAUSED = 4,
    STATUS_SEEKING = 5
};

typedef enum 
{
    PLAYER_CTL_NULL = 0,
    PLAYER_CTL_STOP = 1,
    PLAYER_CTL_REPEAT = 2,
    PLAYER_CTL_EXIT = 3,
	PLAYER_CTL_START = 4,
} PlayerCtlMsgE;

typedef struct DemoPlayerContext
{
    XPlayer*       mAwPlayer;
    int             mPreStatus;
    int             mStatus;
    int             mError;
    pthread_mutex_t mMutex;
    int x;
    int y;
    int w;
    int h;
    int repeat_flag;
    char uri[1024];
    CdxQueueT *msg_queue;
    AwPoolT *pool;
} DemoPlayerContext;

static DemoPlayerContext player_cnt;

//* a callback for awplayer.
static int __CallbackForAwPlayer(void* pUserData, int msg, int ext1, void* param)
{
    DemoPlayerContext* pDemoPlayer = (DemoPlayerContext*)pUserData;

    switch(msg)
    {
        case AWPLAYER_MEDIA_ERROR:
        {
            pthread_mutex_lock(&pDemoPlayer->mMutex);
            pDemoPlayer->mStatus = STATUS_STOPPED;
            pDemoPlayer->mPreStatus = STATUS_STOPPED;
            printf("error: open media source fail.\n");
            pthread_mutex_unlock(&pDemoPlayer->mMutex);
            pDemoPlayer->mError = 1;

            loge(" error : how to deal with it");
            break;
        }

        case AWPLAYER_MEDIA_PREPARED:
        {
            logd("info : preared");
            pDemoPlayer->mPreStatus = pDemoPlayer->mStatus;
            pDemoPlayer->mStatus = STATUS_PREPARED;
            printf("info: prepare ok.\n");
            CdxQueuePush(player_cnt.msg_queue, (CdxQueueDataT)PLAYER_CTL_START);
            break;
        }

        case AWPLAYER_MEDIA_PLAYBACK_COMPLETE:
        {
            if (player_cnt.repeat_flag)
            {
                CdxQueuePush(player_cnt.msg_queue, (CdxQueueDataT)PLAYER_CTL_REPEAT);
                logd("repeat now...");
            }
            else
            {
                logd("single mode exit now...");                
                CdxQueuePush(player_cnt.msg_queue, (CdxQueueDataT)PLAYER_CTL_EXIT);
            }
            break;
        }

        default:
        {
            logd("event '%d' ", msg);
            //printf("warning: unknown callback from AwPlayer.\n");
            break;
        }
    }

    return 0;
}

static void __SignalProc(int sig)
{
    switch (sig)
    { 
        case SIGUSR1:
        case SIGINT:
            logd("revice sig '%d' ", sig);
            CdxQueuePush(player_cnt.msg_queue, (CdxQueueDataT)PLAYER_CTL_STOP);
            break;
            
        default:
            loge("unexpect signal '%d' ", sig);
            break;
    } 
    return;
    
}

static int playerInit()
{
    //* create a player.
    player_cnt.mAwPlayer = XPlayerCreate();
    if(player_cnt.mAwPlayer == NULL)
    {
        printf("can not create AwPlayer, quit.\n");
        return -1;
    }

    //* set callback to player.
    XPlayerSetNotifyCallback(player_cnt.mAwPlayer, __CallbackForAwPlayer, (void*)&player_cnt);

    //* check if the player work.
    if(XPlayerInitCheck(player_cnt.mAwPlayer) != 0)
    {
        printf("initCheck of the player fail, quit.\n");
        XPlayerDestroy(player_cnt.mAwPlayer);
        player_cnt.mAwPlayer = NULL;
        return -1;
    }

    //LayerCtrl* layer = LayerCreate();
    LayerCtrl* layer = LayerCreate_DE();
    LayerSetDisplayRegion(layer, player_cnt.x, player_cnt.y, player_cnt.w, player_cnt.h);

    SoundCtrl* sound;
	if (GetConfigParamterInt("sound_ahub", 0) == 1) /* use AHub sound control */
	{
		sound = SoundDeviceCreate();
	}
	else
	{
		sound = TinaSoundDeviceInit();
	}

    XPlayerSetAudioSink(player_cnt.mAwPlayer, (void*)sound);
    XPlayerSetVideoSurfaceTexture(player_cnt.mAwPlayer, (void*)layer);
//    XPlayerSetSubCtrl(player_cnt.mAwPlayer, sub);
    return 0;
}

static int playerPrepare()
{
    /* set url */
    logd("uri: '%s'", player_cnt.uri);
    if(XPlayerSetDataSourceUrl(player_cnt.mAwPlayer, &player_cnt.uri[0], NULL, NULL) != 0)
    {
        printf("error:\n");
        printf("    AwPlayer::setDataSource() return fail.\n");
        return -1;
    }
     printf("setDataSource end\n");

    //* start preparing.
    pthread_mutex_lock(&player_cnt.mMutex);
    if(XPlayerPrepareAsync(player_cnt.mAwPlayer) != 0)
    {
        printf("error:\n");
        printf("    AwPlayer::prepareAsync() return fail.\n");
        pthread_mutex_unlock(&player_cnt.mMutex);
        return -1;
    }

    player_cnt.mPreStatus = STATUS_STOPPED;
    player_cnt.mStatus    = STATUS_PREPARING;
    printf("preparing...\n");
    pthread_mutex_unlock(&player_cnt.mMutex);

    return 0;
}

static int playerStart()
{
    /* start */
    if(XPlayerStart(player_cnt.mAwPlayer) != 0)
    {
        printf("error:\n");
        printf("    AwPlayer::start() return fail.\n");
        return -1;
    }
    player_cnt.mPreStatus = player_cnt.mStatus;
    player_cnt.mStatus    = STATUS_PLAYING;
    printf("playing.\n");

    return 0;
}

int playerStop()
{
    int ret = 0;
    ret = XPlayerReset(player_cnt.mAwPlayer);
    player_cnt.mPreStatus = player_cnt.mStatus;
    player_cnt.mStatus = STATUS_STOPPED;
    return ret;
}

//* the main method.
int main(int argc, char** argv)
{
    int opt = 0;
    char *optstr = "x:y:w:h:d:rf:";
    int ret = 0;

	logd("* Hello xplayerdemo2.");
	printf("* Hello xplayerdemo2.");
    
//    AwStreamInit();

    memset(&player_cnt, 0, sizeof(DemoPlayerContext));
    pthread_mutex_init(&player_cnt.mMutex, NULL);

    /* parse command line */
    opt = getopt(argc, argv, optstr);
    while (opt != -1)
    {
        switch (opt)
        {
            case 'x':
                player_cnt.x = atoi(optarg);
                break;
                 
            case 'y':
                player_cnt.y = atoi(optarg);
                break;
                 
            case 'w':
                player_cnt.w = atoi(optarg);
                break;
                 
            case 'h':
                player_cnt.h = atoi(optarg);
                break;

            case 'r':
                player_cnt.repeat_flag = 1;
                break;
                
            case 'f':
                if (optarg[0] == '/')
                {
                    snprintf(player_cnt.uri, 1023, "file://%s", optarg);
                }
                else
                {
                    snprintf(player_cnt.uri, 1023, "%s", optarg);
                }
                break;
                 
            default:
                CDX_CHECK(0);
                break;
        }
        
        opt = getopt(argc, argv, optstr);
    }

    logd("crop(%d, %d, %d, %d)", player_cnt.x, player_cnt.y, player_cnt.w, player_cnt.h);
    logd("uri: '%s'", player_cnt.uri);

//    OpenSoundDevice();

    /* init msg queue */
    player_cnt.pool = AwPoolCreate(NULL);
    player_cnt.msg_queue = CdxQueueCreate(player_cnt.pool);
    signal(SIGUSR1, __SignalProc);
    signal(SIGINT, __SignalProc);
    
    ret = playerInit();
    if (ret != 0)
    {
        goto out;
    }
    
    ret = playerPrepare();
    if (ret != 0)
    {
        goto out;
    }

    /* msg main process */
    while (1)
    {
        PlayerCtlMsgE msg = (PlayerCtlMsgE)CdxQueuePop(player_cnt.msg_queue);
        if (msg == 0)
        {
            usleep(200000);
            continue;
        }
        switch (msg)
        {
            case PLAYER_CTL_STOP:
            {
                ret = playerStop();
                if (ret != 0)
                {
                    logd("stop error....");
                }
                sleep(2);
                goto out;
            }
            case PLAYER_CTL_START:
            {
            	ret = playerStart();
            	if (ret != 0)
            	{
                    logd("start error....");
            	}
            	break;
            }
            case PLAYER_CTL_EXIT:
            {
                goto out;
            }
            case PLAYER_CTL_REPEAT:
            {
                logd("--------- repeat now ---------");
#if 0
                ret = playerStop();
                if (ret != 0)
                {
                    logd("stop error....");
                    goto out;
                }
            
                ret = playerPrepare();
                if (ret != 0)
                {
                    logd("prepare error....");
                    goto out;
                }
#else   /* use xplayer seek interface */
                ret = XPlayerSeekTo(player_cnt.mAwPlayer, 0, AW_SEEK_PREVIOUS_SYNC);
                if (ret != 0)
                {
                    logd("prepare error....");
                    goto out;
                }
                
#endif
                ret = playerStart();
                if (ret != 0)
                {
                    logd("start error....");
                    goto out;
                }
                break;
            }
            default:
                loge("what happen!!! shold not be here... '%d' ", msg);
                break;
        }
        
    }
    
out:

    if(player_cnt.mAwPlayer != NULL)
    {
        XPlayerDestroy(player_cnt.mAwPlayer);
        player_cnt.mAwPlayer = NULL;
    }

    CdxQueueDestroy(player_cnt.msg_queue);
    AwPoolDestroy(player_cnt.pool);
    
//    CloseSoundDevice();
    logd("destroy AwPlayer");
    pthread_mutex_destroy(&player_cnt.mMutex);

    logd("* Quit the program, goodbye!");

    return 0;
}

