#include <stdio.h>
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "app_ipcam_ai.h"
#include "turbojpeg.h"

 /**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/

#define FACE_CAPTURE_FILENAME         "/mnt/sd/capture_face_%"PRIu64"_%f.jpg"
#define MAIN_CAPTURE_FILENAME         "/mnt/sd/capture_main_%"PRIu64"_%f.png"
#define FACE_CAPTURE_SIZE            (15)
#define FACE_CAPTURE_STABLE_TIME     (15)        /*face stable time(fps) start capture*/
#define FACE_UNMATCHED_NUM           (15)

#define FACE_SCORE_THRESHOLD         (70)
#define MAX_STRING_LEN               (255)

static cvitdl_app_handle_t g_stFaceCaptureHandle = NULL;
static face_capture_config_t g_stFaceCaptureCfg =
{
    .thr_size_min = 20,             /*face size less than it, quality = 0*/
    .thr_size_max = 1024,           /*face size more than it, quality = 0*/
    .qa_method = 0,                 /*use fq?*/
    .thr_quality = 0.1,             /*capture new face quality difference*/
    .thr_yaw = 0.75,                /*angle more than it, quality = 0*/
    .thr_pitch = 0.75,              /*angle more than it, quality = 0*/
    .thr_roll = 0.75,               /*angle more than it, quality = 0*/
    .thr_laplacian = 20,
    .miss_time_limit = 40,          /*face leave time(fps)*/
    .m_interval = 25,               /*capture time(fps)*/
    .m_capture_num = 5,             /*how many face capture */
    .auto_m_fast_cap = 0,           /*auto mode fase capture?*/
    .store_feature = 0,			    /*store face feature?*/
    .img_capture_flag = 0,          /*captre 0:face_frame, 1:face_frame+full_frame, 2:half_body_frame 3:half_body_frame+full_frame*/
    .eye_dist_thresh = 10,          /*pupillary distance*/
    .landmark_score_thresh = 0.5,   /*landmark_score_thresh*/
    .venc_channel_id = 2,           /*when img_capture_flag > 0, chose which Venc chn to encode full frame*/
};

CVI_S32 app_ipcam_Ai_Face_Capture_Init(cvitdl_handle_t *handle)
{
        CVI_S32 s32Ret = CVI_SUCCESS;

        APP_PARAM_AI_FD_CFG_S *FdParam = app_ipcam_Ai_FD_Param_Get();
        s32Ret |= CVI_TDL_APP_CreateHandle(&g_stFaceCaptureHandle, *(handle));
        s32Ret |= CVI_TDL_APP_FaceCapture_Init(g_stFaceCaptureHandle, FACE_CAPTURE_SIZE);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "set FaceCapture failed with %#x!\n", s32Ret);
            return s32Ret;
        }

        s32Ret |= CVI_TDL_APP_FaceCapture_QuickSetUp(g_stFaceCaptureHandle, FdParam->model_id_fd, FdParam->model_id_fr, FdParam->model_path_fd,
                                                  NULL, NULL,FdParam->model_path_capture);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_TDL_APP_FaceCapture_QuickSetUp failed with %#x!\n", s32Ret);
            return s32Ret;
        }

        CVI_TDL_SetModelThreshold(*(handle), FdParam->model_id_fd, FdParam->threshold_fd);
        CVI_TDL_APP_FaceCapture_SetMode(g_stFaceCaptureHandle, AUTO);

        g_stFaceCaptureCfg.thr_size_max = FdParam->thr_size_max;
        g_stFaceCaptureCfg.thr_size_min = FdParam->thr_size_min;
        g_stFaceCaptureCfg.thr_laplacian = FdParam->thr_laplacian;

        g_stFaceCaptureHandle->face_cpt_info->fd_inference = CVI_TDL_ScrFDFace;
        g_stFaceCaptureHandle->face_cpt_info->fr_inference = CVI_TDL_FaceRecognition;

        // if (g_pstFdCfg->FR_bEnable)
        // {
        //     g_stFaceCaptureHandle->face_cpt_info->do_FR = 1;
        // }

        CVI_TDL_APP_FaceCapture_SetConfig(g_stFaceCaptureHandle, &g_stFaceCaptureCfg);
        return s32Ret;
}

