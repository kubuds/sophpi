
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <errno.h>
#include <math.h>
#include "cvi_sys.h"
#include "linux/cvi_comm_video.h"
#include "app_ipcam_osd.h"
#include "app_ipcam_comm.h"
#include "app_ipcam_loadbmp.h"
#include "app_ipcam_fontmod.h"
#include "app_ipcam_vi.h"
#include "app_ipcam_vpss.h"
#ifdef AI_SUPPORT
#include "app_ipcam_ai.h"
#endif

#ifdef MD_SUPPORT
#include "app_ipcam_md.h"
#endif
/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
#define OSD_LIB_FONT_W      24
#define OSD_LIB_FONT_H      24

#define OSD_EDGE_SIZE 2
#define NOASCII_CHARACTER_BYTES 2
#define BYTE_BITS               8
#define ISASCII(a)              (((a) >= 0x00 && (a) <= 0x7F) ? 1 : 0)

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/
typedef enum APP_AI_RECT_SHOW_T {
    APP_AI_ALL_RECT_HIDDEN = 0x0,
    APP_AI_PD_RECT_SHOW = 0x01,
    APP_MD_RECT_SHOW = 0x02,
    APP_AI_FD_RECT_SHOW = 0x04,
    APP_AI_ALL_RECT_SHOW = 0x07
} APP_AI_RECT_SHOW_E;

typedef struct APP_OSDC_CANVAS_CFG_T {
    CVI_BOOL createCanvas;
    CVI_U16 drawFlag;
    BITMAP_S rgnBitmap;
} APP_OSDC_CANVAS_CFG_S;

#ifdef AI_SUPPORT
typedef struct APP_OSDC_AI_RECT_RATIO_T {
    CVI_S32 VpssChn_W;
    CVI_S32 VpssChn_H;
    float ScaleX;
    float ScaleY;
} APP_OSDC_AI_RECT_RATIO_S;
#endif

#if defined MD_SUPPORT
typedef struct APP_OSDC_MD_RECT_RATIO_T {
    CVI_S32 VpssChn_W;
    CVI_S32 VpssChn_H;
    float ScaleX;
    float ScaleY;
} APP_OSDC_MD_RECT_RATIO_S;
#endif
/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static APP_PARAM_OSDC_CFG_S g_stOsdcCfg, *g_pstOsdcCfg = &g_stOsdcCfg;

static CVI_BOOL g_bOsdcThreadRun;
static pthread_t g_pthOsdcRgn;
static pthread_mutex_t OsdcMutex = PTHREAD_MUTEX_INITIALIZER;

static APP_OSDC_CANVAS_CFG_S g_stOsdcCanvasCfg = {0};
// static OSDC_DRAW_OBJ_S g_ObjsVec[OSDC_OBJS_MAX] = {0};

#ifdef MD_SUPPORT
APP_OSDC_MD_RECT_RATIO_S g_stMdRectRatio = {0};
static cvimd_object_t g_objMetaMd = {0};
#endif

#ifdef AI_SUPPORT
APP_OSDC_AI_RECT_RATIO_S g_stPdRectRatio = {0};
APP_OSDC_AI_RECT_RATIO_S g_stHdRectRatio = {0};
APP_OSDC_AI_RECT_RATIO_S g_stCountRectRatio = {0};
APP_OSDC_AI_RECT_RATIO_S g_stFdRectRatio = {0};
#ifdef PD_SUPPORT
static cvtdl_object_t g_objMetaPd = {0};
#endif
#ifdef HAND_DETECT_SUPPORT
static cvtdl_object_t g_objMetaHd = {0};
#endif
#ifdef CONSUMER_COUNTING_SUPPORT
static cvtdl_object_t g_objMetaCountHead = {0};
static cvtdl_object_t g_objMetaCountPed = {0};
#endif
#ifdef FACE_SUPPORT
static cvtdl_face_t g_objMetaFd = {0};
#endif
#endif

#ifdef WEB_SOCKET
static APP_OSDC_OBJS_INFO_S g_stOsdcPrivacy[4];
static APP_OSDC_OBJS_INFO_S *g_pstOsdcPrivacy = &g_stOsdcPrivacy[0];
#endif
/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/


#ifdef WEB_SOCKET
APP_OSDC_OBJS_INFO_S *app_ipcam_OsdcPrivacy_Param_Get(void)
{
    return g_pstOsdcPrivacy;
}
#endif

static CVI_S32 GetNonASCNum(char *string, CVI_S32 len)
{
    CVI_S32 i;
    CVI_S32 n = 0;

    for (i = 0; i < len; i++) {
        if (string[i] == '\0')
            break;
        if (!ISASCII(string[i])) {
            i++;
            n++;
        }
    }

    return n;
}

static CVI_S32 GetFontMod(char *Character, uint8_t **FontMod, CVI_S32 *FontModLen)
{
    CVI_U32 offset = 0;
    CVI_U32 areacode = 0;
    CVI_U32 bitcode = 0;

    if (ISASCII(Character[0])) {
        areacode = 3;
        bitcode = (CVI_U32)((uint8_t)Character[0] - 0x20);
    } else {
        areacode = (CVI_U32)((uint8_t)Character[0] - 0xA0);
        bitcode = (CVI_U32)((uint8_t)Character[1] - 0xA0);
    }
    offset = (94 * (areacode - 1) + (bitcode - 1)) * (OSD_LIB_FONT_W * OSD_LIB_FONT_H / 8);
    *FontMod = (uint8_t *)g_fontLib + offset;
    *FontModLen = OSD_LIB_FONT_W*OSD_LIB_FONT_H / 8;
    return CVI_SUCCESS;
}

static CVI_VOID GetTimeStr(const struct tm *pstTime, char *pazStr, CVI_S32 s32MaxLen)
{
    time_t nowTime;
    struct tm stTime = {0};

    if (!pstTime) {
        time(&nowTime);
        localtime_r(&nowTime, &stTime);
        pstTime = &stTime;
    }

    snprintf(pazStr, s32MaxLen, "%04d-%02d-%02d %02d:%02d:%02d",
        pstTime->tm_year + 1900, pstTime->tm_mon + 1, pstTime->tm_mday,
        pstTime->tm_hour, pstTime->tm_min, pstTime->tm_sec);
}

#if defined AI_SUPPORT || defined MD_SUPPORT
static CVI_VOID GetDebugStr(char *pazStr, CVI_S32 s32MaxLen)
{
    if (NULL == pazStr || 0 >= s32MaxLen) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "szStr is NULL or s32MaxLen:%d invalid!\n", s32MaxLen);
        return ;
    }

#ifdef MD_SUPPORT
    char md_str[20] = {0};
    snprintf(md_str, 20, "MD[%02dFPS]",  app_ipcam_MD_ProcFps_Get());
    strncat(pazStr,md_str,10);
#endif

#ifdef PD_SUPPORT
    char pd_str[20] = {0};
    snprintf(pd_str, 20, "PD[%02dFPS]",  app_ipcam_Ai_PD_ProcFps_Get());
    strncat(pazStr,pd_str,10);
#endif

#ifdef FACE_SUPPORT
    char fd_str[20] = {0};
    snprintf(fd_str, 20, "FD[%02dFPS]",  app_ipcam_Ai_FD_ProcFps_Get());
    strncat(pazStr,fd_str,10);
