#ifndef __APP_IPCAM_DISPLAY_FRM_BUF_H__
#define __APP_IPCAM_DISPLAY_FRM_BUF_H__

#include <sys/prctl.h>
#include "linux/cvi_type.h"
#include <linux/cvi_common.h>
#include "app_ipcam_frmbuf.h"

#ifdef VDEC_SOFT
#include "app_ipcam_vdec_soft.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct APP_PARAM_DISP_FRMBUF_CTX_T {
    CVI_BOOL bEnable;
    CVI_U32 u32Width;
    CVI_U32 u32Height;
    CVI_U32 thrFlag;
    pthread_t dispFrmBufThr;
    CVI_U32 vdecSoftEnable;
    CVI_U32 drawCharEnable;
    APP_PARAM_FRMBUF_CTX_S *pstFrmBufCtx;
    CVI_VOID *pVdecSoftCtx;

} APP_PARAM_DISP_FRMBUF_CTX_S;

APP_PARAM_DISP_FRMBUF_CTX_S *app_ipcam_Disp_FrmBuf_Param_Get(CVI_VOID);
CVI_S32 app_ipcam_Disp_FrmBuf_Start(CVI_VOID);
CVI_S32 app_ipcam_Disp_FrmBuf_Stop(CVI_VOID);

#ifdef __cplusplus
}
#endif

#endif
