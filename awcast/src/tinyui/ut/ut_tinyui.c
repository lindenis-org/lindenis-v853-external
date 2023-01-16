#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <linux/input.h>
#include <pthread.h>

/*if run gui as a thread,it must be include these head file*/
#include <minigui/common.h>
#include <minigui/minigui.h>
#ifdef _MGRM_THREADS
#include <minigui/dti.c>
#endif
/*end*/

#include <tinyui.h>
#include <cdx_log.h>

#define WIDGET_PROCESS_BAR "progress_bar"
#define MD_POSITION "metadata.position"
#define MD_DURATION "metadata.duration"


int MiniGUIMain (int argc, const char *argv[])
{
	if (argc < 3)
	{
		CDX_LOGE("useage:\n"
				 "\t ut_tui [layout file] [widget]");
		return 1;
	}

	const char *layout_file = argv[1];
	const char *widget = argv[2];

	TinyUIT *tui;

	tui = TUI_Instance(layout_file);

	TUI_MetedataSet(tui, MD_DURATION, "1:00:00");

	TUI_MetedataSet(tui, MD_POSITION, "2:00:00");

	TUI_WidgetShow(tui,widget);

    while (1)
    {
        sleep(3);
	//	TUI_WidgetHide(tui, widget);
		sleep(3);
    }
    return 0;
}


