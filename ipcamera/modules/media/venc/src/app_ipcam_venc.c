
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include "cvi_vpss.h"
#include "errno.h"
#include "linux/cvi_type.h"
#include "cvi_ae.h"
#include "app_ipcam_comm.h"
#include "app_ipcam_sys.h"
#include "app_ipcam_vi.h"
#include "app_ipcam_vpss.h"
#include "app_ipcam_venc.h"

#include "app_ipcam_osd.h"
#include "app_ipcam_ll.h"
#ifdef RTSP_SUPPORT
#include "app_ipcam_rtsp.h"
#endif
#ifdef MD_SUPPORT
#include "app_ipcam_md.h"
#endif

#ifdef AI_SUPPORT
#include "app_ipcam_ai.h"
#endif

#ifdef WEB_SOCKET
#include "app_ipcam_websocket.h"
#endif

#ifdef RECORD_SUPPORT
#include "app_ipcam_record.h"
#endif

#ifdef CVI_UVC_SUPPORT
#include "cvi_uvc.h"
#endif

#ifdef STITCH_SUPPORT
#include "cvi_stitch.h"
#include <semaphore.h>
#include "app_ipcam_stitch.h"
#endif


#define P_MAX_SIZE (512 * 1024) //P oversize 512K lost it
/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
#define H26X_MAX_NUM_PACKS      8
#define JPEG_MAX_NUM_PACKS      1

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
APP_PARAM_VENC_CTX_S g_stVencCtx, *g_pstVencCtx = &g_stVencCtx;

static pthread_t g_Venc_pthread[VENC_CHN_MAX];

static CVI_BOOL bJpgCapFlag = CVI_FALSE;
pthread_cond_t JpgCapCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t JpgCapMutex = PTHREAD_MUTEX_INITIALIZER;

static void *g_pDataCtx[VENC_CHN_MAX];

CVI_S32 g_s32SwitchSizeiTime;
static pthread_once_t venc_remap_once = PTHREAD_ONCE_INIT;

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

#ifdef __CV186AH__
static CVI_S32 app_ipcam_Venc_Get_Frame(APP_VENC_CHN_CFG_S *pstVencChnCfg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    VENC_CHN VencChn = pstVencChnCfg->VencChn;
    MOD_ID_E src_mod_id = pstVencChnCfg->stVencFrameSourceCfg.enModId;
    CVI_S32 src_dev_id = pstVencChnCfg->stVencFrameSourceCfg.s32DevId;
    CVI_S32 src_chn_id = pstVencChnCfg->stVencFrameSourceCfg.s32ChnId;
    VIDEO_FRAME_INFO_S stVencFrame = {0};
#ifdef STITCH_SUPPORT
    APP_PARAM_STITCH_CFG_S *pstStitchCfg= (APP_PARAM_STITCH_CFG_S *)app_ipcam_Stitch_Param_Get();
#endif

    /*Frame source from CVI_ID_VPSS*/
    if (src_mod_id == CVI_ID_VPSS) {
        s32Ret = CVI_VPSS_GetChnFrame(src_dev_id, src_chn_id, &stVencFrame, 3000);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(
                    LEVEL_ERROR,
                    "CVI_VPSS_GetChnFrame grp%d chn%d failed with %#x\n",
                    src_dev_id, src_chn_id, s32Ret);
            return s32Ret;
        }
    }

    /*Frame source from CVI_ID_STITCH*/
#ifdef STITCH_SUPPORT
    if (src_mod_id == CVI_ID_STITCH) {
        if (pstStitchCfg->Enable) {
            s32Ret = CVI_STITCH_GetChnFrame(&stVencFrame, 3000);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_STITCH_GetChnFrame failed with %#x\n", s32Ret);

                /*Increase semv to wake up CVI_STITCH_SendFrame*/
                for (int i = 0; i < (int)pstStitchCfg->srcNum; i++) {
                    sem_post(pstStitchCfg->srcParam[i].semv);
                }
                return s32Ret;
            }

            /*Increase semv to wake up CVI_STITCH_SendFrame*/
            for (int i = 0; i < (int)pstStitchCfg->srcNum; i++) {
                sem_post(pstStitchCfg->srcParam[i].semv);
            }
        }
    }
#endif

    if (stVencFrame.stVFrame.u64PhyAddr[0]) {
        s32Ret = CVI_VENC_SendFrame(VencChn, &stVencFrame, 3000);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VENC_SendFrame failed! %d\n", s32Ret);
        }
    }

    /*Frame release by CVI_ID_VPSS*/
    if (src_mod_id == CVI_ID_VPSS) {
        CVI_VPSS_ReleaseChnFrame(src_dev_id, src_chn_id, &stVencFrame);
    }

    /*Frame release by CVI_ID_STITCH*/
#ifdef STITCH_SUPPORT
    if (src_mod_id == CVI_ID_STITCH) {
        if (pstStitchCfg->Enable) {
            CVI_STITCH_ReleaseChnFrame(&stVencFrame);
        }
    }
#endif

    return s32Ret;
}
#endif

APP_PARAM_VENC_CTX_S *app_ipcam_Venc_Param_Get(void)
{
    return g_pstVencCtx;
}

APP_VENC_CHN_CFG_S *app_ipcam_VencChnCfg_Get(VENC_CHN VencChn)
{
    APP_VENC_CHN_CFG_S *pstVencChnCfg = &g_pstVencCtx->astVencChnCfg[VencChn];

    return pstVencChnCfg;
}

int app_ipcam_Venc_RefPara_Set(VENC_CHN VencChn, APP_REF_PARAM_S *pstRefCfg)
{
    VENC_REF_PARAM_S stRefParam, *pstRefParam = &stRefParam;

    APP_CHK_RET(CVI_VENC_GetRefParam(VencChn, pstRefParam), "get ref param");

    if (pstRefCfg->tempLayer == 2) {
        pstRefParam->u32Base = 1;
        pstRefParam->u32Enhance = 1;
        pstRefParam->bEnablePred = CVI_TRUE;
    } else if (pstRefCfg->tempLayer == 3) {
        pstRefParam->u32Base = 2;
        pstRefParam->u32Enhance = 1;
        pstRefParam->bEnablePred = CVI_TRUE;
    } else {
        pstRefParam->u32Base = 0;
        pstRefParam->u32Enhance = 0;
        pstRefParam->bEnablePred = CVI_TRUE;
    }

    APP_CHK_RET(CVI_VENC_SetRefParam(VencChn, pstRefParam), "set ref param");

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Venc_CuPrediction_Set(VENC_CHN VencChn, APP_CU_PREDI_PARAM_S *pstCuPrediCfg)
{
    VENC_CU_PREDICTION_S stCuPrediction, *pstCuPrediction = &stCuPrediction;

    APP_CHK_RET(CVI_VENC_GetCuPrediction(VencChn, pstCuPrediction), "get CU Prediction");

    pstCuPrediction->u32IntraCost = pstCuPrediCfg->u32IntraCost;

    APP_CHK_RET(CVI_VENC_SetCuPrediction(VencChn, pstCuPrediction), "set CU Prediction");

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Venc_ReEncode_Close(void)
{
    return 0;
}

CVI_S32 app_ipcam_Venc_FrameLost_Set(VENC_CHN VencChn, APP_FRAMELOST_PARAM_S *pFrameLostCfg)
{
    VENC_FRAMELOST_S stFL, *pstFL = &stFL;

    APP_CHK_RET(CVI_VENC_GetFrameLostStrategy(VencChn, pstFL), "get FrameLost Strategy");

    pstFL->bFrmLostOpen = (pFrameLostCfg->bframeLosten) == 1 ? CVI_TRUE : CVI_FALSE;
    pstFL->enFrmLostMode = pFrameLostCfg->enFrmLostMode;
    pstFL->u32EncFrmGaps = pFrameLostCfg->u32frameLostGap;
    pstFL->u32FrmLostBpsThr = pFrameLostCfg->u32frameLostBspThr;

    APP_PROF_LOG_PRINT(LEVEL_TRACE,"bFrmLostOpen=%d enFrmLostMode=%d u32EncFrmGaps=%d u32FrmLostBpsThr=%d\n",
        pstFL->bFrmLostOpen, pstFL->enFrmLostMode, pstFL->u32EncFrmGaps, pstFL->u32FrmLostBpsThr);
    APP_CHK_RET(CVI_VENC_SetFrameLostStrategy(VencChn, pstFL), "set FrameLost Strategy");

    return CVI_SUCCESS;
}

#if defined(__CV186AH__)
#else
CVI_S32 app_ipcam_Venc_SvcParam_Set(VENC_CHN VencChn, VENC_RC_MODE_E enRcMode, APP_SVC_PARAM_S *pSvcCfg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    //only avbr support svc
    if((enRcMode != VENC_RC_MODE_H264AVBR) && (enRcMode != VENC_RC_MODE_H265AVBR)) {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "Rcmode not support svc\n");
        return s32Ret;
    }

    s32Ret = CVI_VENC_EnableSvc(VencChn, pSvcCfg->bsvc_enable);
	if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Svc enable failed!\n");
		return s32Ret;
	}

	VENC_SVC_PARAM_S stSvcParam, *pstSvcParam = &stSvcParam;

	s32Ret = CVI_VENC_GetSvcParam(VencChn, pstSvcParam);
	if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "GetSvcParam failed!\n");
		return s32Ret;
	}
    
    pstSvcParam->fg_protect_en = pSvcCfg->bfg_protect_en;
	pstSvcParam->fg_dealt_qp = pSvcCfg->s32fg_dealt_qp;
	if (pSvcCfg->bcomplex_scene_detect_en) {
		pstSvcParam->complex_scene_detect_en = pSvcCfg->bcomplex_scene_detect_en;
		if(pSvcCfg->u32complex_scene_hight_th > pSvcCfg->u32complex_scene_low_th &&
		  pSvcCfg->u32complex_scene_low_th) {
			pstSvcParam->complex_scene_hight_th = pSvcCfg->u32complex_scene_hight_th;
			pstSvcParam->complex_scene_low_th = pSvcCfg->u32complex_scene_low_th;
		}
        if(pSvcCfg->u32complex_min_percent > pSvcCfg->u32middle_min_percent &&
		  pSvcCfg->u32middle_min_percent) {
			pstSvcParam->middle_min_percent = pSvcCfg->u32middle_min_percent;
			pstSvcParam->complex_min_percent = pSvcCfg->u32complex_min_percent;
		}
	}
	pstSvcParam->smart_ai_en = pSvcCfg->bsmart_ai_en;
	s32Ret = CVI_VENC_SetSvcParam(VencChn, pstSvcParam);
	if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "SetSvcParam failed!\n");
		return s32Ret;
	}

    APP_PROF_LOG_PRINT(LEVEL_TRACE, "bsvc_enable=%d bsmart_ai_en=%d bcomplex_scene_detect_en=%d bfg_protect_en=%d\n",
        pSvcCfg->bsvc_enable, pSvcCfg->bsmart_ai_en, pSvcCfg->bcomplex_scene_detect_en, pSvcCfg->bfg_protect_en);

    return s32Ret;
}
#endif

CVI_S32 app_ipcam_Venc_H264Trans_Set(VENC_CHN VencChn)
{
    VENC_H264_TRANS_S h264Trans = { 0 };

    APP_H264_TRANS_PARAM_S stH264TransParam;
    memset(&stH264TransParam, 0, sizeof(APP_H264_TRANS_PARAM_S));
    APP_CHK_RET(CVI_VENC_GetH264Trans(VencChn, &h264Trans), "get H264 trans");
    stH264TransParam.h264ChromaQpOffset = 0;

    h264Trans.chroma_qp_index_offset = stH264TransParam.h264ChromaQpOffset;

    APP_CHK_RET(CVI_VENC_SetH264Trans(VencChn, &h264Trans), "set H264 trans");

    return CVI_SUCCESS;
}


