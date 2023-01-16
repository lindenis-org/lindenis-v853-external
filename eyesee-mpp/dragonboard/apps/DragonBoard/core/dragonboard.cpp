#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "minigui/common.h"
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>
#include <minigui/ctrl/edit.h>
#include <mm_common.h>
#include "mm_comm_video.h"
#include <vo/hwdisplay.h>
#include <mpi_sys.h>
#include <mpi_vo.h>
#include "lua/lua_config_parser.h"
#include <cutils/memory.h>
#include <utils/plat_log.h>

#define TAG "MiniGUI"
#include <dragonboard/dragonboard.h>

extern "C" {
#include "script_parser.h"
#include "script.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
}

using namespace std;

#define DRAGONBOARD_TEST_PATH "/usr/bin/testcase"
#define DRAGONBOARD_TEST_CONFIG_NAME "/usr/bin/test_config.fex"
#define DRAGONBOARD_TEST_RESULT_FILE "/mnt/extsd/test_result.txt"

#define FBIO_CACHE_SYNC         0x4630

#define FILE_EXIST(PATH)   (access(PATH, F_OK) == 0)
#define FILE_READABLE(PATH)   (access(PATH, R_OK) == 0)
#define TEST_OK "Test OK"
#define TEST_FAIL "Test fail"

#define LCDR   107
#define LCDG   108
#define LCDB   109
#define REFRESH_TIMER  110

// whnd ID define
enum{
    ID_TF = 111,
    ID_CPU,
#ifdef WIFI_TEST
    ID_WIFI,
#endif
#ifdef GPS_TEST
    ID_GPS,
#endif
#ifdef GSENSOR_TEST
    ID_GSENSOR,
#endif
    ID_KEY,
    ID_RTC,
    ID_LED,
    ID_USB,
    ID_DDR,
    ID_NOR,
    ID_NAND,
    ID_MIC_SPK,
    ID_CSI,
    ID_VENC,
    ID_VDEC,
    ID_ISP,
    ID_G2D,
    ID_CE,
    ID_VO,
    ID_RESULT,
};

#define PATH_LEN 50
#define TEST_FOR_QC 1
#define X_start 0
#define Y_start 0

#define CHECK_OK 0
#define CHECK_FAIL 1

#define TEST_KEY_BIT	0xFFFFFFEF
#define TEST_TP_BIT		0xFFFFDFFF
#define TEST_ETHERNET_BIT	0xFFFFBFFF
#define TEST_RTC_BIT	0xFFFFFEFF

// content that read from fifo will display on LCD
static char tf_str[PATH_LEN];
static char cpu_str[PATH_LEN];
#ifdef WIFI_TEST
static char wifi_str[PATH_LEN];
#endif
#ifdef GPS_TEST
static char gps_str[PATH_LEN];
#endif
#ifdef GSENSOR_TEST
static char gsensor_str[PATH_LEN];
#endif
static char key_str[PATH_LEN];
static char rtc_str[PATH_LEN];
static char led_str[PATH_LEN];
static char usb_str[PATH_LEN];
static char ddr_str[PATH_LEN];
static char nor_str[PATH_LEN];
static char nand_str[PATH_LEN];
static char spk_mic_str[PATH_LEN];
static char csi_str[PATH_LEN];
static char venc_str[PATH_LEN];
static char vdec_str[PATH_LEN];
static char isp_str[PATH_LEN];
static char g2d_str[PATH_LEN];
static char ce_str[PATH_LEN];
static char vo_str[PATH_LEN];

/* Test case switch */
static int  testTf = 1;
static char testTfPath[PATH_LEN] = "/usr/bin/testcase/tftester";    // if it can NOT be found, use /system/bin/ by system()
static int  testCPU = 0;//1;
static char testCPUPatth[PATH_LEN] = "/usr/bin/testcase/CPUtest";
#ifdef WIFI_TEST
static int  testWifi = 0;//1;
static char testWifiPath[PATH_LEN] = "/usr/bin/testcase/wifitester";
#endif
#ifdef GPS_TEST
static int  testGPS = 0;//1;
static char testGPSPath[PATH_LEN] = "/usr/bin/testcase/gpstester";
#endif
#ifdef GSENSOR_TEST
static int  testGSENSOR = 0;//1;
static char testGSENSORPath[PATH_LEN] = "/usr/bin/testcase/gsensortester";
#endif
static int  testKey = 0;//1;
static char testKeyPath[PATH_LEN] = "/usr/bin/testcase/keytester";
static int  testRtc = 1;
static char testRtcPath[PATH_LEN] = "/usr/bin/testcase/rtctester";
static int  testLed = 1;
static char testLedPath[PATH_LEN] = "/usr/bin/testcase/ledtester";
static int  testUsb = 1;
static char testUsbPath[PATH_LEN] = "/usr/bin/testcase/usbtester";
static int  testDdr = 1;
static char testDdrPath[PATH_LEN] = "/usr/bin/testcase/ddrtester";
static int  testNor = 1;
static char testNorPath[PATH_LEN] = "/usr/bin/testcase/nortester";
static int  testNand = 0;//1;
static char testNandPath[PATH_LEN] = "/usr/bin/testcase/nandtester";

// MPP
static int  testSpk_MIC = 1;
static int  testCSI = 1;
static int  testVENC = 1;
static int  testVDEC = 1;
static int  testISP = 1;
static int  testG2D = 1;
static int  testCE = 1;
static int  testVO = 1;

static int lcdWidth, lcdHeight;
static int chipType;            // 0-V3; 1-A20
static PLOGFONT mLogFont;
static char MainWin_Title[200]={0};
static int IsResultOver = 0;
static int textout_i = 0;

static int gYstep = 0;
static int gMaxListNum = 0;

#define y_step  80
#define m_height 60

#define FUNCTION_ENTER_PRINT \
do { \
    alogd("************%s enter************", __func__); \
} while (0)

#define FUNCTION_LEAVE_PRINT \
do { \
    alogd("************%s leave************", __func__); \
} while (0)

static int super_system(const char *cmd, char *retmsg, int msg_len)
{
    FILE *fp = NULL;
    int res = 0;
    char resStr[5] = {0};
    if (cmd == NULL || retmsg == NULL || msg_len < 0)
    {
        printf("fatal error, %s system paramer invalid!\n", __func__);
        return 1;
    }

    if ((fp = popen(cmd, "r") ) == NULL)
    {
        perror("popen");
        printf("fatal error, %s popen error: %s\n", __func__, strerror(errno));
        return 2;
    }
    else
    {
        memset(retmsg, 0, msg_len);
        while(fgets(retmsg, msg_len, fp));
        {
            printf("%s fgets buf is %s\n", __func__, retmsg);
        }

        strncpy(resStr, retmsg, sizeof(resStr));
        alogd("cmd[%s] result[%s]\n", cmd, resStr);
        if (!strcmp(resStr, "FAIL"))
        {
            res = -1;
        }

        if ((res = pclose(fp)) == -1)
        {
            printf("fatal error, %s close popen file pointer fp error!\n", __func__);
            return 3;
        }

        //drop #012 from system result retmsg.
        retmsg[strlen(retmsg)-1] = '\0';
        return res;
    }
}

static long int  HelloWinProc(HWND hWnd, unsigned int message, WPARAM wParam, LPARAM lParam)
{
    int keyVal;
    static clock_t timestamp1, timestamp2;
    static HWND hHandTester, hAutoTester,hMicBar;
    static HWND hwnd_tf,hwnd_CPU,hwnd_key;
    static HWND hwnd_rtc,hwnd_led,hwnd_usb,hwnd_ddr,hwnd_nor,hwnd_nand;
#ifdef WIFI_TEST
    static HWND hwnd_wifi;
#endif
#ifdef GPS_TEST
    static HWND hwnd_gps;
#endif
#ifdef GSENSOR_TEST
    static HWND hwnd_gsensor;
#endif
	static HWND hwnd_CSI,hwnd_mic_spk;
	static HWND hwnd_VENC,hwnd_VDEC,hwnd_ISP,hwnd_G2D,hwnd_CE,hwnd_VO;
    static HWND hwnd_lcdr, hwnd_lcdg, hwnd_lcdb;
	static HWND hwnd_result;
    static HWND tmp;
	int index_x = 0;
	int index_y = 0;
	int index_y1 = 0;
    int *pIndexTmp = NULL;
    int xTmp = 0;
	HDC hdc;
	char test_out[4][20]={"Test Waiting","Test Waiting.","Test Waiting..","Test Waiting..."};
    LOGFONT *pLogFont = NULL;

    switch (message) {

        case MSG_CREATE:
            aloge("MSG_CREATE============");
            if (testTf) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_tf = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_TF,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_tf, COLOR_lightwhite);
				(*pIndexTmp)++;
				aloge("SetWindowBkColor tf");
            }
            if (testCPU) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_CPU = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_CPU,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_CPU, COLOR_lightwhite);
				(*pIndexTmp)++;
            }
