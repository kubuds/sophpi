#include "app_ipcam_comm.h"
#include "app_ipcam_sys.h"
#include "app_ipcam_vi.h"
#include "app_ipcam_vpss.h"
#include "app_ipcam_dwa.h"

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
APP_PARAM_DWA_CFG_T g_stDwaCfg;
APP_PARAM_DWA_CFG_T *g_pstDwaCfg = &g_stDwaCfg;

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

APP_PARAM_DWA_CFG_T *app_ipcam_Dwa_Param_Get(void)
{
    return g_pstDwaCfg;
}

int app_ipcam_Ldc_Init(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PARAM_DWA_CFG_T *pastDwaCfg = app_ipcam_Dwa_Param_Get();
	if (pastDwaCfg->bUserEnable == 1) {
		return 0;
	}
    for (CVI_U32 CfgIdx = 0; CfgIdx < pastDwaCfg->u32CfgCnt; CfgIdx++) {
        APP_LDC_CFG_T *pLdcCfg = &pastDwaCfg->astDwaCfg[CfgIdx].LdcCfg;
        if (!pLdcCfg->stLdcAttr.bEnable)
            continue;
        if (pLdcCfg->stMod.bind_mod_id == 0) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "vi dev[%d] chn:[%d] start gen mesh\n", pLdcCfg->stMod.bind_dev_id, pLdcCfg->stMod.bind_chn_id);
            s32Ret = CVI_VI_SetChnLDCAttr(pLdcCfg->stMod.bind_dev_id, pLdcCfg->stMod.bind_dev_id, (VI_LDC_ATTR_S*)&pLdcCfg->stLdcAttr);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VI_SetChnLDCAttr failed with %#x!\n", s32Ret);
                return s32Ret;
            }
            APP_PROF_LOG_PRINT(LEVEL_INFO, "vi dev[%d] chn:[%d] done gen mesh\n", pLdcCfg->stMod.bind_dev_id, pLdcCfg->stMod.bind_chn_id);
        } else if (pLdcCfg->stMod.bind_mod_id == 1) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "vpss grp[%d] chn:[%d] start gen mesh\n", pLdcCfg->stMod.bind_dev_id, pLdcCfg->stMod.bind_chn_id);
            s32Ret = CVI_VPSS_SetChnLDCAttr(pLdcCfg->stMod.bind_dev_id, pLdcCfg->stMod.bind_chn_id, (VPSS_LDC_ATTR_S*)&pLdcCfg->stLdcAttr);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_SetChnLDCAttr failed with %#x!\n", s32Ret);
                return s32Ret;
            }
            APP_PROF_LOG_PRINT(LEVEL_INFO, "vpss grp[%d] chn:[%d] done gen mesh\n", pLdcCfg->stMod.bind_dev_id, pLdcCfg->stMod.bind_chn_id);
        }
    }

    return s32Ret;
}