CVI_S32 app_ipcam_Venc_H264Entropy_Set(VENC_CHN VencChn)
{
    APP_VENC_CHN_CFG_S *pstVencChnCfg = &g_pstVencCtx->astVencChnCfg[VencChn];
    VENC_H264_ENTROPY_S h264Entropy = { 0 };
    switch (pstVencChnCfg->u32Profile) {
        case H264E_PROFILE_BASELINE:
            h264Entropy.u32EntropyEncModeI = H264E_ENTROPY_CAVLC;
            h264Entropy.u32EntropyEncModeP = H264E_ENTROPY_CAVLC;
            break;
        default:
            h264Entropy.u32EntropyEncModeI = H264E_ENTROPY_CABAC;
            h264Entropy.u32EntropyEncModeP = H264E_ENTROPY_CABAC;
            break;
    }

    APP_CHK_RET(CVI_VENC_SetH264Entropy(VencChn, &h264Entropy), "set H264 entropy");

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Venc_H264Vui_Set(VENC_CHN VencChn)
{
    VENC_H264_VUI_S h264Vui = { 0 };
    APP_VENC_CHN_CFG_S *pstVencChnCfg = &g_pstVencCtx->astVencChnCfg[VencChn];

    APP_H264_VUI_PARAM_S stH264VuiParam;
    memset(&stH264VuiParam, 0, sizeof(APP_H264_VUI_PARAM_S));
    APP_CHK_RET(CVI_VENC_GetH264Vui(VencChn, &h264Vui), "get H264 Vui");
    stH264VuiParam.aspectRatioInfoPresentFlag   = CVI_H26X_ASPECT_RATIO_INFO_PRESENT_FLAG_DEFAULT;
    stH264VuiParam.aspectRatioIdc               = CVI_H26X_ASPECT_RATIO_IDC_DEFAULT;
    stH264VuiParam.overscanInfoPresentFlag      = CVI_H26X_OVERSCAN_INFO_PRESENT_FLAG_DEFAULT;
    stH264VuiParam.overscanAppropriateFlag      = CVI_H26X_OVERSCAN_APPROPRIATE_FLAG_DEFAULT;
    stH264VuiParam.sarWidth                     = CVI_H26X_SAR_WIDTH_DEFAULT;
    stH264VuiParam.sarHeight                    = CVI_H26X_SAR_HEIGHT_DEFAULT;
    stH264VuiParam.fixedFrameRateFlag           = CVI_H264_FIXED_FRAME_RATE_FLAG_DEFAULT;
    stH264VuiParam.videoSignalTypePresentFlag   = CVI_H26X_VIDEO_SIGNAL_TYPE_PRESENT_FLAG_DEFAULT;
    stH264VuiParam.videoFormat                  = CVI_H26X_VIDEO_FORMAT_DEFAULT;
    stH264VuiParam.videoFullRangeFlag           = CVI_H26X_VIDEO_FULL_RANGE_FLAG_DEFAULT;
    stH264VuiParam.colourDescriptionPresentFlag = CVI_H26X_COLOUR_DESCRIPTION_PRESENT_FLAG_DEFAULT;
    stH264VuiParam.colourPrimaries              = CVI_H26X_COLOUR_PRIMARIES_DEFAULT;
    stH264VuiParam.transferCharacteristics      = CVI_H26X_TRANSFER_CHARACTERISTICS_DEFAULT;
    stH264VuiParam.matrixCoefficients           = CVI_H26X_MATRIX_COEFFICIENTS_DEFAULT;

    stH264VuiParam.timingInfoPresentFlag = 1;
#if defined(__CV186AH__)
    stH264VuiParam.numUnitsInTick = 1000;
#else
    stH264VuiParam.numUnitsInTick = 1;
#endif

    h264Vui.stVuiAspectRatio.aspect_ratio_info_present_flag = stH264VuiParam.aspectRatioInfoPresentFlag;
    if (h264Vui.stVuiAspectRatio.aspect_ratio_info_present_flag) {
        h264Vui.stVuiAspectRatio.aspect_ratio_idc = stH264VuiParam.aspectRatioIdc;
        h264Vui.stVuiAspectRatio.sar_width = stH264VuiParam.sarWidth;
        h264Vui.stVuiAspectRatio.sar_height = stH264VuiParam.sarHeight;
    }

    h264Vui.stVuiAspectRatio.overscan_info_present_flag = stH264VuiParam.overscanInfoPresentFlag;
    if (h264Vui.stVuiAspectRatio.overscan_info_present_flag) {
        h264Vui.stVuiAspectRatio.overscan_appropriate_flag = stH264VuiParam.overscanAppropriateFlag;
    }

    h264Vui.stVuiTimeInfo.timing_info_present_flag = stH264VuiParam.timingInfoPresentFlag;
    if (h264Vui.stVuiTimeInfo.timing_info_present_flag) {
        h264Vui.stVuiTimeInfo.fixed_frame_rate_flag = stH264VuiParam.fixedFrameRateFlag;
        h264Vui.stVuiTimeInfo.num_units_in_tick = stH264VuiParam.numUnitsInTick;
        //264 fps = time_scale / (2 * num_units_in_tick)
        h264Vui.stVuiTimeInfo.time_scale = pstVencChnCfg->u32DstFrameRate * 2 * stH264VuiParam.numUnitsInTick;
    }

    h264Vui.stVuiVideoSignal.video_signal_type_present_flag = stH264VuiParam.videoSignalTypePresentFlag;
    if (h264Vui.stVuiVideoSignal.video_signal_type_present_flag) {
        h264Vui.stVuiVideoSignal.video_format = stH264VuiParam.videoFormat;
        h264Vui.stVuiVideoSignal.video_full_range_flag = stH264VuiParam.videoFullRangeFlag;
        h264Vui.stVuiVideoSignal.colour_description_present_flag = stH264VuiParam.colourDescriptionPresentFlag;
        if (h264Vui.stVuiVideoSignal.colour_description_present_flag) {
            h264Vui.stVuiVideoSignal.colour_primaries = stH264VuiParam.colourPrimaries;
            h264Vui.stVuiVideoSignal.transfer_characteristics = stH264VuiParam.transferCharacteristics;
            h264Vui.stVuiVideoSignal.matrix_coefficients = stH264VuiParam.matrixCoefficients;
        }
    }

    APP_CHK_RET(CVI_VENC_SetH264Vui(VencChn, &h264Vui), "set H264 Vui");

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Venc_H265Trans_Set(VENC_CHN VencChn)
{
    VENC_H265_TRANS_S h265Trans = { 0 };

    APP_H265_TRANS_PARAM_S stH265TransParam;
    memset(&stH265TransParam, 0, sizeof(APP_H265_TRANS_PARAM_S));
    APP_CHK_RET(CVI_VENC_GetH265Trans(VencChn, &h265Trans), "get H265 Trans");
    stH265TransParam.h265CbQpOffset = 0;
    stH265TransParam.h265CrQpOffset = 0;

    h265Trans.cb_qp_offset = stH265TransParam.h265CbQpOffset;
    h265Trans.cr_qp_offset = stH265TransParam.h265CrQpOffset;

    APP_CHK_RET(CVI_VENC_SetH265Trans(VencChn, &h265Trans), "set H265 Trans");

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Venc_H265Vui_Set(VENC_CHN VencChn)
{
    VENC_H265_VUI_S h265Vui = { 0 };
    APP_VENC_CHN_CFG_S *pstVencChnCfg = &g_pstVencCtx->astVencChnCfg[VencChn];

    APP_H265_VUI_PARAM_S stH265VuiParam;
    memset(&stH265VuiParam, 0, sizeof(APP_H265_VUI_PARAM_S));
    APP_CHK_RET(CVI_VENC_GetH265Vui(VencChn, &h265Vui), "get H265 Vui");
    stH265VuiParam.aspectRatioInfoPresentFlag   = CVI_H26X_ASPECT_RATIO_INFO_PRESENT_FLAG_DEFAULT;
    stH265VuiParam.aspectRatioIdc               = CVI_H26X_ASPECT_RATIO_IDC_DEFAULT;
    stH265VuiParam.overscanInfoPresentFlag      = CVI_H26X_OVERSCAN_INFO_PRESENT_FLAG_DEFAULT;
    stH265VuiParam.overscanAppropriateFlag      = CVI_H26X_OVERSCAN_APPROPRIATE_FLAG_DEFAULT;
    stH265VuiParam.sarWidth                     = CVI_H26X_SAR_WIDTH_DEFAULT;
    stH265VuiParam.sarHeight                    = CVI_H26X_SAR_HEIGHT_DEFAULT;
    stH265VuiParam.videoSignalTypePresentFlag   = CVI_H26X_VIDEO_SIGNAL_TYPE_PRESENT_FLAG_DEFAULT;
    stH265VuiParam.videoFormat                  = CVI_H26X_VIDEO_FORMAT_DEFAULT;
    stH265VuiParam.videoFullRangeFlag           = CVI_H26X_VIDEO_FULL_RANGE_FLAG_DEFAULT;
    stH265VuiParam.colourDescriptionPresentFlag = CVI_H26X_COLOUR_DESCRIPTION_PRESENT_FLAG_DEFAULT;
    stH265VuiParam.colourPrimaries              = CVI_H26X_COLOUR_PRIMARIES_DEFAULT;
    stH265VuiParam.transferCharacteristics      = CVI_H26X_TRANSFER_CHARACTERISTICS_DEFAULT;
    stH265VuiParam.matrixCoefficients           = CVI_H26X_MATRIX_COEFFICIENTS_DEFAULT;

    stH265VuiParam.timingInfoPresentFlag = 1;
#if defined(__CV186AH__)
    stH265VuiParam.numUnitsInTick = 1000;
#else
    stH265VuiParam.numUnitsInTick = 1;
#endif

    h265Vui.stVuiAspectRatio.aspect_ratio_info_present_flag = stH265VuiParam.aspectRatioInfoPresentFlag;
    if (h265Vui.stVuiAspectRatio.aspect_ratio_info_present_flag) {
        h265Vui.stVuiAspectRatio.aspect_ratio_idc = stH265VuiParam.aspectRatioIdc;
        h265Vui.stVuiAspectRatio.sar_width = stH265VuiParam.sarWidth;
        h265Vui.stVuiAspectRatio.sar_height = stH265VuiParam.sarHeight;
    }

    h265Vui.stVuiAspectRatio.overscan_info_present_flag = stH265VuiParam.overscanInfoPresentFlag;
    if (h265Vui.stVuiAspectRatio.overscan_info_present_flag) {
        h265Vui.stVuiAspectRatio.overscan_appropriate_flag = stH265VuiParam.overscanAppropriateFlag;
    }

    h265Vui.stVuiTimeInfo.timing_info_present_flag = stH265VuiParam.timingInfoPresentFlag;
    if (h265Vui.stVuiTimeInfo.timing_info_present_flag) {
        h265Vui.stVuiTimeInfo.num_units_in_tick = stH265VuiParam.numUnitsInTick;
        //265 fps = time_scale / num_units_in_tick
        h265Vui.stVuiTimeInfo.time_scale = pstVencChnCfg->u32DstFrameRate * stH265VuiParam.numUnitsInTick;
    }

    h265Vui.stVuiVideoSignal.video_signal_type_present_flag = stH265VuiParam.videoSignalTypePresentFlag;
    if (h265Vui.stVuiVideoSignal.video_signal_type_present_flag) {
        h265Vui.stVuiVideoSignal.video_format = stH265VuiParam.videoFormat;
        h265Vui.stVuiVideoSignal.video_full_range_flag = stH265VuiParam.videoFullRangeFlag;
        h265Vui.stVuiVideoSignal.colour_description_present_flag = stH265VuiParam.colourDescriptionPresentFlag;
        if (h265Vui.stVuiVideoSignal.colour_description_present_flag) {
            h265Vui.stVuiVideoSignal.colour_primaries = stH265VuiParam.colourPrimaries;
            h265Vui.stVuiVideoSignal.transfer_characteristics = stH265VuiParam.transferCharacteristics;
            h265Vui.stVuiVideoSignal.matrix_coefficients = stH265VuiParam.matrixCoefficients;
        }
    }

    APP_CHK_RET(CVI_VENC_SetH265Vui(VencChn, &h265Vui), "set H265 Vui");

    return CVI_SUCCESS;
}

static CVI_S32 app_ipcam_Venc_Roi_Set(VENC_CHN vencChn, APP_VENC_ROI_CFG_S *pstRoiAttr)
{
    if (NULL == pstRoiAttr)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstRoiAttr is NULL!\n");
        return CVI_FAILURE;
    }

    CVI_S32 ret;
    CVI_S32 i = 0;
    VENC_ROI_ATTR_S roiAttr;
    memset(&roiAttr, 0, sizeof(roiAttr));

    for (i = 0; i < MAX_NUM_ROI; i++)
    {
        if (vencChn == pstRoiAttr[i].VencChn)
        {
            ret = CVI_VENC_GetRoiAttr(vencChn, i, &roiAttr);
            if (ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "GetRoiAttr failed!\n");
                return CVI_FAILURE;
            }
            roiAttr.bEnable = pstRoiAttr[i].bEnable;
            roiAttr.bAbsQp = pstRoiAttr[i].bAbsQp;
            roiAttr.u32Index = i;
            roiAttr.s32Qp = pstRoiAttr[i].u32Qp;
            roiAttr.stRect.s32X = pstRoiAttr[i].u32X;
            roiAttr.stRect.s32Y = pstRoiAttr[i].u32Y;
            roiAttr.stRect.u32Width = pstRoiAttr[i].u32Width;
            roiAttr.stRect.u32Height = pstRoiAttr[i].u32Height;
            ret = CVI_VENC_SetRoiAttr(vencChn, &roiAttr);
            if (ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "SetRoiAttr failed!\n");
                return CVI_FAILURE;
            }
        }
    }

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Venc_Jpeg_Param_Set(VENC_CHN VencChn, APP_JPEG_CODEC_PARAM_S *pstJpegCodecCfg)
{
    VENC_JPEG_PARAM_S stJpegParam, *pstJpegParam = &stJpegParam;

    APP_CHK_RET(CVI_VENC_GetJpegParam(VencChn, pstJpegParam), "get jpeg param");

    if (pstJpegCodecCfg->quality <= 0)
        pstJpegCodecCfg->quality = 1;
    else if (pstJpegCodecCfg->quality >= 100)
        pstJpegCodecCfg->quality = 99;

    pstJpegParam->u32Qfactor = pstJpegCodecCfg->quality;
    pstJpegParam->u32MCUPerECS = pstJpegCodecCfg->MCUPerECS;

    APP_CHK_RET(CVI_VENC_SetJpegParam(VencChn, pstJpegParam), "set jpeg param");

    return CVI_SUCCESS;
}

int app_ipcam_Venc_Chn_Attr_Set(VENC_ATTR_S *pstVencAttr, APP_VENC_CHN_CFG_S *pstVencChnCfg)
{
    pstVencAttr->enType = pstVencChnCfg->enType;
    pstVencAttr->u32MaxPicWidth = pstVencChnCfg->u32Width;
    pstVencAttr->u32MaxPicHeight = pstVencChnCfg->u32Height;
    pstVencAttr->u32PicWidth = pstVencChnCfg->u32Width;
    pstVencAttr->u32PicHeight = pstVencChnCfg->u32Height;
    pstVencAttr->u32Profile = pstVencChnCfg->u32Profile;
    pstVencAttr->bSingleCore = pstVencChnCfg->bSingleCore;
    pstVencAttr->bByFrame = CVI_TRUE;
    pstVencAttr->bEsBufQueueEn = CVI_TRUE;
    pstVencAttr->bIsoSendFrmEn = CVI_TRUE;
    pstVencAttr->u32BufSize = pstVencChnCfg->u32StreamBufSize;

    APP_PROF_LOG_PRINT(LEVEL_TRACE,"enType=%d u32Profile=%d bSingleCore=%d\n",
        pstVencAttr->enType, pstVencAttr->u32Profile, pstVencAttr->bSingleCore);
    APP_PROF_LOG_PRINT(LEVEL_TRACE,"u32MaxPicWidth=%d u32MaxPicHeight=%d u32PicWidth=%d u32PicHeight=%d\n",
        pstVencAttr->u32MaxPicWidth, pstVencAttr->u32MaxPicHeight, pstVencAttr->u32PicWidth, pstVencAttr->u32PicHeight);

    /* Need set true when Venc is bind mode */
    if (pstVencChnCfg->enBindMode == VENC_BIND_DISABLE) {
        pstVencAttr->bEsBufQueueEn = CVI_FALSE;
        pstVencAttr->bIsoSendFrmEn = CVI_FALSE;
    }
    
    /* Venc encode type validity check */
    if ((pstVencAttr->enType != PT_H265) && (pstVencAttr->enType != PT_H264)
        && (pstVencAttr->enType != PT_JPEG) && (pstVencAttr->enType != PT_MJPEG)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"encode type = %d invalid\n", pstVencAttr->enType);
        return CVI_FAILURE;
    }

    if (pstVencAttr->enType == PT_H264) {
        // pstVencAttr->stAttrH264e.bSingleLumaBuf = 1;
    }

    if (PT_JPEG == pstVencChnCfg->enType || PT_MJPEG == pstVencChnCfg->enType) {
        VENC_ATTR_JPEG_S *pstJpegAttr = &pstVencAttr->stAttrJpege;

        pstJpegAttr->bSupportDCF = CVI_FALSE;
        pstJpegAttr->stMPFCfg.u8LargeThumbNailNum = 0;
        pstJpegAttr->enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
    }

    return CVI_SUCCESS;
}

