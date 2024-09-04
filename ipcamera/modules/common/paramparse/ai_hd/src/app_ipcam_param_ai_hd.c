#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "minIni.h"
#include "app_ipcam_paramparse.h"


int Load_Param_Ai_HD(const char * file)
{
    APP_PARAM_AI_HD_CFG_S *Ai = app_ipcam_Ai_HD_Param_Get();
    int enum_num = 0;
    int ret = 0;
    char tmp_section[16] = {0};
    char str_name[PARAM_STRING_NAME_LEN] = {0};
    const char ** ai_supported_model = app_ipcam_Param_get_ai_supported_model();
    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading AI HD config ------------------> start \n");

    char tmp_buff[128] = {0};

    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "ai_hd_config");

    Ai->bEnable           = ini_getl(tmp_section, "hd_enable", 0, file);
    Ai->VpssGrp           = ini_getl(tmp_section, "vpss_grp", 0, file);
    Ai->VpssChn           = ini_getl(tmp_section, "vpss_chn", 0, file);
    Ai->attach_pool       = ini_getl(tmp_section, "hd_poolid", 0, file);
    Ai->u32GrpWidth       = ini_getl(tmp_section, "grp_width", 0, file);
    Ai->u32GrpHeight      = ini_getl(tmp_section, "grp_height", 0, file);
    Ai->bVpssPreProcSkip  = ini_getl(tmp_section, "vpssPreProcSkip", 0, file);

    Ai->threshold        = ini_getf(tmp_section, "threshold", 0, file);

    ini_gets(tmp_section, "model_id_hd", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = app_ipcam_Param_Convert_StrName_to_EnumNum(str_name, ai_supported_model, CVI_TDL_SUPPORTED_MODEL_END, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][model_id_hd] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][model_id_hd] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Ai->model_id_hd = enum_num;
    }

    ini_gets(tmp_section, "model_id_hdkey", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = app_ipcam_Param_Convert_StrName_to_EnumNum(str_name, ai_supported_model, CVI_TDL_SUPPORTED_MODEL_END, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][model_id_hdkey] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][model_id_hdkey] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Ai->model_id_hdkey = enum_num;
    }

    ini_gets(tmp_section, "model_id_hr", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = app_ipcam_Param_Convert_StrName_to_EnumNum(str_name, ai_supported_model, CVI_TDL_SUPPORTED_MODEL_END, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][model_id_hr] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][model_id_hr] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Ai->model_id_hr = enum_num;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "bEnable=%d Grp=%d Chn=%d GrpW=%d GrpH=%d\n", 
        Ai->bEnable, Ai->VpssGrp, Ai->VpssChn, Ai->u32GrpWidth, Ai->u32GrpHeight);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "attach_pool =%d bSkip=%d threshold=%f\n",
        Ai->attach_pool, Ai->bVpssPreProcSkip, Ai->threshold);

    ini_gets(tmp_section, "model_path_hd", " ", tmp_buff, 128, file);
    strncpy(Ai->model_path_hd, tmp_buff, 128);
    ini_gets(tmp_section, "model_path_hdkey", " ", tmp_buff, 128, file);
    strncpy(Ai->model_path_hdkey, tmp_buff, 128);
    ini_gets(tmp_section, "model_path_hr", " ", tmp_buff, 128, file);
    strncpy(Ai->model_path_hr, tmp_buff, 128);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "model_path_hd=%s,model_path_hdkey=%s,model_path_hr=%s\n",Ai->model_path_hd,Ai->model_path_hdkey,Ai->model_path_hr);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading AI HD config ------------------> done \n\n");

    return CVI_SUCCESS;
}