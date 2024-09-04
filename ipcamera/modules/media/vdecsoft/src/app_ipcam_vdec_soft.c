#include <sys/prctl.h>
#include <stdlib.h>
#include "cvi_sys.h"
#include "cvi_vo.h"
#include "app_ipcam_comm.h"
#include "app_ipcam_vdec_soft.h"

APP_PARAM_VDEC_SOFT_CTX_S g_stVdecSoftCtx, *g_pstVdecSoftCtx = &g_stVdecSoftCtx;

APP_PARAM_VDEC_SOFT_CTX_S *app_ipcam_Vdec_Soft_Param_Get() {
    return g_pstVdecSoftCtx;
}

CVI_S32 app_ipcam_Vdec_Soft_Proc(APP_VDEC_SOFT_CHN_CFG_S *pstVdecSoftChnCfg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_S32 s32UsedBytes = 0;
    CVI_S32 s32ReadLen = 0;
    CVI_U32 u32Start = 0;
    CVI_S32 bufSize = 0;
    CVI_U8  *pDecFrame = NULL;

    s32UsedBytes = pstVdecSoftChnCfg->s32UsedBytes;

    bufSize = (pstVdecSoftChnCfg->u32Width * pstVdecSoftChnCfg->u32Height * 3) >> 1;
    fseek(pstVdecSoftChnCfg->fpStrm, s32UsedBytes, SEEK_SET);
    s32ReadLen = fread(pstVdecSoftChnCfg->pstAVPacket->data, 1, bufSize, pstVdecSoftChnCfg->fpStrm);
    if (s32ReadLen == 0) {
        s32UsedBytes = 0;
        fseek(pstVdecSoftChnCfg->fpStrm, 0, SEEK_SET);
        s32ReadLen = fread(pstVdecSoftChnCfg->pstAVPacket->data, 1, bufSize, pstVdecSoftChnCfg->fpStrm);
    }

    /* CV181x VDEC support PT_JPEG/PT_MJPEG/PT_H264
           CV180X VDEC support PT_JPEG/PT_MJPEG */
    if (pstVdecSoftChnCfg->de_type == AV_CODEC_ID_H264) {
        s32Ret = h264Parse(pstVdecSoftChnCfg->pstAVPacket->data, &s32ReadLen);
        if (s32Ret != CVI_SUCCESS) {
            if (s32ReadLen >= bufSize) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"can not find H264 start code! "
                    "s32ReadLen %d, s32UsedBytes %d.!\n",
                    s32ReadLen, s32UsedBytes);
            } else {
                APP_PROF_LOG_PRINT(LEVEL_DEBUG, "No a complete framee! ");
            }
        }
    } else if (pstVdecSoftChnCfg->de_type == AV_CODEC_ID_H265) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Don't support H265.\n");
    } else if (pstVdecSoftChnCfg->de_type == AV_CODEC_ID_MJPEG) {
        s32Ret = mjpegParse(pstVdecSoftChnCfg->pstAVPacket->data, &s32ReadLen, &u32Start);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"can not find JPEG start code! "
                "s32ReadLen %d, s32UsedBytes %d.!\n",
                s32ReadLen, s32UsedBytes);
        }
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "No support de_type:%d.\n"
            , pstVdecSoftChnCfg->de_type);
        return CVI_FAILURE;
    }

    pDecFrame = pstVdecSoftChnCfg->pstAVPacket->data;
    pstVdecSoftChnCfg->pstAVPacket->data += u32Start;
    pstVdecSoftChnCfg->pstAVPacket->size = s32ReadLen;

    // Send packet to decode
    s32Ret = avcodec_send_packet(pstVdecSoftChnCfg->pAVCodecCtx, pstVdecSoftChnCfg->pstAVPacket);
    if (s32Ret < 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "send packet to codec failed. s32Ret:%d. \n",s32Ret);
        return CVI_FAILURE;
    }
    pstVdecSoftChnCfg->pstAVPacket->data = pDecFrame;
    s32UsedBytes = s32UsedBytes + s32ReadLen + u32Start;

    // Receive decoded yuv420p data
    s32Ret = avcodec_receive_frame(pstVdecSoftChnCfg->pAVCodecCtx, pstVdecSoftChnCfg->pstDecFrame);
    if (s32Ret < 0) {
        if (s32Ret == -11) {
            /* error code interpretation
             * The input decoded data is insufficient, and valid frames cannot be output.
             * Try send packet again */
            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "receive frame failed. s32Ret:%d. \n", s32Ret);
            return CVI_SUCCESS;
        }
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "receive frame failed. s32Ret:%d. \n", s32Ret);
        return CVI_FAILURE;
    }

    // Convert yuv420p to rgb565 format
    s32Ret = sws_scale(pstVdecSoftChnCfg->pstConvCtx
                    , (const uint8_t* const *) pstVdecSoftChnCfg->pstDecFrame->data
                    , pstVdecSoftChnCfg->pstDecFrame->linesize
                    , 0
                    , pstVdecSoftChnCfg->pstDecFrame->height
                    , pstVdecSoftChnCfg->pstResultFrame->data
                    , pstVdecSoftChnCfg->pstResultFrame->linesize);
    if (s32Ret != pstVdecSoftChnCfg->pstDecFrame->height) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "sws_scale failed. s32Ret:%d. \n", s32Ret);
        return CVI_FAILURE;
    }

    pstVdecSoftChnCfg->s32UsedBytes = s32UsedBytes;

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Vdec_Soft_Start(APP_VDEC_SOFT_CHN_E VencSoftIdx)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_S32 bufSize = 0;
    APP_PARAM_VDEC_SOFT_CTX_S *pstVdecSoftCtx = NULL;
    APP_VDEC_SOFT_CHN_CFG_S *pstVdecSoftChnCfg = NULL;

	pstVdecSoftCtx = app_ipcam_Vdec_Soft_Param_Get();

    APP_PROF_LOG_PRINT(LEVEL_INFO, "app_ipcam_Vdec_Soft_Start start. vdecChnCnt:%d.\n"
        , pstVdecSoftCtx->s32VdecChnCnt);

    for (CVI_S32 s32ChnIdx = 0; s32ChnIdx < pstVdecSoftCtx->s32VdecChnCnt; s32ChnIdx++) {
        pstVdecSoftChnCfg = &pstVdecSoftCtx->astVdecSoftChnCfg[s32ChnIdx];
        if (pstVdecSoftChnCfg->bEnable) {
            continue;
        }

        if (!((VencSoftIdx >> s32ChnIdx) & 0x01)) {
            continue;
        }

        pstVdecSoftChnCfg->fpStrm = fopen(pstVdecSoftChnCfg->decode_file_name, "rb");
        if (pstVdecSoftChnCfg->fpStrm == NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "open file err, filename:%s.\n"
                , pstVdecSoftChnCfg->decode_file_name);
            goto FREE_MEM;
        }
        
        pstVdecSoftChnCfg->pstAVPacket  = av_packet_alloc();
        if (!pstVdecSoftChnCfg->pstAVPacket) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAVPacket malloc failed.\n");
            goto FREE_MEM;
        }
        memset(pstVdecSoftChnCfg->pstAVPacket->data, 0, bufSize);
        
        bufSize = (pstVdecSoftChnCfg->u32Width * pstVdecSoftChnCfg->u32Height * 3) >> 1;
        pstVdecSoftChnCfg->pstAVPacket->data = (CVI_U8 *)malloc(bufSize);
        if (!pstVdecSoftChnCfg->pstAVPacket) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAVPacket malloc failed.\n");
            goto FREE_MEM;
        }
        
        pstVdecSoftChnCfg->pstDecFrame = av_frame_alloc();
        if (!pstVdecSoftChnCfg->pstDecFrame) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstDecFrame malloc failed.\n");
            goto FREE_MEM;
        }

        pstVdecSoftChnCfg->pstResultFrame = av_frame_alloc();
        if (!pstVdecSoftChnCfg->pstResultFrame) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstResultFrame malloc failed.\n");
            goto FREE_MEM;
        }
        pstVdecSoftChnCfg->pstResultFrame->format      = pstVdecSoftChnCfg->scaleFormat;
        pstVdecSoftChnCfg->pstResultFrame->width       = pstVdecSoftChnCfg->u32Width;
        pstVdecSoftChnCfg->pstResultFrame->height      = pstVdecSoftChnCfg->u32Height;
        pstVdecSoftChnCfg->pstResultFrame->linesize[0] = pstVdecSoftChnCfg->u32Width * 2;
        
        s32Ret = av_image_alloc(pstVdecSoftChnCfg->pstResultFrame->data
                            , pstVdecSoftChnCfg->pstResultFrame->linesize
                            , pstVdecSoftChnCfg->pstResultFrame->width
                            , pstVdecSoftChnCfg->pstResultFrame->height
                            , pstVdecSoftChnCfg->scaleFormat
                            , 1);
        if (s32Ret < 0) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Could not allocate raw video buffer. \n");
            goto FREE_MEM;
        }

        pstVdecSoftChnCfg->bEnable = CVI_TRUE;

        APP_PROF_LOG_PRINT(LEVEL_INFO, "app_ipcam_Vdec_Soft_Start. vdecChn:%d.\n"
            , s32ChnIdx);
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "app_ipcam_Vdec_Soft_Start done.\n");
    return CVI_SUCCESS;