int app_ipcam_Venc_Gop_Attr_Set(VENC_GOP_ATTR_S *pstGopAttr, APP_VENC_CHN_CFG_S *pstVencChnCfg)
{
    VENC_GOP_MODE_E enGopMode = pstVencChnCfg->enGopMode;

    /* Venc gop mode validity check */
    if ((enGopMode != VENC_GOPMODE_NORMALP) && (enGopMode != VENC_GOPMODE_SMARTP)
        && (enGopMode != VENC_GOPMODE_DUALP) && (enGopMode != VENC_GOPMODE_BIPREDB)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"gop mode = %d invalid\n", enGopMode);
        return CVI_FAILURE;
    }

    switch (enGopMode) {
        case VENC_GOPMODE_NORMALP:
            pstGopAttr->stNormalP.s32IPQpDelta = pstVencChnCfg->unGopParam.stNormalP.s32IPQpDelta;

            APP_PROF_LOG_PRINT(LEVEL_TRACE,"stNormalP -> s32IPQpDelta=%d\n",
                pstGopAttr->stNormalP.s32IPQpDelta);
            break;
        case VENC_GOPMODE_SMARTP:
            pstGopAttr->stSmartP.s32BgQpDelta = pstVencChnCfg->unGopParam.stSmartP.s32BgQpDelta;
            pstGopAttr->stSmartP.s32ViQpDelta = pstVencChnCfg->unGopParam.stSmartP.s32ViQpDelta;
            pstGopAttr->stSmartP.u32BgInterval = pstVencChnCfg->unGopParam.stSmartP.u32BgInterval;

            APP_PROF_LOG_PRINT(LEVEL_TRACE,"stSmartP -> s32BgQpDelta=%d s32ViQpDelta=%d u32BgInterval=%d\n",
                pstGopAttr->stSmartP.s32BgQpDelta, pstGopAttr->stSmartP.s32ViQpDelta, pstGopAttr->stSmartP.u32BgInterval);
            break;

        case VENC_GOPMODE_DUALP:
            pstGopAttr->stDualP.s32IPQpDelta = pstVencChnCfg->unGopParam.stDualP.s32IPQpDelta;
            pstGopAttr->stDualP.s32SPQpDelta = pstVencChnCfg->unGopParam.stDualP.s32SPQpDelta;
            pstGopAttr->stDualP.u32SPInterval = pstVencChnCfg->unGopParam.stDualP.u32SPInterval;

            APP_PROF_LOG_PRINT(LEVEL_TRACE,"stDualP -> s32IPQpDelta=%d s32SPQpDelta=%d u32SPInterval=%d\n",
                pstGopAttr->stDualP.s32IPQpDelta, pstGopAttr->stDualP.s32SPQpDelta, pstGopAttr->stDualP.u32SPInterval);
            break;

        case VENC_GOPMODE_BIPREDB:
            pstGopAttr->stBipredB.s32BQpDelta = pstVencChnCfg->unGopParam.stBipredB.s32BQpDelta;
            pstGopAttr->stBipredB.s32IPQpDelta = pstVencChnCfg->unGopParam.stBipredB.s32IPQpDelta;
            pstGopAttr->stBipredB.u32BFrmNum = pstVencChnCfg->unGopParam.stBipredB.u32BFrmNum;

            APP_PROF_LOG_PRINT(LEVEL_TRACE,"stBipredB -> s32BQpDelta=%d s32IPQpDelta=%d u32BFrmNum=%d\n",
                pstGopAttr->stBipredB.s32BQpDelta, pstGopAttr->stBipredB.s32IPQpDelta, pstGopAttr->stBipredB.u32BFrmNum);
            break;

        default:
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"not support the gop mode !\n");
            return CVI_FAILURE;
    }

    pstGopAttr->enGopMode = enGopMode;
    if (PT_MJPEG == pstVencChnCfg->enType || PT_JPEG == pstVencChnCfg->enType) {
        pstGopAttr->enGopMode = VENC_GOPMODE_NORMALP;
        pstGopAttr->stNormalP.s32IPQpDelta = pstVencChnCfg->unGopParam.stNormalP.s32IPQpDelta;
    }

    return CVI_SUCCESS;
}

int app_ipcam_Venc_Rc_Attr_Set(VENC_RC_ATTR_S *pstRCAttr, APP_VENC_CHN_CFG_S *pstVencChnCfg)
{
    int SrcFrmRate = pstVencChnCfg->u32SrcFrameRate;
    int DstFrmRate = pstVencChnCfg->u32DstFrameRate;
    int BitRate    = pstVencChnCfg->u32BitRate;
    int MaxBitrate = pstVencChnCfg->u32MaxBitRate;
    int StatTime   = pstVencChnCfg->statTime;
    int Gop        = pstVencChnCfg->u32Gop;
    int IQP        = pstVencChnCfg->u32IQp;
    int PQP        = pstVencChnCfg->u32PQp;

    APP_PROF_LOG_PRINT(LEVEL_TRACE,"RcMode=%d EncType=%d SrcFR=%d DstFR=%d\n",
        pstVencChnCfg->enRcMode, pstVencChnCfg->enType,
        pstVencChnCfg->u32SrcFrameRate, pstVencChnCfg->u32DstFrameRate);
    APP_PROF_LOG_PRINT(LEVEL_TRACE,"BR=%d MaxBR=%d statTime=%d gop=%d IQP=%d PQP=%d\n",
        pstVencChnCfg->u32BitRate, pstVencChnCfg->u32MaxBitRate,
        pstVencChnCfg->statTime, pstVencChnCfg->u32Gop, pstVencChnCfg->u32IQp, pstVencChnCfg->u32PQp);

    pstRCAttr->enRcMode = pstVencChnCfg->enRcMode;
    switch (pstVencChnCfg->enType) {
    case PT_H265: {
        if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H265CBR) {
            VENC_H265_CBR_S *pstH265Cbr = &pstRCAttr->stH265Cbr;

            pstH265Cbr->u32Gop = Gop;
            pstH265Cbr->u32StatTime = StatTime;
            pstH265Cbr->u32SrcFrameRate = SrcFrmRate;
            pstH265Cbr->fr32DstFrameRate = DstFrmRate;
            pstH265Cbr->u32BitRate = BitRate;
            pstH265Cbr->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H265FIXQP) {
            VENC_H265_FIXQP_S *pstH265FixQp = &pstRCAttr->stH265FixQp;

            pstH265FixQp->u32Gop = Gop;
            pstH265FixQp->u32SrcFrameRate = SrcFrmRate;
            pstH265FixQp->fr32DstFrameRate = DstFrmRate;
            pstH265FixQp->u32IQp = IQP;
            pstH265FixQp->u32PQp = PQP;
            pstH265FixQp->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H265VBR) {
            VENC_H265_VBR_S *pstH265Vbr = &pstRCAttr->stH265Vbr;

            pstH265Vbr->u32Gop = Gop;
            pstH265Vbr->u32StatTime = StatTime;
            pstH265Vbr->u32SrcFrameRate = SrcFrmRate;
            pstH265Vbr->fr32DstFrameRate = DstFrmRate;
            pstH265Vbr->u32MaxBitRate = MaxBitrate;
            pstH265Vbr->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H265AVBR) {
            VENC_H265_AVBR_S *pstH265AVbr = &pstRCAttr->stH265AVbr;

            pstH265AVbr->u32Gop = Gop;
            pstH265AVbr->u32StatTime = StatTime;
            pstH265AVbr->u32SrcFrameRate = SrcFrmRate;
            pstH265AVbr->fr32DstFrameRate = DstFrmRate;
            pstH265AVbr->u32MaxBitRate = MaxBitrate;
            pstH265AVbr->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H265QVBR) {
            VENC_H265_QVBR_S *pstH265QVbr = &pstRCAttr->stH265QVbr;

            pstH265QVbr->u32Gop = Gop;
            pstH265QVbr->u32StatTime = StatTime;
            pstH265QVbr->u32SrcFrameRate = SrcFrmRate;
            pstH265QVbr->fr32DstFrameRate = DstFrmRate;
            pstH265QVbr->u32TargetBitRate = BitRate;
            pstH265QVbr->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H265QPMAP) {
            VENC_H265_QPMAP_S *pstH265QpMap = &pstRCAttr->stH265QpMap;

            pstH265QpMap->u32Gop = Gop;
            pstH265QpMap->u32StatTime = StatTime;
            pstH265QpMap->u32SrcFrameRate = SrcFrmRate;
            pstH265QpMap->fr32DstFrameRate = DstFrmRate;
            pstH265QpMap->enQpMapMode = VENC_RC_QPMAP_MODE_MEANQP;
            pstH265QpMap->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"VencChn(%d) enRcMode(%d) not support\n", pstVencChnCfg->VencChn, pstVencChnCfg->enRcMode);
            return CVI_FAILURE;
        }
    }
    break;
    case PT_H264: {
        if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H264CBR) {
            VENC_H264_CBR_S *pstH264Cbr = &pstRCAttr->stH264Cbr;

            pstH264Cbr->u32Gop = Gop;
            pstH264Cbr->u32StatTime = StatTime;
            pstH264Cbr->u32SrcFrameRate = SrcFrmRate;
            pstH264Cbr->fr32DstFrameRate = DstFrmRate;
            pstH264Cbr->u32BitRate = BitRate;
            pstH264Cbr->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H264FIXQP) {
            VENC_H264_FIXQP_S *pstH264FixQp = &pstRCAttr->stH264FixQp;

            pstH264FixQp->u32Gop = Gop;
            pstH264FixQp->u32SrcFrameRate = SrcFrmRate;
            pstH264FixQp->fr32DstFrameRate = DstFrmRate;
            pstH264FixQp->u32IQp = IQP;
            pstH264FixQp->u32PQp = PQP;
            pstH264FixQp->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H264VBR) {
            VENC_H264_VBR_S *pstH264Vbr = &pstRCAttr->stH264Vbr;

            pstH264Vbr->u32Gop = Gop;
            pstH264Vbr->u32StatTime = StatTime;
            pstH264Vbr->u32SrcFrameRate = SrcFrmRate;
            pstH264Vbr->fr32DstFrameRate = DstFrmRate;
            pstH264Vbr->u32MaxBitRate = MaxBitrate;
            pstH264Vbr->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H264AVBR) {
            VENC_H264_AVBR_S *pstH264AVbr = &pstRCAttr->stH264AVbr;

            pstH264AVbr->u32Gop = Gop;
            pstH264AVbr->u32StatTime = StatTime;
            pstH264AVbr->u32SrcFrameRate = SrcFrmRate;
            pstH264AVbr->fr32DstFrameRate = DstFrmRate;
            pstH264AVbr->u32MaxBitRate = MaxBitrate;
            pstH264AVbr->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H264QVBR) {
            VENC_H264_QVBR_S *pstH264QVbr = &pstRCAttr->stH264QVbr;

            pstH264QVbr->u32Gop = Gop;
            pstH264QVbr->u32StatTime = StatTime;
            pstH264QVbr->u32SrcFrameRate = SrcFrmRate;
            pstH264QVbr->fr32DstFrameRate = DstFrmRate;
            pstH264QVbr->u32TargetBitRate = BitRate;
            pstH264QVbr->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_H264QPMAP) {
            VENC_H264_QPMAP_S *pstH264QpMap = &pstRCAttr->stH264QpMap;

            pstH264QpMap->u32Gop = Gop;
            pstH264QpMap->u32StatTime = StatTime;
            pstH264QpMap->u32SrcFrameRate = SrcFrmRate;
            pstH264QpMap->fr32DstFrameRate = DstFrmRate;
            pstH264QpMap->bVariFpsEn = (pstVencChnCfg->VencChn == 0) ? 0 : 1; //SBM unsupport
        } else {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"enRcMode(%d) not support\n", pstVencChnCfg->enRcMode);
            return CVI_FAILURE;
        }
    }
    break;
    case PT_MJPEG: {
        if (pstVencChnCfg->enRcMode == VENC_RC_MODE_MJPEGFIXQP) {
            VENC_MJPEG_FIXQP_S *pstMjpegeFixQp = &pstRCAttr->stMjpegFixQp;

            // 0 use old q-table for forward compatible.
            pstMjpegeFixQp->u32Qfactor = 0;
            pstMjpegeFixQp->u32SrcFrameRate = SrcFrmRate;
            pstMjpegeFixQp->fr32DstFrameRate = DstFrmRate;
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_MJPEGCBR) {
            VENC_MJPEG_CBR_S *pstMjpegeCbr = &pstRCAttr->stMjpegCbr;

            pstMjpegeCbr->u32StatTime = StatTime;
            pstMjpegeCbr->u32SrcFrameRate = SrcFrmRate;
            pstMjpegeCbr->fr32DstFrameRate = DstFrmRate;
            pstMjpegeCbr->u32BitRate = BitRate;
        } else if (pstVencChnCfg->enRcMode == VENC_RC_MODE_MJPEGVBR) {
            VENC_MJPEG_VBR_S *pstMjpegeVbr = &pstRCAttr->stMjpegVbr;

            pstMjpegeVbr->u32StatTime = StatTime;
            pstMjpegeVbr->u32SrcFrameRate = SrcFrmRate;
            pstMjpegeVbr->fr32DstFrameRate = DstFrmRate;
            pstMjpegeVbr->u32MaxBitRate = MaxBitrate;
        } else {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"cann't support other mode(%d) in this version!\n", pstVencChnCfg->enRcMode);
            return CVI_FAILURE;
        }
    }
    break;

    case PT_JPEG:
        break;

    default:
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"cann't support this enType (%d) in this version!\n", pstVencChnCfg->enType);
        return CVI_ERR_VENC_NOT_SUPPORT;
    }

    return CVI_SUCCESS;
}

