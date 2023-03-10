/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : soundControl_alsa.c
 * Description : soundControl for alsa
 * History :
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <cdx_log.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <asoundlib.h>
#include "soundControl.h"
#include "auGaincom.h"

#include "volume_uci_config_api.h"

enum SDSTATUS
{
    SD_STATUS_STOPPED = 0,
    SD_STATUS_STARTED,
    SD_STATUS_PAUSED
};


typedef struct SoundCtrlContext
{
    SoundCtrl                   base;
    snd_pcm_uframes_t           chunk_size;
    snd_pcm_format_t            alsa_format;
    snd_pcm_hw_params_t         *alsa_hwparams;
    snd_pcm_sw_params_t         *alsa_swparams;
    snd_pcm_t                   *alsa_handler;
    snd_pcm_t                   *alsa_sub_handler;

    unsigned int                nSampleRate;
    unsigned int                nChannelNum;
    int                         alsa_fragcount;
    int                         ao_noblock;
    int                         open_mode;
    int                         alsa_can_pause;
    size_t                      bytes_per_sample;

    enum SDSTATUS               eStatus;
    pthread_mutex_t             mutex;

    int                         set_format_done;
    int                         volume;
    int                         mute;
} SoundCtrlContext;

#define OUTBURST            512
#define ALSA_DEVICE_SIZE    256
#define MAX_AUDIO_DEVICES   5

typedef struct pcm_hw_params_t
{
    unsigned int samplerate;
    unsigned int channels;
    int format;
    int bps;
    int outburst;
    int buffersize;
    int pts;
} pcm_hw_params;
pcm_hw_params pcm_params = {0, \
                            0, \
                            0, \
                            0, \
                            OUTBURST,\
                            -1, \
                            0
                           };

typedef struct strarg_s
{
    int         len;
    char const  *str;
} strarg_t;

typedef struct name_map_t
{
    char name_linux[32];
    int card;
}name_map;

static name_map audio_name_map[MAX_AUDIO_DEVICES] =
{
    {"audiocodec",       -1},
    {"sndspdif",         -1},
    {"sndahub",          -1},
    {"sndhdmi",          -1},
    {"snddaudio0",       -1},
};
static void init_id_map(char * snd_id, int card)
{
    int i;
    for (i = 0; i < MAX_AUDIO_DEVICES; i++)
    {
        if (!strcmp(snd_id, audio_name_map[i].name_linux))
        {
            audio_name_map[i].card = card;
            break;
        }
    }
}
static int find_id_card(char * snd_id)
{
    int i;
    for (i = 0; i < MAX_AUDIO_DEVICES; i++)
    {
        if (!strcmp(snd_id, audio_name_map[i].name_linux))
        {
            return audio_name_map[i].card;
        }
    }
    return -1;
}

static int do_init_audio_card(int card)
{
    int ret = -1;
    int fd = 0;
    char * snd_path = "/sys/class/sound";
    char snd_card[128], snd_node[128];
    char snd_id[32];

    memset(snd_card, 0, sizeof(snd_card));
    memset(snd_node, 0, sizeof(snd_node));
    memset(snd_id, 0, sizeof(snd_id));

    sprintf(snd_card, "%s/card%d", snd_path, card);
    ret = access(snd_card, F_OK);
    if(ret == 0)
    {
        // id / name
        sprintf(snd_node, "%s/card%d/id", snd_path, card);
        logd("read card %s/card%d/id",snd_path, card);
        fd = open(snd_node, O_RDONLY);
        if (fd > 0)
        {
            ret = read(fd, snd_id, sizeof(snd_id));
            if (ret > 0)
            {
                snd_id[ret - 1] = 0;
                logd("%s, %s, len: %d", snd_node, snd_id, ret);
            }
            close(fd);
        }
        else
        {
            return -1;
        }
    }
    init_id_map(snd_id, card);
    return 0;
}
static void do_init_audio_mixer()
{
    struct mixer *mixer;
    struct mixer_ctl *mixer_ctl_ahub;
    mixer = mixer_open(find_id_card("sndahub"));
    mixer_ctl_ahub = mixer_get_ctl_by_name(mixer, "I2S1 Src Select");
    mixer_ctl_set_value(mixer_ctl_ahub, 0, 1);
    mixer_ctl_ahub = mixer_get_ctl_by_name(mixer, "I2S1OUT Switch");
    mixer_ctl_set_value(mixer_ctl_ahub, 0, 1);
}