FREE_MEM:
    for (CVI_S32 s32ChnIdx = 0; s32ChnIdx < pstVdecSoftCtx->s32VdecChnCnt; s32ChnIdx++) {
        if (pstVdecSoftChnCfg->pstResultFrame->data[0]) {
            av_freep(&pstVdecSoftChnCfg->pstResultFrame->data[0]);
            pstVdecSoftChnCfg->pstResultFrame->data[0] = NULL;
        }

        if (pstVdecSoftChnCfg->pstResultFrame) {
            av_frame_free(&pstVdecSoftChnCfg->pstResultFrame);
            pstVdecSoftChnCfg->pstResultFrame = NULL;
        }

        if (pstVdecSoftChnCfg->pstDecFrame) {
            av_frame_free(&pstVdecSoftChnCfg->pstDecFrame);
            pstVdecSoftChnCfg->pstDecFrame = NULL;
        }

        if (pstVdecSoftChnCfg->pstAVPacket->data) {
            free(pstVdecSoftChnCfg->pstAVPacket->data);
            pstVdecSoftChnCfg->pstAVPacket->data = NULL;
        }

        if (pstVdecSoftChnCfg->pstAVPacket) {
            av_packet_unref(pstVdecSoftChnCfg->pstAVPacket);
            av_packet_free(&pstVdecSoftChnCfg->pstAVPacket);
            pstVdecSoftChnCfg->pstAVPacket = NULL;
        }

        if (pstVdecSoftChnCfg->fpStrm) {
            fclose(pstVdecSoftChnCfg->fpStrm);
            pstVdecSoftChnCfg->fpStrm = NULL;
        }
    }

    APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Vdec_Soft_Start failed.\n");
    return CVI_FAILURE;
}