void app_ipcam_Venc_Attr_Check(VENC_CHN_ATTR_S *pstVencChnAttr)
{
    if (pstVencChnAttr->stVencAttr.enType == PT_H264) {
        // pstVencChnAttr->stVencAttr.stAttrH264e.bSingleLumaBuf = 1;
    }

    if ((pstVencChnAttr->stGopAttr.enGopMode == VENC_GOPMODE_BIPREDB) &&
        (pstVencChnAttr->stVencAttr.enType == PT_H264)) {
        if (pstVencChnAttr->stVencAttr.u32Profile == 0) {
            pstVencChnAttr->stVencAttr.u32Profile = 1;
            APP_PROF_LOG_PRINT(LEVEL_WARN,"H.264 base not support BIPREDB, change to main\n");
        }
    }

    if ((pstVencChnAttr->stRcAttr.enRcMode == VENC_RC_MODE_H264QPMAP) ||
        (pstVencChnAttr->stRcAttr.enRcMode == VENC_RC_MODE_H265QPMAP)) {
        if (pstVencChnAttr->stGopAttr.enGopMode == VENC_GOPMODE_ADVSMARTP) {
            pstVencChnAttr->stGopAttr.enGopMode = VENC_GOPMODE_SMARTP;
            APP_PROF_LOG_PRINT(LEVEL_WARN,"advsmartp not support QPMAP, so change gopmode to smartp!\n");
        }
    }

    if ((pstVencChnAttr->stGopAttr.enGopMode == VENC_GOPMODE_BIPREDB) &&
        (pstVencChnAttr->stVencAttr.enType == PT_H264)) {
        if (pstVencChnAttr->stVencAttr.u32Profile == 0) {
            pstVencChnAttr->stVencAttr.u32Profile = 1;
            APP_PROF_LOG_PRINT(LEVEL_WARN,"H.264 base not support BIPREDB, change to main\n");
        }
    }
    if ((pstVencChnAttr->stRcAttr.enRcMode == VENC_RC_MODE_H264QPMAP) ||
        (pstVencChnAttr->stRcAttr.enRcMode == VENC_RC_MODE_H265QPMAP)) {
        if (pstVencChnAttr->stGopAttr.enGopMode == VENC_GOPMODE_ADVSMARTP) {
            pstVencChnAttr->stGopAttr.enGopMode = VENC_GOPMODE_SMARTP;
            APP_PROF_LOG_PRINT(LEVEL_WARN,"advsmartp not support QPMAP, so change gopmode to smartp!\n");
        }
    }
}

int app_ipcam_Venc_Rc_Param_Set(
    VENC_CHN VencChn,
    PAYLOAD_TYPE_E enCodecType,
    VENC_RC_MODE_E enRcMode,
    APP_RC_PARAM_S *pstRcParamCfg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    VENC_RC_PARAM_S stRcParam, *pstRcParam = &stRcParam;

    s32Ret = CVI_VENC_GetRcParam(VencChn, pstRcParam);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "GetRcParam, 0x%X\n", s32Ret);
        return s32Ret;
    }

    CVI_U32 u32MaxIprop          = pstRcParamCfg->u32MaxIprop;
    CVI_U32 u32MinIprop          = pstRcParamCfg->u32MinIprop;
    CVI_U32 u32MaxQp             = pstRcParamCfg->u32MaxQp;
    CVI_U32 u32MinQp             = pstRcParamCfg->u32MinQp;
    CVI_U32 u32MaxIQp            = pstRcParamCfg->u32MaxIQp;
    CVI_U32 u32MinIQp            = pstRcParamCfg->u32MinIQp;
    CVI_S32 s32ChangePos         = pstRcParamCfg->s32ChangePos;
    CVI_S32 s32MinStillPercent   = pstRcParamCfg->s32MinStillPercent;
    CVI_U32 u32MaxStillQP        = pstRcParamCfg->u32MaxStillQP;
    CVI_U32 u32MotionSensitivity = pstRcParamCfg->u32MotionSensitivity;
    CVI_S32 s32AvbrFrmLostOpen   = pstRcParamCfg->s32AvbrFrmLostOpen;
    CVI_S32 s32AvbrFrmGap        = pstRcParamCfg->s32AvbrFrmGap;
    CVI_S32 s32AvbrPureStillThr  = pstRcParamCfg->s32AvbrPureStillThr;

    CVI_U32 u32ThrdLv            = pstRcParamCfg->u32ThrdLv;
    CVI_S32 s32FirstFrameStartQp = pstRcParamCfg->s32FirstFrameStartQp;
    CVI_S32 s32InitialDelay      = pstRcParamCfg->s32InitialDelay;
    CVI_U32 s32BgDeltaQp         = pstRcParamCfg->s32BgDeltaQp;
    CVI_S32 u32RowQpDelta        = pstRcParamCfg->u32RowQpDelta;
    CVI_S32 bBgEnhanceEn         = pstRcParamCfg->bBgEnhanceEn;

    APP_PROF_LOG_PRINT(LEVEL_TRACE,"MaxIprop=%d MinIprop=%d MaxQp=%d MinQp=%d MaxIQp=%d MinIQp=%d\n",
        u32MaxIprop, u32MinIprop, u32MaxQp, u32MinQp, u32MaxIQp, u32MinIQp);
    APP_PROF_LOG_PRINT(LEVEL_TRACE,"ChangePos=%d MinStillPercent=%d MaxStillQP=%d MotionSensitivity=%d AvbrFrmLostOpen=%d\n",
        s32ChangePos, s32MinStillPercent, u32MaxStillQP, u32MotionSensitivity, s32AvbrFrmLostOpen);
    APP_PROF_LOG_PRINT(LEVEL_TRACE,"AvbrFrmGap=%d AvbrPureStillThr=%d ThrdLv=%d FirstFrameStartQp=%d InitialDelay=%d\n",
        s32AvbrFrmGap, s32AvbrPureStillThr, u32ThrdLv, s32FirstFrameStartQp, s32InitialDelay);
    APP_PROF_LOG_PRINT(LEVEL_TRACE, "BgDeltaQp=%d RowQpDelta=%d BgEnhanceEn=%d\n",
        s32BgDeltaQp, u32RowQpDelta, bBgEnhanceEn);
               
    pstRcParam->u32ThrdLv = u32ThrdLv;
    pstRcParam->s32FirstFrameStartQp = s32FirstFrameStartQp;
    pstRcParam->s32InitialDelay = s32InitialDelay;
    pstRcParam->s32BgDeltaQp = s32BgDeltaQp;
    pstRcParam->u32RowQpDelta = u32RowQpDelta;
    pstRcParam->bBgEnhanceEn = bBgEnhanceEn;

    switch (enCodecType) {
    case PT_H265: {
        if (enRcMode == VENC_RC_MODE_H265CBR) {
            pstRcParam->stParamH265Cbr.u32MaxIprop = u32MaxIprop;
            pstRcParam->stParamH265Cbr.u32MinIprop = u32MinIprop;
            pstRcParam->stParamH265Cbr.u32MaxIQp = u32MaxIQp;
            pstRcParam->stParamH265Cbr.u32MinIQp = u32MinIQp;
            pstRcParam->stParamH265Cbr.u32MaxQp = u32MaxQp;
            pstRcParam->stParamH265Cbr.u32MinQp = u32MinQp;
        } else if (enRcMode == VENC_RC_MODE_H265VBR) {
            pstRcParam->stParamH265Vbr.u32MaxIprop = u32MaxIprop;
            pstRcParam->stParamH265Vbr.u32MinIprop = u32MinIprop;
            pstRcParam->stParamH265Vbr.u32MaxIQp = u32MaxIQp;
            pstRcParam->stParamH265Vbr.u32MinIQp = u32MinIQp;
            pstRcParam->stParamH265Vbr.u32MaxQp = u32MaxQp;
            pstRcParam->stParamH265Vbr.u32MinQp = u32MinQp;
            pstRcParam->stParamH265Vbr.s32ChangePos = s32ChangePos;
        } else if (enRcMode == VENC_RC_MODE_H265AVBR) {
            pstRcParam->stParamH265AVbr.u32MaxIprop = u32MaxIprop;
            pstRcParam->stParamH265AVbr.u32MinIprop = u32MinIprop;
            pstRcParam->stParamH265AVbr.u32MaxIQp = u32MaxIQp;
            pstRcParam->stParamH265AVbr.u32MinIQp = u32MinIQp;
            pstRcParam->stParamH265AVbr.u32MaxQp = u32MaxQp;
            pstRcParam->stParamH265AVbr.u32MinQp = u32MinQp;
            pstRcParam->stParamH265AVbr.s32ChangePos = s32ChangePos;
            pstRcParam->stParamH265AVbr.s32MinStillPercent = s32MinStillPercent;
            pstRcParam->stParamH265AVbr.u32MaxStillQP = u32MaxStillQP;
            pstRcParam->stParamH265AVbr.u32MotionSensitivity = u32MotionSensitivity;
            pstRcParam->stParamH265AVbr.s32AvbrFrmLostOpen = s32AvbrFrmLostOpen;
            pstRcParam->stParamH265AVbr.s32AvbrFrmGap = s32AvbrFrmGap;
            pstRcParam->stParamH265AVbr.s32AvbrPureStillThr = s32AvbrPureStillThr;
        } else {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"enRcMode(%d) not support\n", enRcMode);
            return CVI_FAILURE;
        }
    }
    break;
    case PT_H264: {
        if (enRcMode == VENC_RC_MODE_H264CBR) {
            pstRcParam->stParamH264Cbr.u32MaxIprop = u32MaxIprop;
            pstRcParam->stParamH264Cbr.u32MinIprop = u32MinIprop;
            pstRcParam->stParamH264Cbr.u32MaxIQp = u32MaxIQp;
            pstRcParam->stParamH264Cbr.u32MinIQp = u32MinIQp;
            pstRcParam->stParamH264Cbr.u32MaxQp = u32MaxQp;
            pstRcParam->stParamH264Cbr.u32MinQp = u32MinQp;
        } else if (enRcMode == VENC_RC_MODE_H264VBR) {
            pstRcParam->stParamH264Vbr.u32MaxIprop = u32MaxIprop;
            pstRcParam->stParamH264Vbr.u32MinIprop = u32MinIprop;
            pstRcParam->stParamH264Vbr.u32MaxIQp = u32MaxIQp;
            pstRcParam->stParamH264Vbr.u32MinIQp = u32MinIQp;
            pstRcParam->stParamH264Vbr.u32MaxQp = u32MaxQp;
            pstRcParam->stParamH264Vbr.u32MinQp = u32MinQp;
            pstRcParam->stParamH264Vbr.s32ChangePos = s32ChangePos;
        } else if (enRcMode == VENC_RC_MODE_H264AVBR) {
            pstRcParam->stParamH264AVbr.u32MaxIprop = u32MaxIprop;
            pstRcParam->stParamH264AVbr.u32MinIprop = u32MinIprop;
            pstRcParam->stParamH264AVbr.u32MaxIQp = u32MaxIQp;
            pstRcParam->stParamH264AVbr.u32MinIQp = u32MinIQp;
            pstRcParam->stParamH264AVbr.u32MaxQp = u32MaxQp;
            pstRcParam->stParamH264AVbr.u32MinQp = u32MinQp;
            pstRcParam->stParamH264AVbr.s32ChangePos = s32ChangePos;
            pstRcParam->stParamH264AVbr.s32MinStillPercent = s32MinStillPercent;
            pstRcParam->stParamH264AVbr.u32MaxStillQP = u32MaxStillQP;
            pstRcParam->stParamH264AVbr.u32MotionSensitivity = u32MotionSensitivity;
            pstRcParam->stParamH264AVbr.s32AvbrFrmLostOpen = s32AvbrFrmLostOpen;
            pstRcParam->stParamH264AVbr.s32AvbrFrmGap = s32AvbrFrmGap;
            pstRcParam->stParamH264AVbr.s32AvbrPureStillThr = s32AvbrPureStillThr;
        } else {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"enRcMode(%d) not support\n", enRcMode);
            return CVI_FAILURE;
        }
    }
    break;
    default:
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"cann't support this enType (%d) in this version!\n", enCodecType);
        return CVI_ERR_VENC_NOT_SUPPORT;
    }

    s32Ret = CVI_VENC_SetRcParam(VencChn, pstRcParam);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "SetRcParam, 0x%X\n", s32Ret);
        return s32Ret;
    }

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Venc_SingleEsBuff_Set(CVI_VOID)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    for (VENC_MODTYPE_E modtype = MODTYPE_H264E; modtype <= MODTYPE_JPEGE; modtype++) {
        VENC_PARAM_MOD_S stModParam;
        VB_SOURCE_E eVbSource;

        stModParam.enVencModType = modtype;
        s32Ret = CVI_VENC_GetModParam(&stModParam);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VENC_GetModParam type %d failure\n", modtype);
            return s32Ret;
        }

        switch (modtype) {
        case MODTYPE_H264E:
            stModParam.stH264eModParam.bSingleEsBuf = true;
            stModParam.stH264eModParam.u32SingleEsBufSize = 0x0100000;  // 1.0MB
            eVbSource = stModParam.stH264eModParam.enH264eVBSource;
            break;
        case MODTYPE_H265E:
            stModParam.stH265eModParam.bSingleEsBuf = true;
            stModParam.stH265eModParam.u32SingleEsBufSize = 0x0100000;  // 1.0MB
            eVbSource = stModParam.stH265eModParam.enH265eVBSource;
            break;
        case MODTYPE_JPEGE:
            stModParam.stJpegeModParam.bSingleEsBuf = true;
            stModParam.stJpegeModParam.u32SingleEsBufSize = 0x0100000;  // 1.0MB
            break;
        default:
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Venc_SingleEsBuff_Set invalid type %d failure\n", modtype);
            break;
        }

        if (((modtype == MODTYPE_H264E) || (modtype == MODTYPE_H265E)) && (eVbSource == VB_SOURCE_USER)) {
            #if 0   // TODO: create Venc VB Pool if needed
            VENC_CHN_POOL_S stPool;
            s32Ret = CVI_VENC_AttachVbPool(VencChn, &stPool);
            #endif
        }

        s32Ret = CVI_VENC_SetModParam(&stModParam);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "VENC PARAM_MODE type %d failure\n", modtype);
            return s32Ret;
        }
    }

    return CVI_SUCCESS;
}