static inline CVI_VOID COMMON_GetPicBufferConfig(CVI_U32 u32Width, CVI_U32 u32Height,
	PIXEL_FORMAT_E enPixelFormat, DATA_BITWIDTH_E enBitWidth,
	COMPRESS_MODE_E enCmpMode, CVI_U32 u32Align, VB_CAL_CONFIG_S *pstCalConfig)
{
	CVI_U8  u8BitWidth = 0;
	CVI_U32 u32VBSize = 0;
	CVI_U32 u32AlignHeight = 0;
	CVI_U32 u32MainStride = 0;
	CVI_U32 u32CStride = 0;
	CVI_U32 u32MainSize = 0;
	CVI_U32 u32YSize = 0;
	CVI_U32 u32CSize = 0;

	/* u32Align: 0 is automatic mode, alignment size following system. Non-0 for specified alignment size */
	if (u32Align == 0)
		u32Align = DEFAULT_ALIGN;
	else if (u32Align > MAX_ALIGN)
		u32Align = MAX_ALIGN;

	switch (enBitWidth) {
	case DATA_BITWIDTH_8: {
		u8BitWidth = 8;
		break;
	}
	case DATA_BITWIDTH_10: {
		u8BitWidth = 10;
		break;
	}
	case DATA_BITWIDTH_12: {
		u8BitWidth = 12;
		break;
	}
	case DATA_BITWIDTH_14: {
		u8BitWidth = 14;
		break;
	}
	case DATA_BITWIDTH_16: {
		u8BitWidth = 16;
		break;
	}
	default: {
		u8BitWidth = 0;
		break;
	}
	}

	if ((enPixelFormat == PIXEL_FORMAT_YUV_PLANAR_420)
	 || (enPixelFormat == PIXEL_FORMAT_NV12)
	 || (enPixelFormat == PIXEL_FORMAT_NV21)) {
		u32AlignHeight = ALIGN(u32Height, 2);
	} else
		u32AlignHeight = u32Height;

	if (enCmpMode == COMPRESS_MODE_NONE) {
		u32MainStride = ALIGN((u32Width * u8BitWidth + 7) >> 3, u32Align);
		u32YSize = u32MainStride * u32AlignHeight;

		if (enPixelFormat == PIXEL_FORMAT_YUV_PLANAR_420) {
			u32CStride = ALIGN(((u32Width >> 1) * u8BitWidth + 7) >> 3, u32Align);
			u32CSize = (u32CStride * u32AlignHeight) >> 1;

			u32MainStride = u32CStride * 2;
			u32YSize = u32MainStride * u32AlignHeight;
			u32MainSize = u32YSize + (u32CSize << 1);
			pstCalConfig->plane_num = 3;
		} else if (enPixelFormat == PIXEL_FORMAT_YUV_PLANAR_422) {
			u32CStride = ALIGN(((u32Width >> 1) * u8BitWidth + 7) >> 3, u32Align);
			u32CSize = u32CStride * u32AlignHeight;

			u32MainSize = u32YSize + (u32CSize << 1);
			pstCalConfig->plane_num = 3;
		} else if (enPixelFormat == PIXEL_FORMAT_RGB_888_PLANAR ||
			   enPixelFormat == PIXEL_FORMAT_BGR_888_PLANAR ||
			   enPixelFormat == PIXEL_FORMAT_HSV_888_PLANAR ||
			   enPixelFormat == PIXEL_FORMAT_YUV_PLANAR_444) {
			u32CStride = u32MainStride;
			u32CSize = u32YSize;

			u32MainSize = u32YSize + (u32CSize << 1);
			pstCalConfig->plane_num = 3;
		} else if (enPixelFormat == PIXEL_FORMAT_RGB_BAYER_12BPP) {
			u32MainSize = u32YSize;
			pstCalConfig->plane_num = 1;
		} else if (enPixelFormat == PIXEL_FORMAT_YUV_400) {
			u32MainSize = u32YSize;
			pstCalConfig->plane_num = 1;
		} else if (enPixelFormat == PIXEL_FORMAT_NV12 || enPixelFormat == PIXEL_FORMAT_NV21) {
			u32CStride = ALIGN((u32Width * u8BitWidth + 7) >> 3, u32Align);
			u32CSize = (u32CStride * u32AlignHeight) >> 1;

			u32MainSize = u32YSize + u32CSize;
			pstCalConfig->plane_num = 2;
		} else if (enPixelFormat == PIXEL_FORMAT_NV16 || enPixelFormat == PIXEL_FORMAT_NV61) {
			u32CStride = ALIGN((u32Width * u8BitWidth + 7) >> 3, u32Align);
			u32CSize = u32CStride * u32AlignHeight;

			u32MainSize = u32YSize + u32CSize;
			pstCalConfig->plane_num = 2;
		} else if (enPixelFormat == PIXEL_FORMAT_YUYV || enPixelFormat == PIXEL_FORMAT_YVYU ||
			   enPixelFormat == PIXEL_FORMAT_UYVY || enPixelFormat == PIXEL_FORMAT_VYUY) {
			u32MainStride = ALIGN(((u32Width * u8BitWidth + 7) >> 3) * 2, u32Align);
			u32YSize = u32MainStride * u32AlignHeight;
			u32MainSize = u32YSize;
			pstCalConfig->plane_num = 1;
		} else if (enPixelFormat == PIXEL_FORMAT_ARGB_1555 || enPixelFormat == PIXEL_FORMAT_ARGB_4444) {
			// packed format
			u32MainStride = ALIGN((u32Width * 16 + 7) >> 3, u32Align);
			u32YSize = u32MainStride * u32AlignHeight;
			u32MainSize = u32YSize;
			pstCalConfig->plane_num = 1;
		} else if (enPixelFormat == PIXEL_FORMAT_ARGB_8888) {
			// packed format
			u32MainStride = ALIGN((u32Width * 32 + 7) >> 3, u32Align);
			u32YSize = u32MainStride * u32AlignHeight;
			u32MainSize = u32YSize;
			pstCalConfig->plane_num = 1;
		} else if (enPixelFormat == PIXEL_FORMAT_FP32_C3_PLANAR) {
			u32MainStride = ALIGN(((u32Width * u8BitWidth + 7) >> 3) * 4, u32Align);
			u32YSize = u32MainStride * u32AlignHeight;
			u32CStride = u32MainStride;
			u32CSize = u32YSize;
			u32MainSize = u32YSize + (u32CSize << 1);
			pstCalConfig->plane_num = 3;
		} else if (enPixelFormat == PIXEL_FORMAT_FP16_C3_PLANAR ||
				enPixelFormat == PIXEL_FORMAT_BF16_C3_PLANAR) {
			u32MainStride = ALIGN(((u32Width * u8BitWidth + 7) >> 3) * 2, u32Align);
			u32YSize = u32MainStride * u32AlignHeight;
			u32CStride = u32MainStride;
			u32CSize = u32YSize;
			u32MainSize = u32YSize + (u32CSize << 1);
			pstCalConfig->plane_num = 3;
		} else if (enPixelFormat == PIXEL_FORMAT_INT8_C3_PLANAR ||
				enPixelFormat == PIXEL_FORMAT_UINT8_C3_PLANAR) {
			u32CStride = u32MainStride;
			u32CSize = u32YSize;
			u32MainSize = u32YSize + (u32CSize << 1);
			pstCalConfig->plane_num = 3;
		} else {
			// packed format
			u32MainStride = ALIGN(((u32Width * u8BitWidth + 7) >> 3) * 3, u32Align);
			u32YSize = u32MainStride * u32AlignHeight;
			u32MainSize = u32YSize;
			pstCalConfig->plane_num = 1;
		}

		u32VBSize = u32MainSize;
	} else {
		// TODO: compression mode
		pstCalConfig->plane_num = 0;
	}

	pstCalConfig->u32VBSize = u32VBSize;
	pstCalConfig->u32MainStride = u32MainStride;
	pstCalConfig->u32CStride = u32CStride;
	pstCalConfig->u32MainYSize = u32YSize;
	pstCalConfig->u32MainCSize = u32CSize;
	pstCalConfig->u32MainSize = u32MainSize;
	pstCalConfig->u16AddrAlign = u32Align;
}

