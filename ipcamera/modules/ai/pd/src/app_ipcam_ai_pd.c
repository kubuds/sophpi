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

// Ai Model info
/*****************************************************************
 * Model Func : Persion , vehicle , non-motor vehicle
 * Model ID   : CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PERSON_VEHICLE
 * Inference  : CVI_TDL_MobileDetV2_Person_Vehicle
 * Model file : mobiledetv2-person-vehicle-ls-768.cvimodel (input size 1x3x512x768)
 *              mobiledetv2-person-vehicle-ls.cvimodel (input size 1x3x512x768)
 =================================================================
 * Model Func : Pedestrian detection
 * Model ID   : CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN_D0
 * Inference  : CVI_TDL_MobileDetV2_Pedestrian_D0
 * Model file : mobiledetv2-pedestrian-d0-ls-384.cvimodel (input size 1x3x256x384)
 *              mobiledetv2-pedestrian-d0-ls-640.cvimodel (input size 1x3x384x640)
 *              mobiledetv2-pedestrian-d0-ls-768.cvimodel (input size 1x3x512x768)
 *              mobiledetv2-pedestrian-d1-ls.cvimodel (input size 1x3x512x896)
 *              mobiledetv2-pedestrian-d1-ls-1024.cvimodel (input size 1x3x384x640)
 =================================================================
 * Model Func : Vehicle detection
 * Model ID   : CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_VEHICLE_D0
 * Inference  : CVI_TDL_MobileDetV2_Vehicle_D0
 * Model file : mobiledetv2-vehicle-d0-ls.cvimodel (input size 1x3x384x512)
****************************************************************/

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
SMT_MUTEXAUTOLOCK_INIT(g_PDMutex);
static pthread_mutex_t g_PDStatusMutex = PTHREAD_MUTEX_INITIALIZER;
/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
#if 0
static APP_PARAM_AI_PD_CFG_S g_stPdCfg = {
    .bEnable = CVI_TRUE,
    .VpssGrp = 3,
    .VpssChn = 0,
    .u32GrpWidth = 640,
    .u32GrpHeight = 360,
    .model_size_w = 384,
    .model_size_h = 256,
    .bVpssPreProcSkip = CVI_TRUE,
    .threshold = 0.7,
    .model_id = CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN_D0,
    .model_path = "/mnt/data/ai_models/mobiledetv2-pedestrian-d0-ls-384.cvimodel",
    .rect_brush = {
        .color = {
            .r = 3.0*255,
            .g = 8.0*255,
            .b = 247.0*255,
        },
        .size = 4,
    },
};
#else
static APP_PARAM_AI_PD_CFG_S g_stPdCfg;
#endif

static APP_PARAM_AI_PD_CFG_S *g_pstPdCfg = &g_stPdCfg;

static CVI_U32 g_PDFps;
static CVI_U32 g_IntrusionNum;
static CVI_U32 g_PDProc;
static CVI_U32 g_PDRectNum;
static float g_PDScaleX, g_PDScaleY;
static volatile bool g_bPDRunning = CVI_FALSE;
static volatile bool g_bPDPause = CVI_FALSE;
static pthread_t g_PDThreadHandle;
static cvitdl_handle_t g_PDAiHandle = NULL;
static cvitdl_service_handle_t g_PDAiServiceHandle = NULL;
static cvtdl_object_t g_stPDObjDraw;
static pfpInferenceFunc g_pfpPDInference;
static int g_PDCaptureCounts = 0;

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

APP_PARAM_AI_PD_CFG_S *app_ipcam_Ai_PD_Param_Get(void)
{
    return g_pstPdCfg;
}

CVI_VOID app_ipcam_Ai_PD_ProcStatus_Set(CVI_BOOL flag)
{
    g_bPDRunning = flag;
}

CVI_BOOL app_ipcam_Ai_PD_ProcStatus_Get(void)
{
    return g_bPDRunning;
}

CVI_VOID app_ipcam_Ai_PD_Pause_Set(CVI_BOOL flag)
{
    pthread_mutex_lock(&g_PDStatusMutex);
    g_bPDPause = flag;
    pthread_mutex_unlock(&g_PDStatusMutex);
}

CVI_BOOL app_ipcam_Ai_PD_Pause_Get(void)
{
    return g_bPDPause;
}

CVI_U32 app_ipcam_Ai_PD_ProcIntrusion_Num_Get(void)
{
    return g_IntrusionNum;
}

CVI_U32 app_ipcam_Ai_PD_ProcFps_Get(void)
{
    return g_PDFps;
}

CVI_S32 app_ipcam_Ai_PD_ProcTime_Get(void)
{
    return g_PDProc;
}

#if 0
static int getNumDigits(uint64_t num) 
{
    int digits = 0;
    do {
        num /= 10;
        digits++;
    } while (num != 0);

    return digits;
}

static char *uint64ToString(uint64_t number) 
{
    int n = getNumDigits(number);
    int i;
    char *numArray = calloc(n, sizeof(char));
    for (i = n - 1; i >= 0; --i, number /= 10) {
        numArray[i] = (number % 10) + '0';
    }

    return numArray;
}
#endif

static CVI_S32 app_ipcam_Ai_InferenceFunc_Get(CVI_TDL_SUPPORTED_MODEL_E model_id)
{
    switch (model_id)
    {
        case CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN:
            g_pfpPDInference = CVI_TDL_Detection;
        break;

        default:
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "model id (%d) invalid!\n", model_id);
            return CVI_FAILURE;
    }

    return CVI_SUCCESS;
}

static void app_ipcam_Ai_Param_dump(void)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "bEnable=%d Grp=%d Chn=%d GrpW=%d GrpH=%d\n", 
        g_pstPdCfg->bEnable, g_pstPdCfg->VpssGrp, g_pstPdCfg->VpssChn, g_pstPdCfg->u32GrpWidth, g_pstPdCfg->u32GrpHeight);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "model_w=%d model_h=%d bSkip=%d threshold=%f\n",
        g_pstPdCfg->model_size_w, g_pstPdCfg->model_size_h, g_pstPdCfg->bVpssPreProcSkip, g_pstPdCfg->threshold);
    APP_PROF_LOG_PRINT(LEVEL_INFO, " model_id=%d model_path=%s\n",
        g_pstPdCfg->model_id, g_pstPdCfg->model_path);
    APP_PROF_LOG_PRINT(LEVEL_INFO, " color r=%f g=%f b=%f size=%d\n",
        g_pstPdCfg->rect_brush.color.r, g_pstPdCfg->rect_brush.color.g,
        g_pstPdCfg->rect_brush.color.b, g_pstPdCfg->rect_brush.size);

}


