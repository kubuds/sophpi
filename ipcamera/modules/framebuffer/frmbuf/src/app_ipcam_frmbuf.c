#include <time.h>
#include "app_ipcam_comm.h"
#include "app_ipcam_frmbuf.h"
#include "app_ipcam_frmbuf_fm.h"

#define FRMBUF_FONT_LIB_48x64_W      48
#define FRMBUF_FONT_LIB_48x64_H      64
#define FRMBUF_FONT_LIB_24x24_W      24
#define FRMBUF_FONT_LIB_24x24_H      24
#define FRMBUF_FONT_LIB_16x24_W      16
#define FRMBUF_FONT_LIB_16x24_H      24
#define BYTE_BITS                    8

APP_PARAM_FRMBUF_CTX_S g_stFrmBufCtx, *g_pstFrmBufCtx = &g_stFrmBufCtx;

APP_PARAM_FRMBUF_CTX_S *app_ipcam_FrmBuf_Param_Get(CVI_VOID) {
    return g_pstFrmBufCtx;
}

static CVI_S32 getFontMod48x64(CVI_CHAR *Character, CVI_U8 **FontMod, CVI_S32 *FontModLen) {
    CVI_U32 offset = 0;

    offset = (Character[0] - 0x30) * (FRMBUF_FONT_LIB_48x64_W * FRMBUF_FONT_LIB_48x64_H / 8);
    *FontMod = (CVI_U8 *)fontMatrix48x64 + offset;
    *FontModLen = FRMBUF_FONT_LIB_48x64_W * FRMBUF_FONT_LIB_48x64_H / 8;

    return CVI_SUCCESS;
}

static CVI_S32 getFontMod(CVI_CHAR *Character, CVI_U8 **FontMod, CVI_S32 *FontModLen, 
    CVI_S32 weekFlag) {
    CVI_U32 offset = 0;

    if (weekFlag) { //When Character represents week
        offset = ((Character[0] - 0x30) + 3) * (FRMBUF_FONT_LIB_24x24_W * FRMBUF_FONT_LIB_24x24_H / 8);
        *FontMod = (CVI_U8 *)fontMatrixChar24x24 + offset;
        *FontModLen = FRMBUF_FONT_LIB_24x24_W * FRMBUF_FONT_LIB_24x24_H / 8;
        return CVI_SUCCESS;
    }

    if ((Character[0] >= 0x30) && (Character[0] <= 0x39)) {
        offset = (Character[0] - 0x30) * (FRMBUF_FONT_LIB_16x24_W * FRMBUF_FONT_LIB_16x24_H / 8);
        *FontMod = (CVI_U8 *)fontMatrixFigure16x24 + offset;
        *FontModLen = FRMBUF_FONT_LIB_16x24_W * FRMBUF_FONT_LIB_16x24_H / 8;
    } else if (Character[0] == 0x6D){    // 0x6D is m in ASCII code
        offset = 0;
        *FontMod = (CVI_U8 *)fontMatrixChar24x24 + offset;
        *FontModLen = FRMBUF_FONT_LIB_24x24_W * FRMBUF_FONT_LIB_24x24_H / 8;
    } else if (Character[0] == 0x64){    // 0x64 is d in ASCII code
        offset = (FRMBUF_FONT_LIB_24x24_W * FRMBUF_FONT_LIB_24x24_H / 8);
        *FontMod = (CVI_U8 *)fontMatrixChar24x24 + offset;
        *FontModLen = FRMBUF_FONT_LIB_24x24_W * FRMBUF_FONT_LIB_24x24_H / 8;
    } else if (Character[0] == 0x77){    // 0x77 is w in ASCII code
        offset = 2 * (FRMBUF_FONT_LIB_24x24_W * FRMBUF_FONT_LIB_24x24_H / 8);
        *FontMod = (CVI_U8 *)fontMatrixChar24x24 + offset;
        *FontModLen = FRMBUF_FONT_LIB_24x24_W * FRMBUF_FONT_LIB_24x24_H / 8;
    } else if (Character[0] == 0x6B){    // 0x6B is k in ASCII code
        offset = 3 * (FRMBUF_FONT_LIB_24x24_W * FRMBUF_FONT_LIB_24x24_H / 8);
        *FontMod = (CVI_U8 *)fontMatrixChar24x24 + offset;
        *FontModLen = FRMBUF_FONT_LIB_24x24_W * FRMBUF_FONT_LIB_24x24_H / 8;
    } else if (Character[0] == 0x20){    // 0x20 is space in ASCII code
        offset = 11 * (FRMBUF_FONT_LIB_24x24_W * FRMBUF_FONT_LIB_24x24_H / 8);
        *FontMod = (CVI_U8 *)fontMatrixChar24x24 + offset;
        *FontModLen = FRMBUF_FONT_LIB_24x24_W * FRMBUF_FONT_LIB_24x24_H / 8;
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "Character[0]=%d\n", Character[0]);
        return CVI_FAILURE;
    }

    return CVI_SUCCESS;
}

