#ifndef __APP_IPCAM_PARAM_PARSE_H__
#define __APP_IPCAM_PARAM_PARSE_H__

#include "linux/cvi_common.h"
#include "linux/cvi_comm_video.h"
#include "app_ipcam_comm.h"
#include "app_ipcam_sys.h"
#include "app_ipcam_vi.h"
#include "app_ipcam_vpss.h"
#include "app_ipcam_osd.h"
#include "app_ipcam_venc.h"

#ifdef DWA_SUPPORT
#include "app_ipcam_dwa.h"
#endif

#ifdef RTSP_SUPPORT
#include "app_ipcam_rtsp.h"
#endif
#ifdef VDEC 
#include "app_ipcam_vdec.h"
#endif

#ifdef FRMBUF
#include "app_ipcam_frmbuf.h"
#endif

#ifdef VDEC_SOFT 
#include "app_ipcam_vdec_soft.h"
#endif

#ifdef DISP_FRMBUF
#include "app_ipcam_disp_frmbuf.h"
#endif

#ifdef DISPLAY
#include "app_ipcam_display.h"
#endif

#ifdef FRMBUF
#include "app_ipcam_frmbuf.h"
#endif

#ifdef AUDIO_SUPPORT
#include "app_ipcam_audio.h"
#endif

#ifdef AI_SUPPORT
#include "app_ipcam_ai.h"
#endif

#ifdef MD_SUPPORT
#include "app_ipcam_md.h"
#endif

#ifdef RECORD_SUPPORT
#include "app_ipcam_record.h"
#endif

#ifdef STITCH_SUPPORT
#include "app_ipcam_stitch.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#define PARAM_STRING_LEN (32)
#define PARAM_STRING_NAME_LEN (128)

#define VI_FRAMERATE			15

int app_ipcam_Param_Convert_StrName_to_EnumNum(const char *str_name, const char *str_enum[], const int enum_upper_bound, int * const enum_num);
//common attribute
const char ** app_ipcam_Param_get_pixel_format();
const char ** app_ipcam_Param_get_data_bitwidth();
const char ** app_ipcam_Param_get_compress_mode();
const char ** app_ipcam_Param_get_vi_vpss_mode();
const char ** app_ipcam_Param_get_mode_id();
const char ** app_ipcam_Param_get_video_format();
const char ** app_ipcam_Param_get_payload_type();
#ifdef AI_SUPPORT
const char ** app_ipcam_Param_get_ai_supported_model();
#endif

int app_ipcam_Param_Load(void);

int app_ipcam_Opts_Parse(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif
