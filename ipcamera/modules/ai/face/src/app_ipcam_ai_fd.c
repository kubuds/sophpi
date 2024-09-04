#include <stdio.h>
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include <dirent.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "app_ipcam_ai.h"

// Ai Model info
/*****************************************************************
 * Model Func : Face detection
 * Model ID   : CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PERSON_VEHICLE
 * Inference  : CVI_TDL_MobileDetV2_Person_Vehicle
 * Model file : mobiledetv2-person-vehicle-ls-768.cvimodel
 *              mobiledetv2-person-vehicle-ls.cvimodel
 *================================================================
 * Model Func : Face recognition
 * Model ID   : CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN_D0
 * Inference  : CVI_TDL_MobileDetV2_Pedestrian_D0
 * Model file : mobiledetv2-pedestrian-d0-ls-384.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-640.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-768.cvimodel
 *              mobiledetv2-pedestrian-d1-ls.cvimodel
 *              mobiledetv2-pedestrian-d1-ls-1024.cvimodel
 ****************************************************************
 * Model Func : Mask Face detection
 * Model ID   : CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN_D0
 * Inference  : CVI_TDL_MobileDetV2_Pedestrian_D0
 * Model file : mobiledetv2-pedestrian-d0-ls-384.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-640.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-768.cvimodel
 *              mobiledetv2-pedestrian-d1-ls.cvimodel
 *              mobiledetv2-pedestrian-d1-ls-1024.cvimodel
 ****************************************************************
  * Model Func : Face capture,
 * Model ID   : CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN_D0
 * Inference  : CVI_TDL_MobileDetV2_Pedestrian_D0
 * Model file : mobiledetv2-pedestrian-d0-ls-384.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-640.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-768.cvimodel
 *              mobiledetv2-pedestrian-d1-ls.cvimodel
 *              mobiledetv2-pedestrian-d1-ls-1024.cvimodel
 ****************************************************************/


/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/

SMT_MUTEXAUTOLOCK_INIT(g_FDMutex);
static pthread_mutex_t g_FDStatusMutex = PTHREAD_MUTEX_INITIALIZER;
/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static APP_PARAM_AI_FD_CFG_S g_stFDCfg;

static APP_PARAM_AI_FD_CFG_S *g_pstFDCfg = &g_stFDCfg;

static volatile bool g_bFDRunning = CVI_FALSE;
static volatile bool g_bFDPause = CVI_FALSE;
static pthread_t g_FDThreadHandle;
static cvitdl_handle_t g_FDAiHandle = NULL;
static cvitdl_service_handle_t g_FDAiServiceHandle = NULL;
static cvtdl_face_t g_stFDObjDraw;
static pfpInferenceFunc g_pfpFDInference;

#define IMAGE_DIR "/mnt/sd/picture/"
struct LinkList
{
    int index;
    char name[128];
    struct LinkList *next;
};
static CVI_U32 g_FDFps;
static CVI_U32 g_FDProc;
static CVI_U32 g_FaceCnt = 0;
static struct LinkList *g_FaceHead;
static struct LinkList *g_FaceTail;
static IVE_HANDLE g_stFDIveHandle = NULL;
static cvtdl_service_feature_array_t g_FaceFeatureArray;


/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

APP_PARAM_AI_FD_CFG_S *app_ipcam_Ai_FD_Param_Get(void)
{
    return g_pstFDCfg;
}

CVI_VOID app_ipcam_Ai_FD_ProcStatus_Set(CVI_BOOL flag)
{
    g_bFDRunning = flag;
}

CVI_BOOL app_ipcam_Ai_FD_ProcStatus_Get(void)
{
    return g_bFDRunning;
}

CVI_VOID app_ipcam_Ai_FD_Pause_Set(CVI_BOOL flag)
{
    pthread_mutex_lock(&g_FDStatusMutex);
    g_bFDPause = flag;
    pthread_mutex_unlock(&g_FDStatusMutex);
}

CVI_BOOL app_ipcam_Ai_FD_Pause_Get(void)
{
    return g_bFDPause;
}

CVI_U32 app_ipcam_Ai_FD_ProcFps_Get(void)
{
    return g_FDFps;
}

CVI_S32 app_ipcam_Ai_FD_ProcTime_Get(void)
{
    return g_FDProc;
}

