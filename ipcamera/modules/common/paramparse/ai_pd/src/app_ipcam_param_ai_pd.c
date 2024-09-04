#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "minIni.h"
#include "app_ipcam_paramparse.h"

int Load_Param_Ai_PD(const char * file)
{
    APP_PARAM_AI_PD_CFG_S *Ai = app_ipcam_Ai_PD_Param_Get();
    int enum_num = 0;
    int ret = 0;
    char tmp_section[16] = {0};
    char str_name[PARAM_STRING_NAME_LEN] = {0};
    const char ** ai_supported_model = app_ipcam_Param_get_ai_supported_model();

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading AI PD config ------------------> start \n");

    char tmp_buff[128] = {0};
    float color_t;

    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "ai_pd_config");

    Ai->bEnable           = ini_getl(tmp_section, "pd_enable", 0, file);
    Ai->Intrusion_bEnable = ini_getl(tmp_section, "intrusion_enable", 0, file);
    Ai->capture_enable    = ini_getl(tmp_section, "capture_enable", 0, file);
    Ai->capture_frames    = ini_getl(tmp_section, "capture_frames", 0, file);
    Ai->VpssGrp           = ini_getl(tmp_section, "vpss_grp", 0, file);
    Ai->VpssChn           = ini_getl(tmp_section, "vpss_chn", 0, file);
    Ai->u32GrpWidth       = ini_getl(tmp_section, "grp_width", 0, file);
    Ai->u32GrpHeight      = ini_getl(tmp_section, "grp_height", 0, file);
    Ai->model_size_w      = ini_getl(tmp_section, "model_width", 0, file);
    Ai->model_size_h      = ini_getl(tmp_section, "model_height", 0, file);
    Ai->bVpssPreProcSkip  = ini_getl(tmp_section, "vpssPreProcSkip", 0, file);

    ini_gets(tmp_section, "model_id", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = app_ipcam_Param_Convert_StrName_to_EnumNum(str_name, ai_supported_model, CVI_TDL_SUPPORTED_MODEL_END, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][model_id] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][model_id] Convert string name [%s] to enum number [%d]!\n", tmp_section, str_name, enum_num);
        Ai->model_id = enum_num;
    }

    Ai->region_stRect_x1 = ini_getf(tmp_section, "region_stRect_x1", 0, file);
    Ai->region_stRect_y1 = ini_getf(tmp_section, "region_stRect_y1", 0, file);
    Ai->region_stRect_x2 = ini_getf(tmp_section, "region_stRect_x2", 0, file);
    Ai->region_stRect_y2 = ini_getf(tmp_section, "region_stRect_y2", 0, file);
    Ai->region_stRect_x3 = ini_getf(tmp_section, "region_stRect_x3", 0, file);
    Ai->region_stRect_y3 = ini_getf(tmp_section, "region_stRect_y3", 0, file);
    Ai->region_stRect_x4 = ini_getf(tmp_section, "region_stRect_x4", 0, file);
    Ai->region_stRect_y4 = ini_getf(tmp_section, "region_stRect_y4", 0, file);
    Ai->region_stRect_x5 = ini_getf(tmp_section, "region_stRect_x5", 0, file);
    Ai->region_stRect_y5 = ini_getf(tmp_section, "region_stRect_y5", 0, file);
    Ai->region_stRect_x6 = ini_getf(tmp_section, "region_stRect_x6", 0, file);
    Ai->region_stRect_y6 = ini_getf(tmp_section, "region_stRect_y6", 0, file);
    Ai->threshold        = ini_getf(tmp_section, "threshold", 0, file);

    color_t = ini_getf(tmp_section, "color_r", 0, file);
    Ai->rect_brush.color.r = color_t*255;
    color_t = ini_getf(tmp_section, "color_g", 0, file);
    Ai->rect_brush.color.g = color_t*255;
    color_t = ini_getf(tmp_section, "color_b", 0, file);
    Ai->rect_brush.color.b = color_t*255;

    Ai->rect_brush.size = ini_getl(tmp_section, "color_size", 0, file);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "bEnable=%d Grp=%d Chn=%d GrpW=%d GrpH=%d\n",
        Ai->bEnable, Ai->VpssGrp, Ai->VpssChn, Ai->u32GrpWidth, Ai->u32GrpHeight);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "model_w=%d model_h=%d bSkip=%d threshold=%f\n",
        Ai->model_size_w, Ai->model_size_h, Ai->bVpssPreProcSkip, Ai->threshold);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "color r=%f g=%f b=%f size=%d\n",
        Ai->rect_brush.color.r, Ai->rect_brush.color.g,
        Ai->rect_brush.color.b, Ai->rect_brush.size);

    ini_gets(tmp_section, "model_path", " ", tmp_buff, 128, file);
    strncpy(Ai->model_path, tmp_buff, 128);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "model_id=%d model_path=%s\n",
        Ai->model_id, Ai->model_path);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading AI PD config ------------------> done \n\n");

    return CVI_SUCCESS;
}