#endif

#ifdef CONSUMER_COUNTING_SUPPORT
    char count_str[50] = {0};
    CVI_S32 cur_num,entry_num,miss_num;
    app_ipcam_Ai_Consumer_Counting_People_Num_Get(&cur_num, &entry_num, &miss_num);
    snprintf(count_str, 50, "Count[%02dFPS] N:%04d E:%04d L:%04d",  app_ipcam_Ai_Consumer_Counting_ProcFps_Get(),
    cur_num,entry_num,miss_num);
    strncat(pazStr,count_str,50);
#endif
}
#endif

int app_ipcam_Osd_Bitmap_Update(char *szStr, BITMAP_S *pstBitmap, int iDateLen)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    if (NULL == szStr || NULL == pstBitmap)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "szStr/pstBitmap is NULL\n");
        return s32Ret;
    }
    CVI_U32 u32CanvasWidth, u32CanvasHeight, u32Color, u32EdgeColor;
    SIZE_S stFontSize;
    CVI_S32 s32StrLen = strnlen(szStr, APP_OSD_STR_LEN_MAX);
    CVI_S32 NonASCNum = GetNonASCNum(szStr, s32StrLen);

    u32CanvasWidth = OSD_LIB_FONT_W * (s32StrLen - NonASCNum * (NOASCII_CHARACTER_BYTES - 1));
    u32CanvasHeight = OSD_LIB_FONT_H;
    stFontSize.u32Width = OSD_LIB_FONT_W;
    stFontSize.u32Height = OSD_LIB_FONT_H;
    u32EdgeColor = 0x8000;
    u32Color = 0xffff;

    if (szStr == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "szStr NULL pointer!\n");
        return CVI_FAILURE;
    }

    uint16_t *puBmData = (uint16_t *)pstBitmap->pData;
    CVI_U32 u32BmRow, u32BmCol;

    for (u32BmRow = 0; u32BmRow < u32CanvasHeight; ++u32BmRow) {
        CVI_S32 NonASCShow = 0;

        for (u32BmCol = 0; u32BmCol < u32CanvasWidth; ++u32BmCol) {
            CVI_S32 s32BmDataIdx = (u32BmRow + OSD_EDGE_SIZE) * pstBitmap->u32Width + u32BmCol + OSD_EDGE_SIZE;
            CVI_S32 s32CharIdx = u32BmCol / stFontSize.u32Width;
            CVI_S32 s32StringIdx = s32CharIdx + NonASCShow * (NOASCII_CHARACTER_BYTES - 1);

            if (NonASCNum > 0 && s32CharIdx > 0) {
                NonASCShow = GetNonASCNum(szStr, s32StringIdx);
                s32StringIdx = s32CharIdx + NonASCShow * (NOASCII_CHARACTER_BYTES - 1);
            }
            CVI_S32 s32CharCol = (u32BmCol - (stFontSize.u32Width * s32CharIdx)) * OSD_LIB_FONT_W /
                            stFontSize.u32Width;
            CVI_S32 s32CharRow = u32BmRow * OSD_LIB_FONT_H / stFontSize.u32Height;
            CVI_S32 s32HexOffset = s32CharRow * OSD_LIB_FONT_W / BYTE_BITS + s32CharCol / BYTE_BITS;
            CVI_S32 s32BitOffset = s32CharCol % BYTE_BITS;
            uint8_t *FontMod = NULL;
            CVI_S32 FontModLen = 0;

            if (GetFontMod(&szStr[s32StringIdx], &FontMod, &FontModLen) == CVI_SUCCESS) {
                if (FontMod != NULL && s32HexOffset < FontModLen) {
                    uint8_t temp = FontMod[s32HexOffset];

                    if ((temp >> ((BYTE_BITS - 1) - s32BitOffset)) & 0x1) {
#if OSD_EDGE_SIZE
                        int offset = 0;

                        //left right
                        offset = s32BmDataIdx - OSD_EDGE_SIZE;
                        if (u32Color != puBmData[offset]) {
                            puBmData[offset] = (uint16_t)u32EdgeColor;
                        }
                        offset = s32BmDataIdx + OSD_EDGE_SIZE;
                        if (u32Color != puBmData[offset]) {
                            puBmData[offset] = (uint16_t)u32EdgeColor;
                        }

                        //up down
                        offset = s32BmDataIdx - (OSD_EDGE_SIZE * pstBitmap->u32Width);
                        if (u32Color != puBmData[offset]) {
                            puBmData[offset] = (uint16_t)u32EdgeColor;
                        }
                        offset = s32BmDataIdx + (OSD_EDGE_SIZE * pstBitmap->u32Width);
                        if (u32Color != puBmData[offset]) {
                            puBmData[offset] = (uint16_t)u32EdgeColor;
                        }

                        //angle
                        offset = s32BmDataIdx - (OSD_EDGE_SIZE * pstBitmap->u32Width) - OSD_EDGE_SIZE;
                        if (u32Color != puBmData[offset]) {
                            puBmData[offset] = (uint16_t)u32EdgeColor;
                        }
                        offset = s32BmDataIdx - (OSD_EDGE_SIZE * pstBitmap->u32Width) + OSD_EDGE_SIZE;
                        if (u32Color != puBmData[offset]) {
                            puBmData[offset] = (uint16_t)u32EdgeColor;
                        }
                        offset = s32BmDataIdx + (OSD_EDGE_SIZE * pstBitmap->u32Width) - OSD_EDGE_SIZE;
                        if (u32Color != puBmData[offset]) {
                            puBmData[offset] = (uint16_t)u32EdgeColor;
                        }
                        offset = s32BmDataIdx + (OSD_EDGE_SIZE * pstBitmap->u32Width) + OSD_EDGE_SIZE;
                        if (u32Color != puBmData[offset]) {
                            puBmData[offset] = (uint16_t)u32EdgeColor;
                        }
#endif

                        puBmData[s32BmDataIdx] = (uint16_t)u32Color;

                    }
                    //else
                        //puBmData[s32BmDataIdx] = (uint16_t)u32BgColor;
                    continue;
                }
            }
            APP_PROF_LOG_PRINT(LEVEL_INFO, "GetFontMod Fail\n");
            return CVI_FAILURE;
        }
    }

    return s32Ret;
}