static void app_ipcam_Ai_Fd_Param_dump(void)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "FD_bEnable=%d FR_bEnable=%d MASK_bEnable=%d FACE_AE_bEnable=%d Grp=%d Chn=%d GrpW=%d GrpH=%d\n",
        g_pstFDCfg->FD_bEnable, g_pstFDCfg->FR_bEnable,g_pstFDCfg->MASK_bEnable, g_pstFDCfg->FACE_AE_bEnable,\
                g_pstFDCfg->VpssGrp, g_pstFDCfg->VpssChn, g_pstFDCfg->u32GrpWidth, g_pstFDCfg->u32GrpHeight);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "model_w=%d model_h=%d bSkip=%d FdPoolId=%d threshold_fd=%f threshold_fr=%f  threshold_mask=%f \n",
        g_pstFDCfg->model_size_w, g_pstFDCfg->model_size_h, g_pstFDCfg->bVpssPreProcSkip, g_pstFDCfg->FdPoolId,\
        g_pstFDCfg->threshold_fd,g_pstFDCfg->threshold_fr,g_pstFDCfg->threshold_mask);
    APP_PROF_LOG_PRINT(LEVEL_INFO, " model_id_fd=%d model_path_fd=%s model_id_fr=%d model_path_fr=%s model_id_mask=%d model_path_mask=%s\n",
                    g_pstFDCfg->model_id_fd, g_pstFDCfg->model_path_fd,\
                    g_pstFDCfg->model_id_fr, g_pstFDCfg->model_path_fr,\
                    g_pstFDCfg->model_id_mask, g_pstFDCfg->model_path_mask);
    APP_PROF_LOG_PRINT(LEVEL_INFO, " color r=%f g=%f b=%f size=%d\n",
        g_pstFDCfg->rect_brush.color.r, g_pstFDCfg->rect_brush.color.g, \
        g_pstFDCfg->rect_brush.color.b, g_pstFDCfg->rect_brush.size);
}


static void App_Clean_List(struct LinkList *List_Head)
{
    struct LinkList *pHead = NULL;
    struct LinkList *p = NULL;

    if (List_Head == NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "Pkg Invalid input!\n");
        return ;
    }

    pHead = List_Head;
    while (pHead != NULL)
    {
        p = pHead->next;
        free(pHead);
        pHead = NULL;
        pHead = p;
    }
    List_Head = NULL;
    return ;
}

static CVI_S32 app_ipcam_Ai_InferenceFunc_Get(CVI_TDL_SUPPORTED_MODEL_E model_id)
{
    switch (model_id)
    {
        case CVI_TDL_SUPPORTED_MODEL_SCRFDFACE:
           g_pfpFDInference = CVI_TDL_FaceDetection;
        break;

        default:
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "model id (%d) invalid!\n", model_id);
            return CVI_FAILURE;
    }

    return CVI_SUCCESS;
}

static CVI_S32 app_ipcam_Ai_FR_Init(void)
{
   /* face_name list */
    CVI_S32 ret = CVI_SUCCESS;
    g_FaceHead= (struct LinkList *)malloc(sizeof(struct LinkList));
    g_FaceHead->next = NULL;
    g_FaceTail = g_FaceHead;
    /* face  Facial Feature definition */
    g_FaceFeatureArray.data_num = 1000;
    g_FaceFeatureArray.feature_length = 256;
    g_FaceFeatureArray.ptr = (int8_t *)malloc(g_FaceFeatureArray.data_num * g_FaceFeatureArray.feature_length);
    g_FaceFeatureArray.type = TYPE_INT8;
    /* face  Face registet modle init */
    ret = CVI_TDL_OpenModel(g_FDAiHandle, g_pstFDCfg->model_id_fr, g_pstFDCfg->model_path_fr);
    if (ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_TDL_SetModelPath failed with %#x!\n",  g_pstFDCfg->model_path_fr, ret);
        return ret;
    }

    ret = CVI_TDL_SetModelThreshold(g_FDAiHandle, g_pstFDCfg->model_id_fr, g_pstFDCfg->threshold_fr);
    if (ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_TDL_SetModelThreshold failed with %#x!\n", g_pstFDCfg->model_path_fr, ret);
        return ret;
    }

    return ret;
}


