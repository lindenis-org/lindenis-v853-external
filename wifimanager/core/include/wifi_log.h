#ifndef __WIFI_LOG_H
#define __WIFI_LOG_H

#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include "log_core.h"

#ifndef DEFAULT_LOG_FILE
#define DEFAULT_LOG_FILE "/tmp/wifi_log.log"
#endif

#ifndef DEFAULT_LOG_CMD_FIFO
#define DEFAULT_LOG_CMD_FIFO "/tmp/wifi_debug"
#endif

#ifndef DEBUG_TAG
#define DEBUG_TAG ""
#endif

#if __cplusplus
extern "C" {
#endif

#define CONFIG_DEBUG_FUNCTION_LINE 1
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

extern int wmg_debug_level;
extern int wmg_debug_show_keys;
extern int wmg_debug_timestamp;

#ifdef __GNUC__
#define PRINTF_FORMAT(a,b) __attribute__ ((format (printf, (a), (b))))
#define STRUCT_PACKED __attribute__ ((packed))
#else
#define PRINTF_FORMAT(a,b)
#define STRUCT_PACKED
#endif

typedef enum wmg_prtk_level{
	MSG_NONE = 0,
	MSG_ERROR,
	MSG_WARNING,
	MSG_INFO,
	MSG_DEBUG,
	MSG_MSGDUMP,
	MSG_EXCESSIVE
}wmg_prtk_level;

#ifdef CONFIG_NO_STDOUT_DEBUG

#define wmg_printf(args...) do { } while (0)
#define wmg_debug_open_file(p) do { } while (0)
#define wmg_debug_close_file() do { } while (0)

#else
int wmg_set_debug_level(int level);
int wmg_get_debug_level();

#ifdef CONFIG_DEBUG_FUNCTION_LINE
#define wmg_printf(level,fmt,arg...) \
	wmg_print(level,"%s:%u: " fmt "\n",__FUNCTION__,__LINE__,##arg)
#else
#define wmg_printf(level,fmt,arg...) \
	wmg_print(level,fmt "\n",##arg)
#endif /*CONFIG_DEBUG_FUNCTION_LINE*/

#define WMG_PRTK(level,fmt,arg...) \
	wmg_print(level,"WMG [%s:%s:%u]:  " fmt "",__FILE__,__func__,__LINE__,##arg)

#define WMG_DEBUG(fmt,arg...) \
	wmg_print(MSG_DEBUG,"WMG_DEBUG [%s:%s:%u]:  " fmt,__FILE__,__func__,__LINE__,##arg)

#define WMG_INFO(fmt,arg...) \
	wmg_print(MSG_INFO,"WMG_INFO [%s:%s:%u]:  " fmt,__FILE__,__func__,__LINE__,##arg)

#define WMG_WARNG(fmt,arg...) \
	wmg_print(MSG_WARNING,"WMG_WARNG [%s:%s:%u]:  " fmt,__FILE__,__func__,__LINE__,##arg)

#define WMG_ERROR(fmt,arg...) \
	wmg_print(MSG_ERROR,"WMG_ERROR [%s:%s:%u]:  " fmt,__FILE__,__func__,__LINE__,##arg)

#define WMG_DUMP(fmt,arg...) \
	wmg_print(MSG_MSGDUMP,"WMG_DUMP [%s:%s:%u]: " fmt,__FILE__,__func__,__LINE__,##arg)

#define WMG_EXCESSIVE(fmt,arg...) \
	wmg_print(MSG_EXCESSIVE,"WMG_EXCESSIVE [%s:%s:%u]: " fmt,__FILE__,__func__,__LINE__,##arg)

// void wmg_print(int level, const char *fmt, ...)
// PRINTF_FORMAT(2, 3);
#define wmg_print(level, fmt, arg...) log_print(DEBUG_TAG, level,fmt, ##arg)

#endif/* CONFIG_NO_STDOUT_DEBUG */

#if __cplusplus
};  // extern "C"
#endif

#endif //__WIFI_LOG_H
