#include <stdio.h>
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include <pthread.h>
#include <math.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "app_ipcam_ai.h"
#include "app_ipcam_osd.h"


/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
SMT_MUTEXAUTOLOCK_INIT(g_CountMutex);
static pthread_mutex_t g_CountStatusMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_CountNumMutex = PTHREAD_MUTEX_INITIALIZER;
#define PERSON_BUFFER_SIZE 10
#define MODEL_NAME_OD "yolov8"
/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static APP_PARAM_AI_CONSUMER_COUNTING_CFG_S g_stCountCfg;

static APP_PARAM_AI_CONSUMER_COUNTING_CFG_S *g_pstCountCfg = &g_stCountCfg;

static CVI_U32 g_CountProc;
static CVI_U32 g_CountFps;
static volatile bool g_bCountRunning = CVI_FALSE;
static volatile bool g_bCountPause = CVI_FALSE;
static pthread_t g_CountThreadHandle = 0;
static cvitdl_handle_t g_CountAiHandle = NULL;
static cvitdl_service_handle_t g_CountServicHandle = NULL;
static cvitdl_app_handle_t g_CountAppHandle = NULL;
static cvtdl_object_t g_stObjDrawHead;
static cvtdl_object_t g_stObjDrawPed;
static CVI_S32 g_CurNum, g_EntryNum, g_MissNum;
/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

APP_PARAM_AI_CONSUMER_COUNTING_CFG_S *app_ipcam_Ai_Consumer_Counting_Param_Get(void)
{
    return g_pstCountCfg;
}

CVI_VOID app_ipcam_Ai_Consumer_Counting_ProcStatus_Set(CVI_BOOL flag)
{
    g_bCountRunning = flag;
}

CVI_BOOL app_ipcam_Ai_Consumer_Counting_ProcStatus_Get(void)
{
    return g_bCountRunning;
}

CVI_VOID app_ipcam_Ai_Consumer_Counting_Pause_Set(CVI_BOOL flag)
{
    pthread_mutex_lock(&g_CountStatusMutex);
    g_bCountPause = flag;
    pthread_mutex_unlock(&g_CountStatusMutex);
}

CVI_BOOL app_ipcam_Ai_Consumer_Counting_Pause_Get(void)
{
    return g_bCountPause;
}

CVI_U32 app_ipcam_Ai_Consumer_Counting_ProcFps_Get(void)
{
    return g_CountFps;
}

CVI_VOID app_ipcam_Ai_Consumer_Counting_People_Num_Get(CVI_S32 *cur_num, CVI_S32 *entry_num, CVI_S32 *miss_num)
{
    pthread_mutex_lock(&g_CountNumMutex);
    *cur_num = g_CurNum;
    *entry_num = g_EntryNum;
    *miss_num = g_MissNum;
    pthread_mutex_unlock(&g_CountNumMutex);
}

static void app_ipcam_Ai_Param_dump(void)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "bEnable=%d Grp=%d Chn=%d GrpW=%d GrpH=%d\n",
        g_pstCountCfg->bEnable, g_pstCountCfg->VpssGrp, g_pstCountCfg->VpssChn, g_pstCountCfg->u32GrpWidth, g_pstCountCfg->u32GrpHeight);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "bSkip=%d threshold=%f\n",
         g_pstCountCfg->bVpssPreProcSkip, g_pstCountCfg->threshold);
    APP_PROF_LOG_PRINT(LEVEL_INFO, " model_path_count=%s\n",
        g_pstCountCfg->model_path_count);
}