CVI_S32 app_ipcam_Vdec_Soft_Stop(APP_VDEC_SOFT_CHN_E VencSoftIdx)
{
    APP_PARAM_VDEC_SOFT_CTX_S *pstVdecSoftCtx = NULL;

	pstVdecSoftCtx = app_ipcam_Vdec_Soft_Param_Get();

    for (CVI_S32 s32ChnIdx = 0; s32ChnIdx < pstVdecSoftCtx->s32VdecChnCnt; s32ChnIdx++) {
        APP_VDEC_SOFT_CHN_CFG_S *pstVdecSoftChnCfg = &pstVdecSoftCtx->astVdecSoftChnCfg[s32ChnIdx];

        if (!((VencSoftIdx >> s32ChnIdx) & 0x01)) {
            continue;
        }

        if (!pstVdecSoftChnCfg->bEnable) {
            continue;
        }

        pstVdecSoftChnCfg->vdecChn = s32ChnIdx;
        pstVdecSoftChnCfg->bEnable = CVI_FALSE;

        if (pstVdecSoftChnCfg->pstResultFrame->data[0]) {
            av_freep(&pstVdecSoftChnCfg->pstResultFrame->data[0]);
            pstVdecSoftChnCfg->pstResultFrame->data[0] = NULL;
        }

        if (pstVdecSoftChnCfg->pstResultFrame) {
            av_frame_free(&pstVdecSoftChnCfg->pstResultFrame);
            pstVdecSoftChnCfg->pstResultFrame = NULL;
        }

        if (pstVdecSoftChnCfg->pstDecFrame) {
            av_frame_free(&pstVdecSoftChnCfg->pstDecFrame);
            pstVdecSoftChnCfg->pstDecFrame = NULL;
        }

        if (pstVdecSoftChnCfg->pstAVPacket->data) {
            free(pstVdecSoftChnCfg->pstAVPacket->data);
            pstVdecSoftChnCfg->pstAVPacket->data = NULL;
        }

        if (pstVdecSoftChnCfg->pstAVPacket) {
            av_packet_unref(pstVdecSoftChnCfg->pstAVPacket);
            av_packet_free(&pstVdecSoftChnCfg->pstAVPacket);
            pstVdecSoftChnCfg->pstAVPacket = NULL;
        }

        if (pstVdecSoftChnCfg->fpStrm) {
            fclose(pstVdecSoftChnCfg->fpStrm);
            pstVdecSoftChnCfg->fpStrm = NULL;
        }
    }
    APP_PROF_LOG_PRINT(LEVEL_INFO, "app_ipcam_Vdec_Soft_Stop done.\n");

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Vdec_Soft_Init(CVI_VOID)
{
    APP_PARAM_VDEC_SOFT_CTX_S *pstVdecSoftCtx = NULL;
	pstVdecSoftCtx = app_ipcam_Vdec_Soft_Param_Get();

    APP_PROF_LOG_PRINT(LEVEL_INFO, "Vdec soft init ------------------> start \n");

    for (CVI_S32 s32ChnIdx = 0; s32ChnIdx < pstVdecSoftCtx->s32VdecChnCnt; s32ChnIdx++) {
        APP_VDEC_SOFT_CHN_CFG_S *pstVdecSoftChnCfg = &pstVdecSoftCtx->astVdecSoftChnCfg[s32ChnIdx];

        enum AVCodecID codecID = pstVdecSoftChnCfg->de_type;
        pstVdecSoftChnCfg->pAVCodec = avcodec_find_decoder(codecID);
        if (!pstVdecSoftChnCfg->pAVCodec) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Can not find codec:%d\n", pstVdecSoftChnCfg->de_type);
            goto INIT_EXIT;
        }
        
        pstVdecSoftChnCfg->pAVCodecCtx = avcodec_alloc_context3(pstVdecSoftChnCfg->pAVCodec);
        if (!pstVdecSoftChnCfg->pAVCodecCtx) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Failed to alloc codec context.");
            goto INIT_EXIT;
        }
            
        if (avcodec_open2(pstVdecSoftChnCfg->pAVCodecCtx, pstVdecSoftChnCfg->pAVCodec, NULL) < 0){
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Failed to open decoder");
            goto INIT_EXIT;
        }

        pstVdecSoftChnCfg->pstConvCtx = sws_getContext(pstVdecSoftChnCfg->u32Width
                                                    , pstVdecSoftChnCfg->u32Height
                                                    , pstVdecSoftChnCfg->videoFormat
                                                    , pstVdecSoftChnCfg->u32Width
                                                    , pstVdecSoftChnCfg->u32Height
                                                    , pstVdecSoftChnCfg->scaleFormat
                                                    , SWS_FAST_BILINEAR
                                                    , NULL, NULL, NULL);
        if (!pstVdecSoftChnCfg->pstConvCtx) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Could not initialize the conversion context\n");
            goto INIT_EXIT;
        }
        continue;

    INIT_EXIT:
        if (!pstVdecSoftChnCfg->pstConvCtx) {
            sws_freeContext(pstVdecSoftChnCfg->pstConvCtx);
            pstVdecSoftChnCfg->pstConvCtx = NULL;
        }

        if (pstVdecSoftChnCfg->pAVCodecCtx) {
            avcodec_close(pstVdecSoftChnCfg->pAVCodecCtx);
            avcodec_free_context(&pstVdecSoftChnCfg->pAVCodecCtx);
            pstVdecSoftChnCfg->pAVCodecCtx = NULL;
            pstVdecSoftChnCfg->pAVCodec = NULL;
        }

        return CVI_FAILURE;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "Vdec soft init ------------------> done \n");
    
    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Vdec_Soft_DeInit(CVI_VOID)
{
    APP_PARAM_VDEC_SOFT_CTX_S *pstVdecSoftCtx = NULL;

	pstVdecSoftCtx = app_ipcam_Vdec_Soft_Param_Get();

    APP_PROF_LOG_PRINT(LEVEL_INFO, "Vdec soft deinit ------------------> start \n");

    for (CVI_S32 s32ChnIdx = 0; s32ChnIdx < pstVdecSoftCtx->s32VdecChnCnt; s32ChnIdx++) {
        APP_VDEC_SOFT_CHN_CFG_S *pstVdecSoftChnCfg = &pstVdecSoftCtx->astVdecSoftChnCfg[s32ChnIdx];
        if (!pstVdecSoftChnCfg->pstConvCtx) {
            sws_freeContext(pstVdecSoftChnCfg->pstConvCtx);
            pstVdecSoftChnCfg->pstConvCtx = NULL;
        }

        if (pstVdecSoftChnCfg->pAVCodecCtx) {
            avcodec_close(pstVdecSoftChnCfg->pAVCodecCtx);
            avcodec_free_context(&pstVdecSoftChnCfg->pAVCodecCtx);
            pstVdecSoftChnCfg->pAVCodecCtx = NULL;
            pstVdecSoftChnCfg->pAVCodec = NULL;
        }
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "Vdec soft deinit ------------------> done \n");

    return CVI_SUCCESS;
}