static int try_open_device(SoundCtrlContext* sc, const char *device, int open_mode, int try_ac3)
{
    int err;

    err = snd_pcm_open(&sc->alsa_handler, device, SND_PCM_STREAM_PLAYBACK,
                       open_mode);
    return err;
}

static void parse_device(char *dest, const char *src, int len)
{
    char *tmp;
    memmove(dest, src, len);
    dest[len] = 0;
    while ((tmp = strrchr(dest, '.')))
        tmp[0] = ',';
    while ((tmp = strrchr(dest, '=')))
        tmp[0] = ':';
}

static int SoundDeviceStop_l(SoundCtrlContext* sc);

static void __Release(SoundCtrl* s)
{
    SoundCtrlContext* sc;

    sc = (SoundCtrlContext*)s;
    pthread_mutex_lock(&sc->mutex);
    if(sc->eStatus != SD_STATUS_STOPPED)
    {
        if (sc->alsa_handler)
        {
            int err;
            if ((err = snd_pcm_close(sc->alsa_handler)) < 0)
            {
                logw("MSGTR_AO_ALSA_PcmCloseError");
            }
            else
            {
                sc->alsa_handler = NULL;
                logd("alsa-uninit: pcm closed\n");
            }
        }
        else
        {
            logw("MSGTR_AO_ALSA_NoHandlerDefined");
        }

        if (sc->alsa_sub_handler)
        {
            int err;
            if ((err = snd_pcm_close(sc->alsa_sub_handler)) < 0)
            {
                logw("MSGTR_AO_ALSA_PcmCloseError");
            }
            else
            {
                sc->alsa_sub_handler = NULL;
                logd("alsa-uninit: pcm closed\n");
            }
        }
        else
        {
            logw("MSGTR_AO_ALSA_NoHandlerDefined");
        }

    }
    pthread_mutex_unlock(&sc->mutex);

    pthread_mutex_destroy(&sc->mutex);

    free(sc);
    sc = NULL;

    return;
}

static void __SetFormat(SoundCtrl* s, CdxPlaybkCfg* cfg)//unsigned int nSampleRate, unsigned int nChannelNum)
{
    SoundCtrlContext* sc;

    sc = (SoundCtrlContext*)s;

    pthread_mutex_lock(&sc->mutex);

    if(sc->eStatus != SD_STATUS_STOPPED)
    {
        logd("Sound device not int stop status, can not set audio params.");
        abort();
    }

    pcm_params.samplerate = (cfg->nSamplerate < 32000) ? 44100 : cfg->nSamplerate;
    pcm_params.channels = cfg->nChannels;
    pcm_params.format = 1; // TODO, 0/1 default audiocodec/sndhdmi
    sc->set_format_done = 1;
    pthread_mutex_unlock(&sc->mutex);
    logd("alsa,set audio format ok !");

    return;
}


