#include <stdio.h>
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include "app_ipcam_md.h"

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/

SMT_MUTEXAUTOLOCK_MD_INIT(g_MDMutex);
static pthread_mutex_t g_MdStatusMutex = PTHREAD_MUTEX_INITIALIZER;
/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static APP_PARAM_MD_CFG_S g_stMdCfg;
static APP_PARAM_MD_CFG_S *g_pstMdCfg = &g_stMdCfg;

static volatile CVI_U32 g_MDThreshold = 0;
static CVI_U32 g_MDFps;
static CVI_U32 g_MDProc;
static volatile bool g_bMDRunning = CVI_FALSE;
static volatile bool g_bMDPause = CVI_FALSE;
static pthread_t g_MDThreadHandle;
static cvi_md_handle_t g_MDHandle = NULL;
static cvimd_object_t g_stMDObjDraw;
/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

APP_PARAM_MD_CFG_S *app_ipcam_MD_Param_Get(void)
{
    return g_pstMdCfg;
}

CVI_VOID app_ipcam_MD_Thresold_Set(CVI_U32 value)
{
    g_MDThreshold = value;
}

CVI_U32 app_ipcam_MD_Thresold_Get(void)
{
    return g_MDThreshold;
}

CVI_VOID app_ipcam_MD_ProcStatus_Set(CVI_BOOL flag)
{
    g_bMDRunning = flag;
}

CVI_BOOL app_ipcam_MD_ProcStatus_Get(void)
{
    return g_bMDRunning;
}

CVI_VOID app_ipcam_MD_Pause_Set(CVI_BOOL flag)
{
    pthread_mutex_lock(&g_MdStatusMutex);
    g_bMDPause = flag;
    pthread_mutex_unlock(&g_MdStatusMutex);
}

CVI_BOOL app_ipcam_MD_Pause_Get(void)
{
    return g_bMDPause;
}

CVI_U32 app_ipcam_MD_ProcFps_Get(void)
{
    return g_MDFps;
}

CVI_S32 app_ipcam_MD_ProcTime_Get(void)
{
    return g_MDProc;
}

static void app_ipcam_Ai_Param_dump(void)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "bEnable=%d Grp=%d Chn=%d GrpW=%d GrpH=%d\n", 
        g_pstMdCfg->bEnable, g_pstMdCfg->VpssGrp, g_pstMdCfg->VpssChn, g_pstMdCfg->u32GrpWidth, g_pstMdCfg->u32GrpHeight);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "threshold=%d miniArea=%d u32BgUpPeriod=%d\n",
        g_pstMdCfg->threshold, g_pstMdCfg->miniArea, g_pstMdCfg->u32BgUpPeriod);

}

CVI_VOID app_ipcam_Md_obj_Free(cvimd_object_t *pstMdObj)
{
    pstMdObj->num_boxes = 0;
    if(pstMdObj->p_boxes)
    {
        free(pstMdObj->p_boxes);
        pstMdObj->p_boxes = NULL;
    }
}

static CVI_S32 app_ipcam_Md_Proc_Init()
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI MD init ------------------> start \n");

    if (g_MDHandle != NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Ai MD Proc Init but handle is not NULL!\n");
        return CVI_SUCCESS;
    }

    app_ipcam_Ai_Param_dump();

    s32Ret  = CVI_MD_Create_Handle(&g_MDHandle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_MD_Create_Handle failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI MD init ------------------> done \n");

    return CVI_SUCCESS;
}