CVI_S32 app_ipcam_Ai_Pd_Intrusion_Init(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PARAM_OSDC_CFG_S *OsdcCfg = app_ipcam_Osdc_Param_Get();
    float r0[2][6] = {{ g_pstPdCfg->region_stRect_x1, g_pstPdCfg->region_stRect_x2,
                        g_pstPdCfg->region_stRect_x3, g_pstPdCfg->region_stRect_x4,
                        g_pstPdCfg->region_stRect_x5, g_pstPdCfg->region_stRect_x6 },

                      { g_pstPdCfg->region_stRect_y1, g_pstPdCfg->region_stRect_y2,
                        g_pstPdCfg->region_stRect_y3, g_pstPdCfg->region_stRect_y4,
                        g_pstPdCfg->region_stRect_y5, g_pstPdCfg->region_stRect_y6 }};
    cvtdl_pts_t test_region_0;
    test_region_0.size = (uint32_t)sizeof(r0) / (sizeof(float) * 2);
    test_region_0.x = malloc(sizeof(float) * test_region_0.size);
    test_region_0.y = malloc(sizeof(float) * test_region_0.size);
    memcpy(test_region_0.x, r0[0], sizeof(float) * test_region_0.size);
    memcpy(test_region_0.y, r0[1], sizeof(float) * test_region_0.size);

    if (g_PDAiServiceHandle == NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Pd g_PDAiServiceHandle not creat\n");
    }
    else
    {
        s32Ret = CVI_TDL_Service_Polygon_SetTarget(g_PDAiServiceHandle, &test_region_0);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_Service_Polygon_SetTarget failed with %#x!\n", s32Ret);
        }

        int iOsdcIndex = 0;
        g_PDRectNum = 0;
        if (OsdcCfg->bShow[iOsdcIndex]) {
            OsdcCfg->osdcObjNum[iOsdcIndex] = OsdcCfg->osdcObjNum[iOsdcIndex] + 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].bShow = 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].type  = 2;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].color = 0x83ff;//0xff00ffff;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x1    = g_pstPdCfg->region_stRect_x1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y1    = g_pstPdCfg->region_stRect_y1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x2    = g_pstPdCfg->region_stRect_x2;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y2    = g_pstPdCfg->region_stRect_y2;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].thickness = 4;      

            OsdcCfg->osdcObjNum[iOsdcIndex] = OsdcCfg->osdcObjNum[iOsdcIndex] + 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].bShow = 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].type  = 2;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].color = 0x83ff;//0xff00ffff;;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x1    = g_pstPdCfg->region_stRect_x2;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y1    = g_pstPdCfg->region_stRect_y2;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x2    = g_pstPdCfg->region_stRect_x3;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y2    = g_pstPdCfg->region_stRect_y3;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].thickness = 4;  

            OsdcCfg->osdcObjNum[iOsdcIndex] = OsdcCfg->osdcObjNum[iOsdcIndex] + 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].bShow = 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].type  = 2;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].color = 0x83ff;//0xff00ffff;;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x1    = g_pstPdCfg->region_stRect_x3;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y1    = g_pstPdCfg->region_stRect_y3;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x2    = g_pstPdCfg->region_stRect_x4;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y2    = g_pstPdCfg->region_stRect_y4;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].thickness = 4;  

            OsdcCfg->osdcObjNum[iOsdcIndex] = OsdcCfg->osdcObjNum[iOsdcIndex] + 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].bShow = 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].type  = 2;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].color = 0x83ff;//0xff00ffff;;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x1    = g_pstPdCfg->region_stRect_x4;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y1    = g_pstPdCfg->region_stRect_y4;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x2    = g_pstPdCfg->region_stRect_x5;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y2    = g_pstPdCfg->region_stRect_y5;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].thickness = 4;  

            OsdcCfg->osdcObjNum[iOsdcIndex] = OsdcCfg->osdcObjNum[iOsdcIndex] + 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].bShow = 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].type  = 2;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].color = 0x83ff;//0xff00ffff;;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x1    = g_pstPdCfg->region_stRect_x5;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y1    = g_pstPdCfg->region_stRect_y5;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x2    = g_pstPdCfg->region_stRect_x6;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y2    = g_pstPdCfg->region_stRect_y6;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].thickness = 4;

            OsdcCfg->osdcObjNum[iOsdcIndex] = OsdcCfg->osdcObjNum[iOsdcIndex] + 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].bShow = 1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].type  = 2;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].color = 0x83ff;//0xff00ffff;;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x1    = g_pstPdCfg->region_stRect_x6;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y1    = g_pstPdCfg->region_stRect_y6;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].x2    = g_pstPdCfg->region_stRect_x1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].y2    = g_pstPdCfg->region_stRect_y1;
            OsdcCfg->osdcObj[iOsdcIndex][OsdcCfg->osdcObjNum[iOsdcIndex]].thickness = 4; 

            g_PDRectNum = 6;
        }
    }

    free(test_region_0.x);
    free(test_region_0.y);
    return s32Ret;
}