static  CVI_S32 app_ipcam_Face_Registeration(CVI_VOID)
{
    CVI_S32 ret = CVI_SUCCESS;
    DIR * dir = NULL;
    cvtdl_face_t face_test;
    char image_path[512];
    VIDEO_FRAME_INFO_S fdFrame ={0};
    dir = opendir(IMAGE_DIR);
    if (dir == NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "%s is open fail\n",IMAGE_DIR);
        return -1;
    }
    rewinddir(dir);
    struct dirent * ptr = NULL;

    if (g_stFDIveHandle == NULL)
    {
        g_stFDIveHandle = CVI_IVE_CreateHandle();
    }
    for (int i = 0; i < 1000; i++)
    {
        if ((ptr = readdir(dir)) != NULL)
        { 
            if (strcmp( ptr->d_name, ".") == 0 || strcmp( ptr->d_name, "..") == 0)
            {
                continue;
            }
            APP_PROF_LOG_PRINT(LEVEL_INFO, "%s is registering\n", ptr->d_name);
            sprintf(image_path, "/mnt/sd/picture/%s", ptr->d_name);
            IVE_IMAGE_S image = CVI_IVE_ReadImage(g_stFDIveHandle, image_path, IVE_IMAGE_TYPE_U8C3_PACKAGE);
            if (image.u32Width == 0)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s Read image failed with !\n", ptr->d_name);
                goto REGISTER_EXIT;
            }


            ret = CVI_IVE_Image2VideoFrameInfo(&image, &fdFrame);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s Convert to video frame failed with %#x!\n", ptr->d_name , ret);
                goto REGISTER_EXIT;
            }

            memset(&face_test, 0, sizeof(cvtdl_face_t));
            ret = CVI_TDL_ScrFDFace(g_FDAiHandle, &fdFrame, &face_test);
            if (ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_TDL_ScrFDFace fail\n",ptr->d_name);
                goto REGISTER_EXIT;
            }

            if (face_test.size == 0)
            {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "%s picture not have face\n",ptr->d_name);
                goto REGISTER_EXIT;
            }
            ret = CVI_TDL_FaceRecognition(g_FDAiHandle, &fdFrame, &face_test);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_TDL_FaceRecognition fail \n",ptr->d_name);
                goto REGISTER_EXIT;
            }

            usleep(200);
            g_FaceCnt ++;

            for (CVI_U32 j = 0; j < g_FaceFeatureArray.data_num; j++)
            {
                for (CVI_U32 i = 0; i < g_FaceFeatureArray.feature_length; i++)
                {
                    if(j == g_FaceCnt)
                    {
                        ((int8_t *)g_FaceFeatureArray.ptr)[j * g_FaceFeatureArray.feature_length + i] =  face_test.info[0].feature.ptr[i];
                    }
                }
            }

            ret = CVI_TDL_Service_RegisterFeatureArray(g_FDAiServiceHandle, g_FaceFeatureArray, COS_SIMILARITY);
            if (ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_Service_RegisterFeatureArray fail \n");
                continue;
            }

            struct LinkList *node = (struct LinkList *) malloc(sizeof(struct LinkList));
            char *put;
            char image_name[128]={0};
            memset(image_name, 0, sizeof(image_name));
            put = strrchr(ptr->d_name, '.');
            strncpy(image_name, ptr->d_name, (strlen(ptr->d_name) - strlen(put)));
            node->index = g_FaceCnt;
            strcpy(node->name,image_name);
            g_FaceTail->next = node;
            node->next = NULL;
            g_FaceTail = node;
            APP_PROF_LOG_PRINT(LEVEL_INFO, "'%s' is registering face\n",g_FaceTail->name);
            APP_PROF_LOG_PRINT(LEVEL_INFO, "face register index is %d\n",g_FaceCnt);
REGISTER_EXIT:
            CVI_TDL_Free(&face_test);
            CVI_SYS_FreeI(g_stFDIveHandle, &image);
        }
    }

    closedir(dir);
    ptr = NULL;
    dir = NULL;

    return ret;
}