#ifdef WIFI_TEST
            if (testWifi) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_wifi = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_WIFI,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_wifi, COLOR_lightwhite);
				(*pIndexTmp)++;
            }
#endif

#ifdef GPS_TEST
            if (testGPS) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_wifi = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_GPS,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_gps, COLOR_lightwhite);
                (*pIndexTmp)++;
            }
#endif

#if 0
            if (testGSENSOR) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_gsensor = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_GSENSOR,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_gsensor, COLOR_lightwhite);
                (*pIndexTmp)++;
            }
#endif

            if (testKey){
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_key = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_KEY,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_key, COLOR_lightwhite);
				(*pIndexTmp)++;
            }

            if (testRtc) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_rtc = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_RTC,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_rtc, COLOR_lightwhite);
				(*pIndexTmp)++;
            }

            if (testLed) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_led = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_LED,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_led, COLOR_lightwhite);
				(*pIndexTmp)++;
            }

            if (testUsb) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_usb = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_USB,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_usb, COLOR_lightwhite);
				(*pIndexTmp)++;
            }

            if (testDdr) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_ddr = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_DDR,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_ddr, COLOR_lightwhite);
				(*pIndexTmp)++;
            }
            if (testNor) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_nor = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_NOR,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_nor, COLOR_lightwhite);
				(*pIndexTmp)++;
            }

            if (testNand) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_nand = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_NAND,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_nand, COLOR_lightwhite);
				(*pIndexTmp)++;
            }

			if(testSpk_MIC)
			{
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
			    hwnd_mic_spk= CreateWindow(CTRL_STATIC, NULL,
                       WS_CHILD | WS_VISIBLE, ID_MIC_SPK,
                       xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
                       hWnd, 0);
                SetWindowBkColor(hwnd_mic_spk, COLOR_lightwhite);
                (*pIndexTmp)++;
			}

			if(testCSI)
			{
                 pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                 xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
			     hwnd_CSI = CreateWindow(CTRL_STATIC, NULL,
						WS_CHILD | WS_VISIBLE, ID_CSI,
                        xTmp, *pIndexTmp*gYstep, (lcdWidth-10)/2, m_height,
						hWnd, 0);
				SetWindowBkColor(hwnd_CSI, COLOR_lightwhite);
				(*pIndexTmp)++;
			}

            if(testVENC)
            {
                 pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                 xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                 hwnd_VENC = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_VENC,
                        xTmp, *pIndexTmp*gYstep, lcdWidth/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_VENC, COLOR_lightwhite);
                (*pIndexTmp)++;
            }

            if(testVDEC)
            {
                 pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                 xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                 hwnd_VDEC = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_VDEC,
                        xTmp, *pIndexTmp*gYstep, lcdWidth/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_VDEC, COLOR_lightwhite);
                (*pIndexTmp)++;
            }

            if(testISP)
            {
                 pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                 xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                 hwnd_ISP = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_ISP,
                        xTmp, *pIndexTmp*gYstep, lcdWidth/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_ISP, COLOR_lightwhite);
                (*pIndexTmp)++;
            }

            if(testG2D)
            {
                 pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                 xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                 hwnd_G2D = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_G2D,
                        xTmp, *pIndexTmp*gYstep, lcdWidth/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_G2D, COLOR_lightwhite);
                (*pIndexTmp)++;
            }

            if(testCE)
            {
                 pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                 xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                 hwnd_CE = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_CE,
                        xTmp, *pIndexTmp*gYstep, lcdWidth/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_CE, COLOR_lightwhite);
                (*pIndexTmp)++;
            }

            if(testVO)
            {
                 pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                 xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                 hwnd_VO = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_VO,
                        xTmp, *pIndexTmp*gYstep, lcdWidth/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_VO, COLOR_lightwhite);
                (*pIndexTmp)++;
            }

#ifdef GSENSOR_TEST
            if (testGSENSOR) {
                pIndexTmp = index_y < gMaxListNum ? &index_y : &index_y1;
                xTmp = index_y < gMaxListNum ? X_start : (lcdWidth/2 + 5);
                hwnd_gsensor = CreateWindow(CTRL_STATIC, NULL,
                        WS_CHILD | WS_VISIBLE, ID_GSENSOR,
                        xTmp, *pIndexTmp*gYstep, lcdWidth/2, m_height,
                        hWnd, 0);
                SetWindowBkColor(hwnd_gsensor, COLOR_blue);
                (*pIndexTmp)++;
            }
#endif

			index_y = index_y > index_y1?index_y:index_y1;
			db_error("index_y %d",index_y);
#if 1
            hwnd_lcdr = CreateWindow(CTRL_STATIC, NULL,
                    WS_CHILD | WS_VISIBLE, LCDR,
                   0, 800, lcdWidth, 50,     // make LCD_R/G/B height = 10
                    hWnd, 0);
            pLogFont = CreateLogFont("sxf", "arialuni", "UTF-8",
                FONT_WEIGHT_REGULAR, FONT_SLANT_ROMAN, FONT_SETWIDTH_NORMAL,
                FONT_OTHER_AUTOSCALE, FONT_UNDERLINE_NONE, FONT_STRUCKOUT_NONE, 30, 0);
            SetWindowFont(GetDlgItem(hWnd, LCDR), pLogFont);
            SetWindowBkColor(hwnd_lcdr, COLOR_blue);
            SetWindowElementAttr(hwnd_lcdr, WE_FGC_WINDOW, 0xFFFFFFFF);
            SetDlgItemText(hWnd, LCDR, "          AllwinnerTech DragonBoard Test");
            /*DestroyLogFont(pLogFont);
            pLogFont = NULL;*/
/*
			hwnd_lcdr = CreateWindow(CTRL_STATIC, NULL,
                    WS_CHILD | WS_VISIBLE, LCDR,
                   0, index_y*y_step+40, lcdWidth/3, m_height,     // make LCD_R/G/B height = 10
                    hWnd, 0);
            SetWindowBkColor(hwnd_lcdr, COLOR_red);
            hwnd_lcdg = CreateWindow(CTRL_STATIC, NULL,
                    WS_CHILD | WS_VISIBLE, LCDG,
                    lcdWidth/3, index_y*y_step+40, lcdWidth/3, m_height,
                    hWnd, 0);
            SetWindowBkColor(hwnd_lcdg, COLOR_green);
            hwnd_lcdb = CreateWindow(CTRL_STATIC, NULL,
                    WS_CHILD | WS_VISIBLE, LCDB,
                   lcdWidth*2/3-1, index_y*y_step+40, lcdWidth/3, m_height,
                    hWnd, 0);
            SetWindowBkColor(hwnd_lcdb, COLOR_blue);
*/
#endif
#if 0
			hwnd_result = CreateWindow(CTRL_STATIC, NULL,
                    WS_CHILD | WS_VISIBLE |WS_TABSTOP, ID_RESULT,
                    lcdWidth/3+lcdWidth/12, index_y*y_step+100, lcdWidth/3, m_height,
                    hWnd, 0);

			hdc = GetDC(hwnd_result);
			SetWindowBkColor(hwnd_result, RGBA2Pixel(hdc, 0xaf, 0xaf, 0xff, 0x00));