static CVI_S32 app_ipcam_Ai_VpssChnAttr_Set(CVI_VOID)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    CVI_TDL_SUPPORTED_MODEL_E model_id = g_pstPdCfg->model_id;
    VPSS_GRP VpssGrp = g_pstPdCfg->VpssGrp;
    VPSS_CHN VpssChn = g_pstPdCfg->VpssChn;

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
                            g_PDAiHandle,
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
    if ((pastVpssChnAttr->u32Width == 384) && (pastVpssChnAttr->u32Height == 256)) {
        vpssConfig.chn_attr.stAspectRatio.stVideoRect.u32Width = 384;
        vpssConfig.chn_attr.stAspectRatio.stVideoRect.u32Height = 216;
    }

    s32Ret = CVI_VPSS_SetChnScaleCoefLevel(VpssGrp, VpssChn, vpssConfig.chn_coeff);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_SetChnScaleCoefLevel failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "vpssConfig.chn_attr u32Width=%d u32Height=%d SrcFR=%d DstFR=%d\n", 
                    vpssConfig.chn_attr.u32Width, vpssConfig.chn_attr.u32Height, s32SrcFrameRate, s32DstFrameRate);

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

    s32Ret = CVI_TDL_SetVpssTimeout(g_PDAiHandle, 2000);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "set vpss timeout failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    return CVI_SUCCESS;
}

static CVI_S32 app_ipcam_Ai_PD_Proc_Init(CVI_VOID)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI PD init ------------------> start \n");

    CVI_S32 s32Ret = CVI_SUCCESS;

    app_ipcam_Ai_Param_dump();

    CVI_TDL_SUPPORTED_MODEL_E model_id = g_pstPdCfg->model_id;

    if (g_PDAiHandle == NULL)
    {
        s32Ret = CVI_TDL_CreateHandle(&g_PDAiHandle);
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

    if (g_PDAiServiceHandle == NULL)
    {
        s32Ret = CVI_TDL_Service_CreateHandle(&g_PDAiServiceHandle, g_PDAiHandle);
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

    s32Ret = app_ipcam_Ai_InferenceFunc_Get(model_id);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "unsupported model id: %d \n", model_id);
        return s32Ret;
    }

    s32Ret = CVI_TDL_OpenModel(g_PDAiHandle, model_id, g_pstPdCfg->model_path);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_SetModelPath failed with %#x! maybe reset model path\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_TDL_SetModelThreshold(g_PDAiHandle, model_id, g_pstPdCfg->threshold);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_SetModelThreshold failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    CVI_BOOL bSkip = g_pstPdCfg->bVpssPreProcSkip;
    CVI_TDL_SetSkipVpssPreprocess(g_PDAiHandle, model_id, bSkip);
    if (bSkip)
    {
        s32Ret = app_ipcam_Ai_VpssChnAttr_Set();
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "VpssChnAttr Set failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }

    if (g_pstPdCfg->Intrusion_bEnable)
    {
        s32Ret = app_ipcam_Ai_Pd_Intrusion_Init();
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Ai_Pd_Intrusion_Init failed with %#x!\n", s32Ret);
        }
    }

    s32Ret = CVI_TDL_SelectDetectClass(g_PDAiHandle, model_id, 6,
                                        CVI_TDL_DET_TYPE_PERSON,
                                        CVI_TDL_DET_TYPE_BICYCLE,
                                        CVI_TDL_DET_TYPE_CAR,
                                        CVI_TDL_DET_TYPE_MOTORBIKE,
                                        CVI_TDL_DET_TYPE_TRUCK,
                                        CVI_TDL_DET_TYPE_BUS);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_SelectDetectClass failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI PD init ------------------> done \n");

    return CVI_SUCCESS;
}