static  CVI_S32 app_ipcam_Face_Recognition(VIDEO_FRAME_INFO_S *pstFrame, cvtdl_face_t *pstFace , int face_idx)
{
    CVI_S32 ret = CVI_SUCCESS;

    CVI_FLOAT value;
    cvtdl_feature_t db_feature;
    db_feature.ptr = (int8_t *)malloc(g_FaceFeatureArray.feature_length);
    db_feature.size = g_FaceFeatureArray.feature_length;
    db_feature.type = TYPE_INT8;

    if (!g_pstFDCfg->CAPTURE_bEnable)
    {
        ret = CVI_TDL_FaceRecognitionOne(g_FDAiHandle, pstFrame, pstFace, face_idx);
        if (ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_FaceRecognitionOne fail \n");
            free(db_feature.ptr);
            return ret;
        }
    }

    for (CVI_U32 i = 0; i < g_FaceFeatureArray.data_num; i++)
    {
        for (CVI_U32 k = 0; k < g_FaceFeatureArray.feature_length; k++)
        {
            ((int8_t *)db_feature.ptr)[k]=((int8_t *)g_FaceFeatureArray.ptr)[i * g_FaceFeatureArray.feature_length + k];
        }

        CVI_TDL_Service_CalculateSimilarity(g_FDAiServiceHandle, &pstFace->info[face_idx].feature, &db_feature, &value);
        if(value > 0.6)
        {
            struct LinkList *display;
            display=g_FaceHead;
            while (display->next != NULL)
            {
                if(display->next->index== (int)i)
                {
                    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "face_name:%s\n",display->next->name);
                    strcpy(pstFace->info[face_idx].name,display->next->name);
                    break;
                }
                display = display->next;
            }
            APP_PROF_LOG_PRINT(LEVEL_DEBUG,    "index: %d\n",i);
            APP_PROF_LOG_PRINT(LEVEL_DEBUG,    "value %4.2f\n",value);
        }
    }

    free(db_feature.ptr);    
    return ret;
}

static CVI_S32 app_ipcam_Ai_FD_Proc_Init(CVI_VOID)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI FD init ------------------> start \n");

    CVI_S32 s32Ret = CVI_SUCCESS;

    app_ipcam_Ai_Fd_Param_dump();

    /* 1.Creat Handle And  Service Handle */
    if (g_FDAiHandle == NULL)
    {
        s32Ret = CVI_TDL_CreateHandle(&g_FDAiHandle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_FD_CreateHandle failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_FD_CreateHandle has created\n");
        return s32Ret;
    }

    if (g_FDAiServiceHandle == NULL)
    {
        s32Ret = CVI_TDL_Service_CreateHandle(&g_FDAiServiceHandle, g_FDAiHandle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_FD_CreateHandle failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_FD_Service_CreateHandle has created\n");
        return s32Ret;
    }

    s32Ret = app_ipcam_Ai_InferenceFunc_Get(g_pstFDCfg->model_id_fd);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "unsupported model id: %d \n", g_pstFDCfg->model_id_fd);
        return s32Ret;
    }

    /* 2.Set Modle Path And threshold */
    //scrfd
    if (g_pstFDCfg->CAPTURE_bEnable)
    {
        s32Ret = app_ipcam_Ai_Face_Capture_Init(&g_FDAiHandle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Face Capture init failed with %#x!\n", s32Ret);
        }
    }
    else
    {
        s32Ret = CVI_TDL_OpenModel(g_FDAiHandle, g_pstFDCfg->model_id_fd, g_pstFDCfg->model_path_fd);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_TDL_SetModelPath failed with %#x!\n", g_pstFDCfg->model_path_fd, s32Ret);
            return s32Ret;
        }

        s32Ret = CVI_TDL_SetModelThreshold(g_FDAiHandle,  g_pstFDCfg->model_id_fd, g_pstFDCfg->threshold_fd);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_TDL_SetModelThreshold failed with %#x!\n", g_pstFDCfg->model_path_fd, s32Ret);
            return s32Ret;
        }
    }

    /* 3. Set Skip Vpss Preprocess And Timeout*/

    CVI_BOOL bSkip = g_pstFDCfg->bVpssPreProcSkip;
    s32Ret = CVI_TDL_SetSkipVpssPreprocess(g_FDAiHandle,  g_pstFDCfg->model_id_fd, bSkip);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Fd Detection Not Support Skip VpssPreprocess %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_TDL_SetVpssTimeout(g_FDAiHandle, 2000);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Fd Set vpss timeout failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    /* 4.Attach Vb POOL For FD Function */
    s32Ret = CVI_TDL_SetVBPool(g_FDAiHandle, 0, g_pstFDCfg->FdPoolId);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Fd Set vpss vbpool failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    if (g_pstFDCfg->FR_bEnable)
    {
        /* 5.Face Recognition function */
        s32Ret = app_ipcam_Ai_FR_Init();
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Ai_FR_Init init failed with %#x!\n", s32Ret);
        }

        s32Ret = app_ipcam_Face_Registeration();
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Face regestering failed with %#x!\n", s32Ret);
        }
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI FD init ------------------> done \n");
    return CVI_SUCCESS;
}

