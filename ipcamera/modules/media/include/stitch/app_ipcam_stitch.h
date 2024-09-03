#ifndef __APP_IPCAM_STITCH_H__
#define __APP_IPCAM_STITCH_H__

#include <semaphore.h>
#include <linux/cvi_comm_stitch.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum APP_IPCAM_STITCH_BIND_MODE_E {
    STITCH_BIND_DISABLE = 0,
    STITCH_BIND_VENC,
    STITCH_BIND_SEP,
};

typedef struct APP_STITCH_SRC_CFG_S {
    STITCH_SRC_IDX src_idx;
    MOD_ID_E enModId;
    CVI_S32 s32DevId;
    CVI_S32 s32ChnId;
    VIDEO_FRAME_INFO_S stImgIn;
    sem_t *semv;
} APP_STITCH_SRC_CFG_S;

typedef struct APP_PARAM_STITCH_CFG_S {
    CVI_U8 srcNum;
    APP_STITCH_SRC_CFG_S srcParam[STITCH_MAX_SRC_NUM];
    STITCH_SRC_ATTR_S srcAttr;
    STITCH_OP_ATTR_S opAttr;
    STITCH_WGT_ATTR_S wgtAttr;
    STITCH_CHN_ATTR_S chnAttr;
    CVI_U32 enBindMode;
    MMF_CHN_S astChn[1];
    CVI_BOOL bAttachEn;
    CVI_U32 u32AttachVbPool;
    CVI_BOOL Enable;
    //wgtAttr
    CVI_S32 wgt_value_alpha[STITCH_MAX_SRC_NUM -1];
    CVI_S32 wgt_value_beta[STITCH_MAX_SRC_NUM -1];
    CVI_CHAR wgt_alpha_name[STITCH_MAX_SRC_NUM -1][128];
    CVI_CHAR wgt_beta_name[STITCH_MAX_SRC_NUM -1][128];
    CVI_U64 u64PhyAddr_alpha[STITCH_MAX_SRC_NUM -1];
    CVI_U64 u64PhyAddr_beta[STITCH_MAX_SRC_NUM -1];
    CVI_VOID *VirAddr_alpha[STITCH_MAX_SRC_NUM -1];
    CVI_VOID *VirAddr_beta[STITCH_MAX_SRC_NUM -1];
} APP_PARAM_STITCH_CFG_S;

APP_PARAM_STITCH_CFG_S *app_ipcam_Stitch_Param_Get(void);
CVI_S32 app_ipcam_Stitch_Init(void);
CVI_S32 app_ipcam_Stitch_UnInit(void);
CVI_S32 cfg_wgt_image(SIZE_S size, enum stitch_wgt_mode wgtmode, char *name, CVI_U64 *u64PhyAddr, CVI_VOID **pVirAddr, CVI_S32 value);

#ifdef __cplusplus
}
#endif

#endif