CVI_S32 DWA_COMM_PrepareFrame(SIZE_S *stSize, PIXEL_FORMAT_E enPixelFormat, VIDEO_FRAME_INFO_S *pstVideoFrame)
{
	VB_BLK blk;
	VB_CAL_CONFIG_S stVbCalConfig;

	if (pstVideoFrame == CVI_NULL) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "Null pointer!\n");
		return CVI_FAILURE;
	}

	//COMMON_GetPicBufferConfig(stSize->u32Width, stSize->u32Height, enPixelFormat, DATA_BITWIDTH_8
		//, COMPRESS_MODE_NONE, DWA_STRIDE_ALIGN, &stVbCalConfig);
	COMMON_GetPicBufferConfig(stSize->u32Width, stSize->u32Height, enPixelFormat, DATA_BITWIDTH_8
		, COMPRESS_MODE_NONE, 1, &stVbCalConfig);

	memset(pstVideoFrame, 0, sizeof(*pstVideoFrame));
	pstVideoFrame->stVFrame.enCompressMode = COMPRESS_MODE_NONE;
	pstVideoFrame->stVFrame.enPixelFormat = enPixelFormat;
	pstVideoFrame->stVFrame.enVideoFormat = VIDEO_FORMAT_LINEAR;
	pstVideoFrame->stVFrame.enColorGamut = COLOR_GAMUT_BT601;
	pstVideoFrame->stVFrame.u32Width = stSize->u32Width;
	pstVideoFrame->stVFrame.u32Height = stSize->u32Height;
	pstVideoFrame->stVFrame.u32Stride[0] = stVbCalConfig.u32MainStride;
	pstVideoFrame->stVFrame.u32Stride[1] = stVbCalConfig.u32CStride;
	pstVideoFrame->stVFrame.u32TimeRef = 0;
	pstVideoFrame->stVFrame.u64PTS = 0;
	pstVideoFrame->stVFrame.enDynamicRange = DYNAMIC_RANGE_SDR8;

	blk = CVI_VB_GetBlock(VB_INVALID_POOLID, stVbCalConfig.u32VBSize);
	if (blk == VB_INVALID_HANDLE) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "Can't acquire vb block\n");
		return CVI_FAILURE;
	}

	pstVideoFrame->u32PoolId = CVI_VB_Handle2PoolId(blk);
	pstVideoFrame->stVFrame.u32Length[0] = stVbCalConfig.u32MainYSize;
	pstVideoFrame->stVFrame.u32Length[1] = stVbCalConfig.u32MainCSize;
	pstVideoFrame->stVFrame.u64PhyAddr[0] = CVI_VB_Handle2PhysAddr(blk);
	pstVideoFrame->stVFrame.u64PhyAddr[1] = pstVideoFrame->stVFrame.u64PhyAddr[0]
		+ ALIGN(stVbCalConfig.u32MainYSize, stVbCalConfig.u16AddrAlign);
	if (stVbCalConfig.plane_num == 3) {
		pstVideoFrame->stVFrame.u32Stride[2] = stVbCalConfig.u32CStride;
		pstVideoFrame->stVFrame.u32Length[2] = stVbCalConfig.u32MainCSize;
		pstVideoFrame->stVFrame.u64PhyAddr[2] = pstVideoFrame->stVFrame.u64PhyAddr[1]
			+ ALIGN(stVbCalConfig.u32MainCSize, stVbCalConfig.u16AddrAlign);
	}
	for (int i = 0; i < 3; ++i) {
		if (pstVideoFrame->stVFrame.u32Length[i] == 0)
			continue;
		pstVideoFrame->stVFrame.pu8VirAddr[i] = CVI_SYS_Mmap(pstVideoFrame->stVFrame.u64PhyAddr[i], pstVideoFrame->stVFrame.u32Length[i]);
		memset(pstVideoFrame->stVFrame.pu8VirAddr[i], 0, pstVideoFrame->stVFrame.u32Length[i]);
		CVI_SYS_IonFlushCache(pstVideoFrame->stVFrame.u64PhyAddr[i], pstVideoFrame->stVFrame.pu8VirAddr[i], pstVideoFrame->stVFrame.u32Length[i]);
		CVI_SYS_Munmap(pstVideoFrame->stVFrame.pu8VirAddr[i], pstVideoFrame->stVFrame.u32Length[i]);
	}

	return CVI_SUCCESS;
}

