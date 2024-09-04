
#ifndef __APP_IPCAM_FRMBUF_H__
#define __APP_IPCAM_FRMBUF_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>

#include "linux/cvi_type.h"
#include <linux/cvi_common.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum APP_PARAM_FRMBUF_FMT_T{
    FRMBUF_ARGB8888 = 0,
	FRMBUF_ARGB4444,
	FRMBUF_ARGB1555,
	FRMBUF_256LUT,
	FRMBUF_MAX
} APP_PARAM_FRMBUF_FMT_S;

typedef enum APP_PARAM_FRMBUF_FONT_T{
    FONT_SIZE_16x24 = 0,
    FONT_SIZE_24x24,
    FONT_SIZE_48x64,

} APP_PARAM_FRMBUF_FONT_E;

typedef struct APP_PARAM_FRMBUF_DRAW_T {
    CVI_U32  start_y;
    CVI_U32  start_x;
    CVI_CHAR *charData;
    CVI_S32  charLen;
    CVI_VOID *pstArg;
    CVI_S32  weekFlag;
    APP_PARAM_FRMBUF_FONT_E enFontType;
    CVI_BOOL bgColor;
} APP_PARAM_FRMBUF_DRAW_S;

typedef struct APP_PARAM_FRMBUF_CTX_T {
    CVI_BOOL bEnable;
    CVI_S32 fbfd;
    CVI_VOID *fbp;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo; 
    CVI_S32 thread_enable_flag;
    CVI_S32 screensize;
    pthread_t show_thread;
    APP_PARAM_FRMBUF_FMT_S pixel_fmt;
    CVI_U32 time_start_x;
    CVI_U32 time_start_y;
    CVI_U32 date_start_x;
    CVI_U32 date_start_y;
    CVI_U32 xres;
    CVI_U32 yres;

} APP_PARAM_FRMBUF_CTX_S;

APP_PARAM_FRMBUF_CTX_S *app_ipcam_FrmBuf_Param_Get(CVI_VOID);
CVI_S32 app_ipcam_FrmBuf_DrawChar(APP_PARAM_FRMBUF_DRAW_S *pstDrawCfg);
CVI_S32 app_ipcam_FrmBuf_Init(CVI_VOID);
CVI_S32 app_ipcam_FrmBuf_DeInit(CVI_VOID);

#ifdef __cplusplus
}
#endif

#endif