CVI_S32 app_ipcam_Rgn_Mst_LoadBmp(
    const char *filename,
    BITMAP_S *pstBitmap,
    CVI_BOOL bFil,
    CVI_U32 u16FilColor,
    PIXEL_FORMAT_E enPixelFormat)
{
    OSD_SURFACE_S Surface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;
    CVI_S32 Bpp;
    CVI_U32 nColors;
    CVI_U32 u32PdataSize;

    if (GetBmpInfo(filename, &bmpFileHeader, &bmpInfo) < 0) {
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "GetBmpInfo err!\n");
        return CVI_FAILURE;
    }

    Bpp = bmpInfo.bmiHeader.biBitCount / 8;
    nColors = 0;
    if (Bpp == 1) {
        if (bmpInfo.bmiHeader.biClrUsed == 0)
            nColors = 1 << bmpInfo.bmiHeader.biBitCount;
        else
            nColors = bmpInfo.bmiHeader.biClrUsed;

        if (nColors > 256) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Number of indexed palette is over 256.");
            return CVI_FAILURE;
        }
    }

    if (enPixelFormat == PIXEL_FORMAT_ARGB_1555) {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;
    } else if (enPixelFormat == PIXEL_FORMAT_ARGB_4444) {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB4444;
    } else if (enPixelFormat == PIXEL_FORMAT_ARGB_8888) {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB8888;
    } else if (enPixelFormat == PIXEL_FORMAT_8BIT_MODE) {
        Surface.enColorFmt = OSD_COLOR_FMT_8BIT_MODE;
    }  else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Pixel format is not support!\n");
        return CVI_FAILURE;
    }

    u32PdataSize = Bpp * (bmpInfo.bmiHeader.biWidth) * (bmpInfo.bmiHeader.biHeight);
    pstBitmap->pData = malloc(u32PdataSize);
    if (pstBitmap->pData == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "malloc osd memory err!\n");
        return CVI_FAILURE;
    }

   if (0 != CreateSurfaceByBitMap(filename, &Surface, (CVI_U8 *)(pstBitmap->pData))) {
       APP_PROF_LOG_PRINT(LEVEL_ERROR, "CreateSurfaceByBitMap failed!\n");
       return CVI_FAILURE;
   }

    pstBitmap->u32Width = Surface.u16Width;
    pstBitmap->u32Height = Surface.u16Height;
    pstBitmap->enPixelFormat = enPixelFormat;

    // if pixel value match color, make it transparent.
    // Only works for ARGB1555
    if (bFil) {
        CVI_U32 i, j;
        CVI_U16 *pu16Temp;

        pu16Temp = (CVI_U16 *)pstBitmap->pData;
        for (i = 0; i < pstBitmap->u32Height; i++) {
            for (j = 0; j < pstBitmap->u32Width; j++) {
                if (u16FilColor == *pu16Temp)
                    *pu16Temp &= 0x7FFF;

                pu16Temp++;
            }
        }
    }

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Rgn_Mst_Canvas_Update(
    const char *filename,
    BITMAP_S *pstBitmap,
    CVI_BOOL bFil,
    CVI_U32 u16FilColor,
    SIZE_S *pstSize,
    CVI_U32 u32Stride,
    PIXEL_FORMAT_E enPixelFormat)
{
    OSD_SURFACE_S Surface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    if (GetBmpInfo(filename, &bmpFileHeader, &bmpInfo) < 0) {
        APP_PROF_LOG_PRINT(LEVEL_WARN,"GetBmpInfo err!\n");
        return CVI_FAILURE;
    }

    if (enPixelFormat == PIXEL_FORMAT_ARGB_1555) {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;
    } else if (enPixelFormat == PIXEL_FORMAT_ARGB_4444) {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB4444;
    } else if (enPixelFormat == PIXEL_FORMAT_ARGB_8888) {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB8888;
    } else {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "Pixel format is not support!\n");
        return CVI_FAILURE;
    }

    if (pstBitmap->pData == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"malloc osd memroy err!\n");
        return CVI_FAILURE;
    }

    CreateSurfaceByCanvas(filename, &Surface, (CVI_U8 *)(pstBitmap->pData)
                , pstSize->u32Width, pstSize->u32Height, u32Stride);

    pstBitmap->u32Width = Surface.u16Width;
    pstBitmap->u32Height = Surface.u16Height;
    pstBitmap->enPixelFormat = enPixelFormat;

    // if pixel value match color, make it transparent.
    // Only works for ARGB1555
    if (bFil) {
        CVI_U32 i, j;
        CVI_U16 *pu16Temp;

        pu16Temp = (CVI_U16 *)pstBitmap->pData;
        for (i = 0; i < pstBitmap->u32Height; i++) {
            for (j = 0; j < pstBitmap->u32Width; j++) {
                if (u16FilColor == *pu16Temp)
                    *pu16Temp &= 0x7FFF;

                pu16Temp++;
            }
        }
    }

    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Rgn_Canvas_Update(RGN_HANDLE Handle, const char *filename)
{
    CVI_S32 s32Ret;
    SIZE_S stSize;
    BITMAP_S stBitmap;
    RGN_CANVAS_INFO_S stCanvasInfo;

    s32Ret = CVI_RGN_GetCanvasInfo(Handle, &stCanvasInfo);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_RGN_GetCanvasInfo failed with %#x!\n", s32Ret);
        return CVI_FAILURE;
    }

    // stBitmap.pData = stCanvasInfo.pu8VirtAddr;
    stSize.u32Width = stCanvasInfo.stSize.u32Width;
    stSize.u32Height = stCanvasInfo.stSize.u32Height;

    stBitmap.pData = CVI_SYS_Mmap(stCanvasInfo.u64PhyAddr, stCanvasInfo.u32Stride * stCanvasInfo.stSize.u32Height);
    if (stBitmap.pData == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "stBitmap.pData == NULL!\n");
    }

    app_ipcam_Rgn_Mst_Canvas_Update(filename, &stBitmap, CVI_FALSE, 0,
            &stSize, stCanvasInfo.u32Stride, PIXEL_FORMAT_ARGB_1555);

    s32Ret = CVI_RGN_UpdateCanvas(Handle);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_RGN_UpdateCanvas failed with %#x!\n", s32Ret);
        CVI_SYS_Munmap(stBitmap.pData, stCanvasInfo.u32Stride * stCanvasInfo.stSize.u32Height);
        return CVI_FAILURE;
    }

    CVI_SYS_Munmap(stBitmap.pData, stCanvasInfo.u32Stride * stCanvasInfo.stSize.u32Height);

    return s32Ret;
}


APP_PARAM_OSDC_CFG_S *app_ipcam_Osdc_Param_Get(void)
{
    return g_pstOsdcCfg;
}