static CVI_VOID *Thread_MD_Proc(CVI_VOID *pArgs)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    CVI_CHAR TaskName[64] = {'\0'};
    sprintf(TaskName, "Thread_MD_Proc");
    prctl(PR_SET_NAME, TaskName, 0, 0, 0);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI MD start running!\n");

    CVI_U32 md_frame = 0;
    CVI_S32 iTime_start, iTime_proc,iTime_fps;
    float iTime_gop;
    iTime_start = GetCurTimeInMsec();

    CVI_U32 count = 0;
    CVI_U32 u32BgUpPeriod = g_pstMdCfg->u32BgUpPeriod;

    CVI_U32 miniArea = g_pstMdCfg->miniArea;
    app_ipcam_MD_Thresold_Set(g_pstMdCfg->threshold);
    cvimd_object_t obj_meta;
    memset(&obj_meta, 0, sizeof(cvimd_object_t));
    
    VIDEO_FRAME_INFO_S stVencFrame;
    memset(&stVencFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

    VPSS_GRP VpssGrp = g_pstMdCfg->VpssGrp;
    VPSS_CHN VpssChn = g_pstMdCfg->VpssChn;

    while (app_ipcam_MD_ProcStatus_Get()) {
        pthread_mutex_lock(&g_MdStatusMutex);
        s32Ret = app_ipcam_MD_Pause_Get();
        
        if (s32Ret) {
            pthread_mutex_unlock(&g_MdStatusMutex);
            usleep(1000*1000);
            continue;
        }
        s32Ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stVencFrame, 3000);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) get frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
            pthread_mutex_unlock(&g_MdStatusMutex);
            usleep(100*1000);
            continue;
        }
        pthread_mutex_unlock(&g_MdStatusMutex);
        iTime_proc = GetCurTimeInMsec();
        
        if ((count % u32BgUpPeriod) == 0)
        {
            APP_PROF_LOG_PRINT(LEVEL_TRACE, "update BG interval=%d, threshold=%d, miniArea=%d\n",
            u32BgUpPeriod, g_MDThreshold, miniArea);
            // Update background. For simplicity, we just set new frame directly.
            if (CVI_MD_Set_Background(g_MDHandle, &stVencFrame) != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "Cannot update background for motion detection\n");
                CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stVencFrame);
                break;
            }
        }

        // Detect moving objects. All moving objects are store in obj_meta.
        CVI_MD_Detect(g_MDHandle, &stVencFrame, &obj_meta.p_boxes, &obj_meta.num_boxes, g_MDThreshold, miniArea);
        g_MDProc = GetCurTimeInMsec() - iTime_proc;
        APP_PROF_LOG_PRINT(LEVEL_TRACE, "MD process takes %d\n", g_MDProc);

        s32Ret = CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stVencFrame);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) release frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
        }

        md_frame ++;
        iTime_fps = GetCurTimeInMsec();
        iTime_gop = (float)(iTime_fps - iTime_start)/1000;
        if(iTime_gop >= 1)
        {
            g_MDFps = md_frame/iTime_gop;
            md_frame = 0;
            iTime_start = iTime_fps;
        }
        
        if (obj_meta.num_boxes == 0) {
            count = (count == u32BgUpPeriod) ? (1) : (count+1);
            app_ipcam_Md_obj_Free(&obj_meta);
            continue;
        }
        SMT_MutexAutoLock_MD(g_MDMutex, lock);
        if (g_stMDObjDraw.p_boxes) {
            app_ipcam_Md_obj_Free(&g_stMDObjDraw);
        }
        memset(&g_stMDObjDraw, 0, sizeof(cvimd_object_t));
        g_stMDObjDraw.num_boxes = obj_meta.num_boxes;
        g_stMDObjDraw.p_boxes = (int *)malloc(g_stMDObjDraw.num_boxes * sizeof(int) * 4);
        memset(g_stMDObjDraw.p_boxes, 0,  g_stMDObjDraw.num_boxes * sizeof(int) * 4 );
        memcpy(g_stMDObjDraw.p_boxes, obj_meta.p_boxes, g_stMDObjDraw.num_boxes * sizeof(int) * 4);

        app_ipcam_Md_obj_Free(&obj_meta);

        count = (count == u32BgUpPeriod) ? (1) : (count+1);
    }

    pthread_exit(NULL);

    return NULL;
}

int app_ipcam_MD_ObjDrawInfo_Get(cvimd_object_t *pstMdObj)
{
    _NULL_POINTER_CHECK_(pstMdObj, -1);

    SMT_MutexAutoLock_MD(g_MDMutex, lock);

    if (g_stMDObjDraw.num_boxes == 0) {
        return CVI_SUCCESS;
    } else {
        pstMdObj->num_boxes = g_stMDObjDraw.num_boxes;
        pstMdObj->p_boxes = (int *)malloc(pstMdObj->num_boxes * sizeof(int) * 4);
        _NULL_POINTER_CHECK_(pstMdObj->p_boxes, -1);
        memset(pstMdObj->p_boxes, 0, pstMdObj->num_boxes * sizeof(int) * 4);
        memcpy(pstMdObj->p_boxes, g_stMDObjDraw.p_boxes, pstMdObj->num_boxes * sizeof(int) * 4);

        app_ipcam_Md_obj_Free(&g_stMDObjDraw);
    }

    return CVI_SUCCESS;
}

int app_ipcam_MD_Stop(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstMdCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI MD not enable\n");
        return CVI_SUCCESS;
    }

    if (!app_ipcam_MD_ProcStatus_Get())
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI MD has not running!\n");
        return s32Ret;
    }

    app_ipcam_MD_ProcStatus_Set(CVI_FALSE);

    CVI_S32 iTime = GetCurTimeInMsec();

    if (g_MDThreadHandle)
    {
        pthread_join(g_MDThreadHandle, NULL);
        g_MDThreadHandle = 0;
    }

    s32Ret = CVI_MD_Destroy_Handle(g_MDHandle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_MD_Destroy_Handle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        g_MDHandle = NULL;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI MD Thread exit takes %u ms\n", (GetCurTimeInMsec() - iTime));

    return CVI_SUCCESS;
}

int app_ipcam_MD_Start(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstMdCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI MD not enable\n");
        return CVI_SUCCESS;
    }

    if (g_bMDRunning)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI MD has started\n");
        return CVI_SUCCESS;
    }

    s32Ret = app_ipcam_Md_Proc_Init();
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Md_Proc_Init failed with error %d\n", s32Ret);
        return s32Ret;
    }

    app_ipcam_MD_ProcStatus_Set(CVI_TRUE);

    s32Ret = pthread_create(&g_MDThreadHandle, NULL, Thread_MD_Proc, NULL);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Thread_Motion_Detecte failed with error %d\n", s32Ret);
        return s32Ret;
    }

    return s32Ret;
}

/*****************************************************************
 *  The following API for command test used             Front
 * **************************************************************/
CVI_S32 app_ipcam_MD_StatusGet(void)
{
    return g_MDHandle ? 1 : 0;
}

/*****************************************************************
 *  The above API for command test used                 End
 * **************************************************************/
