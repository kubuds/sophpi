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
SMT_MUTEXAUTOLOCK_INIT(g_HDMutex);
static pthread_mutex_t g_HdStatusMutex = PTHREAD_MUTEX_INITIALIZER;
/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static APP_PARAM_AI_HD_CFG_S g_stHdCfg;

static APP_PARAM_AI_HD_CFG_S *g_pstHdCfg = &g_stHdCfg;

static CVI_U32 g_HDProc;
static volatile bool g_bHDRunning = CVI_FALSE;
static volatile bool g_bHDPause = CVI_FALSE;
static pthread_t g_HDThreadHandle;
static cvitdl_handle_t g_HDAiHandle = NULL;
static cvitdl_service_handle_t g_HDAiServiceHandle = NULL;
static cvtdl_object_t g_stHDObjDraw;
static pfpInferenceFunc g_pfpHDInference;
static const char *cls_name[] = {"fist", "five",  "four",   "none", "ok",
                                 "one",  "three", "three2", "two"};
/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

APP_PARAM_AI_HD_CFG_S *app_ipcam_Ai_HD_Param_Get(void)
{
    return g_pstHdCfg;
}

CVI_VOID app_ipcam_Ai_HD_ProcStatus_Set(CVI_BOOL flag)
{
    g_bHDRunning = flag;
}

CVI_BOOL app_ipcam_Ai_HD_ProcStatus_Get(void)
{
    return g_bHDRunning;
}

CVI_VOID app_ipcam_Ai_HD_Pause_Set(CVI_BOOL flag)
{
    pthread_mutex_lock(&g_HdStatusMutex);
    g_bHDPause = flag;
    pthread_mutex_unlock(&g_HdStatusMutex);
}

CVI_BOOL app_ipcam_Ai_HD_Pause_Get(void)
{
    return g_bHDPause;
}

static void app_ipcam_Ai_Param_dump(void)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "bEnable=%d Grp=%d Chn=%d GrpW=%d GrpH=%d\n", 
        g_pstHdCfg->bEnable, g_pstHdCfg->VpssGrp, g_pstHdCfg->VpssChn, g_pstHdCfg->u32GrpWidth, g_pstHdCfg->u32GrpHeight);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "bSkip=%d threshold=%f\n",
         g_pstHdCfg->bVpssPreProcSkip, g_pstHdCfg->threshold);
    APP_PROF_LOG_PRINT(LEVEL_INFO, " model_id_hd=%d model_path_hd=%s model_id_hr=%d model_path_hr=%s model_id_hdkey=%d model_path_hdkey=%s\n",
                    g_pstHdCfg->model_id_hd, g_pstHdCfg->model_path_hd,\
                    g_pstHdCfg->model_id_hr, g_pstHdCfg->model_path_hr,\
                    g_pstHdCfg->model_id_hdkey, g_pstHdCfg->model_path_hdkey);
}

static CVI_S32 app_ipcam_Ai_InferenceFunc_Get(CVI_TDL_SUPPORTED_MODEL_E model_id)
{
    switch (model_id)
    {
        case CVI_TDL_SUPPORTED_MODEL_HAND_DETECTION:
            g_pfpHDInference = CVI_TDL_Detection;
        break;

        default:
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "model id (%d) invalid!\n", model_id);
            return CVI_FAILURE;
    }

    return CVI_SUCCESS;
}

static CVI_S32 app_ipcam_Ai_HD_Proc_Init(CVI_VOID)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI HD init ------------------> start \n");

    CVI_S32 s32Ret = CVI_SUCCESS;

    app_ipcam_Ai_Param_dump();

    if (g_HDAiHandle == NULL)
    {
        s32Ret = CVI_TDL_CreateHandle(&g_HDAiHandle);
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

    if (g_HDAiServiceHandle == NULL)
    {
        s32Ret = CVI_TDL_Service_CreateHandle(&g_HDAiServiceHandle, g_HDAiHandle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_CreateHandle failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_Service_CreateHandle has created\n");
        return s32Ret;
    }

    s32Ret = app_ipcam_Ai_InferenceFunc_Get(g_pstHdCfg->model_id_hd);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "unsupported model id: %d \n", g_pstHdCfg->model_id_hd);
        return s32Ret;
    }

    s32Ret = CVI_TDL_OpenModel(g_HDAiHandle, g_pstHdCfg->model_id_hd, g_pstHdCfg->model_path_hd);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_SetModelPath failed with %#x! maybe reset model path\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_TDL_OpenModel(g_HDAiHandle, g_pstHdCfg->model_id_hdkey, g_pstHdCfg->model_path_hdkey);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_SetModelPath failed with %#x! maybe reset model path\n", s32Ret);
        return s32Ret;
    }

     s32Ret = CVI_TDL_OpenModel(g_HDAiHandle, g_pstHdCfg->model_id_hr, g_pstHdCfg->model_path_hr);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_SetModelPath failed with %#x! maybe reset model path\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_TDL_SetModelThreshold(g_HDAiHandle, g_pstHdCfg->model_id_hd, g_pstHdCfg->threshold);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_SetModelThreshold failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    //Hand detect don't support VpssPreProcSkip
    if (g_pstHdCfg->bVpssPreProcSkip) {
        #if 0
        // CVI_TDL_SetSkipVpssPreprocess(g_HDAiHandle, CVI_TDL_SUPPORTED_MODEL_HAND_DETECTION, g_pstHdCfg->bVpssPreProcSkip);
        // CVI_TDL_SetSkipVpssPreprocess(g_HDAiHandle, CVI_TDL_SUPPORTED_MODEL_HANDCLASSIFICATION, g_pstHdCfg->bVpssPreProcSkip);
        #endif
    }

    s32Ret = CVI_TDL_SetVBPool(g_HDAiHandle, 0, g_pstHdCfg->attach_pool);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_SetVBPool failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    CVI_TDL_SetVpssTimeout(g_HDAiHandle, 2000);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI HD init ------------------> done \n");

    return CVI_SUCCESS;
}