CVI_S32 app_ipcam_OSDCRgn_Create(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    int iOsdcIndex = 0;
    for (iOsdcIndex = 0; iOsdcIndex < OSDC_NUM_MAX; iOsdcIndex++) {
        if (g_pstOsdcCfg->bShow[iOsdcIndex]) {
            MMF_CHN_S *mmfChn = &g_pstOsdcCfg->mmfChn[iOsdcIndex];
            RGN_HANDLE handle = g_pstOsdcCfg->handle[iOsdcIndex];
            CVI_U32 u32CpsSize = g_pstOsdcCfg->CompressedSize[iOsdcIndex];

            VPSS_CHN_ATTR_S *pVpssChnAttr = &app_ipcam_Vpss_Param_Get()->astVpssGrpCfg[mmfChn->s32DevId].astVpssChnAttr[mmfChn->s32ChnId];
            CVI_U32 u32Width = pVpssChnAttr->u32Width;
            CVI_U32 u32Height = pVpssChnAttr->u32Height;
            APP_PROF_LOG_PRINT(LEVEL_INFO, "OSDC RGN handle(%d) RGN size W:%d H:%d\n", handle, u32Width, u32Height);

            for (CVI_U32 i = 0; i < g_pstOsdcCfg->osdcObjNum[iOsdcIndex]; i++) {
                if (g_pstOsdcCfg->osdcObj[iOsdcIndex][i].type == RGN_CMPR_BIT_MAP) {
                    g_pstOsdcCfg->osdcObj[iOsdcIndex][i].u64BitmapPhyAddr = (CVI_U64)0;
                    g_pstOsdcCfg->osdcObj[iOsdcIndex][i].pBitmapVirAddr = NULL;
                    g_pstOsdcCfg->osdcObj[iOsdcIndex][i].maxlen = 0;
                }
            }
            RGN_ATTR_S regAttr;
            memset(&regAttr, 0, sizeof(regAttr));
            regAttr.enType = OVERLAY_RGN;
            regAttr.unAttr.stOverlay.enPixelFormat = g_pstOsdcCfg->format[iOsdcIndex];
            regAttr.unAttr.stOverlay.stSize.u32Width = u32Width;
            regAttr.unAttr.stOverlay.stSize.u32Height = u32Height;
            regAttr.unAttr.stOverlay.u32BgColor = 0x00000000; // ARGB1555 transparent
            regAttr.unAttr.stOverlay.u32CanvasNum = 2;
#if defined(__CV186AH__)
            regAttr.unAttr.stOverlay.stCompressInfo.enOSDCompressMode = OSD_COMPRESS_MODE_SW;
            regAttr.unAttr.stOverlay.stCompressInfo.u32EstCompressedSize = u32CpsSize;
#else
            regAttr.unAttr.stOverlay.stCompressInfo.enOSDCompressMode = OSD_COMPRESS_MODE_HW;
            regAttr.unAttr.stOverlay.stCompressInfo.u32CompressedSize = u32CpsSize;
#endif
            s32Ret = CVI_RGN_Create(handle, &regAttr);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_RGN_Create failed with %#x, hdl(%d)\n", s32Ret, handle);
                return s32Ret;
            }

            CVI_BOOL bShow = g_pstOsdcCfg->bShow[iOsdcIndex];
            RGN_CHN_ATTR_S regChnAttr;
            memset(&regChnAttr, 0, sizeof(regChnAttr));
            regChnAttr.bShow = bShow;
            regChnAttr.enType = OVERLAY_RGN;
            regChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = CVI_FALSE;
            regChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 0;
            regChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 0;
            regChnAttr.unChnAttr.stOverlayChn.u32Layer = 0;
            s32Ret = CVI_RGN_AttachToChn(handle, mmfChn, &regChnAttr);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_RGN_AttachToChn failed with %#x, hdl(%d), chn(%d %d %d)\n",
                           s32Ret, handle, mmfChn->enModId, mmfChn->s32DevId, mmfChn->s32ChnId);
                CVI_RGN_Destroy(handle);
            }

        }
    }

    return s32Ret;
}

CVI_S32 app_ipcam_OSDCRgn_Destory(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    int iOsdcIndex = 0;
    for (iOsdcIndex = 0; iOsdcIndex < OSDC_NUM_MAX; iOsdcIndex++) {
        if (g_pstOsdcCfg->bShow[iOsdcIndex]) {
            RGN_HANDLE handle = g_pstOsdcCfg->handle[iOsdcIndex];
            MMF_CHN_S *mmfChn = &g_pstOsdcCfg->mmfChn[iOsdcIndex];

            s32Ret = CVI_RGN_DetachFromChn(handle, mmfChn);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_RGN_DetachFromChn failedwith %#x, hdl(%d), chn(%d %d %d)\n",
                           s32Ret, handle, mmfChn->enModId, mmfChn->s32DevId, mmfChn->s32ChnId);
                return CVI_FAILURE;
            }

            s32Ret = CVI_RGN_Destroy(handle);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_OSDCRgn_Destory failed with %#x, hdl(%d)\n", s32Ret, handle);
                return CVI_FAILURE;
            }
        }
        for (CVI_U32 i = 0; i < g_pstOsdcCfg->osdcObjNum[iOsdcIndex]; i++) {
            if (g_pstOsdcCfg->osdcObj[iOsdcIndex][i].maxlen) {
                CVI_SYS_IonFree(g_pstOsdcCfg->osdcObj[iOsdcIndex][i].u64BitmapPhyAddr, g_pstOsdcCfg->osdcObj[iOsdcIndex][i].pBitmapVirAddr);
                g_pstOsdcCfg->osdcObj[iOsdcIndex][i].u64BitmapPhyAddr = (CVI_U64)0;
                g_pstOsdcCfg->osdcObj[iOsdcIndex][i].pBitmapVirAddr = NULL;
                g_pstOsdcCfg->osdcObj[iOsdcIndex][i].maxlen = 0;
            }
        }
    }

    return s32Ret;
}

/**
 * color format: RGB8888
 * 0xffffffff   white
 * 0xff000000   black
 * 0xff0000ff   blue
 * 0xff00ff00   green
 * 0xffff0000   red
 * 0xff00ffff   cyan
 * 0xffffff00   yellow
 * 0xffff00ff   carmine
 */
static int app_ipcam_ObjsRectInfo_Update(RGN_HANDLE OsdcHandle, int iOsdcIndex)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_U32 i = 0;
    CVI_U32 OsdcObjsNum = 0;
    CVI_S32 s32StrLen = 0;
    CVI_S32 s32DataLen = 0;
    char *pszStr = NULL;
    char szStr[APP_OSD_STR_LEN_MAX] = {0};
    float s32Ratio = 1;
    BITMAP_S stBitmap;
    RGN_CANVAS_INFO_S stCanvasInfo = {0};
    s32Ret = CVI_RGN_GetCanvasInfo(OsdcHandle, &stCanvasInfo);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI RGN GetCanvasInfo failed with %#x!\n", s32Ret);
        return s32Ret;
    }
    RGN_CANVAS_CMPR_ATTR_S *pstCanvasCmprAttr = stCanvasInfo.pstCanvasCmprAttr;
    RGN_CMPR_OBJ_ATTR_S *pstObjAttr = stCanvasInfo.pstObjAttr;

    APP_PARAM_OSDC_CFG_S *stOsdcCfg = app_ipcam_Osdc_Param_Get();
    MMF_CHN_S *pstChn = &stOsdcCfg->mmfChn[iOsdcIndex];
    APP_PARAM_CHN_CFG_T *pastChnInfo = &app_ipcam_Vi_Param_Get()->astChnInfo[0];
    VPSS_CHN_ATTR_S *pVpssChnAttr = &app_ipcam_Vpss_Param_Get()->astVpssGrpCfg[pstChn->s32DevId].astVpssChnAttr[pstChn->s32ChnId];
    s32Ratio = fmax(((float)pVpssChnAttr->u32Width / (float)pastChnInfo->u32Width), ((float)pVpssChnAttr->u32Height / (float)pastChnInfo->u32Height));