static CVI_S32 dwa_add_tsk(APP_DWA_CFG_T *pstDwaCfg)
{
	FISHEYE_ATTR_S *FisheyeAttr;
	AFFINE_ATTR_S *affineAttr;
	LDC_ATTR_S *LDCAttr;
	ROTATION_E enRotation;

	CVI_S32 s32Ret = CVI_FAILURE;

	if (!pstDwaCfg) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstDwaCfg is null ptr\n");
		return CVI_FAILURE;
	}

	switch (pstDwaCfg->u32Operation) {
	case DWA_FISHEYE:
		FisheyeAttr = &pstDwaCfg->FisheyeAttr;

		s32Ret = CVI_DWA_AddCorrectionTask(pstDwaCfg->hHandle, &pstDwaCfg->stTask, FisheyeAttr);
		if (s32Ret) {
			APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_DWA_AddCorrectionTask failed!\n");
		}
		break;
	case DWA_ROT:
		enRotation = pstDwaCfg->enRotation;

		s32Ret = CVI_DWA_AddRotationTask(pstDwaCfg->hHandle, &pstDwaCfg->stTask, enRotation);
		if (s32Ret) {
			APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_DWA_AddRotationTask failed!\n");
		}
		break;
	case DWA_LDC:
		LDCAttr = &pstDwaCfg->LdcCfg.stLdcAttr.stAttr;
		enRotation = (ROTATION_E)pstDwaCfg->stTask.reserved;

		s32Ret = CVI_DWA_AddLDCTask(pstDwaCfg->hHandle, &pstDwaCfg->stTask, LDCAttr, enRotation);
		if (s32Ret) {
			APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_DWA_AddLDCTask failed!\n");
		}
		break;
	case DWA_AFFINE:
		affineAttr = &pstDwaCfg->AffineAttr.stAffineAttr;

		s32Ret = CVI_DWA_AddAffineTask(pstDwaCfg->hHandle, &pstDwaCfg->stTask, affineAttr);
		if (s32Ret) {
			APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_DWA_AddAffineTask failed!\n");
		}
		break;
	default:
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "not allow this op(%d) fail\n", pstDwaCfg->u32Operation);
		break;
	}

	return s32Ret;
}

