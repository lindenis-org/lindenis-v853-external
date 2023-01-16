#this file is used for EyeseeLinux compile configuration
############################################################################
#[config item], valid in makefile.
#config chip version: AW1721
##MPPCFG_CHIP := AW1816
#config write file method: LINUX_VFS, DIRECT_IO
##MPPCFG_FILE_SYSTEM := LINUX_VFS
#config if use hw display method
##MPPCFG_HW_DISPLAY := Y
#config if include libmpp_ise.so and enable ise function.
##MPPCFG_ISE := Y
#config if include libmpp_ise.so and enable ise mo module.
##MPPCFG_ISE_MO := Y
#config if include libmpp_ise.so and enable ise bi module.
##MPPCFG_ISE_BI := N
#config if include libmpp_ise.so and enable ise bi soft module.
##MPPCFG_ISE_BI_SOFT := N
#config if include libmpp_ise.so and enable ise sti module.
##MPPCFG_ISE_STI := N
#config if include libmpp_eis.so and enable eis function.
##MPPCFG_EIS := N
#config if include libai_MOD.so and enable MOD(Motion Object Detection) detect.
##MPPCFG_MOD := N
#config if include libeve_face.so and enable face detect.
##MPPCFG_EVEFACE := N
#config if include libai_VLPR.so and enable VLPR(Vehicle License Plate Recognition) detect.
##MPPCFG_VLPR := N
#config if include libai_HCNT.so and enable HCNT(human count) detect.
##MPPCFG_HCNT := N
#config if include libai_BDII.so and enable BDII() detect.
##MPPCFG_BDII := N

#config if include libmpp_uvc.so
##MPPCFG_UVC := N

#prebuild all AI libs and component by static
##MPPCFG_COMPILE_STATIC_LIB := Y
#build component to dynamic so if possible
##MPPCFG_COMPILE_DYNAMIC_LIB := Y

############################################################################
#[define macro according to config item], valid in c/cpp file.
CEDARX_EXT_CFLAGS := 
# define some macro to config mpp
#CEDARX_EXT_CFLAGS += -DMPPCFG_CHIP_$(MPPCFG_CHIP)
ifeq ($(MPPCFG_CHIP), AW1721)
  CEDARX_EXT_CFLAGS += -DAWCHIP=0x1721#AW_V5, must keep sync with []plat_defines.h]
else ifeq ($(MPPCFG_CHIP), AW1816)
  CEDARX_EXT_CFLAGS += -DAWCHIP=0x1816#AW_V316
else ifeq ($(MPPCFG_CHIP), AW1817)
  CEDARX_EXT_CFLAGS += -DAWCHIP=0x1817#AW_V459
else ifeq ($(MPPCFG_CHIP), AW1886)
  CEDARX_EXT_CFLAGS += -DAWCHIP=0x1886#AW_V853
else
  CEDARX_EXT_CFLAGS += -DAWCHIP=0x1721#AW_V5
endif