#ifdef MD_SUPPORT
    if (iOsdcIndex == 0 && g_pstOsdcCfg->bShowMdRect[iOsdcIndex]) {
        app_ipcam_MD_ObjDrawInfo_Get(&g_objMetaMd);
        if (g_objMetaMd.num_boxes > 0) {
            for (i = 0; i < g_objMetaMd.num_boxes; i++) {
                if (OsdcObjsNum >= OSDC_OBJS_MAX) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "OsdcObjsNum(%d) > OSDC_OBJS_MAX(%d)!\n", OsdcObjsNum, OSDC_OBJS_MAX);
                    break;
                }

                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32X = (int)(g_stMdRectRatio.ScaleX * g_objMetaMd.p_boxes[4*i]);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32Y = (int)(g_stMdRectRatio.ScaleY * g_objMetaMd.p_boxes[4*i+1]);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Width = g_stMdRectRatio.ScaleX * (g_objMetaMd.p_boxes[4*i+2] - g_objMetaMd.p_boxes[4*i]);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Height = g_stMdRectRatio.ScaleY * (g_objMetaMd.p_boxes[4*i+3] - g_objMetaMd.p_boxes[4*i+1]);
                pstObjAttr[OsdcObjsNum].stRgnRect.u32Thick = 4;
                pstObjAttr[OsdcObjsNum].stRgnRect.u32Color = COLOR_YELLOW(0);
                pstObjAttr[OsdcObjsNum].stRgnRect.u32IsFill = CVI_FALSE;
                pstObjAttr[OsdcObjsNum].enObjType = RGN_CMPR_RECT;

                OsdcObjsNum++;
            }
            app_ipcam_Md_obj_Free(&g_objMetaMd);
        }
    }
#endif