static CVI_VOID *Thread_HD_PROC(CVI_VOID *arg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI HD start running!\n");

    prctl(PR_SET_NAME, "Thread_HD_PROC");

    VPSS_GRP VpssGrp = g_pstHdCfg->VpssGrp;
    VPSS_CHN VpssChn = g_pstHdCfg->VpssChn;

    VIDEO_FRAME_INFO_S sthdFrame = {0};
    float buffer[42];
    CVI_S32 iTime_proc;

    while (app_ipcam_Ai_HD_ProcStatus_Get()) {
        pthread_mutex_lock(&g_HdStatusMutex);
        s32Ret = app_ipcam_Ai_HD_Pause_Get();
        
        if (s32Ret) {
            pthread_mutex_unlock(&g_HdStatusMutex);
            usleep(1000*1000);
            continue;
        }
        s32Ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &sthdFrame, 3000);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) get frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
            pthread_mutex_unlock(&g_HdStatusMutex);
            usleep(100*1000);
            continue;
        }

        pthread_mutex_unlock(&g_HdStatusMutex);
        iTime_proc = GetCurTimeInMsec();
        cvtdl_object_t obj_meta;
        cvtdl_handpose21_meta_ts stHandKptMeta;
        memset(&obj_meta, 0, sizeof(cvtdl_object_t));
        memset(&stHandKptMeta, 0, sizeof(cvtdl_handpose21_meta_ts));


        s32Ret = g_pfpHDInference(g_HDAiHandle, &sthdFrame, g_pstHdCfg->model_id_hd, &obj_meta);
        if (s32Ret != CVI_TDL_SUCCESS)
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_TDL_Hand_Detection failed!, ret=%x\n", s32Ret);

        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "HD obj: %d \n", obj_meta.size);

        stHandKptMeta.size = obj_meta.size;
        stHandKptMeta.width = obj_meta.width;
        stHandKptMeta.height = obj_meta.height;
        stHandKptMeta.info = (cvtdl_handpose21_meta_t *)malloc(sizeof(cvtdl_handpose21_meta_t) * (obj_meta.size));

        for (uint32_t i = 0; i < obj_meta.size; i++)
        {
            stHandKptMeta.info[i].bbox_x = obj_meta.info[i].bbox.x1;
            stHandKptMeta.info[i].bbox_y = obj_meta.info[i].bbox.y1;
            stHandKptMeta.info[i].bbox_w = obj_meta.info[i].bbox.x2 - obj_meta.info[i].bbox.x1;
            stHandKptMeta.info[i].bbox_h = obj_meta.info[i].bbox.y2 - obj_meta.info[i].bbox.y1;
        }

        s32Ret = CVI_TDL_HandKeypoint(g_HDAiHandle, &sthdFrame, &stHandKptMeta);
        if (s32Ret != CVI_TDL_SUCCESS)
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_TDL_HandKeypoint failed!, ret=%x\n", s32Ret);

        for (uint32_t i = 0; i < stHandKptMeta.size; i++) {
            for (uint32_t j = 0; j < 42; j++) {
                if (j % 2 == 0) {
                buffer[j] = stHandKptMeta.info[i].xn[j / 2];
                } else {
                buffer[j] = stHandKptMeta.info[i].yn[j / 2];
                }
            }
            VIDEO_FRAME_INFO_S Frame;
            Frame.stVFrame.pu8VirAddr[0] = (void*)buffer;  // Global buffer
            Frame.stVFrame.u32Height = 1;
            Frame.stVFrame.u32Width = 42 * sizeof(float);
            CVI_TDL_HandKeypointClassification(g_HDAiHandle, &Frame, &stHandKptMeta.info[i]);
        }

        g_HDProc = GetCurTimeInMsec() - iTime_proc;
        APP_PROF_LOG_PRINT(LEVEL_TRACE, "HD process takes %d\n", g_HDProc);
        s32Ret = CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &sthdFrame);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) release frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
        }

        if (obj_meta.size == 0) {
            continue;
        }
        SMT_MutexAutoLock(g_HDMutex, lock);
        if (g_stHDObjDraw.info != NULL) {
            CVI_TDL_Free(&g_stHDObjDraw);
        }
        memset(&g_stHDObjDraw, 0, sizeof(cvtdl_object_t));
        memcpy(&g_stHDObjDraw, &obj_meta, sizeof obj_meta);
        g_stHDObjDraw.info = (cvtdl_object_info_t *)malloc(obj_meta.size * sizeof(cvtdl_object_info_t));
        memset(g_stHDObjDraw.info, 0, sizeof(cvtdl_object_info_t) * g_stHDObjDraw.size);
        for (uint32_t i = 0; i < obj_meta.size; i++) {
            sprintf(obj_meta.info[i].name, "cls:%s", cls_name[stHandKptMeta.info[i].label]);
            APP_PROF_LOG_PRINT(LEVEL_INFO, "HD obj[%d] name: %s \n", i,obj_meta.info[i].name);
            CVI_TDL_CopyObjectInfo(&obj_meta.info[i], &g_stHDObjDraw.info[i]);
        }
		CVI_TDL_Free(&stHandKptMeta);
        CVI_TDL_Free(&obj_meta);
    }

    pthread_exit(NULL);

    return NULL;
}

