#ifndef __APP_IPCAM_VDEC_SOFT_H__
#define __APP_IPCAM_VDEC_SOFT_H__

#include "linux/cvi_type.h"
#include <linux/cvi_common.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SWS_FAST_BILINEAR 1
#define VENC_SOFT_CHN_MAX 8

typedef enum APP_VDEC_SOFT_CHN_T {
    APP_VDEC_SOFT_NULL = 0,
    APP_VDEC_SOFT_1ST = 0x01,
    APP_VDEC_SOFT_2ND = 0x02,
    APP_VDEC_SOFT_3RD = 0x04,
    APP_VDEC_SOFT_4TH = 0x08,
    APP_VDEC_SOFT_5TH = 0x10,
    APP_VDEC_SOFT_6TH = 0x20,
    APP_VDEC_SOFT_7TH = 0x40,
    APP_VDEC_SOFT_8TH = 0x80,
    APP_VDEC_SOFT_ALL = 0xFF
} APP_VDEC_SOFT_CHN_E;

typedef struct APP_VDEC_SOFT_CHN_CFG_T {
    CVI_BOOL bEnable;
    VDEC_CHN vdecChn;
    CVI_S32 de_type;
    CVI_CHAR decode_file_name[32];
    FILE *fpStrm;

    const struct AVCodec *pAVCodec;
    struct AVCodecContext *pAVCodecCtx;
    AVPacket *pstAVPacket;
    struct SwsContext *pstConvCtx;
    struct AVFrame *pstDecFrame;
    struct AVFrame *pstResultFrame;
    
    CVI_U32 u32Width;
    CVI_U32 u32Height;
    CVI_U32 videoFormat;
    CVI_U32 scaleFormat;
    CVI_U32 s32UsedBytes;
} APP_VDEC_SOFT_CHN_CFG_S;

typedef struct APP_PARAM_VDEC_SOFT_CTX_T {
    CVI_S32 s32VdecChnCnt;
    APP_VDEC_SOFT_CHN_CFG_S astVdecSoftChnCfg[VENC_SOFT_CHN_MAX];
} APP_PARAM_VDEC_SOFT_CTX_S;

APP_PARAM_VDEC_SOFT_CTX_S *app_ipcam_Vdec_Soft_Param_Get();
CVI_S32 app_ipcam_Vdec_Soft_Proc(APP_VDEC_SOFT_CHN_CFG_S *pstVdecSoftChnCfg);
CVI_S32 app_ipcam_Vdec_Soft_Start(APP_VDEC_SOFT_CHN_E VencSoftIdx);
CVI_S32 app_ipcam_Vdec_Soft_Stop(APP_VDEC_SOFT_CHN_E VencSoftIdx);
CVI_S32 app_ipcam_Vdec_Soft_Init(CVI_VOID);
CVI_S32 app_ipcam_Vdec_Soft_DeInit(CVI_VOID);

#ifdef __cplusplus
}
#endif

#endif