static int __Start(SoundCtrl* s)
{
    SoundCtrlContext* sc;

    int                 err;
    int                 block;
    strarg_t            device;
    snd_pcm_uframes_t   bufsize;
    snd_pcm_uframes_t   boundary;

    sc = (SoundCtrlContext*)s;

    //judge if audio format setting finished
    pthread_mutex_lock(&sc->mutex);
    int setFormatDone = sc->set_format_done;
    pthread_mutex_unlock(&sc->mutex);
    if(setFormatDone == 0)
    {
        logw("sound device may be not set format,current status is %d....\n",sc->eStatus);
        return -1;
    }

    pthread_mutex_lock(&sc->mutex);
    if(sc->eStatus == SD_STATUS_STARTED)
    {
        logw("Sound device already started.");
        pthread_mutex_unlock(&sc->mutex);
        return -1;
    }

    if(sc->eStatus == SD_STATUS_STOPPED)
    {
        char alsa_device[ALSA_DEVICE_SIZE + 1];
        char alsa_sub_device[ALSA_DEVICE_SIZE + 1];
        // make sure alsa_device is null-terminated even when using strncpy etc.
        memset(alsa_device, 0, ALSA_DEVICE_SIZE + 1);
        memset(alsa_sub_device, 0, ALSA_DEVICE_SIZE + 1);

        logd("alsa-init: requested format: %d Hz, %d channels, %x\n", pcm_params.samplerate,
             pcm_params.channels, pcm_params.format);

        sc->alsa_handler = NULL;
        sc->alsa_sub_handler = NULL;
        sc->alsa_format = SND_PCM_FORMAT_S16_LE; //TODO: is it right??
        block = 1;
        switch (pcm_params.channels)
        {
        case 1:
        case 2:
            device.str = "default";
            break;
        case 4:
            if (sc->alsa_format == SND_PCM_FORMAT_FLOAT_LE)
                device.str = "plug:surround40";
            else
                device.str = "surround40";
            break;
        case 6:
            if (sc->alsa_format == SND_PCM_FORMAT_FLOAT_LE)
                device.str = "plug:surround51";
            else
                device.str = "surround51";
            break;
        default:
            device.str = "default";

        }
        device.len = strlen(device.str);

        sc->ao_noblock = !block;
        parse_device(alsa_device, device.str, device.len);
        sprintf(alsa_device, "hw:%d",find_id_card("sndahub"));
        sprintf(alsa_sub_device, "hw:%d",find_id_card("sndhdmi"));
        /*if(pcm_params.format == 1)
            strcpy(alsa_device, "hw:1");
        else if(pcm_params.format == 2)
            strcpy(alsa_device, "hw:2");
        else
            strcpy(alsa_device, "hw:0");*/

        logd("alsa-init: using device %s, subdevice %s\n", alsa_device, alsa_sub_device);

        //setting modes for block or nonblock-mode
        if (sc->ao_noblock)
        {
            sc->open_mode = SND_PCM_NONBLOCK;
        }
        else
        {
            sc->open_mode = 0;
        }

        //sets buff/chunksize if its set manually
        if (pcm_params.buffersize)
        {
            switch (pcm_params.buffersize)
            {
            case 1:
                sc->alsa_fragcount = 16;
                sc->chunk_size = 512;
                break;
            case 2:
                sc->alsa_fragcount = 8;
                sc->chunk_size = 1024;
                break;
            case 3:
                sc->alsa_fragcount = 32;
                sc->chunk_size = 512;
                break;
            case 4:
                sc->alsa_fragcount = 16;
                sc->chunk_size = 1024;
                break;
            default:
                sc->alsa_fragcount = 16;
                sc->chunk_size = 1024;
                break;
            }
        }

        if (!sc->alsa_handler)
        {
            if (((err = try_open_device(sc, alsa_device, sc->open_mode, 0)) < 0) || (err = snd_pcm_open(&sc->alsa_sub_handler, alsa_sub_device, SND_PCM_STREAM_PLAYBACK, sc->open_mode) < 0))
            {
                if (err != -EBUSY && sc->ao_noblock)
                {
                    loge("MSGTR_AO_ALSA_OpenInNonblockModeFailed");
                    if (((err = try_open_device(sc, alsa_device, 0, 0)) < 0) || (err = snd_pcm_open(&sc->alsa_sub_handler, alsa_sub_device, SND_PCM_STREAM_PLAYBACK, sc->open_mode) < 0))
                    {
                        pthread_mutex_unlock(&sc->mutex);
                        return 0;
                    }
                }
                else
                {
                    loge("Playback open error: %s", snd_strerror(err));
                    pthread_mutex_unlock(&sc->mutex);
                    return 0;
                }
            }

            if (((err = snd_pcm_nonblock(sc->alsa_handler, 0)) < 0) || ((err = snd_pcm_nonblock(sc->alsa_sub_handler, 0)) < 0))
            {
                logw("MSGTR_AO_ALSA_ErrorSetBlockMode");
            }
            else
            {
                logv("alsa-init: pcm opened in blocking mode\n");
            }

            snd_pcm_hw_params_alloca(&sc->alsa_hwparams);
            snd_pcm_sw_params_alloca(&sc->alsa_swparams);

            // setting hw-parameters
            if ((err = snd_pcm_hw_params_any(sc->alsa_handler, sc->alsa_hwparams)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToGetInitialParameters");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            if ((err = snd_pcm_hw_params_any(sc->alsa_sub_handler, sc->alsa_hwparams)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToGetInitialParameters");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }

            err = snd_pcm_hw_params_set_access(sc->alsa_handler, sc->alsa_hwparams,
                                               SND_PCM_ACCESS_RW_INTERLEAVED);
            if (err < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetAccessType");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            err = snd_pcm_hw_params_set_access(sc->alsa_sub_handler, sc->alsa_hwparams,
                                               SND_PCM_ACCESS_RW_INTERLEAVED);
            if (err < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetAccessType");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }


            /* workaround for nonsupported formats
             sets default format to S16_LE if the given formats aren't supported */
            if ((err = snd_pcm_hw_params_test_format(sc->alsa_handler, sc->alsa_hwparams,
                       sc->alsa_format)) < 0)
            {
                logw("MSGTR_AO_ALSA_FormatNotSupportedByHardware");
                sc->alsa_format = SND_PCM_FORMAT_S16_LE;
            }

            if ((err = snd_pcm_hw_params_set_format(sc->alsa_handler, sc->alsa_hwparams,
                                                    sc->alsa_format)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetFormat");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            if ((err = snd_pcm_hw_params_set_format(sc->alsa_sub_handler, sc->alsa_hwparams,
                                                    sc->alsa_format)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetFormat");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }

            if ((err = snd_pcm_hw_params_set_channels_near(sc->alsa_handler,
                       sc->alsa_hwparams, &pcm_params.channels)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetChannels");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            if ((err = snd_pcm_hw_params_set_channels_near(sc->alsa_sub_handler,
                       sc->alsa_hwparams, &pcm_params.channels)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetChannels");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }

            /* workaround for buggy rate plugin (should be fixed in ALSA 1.0.11)
                         prefer our own resampler */
#if SND_LIB_VERSION >= 0x010009
            if ((err = snd_pcm_hw_params_set_rate_resample(sc->alsa_handler, sc->alsa_hwparams,
                       0)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToDisableResampling");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            if ((err = snd_pcm_hw_params_set_rate_resample(sc->alsa_sub_handler, sc->alsa_hwparams,
                       0)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToDisableResampling");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }

#endif

            if ((err = snd_pcm_hw_params_set_rate_near(sc->alsa_handler, sc->alsa_hwparams,
                       &pcm_params.samplerate, NULL)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetSamplerate2");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            if ((err = snd_pcm_hw_params_set_rate_near(sc->alsa_sub_handler, sc->alsa_hwparams,
                       &pcm_params.samplerate, NULL)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetSamplerate2");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }

            sc->bytes_per_sample = snd_pcm_format_physical_width(sc->alsa_format) / 8;
            sc->bytes_per_sample *= pcm_params.channels;
            pcm_params.bps = pcm_params.samplerate * sc->bytes_per_sample;

#ifdef BUFFERTIME
            {
                int alsa_buffer_time = 500000; /* original 60 */
                int alsa_period_time;
                alsa_period_time = alsa_buffer_time/4;
                if (((err = snd_pcm_hw_params_set_buffer_time_near(sc->alsa_handler,
                           sc->alsa_hwparams, &alsa_buffer_time, NULL)) < 0)
                           || ((err = snd_pcm_hw_params_set_buffer_time_near(sc->alsa_sub_handler,
                           sc->alsa_hwparams, &alsa_buffer_time, NULL)) < 0 ))
                {
                    logw("MSGTR_AO_ALSA_UnableToSetBufferTimeNear");
                    pthread_mutex_unlock(&sc->mutex);
                    return 0;
                }
                else
                    alsa_buffer_time = err;

                if ((err = snd_pcm_hw_params_set_period_time_near(sc->alsa_handler,
                           sc->alsa_hwparams, &alsa_period_time, NULL)) < 0)
                    /* original: alsa_buffer_time/pcm_params.bps */
                {
                    logw("MSGTR_AO_ALSA_UnableToSetBufferTimeNear");
                    pthread_mutex_unlock(&sc->mutex);
                    return 0;
                }
                if ((err = snd_pcm_hw_params_set_period_time_near(sc->alsa_sub_handler,
                           sc->alsa_hwparams, &alsa_period_time, NULL)) < 0)
                    /* original: alsa_buffer_time/pcm_params.bps */
                {
                    logw("MSGTR_AO_ALSA_UnableToSetBufferTimeNear");
                    pthread_mutex_unlock(&sc->mutex);
                    return 0;
                }

            }
#endif//end SET_BUFFERTIME
//#ifdef SET_CHUNKSIZE
            {
                //set chunksize
                if (((err = snd_pcm_hw_params_set_period_size_near(sc->alsa_handler,
                           sc->alsa_hwparams, &sc->chunk_size, NULL)) < 0)
                           || ((err = snd_pcm_hw_params_set_period_size_near(sc->alsa_sub_handler,
                           sc->alsa_hwparams, &sc->chunk_size, NULL)) < 0))
                {
                    logw("MSGTR_AO_ALSA_UnableToSetPeriodSize");
                    pthread_mutex_unlock(&sc->mutex);
                    return 0;
                }
                else
                {
                    logv("alsa-init: chunksize set to %li\n", sc->chunk_size);
                }

                if (((err = snd_pcm_hw_params_set_periods_near(sc->alsa_handler,
                           sc->alsa_hwparams, (unsigned int*)&sc->alsa_fragcount, NULL)) < 0)
                           || ((err = snd_pcm_hw_params_set_periods_near(sc->alsa_sub_handler,
                           sc->alsa_hwparams, (unsigned int*)&sc->alsa_fragcount, NULL)) < 0))
                {
                    logw("MSGTR_AO_ALSA_UnableToSetPeriods");
                    pthread_mutex_unlock(&sc->mutex);
                    return 0;
                }
                else
                {
                    logv("alsa-init: fragcount=%i\n", sc->alsa_fragcount);
                }
            }
//#endif//end SET_CHUNKSIZE

            if ((err = snd_pcm_hw_params(sc->alsa_handler, sc->alsa_hwparams)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetHwParameters");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            if ((err = snd_pcm_hw_params(sc->alsa_sub_handler, sc->alsa_hwparams)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetHwParameters");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }

            // gets buffersize for control
            if ((err = snd_pcm_hw_params_get_buffer_size(sc->alsa_hwparams, &bufsize))
                    < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToGetBufferSize");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            else
            {
                pcm_params.buffersize = bufsize * sc->bytes_per_sample;
                logv("alsa-init: got buffersize=%i\n", pcm_params.buffersize);
            }

            if ((err = snd_pcm_hw_params_get_period_size(sc->alsa_hwparams,
                       &sc->chunk_size, NULL)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToGetPeriodSize");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            else
            {
                logv("alsa-init: got period size %li\n", sc->chunk_size);
            }
            pcm_params.outburst = sc->chunk_size * sc->bytes_per_sample;

            /* setting software parameters */
            if ((err = snd_pcm_sw_params_current(sc->alsa_handler, sc->alsa_swparams)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToGetSwParameters");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            if ((err = snd_pcm_sw_params_current(sc->alsa_sub_handler, sc->alsa_swparams)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToGetSwParameters");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }

#if SND_LIB_VERSION >= 0x000901
            if ((err = snd_pcm_sw_params_get_boundary(sc->alsa_swparams, &boundary)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToGetBoundary");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
#else
            boundary = 0x7fffffff;
#endif
            /* start playing when one period has been written */
            if ((err = snd_pcm_sw_params_set_start_threshold(sc->alsa_handler,
                       sc->alsa_swparams, sc->chunk_size)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetStartThreshold");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            if ((err = snd_pcm_sw_params_set_start_threshold(sc->alsa_sub_handler,
                       sc->alsa_swparams, sc->chunk_size)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetStartThreshold");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }

            /* disable underrun reporting */
            if ((err = snd_pcm_sw_params_set_stop_threshold(sc->alsa_handler,
                       sc->alsa_swparams, boundary)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetStopThreshold");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            if ((err = snd_pcm_sw_params_set_stop_threshold(sc->alsa_sub_handler,
                    sc->alsa_swparams, boundary)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetStopThreshold");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }

#if SND_LIB_VERSION >= 0x000901
            /* play silence when there is an underrun */
            if ((err = snd_pcm_sw_params_set_silence_size(sc->alsa_handler,
                       sc->alsa_swparams, boundary)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetSilenceSize");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            if ((err = snd_pcm_sw_params_set_silence_size(sc->alsa_sub_handler,
                       sc->alsa_swparams, boundary)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToSetSilenceSize");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }

#endif
            if ((err = snd_pcm_sw_params(sc->alsa_handler, sc->alsa_swparams)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToGetSwParameters");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }
            if ((err = snd_pcm_sw_params(sc->alsa_sub_handler, sc->alsa_swparams)) < 0)
            {
                logw("MSGTR_AO_ALSA_UnableToGetSwParameters");
                pthread_mutex_unlock(&sc->mutex);
                return 0;
            }

            /* end setting sw-params */
            logv("alsa: %d Hz/%d channels/%d bpf/%d bytes buffer\n",
                 pcm_params.samplerate, pcm_params.channels,
                 (int)sc->bytes_per_sample, pcm_params.buffersize);

        } // end switch alsa_handler (spdif)
        sc->alsa_can_pause = snd_pcm_hw_params_can_pause(sc->alsa_hwparams);

    }
    else if(sc->eStatus == SD_STATUS_PAUSED)
    {
        logw("alsa-start in paused status... \n");

        if (!sc->alsa_handler || !sc->alsa_sub_handler)
        {
            loge("====================alsa_handler is null==================== \n");
            abort();
        }

        if(snd_pcm_state(sc->alsa_handler) == SND_PCM_STATE_SUSPENDED) //it is right?
        {
            logw("MSGTR_AO_ALSA_PcmInSuspendModeTryingResume");
            while((err = snd_pcm_resume(sc->alsa_handler)) == -EAGAIN)
                sleep(1);
        }
        if(snd_pcm_state(sc->alsa_sub_handler) == SND_PCM_STATE_SUSPENDED) //it is right?
        {
            logw("MSGTR_AO_ALSA_PcmInSuspendModeTryingResume");
            while((err = snd_pcm_resume(sc->alsa_sub_handler)) == -EAGAIN)
                sleep(1);
        }

        if(0 && sc->alsa_can_pause) //TODO fix
        {
            if((err = snd_pcm_pause(sc->alsa_handler, 0)) < 0)
            {
                logw("MSGTR_AO_ALSA_PcmResumeError");
                pthread_mutex_unlock(&sc->mutex);
                return -1;
            }
            if((err = snd_pcm_pause(sc->alsa_sub_handler, 0)) < 0)
            {
                logw("MSGTR_AO_ALSA_PcmResumeError");
                pthread_mutex_unlock(&sc->mutex);
                return -1;
            }

            logw("alsa-resume: resume supported by hardware\n");
        }
        else
        {
            if((err = snd_pcm_prepare(sc->alsa_handler)) < 0)
            {
                logw("MSGTR_AO_ALSA_PcmPrepareError");
                pthread_mutex_unlock(&sc->mutex);
                return -1;
            }
            if((err = snd_pcm_prepare(sc->alsa_sub_handler)) < 0)
            {
                logw("MSGTR_AO_ALSA_PcmPrepareError");
                pthread_mutex_unlock(&sc->mutex);
                return -1;
            }

        }
    }

    sc->eStatus = SD_STATUS_STARTED;
    pthread_mutex_unlock(&sc->mutex);

    return 0;
}


static int __Stop(SoundCtrl* s)
{
    int               ret;
    SoundCtrlContext* sc;

    sc = (SoundCtrlContext*)s;

    ret = SoundDeviceStop_l(sc);

    return ret;
}


static int SoundDeviceStop_l(SoundCtrlContext* sc)
{
    int err = 0;

    if(sc->eStatus == SD_STATUS_STOPPED)
    {
        logw("Sound device already stopped.");
        return 0;
    }
    if (sc->alsa_handler==NULL || sc->alsa_sub_handler==NULL)
    {
            logw("SoundDeviceStop_l Sound device NULLL.");
            sc->eStatus = SD_STATUS_STOPPED;
            return 0;
    }

    if ((err = snd_pcm_drop(sc->alsa_handler)) < 0)
    {
        logw("MSGTR_AO_ALSA_PcmPrepareError");
        return err;
    }
    if ((err = snd_pcm_drop(sc->alsa_sub_handler)) < 0)
    {
        logw("MSGTR_AO_ALSA_PcmPrepareError");
        return err;
    }

    if ((err = snd_pcm_prepare(sc->alsa_handler)) < 0)
    {
        logw("MSGTR_AO_ALSA_PcmPrepareError");
        return err;
    }
    if ((err = snd_pcm_prepare(sc->alsa_sub_handler)) < 0)
    {
        logw("MSGTR_AO_ALSA_PcmPrepareError");
        return err;
    }

    if(sc->eStatus != SD_STATUS_STOPPED)
    {
        if (sc->alsa_handler)
        {
            int err;
            if ((err = snd_pcm_close(sc->alsa_handler)) < 0)
            {
                logw("MSGTR_AO_ALSA_PcmCloseError");
            }
            else
            {
                sc->alsa_handler = NULL;
                logd("alsa-uninit: pcm closed\n");
            }
        }
        else
        {
            logw("MSGTR_AO_ALSA_NoHandlerDefined");
        }
        if (sc->alsa_sub_handler)
        {
            int err;
            if ((err = snd_pcm_close(sc->alsa_sub_handler)) < 0)
            {
                logw("MSGTR_AO_ALSA_PcmCloseError");
            }
            else
            {
                sc->alsa_sub_handler = NULL;
                logd("alsa-uninit: pcm closed\n");
            }
        }
        else
        {
            logw("MSGTR_AO_ALSA_NoHandlerDefined");
        }

        sc->eStatus = SD_STATUS_STOPPED;
    }
    return err;
}

static int __Pause(SoundCtrl* s)
{
    SoundCtrlContext* sc;

    sc = (SoundCtrlContext*)s;
    int err;

    if(sc->eStatus != SD_STATUS_STARTED)
    {
        logw("alsa-pause: not in started status,ignore...!\n");
        return -1;
    }

    if (0 && sc->alsa_can_pause) //TODO fix
    {
        logw("alsa-pause: pause supported by hardware\n");
        if ((err = snd_pcm_pause(sc->alsa_handler, 1)) < 0)
        {
            logw("MSGTR_AO_ALSA_PcmPauseError");
            return -1;
        }
        if ((err = snd_pcm_pause(sc->alsa_sub_handler, 1)) < 0)
        {
            logw("MSGTR_AO_ALSA_PcmPauseError");
            return -1;
        }

    }
    else
    {
        logw("alsa-pause: pause not supported by hardware\n");
        if(sc->alsa_handler!=NULL)
        {
            if ((err = snd_pcm_drop(sc->alsa_handler)) < 0)
            {
                logw("MSGTR_AO_ALSA_PcmDropError");
            }
        }
        if(sc->alsa_sub_handler!=NULL)
        {
            if ((err = snd_pcm_drop(sc->alsa_sub_handler)) < 0)
            {
                logw("MSGTR_AO_ALSA_PcmDropError");
            }
        }

    }

    sc->eStatus = SD_STATUS_PAUSED;

    return 0;
}


static int __Write(SoundCtrl* s, void* pData, int nDataSize)
{
    SoundCtrlContext* sc;

    sc = (SoundCtrlContext*)s;

    if(sc->eStatus == SD_STATUS_STOPPED || sc->eStatus == SD_STATUS_PAUSED)
    {
        return 0;
    }

    int num_frames = nDataSize / sc->bytes_per_sample;
    snd_pcm_sframes_t res = 0;

    logv("alsa-play: frames=%i, len=%i\n",num_frames,nDataSize);
    if (!sc->alsa_handler)
    {
        logw("MSGTR_AO_ALSA_DeviceConfigurationError");
        return 0;
    }

    if (num_frames == 0)
        return 0;

    if (sc->volume == -1)
    {
        char volumeChars[4] = "";
        int volumeValue = 100;
        char muteChars[8] = "";
        int muteValue = 0;
        if(volume_uci_get_config("volume", volumeChars, sizeof(volumeChars)) != -1)
        {
            volumeValue = atoi(volumeChars);
        }
        if(volume_uci_get_config("mute", muteChars, sizeof(muteChars)) != -1)
        {
            muteValue = atoi(muteChars);
        }

        sc->mute = muteValue;
        sc->volume = (muteValue == 0) ? volumeValue : 0;
        logd("init soundControl_alsa mute=%d, volume=%d",sc->mute,sc->volume);
    }

    if (sc->mute != 0 || (sc->volume / 5 - 20) != 0)
    {
        AudioGain audioGain;
        audioGain.preamp = (sc->mute == 0) ? (sc->volume / 5 - 20) : -20;
        audioGain.InputChan = (int)pcm_params.channels;
        audioGain.OutputChan = (int)pcm_params.channels;
        audioGain.InputLen = nDataSize;
        audioGain.InputPtr = (short*)pData;
        audioGain.OutputPtr = (short*)pData;
        int gainRet = tina_do_AudioGain(&audioGain);
        if(gainRet == 0)
        {
            loge("tina_do_AudioGain fail");
        }
    }

    do
    {
        res = snd_pcm_writei(sc->alsa_handler, pData, num_frames);
        if (res == -EINTR)
        {
            /* nothing to do */
            res = 0;
        }
        else if (res == -ESTRPIPE)
        {
            /* suspend */
            logw("MSGTR_AO_ALSA_PcmInSuspendModeTryingResume");
            while ((res = snd_pcm_resume(sc->alsa_handler)) == -EAGAIN)
                sleep(1);
        }
        if (res < 0)
        {
            logw("MSGTR_AO_ALSA_WriteError");
            if ((res = snd_pcm_prepare(sc->alsa_handler)) < 0)
            {
                logw("MSGTR_AO_ALSA_PcmPrepareError");
                return 0;
                break;
            }
        }
    }
    while (res == 0);

    return res < 0 ? res : res * sc->bytes_per_sample;
}


//* called at player seek operation.
static int __Reset(SoundCtrl* s)
{
    return SoundDeviceStop(s);
}


static int __GetCachedTime(SoundCtrl* s)
{
    SoundCtrlContext* sc;

    sc = (SoundCtrlContext*)s;

    if (sc->alsa_handler)
    {
        snd_pcm_sframes_t delay;

        if (snd_pcm_delay(sc->alsa_handler, &delay) < 0)
            return 0;

        if (delay < 0)
        {
            /* underrun - move the application pointer forward to catch up */
#if SND_LIB_VERSION >= 0x000901 /* snd_pcm_forward() exists since 0.9.0rc8 */
            snd_pcm_forward(sc->alsa_handler, -delay);
#endif
            delay = 0;
        }
        return ((int)((float) delay * 1000000 / (float) pcm_params.samplerate));
    }
    else
    {
        return 0;
    }

}

static int __GetFrameCount(SoundCtrl* s)
{
    return 0;
}

static int __Control(SoundCtrl* s, int cmd, void* para)
{
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;

    pthread_mutex_lock(&sc->mutex);
    switch(cmd)
    {
        default:
            logw("__Control: unknown command (%d)...", cmd);
            break;
    }
    pthread_mutex_unlock(&sc->mutex);

    return 0;
}

static SoundControlOpsT mSoundControlOps =
{
    .destroy       =         __Release,
    .setFormat     =         __SetFormat,
    .start         =         __Start,
    .stop          =         __Stop,
    .pause         =         __Pause,
    .write         =         __Write,
    .reset         =         __Reset,
    .getCachedTime =         __GetCachedTime,
    .getFrameCount =         __GetFrameCount,
    .control       =         __Control,
};

SoundCtrl* SoundDeviceCreate()
{
    SoundCtrlContext* s;
    int card;
    logd("==   SoundDeviceInit");
    s = (SoundCtrlContext*)malloc(sizeof(SoundCtrlContext));
    if(s == NULL)
    {
        loge("malloc memory fail.");
        return NULL;
    }
    memset(s, 0, sizeof(SoundCtrlContext));

    s->base.ops = &mSoundControlOps;

    s->eStatus          = SD_STATUS_STOPPED;
    s->alsa_fragcount   = 16;
    s->volume   = -1;
    s->mute   = 0;
    for (card = 0; card < MAX_AUDIO_DEVICES; card++)
    {
        do_init_audio_card(card) != 0;
    }
    do_init_audio_mixer();
    pthread_mutex_init(&s->mutex, NULL);
    return (SoundCtrl*)s;
}

int SoundDeviceSetVolume(SoundCtrl* s, int volume){
    logd("SoundDeviceSetVolume(%d)", volume);
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if(sc)
    {
        if(volume > 100)
        {
            volume = 100;
        }
        else if(volume < 0)
        {
            volume = 0;
        }

        char volumeChars[4] = "";
        sprintf(volumeChars, "%d", volume);
        sc->volume = volume;

        return volume_uci_set_config("volume", volumeChars);
    }
    else
    {
        return -1;
    }
}

int SoundDeviceGetVolume(SoundCtrl* s, int* volume){
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if(sc)
    {
        if (sc->volume == -1)
        {
            char volumeChars[4] = "";
            int volumeValue = 100;
            char muteChars[8] = "";
            int muteValue = 0;
            if(volume_uci_get_config("volume", volumeChars, sizeof(volumeChars)) != -1)
            {
                volumeValue = atoi(volumeChars);
            }
            if(volume_uci_get_config("mute", muteChars, sizeof(muteChars)) != -1)
            {
                muteValue = atoi(muteChars);
            }

            sc->volume = (muteValue == 0) ? volumeValue : 0;
        }
        *volume = sc->volume;

        logd("SoundDeviceGetVolume(%d)", *volume);
        return 0;
    }
    else
    {
        return -1;
    }
}

int SoundDeviceSetMute(SoundCtrl* s, int mute){
    logd("SoundDeviceSetMute(%d)", mute);

    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if(sc)
    {
        if(mute != 0)
        {
            mute = 1;
        }

        char muteChars[8] = "";
        sprintf(muteChars, "%d", mute);

        return volume_uci_set_config("volume", muteChars);
    }
    else
    {
        return -1;
    }
}

int SoundDeviceGetMute(SoundCtrl* s, int* mute){
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if(sc)
    {
        char volumeChars[4] = "";
        int volumeValue = 100;
        char muteChars[8] = "";
        int muteValue = 0;
        if(volume_uci_get_config("volume", volumeChars, sizeof(volumeChars)) != -1)
        {
            volumeValue = atoi(volumeChars);
        }
        if(volume_uci_get_config("mute", muteChars, sizeof(muteChars)) != -1)
        {
            muteValue = atoi(muteChars);
        }

        *mute = (muteValue == 0) ? (volumeValue == 0) : muteValue;

        return 0;
    }
    else
    {
        return -1;
    }
}