static CVI_VOID *Thread_PD_PROC(CVI_VOID *arg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI PD start running!\n");

    prctl(PR_SET_NAME, "Thread_PD_PROC");

    VPSS_GRP VpssGrp = g_pstPdCfg->VpssGrp;
    VPSS_CHN VpssChn = g_pstPdCfg->VpssChn;

    VIDEO_FRAME_INFO_S stfdFrame = {0};

    CVI_U32 pd_frame = 0;
    CVI_S32 iTime_start, iTime_proc,iTime_fps;
    float iTime_gop;
    iTime_start = GetCurTimeInMsec();

    while (app_ipcam_Ai_PD_ProcStatus_Get()) {
        pthread_mutex_lock(&g_PDStatusMutex);
        s32Ret = app_ipcam_Ai_PD_Pause_Get();
        
        if (s32Ret) {
            pthread_mutex_unlock(&g_PDStatusMutex);
            usleep(1000*1000);
            continue;
        }
        s32Ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stfdFrame, 3000);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) get frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
            pthread_mutex_unlock(&g_PDStatusMutex);
            usleep(100*1000);
            continue;
        }
        pthread_mutex_unlock(&g_PDStatusMutex);
        iTime_proc = GetCurTimeInMsec();
        cvtdl_object_t obj_meta;
        memset(&obj_meta, 0, sizeof(cvtdl_object_t));

        g_pfpPDInference(g_PDAiHandle, &stfdFrame, g_pstPdCfg->model_id, &obj_meta);
        if (g_pstPdCfg->capture_enable){
            if (obj_meta.size > 0){
                g_PDCaptureCounts ++;
            }else{
                g_PDCaptureCounts = 0;
            }
            if (g_PDCaptureCounts > g_pstPdCfg->capture_frames){
                app_ipcam_JpgCapFlag_Set(CVI_TRUE);
                g_PDCaptureCounts = 0;
            }
        }

        if (g_pstPdCfg->Intrusion_bEnable)
        {
            g_IntrusionNum = 0;
            for (uint32_t i = 0; i < obj_meta.size; i++)
            {
                bool is_intrusion;
                cvtdl_bbox_t t_bbox = obj_meta.info[i].bbox;
                t_bbox.x1 *= g_PDScaleX;
                t_bbox.y1 *= g_PDScaleY;
                t_bbox.x2 *= g_PDScaleX;
                t_bbox.y2 *= g_PDScaleY;
                CVI_TDL_Service_Polygon_Intersect(g_PDAiServiceHandle, &t_bbox, &is_intrusion);
                if (is_intrusion)
                {
                    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "[%u] intrusion! (%.1f,%.1f,%.1f,%.1f)\n", i, obj_meta.info[i].bbox.x1,
                    obj_meta.info[i].bbox.y1, obj_meta.info[i].bbox.x2, obj_meta.info[i].bbox.y2);
                    strcpy(obj_meta.info[i].name,"Intrusion");
                    g_IntrusionNum ++;
                }
                else
                {
                    strcpy(obj_meta.info[i].name,"");
                }
            }
        }

        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "PD obj: %d \n", obj_meta.size);
        g_PDProc = GetCurTimeInMsec() - iTime_proc;
        APP_PROF_LOG_PRINT(LEVEL_TRACE, "PD process takes %d\n", g_PDProc);
        s32Ret = CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stfdFrame);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) release frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
        }

        pd_frame ++;
        iTime_fps = GetCurTimeInMsec();
        iTime_gop = (float)(iTime_fps - iTime_start)/1000;
        if(iTime_gop >= 1)
        {
            g_PDFps = pd_frame/iTime_gop;
            pd_frame = 0;
            iTime_start = iTime_fps;
        }

        if (obj_meta.size == 0) {
            continue;
        }
        SMT_MutexAutoLock(g_PDMutex, lock);
        if (g_stPDObjDraw.info != NULL) {
            CVI_TDL_Free(&g_stPDObjDraw);
        }
        memset(&g_stPDObjDraw, 0, sizeof(cvtdl_object_t));
        memcpy(&g_stPDObjDraw, &obj_meta, sizeof obj_meta);
        g_stPDObjDraw.info = (cvtdl_object_info_t *)malloc(obj_meta.size * sizeof(cvtdl_object_info_t));
        memset(g_stPDObjDraw.info, 0, sizeof(cvtdl_object_info_t) * g_stPDObjDraw.size);
        for (uint32_t i = 0; i < obj_meta.size; i++) {
            CVI_TDL_CopyObjectInfo(&obj_meta.info[i], &g_stPDObjDraw.info[i]);
            // APP_PROF_LOG_PRINT(LEVEL_INFO, "PD obj[%d] classes: %d \n", i, obj_meta.info[i].classes);
            g_stPDObjDraw.info[i].vehicle_properity = NULL;
            g_stPDObjDraw.info[i].pedestrian_properity = NULL;
        }

        CVI_TDL_Free(&obj_meta);
    }

    pthread_exit(NULL);

    return NULL;
}

