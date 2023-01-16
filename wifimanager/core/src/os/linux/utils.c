#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "utils.h"
#include "wifi_log.h"

int check_process_is_exist(const char *process_name, int length)
{
	int bytes;
	int ret;
	char buf[CMD_MAX_LEN];
	char cmd[CMD_MAX_LEN];
    FILE *strea = NULL;

	if (length > PROG_NAME_MAX_LEN) {
		WMG_ERROR("process name '%s' is too long!\n", process_name);
		return -EINVAL;
	}

	sprintf(cmd, "ps | grep %s | grep -v grep", process_name);
    strea = popen(cmd, "r");
    if (NULL == strea)
		return -ESRCH; /* No such process */

    bytes = fread(buf, sizeof(char), sizeof(buf), strea);
    if (bytes > 0){
        WMG_EXCESSIVE("%s exist\n", process_name);
		ret = WMG_TRUE;
    } else {
        WMG_EXCESSIVE("%s not exist\n", process_name);
        ret = WMG_FALSE;
    }
	pclose(strea);

	return ret;
}