static CVI_VOID *Thread_FD_PROC(CVI_VOID *arg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI FD start running!\n");
    prctl(PR_SET_NAME, "Thread_FD_PROC");

    VPSS_GRP VpssGrp = g_pstFDCfg->VpssGrp;
    VPSS_CHN VpssChn = g_pstFDCfg->VpssChn;

    VIDEO_FRAME_INFO_S stfdFrame = {0};
    cvtdl_face_t face;

    CVI_U32 fd_frame = 0;
    CVI_S32 iTime_start, iTime_proc,iTime_fps;
    float iTime_gop;
    iTime_start = GetCurTimeInMsec();

    while (app_ipcam_Ai_FD_ProcStatus_Get()) {
        pthread_mutex_lock(&g_FDStatusMutex);
        s32Ret = app_ipcam_Ai_FD_Pause_Get();

        if (s32Ret) {
            pthread_mutex_unlock(&g_FDStatusMutex);
            usleep(1000*1000);
            continue;
        }
        /* 1.Get frame */
        s32Ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stfdFrame, 3000);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) get frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
            pthread_mutex_unlock(&g_FDStatusMutex);
            usleep(100*1000);
            continue;
        }
        pthread_mutex_unlock(&g_FDStatusMutex);
        iTime_proc = GetCurTimeInMsec();
        /* 2. Face Detect*/
        memset(&face, 0, sizeof(cvtdl_face_t));

        if (g_pstFDCfg->CAPTURE_bEnable)
        {
            s32Ret=app_ipcam_Ai_Face_Capture(&stfdFrame,&face);
            if (s32Ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_Ai_Face_Capture fail \n");
                goto FD_FAILURE;
            }
        }
        else
        {
            s32Ret = g_pfpFDInference(g_FDAiHandle, &stfdFrame, g_pstFDCfg->model_id_fd, &face);
            if (s32Ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_TDL_ScrFDFace fail \n");
                goto FD_FAILURE;
            }
        }

        for (uint32_t i = 0; i < face.size; i++)
        {
            if (face.info[i].mask_score >= g_pstFDCfg->threshold_mask)
            {
                strncpy(face.info[i].name, "Wearing", sizeof(face.info[i].name));
            }
            else 
            {
                if (g_pstFDCfg->FR_bEnable)
                {
                    s32Ret = app_ipcam_Face_Recognition(&stfdFrame, &face, i);
                    if (s32Ret != CVI_SUCCESS)
                    {
                        APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_Face_Recognition fail \n");
                        goto FD_FAILURE;
                    }
                }
            }
        }

        /* 3.add face funtion*/
        if (g_pstFDCfg->FACE_AE_bEnable)
        {
            app_ipcam_Ai_FD_AEStart(&stfdFrame, &face);
        }

        g_FDProc = GetCurTimeInMsec() - iTime_proc;
    FD_FAILURE:
        /* 4.Release resources and assign values for cvtdl_face_t */
        s32Ret = CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stfdFrame);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) release frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
        }

        fd_frame ++;
        iTime_fps = GetCurTimeInMsec();
        iTime_gop = (float)(iTime_fps - iTime_start)/1000;
        if(iTime_gop >= 1)
        {
            g_FDFps = fd_frame/iTime_gop;
            fd_frame = 0;
            iTime_start = iTime_fps;
        }

        if (face.size == 0)
        {
            if (!g_pstFDCfg->CAPTURE_bEnable)
            {
                CVI_TDL_Free(&face);
            }
            continue;
        }

        SMT_MutexAutoLock(g_FDMutex, lock);
        if (g_stFDObjDraw.info)
        {
            CVI_TDL_Free(&g_stFDObjDraw);
        }
        memset(&g_stFDObjDraw, 0, sizeof(cvtdl_face_t));
        memcpy(&g_stFDObjDraw, &face, sizeof face);
        g_stFDObjDraw.info = (cvtdl_face_info_t *)malloc(face.size * sizeof(cvtdl_face_info_t));
        memset(g_stFDObjDraw.info, 0, sizeof(cvtdl_face_info_t) * g_stFDObjDraw.size);
        for (CVI_U32 i = 0; i < face.size; i++)
        {
            CVI_TDL_CopyInfo(&face.info[i], &g_stFDObjDraw.info[i]);
            // APP_PROF_LOG_PRINT(LEVEL_INFO, "Face obj[%d] hardhat_score: %1.2f \n", i, face.info[i].hardhat_score);
        }

        if (!g_pstFDCfg->CAPTURE_bEnable)
        {
            CVI_TDL_Free(&face);
        }
    }

    pthread_exit(NULL);

    return NULL;
}