int app_ipcam_Dwa_SendFrame(APP_DWA_CFG_T *pstDwaCfg, VIDEO_FRAME_INFO_S *pstVideoFrame) {
    CVI_S32 s32Ret = CVI_FAILURE;
    SIZE_S size_out;
    PIXEL_FORMAT_E enPixelFormat;
    APP_DWA_CFG_T *pstdwacfg = pstDwaCfg;
    size_out.u32Width = pstDwaCfg->size_out.u32Width;
    size_out.u32Height = pstDwaCfg->size_out.u32Height;
    enPixelFormat = pstDwaCfg->enPixelFormat;
    memset(pstdwacfg->stTask.au64privateData, 0, sizeof(pstdwacfg->stTask.au64privateData));
	memcpy(&pstdwacfg->stTask.stImgIn, pstVideoFrame, sizeof(pstdwacfg->stTask.stImgIn));
    s32Ret = DWA_COMM_PrepareFrame(&size_out, enPixelFormat, &pstdwacfg->stTask.stImgOut);
    if (s32Ret) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "DWA_COMM_PrepareFrame failed!\n");
        return s32Ret;
    }
    s32Ret = CVI_DWA_BeginJob(&pstdwacfg->hHandle);
    if (s32Ret) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_DWA_BeginJob failed!\n");
        return s32Ret;
    }
    s32Ret = CVI_DWA_SetJobIdentity(pstDwaCfg->hHandle, &pstDwaCfg->identity);
    if (s32Ret) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_DWA_SetJobIdentity failed!\n");
        return s32Ret;
    }
    s32Ret = dwa_add_tsk(pstdwacfg);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "dwa_add_tsk. s32Ret: 0x%x !\n", s32Ret);
        return s32Ret;
    }
    s32Ret = CVI_DWA_EndJob(pstDwaCfg->hHandle);
    if (s32Ret) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_DWA_EndJob failed!\n");
        return s32Ret;
    }

    return s32Ret;
}

int app_ipcam_Dwa_GetFrame(APP_DWA_CFG_T *pstDwaCfg, VIDEO_FRAME_INFO_S *pstVideoFrame) {
    CVI_S32 s32Ret = CVI_FAILURE;
    s32Ret = CVI_DWA_GetChnFrame(&pstDwaCfg->identity, pstVideoFrame, 5000);
    if (s32Ret) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_DWA_GetChnFrame failed!\n");
        return s32Ret;
    }

    return s32Ret;
}