static CVI_S32 app_ipcam_Ai_VpssChnAttr_Set(CVI_VOID)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    CVI_TDL_SUPPORTED_MODEL_E model_id = g_pstCountCfg->model_id; //CVI_TDL_SUPPORTED_MODEL_HEAD_PERSON_DETECTION
    VPSS_GRP VpssGrp = g_pstCountCfg->VpssGrp;
    VPSS_CHN VpssChn = g_pstCountCfg->VpssChn;

    APP_PARAM_VPSS_CFG_T *pstVpssCfg = app_ipcam_Vpss_Param_Get();
    VPSS_CHN_ATTR_S *pastVpssChnAttr = &pstVpssCfg->astVpssGrpCfg[VpssGrp].astVpssChnAttr[VpssChn];
    CVI_U32 u32Width = pstVpssCfg->astVpssGrpCfg[VpssGrp].stVpssGrpAttr.u32MaxW;
    CVI_U32 u32Height = pstVpssCfg->astVpssGrpCfg[VpssGrp].stVpssGrpAttr.u32MaxH;
    CVI_S32 s32SrcFrameRate = pastVpssChnAttr->stFrameRate.s32SrcFrameRate;
    CVI_S32 s32DstFrameRate = pastVpssChnAttr->stFrameRate.s32DstFrameRate;
    CVI_U32 u32Depth = pastVpssChnAttr->u32Depth;

    cvtdl_vpssconfig_t vpssConfig;
    memset(&vpssConfig, 0, sizeof(vpssConfig));

    s32Ret = CVI_TDL_GetVpssChnConfig(
                            g_CountAiHandle,
                            model_id,
                            u32Width,
                            u32Height,
                            0,
                            &vpssConfig);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_GetVpssChnConfig failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    vpssConfig.chn_attr.u32Depth = u32Depth;
    vpssConfig.chn_attr.stFrameRate.s32SrcFrameRate = s32SrcFrameRate;
    vpssConfig.chn_attr.stFrameRate.s32DstFrameRate = s32DstFrameRate;

    s32Ret = CVI_VPSS_SetChnScaleCoefLevel(VpssGrp, VpssChn, vpssConfig.chn_coeff);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_SetChnScaleCoefLevel failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "vpssConfig.chn_attr u32Width=%d u32Height=%d SrcFR=%d DstFR=%d\n", 
                    vpssConfig.chn_attr.u32Width, vpssConfig.chn_attr.u32Height, s32SrcFrameRate, s32DstFrameRate);
    APP_PROF_LOG_PRINT(LEVEL_INFO, " g_pstCountCfg->model_id=%d stNormalize.bEnable=%d stNormalize.factor[0]=%f stNormalize.factor[1]=%f stNormalize.mean[0]=%f\n", 
                 g_pstCountCfg->model_id,vpssConfig.chn_attr.stNormalize.bEnable,vpssConfig.chn_attr.stNormalize.factor[0], vpssConfig.chn_attr.stNormalize.factor[1],vpssConfig.chn_attr.stNormalize.mean[0]);

    s32Ret = CVI_VPSS_SetChnAttr(VpssGrp, VpssChn, &vpssConfig.chn_attr);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_SetChnAttr failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_VPSS_EnableChn(VpssGrp, VpssChn);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_EnableChn failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_TDL_SetVpssTimeout(g_CountAiHandle, 2000);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "set vpss timeout failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    return CVI_SUCCESS;
}

static CVI_S32 app_ipcam_OSDC_Consumer_Counting_Line_Init(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PARAM_OSDC_CFG_S *pstOsdcCfg = app_ipcam_Osdc_Param_Get();
     _NULL_POINTER_CHECK_(pstOsdcCfg, -1);
    // get main-streaming VPSS Grp0Chn0 size 
    APP_VPSS_GRP_CFG_T *pstVpssCfg = &app_ipcam_Vpss_Param_Get()->astVpssGrpCfg[0];
    _NULL_POINTER_CHECK_(pstVpssCfg, -1);

    float ScaleX = 0;
    float ScaleY = 0;
    ScaleX = (float)(pstVpssCfg->astVpssChnAttr[0].u32Width) / (float)(g_pstCountCfg->u32GrpWidth);
    ScaleY = (float)(pstVpssCfg->astVpssChnAttr[0].u32Height) / (float)(g_pstCountCfg->u32GrpHeight);

    // Draw consumer conting line on main stream
    int iOsdcIndex = 0;
    if (pstOsdcCfg->bShow[iOsdcIndex] && pstOsdcCfg->bShowCountRect[iOsdcIndex]) {
        pstOsdcCfg->osdcObj[iOsdcIndex][pstOsdcCfg->osdcObjNum[iOsdcIndex]].bShow = 1;
        pstOsdcCfg->osdcObj[iOsdcIndex][pstOsdcCfg->osdcObjNum[iOsdcIndex]].type  = RGN_CMPR_LINE;
        pstOsdcCfg->osdcObj[iOsdcIndex][pstOsdcCfg->osdcObjNum[iOsdcIndex]].color = 0x83ff;//0xff00ffff;
        pstOsdcCfg->osdcObj[iOsdcIndex][pstOsdcCfg->osdcObjNum[iOsdcIndex]].x1    = (CVI_S32)(g_pstCountCfg->A_x * ScaleX);
        pstOsdcCfg->osdcObj[iOsdcIndex][pstOsdcCfg->osdcObjNum[iOsdcIndex]].y1    = (CVI_S32)(g_pstCountCfg->A_y * ScaleY);
        pstOsdcCfg->osdcObj[iOsdcIndex][pstOsdcCfg->osdcObjNum[iOsdcIndex]].x2    = (CVI_S32)(g_pstCountCfg->B_x * ScaleX);
        pstOsdcCfg->osdcObj[iOsdcIndex][pstOsdcCfg->osdcObjNum[iOsdcIndex]].y2    = (CVI_S32)(g_pstCountCfg->B_y * ScaleY);
        pstOsdcCfg->osdcObj[iOsdcIndex][pstOsdcCfg->osdcObjNum[iOsdcIndex]].thickness = 4;      

        pstOsdcCfg->osdcObjNum[iOsdcIndex] = pstOsdcCfg->osdcObjNum[iOsdcIndex] + 1;
    }

    return s32Ret;
}