int app_ipcam_Ai_FD_ObjDrawInfo_Get(cvtdl_face_t *pstAiObj)
{
    _NULL_POINTER_CHECK_(pstAiObj, -1);

    SMT_MutexAutoLock(g_FDMutex, lock);

    if (g_stFDObjDraw.size == 0)
    {
        return CVI_SUCCESS;
    }
    else
    {
        memcpy(pstAiObj, &g_stFDObjDraw, sizeof g_stFDObjDraw);
        pstAiObj->info = (cvtdl_face_info_t *)malloc(g_stFDObjDraw.size * sizeof(cvtdl_face_info_t));
        _NULL_POINTER_CHECK_(pstAiObj->info, -1);
        memset(pstAiObj->info, 0, sizeof(cvtdl_face_info_t) * g_stFDObjDraw.size);
        for (CVI_U32 i = 0; i < g_stFDObjDraw.size; i++)
        {
            CVI_TDL_CopyInfo(&g_stFDObjDraw.info[i], &pstAiObj->info[i]);
        }
        CVI_TDL_Free(&g_stFDObjDraw);
    }

    return CVI_SUCCESS;
}

int app_ipcam_Ai_FD_Rect_Draw(VIDEO_FRAME_INFO_S *pstVencFrame)
{
    CVI_S32 iTime;

    if (app_ipcam_Ai_FD_ProcStatus_Get())
    {
        SMT_MutexAutoLock(g_FDMutex, lock);
        iTime = GetCurTimeInMsec();
        CVI_TDL_RescaleMetaRB(pstVencFrame, &g_stFDObjDraw);
        CVI_TDL_Service_FaceDrawRect(
                                g_FDAiServiceHandle,
                                &g_stFDObjDraw,
                                pstVencFrame,
                                CVI_TRUE,
                                g_pstFDCfg->rect_brush);

        CVI_TDL_Free(&g_stFDObjDraw);
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "FD draw result takes %u ms \n", (GetCurTimeInMsec() - iTime));
    }

    return CVI_SUCCESS;
}

int app_ipcam_Ai_FD_Stop(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstFDCfg->FD_bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI FD not enable\n");
        return CVI_SUCCESS;
    }

    if (!app_ipcam_Ai_FD_ProcStatus_Get())
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI FD has not running!\n");
        return s32Ret;
    }

    app_ipcam_Ai_FD_ProcStatus_Set(CVI_FALSE);

    CVI_S32 iTime = GetCurTimeInMsec();

    if (g_FDThreadHandle)
    {
        pthread_join(g_FDThreadHandle, NULL);
        g_FDThreadHandle = 0;
    }

    if (g_pstFDCfg->CAPTURE_bEnable)
    {
        app_ipcam_Ai_Face_Capture_Stop();
    }

    if (g_pstFDCfg->FR_bEnable)
    {
        free(g_FaceFeatureArray.ptr);
        g_FaceFeatureArray.ptr = NULL;
        App_Clean_List(g_FaceHead);
        g_FaceHead = NULL;
        g_FaceTail = NULL;
        if (g_stFDIveHandle)
        {
            CVI_IVE_DestroyHandle(g_stFDIveHandle);
            g_stFDIveHandle = NULL;
        }
        g_FaceCnt = 0;
    }

    s32Ret = CVI_TDL_Service_DestroyHandle(g_FDAiServiceHandle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_FD_Service_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        g_FDAiServiceHandle = NULL;
    }

    s32Ret = CVI_TDL_DestroyHandle(g_FDAiHandle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_FD_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        g_FDAiHandle = NULL;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI FD Thread exit takes %u ms\n", (GetCurTimeInMsec() - iTime));

    return CVI_SUCCESS;
}


int app_ipcam_Ai_FD_Start(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstFDCfg->FD_bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI FD not enable\n");
        return CVI_SUCCESS;
    }

    if (g_bFDRunning)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI FD has started\n");
        return CVI_SUCCESS;
    }

    s32Ret = app_ipcam_Ai_FD_Proc_Init();
	if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Ai_Fd_Proc_Init failed!\n");
        return s32Ret;
    }

    app_ipcam_Ai_FD_ProcStatus_Set(CVI_TRUE);

    s32Ret = pthread_create(&g_FDThreadHandle, NULL, Thread_FD_PROC, NULL);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "AI Fd_pthread_create failed!\n");
        return s32Ret;
    }

    return CVI_SUCCESS;
}
