#ifndef __SAMPLE_VI2VENC2MUXER_CONF_H__
#define __SAMPLE_VI2VENC2MUXER_CONF_H__

#define CFG_VIPP_DEV_ID         "vipp_id"
#define CFG_SRC_WIDTH           "src_width"
#define CFG_SRC_HEIGHT          "src_height"
#define CFG_DST_VI_BUFFER_NUM   "vi_buffer_num"
#define CFG_SRC_PIXFMT          "src_pixfmt"
#define CFG_COLOR_SPACE         "color_space"
#define CFG_DROP_FRAME_NUM      "drop_frm_num"

#define CFG_VENC_CH_ID          "venc_ch_id"
#define CFG_DST_VIDEO_FILE_STR  "video_dst_file"
#define CFG_ADD_REPAIR_INFO     "add_repair_info"
#define CFG_FRMSTAG_BACKUP_INTERVAL "frmsTag_backup_interval"
#define CFG_DST_FILE_MAX_CNT    "dst_file_max_cnt"

#define CFG_DST_VIDEO_WIDTH     "video_width"
#define CFG_DST_VIDEO_HEIGHT    "video_height"
#define CFG_DST_VIDEO_FRAMERATE "video_framerate"
#define CFG_DST_VIDEO_BITRATE   "video_bitrate"
#define CFG_DST_VIDEO_ENCODER   "video_encoder"
#define CFG_DST_VIDEO_DURATION  "video_duration"
#define CFG_DST_ENCODE_PROFILE  "profile"
#define CFG_PRODUCT_MODE        "product_mode"
#define CFG_SENSOR_TYPE         "sensor_type"
#define CFG_KEY_FRAME_INTERVAL  "key_frame_interval"
#define CFG_RC_MODE  "rc_mode"
#define CFG_RC_QP0  "qp0"
#define CFG_RC_QP1  "qp1"

#define CFG_GOP_MODE  "gop_mode"
#define CFG_GOP_SIZE  "gop_size"

#define CFG_AdvancedRef_Base        "AdvancedRef_Base"
#define CFG_AdvancedRef_Enhance     "AdvancedRef_Enhance"
#define CFG_AdvancedRef_RefBaseEn   "AdvancedRef_RefBaseEn"
#define CFG_FAST_ENC  "enable_fast_enc"
#define CFG_ENABLE_SMART  "enable_smart"
#define CFG_SVC_LAYER  "svc_layer"
#define CFG_ENCODE_ROTATE "encode_rotate"
#define CFG_TEST_DURATION  "test_duration"

#define CFG_MIRROR          "mirror"
#define CFG_COLOR2GREY      "color2grey"
#define CFG_3DNR            "3dnr"

#define CFG_ROI_NUM         "roi_num"
#define CFG_ROI_QP          "roi_qp"
#define CFG_ROI_BgFrameRateEnable       "roi_BgFrameRateEnable"
#define CFG_ROI_BgFrameRateAttenuation  "roi_BgFrameRateAttenuation"

#define CFG_IntraRefresh_BlockNum  "IntraRefresh_BlockNum"
#define CFG_ORL_NUM  "orl_num"

#define CFG_vbvBufferSize  "vbvBufferSize"
#define CFG_vbvThreshSize  "vbvThreshSize"

#define CFG_CROP_ENABLE       "crop_en"
#define CFG_CROP_RECT_X       "crop_rect_x"
#define CFG_CROP_RECT_Y       "crop_rect_y"
#define CFG_CROP_RECT_WIDTH   "crop_rect_w"
#define CFG_CROP_RECT_HEIGHT  "crop_rect_h"

#define CFG_vui_timing_info_present_flag  "vui_timing_info_present_flag"
#define CFG_Ve_Freq  "ve_freq"

#define CFG_online_en  "online_en"
#define CFG_online_share_buf_num  "online_share_buf_num"

#define CFG_WDR_EN   "wdr_en"
#define CFG_EnMbQpLimit  "mb_qp_limit_en"
#define CFG_EnableGdc "enable_gdc"

#endif //#define __SAMPLE_VI2VENC2MUXER_CONF_H__
