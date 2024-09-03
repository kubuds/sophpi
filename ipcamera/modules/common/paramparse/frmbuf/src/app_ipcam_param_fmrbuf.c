#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "minIni.h"
#include "app_ipcam_paramparse.h"

//private attribute
const char *frmbuf_format[FRMBUF_MAX] = {
    [FRMBUF_ARGB8888] = "FRMBUF_ARGB8888",
	[FRMBUF_ARGB4444] = "FRMBUF_ARGB4444",
	[FRMBUF_ARGB1555] = "FRMBUF_ARGB1555",
	[FRMBUF_256LUT] = "FRMBUF_256LUT"
};

int Load_Param_FrmBuf(const char * file)
{
    int ret = 0;
    int enum_num = 0;
    char tmp_section[32] = {0};
    char str_name[PARAM_STRING_NAME_LEN] = {0};
    APP_PARAM_FRMBUF_CTX_S * pstFrmBufCfg = app_ipcam_FrmBuf_Param_Get();
    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading FrmBuf config ------------------> start \n");

    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "frmbuf_config");

    pstFrmBufCfg->bEnable = ini_getl(tmp_section, "bEnable", 0, file);
    if (!pstFrmBufCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "FRMBUF not enable!\n");
        return CVI_FAILURE;
    }

    ini_gets(tmp_section, "pixel_fmt", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = app_ipcam_Param_Convert_StrName_to_EnumNum(str_name, frmbuf_format, FRMBUF_MAX, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][intf_type] Fail to convert string name [%s] to number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][intf_type] Convert string name [%s] to number [%d].\n", tmp_section, str_name, enum_num);
        pstFrmBufCfg->pixel_fmt = enum_num;
    }

    pstFrmBufCfg->time_start_x = ini_getl(tmp_section, "time_start_x", 0, file);
    pstFrmBufCfg->time_start_y = ini_getl(tmp_section, "time_start_y", 0, file);
    pstFrmBufCfg->date_start_x = ini_getl(tmp_section, "date_start_x", 0, file);
    pstFrmBufCfg->date_start_y = ini_getl(tmp_section, "date_start_y", 0, file);
    pstFrmBufCfg->xres = ini_getl(tmp_section, "xres", 0, file);
    pstFrmBufCfg->yres = ini_getl(tmp_section, "yres", 0, file);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "pixel_fmt:%d, time x:%d, y:%d. date x:%d, y:%d. xres:%d, yres:%d.\n"
        , pstFrmBufCfg->pixel_fmt
        , pstFrmBufCfg->time_start_x
        , pstFrmBufCfg->time_start_y
        , pstFrmBufCfg->date_start_x
        , pstFrmBufCfg->date_start_y
        , pstFrmBufCfg->xres
        , pstFrmBufCfg->yres);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading FrmBuf config ------------------> done \n\n");

    return CVI_SUCCESS;
}