int app_ipcam_Dwa_Frame_SaveToFile(const CVI_CHAR *filename, VIDEO_FRAME_INFO_S *pstVideoFrame) {
	FILE *fp;
	CVI_U32 u32len, u32DataLen;

	fp = fopen(filename, "w");
	if (fp == CVI_NULL) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "open data file error\n");
		return CVI_FAILURE;
	}
	for (int i = 0; i < 3; ++i) {
		u32DataLen = pstVideoFrame->stVFrame.u32Stride[i] * pstVideoFrame->stVFrame.u32Height;
		if (u32DataLen == 0)
			continue;
		if (i > 0 && ((pstVideoFrame->stVFrame.enPixelFormat == PIXEL_FORMAT_YUV_PLANAR_420) ||
			(pstVideoFrame->stVFrame.enPixelFormat == PIXEL_FORMAT_NV12) ||
			(pstVideoFrame->stVFrame.enPixelFormat == PIXEL_FORMAT_NV21)))
			u32DataLen >>= 1;

		pstVideoFrame->stVFrame.pu8VirAddr[i]
			= CVI_SYS_Mmap(pstVideoFrame->stVFrame.u64PhyAddr[i], pstVideoFrame->stVFrame.u32Length[i]);

		APP_PROF_LOG_PRINT(LEVEL_INFO, "plane(%d): paddr(%#"PRIx64") vaddr(%p) stride(%d)\n",
			   i, pstVideoFrame->stVFrame.u64PhyAddr[i],
			   pstVideoFrame->stVFrame.pu8VirAddr[i],
			   pstVideoFrame->stVFrame.u32Stride[i]);
		APP_PROF_LOG_PRINT(LEVEL_INFO, " data_len(%d) plane_len(%d)\n",
			      u32DataLen, pstVideoFrame->stVFrame.u32Length[i]);
		u32len = fwrite(pstVideoFrame->stVFrame.pu8VirAddr[i], u32DataLen, 1, fp);
		if (u32len <= 0) {
			APP_PROF_LOG_PRINT(LEVEL_ERROR, "fwrite data(%d) error\n", i);
			break;
		}
		CVI_SYS_Munmap(pstVideoFrame->stVFrame.pu8VirAddr[i], pstVideoFrame->stVFrame.u32Length[i]);
	}

	fclose(fp);
	return CVI_SUCCESS;
}

CVI_S32 DWAFileToFrame(SIZE_S *stSize, PIXEL_FORMAT_E enPixelFormat,
		CVI_CHAR *filename, VIDEO_FRAME_INFO_S *pstVideoFrame)
{
	VB_BLK blk;
	CVI_U32 u32len;
	CVI_S32 Ret;
	int i;
	FILE *fp;

	if (!pstVideoFrame) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstVideoFrame is null\n");
		return CVI_FAILURE;
	}

	if (!filename) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "filename is null\n");
		return CVI_FAILURE;
	}

	Ret = DWA_COMM_PrepareFrame(stSize, enPixelFormat, pstVideoFrame);
	if (Ret != CVI_SUCCESS) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "DWA_COMM_PrepareFrame FAIL, get VB fail\n");
		return CVI_FAILURE;
	}

	blk = CVI_VB_PhysAddr2Handle(pstVideoFrame->stVFrame.u64PhyAddr[0]);

	//open data file & fread into the mmap address
	fp = fopen(filename, "r");
	if (fp == CVI_NULL) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "open data file[%s] error\n", filename);
		CVI_VB_ReleaseBlock(blk);
		return CVI_FAILURE;
	}

	for (i = 0; i < 3; ++i) {
		if (pstVideoFrame->stVFrame.u32Length[i] == 0)
			continue;
		pstVideoFrame->stVFrame.pu8VirAddr[i]
			= CVI_SYS_Mmap(pstVideoFrame->stVFrame.u64PhyAddr[i], pstVideoFrame->stVFrame.u32Length[i]);
		
		u32len = fread(pstVideoFrame->stVFrame.pu8VirAddr[i], pstVideoFrame->stVFrame.u32Length[i], 1, fp);
		if (u32len <= 0) {
			APP_PROF_LOG_PRINT(LEVEL_ERROR, "file to frame: fread plane%d error\n", i);
			CVI_VB_ReleaseBlock(blk);
			Ret = CVI_FAILURE;
			break;
		}
		CVI_SYS_Munmap(pstVideoFrame->stVFrame.pu8VirAddr[i], pstVideoFrame->stVFrame.u32Length[i]);
	}

	if (Ret)
		CVI_SYS_Munmap(pstVideoFrame->stVFrame.pu8VirAddr[i], pstVideoFrame->stVFrame.u32Length[i]);

	fclose(fp);

	return Ret;
}