int app_ipcam_Ai_HD_ObjDrawInfo_Get(cvtdl_object_t *pstAiObj)
{
    _NULL_POINTER_CHECK_(pstAiObj, -1);

    SMT_MutexAutoLock(g_HDMutex, lock);

    if (g_stHDObjDraw.size == 0) {
        return CVI_SUCCESS;
    } else {
        memcpy(pstAiObj, &g_stHDObjDraw, sizeof g_stHDObjDraw);
        pstAiObj->info = (cvtdl_object_info_t *)malloc(g_stHDObjDraw.size * sizeof(cvtdl_object_info_t));
        _NULL_POINTER_CHECK_(pstAiObj->info, -1);
        memset(pstAiObj->info, 0, sizeof(cvtdl_object_info_t) * g_stHDObjDraw.size);
        for (CVI_U32 i = 0; i < g_stHDObjDraw.size; i++) {
            CVI_TDL_CopyObjectInfo(&g_stHDObjDraw.info[i], &pstAiObj->info[i]);
        }
        CVI_TDL_Free(&g_stHDObjDraw);
    }

    return CVI_SUCCESS;
}

int app_ipcam_Ai_HD_Stop(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstHdCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI HD not enable\n");
        return CVI_SUCCESS;
    }

    if (!app_ipcam_Ai_HD_ProcStatus_Get())
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI HD has not running!\n");
        return s32Ret;
    }

    app_ipcam_Ai_HD_ProcStatus_Set(CVI_FALSE);

    CVI_S32 iTime = GetCurTimeInMsec();

    if (g_HDThreadHandle)
    {
        pthread_join(g_HDThreadHandle, NULL);
        g_HDThreadHandle = 0;
    }

    s32Ret = CVI_TDL_Service_DestroyHandle(g_HDAiServiceHandle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_Service_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        g_HDAiServiceHandle = NULL;
    }

    s32Ret = CVI_TDL_DestroyHandle(g_HDAiHandle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        g_HDAiHandle = NULL;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI HD Thread exit takes %u ms\n", (GetCurTimeInMsec() - iTime));

    return CVI_SUCCESS;
}

int app_ipcam_Ai_HD_Start(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstHdCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI HD not enable\n");
        return CVI_SUCCESS;
    }

    if (g_bHDRunning)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI HD has started\n");
        return CVI_SUCCESS;
    }

    s32Ret = app_ipcam_Ai_HD_Proc_Init();
	if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Ai_HD_Proc_Init failed!\n");
        return s32Ret;
    }

    app_ipcam_Ai_HD_ProcStatus_Set(CVI_TRUE);

    s32Ret = pthread_create(&g_HDThreadHandle, NULL, Thread_HD_PROC, NULL);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "AI pthread_create failed!\n");
        return s32Ret;
    }
    
    return CVI_SUCCESS;
}