#ifdef PD_SUPPORT
    if (iOsdcIndex == 0 && g_pstOsdcCfg->bShowPdRect[iOsdcIndex]) {
        app_ipcam_Ai_PD_ObjDrawInfo_Get(&g_objMetaPd);
        if (g_objMetaPd.size > 0) {
            for (i = 0; i < g_objMetaPd.size; i++) {
                if (OsdcObjsNum >= OSDC_OBJS_MAX) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "OsdcObjsNum(%d) > OSDC_OBJS_MAX(%d)!\n", OsdcObjsNum, OSDC_OBJS_MAX);
                    break;
                }

                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32X = (int)(g_stPdRectRatio.ScaleX * g_objMetaPd.info[i].bbox.x1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32Y = (int)(g_stPdRectRatio.ScaleY * g_objMetaPd.info[i].bbox.y1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Width = g_stPdRectRatio.ScaleX * (g_objMetaPd.info[i].bbox.x2 - g_objMetaPd.info[i].bbox.x1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Height = g_stPdRectRatio.ScaleY * (g_objMetaPd.info[i].bbox.y2 - g_objMetaPd.info[i].bbox.y1);
                pstObjAttr[OsdcObjsNum].stRgnRect.u32Thick = 4;
                if(!strcmp(g_objMetaPd.info[i].name, "Intrusion")){
                    pstObjAttr[OsdcObjsNum].stRgnRect.u32Color = COLOR_GREEN(0);
                }
                else{
                    pstObjAttr[OsdcObjsNum].stRgnRect.u32Color = COLOR_CYAN(0);
                }
                pstObjAttr[OsdcObjsNum].stRgnRect.u32IsFill = CVI_FALSE;
                pstObjAttr[OsdcObjsNum].enObjType = RGN_CMPR_RECT;

                OsdcObjsNum++;
            }
            CVI_TDL_Free(&g_objMetaPd);
        }
    }
#endif

#ifdef HAND_DETECT_SUPPORT
     if (iOsdcIndex == 0 && g_pstOsdcCfg->bShowHdRect[iOsdcIndex]) {
        app_ipcam_Ai_HD_ObjDrawInfo_Get(&g_objMetaHd);
        if (g_objMetaHd.size > 0) {
            for (i = 0; i < g_objMetaHd.size; i++) {
                if (OsdcObjsNum >= OSDC_OBJS_MAX) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "OsdcObjsNum(%d) > OSDC_OBJS_MAX(%d)!\n", OsdcObjsNum, OSDC_OBJS_MAX);
                    break;
                }

                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32X = (int)(g_stHdRectRatio.ScaleX * g_objMetaHd.info[i].bbox.x1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32Y = (int)(g_stHdRectRatio.ScaleY * g_objMetaHd.info[i].bbox.y1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Width = g_stHdRectRatio.ScaleX * (g_objMetaHd.info[i].bbox.x2 - g_objMetaHd.info[i].bbox.x1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Height = g_stHdRectRatio.ScaleY * (g_objMetaHd.info[i].bbox.y2 - g_objMetaHd.info[i].bbox.y1);
                pstObjAttr[OsdcObjsNum].stRgnRect.u32Thick = 4;
                pstObjAttr[OsdcObjsNum].stRgnRect.u32Color = COLOR_BLUE(0);
                pstObjAttr[OsdcObjsNum].stRgnRect.u32IsFill = CVI_FALSE;
                pstObjAttr[OsdcObjsNum].enObjType = RGN_CMPR_RECT;

                OsdcObjsNum++;
            }
            CVI_TDL_Free(&g_objMetaHd);
        }
    }
#endif

#ifdef CONSUMER_COUNTING_SUPPORT
     if (iOsdcIndex == 0 && g_pstOsdcCfg->bShowCountRect[iOsdcIndex]) {
        app_ipcam_Ai_Consumer_Counting_ObjDrawInfo_Get(&g_objMetaCountHead, &g_objMetaCountPed);
        if (g_objMetaCountHead.size > 0) {
            for (i = 0; i < g_objMetaCountHead.size; i++) {
                if (OsdcObjsNum >= OSDC_OBJS_MAX) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "OsdcObjsNum(%d) > OSDC_OBJS_MAX(%d)!\n", OsdcObjsNum, OSDC_OBJS_MAX);
                    break;
                }

                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32X = (int)(g_stCountRectRatio.ScaleX * g_objMetaCountHead.info[i].bbox.x1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32Y = (int)(g_stCountRectRatio.ScaleY * g_objMetaCountHead.info[i].bbox.y1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Width = g_stCountRectRatio.ScaleX * (g_objMetaCountHead.info[i].bbox.x2 - g_objMetaCountHead.info[i].bbox.x1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Height = g_stCountRectRatio.ScaleY * (g_objMetaCountHead.info[i].bbox.y2 - g_objMetaCountHead.info[i].bbox.y1);
                pstObjAttr[OsdcObjsNum].stRgnRect.u32Thick = 4;
                pstObjAttr[OsdcObjsNum].stRgnRect.u32Color = COLOR_RED(0);
                pstObjAttr[OsdcObjsNum].stRgnRect.u32IsFill = CVI_FALSE;
                pstObjAttr[OsdcObjsNum].enObjType = RGN_CMPR_RECT;

                OsdcObjsNum++;
            }
            CVI_TDL_Free(&g_objMetaCountHead);
        }

        if (g_objMetaCountPed.size > 0) {
            for (i = 0; i < g_objMetaCountPed.size; i++) {
                if (OsdcObjsNum >= OSDC_OBJS_MAX) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "OsdcObjsNum(%d) > OSDC_OBJS_MAX(%d)!\n", OsdcObjsNum, OSDC_OBJS_MAX);
                    break;
                }

                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32X = (int)(g_stCountRectRatio.ScaleX * g_objMetaCountPed.info[i].bbox.x1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32Y = (int)(g_stCountRectRatio.ScaleY * g_objMetaCountPed.info[i].bbox.y1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Width = g_stCountRectRatio.ScaleX * (g_objMetaCountPed.info[i].bbox.x2 - g_objMetaCountPed.info[i].bbox.x1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Height = g_stCountRectRatio.ScaleY * (g_objMetaCountPed.info[i].bbox.y2 - g_objMetaCountPed.info[i].bbox.y1);
                pstObjAttr[OsdcObjsNum].stRgnRect.u32Thick = 4;
                pstObjAttr[OsdcObjsNum].stRgnRect.u32Color = COLOR_BLUE(0);
                pstObjAttr[OsdcObjsNum].stRgnRect.u32IsFill = CVI_FALSE;
                pstObjAttr[OsdcObjsNum].enObjType = RGN_CMPR_RECT;

                OsdcObjsNum++;
            }
            CVI_TDL_Free(&g_objMetaCountPed);
        }
    }
#endif

#ifdef FACE_SUPPORT
    if (iOsdcIndex == 0 && g_pstOsdcCfg->bShowFdRect[iOsdcIndex]) {
        app_ipcam_Ai_FD_ObjDrawInfo_Get(&g_objMetaFd);
        if (g_objMetaFd.size > 0) {
            for (i = 0; i < g_objMetaFd.size; i++) {
                if (OsdcObjsNum >= OSDC_OBJS_MAX) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "OsdcObjsNum(%d) > OSDC_OBJS_MAX(%d)!\n", OsdcObjsNum, OSDC_OBJS_MAX);
                    return CVI_FAILURE;
                }

                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32X = (int)(g_stFdRectRatio.ScaleX * g_objMetaFd.info[i].bbox.x1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32Y = (int)(g_stFdRectRatio.ScaleY * g_objMetaFd.info[i].bbox.y1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Width = g_stFdRectRatio.ScaleX * (g_objMetaFd.info[i].bbox.x2 - g_objMetaFd.info[i].bbox.x1);
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Height = g_stFdRectRatio.ScaleY * (g_objMetaFd.info[i].bbox.y2 - g_objMetaFd.info[i].bbox.y1);
                pstObjAttr[OsdcObjsNum].stRgnRect.u32Thick = 4;
                pstObjAttr[OsdcObjsNum].stRgnRect.u32Color = COLOR_RED(0);
                pstObjAttr[OsdcObjsNum].stRgnRect.u32IsFill = CVI_FALSE;
                pstObjAttr[OsdcObjsNum].enObjType = RGN_CMPR_RECT;

                OsdcObjsNum++;
            }
            CVI_TDL_Free(&g_objMetaFd);
        }
    }
#endif

    for (i = 0; i < g_pstOsdcCfg->osdcObjNum[iOsdcIndex]; i++) {
        if (OsdcObjsNum >= OSDC_OBJS_MAX) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "OsdcObjsNum(%d) > OSDC_OBJS_MAX(%d)!\n", OsdcObjsNum, OSDC_OBJS_MAX);
            break;
        }
        if (!g_pstOsdcCfg->osdcObj[iOsdcIndex][i].bShow) {
            continue;
        }

        pstObjAttr[OsdcObjsNum].enObjType = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].type;
        OSD_TYPE_E enType = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].enType;
        if (pstObjAttr[OsdcObjsNum].enObjType == RGN_CMPR_BIT_MAP) {
            memset(&stBitmap, 0, sizeof(BITMAP_S));
            switch (enType) {
                case TYPE_PICTURE:
                    s32Ret = app_ipcam_Rgn_Mst_LoadBmp(g_pstOsdcCfg->osdcObj[iOsdcIndex][i].filename, &stBitmap, CVI_FALSE, 0, PIXEL_FORMAT_ARGB_1555);
                    if (s32Ret != CVI_SUCCESS) {
                        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "app_ipcam_Rgn_Mst_LoadBmp failed with %#x!\n", s32Ret);
                        continue;
                    }
                break;

                case TYPE_STRING:
                    s32StrLen = strlen(g_pstOsdcCfg->osdcObj[iOsdcIndex][i].str) - GetNonASCNum(g_pstOsdcCfg->osdcObj[iOsdcIndex][i].str, strlen(g_pstOsdcCfg->osdcObj[iOsdcIndex][i].str));
                    pszStr = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].str;
                    stBitmap.u32Width = (OSD_LIB_FONT_W  + (2 * OSD_EDGE_SIZE)) * s32StrLen;
                    stBitmap.u32Height = OSD_LIB_FONT_H + (2 * OSD_EDGE_SIZE);
                break;

                case TYPE_TIME:
                    memset(szStr, 0, APP_OSD_STR_LEN_MAX);
                    GetTimeStr(NULL, szStr, APP_OSD_STR_LEN_MAX);
                    s32StrLen = strnlen(szStr, APP_OSD_STR_LEN_MAX);
                    pszStr = szStr;
                    stBitmap.u32Width = (OSD_LIB_FONT_W  + (2 * OSD_EDGE_SIZE)) * s32StrLen;
                    stBitmap.u32Height = OSD_LIB_FONT_H + (2 * OSD_EDGE_SIZE);
                break;

                case TYPE_DEBUG:
                    #if defined AI_SUPPORT || defined MD_SUPPORT
                        memset(szStr, 0, APP_OSD_STR_LEN_MAX);
                        GetDebugStr(szStr, APP_OSD_STR_LEN_MAX);
                        s32StrLen = strnlen(szStr, APP_OSD_STR_LEN_MAX);
                        pszStr = szStr;
                        stBitmap.u32Width = (OSD_LIB_FONT_W  + (2 * OSD_EDGE_SIZE)) * s32StrLen;
                        stBitmap.u32Height = OSD_LIB_FONT_H + (2 * OSD_EDGE_SIZE);
                    #else
                        g_pstOsdcCfg->osdcObj[iOsdcIndex][i].bShow = 0;
                    #endif
                break;

                default:
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "OSDC enType = %d invalid \n", enType);
                break;
            }

            s32DataLen = 2 * (stBitmap.u32Width) * (stBitmap.u32Height);
            if (s32DataLen == 0) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "enType %d s32DataLen invalid!\n", enType);
                continue;
            }

            if (TYPE_PICTURE != enType) {
                stBitmap.pData = malloc(s32DataLen);
                if (stBitmap.pData == NULL) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "malloc osd memroy err!\n");
                    return -1;
                }
                memset(stBitmap.pData, 0, s32DataLen);
                s32Ret = app_ipcam_Osd_Bitmap_Update(pszStr, &stBitmap, s32DataLen);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Osd_Bitmap_Update failed!\n");
                    free(stBitmap.pData);
                    return -1;
                }
            }
            if (s32DataLen > g_pstOsdcCfg->osdcObj[iOsdcIndex][i].maxlen) {
                if(g_pstOsdcCfg->osdcObj[iOsdcIndex][i].maxlen){
                    CVI_SYS_IonFree(g_pstOsdcCfg->osdcObj[iOsdcIndex][i].u64BitmapPhyAddr, g_pstOsdcCfg->osdcObj[iOsdcIndex][i].pBitmapVirAddr);
                    g_pstOsdcCfg->osdcObj[iOsdcIndex][i].u64BitmapPhyAddr = (CVI_U64)0;
                    g_pstOsdcCfg->osdcObj[iOsdcIndex][i].pBitmapVirAddr = NULL;
                }
                s32Ret = CVI_SYS_IonAlloc(&g_pstOsdcCfg->osdcObj[iOsdcIndex][i].u64BitmapPhyAddr, (CVI_VOID **)&g_pstOsdcCfg->osdcObj[iOsdcIndex][i].pBitmapVirAddr,
                    "rgn_cmpr_bitmap2", s32DataLen);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_SYS_IonAlloc failed with %#x!\n", s32Ret);
                    free(stBitmap.pData);
                    return -1;
                }
                g_pstOsdcCfg->osdcObj[iOsdcIndex][i].maxlen = s32DataLen;
            }
            memcpy(g_pstOsdcCfg->osdcObj[iOsdcIndex][i].pBitmapVirAddr, stBitmap.pData, s32DataLen);
            pstObjAttr[OsdcObjsNum].stBitmap.stRect.s32X = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].x1 * s32Ratio;
            pstObjAttr[OsdcObjsNum].stBitmap.stRect.s32Y = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].y1 * s32Ratio;;
            pstObjAttr[OsdcObjsNum].stBitmap.stRect.u32Width = stBitmap.u32Width;
            pstObjAttr[OsdcObjsNum].stBitmap.stRect.u32Height = stBitmap.u32Height;