static CVI_S32 app_ipcam_OSDC_Consumer_Counting_Line_Exit(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PARAM_OSDC_CFG_S *pstOsdcCfg = app_ipcam_Osdc_Param_Get();
    _NULL_POINTER_CHECK_(pstOsdcCfg, -1);
    int iOsdcIndex = 0;
    if (pstOsdcCfg->bShow[iOsdcIndex]) {
        pstOsdcCfg->osdcObjNum[iOsdcIndex] = pstOsdcCfg->osdcObjNum[iOsdcIndex] - 1;
        pstOsdcCfg->osdcObj[iOsdcIndex][pstOsdcCfg->osdcObjNum[iOsdcIndex]].bShow = 0;
    }
    return s32Ret;
}

static CVI_S32 app_ipcam_Ai_Proc_Init(CVI_VOID)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI COUNT init ------------------> start \n");

    CVI_S32 s32Ret = CVI_SUCCESS;

    app_ipcam_Ai_Param_dump();

    if (g_CountAiHandle == NULL)
    {
        s32Ret = CVI_TDL_CreateHandle(&g_CountAiHandle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_CreateHandle failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_CreateHandle has created\n");
        return s32Ret;
    }

    if (g_CountServicHandle == NULL)
    {
        s32Ret = CVI_TDL_Service_CreateHandle(&g_CountServicHandle, g_CountAiHandle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_CreateHandle failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "g_CountServicHandle has created\n");
        return s32Ret;
    }

    if (g_CountAppHandle == NULL)
    {
        s32Ret = CVI_TDL_APP_CreateHandle(&g_CountAppHandle, g_CountAiHandle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_APP_CreateHandle failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "g_CountAppHandle has created\n");
        return s32Ret;
    }

    s32Ret = CVI_TDL_APP_PersonCapture_Init(g_CountAppHandle, PERSON_BUFFER_SIZE);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_APP_PersonCapture_Init failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_TDL_APP_PersonCapture_QuickSetUp(g_CountAppHandle, MODEL_NAME_OD, g_pstCountCfg->model_path_count,
      NULL);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_APP_PersonCapture_QuickSetUp failed with %#x! maybe reset model path\n", s32Ret);
        return s32Ret;
    }

    // Set line. A朝B方向左侧为上，A朝B方向右侧为下。Mode 0：从下到上为进入方向。    Mode 1：从上到下为进入方向
    s32Ret = CVI_TDL_APP_ConsumerCounting_Line(g_CountAppHandle, g_pstCountCfg->A_x, g_pstCountCfg->A_y, g_pstCountCfg->B_x, g_pstCountCfg->B_y, g_pstCountCfg->CountDirMode);  
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_APP_ConsumerCounting_Line failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    if (g_pstCountCfg->bVpssPreProcSkip) {
        s32Ret = app_ipcam_Ai_VpssChnAttr_Set();
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "VpssChnAttr Set failed with %#x!\n", s32Ret);
            return s32Ret;
        }
        CVI_TDL_SetSkipVpssPreprocess(g_CountAiHandle, g_pstCountCfg->model_id, g_pstCountCfg->bVpssPreProcSkip);
    }
    else
    {
        //Attach Vb POOL For Consumer Countiung Function
        s32Ret = CVI_TDL_SetVBPool(g_CountAiHandle, 0, g_pstCountCfg->attach_pool);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Consumer Counting Set vpss vbpool failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }

    s32Ret = CVI_TDL_SetVpssTimeout(g_CountAiHandle, 2000);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "set vpss timeout failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_TDL_SetModelThreshold(g_CountAiHandle, g_pstCountCfg->model_id, g_pstCountCfg->threshold);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_SetModelThreshold failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    person_capture_config_t app_cfg;
    s32Ret = CVI_TDL_APP_PersonCapture_GetDefaultConfig(&app_cfg);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_APP_PersonCapture_GetDefaultConfig failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_TDL_APP_PersonCapture_SetConfig(g_CountAppHandle, &app_cfg);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_APP_PersonCapture_SetConfig failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    //Draw consumer counting line on main stream
    s32Ret = app_ipcam_OSDC_Consumer_Counting_Line_Init();
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_OSDC_Consumer_Counting_Line_Init failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI CONSUMER COUNTING init ------------------> done \n");

    return CVI_SUCCESS;
}