static CVI_S32 app_ipcam_Postfix_Get(PAYLOAD_TYPE_E enPayload, char *szPostfix)
{

    _NULL_POINTER_CHECK_(szPostfix, -1);

    if (enPayload == PT_H264)
        strcpy(szPostfix, ".h264");
    else if (enPayload == PT_H265)
        strcpy(szPostfix, ".h265");
    else if (enPayload == PT_JPEG)
        strcpy(szPostfix, ".jpg");
    else if (enPayload == PT_MJPEG)
        strcpy(szPostfix, ".mjp");
    else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "payload type(%d) err!\n", enPayload);
        return CVI_FAILURE;
    }

    return CVI_SUCCESS;
}

static int _streaming_save_to_flash(APP_VENC_CHN_CFG_S *pstVencChnCfg, VENC_STREAM_S *pstStream)
{
    if (pstVencChnCfg->pFile == NULL) {
        char szPostfix[8] = {0};
        char szFilePath[64] = {0};
        app_ipcam_Postfix_Get(pstVencChnCfg->enType, szPostfix);
        snprintf(szFilePath, 64, "%s/Venc%d_idx_%d%s", pstVencChnCfg->SavePath, pstVencChnCfg->VencChn, pstVencChnCfg->frameNum++, szPostfix);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "update new file name: %s\n", szFilePath);
        pstVencChnCfg->pFile = fopen(szFilePath, "wb");
        if (pstVencChnCfg->pFile == NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "open file err, %s\n", szFilePath);
            return CVI_FAILURE;
        }
        if (pstVencChnCfg->frameNum >= 1000) {
            pstVencChnCfg->frameNum = 0;
        }
    }

    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "u32PackCount = %d\n", pstStream->u32PackCount);

    VENC_PACK_S *ppack;

    for (CVI_U32 i = 0; i < pstStream->u32PackCount; i++) {
        ppack = &pstStream->pstPack[i];
        fwrite(ppack->pu8Addr + ppack->u32Offset,
                ppack->u32Len - ppack->u32Offset, 1, pstVencChnCfg->pFile);

        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "pack[%d], PTS = %"PRId64", Addr = %p, Len = 0x%X, Offset = 0x%X DataType=%d\n",
                i, ppack->u64PTS, ppack->pu8Addr, ppack->u32Len, ppack->u32Offset, ppack->DataType.enH265EType);
    }

    if (pstVencChnCfg->enType == PT_JPEG) {
        fclose(pstVencChnCfg->pFile);
        pstVencChnCfg->pFile = NULL;
    } else {
        if (++pstVencChnCfg->fileNum > pstVencChnCfg->u32Duration) {
            pstVencChnCfg->fileNum = 0;
            fclose(pstVencChnCfg->pFile);
            pstVencChnCfg->pFile = NULL;
        }
    }

    return CVI_SUCCESS;
}

void app_ipcam_JpgCapFlag_Set(CVI_BOOL bEnable)
{
    bJpgCapFlag = bEnable;
    if (bEnable) {
        pthread_mutex_lock(&JpgCapMutex);
        pthread_cond_signal(&JpgCapCond);
        pthread_mutex_unlock(&JpgCapMutex);
    }
}

CVI_BOOL app_ipcam_JpgCapFlag_Get(void)
{
    return bJpgCapFlag;
}

int fpStreamingSendToRtsp(void *pData, void *pArgs)
{
#ifdef RTSP_SUPPORT
    APP_DATA_CTX_S *pstDataCtx = (APP_DATA_CTX_S *)pArgs;
    APP_DATA_PARAM_S *pstDataParam = &pstDataCtx->stDataParam;
    APP_VENC_CHN_CFG_S *pstVencChnCfg = (APP_VENC_CHN_CFG_S *)pstDataParam->pParam;
    VENC_CHN VencChn = pstVencChnCfg->VencChn;


    APP_PARAM_RTSP_T *prtspCtx = app_ipcam_Rtsp_Param_Get();

    if ((!prtspCtx->bStart[VencChn])) {
        return CVI_SUCCESS;
    }

    VENC_STREAM_S *pstStream = (VENC_STREAM_S *)pData;

    CVI_S32 s32Ret = CVI_SUCCESS;
    VENC_PACK_S *ppack;
    CVI_RTSP_DATA data;

    memset(&data, 0, sizeof(CVI_RTSP_DATA));

    data.blockCnt = pstStream->u32PackCount;
    for (CVI_U32 i = 0; i < pstStream->u32PackCount; i++) {
        ppack = &pstStream->pstPack[i];
        data.dataPtr[i] = ppack->pu8Addr + ppack->u32Offset;
        data.dataLen[i] = ppack->u32Len - ppack->u32Offset;

        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "pack[%d], PTS = %"PRId64", Addr = %p, Len = 0x%X, Offset = 0x%X DataType=%d\n",
            i, ppack->u64PTS, ppack->pu8Addr, ppack->u32Len, ppack->u32Offset, ppack->DataType.enH265EType);
    }
    if (0 == pstStream->u32PackCount) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstStream->u32PackCount is %d\n", pstStream->u32PackCount);
        return s32Ret;
    }

    if ((NULL != prtspCtx->pstServerCtx) && (NULL != prtspCtx->pstSession[VencChn])) {
        s32Ret = CVI_RTSP_WriteFrame(prtspCtx->pstServerCtx, prtspCtx->pstSession[VencChn]->video, &data);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_RTSP_WriteFrame failed\n");
        }
    }
#endif
    return CVI_SUCCESS;

}

int fpStreamingSendToWeb(void *pData, void *pArgs)
{
    #ifdef WEB_SOCKET
    app_ipcam_WebSocket_Stream_Send(pData, pArgs);
    #endif

    return CVI_SUCCESS;
}

int fpStreamingSaveToFlash(void *pData, void *pArgs)
{
    VENC_STREAM_S *pstStream = (VENC_STREAM_S *)pData;

    APP_DATA_CTX_S *pstDataCtx = (APP_DATA_CTX_S *)pArgs;
    APP_DATA_PARAM_S *pstDataParam = &pstDataCtx->stDataParam;
    APP_VENC_CHN_CFG_S *pstVencChnCfg = (APP_VENC_CHN_CFG_S *)pstDataParam->pParam;

    /* dynamic touch a file: /tmp/rec then start save sreaming to flash or SD Card */
    if (access("/tmp/rec", F_OK) == 0) {
        if (pstVencChnCfg->pFile == NULL) {
            char szPostfix[8] = {0};
            char szFilePath[100] = {0};
            app_ipcam_Postfix_Get(pstVencChnCfg->enType, szPostfix);
            sprintf(szFilePath, "%s/Venc%d_idx_%d%s", pstVencChnCfg->SavePath, pstVencChnCfg->VencChn, pstVencChnCfg->frameNum++, szPostfix);
            APP_PROF_LOG_PRINT(LEVEL_INFO, "start save file to %s\n", szFilePath);
            pstVencChnCfg->pFile = fopen(szFilePath, "wb");
            if (pstVencChnCfg->pFile == NULL) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "open file err, %s\n", szFilePath);
                return CVI_FAILURE;
            }
        }

        VENC_PACK_S *ppack;
        /* just from I-frame saving */
        if ((pstVencChnCfg->enType == PT_H264) || (pstVencChnCfg->enType == PT_H265)) {
            if (pstVencChnCfg->fileNum == 0 && pstStream->u32PackCount < 2)
                return CVI_SUCCESS;
        }
        for (CVI_U32 i = 0; i < pstStream->u32PackCount; i++) {
            ppack = &pstStream->pstPack[i];
            fwrite(ppack->pu8Addr + ppack->u32Offset,
                    ppack->u32Len - ppack->u32Offset, 1, pstVencChnCfg->pFile);

            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "pack[%d], PTS = %"PRId64", Addr = %p, Len = 0x%X, Offset = 0x%X DataType=%d\n",
                    i, ppack->u64PTS, ppack->pu8Addr, ppack->u32Len, ppack->u32Offset, ppack->DataType.enH265EType);
        }

        if (pstVencChnCfg->enType == PT_JPEG) {
            fclose(pstVencChnCfg->pFile);
            pstVencChnCfg->pFile = NULL;
            APP_PROF_LOG_PRINT(LEVEL_INFO, "End save! \n");
            remove("/tmp/rec");

        } else {
            if (++pstVencChnCfg->fileNum > pstVencChnCfg->u32Duration) {
                pstVencChnCfg->fileNum = 0;
                fclose(pstVencChnCfg->pFile);
                pstVencChnCfg->pFile = NULL;
                APP_PROF_LOG_PRINT(LEVEL_INFO, "End save! \n");
                remove("/tmp/rec");
            }
        }
    }

    return CVI_SUCCESS;
}

int fpStreamingSendToUvc(void *pData, void *pArgs)
{
#ifdef CVI_UVC_SUPPORT
    VENC_STREAM_S *pstStream = (VENC_STREAM_S *)pData;
    cvi_uvc_stream_send_data(pstStream);
#endif
    return CVI_SUCCESS;
}

int fpStreamingSendToRecord(void *pData, void *pArgs)
{
  #ifdef RECORD_SUPPORT
    VENC_STREAM_S *pstStream = (VENC_STREAM_S *)pData;

    APP_DATA_CTX_S *pstDataCtx = (APP_DATA_CTX_S *)pArgs;

    APP_DATA_PARAM_S *pstDataParam = &pstDataCtx->stDataParam;
    APP_VENC_CHN_CFG_S *pstVencChnCfg = (APP_VENC_CHN_CFG_S *)pstDataParam->pParam;
    VENC_CHN VencChn = pstVencChnCfg->VencChn;


    /* process streaming ; save to SD Card */
    app_ipcam_Record_VideoInput(VencChn, pstVencChnCfg->enType, pstStream);
    #endif

    return CVI_SUCCESS;
}

static void _Data_Handle(void *pData, void *pArgs)
{
    APP_DATA_CTX_S *pDataCtx = (APP_DATA_CTX_S *)pArgs;
    APP_DATA_PARAM_S *pstDataParam = &pDataCtx->stDataParam;

    for (int i = 0; i < APP_DATA_COMSUMES_MAX; i++) {
        if (pstDataParam->fpDataConsumes[i] != NULL) {
            pstDataParam->fpDataConsumes[i](pData, pArgs);
        }
    }

}

static CVI_S32 _Data_Save(void **dst, void *src)
{
    if(dst == NULL || src == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "dst or src is NULL\n");
        return CVI_FAILURE;
    }

    CVI_U32 i = 0;
    VENC_STREAM_S *psrc = (VENC_STREAM_S *)src;
    VENC_STREAM_S *pdst = (VENC_STREAM_S *)malloc(sizeof(VENC_STREAM_S));
    if(pdst == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pdst malloc failded\n");
        return CVI_FAILURE;
    }
    memcpy(pdst, psrc, sizeof(VENC_STREAM_S));
    pdst->pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * psrc->u32PackCount);
    if(pdst->pstPack == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pdst->pstPack malloc failded\n");
        goto EXIT1;
    }
    memset(pdst->pstPack, 0, sizeof(VENC_PACK_S) * psrc->u32PackCount);
    for(i = 0; i < psrc->u32PackCount; i++) {
        memcpy(&pdst->pstPack[i], &psrc->pstPack[i], sizeof(VENC_PACK_S));
        pdst->pstPack[i].pu8Addr = (CVI_U8 *)malloc(psrc->pstPack[i].u32Len);
        if(!pdst->pstPack[i].pu8Addr) {
            goto EXIT2;
        }
        memcpy(pdst->pstPack[i].pu8Addr, psrc->pstPack[i].pu8Addr, psrc->pstPack[i].u32Len);
    }

    *dst = (void *)pdst;

    return CVI_SUCCESS;

EXIT2:
    if(pdst->pstPack) {
        for(i = 0;i < psrc->u32PackCount; i++) {
            if(pdst->pstPack[i].pu8Addr) {
                free(pdst->pstPack[i].pu8Addr);
            }
        }
        free(pdst->pstPack);
    }

EXIT1:
    free(pdst);

    return CVI_FAILURE;
}

static CVI_S32 _Data_Free(void **src)
{
    if(src == NULL || *src == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "src is NULL\n");
        return CVI_FAILURE;
    }

    VENC_STREAM_S *psrc = NULL;
    psrc = (VENC_STREAM_S *) *src;

    if(psrc->pstPack) {
        for(CVI_U32 i = 0; i < psrc->u32PackCount; i++) {
            if(psrc->pstPack[i].pu8Addr) {
                free(psrc->pstPack[i].pu8Addr);
            }
        }
        free(psrc->pstPack);
    }
    if(psrc) {
        free(psrc);
    }

    return CVI_SUCCESS;
}