int app_ipcam_Ai_PD_ObjDrawInfo_Get(cvtdl_object_t *pstAiObj)
{
    _NULL_POINTER_CHECK_(pstAiObj, -1);

    SMT_MutexAutoLock(g_PDMutex, lock);

    if (g_stPDObjDraw.size == 0) {
        return CVI_SUCCESS;
    } else {
        memcpy(pstAiObj, &g_stPDObjDraw, sizeof g_stPDObjDraw);
        pstAiObj->info = (cvtdl_object_info_t *)malloc(g_stPDObjDraw.size * sizeof(cvtdl_object_info_t));
        _NULL_POINTER_CHECK_(pstAiObj->info, -1);
        memset(pstAiObj->info, 0, sizeof(cvtdl_object_info_t) * g_stPDObjDraw.size);
        for (CVI_U32 i = 0; i < g_stPDObjDraw.size; i++) {
            CVI_TDL_CopyObjectInfo(&g_stPDObjDraw.info[i], &pstAiObj->info[i]);
            pstAiObj->info[i].vehicle_properity = NULL;
            pstAiObj->info[i].pedestrian_properity = NULL;
        }
        CVI_TDL_Free(&g_stPDObjDraw);
    }

    return CVI_SUCCESS;
}

int app_ipcam_Ai_PD_Rect_Draw(VIDEO_FRAME_INFO_S *pstVencFrame)
{
    CVI_S32 iTime;

    if (app_ipcam_Ai_PD_ProcStatus_Get())
    {
        SMT_MutexAutoLock(g_PDMutex, lock);
        iTime = GetCurTimeInMsec();
        CVI_TDL_RescaleMetaRB(pstVencFrame, &g_stPDObjDraw);
        if (g_pstPdCfg->Intrusion_bEnable)
        {
            for (uint32_t i = 0; i < g_stPDObjDraw.size; i++)
            {
                bool is_intrusion;
                cvtdl_bbox_t t_bbox = g_stPDObjDraw.info[i].bbox;
                t_bbox.x1 *= 1;
                t_bbox.y1 *= 1;
                t_bbox.x2 *= 1;
                t_bbox.y2 *= 1;
                CVI_TDL_Service_Polygon_Intersect(g_PDAiServiceHandle, &t_bbox, &is_intrusion);
                if (is_intrusion)
                {
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "[%u] intrusion! (%.1f,%.1f,%.1f,%.1f)\n", i, g_stPDObjDraw.info[i].bbox.x1,
                    g_stPDObjDraw.info[i].bbox.y1, g_stPDObjDraw.info[i].bbox.x2, g_stPDObjDraw.info[i].bbox.y2);
                }
            }
        }

        CVI_TDL_Service_ObjectDrawRect(
                            g_PDAiServiceHandle,
                            &g_stPDObjDraw,
                            pstVencFrame,
                            CVI_TRUE,
                            g_pstPdCfg->rect_brush);
        #ifdef AI_DRAW_OBJECT_ID
        for (uint32_t i = 0; i < g_stPDObjDraw.size; i++)
        {
            char *id_num = uint64ToString(g_stPDObjDraw.info[i].unique_id);
            CVI_TDL_Service_ObjectWriteText(id_num, g_stPDObjDraw.info[i].bbox.x1, g_stPDObjDraw.info[i].bbox.y1,
                                pstVencFrame, -1, -1, -1);
            free(id_num);
        }
        #endif
        CVI_TDL_Free(&g_stPDObjDraw);
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "PD draw result takes %u ms \n", (GetCurTimeInMsec() - iTime));
    }

    return CVI_SUCCESS;
}