static CVI_VOID *Thread_Consumer_Counting_PROC(CVI_VOID *arg)
{
    CVI_U32 count_frame = 0;
    CVI_S32 iTime_start, iTime_proc,iTime_fps;
    float iTime_gop;
    iTime_start = GetCurTimeInMsec();
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI COUNT start running!\n");

    prctl(PR_SET_NAME, "Thread_COUNT_PROC");

    VPSS_GRP VpssGrp = g_pstCountCfg->VpssGrp;
    VPSS_CHN VpssChn = g_pstCountCfg->VpssChn;

    VIDEO_FRAME_INFO_S stcountFrame = {0};

    while (app_ipcam_Ai_Consumer_Counting_ProcStatus_Get()) {
        pthread_mutex_lock(&g_CountStatusMutex);
        s32Ret = app_ipcam_Ai_Consumer_Counting_Pause_Get();
        
        if (s32Ret) {
            pthread_mutex_unlock(&g_CountStatusMutex);
            usleep(1000*1000);
            continue;
        }
        s32Ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stcountFrame, 3000);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) get frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
            pthread_mutex_unlock(&g_CountStatusMutex);
            usleep(100*1000);
            continue;
        }

        pthread_mutex_unlock(&g_CountStatusMutex);
        iTime_proc = GetCurTimeInMsec();
        cvtdl_object_t *pObjMetaHead = NULL;
        cvtdl_object_t *pObjMetaPed = NULL;

        s32Ret = CVI_TDL_APP_ConsumerCounting_Run(g_CountAppHandle, &stcountFrame);
        if (s32Ret != CVI_TDL_SUCCESS)
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_TDL_APP_ConsumerCounting_Run failed!, ret=%x\n", s32Ret);

        pObjMetaHead = &g_CountAppHandle->person_cpt_info->last_head;
        pObjMetaPed = &g_CountAppHandle->person_cpt_info->last_ped;

        g_CountProc = GetCurTimeInMsec() - iTime_proc;
        APP_PROF_LOG_PRINT(LEVEL_TRACE, "COUNT process takes %d\n", g_CountProc);
        s32Ret = CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stcountFrame);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) release frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
        }
        
        // test_proc_time();
        count_frame ++;
        iTime_fps = GetCurTimeInMsec();
        iTime_gop = (float)(iTime_fps - iTime_start)/1000;
        if(iTime_gop >= 1)
        {
            g_CountFps = count_frame/iTime_gop;
            count_frame = 0;
            iTime_start = iTime_fps;
        }

        //Get g_CurNum, g_EntryNum, g_MissNum
        {
            pthread_mutex_lock(&g_CountNumMutex);
            g_CurNum = pObjMetaHead->size;
            g_EntryNum = pObjMetaHead->entry_num;
            g_MissNum = pObjMetaHead->miss_num;
            pthread_mutex_unlock(&g_CountNumMutex);
        }

        if (pObjMetaHead->size == 0 && pObjMetaPed->size == 0) {
            continue;
        }
        SMT_MutexAutoLock(g_CountMutex, lock);
        if (g_stObjDrawHead.info != NULL) {
            CVI_TDL_Free(&g_stObjDrawHead);
        }
        memset(&g_stObjDrawHead, 0, sizeof(cvtdl_object_t));
        memcpy(&g_stObjDrawHead, pObjMetaHead, sizeof(cvtdl_object_t));
        g_stObjDrawHead.info = (cvtdl_object_info_t *)malloc(pObjMetaHead->size * sizeof(cvtdl_object_info_t));
        memset(g_stObjDrawHead.info, 0, sizeof(cvtdl_object_info_t) * g_stObjDrawHead.size);
        for (uint32_t i = 0; i < pObjMetaHead->size; i++) {
            CVI_TDL_CopyObjectInfo(&pObjMetaHead->info[i], &g_stObjDrawHead.info[i]);
        }

        if (g_stObjDrawPed.info != NULL) {
            CVI_TDL_Free(&g_stObjDrawPed);
        }
        memset(&g_stObjDrawPed, 0, sizeof(cvtdl_object_t));
        memcpy(&g_stObjDrawPed, pObjMetaPed, sizeof(cvtdl_object_t));
        g_stObjDrawPed.info = (cvtdl_object_info_t *)malloc(pObjMetaPed->size * sizeof(cvtdl_object_info_t));
        memset(g_stObjDrawPed.info, 0, sizeof(cvtdl_object_info_t) * g_stObjDrawPed.size);
        for (uint32_t i = 0; i < pObjMetaPed->size; i++) {
            CVI_TDL_CopyObjectInfo(&pObjMetaPed->info[i], &g_stObjDrawPed.info[i]);
        }

    }

    pthread_exit(NULL);

    return NULL;
}