ifeq ($(MPPCFG_SUPPORT_FASTBOOT),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_SUPPORT_FASTBOOT=1
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_SUPPORT_FASTBOOT=0
endif

ifeq ($(MPPCFG_FILE_SYSTEM),LINUX_VFS)
  CEDARX_EXT_CFLAGS += -DCDXCFG_FILE_SYSTEM=0#OPTION_FILE_SYSTEM_LINUX_VFS
else ifeq ($(MPPCFG_FILE_SYSTEM),DIRECT_IO)
  CEDARX_EXT_CFLAGS += -DCDXCFG_FILE_SYSTEM=2#OPTION_FILE_SYSTEM_DIRECT_IO
else
  CEDARX_EXT_CFLAGS += -DCDXCFG_FILE_SYSTEM=0
endif

ifeq ($(MPPCFG_HW_DISPLAY),Y)
  CEDARX_EXT_CFLAGS += -DCDXCFG_HW_DISPLAY=1#OPTION_HW_DISPLAY_ENABLE
else
  CEDARX_EXT_CFLAGS += -DCDXCFG_HW_DISPLAY=0#OPTION_HW_DISPLAY_DISABLE
endif

ifeq ($(MPPCFG_VI),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_VI=1#OPTION_VI_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_VI=0#OPTION_VI_DISABLE
endif

ifeq ($(MPPCFG_VO),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_VO=1#OPTION_VO_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_VO=0#OPTION_VO_DISABLE
endif

ifeq ($(MPPCFG_TEXTENC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_TEXTENC=1#OPTION_TEXTENC_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_TEXTENC=0#OPTION_TEXTENC_DISABLE
endif

ifeq ($(MPPCFG_VENC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_VENC=1#OPTION_VENC_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_VENC=0#OPTION_VENC_DISABLE
endif

ifeq ($(MPPCFG_VDEC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_VDEC=1#OPTION_VDEC_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_VDEC=0#OPTION_VDEC_DISABLE
endif

ifeq ($(MPPCFG_AIO),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_AIO=1#OPTION_AIO_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_AIO=0#OPTION_AIO_DISABLE
endif

ifeq ($(MPPCFG_AENC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC=1#OPTION_AENC_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC=0#OPTION_AENC_DISABLE
endif
ifeq ($(MPPCFG_AENC_PCM),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC_PCM=1
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC_PCM=0
endif
ifeq ($(MPPCFG_AENC_G711),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC_G711=1
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC_G711=0
endif
ifeq ($(MPPCFG_AENC_G726),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC_G726=1
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC_G726=0
endif
ifeq ($(MPPCFG_AENC_AAC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC_AAC=1
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC_AAC=0
endif
ifeq ($(MPPCFG_AENC_MP3),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC_MP3=1
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_AENC_MP3=0
endif

ifeq ($(MPPCFG_ADEC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_ADEC=1#OPTION_ADEC_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_ADEC=0#OPTION_ADEC_DISABLE
endif

ifeq ($(MPPCFG_MUXER),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_MUXER=1#OPTION_MUXER_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_MUXER=0#OPTION_MUXER_DISABLE
endif

ifeq ($(MPPCFG_DEMUXER),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_DEMUXER=1#OPTION_DEMUXER_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_DEMUXER=0#OPTION_DEMUXER_DISABLE
endif

ifeq ($(MPPCFG_ISE),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_ISE=1#OPTION_ISE_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_ISE=0#OPTION_ISE_DISABLE
endif

ifeq ($(MPPCFG_ISE_MO),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_ISE_MO=1#OPTION_ISE_MO_ENABLE
else 
  CEDARX_EXT_CFLAGS += -DMPPCFG_ISE_MO=0#OPTION_ISE_MO_DISABLE
endif

ifeq ($(MPPCFG_ISE_GDC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_ISE_GDC=1#OPTION_ISE_GDC_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_ISE_GDC=0#OPTION_ISE_GDC_DISABLE
endif

ifeq ($(MPPCFG_ISE_TWO_FISHEYE),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_ISE_TWO_FISHEYE=1
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_ISE_TWO_FISHEYE=0
endif

ifeq ($(MPPCFG_ISE_TWO_ISE),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_ISE_TWO_ISE=1
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_ISE_TWO_ISE=0
endif

ifeq ($(MPPCFG_EIS),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_EIS=1#OPTION_EIS_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_EIS=0#OPTION_EIS_DISABLE
endif

ifeq ($(MPPCFG_MOTION_DETECT_SOFT),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_MOTION_DETECT_SOFT=1#OPTION_MOTION_DETECT_SOFT_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_MOTION_DETECT_SOFT=0#OPTION_MOTION_DETECT_SOFT_DISABLE
endif

ifeq ($(MPPCFG_VLPR),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_VLPR=1#OPTION_VLPR_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_VLPR=0#OPTION_VLPR_DISABLE
endif

ifeq ($(MPPCFG_ADAS_DETECT),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_ADAS_DETECT=1#OPTION_ADAS_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_ADAS_DETECT=0#OPTION_ADAS_DISABLE
endif

ifeq ($(MPPCFG_ADAS_DETECT_V2),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_ADAS_DETECT_V2=1#OPTION_ADAS_V2_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_ADAS_DETECT_V2=0#OPTION_ADAS_V2_DISABLE
endif

ifeq ($(MPPCFG_BDII),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_BDII=1#OPTION_BDII_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_BDII=0#OPTION_BDII_DISABLE
endif

ifeq ($(MPPCFG_EVEFACE),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_EVEFACE=1#OPTION_EVEFACE_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_EVEFACE=0#OPTION_EVEFACE_DISABLE
endif

ifeq ($(MPPCFG_AEC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_AEC=1#OPTION_AEC_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_AEC=0#OPTION_AEC_DISABLE
endif
ifeq ($(MPPCFG_SOFTDRC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_SOFTDRC=1#OPTION_SOFTDRC_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_SOFTDRC=0#OPTION_SOFTDRC_DISABLE
endif
ifeq ($(MPPCFG_AGC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_AGC=1#OPTION_AGC_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_AGC=0#OPTION_AGC_DISABLE
endif

ifeq ($(MPPCFG_AI_AGC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_AI_AGC=1#OPTION_AGC_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_AI_AGC=0#OPTION_AGC_DISABLE
endif

ifeq ($(MPPCFG_ANS),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_ANS=1#OPTION_ANS_ENABLE
else
  CEDARX_EXT_CFLAGS += -DMPPCFG_ANS=0#OPTION_ANS_DISABLE
endif

ifeq ($(MPPCFG_ANS_LIB),libwebrtc)
  CEDARX_EXT_CFLAGS += -DMPPCFG_ANS_LIB=0#SELECT ANS LIBRARY FROM WEBRTC
endif

ifeq ($(MPPCFG_ANS_LIB),liblstm)
  CEDARX_EXT_CFLAGS += -DMPPCFG_ANS_LIB=1#SELECT ANS LIBRARY FROM LSTM
endif

ifeq ($(MPPCFG_UVC),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_UVC
endif

ifeq ($(MPPCFG_USE_IOMMU),Y)
  export USE_IOMMU := true#for libcedarc
endif

ifeq ($(MPPCFG_SYSTRACE),Y)
  CEDARX_EXT_CFLAGS += -DMPPCFG_SYSTRACE
endif

