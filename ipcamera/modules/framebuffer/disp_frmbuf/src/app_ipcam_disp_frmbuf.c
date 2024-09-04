#include "app_ipcam_comm.h"
#include "app_ipcam_disp_frmbuf.h"

// #define SHOW_TIME
#define VDEC_TYPE_JPEG "/tmp/vdecsoft1"

APP_PARAM_DISP_FRMBUF_CTX_S g_stDispFrmBufCtx, *g_pstDispFrmBufCtx = &g_stDispFrmBufCtx;

APP_PARAM_DISP_FRMBUF_CTX_S *app_ipcam_Disp_FrmBuf_Param_Get() {
    return g_pstDispFrmBufCtx;
}

static CVI_S32 app_ipcam_Disp_FrmBuf_DrawChar(CVI_VOID *arg, CVI_BOOL bgColor)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_CHAR tmpStr[32];
    APP_PARAM_FRMBUF_DRAW_S stDrawCfg = {0};
    APP_PARAM_FRMBUF_CTX_S *pstFrmBufCtx = (APP_PARAM_FRMBUF_CTX_S *)arg;

    memset(tmpStr, 0, sizeof(tmpStr));
    memset(&stDrawCfg, 0, sizeof(APP_PARAM_FRMBUF_DRAW_S));

    time_t currentTime = time(NULL);
    struct tm *localTime = localtime(&currentTime);
    CVI_S32 mon    = localTime->tm_mon;
    CVI_S32 mday   = localTime->tm_mday;
    CVI_S32 week   = localTime->tm_wday;
    CVI_S32 hour   = localTime->tm_hour;
    CVI_S32 minute = localTime->tm_min;
    // APP_PROF_LOG_PRINT(LEVEL_INFO, "=========== >> hour=%d, minute=%d.\n"
    //    , hour, minute);

    // draw time
    snprintf(tmpStr, sizeof(tmpStr), "%02d:%02d", hour, minute);
    stDrawCfg.start_x = pstFrmBufCtx->time_start_x;
    stDrawCfg.start_y = pstFrmBufCtx->time_start_y;
    stDrawCfg.charData = tmpStr;
    stDrawCfg.charLen  = strlen(tmpStr);
    stDrawCfg.pstArg = arg;
    stDrawCfg.weekFlag = 0;
    stDrawCfg.enFontType = FONT_SIZE_48x64;
    stDrawCfg.bgColor = bgColor;
    s32Ret = app_ipcam_FrmBuf_DrawChar(&stDrawCfg);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "draw char failed. s32Ret:%d.\n", s32Ret);
        return s32Ret;
    }

    // draw date
    snprintf(tmpStr, sizeof(tmpStr), "%02d", mon+1);
    stDrawCfg.start_x = pstFrmBufCtx->date_start_x;
    stDrawCfg.start_y = pstFrmBufCtx->date_start_y;
    stDrawCfg.charData = tmpStr;
    stDrawCfg.charLen  = strlen(tmpStr);
    stDrawCfg.pstArg = arg;
    stDrawCfg.weekFlag = 0;
    stDrawCfg.enFontType = FONT_SIZE_16x24;
    stDrawCfg.bgColor = bgColor;
    s32Ret = app_ipcam_FrmBuf_DrawChar(&stDrawCfg);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "draw char failed. s32Ret:%d.\n", s32Ret);
        return s32Ret;
    }

    /* m is the abbreviation of month */
    strcpy(tmpStr, "m");
    stDrawCfg.start_x = pstFrmBufCtx->date_start_x + 16*2;
    stDrawCfg.start_y = pstFrmBufCtx->date_start_y;
    stDrawCfg.charData = tmpStr;
    stDrawCfg.charLen  = strlen(tmpStr);
    stDrawCfg.pstArg = arg;
    stDrawCfg.weekFlag = 0;
    stDrawCfg.enFontType = FONT_SIZE_24x24;
    stDrawCfg.bgColor = bgColor;
    s32Ret = app_ipcam_FrmBuf_DrawChar(&stDrawCfg);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "draw char failed. s32Ret:%d.\n", s32Ret);
        return s32Ret;
    }

    snprintf(tmpStr, sizeof(tmpStr), "%02d", mday);
    stDrawCfg.start_x = pstFrmBufCtx->date_start_x + 16*2 + 24;
    stDrawCfg.start_y = pstFrmBufCtx->date_start_y;
    stDrawCfg.charData = tmpStr;
    stDrawCfg.charLen  = strlen(tmpStr);
    stDrawCfg.pstArg = arg;
    stDrawCfg.weekFlag = 0;
    stDrawCfg.enFontType = FONT_SIZE_16x24;
    stDrawCfg.bgColor = bgColor;
    s32Ret = app_ipcam_FrmBuf_DrawChar(&stDrawCfg);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "draw char failed. s32Ret:%d.\n", s32Ret);
        return s32Ret;
    }

    /*d is the abbreviation of day 
    * w is the first letter of week
    * k is the last  letter of week */
    strcpy(tmpStr, "d wk");
    stDrawCfg.start_x = pstFrmBufCtx->date_start_x + 16*2 + 24 + 16*2;
    stDrawCfg.start_y = pstFrmBufCtx->date_start_y;
    stDrawCfg.charData = tmpStr;
    stDrawCfg.charLen  = strlen(tmpStr);
    stDrawCfg.pstArg = arg;
    stDrawCfg.weekFlag = 0;
    stDrawCfg.enFontType = FONT_SIZE_24x24;
    stDrawCfg.bgColor = bgColor;
    s32Ret = app_ipcam_FrmBuf_DrawChar(&stDrawCfg);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "draw char failed. s32Ret:%d.\n", s32Ret);
        return s32Ret;
    }

    if (week == 0)
        week = 7;
    snprintf(tmpStr, sizeof(tmpStr), "%d", (CVI_U8)(week));
    stDrawCfg.start_x = pstFrmBufCtx->date_start_x + 16*2 + 24 + 16*2 + 24*4;
    stDrawCfg.start_y = pstFrmBufCtx->date_start_y;
    stDrawCfg.charData = tmpStr;
    stDrawCfg.charLen  = strlen(tmpStr);
    stDrawCfg.pstArg = arg;
    stDrawCfg.weekFlag = 1;
    stDrawCfg.enFontType = FONT_SIZE_24x24;
    stDrawCfg.bgColor = bgColor;
    s32Ret = app_ipcam_FrmBuf_DrawChar(&stDrawCfg);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "draw char failed. s32Ret:%d.\n", s32Ret);
        return s32Ret;
    }

    return CVI_SUCCESS;
}

