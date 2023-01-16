#ifndef _DragonBoard_H_
#define _DragonBoard_H_

#include <linux/rtc.h>
#include <cutils/log.h>

#define CATEGORY_AUTO                   0
#define CATEGORY_MANUAL                 1
#define CATEGORY_WIFI                   2

#define RUN_TYPE_WAIT_COMPLETION        0
#define RUN_TYPE_NO_WAIT_COMPLETION     1

struct testcase_base_info
{
    char name[32];
    char display_name[68];
    int enabled;
    //char binary[20];
    //int id;
    int category; /* 0: auto, 1: manual, 2: wifi */
    int run_type; /* 0: wait completion, 1: no wait completion */
};
typedef struct testcase_base_info testcase_base_info_t;

struct fifo_param {
    char type;                  // A: autotester, H: handtester
    char name[10];              // tester-name
    char result;                // F: Fail, P: Pass, W: waiting
    union {
        float acc[3];           // acc[0-2]: x,y,z
        struct rtc_time rtc;        // rtc[0-5]: Y:M:D:H:M:S
        int MemTotal;           // DRAM capacity
    }val;
};
typedef struct fifo_param fifo_param_t;

//#define WIFI_TEST
#define GSENSOR_TEST
//#define GPS_TEST

/**************SYSTREM**************/
#define FIFO_TF_DEV   "/tmp/fifo_tf"
#define FIFO_CPU_DEV  "/tmp/fifo_cpu"
#ifdef WIFI_TEST
#define FIFO_WIFI_DEV "/tmp/fifo_wifi"
#endif
#ifdef GPS_TEST
#define FIFO_GPS_DEV  "/tmp/fifo_gps"
#endif
#ifdef GSENSOR_TEST
#define FIFO_GSENSOR_DEV  "/tmp/fifo_gsensor"
#endif
#define FIFO_KEY_DEV  "/tmp/fifo_key"
#define FIFO_RTC_DEV  "/tmp/fifo_rtc"
#define FIFO_LED_DEV  "/tmp/fifo_led"
#define FIFO_USB_DEV  "/tmp/fifo_usb"
#define FIFO_DDR_DEV  "/tmp/fifo_ddr"
#define FIFO_NOR_DEV  "/tmp/fifo_nor"
#define FIFO_NAND_DEV  "/tmp/fifo_nand"

/***************MPP*****************/
//#define FIFO_CSI_DEV     "/tmp/fifo_csi"
//#define FIFO_SPK_MIC_DEV "/tmp/fifo_spk_mic"
//#define FIFO_VENC_DEV    "/tmp/fifo_venc"
//#define FIFO_VDEC_DEV    "/tmp/fifo_vdec"
//#define FIFO_ISP_DEV     "/tmp/fifo_isp"
//#define FIFO_G2D_DEV     "/tmp/fifo_g2d"
//#define FIFO_CE_DEV      "/tmp/fifo_ce"
//#define FIFO_VO_DEV      "/tmp/fifo_vo"

#include <log/log.h>
#define DB_ERROR
#define DB_WARN
#define SCREENT_W 640
#define SCREENT_H 480
#ifdef DB_FATAL
#define db_fatal(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_FATAL, fmt, ##arg); \
    } while(0)
#else
#define db_fatal(fmt, arg...)
#endif

#ifdef DB_ERROR
#define db_error(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_ERROR, fmt, ##arg); \
    } while(0)
#else
#define db_error(fmt, arg...)
#endif

#ifdef DB_WARN
#define db_warn(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_WARN, fmt, ##arg); \
    } while(0)
#else
#define db_warn(fmt, arg...)
#endif

#ifdef DB_INFO
#define db_info(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_INFO, fmt, ##arg); \
    } while(0)
#else
#define db_info(fmt, arg...)
#endif

#ifdef DB_DEBUG
#define db_debug(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_INFO, fmt, ##arg); \
    } while(0)
#else
#define db_debug(fmt, arg...)
#endif

#ifdef DB_MSG
#define db_msg(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_INFO, fmt, ##arg); \
    } while(0)
#else
#define db_msg(fmt, arg...)
#endif

#endif /* _DragonBoard_H_ */