CVI_S32 app_ipcam_Ai_Face_Capture(VIDEO_FRAME_INFO_S *stfdFrame,cvtdl_face_t *capture_face)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_U32 i = 0;

    FILE *fp = NULL;
    tjhandle tj_handle = NULL;
    unsigned char *outjpg_buf=NULL;
    int flags = 0;
    int subsamp = TJSAMP_422;
    int pixelfmt = TJPF_RGB;
    int jpegQual = 80;
    unsigned long outjpg_size;

    s32Ret = CVI_TDL_APP_FaceCapture_Run(g_stFaceCaptureHandle, stfdFrame);
    if (NULL == g_stFaceCaptureHandle->face_cpt_info) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "face_cpt_info is NULL\n");
         return 0;
    }
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_TDL_APP_FaceCapture_Run fail \n");
        return s32Ret;
    }

    memcpy(capture_face, &g_stFaceCaptureHandle->face_cpt_info->last_faces, sizeof(cvtdl_face_t));

    if (NULL == g_stFaceCaptureHandle->face_cpt_info->data)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "g_stFaceCaptureHandle->face_cpt_info->data[%d] is NULL\n", i);
        return 0;
    }
    for (i = 0; i < g_stFaceCaptureHandle->face_cpt_info->size; i++)
    {
        if (!g_stFaceCaptureHandle->face_cpt_info->_output[i])
            continue;
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "unique_id:%"PRIu64" imgae[w:%u h:%u stride:(%u %u %u) quality:%f score:%f length:(%u %u %u)]\n",
        g_stFaceCaptureHandle->face_cpt_info->data[i].info.unique_id,
        g_stFaceCaptureHandle->face_cpt_info->data[i].image.width, g_stFaceCaptureHandle->face_cpt_info->data[i].image.height, 
        g_stFaceCaptureHandle->face_cpt_info->data[i].image.stride[0], g_stFaceCaptureHandle->face_cpt_info->data[i].image.stride[1], g_stFaceCaptureHandle->face_cpt_info->data[i].image.stride[2],
        g_stFaceCaptureHandle->face_cpt_info->data[i].info.face_quality, g_stFaceCaptureHandle->face_cpt_info->data[i].info.bbox.score, 
        g_stFaceCaptureHandle->face_cpt_info->data[i].image.length[0], g_stFaceCaptureHandle->face_cpt_info->data[i].image.length[1], g_stFaceCaptureHandle->face_cpt_info->data[i].image.length[2]);
        CVI_CHAR captureFileName[MAX_STRING_LEN];
        memset(captureFileName, 0, sizeof(captureFileName));
        snprintf(captureFileName, sizeof(captureFileName), FACE_CAPTURE_FILENAME ,
        g_stFaceCaptureHandle->face_cpt_info->data[i].info.unique_id, g_stFaceCaptureHandle->face_cpt_info->data[i].info.face_quality);

        tj_handle = tjInitCompress();
        if (NULL == tj_handle){
            goto jpeg_exit;
        }

        int ret = tjCompress2(tj_handle, g_stFaceCaptureHandle->face_cpt_info->data[i].image.pix[0],g_stFaceCaptureHandle->face_cpt_info->data[i].image.width,0, 
        g_stFaceCaptureHandle->face_cpt_info->data[i].image.height,pixelfmt,&outjpg_buf,&outjpg_size,subsamp,jpegQual, flags);
        if (0 != ret) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "tjCompress2 fail\n");
            goto jpeg_exit;
        }

        fp = fopen(captureFileName, "w");
        if(NULL == fp)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s fopen fail\n",captureFileName);
            goto jpeg_exit;
        }
        fwrite(outjpg_buf,1,outjpg_size,fp);
            
    jpeg_exit:
        if(NULL != fp)
        {
            fclose(fp);
            fp = NULL;
        }
        if(NULL != tj_handle)
        {
            tjDestroy(tj_handle);
        }
    }
    return s32Ret;
}

CVI_S32 app_ipcam_Ai_Face_Capture_Stop(void)
{
    CVI_S32 ret = CVI_SUCCESS;
    ret=CVI_TDL_APP_DestroyHandle(g_stFaceCaptureHandle);
    if(ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_TDL_APP_DestroyHandle fail \n");
        return ret;
    }
    g_stFaceCaptureHandle = NULL;
    return ret;
}