#ifndef __CV186AH__
            pstObjAttr[OsdcObjsNum].stBitmap.u32BitmapPAddr = (CVI_U32)g_pstOsdcCfg->osdcObj[iOsdcIndex][i].u64BitmapPhyAddr;
#else
             pstObjAttr[OsdcObjsNum].stBitmap.u64BitmapPAddr = (CVI_U64)g_pstOsdcCfg->osdcObj[iOsdcIndex][i].u64BitmapPhyAddr;
#endif
            free(stBitmap.pData);
        } else {
            pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32X = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].x1 * s32Ratio;;
            pstObjAttr[OsdcObjsNum].stRgnRect.stRect.s32Y = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].y1 * s32Ratio;;
            if (pstObjAttr[OsdcObjsNum].enObjType == RGN_CMPR_LINE) {
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Width = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].x2 * s32Ratio;;
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Height = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].y2 * s32Ratio;;
            } else if (pstObjAttr[OsdcObjsNum].enObjType == RGN_CMPR_RECT) {
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Width = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].width * s32Ratio;
                pstObjAttr[OsdcObjsNum].stRgnRect.stRect.u32Height = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].height * s32Ratio;
            }
            pstObjAttr[OsdcObjsNum].stRgnRect.u32Thick = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].thickness;
            pstObjAttr[OsdcObjsNum].stRgnRect.u32Color = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].color;
            pstObjAttr[OsdcObjsNum].stRgnRect.u32IsFill = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].filled;
#ifdef WEB_SOCKET
            if (g_pstOsdcCfg->osdcObj[iOsdcIndex][i].filled) {
                g_pstOsdcPrivacy->bShow = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].bShow;
                g_pstOsdcPrivacy->x1 = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].x1 * s32Ratio;
                g_pstOsdcPrivacy->y1 = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].y1 * s32Ratio;
                g_pstOsdcPrivacy->width = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].width * s32Ratio;
                g_pstOsdcPrivacy->height = g_pstOsdcCfg->osdcObj[iOsdcIndex][i].height * s32Ratio;
                COLOR_TO_IDX(0, g_pstOsdcPrivacy->color, g_pstOsdcCfg->osdcObj[iOsdcIndex][i].color);
            }
#endif
        }
        OsdcObjsNum++;
    }

    RGN_ATTR_S stRegion;
    s32Ret = CVI_RGN_GetAttr(OsdcHandle, &stRegion);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI RGN GetAttr failed with %#x!\n", s32Ret);
        return s32Ret;
    }
    pstCanvasCmprAttr->u32Width      = stRegion.unAttr.stOverlay.stSize.u32Width;
    pstCanvasCmprAttr->u32Height     = stRegion.unAttr.stOverlay.stSize.u32Height;
    pstCanvasCmprAttr->u32BgColor    = stRegion.unAttr.stOverlay.u32BgColor;
    pstCanvasCmprAttr->enPixelFormat = stRegion.unAttr.stOverlay.enPixelFormat;
    pstCanvasCmprAttr->u32BsSize     = stRegion.unAttr.stOverlay.stCompressInfo.u32CompressedSize;
    pstCanvasCmprAttr->u32ObjNum     = OsdcObjsNum;

    s32Ret = CVI_RGN_UpdateCanvas(OsdcHandle);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI RGN UpdateCanvas failed with %#x!\n", s32Ret);
    }


    return CVI_SUCCESS;
}

static int app_ipcam_ObjRectRatio_Set(void)
{
    #ifdef MD_SUPPORT
    APP_PARAM_MD_CFG_S *pstMdCfg = app_ipcam_MD_Param_Get();
    _NULL_POINTER_CHECK_(pstMdCfg, -1);

    /* get main-streaming VPSS Grp0Chn0 size */
    APP_VPSS_GRP_CFG_T *pstVpssCfg_md = &app_ipcam_Vpss_Param_Get()->astVpssGrpCfg[0];
    _NULL_POINTER_CHECK_(pstVpssCfg_md, -1);
    /* OSD Codec size form main streaming (vpss group_0 channel_0) */
    SIZE_S stOdecSize_md;
    stOdecSize_md.u32Width = pstVpssCfg_md->astVpssChnAttr[0].u32Width;
    stOdecSize_md.u32Height = pstVpssCfg_md->astVpssChnAttr[0].u32Height;
    APP_PROF_LOG_PRINT(LEVEL_INFO, "draw canvas size W=%d H=%d\n", stOdecSize_md.u32Width, stOdecSize_md.u32Height);

    /* set AI MD rect-ratio */
    g_stMdRectRatio.VpssChn_W = pstMdCfg->u32GrpWidth;
    g_stMdRectRatio.VpssChn_H = pstMdCfg->u32GrpHeight;
    g_stMdRectRatio.ScaleX = (float)stOdecSize_md.u32Width / (float)g_stMdRectRatio.VpssChn_W;
    g_stMdRectRatio.ScaleY = (float)stOdecSize_md.u32Height / (float)g_stMdRectRatio.VpssChn_H;
    #endif

    #ifdef AI_SUPPORT
    /* get main-streaming VPSS Grp0Chn0 size */
    APP_VPSS_GRP_CFG_T *pstVpssCfg = &app_ipcam_Vpss_Param_Get()->astVpssGrpCfg[0];
    _NULL_POINTER_CHECK_(pstVpssCfg, -1);
    /* OSD Codec size form main streaming (vpss group_0 channel_0) */
    SIZE_S stOdecSize;
    stOdecSize.u32Width = pstVpssCfg->astVpssChnAttr[0].u32Width;
    stOdecSize.u32Height = pstVpssCfg->astVpssChnAttr[0].u32Height;
    APP_PROF_LOG_PRINT(LEVEL_INFO, "draw canvas size W=%d H=%d\n", stOdecSize.u32Width, stOdecSize.u32Height);
    #ifdef PD_SUPPORT
    APP_PARAM_AI_PD_CFG_S *pstPdCfg = app_ipcam_Ai_PD_Param_Get();
    _NULL_POINTER_CHECK_(pstPdCfg, -1);
    /* set AI PD rect-ratio */
    g_stPdRectRatio.VpssChn_W = pstPdCfg->model_size_w;
    g_stPdRectRatio.VpssChn_H = pstPdCfg->model_size_h;
    g_stPdRectRatio.ScaleX = g_stPdRectRatio.ScaleY =
        fmax(((float)stOdecSize.u32Width / (float)g_stPdRectRatio.VpssChn_W), ((float)stOdecSize.u32Height / (float)g_stPdRectRatio.VpssChn_H));
    #endif
    #ifdef HAND_DETECT_SUPPORT
    APP_PARAM_AI_HD_CFG_S *pstHdCfg = app_ipcam_Ai_HD_Param_Get();
    _NULL_POINTER_CHECK_(pstHdCfg, -1);
    /* set AI HD rect-ratio */
    g_stHdRectRatio.VpssChn_W = pstHdCfg->u32GrpWidth;
    g_stHdRectRatio.VpssChn_H = pstHdCfg->u32GrpHeight;
    g_stHdRectRatio.ScaleX = (float)stOdecSize.u32Width / (float)g_stHdRectRatio.VpssChn_W;
    g_stHdRectRatio.ScaleY = (float)stOdecSize.u32Height / (float)g_stHdRectRatio.VpssChn_H;
    #endif

    #ifdef CONSUMER_COUNTING_SUPPORT
    APP_PARAM_AI_CONSUMER_COUNTING_CFG_S *pstCountCfg = app_ipcam_Ai_Consumer_Counting_Param_Get();
    _NULL_POINTER_CHECK_(pstCountCfg, -1);
    /* set AI Consumer Counting rect-ratio */
    g_stCountRectRatio.VpssChn_W = pstCountCfg->u32GrpWidth;
    g_stCountRectRatio.VpssChn_H = pstCountCfg->u32GrpHeight;
    g_stCountRectRatio.ScaleX = (float)stOdecSize.u32Width / (float)g_stCountRectRatio.VpssChn_W;
    g_stCountRectRatio.ScaleY = (float)stOdecSize.u32Height / (float)g_stCountRectRatio.VpssChn_H;
    #endif

    #ifdef FACE_SUPPORT
    APP_PARAM_AI_FD_CFG_S *pstFdCfg = app_ipcam_Ai_FD_Param_Get();
    _NULL_POINTER_CHECK_(pstFdCfg, -1);
    /* set AI FD rect-ratio */
    g_stFdRectRatio.VpssChn_W = pstFdCfg->u32GrpWidth;
    g_stFdRectRatio.VpssChn_H = pstFdCfg->u32GrpHeight;
    g_stFdRectRatio.ScaleX = (float)stOdecSize.u32Width / (float)g_stFdRectRatio.VpssChn_W;
    g_stFdRectRatio.ScaleY = (float)stOdecSize.u32Height / (float)g_stFdRectRatio.VpssChn_H;
    #endif
    #endif

    return CVI_SUCCESS;
}

