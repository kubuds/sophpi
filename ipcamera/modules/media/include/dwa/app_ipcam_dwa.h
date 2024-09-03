#ifndef __APP_IPCAM_DWA_H__
#define __APP_IPCAM_DWA_H__
#include "cvi_dwa.h"
#include <linux/cvi_math.h>
#include <linux/cvi_type.h>
#include <linux/cvi_common.h>
#include "linux/cvi_comm_video.h"
#include "cvi_vb.h"
#include "cvi_vpss.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**************************************************************************
 *                        H E A D E R   F I L E S
 **************************************************************************/


 /**************************************************************************
 *                              M A C R O S
 **************************************************************************/

 /**************************************************************************
 *                          C O N S T A N T S
 **************************************************************************/

/**************************************************************************
 *                          D A T A   T Y P E S
 **************************************************************************/

#define DWA_MAX_CFG_NUM 8

typedef enum _DWA__OP {
	DWA_FISHEYE = 0,
	DWA_ROT,
	DWA_LDC,
	DWA_AFFINE,
} DWA_OP;

typedef struct BIND_MOD_S {
    CVI_U32 bind_mod_id;
    CVI_U32 bind_dev_id;
    CVI_U32 bind_chn_id;
} BIND_MOD_T;

typedef struct APP_LDC_ATTR_S {
    CVI_BOOL bEnable;
    LDC_ATTR_S stAttr;
} APP_LDC_ATTR_T;


typedef struct APP_LDC_CFG_S {
    APP_LDC_ATTR_T stLdcAttr;
    BIND_MOD_T stMod;
} APP_LDC_CFG_T;

typedef struct APP_AFFINE_ATTR_S {
    CVI_BOOL bEnable;
    AFFINE_ATTR_S stAffineAttr;
} APP_AFFINE_ATTR_T;

typedef struct APP_DWA_CFG_S {
    CVI_BOOL bEnable;
    GDC_HANDLE hHandle;
    SIZE_S size_in;
    SIZE_S size_out;
    PIXEL_FORMAT_E enPixelFormat;
    CVI_U32 u32Operation;
    GDC_IDENTITY_ATTR_S identity;
    GDC_TASK_ATTR_S stTask;
    ROTATION_E enRotation;
    APP_LDC_CFG_T LdcCfg;
    FISHEYE_ATTR_S FisheyeAttr;
    APP_AFFINE_ATTR_T AffineAttr;
    CVI_CHAR filename_in[64];
    CVI_CHAR filename_out[64];
} APP_DWA_CFG_T;

typedef struct APP_PARAM_DWA_CFG_S {
    CVI_U32 u32CfgCnt;
    CVI_BOOL bUserEnable;
    APP_DWA_CFG_T astDwaCfg[DWA_MAX_CFG_NUM];
} APP_PARAM_DWA_CFG_T;

 /**************************************************************************
 *              F U N C T I O N   D E C L A R A T I O N S
 **************************************************************************/

APP_PARAM_DWA_CFG_T *app_ipcam_Dwa_Param_Get(void);

int app_ipcam_Ldc_Init(void);
int app_ipcam_Dwa_Init(void);
int app_ipcam_Dwa_DeInit(void);
int app_ipcam_Dwa_SendFrame(APP_DWA_CFG_T *pstDwaCfg, VIDEO_FRAME_INFO_S *pstVideoFrame);
int app_ipcam_Dwa_GetFrame(APP_DWA_CFG_T *pstDwaCfg, VIDEO_FRAME_INFO_S *pstVideoFrame);
int app_ipcam_Dwa_SendFile(APP_DWA_CFG_T *pstDwaCfg);


#ifdef __cplusplus
}
#endif

#endif /* __APP_IPCAM_DWA_H__ */