int app_ipcam_Dwa_SendFile(APP_DWA_CFG_T *pstDwaCfg) {
	CVI_S32 s32Ret = CVI_FAILURE;
	VIDEO_FRAME_INFO_S stInVideoFrame;
	memset(&stInVideoFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
	s32Ret = DWAFileToFrame(&pstDwaCfg->size_in, pstDwaCfg->enPixelFormat, pstDwaCfg->filename_in, &stInVideoFrame);
	if (s32Ret) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "DWAFileToFrame failed!\n");
        return s32Ret;
    }
	s32Ret = app_ipcam_Dwa_SendFrame(pstDwaCfg, &stInVideoFrame);
    if (s32Ret != CVI_SUCCESS) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Dwa_SendFrame failed!\n");
		return s32Ret;
	}

	return s32Ret;
}

int app_ipcam_Dwa_Init(void) {
    CVI_S32 s32Ret = CVI_FAILURE;
    CVI_CHAR filename[64];
	APP_PARAM_DWA_CFG_T *pastDwaCfg = app_ipcam_Dwa_Param_Get();
	if (pastDwaCfg->bUserEnable == 0) {
		return 0;
	}
    s32Ret = CVI_DWA_Init();
    if (s32Ret != CVI_SUCCESS) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_DWA_Init failed!\n");
		return s32Ret;
	}
    VIDEO_FRAME_INFO_S stInVideoFrame;
    VIDEO_FRAME_INFO_S stOutVideoFrame;
	
	// send frame 默认从 vpss 0 chn 0 get frame
    s32Ret = CVI_VPSS_GetChnFrame(0, 0, &stInVideoFrame, 1000);
    s32Ret = app_ipcam_Dwa_SendFrame(&pastDwaCfg->astDwaCfg[0], &stInVideoFrame);

	// or send file, 二选一
	// s32Ret = app_ipcam_Dwa_SendFile(&pastDwaCfg->astDwaCfg[0]);
    if (s32Ret != CVI_SUCCESS) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Dwa_SendFrame failed!\n");
		return s32Ret;
	}
    s32Ret = app_ipcam_Dwa_GetFrame(&pastDwaCfg->astDwaCfg[0], &stOutVideoFrame);
    if (s32Ret != CVI_SUCCESS) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Dwa_GetFrame failed!\n");
		return s32Ret;
	}
    snprintf(filename, 64, pastDwaCfg->astDwaCfg[0].filename_out);
    s32Ret = app_ipcam_Dwa_Frame_SaveToFile(filename, &stOutVideoFrame);
    if (s32Ret != CVI_SUCCESS) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Dwa_SaveToFile failed!\n");
		return s32Ret;
	}
    return s32Ret;
}

int app_ipcam_Dwa_DeInit(void) {
    CVI_S32 s32Ret = CVI_FAILURE;
	APP_PARAM_DWA_CFG_T *pastDwaCfg = app_ipcam_Dwa_Param_Get();
	if (pastDwaCfg->bUserEnable == 0) {
		return 0;
	}
    s32Ret = CVI_DWA_DeInit();
    if (s32Ret != CVI_SUCCESS) {
		APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_DWA_DeInit failed!\n");
		return s32Ret;
	}

    return s32Ret;
}