CVI_VOID *dispFrmBufThr(CVI_VOID *arg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_VOID *tempPtr = NULL;
    CVI_BOOL bVdecSoftEnable = CVI_FALSE;
    APP_PARAM_DISP_FRMBUF_CTX_S *pstDispFrmBufCtx = NULL;
    APP_PARAM_FRMBUF_CTX_S     *pstFrmBufCtx      = NULL;
    CVI_U8 *pFrmBuf = NULL;
    CVI_S32 frmLineSize = 0;
    CVI_S32 FrmHeight = 0;
#ifdef SHOW_TIME
    CVI_U32 u32Time = GetCurTimeInMsec();
    CVI_U32 u32TimeVdecMax = 0;
#endif
    prctl(PR_SET_NAME, "VDEC_SOFT_FFMPEG", 0, 0, 0);
    pstDispFrmBufCtx = (APP_PARAM_DISP_FRMBUF_CTX_S *)arg;
    pstFrmBufCtx     = pstDispFrmBufCtx->pstFrmBufCtx;

    APP_PROF_LOG_PRINT(LEVEL_INFO, "dispFrmBufThr running...\n");
    while (pstDispFrmBufCtx->thrFlag) {
        usleep(1000);
        
#ifdef VDEC_SOFT
        // For the ili8341 spi screen, 
        // the test found that the delay of 35ms frame rate and cpu usage is more balanced
        usleep(35*1000);
        
        APP_PARAM_VDEC_SOFT_CTX_S *pstVdecSoftCtx = 
            (APP_PARAM_VDEC_SOFT_CTX_S *)pstDispFrmBufCtx->pVdecSoftCtx;
        CVI_S32 vdecChn = 0;
        if (access(VDEC_TYPE_JPEG, F_OK) == 0) {
            vdecChn = 1;
        } else {
            vdecChn = 0;
        }
        bVdecSoftEnable = pstVdecSoftCtx->astVdecSoftChnCfg[vdecChn].bEnable;
        pFrmBuf         = pstVdecSoftCtx->astVdecSoftChnCfg[vdecChn].pstResultFrame->data[0];
        frmLineSize     = pstVdecSoftCtx->astVdecSoftChnCfg[vdecChn].pstResultFrame->linesize[0];
        FrmHeight       = pstVdecSoftCtx->astVdecSoftChnCfg[vdecChn].pstResultFrame->height;
#endif
        // When the softsolution and overlay characters are enabled at the same time
        if ((bVdecSoftEnable == CVI_TRUE) 
            && (pstFrmBufCtx->bEnable == CVI_TRUE)) {
#ifdef SHOW_TIME
            u32Time = GetCurTimeInMsec();
#endif
#ifdef VDEC_SOFT
            s32Ret = app_ipcam_Vdec_Soft_Proc(&pstVdecSoftCtx->astVdecSoftChnCfg[vdecChn]);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "vdec soft process failed. s32Ret:%d. \n", s32Ret);
                continue;
            }
#endif
            // Overlays characters on video frames
            tempPtr = pstFrmBufCtx->fbp;
            pstFrmBufCtx->fbp = (CVI_VOID *)pFrmBuf;
            s32Ret = app_ipcam_Disp_FrmBuf_DrawChar((CVI_VOID *)pstFrmBufCtx, CVI_FALSE);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "draw char failed. s32Ret:%d. \n", s32Ret);
                continue;
            }
            pstFrmBufCtx->fbp = tempPtr;
            // Copy the video frame with the superimposed characters to the frame buffer
            memcpy(pstFrmBufCtx->fbp, pFrmBuf, frmLineSize * FrmHeight);
