#define LOG_TAG "MiracastReceiver"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "miracast2.h"
#include <cdx_log.h>

static int stop = 0;

static int MT_Miracast_Event_CB(MIRACAST_EVENT_CALLBACK_E enEvent, void* pvPrivateData)
{
	CDX_LOGD("event '%d'", enEvent);
	switch (enEvent)
	{
		case MIRACAST_CBK_P2P_FOUND:
		{

		MIRACAST_P2P_CONNECTING_INFO *info = (MIRACAST_P2P_CONNECTING_INFO *)pvPrivateData;
		CDX_LOGD("'%s' connecting...", info->sourceName);

		break;
		}
		default:
		break;
	}
	return 0;
}

void sigint_handler(int __unused)
{
	CDX_LOGD("recive 'stop'");
    stop = 1;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sigint_handler);

	Miracast_Init(0);
//	Miracast_ModifyName("wfd-modify");

	Miracast_Start("wfd-modify", MT_Miracast_Event_CB);

	while (!stop)
	{
		int state = Miracast_GetState();
//		CDX_LOGD("[running] state '%d'", state);
		usleep(2000000);
	}
	CDX_LOGD("stop...");

	Miracast_Stop();

	Miracast_DeInit();

    return 0;
}