void *Thread_Osdc_Draw(void *arg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    // struct timeval now;
    // struct timespec outtime;
    prctl(PR_SET_NAME, "OSDC_DRAW", 0, 0, 0);
    CVI_U32 SleepCnt = 0;
    int iOsdcIndex = 0;

    while (g_bOsdcThreadRun) {
        if (SleepCnt != 10) {
            SleepCnt++;
            usleep(10*1000);
            continue;
        }
        pthread_mutex_lock(&OsdcMutex);
        for (iOsdcIndex = 0; iOsdcIndex < OSDC_NUM_MAX; iOsdcIndex++) {
            if (g_pstOsdcCfg->bShow[iOsdcIndex]) {
                RGN_HANDLE OsdcHandle = g_pstOsdcCfg->handle[iOsdcIndex];
                s32Ret = app_ipcam_ObjsRectInfo_Update(OsdcHandle, iOsdcIndex);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_ObjsRectInfo_Update failed with %#x!\n", s32Ret);
                }
            }
            if (!g_bOsdcThreadRun) {
                pthread_mutex_unlock(&OsdcMutex);
                break;
            }
        }
        pthread_mutex_unlock(&OsdcMutex);
        SleepCnt = 0;
    }

    return NULL;
}

int app_ipcam_Osdc_Init(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!g_pstOsdcCfg->enable) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "draw Osdc thread not enable!\n");
        return CVI_SUCCESS;
    }

    if (g_stOsdcCanvasCfg.createCanvas == CVI_FALSE) {
        s32Ret = app_ipcam_OSDCRgn_Create();
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_OSDCRgn_Create failed with %#x!\n", s32Ret);
            return CVI_FAILURE;
        }
        g_stOsdcCanvasCfg.createCanvas = CVI_TRUE;
        APP_PROF_LOG_PRINT(LEVEL_INFO, "app_ipcam_OSDCRgn_Created!\n");
    }

    /* calculate AI Rect ratio betwen streaming and AI size */
    APP_IPCAM_CHECK_RET(app_ipcam_ObjRectRatio_Set(), "OSDC OBJ RATIO RECT SET");

    g_bOsdcThreadRun = CVI_TRUE;
    s32Ret = pthread_create(
                &g_pthOsdcRgn,
                NULL,
                Thread_Osdc_Draw,
                (CVI_VOID *)g_pstOsdcCfg);
    if (s32Ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pthread_create failed!\n");
        return CVI_FAILURE;
    }

    return CVI_SUCCESS;
}

int app_ipcam_Osdc_DeInit(void)
{
    // CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_S32 iTime = GetCurTimeInMsec();

    if (!g_pstOsdcCfg->enable) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "draw Osdc thread not enable!\n");
        return CVI_SUCCESS;
    }

    if (!g_stOsdcCanvasCfg.createCanvas) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "draw Osdc Canvas not create!\n");
        return CVI_SUCCESS;
    }

    g_bOsdcThreadRun = CVI_FALSE;
    if (g_pthOsdcRgn > (pthread_t)0) {
        pthread_join(g_pthOsdcRgn, NULL);
        g_pthOsdcRgn = 0;
    }
    APP_PROF_LOG_PRINT(LEVEL_WARN, "waiting osdc thread exit takes %u ms \n", (GetCurTimeInMsec() - iTime));

    app_ipcam_OSDCRgn_Destory();

    g_stOsdcCanvasCfg.createCanvas = CVI_FALSE;

    return CVI_SUCCESS;
}

/*****************************************************************
 *  The following API for command test used             Front
 * **************************************************************/

void app_ipcam_Osdc_Status(APP_PARAM_OSDC_CFG_S *pstOsdcCfg)
{
    if (NULL == pstOsdcCfg) {
        return ;
    }
    if ((pstOsdcCfg->enable == CVI_TRUE) && (g_stOsdcCanvasCfg.createCanvas == CVI_FALSE)) {
        g_stOsdcCfg.enable = CVI_TRUE;
        app_ipcam_Osdc_Init();
    } else if ((g_stOsdcCanvasCfg.createCanvas == CVI_TRUE) && (pstOsdcCfg->enable == CVI_FALSE)) {
        app_ipcam_Osdc_DeInit();
        g_stOsdcCfg.enable = CVI_FALSE;
    }
    if (g_stOsdcCfg.enable) {
        pthread_mutex_lock(&OsdcMutex);
        memcpy(&g_stOsdcCfg, pstOsdcCfg, sizeof(APP_PARAM_OSDC_CFG_S));
        pthread_mutex_unlock(&OsdcMutex);
    }
}


/*****************************************************************
 *  The above API for command test used                 End
 * **************************************************************/