#ifndef __CV186AH__
static CVI_S32 app_ipcam_Venc_VpssStitching(VPSS_GRP VpssGrpSrc1, VPSS_CHN VpssChnSrc1, VPSS_GRP VpssGrpSrc2, VPSS_CHN VpssChnSrc2, VPSS_GRP VpssGrpDst, VPSS_CHN VpssChnDst)
{
    VPSS_CHN_ATTR_S stVpssChnAttr;
    VIDEO_FRAME_INFO_S stSrcFrame1;
    VIDEO_FRAME_INFO_S stSrcFrame2;
    CVI_S32 s32Ret = CVI_SUCCESS;

    s32Ret = CVI_VPSS_GetChnAttr(VpssGrpDst, VpssChnDst, &stVpssChnAttr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_GetChnAttr failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    // set the upper half of the image
    stVpssChnAttr.stAspectRatio.enMode = ASPECT_RATIO_MANUAL;
    stVpssChnAttr.stAspectRatio.bEnableBgColor = CVI_FALSE;
    stVpssChnAttr.stAspectRatio.stVideoRect.s32X = 0;
    stVpssChnAttr.stAspectRatio.stVideoRect.s32Y = 0;
    stVpssChnAttr.stAspectRatio.stVideoRect.u32Width = stVpssChnAttr.u32Width;
    stVpssChnAttr.stAspectRatio.stVideoRect.u32Height = stVpssChnAttr.u32Height / 2;
    s32Ret = CVI_VPSS_SetChnAttr(VpssGrpDst, VpssChnDst, &stVpssChnAttr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_SetChnAttr failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    // get frome src1, send to dest grp
    s32Ret = CVI_VPSS_GetChnFrame(VpssGrpSrc1, VpssChnSrc1, &stSrcFrame1, 3000);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) CVI_VPSS_GetChnFrame failed with %#x\n",
                    VpssGrpSrc1, VpssChnSrc1, s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_VPSS_SendFrame(VpssGrpDst, &stSrcFrame1, -1);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d) CVI_VPSS_SendFrame failed with %#x\n",
                    VpssGrpDst, s32Ret);

        CVI_VPSS_ReleaseChnFrame(VpssGrpSrc1, VpssChnSrc1, &stSrcFrame1);
        return s32Ret;
    }

    s32Ret = CVI_VPSS_ReleaseChnFrame(VpssGrpSrc1, VpssChnSrc1, &stSrcFrame1);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) CVI_VPSS_ReleaseChnFrame failed with %#x\n",
                    VpssGrpSrc1, VpssChnSrc1, s32Ret);
        return s32Ret;
    }

    // get frome dest, send to dest chn
    s32Ret = CVI_VPSS_GetChnFrame(VpssGrpDst, VpssChnDst, &stSrcFrame1, 3000);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) CVI_VPSS_GetChnFrame failed with %#x\n",
                    VpssGrpDst, VpssChnDst, s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_VPSS_SendChnFrame(VpssGrpDst, VpssChnDst, &stSrcFrame1, -1);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) CVI_VPSS_SendChnFrame failed with %#x\n",
                    VpssGrpDst, VpssChnDst, s32Ret);
        CVI_VPSS_ReleaseChnFrame(VpssGrpDst, VpssChnDst, &stSrcFrame1);
        return s32Ret;
    }

    s32Ret = CVI_VPSS_ReleaseChnFrame(VpssGrpDst, VpssChnDst, &stSrcFrame1);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) CVI_VPSS_ReleaseChnFrame failed with %#x\n",
                    VpssGrpDst, VpssChnDst, s32Ret);
        return s32Ret;
    }

    // set the bottom half of the image
    stVpssChnAttr.stAspectRatio.enMode = ASPECT_RATIO_MANUAL;
    stVpssChnAttr.stAspectRatio.bEnableBgColor = CVI_FALSE;
    stVpssChnAttr.stAspectRatio.stVideoRect.s32X = 0;
    stVpssChnAttr.stAspectRatio.stVideoRect.s32Y = stVpssChnAttr.u32Height / 2;
    stVpssChnAttr.stAspectRatio.stVideoRect.u32Width = stVpssChnAttr.u32Width;
    stVpssChnAttr.stAspectRatio.stVideoRect.u32Height = stVpssChnAttr.u32Height / 2;
    s32Ret = CVI_VPSS_SetChnAttr(VpssGrpDst, VpssChnDst, &stVpssChnAttr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_SetChnAttr failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    // get frome src2, send to dset grp
    s32Ret = CVI_VPSS_GetChnFrame(VpssGrpSrc2, VpssChnSrc2, &stSrcFrame2, 3000);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) CVI_VPSS_GetChnFrame failed with %#x\n",
                    VpssGrpSrc2, VpssChnSrc2, s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_VPSS_SendFrame(VpssGrpDst, &stSrcFrame2, -1);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d) CVI_VPSS_SendFrame failed with %#x\n",
                    VpssGrpDst, s32Ret);
        CVI_VPSS_ReleaseChnFrame(VpssGrpSrc2, VpssChnSrc2, &stSrcFrame2);
        return s32Ret;
    }

    s32Ret = CVI_VPSS_ReleaseChnFrame(VpssGrpSrc2, VpssChnSrc2, &stSrcFrame2);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) CVI_VPSS_ReleaseChnFrame failed with %#x\n",
                    VpssGrpSrc2, VpssChnSrc2, s32Ret);
        return s32Ret;
    }

    return s32Ret;
}
#endif

static void *Thread_Streaming_Proc(void *pArgs)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_VENC_CHN_CFG_S *pastVencChnCfg = (APP_VENC_CHN_CFG_S *)pArgs;
    VENC_CHN VencChn = pastVencChnCfg->VencChn;
#ifndef __CV186AH__
    CVI_S32 vpssGrp = pastVencChnCfg->VpssGrp;
    CVI_S32 vpssChn = pastVencChnCfg->VpssChn;
    CVI_S32 VpssGrpSrc1 = pastVencChnCfg->stVencStitchingCfg.VpssStitchingGrpSrc1;
    CVI_S32 VpssChnSrc1 = pastVencChnCfg->stVencStitchingCfg.VpssStitchingChnSrc1;
    CVI_S32 VpssGrpSrc2 = pastVencChnCfg->stVencStitchingCfg.VpssStitchingGrpSrc2;
    CVI_S32 VpssChnSrc2 = pastVencChnCfg->stVencStitchingCfg.VpssStitchingChnSrc2;
    CVI_BOOL bEnableVpssStitching = pastVencChnCfg->stVencStitchingCfg.bEnableVpssStitching;
#endif
    CVI_S32 iTime = GetCurTimeInMsec();

    CVI_CHAR TaskName[64] = {'\0'};
    sprintf(TaskName, "Thread_Venc%d_Proc", VencChn);
    prctl(PR_SET_NAME, TaskName, 0, 0, 0);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "Venc channel_%d start running\n", VencChn);

    CVI_S32 vencFd = 0;
    struct timeval timeoutVal;
    fd_set readFds;

    usleep(1000);

    pastVencChnCfg->bStart = CVI_TRUE;

    while (pastVencChnCfg->bStart) {
#ifndef __CV186AH__
        VIDEO_FRAME_INFO_S stVencFrame = {0};
#endif
        iTime = GetCurTimeInMsec();

        if (pastVencChnCfg->enBindMode == VENC_BIND_DISABLE) {
#ifdef __CV186AH__
            s32Ret = app_ipcam_Venc_Get_Frame(pastVencChnCfg);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Venc_Get_Frame fail\n");
                continue;
            }
#else
            if (bEnableVpssStitching) {
                s32Ret = app_ipcam_Venc_VpssStitching(VpssGrpSrc1, VpssChnSrc1, VpssGrpSrc2, VpssChnSrc2, vpssGrp, vpssChn);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Venc_VpssStitching fail\n");
                    continue;
                }
            }

            if (CVI_VPSS_GetChnFrame(vpssGrp, vpssChn, &stVencFrame, 3000) != CVI_SUCCESS) {
                continue;
            }

            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "VencChn-%d Get Frame takes %u ms \n",
                                            VencChn, (GetCurTimeInMsec() - iTime));

            if (CVI_VENC_SendFrame(VencChn, &stVencFrame, 3000) != CVI_SUCCESS) {   /* takes 0~1ms */
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "Venc send frame failed with %#x\n", s32Ret);
                s32Ret = CVI_VPSS_ReleaseChnFrame(vpssGrp, vpssChn, &stVencFrame);
                if (s32Ret != CVI_SUCCESS)
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "vpss release Chn-frame failed with:0x%x\n", s32Ret);
                continue;
            }
#endif
        }

        vencFd = CVI_VENC_GetFd(VencChn);
        if (vencFd <= 0) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VENC_GetFd failed with%#x!\n", vencFd);
            break;
        }
        FD_ZERO(&readFds);
        FD_SET(vencFd, &readFds);
        timeoutVal.tv_sec = 0;
        timeoutVal.tv_usec = 80*1000;
        iTime = GetCurTimeInMsec();
        s32Ret = select(vencFd + 1, &readFds, NULL, NULL, &timeoutVal);
        if (s32Ret < 0) {
            if (errno == EINTR)
                continue;
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "VencChn(%d) select failed!\n", VencChn);
            break;
        } else if (s32Ret == 0) {
            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "VencChn(%d) select timeout %u ms \n",
                                            VencChn, (GetCurTimeInMsec() - iTime));
            continue;
        }

        VENC_STREAM_S stStream = {0};
        stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * H26X_MAX_NUM_PACKS);
        if (stStream.pstPack == NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "streaming malloc memory failed!\n");
            break;
        }

        ISP_EXP_INFO_S stExpInfo;
        memset(&stExpInfo, 0, sizeof(stExpInfo));
        CVI_ISP_QueryExposureInfo(0, &stExpInfo);
        CVI_S32 timeout = (1000 * 2) / (stExpInfo.u32Fps / 100); //u32Fps = fps * 100
        s32Ret = CVI_VENC_GetStream(VencChn, &stStream, timeout);
        if (s32Ret != CVI_SUCCESS || (0 == stStream.u32PackCount)) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VENC_GetStream, VencChn(%d) cnt(%d), s32Ret = 0x%X timeout:%d %d\n", VencChn, stStream.u32PackCount, s32Ret, timeout, stExpInfo.u32Fps);
            free(stStream.pstPack);
            stStream.pstPack = NULL;

#ifndef __CV186AH__
            if (pastVencChnCfg->enBindMode == VENC_BIND_DISABLE) {
                CVI_VPSS_ReleaseChnFrame(vpssGrp, vpssChn, &stVencFrame);
            }
#endif
            continue;
        } else {
#ifndef __CV186AH__
            if (pastVencChnCfg->enBindMode == VENC_BIND_DISABLE) {
                CVI_VPSS_ReleaseChnFrame(vpssGrp, vpssChn, &stVencFrame);
            }
#endif
        } 
        
        if (!pastVencChnCfg->bFirstStreamTCost) {
            APP_PROF_LOG_PRINT(LEVEL_WARN, "venc(%d) from deinit to get first stream takes %u ms \n", VencChn, (GetCurTimeInMsec() - g_s32SwitchSizeiTime));
            pastVencChnCfg->bFirstStreamTCost = CVI_TRUE;
        }

        if ((1 == stStream.u32PackCount) && (stStream.pstPack[0].u32Len > P_MAX_SIZE)) {
            APP_PROF_LOG_PRINT(LEVEL_WARN, "CVI_VENC_GetStream, VencChn(%d) p oversize:%d\n", VencChn, stStream.pstPack[0].u32Len);
        } else {
            /* save streaming to LinkList and proc it in another thread */
            s32Ret = app_ipcam_LList_Data_Push(&stStream, g_pDataCtx[VencChn]);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "Venc streaming push linklist failed!\n");
            }
        }

        s32Ret = CVI_VENC_ReleaseStream(VencChn, &stStream);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VENC_ReleaseStream, s32Ret = %d\n", s32Ret);
            free(stStream.pstPack);
            stStream.pstPack = NULL;
            continue;
        }
        free(stStream.pstPack);
        stStream.pstPack = NULL;
    }

    return (CVI_VOID *) CVI_SUCCESS;

}

static void *Thread_Jpg_Proc(void *pArgs)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    APP_VENC_CHN_CFG_S *pstVencChnCfg = (APP_VENC_CHN_CFG_S *)pArgs;
    VENC_CHN VencChn = pstVencChnCfg->VencChn;

    CVI_CHAR TaskName[64];
    sprintf(TaskName, "Thread_JpegEnc");
    prctl(PR_SET_NAME, TaskName, 0, 0, 0);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "jpg encode task start \n");

    CVI_S32 vpssGrp = pstVencChnCfg->VpssGrp;
    CVI_S32 vpssChn = pstVencChnCfg->VpssChn;
    VIDEO_FRAME_INFO_S stVencFrame = {0};

    pstVencChnCfg->bStart = CVI_TRUE;

    while (pstVencChnCfg->bStart) {
        if (!pstVencChnCfg->bStart) {
            break;
        }

        if (app_ipcam_JpgCapFlag_Get()) {
            s32Ret = CVI_VPSS_GetChnFrame(vpssGrp, vpssChn, &stVencFrame, 2000);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "CVI_VPSS_GetChnFrame failed! %d\n", s32Ret);
                continue;
            }

            s32Ret = CVI_VENC_SendFrame(VencChn, &stVencFrame, 2000);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "CVI_VENC_SendFrame failed! %d\n", s32Ret);
                goto rls_frame;
            }

            VENC_STREAM_S stStream = {0};
            stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * JPEG_MAX_NUM_PACKS);
            if (stStream.pstPack == NULL) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "streaming malloc memory failed!\n");
                goto rls_frame;
            }

            s32Ret = CVI_VENC_GetStream(VencChn, &stStream, 2000);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VENC_GetStream, VencChn = %d, s32Ret = 0x%X\n", VencChn, s32Ret);
                free(stStream.pstPack);
                stStream.pstPack = NULL;
                goto rls_frame;
            } else {
                CVI_VPSS_ReleaseChnFrame(vpssGrp, vpssChn, &stVencFrame);
            }

            app_ipcam_JpgCapFlag_Set(CVI_FALSE);

            /* save jpg to flash */
            _streaming_save_to_flash(pstVencChnCfg, &stStream);

            CVI_VENC_ReleaseStream(VencChn, &stStream);
            free(stStream.pstPack);
            stStream.pstPack = NULL;

            continue;

        rls_frame:
            CVI_VPSS_ReleaseChnFrame(vpssGrp, vpssChn, &stVencFrame);
        } else {
            pthread_mutex_lock(&JpgCapMutex);
            pthread_cond_wait(&JpgCapCond, &JpgCapMutex);
            pthread_mutex_unlock(&JpgCapMutex);
        }
    }

    return (void *) CVI_SUCCESS;
}

