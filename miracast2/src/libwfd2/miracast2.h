#ifndef __MIRACAST_LINUX_H__
#define __MIRACAST_LINUX_H__
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOURCE_NAME_MAX_LEN (255)

/**Miracast State*//** CNcomment:Miracast 状态 */
typedef enum MIRACAST_STATE
{
    INVALID = 0,
    INIT = 1,
    START = 2,
    CONNECT = 3,
    DISCONNECT = START,
    STOP = INIT,
    DEINIT = INVALID,
} MIRACAST_STATE;

typedef enum LOWDELAY_MODE_E
{
    MIRACAST_LOWDELAY_FIRST,
    MIRACAST_LOWDELAY_SMOOTH_LEVEL1
} MIRACAST_LOWDELAY_MODE_E;

/**Miracast callback function type*//** CNcomment:Miracast 回调函数的类型 */
typedef enum MIRACAST_EVENT_CALLBACK_E
{
    MIRACAST_P2P_CBK_INVALID = -1,               /**<Invalid Enum - Lower Bound Value *//**<CNcomment:无效Enum-下限值 */

    MIRACAST_CBK_P2P_PEERS_CHANGED = 0,          /**< P2P peers changed *//** CNcomment:P2P对端状态变化的回调类型 */
    MIRACAST_CBK_P2P_FOUND,                      /**< p2p found *//** CNcomment:P2P 璁惧鍙戠幇 */
    MIRACAST_CBK_P2P_CONNECTING,                 /**< P2P connecting *//** CNcomment:source端和sink端建立连接中的回调类型 */
    MIRACAST_CBK_P2P_CONNECTED,                  /**< P2P connected *//** CNcomment:source端和sink端连接成功的回调类型 */
    MIRACAST_CBK_P2P_DISCONNECTED,               /**< P2P disconnected *//** CNcomment:source端和sink端断开连接的回调类型 */
    MIRACAST_CBK_P2P_PERSISTENT_GROUPS_CHANGED,  /**< P2P group changed *//** CNcomment:P2P组变化的回调类型 */
    MIRACAST_CHK_P2P_GET_PEERS_ADDRESS,          /**< P2P get peers address*//** CNcomment:成功获取对端IP */

    MIRACAST_CBK_PLAYER_START_ERROR = 10,        /**< Player start error *//** CNcomment:播放器启动失败 */
    MIRACAST_CBK_PLAYER_STOP_FINISHED,           /**< Player start error *//** CNcomment:播放器关闭结束 */
    MIRACAST_CBK_PLAYER_FIRST_SHOW,              /**< Player start error *//** CNcomment:播放器显示第一帧 */
    MIRACAST_CBK_HDCP_INIT_ERROR,                /**< Hdcp init error *//** CNcomment:HDCP初始化失败 */
    MIRACAST_CBK_HDCP_START_ERROR,               /**< Hdcp start error *//** CNcomment:HDCP启动失败 */

    MIRACAST_CBK_RTXP_NETWORK_ERROR = 15,        /**< Rtsp/Rtcp/Rtp Network Error *//** CNcomment:Rtsp/Rtcp/Rtp网络连接失败 */
    MIRACAST_CBK_RTP_LOST_PACKET,                /**< Rtp lost packet *//** CNcomment:RTP数据包丢包 */
    MIRACAST_CBK_WFD_START_FINISHED,             /**< Wfd start finished *//** CNcomment:wifi display协议连接成功 */
    MIRACAST_CBK_WFD_STOP_FINISHED,              /**< Wfd stop finished *//** CNcomment:wifi display协议断开连接成功 */

    MIRACAST_CHK_P2P_NEGOTIATION_ERROR =20,      /**< p2p negotiation error *//** CNcomment:P2P协商失败 */
    MIRACAST_CHK_P2P_FORMATION_ERROR,            /**< p2p formation error *//** CNcomment:GO/GC交互失败 */
    MIRACAST_CHK_P2P_TIMEOUT_ERROR,              /**< p2p connect timeout error *//** CNcomment:P2P连接超时 */
    MIRACAST_CHK_P2P_OVERLAP_ERROR,              /**< p2p overlap error *//** CNcomment:在P2P信号范围内有多设备同时连接导致失败 */

    MIRACAST_CBK_P2P_BUTT,                       /**<Invalid Enum - Higher Bound Value *//**<CNcomment:无效Enum-上限值 */
	MIRACAST_CBK_P2P_GO_NEG_REQUEST,
	MIRACAST_CBK_P2P_INVITATION_ACCEPTED,
	MIRACAST_CBK_P2P_GROUP_STARTED,
} MIRACAST_EVENT_CALLBACK_E;

/**Miracast p2p net mode type *//** CNcomment:Miracast 网络模式类型 */
typedef enum MIRACAST_P2P_NETMODE_E
{
    MIRACAST_P2P_NETMODE_DEFAULT = 0,   /** default P2p net mode, decide by wifi driver, concurrent mode priority *//** CNcomment: 缺省网络模式，共存优先 */
    MIRACAST_P2P_NETMODE_CONCURRENT,    /** concurrent P2p net mode, P2p concurrent with sta *//** CNcomment: P2P与STA共存模式 */
    MIRACAST_P2P_NETMODE_STANDALONE,    /** standalone P2p net mode, P2p doesn't concurrent with sta *//** CNcomment: P2P独立模式，不与STA共存 */
    MIRACAST_P2P_NETMODE_BUTT
} MIRACAST_P2P_NETMODE_E;

