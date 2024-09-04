#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "minIni.h"
#include "app_ipcam_paramparse.h"


int Load_Param_Ai_Consumer_Counting(const char * file)
{
    int enum_num = 0;
    int ret = 0;
    char tmp_section[16] = {0};
    char str_name[PARAM_STRING_NAME_LEN] = {0};
    APP_PARAM_AI_CONSUMER_COUNTING_CFG_S * Ai = app_ipcam_Ai_Consumer_Counting_Param_Get();
    const char ** ai_supported_model = app_ipcam_Param_get_ai_supported_model();
    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading AI Consumer Counting config ------------------> start \n");

    char tmp_buff[128] = {0};

    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "ai_count_config");

    Ai->bEnable           = ini_getl(tmp_section, "count_enable", 0, file);
    Ai->VpssGrp           = ini_getl(tmp_section, "vpss_grp", 0, file);
    Ai->VpssChn           = ini_getl(tmp_section, "vpss_chn", 0, file);
    Ai->attach_pool       = ini_getl(tmp_section, "attach_pool", 0, file);
    Ai->u32GrpWidth       = ini_getl(tmp_section, "grp_width", 0, file);
    Ai->u32GrpHeight      = ini_getl(tmp_section, "grp_height", 0, file);
    Ai->bVpssPreProcSkip  = ini_getl(tmp_section, "vpssPreProcSkip", 0, file);
    Ai->CountDirMode      = ini_getl(tmp_section, "CountDirMode", 0, file);
    Ai->A_x               = ini_getl(tmp_section, "A_x", 0, file);
    Ai->A_y               = ini_getl(tmp_section, "A_y", 0, file);
    Ai->B_x               = ini_getl(tmp_section, "B_x", 0, file);
    Ai->B_y               = ini_getl(tmp_section, "B_y", 0, file);
    Ai->threshold         = ini_getf(tmp_section, "threshold", 0, file);

    ini_gets(tmp_section, "model_id", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = app_ipcam_Param_Convert_StrName_to_EnumNum(str_name, ai_supported_model, CVI_TDL_SUPPORTED_MODEL_END, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][model_id] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][model_id] Convert string name [%s] to enum number [%d]!\n", tmp_section, str_name, enum_num);
        Ai->model_id = enum_num;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "bEnable=%d Grp=%d Chn=%d GrpW=%d GrpH=%d attach_pool=%d\n",
        Ai->bEnable, Ai->VpssGrp, Ai->VpssChn, Ai->u32GrpWidth, Ai->u32GrpHeight, Ai->attach_pool);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "A_x=%d A_y=%d B_x=%d B_y=%d bSkip=%d threshold=%f\n",
        Ai->A_x, Ai->A_y, Ai->B_x, Ai->B_y, Ai->bVpssPreProcSkip, Ai->threshold);

    ini_gets(tmp_section, "model_path_count", " ", tmp_buff, 128, file);
    strncpy(Ai->model_path_count, tmp_buff, 128);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "model_path_count=%s\n",Ai->model_path_count);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading AI Consumer Counting config ------------------> done \n\n");

    return CVI_SUCCESS;
}