#endif
            SetTimer(hWnd, REFRESH_TIMER, 100);       // set timer 0.2s(20), 1s(100)
            break;
        case MSG_TIMER:
            // HandTester
            if (testTf) {
                SetWindowFont(GetDlgItem(hWnd, ID_TF), mLogFont);
                if (*tf_str == 'P') {
                    SetWindowBkColor(hwnd_tf, PIXEL_green);     // green means pass
                    SetDlgItemText(hWnd, ID_TF, "[TF] PASS");
                } else if (*tf_str == 'F') {
                    SetWindowBkColor(hwnd_tf, PIXEL_red);
                    SetDlgItemText(hWnd, ID_TF, "[TF] FAIL");   // red means fail
                } else {
                    SetWindowBkColor(hwnd_tf, PIXEL_cyan);      // cyan means wait
                    SetDlgItemText(hWnd, ID_TF, "[TF] waiting");
                }
            }

            if (testCPU) {
                SetWindowFont(GetDlgItem(hWnd, ID_CPU), mLogFont);
                if (*cpu_str == 'P') {
                    SetWindowBkColor(hwnd_CPU, PIXEL_green);    // green means pass
                    SetDlgItemText(hWnd, ID_CPU, "[CPU] PASS");
                } else if (*cpu_str == 'F') {
                    SetWindowBkColor(hwnd_CPU, PIXEL_red);
                    SetDlgItemText(hWnd, ID_CPU, "[CPU] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_CPU, PIXEL_cyan);     // cyan means wait
                    SetDlgItemText(hWnd, ID_CPU, "[CPU] waiting");
                }
            }
#ifdef WIFI_TEST
            if (testWifi) {
                SetWindowFont(GetDlgItem(hWnd, ID_WIFI), mLogFont);
                if (*wifi_str == 'P') {
                    SetWindowBkColor(hwnd_wifi, PIXEL_green);    // green means pass
                    SetDlgItemText(hWnd, ID_WIFI, wifi_str+1);
                } else if (*wifi_str == 'F') {
                    SetWindowBkColor(hwnd_wifi, PIXEL_red);
                    SetDlgItemText(hWnd, ID_WIFI, "[WIFI] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_wifi, PIXEL_cyan);     // cyan means wait
                    SetDlgItemText(hWnd, ID_WIFI, "[WIFI] waiting");
                }
            }
#endif

#ifdef GPS_TEST
            if (testGPS) {
                SetWindowFont(GetDlgItem(hWnd, ID_GPS), mLogFont);
                if (*gps_str == 'P') {
                    SetWindowBkColor(hwnd_gps, PIXEL_green);    // green means pass
                    SetDlgItemText(hWnd, ID_GPS, gps_str+1);
                } else if (*gps_str == 'F') {
                    SetWindowBkColor(hwnd_gps, PIXEL_red);
                    SetDlgItemText(hWnd, ID_GPS, "[GPS] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_gps, PIXEL_cyan);     // cyan means wait
                    SetDlgItemText(hWnd, ID_GPS, "[GPS] waiting");
                }
            }
#endif

#ifdef GSENSOR_TEST
            if (testGSENSOR) {
                SetWindowFont(GetDlgItem(hWnd, ID_GSENSOR), mLogFont);
                if (*gsensor_str == 'P') {
                    SetWindowBkColor(hwnd_gsensor, PIXEL_green);    // green means pass
                    SetDlgItemText(hWnd, ID_GSENSOR, gsensor_str+1);
                } else if (*gsensor_str == 'F') {
                    SetWindowBkColor(hwnd_gsensor, PIXEL_red);
                    SetDlgItemText(hWnd, ID_GSENSOR, "[GSENSOR] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_gsensor, PIXEL_cyan);     // cyan means wait
                    SetDlgItemText(hWnd, ID_GSENSOR, "[GSENSOR] waiting");
                }
            }
#endif
            if (testKey) {
                SetWindowFont(GetDlgItem(hWnd, ID_KEY), mLogFont);
                    if (*key_str == 'P') {
                    SetWindowBkColor(hwnd_key, PIXEL_green);
                    SetDlgItemText(hWnd, ID_KEY, "[KEY] PASS");
                } else if (*key_str == 'F') {
                    SetWindowBkColor(hwnd_key, PIXEL_red);
                    SetDlgItemText(hWnd, ID_KEY, "[KEY] FAIL");
                } else {
                    SetWindowBkColor(hwnd_key, PIXEL_cyan);
                    SetDlgItemText(hWnd, ID_KEY, "[KEY] waiting");
                }
            }

            if (testRtc) {
                SetWindowFont(GetDlgItem(hWnd, ID_RTC), mLogFont);
                if (*rtc_str == 'P') {
                    SetWindowBkColor(hwnd_rtc, PIXEL_green);    // green means pass
                    SetDlgItemText(hWnd, ID_RTC, "[RTC] PASS");
                } else if (*rtc_str == 'F') {
                    SetWindowBkColor(hwnd_rtc, PIXEL_red);
                    SetDlgItemText(hWnd, ID_RTC, "[RTC] FAIL");    // red means fail
                } else {
                    SetWindowBkColor(hwnd_rtc, PIXEL_cyan);     // cyan means wait
                    SetDlgItemText(hWnd, ID_RTC, "[RTC] waiting");
                }
            }

            if (testLed) {
                SetWindowFont(GetDlgItem(hWnd, ID_LED), mLogFont);
                if (*led_str == 'P') {
                    SetWindowBkColor(hwnd_led, PIXEL_green);
                    SetDlgItemText(hWnd, ID_LED, "[LED] PASS");
                }
                else if (*led_str == 'F') {
                    SetWindowBkColor(hwnd_led, PIXEL_red);
                    SetDlgItemText(hWnd, ID_LED, "[LED] FAIL");
                }
                else {
                    SetWindowBkColor(hwnd_led, PIXEL_cyan);
                    SetDlgItemText(hWnd, ID_LED, "[LED] Waiting");
                }
            }

            if (testUsb) {
                SetWindowFont(GetDlgItem(hWnd, ID_USB), mLogFont);
                if (*usb_str == 'P') {
                    SetWindowBkColor(hwnd_usb, PIXEL_green);    // green means pass
                    SetDlgItemText(hWnd, ID_USB, "[USB] PASS");
                } else if (*usb_str == 'F') {
                    SetWindowBkColor(hwnd_usb, PIXEL_red);
                    SetDlgItemText(hWnd, ID_USB, "[USB] FAIL");    // red means fail
                } else {
                    SetWindowBkColor(hwnd_usb, PIXEL_cyan);     // cyan means wait
                    SetDlgItemText(hWnd, ID_USB, "[USB] waiting");
                }
            }

            if (testDdr) {
                SetWindowFont(GetDlgItem(hWnd, ID_DDR), mLogFont);
                if (*ddr_str == 'P') {
                    SetWindowBkColor(hwnd_ddr, PIXEL_green);
                    SetDlgItemText(hWnd, ID_DDR, "[DDR] PASS");
                } else if (*ddr_str == 'F') {
                    SetWindowBkColor(hwnd_ddr, PIXEL_red);
                    SetDlgItemText(hWnd, ID_DDR, "[DDR] FAIL");
		        } else {
                    SetWindowBkColor(hwnd_ddr, PIXEL_cyan);
                    SetDlgItemText(hWnd, ID_DDR, "[DDR] waiting");
                }
            }

            if (testNor) {
                SetWindowFont(GetDlgItem(hWnd, ID_NOR), mLogFont);
                if (*nor_str == 'P') {
                    SetWindowBkColor(hwnd_nor, PIXEL_green);
                    SetDlgItemText(hWnd, ID_NOR, "[NOR] PASS");
                } else if (*nor_str == 'F') {
                    SetWindowBkColor(hwnd_nor, PIXEL_red);
                    SetDlgItemText(hWnd, ID_NOR, "[NOR] FAIL");
                } else {
                    SetWindowBkColor(hwnd_nor, PIXEL_cyan);
                    SetDlgItemText(hWnd, ID_NOR, "[NOR] waiting");
                }
            }

            if (testNand) {
                SetWindowFont(GetDlgItem(hWnd, ID_NAND), mLogFont);
                if (*nand_str == 'P') {
                    SetWindowBkColor(hwnd_nand, PIXEL_green);
                    SetDlgItemText(hWnd, ID_NAND, "[NAND] PASS");
                } else if (*nand_str == 'F') {
                    SetWindowBkColor(hwnd_nand, PIXEL_red);
                    SetDlgItemText(hWnd, ID_NAND, "[NAND] FAIL");
                } else {
                    SetWindowBkColor(hwnd_nand, PIXEL_cyan);
                    SetDlgItemText(hWnd, ID_NAND, "[NAND] waiting");
                }
            }

            if (testCSI) {
                SetWindowFont(GetDlgItem(hWnd, ID_CSI), mLogFont);
                if (*csi_str == 'P') {
                    SetWindowBkColor(hwnd_CSI, PIXEL_green);	// green means pass
                    SetDlgItemText(hWnd, ID_CSI, "[CSI] PASS");
                } else if (*csi_str == 'F') {
                    SetWindowBkColor(hwnd_CSI, PIXEL_red);
                    SetDlgItemText(hWnd, ID_CSI, "[CSI] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_CSI, PIXEL_cyan);	// cyan means wait
                    SetDlgItemText(hWnd, ID_CSI, "[CSI] waiting");
                }
            }

            if(testSpk_MIC){
                SetWindowFont(GetDlgItem(hWnd, ID_MIC_SPK), mLogFont);
                if (*spk_mic_str == 'P') {
                    SetWindowBkColor(hwnd_mic_spk, PIXEL_green);    // green means pass
                    SetDlgItemText(hWnd, ID_MIC_SPK, "[MIC_SPK] PASS");
                } else if (*spk_mic_str == 'F') {
                    SetWindowBkColor(hwnd_mic_spk, PIXEL_red);
                    SetDlgItemText(hWnd, ID_MIC_SPK, "[MIC_SPK] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_mic_spk, PIXEL_cyan);
                    SetDlgItemText(hWnd, ID_MIC_SPK, "[MIC_SPK] waiting");
                }
            }

            if (testVENC) {
                SetWindowFont(GetDlgItem(hWnd, ID_VENC), mLogFont);
                if (*venc_str == 'P') {
                    SetWindowBkColor(hwnd_VENC, PIXEL_green);	// green means pass
                    SetDlgItemText(hWnd, ID_VENC, "[VENC] PASS");
                } else if (*venc_str == 'F') {
                    SetWindowBkColor(hwnd_VENC, PIXEL_red);
                    SetDlgItemText(hWnd, ID_VENC, "[VENC] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_VENC, PIXEL_cyan);	// cyan means wait
                    SetDlgItemText(hWnd, ID_VENC, "[VENC] waiting");
                }
            }

            if (testVDEC) {
                SetWindowFont(GetDlgItem(hWnd, ID_VDEC), mLogFont);
                if (*vdec_str == 'P') {
                    SetWindowBkColor(hwnd_VDEC, PIXEL_green);	// green means pass
                    SetDlgItemText(hWnd, ID_VDEC, "[VDEC] PASS");
                } else if (*vdec_str == 'F') {
                    SetWindowBkColor(hwnd_VDEC, PIXEL_red);
                    SetDlgItemText(hWnd, ID_VDEC, "[VDEC] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_VDEC, PIXEL_cyan);	// cyan means wait
                    SetDlgItemText(hWnd, ID_VDEC, "[VDEC] waiting");
                }
            }

            if (testISP) {
                SetWindowFont(GetDlgItem(hWnd, ID_ISP), mLogFont);
                if (*isp_str == 'P') {
                    SetWindowBkColor(hwnd_ISP, PIXEL_green);	// green means pass
                    SetDlgItemText(hWnd, ID_ISP, "[ISP] PASS");
                } else if (*isp_str == 'F') {
                    SetWindowBkColor(hwnd_ISP, PIXEL_red);
                    SetDlgItemText(hWnd, ID_ISP, "[ISP] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_ISP, PIXEL_cyan);	// cyan means wait
                    SetDlgItemText(hWnd, ID_ISP, "[ISP] waiting");
                }
            }

            if (testG2D) {
                SetWindowFont(GetDlgItem(hWnd, ID_G2D), mLogFont);
                if (*g2d_str == 'P') {
                    SetWindowBkColor(hwnd_G2D, PIXEL_green);	// green means pass
                    SetDlgItemText(hWnd, ID_G2D, "[G2D] PASS");
                } else if (*g2d_str == 'F') {
                    SetWindowBkColor(hwnd_G2D, PIXEL_red);
                    SetDlgItemText(hWnd, ID_G2D, "[G2D] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_G2D, PIXEL_cyan);	// cyan means wait
                    SetDlgItemText(hWnd, ID_G2D, "[G2D] waiting");
                }
            }

            if (testCE) {
                SetWindowFont(GetDlgItem(hWnd, ID_CE), mLogFont);
                if (*ce_str == 'P') {
                    SetWindowBkColor(hwnd_CE, PIXEL_green);	// green means pass
                    SetDlgItemText(hWnd, ID_CE, "[CE] PASS");
                } else if (*ce_str == 'F') {
                    SetWindowBkColor(hwnd_CE, PIXEL_red);
                    SetDlgItemText(hWnd, ID_CE, "[CE] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_CE, PIXEL_cyan);	// cyan means wait
                    SetDlgItemText(hWnd, ID_CE, "[CE] waiting");
                }
            }

            if (testVO) {
                SetWindowFont(GetDlgItem(hWnd, ID_VO), mLogFont);
                if (*ce_str == 'P') {
                    SetWindowBkColor(hwnd_VO, PIXEL_green);	// green means pass
                    SetDlgItemText(hWnd, ID_VO, "[VO] PASS");
                } else if (*ce_str == 'F') {
                    SetWindowBkColor(hwnd_VO, PIXEL_red);
                    SetDlgItemText(hWnd, ID_VO, "[VO] FAIL"); // red means fail
                } else {
                    SetWindowBkColor(hwnd_VO, PIXEL_cyan);	// cyan means wait
                    SetDlgItemText(hWnd, ID_VO, "[VO] waiting");
                }
            }

            if(IsResultOver)
            {
                SetWindowFont(GetDlgItem(hWnd, ID_RESULT), mLogFont);
                //hdc = GetDC(GetDlgItem(hWnd, ID_RESULT));
                //SetPenColor(hdc, COLOR_green);
                SetDlgItemText(hWnd, ID_RESULT, "Test Over!!!!");
            }
            else
            {
                SetWindowFont(GetDlgItem(hWnd, ID_RESULT), mLogFont);
                //hdc = GetDC(GetDlgItem(hWnd, ID_RESULT));
                //SetPenColor(hdc, COLOR_red);
                if(textout_i < 4)
                {
//                    SetDlgItemText(hWnd, ID_RESULT, test_out[textout_i++]);
                    //printf("Text out[%d]:%s\n",i,test_out[i]);
                }
                else{
                    textout_i = 0;
                }
            }

        break;

        case MSG_KEYDOWN:
            keyVal = LOWORD(wParam);
            break;
        case MSG_KEYUP:
            keyVal = LOWORD(wParam);
            switch (keyVal) {
                case 0x84:
                    timestamp2 = clock() / CLOCKS_PER_SEC;
                    if ( timestamp2 - timestamp1 > 2) { // long press >= 3s
                        // strlcopy(key_str, "P[KEY] reboot...", 20);
                        // sleep(5);                        // sleep 5s to display string "reboot..."
                        // system("reboot");        // NO cmd "poweroff"

                        //android_reboot(ANDROID_RB_POWEROFF, 0, 0);
                    }
            }
            break;
#if 1
		case MSG_PAINT:
		    aloge("MSG_PAINT");
            hdc = BeginPaint (hWnd);
            TextOut (hdc, 0, lcdHeight, "Hello AllwinnerTech!");
            EndPaint (hWnd, hdc);
            aloge("MSG_PAINT");
			break;
#endif
        case MSG_CLOSE:
            KillTimer(hWnd, REFRESH_TIMER);
            DestroyAllControls(hWnd);
            DestroyMainWindow(hWnd);
            PostQuitMessage(hWnd);
            return 0;
    }

    return DefaultMainWinProc(hWnd, message, wParam, lParam);
}

/********************** Test cases **************************/
void* runTfthread(void *argv)
{
    FUNCTION_ENTER_PRINT;
    char tmpPath[50];
    sprintf(tmpPath, "cp %s /tmp", testTfPath);
    system(tmpPath);
    sleep(1);
    system("/tmp/tftester");
    sleep(2);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* tfThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    int tfFd, retVal, fifoFd;
    static char m_strRes[128];
    if ((fifoFd = open(FIFO_TF_DEV, O_RDONLY)) < 0)
    {
       if ((retVal = mkfifo(FIFO_TF_DEV, 0666)) < 0)
       {
           strlcpy(tf_str, "F", 30);
           return NULL;
       }
       else
       {
           fifoFd = open(FIFO_TF_DEV, O_RDONLY);
       }
    }

    while (1)
    {
       read(fifoFd, tf_str, 30);
       sleep(2);
    }
    close(fifoFd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* CPUThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    int  retVal, fifoFd,cpufd;
    char buf[4096] = {0};
    char cpuHardware[10];
    char *cpuHardwarePtr;
    cpuHardwarePtr = cpuHardware;
    if ((cpufd = open("/proc/cpuinfo", O_RDONLY)) > 0) {
        int cnt = read(cpufd, buf, 4096);
        if (cnt != -1) {
            char *cpuInfoPtr = strstr(buf, "sun");
            while ((*cpuHardwarePtr++ = *cpuInfoPtr++) != '\n');
            *--cpuHardwarePtr = '\0';
        }
        aloge("Hardware: %s\n", cpuHardware);
        sprintf(cpu_str, "P[CPU] PASS");
        aloge("cpu read success");
    } else {
        sprintf(cpu_str, "F[CPU]:FAIL");
        aloge("cpu read failed");
    }
    pthread_exit(NULL);
    FUNCTION_LEAVE_PRINT;
    return NULL;
}

#ifdef WIFI_TEST
void* runWifiThread(void* argv)
{
    FUNCTION_ENTER_PRINT;
    char tmpPath[50];
    sprintf(tmpPath, "cp %s /tmp", testWifiPath);
    system(tmpPath);
    sleep(1);
    system("/tmp/wifitester");
    sleep(2);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* wifiThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    int fifoFd,ret;
    char hotspot[256];

    FILE *fd = NULL;
    char buf[50];
    while (1) {
        sleep(2);
        if(access(FIFO_WIFI_DEV, F_OK) == 0 ){
            fd = fopen(FIFO_WIFI_DEV,"r");
            fread(buf, 50,1,fd);
            aloge("buf %s",buf);
            if (*buf == 'F') {
                wifi_str[0] = 'F';
                aloge("wifi_str %s",wifi_str);
                break;
            } else if (*buf == 'P') {
                strncpy(wifi_str, buf, 50);
                aloge("wifi_str %s",wifi_str);
                break;
            }
        }
    }
    fclose(fd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}
#endif

#ifdef GPS_TEST
void* runGPSThread(void* argv)
{
    FUNCTION_ENTER_PRINT;
    char tmpPath[50];
    sprintf(tmpPath, "cp %s /tmp", testGPSPath);
    system(tmpPath);
    sleep(1);
    system("/tmp/gpstester");
    sleep(2);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* GPSThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    int fifoFd,ret;
    char hotspot[256];

    FILE *fd = NULL;
    char buf[50];
    while (1) {
        sleep(2);
        if(access(FIFO_GPS_DEV, F_OK) == 0 ){
            fd = fopen(FIFO_GPS_DEV,"r");
            fread(buf, 50,1,fd);
            aloge("buf %s",buf);
            if (*buf == 'F') {
                gps_str[0] = 'F';
                aloge("gps_str %s",gps_str);
                break;
            } else if (*buf == 'P') {
                strncpy(gps_str, buf, 50);
                aloge("gps_str %s",gps_str);
                break;
            }
        }
    }
    fclose(fd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}
#endif

#ifdef GSENSOR_TEST
void* runGSENSORThread(void* argv)
{
    FUNCTION_ENTER_PRINT;
    while (1) {
       sleep(2);
        char tmpPath[50];
        sprintf(tmpPath, "cp %s /tmp", testGSENSORPath);
        system(tmpPath);
        sleep(3);
        system("/tmp/gsensortester");
        sleep(2);
        break;
    }
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* GSENSORThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    int fifoFd,ret;
    char hotspot[256];

    FILE *fd = NULL;
    char buf[50];
    while (1) {
        sleep(2);
        if(access(FIFO_GSENSOR_DEV, F_OK) == 0 ){
            fd = fopen(FIFO_GSENSOR_DEV,"r");
            fread(buf, 50,1,fd);
            aloge("buf %s",buf);
            if (*buf == 'F') {
                gsensor_str[0] = 'F';
                aloge("gsensor_str %s",gsensor_str);
                break;
            } else if (*buf == 'P') {
                strncpy(gsensor_str, buf, 50);
                aloge("gsensor_str %s",gsensor_str);
                break;
            }
        }
    }
    fclose(fd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}
#endif

void* runKeythread(void* argv)
{
    FUNCTION_ENTER_PRINT;
    char tmpPath[50];
    sprintf(tmpPath, "cp %s /tmp", testKeyPath);
    system(tmpPath);
    sleep(1);
    system("/tmp/keytester");
    sleep(2);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* keyThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    int fifoFd,ret;
    char buf[50];
    if ((fifoFd = open(FIFO_KEY_DEV, O_RDONLY)) < 0)
    {
       if ((ret = mkfifo(FIFO_KEY_DEV, 0666)) < 0)
       {
           strlcpy(key_str, "F", 30);
           return NULL;
       }
       else
       {
           fifoFd = open(FIFO_KEY_DEV, O_RDONLY);
       }
    }
    while (1) {
        read(fifoFd, buf, 50);
        if (*buf == 'F') {
            sprintf(key_str,"P[KEY] FAIL");
        } else {
            sprintf(key_str,"P[KEY] PASS");
        }
        sleep(2);           // time control is done in testcase => avoid read flood in case of write-endian shutdown
    }
    close(fifoFd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* runRtcthread(void* argv)
{
    FUNCTION_ENTER_PRINT;
    char tmpPath[50];
    sprintf(tmpPath, "cp %s /tmp", testRtcPath);
    system(tmpPath);
    sleep(1);
    system("/tmp/rtctester");
    sleep(2);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* rtcThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    FILE *fd = NULL;
    char buf[50];
    while (1) {
       sleep(2);
       if(access(FIFO_RTC_DEV, F_OK) == 0 ){
           fd = fopen(FIFO_RTC_DEV,"r");
           fread(buf, 50,1,fd);
           if (*buf == 'F') {
               sprintf(rtc_str,"F[RTC] FAIL");
               aloge("write rtc fail");
               break;
           } else if (*buf == 'P'){
               sprintf(rtc_str,"P[RTC] PASS");
               aloge("write rtc pass");
               break;
           }
       }
    }
    fclose(fd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* runLedthread(void* argv)
{
    FUNCTION_ENTER_PRINT;
    char tmpPath[50];
    sprintf(tmpPath, "cp %s /tmp", testLedPath);
    system(tmpPath);
    sleep(1);
    system("/tmp/ledtester 145 1");
    sleep(2);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* ledThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    FILE *fd = NULL;
    char buf[50];
    alogd("");
    while (1) {
       sleep(2);
       if(access(FIFO_LED_DEV, F_OK) == 0 ){
           fd = fopen(FIFO_LED_DEV,"r");
           fread(buf, 50,1,fd);
           if (*buf == 'F') {
               sprintf(led_str,"P[LED] FAIL");
               aloge("write led fail");
               break;
           } else if (*buf == 'P'){
               sprintf(led_str,"P[LED] PASS");
               aloge("write led pass");
               break;
           }
       }
    }
    alogd("led test result[%d]", *buf);
    fclose(fd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* runUsbthread(void* argv)
{
    FUNCTION_ENTER_PRINT;
    char tmpPath[50];
    sprintf(tmpPath, "cp %s /tmp", testUsbPath);
    system(tmpPath);
    sleep(1);
    system("/tmp/usbtester");
    sleep(2);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* usbThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    FILE *fd = NULL;
    char buf[50];
    while (1) {
       sleep(2);
       if(access(FIFO_USB_DEV, F_OK) == 0 ){
           fd = fopen(FIFO_USB_DEV,"r");
           fread(buf, 50,1,fd);
           if (*buf == 'F') {
               sprintf(usb_str,"F[USB] FAIL");
               aloge("write usb fail");
               break;
           } else if (*buf == 'P'){
               sprintf(usb_str,"P[USB] PASS");
               aloge("write usb pass");
               break;
           }
       }
    }
    fclose(fd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* runDDRthread(void* argv)
{
    FUNCTION_ENTER_PRINT;
    char tmpPath[50];
    sprintf(tmpPath, "cp %s /tmp", testDdrPath);
    system(tmpPath);
    sleep(1);
    system("/tmp/ddrtester");
    sleep(2);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* DDRThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    FILE *fd = NULL;
    char buf[50];
    while (1) {
       sleep(2);
       if(access(FIFO_DDR_DEV, F_OK) == 0 ){
           fd = fopen(FIFO_DDR_DEV,"r");
           fread(buf, 50,1,fd);
           if (*buf == 'F') {
               sprintf(ddr_str,"P[DDR] FAIL");
               aloge("write ddr fail");
               break;
           } else if (*buf == 'P'){
               sprintf(ddr_str,"P[DDR] PASS");
               aloge("write ddr pass");
               break;
           }
       }
    }
    fclose(fd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* runNORthread(void* argv)
{
    FUNCTION_ENTER_PRINT;
    char tmpPath[50];
    sprintf(tmpPath, "cp %s /tmp", testNorPath);
    system(tmpPath);
    sleep(1);
    system("/tmp/nortester");
    sleep(2);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* NORThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    FILE *fd = NULL;
    char buf[50];
    while (1) {
       sleep(2);
       if(access(FIFO_NOR_DEV, F_OK) == 0 ){
           fd = fopen(FIFO_NOR_DEV,"r");
           fread(buf, 50,1,fd);
           if (*buf == 'F') {
               sprintf(nor_str,"F[NOR] FAIL");
               aloge("write nor fail");
               break;
           } else if (*buf == 'P'){
               sprintf(nor_str,"P[NOR] PASS");
               aloge("write nor pass");
               break;
           }
       }
    }
    fclose(fd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* runNANDthread(void* argv)
{
    FUNCTION_ENTER_PRINT;
    char tmpPath[50];
    sprintf(tmpPath, "cp %s /tmp", testNandPath);
    system(tmpPath);
    sleep(1);
    system("/tmp/nandtester");
    sleep(2);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

void* NANDThreadRead(void* argv)
{
    FUNCTION_ENTER_PRINT;
    FILE *fd = NULL;
    char buf[50];
    while (1) {
       sleep(2);
       if(access(FIFO_NAND_DEV, F_OK) == 0 ){
           fd = fopen(FIFO_NAND_DEV,"r");
           fread(buf, 50,1,fd);
           if (*buf == 'F') {
               sprintf(nand_str,"P[NAND] FAIL");
               aloge("write nand fail");
               break;
           } else if (*buf == 'P'){
               sprintf(nand_str,"P[NAND] PASS");
               aloge("write nand pass");
               break;
           }
       }
    }
    fclose(fd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}

/*************************MPP**************************/
void* runMPPthread(void* argv)
{
    FUNCTION_ENTER_PRINT;

    char cmd[100] = {0};
    char retmsg[1024] = {0};
    int result = 0;

    if (testCSI) {
        sleep(2);
        alogd("************run csitester************");
        //system("/mnt/extsd/csitester -path /mnt/extsd/csitester.conf");
        sprintf(cmd, "%s/csitester -path %s/csitester.conf", DRAGONBOARD_TEST_PATH, DRAGONBOARD_TEST_PATH);
        result = super_system(cmd, retmsg, sizeof(retmsg));
        if (0 == result) {
            sprintf(csi_str,"PASS");
            aloge("csi test pass");
        } else {
            sprintf(csi_str,"FAIL");
            aloge("csi test fail");
        }
    }
    if (testSpk_MIC) {
        sleep(2);
        alogd("************run mictester************");
        //system("/mnt/extsd/mictester");
        sprintf(cmd, "%s/mictester -path %s/mictester.conf", DRAGONBOARD_TEST_PATH, DRAGONBOARD_TEST_PATH);
        result = super_system(cmd, retmsg, sizeof(retmsg));
        if (0 == result) {
            sprintf(spk_mic_str,"PASS");
            aloge("mic test pass");
        } else {
            sprintf(spk_mic_str,"FAIL");
            aloge("mic test fail");
        }
    }
    if (testVENC) {
        sleep(2);
        alogd("************run venctester************");
        //system("/mnt/extsd/venctester -path /mnt/extsd/venctester.conf");
        sprintf(cmd, "%s/venctester -path %s/venctester.conf", DRAGONBOARD_TEST_PATH, DRAGONBOARD_TEST_PATH);
        result = super_system(cmd, retmsg, sizeof(retmsg));
        if (0 == result) {
            sprintf(venc_str,"PASS");
            aloge("venc test pass");
        } else {
            sprintf(venc_str,"FAIL");
            aloge("venc test fail");
        }
    }
    if (testVDEC) {
        sleep(2);
        alogd("************run vdectester************");
        //system("/mnt/extsd/vdectester -path /mnt/extsd/vdectester.conf");
        sprintf(cmd, "%s/vdectester -path %s/vdectester.conf", DRAGONBOARD_TEST_PATH, DRAGONBOARD_TEST_PATH);
        result = super_system(cmd, retmsg, sizeof(retmsg));
        if (0 == result) {
            sprintf(vdec_str,"PASS");
            aloge("vdec test pass");
        } else {
            sprintf(vdec_str,"FAIL");
            aloge("vdec test fail");
        }
    }
    if (testISP) {
        sleep(2);
        alogd("************run isptester************");
        //system("/mnt/extsd/isptester 0 512 288 ./ 1 20 1");
        sprintf(cmd, "%s/isptester 0 512 288 /tmp 1 20 1", DRAGONBOARD_TEST_PATH);
        result = super_system(cmd, retmsg, sizeof(retmsg));
        if (0 == result) {
            sprintf(isp_str,"PASS");
            aloge("isp test pass");
        } else {
            sprintf(isp_str,"FAIL");
            aloge("isp test fail");
        }
    }
    if (testG2D) {
        sleep(2);
        alogd("************run g2dtester************");
        //system("/mnt/extsd/g2dtester -path /mnt/extsd/g2dtester.conf");
        sprintf(cmd, "%s/g2dtester -path %s/g2dtester.conf", DRAGONBOARD_TEST_PATH, DRAGONBOARD_TEST_PATH);
        result = super_system(cmd, retmsg, sizeof(retmsg));
        if (0 == result) {
            sprintf(g2d_str,"PASS");
            aloge("g2d test pass");
        } else {
            sprintf(g2d_str,"FAIL");
            aloge("g2d test fail");
        }
    }
    if (testCE) {
        sleep(2);
        alogd("************run cetester************");
        //system("/mnt/extsd/cetester -path /mnt/extsd/cetester.conf");
        sprintf(cmd, "%s/cetester -path %s/cetester.conf", DRAGONBOARD_TEST_PATH, DRAGONBOARD_TEST_PATH);
        result = super_system(cmd, retmsg, sizeof(retmsg));
        if (0 == result) {
            sprintf(ce_str,"PASS");
            aloge("ce test pass");
        } else {
            sprintf(ce_str,"FAIL");
            aloge("ce test fail");
        }
    }
    if (testVO) {
        sleep(2);
        alogd("************run votester************");
        //system("/mnt/extsd/votester -path /mnt/extsd/votester.conf");
        sprintf(cmd, "%s/votester -path %s/votester.conf", DRAGONBOARD_TEST_PATH, DRAGONBOARD_TEST_PATH);
        result = super_system(cmd, retmsg, sizeof(retmsg));
        if (0 == result) {
            sprintf(vo_str,"PASS");
            aloge("vo test pass");
        } else {
            sprintf(vo_str,"FAIL");
            aloge("vo test fail");
        }
    }

    FUNCTION_LEAVE_PRINT;
    return NULL;
}

void* saveResultThread(void* argv)
{
    FUNCTION_ENTER_PRINT;
    FILE *fd = NULL;
    char testResult[50];
    int total_testcases = 0;
    int test_result_get_cnt = 0;
    int test_result_get_flag[20] = {0};
    if (NULL == argv) {
        aloge("fatal error, invalid input param!");
        return NULL;
    }
    total_testcases = *(int*)argv;
    alogd("total_testcases: %d", total_testcases);
    fd = fopen(DRAGONBOARD_TEST_RESULT_FILE, "wb");
    if (NULL == fd) {
        aloge("fatal error, open %s failed!", DRAGONBOARD_TEST_RESULT_FILE);
        return NULL;
    }
    sprintf(testResult, "dragonboard test result:\n");
    fwrite(testResult, 1, sizeof(testResult), fd);
    fflush(fd);
    while (1) {
        if (total_testcases <= test_result_get_cnt) {
            alogd("test Done!");
            break;
        }
        sleep(1);

        if (testRtc && !test_result_get_flag[0]) {
            if (*rtc_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "rtc tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[0] = 1;
                alogd("rtc pass");
            } else if (*rtc_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "rtc tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[0] = 1;
                alogd("rtc fail");
            }
        }
        if (testLed && !test_result_get_flag[1]) {
            if (*led_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "led tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[1] = 1;
            } else if (*led_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "led tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[1] = 1;
            }
        }
        if (testUsb && !test_result_get_flag[2]) {
            if (*usb_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "usb tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[2] = 1;
            } else if (*usb_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "usb tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[2] = 1;
            }
        }
        if (testDdr && !test_result_get_flag[3]) {
            if (*ddr_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "ddr tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[3] = 1;
            } else if (*ddr_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "ddr tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[3] = 1;
            }
        }
        if (testTf && !test_result_get_flag[4]) {
            if (*tf_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "tf tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[4] = 1;
            } else if (*tf_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "tf tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[4] = 1;
            }
        }
        if (testNor && !test_result_get_flag[5]) {
            if (*nor_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "nor tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[5] = 1;
            } else if (*nor_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "nor tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[5] = 1;
            }
        }
        if (testNand && !test_result_get_flag[6]) {
            if (*nand_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "nand tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[6] = 1;
            } else if (*nand_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "nand tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[6] = 1;
            }
        }

        // MPP
        if (testCSI && !test_result_get_flag[7]) {
            if (*csi_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "csi tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[7] = 1;
                alogd("csi pass");
            } else if (*csi_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "csi tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[7] = 1;
                alogd("csi fail");
            }
        }
        if (testSpk_MIC && !test_result_get_flag[8]) {
            if (*spk_mic_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "mic tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[8] = 1;
            } else if (*spk_mic_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "mic tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[8] = 1;
            }
        }
        if (testVENC && !test_result_get_flag[9]) {
            if (*venc_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "venc tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[9] = 1;
            } else if (*venc_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "venc tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[9] = 1;
            }
        }
        if (testVDEC && !test_result_get_flag[10]) {
            if (*vdec_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "vdec tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[10] = 1;
            } else if (*vdec_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "vdec tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[10] = 1;
            }
        }
        if (testISP && !test_result_get_flag[11]) {
            if (*isp_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "isp tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[11] = 1;
            } else if (*isp_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "isp tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[11] = 1;
            }
        }
        if (testG2D && !test_result_get_flag[12]) {
            if (*g2d_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "g2d tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[12] = 1;
            } else if (*spk_mic_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "g2d tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[12] = 1;
            }
        }
        if (testCE && !test_result_get_flag[13]) {
            if (*ce_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "ce tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[13] = 1;
            } else if (*ce_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "ce tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[13] = 1;
            }
        }
        if (testVO && !test_result_get_flag[14]) {
            if (*vo_str == 'P') {
                memset(testResult, 0, 50);
                sprintf(testResult, "vo tester: PASS\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[14] = 1;
            } else if (*vo_str == 'F') {
                memset(testResult, 0, 50);
                sprintf(testResult, "vo tester: FAIL\n");
                fwrite(testResult, 1, sizeof(testResult), fd);
                fflush(fd);
                test_result_get_cnt++;
                test_result_get_flag[14] = 1;
            }
        }
    }
    fclose(fd);
    FUNCTION_LEAVE_PRINT;
    pthread_exit(NULL);
}


static int InitLcd(void)
{
    /* lcd on/off */
    int mdisp_fd_ = -1;
    int retval = 0;
    unsigned long args[32]={0};

    printf(" FUN[%s] LINE[%d]  --->  \n", __func__, __LINE__);
    usleep(88 * 1000);
    mdisp_fd_ = open("/dev/disp", O_RDWR);
    if (mdisp_fd_ < 0) {
        printf(" FUN[%s] LINE[%d]  --->  \n", __func__, __LINE__);
        return -1;
    }

    printf(" FUN[%s] LINE[%d]  ---> mdisp_fd_:%d  \n", __func__, __LINE__, mdisp_fd_);

    args[1] = DISP_OUTPUT_TYPE_NONE;
    retval = ioctl(mdisp_fd_, DISP_DEVICE_SWITCH, args);
    if (retval < 0) {
        printf("fail to set screen off");
        close(mdisp_fd_);
        return -1;
    }

    usleep(88 * 100);

    memset(args, 0, sizeof(args));
    args[1] = DISP_OUTPUT_TYPE_LCD;
    retval = ioctl(mdisp_fd_, DISP_DEVICE_SWITCH, args);
    if (retval < 0) {
        printf("fail to set screen on");
        close(mdisp_fd_);
        return -1;
    }
    usleep(88 * 100);

    close(mdisp_fd_);
    return 0;
}

static void getScreenInfo(int *w, int *h)
{
    *w = 460;
    *h = 232;
}

static int checkDefaultTestcases()
{
    int total_testcases = 0;

    if (testRtc) total_testcases++;
    if (testLed) total_testcases++;
    if (testUsb) total_testcases++;
    if (testDdr) total_testcases++;
    if (testTf) total_testcases++;
    if (testNor) total_testcases++;
    if (testNand) total_testcases++;

    // mpp
    if (testCSI) total_testcases++;
    if (testSpk_MIC) total_testcases++;
    if (testVENC) total_testcases++;
    if (testVDEC) total_testcases++;
    if (testISP) total_testcases++;
    if (testG2D) total_testcases++;
    if (testCE) total_testcases++;

    return total_testcases;
}

static void MatchTestCase(const char *str, int flag)
{
    if (strstr(str, "RTC")) testRtc = flag;
    else if (strstr(str, "LED")) testLed = flag;
    else if (strstr(str, "USB")) testUsb = flag;
    else if (strstr(str, "DDR")) testDdr = flag;
    else if (strstr(str, "TF")) testTf = flag;
    else if (strstr(str, "NOR")) testNor = flag;
    else if (strstr(str, "NAND")) testNand = flag;
    else if (strstr(str, "MIC")) testSpk_MIC = flag;
    else if (strstr(str, "CSI")) testCSI = flag;
    else if (strstr(str, "VENC")) testVENC = flag;
    else if (strstr(str, "VDEC")) testVDEC = flag;
    else if (strstr(str, "ISP")) testISP = flag;
    else if (strstr(str, "G2D")) testG2D = flag;
    else if (strstr(str, "CE")) testCE = flag;
    else if (strstr(str, "VO")) testVO = flag;
    else alogw("str %s is invalid!", str);
}

static int ParseTestCase(struct testcase_base_info *base_info)
{
    int i, j, mainkey_cnt;
    struct testcase_base_info *info;
    char mainkey_name[32];
    char type[32], version[32];
    int enabled, category, run_type;
    int len;
    int total_testcases = 0;
    int total_testcases_auto = 0;
    int total_testcases_manual = 0;

    if (NULL == base_info) {
        aloge("invalid input param, base_info is NULL!");
        return -1;
    }

    mainkey_cnt = script_mainkey_cnt();
    alogd("mainkey_cnt=%d", mainkey_cnt);
    info = (struct testcase_base_info *)malloc(sizeof(struct testcase_base_info) * mainkey_cnt);
    if (info == NULL) {
        db_error("core: allocate memory for temporary test case basic "
                "information failed(%s)\n", strerror(errno));
        return -1;
    }
    memset(info, 0, sizeof(struct testcase_base_info) * mainkey_cnt);

    for (i = 0, j = 0; i < mainkey_cnt; i++) {
        memset(mainkey_name, 0, 32);
        script_mainkey_name(i, mainkey_name);
        alogd("mainkey_name=%s", mainkey_name);

        if (strstr(mainkey_name, "product")) {
            script_fetch(mainkey_name, "type", (int *)type, 1);
            script_fetch(mainkey_name, "version", (int *)version, 1);
            alogd("[product] %s %s", type, version);
        }
        if (strstr(mainkey_name, "platform")) {
            script_fetch(mainkey_name, "type", (int *)type, 1);
            alogd("[platform] %s", type);
        }

        if (script_fetch(mainkey_name, "enabled", &enabled, 1))
            continue;

        alogd("mainkey_name=%s, enabled=%d", mainkey_name, enabled);

        MatchTestCase(mainkey_name, enabled);

        if (enabled == 1) {
            strncpy(info[j].name, mainkey_name, 32);
            info[j].enabled = enabled;

            if (script_fetch(mainkey_name, "category", &category, 1) == 0) {
                alogd("category=%d", category);

                info[j].category = category;

                if(category==0){
                    total_testcases_auto++;
                 }else{
                    total_testcases_manual++;
                 }
            }

            if (script_fetch(mainkey_name, "run_type", &run_type, 1) == 0) {
                alogd("run_type=%d", run_type);
                info[j].run_type = run_type;
            }

            j++;
        }
    }
    total_testcases = j;

    alogd("core: total test cases #%d\n", total_testcases);
    alogd("core: total test cases_auto #%d\n", total_testcases_auto);
    alogd("core: total test cases_manual #%d\n", total_testcases_manual);
    if (total_testcases == 0) {
        if (info) {
            free(info);
            info = NULL;
        }
        return 0;
    }

    if (0 == total_testcases % 2)
    {
        gMaxListNum = total_testcases / 2;
    }
    else
    {
        gMaxListNum = (total_testcases + 1) / 2;
    }
    gYstep = 800 / gMaxListNum;
    alogd("max list num %d, y step %d", gMaxListNum, gYstep);

    len = sizeof(struct testcase_base_info) * total_testcases;
    int base_info_shmid = shmget(IPC_PRIVATE, len, IPC_CREAT | 0666);
    if (base_info_shmid == -1) {
        aloge("core: allocate share memory segment for test case basic "
                "information failed(%s)\n", strerror(errno));
        if (info) {
            free(info);
            info = NULL;
        }
        return -1;
    }

    base_info = (struct testcase_base_info *)shmat(base_info_shmid, 0, 0);
    if (base_info == (void *)-1) {
        aloge("core: attach the share memory for test case basic "
                "information failed(%s)\n", strerror(errno));
        shmctl(base_info_shmid, IPC_RMID, 0);
        if (info) {
            free(info);
            info = NULL;
        }
        return -1;
    }
    memcpy(base_info, info, sizeof(struct testcase_base_info) * total_testcases);
    if (info) {
        free(info);
        info = NULL;
    }

    return total_testcases;
}

static void DeparseTestCase(struct testcase_base_info *base_info)
{
    if (base_info) {
        shmdt(base_info);
        base_info = NULL;
    }

    //total_testcases = 0;
}

#define Main \
	MiniGUIAppMain (int argc, const char* argv[]); \
int main_entry (int argc, const char* argv[]) \
{ \
	    int ret = 0; \
	    ret = MiniGUIAppMain (argc, argv); \
	    TerminateGUI (ret); \
	    return ret; \
} \
int MiniGUIAppMain

int MiniGUIMain(int argc, const char **argv)
//int Main(int argc, const char *argv[])
{
    HWND hMainWnd;
    MSG Msg;

    aloge("Hi dragonboard\n");

    //InitGUI(argc, argv);

    system("dd if=/dev/zero of=/dev/fb0 bs=614400 count=1");
    setenv("FB_SYNC", "1", 1);
    setenv("SCREEN_INFO", "720x1280-32bpp", 1);

    /* load fex config file and parser testcase */
    int script_shmid = 0;
    int ret = 0;
    int total_testcases = 0;
    struct testcase_base_info base_info;
    alogd("core: parse script %s...\n", DRAGONBOARD_TEST_CONFIG_NAME);
    script_shmid = parse_script(DRAGONBOARD_TEST_CONFIG_NAME);
    if (script_shmid == -1) {
        aloge("core: parse script failed, test the default testcases.\n");
        total_testcases = checkDefaultTestcases();
    } else {
        alogd("core: init script...\n");
        ret = init_script(script_shmid);
        if (ret) {
            aloge("core: init script failed(%d)\n", ret);
            deparse_script(script_shmid);
            return -1;
        }

        alogd("core: parse test case from script...\n");
        memset(&base_info, 0, sizeof(struct testcase_base_info));
        total_testcases = ParseTestCase(&base_info);
        if (total_testcases < 0) {
            aloge("core: parse all test case from script failed(%d)\n", ret);
            deinit_script();
            deparse_script(script_shmid);
            return -1;
        }
        else if (total_testcases == 0) {
            alogw("core: NO TEST CASE to be run\n");
            deinit_script();
            deparse_script(script_shmid);
            DeparseTestCase(&base_info);
            return 0;
        }
    }

    /* draw testcase */
    alogd("core: draw testcase");
    //setenv("FB_SYNC", "1", 1);
    //setenv("SCREEN_INFO", "720x1280-32bpp", 1);
    int prompt_w = 720, prompt_h = 1280;
    //show ui
    getScreenInfo(&prompt_w, &prompt_h);

    int y_pos = 0;//+60 是因为这个屏幕是480 高度，实际显示是360，上下都没有了60，所以y要向下偏移60
    int x_pos = 0;
    lcdWidth = GetGDCapability(HDC_SCREEN, GDCAP_MAXX) + 1;
    lcdHeight = GetGDCapability(HDC_SCREEN, GDCAP_MAXY) + 1;
    aloge("LCD width & height: (%d, %d)\n", lcdWidth, lcdHeight);

    MAINWINCREATE CreateInfo;
    CreateInfo.dwStyle= WS_NONE;
//    CreateInfo.dwExStyle = WS_EX_NOCLOSEBOX;
    CreateInfo.dwExStyle = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
    CreateInfo.spCaption =  "dragonboard test";
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor(0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = HelloWinProc;
    CreateInfo.hHosting = HWND_DESKTOP;
    CreateInfo.lx = x_pos;
    CreateInfo.ty = y_pos;
    CreateInfo.rx = lcdWidth;
    CreateInfo.by = 850;
    CreateInfo.iBkColor = COLOR_lightwhite;
    CreateInfo.dwAddData = 0;
    CreateInfo.dwReserved = 0;
    hMainWnd = CreateMainWindow(&CreateInfo);

    HDC hdc = GetDC(hMainWnd);
    //SetWindowBkColor(hMainWnd, RGBA2Pixel(hdc, 0xaf, 0xaf, 0xff, 0x00));
    if (hMainWnd == HWND_INVALID) {
        db_msg("CreateMainWindow error");
        if (script_shmid != -1) {
            deinit_script();
            deparse_script(script_shmid);
            DeparseTestCase(&base_info);
        }
        return -1;
    }

    mLogFont = CreateLogFont("sxf", "arialuni", "UTF-8",
        FONT_WEIGHT_REGULAR, FONT_SLANT_ROMAN, FONT_SETWIDTH_NORMAL,
        FONT_OTHER_AUTOSCALE, FONT_UNDERLINE_NONE, FONT_STRUCKOUT_NONE, 30, 0);
    SetWindowFont(hMainWnd, mLogFont);
    alogd("===============");

    ShowWindow(hMainWnd, SW_SHOWNORMAL);

    /* create and run testcase thread */
    alogd("core: create and run testcase thread");
    /**************SYSTREM**************/
    pthread_t tfTid,start_tfTid,cpuTid,start_cpuTid,keyTid,start_keyTid;
    pthread_t rtcTid,start_rtcTid,ledTid,start_ledTid,usbTid,start_usbTid;
    pthread_t ddrTid,start_ddrTid,norTid,start_norTid,nandTid,start_nandTid;
#ifdef WIFI_TEST
    pthread_t wifiTid,start_wifiTid;
#endif
#ifdef GPS_TEST
    pthread_t gpsTid,start_gpsTid;
#endif
#ifdef GSENSOR_TEST
    pthread_t gsensorTid,start_gsensorTid;
#endif

#if 1
    if(testTf){
        if (pthread_create(&tfTid, NULL, tfThreadRead, NULL) < 0) {
            aloge("pthread_create for tf failed");
        }
        if (pthread_create(&start_tfTid, NULL, runTfthread, NULL) < 0) {
            aloge("pthread_create for tf failed");
        }
    }

    if(testCPU){
        if (pthread_create(&cpuTid, NULL, CPUThreadRead, NULL) < 0) {
            aloge("pthread_create for CPU failed");
        }
    }

#ifdef WIFI_TEST
    if(testWifi){
        if (pthread_create(&wifiTid, NULL, wifiThreadRead, NULL) < 0) {
            aloge("pthread_create for wifi failed");
        }
        if (pthread_create(&start_wifiTid, NULL, runWifiThread, NULL) < 0) {
            aloge("pthread_create for wifi failed");
        }
    }
#endif

#ifdef GPS_TEST
    if(testGPS){
        if (pthread_create(&gpsTid, NULL, GPSThreadRead, NULL) < 0) {
            aloge("pthread_create for gps failed");
        }
        if (pthread_create(&start_gpsTid, NULL, runGPSThread, NULL) < 0) {
            aloge("pthread_create for gps failed");
        }
    }
#endif

#ifdef GSENSOR_TEST
    if(testGSENSOR){
        if (pthread_create(&gsensorTid, NULL, GSENSORThreadRead, NULL) < 0) {
            aloge("pthread_create for gsensor failed");
        }
        if (pthread_create(&start_gsensorTid, NULL, runGSENSORThread, NULL) < 0) {
            aloge("pthread_create for gsensor failed");
        }
    }
#endif

    if(testKey){
        if (pthread_create(&keyTid, NULL, keyThreadRead, NULL) < 0) {
            aloge("pthread_create for key failed");
        }
        if (pthread_create(&start_keyTid, NULL, runKeythread, NULL) < 0) {
            aloge("pthread_create for key failed");
        }
    }

    if(testRtc){
        if (pthread_create(&rtcTid, NULL, rtcThreadRead, NULL) < 0) {
            aloge("pthread_create for rtc failed");
        }
        if (pthread_create(&start_rtcTid, NULL, runRtcthread, NULL) < 0) {
            aloge("pthread_create for rtc failed");
        }
    }

    if(testLed){
        if (pthread_create(&ledTid, NULL, ledThreadRead, NULL) < 0) {
            aloge("pthread_create for led failed");
        }
        if (pthread_create(&start_ledTid, NULL, runLedthread, NULL) < 0) {
            aloge("pthread_create for led failed");
        }
    }

    if(testUsb){
        if (pthread_create(&usbTid, NULL, usbThreadRead, NULL) < 0) {
            aloge("pthread_create for usb failed");
        }
        if (pthread_create(&start_usbTid, NULL, runUsbthread, NULL) < 0) {
            aloge("pthread_create for usb failed");
        }
    }

    if(testDdr){
        if (pthread_create(&ddrTid, NULL, DDRThreadRead, NULL) < 0) {
            aloge("pthread_create for ddr failed");
        }
        if (pthread_create(&start_ddrTid, NULL, runDDRthread, NULL) < 0) {
            aloge("pthread_create for ddr failed");
        }
    }

    if(testNor){
        if (pthread_create(&norTid, NULL, NORThreadRead, NULL) < 0) {
            aloge("pthread_create for nor failed");
        }
        if (pthread_create(&start_norTid, NULL, runNORthread, NULL) < 0) {
            aloge("pthread_create for nor failed");
        }
    }

    if(testNand){
        if (pthread_create(&nandTid, NULL, NANDThreadRead, NULL) < 0) {
            aloge("pthread_create for nand failed");
        }
        if (pthread_create(&start_nandTid, NULL, runNANDthread, NULL) < 0) {
            aloge("pthread_create for nand failed");
        }
    }

    /****************MPP****************/
    pthread_t start_mppTid;
    if (pthread_create(&start_mppTid, NULL, runMPPthread, NULL) < 0) {
       aloge("pthread_create for MPP failed");
    }
#endif
    // write test result to sd card.
    pthread_t test_resultTid;
    if (pthread_create(&test_resultTid, NULL, saveResultThread, &total_testcases) < 0) {
       aloge("pthread_create for test_result failed");
   }

    while (GetMessage(&Msg, hMainWnd)) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    if (script_shmid != -1) {
        deinit_script();
        deparse_script(script_shmid);
        DeparseTestCase(&base_info);
    }

    DestroyLogFont(mLogFont);
    MainWindowCleanup(hMainWnd);
    aloge("bye dragonboard");

    _exit(0);
    return 0;
}

#ifndef _MGRM_PROCESSES
#include <minigui/dti.c>
#endif