static void app_ipcam_VencRemap(void)
{
#if defined(__CV186AH__)
#else
    APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s](%d) Enter +\n", __func__, __LINE__);
    system("echo 1 > /sys/module/cvi_vc_driver/parameters/addrRemapEn");
    system("echo 256 > /sys/module/cvi_vc_driver/parameters/ARExtraLine");
    system("echo 1 > /sys/module/cvi_vc_driver/parameters/ARMode");
#endif
    APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s](%d) Exit -\n", __func__, __LINE__);
}

int app_ipcam_Venc_Init(APP_VENC_CHN_E VencIdx)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PARAM_SYS_CFG_S *pstSysCfg = app_ipcam_Sys_Param_Get();
    CVI_BOOL bSBMEnable = pstSysCfg->astSBMCfg.bSBMEnable;
    CVI_S32 iTime;

    APP_PROF_LOG_PRINT(LEVEL_INFO, "Venc init ------------------> start \n");

    // app_ipcam_Venc_SingleEsBuff_Set();

    pthread_once(&venc_remap_once, app_ipcam_VencRemap);

    for (VENC_CHN s32ChnIdx = 0; s32ChnIdx < g_pstVencCtx->s32VencChnCnt; s32ChnIdx++) {
        APP_VENC_CHN_CFG_S *pstVencChnCfg = &g_pstVencCtx->astVencChnCfg[s32ChnIdx];
        APP_VENC_ROI_CFG_S *pstVencRoiCfg = g_pstVencCtx->astRoiCfg;
        VENC_CHN VencChn = pstVencChnCfg->VencChn;
        if ((!pstVencChnCfg->bEnable) || (pstVencChnCfg->bStart))
            continue;

        if (!((VencIdx >> s32ChnIdx) & 0x01))
            continue;

        APP_PROF_LOG_PRINT(LEVEL_INFO, "Ven_%d init info\n", VencChn);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "VpssGrp=%d VpssChn=%d size_W=%d size_H=%d CodecType=%d cmd_queue_depth=%d save_path=%s\n",
            pstVencChnCfg->VpssGrp, pstVencChnCfg->VpssChn, pstVencChnCfg->u32Width, pstVencChnCfg->u32Height,
            pstVencChnCfg->enType, pstVencChnCfg->u32CmdQueueDepth, pstVencChnCfg->SavePath);

        PAYLOAD_TYPE_E enCodecType = pstVencChnCfg->enType;
        VENC_CHN_ATTR_S stVencChnAttr, *pstVencChnAttr = &stVencChnAttr;
        memset(&stVencChnAttr, 0, sizeof(stVencChnAttr));

        /* Venc channel validity check */
        if (VencChn != pstVencChnCfg->VencChn) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"VencChn error %d\n", VencChn);
            goto VENC_EXIT0;
        }

#if defined(__CV186AH__)
        pstVencChnAttr->stGopExAttr.u32GopPreset = GOP_PRESET_IDX_IPP_SINGLE;
        pstVencChnAttr->stVencAttr.u32CmdQueueDepth = pstVencChnCfg->u32CmdQueueDepth < 16? pstVencChnCfg->u32CmdQueueDepth : 16;
#else
#endif
        s32Ret = app_ipcam_Venc_Chn_Attr_Set(&pstVencChnAttr->stVencAttr, pstVencChnCfg);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"media_venc_set_attr [%d] failed with 0x%x\n", VencChn, s32Ret);
            goto VENC_EXIT0;
        }

        s32Ret = app_ipcam_Venc_Gop_Attr_Set(&pstVencChnAttr->stGopAttr, pstVencChnCfg);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"media_venc_set_gop [%d] failed with 0x%x\n", VencChn, s32Ret);
            goto VENC_EXIT0;
        }

        s32Ret = app_ipcam_Venc_Rc_Attr_Set(&pstVencChnAttr->stRcAttr, pstVencChnCfg);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"media_venc_set_rc_attr [%d] failed with 0x%x\n", VencChn, s32Ret);
            goto VENC_EXIT0;
        }

        app_ipcam_Venc_Attr_Check(pstVencChnAttr);

        APP_PROF_LOG_PRINT(LEVEL_DEBUG,"u32Profile [%d]\n", pstVencChnAttr->stVencAttr.u32Profile);

        iTime = GetCurTimeInMsec();
        s32Ret = CVI_VENC_CreateChn(VencChn, pstVencChnAttr);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_VENC_CreateChn [%d] failed with 0x%x\n", VencChn, s32Ret);
            goto VENC_EXIT1;
        }
        APP_PROF_LOG_PRINT(LEVEL_WARN, "venc_%d CVI_VENC_CreateChn takes %u ms \n", VencChn, (GetCurTimeInMsec() - iTime));

        if ((enCodecType == PT_H265) || (enCodecType == PT_H264)) {
            if (enCodecType == PT_H264)
            {
                s32Ret = app_ipcam_Venc_H264Entropy_Set(VencChn);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_Venc_H264Entropy_Set [%d] failed with 0x%x\n", VencChn, s32Ret);
                    goto VENC_EXIT1;
                }
                s32Ret = app_ipcam_Venc_H264Trans_Set(VencChn);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_Venc_H264Trans_Set [%d] failed with 0x%x\n", VencChn, s32Ret);
                    goto VENC_EXIT1;
                }
                s32Ret = app_ipcam_Venc_H264Vui_Set(VencChn);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_Venc_H264Vui_Set [%d] failed with 0x%x\n", VencChn, s32Ret);
                    goto VENC_EXIT1;
                }
                s32Ret = app_ipcam_Venc_Roi_Set(VencChn, pstVencRoiCfg);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_Venc_Roi_Set [%d] failed with 0x%x\n", VencChn, s32Ret);
                    goto VENC_EXIT1;
                }
            } else {
                s32Ret = app_ipcam_Venc_H265Trans_Set(VencChn);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_Venc_H265Trans_Set [%d] failed with 0x%x\n", VencChn, s32Ret);
                    goto VENC_EXIT1;
                }
                s32Ret = app_ipcam_Venc_H265Vui_Set(VencChn);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_Venc_H265Vui_Set [%d] failed with 0x%x\n", VencChn, s32Ret);
                    goto VENC_EXIT1;
                }
            }
            s32Ret = app_ipcam_Venc_Rc_Param_Set(VencChn, enCodecType, pstVencChnCfg->enRcMode, &pstVencChnCfg->stRcParam);
                if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"Venc_%d RC Param Set failed with 0x%x\n", VencChn, s32Ret);
                goto VENC_EXIT1;
            }

            s32Ret = app_ipcam_Venc_FrameLost_Set(VencChn, &pstVencChnCfg->stFrameLostCtrl);
                if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"Venc_%d RC frame lost control failed with 0x%x\n", VencChn, s32Ret);
                goto VENC_EXIT1;
            }

            #if defined(__CV186AH__)
            #else
            s32Ret = app_ipcam_Venc_SvcParam_Set(VencChn, pstVencChnCfg->enRcMode,&pstVencChnCfg->stSvcParam);
                if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"Venc_%d Svc param set failed with 0x%x\n", VencChn, s32Ret);
                goto VENC_EXIT1;
            }
            #endif
        } else if (enCodecType == PT_JPEG) {
            s32Ret = app_ipcam_Venc_Jpeg_Param_Set(VencChn, &pstVencChnCfg->stJpegCodecParam);
                if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"Venc_%d JPG Param Set failed with 0x%x\n", VencChn, s32Ret);
                goto VENC_EXIT1;
            }
        }

        if (bSBMEnable) {
            if ((s32ChnIdx != 0) && (pstVencChnCfg->enBindMode != VENC_BIND_DISABLE)) {
                s32Ret = CVI_SYS_Bind(&pstVencChnCfg->astChn[0], &pstVencChnCfg->astChn[1]);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_SYS_Bind failed with %#x\n", s32Ret);
                    goto VENC_EXIT1;
                }
            }
        } else {
            if (pstVencChnCfg->enBindMode != VENC_BIND_DISABLE) {
                s32Ret = CVI_SYS_Bind(&pstVencChnCfg->astChn[0], &pstVencChnCfg->astChn[1]);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_SYS_Bind failed with %#x\n", s32Ret);
                    goto VENC_EXIT1;
                }
            }
        }

        if((enCodecType != PT_JPEG) && (enCodecType != PT_MJPEG)) {
            APP_DATA_PARAM_S stDataParam = {0};
            stDataParam.pParam = pstVencChnCfg;
            stDataParam.fpDataSave = _Data_Save;
            stDataParam.fpDataFree = _Data_Free;
            stDataParam.fpDataHandle = _Data_Handle;
            stDataParam.fpDataConsumes[0] = fpStreamingSendToRtsp;
            stDataParam.fpDataConsumes[1] = (VencChn == APP_VENC_STREAM_SUB) ? fpStreamingSendToWeb : NULL;
            stDataParam.fpDataConsumes[2] = fpStreamingSendToRecord;
            stDataParam.fpDataConsumes[3] = (VencChn == APP_VENC_STREAM_MAIN) ? fpStreamingSaveToFlash : NULL;
            stDataParam.fpDataConsumes[4] = (VencChn == APP_VENC_STREAM_SUB) ? fpStreamingSendToUvc : NULL;

            s32Ret = app_ipcam_LList_Data_Init(&g_pDataCtx[VencChn], &stDataParam);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "Link list data init failed with %#x\n", s32Ret);
                goto VENC_EXIT1;
            }
        }
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "Venc init ------------------> done \n");

    return CVI_SUCCESS;

VENC_EXIT1:
    for (VENC_CHN s32ChnIdx = 0; s32ChnIdx < g_pstVencCtx->s32VencChnCnt; s32ChnIdx++) {
        CVI_VENC_ResetChn(g_pstVencCtx->astVencChnCfg[s32ChnIdx].VencChn);
        CVI_VENC_DestroyChn(g_pstVencCtx->astVencChnCfg[s32ChnIdx].VencChn);
    }

VENC_EXIT0:
    app_ipcam_Vpss_DeInit();
    app_ipcam_Vi_DeInit();
    app_ipcam_Sys_DeInit();

    return s32Ret;
}

int app_ipcam_Venc_Stop(APP_VENC_CHN_E VencIdx)
{
    CVI_S32 s32Ret;

    APP_PROF_LOG_PRINT(LEVEL_INFO, "Venc Count=%d and will stop VencChn=0x%x\n", g_pstVencCtx->s32VencChnCnt, VencIdx);

    for (VENC_CHN s32ChnIdx = 0; s32ChnIdx < g_pstVencCtx->s32VencChnCnt; s32ChnIdx++) {
        APP_VENC_CHN_CFG_S *pstVencChnCfg = &g_pstVencCtx->astVencChnCfg[s32ChnIdx];

        if (!((VencIdx >> s32ChnIdx) & 0x01))
            continue;

        if (!pstVencChnCfg->bStart)
            continue;

        pstVencChnCfg->bStart = CVI_FALSE;
    }

    for (VENC_CHN s32ChnIdx = 0; s32ChnIdx < g_pstVencCtx->s32VencChnCnt; s32ChnIdx++) {
        APP_VENC_CHN_CFG_S *pstVencChnCfg = &g_pstVencCtx->astVencChnCfg[s32ChnIdx];
        VENC_CHN VencChn = pstVencChnCfg->VencChn;

        if (!((VencIdx >> s32ChnIdx) & 0x01))
            continue;

        /* for jpg capture */
        if (pstVencChnCfg->enType == PT_JPEG) {
            pthread_mutex_lock(&JpgCapMutex);
            pthread_cond_signal(&JpgCapCond);
            pthread_mutex_unlock(&JpgCapMutex);
        }

        if (g_Venc_pthread[VencChn] != 0) {
            pthread_join(g_Venc_pthread[VencChn], CVI_NULL);
            APP_PROF_LOG_PRINT(LEVEL_WARN, "Venc_%d Streaming Proc done \n", VencChn);
            g_Venc_pthread[VencChn] = 0;
        }

        pstVencChnCfg->bFirstStreamTCost = CVI_FALSE;

        if (pstVencChnCfg->enBindMode != VENC_BIND_DISABLE) {
            s32Ret = CVI_SYS_UnBind(&pstVencChnCfg->astChn[0], &pstVencChnCfg->astChn[1]);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_SYS_UnBind failed with %#x\n", s32Ret);
                return s32Ret;
            }
        }

        s32Ret = CVI_VENC_StopRecvFrame(VencChn);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_VENC_StopRecvFrame vechn[%d] failed with %#x\n", VencChn, s32Ret);
            return s32Ret;
        }

        s32Ret = CVI_VENC_ResetChn(VencChn);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_VENC_ResetChn vechn[%d] failed with %#x\n", VencChn, s32Ret);
            return s32Ret;
        }

        s32Ret = CVI_VENC_DestroyChn(VencChn);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_VENC_DestroyChn vechn[%d] failed with %#x\n", VencChn, s32Ret);
            return s32Ret;
        }

        PAYLOAD_TYPE_E enCodecType = pstVencChnCfg->enType;
        if((enCodecType != PT_JPEG) && (enCodecType != PT_MJPEG)) {
            s32Ret = app_ipcam_LList_Data_DeInit(&g_pDataCtx[VencChn]);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"vechn[%d] LList Data Cache DeInit failed with %#x\n", VencChn, s32Ret);
                return s32Ret;
            }
        }
    }

    return CVI_SUCCESS;
}