int app_ipcam_Ai_Consumer_Counting_ObjDrawInfo_Get(cvtdl_object_t *pstAiObjHead, cvtdl_object_t *pstAiObjPed)
{
    _NULL_POINTER_CHECK_(pstAiObjHead, -1);
    _NULL_POINTER_CHECK_(pstAiObjPed, -1);

    SMT_MutexAutoLock(g_CountMutex, lock);

    if (g_stObjDrawHead.size == 0 && g_stObjDrawPed.size == 0) {
        return CVI_SUCCESS;
    } else {
        memcpy(pstAiObjHead, &g_stObjDrawHead, sizeof(g_stObjDrawHead));
        memcpy(pstAiObjPed, &g_stObjDrawPed, sizeof(g_stObjDrawPed));
        pstAiObjHead->info = (cvtdl_object_info_t *)malloc(g_stObjDrawHead.size * sizeof(cvtdl_object_info_t));
        pstAiObjPed->info = (cvtdl_object_info_t *)malloc(g_stObjDrawPed.size * sizeof(cvtdl_object_info_t));
        _NULL_POINTER_CHECK_(pstAiObjHead->info, -1);
        _NULL_POINTER_CHECK_(pstAiObjPed->info, -1);
        memset(pstAiObjHead->info, 0, sizeof(cvtdl_object_info_t) * g_stObjDrawHead.size);
        memset(pstAiObjPed->info, 0, sizeof(cvtdl_object_info_t) * g_stObjDrawPed.size);
        for (CVI_U32 i = 0; i < g_stObjDrawHead.size; i++) {
            CVI_TDL_CopyObjectInfo(&g_stObjDrawHead.info[i], &pstAiObjHead->info[i]);
        }
        for (CVI_U32 i = 0; i < g_stObjDrawPed.size; i++) {
            CVI_TDL_CopyObjectInfo(&g_stObjDrawPed.info[i], &pstAiObjPed->info[i]);
        }
        CVI_TDL_Free(&g_stObjDrawHead);
        CVI_TDL_Free(&g_stObjDrawPed);
    }

    return CVI_SUCCESS;
}

int app_ipcam_Ai_Consumer_Counting_Stop(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstCountCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI COUNT not enable\n");
        return CVI_SUCCESS;
    }

    if (!app_ipcam_Ai_Consumer_Counting_ProcStatus_Get())
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI COUNT has not running!\n");
        return s32Ret;
    }

    app_ipcam_Ai_Consumer_Counting_ProcStatus_Set(CVI_FALSE);

    CVI_S32 iTime = GetCurTimeInMsec();

    if (g_CountThreadHandle)
    {
        pthread_join(g_CountThreadHandle, NULL);
        g_CountThreadHandle = 0;
    }

    s32Ret = app_ipcam_OSDC_Consumer_Counting_Line_Exit();
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_OSDC_Consumer_Counting_Line_Exit failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_TDL_Service_DestroyHandle(g_CountServicHandle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_Service_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        g_CountServicHandle = NULL;
    }

    s32Ret = CVI_TDL_DestroyHandle(g_CountAiHandle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        g_CountAiHandle = NULL;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI COUNT Thread exit takes %u ms\n", (GetCurTimeInMsec() - iTime));

    return CVI_SUCCESS;
}

int app_ipcam_Ai_Consumer_Counting_Start(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstCountCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI COUNT not enable\n");
        return CVI_SUCCESS;
    }

    if (g_bCountRunning)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI COUNT has started\n");
        return CVI_SUCCESS;
    }

    s32Ret = app_ipcam_Ai_Proc_Init();
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Consumer counting app_ipcam_Ai_Proc_Init failed!\n");
        return s32Ret;
    }

    app_ipcam_Ai_Consumer_Counting_ProcStatus_Set(CVI_TRUE);

    s32Ret = pthread_create(&g_CountThreadHandle, NULL, Thread_Consumer_Counting_PROC, NULL);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "AI pthread_create failed!\n");
        return s32Ret;
    }
    
    return CVI_SUCCESS;
}
