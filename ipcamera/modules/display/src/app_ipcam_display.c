#include <stdbool.h>
#include <stdio.h>

#include "linux/cvi_comm_vo.h"
#include "linux/cvi_comm_video.h"

#include "app_ipcam_comm.h"
#include "app_ipcam_mipi_tx.h"
#include "app_ipcam_display.h"

APP_PARAM_MULTI_DISPLAY_CFG_T g_stDisplayCfg = {
    .vo_cfg[0]={
        .enPanelType = PANEL_DSI_HX8394_EVB,
        .stVoCfg = {
            .s32VoDev = 0,
            .stVoPubAttr = {
                .u32BgColor = 0x00000000,
                .enIntfType = VO_INTF_MIPI,
                .enIntfSync = VO_OUTPUT_720x1280_60,
                .stSyncInfo = {CVI_FALSE, CVI_FALSE, 0, 0, 0, 0, 0, 0, 0, 0, 0, CVI_FALSE, CVI_FALSE, CVI_FALSE},
                #if !defined(__CV186AH__)
                .sti80Cfg = {{0, 1, 2, 3}, VO_I80_FORMAT_RGB565, 200},
                #endif
            },
            #if defined(__CV186AH__)
            .sti80Cfg = {{0, 1, 2, 3}, VO_I80_FORMAT_RGB565, 200},
            #endif
            .stLayerAttr = {
                .stDispRect = {.s32X = 0, .s32Y = 0, .u32Width = 720, .u32Height = 1280},
                .stImageSize = {.u32Width = 720, .u32Height = 1280},
                .u32DispFrmRt = 60,
                .enPixFormat = PIXEL_FORMAT_NV21
            },
            .enVoMode = VO_MODE_1MUX,
            .enRotation = ROTATION_90,
            .u32DisBufLen = 3,
            .stSrcChn = {.enModId = CVI_ID_VPSS, .s32DevId = 0, .s32ChnId = 2},
            .stDstChn = {.enModId = CVI_ID_VO, .s32DevId = 1, .s32ChnId = 0}
        }
    },
    .vo_num=1
};

APP_PARAM_MULTI_DISPLAY_CFG_T *g_pstDisplayCfg = &g_stDisplayCfg;

APP_PARAM_MULTI_DISPLAY_CFG_T *app_ipcam_Display_Param_Get(void)
{
    return g_pstDisplayCfg;
}

CVI_S32 s32MipiTxFd[VO_MAX_DEV_NUM];

CVI_S32 app_ipcam_Display_Init(void)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "======================================== Display Init START ========================================\n");
    PANEL_DESC_T stPanelDesc = {
        .pchPanelName = NULL,
        .pstDevCfg = NULL,
        .pstHsTimingCfg = NULL,
        .pstDsiInitCmds = NULL,
        .s32DsiInitCmdsSize = 0
    };
    CVI_U32 vonum = g_pstDisplayCfg->vo_num;
    while(vonum > 0)
    {
        vonum--;
        s32MipiTxFd[vonum] = -1;
        app_ipcam_Panel_PanelDesc_Get(&g_pstDisplayCfg->vo_cfg[vonum].enPanelType, &stPanelDesc);
        app_ipcam_MipiTx_Enable(
            &g_pstDisplayCfg->vo_cfg[vonum].stVoCfg.s32VoDev,
            stPanelDesc.pchPanelName,
            stPanelDesc.pstDevCfg,
            stPanelDesc.pstHsTimingCfg,
            stPanelDesc.pstDsiInitCmds,
            &stPanelDesc.s32DsiInitCmdsSize,
            &s32MipiTxFd[vonum]
        );
        app_ipcam_Panel_FillIntfAttr(&g_pstDisplayCfg->vo_cfg[vonum].stVoCfg);
        app_ipcam_Vo_Start(&g_pstDisplayCfg->vo_cfg[vonum].stVoCfg);
    }
    APP_PROF_LOG_PRINT(LEVEL_INFO, "========================================= Display Init END =========================================\n");

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Display_Exit(void)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "======================================== Display Exit START ========================================\n");
    for (unsigned i = 0; i < g_pstDisplayCfg->vo_num; i++) {
        APP_CHK_RET(app_ipcam_Vo_Stop(&g_pstDisplayCfg->vo_cfg[i].stVoCfg), "app_ipcam_Vo_Stop");
        APP_CHK_RET(app_ipcam_MipiTx_Disable(&s32MipiTxFd[i]), "app_ipcam_MipiTx_Disable");
    }
    APP_PROF_LOG_PRINT(LEVEL_INFO, "========================================= Display Exit END =========================================\n");

    return CVI_SUCCESS;
}