static CVI_S32 fillColorByte(CVI_VOID *addr, CVI_U32 val, CVI_U32 numBytes) {
    CVI_U32 i = 0;
    CVI_U8 *ptr = NULL;

    if ((addr == NULL) || (numBytes == 0)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Param is invalid. addr:%p, numBytes:%d.\n"
            , addr, numBytes);
        return CVI_FAILURE;
    }
    ptr = (CVI_U8 *)addr;

    for(i = 0; i < numBytes; i++) {
        ptr[i] = (val >> (i*8)) & 0xFF;
    }
    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_FrmBuf_DrawChar(APP_PARAM_FRMBUF_DRAW_S *pstDrawCfg) {
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_U32 row = 0;
    CVI_U32 col = 0;
    CVI_U8  temp = 0;
    CVI_U32 u32FgColor = 0;
    CVI_U32 u32BgColor = 0;
    CVI_U32 u32FriCanvasWidth = 0;
    CVI_U32 u32FriCanvasHeight = 0;
    CVI_U8  *FontMod = NULL;
    CVI_S32 FontModLen = 0;
    CVI_U32 fontWidth = 0;
    CVI_U32 fontHeight = 0;

    APP_PARAM_FRMBUF_CTX_S *pstFrmBufCtx = (APP_PARAM_FRMBUF_CTX_S *)pstDrawCfg->pstArg;
    struct fb_fix_screeninfo *pstFinfo = &pstFrmBufCtx->finfo;
    struct fb_var_screeninfo *pstVinfo = &pstFrmBufCtx->vinfo;

    CVI_U32  start_y   = pstDrawCfg->start_y;
    CVI_U32  start_x   = pstDrawCfg->start_x;
    CVI_CHAR *charData = pstDrawCfg->charData;
    CVI_S32  charLen   = pstDrawCfg->charLen;
    CVI_S32  weekFlag  = pstDrawCfg->weekFlag;

    APP_PARAM_FRMBUF_FONT_E enFontType = pstDrawCfg->enFontType;
    switch (enFontType)
    {
    case FONT_SIZE_16x24:
        fontWidth  = 16;
        fontHeight = 24;
        break;
    case FONT_SIZE_24x24:
        fontWidth  = 24;
        fontHeight = 24;
        break;
    case FONT_SIZE_48x64:
        fontWidth  = 48;
        fontHeight = 64;
        break;
    default:
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Don't support enFontType:%d. \n"
            , enFontType);
        return CVI_FAILURE;
    }

    u32FgColor = 0xEFFFFFFF | 0x80000000; //white, enable
    u32BgColor = 0x00000000 | 0x00000000; //black, disable

    u32FriCanvasWidth = charLen * fontWidth;
    u32FriCanvasHeight = fontHeight;

    for (row = start_y; row < u32FriCanvasHeight+start_y; ++row) {
        for (col = start_x; col < u32FriCanvasWidth+start_x; ++col) {
            //Here, if pstFinfo->line_length is not divided by 2, it will cause pixel gaps.
            CVI_S32 s32Offset = row * pstFinfo->line_length + col*pstVinfo->bits_per_pixel/8;
            //The xth character, starting from 0
            CVI_S32 s32CharIdx = (col - start_x) / fontWidth;
            //The xth column pixel of the current character, starting from 0
            CVI_S32 s32CharCol = (col - start_x) - (fontWidth * s32CharIdx);
            //The xth row of pixels of the current character, starting from 0
            CVI_S32 s32CharRow = row - start_y;
            //In the font of the current character, one byte corresponding to the current pixel
            CVI_S32 s32HexOffset = 
                s32CharRow * fontWidth / BYTE_BITS + s32CharCol / BYTE_BITS;
            //The current pixel currently traversed, the xth bit in the byte
            CVI_S32 s32BitOffset = s32CharCol % BYTE_BITS;

            if ((enFontType == FONT_SIZE_16x24) || (enFontType == FONT_SIZE_24x24)) {
                if (getFontMod(&charData[s32CharIdx], &FontMod, &FontModLen, weekFlag) == CVI_SUCCESS) {
                    if (FontMod != NULL && s32HexOffset < FontModLen) {
                        temp = FontMod[s32HexOffset];
                        if ((temp >> ((BYTE_BITS - 1) - s32BitOffset)) & 0x1) {
                            s32Ret = fillColorByte(pstFrmBufCtx->fbp + s32Offset, u32FgColor, pstVinfo->bits_per_pixel/8);
                            if (s32Ret != CVI_SUCCESS) {
                                APP_PROF_LOG_PRINT(LEVEL_ERROR, "fillColorByte filed. s32Ret:%d. \n"
                                    , s32Ret);
                            }
                        } else {
                            if (pstDrawCfg->bgColor) {
                                s32Ret = fillColorByte(pstFrmBufCtx->fbp + s32Offset, u32BgColor, pstVinfo->bits_per_pixel/8);
                                if (s32Ret != CVI_SUCCESS) {
                                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "fillColorByte filed. s32Ret:%d. \n"
                                        , s32Ret);
                                }
                            }
                        }
                        continue;
                    }
                }
            } else if (enFontType == FONT_SIZE_48x64) {
                if (getFontMod48x64(&charData[s32CharIdx], &FontMod, &FontModLen) == CVI_SUCCESS) {
                    if (FontMod != NULL && s32HexOffset < FontModLen) {
                        temp = FontMod[s32HexOffset];
                        if ((temp >> ((BYTE_BITS - 1) - s32BitOffset)) & 0x1) {
                            s32Ret = fillColorByte(pstFrmBufCtx->fbp + s32Offset, u32FgColor, pstVinfo->bits_per_pixel/8);
                            if (s32Ret != CVI_SUCCESS) {
                                APP_PROF_LOG_PRINT(LEVEL_ERROR, "fillColorByte filed. s32Ret:%d. \n"
                                    , s32Ret);
                            }
                        } else {
                            if (pstDrawCfg->bgColor) {
                                s32Ret = fillColorByte(pstFrmBufCtx->fbp + s32Offset, u32BgColor, pstVinfo->bits_per_pixel/8);
                                if (s32Ret != CVI_SUCCESS) {
                                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "fillColorByte filed. s32Ret:%d. \n"
                                        , s32Ret);
                                }
                            }
                        }
                        continue;
                    }
                }
            } else {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "Don't support enFontType:%d. \n"
                    , enFontType);
                return CVI_FAILURE;
            }

            APP_PROF_LOG_PRINT(LEVEL_INFO, "getFontMod Fail. str:(%c), "
                "idx:%d, offset:%d, modlen:%d.\n"
                , charData[s32CharIdx]
                , s32CharIdx
                , s32HexOffset
                , FontModLen);
            continue;
        }
    }
    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_FrmBuf_Init(CVI_VOID) {
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_S32 fbfd = 0;
    CVI_S32 screensize = 0;
    struct fb_var_screeninfo vinfo = {0};
    struct fb_fix_screeninfo finfo = {0}; 
    CVI_VOID *fbp = NULL;
    APP_PARAM_FRMBUF_CTX_S *pstFrmBufCtx = NULL;

    memset(&vinfo, 0, sizeof(struct fb_var_screeninfo));
    memset(&finfo, 0, sizeof(struct fb_fix_screeninfo));
    pstFrmBufCtx = app_ipcam_FrmBuf_Param_Get();

    if (!pstFrmBufCtx->bEnable) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Frame buffer not enable.\n");
        return CVI_FAILURE;
    }

    // open device
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Error opening framebuffer device");
        return CVI_FAILURE;
    }
    APP_PROF_LOG_PRINT(LEVEL_INFO, "The framebuffer device opened successfully.\n");

    // Get variable screen information
    ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "%dx%d, %dx%d, offset%dx%d, %dbpp, "
        "grayscale:%d, transp:%d,%d,%d, red:%d,%d,%d, green:%d,%d,%d, blue:%d,%d,%d.\n"
        , vinfo.xres, vinfo.yres
        , vinfo.xres_virtual, vinfo.yres_virtual
        , vinfo.xoffset, vinfo.yoffset
        , vinfo.bits_per_pixel
        , vinfo.grayscale
        , vinfo.transp.length, vinfo.transp.msb_right, vinfo.transp.offset
        , vinfo.red.length,    vinfo.red.msb_right,    vinfo.red.offset
        , vinfo.green.length,  vinfo.green.msb_right,  vinfo.green.offset
        , vinfo.blue.length,   vinfo.blue.msb_right,   vinfo.blue.offset);

    // Set variable screen information
    vinfo.xres = pstFrmBufCtx->xres;
    vinfo.yres = pstFrmBufCtx->yres;
    switch (pstFrmBufCtx->pixel_fmt) {
        case FRMBUF_ARGB8888:
            vinfo.bits_per_pixel = 32;
            break;
        case FRMBUF_ARGB4444:
            vinfo.bits_per_pixel = 16;
            vinfo.green.length = 4;
            break;
        case FRMBUF_ARGB1555:
            vinfo.bits_per_pixel = 16;
            break;
        case FRMBUF_256LUT:
            vinfo.bits_per_pixel = 8;
            break;
        default:
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Don't support format:%d.\n"
                , pstFrmBufCtx->pixel_fmt);
            if (pstFrmBufCtx->fbfd) {
                close(pstFrmBufCtx->fbfd);
            }
            return CVI_FAILURE;
    }
    ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo);

    ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "%dx%d, %dx%d, offset%dx%d, %dbpp, "
        "grayscale:%d, transp:%d,%d,%d, red:%d,%d,%d, green:%d,%d,%d, blue:%d,%d,%d.\n"
        , vinfo.xres, vinfo.yres
        , vinfo.xres_virtual, vinfo.yres_virtual
        , vinfo.xoffset, vinfo.yoffset
        , vinfo.bits_per_pixel
        , vinfo.grayscale
        , vinfo.transp.length, vinfo.transp.msb_right, vinfo.transp.offset
        , vinfo.red.length,    vinfo.red.msb_right,    vinfo.red.offset
        , vinfo.green.length,  vinfo.green.msb_right,  vinfo.green.offset
        , vinfo.blue.length,   vinfo.blue.msb_right,   vinfo.blue.offset);

    // Get fixed screen information
    ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "smem_len:%d, type:%d, type_aux:%d, "
        "visual:%d, xp:%d, yp:%d, yw:%d, line_length:%d, mmio_len:%d, accel:%d, cap:%d.\n"
        , finfo.smem_len
        , finfo.type, finfo.type_aux
        , finfo.visual
        , finfo.xpanstep, finfo.ypanstep, finfo.ywrapstep
        , finfo.line_length
        , finfo.mmio_len
        , finfo.accel
        , finfo.capabilities);

    // Map framebuffer to user space
    screensize = finfo.line_length * vinfo.yres;

    fbp = mmap(0, screensize
        , PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbp == (CVI_VOID *)-1) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Error mapping framebuffer device to memory");
        return CVI_FAILURE;
    }

    memcpy(&pstFrmBufCtx->vinfo, &vinfo, sizeof(struct fb_var_screeninfo));
    memcpy(&pstFrmBufCtx->finfo, &finfo, sizeof(struct fb_fix_screeninfo));

    memcpy(&pstFrmBufCtx->vinfo.transp, &vinfo.transp, sizeof(struct fb_bitfield));
    memcpy(&pstFrmBufCtx->vinfo.red,    &vinfo.red,    sizeof(struct fb_bitfield));
    memcpy(&pstFrmBufCtx->vinfo.green,  &vinfo.green,  sizeof(struct fb_bitfield));
    memcpy(&pstFrmBufCtx->vinfo.blue,   &vinfo.blue,   sizeof(struct fb_bitfield));

    pstFrmBufCtx->screensize = screensize;
    pstFrmBufCtx->fbfd = fbfd;
    pstFrmBufCtx->fbp = fbp;
    pstFrmBufCtx->thread_enable_flag = CVI_TRUE;

    APP_PROF_LOG_PRINT(LEVEL_INFO, "app_ipcam_FrmBuf_Init done.\n");
    return s32Ret;
}

CVI_S32 app_ipcam_FrmBuf_DeInit(CVI_VOID) {
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PARAM_FRMBUF_CTX_S *pstFrmBufCtx = NULL;
    pstFrmBufCtx = app_ipcam_FrmBuf_Param_Get();

    // Release mapped memory and close framebuffer
    munmap(pstFrmBufCtx->fbp, pstFrmBufCtx->screensize);
    if (pstFrmBufCtx->fbfd) {
        close(pstFrmBufCtx->fbfd);
        pstFrmBufCtx->fbfd = 0;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "app_ipcam_FrmBuf_DeInit done.\n");

    return s32Ret;
}