int app_ipcam_Venc_Start(APP_VENC_CHN_E VencIdx)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PARAM_SYS_CFG_S *pstSysCfg = app_ipcam_Sys_Param_Get();
    CVI_BOOL bSBMEnable = pstSysCfg->astSBMCfg.bSBMEnable;

    for (VENC_CHN s32ChnIdx = 0; s32ChnIdx < g_pstVencCtx->s32VencChnCnt; s32ChnIdx++) {
        APP_VENC_CHN_CFG_S *pstVencChnCfg = &g_pstVencCtx->astVencChnCfg[s32ChnIdx];
        VENC_CHN VencChn = pstVencChnCfg->VencChn;
        if ((!pstVencChnCfg->bEnable) || (pstVencChnCfg->bStart))
            continue;

        if (!((VencIdx >> s32ChnIdx) & 0x01))
            continue;

        VENC_RECV_PIC_PARAM_S stRecvParam = {0};
        stRecvParam.s32RecvPicNum = -1;

        APP_CHK_RET(CVI_VENC_StartRecvFrame(VencChn, &stRecvParam), "Start recv frame");

        if (bSBMEnable) {
            if ((s32ChnIdx == 0) && (pstVencChnCfg->enBindMode != VENC_BIND_DISABLE)) {
                s32Ret = CVI_SYS_Bind(&pstVencChnCfg->astChn[0], &pstVencChnCfg->astChn[1]);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_SYS_Bind failed with %#x\n", s32Ret);
                }
            }
        }

        pthread_attr_t pthread_attr;
        pthread_attr_init(&pthread_attr);

        // pthread_mutex_init(&pastVencChnCfg->SwitchMutex, NULL);
        pfp_task_entry fun_entry = NULL;
        if (pstVencChnCfg->enType == PT_JPEG) {
            fun_entry = Thread_Jpg_Proc;
        } else {
            struct sched_param param;
            param.sched_priority = 80;
            pthread_attr_setschedpolicy(&pthread_attr, SCHED_RR);
            pthread_attr_setschedparam(&pthread_attr, &param);
            pthread_attr_setinheritsched(&pthread_attr, PTHREAD_EXPLICIT_SCHED);
            fun_entry = Thread_Streaming_Proc;
        }

        g_Venc_pthread[VencChn] = 0;
        s32Ret = pthread_create(
                        &g_Venc_pthread[VencChn],
                        &pthread_attr,
                        fun_entry,
                        (CVI_VOID *)pstVencChnCfg);
        if (s32Ret) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Chn %d]pthread_create failed:0x%x\n", VencChn, s32Ret);
            return CVI_FAILURE;
        }
    }
    return CVI_SUCCESS;
}

int app_ipcam_VencResize_Stop(APP_VENC_CHN_E enVencChn, CVI_S32 bSubSizeReset)
{
    CVI_S32 iTime = GetCurTimeInMsec();
    g_s32SwitchSizeiTime = iTime;
    APP_PROF_LOG_PRINT(LEVEL_WARN, "stop video channle (0x%x)\n", enVencChn);

    //avoid lost record
    #ifdef RECORD_SUPPORT
    app_ipcam_Record_UnInit();
    APP_PROF_LOG_PRINT(LEVEL_WARN, "Set_Record_Status to 0 takes %u ms \n", (GetCurTimeInMsec() - iTime));
    iTime = GetCurTimeInMsec();
    #endif


    APP_CHK_RET(app_ipcam_Osdc_DeInit(), "draw rect deinit");
    APP_PROF_LOG_PRINT(LEVEL_WARN, "osdc deinit takes %u ms \n", (GetCurTimeInMsec() - iTime));

    #ifdef MD_SUPPORT
    app_ipcam_MD_Pause_Set(CVI_TRUE);
    #endif

    #ifdef AI_SUPPORT
    #ifdef PD_SUPPORT
    app_ipcam_Ai_PD_Pause_Set(CVI_TRUE);
    #endif

    #ifdef HAND_DETECT_SUPPORT
    app_ipcam_Ai_HD_Pause_Set(CVI_TRUE);
    #endif

    #ifdef CONSUMER_COUNTING_SUPPORT
    app_ipcam_Ai_Consumer_Counting_Pause_Set(CVI_TRUE);
    #endif

    #ifdef FACE_SUPPORT
    app_ipcam_Ai_FD_Pause_Set(CVI_TRUE);
    #endif
    #endif

    /*          stop RTSP server          */
    // APP_CHK_RET(app_ipcam_rtsp_Server_Destroy(), "Destory RTSP Server");

    iTime = GetCurTimeInMsec();
    #if 0
    APP_CHK_RET(app_ipcam_Vpss_DeInit(), "Vpss deinit");
    #else
    for (VENC_CHN s32ChnIdx = 0; s32ChnIdx < g_pstVencCtx->s32VencChnCnt; s32ChnIdx++) {
        // if (!((enVencChn >> s32ChnIdx) & 0x01)) continue;
        APP_VENC_CHN_CFG_S *pstVencChnCfg = &g_pstVencCtx->astVencChnCfg[s32ChnIdx];
        VPSS_GRP VpssGrp = pstVencChnCfg->VpssGrp;
        APP_VPSS_GRP_CFG_T *pstVpssGrpCfg = &app_ipcam_Vpss_Param_Get()->astVpssGrpCfg[VpssGrp];
        if (!pstVpssGrpCfg->bCreate) {
            continue;
        }
        app_ipcam_Vpss_Destroy(VpssGrp);
        if (VpssGrp != 0) app_ipcam_Vpss_Unbind(VpssGrp);     /* group0 no need unbind */
    }
    #ifdef MD_SUPPORT
    APP_PARAM_MD_CFG_S *pstAiMdCfg = app_ipcam_MD_Param_Get();
    VPSS_GRP VpssGrp_MD = pstAiMdCfg->VpssGrp;
    app_ipcam_Vpss_Unbind(VpssGrp_MD);

    if (bSubSizeReset) {
        app_ipcam_Vpss_Destroy(VpssGrp_MD);
        app_ipcam_MD_Stop();
    }
    #endif

    #ifdef AI_SUPPORT
    #ifdef CONSUMER_COUNTING_SUPPORT
    APP_PARAM_AI_CONSUMER_COUNTING_CFG_S *pstAiCountCfg = app_ipcam_Ai_Consumer_Counting_Param_Get();
    VPSS_GRP VpssGrp_COUNT = pstAiCountCfg->VpssGrp;
    app_ipcam_Vpss_Unbind(VpssGrp_COUNT);
    if (bSubSizeReset) {
        app_ipcam_Vpss_Destroy(VpssGrp_COUNT);
        app_ipcam_Ai_Consumer_Counting_Stop();
    }
    #endif

    #ifdef HAND_DETECT_SUPPORT
    APP_PARAM_AI_HD_CFG_S *pstAiHdCfg = app_ipcam_Ai_HD_Param_Get();
    VPSS_GRP VpssGrp_HD = pstAiHdCfg->VpssGrp;
    app_ipcam_Vpss_Unbind(VpssGrp_HD);
    if (bSubSizeReset) {
        app_ipcam_Vpss_Destroy(VpssGrp_HD);
        app_ipcam_Ai_HD_Stop();
    }
    #endif

    #ifdef PD_SUPPORT
    APP_PARAM_AI_PD_CFG_S *pstAiPdCfg = app_ipcam_Ai_PD_Param_Get();
    VPSS_GRP VpssGrp_PD = pstAiPdCfg->VpssGrp;
    app_ipcam_Vpss_Unbind(VpssGrp_PD);

    //MainStream not support reboot ai, avoid take a long time
    if (bSubSizeReset) {
        app_ipcam_Vpss_Destroy(VpssGrp_PD);
        app_ipcam_Ai_PD_Stop();
    }
    #endif
    #endif
    #endif
    APP_PROF_LOG_PRINT(LEVEL_WARN, "vpss deinit takes %u ms \n", (GetCurTimeInMsec() - iTime));

    iTime = GetCurTimeInMsec();
    APP_CHK_RET(app_ipcam_Venc_Stop(APP_VENC_ALL), "Venc Stop");
    APP_PROF_LOG_PRINT(LEVEL_WARN, "venc (0x%x) stop takes %u ms \n", enVencChn, (GetCurTimeInMsec() - iTime));

    return 0;
}

int app_ipcam_VencResize_Start(APP_VENC_CHN_E enVencChn, CVI_S32 bSubSizeReset)
{
    CVI_S32 iTime = GetCurTimeInMsec();
    APP_PROF_LOG_PRINT(LEVEL_WARN, "start video channle (0x%x)\n", enVencChn);

    #if 0
    APP_CHK_RET(app_ipcam_Vpss_Init(), "vpss init");
    #else
    for (VENC_CHN s32ChnIdx = 0; s32ChnIdx < g_pstVencCtx->s32VencChnCnt; s32ChnIdx++) {
        // if (!((enVencChn >> s32ChnIdx) & 0x01)) continue;
        APP_VENC_CHN_CFG_S *pstVencChnCfg = &g_pstVencCtx->astVencChnCfg[s32ChnIdx];
        VPSS_GRP VpssGrp = pstVencChnCfg->VpssGrp;
        APP_VPSS_GRP_CFG_T *pstVpssGrpCfg = &app_ipcam_Vpss_Param_Get()->astVpssGrpCfg[VpssGrp];
        if (pstVpssGrpCfg->bCreate) {
            continue;
        }
        app_ipcam_Vpss_Create(VpssGrp);
        if (VpssGrp != 0) app_ipcam_Vpss_Bind(VpssGrp);     /* group0 no need bind, because online mode */
    }

    #ifdef MD_SUPPORT
    APP_PARAM_MD_CFG_S *pstAiMdCfg = app_ipcam_MD_Param_Get();
    VPSS_GRP VpssGrp_MD = pstAiMdCfg->VpssGrp;
    app_ipcam_Vpss_Bind(VpssGrp_MD);
    if (bSubSizeReset) {
        app_ipcam_Vpss_Create(VpssGrp_MD);
        app_ipcam_MD_Start();
    }
    #endif

    #ifdef AI_SUPPORT
    #ifdef HAND_DETECT_SUPPORT
    APP_PARAM_AI_HD_CFG_S *pstAiHdCfg = app_ipcam_Ai_HD_Param_Get();
    VPSS_GRP VpssGrp_HD = pstAiHdCfg->VpssGrp;
    app_ipcam_Vpss_Bind(VpssGrp_HD);
    if (bSubSizeReset) {
        app_ipcam_Vpss_Create(VpssGrp_HD);
        app_ipcam_Ai_HD_Start();
    }
    #endif

    #ifdef CONSUMER_COUNTING_SUPPORT
    APP_PARAM_AI_CONSUMER_COUNTING_CFG_S *pstAiCountCfg = app_ipcam_Ai_Consumer_Counting_Param_Get();
    VPSS_GRP VpssGrp_COUNT = pstAiCountCfg->VpssGrp;
    app_ipcam_Vpss_Bind(VpssGrp_COUNT);
    if (bSubSizeReset) {
        app_ipcam_Vpss_Create(VpssGrp_COUNT);
        app_ipcam_Ai_Consumer_Counting_Start();
    }
    #endif
    #ifdef PD_SUPPORT
    APP_PARAM_AI_PD_CFG_S *pstAiPdCfg = app_ipcam_Ai_PD_Param_Get();
    VPSS_GRP VpssGrp_PD = pstAiPdCfg->VpssGrp;
    app_ipcam_Vpss_Bind(VpssGrp_PD);
    if (bSubSizeReset) {
        app_ipcam_Vpss_Create(VpssGrp_PD);
        app_ipcam_Ai_PD_Start();
    }
    #endif
    #endif
    #endif
    APP_PROF_LOG_PRINT(LEVEL_WARN, "vpss init takes %u ms \n", (GetCurTimeInMsec() - iTime));

    // APP_CHK_RET(app_ipcam_Rtsp_Server_Create(), "Create RTSP Server");

    iTime = GetCurTimeInMsec();
    APP_CHK_RET(app_ipcam_Venc_Init(APP_VENC_ALL), "init video encode");
    APP_PROF_LOG_PRINT(LEVEL_WARN, "venc init takes %u ms \n", (GetCurTimeInMsec() - iTime));

    iTime = GetCurTimeInMsec();
    APP_CHK_RET(app_ipcam_Venc_Start(APP_VENC_ALL), "start video processing");
    APP_PROF_LOG_PRINT(LEVEL_WARN, "venc start takes %u ms \n", (GetCurTimeInMsec() - iTime));

    iTime = GetCurTimeInMsec();
    APP_CHK_RET(app_ipcam_Osdc_Init(), "draw rect init");
    APP_PROF_LOG_PRINT(LEVEL_WARN, "osdc init takes %u ms \n", (GetCurTimeInMsec() - iTime));

    #ifdef MD_SUPPORT
    app_ipcam_MD_Pause_Set(CVI_FALSE);
    #endif

    #ifdef AI_SUPPORT
    #ifdef PD_SUPPORT
    app_ipcam_Ai_PD_Pause_Set(CVI_FALSE);
    #endif

    #ifdef HAND_DETECT_SUPPORT
    app_ipcam_Ai_HD_Pause_Set(CVI_FALSE);
    #endif

    #ifdef  CONSUMER_COUNTING_SUPPORT
    app_ipcam_Ai_Consumer_Counting_Pause_Set(CVI_FALSE);
    #endif

    #ifdef FACE_SUPPORT
    app_ipcam_Ai_FD_Pause_Set(CVI_FALSE);
    #endif
    #endif

    #ifdef RECORD_SUPPORT
    iTime = GetCurTimeInMsec();
    app_ipcam_Record_Init();
    APP_PROF_LOG_PRINT(LEVEL_WARN, "Set_Record_Status to 0 takes %u ms \n", (GetCurTimeInMsec() - iTime));
    #endif

    return CVI_SUCCESS;
}
