#include <stdarg.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdint.h>
#include <stdbool.h>

#include <wifi_log.h>

int wmg_debug_level = 2;

int wmg_set_debug_level(int level)
{
	wmg_debug_level = level;
	return log_set_debug_level((log_level_t)level);
}

int wmg_get_debug_level()
{
	return log_get_debug_level();
}