/**Miracast p2p groupowner mode *//** CNcomment:Miracast Group Owner模式 */
typedef enum MIRACAST_P2P_GOMODE_E
{
    MIRACAST_P2P_GOMODE_DEFAULT = 0,   /** default P2p go mode, decide by wifi driver, 2.4G force and 5G negotiation priority *//** CNcomment:缺省P2p GO模式，优先策略为2.4强制GO，5G协商GO */
    MIRACAST_P2P_GOMODE_NEGOTIATION,   /** P2p go negotiation *//** CNcomment:P2p协商GO模式 */
    MIRACAST_P2P_GOMODE_FORCEGO,       /** P2p force go *//** CNcomment:P2p强制GO模式 */
    MIRACAST_NETMODE_BUTT
} MIRACAST_P2P_GOMODE_E;

typedef struct MIRACAST_P2P_CONNECTING_INFO
{
    char sourceName[SOURCE_NAME_MAX_LEN + 1];
} MIRACAST_P2P_CONNECTING_INFO;

typedef struct MIRACAST_LOST_INFO
{
    uint32_t totalPacket;
    uint32_t lostPacket;
} MIRACAST_LOST_INFO;

/** Callback function of receiving Miracast events *//** CNcomment:接收Miracast事件的回调函数 */
typedef int (*Miracast_Event_CallBack)(MIRACAST_EVENT_CALLBACK_E enEvent, void* pvPrivateData);

/**
\brief: init Miracast.CNcomment:初始化Miracast CNend
\attention \n
\param[in] whether support HDCP.CNcomment:是否支持HDCP功能 CNend
\retval  ::SUCCESS
\retval  ::FAILURE
\see \n
::Miracast_DeInit
*/
int Miracast_Init(int isHdcp);

/* p2p_interface: wlan0 or wlan1 or p2p0 */
int Miracast_Init_ex(int isHdcp, const char *p2p_interface);

/**
\brief: start Miracast.CNcomment:启动Miracast CNend
\attention \n
\param[in] pcDeviceName sink device name, must be less than 33 bytes, can be null.CNcomment:板端设备名字，必须小于33个字节，可以为空 CNend
\param[in] pFnEventCb callback func.CNcomment:回调函数实现 CNend
\retval  ::SUCCESS
\retval  ::FAILURE
\see \n
::Miracast_Stop
*/
int Miracast_Start(const char* pcDeviceName, Miracast_Event_CallBack pFnEventCb);

/**
\brief: disconnect connection.CNcomment:断开连接 CNend
\attention \n
\param    N/A.CNcomment:无 CNend
\retval  ::SUCCESS
\retval  ::FAILURE
\see \n
::Miracast_Disconnect
*/
int Miracast_Disconnect(void);

/**
\brief: stop Miracast.CNcomment:停用Miracast CNend
\attention \n
\param     N/A.CNcomment:无 CNend
\retval  ::SUCCESS
\retval  ::FAILURE
\see \n
::Miracast_Stop
*/
int Miracast_Stop(void);

/**
\brief: deinit Miracast.CNcomment:去初始化Miracast CNend
\attention \n
\param     N/A.CNcomment:无 CNend
\retval  ::SUCCESS
\retval  ::FAILURE
\see \n
::Miracast_DeInit
*/
int Miracast_DeInit(void);

/**
\brief: get Miracast Current State.CNcomment:获取Miracast当前状态 CNend
\attention \n
\param     N/A.CNcomment:无 CNend
\retval  ::MIRACAST_STATE
\see \n
::
*/
MIRACAST_STATE Miracast_GetState(void);

/**
\brief: modify Miracast Name.CNcomment:修改Miracast名字 CNend
\attention \n
\param[in] pcDeviceName sink device name, must be less than 33 bytes, can be null.CNcomment:板端设备名字，必须小于33个字节，可以为空 CNend
\retval  ::SUCCESS
\retval  ::FAILURE
\see \n
::
*/
int Miracast_ModifyName(const char* pcDeviceName);

/**
\brief: set Miracast P2 net mode.CNcomment:设置Miracast网络模式 CNend
\attention \n
\param[in] p2pNetMode p2p net mode, default or concurrent or standalone.CNcomment:P2p网络模式，系统缺省的模式或者与STA共存模式或者独立不共存模式 CNend
\retval  ::SUCCESS
\retval  ::FAILURE
\see \n
*/
int Miracast_SetP2pNetMode(MIRACAST_P2P_NETMODE_E p2pNetMode);

/**
\brief: set Miracast P2 Group Owner mode.CNcomment:设置Miracast Group Owner模式 CNend
\attention \n
\param[in] p2pGoMode p2p go mode, default or force or negotiation.CNcomment:P2p go模式，缺省的模式或者强制GO模式或者自动协商模式 CNend
\retval  ::SUCCESS
\retval  ::FAILURE
\see \n
*/
int Miracast_SetP2pGOMode(MIRACAST_P2P_GOMODE_E p2pGoMode);

void Miracast_SetLowDelay(MIRACAST_LOWDELAY_MODE_E lowDelayMode);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif /* __MIRACAST_LINUX_H__ */