#if 0
            // Here the ioctl has TE pins for the ftf spi screen
            // Trigger DMA copy
            s32Ret = ioctl(pstFrmBufCtx->fbfd, FBIOPAN_DISPLAY, &pstFrmBufCtx->vinfo);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "FBIOPAN_DISPLAY failed. s32Ret:%d. \n", s32Ret);
                continue;
            }
#endif
#ifdef SHOW_TIME
            if ((GetCurTimeInMsec() - u32Time) > u32TimeVdecMax) {
                u32TimeVdecMax = GetCurTimeInMsec() - u32Time;
            }
            APP_PROF_LOG_PRINT(LEVEL_INFO, "vdec soft and send fb take %u ms, max %u ms.\n\n"
                , GetCurTimeInMsec() - u32Time, u32TimeVdecMax);
#endif
        } else if ((bVdecSoftEnable == CVI_FALSE)
            && (pstFrmBufCtx->bEnable == CVI_TRUE)) {
            // 直接在frame buffer上叠加字符
            s32Ret = app_ipcam_Disp_FrmBuf_DrawChar((CVI_VOID *)pstFrmBufCtx, CVI_TRUE);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "draw char failed. s32Ret:%d. \n", s32Ret);
                continue;
            }
        } else {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Input param is incorrect. "
                "bVdecSoftEnable:%d, frmBufEnable:%d. \n"
                , bVdecSoftEnable
                , pstFrmBufCtx->bEnable);
        }
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "dispFrmBufThr exit.\n");

    return (CVI_VOID *)(CVI_SUCCESS);
}

CVI_S32 app_ipcam_Disp_FrmBuf_Start(CVI_VOID)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    struct sched_param param = {0};
	pthread_attr_t attr = {0};
    APP_PARAM_DISP_FRMBUF_CTX_S *pstDispFrmBufCtx = NULL;

    memset(&param, 0, sizeof(struct sched_param));
    memset(&attr, 0, sizeof(pthread_attr_t));

	pstDispFrmBufCtx = app_ipcam_Disp_FrmBuf_Param_Get();
    pstDispFrmBufCtx->pstFrmBufCtx = app_ipcam_FrmBuf_Param_Get();
#ifdef VDEC_SOFT
    pstDispFrmBufCtx->pVdecSoftCtx = (CVI_VOID *)app_ipcam_Vdec_Soft_Param_Get();
#endif
    pstDispFrmBufCtx->thrFlag = CVI_TRUE;
    
	param.sched_priority = 80;
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_RR);
	pthread_attr_setschedparam(&attr, &param);
	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    s32Ret = pthread_create(&pstDispFrmBufCtx->dispFrmBufThr, &attr, dispFrmBufThr
            , (CVI_VOID *)pstDispFrmBufCtx);
    if (s32Ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Create dispFrmBufThr failed. s32Ret:%d.\n", s32Ret);
        return CVI_FAILURE;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "app_ipcam_Disp_FrmBuf_Start done.\n");

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Disp_FrmBuf_Stop(CVI_VOID)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PARAM_DISP_FRMBUF_CTX_S *pstDispFrmBufCtx = NULL;

    pstDispFrmBufCtx = app_ipcam_Disp_FrmBuf_Param_Get();
    pstDispFrmBufCtx->thrFlag = CVI_FALSE;

    s32Ret = pthread_join(pstDispFrmBufCtx->dispFrmBufThr, NULL);
    if (s32Ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"pthread_join dispFrmBufThr failed. errCode:%d.\n", s32Ret);
        return s32Ret;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "app_ipcam_Disp_FrmBuf_Stop done.\n");

    return CVI_SUCCESS;
}