int app_ipcam_Ai_PD_Stop(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstPdCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI PD not enable\n");
        return CVI_SUCCESS;
    }

    if (!app_ipcam_Ai_PD_ProcStatus_Get())
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI PD has not running!\n");
        return s32Ret;
    }

    app_ipcam_Ai_PD_ProcStatus_Set(CVI_FALSE);

    CVI_S32 iTime = GetCurTimeInMsec();

    if (g_PDThreadHandle)
    {
        pthread_join(g_PDThreadHandle, NULL);
        g_PDThreadHandle = 0;
    }
    
    if (g_pstPdCfg->Intrusion_bEnable)
    {
        g_IntrusionNum = 0;
        APP_PARAM_OSDC_CFG_S *stOsdcCfg = app_ipcam_Osdc_Param_Get();
        int iOsdcIndex = 0;
        CVI_U32 i = 0;
        if (stOsdcCfg->bShow[iOsdcIndex]) {
            for (i = 0; i < g_PDRectNum; i++)
            {
                stOsdcCfg->osdcObj[iOsdcIndex][stOsdcCfg->osdcObjNum[iOsdcIndex]].bShow = 0;
                stOsdcCfg->osdcObjNum[iOsdcIndex] = stOsdcCfg->osdcObjNum[iOsdcIndex] - 1;
            }
        }
    }

    s32Ret = CVI_TDL_Service_DestroyHandle(g_PDAiServiceHandle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_Service_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        g_PDAiServiceHandle = NULL;
    }

    s32Ret = CVI_TDL_DestroyHandle(g_PDAiHandle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        g_PDAiHandle = NULL;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI PD Thread exit takes %u ms\n", (GetCurTimeInMsec() - iTime));

    return CVI_SUCCESS;
}

int app_ipcam_Ai_PD_Start(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_VPSS_GRP_CFG_T *pstVpssCfg = &app_ipcam_Vpss_Param_Get()->astVpssGrpCfg[0];

    if (!g_pstPdCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI PD not enable\n");
        return CVI_SUCCESS;
    }

    if (g_bPDRunning)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI PD has started\n");
        return CVI_SUCCESS;
    }

    s32Ret = app_ipcam_Ai_PD_Proc_Init();
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Ai_PD_Proc_Init failed!\n");
        return s32Ret;
    }
    
    g_PDScaleX = g_PDScaleY = fmax(((float)pstVpssCfg->astVpssChnAttr[0].u32Width / (float)g_pstPdCfg->model_size_w), ((float)pstVpssCfg->astVpssChnAttr[0].u32Height / (float)g_pstPdCfg->model_size_h));

    app_ipcam_Ai_PD_ProcStatus_Set(CVI_TRUE);

    s32Ret = pthread_create(&g_PDThreadHandle, NULL, Thread_PD_PROC, NULL);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "AI pthread_create failed!\n");
        return s32Ret;
    }
    
    return CVI_SUCCESS;
}

/*****************************************************************
 *  The following API for command test used             Front
 * **************************************************************/
CVI_S32 app_ipcam_Ai_PD_StatusGet(void)
{
    return g_PDAiHandle ? 1 : 0;
}

CVI_S32 app_ipcam_Pd_threshold_Set(float threshold)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (g_PDAiHandle) {
        s32Ret = CVI_TDL_SetModelThreshold(g_PDAiHandle, g_pstPdCfg->model_id, threshold);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_TDL_SetModelThreshold failed with %#x!\n",g_pstPdCfg->model_path, s32Ret);
            return s32Ret;
        }
    }

    return s32Ret;
}

/*****************************************************************
 *  The above API for command test used                 End
 * **************************************************************/
